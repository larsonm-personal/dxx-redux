"""Compact ordinary-start replays with immutable hashes of live recorder evidence."""

import hashlib
import json
import re
from collections import Counter
from pathlib import Path

import d1_replay_parity as parity

MAX_BYTES = 100000
MAX_FRAMES = 1000
PUBLIC_MISSIONS = {"d1": "d1", "d2": "d2demo"}
ORACLE_KEYS = {
    "schema",
    "game",
    "mission",
    "level",
    "frames",
    "full_recording_sha256",
    "compact_sha256",
    "provenance",
    "frame_sha256",
    "simulation_rng_sha256",
    "simulation_rng_events",
    "terminal_sha256",
    "removed_telemetry",
}
PROVENANCE_KEYS = {"source_revision", "capture_executable_sha256", "capture_source_sha256", "asset_sha256"}
HEADER_KEYS = {
    "type",
    "version",
    "game",
    "mission",
    "build_number",
    "git_version",
    "arch",
    "level",
    "difficulty",
    "start_mode",
    "rng_mode",
    "frame_count",
    "classic_preview",
    "player_cfg",
}
FRAME_KEYS = {"type", "f", "ft", "input", "rng", "events", "state", "diag"}
# Reviewed producers: d1/d2 input_demo_hooks, input_demo_energy_trace and collide
# D2 gauges score and robot-damage/rotation probes are observational only
# input_demo_replay.cpp parses only direct_command into executable replay events
DIAGNOSTIC_EVENTS = frozenset(
    {
        "probe_weapon_life",
        "probe_log",
        "probe_ai_schedule",
        "probe_ai_follow_path_transition",
        "probe_ai_awareness_roll",
        "probe_ai_chase_path_gate",
        "probe_fvi_weapon_robot",
        "probe_awareness_source",
        "probe_awareness_entry",
        "probe_awareness_object",
        "probe_awareness_add_return",
        "probe_awareness_post_add",
        "probe_awareness_gate",
        "probe_awareness_result",
        "probe_ai_visibility",
        "weapon_create",
        "player_shot",
        "probe_robot_fire",
        "energy_change",
        "shield_change",
        "player_damage",
        "impact",
        "weapon_robot_path",
        "robot_damage",
        "score",
        "probe_phys_apply_rot",
    }
)


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False, allow_nan=False).encode("utf-8")


def sha(value):
    return hashlib.sha256(canonical(value)).hexdigest()


def _pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise parity.EvidenceError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def _json(text):
    try:
        if isinstance(text, bytes):
            text = text.decode("utf-8")
        return json.loads(
            text, object_pairs_hook=_pairs, parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value))
        )
    except (ValueError, UnicodeError) as error:
        raise parity.EvidenceError(f"Invalid strict JSON: {error}") from error


def _no_save(value):
    if isinstance(value, dict):
        for key, child in value.items():
            if (
                "save" in key.lower()
                or "checkpoint" in key.lower()
                or key.lower() in ("encoding", "compression", "data")
            ):
                raise parity.EvidenceError(f"Save/checkpoint or encoded payload field is forbidden: {key}")
            _no_save(child)
    elif isinstance(value, list):
        for child in value:
            _no_save(child)
    elif isinstance(value, str) and value.lower() in ("save_checkpoint", "dgss", "base64", "zlib"):
        raise parity.EvidenceError("Save/checkpoint or encoded payload is forbidden")


def pinned_assets(game):
    archive = "desc14sw.exe" if game == "d1" else "d2demo10.zip"
    path = Path(__file__).parent / "fixtures/demo_import_oracles.json"
    rows = _json(path.read_bytes())["archives"]
    matches = [row for row in rows if row["archive"] == archive]
    if len(matches) != 1:
        raise parity.EvidenceError("Public asset oracle is ambiguous")
    return {row["file"]: row["sha256"] for row in matches[0]["files"]}


def _digest(value, length=64):
    return isinstance(value, str) and re.fullmatch(rf"[0-9a-f]{{{length}}}", value) is not None


def _provenance(value, game):
    if not isinstance(value, dict) or set(value) != PROVENANCE_KEYS:
        raise parity.EvidenceError("Incomplete or unexpected capture provenance")
    if not _digest(value["source_revision"], 40) or any(
        not _digest(value[key]) for key in ("capture_executable_sha256", "capture_source_sha256")
    ):
        raise parity.EvidenceError("Invalid capture source or executable digest")
    if value["asset_sha256"] != pinned_assets(game):
        raise parity.EvidenceError("Capture assets differ from the pinned public source package")


def _records(path, full):
    raw = Path(path).read_bytes()
    try:
        text = raw.decode("utf-8")
    except UnicodeError as error:
        raise parity.EvidenceError("Replay must be uncompressed UTF-8 JSONL") from error
    rows = [_json(line) for line in text.splitlines() if line.strip()]
    if len(rows) < 3 or any(not isinstance(row, dict) for row in rows):
        raise parity.EvidenceError("Replay needs a header, frames and terminal result")
    for row in rows:
        _no_save(row)
    header = rows[0]
    game = header.get("game")
    count = header.get("frame_count")
    if (
        header.get("type") != "header"
        or set(header) - HEADER_KEYS
        or game not in PUBLIC_MISSIONS
        or header.get("mission") != PUBLIC_MISSIONS[game]
        or type(header.get("level")) is not int
        or header["level"] not in ((1, 2) if game == "d2" else (1, 3))
        or header.get("start_mode") != "new_level"
        or header.get("rng_mode") != "lcg_state"
        or type(count) is not int
        or not 0 < count <= MAX_FRAMES
    ):
        raise parity.EvidenceError("Replay must describe a bounded ordinary public level start")
    if (
        type(header.get("version")) is not int
        or header["version"] not in (2, 3, 4)
        or type(header.get("difficulty")) is not int
        or not 0 <= header["difficulty"] < 5
        or type(header.get("build_number")) is not int
        or header["build_number"] < 0
        or any(not isinstance(header.get(key), str) or not header[key] for key in ("git_version", "arch"))
    ):
        raise parity.EvidenceError("Incomplete or invalid production recorder metadata")
    if (
        len(rows) != count + 2
        or set(rows[-1]) != {"type", "result"}
        or rows[-1]["type"] != "result"
        or not isinstance(rows[-1]["result"], dict)
    ):
        raise parity.EvidenceError("Replay record count or terminal result is invalid")
    frame_time = None
    for index, row in enumerate(rows[1:-1]):
        required = {"type", "f", "input", "rng"} | ({"state", "diag"} if full else set())
        if (
            row.get("type") != "frame"
            or type(row.get("f")) is not int
            or row["f"] != index
            or not required <= set(row)
            or set(row) - FRAME_KEYS
            or (not full and {"state", "diag"} & set(row))
            or not isinstance(row["input"], dict)
            or not isinstance(row["rng"], dict)
            or (full and not isinstance(row["state"], dict))
        ):
            raise parity.EvidenceError("Malformed, nonstandard or out-of-order replay frame")
        frame_time = row.get("ft", frame_time)
        if type(frame_time) is not int or frame_time <= 0:
            raise parity.EvidenceError("Replay frame time is missing or invalid")
        events = row.get("events", [])
        if (
            not isinstance(events, list)
            or any(not isinstance(event, dict) or event.get("kind") not in DIAGNOSTIC_EVENTS for event in events)
            or (events and not full)
        ):
            raise parity.EvidenceError("Only explicitly reviewed diagnostic event kinds may be removed")
    parity.validate_identity(rows[-1]["result"], header, game)
    return raw, rows


def _frame_hashes(path, header, recorded=False):
    hashes = []
    for frame in parity.frames(path, header, header["game"], recorded=recorded):
        parity.validate_frame_diagnostics(frame["diag"], frame["f"])
        hashes.append(sha({key: frame[key] for key in ("f", "ft", "state", "rng", "diag")}))
    return hashes


def _rng_hash(path):
    events = list(parity.rng_events(path, stream=0))
    return sha(events), len(events)


def _capture_build_identity(header, provenance):
    version = header.get("git_version")
    if (
        not isinstance(version, str)
        or not re.fullmatch(r"[0-9a-f]{7,40}", version)
        or not provenance["source_revision"].startswith(version)
    ):
        raise parity.EvidenceError(
            "Capture build revision differs from its exact source; reconfigure CMake and rebuild"
        )


def combat_observations(path):
    events = [row for row in parity.records(path) if row.get("type") in ("robot_damage", "robot_loot")]
    return {"schema": 1, "count": len(events), "sha256": sha(events)}


def door_observations(path):
    events = [row for row in parity.records(path) if row.get("type") == "door_probe"]
    return {"schema": 1, "count": len(events), "sha256": sha(events)}


GUIDEBOT_FIELDS = (
    "Buddy_objnum",
    "Buddy_allowed_to_talk",
    "Escort_goal_object",
    "Escort_special_goal",
    "Escort_goal_index",
)


def guidebot_observations(path, header=None):
    if header is None:
        events = [row for row in parity.records(path) if row.get("type") == "guidebot_observation"]
    else:
        events = []
        for row in parity.world_states(path, header, "d2"):
            if (
                row["type"] == "world_state"
                and row["f"] > 0
                or row["type"] == "world_boundary"
                and row["phase"] == "terminal"
            ):
                escort = row["state"]["ai"].get("escort", {})
                events.append({"type": "guidebot_observation", "version": 1, "f": len(events), **escort})
    for frame, row in enumerate(events):
        if (
            set(row) != {"type", "version", "f", *GUIDEBOT_FIELDS}
            or row["version"] != 1
            or row["f"] != frame
            or any(type(row[key]) is not int for key in ("version", "f", *GUIDEBOT_FIELDS))
        ):
            raise parity.EvidenceError("Incomplete or malformed native Guide-Bot observation")
    return {"schema": 1, "count": len(events), "sha256": sha(events)}


def pack(full_recording, rng_trace, demo_output, oracle_output, provenance, combat_trace=None, door_trace=None, guidebot_trace=None):
    """Derive all expectations from live capture before any replay and write new files."""
    full_recording, demo_output, oracle_output = map(Path, (full_recording, demo_output, oracle_output))
    if (
        len({path.resolve() for path in (full_recording, Path(rng_trace), demo_output, oracle_output)}) != 4
        or demo_output.exists()
        or oracle_output.exists()
    ):
        raise parity.EvidenceError("Pack requires separate, new output files and preserves its live inputs")
    raw, rows = _records(full_recording, full=True)
    header = rows[0]
    _provenance(provenance, header["game"])
    frame_hashes = _frame_hashes(full_recording, header, recorded=True)
    rng_hash, rng_count = _rng_hash(rng_trace)
    telemetry = [{"f": row["f"], "event": event} for row in rows[1:-1] for event in row.get("events", [])]
    compact_rows = [
        {
            key: ([] if key == "events" and row["type"] == "frame" else value)
            for key, value in row.items()
            if row["type"] != "frame" or key not in ("state", "diag")
        }
        for row in rows
    ]
    compact = b"".join(canonical(row) + b"\n" for row in compact_rows)
    oracle = {
        "schema": 1,
        "game": header["game"],
        "mission": header["mission"],
        "level": header["level"],
        "frames": header["frame_count"],
        "full_recording_sha256": hashlib.sha256(raw).hexdigest(),
        "compact_sha256": hashlib.sha256(compact).hexdigest(),
        "provenance": provenance,
        "frame_sha256": frame_hashes,
        "simulation_rng_sha256": rng_hash,
        "simulation_rng_events": rng_count,
        "terminal_sha256": sha(rows[-1]["result"]),
        "removed_telemetry": {
            "counts": dict(Counter(item["event"]["kind"] for item in telemetry)),
            "events": len(telemetry),
            "sha256": sha(telemetry),
            "compared_on_replay": False,
        },
    }
    if combat_trace is not None:
        _capture_build_identity(header, provenance)
        oracle["combat_observations"] = combat_observations(combat_trace)
        if oracle["combat_observations"]["count"] < 2:
            raise parity.EvidenceError("Combat capture requires live damage and loot observations")
    if door_trace is not None:
        _capture_build_identity(header, provenance)
        oracle["door_observations"] = door_observations(door_trace)
        if oracle["door_observations"]["count"] < 2:
            raise parity.EvidenceError("Door capture requires live obstruction observations")
    if guidebot_trace is not None:
        if combat_trace is not None or door_trace is not None or header["game"] != "d2":
            raise parity.EvidenceError("Guide-Bot observations require their own ordinary D2 capture")
        _capture_build_identity(header, provenance)
        oracle["guidebot_observations"] = guidebot_observations(guidebot_trace)
        if oracle["guidebot_observations"]["count"] != header["frame_count"]:
            raise parity.EvidenceError("Guide-Bot capture requires a live observation of every frame")
    encoded_oracle = (json.dumps(oracle, sort_keys=True, indent=4, ensure_ascii=False, allow_nan=False) + "\n").encode(
        "utf-8"
    )
    if len(compact) + len(encoded_oracle) > MAX_BYTES:
        raise parity.EvidenceError("Replay and oracle together exceed 100000 uncompressed UTF-8 bytes")
    with demo_output.open("xb") as stream:
        stream.write(compact)
    with oracle_output.open("xb") as stream:
        stream.write(encoded_oracle)
    return oracle


def validate(demo, oracle):
    """Validate an immutable compact replay and its strict, hash-only oracle."""
    if Path(demo).stat().st_size + Path(oracle).stat().st_size > MAX_BYTES:
        raise parity.EvidenceError("Replay and oracle together exceed 100000 uncompressed UTF-8 bytes")
    raw, rows = _records(demo, full=False)
    expected = _json(Path(oracle).read_bytes())
    if (
        not isinstance(expected, dict)
        or set(expected) not in (ORACLE_KEYS, ORACLE_KEYS | {"combat_observations"}, ORACLE_KEYS | {"door_observations"}, ORACLE_KEYS | {"guidebot_observations"})
        or type(expected["schema"]) is not int
        or expected["schema"] != 1
    ):
        raise parity.EvidenceError("Unexpected compact oracle schema")
    if "combat_observations" in expected:
        combat = expected["combat_observations"]
        if (
            not isinstance(combat, dict)
            or set(combat) != {"schema", "count", "sha256"}
            or type(combat["schema"]) is not int
            or combat["schema"] != 1
            or type(combat["count"]) is not int
            or combat["count"] < 2
            or not _digest(combat["sha256"])
        ):
            raise parity.EvidenceError("Invalid immutable combat observation oracle")
    if "door_observations" in expected:
        doors = expected["door_observations"]
        if (not isinstance(doors, dict) or set(doors) != {"schema", "count", "sha256"}
            or type(doors["schema"]) is not int or doors["schema"] != 1
            or type(doors["count"]) is not int or doors["count"] < 2 or not _digest(doors["sha256"])):
            raise parity.EvidenceError("Invalid immutable door observation oracle")
    if "guidebot_observations" in expected:
        guidebot = expected["guidebot_observations"]
        if (
            not isinstance(guidebot, dict)
            or set(guidebot) != {"schema", "count", "sha256"}
            or type(guidebot["schema"]) is not int
            or guidebot["schema"] != 1
            or type(guidebot["count"]) is not int
            or guidebot["count"] != expected["frames"]
            or not _digest(guidebot["sha256"])
        ):
            raise parity.EvidenceError("Invalid immutable Guide-Bot observation oracle")
    _no_save(expected)
    header = rows[0]
    if "guidebot_observations" in expected and header["game"] != "d2":
        raise parity.EvidenceError("Guide-Bot observation oracle requires ordinary D2 content")
    for key, value in (
        ("game", header["game"]),
        ("mission", header["mission"]),
        ("level", header["level"]),
        ("frames", header["frame_count"]),
    ):
        if type(expected[key]) is not type(value) or expected[key] != value:
            raise parity.EvidenceError("Oracle identity differs from the compact recording")
    _provenance(expected["provenance"], header["game"])
    if "combat_observations" in expected or "door_observations" in expected or "guidebot_observations" in expected:
        _capture_build_identity(header, expected["provenance"])
    telemetry = expected["removed_telemetry"]
    if (
        not isinstance(telemetry, dict)
        or set(telemetry) != {"counts", "events", "sha256", "compared_on_replay"}
        or not isinstance(telemetry["counts"], dict)
        or set(telemetry["counts"]) - DIAGNOSTIC_EVENTS
        or any(type(value) is not int or value <= 0 for value in telemetry["counts"].values())
        or type(telemetry["events"]) is not int
        or telemetry["events"] < 0
        or sum(telemetry["counts"].values()) != telemetry["events"]
        or not _digest(telemetry["sha256"])
        or telemetry["compared_on_replay"] is not False
    ):
        raise parity.EvidenceError("Invalid diagnostic-only telemetry provenance")
    for key in ("full_recording_sha256", "compact_sha256", "simulation_rng_sha256", "terminal_sha256"):
        if not _digest(expected[key]):
            raise parity.EvidenceError("Invalid compact oracle digest")
    if (
        not isinstance(expected["frame_sha256"], list)
        or len(expected["frame_sha256"]) != header["frame_count"]
        or any(not _digest(value) for value in expected["frame_sha256"])
    ):
        raise parity.EvidenceError("Missing or invalid per-frame evidence hashes")
    if type(expected["simulation_rng_events"]) is not int or expected["simulation_rng_events"] < 0:
        raise parity.EvidenceError("Invalid simulation RNG event count")
    if (
        hashlib.sha256(raw).hexdigest() != expected["compact_sha256"]
        or sha(rows[-1]["result"]) != expected["terminal_sha256"]
    ):
        raise parity.EvidenceError("Compact recording or terminal expectation changed")
    return header, expected


def verify(game, demo, oracle, state_trace, rng_trace, actual_result):
    """Compare replay outputs exactly against immutable live-capture expectations."""
    header, expected = validate(demo, oracle)
    if game != header["game"]:
        raise parity.EvidenceError("Requested engine does not match the recorded public game")
    hashes = _frame_hashes(state_trace, header)
    if hashes != expected["frame_sha256"]:
        raise parity.EvidenceError("Replay frame state, RNG or diagnostics differ from live capture")
    rng_hash, rng_count = _rng_hash(rng_trace)
    if (rng_hash, rng_count) != (expected["simulation_rng_sha256"], expected["simulation_rng_events"]):
        raise parity.EvidenceError("Replay simulation RNG event stream differs from live capture")
    actual = _json(Path(actual_result).read_bytes())
    parity.validate_identity(actual, header, game)
    if sha(actual) != expected["terminal_sha256"]:
        raise parity.EvidenceError("Replay terminal result differs from live capture")
    report = {"status": "pass", "frames_compared": len(hashes), "simulation_events_compared": rng_count}
    if "combat_observations" in expected:
        if combat_observations(state_trace) != expected["combat_observations"]:
            raise parity.EvidenceError("Replay combat damage/loot observations differ from live capture")
        report["combat_observations_compared"] = expected["combat_observations"]["count"]
    if "door_observations" in expected:
        if door_observations(state_trace) != expected["door_observations"]:
            raise parity.EvidenceError("Replay door obstruction observations differ from live capture")
        report["door_observations_compared"] = expected["door_observations"]["count"]
    if "guidebot_observations" in expected:
        if guidebot_observations(state_trace, header) != expected["guidebot_observations"]:
            raise parity.EvidenceError("Replay Guide-Bot observations differ from live capture")
        report["guidebot_observations_compared"] = expected["guidebot_observations"]["count"]
    return report
