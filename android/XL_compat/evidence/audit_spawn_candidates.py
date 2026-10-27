"""Reinspect original surveyed gameplay candidates without qualifying gameplay."""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import zipfile


def members(data):
    if data[:3] != b"DHF":
        raise ValueError("Candidate requires a classic HOG container")
    offset = 3
    result = {}
    while offset < len(data):
        if offset + 17 > len(data):
            raise ValueError("Truncated candidate HOG member")
        name = data[offset : offset + 13].split(b"\0", 1)[0].decode("ascii").lower()
        size = struct.unpack_from("<i", data, offset + 13)[0]
        offset += 17
        if size < 0 or offset + size > len(data) or name in result:
            raise ValueError("Invalid candidate HOG extent or duplicate member")
        result[name] = data[offset : offset + size]
        offset += size
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("bundle", "missions-root", "inspector", "work", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--feature", choices=("spawn-bot", "equipment"), default="spawn-bot")
    args = parser.parse_args()
    bundle_hash = hashlib.sha256(args.bundle.read_bytes()).hexdigest()
    if bundle_hash != "5b8ae8e1359d5ecaa31736c16188ff4fde0a233d54d425eb93cea3959fc85edd":
        raise ValueError("Survey bundle differs from audited research")
    with zipfile.ZipFile(args.bundle) as archive:
        rows = json.loads(archive.read("features.json"))
    args.work.mkdir(parents=True, exist_ok=True)
    root = args.missions_root.resolve()
    cached = {}
    records = []
    for row in rows:
        actions = [action for action in row.get("object_triggers", []) if action["type"] == 26]
        equipment = row.get("sections", {}).get("equipment", {})
        if not (actions if args.feature == "spawn-bot" else equipment.get("count", 0)):
            continue
        _, archive_name, level_name = row["path"].split("::")
        source = (root / archive_name).resolve()
        source.relative_to(root)
        if source not in cached:
            data = source.read_bytes()
            digest = hashlib.sha256(data).hexdigest()
            report = args.work / (source.stem + ".json")
            report.unlink(missing_ok=True)
            result = subprocess.run(
                [str(args.inspector.resolve()), str(source), str(report)],
                capture_output=True,
                timeout=60,
                check=False,
            )
            if result.returncode not in (0, 1) or not report.exists():
                raise ValueError("Native candidate inspection did not produce a report")
            inspection = json.loads(report.read_text())
            if inspection["status"] != "inspection_only" or inspection["playable"]:
                raise ValueError("Candidate inspection made a gameplay claim")
            if hashlib.sha256(source.read_bytes()).hexdigest() != digest:
                raise ValueError("Original HOG changed during inspection")
            cached[source] = (digest, members(data), inspection)
        digest, payloads, inspection = cached[source]
        payload_hash = hashlib.sha256(payloads[level_name.lower()]).hexdigest()
        if payload_hash != row["sha256"]:
            raise ValueError("Original candidate does not match surveyed payload")
        inventory = next(
            level["inventory"] for level in inspection["levels"] if level["name"].lower() == level_name.lower()
        )
        decoded = inventory.get("status") != "rejected"
        record = {
            "archive": archive_name,
            "archive_sha256": digest,
            "level": level_name,
            "level_sha256": payload_hash,
            "survey_profile": f"{row['version']}/{row['game_version']}",
            "survey_spawn_actions": [
                {
                    "flags": action["flags"],
                    "value": action["value"],
                    "time": action["time"],
                    "links": action["links"],
                    "first_segment": action["segments"][0] if action["segments"] else None,
                }
                for action in actions
            ],
            "native_decoded": decoded,
            "runtime_qualified": False,
        }
        if args.feature == "equipment":
            del record["survey_spawn_actions"]
            record["survey_equipment"] = equipment
            record["survey_equipment_segments"] = row["segment_functions"].get(
                "11" if row["version"] >= 21 else "15", 0
            )
        if decoded:
            record["native_profile"] = f"{inventory['level_version']}/{inventory['game_version']}"
            record["inspector_findings"] = {key: inventory[key] for key in ("unsupported", "unknown")}
            if args.feature == "equipment":
                record["native_equipment"] = [
                    dict(zip(("flags0", "flags1", "hitpoints", "interval", "raw_segment", "raw_station"), maker))
                    for maker in inventory.get("equipment", [])
                ]
        else:
            record["native_rejection"] = inventory["reason"]
        records.append(record)
    report = {
        "schema": 1,
        "scope": "Fresh native decoding of hash-matched survey candidates; no admission or gameplay qualification",
        "bundle_sha256": bundle_hash,
        "levels": records,
    }
    text = json.dumps(report, indent=4, ensure_ascii=True) + "\n"
    if not args.output.exists() or args.output.read_text() != text:
        args.output.write_text(text, encoding="utf-8")
    print(f"Inspected {len(records)} original {args.feature} candidates; none promoted")


if __name__ == "__main__":
    main()
