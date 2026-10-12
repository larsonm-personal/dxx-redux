#!/usr/bin/env python3
"""Validate mod packs; optionally publish or update the existing GitHub release."""

# Run from the repository root with Python 3.11+ and authenticated GitHub CLI:
# python game_data/mods/d2x-xl/prepare_texture_release.py --publish
# Without --publish, regenerate RELEASE.md and metadata locally only
# Existing archives are validated and uploaded without reconversion or recompression
# Replacement uploads are not atomic; rerun --publish after an interruption
# This assets-only release stays separate from the latest Android APK release
# Runtime acceptance still requires an updated APK and in-game verification:
# import/enable packs, restart the game session, then inspect Video Info for
# actual texture use and Mods > Mod Details for sound/image/font usage
# Updated release assets do not replace copies already imported into the app
# Remove and re-download installed packs to use their updated contents

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import zipfile

REPOSITORY = "larsonm-personal/dxx-redux"
TAG = "texture-packs-v1"  # Keep published download URLs stable
TITLE = "D1 and D2 texture, sound and presentation packs v1"
EXTRA_PACKS = (
    ("d1-hires-sounds.dxa", "sounds", ["d1", "d1-in-d2"]),
    ("d2-hires-sounds.dxa", "sounds", ["d2"]),
    ("d1xr-hires.dxa", "menus_and_fonts", ["d1", "d1-in-d2", "mac-d1"]),
    ("d1xr-mac-demo-sounds.dxa", "missing_mac_sounds", ["mac-d1"]),
)
EXTRA_DESCRIPTIONS = {
    "uud2sp-sounds.dxa": (
        "UUD2SP Sound Fixes and Restorations",
        "D2 retail",
        "Restores robot and door sounds and fixes effects' playback rates. "
        "Place above D2 High-Res Sounds to retain these effects "
        "(higher mods in the list mask matching assets in lower ones)",
    ),
    "uud1tp-textures.dxa": (
        "UUD1 Texture Fixes",
        "D1 PC retail and D1-in-D2",
        "Fixes original-resolution textures, selected level surfaces and door timing. "
        "Place above high-res packs to retain its corrected artwork "
        "(higher mods in the list mask matching assets in lower ones)",
    ),
    "uud2tp-textures.dxa": (
        "UUD2 Texture Fixes and Extras",
        "D2 retail",
        "Fixes original-resolution textures and restores extra doors, lava and surfaces for custom missions. "
        "Place above high-res packs to retain its corrected artwork "
        "(higher mods in the list mask matching assets in lower ones)",
    ),
    "d1-hires-sounds.dxa": (
        "D1 High-Res Sounds",
        "D1 and D1-in-D2",
        "Higher-quality 22 kHz sound effects. Can be combined with texture and menu packs",
    ),
    "d2-hires-sounds.dxa": (
        "D2 High-Res Sounds",
        "D2",
        "Higher-quality 22 kHz sound effects. Can be combined with a texture pack",
    ),
    "d1xr-hires.dxa": (
        "D1 High-Res Menus and Fonts",
        "D1 and D1-in-D2",
        "Higher-resolution fonts and 640x480 menu and briefing images. Does not replace level textures",
    ),
    "d1xr-mac-demo-sounds.dxa": (
        "Mac D1 Missing Sounds",
        "Mac D1 game data in the D1 engine",
        "Restores missing sound effects. Not needed for PC data and does not support Mac data in D1-in-D2",
    ),
}
CREDITS = (
    "Credits: D1 source textures and sounds by DizzyRox, Novacron and Aus-RED-5; "
    "D2 source textures and sounds by Aus-RED-5D, DizzyRox, MetalBeast, Novacron and Theftbot, "
    "from the D2X-XL packs. D1 presentation and Mac sound add-ons are from D1X-Rebirth by Zico. "
    "UUD1TP, UUD2TP and UUD2SP are by Xfing. "
    "Original archive README files are preserved"
)


def release_notes(packs: list[dict]) -> str:
    rows = [
        "Original level textures are 64x64 pixels. **256px texture packs are recommended.**",
        "",
        "| Download | Applies to | Description |",
        "| --- | --- | --- |",
    ]
    texture_descriptions = {
        "128": "Replacement wall and object textures with lower memory and storage use",
        "256": "Sharper wall and object textures. Recommended balance of detail and memory use",
        "512": "Detailed wall and object textures with higher memory and storage use",
    }
    for pack in packs:
        filename = pack["filename"]
        if pack["kind"] == "textures":
            game, _, resolution, *_ = filename.split("-")
            name = f"{game.upper()} High-Res Level Textures ({resolution}px)"
            applies_to = "D1 and D1-in-D2" if game == "d1" else "D2"
            description = texture_descriptions[resolution]
        else:
            name, applies_to, description = EXTRA_DESCRIPTIONS[filename]
        url = f"https://github.com/{REPOSITORY}/releases/download/{TAG}/{filename}"
        rows.append(f"| [{name}]({url}) | {applies_to} | {description} |")
    return "\n".join(rows) + "\n\n" + CREDITS + "\n"


def file_identity(path: Path) -> dict:
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    return {"filename": path.name, "bytes": path.stat().st_size, "sha256": digest}


def validate_archive(archive: zipfile.ZipFile) -> None:
    if archive.testzip() is not None:
        raise ValueError(f"Corrupt archive: {archive.filename}")
    names = archive.namelist()
    if len(names) != len(set(name.casefold() for name in names)):
        raise ValueError(f"Duplicate paths: {archive.filename}")
    for name in names:
        parts = name.rstrip("/").split("/")
        if any(part in ("", ".", "..") for part in parts) or "\\" in name or ":" in name:
            raise ValueError(f"Invalid archive path: {archive.filename}:{name}")
    if not any(name.casefold() in ("readme.md", "readme.txt") for name in names):
        raise ValueError(f"Missing credits: {archive.filename}")


def inspect_pack(path: Path) -> dict:
    textures = 0
    mipmapped = 0
    largest = 0
    with zipfile.ZipFile(path) as archive:
        validate_archive(archive)
        names = archive.namelist()
        for name in names:
            if not name.endswith(".ktx2"):
                continue
            data = archive.read(name)
            if data[:12] != b"\xabKTX 20\xbb\r\n\x1a\n" or len(data) < 80:
                raise ValueError(f"Invalid KTX2: {path}:{name}")
            fmt, _, width, height, depth, layers, faces, levels, compression = struct.unpack_from("<9I", data, 12)
            if fmt not in (147, 151) or compression or depth or layers or faces != 1:
                raise ValueError(f"Unsupported KTX2: {path}:{name}")
            if not (0 < width <= 16384 and 0 < height <= 16384 and 0 < levels <= 15):
                raise ValueError(f"Unsupported dimensions: {path}:{name}")
            if len(data) < 80 + levels * 24:
                raise ValueError(f"Truncated mip index: {path}:{name}")
            total_bytes = 0
            for level in range(levels):
                offset, length, raw_length = struct.unpack_from("<3Q", data, 80 + level * 24)
                w, h = max(1, width >> level), max(1, height >> level)
                expected = ((w + 3) // 4) * ((h + 3) // 4) * (16 if fmt == 151 else 8)
                total_bytes += length
                if (
                    length != expected
                    or raw_length != expected
                    or offset < 80 + levels * 24
                    or offset + length > len(data)
                    or total_bytes > 64 * 1024 * 1024
                    or (w == h == 1 and level + 1 < levels)
                ):
                    raise ValueError(f"Invalid authored mip {level}: {path}:{name}")
            textures += 1
            mipmapped += levels > 1
            largest = max(largest, width, height)
    if not textures:
        raise ValueError(f"No KTX2 textures: {path}")
    return {
        **file_identity(path),
        "textures": textures,
        "mipmapped": mipmapped,
        "max_dimension": largest,
    }


def inspect_extra_pack(path: Path, kind: str, modes: list[str]) -> dict:
    sounds = images = fonts = 0
    rates = set()
    with zipfile.ZipFile(path) as archive:
        validate_archive(archive)
        for entry in archive.infolist():
            suffix = Path(entry.filename).suffix.lower()
            if suffix in (".raw", ".r22"):
                if not entry.file_size:
                    raise ValueError(f"Empty sound: {path}:{entry.filename}")
                sounds += 1
                rates.add(22050 if suffix == ".r22" else 11025)
            elif suffix in (".pcx", ".fnt"):
                with archive.open(entry) as stream:
                    header = stream.read(128)
                if suffix == ".pcx":
                    if len(header) < 128 or header[0] != 10 or header[2:4] != b"\x01\x08":
                        raise ValueError(f"Invalid PCX: {path}:{entry.filename}")
                    x0, y0, x1, y1 = struct.unpack_from("<4H", header, 4)
                    if x1 < x0 or y1 < y0:
                        raise ValueError(f"Invalid PCX dimensions: {path}:{entry.filename}")
                    images += 1
                else:
                    if len(header) < 16 or header[:4] != b"PSFN":
                        raise ValueError(f"Invalid font: {path}:{entry.filename}")
                    fonts += 1
        if kind == "menus_and_fonts":
            if not images or not fonts:
                raise ValueError(f"Missing images or fonts: {path}")
        elif not sounds:
            raise ValueError(f"Missing sounds: {path}")
        if kind == "missing_mac_sounds" and "Sounds/sounds.array" not in archive.namelist():
            raise ValueError(f"Missing Mac sound mapping: {path}")
    return {
        **file_identity(path),
        "kind": kind,
        "supported_modes": modes,
        "sounds": sounds,
        "sample_rates": sorted(rates),
        "images": images,
        "fonts": fonts,
    }


def write_if_changed(path: Path, text: str) -> None:
    if not path.exists() or path.read_text(encoding="utf-8") != text:
        path.write_text(text, encoding="utf-8", newline="\n")


def inspect_uud_pack(path: Path, game: str) -> dict:
    # Packaging checks supplement the independent source audit and real-engine acceptance
    with zipfile.ZipFile(path) as archive:
        validate_archive(archive)
        manifest = json.loads(archive.read("metadata/manifest.json"))
        if manifest.get("game") != game or manifest.get("pack") != f"uu{game}tp":
            raise ValueError(f"Wrong UUD manifest: {path}")
        palettes = ["descent"] if game == "d1" else ["alien1", "alien2", "descent", "fire", "groupa", "ice", "water"]
        banks = (
            ["patches/d1/bitmaps.pog"] if game == "d1" else [f"patches/d2/sets/{palette}.pog" for palette in palettes]
        )
        indexed = 0
        for name in banks:
            data = archive.read(name)
            if len(data) < 12 or data[:8] != b"DPOG\x01\x00\x00\x00":
                raise ValueError(f"Missing or invalid indexed bitmap bank: {path}:{name}")
            count = struct.unpack_from("<I", data, 8)[0]
            if not 0 < count <= 4096 or len(data) < 12 + count * 20:
                raise ValueError(f"Truncated indexed bitmap bank: {path}:{name}")
            indexed += count
        patches = (
            ["patches/d1/properties_patch.rfc6902.json", "patches/d1/level_surface_patch.rfc6902.json"]
            if game == "d1"
            else ["patches/d2/ham_patch.rfc6902.json"]
        )
        for name in patches:
            operations = json.loads(archive.read(name))
            if not isinstance(operations, list) or not any(row.get("op") == "test" for row in operations):
                raise ValueError(f"Missing tested semantic patch: {path}:{name}")
            if not any(row.get("op") in ("add", "replace") for row in operations):
                raise ValueError(f"Empty semantic patch: {path}:{name}")
        if not any(name.lower() == f"uu{game}tp.rtf" for name in archive.namelist()):
            raise ValueError(f"Missing original author documentation: {path}")
        textures = 0
        for name in archive.namelist():
            if not name.endswith(".png") or name.endswith("_mask.png"):
                continue
            data = archive.read(name)
            if data[:8] != b"\x89PNG\r\n\x1a\n" or len(data) < 24:
                raise ValueError(f"Invalid texture PNG: {path}:{name}")
            width, height = struct.unpack_from(">2I", data, 16)
            if not 0 < width <= 2048 or not 0 < height <= 2048:
                raise ValueError(f"Unsupported texture dimensions: {path}:{name}")
            textures += 1
        if textures != indexed:
            raise ValueError(f"PNG/indexed bitmap coverage differs: {path}: {textures}/{indexed}")
    return {
        **file_identity(path),
        "kind": "texture_fixes",
        "supported_modes": ["d1", "d1-in-d2"] if game == "d1" else ["d2"],
        "textures": textures,
        "palettes": palettes,
        "semantic_patches": len(patches),
    }


def inspect_uud_sound_pack(path: Path) -> dict:
    with zipfile.ZipFile(path) as archive:
        validate_archive(archive)
        manifest = json.loads(archive.read("metadata/manifest.json"))
        if manifest.get("pack") != "uud2sp" or manifest.get("game") != "d2":
            raise ValueError(f"Wrong UUD2SP manifest: {path}")
        if any(name.lower().endswith(".ham") for name in archive.namelist()):
            raise ValueError(f"Unexpected full HAM override: {path}")
        names_by_rate = []
        for bank in manifest["d2"]["soundFiles"]:
            data = archive.read(bank["path"])
            if len(data) != bank["size"] or hashlib.sha256(data).hexdigest() != bank["sha256"]:
                raise ValueError(f"Sound bank identity mismatch: {path}:{bank['path']}")
            if data[:8] != b"DSND\x01\0\0\0" or len(data) < 12:
                raise ValueError(f"Invalid sound bank: {path}")
            count = struct.unpack_from("<I", data, 8)[0]
            start = 12 + count * 20
            if not 0 < count <= 256 or start > len(data):
                raise ValueError(f"Invalid sound bank headers: {path}")
            names = []
            for index in range(count):
                name, length, _, offset = struct.unpack_from("<8sIII", data, 12 + index * 20)
                if not length or start + offset + length > len(data):
                    raise ValueError(f"Truncated sound sample: {path}:{index}")
                names.append(name.rstrip(b"\0"))
            if len(set(names)) != count:
                raise ValueError(f"Duplicate sound names: {path}")
            names_by_rate.append(names)
        if len(names_by_rate) != 2 or names_by_rate[0] != names_by_rate[1]:
            raise ValueError(f"11/22 kHz bank slots differ: {path}")
        operations = json.loads(archive.read(manifest["d2"]["hamPatchPath"]))
        if not operations or len(operations) != manifest["d2"]["hamPatchOperationCount"]:
            raise ValueError(f"Missing UUD2SP semantic patches: {path}")
        if "UUD2SP.rtf" not in archive.namelist():
            raise ValueError(f"Missing author documentation: {path}")
    return {
        **file_identity(path),
        "kind": "sound_fixes",
        "supported_modes": ["d2"],
        "sounds": count,
        "sample_rates": [11025, 22050],
        "semantic_patches": 1,
    }


def gh(*args: str) -> str:
    # Argument arrays preserve paths with spaces without invoking a shell
    result = subprocess.run(["gh", *args], capture_output=True, text=True, check=False)
    if result.returncode:
        raise RuntimeError(f"GitHub command failed: {result.stderr.strip() or result.stdout.strip()}")
    return result.stdout


def get_release() -> dict | None:
    # Listing also finds drafts; a failed API request must not become 'not found'
    pages = json.loads(gh("api", f"repos/{REPOSITORY}/releases?per_page=100", "--paginate", "--slurp"))
    return next((release for page in pages for release in page if release["tag_name"] == TAG), None)


def asset_matches(asset: dict, expected: dict) -> bool:
    return asset.get("size") == expected["bytes"] and asset.get("digest") == "sha256:" + expected["sha256"]


def publish(files: list[Path], notes: Path, validated_packs: list[dict]) -> None:
    expected = {path.name: file_identity(path) for path in files}
    for pack in validated_packs:
        if any(expected[pack["filename"]][key] != pack[key] for key in ("bytes", "sha256")):
            raise RuntimeError(f"Pack changed after validation: {pack['filename']}; rerun after conversion finishes")
    release = get_release()
    if release is None:
        gh(
            "release",
            "create",
            TAG,
            "--repo",
            REPOSITORY,
            "--draft",
            "--latest=false",
            "--title",
            TITLE,
            "--notes-file",
            str(notes),
        )
        release = get_release()
    if release is None:
        raise RuntimeError("Created release could not be found")
    release_id = release["id"]
    assets = {asset["name"]: asset for asset in release["assets"]}
    changed = [path for path in files if not asset_matches(assets.get(path.name, {}), expected[path.name])]
    print(f"Publishing {len(changed)} changed assets; {len(files) - len(changed)} unchanged")
    if changed:
        gh("release", "upload", TAG, "--repo", REPOSITORY, "--clobber", *(str(path) for path in changed))
    refreshed = get_release()
    if refreshed is None or refreshed["id"] != release_id:
        raise RuntimeError("Release identity changed during publication")
    assets = {asset["name"]: asset for asset in refreshed["assets"]}
    for name, identity in expected.items():
        if not asset_matches(assets.get(name, {}), identity):
            raise RuntimeError(f"Uploaded asset size/SHA-256 mismatch: {name}; rerun --publish to retry")
    body = notes.read_text(encoding="utf-8")
    if refreshed["draft"] or refreshed["name"] != TITLE or (refreshed.get("body") or "").strip() != body.strip():
        gh(
            "release",
            "edit",
            TAG,
            "--repo",
            REPOSITORY,
            "--draft=false",
            "--latest=false",
            "--title",
            TITLE,
            "--notes-file",
            str(notes),
        )
        refreshed = get_release()
        if refreshed is None or refreshed["id"] != release_id or refreshed["draft"]:
            raise RuntimeError("Release publication could not be verified")
    print(f"Verified {len(files)} assets: {refreshed['html_url']}")


def main() -> None:
    source = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=source.parents[2] / "android/temp/hires-release-assets")
    parser.add_argument(
        "--publish",
        action="store_true",
        help="Create/update texture-packs-v1; replace changed assets and verify hashes",
    )
    args = parser.parse_args()
    packs = [
        inspect_pack(source / f"{game}-hires-{size}-textures-ktx2.dxa")
        for game in ("d1", "d2")
        for size in (128, 256, 512)
    ]
    args.output.mkdir(parents=True, exist_ok=True)
    extras = [inspect_extra_pack(source / name, kind, modes) for name, kind, modes in EXTRA_PACKS]
    uud_source = source.parent / "xfing/dxx_tp/tmp/plain_texture_dxa"
    uud_paths = [uud_source / f"uu{game}tp-textures.dxa" for game in ("d1", "d2")]
    uud_packs = [inspect_uud_pack(path, game) for path, game in zip(uud_paths, ("d1", "d2"), strict=True)]
    sound_patch = source.parent / "xfing/d2x_sp/uud2sp-sounds.dxa"
    uud_packs.append(inspect_uud_sound_pack(sound_patch))
    uud_paths.append(sound_patch)
    manifest = {"schema": 1, "tag": TAG, "recommended_resolution": 256, "packs": packs}
    write_if_changed(args.output / "texture-packs.json", json.dumps(manifest, indent=2) + "\n")
    all_packs = (
        [
            {
                **pack,
                "kind": "textures",
                "supported_modes": ["d1", "d1-in-d2"] if pack["filename"].startswith("d1-") else ["d2"],
            }
            for pack in packs
        ]
        + extras
        + uud_packs
    )
    write_if_changed(source / "RELEASE.md", release_notes(all_packs))
    write_if_changed(args.output / "mod-packs.json", json.dumps({**manifest, "packs": all_packs}, indent=2) + "\n")
    write_if_changed(
        args.output / "SHA256SUMS.txt", "".join(f"{pack['sha256']}  {pack['filename']}\n" for pack in all_packs)
    )
    for pack in all_packs:
        print(f"Validated {pack['filename']}: {pack['bytes']} bytes, SHA-256 {pack['sha256']}")
    if args.publish:
        publish(
            [source / pack["filename"] for pack in packs + extras]
            + uud_paths
            + [args.output / name for name in ("texture-packs.json", "mod-packs.json", "SHA256SUMS.txt")],
            source / "RELEASE.md",
            all_packs,
        )


if __name__ == "__main__":
    main()
