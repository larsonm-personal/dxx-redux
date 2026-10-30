"""Compare original classic WAV selections with independent DSND and PCM readers."""

import argparse
from array import array
import hashlib
import json
from pathlib import Path
import struct
import sys
import wave


def bank_names(path):
    data = path.read_bytes()
    if len(data) < 12 or data[:4] != b"DSND":
        raise ValueError("Original sound bank signature differs")
    version, count = struct.unpack_from("<II", data, 4)
    if version != 1 or not 0 < count <= 256 or len(data) < 12 + count * 20:
        raise ValueError("Original sound bank header differs")
    names = []
    for index in range(count):
        name, length, stored, offset = struct.unpack_from("<8sIII", data, 12 + index * 20)
        name = name.split(b"\0", 1)[0].decode("ascii")
        if not name or name in names or "/" in name or "\\" in name or stored != length:
            raise ValueError("Original sound bank name or length differs")
        if length == 0 or 12 + count * 20 + offset + length > len(data):
            raise ValueError("Original sound sample exceeds its bank")
        names.append(name)
    return names


def wave_receipt(path):
    with wave.open(str(path), "rb") as source:
        channels, width, rate, frames = (
            source.getnchannels(),
            source.getsampwidth(),
            source.getframerate(),
            source.getnframes(),
        )
        if channels not in (1, 2) or width not in (1, 2) or source.getcomptype() != "NONE" or frames <= 0:
            raise ValueError("Original WAV exceeds independent PCM profile")
        pcm = source.readframes(frames)
    if len(pcm) != frames * channels * width:
        raise ValueError("Short original WAV samples")
    if width == 1:
        words = array("h", ((byte - 128) * 256 for byte in pcm))
        if sys.byteorder != "little":
            words.byteswap()
        pcm = words.tobytes()
    return {
        "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "pcm_sha256": hashlib.sha256(pcm).hexdigest(),
        "rate": rate,
        "channels": channels,
        "bits": width * 8,
        "frames": frames,
    }


def output_receipt(path, output_rate):
    """Independent device-timestamp interpolation, without production helpers."""
    with wave.open(str(path), "rb") as source:
        channels, width, rate, frames = (
            source.getnchannels(),
            source.getsampwidth(),
            source.getframerate(),
            source.getnframes(),
        )
        raw = source.readframes(frames)
    if width == 1:
        words = array("h", ((byte - 128) * 256 for byte in raw))
    else:
        words = array("h", raw)
        if sys.byteorder != "little":
            words.byteswap()
    count = max(1, (frames * output_rate + rate // 2) // rate)
    if rate == output_rate and channels == 2:
        output = words
    else:
        output = array("h")
        for frame in range(count):
            whole, fraction = divmod(frame * rate, output_rate)
            first, second = min(whole, frames - 1), min(whole + 1, frames - 1)
            for channel in range(2):
                offset = channel if channels == 2 else 0
                numerator = words[first * channels + offset] * (output_rate - fraction)
                numerator += words[second * channels + offset] * fraction
                output.append(numerator // output_rate if numerator >= 0 else -((-numerator) // output_rate))
    if sys.byteorder != "little":
        output.byteswap()
    return {"frames": count, "pcm_sha256": hashlib.sha256(output.tobytes()).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bank", type=Path, required=True)
    parser.add_argument("--sounds", type=Path, required=True)
    parser.add_argument("--identity", required=True)
    parser.add_argument("--levels", type=int, required=True)
    parser.add_argument("--waves", type=int, required=True)
    parser.add_argument("--selections", type=int, required=True)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--receipts", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not 0 < args.levels <= 9999 or args.waves <= 0 or args.selections <= 0:
        parser.error("Exact positive original coverage counts are required")
    names = bank_names(args.bank)
    expected, source_rows, files = {}, [], {}
    root = args.sounds.resolve()
    for ordinal in range(1, args.levels + 1):
        rows = []
        for sample, name in enumerate(names):
            candidates = (root / f"level{ordinal:02d}" / (name + ".wav"), root / (name + ".wav"))
            selected = next((path for path in candidates if path.exists()), None)
            if selected is None:
                continue
            if selected.is_symlink() or not selected.is_file() or not selected.resolve().is_relative_to(root):
                raise ValueError("Original WAV escaped its package")
            relative = selected.relative_to(root).as_posix()
            if relative not in files:
                files[relative] = wave_receipt(selected)
            rows.append(
                {"sample": sample, "name": name, "path": f"mods/{args.identity}/sounds/{relative}", **files[relative]}
            )
            source_rows.append((ordinal, sample, name, relative, selected.stat().st_size))
        expected[str(ordinal)] = rows
    actual_source = []
    for line in args.reference.read_text(encoding="utf-8-sig").splitlines():
        if not line.startswith("CLASSIC_SELECTION\t"):
            continue
        _, ordinal, sample, name, path, size = line.split("\t")
        actual_source.append((int(ordinal), int(sample), name, path, int(size)))
    if sorted(source_rows) != sorted(actual_source):
        raise ValueError("Pinned XL source selection differs from independent bank/folder selection")
    if len(files) != args.waves or len(source_rows) != args.selections:
        raise ValueError("Original replacement coverage differs")
    converted = {}
    for rate in (44100, 48000):
        native = json.loads((args.receipts / f"classic-resources-{rate}.json").read_text())
        if native != expected:
            raise ValueError(f"Production original resources or decoded PCM differ at output profile {rate}")
        outputs = {relative: output_receipt(root / relative, rate) for relative in files}
        converted[str(rate)] = outputs
        playback = {}
        for ordinal, rows in expected.items():
            playback[ordinal] = [
                {
                    "sample": row["sample"],
                    "path": row["path"],
                    **outputs[row["path"].removeprefix(f"mods/{args.identity}/sounds/")],
                }
                for row in rows
            ]
        native_playback = json.loads((args.receipts / f"classic-playback-{rate}.json").read_text())
        if native_playback != playback:
            raise ValueError(f"Complete original bank or converted playback PCM differs at {rate} Hz")
    report = {
        "bank_sha256": hashlib.sha256(args.bank.read_bytes()).hexdigest(),
        "bank_names": len(names),
        "levels": args.levels,
        "selections": len(source_rows),
        "distinct_replacements": len(files),
        "resources": files,
        "prepared_output": converted,
        "playback_qualified": False,
    }
    encoded = (json.dumps(report, indent=2, sort_keys=True) + "\n").encode()
    if not args.output.exists() or args.output.read_bytes() != encoded:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(encoded)
    print(
        f"PASS: independent original bank, pinned source and both production profiles agree on {len(source_rows)} selections and {len(files)} decoded/converted WAVs; original level playback unqualified"
    )


if __name__ == "__main__":
    main()
