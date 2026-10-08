"""Run the real initial MacPlay disc through Android import and D1 sound playback."""

import argparse
import array
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import wave

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--package", required=True)
    parser.add_argument("--adb", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--script", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    media = ROOT / "game_data/CD images/Descent - Mac macplay"
    cue = media / "Descent - Mac macplay.cue"
    binary = media / "Descent - Mac macplay.bin"
    if not cue.is_file() or not binary.is_file():
        raise RuntimeError("Initial MacPlay disc fixture is required")
    staged_cue = output / "disc.cue"
    staged_cue.write_text(re.sub(r'FILE "[^"]+"', 'FILE "disc.bin"', cue.read_text()))
    adb = [args.adb, "-s", args.serial]

    def device(*parts):
        return subprocess.check_output([*adb, *parts], timeout=180)

    device("shell", "mkdir", "-p", "/data/local/tmp/dxx-macplay-audio")
    device("push", str(staged_cue), "/data/local/tmp/dxx-macplay-audio/disc.cue")
    device("push", str(binary), "/data/local/tmp/dxx-macplay-audio/disc.bin")
    device("logcat", "-c")
    with (output / "automation.log").open("w") as log:
        subprocess.run(
            [
                "pwsh",
                "-NoProfile",
                "-File",
                str(ROOT / "android/helpers/run_test.ps1"),
                "-ScriptName",
                str(args.script.resolve()),
                "-Game",
                "d1",
                "-Serial",
                args.serial,
                "-LeaveRunning",
            ],
            cwd=ROOT,
            stdout=log,
            stderr=subprocess.STDOUT,
            check=True,
            timeout=480,
        )
    for name in ("store-audio.wav", "store-music.wav", "introspect.json", "automation_result.json"):
        (output / name).write_bytes(device("exec-out", "run-as", args.package, "cat", "files/" + name))
    bank = device(
        "exec-out",
        "run-as",
        args.package,
        "cat",
        "files/imported/sets/macplay_audio_test/descent.rsrc",
    )
    if len(bank) != 1328290 or hashlib.sha256(bank).hexdigest() != (
        "d680bac0ddf87d8f6585597026e8c2a5426614125d1d191488212fb2cf752e59"
    ):
        raise RuntimeError("Launcher did not retain the complete MacPlay resource fork")
    state = json.loads((output / "introspect.json").read_text())
    audio = state["audio"]
    if audio["effects_volume"] != 8 or audio["effects_channel_volume"] != 20 or audio["sfx_probe_count"] < 2:
        raise RuntimeError("Effects did not reach the calibrated mixer")
    peaks = {}
    for kind in ("audio", "music"):
        with wave.open(str(output / ("store-" + kind + ".wav"))) as wav:
            if wav.getsampwidth() != 2 or wav.getframerate() != 48000 or wav.getnchannels() != 2:
                raise RuntimeError("Unexpected capture format")
            pcm = array.array("h", wav.readframes(wav.getnframes()))
            if sys.byteorder != "little":
                pcm.byteswap()
            peaks[kind] = max(map(abs, pcm))
            if kind == "audio" and (len(pcm) < 3 * 48000 * 2 or peaks[kind] < 100):
                raise RuntimeError("MacPlay effects capture is silent or incomplete")
    if peaks["music"] != 0:
        raise RuntimeError("Music must be silent so it cannot mask missing effects")
    report = {
        "result": "PASS",
        "resource_bytes": len(bank),
        "resource_sha256": hashlib.sha256(bank).hexdigest(),
        "effects_peak_pcm": peaks["audio"],
        "music_peak_pcm": peaks["music"],
        "effects_volume": audio["effects_volume"],
        "sfx_probe_count": audio["sfx_probe_count"],
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
