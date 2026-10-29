"""Compare native channel-preserving PCM with Python's independent WAV reader."""

import argparse
from array import array
import hashlib
import io
import json
from pathlib import Path
import random
import struct
import subprocess
import sys
import tempfile
import wave


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while data := source.read(1024 * 1024):
            digest.update(data)
    return digest.hexdigest()


def compare(original, converted):
    with wave.open(str(original), "rb") as source:
        rate, channels, width, frames = (
            source.getframerate(),
            source.getnchannels(),
            source.getsampwidth(),
            source.getnframes(),
        )
        if width not in (1, 2) or channels not in (1, 2) or source.getcomptype() != "NONE":
            raise ValueError("Unsupported independent WAV witness")
        samples = source.readframes(frames)
    expected = samples
    if width == 1:
        words = array("h", ((value - 128) * 256 for value in samples))
        if sys.byteorder != "little":
            words.byteswap()
        expected = words.tobytes()
    actual = converted.read_bytes()
    header = struct.pack("<IHHI", rate, channels, width * 8, frames)
    if actual != header + expected:
        raise ValueError("Native authored channels, format or PCM differs: " + original.name)
    return {
        "name": original.name,
        "rate": rate,
        "channels": channels,
        "bits": width * 8,
        "frames": frames,
        "wave_sha256": hashlib.sha256(original.read_bytes()).hexdigest(),
        "pcm_sha256": hashlib.sha256(expected).hexdigest(),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--hog", type=Path)
    parser.add_argument("--sha256")
    parser.add_argument("--expected-waves", type=int)
    args = parser.parse_args()
    if bool(args.hog) != bool(args.sha256) or bool(args.hog) != bool(args.expected_waves):
        parser.error("Original HOG, expected SHA-256 and positive WAV count must be supplied together")
    if args.expected_waves is not None and args.expected_waves <= 0:
        parser.error("Expected WAV count must be positive")
    args.work.mkdir(parents=True, exist_ok=True)
    native = args.native.resolve()
    rows = []
    cases = 0
    rng = random.Random(2047)
    with tempfile.TemporaryDirectory(prefix="channels-", dir=args.work) as scratch:
        root = Path(scratch)
        for channels in (1, 2):
            for width in (1, 2):
                for rate in (8000, 11025, 22050, 44100, 48000, 96000, 192000):
                    pcm = bytes(rng.randrange(256) for _ in range(4099 * channels * width))
                    memory = io.BytesIO()
                    with wave.open(memory, "wb") as target:
                        target.setparams((channels, width, rate, 0, "NONE", "not compressed"))
                        target.writeframes(pcm)
                    raw = memory.getvalue()
                    # RIFF odd-byte padding is outside the data chunk length
                    if len(raw) & 1:
                        raw = raw[:4] + struct.pack("<I", len(raw) - 7) + raw[8:] + b"\0"
                    original, converted = root / "input.wav", root / "output.pcm"
                    original.write_bytes(raw)
                    subprocess.run([str(native), "--channels", str(original), str(converted)], check=True)
                    compare(original, converted)
                    cases += 1
        if cases != 28:
            raise ValueError("Independent PCM case membership changed")
        if args.hog:
            initial = sha256(args.hog)
            if initial != args.sha256:
                raise ValueError("Original audio HOG hash differs")
            staged = root / "original"
            subprocess.run([str(native), "--archive", str(args.hog.resolve()), str(staged)], check=True)
            for original in sorted(staged.glob("*.wav")):
                rows.append(compare(original, original.with_suffix(".wav.pcm")))
            if len(rows) != args.expected_waves or sha256(args.hog) != initial:
                raise ValueError("Original audio comparison missing or source changed")
            report = {"hog_sha256": initial, "originals": rows, "playback_qualified": False}
            report_path = args.work / "original-wave-channels.json"
            encoded = (json.dumps(report, indent=2, sort_keys=True) + "\n").encode()
            if not report_path.exists() or report_path.read_bytes() != encoded:
                report_path.write_bytes(encoded)
    print(f"PASS: {cases} independent mono/stereo PCM cases and {len(rows)} original WAVs; playback unqualified")


if __name__ == "__main__":
    main()
