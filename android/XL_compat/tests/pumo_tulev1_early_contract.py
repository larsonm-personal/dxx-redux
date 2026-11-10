"""Independent bounded Tulev1 early-route receipts; never reads original assets."""

import hashlib
import json
import re
import stat
import sys
from itertools import pairwise
from pathlib import Path


class WitnessError(ValueError):
    pass


EARLY_ORDER = ("combat", "switch", "cloak", "red_key", "red_door", "master", "guided_inventory")


def need(ok, message):
    if not ok:
        raise WitnessError(message)


def read(path, limit):
    info = path.lstat()
    need(
        stat.S_ISREG(info.st_mode) and 0 < info.st_size <= limit,
        f"unsafe or oversized {path.name}",
    )
    return path.read_bytes()


def sha(path, limit=64 << 20):
    return hashlib.sha256(read(path, limit)).hexdigest()


def doc(path, limit=4 << 20):
    return json.loads(read(path, limit))


def trace(path):
    data = read(path, 16 << 20)
    lines = data.splitlines()
    need(
        len(lines) <= 10000 and all(len(line) <= 1 << 18 for line in lines),
        "trace bounds exceeded",
    )
    return [json.loads(line) for line in lines]


def checkpoint(root, name):
    need(
        type(name) is str and re.fullmatch(r"[A-Za-z0-9_.-]+\.sav", name) and name not in (".", ".."),
        "unsafe save name",
    )
    return root / "preview-user" / name


def state_at(events, index):
    found = [event["state"] for event in events if event.get("step") == index and "state" in event]
    need(len(found) == 1, f"step {index} state missing or duplicate")
    return found[0]


def live(state):
    player = state["player"]
    need(
        state["current_level_num"] == 7
        and state["player_dead"] is False
        and state["cheats_enabled"] is False
        and player["invulnerable"] is False
        and player["shields"] > 0
        and player["energy"] >= 0,
        "player dead, unauthorized, or resources exhausted",
    )
    return player


def producer_progression(receipt):
    need(
        type(receipt["initial_level_setup"]) is int
        and receipt["initial_level_setup"] == 6
        and type(receipt["final_level"]) is int
        and receipt["final_level"] == 7
        and type(receipt["transitions"]) is list
        and len(receipt["transitions"]) == 1
        and type(receipt["transitions"][0]) is dict
        and type(receipt["transitions"][0].get("from")) is int
        and receipt["transitions"][0]["from"] == 6
        and type(receipt["transitions"][0].get("to")) is int
        and receipt["transitions"][0]["to"] == 7,
        "Sophodo main progression transition invalid",
    )


def producer(result_path, save, engine):
    root = result_path.parent
    need(
        result_path.name == "main-result.json"
        and save.name == "sophodo-main-level7.sav"
        and save.parent == root / "preview-user",
        "wrong Sophodo warm producer",
    )
    receipt = doc(result_path)
    plan_path = root / "main-executed-plan.json"
    plan = doc(plan_path, 1 << 20)
    need(
        receipt["result"] == "PASS"
        and receipt["receipt_version"] == 2
        and receipt["steps"] == 144
        and type(receipt["steps"]) is int,
        "Sophodo main producer result invalid",
    )
    producer_progression(receipt)
    need(
        re.fullmatch(r"[0-9a-f]{32}", receipt["run_id"])
        and plan["receipt_run_id"] == receipt["run_id"]
        and sha(plan_path, 1 << 20) == receipt["plan_sha256"]
        and sha(engine) == receipt["executable_sha256"],
        "Sophodo main producer input identity invalid",
    )
    warm = receipt["checkpoint_history"][-1]
    need(
        warm["step"] == 139
        and warm["sha256"] == sha(save, 4 << 20)
        and receipt["checkpoints"]["7"]["sha256"] == warm["sha256"],
        "Sophodo preserved warm checkpoint invalid",
    )
    live(warm["state"])


def bound(state):
    obs = state["combat_observation"]
    entries = [item for item in obs["tracked_objects"] if item["object"] == 256]
    need(len(entries) == 1, "robot256 binding missing or duplicated")
    item = entries[0]
    need(
        item["expected_id"] == 28
        and item["expected_type"] == 2
        and item["expected_level"] == item["observed_level"] == 7
        and item["expected_load_generation"] == item["observed_load_generation"] == obs["load_generation"],
        "robot256 binding invalidated",
    )
    return item


def combat_damage(pre, damaged, defeated):
    binding = pre["binding_shields_raw"]
    current = damaged["actor_state"]["shields_raw"]
    reported = damaged["damage_since_binding_raw"]
    need(
        pre["exists"] is True
        and damaged["exists"] is True
        and defeated["exists"] is False
        and defeated["actor_state"] is None
        and pre["expected_signature"] == damaged["expected_signature"] == defeated["expected_signature"]
        and pre["expected_load_generation"]
        == damaged["expected_load_generation"]
        == defeated["expected_load_generation"]
        and type(binding) is int
        and -(1 << 63) <= binding < (1 << 63)
        and binding == pre["actor_state"]["shields_raw"]
        and pre["damage_since_binding_raw"] == 0
        and type(pre["damage_since_binding_raw"]) is int
        and type(damaged["binding_shields_raw"]) is int
        and type(defeated["binding_shields_raw"]) is int
        and binding == damaged["binding_shields_raw"] == defeated["binding_shields_raw"]
        and type(pre["actor_state"]["shields_raw"]) is int
        and type(current) is int
        and -(1 << 63) <= current < (1 << 63)
        and current < pre["actor_state"]["shields_raw"]
        and type(reported) is int
        and reported == binding - current
        and reported > 0
        and defeated["damage_since_binding_raw"] is None,
        "robot256 live-HP damage or defeat not established",
    )


def combat_trace_witness(events):
    actions = (
        ["assert"]
        + ["send_axis"] * 5
        + [
            "pose_view",
            "assert",
            "wait_for",
            "introspect",
            "send_button",
            "wait_for",
            "send_button",
            "introspect",
            "wait_ms",
            "introspect",
            "assert",
            "set_debug",
        ]
    )
    need(
        all(
            any(e.get("step") == index and e.get("action") == action for e in events)
            for index, action in enumerate(actions)
        ),
        "robot256 combat action trace incomplete",
    )
    # All state-bearing step records are PRE action. The prefire wait is one tick;
    # the 3-second goal has a hard maximum of 180 additional frames.
    source = state_at(events, 0)
    origin = source["game_time_raw"]
    prefire = origin + 65536 // 60
    at_goal = state_at(events, 12)["game_time_raw"]
    elapsed = at_goal - origin
    goal_frames = next((n for n in range(1, 182) if n * 65536 // 60 == elapsed), None)
    need(
        all(state_at(events, n)["game_time_raw"] == origin for n in range(1, 9))
        and all(state_at(events, n)["game_time_raw"] == prefire for n in (9, 10, 11))
        and goal_frames is not None
        and all(state_at(events, n)["game_time_raw"] == at_goal for n in (13, 14))
        and all(state_at(events, n)["game_time_raw"] == origin + (goal_frames + 45) * 65536 // 60 for n in (15, 16, 17))
        and all(state_at(events, n)["primary_fire"]["held"] == 0 for n in range(11))
        and state_at(events, 11)["primary_fire"]["held"] == 0,
        "combat PRE clock, goal bound, fire latch, or 45-frame neutral tail invalid",
    )
    # The fixed-view target must stay the same loaded robot during prefire and
    # goal polling. Cadence is bounded by the protocol's 17ms face interval;
    # death may end polling before the next aim update.
    aims = [e for e in events if e.get("event") == "fixed_view_aim"]
    pre_binding = bound(source)
    need(
        len(aims) >= 2
        and aims[0]["game_time_raw"] == origin
        and all(
            a["target"] == 256
            and a["target_state"]["expected_signature"] == pre_binding["expected_signature"]
            and a["target_state"]["expected_load_generation"] == pre_binding["expected_load_generation"]
            and a["target_state"]["exists"] is True
            and origin <= a["game_time_raw"] < at_goal
            for a in aims
        )
        and all(0 < b["game_time_raw"] - a["game_time_raw"] <= 3 * 65536 // 60 for a, b in pairwise(aims)),
        "combat bound target or view-aim cadence invalid",
    )
    # The goal is visible only after action 11; action 12 releases the button.
    pre, killed, removed = (bound(state_at(events, n)) for n in (0, 12, 16))
    combat_damage(pre, killed, removed)
    need(
        killed["actor_state"]["shields_raw"] < 0
        and state_at(events, 12)["primary_fire"]["held"] == 1
        and state_at(events, 13)["primary_fire"]["held"] == 1
        and state_at(events, 16)["primary_fire"]["held"] == 0
        and state_at(events, 12)["player"]["robots_killed_level"] == source["player"]["robots_killed_level"] + 1,
        "robot256 same-bound negative HP, released fire, or kill credit missing",
    )


def operations(events, expected):
    found = [e for e in events if e.get("event") == "successful_wall_operation"]
    need(len(found) == len(expected), "operation count mismatch")
    for event, (index, sequence, shot) in zip(found, expected):
        need(
            all(type(event.get(field)) is int for field in ("index", "sequence", "shot"))
            and (event["index"], event["sequence"], event["shot"]) == (index, sequence, shot)
            and event["state"]["current_level_num"] == 7,
            "wrong native operation sequence, type, or source level",
        )


def guided_inventory_witness(events, plan):
    """Check physical pickup and native secondary inventory, not missile flight."""
    actions = [
        "assert",
        "face_view",
        "assert",
        "send_axis",
        "wait_for",
        "send_axis",
        "assert",
        "set_debug",
    ]
    need(
        len(plan["steps"]) == len(actions)
        and "qualification" not in plan
        and "flight_claim" not in plan
        and [step.get("action") for step in plan["steps"]] == actions
        and [(plan["steps"][n].get("axis"), plan["steps"][n].get("axis_value")) for n in (3, 5)] == [(1, -0.2), (1, 0)]
        and plan["steps"][1].get("segment") == 457
        and plan["steps"][1].get("side") == 4
        and plan["steps"][1].get("distance") == 8
        and plan["steps"][4].get("timeout_ms") == 5000
        and set(plan["steps"][4]) == {"action", "expect", "timeout_ms"}
        and plan["steps"][7].get("field") == "android_game_request"
        and plan["steps"][7].get("value") == "quick_save"
        and all(
            any(e.get("step") == n and e.get("action") == action for e in events) for n, action in enumerate(actions)
        ),
        "guided pickup action or timing changed",
    )
    initial, staged, moving, final = (state_at(events, n) for n in (0, 2, 4, 7))

    def object140(state):
        return [p for p in state["level_scene"]["powerups"] if p["object"] == 140]

    need(
        initial["position"]["segment"] == 1149
        and staged["position"]["segment"] == moving["position"]["segment"] == 457
        and final["position"]["segment"] == 458
        and all(object140(s) == [{"object": 140, "id": 41, "segment": 458}] for s in (initial, staged, moving))
        and not object140(final)
        and initial["xl_pilot"]["wall_triggers"]["6"]["operated"] is True
        and all(initial["xl_pilot"]["doorways"][str(w)]["type"] == 4 for w in (55, 56, 57, 58))
        and final["xl_pilot"]["remaining_objectives"] == initial["xl_pilot"]["remaining_objectives"] == 1
        and final["xl_pilot"]["operation_sequence"] == initial["xl_pilot"]["operation_sequence"],
        "guided object identity, consumption, entrance, or objective invalid",
    )
    before, after = initial["player"], final["player"]
    ammo0, ammo1 = before["secondary_ammo"], after["secondary_ammo"]
    need(
        type(ammo0) is type(ammo1) is list
        and len(ammo0) == len(ammo1) == 10
        and all(type(a) is int and a >= 0 for a in ammo0 + ammo1)
        and ammo0[6] == 0
        and ammo1[6] == 4
        and all(ammo0[n] == ammo1[n] for n in range(10) if n != 6)
        and type(before["secondary_weapon_flags"]) is type(after["secondary_weapon_flags"]) is int
        and before["secondary_weapon_flags"] == 1
        and after["secondary_weapon_flags"] == 65
        and before["secondary_weapon"] == after["secondary_weapon"] == 0
        and all(
            s["secondary_weapon_definition"]["selected_slot"] == 0
            and s["secondary_weapon_definition"]["weapon_id"] == 8
            for s in (initial, final)
        )
        and all(s["combat_observation"]["guided_control"] is None for s in (initial, final))
        and all(s["secondary_fire"]["count"] == s["secondary_fire"]["held"] == 0 for s in (initial, final)),
        "guided secondary inventory, selection, or fire witness invalid",
    )
    for field in (
        "energy",
        "shields",
        "vulcan_ammo",
        "lives",
        "score",
        "primary_weapon",
        "primary_weapon_flags",
        "robots_killed_level",
        "has_blue_key",
        "has_red_key",
        "has_gold_key",
        "invulnerable",
    ):
        need(
            before[field] == after[field],
            f"guided pickup changed primary resource: {field}",
        )


def guided_receipt_controls(result):
    need(
        type(result["fixed_view_aim_events"]) is int and result["fixed_view_aim_events"] == 0,
        "guided pickup used an undisclosed fixed-view aim",
    )


def earned_predecessor(prior, result):
    need(
        re.fullmatch(r"[0-9a-f]{64}", prior["sha256"]) and prior["sha256"] == result["resume_sha256"],
        "phase did not resume its immediate earned predecessor",
    )


def phase_predecessor(name, order):
    need(tuple(order) == EARLY_ORDER and name in EARLY_ORDER, "unsafe switch-first or unknown early phase order")
    index = EARLY_ORDER.index(name)
    return EARLY_ORDER[index - 1] if index else None


def combat_setup(steps):
    need(
        len(steps) == 18
        and steps[1:6] == [{"action": "send_axis", "axis": axis, "axis_value": 0.0} for axis in range(5)]
        and steps[6]
        == {
            "action": "pose_view",
            "segment": 449,
            "position_fixed": [-30317922, 12828834, -11607676],
            "pitch": 0,
            "bank": 0,
            "heading": 0,
        }
        and steps[8]["action"] == "wait_for"
        and steps[8]["face_object"] == 256
        and steps[8]["face_interval_ms"] == steps[11]["face_interval_ms"] == 17
        and steps[8]["timeout_ms"] == 17
        and steps[11]["action"] == "wait_for"
        and steps[11]["face_object"] == 256
        and steps[11]["timeout_ms"] == 3000
        and steps[11]["expect"]["combat_observation.tracked_objects"]["contains_object"]
        == {"object": 256, "expected_id": 28, "exists": True, "actor_state": {"shields_raw": {"lte": -1}}}
        and steps[11]["expect"]["player_dead"] is False
        and steps[10] == {"action": "send_button", "button": 100, "held": 1}
        and steps[12] == {"action": "send_button", "button": 100, "held": 0}
        and steps[14] == {"action": "wait_ms", "ms": 750}
        and steps[17]
        == {
            "action": "set_debug",
            "field": "android_game_request",
            "value": "quick_save",
        },
        "unsafe guard setup or nonordinary combat input",
    )


def cold_boundary(saved, restored):
    """Check durable save state, not process-local input or observer counters."""
    need(
        saved["position"] == restored["position"]
        and saved["game_time_raw"] == restored["game_time_raw"]
        and saved["level_scene"] == restored["level_scene"]
        and saved["simulation_rng"]["state_available"] is True
        and restored["simulation_rng"]["state_available"] is True
        and saved["simulation_rng"]["state"] == restored["simulation_rng"]["state"]
        and restored["simulation_rng"]["call_count"] == 0
        and saved["xl_pilot"]["wall_triggers"] == restored["xl_pilot"]["wall_triggers"]
        and saved["xl_pilot"]["doorways"] == restored["xl_pilot"]["doorways"]
        and saved["xl_pilot"]["remaining_objectives"] == restored["xl_pilot"]["remaining_objectives"],
        "earned checkpoint did not cold-restore durable scene, gates, position, time, or RNG",
    )
    fields = (
        "shields",
        "energy",
        "vulcan_ammo",
        "lives",
        "score",
        "primary_weapon",
        "primary_weapon_flags",
        "laser_level",
        "quad_lasers",
        "secondary_ammo",
        "secondary_weapon",
        "secondary_weapon_flags",
        "robots_killed_level",
        "has_blue_key",
        "has_red_key",
        "has_gold_key",
        "cloaked",
        "invulnerable",
    )
    need(
        all(saved["player"][field] == restored["player"][field] for field in fields),
        "earned checkpoint did not cold-restore player inventory or resources",
    )


def powerup_map(state):
    powerups = state["level_scene"]["powerups"]
    found = {p["object"]: (p["id"], p["segment"]) for p in powerups}
    need(len(found) == len(powerups), "duplicate powerup object identity")
    return found


def guard_drops(pre, post):
    before, after = powerup_map(pre), powerup_map(post)
    need(
        all(after.get(obj) == identity for obj, identity in before.items())
        and post["player"]["vulcan_ammo"] == pre["player"]["vulcan_ammo"]
        and post["player"]["primary_weapon_flags"] == pre["player"]["primary_weapon_flags"],
        "guard clear removed an original item or changed inherited primary ammo or weapons",
    )
    drops = {obj: identity for obj, identity in after.items() if obj not in before}
    need(
        all(identity[0] == 22 and identity[1] == 450 for identity in drops.values()),
        "guard clear produced an unverified item",
    )
    return drops


def switch_drops(pre, post, drops):
    before, after = powerup_map(pre), powerup_map(post)
    need(
        all(after.get(obj) == identity for obj, identity in before.items() if obj not in drops)
        and not (set(after) - set(before))
        and all(after[obj][0] == 22 for obj in drops if obj in after),
        "switch phase lost an original item or added an unearned item",
    )
    consumed = set(drops) - set(after)
    # Stock d2/main/powerup.h VULCAN_AMMO_AMOUNT is 49*2.
    need(
        post["player"]["vulcan_ammo"] == pre["player"]["vulcan_ammo"] + 98 * len(consumed),
        "Vulcan ammo differs from exactly the earned consumed drop count",
    )
    return consumed


def switch_shot_witness(events):
    born = state_at(events, 10)["combat_observation"]["owned_weapons"]
    ids = {(w["object"], w["signature"]) for w in born}
    need(
        len(born) == len(ids) == 2
        and all(
            w["weapon_id"] == 0 and w["parent_object"] == 0 and w["parent_signature"] == 0 and w["parent_type"] == 4
            for w in born
        ),
        "single ordinary owned two-bolt Laser0 pulse missing",
    )
    contacts = [event["result"] for event in events if event.get("event") == "weapon_wall_contacts"]
    need(len(contacts) == 1, "ordinary wall contact observer missing")
    result = contacts[0]
    hits = result["contacts"]
    need(
        result["truncated"] is False
        and result["contact_count_lower_bound"] == len(hits) == 2
        and all(
            (hit["object"], hit["signature"]) in ids
            and hit["parent_object"] == hit["parent_signature"] == 0
            and hit["weapon_id"] == 0
            and hit["hit_segment"] == 457
            and hit["hit_side"] == 5
            and hit["wall"] == 59
            and hit["trigger"] == 6
            and (hit["overlay_raw"] & 16383) > 0
            for hit in hits
        )
        and sum(hit["blew_up"] == 1 for hit in hits) == 1,
        "owned Laser0 switch contact or opaque overlay operation missing",
    )


def switch_trace_witness(events, plan, drops):
    """PRE snapshots bind the finite pulse, drops, and live-actor identity."""
    steps = plan["steps"]
    source = state_at(events, 0)
    bound = source["combat_observation"]["tracked_objects"]
    need(
        len(bound) == 1 and bound[0]["object"] == 305 and bound[0]["expected_id"] == 44 and bound[0]["exists"] is True,
        "fresh switch source must bind the live adjacent actor",
    )
    signature = bound[0]["expected_signature"]
    frame = held = firing = 0
    for index, action in enumerate(steps):
        state = state_at(events, index)
        need(
            state["game_time_raw"] == source["game_time_raw"] + frame * 65536 // 60
            and state["primary_fire"]["held"] == held,
            f"switch PRE clock or Controls mismatch at {index}",
        )
        if index >= 7:
            actor = state["combat_observation"]["tracked_objects"]
            need(
                state["position"]["segment"] == 450
                and state["player_dead"] is False
                and state["player"]["shields"] > 0
                and state["player"]["shields"] <= source["player"]["shields"]
                and state["player"]["energy"] <= source["player"]["energy"]
                and len(actor) == 1
                and actor[0]["object"] == 305
                and actor[0]["expected_signature"] == signature
                and actor[0]["exists"] is True
                and actor[0]["actor_state"]["shields_raw"] > 0,
                f"switch observed segment/life/resource status at {index}",
            )
            switch_drops(source, state, drops)
        if action["action"] == "send_button":
            firing = action["held"]
        if action["action"] == "wait_ms":
            frame += (action["ms"] * 60 + 999) // 1000
            held = firing
    need(frame == 91 and held == firing == 0, "switch pulse/tail did not settle")
    switch_shot_witness(events)


def restored_inventory(saved, restored):
    live(restored)
    fields = (
        "secondary_ammo",
        "secondary_weapon_flags",
        "secondary_weapon",
        "shields",
        "energy",
        "vulcan_ammo",
        "lives",
        "score",
        "primary_weapon",
        "primary_weapon_flags",
        "robots_killed_level",
        "has_blue_key",
        "has_red_key",
        "has_gold_key",
        "cloaked",
        "invulnerable",
    )
    need(
        saved["position"] == restored["position"]
        and saved["game_time_raw"] == restored["game_time_raw"]
        and saved["simulation_rng"]["state_available"] is True
        and restored["simulation_rng"]["state_available"] is True
        and saved["simulation_rng"]["state"] == restored["simulation_rng"]["state"]
        and restored["simulation_rng"]["call_count"] == 0
        and all(saved["player"][f] == restored["player"][f] for f in fields)
        and restored["player"]["secondary_ammo"][6] == 4
        and restored["player"]["secondary_weapon_flags"] == 65
        and restored["player"]["secondary_weapon"] == 0
        and restored["secondary_weapon_definition"]["selected_slot"] == 0
        and restored["secondary_weapon_definition"]["weapon_id"] == 8
        and restored["combat_observation"]["guided_control"] is None
        and restored["secondary_fire"]["count"] == restored["secondary_fire"]["held"] == 0
        and saved["xl_pilot"]["operation_sequence"] == restored["xl_pilot"]["operation_sequence"]
        and saved["xl_pilot"]["remaining_objectives"] == restored["xl_pilot"]["remaining_objectives"] == 1,
        "guided inventory did not persist on fresh load",
    )


def load(root, engine, guided_root):
    result = doc(root / "guided_load-result.json")
    plan_path = root / "guided_load-executed-plan.json"
    plan = doc(plan_path, 1 << 20)
    events = trace(root / "guided_load-trace.jsonl")
    state = doc(root / "guided_load-state.json", 1 << 20)
    source = doc(guided_root / "guided_inventory-result.json")
    saved = source["checkpoint_history"][0]
    need(
        result["receipt_version"] == 2
        and result["result"] == "PASS"
        and result["steps"] == 1
        and result["initial_level_setup"] == result["final_level"] == 7
        and result["resumed_earned_checkpoint"] is True
        and result["transitions"] == []
        and result["checkpoint_history"] == []
        and result["checkpoints"] == {}
        and result["position_assist_events"] == 0
        and result["fixed_view_aim_events"] == 0
        and len(events) == 1
        and events[0].get("step") == 0
        and events[0].get("action") == "introspect"
        and len(plan["steps"]) == 1
        and plan["steps"][0] == {"action": "introspect"}
        and state == events[0]["state"]
        and result["run_id"] == plan["receipt_run_id"]
        and result["plan_sha256"] == sha(plan_path, 1 << 20)
        and result["executable_sha256"] == sha(engine)
        and result["resume_sha256"] == plan["resume_sha256"]
        and result["resume_sha256"] == sha(checkpoint(root, plan["resume_checkpoint"]), 4 << 20)
        and result["resume_sha256"] == saved["sha256"],
        "guided fresh-load input or receipt invalid",
    )
    restored_inventory(saved["state"], state)


def phase(name, root, engine, producer_path):
    steps = {
        "combat": 18,
        "switch": 194,
        "cloak": 7,
        "red_key": 8,
        "red_door": 8,
        "master": 12,
        "guided_inventory": 8,
    }
    need(name in steps, "unknown phase")
    result = doc(root / f"{name}-result.json")
    plan_path = root / f"{name}-executed-plan.json"
    plan = doc(plan_path, 1 << 20)
    events = trace(root / f"{name}-trace.jsonl")
    saved_state = doc(root / f"{name}-state.json", 1 << 20)
    need(
        result["receipt_version"] == 2
        and result["result"] == "PASS"
        and result["steps"] == steps[name]
        and result["initial_level_setup"] == result["final_level"] == 7
        and result["resumed_earned_checkpoint"] is True
        and len(result["transitions"]) == 0
        and type(result["process_id"]) is int
        and result["process_id"] > 0,
        "native phase receipt invalid",
    )
    need(
        re.fullmatch(r"[0-9a-f]{32}", result["run_id"])
        and result["run_id"] == plan["receipt_run_id"]
        and result["plan_sha256"] == sha(plan_path, 1 << 20)
        and result["executable_sha256"] == sha(engine)
        and result["resume_sha256"]
        == sha(checkpoint(root, plan["resume_checkpoint"]), 4 << 20)
        == plan["resume_sha256"],
        "phase input identity mismatch",
    )
    history = result["checkpoint_history"]
    need(
        len(history) == 1 and history[0]["step"] == steps[name] - 1,
        "versioned save history invalid",
    )
    saved = checkpoint(root, history[0]["file"])
    need(sha(saved, 4 << 20) == history[0]["sha256"], "versioned save hash invalid")
    initial, final = state_at(events, 0), state_at(events, steps[name] - 1)
    need(
        saved_state == final and history[0]["state"] == final,
        "saved resources or state differ from execution",
    )
    start_player, end_player = live(initial), live(final)
    previous = phase_predecessor(name, steps)
    if previous is None:
        prior = doc(producer_path)["checkpoint_history"][-1]
    else:
        prior = doc(root.parent / previous / f"{previous}-result.json")["checkpoint_history"][0]
    earned_predecessor(prior, result)
    if name in ("combat", "switch", "cloak"):
        cold_boundary(prior["state"], initial)
    for field in (
        "shields",
        "energy",
        "vulcan_ammo",
        "lives",
        "score",
        "primary_weapon",
        "primary_weapon_flags",
        "robots_killed_level",
        "has_blue_key",
        "has_red_key",
        "has_gold_key",
        "cloaked",
        "invulnerable",
    ):
        need(
            prior["state"]["player"][field] == start_player[field],
            f"earned resource changed during phase load: {field}",
        )
    if name == "guided_inventory":
        for field in ("secondary_ammo", "secondary_weapon_flags", "secondary_weapon"):
            need(
                prior["state"]["player"][field] == start_player[field],
                f"earned secondary changed during load: {field}",
            )
    for field in (
        "shields",
        "energy",
        "vulcan_ammo",
        "lives",
        "score",
        "primary_weapon",
        "primary_weapon_flags",
        "robots_killed_level",
        "has_blue_key",
        "has_red_key",
        "has_gold_key",
        "cloaked",
    ):
        need(
            field in start_player and field in end_player,
            f"missing saved player resource {field}",
        )
    assists = [event for event in events if event.get("event") == "position_assist"]
    expected_assists = 2 if name == "master" else 1
    need(
        result["position_assist_events"] == len(assists) == expected_assists,
        "undisclosed position assist",
    )
    if name == "combat":
        need(
            assists[0]["step"] == 6
            and assists[0]["action"] == "pose_view"
            and initial["position"]["segment"] == 1183
            and state_at(events, 7)["position"]["segment"] == 449
            and final["position"]["segment"] == 449
            and final["player"]["robots_killed_level"] == initial["player"]["robots_killed_level"] + 1
            and final["xl_pilot"]["wall_triggers"]["6"]["operated"] is False
            and not end_player["cloaked"]
            and not end_player["has_red_key"],
            "adjacent guard256 combat incomplete",
        )
        combat_setup(plan["steps"])
        need(
            plan["track_objects"] == [256, 305]
            and plan["steps"][8]["expect"]["game_time_raw"]["gte"] == initial["game_time_raw"] + 65536 // 60
            and plan["steps"][11]["expect"]["player.robots_killed_level"]["gte"]
            == initial["player"]["robots_killed_level"] + 1
            and plan["steps"][11]["face_object"] == 256
            and plan["steps"][11]["face_interval_ms"] == 17
            and plan["steps"][11]["timeout_ms"] == 3000
            and plan["steps"][14] == {"action": "wait_ms", "ms": 750},
            "combat guard goal or bounded neutral tail changed",
        )
        combat_trace_witness(events)
        need(result["fixed_view_aim_events"] > 0, "robot256 bound view aim missing")
        guard_drops(initial, final)
        need(
            0 < end_player["shields"] <= start_player["shields"]
            and 0 < end_player["energy"] <= start_player["energy"]
            and end_player["secondary_ammo"] == start_player["secondary_ammo"]
            and end_player["secondary_weapon_flags"] == start_player["secondary_weapon_flags"]
            and end_player["laser_level"] == start_player["laser_level"] == 0
            and end_player["quad_lasers"] is start_player["quad_lasers"] is False,
            "guard combat acquired an upgrade or unearned energy/shields",
        )
        operations(events, [])
    elif name == "switch":
        need(
            assists[0]["step"] == 6
            and assists[0]["action"] == "pose_view"
            and initial["position"]["segment"] == 449
            and plan["steps"][6]
            == {
                "action": "pose_view",
                "segment": 450,
                "position_fixed": [-29216306, 12619118, -10993001],
                "pitch": 285,
                "bank": 0,
                "heading": 4553,
            }
            and state_at(events, 7)["position"]["segment"] == 450
            and final["position"]["segment"] == 450
            and plan["track_objects"] == [305]
            and result["fixed_view_aim_events"] == 0
            and final["xl_pilot"]["wall_triggers"]["6"]["operated"] is True
            and final["xl_pilot"]["wall_triggers"]["6"]["shot"] == 1
            and all(final["xl_pilot"]["doorways"][str(w)]["type"] == 4 for w in (55, 56, 57, 58))
            and not any(p["object"] == 256 for p in initial["level_scene"]["robots"])
            and not end_player["cloaked"]
            and not end_player["has_red_key"],
            "switch6 cold guard-clear or opening incomplete",
        )
        need(
            all(plan["steps"][i] == {"action": "send_axis", "axis": i - 1, "axis_value": 0.0} for i in range(1, 6))
            and [plan["steps"][i]["action"] for i in range(7, 12)]
            == ["introspect", "send_button", "wait_ms", "introspect", "send_button"]
            and plan["steps"][8] == {"action": "send_button", "button": 100, "held": 1}
            and plan["steps"][9] == {"action": "wait_ms", "ms": 16}
            and plan["steps"][11] == {"action": "send_button", "button": 100, "held": 0}
            and all(
                plan["steps"][i : i + 2]
                == [
                    {"action": "wait_ms", "ms": 16},
                    {"action": "introspect"},
                ]
                for i in range(12, 192, 2)
            )
            and plan["steps"][193]
            == {
                "action": "set_debug",
                "field": "android_game_request",
                "value": "quick_save",
            },
            "single ordinary Laser0 pulse or bounded neutral tail changed",
        )
        original = doc(producer_path)["checkpoint_history"][-1]["state"]
        drops = guard_drops(original, initial)
        switch_trace_witness(events, plan, drops)
        need(
            end_player["energy"] < start_player["energy"]
            and end_player["shields"] <= start_player["shields"]
            and end_player["primary_weapon_flags"] == start_player["primary_weapon_flags"]
            and end_player["laser_level"] == start_player["laser_level"] == 0
            and end_player["quad_lasers"] is start_player["quad_lasers"] is False
            and end_player["secondary_ammo"] == start_player["secondary_ammo"]
            and end_player["secondary_weapon_flags"] == start_player["secondary_weapon_flags"],
            "switch acquired an upgrade or unearned energy/shields",
        )
        switch_shot_witness(events)
        operations(events, [(6, 1, 1)])
    elif name == "cloak":
        need(
            assists[0]["step"] == 1
            and assists[0]["action"] == "pose_view"
            and initial["position"]["segment"] == 450
            and state_at(events, 2)["position"]["segment"] == 450
            and final["position"]["segment"] == 457
            and not start_player["cloaked"]
            and end_player["cloaked"]
            and any(p["object"] == 113 and p["id"] == 23 for p in initial["level_scene"]["powerups"])
            and not any(p["object"] == 113 for p in final["level_scene"]["powerups"]),
            "cloak113 physical pickup incomplete",
        )
    elif name == "red_key":
        need(
            assists[0]["step"] == 1
            and assists[0]["action"] == "face_view"
            and initial["position"]["segment"] == 457
            and state_at(events, 2)["position"]["segment"] == 427
            and final["position"]["segment"] == 424
            and not start_player["has_red_key"]
            and end_player["has_red_key"]
            and start_player["cloaked"]
            and end_player["cloaked"]
            and any(p["object"] == 51 and p["id"] == 5 for p in initial["level_scene"]["powerups"])
            and not any(p["object"] == 51 for p in final["level_scene"]["powerups"]),
            "red key51 physical pickup incomplete",
        )
    elif name == "red_door":
        trigger = final["xl_pilot"]["wall_triggers"]["54"]
        operations(events, [(54, 1, 0)])
        need(
            assists[0]["step"] == 1
            and assists[0]["action"] == "face_view"
            and initial["position"]["segment"] == 424
            and state_at(events, 2)["position"]["segment"] == 1199
            and final["position"]["segment"] == 1188
            and not initial["xl_pilot"]["wall_triggers"]["54"]["operated"]
            and trigger["operated"]
            and trigger["actor"] == 0
            and trigger["shot"] == 0
            and trigger["type"] == 28
            and initial["xl_pilot"]["spawn_count"] == 0
            and final["xl_pilot"]["spawn_count"] == 16
            and all(
                final["xl_pilot"]["doorways"][str(w)]["flags"] == 18
                and final["xl_pilot"]["doorways"][str(w)]["state"] == 1
                for w in (128, 129)
            )
            and start_player["has_red_key"]
            and end_player["has_red_key"]
            and end_player["cloaked"],
            "red-door54 physical operation incomplete",
        )
    elif name == "master":
        operations(events, [(64, 1, 0), (63, 2, 0), (61, 3, 0)])
        need(
            [(e["step"], e["action"]) for e in assists] == [(1, "face_view"), (5, "face_view")]
            and initial["position"]["segment"] == 1188
            and state_at(events, 4)["position"]["segment"] == 1199
            and state_at(events, 6)["position"]["segment"] == 1199
            and final["position"]["segment"] == 1149
            and final["xl_pilot"]["remaining_objectives"] == 1
            and final["xl_pilot"]["spawn_count"] == 16
            and final["xl_pilot"]["operation_sequence"] == 3
            and all(
                final["xl_pilot"]["wall_triggers"][str(i)]["operated"]
                and final["xl_pilot"]["wall_triggers"][str(i)]["actor"] == 0
                and final["xl_pilot"]["wall_triggers"][str(i)]["shot"] == 0
                and final["xl_pilot"]["wall_triggers"][str(i)]["flags"] == 7
                for i in (64, 63, 61)
            )
            and start_player["has_red_key"]
            and end_player["has_red_key"]
            and end_player["cloaked"],
            "master61 child operation incomplete",
        )
    else:
        need(
            assists[0]["step"] == 1 and assists[0]["action"] == "face_view",
            "guided assist invalid",
        )
        guided_receipt_controls(result)
        operations(events, [])
        guided_inventory_witness(events, plan)
    need(
        not end_player["has_blue_key"] and not end_player["has_gold_key"],
        "unearned key grant",
    )


def main():
    if len(sys.argv) == 5 and sys.argv[1] == "producer":
        producer(Path(sys.argv[2]), Path(sys.argv[3]), Path(sys.argv[4]))
    elif len(sys.argv) == 6 and sys.argv[1] == "phase":
        phase(sys.argv[2], Path(sys.argv[3]), Path(sys.argv[4]), Path(sys.argv[5]))
    elif len(sys.argv) == 5 and sys.argv[1] == "load":
        load(Path(sys.argv[2]), Path(sys.argv[3]), Path(sys.argv[4]))
    else:
        raise WitnessError("expected producer or phase arguments")
    print("Tulev1 early witness PASS")


if __name__ == "__main__":
    try:
        main()
    except (WitnessError, KeyError, IndexError, TypeError, ValueError, OSError) as exc:
        raise SystemExit(f"Tulev1 early witness FAIL: {exc}")
