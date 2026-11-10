"""Registered, assisted-local switch 14 owner; no connected body-route claim."""

import json
import re
import sys
from collections import Counter
from itertools import pairwise
from pathlib import Path

import pumo_tulev1_supplies_contract as supplies

need = supplies.need
same = supplies.same
doc = supplies.doc
sha = supplies.sha
save = supplies.save
restored = supplies.restored

STEPS = {"warm": 23, "cold": 3}
SAVE_STEP = {"warm": 21, "cold": 1}
TRACKED = (210, 282, 283, 314)
GATES = (183, 184, 185, 186)
POSE = {
    "action": "pose_view",
    "segment": 1547,
    "position_fixed": [14876672, -4161536, -6094848],
    "pitch": 0,
    "bank": 0,
    "heading": 16882,
}


def template(kind):
    need(kind in STEPS, "unknown switch14 phase")
    return doc(Path(__file__).with_name(f"pumo_tulev1_switch14_{kind}.json"))


def producer_save(path, kind):
    need(
        path.name
        == ("guided140_cold-result.json" if kind == "warm" else "warm-result.json"),
        "not immediate registered switch14 producer",
    )
    receipt = doc(path)
    history = receipt["checkpoint_history"]
    step = 1 if kind == "warm" else SAVE_STEP["warm"]
    need(
        receipt["result"] == "PASS"
        and len(history) == 1
        and history[0]["step"] == step,
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
            state["position"]["segment"] == 458
            and state["player"]["secondary_ammo"][6] == 4
            and state["xl_pilot"]["wall_triggers"]["14"]["operated"] is False,
            "registered Guided140 source not ready for switch14",
        )
    return receipt, item


def binding(kind, plan, producer):
    _receipt, item = producer_save(producer, kind)
    expected = template(kind)
    for key in ("resume_checkpoint", "resume_sha256", "receipt_run_id"):
        if key in plan:
            expected[key] = plan[key]
    need(same(plan, expected), "unbound switch14 template changed")
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
        for index in (7, 9, 11):
            plan["steps"][index]["expect"]["player.secondary_ammo"] = before
        for index in (16, 19):
            plan["steps"][index]["expect"]["player.secondary_ammo"] = after
        for index in (11, 19):
            plan["steps"][index]["expect"].update(
                {
                    "player.shields": state["player"]["shields"],
                    "player.energy": state["player"]["energy"],
                    "player.robots_killed_level": state["player"][
                        "robots_killed_level"
                    ],
                }
            )
        plan["steps"][16]["expect"][
            "combat_observation.guided_control.parent_object"
        ] = state["combat_observation"]["player_object"]
    return plan


def bind(kind, path, producer):
    path.write_text(json.dumps(binding(kind, doc(path), producer), indent=4) + "\n")


def source(
    supplies_root,
    engine,
    courtyard,
    blast,
    guided_producer,
    guided_guard,
    quad,
    guard,
    blue,
):
    supplies.source(
        courtyard, engine, blast, guided_producer, guided_guard, quad, guard, blue
    )
    phases = (
        "switch6_warm",
        "switch6_cold",
        "cloak113_warm",
        "cloak113_cold",
        "guided140_warm",
        "guided140_cold",
    )
    prior = courtyard / "guard305" / "guard305-result.json"
    for phase in phases:
        root = supplies_root / phase
        supplies.verify(phase, root, engine, prior)
        prior = root / f"{phase}-result.json"
    producer_save(prior, "warm")


def player_common(state, baseline):
    pilot = state["xl_pilot"]
    player = state["player"]
    first = baseline["player"]
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
        and not player["has_gold_key"]
        and not player["has_red_key"]
        and player["robots_killed_level"] == first["robots_killed_level"] == 4
        and pilot["remaining_objectives"] == 1,
        "switch14 survival or inventory drifted",
    )
    for key, value in first.items():
        if key not in ("secondary_ammo", "secondary_weapon"):
            need(same(player[key], value), "unplanned player state change")
    ammo = player["secondary_ammo"]
    need(
        ammo[:6] == first["secondary_ammo"][:6]
        and ammo[6] in (4, 3)
        and ammo[7:] == first["secondary_ammo"][7:],
        "unplanned ammunition change",
    )


def actor_guards(state, source_actors):
    tracked = state["combat_observation"]["tracked_objects"]
    need(
        len(tracked) == len(TRACKED) and {a["object"] for a in tracked} == set(TRACKED),
        "switch14 tracked actor set changed",
    )
    for actor in tracked:
        slot = actor["object"]
        original = source_actors[slot]
        health = actor["actor_state"]
        need(
            actor["exists"]
            and actor["expected_type"] == actor["current_slot"]["type"] == 2
            and actor["expected_id"] == actor["current_slot"]["id"] == original[0]
            and actor["expected_signature"]
            == actor["current_slot"]["signature"]
            == original[2]
            and actor["damage_since_binding_raw"] == 0
            and actor["binding_shields_raw"] == health["shields_raw"] == original[3] > 0
            and not health["should_be_dead"]
            and actor["expected_level"] == actor["observed_level"] == 7
            and actor["expected_load_generation"]
            == actor["observed_load_generation"]
            == state["combat_observation"]["load_generation"]
            and health["segment"] not in (1547, 1546, 1544),
            "tracked actor identity, health, or corridor guard failed",
        )


def contact_guard(hit, shot, owner):
    need(
        hit["level"] == 7
        and hit["object"] == shot["object"]
        and hit["signature"] == shot["signature"]
        and hit["weapon_id"] == 37
        and hit["parent_object"] == owner["player_object"]
        and hit["parent_signature"] == owner["player_signature"]
        and hit["hit_segment"] == 1544
        and hit["hit_side"] == 2
        and hit["wall"] == 205
        and hit["trigger"] == 14
        and hit["base"] == 41
        and hit["overlay_raw"] == 414
        and hit["eclip"] == 72
        and hit["effect_dest"] == 413
        and hit["blew_up"] == 1,
        "original switch14 Guide contact invalid",
    )


def operation_guard(initial, final, operations):
    need(
        len(operations) == 1
        and operations[0]["index"] == 14
        and operations[0]["shot"] == 1
        and operations[0]["sequence"] == 1,
        "sole switch14 operation invalid",
    )
    need(
        final["xl_pilot"]["operation_sequence"] == 1
        and final["xl_pilot"]["wall_triggers"]["14"]["operated"] is True
        and all(final["xl_pilot"]["doorways"][str(w)]["type"] == 4 for w in GATES)
        and final["combat_observation"]["guided_control"] is None,
        "all switch14 gates not fully open or Guide not settled",
    )
    for name in ("doorways", "wall_triggers"):
        before, after = initial["xl_pilot"][name], final["xl_pilot"][name]
        allowed_ids = set(map(str, GATES if name == "doorways" else (14,)))
        need(
            set(before) == set(after)
            and {key for key in before if not same(before[key], after[key])}
            == allowed_ids,
            "unrelated switch14 wall or trigger changed",
        )


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
        and all(
            set(x) == {"step", "action", "state", "simulation_fps"}
            and x["simulation_fps"] == 60
            for x in rows
        ),
        "switch14 receipt or step trace invalid",
    )
    nonstates = Counter(x["event"] for x in events if "event" in x)
    allowed = (
        {
            "position_assist": 1,
            "successful_wall_operation": 1,
            "trigger_change": 1,
            "weapon_wall_contacts": 1,
        }
        if kind == "warm"
        else {}
    )
    need(
        nonstates == allowed
        and all("event" in x or ("step" in x and "state" in x) for x in events),
        "unexpected or missing switch14 event",
    )
    need(
        receipt["position_assist_events"] == (1 if kind == "warm" else 0)
        and receipt["fixed_view_aim_events"] == 0,
        "unplanned player assistance",
    )
    return rows


def normal_save_guard(root, receipt, saved, final, save_step, source_file):
    need(
        len(saved) == 1
        and saved[0]["step"] == save_step
        and re.fullmatch(
            rf"pumo-player-level7-[0-9]+-step{save_step}\.sav", saved[0]["file"]
        )
        and saved[0]["sha256"] == sha(save(root, saved[0]["file"]))
        and same(saved[0]["state"], final),
        "normal versioned switch14 save invalid",
    )
    need(set(receipt["checkpoints"]) == {"7"}, "normal save alias absent")
    alias = receipt["checkpoints"]["7"]
    need(
        alias["file"] == "pumo-player-level7.sav"
        and alias["sha256"] == saved[0]["sha256"]
        and alias["sha256"] == sha(save(root, alias["file"]))
        and alias["sha256"] == sha(save(root, "pumo-player-quick.sav"))
        and same(alias["state"], saved[0]["state"])
        and {p.name for p in (root / "preview-user").glob("*.sav")}
        == {source_file, saved[0]["file"], alias["file"], "pumo-player-quick.sav"},
        "normal save alias or profile inventory invalid",
    )


def shot_timing_guard(rows):
    before, born, asserted = (rows[index]["state"] for index in (14, 15, 16))
    need(
        before["player"]["secondary_ammo"][6] == 4
        and before["combat_observation"]["guided_control"] is None
        and before["combat_observation"]["owned_weapon_count"] == 0
        and born["player"]["secondary_ammo"][6] == 3
        and born["combat_observation"]["owned_weapon_count"] == 1
        and born["combat_observation"]["guided_control"] is not None
        and born["combat_observation"]["guided_control"]["weapon_id"] == 37
        and asserted["player"]["secondary_ammo"][6] == 3
        and same(
            asserted["combat_observation"]["guided_control"],
            born["combat_observation"]["guided_control"],
        ),
        "Guide birth did not occur on ordinary release step",
    )


def verify(kind, root, engine, producer):
    _prior, history = producer_save(producer, kind)
    receipt = doc(root / f"{kind}-result.json")
    plan_path = root / f"{kind}-executed-plan.json"
    plan = doc(plan_path)
    expected = template(kind) | {
        k: plan[k] for k in ("resume_checkpoint", "resume_sha256", "receipt_run_id")
    }
    need(
        same(plan, binding(kind, expected, producer)), "executed switch14 plan changed"
    )
    final = doc(root / f"{kind}-state.json")
    events = [
        json.loads(line)
        for line in (root / f"{kind}-trace.jsonl").read_text().splitlines()
        if line
    ]
    rows = trace_guard(kind, receipt, plan, final, events)
    need(
        receipt["resume_sha256"] == plan["resume_sha256"] == history["sha256"]
        and plan["resume_checkpoint"] == history["file"]
        and receipt["plan_sha256"] == sha(plan_path)
        and receipt["executable_sha256"] == sha(engine)
        and receipt["run_id"] == plan["receipt_run_id"] != _prior["run_id"]
        and receipt["process_id"] != _prior["process_id"],
        "switch14 producer or execution identity invalid",
    )
    need(
        not receipt["transitions"] and receipt["final_level"] == 7,
        "switch14 unexpectedly changed levels",
    )
    initial = rows[0]["state"]
    earned = restored(history["state"])
    earned["combat_observation"]["tracked_objects"] = (
        initial["combat_observation"]["tracked_objects"] if kind == "warm" else []
    )
    need(same(initial, earned), "not immediate registered predecessor resume")
    saved = receipt["checkpoint_history"]
    save_step = SAVE_STEP[kind]
    normal_save_guard(root, receipt, saved, final, save_step, history["file"])
    if kind == "cold":
        supplies.cold_save_parity(history, saved[0], producer.parent, root)
        need(
            final["game_time_raw"] == initial["game_time_raw"] and same(final, initial),
            "switch14 cold no-frame replay drifted",
        )
        return
    assists = [x for x in events if x.get("event") == "position_assist"]
    need(
        len(assists) == 1
        and assists[0]["step"] == 6
        and assists[0]["action"] == "pose_view"
        and plan["steps"][6] == POSE,
        "switch14 pose or assist invalid",
    )
    need(
        [x for x in plan["steps"] if x["action"] == "send_button"]
        == [
            {"action": "send_button", "button": 101, "held": 1},
            {"action": "send_button", "button": 101, "held": 0},
        ]
        and [(x["ms"]) for x in plan["steps"] if x["action"] == "wait_ms"]
        == [1000, 1, 2100],
        "not one ordinary rearmed shot and neutral tail",
    )
    source_actors = supplies.saved_actor_radii(
        save(producer.parent, history["file"]), history["state"]
    )
    need(all(x in source_actors for x in TRACKED), "missing source actors")
    initial_items = supplies.items(initial)
    initial_gates = {w: initial["xl_pilot"]["doorways"][str(w)] for w in GATES}
    basis = rows[7]["state"]["position"]
    axes = ("orient_right_fixed", "orient_up_fixed", "orient_forward_fixed")
    need(
        all(g["type"] == 5 for g in initial_gates.values())
        and not initial["xl_pilot"]["wall_triggers"]["14"]["operated"],
        "switch14 initially open or operated",
    )
    for index, row in enumerate(rows):
        state = row["state"]
        player_common(state, initial)
        actor_guards(state, source_actors)
        need(supplies.items(state) == initial_items, "switch14 local pickup changed")
        if index >= 7:
            pos = state["position"]
            need(
                pos["segment"] == 1547
                and abs(pos["x"] - 227) <= 0.25
                and abs(pos["y"] + 63.5) <= 0.6
                and abs(pos["z"] + 93) <= 0.25
                and all(pos[axis] == basis[axis] for axis in axes),
                "switch14 local position escaped authored envelope",
            )
        if index < 15:
            need(
                state["player"]["secondary_ammo"][6] == 4,
                "shot fired before owned birth",
            )
        if index >= 15:
            need(state["player"]["secondary_ammo"][6] == 3, "extra Guide round spent")
    shot_timing_guard(rows)
    need(
        all(
            a["state"]["game_time_raw"] <= b["state"]["game_time_raw"]
            for a, b in pairwise(rows)
        ),
        "switch14 clock went backward",
    )
    for event in events:
        if event.get("event") in ("successful_wall_operation", "trigger_change"):
            player_common(event["state"], initial)
            actor_guards(event["state"], source_actors)
            need(
                supplies.items(event["state"]) == initial_items,
                "pickup occurred during switch14 operation",
            )
    ready, born = rows[11]["state"], rows[15]["state"]
    need(
        rows[9]["state"]["secondary_fire"]["next_delta_raw"] == 65536
        and ready["secondary_fire"]["next_delta_raw"] == 0
        and ready["game_time_raw"] - initial["game_time_raw"] >= 65536
        and born["combat_observation"]["guided_control"]["weapon_id"] == 37,
        "ordinary secondary rearm or owned Guide birth missing",
    )
    shot = born["combat_observation"]["guided_control"]
    owner = initial["combat_observation"]
    need(
        shot["parent_object"] == owner["player_object"]
        and shot["parent_signature"] == owner["player_signature"]
        and shot["lifeleft_raw"] > 0,
        "Guide parent or life invalid",
    )
    contacts = [x["result"] for x in events if x.get("event") == "weapon_wall_contacts"]
    need(
        len(contacts) == 1
        and contacts[0]["contact_count_lower_bound"] == 1
        and not contacts[0]["truncated"]
        and len(contacts[0]["contacts"]) == 1,
        "sole actual wall contact missing",
    )
    hit = contacts[0]["contacts"][0]
    contact_guard(hit, shot, owner)
    ops = [x for x in events if x.get("event") == "successful_wall_operation"]
    need(
        next(
            i
            for i, x in enumerate(events)
            if x.get("event") == "successful_wall_operation"
        )
        < next(i for i, x in enumerate(events) if x.get("event") == "trigger_change")
        < next(
            i for i, x in enumerate(events) if x.get("event") == "weapon_wall_contacts"
        ),
        "actual contact/operation observation order changed",
    )
    operation_guard(initial, final, ops)


def main():
    if len(sys.argv) == 11 and sys.argv[1] == "source":
        source(*(Path(x) for x in sys.argv[2:]))
    elif len(sys.argv) == 5 and sys.argv[1] == "bind":
        bind(sys.argv[2], Path(sys.argv[3]), Path(sys.argv[4]))
    elif len(sys.argv) == 6 and sys.argv[1] == "verify":
        verify(sys.argv[2], Path(sys.argv[3]), Path(sys.argv[4]), Path(sys.argv[5]))
    else:
        raise ValueError("invalid switch14 contract arguments")


if __name__ == "__main__":
    main()
