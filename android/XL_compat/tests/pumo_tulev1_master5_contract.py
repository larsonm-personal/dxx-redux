"""Asset-free witness checks for assisted-local Cloak114/master5/Blue50.

The upstream spawn owner proves its own chain. This owner binds its final
versioned door138 save, then proves ordinary pickup/fire effects and a no-frame
cold restore. The local view assists do not establish connected flight.
"""

import copy
import hashlib
import json
import math
import re
import stat
import sys
from itertools import pairwise
from pathlib import Path

ENGINE_SHA = "dece6cf76b0f5608bb586ee3143426fb3c0b909b86a73be85effc7a0a5d1d5db"
AMMO = [7, 0, 0, 0, 0, 0, 1, 0, 0, 0]
WALLS = (148, 149, 159, 160)
OPERATIONS = (71, 52, 5)
COUNTS = {"71": 1, "52": 1, "5": 1}
COUNTS_BY_PHASE = {"door138": 9, "cloak": 9, "blue": 19, "cold": 1}


def need(value, message):
    if not value:
        raise ValueError(message)


def read(path, limit=16 << 20):
    info = path.lstat()
    need(
        stat.S_ISREG(info.st_mode) and 0 < info.st_size <= limit,
        f"unsafe or empty {path.name}",
    )
    return path.read_bytes()


def sha(path):
    return hashlib.sha256(read(path, 64 << 20)).hexdigest()


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        need(key not in result, f"duplicate JSON field {key}")
        result[key] = value
    return result


def decode(value):
    def invalid_constant(token):
        raise ValueError(f"nonfinite JSON value {token}")

    return json.loads(value, object_pairs_hook=unique_object, parse_constant=invalid_constant)


def doc(path):
    return decode(read(path))


def same(left, right):
    # Python equality alone accepts True == 1 and 1.0 == 1
    return json.dumps(left, sort_keys=True, allow_nan=False) == json.dumps(right, sort_keys=True, allow_nan=False)


def integer(value):
    return type(value) is int and value >= 0


def number(value):
    return type(value) in (int, float) and math.isfinite(value)


def save(root, name):
    need(
        type(name) is str and re.fullmatch(r"[A-Za-z0-9_.-]+\.sav", name),
        "unsafe save filename",
    )
    return root / "preview-user" / name


def template(name):
    owner = "spawn" if name == "door138" else "master5"
    return doc(Path(__file__).with_name(f"pumo_tulev1_{owner}_{name}.json"))


def live(state, *, cloak, blue):
    player = state["player"]
    need(
        type(state["current_level_num"]) is int
        and state["current_level_num"] == 7
        and state["player_dead"] is False
        and state["cheats_enabled"] is False
        and player["invulnerable"] is False
        and player["cloaked"] is cloak
        and player["has_blue_key"] is blue
        and player["has_red_key"] is False
        and player["has_gold_key"] is False
        and player["quad_lasers"] is False,
        "wrong level, life, cloak, key, or cheat state",
    )
    for field, expected in (
        ("primary_weapon", 0),
        ("secondary_weapon", 0),
        ("primary_weapon_flags", 65),
        ("secondary_weapon_flags", 65),
        ("laser_level", 0),
    ):
        need(type(player[field]) is int and player[field] == expected, field)
    need(same(player["secondary_ammo"], AMMO), "Guided inventory changed")
    need(integer(player["vulcan_ammo"]), "vulcan ammo type invalid")
    for field in ("shields", "energy"):
        need(number(player[field]) and 0 < player[field] <= 200, field)
    need(
        integer(state["game_time_raw"])
        and integer(state["position"]["segment"])
        and state["simulation_rng"]["state_available"] is True
        and integer(state["simulation_rng"]["state"])
        and integer(state["simulation_rng"]["call_count"]),
        "clock, segment, or RNG schema invalid",
    )
    pilot = state["xl_pilot"]
    need(
        type(pilot["remaining_objectives"]) is int
        and pilot["remaining_objectives"] == 1
        and integer(pilot["operation_sequence"])
        and all(type(value) is bool for value in pilot["trigger_activated"].values()),
        "pilot schema invalid",
    )
    powerups = state["level_scene"]["powerups"]
    need(type(powerups) is list, "powerup list invalid")
    need(
        all(all(integer(item[field]) for field in ("object", "id", "segment")) for item in powerups)
        and len({item["object"] for item in powerups}) == len(powerups),
        "powerup identity invalid",
    )
    return player


def powerup(state, index, present, kind=None, segment=None):
    items = [p for p in state["level_scene"]["powerups"] if p["object"] == index]
    need(
        items == [] if not present else same(items, [{"object": index, "id": kind, "segment": segment}]),
        f"powerup {index} presence invalid",
    )


def walls(state, kind, status=0, indices=WALLS):
    for index in indices:
        wall = state["xl_pilot"]["doorways"][str(index)]
        need(
            same(wall, {"type": kind, "state": status, "flags": 16, "keys": 1, "clip": -1}),
            f"wall {index} not typed type{kind}/state{status}",
        )


def operations(state, operated):
    pilot = state["xl_pilot"]
    need(
        same(pilot["operation_sequence"], 3 if operated else 0)
        and (same(pilot.get("operation_counts"), COUNTS) if operated else "operation_counts" not in pilot),
        "operation counters invalid",
    )
    for index, kind, value, shot in ((71, 9, 0, 0), (52, 33, 327680, 0), (5, 31, 0, 1)):
        expected = {
            "actor": 0 if operated else -1,
            "attached": False,
            "delay_ms": -1,
            "flags": (7 if operated else 3) if index == 5 else 0,
            "operated": operated,
            "shot": shot if operated else 0,
            "type": kind,
            "value": value,
        }
        need(same(pilot["wall_triggers"][str(index)], expected), "trigger state invalid")
        need(pilot["trigger_activated"][str(index)] is operated, "activation flag invalid")


def restored(saved):
    """Reset only source-proven process-local fields, preserving every other key.

    d2/main/state.c restores SIM RNG state and calls d_rand_reset_call_count
    runtime/pumo_player_test.h resets operation_counts/operation_sequence at run
    entry and omits zero counts in snapshot. runtime/preview.cpp clears the
    activated_triggers array on unload. Persisted wall_triggers are NOT cleared.
    """
    expected = copy.deepcopy(saved)
    expected["simulation_rng"]["call_count"] = 0
    pilot = expected["xl_pilot"]
    pilot.pop("operation_counts", None)
    pilot["operation_sequence"] = 0
    pilot["trigger_activated"] = {key: False for key in pilot["trigger_activated"]}
    return expected


def bundle(name, root, engine):
    receipt = doc(root / f"{name}-result.json")
    plan_path = root / f"{name}-executed-plan.json"
    plan = doc(plan_path)
    final = doc(root / f"{name}-state.json")
    events = [decode(line) for line in read(root / f"{name}-trace.jsonl").splitlines()]
    steps = [event for event in events if "step" in event and "state" in event]
    count = COUNTS_BY_PHASE[name]
    for field, expected in (
        ("receipt_version", 2),
        ("steps", count),
        ("initial_level_setup", 7),
        ("final_level", 7),
        ("fixed_view_aim_events", 0),
        ("position_assist_events", {"door138": 1, "cloak": 1, "blue": 2, "cold": 0}[name]),
        ("operation_sequence", 3 if name == "blue" else 0),
    ):
        need(same(receipt[field], expected), f"receipt {field} invalid")
    need(
        receipt["result"] == "PASS"
        and receipt["resumed_earned_checkpoint"] is True
        and receipt["transitions"] == []
        and receipt["rendered"] is False
        and integer(receipt["process_id"])
        and receipt["process_id"] > 0,
        "receipt not a successful isolated level7 process",
    )
    need(
        type(receipt["run_id"]) is str
        and re.fullmatch(r"[0-9a-f]{32}", receipt["run_id"])
        and receipt["run_id"] == plan["receipt_run_id"]
        and receipt["plan_sha256"] == sha(plan_path)
        and receipt["executable_sha256"] == sha(engine) == ENGINE_SHA
        and receipt["resume_sha256"] == plan["resume_sha256"] == sha(save(root, plan["resume_checkpoint"])),
        "receipt input identity invalid",
    )
    expected = template(name)
    for field in ("resume_checkpoint", "resume_sha256", "receipt_run_id"):
        expected[field] = plan[field]
    need(same(plan, expected), "executed plan differs from checked-in template")
    need(
        same([step["step"] for step in steps], list(range(count)))
        and [step["action"] for step in steps] == [step["action"] for step in plan["steps"]],
        "native PRE-action index or action mismatch",
    )
    need(same(final, steps[-1]["state"]), "final introspection differs")
    allowed = {"position_assist", "successful_wall_operation", "trigger_change", "weapon_wall_contacts"}
    need(
        all(
            set(event) == {"step", "action", "state", "simulation_fps"} and same(event["simulation_fps"], 60)
            if "step" in event and "state" in event
            else event.get("event") in allowed
            for event in events
        ),
        "unexpected trace event",
    )
    assists = [event for event in events if event.get("event") == "position_assist"]
    expected_assists = [] if name == "cold" else [(1, "face_view")]
    if name == "blue":
        expected_assists = [(1, "pose_view"), (11, "pose_view")]
    need(
        same([[event["step"], event["action"]] for event in assists], [list(a) for a in expected_assists]),
        "assist events differ",
    )
    for event in assists:
        need(same(event["position"], steps[event["step"] + 1]["state"]["position"]), "assist position differs")
    return receipt, plan, final, events, steps


def checkpoint(name, root, receipt, final):
    history = receipt["checkpoint_history"]
    step = 17 if name == "blue" else 7
    need(type(history) is list and len(history) == 1, "one immutable save required")
    saved = history[0]
    need(
        same(saved["step"], step)
        and type(saved["file"]) is str
        and re.fullmatch(rf"pumo-player-level7-[0-9]+-step{step}\.sav", saved["file"])
        and saved["sha256"] == sha(save(root, saved["file"]))
        and same(saved["state"], final),
        "versioned save history invalid",
    )
    aliases = receipt["checkpoints"]
    need(set(aliases) == {"7"}, "checkpoint level aliases invalid")
    alias = aliases["7"]
    need(
        alias["file"] == "pumo-player-level7.sav"
        and alias["file"] != saved["file"]
        and alias["sha256"] == saved["sha256"] == sha(save(root, alias["file"]))
        and same(alias["state"], saved["state"]),
        "mutable alias is not the immutable save",
    )
    return saved


def contact_witness(events, steps):
    contacts = [event for event in events if event.get("event") == "weapon_wall_contacts"]
    need(len(contacts) == 1, "one actual weapon contact observation required")
    result = contacts[0]["result"]
    need(
        result["kind"] == "actual_contacts_reaching_effect_blowup_gate"
        and result["truncated"] is False
        and same(result["contact_limit"], 2)
        and same(result["contact_count_lower_bound"], 2)
        and len(result["contacts"]) == 2,
        "not the actual bounded contact witness",
    )
    first, second = result["contacts"]
    shot = steps[7]["state"]["combat_observation"]
    need(same(shot["owned_weapon_count"], 2) and len(shot["owned_weapons"]) == 2, "ordinary laser pair missing")
    for contact in (first, second):
        matches = [
            weapon
            for weapon in shot["owned_weapons"]
            if same(weapon["object"], contact["object"]) and same(weapon["signature"], contact["signature"])
        ]
        need(len(matches) == 1, "contact does not match the emitted laser")
        for field in ("weapon_id", "parent_object", "parent_signature"):
            need(same(matches[0][field], contact[field]), "emitted laser ownership differs")
    for contact in (first, second):
        need(
            all(type(value) is int for key, value in contact.items() if key != "hit_point_fixed"),
            "contact numeric schema invalid",
        )
        need(
            type(contact["hit_point_fixed"]) is list
            and len(contact["hit_point_fixed"]) == 3
            and all(type(value) is int for value in contact["hit_point_fixed"]),
            "contact hit point invalid",
        )
        for field, value in (
            ("level", 7),
            ("wall", 158),
            ("trigger", 5),
            ("hit_segment", 1335),
            ("hit_side", 3),
            ("weapon_id", 0),
            ("base", 148),
        ):
            need(same(contact[field], value), f"contact {field} invalid")
        need(
            contact["parent_object"] == steps[7]["state"]["combat_observation"]["player_object"]
            and contact["parent_signature"] == steps[7]["state"]["combat_observation"]["player_signature"],
            "contact is not owned by the player",
        )
    for field, value in (
        ("blew_up", 1),
        ("overlay_raw", 414),
        ("eclip", 72),
        ("effect_dest", 413),
        ("effect_flags", 0),
    ):
        need(same(first[field], value), f"first actual contact {field} invalid")
    need(same(second["blew_up"], 0) and same(second["overlay_raw"], 413), "post-blowup contact invalid")
    need(
        first["object"] != second["object"] and first["signature"] != second["signature"], "contact identity duplicated"
    )
    ops = [event for event in events if event.get("event") == "successful_wall_operation"]
    need(
        same([event["index"] for event in ops], list(OPERATIONS))
        and same([event["sequence"] for event in ops], [1, 2, 3])
        and same([event["shot"] for event in ops], [0, 0, 1]),
        "actual recursive operation order invalid",
    )
    changes = [event for event in events if event.get("event") == "trigger_change"]
    need(len(changes) == 1, "one post-frame trigger change required")
    changed = changes[0]["state"]
    live(changed, cloak=True, blue=False)
    operations(changed, True)
    walls(changed, 4, indices=(148, 149))
    walls(changed, 5, status=5, indices=(159, 160))
    need(
        changed["observation_boundary"] == "post_frame"
        and events.index(ops[-1]) < events.index(changes[0]) < events.index(steps[9])
        and changed["game_time_raw"] == ops[-1]["state"]["game_time_raw"],
        "trigger change outside the shot frame",
    )
    for ordinal, event in enumerate(ops, 1):
        state = event["state"]
        live(state, cloak=True, blue=False)
        need(
            events.index(steps[8]) < events.index(event) < events.index(steps[9])
            and steps[8]["state"]["game_time_raw"] < state["game_time_raw"] <= steps[9]["state"]["game_time_raw"]
            and same(state["xl_pilot"]["operation_sequence"], ordinal)
            and same(state["xl_pilot"]["operation_counts"], {str(index): 1 for index in OPERATIONS[:ordinal]}),
            "operation not observed during ordinary shot",
        )


def route(name, data):
    receipt, plan, final, events, steps = data
    states = [step["state"] for step in steps]
    for index, state in enumerate(states):
        live(state, cloak=name == "blue" or (name == "cloak" and index >= 5), blue=name == "blue" and index >= 15)
    for before, after in pairwise(states):
        need(
            after["game_time_raw"] >= before["game_time_raw"]
            and after["player"]["shields"] <= before["player"]["shields"]
            and after["player"]["energy"] <= before["player"]["energy"]
            and after["player"]["vulcan_ammo"] == before["player"]["vulcan_ammo"],
            "clock regressed or unearned resource increase",
        )
    need(
        same(final["primary_fire"]["held"], 0)
        and same(final["secondary_fire"]["held"], 0)
        and same(final["combat_observation"]["owned_weapon_count"], 0),
        "unreleased fire or live owned projectile at save",
    )
    if name != "blue":
        need(
            not any(
                event.get("event") in ("successful_wall_operation", "trigger_change", "weapon_wall_contacts")
                for event in events
            ),
            "unexpected weapon or operation event",
        )
        for state in states:
            operations(state, False)
            walls(state, 5)
        if name == "door138":
            need(
                [states[i]["position"]["segment"] for i in (0, 2, 6, 8)] == [1277, 1210, 689, 689],
                "spawn gate geometry invalid",
            )
            for index in (138, 139):
                for state, flags, status in ((states[2], 16, 0), (states[6], 18, 2)):
                    need(
                        same(
                            state["xl_pilot"]["doorways"][str(index)],
                            {"type": 2, "state": status, "flags": flags, "keys": 1, "clip": 30},
                        ),
                        "original spawn gate not opened",
                    )
            powerup(final, 114, True, 23, 618)
        else:
            need(
                [states[i]["position"]["segment"] for i in (0, 2, 5, 8)] == [689, 634, 618, 618],
                "Cloak114 pickup geometry invalid",
            )
            for index, state in enumerate(states):
                powerup(state, 114, index < 5, 23, 618)
            need(
                same(
                    [p for p in states[0]["level_scene"]["powerups"] if p["object"] != 114],
                    final["level_scene"]["powerups"],
                ),
                "pickup changed unrelated objects",
            )
        return
    contact_witness(events, steps)
    for step, raw, forward in (
        (2, [28455613, -8945664, -5779839], [-14265, -27184, -57891]),
        (12, [29816514, -6062080, 9426920], [0, 0, 65536]),
    ):
        position = states[step]["position"]
        need(
            all(number(position[axis]) and position[axis] * 65536 == value for axis, value in zip(("x", "y", "z"), raw))
            and same(position["orient_forward_fixed"], forward),
            "declared local pose differs from actual state",
        )
    need(same(states[5]["combat_observation"]["owned_weapon_count"], 0), "preexisting projectile before shot")
    need(
        [states[i]["position"]["segment"] for i in (0, 2, 10, 12, 15, 18)] == [618, 1329, 1329, 1313, 1315, 1315],
        "master5/Blue50 geometry invalid",
    )
    for index, state in enumerate(states):
        powerup(state, 50, index < 15, 4, 1315)
        powerup(state, 114, False)
        operations(state, index >= 9)
        if index < 9:
            walls(state, 5)
        elif index == 9:
            walls(state, 4, indices=(148, 149))
            walls(state, 5, status=5, indices=(159, 160))
        else:
            walls(state, 4)
    need(
        same([p for p in states[0]["level_scene"]["powerups"] if p["object"] != 50], final["level_scene"]["powerups"])
        and states[7]["player"]["energy"] < states[6]["player"]["energy"]
        and states[10]["game_time_raw"] - states[9]["game_time_raw"] >= 65536
        and states[15]["game_time_raw"] > states[14]["game_time_raw"],
        "shot, animation completion, or ordinary pickup evidence missing",
    )


def producer(path, engine, name="door138"):
    need(path.name == f"{name}-result.json", "wrong producer receipt name")
    data = bundle(name, path.parent, engine)
    route(name, data)
    receipt, plan, final, _, _ = data
    saved = checkpoint(name, path.parent, receipt, final)
    need(saved["file"] != plan["resume_checkpoint"], "output overwrote input save")
    return receipt, saved


def phase(name, root, engine, source):
    need(name in ("cloak", "blue", "cold"), "unknown phase")
    prior_name = {"cloak": "door138", "blue": "cloak", "cold": "blue"}[name]
    prior_receipt, prior = producer(source, engine, prior_name)
    data = bundle(name, root, engine)
    receipt, plan, final, events, steps = data
    need(
        plan["resume_checkpoint"] == prior["file"]
        and receipt["resume_sha256"] == prior["sha256"]
        and receipt["process_id"] != prior_receipt["process_id"]
        and receipt["run_id"] != prior_receipt["run_id"],
        "not a fresh process from the immediate versioned predecessor",
    )
    need(same(steps[0]["state"], restored(prior["state"])), "loaded predecessor differs beyond process-local resets")
    if name == "cold":
        live(final, cloak=True, blue=True)
        walls(final, 4)
        powerup(final, 50, False)
        need(
            len(events) == 1
            and receipt["checkpoint_history"] == []
            and receipt["checkpoints"] == {}
            and same(final, restored(prior["state"])),
            "cold phase is not a no-frame, no-save exact restore",
        )
        return
    route(name, data)
    saved = checkpoint(name, root, receipt, final)
    need(saved["file"] != prior["file"] and saved["sha256"] != prior["sha256"], "new earned save missing")


def main():
    if len(sys.argv) == 4 and sys.argv[1] == "producer":
        producer(*(Path(arg) for arg in sys.argv[2:]))
    elif len(sys.argv) == 6 and sys.argv[1] == "phase":
        phase(sys.argv[2], *(Path(arg) for arg in sys.argv[3:]))
    else:
        raise ValueError("usage: producer RESULT ENGINE | phase NAME ROOT ENGINE SOURCE_RESULT")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, IndexError, TypeError, OSError) as error:
        sys.exit(str(error))
