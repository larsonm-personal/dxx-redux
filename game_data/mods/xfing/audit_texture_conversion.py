#!/usr/bin/env python3
"""Compare source PIG pixels/metadata with converted UUD archives (requires Pillow).

This is an independent decoder, not a replay of converter manifest hashes.
Passing pixel checks does not prove engine lookup, semantic patches or gameplay.
Run from any directory; optional --output writes a stable JSON evidence file.
"""

import argparse
import hashlib
import io
import json
import struct
import zipfile
from collections import Counter
from dataclasses import dataclass
from pathlib import Path

from PIL import Image


@dataclass
class Bitmap:
    index: int
    name: str
    width: int
    height: int
    flags: int
    average: int
    pixels: bytes


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def decode_pixels(data: bytes, offset: int, width: int, height: int, flags: int) -> bytes:
    if not 0 < width * height <= 16 * 1024 * 1024:
        raise ValueError("PIG bitmap dimensions exceed audit bounds")
    if not flags & 8:
        result = data[offset : offset + width * height]
        if len(result) != width * height:
            raise ValueError("Truncated PIG pixels")
        return result
    end = offset + u32(data, offset)
    size_width = 2 if flags & 32 else 1
    cursor = offset + 4 + height * size_width
    result = bytearray()
    for row in range(height):
        row_size = int.from_bytes(
            data[offset + 4 + row * size_width : offset + 4 + (row + 1) * size_width],
            "little",
        )
        row_end = cursor + row_size
        if row_end > min(end, len(data)):
            raise ValueError("RLE row exceeds bitmap")
        decoded = bytearray()
        terminated = False
        while cursor < row_end:
            value = data[cursor]
            cursor += 1
            if value & 0xE0 != 0xE0:
                decoded.append(value)
            elif value == 0xE0:
                terminated = True
                break
            else:
                if cursor == row_end:
                    raise ValueError("Truncated RLE run")
                decoded.extend([data[cursor]] * (value & 31))
                cursor += 1
        if not terminated or cursor != row_end or len(decoded) != width:
            raise ValueError("Invalid RLE row length or terminator")
        result.extend(decoded)
    if cursor != end:
        raise ValueError("RLE rows do not consume bitmap")
    return bytes(result)


def read_pig(path: Path, game: str) -> tuple[bytes, list[Bitmap]]:
    data = path.read_bytes()
    if game == "d1":
        start = u32(data, 0)
        count, sounds = struct.unpack_from("<II", data, start)
        table, stride = start + 8, 17
        payload = table + count * stride + sounds * 20
        prefix = data[4:start]
    else:
        if data[:8] != b"PPIG\x02\x00\x00\x00":
            raise ValueError(f"Not a retail D2 PIG: {path}")
        count = u32(data, 8)
        table, stride = 12, 18
        payload = table + count * stride
        prefix = b""
    if count > 10000 or payload > len(data):
        raise ValueError("PIG table exceeds audit bounds")
    bitmaps = []
    for index in range(count):
        pos = table + index * stride
        name = data[pos : pos + 8].split(b"\0", 1)[0].decode("ascii")
        dflags, width, height = data[pos + 8 : pos + 11]
        if dflags & 64:
            name += f"#{dflags & 63}"
        if game == "d1":
            width += 256 if dflags & 128 else 0
            flags, average = data[pos + 11 : pos + 13]
        else:
            extra, flags, average = data[pos + 11 : pos + 14]
            width += (extra & 15) << 8
            height += (extra & 240) << 4
        offset = payload + u32(data, pos + stride - 4)
        bitmaps.append(
            Bitmap(
                index + 1,
                name,
                width,
                height,
                flags,
                average,
                decode_pixels(data, offset, width, height, flags),
            )
        )
    return prefix, bitmaps


def hog_entry(path: Path, name: str) -> bytes:
    data = path.read_bytes()
    if data[:3] != b"DHF":
        raise ValueError("Invalid HOG signature")
    pos = 3
    while pos + 17 <= len(data):
        entry = data[pos : pos + 13].split(b"\0", 1)[0].decode("ascii")
        size = u32(data, pos + 13)
        pos += 17
        if pos + size > len(data):
            raise ValueError("Truncated HOG entry")
        if entry.casefold() == name.casefold():
            return data[pos : pos + size]
        pos += size
    raise ValueError(f"Missing {name} in {path}")


def rgba(bitmap: Bitmap, palette: bytes) -> tuple[bytes, bytes]:
    colors = []
    masks = []
    for color in range(256):
        rgb = bytes(min(255, c * 4) for c in palette[color * 3 : color * 3 + 3])
        if color == 255 and bitmap.flags & 1:
            colors.append(b"\0\0\0\0")
        elif color == 254 and bitmap.flags & 2:
            colors.append(bytes((120, 88, 128, 0)))
        else:
            colors.append(rgb + b"\xff")
        masks.append(bytes((0, 0, 0, 255)) if color == 254 and bitmap.flags & 2 else b"\xff" * 4)
    return b"".join(colors[p] for p in bitmap.pixels), b"".join(masks[p] for p in bitmap.pixels)


def read_pog(data: bytes) -> dict[int, Bitmap]:
    if data[:8] != b"DPOG\x01\x00\x00\x00":
        raise ValueError("Invalid indexed overlay header")
    count = u32(data, 8)
    payload = 12 + count * 20
    cursor = payload
    result = {}
    for row in range(count):
        index = struct.unpack_from("<H", data, 12 + row * 2)[0]
        header = 12 + count * 2 + row * 18
        name = data[header : header + 8].split(b"\0")[0].decode("ascii")
        animation, width, height, high, flags, average, offset = struct.unpack_from("<6BI", data, header + 8)
        if animation & 64:
            name += f"#{animation & 63}"
        width |= (high & 15) << 8
        height |= (high >> 4) << 8
        if flags & ~7 or index in result or payload + offset != cursor:
            raise ValueError("Noncanonical indexed overlay table")
        pixels = decode_pixels(data, cursor, width, height, flags)
        result[index] = Bitmap(index, name, width, height, flags, average, pixels)
        cursor += width * height
    if cursor != len(data):
        raise ValueError("Unconsumed indexed overlay bytes")
    return result


def audit_pig(
    base: Path | None,
    source: Path,
    game: str,
    palette: bytes,
    archive: zipfile.ZipFile,
    rows: list[dict],
) -> dict:
    prefix, bitmaps = read_pig(source, game)
    base_prefix, originals = read_pig(base, game) if base else (b"", [])
    records = {row["patchIndex"]: row for row in rows}
    issues = []
    flags = []
    renames = []
    counts = Counter()
    archive_names = set(archive.namelist())
    pog_path = "patches/d1/bitmaps.pog" if game == "d1" else f"patches/d2/sets/{source.stem.lower()}.pog"
    indexed = read_pog(archive.read(pog_path)) if pog_path in archive_names else {}
    if set(indexed) != set(records):
        issues.append({"problem": "indexed overlay entries differ from converted textures", "path": pog_path})
    for bitmap in bitmaps:
        original = originals[bitmap.index - 1] if bitmap.index <= len(originals) else None
        changed = bitmap != original
        if original and bitmap.flags & 7 != original.flags & 7:
            flags.append(
                {
                    "index": bitmap.index,
                    "name": bitmap.name,
                    "base": original.flags & 7,
                    "patch": bitmap.flags & 7,
                }
            )
        if original and bitmap.name != original.name:
            renames.append({"index": bitmap.index, "base": original.name, "patch": bitmap.name})
        counts["uncompared" if base is None else "extra" if not original else "changed" if changed else "same"] += 1
        record = records.get(bitmap.index)
        if not record:
            if changed:
                issues.append(
                    {
                        "index": bitmap.index,
                        "name": bitmap.name,
                        "problem": "source change absent from package",
                    }
                )
            continue
        runtime_name = original.name if original else f"idx{bitmap.index}"
        expected_indexed = Bitmap(
            bitmap.index,
            runtime_name,
            bitmap.width,
            bitmap.height,
            bitmap.flags & 7,
            bitmap.average,
            bitmap.pixels,
        )
        if indexed.get(bitmap.index) != expected_indexed:
            issues.append({"index": bitmap.index, "problem": "indexed pixels or semantic metadata differ from source"})
        else:
            counts["verified_indexed_bitmaps"] += 1
        if Path(record["path"]).stem.lower() != runtime_name.lower():
            issues.append(
                {
                    "index": bitmap.index,
                    "path": record["path"],
                    "problem": "image name differs from runtime bitmap name",
                    "runtime_name": runtime_name,
                }
            )
        expected, mask = rgba(bitmap, palette)
        paths = [(record["path"], expected)]
        if bitmap.flags & 2 and 254 in bitmap.pixels:
            paths.append((record.get("maskPath"), mask))
        for path, pixels in paths:
            if path not in archive_names:
                issues.append(
                    {
                        "index": bitmap.index,
                        "path": path,
                        "problem": "missing image or mask",
                    }
                )
                continue
            with Image.open(io.BytesIO(archive.read(path))) as image:
                if image.size != (bitmap.width, bitmap.height) or image.convert("RGBA").tobytes() != pixels:
                    issues.append(
                        {
                            "index": bitmap.index,
                            "path": path,
                            "problem": "pixels differ from source",
                        }
                    )
                else:
                    counts["verified_images"] += 1
    prefix_differences = [i + 4 for i, (a, b) in enumerate(zip(base_prefix, prefix)) if a != b]
    return {
        "pig": source.name,
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "baseline_available": base is not None,
        "counts": dict(sorted(counts.items())),
        "source_metadata_prefix_sizes": [len(base_prefix), len(prefix)],
        "changed_source_metadata_byte_offsets": prefix_differences,
        "runtime_flag_changes": flags,
        "renames": renames,
        "issues": issues,
    }


def main() -> int:
    root = Path(__file__).resolve().parents[3]
    workspace = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, default=root / "game_data_to_copy_to_emulator/temp")
    parser.add_argument("--packs", type=Path, default=workspace / "dxx_tp/tmp/plain_texture_dxa")
    parser.add_argument("--output", type=Path)
    parser.add_argument(
        "--runtime-expectations", type=Path, help="Write source-derived indexed bitmap expectations for native fixtures"
    )
    parser.add_argument(
        "--d2-descent-baseline",
        type=Path,
        default=workspace / "dxx_tp/baseline-d2-descent/original-2015/DESCENT.PIG",
    )
    args = parser.parse_args()
    report = {
        "scope": "Source pixels and metadata inventory; runtime acceptance is separate",
        "games": {},
    }
    runtime = {}
    for game in ("d1", "d2"):
        source_root = workspace / f"dxx_tp/uu{game}tp"
        with zipfile.ZipFile(args.packs / f"uu{game}tp-textures.dxa") as archive:
            manifest = json.loads(archive.read("metadata/manifest.json"))
            summaries = (
                manifest[game]["texturePigs"]
                if game == "d1"
                else json.loads(archive.read("metadata/d2/texture_summary.json"))
            )
            if isinstance(summaries, dict):
                summaries = [summaries]
            by_pig = {s["pig"].lower(): s["textures"] for s in summaries}
            pigs = ["descent"] if game == "d1" else ["groupa", "alien1", "alien2", "fire", "ice", "water", "descent"]
            results = []
            runtime[game] = {}
            for pig in pigs:
                source = next(p for p in source_root.iterdir() if p.name.lower() == pig + ".pig")
                baseline = (
                    args.baseline / (pig.upper() + ".PIG")
                    if game == "d1" or pig != "descent"
                    else args.d2_descent_baseline
                )
                if baseline is not None and not baseline.exists():
                    baseline = None
                palette = (
                    (source_root / "descent.256").read_bytes()
                    if game == "d2" and pig == "descent"
                    else hog_entry(
                        args.baseline / ("DESCENT.HOG" if game == "d1" else "DESCENT2.HOG"),
                        "palette.256" if game == "d1" else pig + ".256",
                    )
                )
                results.append(
                    audit_pig(
                        baseline,
                        source,
                        game,
                        palette,
                        archive,
                        by_pig.get(pig + ".pig", []),
                    )
                )
                if args.runtime_expectations:
                    _, originals = read_pig(baseline, game)
                    _, patched = read_pig(source, game)
                    runtime[game][pig] = [
                        {
                            "index": b.index,
                            "name": originals[b.index - 1].name if b.index <= len(originals) else f"idx{b.index}",
                            "width": b.width,
                            "height": b.height,
                            "flags": b.flags & 7,
                            "average": b.average,
                            "sha256": hashlib.sha256(b.pixels).hexdigest(),
                        }
                        for b in patched
                        if b.index > len(originals) or b != originals[b.index - 1]
                    ]
            report["games"][game] = results
    if args.runtime_expectations:
        expected = (json.dumps(runtime, indent=2) + "\n").encode()
        args.runtime_expectations.parent.mkdir(parents=True, exist_ok=True)
        if not args.runtime_expectations.exists() or args.runtime_expectations.read_bytes() != expected:
            args.runtime_expectations.write_bytes(expected)
    content = (json.dumps(report, indent=2, ensure_ascii=True) + "\n").encode()
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        if not args.output.exists() or args.output.read_bytes() != content:
            args.output.write_bytes(content)
    for game, pigs in report["games"].items():
        for pig in pigs:
            print(
                f"{game}/{pig['pig']}: {pig['counts']}, {len(pig['issues'])} image issues, {len(pig['runtime_flag_changes'])} flag changes, {len(pig['changed_source_metadata_byte_offsets'])} metadata bytes changed"
            )
    return int(any(pig["issues"] for pigs in report["games"].values() for pig in pigs))


if __name__ == "__main__":
    raise SystemExit(main())
