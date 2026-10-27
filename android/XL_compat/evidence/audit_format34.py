"""Audit original 15/34 decoding without qualifying mission gameplay."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from audit_spawn_candidates import members, findings


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("hog", "inspector", "work", "output"):
        parser.add_argument("--" + key, type=Path, required=True)
    args = parser.parse_args()
    data = args.hog.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != "f0611f1428549373526dfca8ca5a1e1883ace910f4209a938698086d189919db":
        raise ValueError("Original Apocalyptic Factor XL archive differs")
    args.work.mkdir(parents=True, exist_ok=True)
    report = args.work / "inventory.json"
    result = subprocess.run(
        [str(args.inspector.resolve()), str(args.hog.resolve()), str(report.resolve())],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode not in (0, 1):
        raise ValueError("Inspector failed: " + result.stderr)
    parsed = json.loads(report.read_text())
    payloads = members(data)
    levels = []
    for row in parsed["levels"]:
        inventory = row["inventory"]
        record = {
            "name": row["name"],
            "sha256": hashlib.sha256(payloads[row["name"].lower()]).hexdigest(),
            "runtime_qualified": False,
        }
        if inventory["status"] == "rejected":
            record.update(status="rejected", reason=inventory["reason"])
        else:
            record.update(
                status="decoded",
                level_version=inventory["level_version"],
                game_version=inventory["game_version"],
                counts={
                    key: len(inventory[key])
                    for key in ("vertices", "segments", "objects", "walls", "triggers", "light_indices", "delta_lights")
                },
                unsupported=findings(inventory["unsupported"]),
                unknown=findings(inventory["unknown"]),
                observations=findings(inventory["observations"]),
            )
        levels.append(record)
    if len(levels) != 15:
        raise ValueError("Incomplete original 15/34 audit")
    result = {
        "schema": 1,
        "scope": "Original format decoding only; runtime resource validation and gameplay qualification remain separate",
        "hog_sha256": digest,
        "levels": sorted(levels, key=lambda row: row["name"]),
    }
    content = json.dumps(result, indent=4) + "\n"
    if not args.output.exists() or args.output.read_text() != content:
        args.output.write_text(content, encoding="utf-8", newline="\n")
    print(
        f"Audited {len(levels)} original levels; {sum(row['status'] == 'decoded' for row in levels)} decoded; none runtime-qualified"
    )


if __name__ == "__main__":
    main()
