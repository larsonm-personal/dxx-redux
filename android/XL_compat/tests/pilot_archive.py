"""Require the unchanged original pilot inventory without repeated JSON parsing."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

PILOT_HASH = "7491c45d217831509a840d934641bf1791e029ae6c1bcbe3e4efba2847d53d32"
PILOT_COUNTS = {
    "vertices": 828,
    "segments": 228,
    "objects": 110,
    "walls": 86,
    "triggers": 13,
    "vertex_colors": 828,
    "side_colors": 1368,
    "texture_colors": 910,
    "object_trigger_heads": 700,
    "light_indices": 70,
    "delta_lights": 3960,
}


def validate_inventory(report):
    levels = report["levels"]
    if len(levels) != 30 or report["playable"] is not False:
        raise ValueError("Expected all 30 original levels without gameplay qualification")
    found = False
    for level in levels:
        inventory = level["inventory"]
        if inventory["status"] != "inspection_only" or inventory["playable"] is not False:
            raise ValueError("Original level rejected or falsely declared playable")
        if level["name"] == "d2leva-1.rl2":
            if found:
                raise ValueError("Duplicate pilot level")
            found = True
            for key, expected in PILOT_COUNTS.items():
                if len(inventory[key]) != expected:
                    raise ValueError("Pilot inventory differs: " + key)
    if not found:
        raise ValueError("Pilot level missing")


def fingerprint(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inspect", type=Path, required=True)
    parser.add_argument("--hog", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if fingerprint(args.hog) != PILOT_HASH:
        raise ValueError("Pilot HOG differs from the audited original")
    command = [str(args.inspect), str(args.hog), str(args.report)]
    subprocess.run(command, check=True, timeout=90)
    validate_inventory(json.loads(args.report.read_text()))
    first_hash, first_time = fingerprint(args.report), args.report.stat().st_mtime_ns
    subprocess.run(command, check=True, timeout=90)
    if fingerprint(args.report) != first_hash or args.report.stat().st_mtime_ns != first_time:
        raise ValueError("Identical regeneration changed the report")
    if fingerprint(args.hog) != PILOT_HASH:
        raise ValueError("Inspection modified the input")
    print("PASS: all 30 original pilot levels, exact pilot counts and unchanged regeneration")


if __name__ == "__main__":
    main()
