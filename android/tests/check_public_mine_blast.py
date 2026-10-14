"""Fail-closed live proximity-mine evidence, with raw cross-engine differences.

The oracle contains only native live observations, recording identity and hashes.
It neither learns expectations from replay nor changes inputs, objects or RNG.
A matching physical trajectory does not excuse a raw identity/reference mismatch.
This bounded diagnostic is not full gameplay or public-replay admission.
"""

import argparse
import copy
import hashlib
import json
import re
import sys
from pathlib import Path

import check_public_effect_phase as strict
import compact_public_replay as compact
import d1_replay_parity as parity
import ordered_public_replay as ordered

SCHEMA = "native-d1-live-mine-v1"
MAX_ORACLE_BYTES = 100000
MAX_FRAMES = 1000
# d1/main/object.h and d2/main/object.h, not a normalization mask
OF_SHOULD_BE_DEAD = 2
META_FIELDS = (
    "frames",
    "mine",
    "mine_signature",
    "mine_pickup",
    "mine_pickup_frame",
    "mine_drop_frame",
    "shot_frame",
    "request_frame",
    "negative_no_missile",
    "initial_player",
    "scenario",
    "game",
    "damage_frame_scope",
)
SOURCES = {
    "capture_sha256",
    "full_recording_sha256",
    "compact_sha256",
    "capture_provenance_sha256",
    "capture_rng_sha256",
}
TRACE_KINDS = {
    "meta",
    "input_view",
    "frame_state",
    "object_state",
    "object_boundary",
    "world_state",
    "world_boundary",
    "door_probe",
    "robot_damage",
    "robot_loot",
    "player_weapon_damage",
}
require, keys, integer, vector = (
    strict.require,
    strict.keys,
    strict.integer,
    strict.vector,
)


def sha(value):
    return hashlib.sha256(compact.canonical(value)).hexdigest()


def digest_value(value, label):
    require(
        isinstance(value, str) and len(value) == 64 and all(c in "0123456789abcdef" for c in value),
        label,
    )


def identity(value, label):
    keys(value, {"slot", "signature", "type", "id", "flags", "parent"}, label)
    for field in ("slot", "signature", "type", "id", "flags"):
        integer(value[field], f"{label}.{field}", 0)
    require(
        value["slot"] < 1000 and value["type"] == 5 and value["id"] == 16,
        "Expected live proximity weapon",
    )
    require(not value["flags"] & OF_SHOULD_BE_DEAD, "Mine is marked dead")
    keys(
        value["parent"],
        {"slot", "signature", "type", "creation_time"},
        label + ".parent",
    )
    for field in value["parent"]:
        integer(value["parent"][field], f"{label}.parent.{field}", 0)
    require(
        value["parent"]["slot"] < 1000 and value["parent"]["type"] == 4,
        "Mine parent is not a player",
    )


def validate_capture(capture):
    require(
        isinstance(capture, dict) and set(META_FIELDS) | {"observations"} <= set(capture),
        "Missing live mine capture fields",
    )
    require(
        capture["game"] == "d1" and capture["scenario"] == "mine-blast",
        "Oracle source must be native D1 mine-blast",
    )
    require(
        capture["damage_frame_scope"] == "first_any_player_damage",
        "Ambiguous player damage timing",
    )
    integer(capture["frames"], "frames", 1, MAX_FRAMES)
    for field in ("mine", "mine_pickup"):
        integer(capture[field], field, 0, 999)
    integer(capture["mine_signature"], "mine_signature", 0)
    integer(capture["mine_pickup_frame"], "mine_pickup_frame", 0, capture["frames"] - 1)
    integer(
        capture["mine_drop_frame"],
        "mine_drop_frame",
        capture["mine_pickup_frame"] + 1,
        capture["frames"] - 1,
    )
    integer(
        capture["request_frame"],
        "request_frame",
        capture["mine_drop_frame"] + 1,
        capture["frames"] - 1,
    )
    require(
        type(capture["negative_no_missile"]) is bool,
        "Missing no-concussion declaration",
    )
    integer(capture["shot_frame"], "shot_frame", -1, capture["frames"] - 1)
    require(
        capture["shot_frame"] == (-1 if capture["negative_no_missile"] else capture["request_frame"]),
        "Invalid concussion shot timing",
    )
    player = capture["initial_player"]
    keys(player, {"slot", "signature", "type"}, "initial player")
    for field in player:
        integer(player[field], "initial player." + field, 0)
    require(player["type"] == 4 and player["slot"] < 1000, "Initial object is not a player")
    rows = capture["observations"]
    require(
        isinstance(rows, list) and len(rows) == capture["frames"],
        "Incomplete live observations",
    )
    first = None
    for frame, row in enumerate(rows):
        require(isinstance(row, dict), "Malformed live observation")
        integer(row.get("f"), "capture frame", frame, frame)
        require(type(row.get("mine_live")) is bool, "Missing mine liveness")
        required = {"mine_identity", "mine_position", "mine_velocity"}
        require(required <= set(row), "Missing actual live mine identity or trajectory")
        if frame < capture["mine_drop_frame"]:
            require(
                not row["mine_live"] and all(row[key] is None for key in required),
                "Mine existed before declared drop",
            )
            continue
        require(row["mine_live"], "Mine missing or dead during declared lifetime")
        item = row["mine_identity"]
        identity(item, "live mine")
        require(
            item["slot"] == capture["mine"] and item["signature"] == capture["mine_signature"],
            "Live mine slot reused or signature changed",
        )
        require(
            {k: item["parent"][k] for k in player} == player,
            "Live mine parent does not bind to observed player",
        )
        if first is None:
            first = item
        require(item == first, "Live mine identity changed during lifetime")
        vector(row["mine_position"], "mine_position")
        vector(row["mine_velocity"], "mine_velocity")
    return capture


def validate_result(value, header, engine):
    keys(
        value,
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
    parity.validate_identity(value, header, engine)
    require(
        value["version"] == 2 and value["level"] == header["level"],
        "Wrong terminal version or level",
    )
    for key in ("version", "level", "difficulty", "frame_count", "game_time64"):
        integer(value[key], "terminal." + key)
    player_fields = [
        "energy",
        "shields",
        "score",
        "lives",
        "laser_level",
        "primary_weapon",
        "secondary_weapon",
        "flags",
        "hostages",
    ]
    keys(
        value["player0"],
        set(player_fields) | {"primary_ammo", "secondary_ammo"},
        "terminal player",
    )
    parity.validate_world_value(
        value["player0"],
        {
            **dict.fromkeys(player_fields, int),
            "primary_ammo": (int,) * 16,
            "secondary_ammo": (int,) * 16,
        },
        "$.player0",
    )
    position_fields = ["segment", "x", "y", "z", "forward_x", "forward_y", "forward_z"]
    keys(value["position"], position_fields, "terminal position")
    parity.validate_world_value(value["position"], dict.fromkeys(position_fields, int), "$.position")
    summary_fields = [
        "robots_alive",
        "robots_killed",
        "hostages_remaining",
        "powerups_remaining",
    ]
    keys(
        value["level_summary"],
        set(summary_fields) | {"control_center_destroyed", "endlevel_completed"},
        "terminal summary",
    )
    parity.validate_world_value(value["level_summary"], dict.fromkeys(summary_fields, int), "$.level_summary")
    for key in ("control_center_destroyed", "endlevel_completed"):
        require(type(value["level_summary"][key]) is bool, "Invalid terminal summary flag")


def read_demo(path, full=False):
    rows = list(strict.strict_records(path, strict.MAX_DEMO_BYTES if full else MAX_ORACLE_BYTES))
    require(len(rows) >= 4, "Incomplete recording")
    header = rows[0]
    require(
        header.get("type") == "header" and set(header) <= compact.HEADER_KEYS,
        "Invalid recording header",
    )
    require(
        header.get("game") == "d1" and header.get("mission") == "d1" and header.get("level") == 3,
        "Expected native D1 level 3 recording",
    )
    require(
        header.get("start_mode") == "new_level" and header.get("rng_mode") == "lcg_state",
        "Expected ordinary start and observational LCG state",
    )
    integer(header.get("frame_count"), "recording frames", 1, MAX_FRAMES)
    integer(header.get("difficulty"), "difficulty", 0, 4)
    require(len(rows) == header["frame_count"] + 3, "Incomplete recording boundaries")
    keys(rows[-1], {"type", "result"}, "recording result")
    require(rows[-1]["type"] == "result", "Missing recording terminal result")
    validate_result(rows[-1]["result"], header, "d1")
    require(rows[-1]["result"].get("level") == 3, "Recording terminal level mismatch")
    batches = ordered.validate(header, rows[1:-2], rows[-2])
    for row in rows:
        compact._no_save(row)
    for row in rows[1:-1]:
        require(
            all(e.get("kind") in compact.DIAGNOSTIC_EVENTS for e in row.get("events", [])),
            "Unknown recording event",
        )
        require(
            full or (not {"state", "diag"} & set(row) and not row.get("events")),
            "Compact input contains telemetry",
        )
    require(
        all(op["k"] == "sample" for b in batches for op in b["ops"]),
        "Mine demo permits only ordinary sampled controls",
    )
    return {"header": header, "batches": batches, "result": rows[-1]["result"]}


def check_actions(capture, demo):
    require(
        demo["header"]["frame_count"] == capture["frames"],
        "Recording/live frame count mismatch",
    )
    drops, shots = [], []
    for batch in demo["batches"]:
        for op in batch["ops"]:
            if op["p"]["db"]:
                require(op["p"]["db"] == 1, "Multiple mine drops in one sample")
                drops.append(batch["f"])
            if op["s"]["f2s"] or op["p"]["f2"]:
                shots.append(batch["f"])
    require(
        drops == [capture["mine_drop_frame"]],
        "Mine drop input disagrees with live capture",
    )
    created = capture["observations"][capture["mine_drop_frame"]]["mine_identity"]["parent"]["creation_time"]
    require(
        created == sum(b["frame_time"] for b in demo["batches"][: capture["mine_drop_frame"]]),
        "Live mine creation time disagrees with input clock",
    )
    require(
        shots == ([] if capture["negative_no_missile"] else [capture["shot_frame"]]),
        "Concussion input disagrees with live capture",
    )


def frame_fingerprint(row, frame, previous_time):
    integer(row.get("f"), "gameplay frame", frame, frame)
    ft = row.get("ft", previous_time)
    integer(ft, "gameplay timestep", 1, ordered.MAX_FRAME_TIME)
    require(
        isinstance(row.get("state"), dict) and isinstance(row.get("rng"), dict),
        "Missing gameplay frame state/RNG",
    )
    ordered._rng(row["rng"])
    parity.validate_frame_diagnostics(row.get("diag"), frame)
    gameplay = {"f": frame, "ft": ft, "state": row["state"], "rng": row["rng"]}
    return ft, sha({**gameplay, "diag": row["diag"]}), sha(gameplay)


def recording_fingerprints(path):
    full, gameplay, ft = [], [], None
    for row in strict.strict_records(path):
        if row.get("type") == "frame":
            ft, raw_hash, gameplay_hash = frame_fingerprint(row, len(full), ft)
            full.append(raw_hash)
            gameplay.append(gameplay_hash)
    return {"full_frame_sha256": full, "gameplay_frame_sha256": gameplay}


def analyze_rng(path, demo):
    # Strict parsing covers both streams; SIM must carry its actual state through
    # every call. A replay-time srand is rejected rather than normalized away
    for row in strict.strict_records(path, strict.MAX_TRACE_BYTES):
        if row.get("type") != "meta":
            for key in (
                "seq",
                "frame",
                "gt",
                "call_count",
                "state_before",
                "state_after",
            ):
                integer(row.get(key), "RNG." + key, 0)
            if row.get("type") == "rand":
                integer(row.get("result"), "RNG.result", 0, 32767)
    events = list(parity.rng_events(path, stream=0))
    current = dict(demo["batches"][0]["pre_rng"])
    for event in events:
        require(event["type"] == "rand", "SIM RNG was reseeded during capture/replay")
        require(
            event["state_before"] == current["s"] and event["call_count"] == current["c"] + 1,
            "SIM RNG state/call chain is not carried",
        )
        expected = (current["s"] * 1103515245 + 12345) & 0xFFFFFFFF
        require(
            event["state_after"] == expected and event["result"] == (expected >> 16) & 32767,
            "Invalid LCG SIM event",
        )
        integer(event["frame"], "RNG frame", 0, demo["header"]["frame_count"])
        current = {"s": expected, "c": current["c"] + 1}
    require(
        current == demo["batches"][-1]["post_rng"],
        "SIM RNG terminal state/calls disagree with captured input tail",
    )
    return {
        "sha256": sha(events),
        "algorithm_sha256": sha(
            [{k: v for k, v in e.items() if k not in ("ctx_obj", "ctx_sig", "ctx_id")} for e in events]
        ),
        "events": len(events),
        "carried_state": current,
        "raw_sha256": parity.digest(path),
    }


def make_oracle(capture_path, full_demo_path, compact_path, provenance_path, rng_path):
    capture = validate_capture(strict.load_json(capture_path))
    full, small = read_demo(full_demo_path, True), read_demo(compact_path)
    require(full == small, "Compaction changed header, decoded input/RNG or terminal result")
    check_actions(capture, full)
    provenance = strict.load_json(provenance_path)
    require(isinstance(provenance, dict), "Missing capture provenance")
    require(
        provenance.get("engine_inputs_unchanged") is True,
        "Capture engine inputs changed",
    )
    digest_value(provenance.get("executable_sha256"), "Missing capture executable hash")
    require(
        isinstance(provenance.get("engine_source_revision"), str) and provenance["engine_source_revision"],
        "Missing engine source revision",
    )
    require(
        isinstance(provenance.get("status"), str) and provenance["status"],
        "Missing provenance qualification",
    )
    source = {
        "capture_sha256": parity.digest(capture_path),
        "full_recording_sha256": parity.digest(full_demo_path),
        "compact_sha256": parity.digest(compact_path),
        "capture_provenance_sha256": parity.digest(provenance_path),
        "capture_rng_sha256": parity.digest(rng_path),
    }
    oracle = {
        "schema": SCHEMA,
        "source": source,
        "provenance": {
            key: provenance[key]
            for key in (
                "status",
                "engine_source_revision",
                "executable_sha256",
                "engine_inputs_unchanged",
            )
        },
        "metadata": {key: copy.deepcopy(capture[key]) for key in META_FIELDS},
        "header": full["header"],
        "controls_sha256": sha(full["batches"]),
        "recording_checks": {
            **recording_fingerprints(full_demo_path),
            "simulation_rng": analyze_rng(rng_path, full),
        },
        "terminal": full["result"],
        "observations": [
            [r["f"], r["mine_identity"], r["mine_position"], r["mine_velocity"]] for r in capture["observations"]
        ],
    }
    validate_oracle(oracle)
    return oracle


def validate_oracle(oracle):
    keys(
        oracle,
        {
            "schema",
            "source",
            "provenance",
            "metadata",
            "header",
            "controls_sha256",
            "terminal",
            "observations",
            "recording_checks",
        },
        "oracle",
    )
    require(oracle["schema"] == SCHEMA, "Unknown live mine oracle schema")
    require(
        len(strict.encoded(oracle)) <= MAX_ORACLE_BYTES,
        "Oracle exceeds 100000-byte budget",
    )
    keys(oracle["source"], SOURCES, "oracle source hashes")
    for key, value in oracle["source"].items():
        digest_value(value, "Invalid " + key)
    digest_value(oracle["controls_sha256"], "Invalid decoded controls hash")
    keys(
        oracle["provenance"],
        {
            "status",
            "engine_source_revision",
            "executable_sha256",
            "engine_inputs_unchanged",
        },
        "oracle provenance",
    )
    require(oracle["provenance"]["engine_inputs_unchanged"] is True, "Changed engine inputs")
    digest_value(oracle["provenance"]["executable_sha256"], "Invalid executable hash")
    for key in ("status", "engine_source_revision"):
        require(
            isinstance(oracle["provenance"][key], str) and oracle["provenance"][key],
            "Missing provenance qualification",
        )
    keys(oracle["metadata"], META_FIELDS, "oracle metadata")
    capture = copy.deepcopy(oracle["metadata"])
    require(isinstance(oracle["observations"], list), "Missing packed live observations")
    capture["observations"] = []
    for row in oracle["observations"]:
        require(isinstance(row, list) and len(row) == 4, "Invalid packed live observation")
        capture["observations"].append(
            dict(
                zip(("f", "mine_identity", "mine_position", "mine_velocity"), row),
                mine_live=row[1] is not None,
            )
        )
    validate_capture(capture)
    checks = oracle["recording_checks"]
    keys(
        checks,
        {"full_frame_sha256", "gameplay_frame_sha256", "simulation_rng"},
        "recorded checks",
    )
    for field in ("full_frame_sha256", "gameplay_frame_sha256"):
        require(
            isinstance(checks[field], list) and len(checks[field]) == capture["frames"],
            "Incomplete recorded frame hashes",
        )
        for value in checks[field]:
            digest_value(value, "Invalid recorded frame hash")
    rng = checks["simulation_rng"]
    keys(
        rng,
        {"sha256", "algorithm_sha256", "events", "carried_state", "raw_sha256"},
        "recorded SIM RNG",
    )
    digest_value(rng["sha256"], "Invalid recorded SIM RNG digest")
    digest_value(rng["algorithm_sha256"], "Invalid recorded algorithm RNG digest")
    require(
        rng["raw_sha256"] == oracle["source"]["capture_rng_sha256"],
        "Recorded SIM RNG provenance mismatch",
    )
    integer(rng["events"], "recorded SIM events", 0)
    ordered._rng(rng["carried_state"])
    header = oracle["header"]
    require(
        isinstance(header, dict)
        and header.get("game") == "d1"
        and header.get("mission") == "d1"
        and header.get("level") == 3
        and header.get("start_mode") == "new_level"
        and header.get("rng_mode") == "lcg_state",
        "Invalid oracle ordinary-start identity",
    )
    require(header.get("frame_count") == capture["frames"], "Oracle frame count mismatch")
    validate_result(oracle["terminal"], header, "d1")
    return capture


def validate_companion(oracle, path):
    capture = validate_oracle(oracle)
    require(
        parity.digest(path) == oracle["source"]["compact_sha256"],
        "Compact input provenance mismatch",
    )
    demo = read_demo(path)
    require(
        demo["header"] == oracle["header"] and demo["result"] == oracle["terminal"],
        "Companion recording identity or terminal changed",
    )
    require(sha(demo["batches"]) == oracle["controls_sha256"], "Companion input/RNG changed")
    check_actions(capture, demo)
    return capture


def raw_identity(slot, obj):
    weapon = obj["weapon"]
    return {
        "slot": slot,
        **{key: obj[key] for key in ("signature", "type", "id", "flags")},
        "parent": {
            "slot": weapon["parent_num"],
            "type": weapon["parent_type"],
            "signature": weapon["parent_signature"],
            "creation_time": weapon["creation_time"],
        },
    }


def read_trace(path, oracle, engine):
    """Require strict JSONL plus every object delta and both complete boundaries."""
    capture = validate_oracle(oracle)
    header, count = oracle["header"], capture["frames"]
    boundaries, seen, meta = {}, 0, False
    full_hashes, gameplay_hashes, ft = [], [], None
    for index, row in enumerate(strict.strict_records(path, strict.MAX_TRACE_BYTES)):
        kind = row.get("type")
        require(kind in TRACE_KINDS, "Unknown or missing raw trace record type")
        require(index != 0 or kind == "meta", "Trace must begin with metadata")
        if kind == "meta":
            require(not meta, "Duplicate trace metadata")
            parity.validate_identity(row, header, "d1")
            require(
                row.get("level") == header["level"] and row.get("start_mode") == "new_level",
                "Wrong trace start",
            )
            meta = True
        elif kind == "frame_state":
            require(
                meta and "restored" in boundaries and "terminal" not in boundaries,
                "Gameplay frame outside object boundaries",
            )
            ft, full_hash, gameplay_hash = frame_fingerprint(row, len(full_hashes), ft)
            full_hashes.append(full_hash)
            gameplay_hashes.append(gameplay_hash)
        elif kind == "object_state":
            require(
                meta and "restored" in boundaries and "terminal" not in boundaries,
                "Object state outside boundaries",
            )
            integer(row.get("f"), "object frame", seen, seen)
            seen += 1
        elif kind == "object_boundary":
            require(meta, "Boundary before metadata")
            phase = row.get("phase")
            require(
                phase in ("restored", "terminal") and phase not in boundaries,
                "Duplicate or unknown object boundary",
            )
            require(
                seen == (0 if phase == "restored" else count),
                "Misplaced or incomplete object boundary",
            )
            require(
                row.get("reset") is True and type(row.get("version")) is int and row["version"] == 2,
                "Invalid object boundary schema",
            )
            integer(row.get("f"), "boundary frame", seen, seen)
            parity.validate_object_storage(row, engine)
            boundaries[phase] = row
    require(len(full_hashes) == count, "Missing gameplay frames")
    require(
        meta and seen == count and set(boundaries) == {"restored", "terminal"},
        "Missing complete object boundaries or frames",
    )
    # Existing engine-schema validation owns delta reconstruction and cardinality
    samples = []
    for frame, row in enumerate(parity.object_states(path, header, engine)):
        samples.append(select_objects(row["objects"], frame, "pre_frame", capture))
    samples.append(select_objects(boundaries["terminal"]["slots"], count, "terminal", capture))
    initial = select_objects(boundaries["restored"]["slots"], 0, "restored", capture)
    require(initial["mine"] is None, "Tracked mine already present at ordinary start")
    return {
        "initial": initial,
        "samples": samples,
        "sha256": parity.digest(path),
        "full_frame_sha256": full_hashes,
        "gameplay_frame_sha256": gameplay_hashes,
    }


def select_objects(objects, frame, phase, capture):
    slot, signature = str(capture["mine"]), capture["mine_signature"]
    player_slot = str(capture["initial_player"]["slot"])
    player = objects.get(player_slot)
    require(
        isinstance(player, dict) and player["type"] == 4 and not player["flags"] & OF_SHOULD_BE_DEAD,
        f"Missing or dead player reference at {frame}",
    )
    candidate = objects.get(slot)
    active = phase != "restored" and frame >= capture["mine_drop_frame"]
    if not active:
        require(
            not (
                candidate and candidate["signature"] == signature and candidate["type"] == 5 and candidate["id"] == 16
            ),
            "Mine precedes drop input",
        )
        mine = None
    else:
        require(isinstance(candidate, dict), f"Missing mine slot at frame {frame}")
        require(candidate["signature"] == signature, f"Reused mine slot at frame {frame}")
        require(
            candidate["type"] == 5
            and candidate["id"] == 16
            and candidate["control_type"] == 9
            and candidate["movement_type"] == 1,
            f"Wrong mine identity at frame {frame}",
        )
        require(
            not candidate["flags"] & OF_SHOULD_BE_DEAD and candidate["lifeleft"] > 0,
            f"Dead mine at frame {frame}",
        )
        parent = candidate["weapon"]
        require(
            parent["parent_num"] == int(player_slot)
            and parent["parent_type"] == player["type"]
            and parent["parent_signature"] == player["signature"],
            f"Mine parent does not bind to actual player at frame {frame}",
        )
        mine = candidate
    return {
        "trace_frame": frame,
        "phase": phase,
        "mine": copy.deepcopy(mine),
        "player": copy.deepcopy(player),
    }


def result(differences):
    return {
        "status": "fail" if differences else "pass",
        "mismatch_count": len(differences),
        "first_mismatch": differences[0] if differences else None,
        "differences": differences,
    }


def compare_live(oracle, trace):
    capture = validate_oracle(oracle)
    physical, identities, references = [], [], []
    initial_player = capture["initial_player"]
    for sample in [trace["initial"], *trace["samples"]]:
        actual = {
            "slot": initial_player["slot"],
            **{key: sample["player"][key] for key in ("signature", "type")},
        }
        references.extend(
            {"trace_frame": sample["trace_frame"], "phase": sample["phase"], **d}
            for d in parity.field_differences(initial_player, actual, "$.player")
        )
    for observation in capture["observations"]:
        frame = observation["f"]
        sample = trace["samples"][frame + 1]
        if frame < capture["mine_drop_frame"]:
            # A mine dropped during dispatch f exists in pre-SIM f already
            continue
        mine = sample["mine"]
        stamp = {
            "capture_frame": frame,
            "trace_frame": frame + 1,
            "phase": sample["phase"],
        }
        expected = {
            "position": observation["mine_position"],
            "velocity": observation["mine_velocity"],
        }
        actual = {"position": mine["pos"], "velocity": mine["physics"]["velocity"]}
        physical.extend({**stamp, **d} for d in parity.field_differences(expected, actual, "$.mine"))
        identities.extend(
            {**stamp, **d}
            for d in parity.field_differences(
                observation["mine_identity"],
                raw_identity(capture["mine"], mine),
                "$.mine_identity",
            )
        )
    checks = {
        "physical_trajectory": result(physical),
        "live_identity": result(identities),
        "player_reference": result(references),
    }
    return {
        "status": "pass" if all(c["status"] == "pass" for c in checks.values()) else "fail",
        "samples_compared": capture["frames"] - capture["mine_drop_frame"],
        "parent_binding": {
            "status": "pass",
            "scope": "each mine parent binds to the actual raw player in its own engine",
        },
        **checks,
    }


def compare_pair(
    oracle,
    compact_path,
    native_path,
    imported_path,
    native_result,
    imported_result,
    native_rng,
    imported_rng,
):
    validate_companion(oracle, compact_path)
    traces = {
        "native": read_trace(native_path, oracle, "d1"),
        "imported": read_trace(imported_path, oracle, "d2"),
    }
    checks = {name: compare_live(oracle, trace) for name, trace in traces.items()}
    demo = read_demo(compact_path)
    recorded = oracle["recording_checks"]
    frame_checks = {
        "native_full": result(
            list(parity.field_differences(recorded["full_frame_sha256"], traces["native"]["full_frame_sha256"]))
        ),
        **{
            name + "_gameplay": result(
                list(
                    parity.field_differences(
                        recorded["gameplay_frame_sha256"],
                        trace["gameplay_frame_sha256"],
                    )
                )
            )
            for name, trace in traces.items()
        },
    }
    rng_checks, rng_value_checks = {}, {}
    for name, path in (("native", native_rng), ("imported", imported_rng)):
        actual = analyze_rng(path, demo)
        fields = ("sha256", "events", "carried_state")
        rng_checks[name] = {
            **result(
                list(
                    parity.field_differences(
                        {k: recorded["simulation_rng"][k] for k in fields},
                        {k: actual[k] for k in fields},
                    )
                )
            ),
            "actual": actual,
        }
        fields = ("algorithm_sha256", "events", "carried_state")
        rng_value_checks[name] = result(
            list(
                parity.field_differences(
                    {k: recorded["simulation_rng"][k] for k in fields},
                    {k: actual[k] for k in fields},
                )
            )
        )
    raw_rng = result(
        list(
            parity.field_differences(
                list(parity.rng_events(native_rng)),
                list(parity.rng_events(imported_rng)),
                "$.simulation_rng",
            )
        )
    )

    summary = {}
    for name, path, engine in (
        ("native", native_result, "d1"),
        ("imported", imported_result, "d2"),
    ):
        raw = strict.load_json(path)
        validate_result(raw, oracle["header"], engine)
        comparable = parity.canonical_result(raw, oracle["header"], engine)
        summary[name] = result(list(parity.field_differences(oracle["terminal"], comparable)))
    raw_mine, raw_reference = [], []
    for left, right in zip(traces["native"]["samples"], traces["imported"]["samples"]):
        stamp = {"trace_frame": left["trace_frame"], "phase": left["phase"]}
        if left["mine"] is not None or right["mine"] is not None:
            raw_mine.extend({**stamp, **d} for d in parity.field_differences(left["mine"], right["mine"], "$.mine"))
            raw_reference.extend(
                {**stamp, **d}
                for d in parity.field_differences(left["player"], right["player"], "$.mine_parent_object")
            )
    report = {
        "schema": SCHEMA + "-report",
        "oracle_sha256": sha(oracle),
        "source": oracle["source"],
        "trace_sha256": {name: trace["sha256"] for name, trace in traces.items()},
        "negative_no_missile": oracle["metadata"]["negative_no_missile"],
        "scope": "live mine trajectory/identity, actual parent binding, raw mine/parent fields, recorded gameplay summaries and SIM RNG; no full simulation/world parity",
        "alignment": "live post-SIM f -> raw pre-SIM f+1; last live frame -> terminal boundary; drop object already exists in pre-SIM drop frame after input dispatch",
        "summary": summary,
        "frames_compared": oracle["metadata"]["frames"],
        "recorded_frames": frame_checks,
        "simulation_rng": rng_checks,
        "simulation_rng_values": rng_value_checks,
        "raw_simulation_rng": raw_rng,
        **checks,
        "raw_mine": result(raw_mine),
        "raw_parent_object": result(raw_reference),
    }
    report["status"] = (
        "pass"
        if all(
            c["status"] == "pass"
            for c in [
                *summary.values(),
                *checks.values(),
                *frame_checks.values(),
                *rng_checks.values(),
                *rng_value_checks.values(),
                raw_rng,
                report["raw_mine"],
                report["raw_parent_object"],
            ]
        )
        else "fail"
    )
    return report


# This is an explicit imported fault fingerprint, never native expected data
KNOWN_MINE_FAULT = {
    "frames": 424,
    "pickup_frame": 377,
    "drop_frame": 399,
    "shot_frame": 407,
    "first_trace_frame": 410,
    "expected_position": [-24236042, -6263558, 11083488],
    "actual_position": [-24263060, -6254575, 11076505],
    "expected_velocity": [79237, -44531, 89760],
    "actual_velocity": [-353071, 99215, -21970],
}


def passed(check):
    return (
        isinstance(check, dict)
        and check.get("status") == "pass"
        and type(check.get("mismatch_count")) is int
        and check.get("mismatch_count") == 0
        and check.get("first_mismatch") is None
        and check.get("differences") == []
    )


MAX_REPORT_BYTES = 8 * 1024 * 1024
JSON_PATH = r"(?:\.[A-Za-z_][A-Za-z_0-9]*|\[(?:0|[1-9][0-9]*)\])"


def validate_check(check, pattern, frames, *, stamp=None, extras=()):
    """Reject contradictory summaries before using a report as causal evidence."""
    base = {"status", "mismatch_count", "first_mismatch", "differences"}
    keys(check, base | set(extras), "comparison check")
    integer(check["mismatch_count"], "mismatch count", 0)
    differences = check["differences"]
    require(
        isinstance(differences, list) and len(differences) == check["mismatch_count"],
        "Mismatch count disagrees with differences",
    )
    require(
        check["status"] == ("fail" if differences else "pass"),
        "Comparison status disagrees with differences",
    )
    require(
        parity.difference(check["first_mismatch"], differences[0] if differences else None) is None,
        "First mismatch disagrees with differences",
    )
    seen = set()
    previous_frame = -1
    for difference in differences:
        stamps = {"trace_frame", "phase"} if stamp else set()
        if stamp == "live":
            stamps.add("capture_frame")
        keys(difference, {"path", "expected", "actual"} | stamps, "field difference")
        path = difference["path"]
        require(
            isinstance(path, str) and re.fullmatch(pattern, path),
            "Unsupported comparison difference path",
        )
        require(
            parity.difference(difference["expected"], difference["actual"]) is not None,
            "Reported difference has equal values",
        )
        frame = difference.get("trace_frame", -1)
        if stamp:
            integer(frame, "difference trace frame", 0, frames)
            phase = difference["phase"]
            require(
                phase in ("restored", "pre_frame", "terminal"),
                "Unknown difference boundary",
            )
            require(
                (phase == "restored" and frame == 0)
                or (phase == "terminal" and frame == frames)
                or (phase == "pre_frame" and frame < frames),
                "Difference phase/frame disagree",
            )
            require(frame >= previous_frame, "Out-of-order frame differences")
            previous_frame = frame
        if stamp == "live":
            integer(difference["capture_frame"], "difference capture frame", 0, frames - 1)
            require(
                frame == difference["capture_frame"] + 1 and difference["phase"] != "restored",
                "Wrong live/pre-frame alignment",
            )
        key = (frame, difference.get("phase"), path)
        require(key not in seen, "Duplicate field difference")
        seen.add(key)
    return differences


def validate_comparison_report(report, oracle):
    fields = {
        "schema",
        "oracle_sha256",
        "source",
        "trace_sha256",
        "negative_no_missile",
        "scope",
        "alignment",
        "summary",
        "frames_compared",
        "recorded_frames",
        "simulation_rng",
        "simulation_rng_values",
        "raw_simulation_rng",
        "native",
        "imported",
        "raw_mine",
        "raw_parent_object",
        "status",
    }
    keys(report, fields, "causal comparison report")
    require(
        len(strict.encoded(report)) <= MAX_REPORT_BYTES,
        "Comparison report exceeds byte budget",
    )
    require(report["schema"] == SCHEMA + "-report", "Unknown comparison report schema")
    digest_value(report["oracle_sha256"], "Missing report oracle digest")
    keys(report["source"], SOURCES, "report source hashes")
    for digest in report["source"].values():
        digest_value(digest, "Invalid report source digest")
    keys(report["trace_sha256"], {"native", "imported"}, "report trace hashes")
    for digest in report["trace_sha256"].values():
        digest_value(digest, "Invalid report trace digest")
    require(type(report["negative_no_missile"]) is bool, "Invalid report role")
    for field in ("scope", "alignment"):
        require(
            isinstance(report[field], str) and report[field],
            "Missing report scope/alignment",
        )
    frames = oracle["metadata"]["frames"]
    integer(report["frames_compared"], "report frames", frames, frames)
    checks = []
    for category, names, pattern in (
        ("summary", ("native", "imported"), r"\$" + JSON_PATH + "+"),
        (
            "recorded_frames",
            ("native_full", "native_gameplay", "imported_gameplay"),
            r"\$(?:\[(?:0|[1-9][0-9]*)\]|\.length)",
        ),
        (
            "simulation_rng_values",
            ("native", "imported"),
            r"\$\.(?:algorithm_sha256|events|carried_state\.[sc])",
        ),
    ):
        keys(report[category], names, category)
        for check in report[category].values():
            validate_check(check, pattern, frames)
            checks.append(check)
    keys(report["simulation_rng"], {"native", "imported"}, "SIM RNG checks")
    for check in report["simulation_rng"].values():
        validate_check(
            check,
            r"\$\.(?:sha256|events|carried_state\.[sc])",
            frames,
            extras={"actual"},
        )
        actual = check["actual"]
        keys(
            actual,
            {"sha256", "algorithm_sha256", "events", "carried_state", "raw_sha256"},
            "actual SIM RNG",
        )
        for key in ("sha256", "algorithm_sha256", "raw_sha256"):
            digest_value(actual[key], "Invalid actual SIM RNG digest")
        integer(actual["events"], "actual SIM event count", 0)
        ordered._rng(actual["carried_state"])
        checks.append(check)
    rng_pattern = r"\$\.simulation_rng\[(0|[1-9][0-9]*)\]\.(?:type|frame|gt|call_count|state_before|state_after|result|seed|ctx_obj|ctx_sig|ctx_id)"
    differences = validate_check(report["raw_simulation_rng"], rng_pattern, frames)
    event_count = max(check["actual"]["events"] for check in report["simulation_rng"].values())
    previous_event = -1
    for difference in differences:
        event = int(re.fullmatch(rng_pattern, difference["path"]).group(1))
        require(
            previous_event <= event < event_count,
            "SIM RNG difference index outside complete event sequence",
        )
        previous_event = event
        for key in ("expected", "actual"):
            if difference["path"].endswith(".type"):
                require(
                    isinstance(difference[key], str),
                    "Invalid RNG event type difference",
                )
            else:
                integer(difference[key], "RNG event field difference")
    checks.append(report["raw_simulation_rng"])
    for name in ("native", "imported"):
        engine = report[name]
        keys(
            engine,
            {
                "status",
                "samples_compared",
                "parent_binding",
                "physical_trajectory",
                "live_identity",
                "player_reference",
            },
            "engine mine report",
        )
        samples = frames - oracle["metadata"]["mine_drop_frame"]
        integer(engine["samples_compared"], "mine samples", samples, samples)
        keys(engine["parent_binding"], {"status", "scope"}, "parent binding")
        require(
            engine["parent_binding"]["status"] in ("pass", "fail")
            and isinstance(engine["parent_binding"]["scope"], str),
            "Invalid parent-binding result",
        )
        for field, pattern, stamp in (
            (
                "physical_trajectory",
                r"\$\.mine\.(?:position|velocity)\[[0-2]\]",
                "live",
            ),
            (
                "live_identity",
                r"\$\.mine_identity\.(?:slot|signature|type|id|flags|parent\.(?:slot|signature|type|creation_time))",
                "live",
            ),
            ("player_reference", r"\$\.player\.(?:slot|signature|type)", "raw"),
        ):
            diffs = validate_check(engine[field], pattern, frames, stamp=stamp)
            for difference in diffs:
                integer(difference["expected"], "expected live field")
                integer(difference["actual"], "actual live field")
                if stamp == "live":
                    require(
                        difference["capture_frame"] >= oracle["metadata"]["mine_drop_frame"],
                        "Live difference precedes mine lifetime",
                    )
        expected = (
            "pass"
            if all(
                engine[k]["status"] == "pass"
                for k in (
                    "physical_trajectory",
                    "live_identity",
                    "player_reference",
                    "parent_binding",
                )
            )
            else "fail"
        )
        require(
            engine["status"] == expected,
            "Engine verdict disagrees with scoped comparisons",
        )
        checks.append(engine)
    for field, root in (
        ("raw_mine", "mine"),
        ("raw_parent_object", "mine_parent_object"),
    ):
        validate_check(report[field], r"\$\." + root + JSON_PATH + "+", frames, stamp="raw")
        checks.append(report[field])
    expected = "pass" if all(check["status"] == "pass" for check in checks) else "fail"
    require(
        report["status"] == expected,
        "Overall raw verdict disagrees with scoped comparisons",
    )


def causal_gate(positive_oracle, positive_demo, positive, negative_oracle, negative_demo, negative):
    """Classify this exact two-run fault without changing either raw verdict."""
    captures = [
        validate_companion(positive_oracle, positive_demo),
        validate_companion(negative_oracle, negative_demo),
    ]
    validate_comparison_report(positive, positive_oracle)
    validate_comparison_report(negative, negative_oracle)
    demos = [read_demo(positive_demo), read_demo(negative_demo)]
    fault = KNOWN_MINE_FAULT
    failures = []

    def gate(condition, label):
        if not condition:
            failures.append(label)

    gate(
        positive_oracle["header"] == negative_oracle["header"],
        "different ordinary-start recording headers",
    )
    expected_metadata = {
        "frames": fault["frames"],
        "mine_pickup_frame": fault["pickup_frame"],
        "mine_drop_frame": fault["drop_frame"],
        "request_frame": fault["shot_frame"],
    }
    for capture in captures:
        gate(
            all(capture.get(k) == v for k, v in expected_metadata.items()),
            "wrong declared fixture timing",
        )
    gate(
        captures[0]["negative_no_missile"] is False and captures[1]["negative_no_missile"] is True,
        "wrong positive/negative capture roles",
    )
    gate(
        {k: v for k, v in positive_oracle["metadata"].items() if k not in ("negative_no_missile", "shot_frame")}
        == {k: v for k, v in negative_oracle["metadata"].items() if k not in ("negative_no_missile", "shot_frame")},
        "pickup/drop/player identity differs",
    )
    gate(
        positive_oracle["observations"] == negative_oracle["observations"],
        "native mine observation differs across intervention",
    )
    shot = fault["shot_frame"]
    for field in ("full_frame_sha256", "gameplay_frame_sha256"):
        gate(
            positive_oracle["recording_checks"][field][:shot] == negative_oracle["recording_checks"][field][:shot],
            "native recorded history differs before concussion: " + field,
        )
    gate(
        all(
            demos[0]["batches"][frame][field] == demos[1]["batches"][frame][field]
            for frame in range(shot)
            for field in ("pre_rng", "post_rng")
        ),
        "native input RNG history differs before concussion",
    )
    raw_controls = list(parity.field_differences(demos[0]["batches"], demos[1]["batches"], "$.batches"))
    input_controls = [{k: v for k, v in b.items() if k not in ("pre_rng", "post_rng")} for b in demos[0]["batches"]]
    negative_controls = [{k: v for k, v in b.items() if k not in ("pre_rng", "post_rng")} for b in demos[1]["batches"]]
    controls = list(parity.field_differences(input_controls, negative_controls, "$.batches"))
    allowed = {
        f"$.batches[{fault['shot_frame']}].ops[0].s.f2s",
        f"$.batches[{fault['shot_frame']}].residual.s.f2s",
    }
    gate(
        len(controls) == 2
        and {d["path"] for d in controls} == allowed
        and all(d["expected"] == 1 and d["actual"] == 0 for d in controls),
        "inputs differ beyond withholding the declared concussion",
    )
    for name, oracle, report in (
        ("positive", positive_oracle, positive),
        ("negative", negative_oracle, negative),
    ):
        require(isinstance(report, dict), "Missing causal comparison report")
        gate(
            report.get("schema") == SCHEMA + "-report"
            and report.get("oracle_sha256") == sha(oracle)
            and report.get("source") == oracle["source"],
            name + " report provenance mismatch",
        )
        gate(
            report.get("frames_compared") == fault["frames"],
            name + " incomplete frame checks",
        )
        gate(
            report.get("negative_no_missile") is (name == "negative"),
            name + " report role mismatch",
        )
        for category, names in (
            ("summary", ("native", "imported")),
            (
                "recorded_frames",
                ("native_full", "native_gameplay", "imported_gameplay"),
            ),
            ("simulation_rng", ("native",)),
            ("simulation_rng_values", ("native", "imported")),
        ):
            section = report.get(category, {})
            gate(
                isinstance(section, dict) and all(passed(section.get(key)) for key in names),
                name + " failed or missing " + category,
            )
        rng = report.get("simulation_rng", {})
        for engine in ("native", "imported"):
            actual = rng.get(engine, {}).get("actual", {})
            gate(
                all(
                    actual.get(k) == oracle["recording_checks"]["simulation_rng"][k]
                    for k in (
                        ("sha256", "algorithm_sha256", "events", "carried_state")
                        if engine == "native"
                        else ("algorithm_sha256", "events", "carried_state")
                    )
                ),
                name + " unverified carried SIM RNG",
            )
        raw_rng = report.get("raw_simulation_rng", {})
        raw_diffs = raw_rng.get("differences", [])
        gate(
            passed(raw_rng)
            or (
                raw_rng.get("status") == "fail"
                and bool(raw_diffs)
                and all(
                    d.get("path", "").endswith(".ctx_sig") and d.get("expected") == 1 and d.get("actual") == 0
                    for d in raw_diffs
                )
            ),
            name + " unexpected strict SIM RNG context divergence",
        )
        for engine in ("native", "imported"):
            engine_report = report.get(engine, {})
            gate(
                engine_report.get("parent_binding", {}).get("status") == "pass",
                name + " invalid parent binding",
            )
            gate(
                engine_report.get("samples_compared") == fault["frames"] - fault["drop_frame"],
                name + " incomplete mine lifetime",
            )
        native = report.get("native", {})
        gate(
            native.get("status") == "pass"
            and all(passed(native.get(k)) for k in ("physical_trajectory", "live_identity", "player_reference")),
            name + " native live reproduction failed",
        )
        imported = report.get("imported", {})
        gate(
            imported.get("status") == "fail" and report.get("status") == "fail",
            name + " erased imported raw failure",
        )
        for category, path, count in (
            (
                "live_identity",
                "$.mine_identity.parent.signature",
                fault["frames"] - fault["drop_frame"],
            ),
            ("player_reference", "$.player.signature", fault["frames"] + 2),
        ):
            section = imported.get(category, {})
            diffs = section.get("differences", [])
            gate(
                section.get("status") == "fail"
                and section.get("mismatch_count") == count
                and len(diffs) == count
                and all(d.get("path") == path and d.get("expected") == 1 and d.get("actual") == 0 for d in diffs),
                name + " unexpected raw reference identity pattern",
            )
        gate(
            all(
                report.get(k, {}).get("status") == "fail" and report.get(k, {}).get("differences")
                for k in ("raw_mine", "raw_parent_object")
            ),
            name + " missing raw mine/reference differences",
        )
    positive_physical = positive.get("imported", {}).get("physical_trajectory", {})
    diffs = positive_physical.get("differences", [])
    first = min((d.get("trace_frame", -1) for d in diffs), default=-1)
    gate(
        positive_physical.get("status") == "fail"
        and first == fault["first_trace_frame"]
        and first > fault["shot_frame"],
        "physical divergence absent or at the wrong boundary",
    )
    first_diffs = {d.get("path"): d for d in diffs if d.get("trace_frame") == fault["first_trace_frame"]}
    for field in ("position", "velocity"):
        for axis in range(3):
            diff = first_diffs.get(f"$.mine.{field}[{axis}]", {})
            gate(
                diff.get("expected") == fault["expected_" + field][axis]
                and diff.get("actual") == fault["actual_" + field][axis],
                "wrong first imported impulse/position signature",
            )
    gate(
        passed(negative.get("imported", {}).get("physical_trajectory")),
        "no-concussion mine moved differently",
    )
    return {
        "schema": SCHEMA + "-causal",
        "status": "known_fault_reproduced" if not failures else "fail",
        "scope": "exact documented concussion-dependent imported proximity-mine impulse; raw parity remains failed and full parity is unqualified",
        "failures": failures,
        "fault_signature": fault,
        "report_sha256": {"positive": sha(positive), "negative": sha(negative)},
        "raw_parity_status": {
            "positive": positive.get("status"),
            "negative": negative.get("status"),
        },
        "strict_simulation_rng_status": {
            "positive": positive.get("raw_simulation_rng", {}).get("status"),
            "negative": negative.get("raw_simulation_rng", {}).get("status"),
        },
        "rng_scope": "algorithm/event timing/calls and carried state checked separately; raw context identity failures are retained",
        "controls": {
            "intervention": "withhold only secondary fire at the declared request frame",
            "raw_input_and_rng_differences": raw_controls,
            "input_differences": controls,
        },
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    create = commands.add_parser("create-oracle")
    for name in (
        "native-capture",
        "native-demo",
        "compact-demo",
        "provenance",
        "native-rng",
        "output",
    ):
        create.add_argument("--" + name, required=True, type=Path)
    check = commands.add_parser("compare")
    for name in (
        "oracle",
        "compact-demo",
        "native-trace",
        "imported-trace",
        "native-result",
        "imported-result",
        "native-rng",
        "imported-rng",
        "output",
    ):
        check.add_argument("--" + name, required=True, type=Path)
    causal = commands.add_parser("causal")
    for name in (
        "positive-oracle",
        "positive-demo",
        "positive-report",
        "negative-oracle",
        "negative-demo",
        "negative-report",
        "output",
    ):
        causal.add_argument("--" + name, required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "create-oracle":
            value = make_oracle(
                args.native_capture,
                args.native_demo,
                args.compact_demo,
                args.provenance,
                args.native_rng,
            )
        elif args.command == "causal":
            value = causal_gate(
                strict.load_json(args.positive_oracle, MAX_ORACLE_BYTES),
                args.positive_demo,
                strict.load_json(args.positive_report, MAX_REPORT_BYTES),
                strict.load_json(args.negative_oracle, MAX_ORACLE_BYTES),
                args.negative_demo,
                strict.load_json(args.negative_report, MAX_REPORT_BYTES),
            )
        else:
            oracle = strict.load_json(args.oracle, MAX_ORACLE_BYTES)
            value = compare_pair(
                oracle,
                args.compact_demo,
                args.native_trace,
                args.imported_trace,
                args.native_result,
                args.imported_result,
                args.native_rng,
                args.imported_rng,
            )
        args.output.write_bytes(strict.encoded(value))
        print(json.dumps({"status": value.get("status", "created"), "output": str(args.output)}))
        return 1 if value.get("status") == "fail" else 0
    except (
        parity.EvidenceError,
        OSError,
        KeyError,
        TypeError,
        IndexError,
        AttributeError,
        RecursionError,
    ) as error:
        invalid = {
            "schema": SCHEMA + "-invalid",
            "status": "invalid",
            "error": str(error),
        }
        try:
            args.output.write_bytes(strict.encoded(invalid))
        except OSError as write_error:
            invalid["output_error"] = "Could not invalidate output: " + str(write_error)
        print(json.dumps(invalid), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
