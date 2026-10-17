"""Compare defined historical matcen bindings in existing level metadata.

Never generates or updates references. Logical disk-unbound slots are observations,
not golden robot definitions: their allocated globals can contain previous-level data.
"""

import argparse
import json
from pathlib import Path


class MatcenMismatch(ValueError):
    pass


def validate_capture(value):
    if not isinstance(value, dict) or type(value.get("schema_version")) is not int or value["schema_version"] != 1:
        raise MatcenMismatch("Missing supported matcen capture")
    if value.get("capture_status") != "complete" or value.get("admission") != "loaded":
        raise MatcenMismatch("A rejected, missing or incomplete level is not a compatibility pass")
    for name in ("generated_count", "disk_count", "declared_disk_count"):
        if type(value.get(name)) is not int or value[name] < 0:
            raise MatcenMismatch(f"Invalid {name}")
    for name in ("authored_segments", "disk_records", "triggers", "runtime_bindings"):
        if not isinstance(value.get(name), list) or any(not isinstance(v, dict) for v in value[name]):
            raise MatcenMismatch(f"Invalid {name}")
    if (
        len(value["authored_segments"]) != value["generated_count"]
        or len(value["runtime_bindings"]) != value["generated_count"]
        or len(value["disk_records"]) != value["disk_count"]
        or value["disk_count"] != value["declared_disk_count"]
    ):
        raise MatcenMismatch("Missing captured segments, bindings or disk records")
    for name in ("authored_segments", "runtime_bindings"):
        rows = value[name]
        pairs = [(row.get("segment"), row.get("generated_ordinal")) for row in rows]
        if any(type(a) is not int or type(b) is not int for a, b in pairs) or len(set(pairs)) != len(pairs):
            raise MatcenMismatch("Missing or duplicate segment/ordinal identity")
        if len({a for a, b in pairs}) != len(pairs) or len({b for a, b in pairs}) != len(pairs):
            raise MatcenMismatch("Segment and ordinal identities must each be unique")
    for row in value["runtime_bindings"]:
        if row.get("station_status") == "matching_station":
            station, record = row.get("station", {}), row.get("effective_record", {})
            if (
                record.get("station") != station.get("index")
                or station.get("segment") != row.get("segment")
                or station.get("type") != 4
                or type(station.get("index")) is not int
            ):
                raise MatcenMismatch("Contradictory supposedly safe station join")
    return value


def compare_capture(reference, actual):
    reference, actual = validate_capture(reference), validate_capture(actual)
    for name in (
        "generated_count",
        "disk_count",
        "declared_disk_count",
        "authored_segments",
        "disk_records",
        "triggers",
    ):
        if reference[name] != actual[name]:
            raise MatcenMismatch(f"Authored {name} changed (ordered links and duplicates are significant)")
    unresolved = []
    for before, after in zip(reference["runtime_bindings"], actual["runtime_bindings"]):
        for key in ("segment", "generated_ordinal"):
            if key not in before or before[key] != after.get(key):
                raise MatcenMismatch(f"Changed binding {key}")
        if before.get("binding_status") != "disk_bound" or before.get("station_status") != "matching_station":
            unresolved.append(
                {
                    "segment": before["segment"],
                    "historical_status": before.get("binding_status"),
                    "current_status": after.get("binding_status"),
                }
            )
            continue
        if after.get("binding_status") != "disk_bound" or after.get("station_status") != "matching_station":
            raise MatcenMismatch("Defined historical disk binding was lost")
        for key in ("effective_ordinal", "segment_station", "station", "eligible_robots"):
            if key not in before or before[key] != after.get(key):
                raise MatcenMismatch(f"Defined historical {key} changed")
        for key in ("robot_flags", "hit_points", "interval"):
            if key not in before.get("effective_record", {}) or before["effective_record"][key] != after.get(
                "effective_record", {}
            ).get(key):
                raise MatcenMismatch(f"Defined historical robot record {key} changed")
    return {"defined_bindings_match": True, "unbound_requires_review": unresolved}


def level_index(document):
    missions = document if isinstance(document, list) else [document]
    result = {}
    for mission in missions:
        name = mission.get("mission_filename", "").casefold()
        if name.endswith((".msn", ".mn2")):
            name = name.rsplit(".", 1)[0]
        game = mission.get("game")
        if game not in ("d1", "d2"):
            raise MatcenMismatch("Missing native game/edition identity")
        for level in mission.get("levels", []):
            key = (game, name, level.get("level_num"), level.get("level_file", "").casefold())
            if key in result:
                raise MatcenMismatch("Duplicate level identity")
            result[key] = level
    return result


def compare_documents(reference, actual):
    expected, observed = level_index(reference), level_index(actual)
    results = []
    for key, level in expected.items():
        if "matcen_regression" not in level:
            continue  # Existing files may deliberately contain only selected reference levels.
        if key not in observed:
            raise MatcenMismatch(f"Missing reference level {key}")
        results.append(
            {"level": key, **compare_capture(level["matcen_regression"], observed[key].get("matcen_regression"))}
        )
    if not results:
        raise MatcenMismatch("No matcen reference cases were compared")
    return results


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("actual", type=Path)
    args = parser.parse_args()
    try:
        results = compare_documents(json.loads(args.reference.read_text()), json.loads(args.actual.read_text()))
        print(json.dumps(results, indent=2))
        if any(row["unbound_requires_review"] for row in results):
            parser.exit(2, "Defined bindings matched; historical unbound/unsafe bindings still require review\n")
    except (MatcenMismatch, KeyError, TypeError) as exc:
        parser.exit(1, f"Matcen regression: {exc}\n")
