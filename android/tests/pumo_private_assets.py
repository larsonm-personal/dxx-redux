#!/usr/bin/env python3
"""Trusted, bounded staging of the separately pinned original Pumo archive."""

import argparse
import hashlib
import json
import os
import shutil
import stat
import unicodedata
from pathlib import Path, PurePosixPath

ASSETS_COMMIT = "dc6565f6b5400d7c700fa8378e766b2354b6ceb6"
MANIFEST_SHA256 = "e670d3c67d620f92b4b6991ccfcf09038ce6da846edf163aa1670b5be39f19e0"
ARCHIVE_SHA256 = "82c8cc4cfc740c8169f9d21a961c9ac5d2518342b3e27ac3ac438c91ca8f2ddf"
ARCHIVE_SIZE = 442181363
CAMERA_SHA256 = "e7351d66d534c3fdb400f03b91b771ad2d0c0d2b0100cf48718d504008ada7e6"
CAMERA_SIZE = 3815963
CHUNK_SIZE = 8 * 1024**2
FILE_COUNT = 2672
DIR_COUNT = 51
EXPANDED_BYTES = 1677820774
MAX_ENTRIES = 4096
MAX_EXPANDED = 2 * 1024**3


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024**2), b""):
            value.update(block)
    return value.hexdigest()


def directory(path):
    path = path.absolute()
    if ".." in path.parts:
        raise ValueError("Parent traversal")
    for item in [*reversed(path.parents), path]:
        if not stat.S_ISDIR(item.lstat().st_mode):
            raise ValueError("Linked or non-directory ancestor")


def regular(path):
    directory(path.parent)
    info = path.lstat()
    if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1:
        raise ValueError("Linked or non-regular input")
    return info.st_size


def safe_path(name):
    if (
        not isinstance(name, str)
        or not name
        or len(name) > 1024
        or name.startswith("/")
        or "\\" in name
        or ":" in name
        or any(unicodedata.category(c) in ("Cc", "Cf") for c in name)
        or any(
            part in ("", ".", "..") or part.endswith((" ", ".")) or len(part) > 255
            for part in name.split("/")
        )
    ):
        raise ValueError("Unsafe archive path")
    reserved = {"con", "prn", "aux", "nul"} | {
        f"{prefix}{n}" for prefix in ("com", "lpt") for n in range(1, 10)
    }
    if any(part.split(".", 1)[0].casefold() in reserved for part in name.split("/")):
        raise ValueError("Reserved archive path")
    return PurePosixPath(name)


def unique(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate manifest key")
        result[key] = value
    return result


def reconstruct(root, output):
    """Check every input byte twice and exclusively create the unchanged archive."""
    directory(output.parent)
    if output.exists() or output.is_symlink():
        raise FileExistsError("Archive output already exists")
    directory(root)
    manifest_path = root / "manifest.json"
    if regular(manifest_path) > 65536 or digest(manifest_path) != MANIFEST_SHA256:
        raise ValueError("Pumo manifest pin differs")
    manifest = json.loads(manifest_path.read_text(), object_pairs_hook=unique)
    source = manifest.get("source", {})
    chunks = manifest.get("chunks")
    if (
        set(manifest) != {"profile", "source", "chunk_size", "chunks"}
        or manifest["profile"] != "pumo-full-archive-v1"
        or source.get("size") != ARCHIVE_SIZE
        or source.get("sha256") != ARCHIVE_SHA256
        or source.get("filename") != "pmines_v11.7z"
        or manifest["chunk_size"] != CHUNK_SIZE
        or not isinstance(chunks, list)
        or len(chunks) != (ARCHIVE_SIZE + CHUNK_SIZE - 1) // CHUNK_SIZE
    ):
        raise ValueError("Pumo archive manifest shape differs")
    expected = {"manifest.json"}
    for index, item in enumerate(chunks):
        path = f"chunks/pmines_v11.7z.{index:03d}"
        size = min(CHUNK_SIZE, ARCHIVE_SIZE - index * CHUNK_SIZE)
        if (
            set(item) != {"path", "size", "sha256"}
            or item["path"] != path
            or type(item["size"]) is not int
            or item["size"] != size
            or not isinstance(item["sha256"], str)
            or len(item["sha256"]) != 64
        ):
            raise ValueError("Pumo chunk inventory differs")
        expected.add(path)
    found = set()
    for base, dirs, files in os.walk(root, followlinks=False):
        for name in dirs:
            path = Path(base) / name
            if path.relative_to(root).as_posix() != "chunks":
                raise ValueError("Unexpected Pumo fixture directory")
            directory(path)
        for name in files:
            path = Path(base) / name
            regular(path)
            found.add(path.relative_to(root).as_posix())
    if found != expected:
        raise ValueError("Missing or extra Pumo fixture file")

    def stream(destination=None):
        whole = hashlib.sha256()
        total = 0
        for item in chunks:
            path = root / item["path"]
            if regular(path) != item["size"]:
                raise ValueError("Pumo chunk size differs")
            part = hashlib.sha256()
            count = 0
            with path.open("rb") as source_file:
                for block in iter(lambda: source_file.read(1024**2), b""):
                    count += len(block)
                    total += len(block)
                    if count > item["size"] or total > ARCHIVE_SIZE:
                        raise ValueError("Pumo archive size budget exceeded")
                    part.update(block)
                    whole.update(block)
                    if destination is not None:
                        destination.write(block)
            if count != item["size"] or part.hexdigest() != item["sha256"]:
                raise ValueError("Pumo chunk hash differs")
        if total != ARCHIVE_SIZE or whole.hexdigest() != ARCHIVE_SHA256:
            raise ValueError("Pumo original archive hash differs")

    stream()
    destination = output.open("xb")
    try:
        with destination:
            stream(destination)
    except BaseException:
        if output.exists():
            output.unlink()
        raise


def inventory(reader, *, expected_counts=(FILE_COUNT, DIR_COUNT, EXPANDED_BYTES)):
    """Independently reject unsafe members before any extraction."""
    seen = {}
    selected = {}
    files = dirs = expanded = 0
    for entry in reader:
        name = entry.pathname
        if name.endswith("/") and entry.filetype == stat.S_IFDIR:
            name = name[:-1]
        safe_path(name)
        key = name.casefold()
        if key in seen:
            raise ValueError("Duplicate archive path")
        seen[key] = entry.filetype
        if len(seen) > MAX_ENTRIES:
            raise ValueError("Pumo entry budget exceeded")
        if entry.filetype == stat.S_IFDIR:
            dirs += 1
            continue
        if (
            entry.filetype != stat.S_IFREG
            or type(entry.size) is not int
            or entry.size < 0
        ):
            raise ValueError("Linked, special or invalid archive entry")
        files += 1
        expanded += entry.size
        if expanded > MAX_EXPANDED:
            raise ValueError("Pumo expansion budget exceeded")
        target = {
            "missions/single/pmines.hog": "missions/pmines.hog",
            "missions/single/pmines.mn2": "missions/pmines.mn2",
        }.get(name)
        if target is None and name.startswith("mods/pmines/"):
            target = name
        if target is not None:
            safe_path(target)
            folded = target.casefold()
            if folded in selected:
                raise ValueError("Duplicate staged Pumo path")
            selected[folded] = (name, target, entry.size)
    for path in seen:
        parts = path.split("/")
        if any(
            "/".join(parts[:index]) in seen
            and seen["/".join(parts[:index])] != stat.S_IFDIR
            for index in range(1, len(parts))
        ):
            raise ValueError("Archive file and directory overlap")
    if (files, dirs, expanded) != expected_counts:
        raise ValueError("Pumo archive inventory differs")
    required = {
        "missions/pmines.hog",
        "missions/pmines.mn2",
        "mods/pmines/models/model171.oof",
    }
    if not required <= {item[1] for item in selected.values()}:
        raise ValueError("Required Pumo runtime members absent")
    return {item[0]: item[1:] for item in selected.values()}


def extract(archive, output, reader_factory):
    directory(output.parent)
    if output.exists() or output.is_symlink():
        raise FileExistsError("Runtime output already exists")
    if regular(archive) != ARCHIVE_SIZE or digest(archive) != ARCHIVE_SHA256:
        raise ValueError("Pumo archive pin differs")
    with reader_factory(str(archive)) as reader:
        selected = inventory(reader)
    output.mkdir()
    try:
        completed = set()
        with reader_factory(str(archive)) as reader:
            for entry in reader:
                if entry.pathname not in selected:
                    continue
                target, declared = selected[entry.pathname]
                if entry.filetype != stat.S_IFREG or entry.size != declared:
                    raise ValueError("Pumo member changed between passes")
                path = output / target
                path.parent.mkdir(parents=True, exist_ok=True)
                directory(path.parent)
                count = 0
                member_hash = hashlib.sha256()
                with path.open("xb") as destination:
                    for block in entry.get_blocks():
                        count += len(block)
                        if count > declared or count > MAX_EXPANDED:
                            raise ValueError("Pumo member exceeds declared size")
                        member_hash.update(block)
                        destination.write(block)
                if (
                    count != declared
                    or regular(path) != declared
                    or digest(path) != member_hash.hexdigest()
                ):
                    raise ValueError("Pumo member size differs")
                completed.add(entry.pathname)
        if completed != set(selected) or digest(archive) != ARCHIVE_SHA256:
            raise ValueError("Pumo extraction incomplete or source changed")
    except BaseException:
        shutil.rmtree(output)
        raise
    return len(completed)


def stage_camera(root, stage):
    """Keep the separately sourced addon outside the Pumo archive inventory."""
    source = root / "xl-resources/d2x-xl.hog"
    if regular(source) != CAMERA_SIZE or digest(source) != CAMERA_SHA256:
        raise ValueError("Pumo camera addon pin differs")
    directory(stage)
    destination = stage / "data/d2x-xl.hog"
    destination.parent.mkdir()
    with source.open("rb") as original, destination.open("xb") as output:
        shutil.copyfileobj(original, output, 1024**2)
    if regular(destination) != CAMERA_SIZE or digest(destination) != CAMERA_SHA256:
        raise ValueError("Staged Pumo camera addon pin differs")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--stage", type=Path, required=True)
    args = parser.parse_args()
    import libarchive

    reconstruct(args.root / "pumo-full", args.archive)
    count = extract(args.archive, args.stage, libarchive.file_reader)
    try:
        stage_camera(args.root, args.stage)
    except BaseException:
        shutil.rmtree(args.stage)
        raise
    print(
        f"Verified and staged {count} original Pumo runtime files and the separate camera addon"
    )


if __name__ == "__main__":
    main()
