"""Validate format-5 input order and compact only exact, lossless aliases.

The wire contract mirrors input_demo_dispatch.h and input_demo_fixture.cpp.
Simulation residual held controls inherit frame deltas; operation samples and the
terminal input tail are independently zero-based. Pulses never inherit.
"""

import copy
import json

import d1_replay_parity as parity

MAX_OPERATIONS = 1024
MAX_BATCH_BYTES = 256 * 1024
MAX_RECORD_BYTES = 8 * 1024 * 1024
MAX_FRAME_TIME = 65536
MAX_VIEW_DIMENSION = 16384
MAX_VIEW_PIXELS = 67108864
MAX_VIEW_SCALE = 16 * 65536
MAX_VIEW_DEPTH = 9000
UINT32_MAX = 2**32 - 1
INT32_MIN = -(2**31)
INT32_MAX = 2**31 - 1

AXES = {"p", "h", "b", "f", "x", "z"}
HELD = {"f1s", "f2s", "rvs", "ams"}
D2_HELD = {"ab", "es"}
PULSES = {"f1", "f2", "fl", "db", "cp", "cs", "sw", "rv", "am"}
D2_PULSES = {"tb", "hl"}
FRAME_REQUIRED = {"type", "f", "input", "rng", "dispatch"}
FRAME_OPTIONAL = {"ft", "state", "diag", "events"}
COMMANDS = {
    "guidebot_goal",
    "drop_marker",
    "delete_marker",
    "drop_current_weapon",
    "drop_secondary_weapon",
    "drop_flag",
    "escort_release_control",
    "death_abort",
    "change_difficulty",
    "guidebot_spawn",
    "guidebot_find_secret",
    "guidebot_warp_to_me",
    "guidebot_find_unexplored",
    "select_weapon_exact",
    "enable_cheats",
    "grant_all_weapons",
}
D1_COMMANDS = {"death_abort", "change_difficulty", "select_weapon_exact", "enable_cheats", "grant_all_weapons"}
# Synchronized with INPUT_DEMO_GRANT_ALL_WEAPONS_* in input_demo_dispatch.h
D1_ALL_WEAPONS_GRANTS = {"d1_scourge", "d1_bigred"}
ALL_WEAPONS_GRANTS = D1_ALL_WEAPONS_GRANTS | {"d2_honestbob"}
# Stable native key codes: KEY_1..KEY_0, optional KEY_SHIFTED, and unshifted KEY_R
GUIDEBOT_KEYS = set(range(0x02, 0x0C)) | set(range(0x102, 0x10C)) | {0x13}
PLAYER_MARKER_COUNT = 9
AUTOMAP_PHASES = {"begin", "depths", "active", "close"}
REAR_VIEW_ACTIONS = {"enter", "toggle_off", "arm", "release_off", "reset"}
COCKPIT_MODES = {0, 2, 3}


def _require(condition, message):
    if not condition:
        raise parity.EvidenceError(message)


def _keys(value, required, optional=frozenset(), label="ordered input"):
    _require(
        isinstance(value, dict) and required <= set(value) <= required | optional,
        f"{label} has missing or unsupported fields",
    )


def _integer(value, minimum, maximum, label):
    _require(type(value) is int and minimum <= value <= maximum, f"Invalid {label}")
    return value


def _encoded(value):
    try:
        return json.dumps(value, ensure_ascii=False, separators=(",", ":"), allow_nan=False).encode("utf-8")
    except (TypeError, ValueError, UnicodeError, OverflowError) as error:
        raise parity.EvidenceError("Ordered input is not finite UTF-8 JSON") from error


def _control_keys(game):
    return AXES | HELD | (D2_HELD if game == "d2" else set()), PULSES | (D2_PULSES if game == "d2" else set())


def _state(value, game, previous=None):
    state_keys, _ = _control_keys(game)
    _keys(value, set(), state_keys, "held controls")
    decoded = dict(previous) if previous is not None else dict.fromkeys(sorted(state_keys), 0)
    for key, field in value.items():
        decoded[key] = _integer(field, INT32_MIN if key in AXES else 0, INT32_MAX if key in AXES else 255, f"s.{key}")
    return decoded


def _pulse(value, game):
    _, pulse_keys = _control_keys(game)
    _keys(value, set(), pulse_keys, "control pulses")
    decoded = dict.fromkeys(sorted(pulse_keys), 0)
    for key, field in value.items():
        decoded[key] = _integer(field, 0, 10 if key == "sw" else 255, f"p.{key}")
    return decoded


def _rng(value):
    _keys(value, {"s", "c"}, label="SIM RNG")
    return {key: _integer(value[key], 0, UINT32_MAX, f"rng.{key}") for key in ("s", "c")}


def _command(value, game):
    _require(isinstance(value, dict), "Input command must be an object")
    command = value.get("command")
    _require(
        value.get("kind") == "direct_command"
        and isinstance(command, str)
        and command in (D1_COMMANDS if game == "d1" else COMMANDS),
        "Unknown or unsupported input command",
    )
    fields = {"kind", "command"}
    if command == "guidebot_goal":
        _keys(value, fields | {"special_key", "from_menu"}, label="Guide-Bot command")
        special_key = _integer(value["special_key"], INT32_MIN, INT32_MAX, "Guide-Bot special_key")
        _require(special_key in GUIDEBOT_KEYS, "Unsupported Guide-Bot key")
        _require(type(value["from_menu"]) is bool, "Guide-Bot from_menu must be a boolean")
    elif command == "drop_marker":
        _keys(value, fields | {"player_marker_num", "message"}, label="marker command")
        _integer(value["player_marker_num"], 0, PLAYER_MARKER_COUNT - 1, "player_marker_num")
        message = value["message"]
        _require(isinstance(message, str) and "\0" not in message, "Invalid marker text")
        try:
            size = len(message.encode("utf-8"))
        except UnicodeError as error:
            raise parity.EvidenceError("Marker text must be UTF-8") from error
        _require(size < 40, "Marker text exceeds the supported byte limit")
    elif command == "delete_marker":
        _keys(value, fields | {"player_marker_num", "expected_signature"}, label="delete marker command")
        _integer(value["player_marker_num"], 0, PLAYER_MARKER_COUNT - 1, "player_marker_num")
        _integer(value["expected_signature"], 0, INT32_MAX, "expected_signature")
    elif command == "change_difficulty":
        _keys(value, fields | {"difficulty"}, label="difficulty command")
        _integer(value["difficulty"], 0, 4, "difficulty")
    elif command == "select_weapon_exact":
        _keys(value, fields | {"weapon_class", "weapon_index"}, label="weapon command")
        _integer(value["weapon_class"], 0, 1, "weapon_class")
        _integer(value["weapon_index"], 0, 4 if game == "d1" else 9, "weapon_index")
    elif command == "grant_all_weapons":
        _keys(value, fields | {"grant"}, label="all-weapons command")
        # D2 also records D1-profile play; native replay checks the active profile
        grants = D1_ALL_WEAPONS_GRANTS if game == "d1" else ALL_WEAPONS_GRANTS
        _require(isinstance(value["grant"], str) and value["grant"] in grants, "Unsupported all-weapons grant")
    else:
        _keys(value, fields, label="input command")
    return copy.deepcopy(value)


def _operation(value, game, residual):
    _require(isinstance(value, dict), "Input operation must be an object")
    kind = value.get("k")
    _require(
        isinstance(kind, str)
        and kind in {"reset", "sample", "command", "clear", "controls", "automap", "rear_view", "cockpit"},
        "Unknown executable input operation",
    )
    is_controls = kind in {"sample", "controls"}
    required = {"k", "ft"} | {
        "sample": {"s"},
        "controls": {"s"},
        "command": {"event"},
        "automap": {"phase"},
        "rear_view": {"action"},
        "cockpit": {"current_cockpit", "preferred_cockpit"},
    }.get(kind, set())
    _keys(value, required, {"p"} if is_controls else set(), "input operation")
    result = {"k": kind, "ft": _integer(value["ft"], 0, MAX_FRAME_TIME, "input-time ft")}
    if is_controls:
        result["s"] = dict(residual["s"]) if value["s"] == "=" else _state(value["s"], game)
        result["p"] = _pulse(value.get("p", {}), game)
    elif kind == "command":
        result["event"] = _command(value["event"], game)
    elif kind == "automap":
        _require(isinstance(value["phase"], str) and value["phase"] in AUTOMAP_PHASES, "Unsupported automap phase")
        result["phase"] = value["phase"]
    elif kind == "rear_view":
        _require(
            isinstance(value["action"], str) and value["action"] in REAR_VIEW_ACTIONS, "Unsupported rear-view action"
        )
        result["action"] = value["action"]
    elif kind == "cockpit":
        for field in ("current_cockpit", "preferred_cockpit"):
            result[field] = _integer(value[field], 0, 4, field)
            _require(result[field] in COCKPIT_MODES, "Unsupported ordinary cockpit mode")
    return result


def _batch(row, game, residual):
    batch = row["dispatch"]
    _keys(batch, {"pre_rng", "ops"}, label="input batch")
    _require(
        isinstance(batch["ops"], list) and len(batch["ops"]) <= MAX_OPERATIONS,
        "Invalid or oversized input operation list",
    )
    _require(len(_encoded(batch)) <= MAX_BATCH_BYTES, "Input batch exceeds the supported byte limit")
    post_rng = _rng(row["rng"])
    pre_rng = dict(post_rng) if batch["pre_rng"] == "=" else _rng(batch["pre_rng"])
    operations = [_operation(operation, game, residual) for operation in batch["ops"]]
    return pre_rng, post_rng, operations


def validate(header, frames, tail):
    """Return decoded input batches; callers retain their existing world/oracle checks."""
    _require(
        isinstance(header, dict) and type(header.get("version")) is int and header["version"] == 5,
        "Ordered input requires format 5",
    )
    game = header.get("game")
    _require(isinstance(game, str) and game in {"d1", "d2"}, "Ordered input requires a known game")
    if "rear_view" in header:
        baseline = header["rear_view"]
        _keys(baseline, {"active", "leave_mode", "current_cockpit", "preferred_cockpit"}, label="rear-view baseline")
        for field, value in baseline.items():
            _integer(value, 0, 1 if field in {"active", "leave_mode"} else 4, f"rear-view {field}")
    if "view_projection" in header:
        projection = header["view_projection"]
        maxima = {
            "screen_width": MAX_VIEW_DIMENSION,
            "screen_height": MAX_VIEW_DIMENSION,
            "aspect": MAX_VIEW_SCALE,
            "zoom": MAX_VIEW_SCALE,
            "depth": MAX_VIEW_DEPTH,
            "classic_depth": 1,
            "missile_view_enabled": 0 if game == "d1" else 1,
        }
        _keys(projection, set(maxima), label="view projection")
        for field, value in projection.items():
            _integer(
                value,
                0 if field in {"classic_depth", "missile_view_enabled"} else 1,
                maxima[field],
                f"view projection {field}",
            )
        _require(
            projection["screen_width"] * projection["screen_height"] <= MAX_VIEW_PIXELS,
            "View projection exceeds the supported pixel count",
        )
    count = _integer(header.get("frame_count"), 1, UINT32_MAX, "frame_count")
    _require(isinstance(frames, list) and len(frames) == count, "Ordered frame count differs from the header")
    decoded = []
    previous = None
    frame_time = None
    automap_phase = None
    for index, row in enumerate([*frames, tail]):
        is_tail = index == count
        _keys(
            row, FRAME_REQUIRED, {"events"} if is_tail else FRAME_OPTIONAL, "input tail" if is_tail else "ordered frame"
        )
        _require(row["type"] == ("input_tail" if is_tail else "frame"), "Misplaced input tail or simulation frame")
        _require(_integer(row["f"], 0, UINT32_MAX, "frame index") == index, "Noncontiguous ordered frame index")
        if not is_tail:
            frame_time = _integer(row.get("ft", frame_time), 1, MAX_FRAME_TIME, "simulation ft")
            for name in ("state", "diag"):
                if name in row:
                    _require(isinstance(row[name], dict), f"Frame {name} must be an object")
        events = row.get("events", [])
        _require(
            isinstance(events, list)
            and all(isinstance(event, dict) and event.get("kind") != "direct_command" for event in events),
            "Format-5 executable commands must remain in ordered input",
        )
        _require(len(_encoded(row)) <= MAX_RECORD_BYTES, "Ordered frame or tail exceeds the record limit")
        _keys(row["input"], set(), {"s", "p"}, "residual controls")
        residual = {
            "s": _state(row["input"].get("s", {}), game, None if is_tail else previous),
            "p": _pulse(row["input"].get("p", {}), game),
        }
        pre_rng, post_rng, operations = _batch(row, game, residual)
        for operation in operations:
            if operation["k"] != "automap":
                continue
            phase = operation["phase"]
            expected = {"begin": None, "depths": "begin", "active": "depths"}
            _require(
                automap_phase in {"begin", "depths", "active"}
                if phase == "close"
                else automap_phase == expected[phase],
                "Invalid ordered automap transition",
            )
            automap_phase = None if phase == "close" else phase
        _require(
            automap_phase is None or (is_tail and automap_phase == "active"),
            "Ordered batch ends in an incomplete or unclosed automap transition",
        )
        decoded.append(
            {
                "f": index,
                "is_tail": is_tail,
                "frame_time": None if is_tail else frame_time,
                "pre_rng": pre_rng,
                "post_rng": post_rng,
                "residual": residual,
                "ops": operations,
            }
        )
        previous = residual["s"]
    return decoded


def compact(header, frames, tail):
    """Return copied frames/tail using only aliases proved equal to their source."""
    decoded = validate(header, frames, tail)
    compact_frames, compact_tail = copy.deepcopy(frames), copy.deepcopy(tail)
    for row, batch in zip([*compact_frames, compact_tail], decoded):
        if batch["pre_rng"] == batch["post_rng"]:
            row["dispatch"]["pre_rng"] = "="
        for operation, expanded in zip(row["dispatch"]["ops"], batch["ops"]):
            if expanded["k"] in {"sample", "controls"}:
                if expanded["s"] == batch["residual"]["s"]:
                    operation["s"] = "="
                if not any(expanded["p"].values()):
                    operation.pop("p", None)
    _require(validate(header, compact_frames, compact_tail) == decoded, "Input compaction changed decoded behavior")
    return compact_frames, compact_tail
