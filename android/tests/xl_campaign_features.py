"""Inventory authored level features for every advertised ordinal of one readiness catalog."""

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

from nightly_corpus_stage import extract_archive, member_path, reject_links
from xl_readiness import levels


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def inspect(exe, path, report, archive_only=False):
    mode = "--archive-only" if archive_only else "--mission-features"
    run = subprocess.run([exe, path, report, mode], capture_output=True, text=True, timeout=180)
    if run.returncode not in (0, 1) or not report.is_file():
        return {"inspection_error": run.stderr.strip() or f"Inspector exited {run.returncode}"}
    return json.loads(report.read_bytes())


def measure(exe, folder, cache, case, temporary, first_level):
    descriptor = folder / member_path(case["descriptor"])
    filename = case["level_file"].casefold()
    # Non-builtin original missions try the sibling HOG, then the same-stem
    # single level, then the first advertised file. Never search other releases.
    siblings = {p.name.casefold(): p for p in descriptor.parent.iterdir() if p.is_file()}
    stem = descriptor.stem.casefold()
    hog = siblings.get(stem + ".hog")
    path = (
        hog
        or siblings.get(stem + (".rdl" if descriptor.suffix.casefold() == ".msn" else ".rl2"))
        or siblings.get(first_level)
    )
    if not path:
        return {"resolution": "no_package_candidate"}
    if not hog and path.name.casefold() != filename:
        return {"resolution": "single_level_candidate_differs", "candidate": path.name}
    identity = digest(path) if path not in cache else cache[path][0]
    if path not in cache:
        report = temporary / f"{len(cache)}.json"
        output = inspect(exe, path, report, path.suffix.casefold() == ".rdl")
        cache[path] = (identity, output)
    output = cache[path][1]
    result = {"candidate": path.relative_to(folder).as_posix(), "candidate_sha256": identity}
    if "inspection_error" in output:
        return dict(result, resolution="inspection_failed", error=output["inspection_error"])
    if hog:
        member = next((m for m in output["members"] if m["name"].casefold() == filename), None)
        if not member:
            return dict(result, resolution="absent_from_sibling_hog")
        with path.open("rb") as data:
            data.seek(member["offset"])
            remaining = member["bytes"]
            checksum = hashlib.sha256()
            while remaining:
                block = data.read(min(remaining, 1024 * 1024))
                if not block:
                    raise ValueError("Inspected HOG span exceeds file")
                checksum.update(block)
                remaining -= len(block)
        result.update(member=member["name"], level_sha256=checksum.hexdigest())
    else:
        result.update(member=path.name, level_sha256=identity)
    inventory = next((entry["inventory"] for entry in output["levels"] if entry["name"].casefold() == filename), None)
    return dict(
        result, resolution="package_bytes_identified", inventory=inventory or {"status": "layout_not_inspected"}
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("catalog", "assets", "inspector", "work"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--seven-zip")
    args = parser.parse_args()
    catalog = json.loads(args.catalog.read_bytes())
    cases = defaultdict(list)
    for case in catalog["cases"]:
        cases[case["archive"]].append(case)
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    rows = []
    for archive in catalog["archives"]:
        name = archive["path"]
        source = args.assets / member_path(name)
        reject_links(source)
        if source.stat().st_size != archive["size"] or digest(source) != archive["sha256"]:
            raise ValueError(f"Catalog archive changed: {name}")
        with tempfile.TemporaryDirectory(dir=work) as temporary:
            temp = Path(temporary)
            folder = temp / "input"
            extract_archive(source, folder, args.seven_zip)
            descriptors, cache = {}, {}
            for case in cases[name]:
                descriptor = case["descriptor"]
                if descriptor not in descriptors:
                    path = folder / member_path(descriptor)
                    descriptors[descriptor] = digest(path), {r["level"]: r["level_file"] for r in levels(path)}
                sha, advertised = descriptors[descriptor]
                if sha != case["descriptor_sha256"] or advertised[case["level"]] != case["level_file"]:
                    raise ValueError(f"Catalog descriptor changed: {descriptor}")
                measured = measure(
                    args.inspector.resolve(), folder, cache, case, temp, advertised.get(1, "").casefold()
                )
                rows.append(dict(case, archive_sha256=archive["sha256"], **measured))
        print(f"{len(rows)} advertised entries inspected through {name}", flush=True)
    if Counter(v["id"] for v in rows) != Counter(v["id"] for v in catalog["cases"]):
        raise ValueError("Inventory lost catalog identities")
    result = dict(
        schema="xl-campaign-features-v1",
        catalog_sha256=digest(args.catalog),
        inspector_sha256=digest(args.inspector),
        rows=rows,
    )
    (work / "features.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
