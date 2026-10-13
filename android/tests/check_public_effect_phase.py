"""Compare raw monitor phase against a compact oracle made only from native D1.

Camera and ordered-control provenance are independent checks. An imported known
fault still returns failure; no phase offsets, bitmap aliases or identity fields
are removed. This is a bounded diagnostic, not full public replay admission.
"""

import argparse
import copy
import gzip
import json
import math
from pathlib import Path
import sys
import zlib

import d1_replay_parity as parity
import ordered_public_replay as ordered

SCHEMA = "native-d1-effect-phase-v1"
MAX_ORACLE_BYTES = 100000
MAX_CAPTURE_BYTES = 2000000
MAX_DEMO_BYTES = 64 * 1024 * 1024
MAX_TRACE_BYTES = 256 * 1024 * 1024
PHASE_FIELDS = (
    "effect",
    "frame_count",
    "time_left",
    "frame_time",
    "num_frames",
    "flags",
    "changing_wall_texture",
    "selected_bitmap",
    "selected_bitmap_name",
    "frame_bitmap",
    "frame_bitmap_name",
)
CAMERA_FIELDS = (
    "game_time",
    "segment",
    "position",
    "velocity",
    "orientation",
    "rotational_velocity",
    "shields",
)
FRAME_FIELDS = ("f", "frame_time", "aim_alignment", "stable_view", "stable_frames", "waypoint")
IDENTITY_FIELDS = ("game", "mission", "level", "scenario", "capture_role")
METADATA_FIELDS = set(IDENTITY_FIELDS) | {
    "adjacent_monitor",
    "aim",
    "arrival_frame",
    "first_stable_frame",
    "frame_limit",
    "frames",
    "hold",
    "monitor",
    "observation_phase",
    "public_replay_admission",
    "screenshot_events",
    "screenshot_frames",
    "screenshots_uncropped",
    "stable_frames",
    "stable_start_frame",
}
IDENTITIES = {
    "native_d1_reference": ("d1", "d1"),
    "imported_d1_diagnostic": ("d2", "descent"),
}


def require(condition, message):
    if not condition:
        raise parity.EvidenceError(message)


def keys(value, expected, label):
    require(isinstance(value, dict) and set(value) == set(expected), f"{label}: missing or unsupported fields")


def integer(value, label, minimum=None, maximum=None):
    require(type(value) is int, f"{label}: expected integer")
    require(minimum is None or value >= minimum, f"{label}: below minimum")
    require(maximum is None or value <= maximum, f"{label}: above maximum")


def encoded(value):
    return (json.dumps(value, ensure_ascii=True, separators=(",", ":"), allow_nan=False) + "\n").encode("utf-8")


def reject_duplicates(pairs):
    value = {}
    for key, item in pairs:
        require(key not in value, f"Duplicate JSON key: {key}")
        value[key] = item
    return value


def reject_constant(value):
    raise parity.EvidenceError(f"Non-finite JSON value: {value}")


def finite_float(value):
    number = float(value)
    require(math.isfinite(number), f"Non-finite JSON number: {value}")
    return number


def strict_records(path, limit=MAX_DEMO_BYTES):
    """Bound decompressed bytes and reject ambiguous JSON before interpretation."""
    path = Path(path)
    opener = gzip.open if path.suffix == ".gz" else open
    size = 0
    try:
        with opener(path, "rb") as source:
            while True:
                line = source.readline(ordered.MAX_RECORD_BYTES + 1)
                if not line:
                    break
                size += len(line)
                require(len(line) <= ordered.MAX_RECORD_BYTES and size <= limit, "JSONL evidence exceeds byte budget")
                if not line.strip() or line.lstrip().startswith(b"//"):
                    continue
                value = json.loads(
                    line.decode("utf-8"),
                    object_pairs_hook=reject_duplicates,
                    parse_constant=reject_constant,
                    parse_float=finite_float,
                )
                require(isinstance(value, dict), "JSONL record must be an object")
                yield value
    except (OSError, UnicodeError, ValueError, EOFError, zlib.error) as error:
        raise parity.EvidenceError(f"{path}: invalid JSONL evidence: {error}") from error


def validate_terminal(record, header, capture):
    keys(record, {"type", "result"}, "terminal record")
    require(record["type"] == "result", "Missing terminal result")
    result = record["result"]
    keys(
        result,
        {
            "version",
            "game",
            "mission",
            "level",
            "difficulty",
            "frame_count",
            "game_time64",
            "player0",
            "position",
            "level_summary",
        },
        "terminal result",
    )
    for field in ("version", "level", "difficulty", "frame_count", "game_time64"):
        integer(result[field], f"terminal.{field}")
    require(result["version"] == 2, "Unsupported terminal result version")
    parity.validate_identity(result, header, header["game"])
    require(result["level"] == capture["level"], "Terminal level identity mismatch")
    player = result["player0"]
    player_fields = "energy shields score lives laser_level primary_weapon secondary_weapon flags hostages".split()
    keys(player, set(player_fields) | {"primary_ammo", "secondary_ammo"}, "terminal player0")
    parity.validate_world_value(
        player,
        {**dict.fromkeys(player_fields, int), "primary_ammo": (int,) * 16, "secondary_ammo": (int,) * 16},
        "$.result.player0",
    )
    for field in player_fields:
        bits = 16 if field in {"lives", "laser_level", "primary_weapon", "secondary_weapon", "hostages"} else 32
        unsigned = field in {"flags", "hostages"}
        integer(
            player[field],
            f"terminal.player0.{field}",
            0 if unsigned else -(2 ** (bits - 1)),
            2 ** (bits if unsigned else bits - 1) - 1,
        )
    for field in ("primary_ammo", "secondary_ammo"):
        for value in player[field]:
            integer(value, f"terminal.player0.{field}", 0, 65535)
    position_fields = "segment x y z forward_x forward_y forward_z".split()
    keys(result["position"], position_fields, "terminal position")
    parity.validate_world_value(result["position"], dict.fromkeys(position_fields, int), "$.result.position")
    for field, value in result["position"].items():
        integer(value, f"terminal.position.{field}", -(2**31), 2**31 - 1)
    summary_fields = "robots_alive robots_killed hostages_remaining powerups_remaining".split()
    summary = result["level_summary"]
    keys(summary, set(summary_fields) | {"control_center_destroyed", "endlevel_completed"}, "terminal level_summary")
    parity.validate_world_value(summary, dict.fromkeys(summary_fields, int), "$.result.level_summary")
    for field in summary_fields:
        integer(summary[field], f"terminal.level_summary.{field}", -(2**31), 2**31 - 1)
    for field in ("control_center_destroyed", "endlevel_completed"):
        require(type(summary[field]) is bool, f"terminal.level_summary.{field}: expected boolean")
    final = capture["observations"][-1]
    require(result["game_time64"] == final["game_time"], "Terminal capture clock mismatch")
    expected_position = {
        "segment": final["segment"],
        **dict(zip(("x", "y", "z"), final["position"])),
        **dict(zip(("forward_x", "forward_y", "forward_z"), final["orientation"]["forward"])),
    }
    require(result["position"] == expected_position, "Terminal capture pose mismatch")


def load_json(path, limit=MAX_CAPTURE_BYTES):
    path = Path(path)
    require(path.stat().st_size <= limit, f"{path}: exceeds {limit}-byte budget")
    try:
        return json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=reject_duplicates,
            parse_constant=reject_constant,
            parse_float=finite_float,
        )
    except (UnicodeError, ValueError) as error:
        raise parity.EvidenceError(f"{path}: invalid JSON: {error}") from error


def vector(value, label):
    require(isinstance(value, list) and len(value) == 3, f"{label}: expected three coordinates")
    for item in value:
        integer(item, label)


def validate_capture(capture, native=False):
    keys(capture, METADATA_FIELDS | {"initial", "observations"}, "capture")
    role = capture["capture_role"]
    require(isinstance(role, str) and role in IDENTITIES, "Unrecognized capture identity")
    require((capture["game"], capture["mission"]) == IDENTITIES[role], "Capture game/mission identity mismatch")
    require(not native or role == "native_d1_reference", "Oracle source must be native D1")
    integer(capture["level"], "level", 1, 1)
    require(capture["scenario"] == "effect-phase", "Wrong capture scenario")
    require(capture["public_replay_admission"] is False, "Diagnostic capture cannot claim public replay admission")
    require(capture["observation_phase"] == "post_GameProcessFrame_pre_render", "Wrong observation phase")
    require(capture["screenshots_uncropped"] is True, "Expected uncropped screenshot provenance")
    require(
        capture["monitor"] == {"asset": "misc066.abm", "effect": 24, "segment": 11, "side": 0, "texture": 349},
        "Wrong monitor identity",
    )
    require(
        capture["adjacent_monitor"] == {"effect": 26, "segment": 12, "side": 3, "texture": 352},
        "Wrong adjacent monitor identity",
    )
    for monitor in ("monitor", "adjacent_monitor"):
        for field in ("effect", "segment", "side", "texture"):
            integer(capture[monitor][field], f"{monitor}.{field}")
    for name in ("hold", "aim"):
        vector(capture[name], name)
    integer(capture["frame_limit"], "frame_limit", 1, 240)
    integer(capture["frames"], "frames", 1, capture["frame_limit"])
    observations = capture["observations"]
    require(isinstance(observations, list) and len(observations) == capture["frames"], "Missing observation frames")
    previous_time = None
    stable_count = 0
    for index, row in enumerate([capture["initial"], *observations]):
        label = "initial" if index == 0 else f"observations[{index - 1}]"
        keys(row, set(CAMERA_FIELDS) | {"effect24", "effect26"} | (set(FRAME_FIELDS) if index else set()), label)
        for name in ("game_time", "segment", "shields"):
            integer(row[name], f"{label}.{name}")
        for name in ("position", "velocity", "rotational_velocity"):
            vector(row[name], f"{label}.{name}")
        keys(row["orientation"], {"right", "up", "forward"}, f"{label}.orientation")
        for name, value in row["orientation"].items():
            vector(value, f"{label}.orientation.{name}")
        for effect in (24, 26):
            phase = row[f"effect{effect}"]
            keys(phase, PHASE_FIELDS, f"{label}.effect{effect}")
            for name, value in phase.items():
                if name.endswith("_name"):
                    require(isinstance(value, str) and 0 < len(value) <= 128, f"{label}.{name}: invalid bitmap name")
                else:
                    integer(value, f"{label}.effect{effect}.{name}")
            # Identity and phase values are compared below, never silently mapped
            integer(phase["num_frames"], f"{label}.num_frames", 1, 1000)
            integer(phase["frame_count"], f"{label}.frame_count", 0, phase["num_frames"] - 1)
            integer(phase["frame_time"], f"{label}.effect_frame_time", 1)
        if index:
            for name in ("f", "frame_time", "aim_alignment", "stable_frames", "waypoint"):
                integer(row[name], f"{label}.{name}")
            require(row["f"] == index - 1, "Nonsequential observation frames")
            integer(row["frame_time"], f"{label}.frame_time", 1, ordered.MAX_FRAME_TIME)
            require(row["game_time"] == previous_time + row["frame_time"], f"{label}: inconsistent game time")
            require(type(row["stable_view"]) is bool, f"{label}: stable_view must be boolean")
            stable_count = stable_count + 1 if row["stable_view"] else 0
            require(row["stable_frames"] == stable_count, f"{label}: inconsistent stable frame count")
        else:
            require(row["game_time"] == 0, "Ordinary-start capture must begin at game time zero")
        previous_time = row["game_time"]
    for name in ("arrival_frame", "first_stable_frame", "stable_start_frame"):
        integer(capture[name], name, 0, len(observations) - 1)
    integer(capture["stable_frames"], "stable_frames", 30, len(observations))
    require(capture["stable_frames"] == stable_count, "Incomplete terminal stable view")
    require(capture["stable_start_frame"] == len(observations) - stable_count, "Wrong final stable interval")
    require(
        capture["first_stable_frame"] == next((r["f"] for r in observations if r["stable_view"]), -1),
        "Wrong first stable frame",
    )
    require(
        capture["arrival_frame"] == next((r["f"] for r in observations if r["segment"] == 11), -1),
        "Wrong arrival frame",
    )
    events = capture["screenshot_events"]
    require(isinstance(events, list), "Missing screenshot events")
    previous_frame = -1
    landmarks = {
        "first_frame": 0,
        "arrival": capture["arrival_frame"],
        "first_stable": capture["first_stable_frame"],
        "stable_end": len(observations) - 1,
    }
    for event in events:
        keys(event, {"f", "moment"}, "screenshot event")
        integer(event["f"], "screenshot frame", previous_frame + 1, len(observations) - 1)
        moment = event["moment"]
        require(isinstance(moment, str), "Invalid screenshot moment")
        if moment in ("stable_animation_sample", "stable_animation_phase"):
            require(observations[event["f"]]["stable_view"], "Animation screenshot is not stable")
            if moment == "stable_animation_phase":
                require(event["f"] == 170, "Wrong stable animation phase landmark")
        else:
            require(moment in landmarks and event["f"] == landmarks[moment], "Wrong screenshot landmark")
        previous_frame = event["f"]
    require(capture["screenshot_frames"] == [e["f"] for e in events], "Wrong screenshot frame provenance")
    return capture


def read_controls(path, capture):
    # The retained recording supplies controls; pose similarity alone cannot
    # establish that the native and imported captures received the same inputs
    records = list(strict_records(path))
    require(len(records) == capture["frames"] + 3, "Missing or extra recording records")
    header, frames, tail, result = records[0], records[1:-2], records[-2], records[-1]
    require(header.get("type") == "header" and result.get("type") == "result", "Missing recording boundaries")
    for field in ("game", "mission", "level"):
        require(
            type(header.get(field)) is type(capture[field]) and header.get(field) == capture[field],
            f"Recording {field} identity mismatch",
        )
    require(header.get("start_mode") == "new_level", "Controls must come from ordinary new-level capture")
    require(header.get("frame_count") == capture["frames"], "Recording/capture frame count mismatch")
    integer(header.get("difficulty"), "Recording difficulty", 0, 4)
    validate_terminal(result, header, capture)
    batches = ordered.validate(header, frames, tail)
    rows = []
    for row, batch in zip([*frames, tail], batches):
        if not batch["is_tail"]:
            require(
                batch["frame_time"] == capture["observations"][row["f"]]["frame_time"],
                "Recording/capture timestep mismatch",
            )
        require(all(op["k"] == "sample" for op in batch["ops"]), "Unexpected non-flight input operation")
        for control in [batch["residual"], *batch["ops"]]:
            require(not any(control["p"].values()), "Unexpected control pulse")
            require(
                not any(value for key, value in control["s"].items() if key not in ordered.AXES),
                "Unexpected held action",
            )
        # Keep raw deltas and operations, plus explicit effective timestep
        rows.append([row["f"], batch["frame_time"], row["input"], row["dispatch"]["ops"]])
    return {"header": header, "rows": rows}


def validate_controls(controls, capture):
    keys(controls, {"header", "rows"}, "controls")
    header, rows = controls["header"], controls["rows"]
    require(isinstance(header, dict), "Missing control header")
    for field in ("game", "mission", "level"):
        require(
            type(header.get(field)) is type(capture[field]) and header.get(field) == capture[field],
            f"Control {field} identity mismatch",
        )
    require(header.get("start_mode") == "new_level", "Control start is not ordinary new-level")
    require(header.get("frame_count") == capture["frames"], "Control frame count mismatch")
    require(isinstance(rows, list) and len(rows) == capture["frames"] + 1, "Missing control frames")
    records = []
    for index, row in enumerate(rows):
        require(
            isinstance(row, list) and len(row) == 4 and type(row[0]) is int and row[0] == index,
            "Nonsequential control frames",
        )
        timestep = capture["observations"][index]["frame_time"] if index < capture["frames"] else None
        require(type(row[1]) is type(timestep) and row[1] == timestep, "Wrong control timestep")
        # Placeholders validate only the packed input grammar, not RNG history
        record = {
            "type": "frame" if timestep is not None else "input_tail",
            "f": index,
            "input": row[2],
            "rng": {"s": 0, "c": 0},
            "dispatch": {"pre_rng": {"s": 0, "c": 0}, "ops": row[3]},
        }
        if timestep is not None:
            record["ft"] = timestep
        records.append(record)
    ordered.validate(header, records[:-1], records[-1])


def make_oracle(capture, controls, capture_sha256, demo_sha256):
    validate_capture(capture, native=True)
    validate_controls(controls, capture)
    packed = {
        "schema": SCHEMA,
        "source": {"capture_sha256": capture_sha256, "demo_sha256": demo_sha256},
        "phase_fields": list(PHASE_FIELDS),
        "camera_fields": list(CAMERA_FIELDS),
        "frame_fields": list(FRAME_FIELDS),
        "metadata": {key: copy.deepcopy(capture[key]) for key in sorted(METADATA_FIELDS)},
        "initial": pack_row(capture["initial"], False),
        "observations": [pack_row(row, True) for row in capture["observations"]],
        "controls": copy.deepcopy(controls),
    }
    require(len(encoded(packed)) <= MAX_ORACLE_BYTES, "Native oracle exceeds 100000-byte budget")
    return packed


def pack_row(row, frame):
    return [
        [copy.deepcopy(row[key]) for key in CAMERA_FIELDS + (FRAME_FIELDS if frame else ())],
        *[[row[f"effect{effect}"][key] for key in PHASE_FIELDS] for effect in (24, 26)],
    ]


def unpack_row(row, frame):
    require(isinstance(row, list) and len(row) == 3, "Malformed packed observation")
    fields = CAMERA_FIELDS + (FRAME_FIELDS if frame else ())
    require(isinstance(row[0], list) and len(row[0]) == len(fields), "Malformed packed camera")
    result = dict(zip(fields, copy.deepcopy(row[0])))
    for effect, values in zip((24, 26), row[1:]):
        require(isinstance(values, list) and len(values) == len(PHASE_FIELDS), "Malformed packed effect")
        result[f"effect{effect}"] = dict(zip(PHASE_FIELDS, values))
    return result


def unpack_oracle(oracle):
    keys(
        oracle,
        {
            "schema",
            "source",
            "phase_fields",
            "camera_fields",
            "frame_fields",
            "metadata",
            "initial",
            "observations",
            "controls",
        },
        "oracle",
    )
    require(oracle["schema"] == SCHEMA, "Unsupported oracle schema")
    for name, fields in (
        ("phase_fields", PHASE_FIELDS),
        ("camera_fields", CAMERA_FIELDS),
        ("frame_fields", FRAME_FIELDS),
    ):
        require(oracle[name] == list(fields), f"Malformed oracle {name}")
    keys(oracle["source"], {"capture_sha256", "demo_sha256"}, "oracle source")
    for value in oracle["source"].values():
        require(
            isinstance(value, str) and len(value) == 64 and all(c in "0123456789abcdef" for c in value),
            "Malformed oracle source digest",
        )
    keys(oracle["metadata"], METADATA_FIELDS, "oracle metadata")
    require(isinstance(oracle["observations"], list), "Missing oracle observations")
    capture = {
        **copy.deepcopy(oracle["metadata"]),
        "initial": unpack_row(oracle["initial"], False),
        "observations": [unpack_row(row, True) for row in oracle["observations"]],
    }
    validate_capture(capture, native=True)
    keys(oracle["controls"], {"header", "rows"}, "oracle controls")
    validate_controls(oracle["controls"], capture)
    require(len(encoded(oracle)) <= MAX_ORACLE_BYTES, "Native oracle exceeds 100000-byte budget")
    return capture


def differences(expected, actual, path="$", result=None):
    result = [] if result is None else result
    if type(expected) is not type(actual):
        result.append({"path": path, "expected": expected, "actual": actual})
    elif isinstance(expected, dict):
        for key in sorted(expected.keys() | actual.keys()):
            if key not in expected or key not in actual:
                result.append(
                    {
                        "path": f"{path}.{key}",
                        "expected": expected.get(key, "<missing>"),
                        "actual": actual.get(key, "<missing>"),
                    }
                )
            else:
                differences(expected[key], actual[key], f"{path}.{key}", result)
    elif isinstance(expected, list):
        if len(expected) != len(actual):
            result.append({"path": path + ".length", "expected": len(expected), "actual": len(actual)})
        for index, (left, right) in enumerate(zip(expected, actual)):
            differences(left, right, f"{path}[{index}]", result)
    elif expected != actual:
        result.append({"path": path, "expected": expected, "actual": actual})
    return result


def comparison_result(mismatches):
    return {
        "status": "fail" if mismatches else "pass",
        "mismatch_count": len(mismatches),
        "first_mismatch": mismatches[0] if mismatches else None,
        "mismatches": mismatches,
    }


def compare(oracle, capture, controls):
    expected = unpack_oracle(oracle)
    validate_capture(capture)
    validate_controls(controls, capture)
    phases = []
    left_rows, right_rows = (
        [expected["initial"], *expected["observations"]],
        [capture["initial"], *capture["observations"]],
    )
    if len(left_rows) != len(right_rows):
        phases.append({"path": "$.observations.length", "expected": len(left_rows) - 1, "actual": len(right_rows) - 1})
    for index, (left, right) in enumerate(zip(left_rows, right_rows)):
        label = "initial" if index == 0 else f"observations[{index - 1}]"
        for effect in (24, 26):
            for field in PHASE_FIELDS:
                if left[f"effect{effect}"][field] != right[f"effect{effect}"][field]:
                    phases.append(
                        {
                            "path": f"$.{label}.effect{effect}.{field}",
                            "f": None if index == 0 else index - 1,
                            "game_time": right["game_time"],
                            "effect": effect,
                            "field": field,
                            "expected": left[f"effect{effect}"][field],
                            "actual": right[f"effect{effect}"][field],
                        }
                    )

    def camera(value):
        return {
            "metadata": {key: value[key] for key in sorted(METADATA_FIELDS - set(IDENTITY_FIELDS))},
            "initial": {key: value["initial"][key] for key in CAMERA_FIELDS},
            "observations": [{key: row[key] for key in CAMERA_FIELDS + FRAME_FIELDS} for row in value["observations"]],
        }

    result = {
        "scope": "raw effect phase and paired camera/controls diagnostic; no public replay admission",
        "identity": {
            "status": "pass",
            "native": {key: expected[key] for key in IDENTITY_FIELDS},
            "observed": {key: capture[key] for key in IDENTITY_FIELDS},
        },
        "phase": comparison_result(phases),
        "camera": comparison_result(differences(camera(expected), camera(capture))),
        "controls": comparison_result(differences(oracle["controls"]["rows"], controls["rows"])),
        "recording_headers": {"native": oracle["controls"]["header"], "observed": controls["header"]},
        "native_source": oracle["source"],
    }
    result["status"] = (
        "pass" if all(result[name]["status"] == "pass" for name in ("phase", "camera", "controls")) else "fail"
    )
    result["first_mismatch"] = next(
        (result[name]["first_mismatch"] for name in ("phase", "camera", "controls") if result[name]["first_mismatch"]),
        None,
    )
    return result


def known_imported_fault(oracle, capture, controls):
    """Recognize this exact fault without changing the failing parity result.

    Bitmap identities for the next frame come only from the native live samples,
    never from imported output or an assumption that bitmap indices are adjacent
    """
    comparison = compare(oracle, capture, controls)
    native = unpack_oracle(oracle)
    mismatches = []
    if capture["capture_role"] != "imported_d1_diagnostic":
        mismatches.append(
            {"path": "$.capture_role", "expected": "imported_d1_diagnostic", "actual": capture["capture_role"]}
        )
    identities = ("selected_bitmap", "selected_bitmap_name", "frame_bitmap", "frame_bitmap_name")
    for effect in (24, 26):
        key = f"effect{effect}"
        by_frame = {}
        for row in [native["initial"], *native["observations"]]:
            value = row[key]
            identity = {name: value[name] for name in identities}
            previous = by_frame.setdefault(value["frame_count"], identity)
            require(previous == identity, "Native bitmap identity varies within an eclip frame")
        desired = copy.deepcopy(native["initial"][key])
        require(
            desired["time_left"] == desired["frame_time"], "Native startup timer is not the original full-frame timer"
        )
        desired["time_left"] = 0
        differences(desired, capture["initial"][key], f"$.initial.{key}", mismatches)
        for index, (left, right) in enumerate(zip(native["observations"], capture["observations"])):
            desired = copy.deepcopy(left[key])
            next_frame = (desired["frame_count"] + 1) % desired["num_frames"]
            require(
                next_frame in by_frame, "Native capture does not establish every required next-frame bitmap identity"
            )
            desired.update(by_frame[next_frame], frame_count=next_frame)
            differences(desired, right[key], f"$.observations[{index}].{key}", mismatches)
    for name in ("camera", "controls"):
        mismatches.extend({**item, "check": name} for item in comparison[name]["mismatches"])
    if comparison["status"] != "fail":
        mismatches.append({"path": "$.comparison.status", "expected": "fail", "actual": comparison["status"]})
    return {
        "status": "not_matched" if mismatches else "matched",
        "comparison_status": comparison["status"],
        "pattern": "zero startup timers; thereafter exactly one native eclip frame ahead modulo num_frames, equal timers and all other raw fields; paired camera and controls equal",
        "first_mismatch": mismatches[0] if mismatches else None,
        "mismatches": mismatches,
    }


TRACE_FIELDS = {
    "frame_count": "frame_count",
    "time_left": "time_left",
    "flags": "flags",
    "selected_bitmap": "wall_bitmap",
}


def compare_trace(oracle, path, controls, engine):
    """Compare the declared schema-10 subset, without inventing absent fields."""
    native = unpack_oracle(oracle)
    validate_controls(controls, native)
    require(engine in ("d1", "d2"), "Unknown replay engine")
    # world_states owns schema/delta reconstruction; its older loader is
    # permissive, so reject ambiguous or truncated raw JSONL before it runs
    for _ in strict_records(path, MAX_TRACE_BYTES):
        pass
    phase_mismatches, clock_mismatches = [], []
    samples = 0
    for row in parity.world_states(path, controls["header"], engine):
        if row["type"] == "object_boundary":
            continue
        boundary = row.get("phase")
        if row["type"] == "world_boundary":
            expected = native["initial"] if boundary == "restored" else native["observations"][-1]
            expected_time = expected["game_time"]
            phase_frame = None if boundary == "restored" else native["frames"] - 1
        else:
            frame = row["f"]
            expected = native["initial"] if frame == 0 else native["observations"][frame - 1]
            # calc_game_time precedes the trace callback, but do_special_effects
            # follows it, so clock f and eclip phase f-1 are both observed raw
            expected_time = native["observations"][frame]["game_time"]
            phase_frame = None if frame == 0 else frame - 1
        state = row["state"]
        effects = state["effects"]
        require(isinstance(effects, list) and len(effects) > 26, "Trace is missing monitor effects")
        integer(state["globals"]["GameTime64"], "trace GameTime64")
        if state["globals"]["GameTime64"] != expected_time:
            clock_mismatches.append(
                {
                    "path": "$.globals.GameTime64",
                    "trace_frame": row["f"],
                    "boundary": boundary,
                    "expected": expected_time,
                    "actual": state["globals"]["GameTime64"],
                }
            )
        for effect in (24, 26):
            actual = effects[effect]
            require(isinstance(actual, dict), "Malformed trace effect")
            for field, raw_name in TRACE_FIELDS.items():
                require(raw_name in actual, f"Trace effect is missing {raw_name}")
                integer(actual[raw_name], f"trace effect{effect}.{raw_name}")
                if actual[raw_name] != expected[f"effect{effect}"][field]:
                    phase_mismatches.append(
                        {
                            "path": f"$.effects[{effect}].{raw_name}",
                            "trace_frame": row["f"],
                            "boundary": boundary,
                            "capture_frame": phase_frame,
                            "effect": effect,
                            "field": field,
                            "expected": expected[f"effect{effect}"][field],
                            "actual": actual[raw_name],
                        }
                    )
        samples += 1
    require(samples == native["frames"] + 2, "Incomplete trace phase samples")
    result = {
        "scope": "schema-10 raw eclip timers/frame_count/flags/selected wall bitmap only; no full effect or asset parity",
        "engine": engine,
        "alignment": "restored=startup; pre-frame world_state f=startup for f0 or capture observation f-1; terminal=final capture observation; world clock has already advanced to f",
        "field_mapping": {
            "selected_bitmap": "effects[index].wall_bitmap is Textures[Effects[index].changing_wall_texture].index in input_demo_world_trace.cpp"
        },
        "unobserved_phase_fields": [field for field in PHASE_FIELDS if field not in TRACE_FIELDS and field != "effect"],
        "effect_identity": "raw effects array indices 24 and 26; changing_wall_texture and bitmap names are unobserved",
        "samples_compared": samples,
        "phase_scope": comparison_result(phase_mismatches),
        "clock": comparison_result(clock_mismatches),
        "controls": {
            **comparison_result(differences(oracle["controls"]["rows"], controls["rows"])),
            "scope": "companion recording input only; not independent observation of replay dispatch",
        },
        "camera": {"status": "unobserved", "reason": "bounded effect trace mode does not compare player/camera state"},
        "native_source": oracle["source"],
    }
    result["status"] = (
        "pass" if all(result[name]["status"] == "pass" for name in ("phase_scope", "clock", "controls")) else "fail"
    )
    result["first_mismatch"] = next(
        (
            result[name]["first_mismatch"]
            for name in ("phase_scope", "clock", "controls")
            if result[name]["first_mismatch"]
        ),
        None,
    )
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    create = commands.add_parser("create-oracle", help="derive expectations only from a native live capture")
    create.add_argument("--native-capture", required=True, type=Path)
    create.add_argument("--native-demo", required=True, type=Path)
    create.add_argument("--output", required=True, type=Path)
    check = commands.add_parser("compare", help="return nonzero for every phase or provenance mismatch")
    check.add_argument("--oracle", required=True, type=Path)
    check.add_argument("--capture", required=True, type=Path)
    check.add_argument("--demo", required=True, type=Path)
    check.add_argument("--report", type=Path)
    check.add_argument("--expect-known-imported-fault", action="store_true")
    trace = commands.add_parser("compare-trace", help="check the explicitly bounded raw replay phase scope")
    trace.add_argument("--oracle", required=True, type=Path)
    trace.add_argument("--state", required=True, type=Path)
    trace.add_argument("--demo", required=True, type=Path)
    trace.add_argument("--engine", required=True, choices=("d1", "d2"))
    trace.add_argument("--report", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "create-oracle":
            capture = validate_capture(load_json(args.native_capture), native=True)
            controls = read_controls(args.native_demo, capture)
            oracle = make_oracle(capture, controls, parity.digest(args.native_capture), parity.digest(args.native_demo))
            unpack_oracle(oracle)
            args.output.write_bytes(encoded(oracle))
            print(f"PASS: native oracle contains {capture['frames']} frames, {args.output.stat().st_size} bytes")
            return 0
        oracle = load_json(args.oracle, MAX_ORACLE_BYTES)
        if args.command == "compare-trace":
            native = unpack_oracle(oracle)
            controls = read_controls(args.demo, native)
            report = compare_trace(oracle, args.state, controls, args.engine)
            report["observed_source"] = {
                "state_sha256": parity.digest(args.state),
                "demo_sha256": parity.digest(args.demo),
            }
        else:
            capture = validate_capture(load_json(args.capture))
            controls = read_controls(args.demo, capture)
            report = compare(oracle, capture, controls)
            report["observed_source"] = {
                "capture_sha256": parity.digest(args.capture),
                "demo_sha256": parity.digest(args.demo),
            }
            if args.expect_known_imported_fault:
                report["known_imported_fault"] = known_imported_fault(oracle, capture, controls)
        if args.report:
            args.report.write_bytes(encoded(report))
            scope = "phase_scope" if args.command == "compare-trace" else "phase"
            print(
                f"{report['status'].upper()}: {scope}={report[scope]['status']} camera={report['camera']['status']} controls={report['controls']['status']}; report={args.report}"
            )
            if report["first_mismatch"]:
                print(json.dumps(report["first_mismatch"], sort_keys=True))
        else:
            print(json.dumps(report, indent=2, sort_keys=True))
        known_fault_failed = report.get("known_imported_fault", {}).get("status") == "not_matched"
        return 0 if report["status"] == "pass" and not known_fault_failed else 1
    except (OSError, ValueError, TypeError, KeyError, EOFError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        if getattr(args, "report", None):
            try:
                args.report.write_bytes(encoded({"status": "invalid", "error": str(error)}))
            except OSError as report_error:
                print(f"ERROR: could not retain invalid-evidence report: {report_error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
