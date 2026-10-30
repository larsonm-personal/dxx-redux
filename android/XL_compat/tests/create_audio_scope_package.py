"""Package the derived private fixture for the real Android mission publisher."""

import argparse
import hashlib
from pathlib import Path
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--classic", action="store_true", help="Include the owned classic-resource admission controls")
    parser.add_argument(
        "--static", action="store_true", help="Include the static explosion lifecycle and rendering controls"
    )
    args = parser.parse_args()
    pinned = {
        "missions/xscope.hog": "b472c3f6b74add7af35ec87272c014bf76daf1bfd61d41e4ad3ade100796fd8c",
        "missions/xscope.mn2": "a7e492541d504bbfd7d303e08feca7c4438545a61242f73f43a237c0f79ecc58",
    }
    if args.static:
        pinned.update(
            {
                "missions/xstatic.hog": "be044e377874dfc832f20df182984801871572e0348dd245a04c6424273acc53",
                "missions/xstatic.mn2": "89abea52944d70bd85ac444c13eecc8f32b94fd0d181efae1875e3238efb6603",
            }
        )
    if args.classic:
        pinned["missions/xbank.hog"] = "b472c3f6b74add7af35ec87272c014bf76daf1bfd61d41e4ad3ade100796fd8c"
        pinned["missions/xbank.mn2"] = "3861555cf5bd670fcf8bd47227b7d913eacdebf34ed0190e4ecbab7ce721d4db"
        pinned["missions/xbkhog.hog"] = "e7f739e0e69c170bfb4c53d4dcd7d9d1629a66b69f261515373e26fd1ae8f15c"
        pinned["missions/xbkhog.mn2"] = "c8ee78d51d1448c3b2b24363277f4f2d1205f38817abe1c051001dabc6b64a68"
        pinned["missions/xhwav.hog"] = "09a1c2863373173d82da98f7d1037f889e965f7b78174325486ef3b72cdf525a"
        pinned["missions/xhwav.mn2"] = "075bf964f2cb5ad17a74ebc660bfee7fd9b51ec12b18b4ea369acec08ce45bfd"
        for name in ("xwav", "xwbad", "xsbank"):
            pinned[f"missions/{name}.hog"] = "6544b4ea235166c6131b5c1ab9bee83c57f9d2c6933898d1a73e7a3f9bdaee3b"
        pinned.update(
            {
                "missions/xwav.mn2": "cbce5dc0f03dae1bb4b21672002c2bdbd8341fa7df58fd24a589ad62e33658ef",
                "missions/xwbad.mn2": "74365db3cc3bbc21e36fcdf5d266a65ff1fe16b9dd8a3d101addb1de9046c3fc",
                "missions/xsbank.mn2": "a9770fdd4d2894bca6ce84c78f9fb62d31af7af5454ed385063d2fcc93479f14",
            }
        )
    for name, expected in pinned.items():
        if hashlib.sha256((args.fixture / name).read_bytes()).hexdigest() != expected:
            raise ValueError(f"Derived scoped fixture differs: {name}")
    names = list(pinned)
    root = "mods/xscope/sounds/"
    names += [root + name for name in ("scoped.wav", "mission.wav", "shared.wav")]
    for folder in ("level00", "level01", "level02", "slevel01"):
        names += [root + folder + "/" + name for name in ("scoped.wav", "shared.wav")]
    if args.classic:
        names += ["mods/xwav/sounds/laser01.wav", "mods/xwbad/sounds/laser01.wav", "mods/xsbank/xsbank.s22"]
        bank_root = "mods/xbank/"
        names += [bank_root + "xbank.s22", bank_root + "xbank.s11"]
        names += [bank_root + "sounds/" + name for name in ("scoped.wav", "mission.wav", "shared.wav")]
        for folder in ("", "level01/", "level02/", "slevel01/"):
            names.append(bank_root + "sounds/" + folder + "laser01.wav")
            if folder:
                names += [bank_root + "sounds/" + folder + name for name in ("scoped.wav", "shared.wav")]
        for mission in ("xbkhog", "xhwav"):
            root_path = "mods/" + mission + "/sounds/"
            names += [root_path + name for name in ("scoped.wav", "mission.wav", "laser01.wav")]
            for folder in ("level01/", "level02/", "slevel01/"):
                names.append(root_path + folder + "scoped.wav")
            if mission == "xhwav":
                names.append(root_path + "level02/laser01.wav")
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
        if args.classic:
            # The same independent labels exercise ordinary replacements through
            # the existing normal/secret scope and disk/rewind automation
            for folder in ("", "level01/", "level02/", "slevel01/"):
                source = args.fixture / (root + folder + "scoped.wav")
                info = zipfile.ZipInfo(root + folder + "laser01.wav", date_time=(1980, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                archive.writestr(info, source.read_bytes())
                names.append(info.filename)
    print(f"Packaged {len(names)} scoped resources: {args.output}")


if __name__ == "__main__":
    main()
