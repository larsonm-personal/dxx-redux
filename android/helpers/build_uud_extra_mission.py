"""Build a clearly marked UUD extra-texture fixture from the author's ordinary D2 mission.

Only texture words and one door clip are changed. Geometry, objects and level
structure remain authored data. This exercises the same level format used by
third-party missions; it is separate from the unmodified author demo acceptance.
"""

import argparse
import io
import json
from pathlib import Path
import struct
import zipfile


def surfaces(data):
    signature, version, mine, game = struct.unpack_from("<4s3i", data)
    if signature != b"LVLP" or version != 7 or not 16 < mine < game < len(data):
        raise ValueError("Fixture expects the author's version-7 D2 level")
    vertices, segments = struct.unpack_from("<HH", data, mine + 1)
    cursor = mine + 5 + vertices * 12
    rows = []
    for segment in range(segments):
        child_mask = data[cursor]
        cursor += 1
        children = [-1] * 6
        for side in range(6):
            if child_mask & (1 << side):
                children[side] = struct.unpack_from("<h", data, cursor)[0]
                cursor += 2
        cursor += 16
        wall_mask = data[cursor]
        cursor += 1
        walls = [-1] * 6
        for side in range(6):
            if wall_mask & (1 << side):
                walls[side] = data[cursor]
                cursor += 1
        for side in range(6):
            if children[side] == -1 or walls[side] >= 0:
                base_offset = cursor
                base = struct.unpack_from("<H", data, cursor)[0]
                cursor += 2
                overlay_offset = cursor if base & 0x8000 else None
                cursor += 2 if overlay_offset else 0
                cursor += 24
                rows.append(
                    {
                        "segment": segment,
                        "side": side,
                        "child": children[side],
                        "wall": walls[side],
                        "base_offset": base_offset,
                        "overlay_offset": overlay_offset,
                    }
                )
    # D2 stores eight-byte segment2 records after the compiled surfaces
    if cursor + segments * 8 != game:
        raise ValueError("Compiled mine size does not match the independently parsed surfaces")
    return rows, game


def patch_level(original):
    data = bytearray(original)
    rows, game = surfaces(data)
    fixtures = []
    # Place the two restored lava effects on known solid sides of the spawn room
    selected = set()
    for texture, location in ((923, (0, 3)), (924, (0, 2))):
        row = next(row for row in rows if (row["segment"], row["side"]) == location)
        if row["child"] != -1 or row["overlay_offset"] is not None:
            raise ValueError("Author spawn room no longer matches lava placement")
        struct.pack_into("<H", data, row["base_offset"], texture)
        selected.add(location)
        fixtures.append({"texture": texture, "segment": location[0], "side": location[1], "layer": "base"})
    for texture in range(910, 928):
        if texture in (923, 924):
            continue
        # Light strips and green door frames are overlays, so their alpha has a base image
        overlay = 910 <= texture <= 922
        row = next(
            row
            for row in rows
            if row["child"] == -1
            and row["wall"] == -1
            and (row["segment"], row["side"]) not in selected
            and (row["overlay_offset"] is not None) == overlay
        )
        offset = row["overlay_offset"] if overlay else row["base_offset"]
        struct.pack_into("<H", data, offset, texture)
        selected.add((row["segment"], row["side"]))
        fixtures.append(
            {
                "texture": texture,
                "segment": row["segment"],
                "side": row["side"],
                "layer": "overlay" if overlay else "base",
            }
        )
    wall_offset, wall_count, wall_size = struct.unpack_from("<3i", data, game + 47)
    if wall_size != 24 or wall_offset < game or wall_offset + wall_count * wall_size > len(data):
        raise ValueError("Unexpected author wall table")
    changed = []
    for wall in range(wall_count):
        offset = wall_offset + wall * wall_size
        segment, side = struct.unpack_from("<2i", data, offset)
        if data[offset + 20] != 43:
            continue
        data[offset + 20] = 51
        row = next(row for row in rows if row["segment"] == segment and row["side"] == side)
        old = struct.unpack_from("<H", data, row["base_offset"])[0]
        struct.pack_into("<H", data, row["base_offset"], (old & 0x8000) | 912)
        changed.append({"segment": segment, "side": side})
    if len(changed) != 2:
        raise ValueError("Expected exactly two sides of the author's spawn door")
    return bytes(data), {"surfaces": fixtures, "door": changed[0]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    hog = (args.source / "pogtest.hog").read_bytes()
    if hog[:3] != b"DHF":
        raise ValueError("Invalid source HOG")
    output = bytearray(b"DHF")
    cursor = 3
    evidence = {}
    while cursor < len(hog):
        header = hog[cursor : cursor + 17]
        name = header[:13].split(b"\0", 1)[0].decode("ascii")
        size = struct.unpack_from("<I", header, 13)[0]
        cursor += 17
        payload = hog[cursor : cursor + size]
        if len(payload) != size:
            raise ValueError("Truncated source HOG")
        cursor += size
        if name.casefold().endswith(".rl2"):
            payload, evidence[name.casefold()] = patch_level(payload)
        output.extend(header)
        output.extend(payload)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    buffer = io.BytesIO()
    descriptor = (args.source / "pogtest.mn2").read_text().replace("name = pogtest", "name = UUD extras")
    with zipfile.ZipFile(buffer, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, payload in (
            ("pogtest.hog", output),
            ("pogtest.mn2", descriptor.encode()),
            ("descent.256", (args.source / "descent.256").read_bytes()),
        ):
            entry = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(entry, payload)
    for path, payload in (
        (args.output, buffer.getvalue()),
        (args.output.with_suffix(".json"), (json.dumps(evidence, indent=2) + "\n").encode()),
    ):
        if not path.exists() or path.read_bytes() != payload:
            path.write_bytes(payload)


if __name__ == "__main__":
    main()
