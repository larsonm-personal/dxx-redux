#!/usr/bin/env python3
"""Recover the bounded public export via GitHub's check annotations API.

Run the public-lan-smoke workflow with evidence_run_id first, then pass the
EXPORT run ID here. Most screenshots and unfiltered logs remain in the source CI
artifact. This is a supported authenticated API read, not a storage URL retry.
"""

import argparse
import base64
import gzip
import hashlib
import json
import subprocess
from pathlib import Path


def api(endpoint, paginate=False):
    command = ["gh", "api", endpoint]
    if paginate:
        command += ["--paginate"]
    raw = subprocess.check_output(command, text=True)
    if not paginate:
        return json.loads(raw)
    pages = []
    decoder = json.JSONDecoder()
    while raw.strip():
        page, end = decoder.raw_decode(raw.lstrip())
        pages.append(page)
        raw = raw.lstrip()[end:]
    return pages


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("export_run", type=int)
    parser.add_argument("--repo", default="larsonm-personal/dxx-redux")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    jobs = api(f"repos/{args.repo}/actions/runs/{args.export_run}/jobs")["jobs"]
    jobs = [job for job in jobs if job["name"] == "Export public timing evidence" and job["conclusion"] == "success"]
    if len(jobs) != 1:
        raise SystemExit("Expected exactly one successful evidence export job")
    pages = api(f"repos/{args.repo}/check-runs/{jobs[0]['id']}/annotations?per_page=100", paginate=True)
    chunks = sorted(
        (item["title"], item["message"]) for page in pages for item in page if item["title"].startswith("LAN_EVIDENCE_")
    )
    if not chunks or [title for title, _ in chunks] != [f"LAN_EVIDENCE_{index:04d}" for index in range(len(chunks))]:
        raise SystemExit("Missing or duplicate evidence chunks")
    blob = base64.b64decode("".join(value for _, value in chunks), validate=True)
    files = json.loads(gzip.decompress(blob))
    args.output.mkdir(parents=True, exist_ok=True)
    hashes = {}
    for relative, entry in files.items():
        path = Path(relative)
        if path.is_absolute() or ".." in path.parts:
            raise SystemExit("Unsafe evidence path")
        destination = args.output / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        if "base64" in entry:
            destination.write_bytes(base64.b64decode(entry["base64"], validate=True))
        else:
            destination.write_text(entry["text"], encoding="utf-8")
        hashes[relative] = {
            "original_sha256": entry["source_sha256"],
            "exported_sha256": hashlib.sha256(destination.read_bytes()).hexdigest(),
        }
    provenance = {
        "export_run": args.export_run,
        "repository": args.repo,
        "gzip_sha256": hashlib.sha256(blob).hexdigest(),
        "note": "Bounded text excerpts; observation state reduced. Original hashes refer to full CI source files. Screenshots remain in the original artifact unless explicitly listed here.",
        "files": hashes,
    }
    (args.output / "export-provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
    print(f"Recovered {len(files)} files/excerpts from export run {args.export_run}")


if __name__ == "__main__":
    main()
