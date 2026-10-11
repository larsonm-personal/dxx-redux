#!/usr/bin/env python3
"""Independently compare published PCM with the original WAVs and stock sound names.

Uses Python's WAV decoder and a reference implementation of the documented linear
resampling. This establishes sample rate, duration and byte fidelity, not listening
quality or actual mixer consumption (covered separately by the Android tests).
"""

import argparse
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess
import uuid
import wave
import zipfile


def identity(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def stock_names(path, game):
    data = path.read_bytes()
    if game == "d1":
        start = struct.unpack_from("<I", data)[0]
        bitmaps, count = struct.unpack_from("<II", data, start)
        table = start + 8 + bitmaps * 17
    else:
        if data[:8] != b"DSND\x01\0\0\0":
            raise ValueError(f"Not a retail D2 sound bank: {path}")
        count = struct.unpack_from("<I", data, 8)[0]
        table = 12
    if count > 1000 or table + count * 20 > len(data):
        raise ValueError(f"Invalid sound table: {path}")
    return {
        data[table + index * 20 : table + index * 20 + 8].split(b"\0")[0].decode("ascii").casefold()
        for index in range(count)
    }


def reference_pcm(path):
    with wave.open(str(path), "rb") as source:
        channels, width, rate, frames = (
            source.getnchannels(),
            source.getsampwidth(),
            source.getframerate(),
            source.getnframes(),
        )
        raw = source.readframes(frames)
    if not frames or len(raw) != frames * width * channels or width not in (1, 2, 3):
        raise ValueError(f"Unsupported or incomplete WAV: {path}")
    values = [
        (raw[i] - 128) / 128
        if width == 1
        else int.from_bytes(raw[i : i + width], "little", signed=True) / (1 << (width * 8 - 1))
        for i in range(0, len(raw), width)
    ]
    mono = [sum(values[i : i + channels]) / channels for i in range(0, len(values), channels)]
    ratio = rate / 22050
    output = bytearray()
    for index in range(math.floor(frames / ratio)):
        position = index * ratio
        first = math.floor(position)
        fraction = position - first
        value = mono[first] + (mono[min(first + 1, frames - 1)] - mono[first]) * fraction
        output.append(max(0, min(255, round(value * 127 + 128))))
    return bytes(output), {"rate": rate, "channels": channels, "bits": width * 8, "frames": frames}


def audit_game(sources, pack, names):
    rows = []
    with zipfile.ZipFile(pack) as archive:
        entries = {name.casefold(): name for name in archive.namelist()}
        if len(entries) != len(archive.namelist()) or archive.testzip():
            raise ValueError(f"Duplicate or corrupt archive entries: {pack}")
        sounds = {Path(name).stem for name in entries if name.endswith((".r22", ".raw"))}
        for path in sorted(sources.glob("*.wav")):
            expected, source = reference_pcm(path)
            member = entries.get(f"sounds/{path.stem.casefold()}.r22")
            published = archive.read(member) if member else b""
            rows.append(
                {
                    "name": path.stem,
                    "source": source,
                    "expected_bytes": len(expected),
                    "published_bytes": len(published),
                    "matches": bool(member) and published == expected,
                    "expected_sha256": hashlib.sha256(expected).hexdigest(),
                    "published_sha256": hashlib.sha256(published).hexdigest() if member else None,
                }
            )
        if not rows:
            raise ValueError(f"No source WAV files: {sources}")
        return {
            "filename": pack.name,
            "sha256": identity(pack),
            "stock_sounds": len(names),
            "matching_names": sorted(names & sounds),
            "unused_names": sorted(sounds - names),
            "stock_without_replacement": sorted(names - sounds),
            "pack_without_source": sorted(sounds - {row["name"].casefold() for row in rows}),
            "sources": rows,
        }


def main():
    root = Path(__file__).resolve().parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--packs", type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument("--archive", type=Path, default=Path(__file__).resolve().parent / "d2x-xl/hires-sounds.7z")
    parser.add_argument("--stock", type=Path, default=root / "game_data_to_copy_to_emulator/temp")
    parser.add_argument("--seven-zip", default=shutil.which("7z") or "C:/Program Files/7-Zip/7z.exe")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    workspace = root / "android/temp/d2xxl-sound-audit"
    extracted = workspace / ("run_" + uuid.uuid4().hex)
    subprocess.run(
        [
            "pwsh",
            "-NoProfile",
            "-File",
            str(root / "android/helpers/retain-recent-artifacts.ps1"),
            "-Artifacts",
            str(extracted),
            "-DirectoryPrefix",
            "run_",
        ],
        check=True,
        stdout=subprocess.DEVNULL,
    )
    extracted.mkdir(parents=True)
    try:
        subprocess.run(
            [args.seven_zip, "x", str(args.archive), f"-o{extracted}", "-y"], check=True, stdout=subprocess.DEVNULL
        )
        results = []
        for game, bank in (("d1", "DESCENT.PIG"), ("d2", "DESCENT2.S22")):
            sources = extracted / "sounds" / game
            if game == "d2":
                sources /= "44khz" if (sources / "44khz").is_dir() else "22khz"
            result = audit_game(sources, args.packs / f"{game}-hires-sounds.dxa", stock_names(args.stock / bank, game))
            results.append(result)
            matched = sum(row["matches"] for row in result["sources"])
            print(
                f"{game}: {matched}/{len(result['sources'])} PCM matches, "
                f"{len(result['matching_names'])}/{result['stock_sounds']} stock names"
            )
        text = json.dumps({"source_archive_sha256": identity(args.archive), "packs": results}, indent=2) + "\n"
        args.output.parent.mkdir(parents=True, exist_ok=True)
        if not args.output.exists() or args.output.read_text(encoding="utf-8") != text:
            args.output.write_text(text, encoding="utf-8")
        return int(
            any(not row["matches"] for result in results for row in result["sources"])
            or any(result["pack_without_source"] or result["stock_without_replacement"] for result in results)
        )
    finally:
        if extracted.resolve().parent != workspace.resolve():
            raise RuntimeError("Sound audit cleanup escaped its workspace")
        shutil.rmtree(extracted)


if __name__ == "__main__":
    raise SystemExit(main())
