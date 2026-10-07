"""Measure production SC-55-like MIDI against D2 Definitive Collection CD audio.

Uses the music_spectral Python environment and freshly built test_music_synth.
Reports source levels before shared gameplay headroom trim, without normalizing listening samples.
"""

import argparse
import html
import json
from pathlib import Path
import re

import imageio_ffmpeg
import numpy as np

from compare_music_spectra import ROOT, RATE, decode, digest, levels, run, write_wav


def cd_tracks(cue):
    """Read separate BIN tracks, honoring INDEX 01 instead of including pregaps."""
    filename, track, mode = None, None, None
    seen = set()
    for line in cue.read_text().splitlines():
        match = re.fullmatch(r'\s*FILE "(.+)" BINARY\s*', line)
        if match:
            filename = cue.parent / match[1]
        match = re.fullmatch(r"\s*TRACK (\d+) (\S+)\s*", line)
        if match:
            track, mode = int(match[1]), match[2]
        match = re.fullmatch(r"\s*INDEX 01 (\d+):(\d+):(\d+)\s*", line)
        if match and mode == "AUDIO":
            if filename in seen:
                raise ValueError("This diagnostic requires one BIN per audio track")
            seen.add(filename)
            minutes, seconds, frames = map(int, match.groups())
            yield track, filename, ((minutes * 60 + seconds) * 75 + frames) * 2352


def main():
    # Share gameplay tuning, including the default base gain, with the engine
    calibration = (ROOT / "android/app/src/main/cpp/shared/music_playback_levels.h").read_text()

    def constant(name):
        return float(re.search(r"#define " + name + r" ([-\d.]+)f", calibration)[1])

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--renderer", type=Path, default=ROOT / "android/build/host-extract-tests/Release/test_music_synth.exe"
    )
    parser.add_argument("--manifest", type=Path, default=ROOT / "android/tests/music_spectral/sc55.json")
    parser.add_argument(
        "--cue",
        type=Path,
        default=ROOT
        / "game_data/CD images/Descent I and II - The Definitive Collection (Europe) (Disc 2)/Descent I and II - The Definitive Collection (Europe) (Disc 2).cue",
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seconds", type=int, default=120, choices=range(20, 121))
    parser.add_argument(
        "--gain-db",
        type=float,
        default=constant("MUSIC_GAMEPLAY_GAIN_DB"),
        help="Android base gameplay gain before the D2 profile adjustment (defaults to native header)",
    )
    parser.add_argument(
        "--baseline", type=Path, help="Previous report.json; require improved median balance and no MIDI clipping"
    )
    parser.add_argument(
        "--uncalibrated", action="store_true", help="Measure the previous playback policy using the same renderer"
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = json.loads(args.manifest.read_text())
    font = ROOT / manifest["soundfont"]
    ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()
    report = dict(
        renderer_sha256=digest(args.renderer),
        soundfont_sha256=digest(font),
        settings=dict(seconds=args.seconds, gain_db=args.gain_db, eq=2, voices=128, volume=8, sample_rate=RATE),
        scope="Source levels before shared gameplay headroom trim. First measured seconds per selection, shorter CD tracks measured to their end; separate arrangements, not aligned pairs",
        tracks=[],
    )
    baseline = json.loads(args.baseline.read_text()) if args.baseline else None
    if baseline:
        assert baseline["settings"] == report["settings"], "Comparison settings differ"
        assert baseline["soundfont_sha256"] == report["soundfont_sha256"], "Soundfont differs"
    sections = []

    def record(name, group, pcm, source_hash):
        measurement = levels(ffmpeg, pcm)
        assert np.isfinite(measurement["lufs"]), f"{name} is silent"
        clip = name + "-listen.wav"
        write_wav(args.output / clip, pcm[: 30 * RATE])
        row = dict(id=name, group=group, source_sha256=source_hash, seconds=len(pcm) / RATE, **measurement)
        report["tracks"].append(row)
        before_text, before_player = "", ""
        if baseline:
            previous = next(item for item in baseline["tracks"] if item["id"] == name)
            assert previous["source_sha256"] == source_hash, f"{name} source differs"
            before_text = f"<td>{previous['lufs']:.2f}</td>"
            before_clip = name + "-before.wav"
            (args.output / before_clip).write_bytes((args.baseline.parent / clip).read_bytes())
            before_player = f'<label>Before<audio controls preload="none" src="{before_clip}"></audio></label>'
        sections.append(
            f"<tr><td>{html.escape(name)}</td>{before_text}<td>{measurement['lufs']:.2f}</td>"
            f"<td>{measurement['true_peak_dbfs']:.2f}</td><td>{measurement['boundary_samples']}</td></tr>"
        )
        players.append(
            f'<h3>{html.escape(name)}</h3>{before_player}<label>Current<audio controls preload="none" src="{clip}"></audio></label>'
        )
        print(
            f"{name}: {measurement['lufs']:.2f} LUFS, {measurement['true_peak_dbfs']:.2f} dBTP, {measurement['boundary_samples']} boundary samples",
            flush=True,
        )

    players = []
    cd_scale = 1.0 if args.uncalibrated else constant("MUSIC_CD_VOLUME_SCALE")
    midi_boost = 0.0 if args.uncalibrated else constant("MUSIC_D2_SF2_BOOST_DB")
    report["calibration"] = dict(cd_scale=cd_scale, d2_sf2_boost_db=midi_boost)
    for game in manifest["games"]:
        hog = ROOT / game["hog"]
        songs = (
            ["descent", "briefing", "credits", "game01", "game02", "game03", "game04"]
            if game["game"] == "d2"
            else ["descent", "game01", "game07", "game08"]
        )
        for song in songs:
            name = f"{game['game']}-{song}"
            target = args.output / (name + ".wav")
            gain = args.gain_db + (midi_boost if game["game"] == "d2" else 0)
            result = run(
                [args.renderer, "--render-eq", font, hog, song + ".hmp", target, "sf2", args.seconds, 2, gain],
                text=True,
            )
            assert "actual=sf2" in result.stdout and "equalizer=2" in result.stdout, result.stdout
            assert float(re.search(r"gameplay_gain_db=([-\d.]+)", result.stdout)[1]) == constant(
                "MUSIC_GAMEPLAY_GAIN_DB"
            ), "Rebuild the native renderer"
            assert (
                abs(
                    float(re.search(r"cd_volume_scale=([\d.e+-]+)", result.stdout)[1])
                    - constant("MUSIC_CD_VOLUME_SCALE")
                )
                < 1e-8
            ), "Rebuild the native renderer"
            assert float(re.search(r"d2_sf2_boost_db=([-\d.]+)", result.stdout)[1]) == constant(
                "MUSIC_D2_SF2_BOOST_DB"
            ), "Rebuild the native renderer"
            (args.output / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf8")
            record(name, game["game"] + "-midi", decode(ffmpeg, target, args.seconds), digest(hog))
    for number, path, offset in cd_tracks(args.cue):
        result = run(
            [
                ffmpeg,
                "-v",
                "error",
                "-f",
                "s16le",
                "-ar",
                44100,
                "-ac",
                2,
                "-skip_initial_bytes",
                offset,
                "-i",
                path,
                "-t",
                args.seconds,
                "-f",
                "f32le",
                "-ar",
                RATE,
                "-",
            ]
        )
        pcm = np.frombuffer(result.stdout, dtype="<f4").reshape(-1, 2)
        record(f"cd-{number:02d}", "cd", pcm * cd_scale, digest(path))
    for group in ("d1-midi", "d2-midi", "cd"):
        values = [row for row in report["tracks"] if row["group"] == group]
        assert values, f"No {group} tracks"
        report[group] = dict(
            median_lufs=float(np.median([row["lufs"] for row in values])),
            max_true_peak_dbfs=max(row["true_peak_dbfs"] for row in values),
        )
    report["cd_minus_d2_db"] = report["cd"]["median_lufs"] - report["d2-midi"]["median_lufs"]
    report["midi_boundary_samples"] = sum(row["boundary_samples"] for row in report["tracks"] if row["group"] != "cd")
    report["passed"] = report["midi_boundary_samples"] == 0
    if baseline:
        report["passed"] &= abs(report["cd_minus_d2_db"]) < abs(baseline["cd_minus_d2_db"])
        report["passed"] &= abs(report["cd_minus_d2_db"]) < 1.0
    (args.output / "report.json").write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf8")
    (args.output / "report.html").write_text(
        '<!doctype html><meta charset="utf-8"><title>MIDI / CD loudness</title>'
        "<style>body{font:16px system-ui;max-width:1000px;margin:30px auto}td,th{padding:6px 16px;text-align:right}audio{display:block;width:100%;max-width:600px}</style>"
        "<h1>Production MIDI / CD loudness</h1><p>Bundled SC-55-like bank, balanced EQ, 128 voices, effects enabled, "
        f"{args.gain_db:g} dB gameplay gain, volume 8. First {args.seconds} seconds per selection (shorter CD tracks to their end). "
        "CD and MIDI are different arrangements. Integrated loudness is measured in LUFS; peaks are oversampled true peaks. "
        "Source levels exclude shared gameplay headroom trim. Listening excerpts retain source levels, with no normalization.</p>"
        f"<p>Median CD minus D2 MIDI: {report['cd_minus_d2_db']:.2f} dB. MIDI boundary samples: {report['midi_boundary_samples']}.</p>"
        f"<p>Calibration: D2 soundfont boost {midi_boost:g} dB; CD scale {cd_scale:.6f}. D1 gain unchanged. "
        "CD reference decoding uses FFmpeg resampling; gameplay uses linear resampling. These are host measurements, not speaker recordings.</p>"
        "<table><tr><th>Selection</th>"
        + ("<th>Before LUFS</th>" if baseline else "")
        + "<th>Current LUFS</th><th>True peak dBFS</th><th>Boundary samples</th></tr>"
        + "".join(sections)
        + "</table>"
        + "".join(players)
        + '<script>document.addEventListener("play",e=>document.querySelectorAll("audio").forEach(a=>{if(a!==e.target)a.pause()}),true)</script>',
        encoding="utf8",
    )
    print(json.dumps({key: report[key] for key in ("d1-midi", "d2-midi", "cd", "cd_minus_d2_db", "passed")}, indent=2))
    if not report["passed"]:
        raise SystemExit("Loudness/headroom checks failed; inspect report.json")


if __name__ == "__main__":
    main()
