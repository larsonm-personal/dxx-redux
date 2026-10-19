"""Compare defined historical matcen bindings in existing level metadata.

Never generates or updates references. Logical disk-unbound slots are observations,
not golden robot definitions: their allocated globals can contain previous-level data.
"""

import argparse
import copy
import json
from pathlib import Path


class MatcenMismatch(ValueError):
    pass


def baseline_capture(value):
    """Keep authored/effective state strict while excluding unbound mask observations.

    A physically allocated but logically unbound slot retains its previous level's
    robot mask. Models referenced exclusively by those masks are observations too.
    Never discard binding identity/status, disk records, or models used by them.
    """
    validate_capture(value)
    result = copy.deepcopy(value)

    def robot_types(record):
        words = record.get("robot_flags", [])
        if not isinstance(words, list) or any(type(word) is not int or not 0 <= word <= 0xFFFFFFFF for word in words):
            raise MatcenMismatch("Invalid robot mask observation")
        return {32 * i + bit for i, word in enumerate(words) for bit in range(32) if word & (1 << bit)}

    defined = set()
    observed = set()
    for record in result["disk_records"]:
        defined.update(robot_types(record))
    for binding in result["bindings"]:
        record = resolve_binding(result, binding)["effective_record"]
        if binding.get("binding_status") == "logical_disk_unbound":
            ordinal = binding.get("effective_ordinal")
            if type(ordinal) is not int or ordinal < result["declared_disk_count"]:
                raise MatcenMismatch("Unbound observation refers to a declared disk record")
            observed.update(robot_types(record))
            binding.get("record_overrides", {}).pop("robot_flags", None)
        else:
            defined.update(robot_types(record))
    incidental = observed - defined
    result["robot_models"] = [model for model in result["robot_models"] if model["type"] not in incidental]
    return result


def resolve_binding(capture, binding):
    """Resolve compact deltas without promoting unbound observations to references."""
    ordinal = binding.get("effective_ordinal")
    disk = capture["disk_records"]
    record = dict(disk[ordinal]) if type(ordinal) is int and 0 <= ordinal < len(disk) else {}
    if binding.get("binding_status") in ("invalid_segment", "physical_out_of_bounds"):
        record = {}
    record.update(binding.get("record_overrides", {}))
    station = {"index": record.get("station"), "segment": binding.get("segment"), "type": 4}
    if binding.get("station_status") not in ("matching_station", "wrong_station_type", "different_station_segment"):
        station = {}
    station.update(binding.get("station_overrides", {}))
    words = record.get("robot_flags", [])
    types = {i * 32 + bit for i, word in enumerate(words) for bit in range(32) if word & (1 << bit)}
    return {
        **binding,
        "effective_record": record,
        "station": station,
        "eligible_robots": sorted(
            (robot for robot in capture["robot_models"] if robot["type"] in types), key=lambda robot: robot["type"]
        ),
    }


def validate_capture(value):
    if not isinstance(value, dict) or type(value.get("schema_version")) is not int or value["schema_version"] != 2:
        raise MatcenMismatch("Missing supported matcen capture")
    if value.get("capture_status") != "complete" or value.get("admission") != "loaded":
        raise MatcenMismatch("A rejected, missing or incomplete level is not a compatibility pass")
    if type(value.get("declared_disk_count")) is not int or value["declared_disk_count"] < 0:
        raise MatcenMismatch("Invalid declared_disk_count")
    for name in ("bindings", "disk_records", "triggers", "robot_models"):
        if not isinstance(value.get(name), list) or any(not isinstance(v, dict) for v in value[name]):
            raise MatcenMismatch(f"Invalid {name}")
    if len(value["disk_records"]) != value["declared_disk_count"]:
        raise MatcenMismatch("Missing captured disk records")
    pairs = [(row.get("segment"), row.get("generated_ordinal")) for row in value["bindings"]]
    if any(type(a) is not int or type(b) is not int for a, b in pairs):
        raise MatcenMismatch("Missing segment/ordinal identity")
    if len({a for a, b in pairs}) != len(pairs) or len({b for a, b in pairs}) != len(pairs):
        raise MatcenMismatch("Segment and ordinal identities must each be unique")
    models = [row.get("type") for row in value["robot_models"]]
    if any(type(t) is not int or t < 0 for t in models) or len(set(models)) != len(models):
        raise MatcenMismatch("Missing or duplicate robot model identity")
    for row in value["bindings"]:
        for name in ("record_overrides", "station_overrides"):
            if not isinstance(row.get(name, {}), dict):
                raise MatcenMismatch(f"Invalid {name}")
        resolved = resolve_binding(value, row)
        if row.get("station_status") == "matching_station":
            station, record = resolved["station"], resolved["effective_record"]
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
    for name in ("declared_disk_count", "disk_records", "triggers"):
        if reference[name] != actual[name]:
            raise MatcenMismatch(f"Authored {name} changed (ordered links and duplicates are significant)")
    if len(reference["bindings"]) != len(actual["bindings"]):
        raise MatcenMismatch("Changed generated binding count")
    unresolved = []
    for before, after in zip(reference["bindings"], actual["bindings"]):
        for key in ("segment", "generated_ordinal", "serialized_matcen", "serialized_station"):
            if before.get(key) != after.get(key):
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
        before, after = resolve_binding(reference, before), resolve_binding(actual, after)
        for key in ("effective_ordinal", "segment_station", "station", "eligible_robots"):
            if key not in before or before[key] != after.get(key):
                raise MatcenMismatch(f"Defined historical {key} changed")
        for key in ("robot_flags", "hit_points", "interval"):
            if key not in before["effective_record"] or before["effective_record"][key] != after[
                "effective_record"
            ].get(key):
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
