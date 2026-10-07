"""Capture actual D1/D2 music plus effects and check calibration and headroom.

Run through test_audio_mix.ps1 with a current debug APK installed. Uses the
music_spectral environment and a locally owned Definitive Collection CD image.
"""

import argparse
import copy
import hashlib
import html
import json
import os
from pathlib import Path
import subprocess
import wave

import imageio_ffmpeg
import numpy as np

from compare_music_loudness import cd_tracks
from compare_music_spectra import ROOT, RATE, levels, write_wav


def command(args, **kwargs):
    return subprocess.run(list(map(str, args)), check=True, timeout=600, **kwargs)


def debug(field, value):
    return dict(action="set_debug", field=field, value=str(value))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()
    sdk = Path(os.environ.get("ANDROID_HOME", "C:/local/android-sdk"))
    adb = [str(sdk / "platform-tools" / ("adb.exe" if os.name == "nt" else "adb")), "-s", args.serial]
    package = os.environ.get("DXX_TEST_PACKAGE", "com.dxxredux.app")

    def device(*parts, **kwargs):
        return command([*adb, *parts], **kwargs)

    cue = ROOT / "game_data/CD images/Descent I and II - The Definitive Collection (Europe) (Disc 2)/Descent I and II - The Definitive Collection (Europe) (Disc 2).cue"
    _, bin_path, offset = list(cd_tracks(cue))[1]
    with bin_path.open("rb") as source:
        source.seek(offset)
        pcm = source.read(60 * 44100 * 4)
    assert len(pcm) == 60 * 44100 * 4, "CD fixture needs sixty seconds"
    fixture = output / "reference.bin"
    fixture.write_bytes(bytes(10 * 2352) + pcm)
    mp3 = output / "reference.mp3"
    command([ffmpeg, "-y", "-v", "error", "-f", "s16le", "-ar", "44100", "-ac", "2", "-i", "pipe:0", "-codec:a", "libmp3lame", "-b:a", "320k", mp3], input=pcm)
    for path in (fixture, mp3):
        device("push", path, "/data/local/tmp/audio-mix-" + path.name, stdout=subprocess.DEVNULL)

    template = json.loads((ROOT / "android/game_scripts/test_audio_mix_unified.jsonc").read_text())
    report = dict(scope="30-second live level-1 output, lasers, explosions and eight simultaneous explosions; identical CD and MP3 source excerpt", source_sha256=hashlib.sha256(pcm).hexdigest(), results=[])
    failures = []
    for game in ("d1", "d2"):
        for source in ("midi", "cd", "mp3"):
            name = game + "-" + source
            steps = copy.deepcopy(template)
            launch = next(i for i, step in enumerate(steps) if step.get("action") == "enter_game")
            if source == "cd":
                steps[launch:launch] = [
                    dict(action="write_config", file="audio-mix.cue", content='FILE "audio-mix-reference.bin" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n  TRACK 02 AUDIO\n    INDEX 01 00:00:10\n'),
                    dict(action="setup_command", command="add_audio_source", args=dict(bin_path="/data/local/tmp/audio-mix-reference.bin", cue_name="audio-mix.cue", label="Audio calibration", id="audio-mix")),
                    dict(action="setup_command", command="write_music_prefs", args=dict(source="cd", prefer_mission_soundtrack=False)),
                ]
                steps += [dict(action="music_control", operation="play", track=2)]
            elif source == "mp3":
                steps += [debug("audio_music_file", "/data/local/tmp/audio-mix-reference.mp3")]
            else:
                steps += [dict(action="music_control", operation="play", track=5)]
            steps += [dict(action="wait_ms", ms=500), debug("capture_audio", "start")]
            # Exercise real weapon input, then reproducible effects through the
            # game's sample API. The stack deliberately stresses overlapping SFX
            for index in range(20):
                steps += [dict(action="key", key="lctrl", post_delay_ms=500)]
                if index % 5 == 0:
                    steps += [debug("audio_effects_probe", "explosion")]
            steps += [debug("audio_effects_probe", "stack"), dict(action="wait_ms", ms=18000), debug("capture_audio", "stop"), dict(action="introspect")]
            script = output / (name + ".jsonc")
            script.write_text(json.dumps(steps, indent=2) + "\n")
            print("Capturing " + name, flush=True)
            device("logcat", "-c")
            with (output / (name + ".log")).open("w") as log:
                command(["pwsh", "-NoProfile", "-File", ROOT / "android/helpers/run_test.ps1", "-ScriptName", script, "-Game", game, "-Serial", args.serial], stdout=log, stderr=subprocess.STDOUT)
            arrays = []
            for kind, remote in (("mix", "store-audio.wav"), ("music", "store-music.wav")):
                path = output / (name + "-" + kind + ".wav")
                with path.open("wb") as target:
                    device("exec-out", "run-as", package, "cat", "files/" + remote, stdout=target)
                with wave.open(str(path)) as wav:
                    assert wav.getframerate() == RATE and wav.getnchannels() == 2 and wav.getsampwidth() == 2
                    arrays.append(np.frombuffer(wav.readframes(wav.getnframes()), dtype="<i2").reshape(-1, 2).astype(np.float32) / 32768)
            mix, music = arrays
            assert mix.shape == music.shape and len(mix) > 25 * RATE
            # This residual represents summed effects only when the mixer has
            # not clipped; reject boundary samples before interpreting it
            effects = mix - music
            measurements = {key: levels(ffmpeg, value) for key, value in zip(("mix", "music", "effects"), (mix, music, effects))}
            for key in ("mix", "music"):
                if measurements[key]["boundary_samples"] or measurements[key]["true_peak_dbfs"] >= 0:
                    failures.append(name + ": " + key + " clips or reaches full scale")
            if not all(np.isfinite(row["lufs"]) for row in measurements.values()):
                failures.append(name + ": missing audible music or effects")
            if measurements["mix"]["lufs"] < -30:
                failures.append(name + ": combined output below -30 LUFS")
            write_wav(output / (name + "-effects.wav"), effects)
            row = dict(id=name, seconds=len(mix) / RATE, **measurements)
            report["results"].append(row)
            print(json.dumps(row), flush=True)
            (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")

    for game in ("d1", "d2"):
        rows = {row["id"]: row for row in report["results"]}
        gap = abs(rows[game + "-cd"]["music"]["lufs"] - rows[game + "-mp3"]["music"]["lufs"])
        if gap > 1:
            failures.append(game + ": CD/MP3 matched-source gap exceeds 1 dB")
    report["failures"] = failures
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    rows = []
    for row in report["results"]:
        name = row["id"]
        rows.append(f'<tr><td>{name}</td><td>{row["music"]["lufs"]:.2f}</td><td>{row["effects"]["lufs"]:.2f}</td><td>{row["mix"]["lufs"]:.2f}</td><td>{row["mix"]["true_peak_dbfs"]:.2f}</td><td><audio controls src="{name}-mix.wav"></audio></td></tr>')
    (output / "report.html").write_text('<!doctype html><meta charset="utf-8"><title>Gameplay audio calibration</title><h1>Gameplay audio calibration</h1><p>' + html.escape(report["scope"]) + '</p><p>Raw playback levels; effects are mix minus the aligned music stem. This is a finite fixture, not a clipping guarantee for every mod or soundfont.</p><table><tr><th>Case</th><th>Music LUFS</th><th>Effects LUFS</th><th>Mix LUFS</th><th>True peak dBFS</th><th>Listen</th></tr>' + ''.join(rows) + '</table><pre>' + html.escape('\n'.join(failures) or 'All checks passed') + '</pre>')
    if failures:
        raise RuntimeError("; ".join(failures))


if __name__ == "__main__":
    main()
