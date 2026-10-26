"""Pinned original XL inputs, using the existing bounded corpus extractor."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "helpers"))
from retail_ci_assets import file_identity, reject_links
from nightly_corpus_stage import extract_archive, member_path

ASSETS_COMMIT = "8c9988a96a7e306f8aac04c85e2610492917b7ac"
MANIFEST_SHA256 = "cfa5a81f003958798cb37ee177f07b74c77812c4ab43d1986ccff37244347647"
OWNER_IDS = {"host-profile:retail-xl-originals", "master:test_xl_original_assets"}
D2_FILES = {
    "d2-xl.hog": (
        9871510,
        "7491c45d217831509a840d934641bf1791e029ae6c1bcbe3e4efba2847d53d32",
    ),
    "d2-xl.mn2": (
        514,
        "cfd619db7eff6a37663188905aec1a1c96d50567613d07ac1dcfd97b621bb8ea",
    ),
}
PAIRS = {
    (11, 31),
    (15, 33),
    (15, 34),
    (17, 35),
    (17, 36),
    (18, 36),
    (19, 40),
    (21, 40),
    (23, 40),
}


def selected(plan):
    return any(e["id"] in OWNER_IDS for e in plan["tests"])


def available(identity):
    return bool(identity and identity.get("commit") == ASSETS_COMMIT)


def checked(path, record):
    actual = file_identity(path)
    if actual != (record["size"], record["sha256"]):
        raise ValueError("XL fixture identity mismatch")


def manifest(path):
    if file_identity(path)[1] != MANIFEST_SHA256:
        raise ValueError("XL fixture manifest identity mismatch")
    data = json.loads(Path(path).read_text())
    if (
        data["missing"]
        or data["mismatched"]
        or {(x["level_version"], x["game_version"]) for x in data["format_witnesses"]}
        != PAIRS
    ):
        raise ValueError("XL fixture manifest is incomplete")
    return data


def hog_member(path, name):
    reject_links(path)
    if path.stat().st_size > 32 * 1024**2:
        raise ValueError("XL HOG exceeds fixture bound")
    data = path.read_bytes()
    if data[:3] != b"DHF":
        raise ValueError("XL fixture HOG signature differs")
    offset, found, seen = 3, None, set()
    while offset < len(data):
        if len(data) - offset < 17:
            raise ValueError("XL fixture HOG is truncated")
        raw, size = struct.unpack_from("<13si", data, offset)
        key = raw.split(b"\0")[0].decode("ascii")
        member_path(key)
        if key.casefold() in seen or size < 0 or size > len(data) - offset - 17:
            raise ValueError("XL fixture HOG member is invalid")
        seen.add(key.casefold())
        offset += 17
        if key == name:
            found = data[offset : offset + size]
        offset += size
    if found is None:
        raise ValueError("XL fixture HOG member missing")
    return found


def expected_files(data):
    result = {}
    folded = {}
    for archive in data["archives"]:
        adapter = archive["path"].endswith("/adapters/boilpnt-test.zip")
        for row in archive["members"]:
            path = ("adapter/" if adapter else "") + member_path(row["path"]).as_posix()
            if path.casefold() in folded and folded[path.casefold()] != path:
                raise ValueError("XL fixture members have case-colliding paths")
            folded[path.casefold()] = path
            if path in result and result[path] != (row["size"], row["sha256"]):
                raise ValueError("XL fixture members conflict")
            result[path] = (row["size"], row["sha256"])
    for name, identity in D2_FILES.items():
        result["missions/single/" + name] = identity
    return result


def verify(stage):
    stage = Path(stage)
    reject_links(stage)
    data = manifest(stage / "manifest.json")
    checked(stage / "levelpack.7z", data["archive"])
    for name, (size, digest) in expected_files(data).items():
        actual = file_identity(stage / name)
        if actual[1] != digest or (size and actual[0] != size):
            raise ValueError("XL staged member identity mismatch")
    for row in data["format_witnesses"]:
        content = hog_member(stage / row["archive_member"], row["hog_member"])
        if (
            len(content) != row["size"]
            or hashlib.sha256(content).hexdigest() != row["sha256"]
        ):
            raise ValueError("XL format witness identity mismatch")
        if len(content) < 16 or struct.unpack_from("<I", content)[0] != 0x504C564C:
            raise ValueError("XL format witness header differs")
        level, _, game_offset = struct.unpack_from("<iii", content, 4)
        if not 16 <= game_offset <= len(content) - 4:
            raise ValueError("XL format witness offset differs")
        signature, game = struct.unpack_from("<HH", content, game_offset)
        if signature != 0x6705 or (level, game) != (
            row["level_version"],
            row["game_version"],
        ):
            raise ValueError("XL format witness version differs")
        checked(stage / "witnesses" / row["hog_member"], row)
    return data


def stage_inputs(root, destination, seven_zip):
    root, destination = Path(root), Path(destination)
    reject_links(root)
    reject_links(destination)
    data = manifest(root / "missions/d2xxl/manifest.json")
    if destination.exists():
        raise ValueError("XL fixture destination already exists")
    destination.mkdir(parents=True)
    try:
        shutil.copyfile(
            root / "missions/d2xxl/manifest.json", destination / "manifest.json"
        )
        checked(root / data["archive"]["path"], data["archive"])
        shutil.copyfile(root / data["archive"]["path"], destination / "levelpack.7z")
        for index, record in enumerate(data["archives"]):
            source = root / member_path(record["path"])
            checked(source, record)
            temporary = destination / ("unpack-" + str(index))
            extract_archive(source, temporary)
            actual = {
                p.relative_to(temporary).as_posix()
                for p in temporary.rglob("*")
                if p.is_file()
            }
            if actual != {r["path"] for r in record["members"]}:
                raise ValueError("XL archive member set differs")
            adapter = record["path"].endswith("/adapters/boilpnt-test.zip")
            for row in record["members"]:
                path = member_path(row["path"])
                checked(temporary / path, row)
                target = destination / ("adapter" if adapter else "") / path
                target.parent.mkdir(parents=True, exist_ok=True)
                if target.exists():
                    checked(target, row)
                else:
                    shutil.copyfile(temporary / path, target)
            shutil.rmtree(temporary)
        record = next(
            r
            for r in data["original_mission_archives"]
            if r["path"].endswith("/D2-XL.7z")
        )
        source = root / member_path(record["path"])
        checked(source, record)
        temporary = destination / "unpack-d2"
        extract_archive(source, temporary, seven_zip=seven_zip)
        for name, (_, digest) in D2_FILES.items():
            matches = [
                p
                for p in temporary.rglob("*")
                if p.is_file() and p.name.casefold() == name
            ]
            if len(matches) != 1 or file_identity(matches[0])[1] != digest:
                raise ValueError("XL original Counterstrike package differs")
            target = destination / "missions/single" / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(matches[0], target)
        shutil.rmtree(temporary)
        (destination / "witnesses").mkdir()
        for row in data["format_witnesses"]:
            (destination / "witnesses" / row["hog_member"]).write_bytes(
                hog_member(destination / row["archive_member"], row["hog_member"])
            )
        verify(destination)
    except BaseException:
        shutil.rmtree(destination)
        raise


def require_original_gpu_trace(path):
    # Four visits in each rebuilt GL context include original A and return-to-A
    rows = json.loads(Path(path).read_text())
    if not isinstance(rows, list) or len(rows) != 8:
        raise ValueError("Required original GPU witness count differs")
    seen = set()
    for row in rows:
        context, visit = row.get("context"), row.get("visit")
        if (
            type(context) is not int
            or type(visit) is not int
            or context not in (0, 1)
            or visit not in range(4)
        ):
            raise ValueError("Required original GPU witness identity differs")
        identity = (context, visit)
        if identity in seen:
            raise ValueError("Required original GPU witness is duplicated")
        seen.add(identity)
        owner = ("a", "b", "stock", "a")[visit]
        original = owner == "a"
        if (
            row.get("owner") != owner
            or row.get("correct") is not True
            or row.get("original") is not original
        ):
            raise ValueError("Required original GPU witness did not execute correctly")
        if original and (
            row.get("red") != 228
            or row.get("path") != "mods/pilot/textures/CEIL002.TGA"
        ):
            raise ValueError("Required original GPU source or pixel differs")
    if seen != {(context, visit) for context in (0, 1) for visit in range(4)}:
        raise ValueError("Required original GPU witness is missing")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--stage", type=Path)
    mode.add_argument("--gpu-trace", type=Path)
    parser.add_argument("--seven-zip")
    args = parser.parse_args()
    if args.gpu_trace:
        require_original_gpu_trace(args.gpu_trace)
        print("Verified four original GPU witnesses across two GL contexts")
        return
    if args.root:
        stage_inputs(args.root, args.stage, args.seven_zip)
    else:
        verify(args.stage)
    print("Verified pinned XL original fixtures and nine format witnesses")


if __name__ == "__main__":
    main()
