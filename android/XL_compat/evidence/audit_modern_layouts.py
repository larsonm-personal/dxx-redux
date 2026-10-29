"""Inventory the pinned additional XL distributions without qualifying gameplay."""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess


def digest_file(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def histogram(values):
    return dict(sorted(Counter(str(value) for value in values).items()))


def findings(issues):
    result = []
    for feature in sorted({row["feature"] for row in issues}):
        rows = [row for row in issues if row["feature"] == feature]
        details = sorted({row["detail"] for row in rows})
        result.append(
            {
                "feature": feature,
                "count": len(rows),
                "distinct_details": len(details),
                "example_details": details[:3],
            }
        )
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("root", "inspector", "work", "output"):
        parser.add_argument("--" + key, type=Path, required=True)
    args = parser.parse_args()
    root = args.root.resolve()
    manifest = json.loads(Path(__file__).with_name("extended-hog-distributions.json").read_text(encoding="utf-8"))
    args.work.mkdir(parents=True, exist_ok=True)
    archives = []
    canonical = ((7, 6, 2, 3), (0, 4, 7, 3), (0, 1, 5, 4), (2, 6, 5, 1), (4, 5, 6, 7), (3, 2, 1, 0))
    for original in manifest["archives"]:
        path = (root / original["path"]).resolve()
        path.relative_to(root)
        digest = digest_file(path)
        if digest != original["sha256"]:
            raise ValueError(f"Original archive changed: {original['path']}")
        report_path = args.work / (path.stem + "-modern-inventory.json")
        command = subprocess.run(
            [str(args.inspector.resolve()), str(path), str(report_path.resolve())],
            capture_output=True,
            text=True,
            check=False,
        )
        archive = {"path": original["path"], "sha256": digest, "bytes": path.stat().st_size}
        if command.returncode == 2:
            archive.update(status="container_rejected", reason=command.stderr.strip(), levels=[])
        elif command.returncode in (0, 1):
            report = json.loads(report_path.read_text(encoding="utf-8"))
            members = {row["name"]: row for row in report["members"]}
            archive.update(status="inspected", member_count=len(members), levels=[])
            archive["resource_extensions"] = histogram(Path(name).suffix.lower() for name in members)
            with path.open("rb") as stream:
                for row in report["levels"]:
                    member = members[row["name"]]
                    stream.seek(member["offset"])
                    payload = stream.read(member["bytes"])
                    if len(payload) != member["bytes"]:
                        raise ValueError("Original member became truncated")
                    record = {
                        "name": row["name"],
                        "sha256": hashlib.sha256(payload).hexdigest(),
                        "bytes": member["bytes"],
                        "runtime_qualified": False,
                    }
                    level = row["inventory"]
                    if level["status"] == "rejected":
                        record.update(status="rejected", reason=level["reason"])
                    else:
                        segments = level["segments"]
                        sides = [side for segment in segments for side in segment["sides"]]
                        record.update(
                            status="decoded",
                            level_version=level["level_version"],
                            game_version=level["game_version"],
                            counts={
                                key: len(level.get(key, []))
                                for key in (
                                    "vertices",
                                    "segments",
                                    "objects",
                                    "walls",
                                    "triggers",
                                    "object_trigger_records",
                                    "matcens",
                                    "equipment",
                                    "flicker_lights",
                                    "light_indices",
                                    "delta_lights",
                                )
                            },
                            geometry={
                                "nonstandard_sides": sum(
                                    tuple(side.get("corners", canonical[index])) != canonical[index]
                                    for segment in segments
                                    for index, side in enumerate(segment["sides"])
                                ),
                                "active_corner_counts": histogram(
                                    sum(corner != 255 for corner in side.get("corners", range(4))) for side in sides
                                ),
                                "deleted_vertex_segments": sum(
                                    any(v >= 0xFFF8 for v in segment["vertices"]) for segment in segments
                                ),
                                "producer_range": [
                                    min(s["producer"] for s in segments),
                                    max(s["producer"] for s in segments),
                                ],
                                "value_range": [min(s["value"] for s in segments), max(s["value"] for s in segments)],
                            },
                            segment_functions=histogram(segment["function"] for segment in segments),
                            segment_properties=histogram(segment.get("properties", 0) for segment in segments),
                            wall_trigger_types=histogram(trigger["type"] for trigger in level["triggers"]),
                            object_types=histogram(obj["type"] for obj in level["objects"]),
                            render_types=histogram(obj["render"] for obj in level["objects"]),
                            fog_settings=level.get("fog_settings", []),
                            runtime_blockers=findings(row["runtime_blockers"]),
                            unknown=findings(level["unknown"]),
                        )
                    archive["levels"].append(record)
            archive["levels"].sort(key=lambda row: row["name"])
        else:
            raise RuntimeError(f"Inspector crashed: {command.returncode}: {command.stderr}")
        if digest_file(path) != digest:
            raise ValueError("Original archive changed during inspection")
        archives.append(archive)
        print(f"{path.name}: {archive['status']}, {len(archive['levels'])} level reports")
    result = {
        "schema": 1,
        "scope": "Original layout and detected requirement inventory; source comparisons are documented separately; no runtime or campaign qualification",
        "archives": archives,
    }
    content = json.dumps(result, indent=4) + "\n"
    existing = json.loads(args.output.read_text(encoding="utf-8")) if args.output.exists() else None
    if existing != result:
        args.output.write_text(content, encoding="utf-8", newline="\n")
    levels = [level for archive in archives for level in archive["levels"]]
    print(
        f"{len(levels)} levels, {sum(level['status'] == 'decoded' for level in levels)} decoded; all runtime_qualified=false"
    )


if __name__ == "__main__":
    main()
