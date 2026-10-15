"""Assemble release documentation without changing original documents or gameplay data."""

import copy
import hashlib
import io
import json
from pathlib import Path
import shutil
import tempfile
import zipfile


REDUX_README = "README.dxx-redux.md"
# Keep these filename rules and limits aligned with the launcher PackageDocuments design
DOCUMENT_EXTENSIONS = {".txt", ".md", ".docx", ".pdf", ".rtf", ".doc"}
MAX_ENTRIES = 128
MAX_EXPANDED_BYTES = 32 * 1024 * 1024
MAX_BUNDLE_BYTES = 16 * 1024 * 1024
FIXED_TIME = (1980, 1, 1, 0, 0, 0)


def identity(path: Path) -> dict:
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    return {"filename": path.name, "bytes": path.stat().st_size, "sha256": digest}


def is_document(name: str) -> bool:
    return Path(name).suffix.lower() in DOCUMENT_EXTENSIONS or Path(name).name.lower() == "readme"


def inventory(archive: zipfile.ZipFile) -> dict:
    result = {}
    normalized = set()
    for entry in archive.infolist():
        name = entry.filename
        if any(p in ("", ".", "..") for p in name.rstrip("/").split("/")) or "\\" in name or ":" in name:
            raise ValueError(f"Unsafe archive path: {name}")
        key = name.rstrip("/").casefold()
        if key in normalized:
            raise ValueError(f"Ambiguous archive path: {name}")
        normalized.add(key)
        if entry.is_dir():
            continue
        with archive.open(entry) as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        result[name] = {"bytes": entry.file_size, "sha256": digest}
    return result


def load_policy() -> dict:
    return json.loads(Path(__file__).with_name("documentation-policy.json").read_text(encoding="utf-8"))


def replace_if_changed(candidate: Path, target: Path) -> None:
    if target.exists() and identity(candidate)["sha256"] == identity(target)["sha256"]:
        candidate.unlink()
    else:
        candidate.replace(target)


def prepare_pack(source: Path, output: Path, policy: dict, notes_dir: Path) -> tuple[Path, dict]:
    """Stage an adapted pack; return untouched upstream packs directly"""
    before_identity = identity(source)
    with zipfile.ZipFile(source) as archive:
        before = inventory(archive)
        for name, digest in policy.get("original_documents", {}).items():
            if before.get(name, {}).get("sha256") != digest:
                raise ValueError(f"Original document changed or missing: {source.name}:{name}")
        if policy.get("upstream_sha256"):
            if before_identity["sha256"] != policy["upstream_sha256"]:
                raise ValueError(f"Untouched upstream archive changed: {source.name}")
            return source, {"source": before_identity, "unchanged_upstream": True, "preserved_entries": len(before)}
        note = (notes_dir / (source.name + ".md")).read_bytes()
        if not note or note.startswith(b"\xef\xbb\xbf") or b"\r" in note:
            raise ValueError(f"Redux note must be nonempty UTF-8 without BOM or CR: {source.name}")
        note.decode("utf-8")
        replaced = policy["generated_documents"]
        if set(replaced) & set(policy.get("original_documents", {})):
            raise ValueError("An original document cannot be replaced")
        for name, digests in replaced.items():
            if name in before and before[name]["sha256"] not in digests:
                raise ValueError(f"Unrecognized generated document; refusing to replace: {source.name}:{name}")
        if REDUX_README in before and REDUX_README not in replaced:
            raise ValueError(f"Redux document path collision: {source.name}")
        preserved = {name: value for name, value in before.items() if name not in replaced}
        target = output / source.name
        if target.resolve() == source.resolve():
            raise ValueError("Release output must not overwrite the conversion input")
        output.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(dir=output, suffix=".tmp", delete=False) as temporary:
            candidate = Path(temporary.name)
        try:
            with zipfile.ZipFile(candidate, "w") as dest:
                dest.comment = archive.comment
                for entry in archive.infolist():
                    if entry.filename in replaced:
                        continue
                    entry_copy = copy.copy(entry)
                    with archive.open(entry) as src, dest.open(entry_copy, "w") as dst:
                        shutil.copyfileobj(src, dst, 1024 * 1024)
                entry = zipfile.ZipInfo(REDUX_README, FIXED_TIME)
                entry.compress_type = zipfile.ZIP_DEFLATED
                entry.external_attr = 0o100644 << 16
                dest.writestr(entry, note)
            with zipfile.ZipFile(candidate) as staged:
                after = inventory(staged)
            if {k: v for k, v in after.items() if k != REDUX_README} != preserved:
                raise ValueError(f"Non-Redux entries changed: {source.name}")
            if after[REDUX_README]["sha256"] != hashlib.sha256(note).hexdigest():
                raise ValueError(f"Redux note mismatch: {source.name}")
            if identity(source) != before_identity:
                raise ValueError(f"Source changed while preparing: {source.name}")
            replace_if_changed(candidate, target)
        finally:
            candidate.unlink(missing_ok=True)
    return target, {"source": before_identity, "unchanged_upstream": False, "preserved_entries": len(preserved)}


def prepare_bundle(pack: Path, output: Path, policy: dict) -> dict:
    with zipfile.ZipFile(pack) as archive:
        # Check metadata before reading contents; do not trust compressed sizes alone
        entries = sorted(
            (e for e in archive.infolist() if not e.is_dir() and is_document(e.filename)), key=lambda e: e.filename
        )
        if not entries or len(entries) > MAX_ENTRIES or sum(e.file_size for e in entries) > MAX_EXPANDED_BYTES:
            raise ValueError(f"Invalid documentation inventory: {pack.name}")
        docs = {}
        for entry in entries:
            with archive.open(entry) as stream:
                data = stream.read(MAX_EXPANDED_BYTES + 1)
            if len(data) != entry.file_size or sum(len(b) for b in docs.values()) + len(data) > MAX_EXPANDED_BYTES:
                raise ValueError(f"Documentation exceeds limits: {pack.name}")
            docs[entry.filename] = data
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w") as dest:
        for name, data in docs.items():
            entry = zipfile.ZipInfo(name, FIXED_TIME)
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o100644 << 16
            dest.writestr(entry, data)
    if len(buffer.getvalue()) > MAX_BUNDLE_BYTES:
        raise ValueError(f"Documentation bundle too large: {pack.name}")
    with zipfile.ZipFile(io.BytesIO(buffer.getvalue())) as bundle:
        inventory(bundle)
        if {name: bundle.read(name) for name in bundle.namelist()} != docs:
            raise ValueError(f"Documentation bundle differs from pack: {pack.name}")
    target = output / (pack.name + ".readme.zip")
    output.mkdir(parents=True, exist_ok=True)
    if not target.exists() or target.read_bytes() != buffer.getvalue():
        with tempfile.NamedTemporaryFile(dir=output, suffix=".tmp", delete=False) as temporary:
            candidate = Path(temporary.name)
            temporary.write(buffer.getvalue())
        try:
            replace_if_changed(candidate, target)
        finally:
            candidate.unlink(missing_ok=True)
    return {
        **identity(target),
        "source_pack_sha256": identity(pack)["sha256"],
        "documents": [
            {
                "path": name,
                "bytes": len(data),
                "sha256": hashlib.sha256(data).hexdigest(),
                "kind": (
                    "original"
                    if name in policy.get("original_documents", {})
                    else "redux"
                    if name == REDUX_README and not policy.get("upstream_sha256")
                    else "documentation"
                ),
            }
            for name, data in docs.items()
        ],
    }
