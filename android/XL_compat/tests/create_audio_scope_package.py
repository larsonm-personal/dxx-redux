"""Package the derived private fixture for the real Android mission publisher."""

import argparse
import hashlib
from pathlib import Path
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    pinned = {
        "missions/xscope.hog": "b472c3f6b74add7af35ec87272c014bf76daf1bfd61d41e4ad3ade100796fd8c",
        "missions/xscope.mn2": "a7e492541d504bbfd7d303e08feca7c4438545a61242f73f43a237c0f79ecc58",
    }
    for name, expected in pinned.items():
        if hashlib.sha256((args.fixture / name).read_bytes()).hexdigest() != expected:
            raise ValueError(f"Derived scoped fixture differs: {name}")
    names = list(pinned)
    root = "mods/xscope/sounds/"
    names += [root + name for name in ("scoped.wav", "mission.wav", "shared.wav")]
    for folder in ("level00", "level01", "level02", "slevel01"):
        names += [root + folder + "/" + name for name in ("scoped.wav", "shared.wav")]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "w") as archive:
        for name in sorted(names):
            source = args.fixture / name
            if source.is_symlink() or not source.is_file():
                raise ValueError(f"Scoped fixture is not a regular file: {name}")
            info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, source.read_bytes())
    print(f"Packaged {len(names)} scoped resources: {args.output}")


if __name__ == "__main__":
    main()
