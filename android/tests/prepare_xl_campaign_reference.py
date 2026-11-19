"""Prepare a separate source tree for observing original D2X-XL mission selection and gameplay.

Build the printed tree with the upstream CMake instructions. Its ordinary main
menu is preserved unless a qualification run supplies XL_REFERENCE_DESCRIPTOR.
"""

import argparse
from pathlib import Path
import shutil
import subprocess

REVISION = "2576a98b5de310e3ac7c9777df2677d1627b8bae"


def replace_once(path, old, new):
    data = path.read_bytes()
    newline = b"\r\n" if b"\r\n" in data else b"\n"
    anchor = old.encode().replace(b"\n", newline)
    if data.count(anchor) != 1:
        raise ValueError(f"Expected one original source anchor in {path}")
    path.write_bytes(data.replace(anchor, new.encode().replace(b"\n", newline)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Local original D2X-XL Git repository")
    parser.add_argument("destination", type=Path, help="New, disposable directory")
    args = parser.parse_args()
    source, target = args.source.resolve(), args.destination.resolve()
    if target.exists() or target.is_relative_to(source):
        parser.error("Supply a new directory outside the original source tree")
    subprocess.run(["git", "-C", source, "cat-file", "-e", REVISION + "^{commit}"], check=True)
    subprocess.run(["git", "clone", "--quiet", "--no-hardlinks", "--no-checkout", source, target], check=True)
    subprocess.run(["git", "-C", target, "checkout", "--quiet", "--detach", REVISION], check=True)
    header = Path(__file__).resolve().parents[1] / "XL_compat/tests/campaign_reference.h"
    shutil.copyfile(header, target / "main/campaign_reference.h")
    entry = target / "main/descent.cpp"
    declaration = "int32_t SDLCALL main (int32_t argc, char *argv[])"
    replace_once(entry, declaration, '#include "campaign_reference.h"\n' + declaration)
    replace_once(
        entry,
        "DoSelectPlayer ();\nCreateSoundThread",
        """if (campaign_requested ()) {
    strcpy (LOCALPLAYER.callsign, "xlprobe");
    DefaultAllSettings (false);
} else DoSelectPlayer ();
CreateSoundThread""",
    )
    replace_once(
        entry,
        "#if !DBG\nDUKickstarterNotification ();",
        "if (campaign_requested ()) return campaign_reference ();\n#if !DBG\nDUKickstarterNotification ();",
    )
    replace_once(
        target / "gameio/mission.cpp",
        "i = Parse (cf);",
        "extern void xl_reference_descriptor (const char *, const char *);\nxl_reference_descriptor (szFolder, szFile);\ni = Parse (cf);",
    )
    anchor = "if (m_nResult == 0)\n\t\tcontinue;"
    replace_once(
        target / "main/game.cpp", anchor, anchor + "\n\textern void xl_reference_frame ();\n\txl_reference_frame ();"
    )
    print(target)


if __name__ == "__main__":
    main()
