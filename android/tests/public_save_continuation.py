"""Bounded ordinary-save promises; raw trajectory equivalence is never claimed."""

import argparse
import gzip
import itertools
import json
from pathlib import Path

from d1_replay_parity import (
    EvidenceError,
    digest,
    field_differences,
    object_states,
    records,
    world_states,
    write_json,
)

# Positive promises, not a canonicalization of full engine observations
PLAYER_FIELDS = (
    "connected objnum flags energy shields shields_delta shields_time shields_time_hours "
    "shields_certain lives level laser_level starting_level killer_objnum primary_weapon_flags "
    "secondary_weapon_flags primary_weapon secondary_weapon primary_ammo secondary_ammo last_score "
    "score time_level time_total KillGoalCount net_killed_total net_kills_total num_kills_level "
    "num_kills_total num_robots_level num_robots_total hostages_rescued_total hostages_total "
    "hostages_on_board hostages_level homing_object_dist hours_level hours_total"
).split()
OBJECT_FIELDS = (
    "signature type id segnum control_type movement_type render_type flags pos orient size shields "
    "last_pos contains_type contains_id contains_count matcen_creator lifeleft attached_obj"
).split()
WEAPON_FIELDS = (
    "parent_type parent_num parent_signature last_hitobj track_goal multiplier creation_framecount hitobj_list"
).split()
WORLD_FIELDS = ("tick", "walls", "doors", "triggers")
CASES = (("d1", "flight", 60), ("d2", "flight", 60), ("d1", "door", 180))


def require(condition, message):
    if not condition:
        raise EvidenceError(message)


def select(value, fields):
    try:
        return {key: value[key] for key in fields}
    except (KeyError, TypeError) as error:
        raise EvidenceError(f"Missing promised field: {error}") from error


def players(world):
    require(len(world["players"]) == 1, "Expected one real player")
    return [select(player, PLAYER_FIELDS) for player in world["players"]]


def promised(objects, world):
    selected = {}
    for slot, value in objects["slots"].items():
        item = select(value, OBJECT_FIELDS)
        if value["movement_type"] == 1:
            item["physics"] = value["physics"]
        if value["control_type"] == 9:
            item["weapon"] = select(value["weapon"], WEAPON_FIELDS)
        selected[slot] = item
    return {
        "players": players(world),
        "objects": selected,
        "capacity": objects["capacity"],
        "allocator": objects["allocator"],
        "tick_count": objects["clock"]["d_tick_count"],
        **select(world, WORLD_FIELDS),
    }


def equal(expected, actual, label):
    differences = list(field_differences(expected, actual))
    require(not differences, f"{label}: {differences[:3]}")


def assert_persistence(before, saved, restored):
    bo, bw = before
    so, sw = saved
    ro, rw = restored
    equal(promised(bo, bw), promised(so, sw), "Save changed promised state")
    equal(promised(so, sw), promised(ro, rw), "Restore changed promised state")
    equal(bo["rng"], so["rng"], "Save changed RNG storage")
    for stream in (0, 1):
        require(so["rng"][stream]["available"] == ro["rng"][stream]["available"] == 1, "RNG unavailable")
        equal(so["rng"][stream]["state"], ro["rng"][stream]["state"], f"RNG stream {stream} state")
    require(so["rng"][0]["calls"] > 0, "SIM reset witness is vacuous")
    require(ro["rng"][0]["calls"] == 0, "Ordinary load must reset the absolute SIM counter")
    require(so["rng"][1]["calls"] > 0, "FX persistence witness is vacuous")
    equal(so["rng"][1]["calls"], ro["rng"][1]["calls"], "FX count must persist")
    require(so["clock"]["game_time"] > 0, "Saved clock witness is vacuous")
    equal(so["clock"]["game_time"], ro["clock"]["game_time"], "Ordinary load saved epoch")
    equal(sw["globals"]["GameTime64"], rw["globals"]["GameTime64"], "World saved epoch")


def assert_flight_step(left, right, previous_left, previous_right, lw, rw):
    equal(players(lw), players(rw), "Flight player inventory/health/outcomes")
    for stream in (0, 1):
        require(left["rng"][stream]["available"] == right["rng"][stream]["available"] == 1, "Flight RNG unavailable")
        equal(left["rng"][stream]["state"], right["rng"][stream]["state"], f"Flight RNG {stream}")
    delta_left = left["rng"][0]["calls"] - previous_left["rng"][0]["calls"]
    delta_right = right["rng"][0]["calls"] - previous_right["rng"][0]["calls"]
    require(delta_left >= 0 and delta_right >= 0, "SIM draw count moved backwards")
    equal(delta_left, delta_right, "Flight per-step SIM draw delta")
    equal(left["rng"][1]["calls"], right["rng"][1]["calls"], "Flight FX draw count")


def assert_terminal_objects(previous, terminal):
    expected = {key: value for key, value in previous.items() if key != "objects"}
    expected["slots"] = previous["objects"]
    # Only the record envelope changes between a frame delta and a full boundary
    actual = {key: value for key, value in terminal.items() if key not in ("type", "version", "f", "reset", "phase")}
    equal(expected, actual, "Terminal object/RNG snapshot differs from its last post-frame state")


def assert_terminal_world(previous, terminal):
    equal(previous, terminal["state"], "Terminal world snapshot differs from its last post-frame state")


def assert_door_witness(objects, world, capture):
    target = capture["target"]
    require(type(target) is int and 0 <= target < len(world["walls"]), "Missing authored door target")
    require(0 < world["players"][0]["shields"] < 100 * 65536, "Door checkpoint lacks nonvacuous player damage")
    wall = world["walls"][target]
    require(wall["type"] == 2 and wall["state"] in (1, 2, 3), "Checkpoint target door is inactive")
    require(
        any(target in door["front"][: door["n_parts"]] + door["back"][: door["n_parts"]] for door in world["doors"]),
        "Target has no active door runtime",
    )
    require(capture["action_frame"] >= 0, "Door was not opened by ordinary controls")
    require(
        any(value["type"] == 5 for value in objects["slots"].values()),
        "Door checkpoint lacks live weapon parent/hit data",
    )


def header(game, count):
    return {
        "game": game,
        "mission": "d1" if game == "d1" else "d2demo",
        "level": 1,
        "difficulty": 2,
        "start_mode": "save_checkpoint",
        "frame_count": count,
    }


def inputs(path, scenario, count):
    actual = [row for row in records(path) if row.get("type") == "continuation_input"]
    expected = [
        {
            "type": "continuation_input",
            "version": 1,
            "f": frame,
            "frame_time": 2184,
            "forward": 2184 if scenario == "flight" else 0,
            "fire_primary": int(scenario == "flight" and frame < 30),
        }
        for frame in range(count)
    ]
    equal(expected, actual, "Continuation controls changed or missing")


def boundaries(path, game, count):
    stream = iter(world_states(path, header(game, count), game))
    objects, world = next(stream), next(stream)
    require(
        objects.get("type") == "object_boundary" and world.get("type") == "world_boundary", "Initial boundary order"
    )
    return (objects, world["state"]), stream


def qualify_case(directory, game, scenario, count):
    directory = Path(directory)
    before_path, left_path, right_path = [
        directory / name for name in ("before-save.jsonl.gz", "uninterrupted.jsonl.gz", "resumed.jsonl.gz")
    ]
    before, before_stream = boundaries(before_path, game, 0)
    # Exhaust even zero-frame streams so missing terminal records cannot pass
    terminal = list(before_stream)
    require(len(terminal) == 2, "Pre-save terminal boundary count")
    equal(before[0], {**terminal[0], "phase": "restored"}, "Pre-save object observation changed")
    equal(before[1], terminal[1]["state"], "Pre-save world observation changed")
    saved, left_worlds = boundaries(left_path, game, count)
    restored, right_worlds = boundaries(right_path, game, count)
    failures = []
    try:
        assert_persistence(before, saved, restored)
    except EvidenceError as error:
        failures.append(str(error))
    immediate_status = "fail" if failures else "pass"
    capture = json.loads((directory / "capture.json").read_bytes())
    require(capture.get("game") == game and capture.get("scenario") == scenario, "Wrong checkpoint capture identity")
    require((directory / "public-continuation.sav").stat().st_size > 0, "Native save missing")
    if scenario == "door":
        assert_door_witness(*saved, capture)
    else:
        require(
            len(capture["observations"]) == 360 and capture["wall_contact_frames"] > 0,
            "Flight checkpoint lacks ordinary flight/contact",
        )
    inputs(left_path, scenario, count)
    inputs(right_path, scenario, count)
    strict_count, first = 0, {}
    first_gameplay = {}
    firing_frames = [0, 0]
    terminal_energy = []
    strict_path = directory / "strict-differences.jsonl.gz"
    with gzip.open(strict_path, "wt", encoding="utf-8") as out:

        def differences(label, frame, left, right):
            nonlocal strict_count
            for delta in field_differences(left, right):
                row = {"comparison": label, "frame": frame, **delta}
                out.write(json.dumps(row, separators=(",", ":")) + "\n")
                strict_count += 1
                first.setdefault(label, row)

        for label, left, right in (("save", before, saved), ("restore", saved, restored)):
            differences(label + "_objects", 0, left[0], right[0])
            differences(label + "_world", 0, left[1], right[1])
        left_objects = iter(object_states(left_path, header(game, count), game))
        right_objects = iter(object_states(right_path, header(game, count), game))
        previous_left, previous_right = saved[0], restored[0]
        energy_start = saved[1]["players"][0]["energy"]
        firing_witness = [False, False]
        observed = 0
        last_worlds = None
        for lw, rw in itertools.zip_longest(left_worlds, right_worlds):
            require(
                lw is not None and rw is not None and lw["type"] == rw["type"], "Mismatched continuation record order"
            )
            differences("continuation_" + lw["type"], lw["f"], lw, rw)
            if lw["type"] != "world_state":
                require(observed == count and last_worlds is not None, "Terminal precedes complete post-frame state")
                try:
                    if lw["type"] == "object_boundary":
                        assert_terminal_objects(previous_left, lw)
                        assert_terminal_objects(previous_right, rw)
                    else:
                        assert_terminal_world(last_worlds[0], lw)
                        assert_terminal_world(last_worlds[1], rw)
                except EvidenceError as error:
                    failures.append(f"Terminal: {error}")
                continue
            left, right = next(left_objects), next(right_objects)
            differences("continuation_objects", observed, left, right)
            for group, a, b in (
                ("players", players(lw["state"]), players(rw["state"])),
                ("sim_state", left["rng"][0]["state"], right["rng"][0]["state"]),
                ("fx_state", left["rng"][1]["state"], right["rng"][1]["state"]),
                ("doors", lw["state"]["doors"], rw["state"]["doors"]),
            ):
                first_difference = next(field_differences(a, b), None)
                if first_difference:
                    first_gameplay.setdefault(group, {"frame": observed, **first_difference})
            if scenario == "flight":
                try:
                    assert_flight_step(left, right, previous_left, previous_right, lw["state"], rw["state"])
                except EvidenceError as error:
                    failures.append(f"Frame {observed}: {error}")
                for branch, (state, world) in enumerate(((left, lw["state"]), (right, rw["state"]))):
                    observed_firing = world["players"][0]["energy"] < energy_start and any(
                        value["type"] == 5 and value.get("weapon", {}).get("parent_type") == 4
                        for value in state["objects"].values()
                    )
                    firing_witness[branch] |= observed_firing
                    firing_frames[branch] += int(observed_firing)
            terminal_energy = [world["state"]["players"][0]["energy"] for world in (lw, rw)]
            previous_left, previous_right = left, right
            last_worlds = (lw["state"], rw["state"])
            observed += 1
        require(next(left_objects, None) is None and next(right_objects, None) is None, "Extra object frames")
        require(observed == count, "Incomplete continuation")
    if scenario == "flight":
        require(
            previous_left["rng"][0]["calls"] > saved[0]["rng"][0]["calls"] and previous_right["rng"][0]["calls"] > 0,
            "Flight SIM draw witness is vacuous",
        )
        require(all(firing_witness), "Continuation firing/energy witness is vacuous")
    report = {
        "status": "fail" if failures else "pass",
        "game": game,
        "scenario": scenario,
        "frames": observed,
        "immediate_persistence": immediate_status,
        "terminal_observation_integrity": "fail"
        if any(value.startswith("Terminal:") for value in failures)
        else "pass",
        "continuation_selected_state": "diagnostic_only"
        if scenario == "door"
        else "fail"
        if any(value.startswith("Frame ") for value in failures)
        else "pass",
        "full_trajectory": "UNQUALIFIED",
        "reason": "Ordinary saves preserve GameTime but reset the absolute SIM draw count; raw object/world trajectories differ",
        "promised_fields": {
            "player": PLAYER_FIELDS,
            "object": OBJECT_FIELDS,
            "weapon": WEAPON_FIELDS,
            "world": WORLD_FIELDS,
            "other": ["physics", "allocator", "capacity", "tick_count", "SIM state", "FX state and count"],
        },
        "witnesses": {
            "checkpoint_live_weapons": sum(value["type"] == 5 for value in saved[0]["slots"].values()),
            "checkpoint_active_doors": len(saved[1]["doors"]),
            "energy_before": energy_start,
            "energy_terminal": terminal_energy,
            "firing_frames": firing_frames,
        },
        "first_gameplay_differences": first_gameplay,
        "failures": failures,
        "strict_difference_count": strict_count,
        "first_strict_differences": first,
        "strict_differences": strict_path.name,
        "sim_count_before": saved[0]["rng"][0]["calls"],
        "sim_count_restored": restored[0]["rng"][0]["calls"],
        "fx_count_before": saved[0]["rng"][1]["calls"],
        "fx_count_restored": restored[0]["rng"][1]["calls"],
        "artifacts": {
            path.name: digest(path)
            for path in (before_path, left_path, right_path, strict_path, directory / "public-continuation.sav")
        },
    }
    write_json(directory / "report.json", report)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    results = []
    for game, scenario, count in CASES:
        try:
            results.append(qualify_case(args.output / f"{game}-{scenario}", game, scenario, count))
        except (EvidenceError, OSError, ValueError, KeyError, StopIteration) as error:
            results.append(
                {
                    "game": game,
                    "scenario": scenario,
                    "status": "fail",
                    "error": str(error),
                    "full_trajectory": "UNQUALIFIED",
                }
            )
    report = {
        "status": "pass" if all(row["status"] == "pass" for row in results) else "fail",
        "full_trajectory": "UNQUALIFIED",
        "cases": results,
    }
    write_json(args.output / "report.json", report)
    print(
        f"{report['status'].upper()}: ordinary save persistence; full trajectories UNQUALIFIED; {args.output / 'report.json'}"
    )
    return int(report["status"] != "pass")


if __name__ == "__main__":
    raise SystemExit(main())
