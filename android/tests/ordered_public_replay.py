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
}
D1_COMMANDS = {"death_abort", "change_difficulty", "select_weapon_exact"}
# Stable native key codes: KEY_1..KEY_0, optional KEY_SHIFTED, and unshifted KEY_R
GUIDEBOT_KEYS = set(range(0x02, 0x0C)) | set(range(0x102, 0x10C)) | {0x13}
PLAYER_MARKER_COUNT = 9


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
    elif command == "change_difficulty":
        _keys(value, fields | {"difficulty"}, label="difficulty command")
        _integer(value["difficulty"], 0, 4, "difficulty")
    elif command == "select_weapon_exact":
        _keys(value, fields | {"weapon_class", "weapon_index"}, label="weapon command")
        _integer(value["weapon_class"], 0, 1, "weapon_class")
        _integer(value["weapon_index"], 0, 4 if game == "d1" else 9, "weapon_index")
    else:
        _keys(value, fields, label="input command")
    return copy.deepcopy(value)


def _operation(value, game, residual):
    _require(isinstance(value, dict), "Input operation must be an object")
    kind = value.get("k")
    _require(
        isinstance(kind, str) and kind in {"reset", "sample", "command", "clear"}, "Unknown executable input operation"
    )
    required = {"k", "ft"} | ({"s"} if kind == "sample" else {"event"} if kind == "command" else set())
    _keys(value, required, {"p"} if kind == "sample" else set(), "input operation")
    result = {"k": kind, "ft": _integer(value["ft"], 0, MAX_FRAME_TIME, "input-time ft")}
    if kind == "sample":
        result["s"] = dict(residual["s"]) if value["s"] == "=" else _state(value["s"], game)
        result["p"] = _pulse(value.get("p", {}), game)
    elif kind == "command":
        result["event"] = _command(value["event"], game)
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
    count = _integer(header.get("frame_count"), 1, UINT32_MAX, "frame_count")
    _require(isinstance(frames, list) and len(frames) == count, "Ordered frame count differs from the header")
    decoded = []
    previous = None
    frame_time = None
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
            if expanded["k"] == "sample":
                if expanded["s"] == batch["residual"]["s"]:
                    operation["s"] = "="
                if not any(expanded["p"].values()):
                    operation.pop("p", None)
    _require(validate(header, compact_frames, compact_tail) == decoded, "Input compaction changed decoded behavior")
    return compact_frames, compact_tail
