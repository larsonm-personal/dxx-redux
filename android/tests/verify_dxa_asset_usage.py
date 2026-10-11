"""Compare consumed engine PCM against the actual installed DXA payloads."""

import argparse
import json
import re
import zipfile
from pathlib import Path


def fingerprint(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return f"{value:016x}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("packs", type=Path)
    parser.add_argument("--modes", nargs="+", default=["d1", "d2", "d1-in-d2", "mac-d1"])
    args = parser.parse_args()
    for mode in args.modes:
        snapshots = [
            json.loads((args.directory / f"{mode}-{phase}.json").read_text(encoding="utf-8-sig"))
            for phase in ("off", "on", "off-again")
        ]
        baseline, enabled, restored = [item["mod_usage"] for item in snapshots]
        assert not baseline["sounds"] and not restored["sounds"], f"{mode}: disabled sound overlays remain"
        pack = "d2-hires-sounds.dxa" if mode == "d2" else "d1-hires-sounds.dxa"
        if mode == "mac-d1":
            pack = "d1xr-mac-demo-sounds.dxa"
        sounds = enabled["sounds"]
        assert len(sounds) >= 50, f"{mode}: expected substantial sound coverage, got {len(sounds)}"
        assert any(sound["played"] for sound in sounds), f"{mode}: no replacement reached playback"
        trace = (args.directory / f"{mode}-on.log").read_text(encoding="utf-8-sig")
        assert trace.count("Injecting key: lctrl") >= 2, f"{mode}: mission re-entry playback was not exercised"
        after_reload = trace.rsplit("Injecting key: lctrl", 1)[1]
        played_after_reload = False
        with zipfile.ZipFile(args.packs / pack) as archive:
            for sound in sounds:
                assert Path(sound["source"]).name == pack, f"{mode}: wrong sound pack: {sound}"
                name = sound["file"].removeprefix("d1-mods/")
                data = archive.read(name)
                assert sound["resident"] and sound["hash"] == fingerprint(data), f"{mode}: wrong resident PCM: {name}"
                assert sound["bytes"] == len(data)
                assert sound["rate"] == (22050 if name.endswith(".r22") else 11025)
                if sound["played"]:
                    pattern = (
                        rf"sample={sound['sample']} channel=\d+ input_hash={sound['hash']} "
                        rf"input_bytes={len(data)} input_valid=1 bank_match=1 "
                        r"cache_input_match=1 cache_output_match=1"
                    )
                    assert re.search(pattern, trace), f"{mode}: missing matching mixer playback for {name}"
                    played_after_reload |= re.search(pattern, after_reload) is not None
        assert played_after_reload, f"{mode}: no matching replacement playback after mission re-entry"
        images = [row for row in enabled["presentation"] if Path(row["source"]).name == "d1xr-hires.dxa"]
        if mode != "d2":
            assert any(row["kind"] == "font" for row in images), f"{mode}: no pack fonts loaded"
            assert any(row["kind"] == "image" and row["width"] >= 640 for row in images), (
                f"{mode}: no hires presentation image"
            )
        else:
            assert not images, "D1 presentation pack leaked into D2"
        for phase in (baseline, restored):
            assert not any(Path(row["source"]).name == "d1xr-hires.dxa" for row in phase["presentation"])

        # Off-on-off proves actual baseline playback returns, independently of overlay bookkeeping
        def playback(phase):
            trace = (args.directory / f"{mode}-{phase}.log").read_text(encoding="utf-8-sig")
            return set(re.findall(r"sample=(\d+) channel=\d+ input_hash=([0-9a-f]+) input_bytes=(\d+)", trace))

        if mode == "mac-d1":
            assert not playback("off") and not playback("off-again"), "Mac fixture retained sounds without its pack"
        assert mode == "mac-d1" or playback("off") & playback("off-again"), (
            f"{mode}: no matching baseline playback after disabling"
        )
        assert mode == "mac-d1" or any(
            sample == original_sample and (value, size) != (original_value, original_size)
            for sample, value, size in playback("on")
            for original_sample, original_value, original_size in playback("off")
        ), f"{mode}: enabled playback did not differ from the original sounds"
        print(f"{mode}: {len(sounds)} matching resident samples; playback and off/on/off restoration verified")


if __name__ == "__main__":
    main()
