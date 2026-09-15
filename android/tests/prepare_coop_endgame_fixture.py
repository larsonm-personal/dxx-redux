"""Create a one-level D2 ending fixture and optionally extract an owned ending movie."""

import argparse
from pathlib import Path
import struct


def encode_line(line):
    # Inverse of the two rotations and XOR in d2/main/text.c::decode_text_line
    def rotate_right(value):
        return (value >> 1) | ((value & 1) << 7)

    return bytes(rotate_right(rotate_right(value) ^ 0xD3) for value in line)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--movie-library", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "coopend.mn2").write_text(
        "name = Co-op ending fixture\ntype = normal\nnum_levels = 1\nd2leva-1.rl2\n",
        encoding="ascii",
    )
    text = (
        "$S2\n$D 1 end01.pcx 2 0 10 10 300 180\n$U 1\n$C1\n"
        "Mission complete\n$P\nEach pilot finishes independently\n$S\n"
    ).encode("ascii")
    credits = b"\n".join(
        encode_line(line)
        for line in [b"$Co-op ending fixture", b"*Mission complete", b"Independent credits", b"Thank you for playing"]
    ) + b"\n"
    with (args.output / "coopend.hog").open("wb") as stream:
        stream.write(b"DHF")
        for name, data in [(b"coopend.tex", text), (b"coopend.ctb", credits)]:
            stream.write(name.ljust(13, b"\0") + struct.pack("<I", len(data)) + data)

    if args.movie_library:
        with args.movie_library.open("rb") as stream:
            if stream.read(4) != b"DMVL":
                raise ValueError("Expected a Descent movie library")
            count = struct.unpack("<I", stream.read(4))[0]
            entries = [
                (stream.read(13).split(b"\0")[0].lower(), struct.unpack("<I", stream.read(4))[0])
                for _ in range(count)
            ]
            for name, size in entries:
                if name == b"end.mve":
                    data = stream.read(size)
                    if len(data) != size:
                        raise ValueError("Truncated ending movie")
                    (args.output / "end.mve").write_bytes(data)
                    break
                stream.seek(size, 1)
            else:
                raise ValueError("The movie library does not contain end.mve")


if __name__ == "__main__":
    main()
