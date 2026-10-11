"""Verify actual launcher-imported mod consumption across fresh engine sessions."""

import argparse
import json
import re
import zipfile
from pathlib import Path

from verify_dxa_asset_usage import fingerprint


def verify(directory, packs, mode):
    content_game = "d2" if mode == "d2" else "d1"
    uud = f"uu{content_game}tp-textures.dxa"
    hires = f"{content_game}-hires-256-textures-ktx2.dxa"
    for phase in ("baseline", "uud", "uud-first", "hires-first", "off"):
        state = json.loads((directory / f"{mode}-{phase}.json").read_text(encoding="utf-8-sig"))
        trace = (directory / f"{mode}-{phase}.log").read_text(encoding="utf-8-sig")
        assert state["in_game"] and state["current_level_num"] == 1, (mode, phase, "not in level")
        usage = state["mod_usage"]
        textures = {row["name"]: row for row in state["texture_packs"]["packs"]}
        patches = [row for row in usage["presentation"] if row["kind"] in ("metadata", "bitmap_patch")]
        combined = phase in ("uud-first", "hires-first")
        if phase in ("baseline", "off"):
            assert not textures and not patches and not usage["sounds"], (mode, phase, "disabled mod remains")
        else:
            assert any(row["kind"] == "metadata" for row in patches), (mode, phase, "no semantic patch")
            assert any(row["kind"] == "bitmap_patch" and Path(row["source"]).name == uud for row in patches), (
                mode,
                phase,
                "no indexed UUD pixels",
            )
            # A higher-priority high-res pack can cover every visible UUD image;
            # its indexed/semantic patches must still be applied
            if phase != "hires-first":
                assert textures.get(uud, {}).get("loaded", 0) > 0, (mode, phase, "no rendered UUD replacements")
            if combined:
                assert textures.get(hires, {}).get("mipmapped", 0) > 0, (mode, phase, "no mipmapped high-res textures")
            else:
                assert hires not in textures and not usage["sounds"], (mode, phase, "unrequested pack loaded")
        art = [row for row in usage["presentation"] if Path(row["source"]).name == "d1xr-hires.dxa"]
        if combined and mode != "d2":
            assert any(row["kind"] == "font" for row in art), (mode, phase, "no replacement font")
            assert any(row["kind"] == "image" and row["width"] >= 640 for row in art), (mode, phase, "no menu image")
        else:
            assert not art, (mode, phase, "D1 presentation leaked")
        if combined:
            sound_pack = f"{content_game}-hires-sounds.dxa"
            assert len(usage["sounds"]) >= 50, (mode, phase, "insufficient sound coverage")
            assert trace.count("Injecting key: lctrl") >= 2, (mode, phase, "mission re-entry not exercised")
            after_reload = trace.rsplit("Injecting key: lctrl", 1)[1]
            played = False
            with zipfile.ZipFile(packs / sound_pack) as archive:
                for sound in usage["sounds"]:
                    assert Path(sound["source"]).name == sound_pack, (mode, phase, "wrong sound source")
                    data = archive.read(sound["file"].removeprefix("d1-mods/"))
                    assert sound["resident"] and sound["hash"] == fingerprint(data)
                    assert sound["bytes"] == len(data)
                    if sound["played"]:
                        pattern = (
                            rf"sample={sound['sample']} channel=\d+ input_hash={sound['hash']} "
                            rf"input_bytes={len(data)} input_valid=1 bank_match=1 "
                            r"cache_input_match=1 cache_output_match=1"
                        )
                        played |= re.search(pattern, after_reload) is not None
            assert played, (mode, phase, "no matching mixer consumption after re-entry")
        print(f"{mode} {phase}: imported textures, semantic patches, presentation and PCM verified")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("packs", type=Path)
    parser.add_argument("--modes", nargs="+", default=["d1", "d2", "d1-in-d2"])
    args = parser.parse_args()
    for mode in args.modes:
        verify(args.directory, args.packs, mode)


if __name__ == "__main__":
    main()
