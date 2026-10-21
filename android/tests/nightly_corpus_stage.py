"""Bounded private route staging; no assets or raw diagnostics are published."""

import hashlib
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import tempfile
import time
import zipfile

MAX_BYTES = 2 * 1024**3
MAX_ENTRIES = 16384
COMPANIONS = {
    ".ham",
    ".hxm",
    ".pog",
    ".pig",
    ".256",
    ".s11",
    ".s22",
    ".dtx",
    ".hx1",
    ".pg1",
    ".pal",
    ".rl2",
    ".rdl",
    ".sdl",
    ".sl2",
    ".lgt",
    ".clr",
}


class StageUnavailable(ValueError):
    """A fixed safe reason suitable for the retail receipt boundary."""


def reject_links(path):
    if any(p.is_symlink() for p in (Path(path), *Path(path).parents)):
        raise StageUnavailable("Corpus source or destination contains links")


def member_path(name):
    name = name.replace("\\", "/")
    parts = name.rstrip("/").split("/")
    if (
        not name
        or len(name) > 1024
        or any(ord(c) < 32 for c in name)
        or any(c in name for c in ":<>|?*")
        or any(p in ("", ".", "..") or p.endswith((" ", ".")) for p in parts)
        or any(
            p.split(".")[0].casefold()
            in {
                "con",
                "prn",
                "aux",
                "nul",
                *("com" + str(i) for i in range(1, 10)),
                *("lpt" + str(i) for i in range(1, 10)),
            }
            for p in parts
        )
    ):
        raise StageUnavailable("Unsafe corpus archive path")
    return PurePosixPath(*parts)


def validate_entries(entries):
    seen = {}
    total = 0
    for name, size, directory in entries:
        path = member_path(name)
        key = path.as_posix().casefold()
        if key in seen or len(seen) >= MAX_ENTRIES or size < 0 or size > MAX_BYTES - total:
            raise StageUnavailable("Corpus archive entries exceed safe limits or collide")
        seen[key] = directory
        total += size
    for key in seen:
        for parent in PurePosixPath(key).parents:
            if parent.as_posix() in seen and not seen[parent.as_posix()]:
                raise StageUnavailable("Corpus archive file overlaps a directory")


def bounded_tool(argv, output, limit=8 * 1024**2, timeout=120):
    # Bound tool diagnostics and descendants even when an extractor stalls.
    with output.open("wb") as stream:
        process = subprocess.Popen(argv, stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
        started = time.monotonic()
        try:
            while process.poll() is None:
                if output.stat().st_size > limit or time.monotonic() - started > timeout:
                    raise StageUnavailable("Corpus archive extraction exceeded safe limits")
                time.sleep(0.02)
            if process.returncode or output.stat().st_size > limit:
                raise StageUnavailable("Corpus archive extraction failed")
        finally:
            if process.poll() is None:
                if os.name == "posix":
                    import signal

                    os.killpg(process.pid, signal.SIGKILL)
                else:
                    process.kill()
                process.wait()


def extract_archive(archive, destination, seven_zip=None):
    archive, destination = Path(archive), Path(destination)
    reject_links(archive)
    reject_links(destination)
    if archive.is_symlink() or archive.stat().st_size > MAX_BYTES or destination.exists():
        raise StageUnavailable("Unsafe corpus extraction destination or archive")
    destination.mkdir(parents=True)
    try:
        if archive.suffix.casefold() == ".zip":
            with zipfile.ZipFile(archive) as source:
                entries = source.infolist()
                validate_entries([(e.filename, e.file_size, e.is_dir()) for e in entries])
                for entry in entries:
                    mode = entry.external_attr >> 16
                    if stat.S_IFMT(mode) not in (0, stat.S_IFREG, stat.S_IFDIR) or entry.flag_bits & 1:
                        raise StageUnavailable("Corpus archive links or encrypted entries are unsupported")
                    target = destination / member_path(entry.filename)
                    if entry.is_dir():
                        target.mkdir(parents=True, exist_ok=True)
                    else:
                        target.parent.mkdir(parents=True, exist_ok=True)
                        with source.open(entry) as incoming, target.open("xb") as outgoing:
                            shutil.copyfileobj(incoming, outgoing, 1024**2)
        elif archive.suffix.casefold() in (".7z", ".rar"):
            tool = seven_zip or next((shutil.which(n) for n in ("7zz", "7z", "7za") if shutil.which(n)), None)
            if not tool:
                raise StageUnavailable("7-Zip prerequisite unavailable")
            with tempfile.TemporaryDirectory(dir=destination.parent) as temp:
                listing = Path(temp) / "listing"
                bounded_tool([tool, "l", "-slt", "-ba", "--", str(archive)], listing)
                entries = []
                for block in listing.read_text(encoding="utf-8", errors="strict").strip("\r\n").split("\n\n"):
                    fields = {}
                    for line in block.splitlines():
                        if " = " not in line:
                            raise StageUnavailable("Corpus archive listing is ambiguous")
                        key, value = line.split(" = ", 1)
                        if key in fields:
                            raise StageUnavailable("Corpus archive listing is ambiguous")
                        fields[key] = value
                    attrs = fields.get("Attributes", "").split()
                    if (
                        any(k in fields for k in ("Symbolic Link", "Hard Link"))
                        or any(a.startswith(("l", "L")) for a in attrs)
                        or fields.get("Encrypted") == "+"
                    ):
                        raise StageUnavailable("Corpus archive links or encrypted entries are unsupported")
                    directory = fields.get("Folder") == "+" or any(a.startswith("D") for a in attrs)
                    entries.append((fields["Path"], int(fields.get("Size", "0")), directory))
                validate_entries(entries)
                bounded_tool([tool, "x", "-y", "-o" + str(destination), "--", str(archive)], Path(temp) / "extract")
        else:
            raise StageUnavailable("Corpus archive format requires reviewed extraction support")
        files = list(destination.rglob("*"))
        if any(p.is_symlink() or not (p.is_file() or p.is_dir()) for p in files):
            raise StageUnavailable("Corpus extraction produced a linked or special file")
        validate_entries(
            [(p.relative_to(destination).as_posix(), p.stat().st_size if p.is_file() else 0, p.is_dir()) for p in files]
        )
    except BaseException:
        shutil.rmtree(destination)
        raise


def select_files(root, mission):
    root = Path(root)
    reject_links(root)
    files = [p for p in root.rglob("*") if p.is_file()]
    if any(p.is_symlink() for p in root.rglob("*")):
        raise StageUnavailable("Corpus source contains links")
    index = {}
    for path in files:
        key = member_path(path.relative_to(root).as_posix()).as_posix().casefold()
        if key in index:
            raise StageUnavailable("Corpus source has case-colliding paths")
        index[key] = path
    name = member_path(mission["mission_filename"])
    if len(name.parts) != 1 or name.suffix.casefold() not in (".mn2", ".msn"):
        raise StageUnavailable("Invalid corpus mission descriptor")
    requested = mission.get("mission_path")
    if requested:
        path = member_path(requested)
        if path.name.casefold() != name.name.casefold():
            raise StageUnavailable("Corpus descriptor identity mismatch")
        candidates = [index[path.as_posix().casefold()]] if path.as_posix().casefold() in index else []
    else:
        candidates = [p for p in files if p.name.casefold() == name.name.casefold()]
    if not candidates:
        raise StageUnavailable("Selected corpus descriptor unavailable")
    selections = []
    for descriptor in sorted(candidates):
        siblings = {p.name.casefold(): p for p in files if p.parent == descriptor.parent}
        text = descriptor.read_bytes().decode("utf-8", errors="replace").replace("\x1a", "")
        overrides = re.findall(r"^\s*hog\s*=\s*([^;\r\n]+)", text, flags=re.I | re.M)
        if len(overrides) > 1:
            raise StageUnavailable("Ambiguous corpus descriptor HOG override")
        hog = member_path(overrides[0].strip()) if overrides else name.with_suffix(".hog")
        if len(hog.parts) != 1 or hog.suffix.casefold() != ".hog":
            raise StageUnavailable("Corpus HOG override escapes the descriptor directory")
        # d2/main/mission.c mounts only the descriptor-basename .hog
        if hog.name.casefold() != name.with_suffix(".hog").name.casefold():
            raise StageUnavailable("Corpus HOG override is unsupported by the route engine")
        # Native mission loading allows loose levels without a mission HOG
        level_names = {v.casefold() for v in re.findall(r"(?im)^\s*([^,;\s]+\.(?:rl2|rdl))(?=\s|,|$)", text)}
        selected = {descriptor.name.casefold(): descriptor}
        if hog.name.casefold() in siblings:
            selected[hog.name.casefold()] = siblings[hog.name.casefold()]
        elif not level_names or not all(n in siblings for n in level_names):
            raise StageUnavailable("Descriptor-associated corpus HOG or complete loose levels unavailable")
        alternate = re.findall(r"(?im)^\s*!ham\s*=\s*([^;\r\n]+)", text)
        if len(alternate) > 1:
            raise StageUnavailable("Ambiguous alternate corpus HAM dependency")
        if alternate:
            ham = member_path(alternate[0].strip())
            if len(ham.parts) != 1 or ham.suffix.casefold() != ".ham":
                raise StageUnavailable("Unsafe alternate corpus HAM dependency")
            alternate_hog = ham.with_suffix(".hog").name.casefold()
            if alternate_hog in siblings:
                selected[alternate_hog] = siblings[alternate_hog]
            elif ham.name.casefold() not in siblings:
                raise StageUnavailable("Alternate corpus HAM dependency requires reviewed staging")
        # Loose levels from neighboring missions must not shadow this HOG's members
        selected.update(
            {
                n: p
                for n, p in siblings.items()
                if p.suffix.casefold() in COMPANIONS
                and (p.suffix.casefold() not in (".rl2", ".rdl") or n in level_names)
            }
        )
        fingerprint = {n: hashlib.sha256(p.read_bytes()).hexdigest() for n, p in selected.items()}
        selections.append((selected, fingerprint))
    if any(signature != selections[0][1] for _, signature in selections[1:]):
        raise StageUnavailable("Ambiguous corpus variants have distinct payloads")
    return selections[0][0]


def stage_route(archive, mission, destination, seven_zip=None, source_directory=None):
    destination = Path(destination)
    reject_links(destination)
    if destination.exists() or destination.is_symlink():
        raise StageUnavailable("Corpus route stage already exists")
    destination.parent.mkdir(parents=True, exist_ok=True)
    created = False
    try:
        with tempfile.TemporaryDirectory(dir=destination.parent) as temp:
            if source_directory is None:
                root = Path(temp) / "archive"
                extract_archive(archive, root, seven_zip)
            else:
                root = Path(source_directory)
            selected = select_files(root, mission)
            stage = Path(temp) / "publish" / "missions"
            stage.mkdir(parents=True)
            for name, path in sorted(selected.items()):
                data = path.read_bytes()
                if path.suffix.casefold() in (".mn2", ".msn"):
                    data = data.replace(b"\x1a", b"")
                (stage / name).write_bytes(data)
                # Desktop/headless loaders open loose resources at the search root
                if path.suffix.casefold() in COMPANIONS:
                    (stage.parent / name).write_bytes(data)
            destination.mkdir()
            created = True
            for path in stage.parent.iterdir():
                path.rename(destination / path.name)
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        if created:
            shutil.rmtree(destination)
        if isinstance(error, StageUnavailable):
            raise
        raise StageUnavailable("Corpus archive cannot be staged safely") from error
    except BaseException:
        if created:
            shutil.rmtree(destination)
        raise
    return destination
