"""Prove real replay comparison failures through the complete regression wrappers."""

import argparse
import copy
import re
import hashlib
import os
import shutil
import subprocess
from pathlib import Path

import compact_public_replay as compact
import d1_replay_parity as parity
import public_incoming_robot_hits as incoming
import run_compact_public_replays as runner
import public_door_replay as doors
import ordered_public_replay as ordered


# Qualified fixed-corpus boundaries; a new or shifted failure is never accepted implicitly
CARRY_ABORT_FRAMES = {
    ("d1", "flight"): 61,
    ("d1", "pickup"): 25,
    ("d1", "door"): 25,
    ("d1", "combat"): 105,
    ("d1", "door-lifecycle"): 179,
    ("d1", "loot"): 494,
    ("d2", "flight"): 1,
    ("d2", "pickup"): 11,
    ("d2", "door"): 1,
    ("d2", "shoot-switch"): 59,
    ("d2", "combat"): 182,
    ("d2", "door-lifecycle"): 213,
    ("d2", "guidebot-release"): 98,
}


def input_mutation(records, case, mutation):
    records = copy.deepcopy(records)
    release = next((row["f"] + 1 for row in case["actions"].get("door_events", []) if row["stage"] == 2), None)
    held_keys = sorted({key for row in records[1:-1] for key in row["input"].get("s", {})})
    for record in records[1:-1]:
        if mutation == "no-collect":
            if record["f"] > case["actions"]["loot_frame"]:
                record["input"] = (
                    {"s": {key: 0 for key in ("f", "x", "z", "h", "p", "f1s")}}
                    if record["f"] == case["actions"]["loot_frame"] + 1
                    else {}
                )
        elif case["scenario"] in ("secondary-release-first", "secondary-mouse-first"):
            record["input"] = {"s": {"f2s": 0}}
            for operation in record["dispatch"]["ops"]:
                if operation["k"] == "sample":
                    operation["s"] = {}
                    operation.pop("p", None)
        elif case["scenario"] == "loot":
            # Sparse held input: clear every fire transition without bloating the bounded replay
            if record["f"] == 0 or "f1s" in record["input"].get("s", {}):
                record["input"].setdefault("s", {})["f1s"] = 0
        elif case["scenario"] in ("combat", "guidebot-release"):
            # Preserve the recorded flight/aim route but remove every primary shot
            record["input"].setdefault("s", {})["f1s"] = 0
        elif case["scenario"] == "door-lifecycle":
            # Reach and obstruct the door normally, then remove the clearance controls
            if record["f"] >= release:
                record["input"] = {"s": {key: 0 for key in held_keys}} if record["f"] == release else {}
        else:
            # With no controls from frame zero the ship cannot follow any route
            record["input"] = {}
    return records


def corrupt_corpus(source, destination, cases, mutation):
    """Mutate disposable copies only; admission hashes are not gameplay oracles."""
    shutil.copytree(source, destination)
    for case in cases:
        demo, oracle = (destination / case[key] for key in ("demo", "oracle"))
        expected = compact._json(oracle.read_bytes())
        if mutation == "wrong-oracle":
            digest = expected["frame_sha256"][0]
            expected["frame_sha256"][0] = ("1" if digest[0] == "0" else "0") + digest[1:]
            # A disposable wrong-oracle copy deliberately rebinds its admitted input
            expected["compact_sha256"] = parity.digest(demo)
        elif mutation in ("bad-input", "no-collect"):
            if mutation == "no-collect" and case["scenario"] != "loot":
                continue
            records = input_mutation(list(parity.records(demo)), case, mutation)
            raw = b"".join(compact.canonical(record) + b"\n" for record in records)
            demo.write_bytes(raw)
            expected["compact_sha256"] = hashlib.sha256(raw).hexdigest()
        else:
            raise parity.EvidenceError("Unknown negative-control mutation")
        parity.write_json(oracle, expected)
    # Negative controls must reach the engine, not fail input admission
    if runner.load_corpus(destination) != cases:
        raise parity.EvidenceError("Negative corpus changed its scenario inventory")


def verify_loot_negative(actions, header, state, actual_result, mutation):
    """Require the intended live outcome, not just rejection of a damaged trace."""
    terminal_objects, terminal_world = None, None
    for row in parity.world_states(state, header, "d1"):
        if row.get("phase") == "terminal":
            if row["type"] == "object_boundary":
                terminal_objects = row["slots"]
            elif row["type"] == "world_boundary":
                terminal_world = row["state"]
    if terminal_objects is None or terminal_world is None:
        raise parity.EvidenceError("Loot negative lacks complete terminal world/object evidence")
    player = terminal_world["players"][0]
    if player["laser_level"] != 0 or actual_result["player0"]["laser_level"] != 0:
        raise parity.EvidenceError("Loot negative unexpectedly gained the laser upgrade")
    pickup = terminal_objects.get(str(actions["pickup"]))
    exact_pickup = pickup is not None and tuple(pickup[key] for key in ("type", "id", "signature")) == (
        7,
        3,
        actions["pickup_signature"],
    )
    if mutation == "no-collect":
        if not (
            exact_pickup
            and not pickup["flags"] & 2
            and pickup["lifeleft"] > 0
            and player["shields"] >= 0
            and terminal_world["death"]["active"] == 0
            and player["score"] == actual_result["player0"]["score"] == 500
        ):
            raise parity.EvidenceError(
                "No-collect must leave the exact drop unconsumed and the player alive after the kill"
            )
    else:
        carrier = terminal_objects.get(str(actions["target"]))
        if not (
            carrier is not None
            and tuple(carrier[key] for key in ("type", "id", "signature"))
            == (2, actions["target_id"], actions["target_signature"])
            and carrier["shields"] >= 0
            and not carrier["flags"] & 1
            and not exact_pickup
        ):
            raise parity.EvidenceError("No-fire must leave the original carrier alive without its drop")
        if any(
            row.get("type") == "robot_loot" and row.get("robot_signature") == actions["target_signature"]
            for row in parity.records(state)
        ):
            raise parity.EvidenceError("No-fire unexpectedly resolved the carrier's loot")


def require(condition, message):
    if not condition:
        raise parity.EvidenceError(message)


def early_abort_spec(case, mutation):
    key = (case["game"], case["scenario"])
    if mutation == "no-collect" and key == ("d1", "loot"):
        return "carry", 567
    if mutation != "bad-input":
        return None
    if key in CARRY_ABORT_FRAMES:
        return "carry", CARRY_ABORT_FRAMES[key]
    if key in (("d2", "secondary-release-first"), ("d2", "secondary-mouse-first")):
        return "post-input", 2
    return None


def verify_declared_mutation(args, case, command, mutation, frame, phase):
    require(command.count("-DemoPath") == 1, "Negative run must identify its exact launched recording")
    launched = Path(command[command.index("-DemoPath") + 1]).resolve()
    intended = (args.output / (mutation + "-corpus") / case["demo"]).resolve()
    original_path = args.corpus / case["demo"]
    require(
        launched == intended and launched != original_path.resolve(), "Negative run launched an unintended recording"
    )
    header, expected = compact.validate(original_path, args.corpus / case["oracle"])
    _, changed_oracle = compact.validate(launched, launched.parent / case["oracle"])
    rebound = {**expected, "compact_sha256": parity.digest(launched)}
    require(changed_oracle == rebound, "Input mutation changed immutable gameplay expectations")
    original = list(parity.records(original_path))
    changed = list(parity.records(launched))
    require(
        changed == input_mutation(original, case, mutation),
        "Launched recording is not the declared input-only mutation",
    )
    source_migration = original_path.with_suffix(".migration.json")
    changed_migration = launched.with_suffix(".migration.json")
    require(
        (not source_migration.exists() and not changed_migration.exists())
        or (
            source_migration.is_file()
            and changed_migration.is_file()
            and source_migration.read_bytes() == changed_migration.read_bytes()
        ),
        "Disposable mutation changed conversion history",
    )
    before = ordered.validate(header, original[1:-2], original[-2])
    after = ordered.validate(header, changed[1:-2], changed[-2])
    end = frame + (phase == "post-input")
    require(
        any(a["ops"] != b["ops"] for a, b in zip(before[:end], after[:end])),
        "No executed ordered input was changed before rejection",
    )
    return header, expected, before, after


# Shared result emitter contract: full compact observations, not sparse expected results
PARTIAL_FRAME_SCHEMA = parity.integer_fields(
    "game_time64",
    player0=parity.integer_fields(
        "energy shields score lives laser_level primary_weapon secondary_weapon flags hostages",
        primary_ammo=(int,) * 16,
        secondary_ammo=(int,) * 16,
    ),
    position=parity.integer_fields("segment x y z forward_x forward_y forward_z"),
    level_summary=parity.integer_fields("robots_alive robots_killed hostages_remaining powerups_remaining"),
)


def validate_partial_frame(row, world, objects, elapsed):
    require(
        row.get("source") == "replay" and type(row.get("ft")) is int and row["ft"] > 0,
        "Partial frame lacks replay source or valid timestep",
    )
    parity.validate_world_value(row.get("state"), PARTIAL_FRAME_SCHEMA, "$.frame.state")
    parity.validate_world_value(row.get("rng"), {"s": int, "c": int}, "$.frame.rng")
    state = row["state"]
    summary = state["level_summary"]
    require(
        all(type(summary.get(key)) is bool for key in ("control_center_destroyed", "endlevel_completed")),
        "Partial frame lacks complete level-summary observations",
    )
    require(
        state["game_time64"] == world["globals"]["GameTime64"] == elapsed,
        "Partial frame clock differs from completed simulation time",
    )
    player = world["players"][world["globals"]["Player_num"]]
    ship = objects[str(player["objnum"])]
    observed = state["player0"]
    expected = {key: player[key] for key in "score lives laser_level primary_weapon secondary_weapon flags".split()}
    expected.update(
        energy=player["energy"] >> 16, shields=player["shields"] >> 16, hostages=player["hostages_on_board"]
    )
    for key in ("primary_ammo", "secondary_ammo"):
        expected[key] = player[key] + [0] * (16 - len(player[key]))
    require(observed == expected, "Partial player observation differs from reconstructed world storage")
    expected_position = dict(zip("x y z".split(), ship["pos"]))
    expected_position.update(zip("forward_x forward_y forward_z".split(), ship["orient"][2]))
    expected_position["segment"] = ship["segnum"]
    require(state["position"] == expected_position, "Partial position differs from reconstructed object storage")


def partial_trace(path, header, frame, game):
    frames, boundaries = [], {}
    counts = {"object_state": 0, "world_state": 0}
    metadata = None
    objects, world = {}, {}
    elapsed = 0
    validated_count = 0
    for row in parity.records(path):
        kind = row.get("type")
        if metadata is None:
            require(kind == "meta", "Partial trace must begin with native metadata")
            parity.validate_identity(row, header, game)
            require(
                row.get("source") == "replay"
                and type(row.get("version")) is int
                and row["version"] == 1
                and row.get("start_mode") == header["start_mode"]
                and row.get("level") == header["level"]
                and row.get("diag_version") == parity.FRAME_DIAGNOSTIC_VERSION,
                "Partial trace startup identity or diagnostic schema differs",
            )
            metadata = row
            continue
        require(kind != "meta", "Duplicate partial trace metadata")
        if kind in ("object_boundary", "world_boundary"):
            phase = row.get("phase")
            key = (kind, phase)
            require(
                phase in ("restored", "aborted")
                and key not in boundaries
                and row.get("reset") is True
                and type(row.get("f")) is int
                and row["f"] == (0 if phase == "restored" else frame)
                and len(frames) == (0 if phase == "restored" else frame),
                "Partial trace has a terminal, duplicate or wrong-frame boundary",
            )
            require(
                row.get("version") == (2 if kind == "object_boundary" else 10), "Unsupported partial boundary schema"
            )
            if kind == "object_boundary":
                parity.validate_object_storage(row, game)
                require(row["clock"]["game_time"] == elapsed, "Partial boundary clock differs from completed timesteps")
            else:
                parity.validate_world_state(row["state"], game)
                require(
                    row["state"]["globals"]["GameTime64"] == elapsed,
                    "Partial world boundary clock differs from completed timesteps",
                )
            boundaries[key] = row
        elif kind in ("frame_state", "object_state", "world_state"):
            require(not any(phase == "aborted" for _, phase in boundaries), "Simulation continued after abort")
            require(
                all((name, "restored") in boundaries for name in ("object_boundary", "world_boundary")),
                "Simulation precedes restored-state evidence",
            )
            index = len(frames) if kind == "frame_state" else counts[kind]
            require(
                type(row.get("f")) is int and row["f"] == index and index < frame,
                "Partial trace has missing, repeated or reordered simulation frames",
            )
            if kind == "frame_state":
                require(all(key in row for key in ("ft", "state", "rng", "diag")), "Incomplete partial frame")
                parity.validate_frame_diagnostics(row["diag"], index)
                require(type(row["ft"]) is int and row["ft"] > 0, "Invalid partial frame timestep")
                elapsed += row["ft"]
                frames.append(row)
            else:
                require(
                    row.get("reset") is (index == 0)
                    and type(row.get("version")) is int
                    and row["version"] == (2 if kind == "object_state" else 10),
                    "Partial world/object schema or reset differs",
                )
                if kind == "object_state":
                    parity.validate_object_storage(row, game)
                    for slot, value in row["slots"].items():
                        if value is None:
                            require(slot in objects, "Partial delta deletes an absent object")
                            del objects[slot]
                        else:
                            objects[slot] = value
                    parity.validate_object_cardinality(row, objects)
                else:
                    require(isinstance(row.get("state"), dict), "Missing partial world delta")
                    world.update(row["state"])
                    parity.validate_world_state(world, game)
                counts[kind] += 1
            if len(frames) == validated_count + 1 and all(value == len(frames) for value in counts.values()):
                validate_partial_frame(frames[-1], world, objects, elapsed)
                validated_count += 1
    require(
        metadata is not None
        and len(frames) == validated_count == frame
        and all(value == frame for value in counts.values())
        and set(boundaries)
        == {(kind, phase) for kind in ("object_boundary", "world_boundary") for phase in ("restored", "aborted")},
        "Incomplete restored/aborted pair or simulation prefix",
    )
    return frames, boundaries


def reconstruct_rng(path, batches, frames, boundary, phase, frame):
    raw = list(parity.records(path))
    require(
        raw
        and raw[0].get("type") == "meta"
        and type(raw[0].get("version")) is int
        and raw[0]["version"] == 1
        and raw[0].get("truncated") is False
        and type(raw[0].get("events")) is int
        and raw[0]["events"] == len(raw) - 1,
        "Incomplete or truncated native RNG trace",
    )
    for index, row in enumerate(raw[1:]):
        require(
            type(row.get("seq")) is int
            and row["seq"] == index
            and type(row.get("stream", 0)) is int
            and row.get("stream", 0) in (0, 1),
            "Malformed or reordered native RNG sequence",
        )
    events = list(parity.rng_events(path, stream=0))
    state, count = batches[0]["pre_rng"]["s"], batches[0]["pre_rng"]["c"]
    first_mismatch = None
    event_index = 0
    elapsed = 0
    for index in range(frame + 1):
        wanted = batches[index]["pre_rng"]
        if index and (state, count) != (wanted["s"], wanted["c"]) and first_mismatch is None:
            first_mismatch = index
        if index < frame:
            require(
                frames[index]["rng"] == batches[index]["post_rng"]
                and frames[index]["ft"] == batches[index]["frame_time"],
                "Completed frame differs from its ordered post-input RNG/time boundary",
            )
        while event_index < len(events) and events[event_index].get("frame") == index:
            event = events[event_index]
            require(
                type(event["frame"]) is int
                and type(event["gt"]) is int
                and event.get("has_context", True) is True
                and all(
                    type(event[key]) is int and 0 <= event[key] <= 0xFFFFFFFF
                    for key in ("call_count", "state_before", "state_after")
                ),
                "Malformed native SIM RNG event",
            )
            require(index < frame, "Rejected input batch consumed unexpected SIM RNG")
            require(
                event["gt"] in (elapsed, elapsed + batches[index]["frame_time"]),
                "SIM RNG event has an unobserved game-time context",
            )
            require(event["state_before"] == state, "SIM RNG state_before does not chain")
            if event["type"] == "rand":
                next_state = (state * 0x41C64E6D + 0x3039) & 0xFFFFFFFF
                next_count = (count + 1) & 0xFFFFFFFF
                require(
                    type(event["result"]) is int and event["result"] == ((next_state >> 16) & 0x7FFF),
                    "Forged SIM RNG rand result",
                )
            else:
                require(type(event["seed"]) is int and 0 <= event["seed"] <= 0xFFFFFFFF, "Malformed SIM srand seed")
                next_state, next_count = event["seed"], count
            require(
                (event["state_after"], event["call_count"]) == (next_state, next_count),
                "SIM RNG state/count transition differs from native LCG semantics",
            )
            state, count = next_state, next_count
            event_index += 1
        if index < frame:
            elapsed += batches[index]["frame_time"]
    require(event_index == len(events), "SIM RNG events have reordered or unobserved frame contexts")
    require(
        (phase == "carry" and first_mismatch == frame)
        or (
            phase == "post-input"
            and first_mismatch is None
            and (state, count) == (batches[frame]["pre_rng"]["s"], batches[frame]["pre_rng"]["c"])
            and (state, count) != (batches[frame]["post_rng"]["s"], batches[frame]["post_rng"]["c"])
        ),
        "RNG evidence does not prove the first mismatch at the declared input phase/frame",
    )
    actual = boundary["rng"][0]
    require(
        actual["available"] == 1 and (state, count) == (actual["state"], actual["calls"]),
        "Reconstructed SIM RNG differs from aborted object storage",
    )
    return {"state": state, "count": count, "simulation_events": len(events), "sha256": compact.sha(events)}


def verify_partial_effect(case, mutation, frames, boundaries, expected):
    initial_world = boundaries["world_boundary", "restored"]["state"]
    world = boundaries["world_boundary", "aborted"]["state"]
    objects = boundaries["object_boundary", "aborted"]["slots"]
    initial_objects = boundaries["object_boundary", "restored"]["slots"]
    player = world["players"][0]
    slot = str(player["objnum"])
    ship = objects[slot]
    require(
        world["death"]["active"] == 0
        and player["shields"] >= 0
        and ship["type"] == 4
        and ship["signature"] == initial_objects[slot]["signature"],
        "Partial negative lost its live original player",
    )
    scenario, actions = case["scenario"], case["actions"]
    hashes = [compact.sha({key: row[key] for key in ("f", "ft", "state", "rng", "diag")}) for row in frames]
    if mutation == "no-collect":
        end = actions["loot_frame"] + 2
        require(
            len(hashes) >= end and hashes[:end] == expected["frame_sha256"][:end],
            "No-collect changed the original pre-drop gameplay prefix",
        )
        pickup = objects.get(str(actions["pickup"]))
        require(
            player["score"] == actions["target_score"]
            and player["laser_level"] == 0
            and str(actions["target"]) not in objects
            and pickup is not None
            and (pickup["type"], pickup["id"], pickup["signature"]) == (7, 3, actions["pickup_signature"])
            and pickup["flags"] & 2 == 0
            and pickup["lifeleft"] > 0,
            "Pre-collection divergence lost the exact live drop, kill award or no-gain inventory",
        )
        return "pre-collection divergence; no completed non-collection claim"
    if scenario == "door-lifecycle":
        release = next(row["f"] + 1 for row in actions["door_events"] if row["stage"] == 2)
        require(
            hashes[: release + 1] == expected["frame_sha256"][: release + 1],
            "Door negative changed gameplay before the clearance-control boundary",
        )
    else:
        require(
            all(row["diag"]["player_weapon_count"] == 0 for row in frames),
            "Missing firing input still produced live player weapons",
        )
    if scenario in ("combat", "loot"):
        target = objects.get(str(actions["target"]))
        require(
            target is not None
            and (target["type"], target["id"], target["signature"])
            == (2, actions["target_id"], actions["target_signature"])
            and target["shields"] >= 0
            and not target["flags"] & 1,
            "No-fire partial run did not retain the identified live target",
        )
    elif scenario == "guidebot-release":
        # The fixed public cage has two native sides, also pinned by public_guidebot_replay
        require(
            all(
                world["walls"][target]["hps"] == initial_world["walls"][target]["hps"] > 0
                and not world["walls"][target]["flags"] & 1
                for target in (74, 75)
            ),
            "No-fire partial run did not retain the intact Guide-Bot barrier",
        )
    elif scenario.startswith("secondary-"):
        require(
            player["secondary_ammo"][0] == actions["initial_ammo"], "Missing secondary fire still consumed ammunition"
        )
    else:
        require(ship["physics"]["thrust"] == [0, 0, 0], "Removed movement still produced player thrust")
    return "partial changed-input gameplay observed before native RNG rejection"


def verify_early_abort(row, args, case, mutation, spec):
    phase, frame = spec
    state_path, rng_path, actual = (Path(row["artifacts"][key]) for key in ("state", "rng", "result"))
    require(
        type(row.get("exit_code")) is int
        and row["exit_code"] == 1
        and not actual.exists()
        and state_path.is_file()
        and rng_path.is_file(),
        "Early RNG rejection lacks a result-free native failure",
    )
    failures = list(actual.parent.glob(actual.name + ".failure_*/failure.json"))
    require(len(failures) == 1, "Early RNG rejection requires one native failure archive")
    failure = compact._json(failures[0].read_bytes())
    require(
        failure.get("forced_stop") is False
        and type(failure.get("native_exit_code")) is int
        and failure["native_exit_code"] == 1
        and not (failures[0].parent / "result.actual.json").exists(),
        "Early RNG rejection did not exit naturally without a terminal result",
    )
    reason = (
        f"carried SIM RNG differs before input at frame {frame}"
        if phase == "carry"
        else f"ordered input SIM RNG differs at frame {frame}"
    )
    # Windowed Windows executables retain native diagnostics in gamelog.txt
    # Console executables also emit them through the wrapper's output
    logs = [actual.with_name("runner.log"), failures[0].parent / "gamelog.txt"]
    diagnostics = [
        re.findall(r"Input demo replay stopped: ([^\r\n]+)", path.read_text(encoding="utf-8"))
        for path in logs
        if path.is_file()
    ]
    require(
        any(diagnostics) and all(reasons == [reason] for reasons in diagnostics if reasons),
        "Early failure has an unrelated or ambiguous native diagnostic",
    )
    header, expected, _, batches = verify_declared_mutation(args, case, row["command"], mutation, frame, phase)
    frames, boundaries = partial_trace(state_path, header, frame, case["game"])
    objects = boundaries["object_boundary", "aborted"]
    rng = reconstruct_rng(rng_path, batches, frames, objects, phase, frame)
    effect = verify_partial_effect(case, mutation, frames, boundaries, expected)
    if mutation == "no-collect":
        require(
            compact.combat_observations(state_path) == expected["combat_observations"],
            "Pre-collection divergence changed original identified damage/drop observations",
        )
    normalized_objects = {**objects, "rng": parity.simulation_rng_storage(objects["rng"])}
    return {
        "phase": phase,
        "frame": frame,
        "rng": rng,
        "effect": effect,
        "partial_frames_sha256": compact.sha(frames),
        "aborted_objects_sha256": compact.sha(normalized_objects),
        "aborted_world_sha256": compact.sha(boundaries["world_boundary", "aborted"]),
    }


def verify_failure(report, exit_code, args, cases, mutation):
    identities = {(case["game"], case["scenario"], repeat) for case in cases for repeat in range(2)}
    rows = report.get("native_runs", [])
    if (
        exit_code != 1
        or report.get("status") != "fail"
        or report.get("backend") != args.run_mode
        or report.get("native_only") is not True
        or report.get("paired", {}).get("status") != "not_run"
        or len(rows) != len(identities)
        or {(row.get("game"), row.get("scenario"), row.get("repeat")) for row in rows} != identities
    ):
        raise parity.EvidenceError("Negative run did not fail through the complete expected runtime inventory")
    evidence = []
    for row in rows:
        case = next(case for case in cases if (case["game"], case["scenario"]) == (row["game"], row["scenario"]))
        demo, oracle = (args.corpus / case[key] for key in ("demo", "oracle"))
        state, rng, actual = (Path(row["artifacts"][key]) for key in ("state", "rng", "result"))
        command = row["command"]
        causal_rejection = None
        door_rejection = None
        incoming_rejection = None
        expected_runner = "headless-console" if args.run_mode == "headless" else "visual"
        if row.get("status") != "fail" or command[command.index("-Runner") + 1] != expected_runner:
            raise parity.EvidenceError("Negative run accepted a scene or selected the wrong backend")
        spec = early_abort_spec(case, mutation)
        if spec is not None:
            observed = verify_early_abort(row, args, case, mutation, spec)
            prior = next(
                (
                    item["early_rng_rejection"]
                    for item in evidence
                    if item["game"] == row["game"] and item["scenario"] == row["scenario"]
                ),
                None,
            )
            require(prior is None or prior == observed, "Repeated partial RNG rejection evidence differs")
            evidence.append(
                {
                    "game": row["game"],
                    "scenario": row["scenario"],
                    "repeat": row["repeat"],
                    "passed": True,
                    "early_rng_rejection": observed,
                }
            )
            continue
        if not all(path.is_file() and path.stat().st_size for path in (state, rng, actual)):
            raise parity.EvidenceError("Negative run lacks completed real-engine result/state/RNG artifacts")
        if mutation == "wrong-oracle":
            if (
                row.get("exit_code") != 0
                or row.get("error") != "Replay frame state, RNG or diagnostics differ from live capture"
            ):
                raise parity.EvidenceError("Wrong expected output did not fail at the live-frame comparison")
            # Matched positive using the exact same runtime evidence and original oracle
            compact.verify(case["game"], demo, oracle, state, rng, actual)
            runner.verify_actions(case, demo, state, actual)
        else:
            # A load/start/process failure is not a successful negative control
            log = actual.with_name("runner.log").read_text(encoding="utf-8")
            if row.get("exit_code") != 1 or "Result compare failed" not in log:
                raise parity.EvidenceError("Changed controls did not fail the real terminal comparison")
            header = next(parity.records(demo))
            if len(list(parity.frames(state, header, case["game"]))) != header["frame_count"]:
                raise parity.EvidenceError("Changed-input replay did not run every frame")
            actual_result = compact._json(actual.read_bytes())
            parity.validate_identity(actual_result, header, case["game"])
            recorded_result = list(parity.records(demo))[-1]["result"]
            if compact.sha(actual_result) == compact.sha(recorded_result):
                raise parity.EvidenceError("Changed controls did not change the actual terminal result")
            if case["scenario"] in ("combat", "loot", "guidebot-release"):
                try:
                    runner.verify_actions(case, demo, state, actual)
                except parity.EvidenceError as error:
                    causal_rejection = str(error)
                else:
                    raise parity.EvidenceError(
                        "Changed combat, loot or Guide-Bot controls still passed their causal action gate"
                    )
                if case["scenario"] == "combat":
                    states = [row["state"] for row in parity.frames(state, header, case["game"])] + [actual_result]
                    try:
                        incoming.verify_trace(header, states, state)
                    except parity.EvidenceError as error:
                        incoming_rejection = str(error)
                    else:
                        raise parity.EvidenceError("Changed combat controls still passed the incoming projectile gate")
                if (
                    case["scenario"] == "guidebot-release"
                    and causal_rejection != "Both cage barriers must blast at the exact declared post-frame boundary"
                ):
                    raise parity.EvidenceError("Guide-Bot no-fire controls did not fail the actual cage-blast proof")
            if case["scenario"] == "door-lifecycle":
                states = [row["state"] for row in parity.frames(state, header, case["game"])] + [actual_result]
                try:
                    doors.verify(case["actions"], header, states, state)
                except parity.EvidenceError as error:
                    door_rejection = str(error)
                else:
                    raise parity.EvidenceError("Door without clearance controls still passed its lifecycle gate")
            if case["scenario"] == "loot":
                verify_loot_negative(case["actions"], header, state, actual_result, mutation)
            if mutation == "no-collect":
                end = case["actions"]["loot_frame"] + 2
                expected = compact._json(oracle.read_bytes())
                if compact._frame_hashes(state, header)[:end] != expected["frame_sha256"][:end]:
                    raise parity.EvidenceError("No-collect control changed gameplay before the original physical drop")
                if actual_result["player0"]["laser_level"] != 0 or actual_result["player0"]["score"] != 500:
                    raise parity.EvidenceError(
                        "No-collect control did not preserve the kill while preventing inventory gain"
                    )
                if "exact new laser pickup" not in (causal_rejection or ""):
                    raise parity.EvidenceError("No-collect control failed for a reason other than physical collection")
        evidence.append(
            {
                "game": row["game"],
                "scenario": row["scenario"],
                "repeat": row["repeat"],
                "passed": True,
                **(
                    {
                        (
                            "guidebot_causal_rejection"
                            if case["scenario"] == "guidebot-release"
                            else "combat_causal_rejection"
                        ): causal_rejection
                    }
                    if causal_rejection is not None
                    else {}
                ),
                **({"door_lifecycle_rejection": door_rejection} if door_rejection is not None else {}),
                **({"incoming_hit_rejection": incoming_rejection} if incoming_rejection is not None else {}),
            }
        )
    return evidence


def run(args):
    cases = runner.load_corpus(args.corpus)
    originals = {path.name: parity.digest(path) for path in args.corpus.iterdir() if path.is_file()}
    args.output.mkdir(parents=True, exist_ok=False)
    with (args.output / "producer.lock").open("w", encoding="ascii") as lease:
        if os.name == "posix":
            import fcntl

            fcntl.flock(lease.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        return _run_controls(args, cases, originals)


def _run_controls(args, cases, originals):
    report = {"backend": args.run_mode, "status": "pending", "case_count": len(cases), "controls": []}
    parity.write_json(args.output / "report.json", report)
    for mutation in ("wrong-oracle", "bad-input"):
        changed = args.output / (mutation + "-corpus")
        corrupt_corpus(args.corpus, changed, cases, mutation)
        output = args.output / (mutation + "-run")
        command = [
            args.pwsh,
            "-NoProfile",
            "-File",
            str(args.repo / "android/tests/test_input_demo_regressions.ps1"),
            "-Corpus",
            "public",
            "-NativeOnly",
            "-RunMode",
            args.run_mode,
            "-DemoRoot",
            str(changed),
            "-D1Exe",
            str(args.exe_d1),
            "-D2Exe",
            str(args.exe_d2),
            "-D1DataDir",
            str(args.data_d1),
            "-D2DataDir",
            str(args.data_d2),
            "-ResultArchiveRoot",
            str(output),
            "-TimeoutSeconds",
            str(args.timeout),
            "-MinimumFreeSpaceGB",
            str(args.minimum_free_gb),
        ]
        item = {"mutation": mutation, "command": command, "status": "fail"}
        report["controls"].append(item)
        parity.write_json(args.output / "report.json", report)
        try:
            # Negative controls exercise native backends, not unrelated imported probes
            limit = 2 * len(cases) * (args.timeout + 120) + 120
            item["outer_exit_code"] = runner._run(command, args.output / (mutation + ".log"), limit, args.repo)
            original = compact._json((output / "report.json").read_bytes())
            item["evidence"] = verify_failure(original, item["outer_exit_code"], args, cases, mutation)
            item["status"] = "pass"
        except (OSError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
            item["error"] = str(error)
        parity.write_json(args.output / "report.json", report)
    # The extra loot-specific negative uses the same native/replay wrappers, twice,
    # without rerunning every unrelated positive scene a third time
    loot_cases = [case for case in cases if case["scenario"] == "loot"]
    if loot_cases:
        changed = args.output / "no-collect-corpus"
        corrupt_corpus(args.corpus, changed, cases, "no-collect")
        local = argparse.Namespace(**vars(args))
        local.corpus, local.output, local.native_only = changed, args.output / "no-collect-run", True
        local.output.mkdir()
        item = {"mutation": "no-collect", "scope": "authored loot only", "status": "fail"}
        report["controls"].append(item)
        try:
            code = runner._run_cases(local, loot_cases)
            observed = compact._json((local.output / "report.json").read_bytes())
            item["evidence"] = verify_failure(observed, code, args, loot_cases, "no-collect")
            item["status"] = "pass"
        except (OSError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
            item["error"] = str(error)
        parity.write_json(args.output / "report.json", report)
    report["original_corpus_unchanged"] = originals == {
        path.name: parity.digest(path) for path in args.corpus.iterdir() if path.is_file()
    }
    report["status"] = (
        "pass"
        if report["original_corpus_unchanged"] and all(item["status"] == "pass" for item in report["controls"])
        else "fail"
    )
    parity.write_json(args.output / "report.json", report)
    print(f"{report['status'].upper()}: real-engine negative controls ({args.run_mode}): {args.output / 'report.json'}")
    return 0 if report["status"] == "pass" else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repo", "corpus", "output", "data-d1", "data-d2", "exe-d1", "exe-d2"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--run-mode", choices=("headless", "graphics"), required=True)
    parser.add_argument("--pwsh", default="pwsh")
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--minimum-free-gb", type=float, default=4)
    args = parser.parse_args()
    if not 1 <= args.timeout <= 3600 or not 0.01 <= args.minimum_free_gb <= 1048576:
        parser.error("Invalid timeout or disk reserve")
    for key, value in vars(args).items():
        if isinstance(value, Path):
            setattr(args, key, value.resolve())
    return run(args)


if __name__ == "__main__":
    raise SystemExit(main())
