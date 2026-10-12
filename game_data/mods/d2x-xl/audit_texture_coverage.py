#!/usr/bin/env python3
"""Audit released texture names, animation coverage and renderer eligibility against stock PIGs.

Uses the independent PIG decoder from the UUD audit. Missing assets are not assumed
to be conversion errors: compare these findings with source artwork before repair.
This checks runtime eligibility, not visual equivalence of lossy ETC2 pixels.
"""

import argparse
from collections import defaultdict
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import zipfile

# Stock muzzle frames confirmed as non-rod, non-wall vclips by the GLES fixture
# The engine resolves live game data; this offline stock audit accepts only these
ADDITIVE_SPRITES = {f"muzl{group:02}#{frame}" for group in (2, 3, 5, 6, 8, 9) for frame in range(3)}


def audit_pack(path, bitmaps, other_bitmaps=()):
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    stock = {bitmap.name.casefold(): bitmap for bitmap in bitmaps}
    other_names = {bitmap.name.casefold() for bitmap in other_bitmaps}
    animations = defaultdict(set)
    for name in stock:
        if "#" in name:
            animations[name.rsplit("#", 1)[0]].add(name)
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if len({name.casefold() for name in names}) != len(names):
            raise ValueError(f"Duplicate archive paths: {path}")
        entries = {Path(name).stem.casefold(): name for name in names if name.endswith(".ktx2")}
        masks = {Path(name).stem.casefold() for name in names if name.endswith("_mask.png")}
        reachable = set(entries) & stock.keys()
        rejected = []
        additive = []
        for name in sorted(reachable):
            bitmap = stock[name]
            with archive.open(entries[name]) as stream:
                header = stream.read(80)
            if header[:12] != b"\xabKTX 20\xbb\r\n\x1a\n":
                raise ValueError(f"Invalid KTX2 header: {path}:{entries[name]}")
            vk_format = struct.unpack_from("<I", header, 12)[0]
            actual_alpha = bool(bitmap.flags & 1 and 255 in bitmap.pixels)
            actual_cutout = bool(bitmap.flags & 2 and 254 in bitmap.pixels)
            reasons = []
            marker = entries[name] + ".blend"
            sprite_additive = False
            if marker in names:
                if name in ADDITIVE_SPRITES and archive.read(marker) == b"additive\n":
                    sprite_additive = True
                    additive.append(name)
                else:
                    reasons.append("invalid or unverified sprite blend marker")
            # Match the current renderer's conservative alpha guard
            if bitmap.flags & 3 and vk_format == 147 and not sprite_additive:
                reasons.append("RGB-only replacement for transparency-flagged bitmap")
            if actual_cutout and name + "_mask" not in masks:
                reasons.append("missing owned supertransparency mask")
            if reasons:
                rejected.append(
                    {
                        "name": name,
                        "reasons": reasons,
                        "source_has_transparent_pixels": actual_alpha,
                        "source_has_cutout_pixels": actual_cutout,
                    }
                )
        unmatched = set(entries) - stock.keys()
        # Describe naming evidence, not inferred support for XL-specific effects
        other_game = unmatched & other_names
        surplus_frames = {
            name for name in unmatched - other_game if "#" in name and name.rsplit("#", 1)[0] in animations
        }
        unsplit_animations = {name for name in unmatched - other_game if name in animations}
        return {
            "filename": path.name,
            "sha256": digest,
            "bytes": path.stat().st_size,
            "image_entries": len(entries),
            "stock_named_entries": len(reachable),
            "unmatched_names": sorted(unmatched),
            "unmatched_categories": {
                "other_game_bitmap_names": sorted(other_game),
                "surplus_animation_frames": sorted(surplus_frames),
                "unframed_animation_names": sorted(unsplit_animations),
                "other_names": sorted(unmatched - other_game - surplus_frames - unsplit_animations),
            },
            "partial_animations": {
                group: sorted(names - reachable)
                for group, names in sorted(animations.items())
                if names & reachable and names - reachable
            },
            "rejected_replacements": rejected,
            "additive_sprite_entries": additive,
        }


def main():
    root = Path(__file__).resolve().parents[3]
    spec = importlib.util.spec_from_file_location(
        "uud_source_decoder", root / "game_data/mods/xfing/audit_texture_conversion.py"
    )
    decoder = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = decoder
    spec.loader.exec_module(decoder)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--packs", type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument("--stock", type=Path, default=root / "game_data_to_copy_to_emulator/temp")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--games", nargs="+", choices=("d1", "d2"), default=["d1", "d2"])
    parser.add_argument("--sizes", nargs="+", type=int, choices=(128, 256, 512), default=[128, 256, 512])
    args = parser.parse_args()
    results = []
    stock_bitmaps = {
        game: decoder.read_pig(args.stock / pig, game)[1] for game, pig in (("d1", "DESCENT.PIG"), ("d2", "GROUPA.PIG"))
    }
    variants = {"d1": {"descent": stock_bitmaps["d1"]}, "d2": {"groupa": stock_bitmaps["d2"]}}
    if "d2" in args.games:
        # Eligibility depends on each palette PIG's flags and indexed cutout pixels
        for palette in ("alien1", "alien2", "fire", "ice", "water"):
            variants["d2"][palette] = decoder.read_pig(args.stock / f"{palette.upper()}.PIG", "d2")[1]
    for game in args.games:
        for size in args.sizes:
            for palette, bitmaps in variants[game].items():
                result = audit_pack(
                    args.packs / f"{game}-hires-{size}-textures-ktx2.dxa",
                    bitmaps,
                    stock_bitmaps["d2" if game == "d1" else "d1"],
                )
                result["palette"] = palette
                results.append(result)
                print(
                    f"{result['filename']} ({palette}): {result['stock_named_entries']} stock names, "
                    f"{len(result['rejected_replacements'])} rejected replacements, "
                    f"{len(result['partial_animations'])} partial animation groups"
                )
    text = json.dumps({"packs": results}, indent=2) + "\n"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != text:
        args.output.write_text(text, encoding="utf-8")
    return int(any(result["rejected_replacements"] or result["partial_animations"] for result in results))


if __name__ == "__main__":
    raise SystemExit(main())
