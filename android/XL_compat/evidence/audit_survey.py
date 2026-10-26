"""Verify the supplied research bundle without executing its exploratory parsers.

This recomputes measurements from supplied decoded data, not from mission bytes.
An optional native inventory comparison cross-checks the exact pilot payload.
"""

import argparse
import collections
import csv
import hashlib
import io
import json
from pathlib import Path
import zipfile


def audit(bundle, native_inventory=None, pilot_level=None):
    digest = hashlib.sha256(bundle.read_bytes()).hexdigest()
    if digest != "5b8ae8e1359d5ecaa31736c16188ff4fde0a233d54d425eb93cea3959fc85edd":
        raise ValueError("Unexpected survey bundle; review provenance before updating pin")
    with zipfile.ZipFile(bundle) as archive:
        manifest = archive.read("MANIFEST.sha256").decode().splitlines()
        members = {name: checksum for checksum, name in (line.split("  ", 1) for line in manifest)}
        if set(archive.namelist()) != set(members) | {"MANIFEST.sha256"}:
            raise ValueError("Unexpected bundle members")
        for name, checksum in members.items():
            if hashlib.sha256(archive.read(name)).hexdigest() != checksum:
                raise ValueError(f"Checksum mismatch: {name}")
        rows = json.loads(archive.read("features.json"))
        csv_rows = list(csv.DictReader(io.StringIO(archive.read("xl-levels.csv").decode())))
    if len(rows) != len(csv_rows):
        raise ValueError("CSV/JSON row count mismatch")
    for row, exported in zip(rows, csv_rows):
        for key, value in {
            "sha256": row["sha256"],
            "archive_member": row["path"],
            "level_version": row["version"],
            "game_version": row["game_version"],
            "vertices": row["vertices"],
            "segments": row["segments"],
            "walls": row["sections"]["walls"]["count"],
            "objects": row["sections"]["objects"]["count"],
            "wall_triggers": len(row["triggers"]),
            "object_triggers": len(row.get("object_triggers", [])),
        }.items():
            if exported[key] != str(value):
                raise ValueError(f"CSV mismatch: {row['path']} {key}")
    pilot_hash = "5e835e76e984c658531175640294a4b05f6127904884ba7acfd65c93165be5b2"
    pilot = next(row for row in rows if row["sha256"] == pilot_hash)
    parsed = [row for row in rows if row.get("objects_end_matches_walls") is True]
    report = {
        "bundle_sha256": digest,
        "scope": "Recomputed supplied static records; no corpus rescan or runtime qualification",
        "payloads": len(rows),
        "distinct_hashes": len({r["sha256"] for r in rows}),
        "hogs": len({r["path"].rsplit("::", 1)[0] for r in rows}),
        "level_versions": dict(sorted(collections.Counter(r["version"] for r in rows).items())),
        "game_versions": sorted({r["game_version"] for r in rows}),
        "geometry_endpoint_matches": sum(r["geometry_end_matches_game"] for r in rows),
        "object_endpoint_matches": len(parsed),
        "object_exclusions": [r["path"] for r in rows if r not in parsed],
        "over_stock_capacity": {
            "vertices_36000": sum(r["vertices"] > 36000 for r in rows),
            "segments_9000": sum(r["segments"] > 9000 for r in rows),
            "walls_255": sum(r["sections"]["walls"]["count"] > 255 for r in rows),
            "objects_1000": sum(r["sections"]["objects"]["count"] > 1000 for r in rows),
            "triggers_100": sum(len(r["triggers"]) > 100 for r in rows),
        },
        "wall_trigger_types": dict(sorted(collections.Counter(t["type"] for r in rows for t in r["triggers"]).items())),
        "object_triggers": sum(len(r.get("object_triggers", [])) for r in rows),
        "levels_with_object_triggers": sum(bool(r.get("object_triggers")) for r in rows),
        "equipment_generators": sum(r["sections"].get("equipment", {}).get("count", 0) for r in rows),
        "levels_with_equipment_generators": sum(r["sections"].get("equipment", {}).get("count", 0) > 0 for r in rows),
        "levels_with_chromatic_colors": {
            key: sum(r[key]["non_gray"] > 0 for r in rows) for key in ("vertex_colors", "side_colors", "texture_colors")
        },
        "effect_records_in_129_sections": {
            str(tag): sum(o["render"] == tag for r in parsed for o in r["object_records"]) for tag in (11, 12, 13)
        },
        "pilot": {
            key: pilot[key]
            for key in (
                "sha256",
                "path",
                "version",
                "game_version",
                "vertices",
                "segments",
                "wall_types",
                "nonzero_cloak",
                "vertex_colors",
                "side_colors",
                "texture_colors",
            )
        },
        "native_pilot_comparison": "Not requested",
        "playable": False,
    }
    if native_inventory:
        if not pilot_level or hashlib.sha256(pilot_level.read_bytes()).hexdigest() != pilot_hash:
            raise ValueError("Native comparison requires the exact --pilot-level bytes")
        native = json.loads(native_inventory.read_text())
        if "levels" in native:
            if len(native["levels"]) != 1:
                raise ValueError("Native comparison needs a single-level report")
            native = native["levels"][0]["inventory"]
        comparisons = {
            "version": native["level_version"],
            "game_version": native["game_version"],
            "mine_offset": native["mine_offset"],
            "game_offset": native["game_offset"],
            "vertices": len(native["vertices"]),
            "segments": len(native["segments"]),
            "wall_types": dict(collections.Counter(str(w["type"]) for w in native["walls"])),
            "nonzero_cloak": dict(collections.Counter(str(w["cloak"]) for w in native["walls"] if w["cloak"])),
        }
        for key, value in comparisons.items():
            if value != pilot[key]:
                raise ValueError(f"Native pilot mismatch: {key}")
        for key in ("objects", "walls", "triggers"):
            if len(native[key]) != pilot["sections"][key]["count"]:
                raise ValueError(f"Native count mismatch: {key}")
        for key in ("vertex_colors", "side_colors", "texture_colors"):
            if sum(len(set(c["rgb"])) > 1 for c in native[key]) != pilot[key]["non_gray"]:
                raise ValueError(f"Native color mismatch: {key}")
        report["native_pilot_comparison"] = (
            "Pilot bytes match survey hash; native counts, offsets, versions, wall types/cloak and chromatic color counts agree"
        )
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path, default=Path(__file__).with_name("dot-survey.zip"))
    parser.add_argument("--native-inventory", type=Path)
    parser.add_argument("--pilot-level", type=Path)
    args = parser.parse_args()
    print(json.dumps(audit(args.bundle, args.native_inventory, args.pilot_level), indent=2) + "\n", end="")
