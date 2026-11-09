"""Registered assisted-local switch 13 owner; no boss or exit claim."""

import json
import math
import re
import struct
import sys
from collections import Counter
from itertools import pairwise
from pathlib import Path

import pumo_tulev1_supplies_contract as supplies
import pumo_tulev1_switch14_contract as switch14

need = supplies.need
same = supplies.same
doc = supplies.doc
sha = supplies.sha
save = supplies.save
restored = supplies.restored
TRACKED = (210, 282, 283, 314)
PRIOR_GATES = (183, 184, 185, 186, 201, 202)
GATES = tuple(range(193, 201))
STEPS = {"warm": 19, "cold": 3}
SAVE_STEP = {"warm": 17, "cold": 1}
POSE = {
    "action": "pose_view",
    "segment": 1537,
    "position_fixed": [20371657, -12582912, 2670085],
    "pitch": -9441,
    "bank": 0,
    "heading": 4975,
}
POSE_POINT = tuple(x / 65536 for x in POSE["position_fixed"])
BASIS = {
    "orient_right_fixed": [58216, 0, -30080],
    "orient_up_fixed": [-23657, 40472, -45786],
    "orient_forward_fixed": [18576, 51544, 35951],
}


def template(kind):
    need(kind in STEPS, "unknown switch13 phase")
    return doc(Path(__file__).with_name(f"pumo_tulev1_switch13_{kind}.json"))


def producer_save(path, kind):
    need(
        path.name == ("cold-result.json" if kind == "warm" else "warm-result.json"),
        "not immediate registered switch13 producer",
    )
    receipt = doc(path)
    history = receipt["checkpoint_history"]
    step = 1 if kind == "warm" else SAVE_STEP["warm"]
    need(
        receipt["result"] == "PASS" and len(history) == 1 and history[0]["step"] == step,
        "producer did not save at expected step",
    )
    item = history[0]
    need(
        re.fullmatch(rf"pumo-player-level7-[0-9]+-step{step}\.sav", item["file"])
        and item["sha256"] == sha(save(path.parent, item["file"])),
        "producer save invalid",
    )
    if kind == "warm":
        state = item["state"]
        need(
            state["current_level_num"] == 7
            and state["position"]["segment"] == 1547
            and state["player"]["secondary_weapon"] == 6
            and state["player"]["secondary_ammo"][6] >= 1
            and state["secondary_fire"]["next_delta_raw"] <= 0
            and state["xl_pilot"]["wall_triggers"]["14"]["operated"]
            and not state["xl_pilot"]["wall_triggers"]["13"]["operated"]
            and all(state["xl_pilot"]["doorways"][str(w)]["type"] == 4 for w in PRIOR_GATES)
            and all(state["xl_pilot"]["doorways"][str(w)]["type"] == 5 for w in GATES),
            "registered switch14 cold source not ready for switch13",
        )
    return receipt, item


def binding(kind, plan, producer):
    _receipt, item = producer_save(producer, kind)
    expected = template(kind)
    for key in ("resume_checkpoint", "resume_sha256", "receipt_run_id"):
        if key in plan:
            expected[key] = plan[key]
    need(same(plan, expected), "unbound switch13 template changed")
    state = item["state"]
    plan["checkpoint_expect"] = {
        "game_time_raw": state["game_time_raw"],
        "position.segment": state["position"]["segment"],
        "player.shields": state["player"]["shields"],
        "player.energy": state["player"]["energy"],
        "player.secondary_ammo": state["player"]["secondary_ammo"],
    }
    plan["steps"][0]["expect"].update(plan["checkpoint_expect"])
    if kind == "warm":
        before = state["player"]["secondary_ammo"]
        after = before.copy()
        after[6] -= 1
        for index in (7, 12, 15):
            plan["steps"][index]["expect"]["player.secondary_ammo"] = before if index == 7 else after
            plan["steps"][index]["expect"].update(
                {
                    "player.shields": state["player"]["shields"],
                    "player.energy": state["player"]["energy"],
                    "player.robots_killed_level": state["player"]["robots_killed_level"],
                }
            )
        plan["steps"][12]["expect"]["combat_observation.guided_control.parent_object"] = state["combat_observation"][
            "player_object"
        ]
    return plan


def bind(kind, path, producer):
    path.write_text(json.dumps(binding(kind, doc(path), producer), indent=4) + "\n")


def source(switch14_root, engine, supplies_root, courtyard, blast, guided_producer, guided_guard, quad, guard, blue):
    switch14.source(supplies_root, engine, courtyard, blast, guided_producer, guided_guard, quad, guard, blue)
    prior = supplies_root / "guided140_cold" / "guided140_cold-result.json"
    for kind in ("warm", "cold"):
        root = switch14_root / kind
        switch14.verify(kind, root, engine, prior)
        prior = root / f"{kind}-result.json"
    producer_save(prior, "warm")


def pose_envelope(position):
    """Keep observed drift near the source-screened local-up displacement."""
    delta = [position[axis] - POSE_POINT[index] for index, axis in enumerate(("x", "y", "z"))]
    up = [value / 65536 for value in BASIS["orient_up_fixed"]]
    along = sum(a * b for a, b in zip(delta, up))
    transverse = [a - along * b for a, b in zip(delta, up)]
    return abs(along) <= 0.7 and sum(x * x for x in transverse) <= 0.02 * 0.02


def player_common(state, baseline, ammo_after):
    player = state["player"]
    first = baseline["player"]
    pilot = state["xl_pilot"]
    need(
        state["current_level_num"] == 7
        and not state["player_dead"]
        and not state["cheats_enabled"]
        and player["shields"] == first["shields"] > 0
        and player["energy"] == first["energy"]
        and not player["invulnerable"]
        and player["quad_lasers"]
        and player["cloaked"]
        and player["has_blue_key"]
        and player["robots_killed_level"] == first["robots_killed_level"]
        and pilot["remaining_objectives"] == baseline["xl_pilot"]["remaining_objectives"]
        and player["secondary_ammo"] in (first["secondary_ammo"], ammo_after),
        "switch13 survival, objective or inventory drifted",
    )
    for key, value in first.items():
        if key != "secondary_ammo":
            need(same(player[key], value), "unplanned player state change")


def actor_guards(state, source_actors):
    tracked = state["combat_observation"]["tracked_objects"]
    need(
        len(tracked) == len(TRACKED) and {a["object"] for a in tracked} == set(TRACKED),
        "switch13 tracked actor set changed",
    )
    for actor in tracked:
        slot = actor["object"]
        original = source_actors[slot]
        health = actor["actor_state"]
        need(
            actor["exists"]
            and actor["expected_type"] == actor["current_slot"]["type"] == 2
            and actor["expected_id"] == actor["current_slot"]["id"] == original[0]
            and actor["expected_signature"] == actor["current_slot"]["signature"] == original[2]
            and actor["damage_since_binding_raw"] == 0
            and actor["binding_shields_raw"] == health["shields_raw"] == original[3] > 0
            and not health["should_be_dead"]
            and actor["expected_level"] == actor["observed_level"] == 7
            and actor["expected_load_generation"]
            == actor["observed_load_generation"]
            == state["combat_observation"]["load_generation"]
            and health["segment"] != 1537,
            "tracked actor identity, health or local corridor guard failed",
        )


def contact_guard(hit, shot, owner):
    need(
        hit["level"] == 7
        and hit["object"] == shot["object"]
        and hit["signature"] == shot["signature"]
        and hit["weapon_id"] == 37
        and hit["parent_object"] == owner["player_object"]
        and hit["parent_signature"] == owner["player_signature"]
        and hit["hit_segment"] == 1537
        and hit["hit_side"] == 2
        and hit["wall"] == 203
        and hit["trigger"] == 13
        and hit["base"] == 137
        and hit["overlay_raw"] == 414
        and hit["eclip"] == 72
        and hit["effect_dest"] == 413
        and hit["blew_up"] == 1,
        "original switch13 Guide contact invalid",
    )


def operation_guard(initial, final, operations):
    need(
        len(operations) == 1
        and operations[0]["index"] == 13
        and operations[0]["shot"] == 1
        and operations[0]["sequence"] == initial["xl_pilot"]["operation_sequence"] + 1,
        "sole switch13 operation invalid",
    )
    need(
        final["xl_pilot"]["operation_sequence"] == initial["xl_pilot"]["operation_sequence"] + 1
        and final["xl_pilot"]["wall_triggers"]["13"]["operated"]
        and all(final["xl_pilot"]["doorways"][str(w)]["type"] == 4 for w in GATES)
        and final["combat_observation"]["guided_control"] is None,
        "all switch13 gates not fully open or Guide not settled",
    )
    for name in ("doorways", "wall_triggers"):
        before, after = initial["xl_pilot"][name], final["xl_pilot"][name]
        allowed = set(map(str, GATES if name == "doorways" else (13,)))
        need(
            set(before) == set(after) and {key for key in before if not same(before[key], after[key])} == allowed,
            "unrelated switch13 wall or trigger changed",
        )


def saved_world_guard(source_path, source_state, final_path, final_state):
    """Check every saved actor and pickup, not a distance-filtered scene list."""

    def extract(path, state):
        raw = path.read_bytes()
        base, count = supplies.saved_object_table(raw, state)
        selected = {}
        for slot in range(count):
            offset = base + slot * 264
            kind = raw[offset + 4]
            if kind not in (2, 7):
                continue
            selected[slot] = (
                kind,
                raw[offset + 5],
                struct.unpack_from("<i", raw, offset)[0],
                struct.unpack_from("<i", raw, offset + 66)[0],
                struct.unpack_from("<i", raw, offset + 70)[0],
                *(
                    (struct.unpack_from("<h", raw, offset + 14)[0], struct.unpack_from("<3i", raw, offset + 18))
                    if kind == 7
                    else ()
                ),
            )
        return selected, raw, base

    original, source_raw, source_base = extract(source_path, source_state)
    actual, final_raw, final_base = extract(final_path, final_state)
    need(original.keys() == actual.keys(), "saved actor or pickup set changed")
    for slot in original:
        if slot != 414:
            need(original[slot] == actual[slot], "saved actor or pickup changed")
            continue
        before, after = original[slot], actual[slot]
        need(
            before[:5] == (7, 1, 1111, 196608, 1310720) and before[5] == after[5] == 447 and before[:6] == after[:6],
            "distant moving pickup identity changed",
        )
        source_at = source_base + slot * 264
        final_at = final_base + slot * 264
        # The only moving pickup is an ordinary bouncing, drag-damped powerup.
        # Static physics and the life clock must remain source-derived.
        need(
            source_raw[source_at + 11] == final_raw[final_at + 11] == 1
            and source_raw[source_at + 106 : source_at + 130] == final_raw[final_at + 106 : final_at + 130]
            and struct.unpack_from("<3i", source_raw, source_at + 106)[0:3] == (0, 0, 0)
            and struct.unpack_from("<ii", source_raw, source_at + 118) == (65536, 512)
            and struct.unpack_from("<H", source_raw, source_at + 156)[0]
            == struct.unpack_from("<H", final_raw, final_at + 156)[0]
            == 4,
            "distant pickup physics changed",
        )
        elapsed = final_state["game_time_raw"] - source_state["game_time_raw"]
        need(
            0 <= elapsed <= 140000
            and struct.unpack_from("<i", source_raw, source_at + 90)[0]
            - struct.unpack_from("<i", final_raw, final_at + 90)[0]
            == elapsed,
            "distant pickup life clock changed",
        )
        velocity = struct.unpack_from("<3i", source_raw, source_at + 94)
        initial_speed = math.dist((0, 0, 0), velocity) / 65536
        final_speed = math.dist((0, 0, 0), struct.unpack_from("<3i", final_raw, final_at + 94)) / 65536
        # The source geometry places the radius-3 pickup within two wall
        # planes. Their summed sphere overlap is below 2.3 units, including
        # fixed-point rounding. PF_BOUNCE can add at most 2 units/s at a
        # clamped wall response. This is a restrictive acceptance bound,
        # not a prediction of every possible multi-bounce trajectory.
        limit = 2.3 + (initial_speed + 2) * (elapsed / 65536)
        movement = math.dist(before[6], after[6]) / 65536
        if elapsed == 0:
            need(
                before[6] == after[6]
                and velocity == struct.unpack_from("<3i", final_raw, final_at + 94)
                and source_raw[source_at + 74 : source_at + 86] == final_raw[final_at + 74 : final_at + 86],
                "no-frame cold pickup drifted",
            )
        need(movement <= limit and final_speed <= initial_speed + 2, "distant pickup motion escaped bounce envelope")
        for raw, offset, record in ((source_raw, source_at, before), (final_raw, final_at, after)):
            last = struct.unpack_from("<3i", raw, offset + 74)
            need(math.dist(last, record[6]) / 65536 <= 2, "distant pickup last position jumped")
        need(math.dist(tuple(x / 65536 for x in after[6]), POSE_POINT) > 70, "distant pickup reached local shot")
    need(all(slot in original for slot in TRACKED), "tracked boss or actor absent")


def trace_guard(kind, receipt, plan, final, events):
    rows = [x for x in events if "step" in x and "state" in x]
    count = STEPS[kind]
    need(
        receipt["result"] == "PASS"
        and receipt["receipt_version"] == 2
        and receipt["resumed_earned_checkpoint"] is True
        and receipt["initial_level_setup"] == receipt["final_level"] == 7
        and receipt["rendered"] is False
        and receipt["transitions"] == []
        and receipt["steps"] == len(rows) == count
        and [x["step"] for x in rows] == list(range(count))
        and [x["action"] for x in rows] == [x["action"] for x in plan["steps"]]
        and same(rows[-1]["state"], final)
        and all(set(x) == {"step", "action", "state", "simulation_fps"} and x["simulation_fps"] == 60 for x in rows),
        "switch13 receipt or step trace invalid",
    )
    nonstates = Counter(x["event"] for x in events if "event" in x)
    allowed = (
        {"position_assist": 1, "successful_wall_operation": 1, "trigger_change": 1, "weapon_wall_contacts": 1}
        if kind == "warm"
        else {}
    )
    need(
        nonstates == allowed and all("event" in x or ("step" in x and "state" in x) for x in events),
        "unexpected or missing switch13 event",
    )
    need(
        receipt["position_assist_events"] == (1 if kind == "warm" else 0) and receipt["fixed_view_aim_events"] == 0,
        "unplanned player assistance",
    )
    return rows


def verify(kind, root, engine, producer):
    prior, history = producer_save(producer, kind)
    receipt = doc(root / f"{kind}-result.json")
    plan_path = root / f"{kind}-executed-plan.json"
    plan = doc(plan_path)
    expected = template(kind) | {k: plan[k] for k in ("resume_checkpoint", "resume_sha256", "receipt_run_id")}
    need(same(plan, binding(kind, expected, producer)), "executed switch13 plan changed")
    final = doc(root / f"{kind}-state.json")
    events = [json.loads(line) for line in (root / f"{kind}-trace.jsonl").read_text().splitlines() if line]
    rows = trace_guard(kind, receipt, plan, final, events)
    need(
        receipt["resume_sha256"] == plan["resume_sha256"] == history["sha256"]
        and plan["resume_checkpoint"] == history["file"]
        and receipt["plan_sha256"] == sha(plan_path)
        and receipt["executable_sha256"] == sha(engine)
        and receipt["run_id"] == plan["receipt_run_id"] != prior["run_id"]
        and receipt["process_id"] != prior["process_id"],
        "switch13 producer or execution identity invalid",
    )
    initial = rows[0]["state"]
    earned = restored(history["state"])
    earned["combat_observation"]["tracked_objects"] = (
        initial["combat_observation"]["tracked_objects"] if kind == "warm" else []
    )
    need(same(initial, earned), "not immediate registered predecessor resume")
    saved = receipt["checkpoint_history"]
    switch14.normal_save_guard(root, receipt, saved, final, SAVE_STEP[kind], history["file"])
    saved_world_guard(
        save(producer.parent, history["file"]), history["state"], save(root, saved[0]["file"]), saved[0]["state"]
    )
    if kind == "cold":
        supplies.cold_save_parity(history, saved[0], producer.parent, root)
        need(
            final["game_time_raw"] == initial["game_time_raw"] and same(final, initial),
            "switch13 cold no-frame replay drifted",
        )
        return
    assists = [x for x in events if x.get("event") == "position_assist"]
    need(
        len(assists) == 1
        and assists[0]["step"] == 6
        and assists[0]["action"] == "pose_view"
        and plan["steps"][6] == POSE,
        "switch13 pose or assist invalid",
    )
    need(
        [x for x in plan["steps"] if x["action"] == "send_button"]
        == [{"action": "send_button", "button": 101, "held": 1}, {"action": "send_button", "button": 101, "held": 0}]
        and [x["ms"] for x in plan["steps"] if x["action"] == "wait_ms"] == [1, 2100],
        "not one ordinary ready Guide shot and neutral tail",
    )
    source_actors = supplies.saved_actor_radii(save(producer.parent, history["file"]), history["state"])
    need(all(x in source_actors for x in TRACKED), "missing source actors")
    initial_items = supplies.items(initial)
    ammo_after = initial["player"]["secondary_ammo"].copy()
    ammo_after[6] -= 1
    need(
        all(initial["xl_pilot"]["doorways"][str(w)]["type"] == 4 for w in PRIOR_GATES)
        and all(initial["xl_pilot"]["doorways"][str(w)]["type"] == 5 for w in GATES)
        and initial["xl_pilot"]["wall_triggers"]["14"]["operated"]
        and not initial["xl_pilot"]["wall_triggers"]["13"]["operated"]
        and initial["secondary_fire"]["next_delta_raw"] <= 0,
        "switch13 prerequistes not ready",
    )
    for index, row in enumerate(rows):
        state = row["state"]
        player_common(state, initial, ammo_after)
        actor_guards(state, source_actors)
        need(supplies.items(state) == initial_items, "switch13 local pickup changed")
        if index >= 7:
            position = state["position"]
            need(
                position["segment"] == 1537
                and 310.55 <= position["x"] <= 311.15
                and -192.7 <= position["y"] <= -191.3
                and 40.15 <= position["z"] <= 41.35
                and all(position[k] == BASIS[k] for k in BASIS)
                and pose_envelope(position),
                "switch13 local position escaped authored envelope",
            )
        need(
            state["player"]["secondary_ammo"] == (initial["player"]["secondary_ammo"] if index < 11 else ammo_after),
            "Guide round spent at wrong step or extra round spent",
        )
    need(
        all(a["state"]["game_time_raw"] <= b["state"]["game_time_raw"] for a, b in pairwise(rows)),
        "switch13 clock went backward",
    )
    for event in events:
        if event.get("event") in ("successful_wall_operation", "trigger_change"):
            player_common(event["state"], initial, ammo_after)
            actor_guards(event["state"], source_actors)
            need(supplies.items(event["state"]) == initial_items, "pickup during switch13 operation")
    # PRE trace rows report state before each action. The 1 ms wait at step 10
    # queues a single 60 fps tick; the owned missile is first visible at 11.
    need(
        rows[9]["state"]["game_time_raw"] == rows[10]["state"]["game_time_raw"]
        and rows[10]["state"]["player"]["secondary_ammo"] == initial["player"]["secondary_ammo"]
        and rows[10]["state"]["combat_observation"]["owned_weapon_count"] == 0
        and rows[11]["state"]["game_time_raw"] - rows[10]["state"]["game_time_raw"] in (1092, 1093)
        and rows[15]["state"]["game_time_raw"] - rows[11]["state"]["game_time_raw"] in (137625, 137626),
        "switch13 PRE tick or finite tail timing invalid",
    )
    before, born = rows[7]["state"], rows[11]["state"]
    need(
        before["combat_observation"]["guided_control"] is None
        and before["combat_observation"]["owned_weapon_count"] == 0
        and before["secondary_fire"]["next_delta_raw"] <= 0
        and born["combat_observation"]["owned_weapon_count"] == 1
        and born["combat_observation"]["guided_control"] is not None
        and born["player"]["secondary_ammo"] == ammo_after,
        "ordinary Guide birth missing",
    )
    shot = born["combat_observation"]["guided_control"]
    owner = initial["combat_observation"]
    need(
        shot["weapon_id"] == 37
        and shot["parent_object"] == owner["player_object"]
        and shot["parent_signature"] == owner["player_signature"]
        and shot["lifeleft_raw"] > 0
        and not born["combat_observation"]["owned_weapons_truncated"]
        and len(born["combat_observation"]["owned_weapons"]) == 1
        and born["combat_observation"]["owned_weapons"][0]["object"] == shot["object"]
        and born["combat_observation"]["owned_weapons"][0]["signature"] == shot["signature"]
        and born["combat_observation"]["owned_weapons"][0]["weapon_id"] == 37
        and born["combat_observation"]["owned_weapons"][0]["parent_type"] == 4
        and born["combat_observation"]["owned_weapons"][0]["parent_object"] == owner["player_object"]
        and born["combat_observation"]["owned_weapons"][0]["parent_signature"] == owner["player_signature"],
        "Guide parent, life or observer invalid",
    )
    contacts = [x["result"] for x in events if x.get("event") == "weapon_wall_contacts"]
    need(
        len(contacts) == 1
        and contacts[0]["contact_count_lower_bound"] == 1
        and not contacts[0]["truncated"]
        and len(contacts[0]["contacts"]) == 1,
        "sole actual wall contact missing",
    )
    contact_guard(contacts[0]["contacts"][0], shot, owner)
    operations = [x for x in events if x.get("event") == "successful_wall_operation"]
    need(
        operations[0]["state"]["game_time_raw"] > born["game_time_raw"]
        and final["game_time_raw"] - operations[0]["state"]["game_time_raw"] > 65536,
        "switch13 operated before birth or did not settle for one second",
    )
    operation_guard(initial, final, operations)


def main():
    if len(sys.argv) == 12 and sys.argv[1] == "source":
        source(*(Path(x) for x in sys.argv[2:]))
    elif len(sys.argv) == 5 and sys.argv[1] == "bind":
        bind(sys.argv[2], Path(sys.argv[3]), Path(sys.argv[4]))
    elif len(sys.argv) == 6 and sys.argv[1] == "verify":
        verify(sys.argv[2], Path(sys.argv[3]), Path(sys.argv[4]), Path(sys.argv[5]))
    else:
        raise ValueError("invalid switch13 contract arguments")


if __name__ == "__main__":
    main()
