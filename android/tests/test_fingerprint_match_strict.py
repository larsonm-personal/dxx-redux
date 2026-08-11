#!/usr/bin/env python3
"""Exercise strict host fingerprint database admission."""

import json
import re
import subprocess
import tempfile
from pathlib import Path


REPO = Path(__file__).parents[2]
MATCHER = REPO / "android/tests/build/Debug/fingerprint_match.exe"
KNOWN_DISCS = REPO / "android/app/src/main/assets/known_albums.json5"


def run_matcher(
    entries: list[dict], duration_tolerance: str = "0.10"
) -> subprocess.CompletedProcess[str]:
    with tempfile.TemporaryDirectory(prefix="dxx-fingerprint-match-") as directory:
        database = Path(directory) / "database.json"
        database.write_text(json.dumps(entries), encoding="utf-8")
        return subprocess.run(
            [str(MATCHER), str(database), "0.65", duration_tolerance],
            capture_output=True,
            check=False,
            text=True,
        )


def main() -> None:
    if not MATCHER.is_file():
        raise SystemExit(f"Missing matcher: {MATCHER}")
    source = KNOWN_DISCS.read_text(encoding="utf-8")
    match = re.search(r'"?chromaprint"?\s*:\s*"([^"]+)"', source)
    if not match:
        raise SystemExit("Maintained database has no Chromaprint fixture")
    valid = {
        "name": "strict fixture",
        "disc_id": "strict-fixture",
        "track": 1,
        "duration_ms": 100000,
        "chromaprint": match.group(1),
    }

    accepted = run_matcher([valid])
    if accepted.returncode != 0 or "Loaded 1 entries" not in accepted.stderr:
        raise SystemExit(f"Valid database failed:\n{accepted.stderr}")

    shorter = dict(valid, disc_id="shorter", duration_ms=90000)
    longer = dict(valid, disc_id="longer", duration_ms=100000)
    for entries in ([shorter, longer], [longer, shorter]):
        symmetric = run_matcher(entries)
        if symmetric.returncode != 0 or '"score"' not in symmetric.stdout:
            raise SystemExit("Duration boundary changed after entry reordering")

    outside = run_matcher([shorter, longer], "0.09")
    if outside.returncode != 0 or '"score"' in outside.stdout:
        raise SystemExit("Host matcher ignored configured duration tolerance")

    malformed = dict(valid, chromaprint="not-a-fingerprint")
    rejected = run_matcher([valid, malformed, valid])
    if rejected.returncode == 0:
        raise SystemExit("Malformed middle entry produced a consumable partial database")

    over_limit = run_matcher([valid] * 4097)
    if over_limit.returncode == 0:
        raise SystemExit("One-over entry database was accepted")

    print("Strict fingerprint matcher tests passed")


if __name__ == "__main__":
    main()
