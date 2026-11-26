"""Build a separate Forest ZIP from an extracted, unmodified frozen levelpack.7z.

Pass the pack's missions directory and a new output ZIP path. The recovered HOG
retains the original 633-segment level and appends the publisher's forest.pog.
"""

import argparse
import hashlib
import io
from pathlib import Path
import struct
import zipfile

from dos_midi_capture import extract_member

FOREST_DESCRIPTOR = "7ab68e2f4db7a45eb09834b661c4ebe50a5f03a85c76267e02d9c58cb724b985"
FOREST_HOG = "640bb5e72e100350db10e06fdcad1e1ce70a14dd656ec05fd12c4851975ec1ed"
MONOLEVEL_POG = "fe251efe163fbd23edd3061a66fba49aa9b8345d7153b21c5e5ec2418645e705"
FOREST_POG = "12f6fc1ee3efc13a60098e610314895a2c524653e509ee750a37bc7e999a8408"
# Registry order from the author's Polylevel release and the publisher's 16-player edition.
FOREST_BITMAP_IDS = (
    1243,
    1289,
    1290,
    1295,
    1326,
    1329,
    1331,
    1332,
    1333,
    1334,
    1342,
    1343,
    1364,
    1367,
    1394,
    1415,
    1434,
)


def verify(data, expected, name):
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError(f"{name} differs from the verified release")


def select_bitmaps(bank, selected):
    # The verified Monolevel bank stores one contiguous payload per directory
    # row in increasing offset order. Retain bitmap metadata and pixels verbatim.
    count = struct.unpack_from("<I", bank, 8)[0]
    indices = struct.unpack_from(f"<{count}H", bank, 12)
    directory = 12 + 2 * count
    pixels = directory + 18 * count
    headers = [bank[pos : pos + 18] for pos in range(directory, pixels, 18)]
    offsets = [struct.unpack_from("<I", header, 14)[0] for header in headers] + [len(bank) - pixels]
    output_headers = bytearray()
    output_pixels = bytearray()
    for ordinal, bitmap in enumerate(selected):
        row = indices.index(bitmap)
        # DLE's generated bitmap labels are numbered within each exported POG.
        output_headers.extend(f"POG{ordinal:04d}".encode("ascii") + b"\0")
        output_headers.extend(headers[row][8:14])
        output_headers.extend(struct.pack("<I", len(output_pixels)))
        output_pixels.extend(bank[pixels + offsets[row] : pixels + offsets[row + 1]])
    return bank[:8] + struct.pack(f"<I{len(selected)}H", len(selected), *selected) + output_headers + output_pixels


def recover(missions, output):
    descriptor = (missions / "diedel" / "forest.mn2").read_bytes()
    verify(descriptor, FOREST_DESCRIPTOR, "forest.mn2")
    hog = (missions / "diedel" / "forest.hog").read_bytes()
    verify(hog, FOREST_HOG, "633-segment forest.hog")
    monolevel = extract_member(missions / "single" / "dinter.hog", "dinter.pog")
    verify(monolevel, MONOLEVEL_POG, "dinter.pog")
    forest = select_bitmaps(monolevel, FOREST_BITMAP_IDS)
    verify(forest, FOREST_POG, "Recovered forest.pog")
    repaired = hog + struct.pack("<13si", b"forest.pog", len(forest)) + forest

    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w") as archive:
        for filename, content in (("forest.mn2", descriptor), ("forest.hog", repaired)):
            # Fixed metadata also makes repeated private builds comparable.
            info = zipfile.ZipInfo(filename)
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            archive.writestr(info, content, compress_type=zipfile.ZIP_DEFLATED)
    with output.open("xb") as stream:
        try:
            stream.write(buffer.getbuffer())
            stream.flush()
        except BaseException:
            stream.close()
            output.unlink()
            raise


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("missions", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        recover(args.missions, args.output)
    except (OSError, ValueError) as exc:
        parser.exit(1, f"{exc}\n")
    print(f"Created {args.output} with forest.pog SHA-256 {FOREST_POG}")
