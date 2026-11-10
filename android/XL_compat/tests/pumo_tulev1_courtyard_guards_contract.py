"""Earned assisted-local Quad proof for original Tulev1 courtyard guards."""

import json
import re
import sys
from pathlib import Path

import pumo_tulev1_blast136_contract as blast

need = blast.need
same = blast.same
doc = blast.doc
sha = blast.sha
save = blast.save
decode = blast.decode
read = blast.read
restored = blast.restored

ACTORS = {256: (28, 257, 449, 44), 305: (44, 306, 450, 19)}
PHASES = {
    "guard256": (256, "warm"),
    "guard256_cold": (256, "cold"),
    "guard305": (305, "warm"),
    "guard305_cold": (305, "cold"),
}


def template(actor, kind):
    return doc(Path(__file__).with_name(f"pumo_tulev1_courtyard_guard{actor}_{kind}.json"))


def source(path, engine, guided_producer, guard, quad, ordinary, blue):
    need(path.name == "warm-result.json", "not registered blast136 producer")
    return blast.warm(path.parent, engine, guided_producer, guard, quad, ordinary, blue)


def source_save(path):
    receipt = doc(path)
    source_steps = {"warm-result.json": 14, "guard256-result.json": 44, "guard305-result.json": 19}
    need(path.name in source_steps, "wrong courtyard producer")
    need(
        receipt["result"] == "PASS" and len(receipt["checkpoint_history"]) == 1, "source has no passing versioned save"
    )
    history = receipt["checkpoint_history"][0]
    expected_step = source_steps[path.name]
    need(
        same(history["step"], expected_step)
        and re.fullmatch(rf"pumo-player-level7-[0-9]+-step{expected_step}\.sav", history["file"])
        and history["sha256"] == sha(save(path.parent, history["file"])),
        "source save identity invalid",
    )
    return receipt, history


def ordinary(state, kills):
    blast.ordinary(state)
    pilot = state["xl_pilot"]
    need(
        state["current_level_num"] == 7
        and state["player"]["shields"] > 0
        and same(state["player"]["robots_killed_level"], kills)
        and same(pilot["remaining_objectives"], 1)
        and pilot["wall_triggers"]["6"]["operated"] is False
        and all(same(pilot["doorways"][str(i)]["type"], 5) for i in (55, 56, 57, 58))
        and all(same(pilot["doorways"][str(i)]["flags"], 17) for i in (136, 137))
        and state["combat_observation"]["guided_control"] is None,
        "ordinary actor, objective, switch or gates drifted",
    )


def actor_binding(state, actor, must_exist):
    ident, signature, _segment, _step = ACTORS[actor]
    combat = state["combat_observation"]
    tracked = combat["tracked_objects"]
    need(len(tracked) == 1, "tracked actor count drifted")
    item = tracked[0]
    generation = combat["load_generation"]
    need(
        all(
            same(item[key], value)
            for key, value in (
                ("object", actor),
                ("expected_id", ident),
                ("expected_signature", signature),
                ("expected_type", 2),
                ("expected_level", 7),
                ("observed_level", 7),
                ("expected_load_generation", generation),
                ("observed_load_generation", generation),
            )
        )
        and item["exists"] is must_exist,
        "wrong original actor binding or generation",
    )
    if must_exist:
        need(
            all(
                same(item["current_slot"][key], value)
                for key, value in (("id", ident), ("signature", signature), ("type", 2))
            )
            and item["actor_state"] is not None,
            "wrong live actor identity",
        )
    return item


def bind(actor, plan_path, producer_path):
    plan = doc(plan_path)
    phase = f"guard{actor}"
    need(producer_path.name == ("warm-result.json" if actor == 256 else "guard256-result.json"), "wrong actor producer")
    _receipt, history = source_save(producer_path)
    expected = template(actor, "warm")
    expected["resume_checkpoint"] = plan["resume_checkpoint"]
    expected["resume_sha256"] = plan["resume_sha256"]
    if "receipt_run_id" in plan:
        expected["receipt_run_id"] = plan["receipt_run_id"]
    need(same(plan, expected), "unbound courtyard template altered")
    source_state = history["state"]
    kills = source_state["player"]["robots_killed_level"]
    need(same(kills, 2 if actor == 256 else 3), "source kill credit drifted")
    fields = {
        "game_time_raw": source_state["game_time_raw"],
        "player.shields": source_state["player"]["shields"],
        "player.energy": source_state["player"]["energy"],
        "player.robots_killed_level": kills,
        "position.segment": source_state["position"]["segment"],
        "player.cloaked": source_state["player"]["cloaked"],
    }
    for key, value in fields.items():
        plan["checkpoint_expect"][key] = value
        plan["steps"][0]["expect"][key] = value
    if actor == 256:
        for burst, index in enumerate(range(9, 24, 2)):
            plan["steps"][index]["expect"] = {
                "game_time_raw": {"gte": fields["game_time_raw"] + (1 + 15 * burst) * 65536 // 60}
            }
    else:
        for frame, index in ((1, 8), (2, 11)):
            plan["steps"][index]["expect"] = {"game_time_raw": {"gte": fields["game_time_raw"] + frame * 65536 // 60}}
        item = source_state["combat_observation"]["tracked_objects"]
        # The source phase watched guard256, so bind guard305 from the new run's
        # initial actor observation rather than copying private measured HP.
        need(len(item) == 1 and item[0]["object"] == 256, "wrong guard256 ancestry")
        source_robots = [
            robot for robot in source_state["level_scene"]["robots"] if robot["object"] == 305 and robot["id"] == 44
        ]
        need(len(source_robots) == 1 and source_robots[0]["shields"] > 0, "original guard305 missing from source scene")
        minimum_damage = round(source_robots[0]["shields"] * 65536) + 1
        plan["steps"][13]["expect"]["combat_observation.tracked_objects"]["contains_object"][
            "damage_since_binding_raw"
        ] = {"gte": minimum_damage}
        plan["steps"][13]["expect"]["player.robots_killed_level"] = {"gte": kills + 1}
    plan_path.write_text(json.dumps(plan, indent=4) + "\n")


def verify(phase, root, engine, producer):
    actor, kind = PHASES[phase]
    expected_producer = {
        "guard256": "warm-result.json",
        "guard256_cold": "guard256-result.json",
        "guard305": "guard256-result.json",
        "guard305_cold": "guard305-result.json",
    }[phase]
    need(producer.name == expected_producer, "wrong phase producer")
    prior, history = source_save(producer)
    receipt = doc(root / f"{phase}-result.json")
    plan_path = root / f"{phase}-executed-plan.json"
    plan = doc(plan_path)
    final = doc(root / f"{phase}-state.json")
    events = [decode(line) for line in read(root / f"{phase}-trace.jsonl").splitlines()]
    steps = [event for event in events if "step" in event and "state" in event]
    count = 45 if phase == "guard256" else 20 if phase == "guard305" else 1
    need(
        receipt["result"] == "PASS"
        and receipt["receipt_version"] == 2
        and receipt["resumed_earned_checkpoint"] is True
        and receipt["initial_level_setup"] == receipt["final_level"] == 7
        and receipt["rendered"] is False
        and receipt["transitions"] == []
        and receipt["steps"] == count
        and len(steps) == count
        and [event["step"] for event in steps] == list(range(count))
        and [event["action"] for event in steps] == [x["action"] for x in plan["steps"]]
        and same(final, steps[-1]["state"]),
        "phase receipt or trace invalid",
    )
    need(
        all(
            set(event) == {"step", "action", "state", "simulation_fps"} and same(event["simulation_fps"], 60)
            for event in steps
        ),
        "trace state-row schema or frame rate invalid",
    )
    need(
        type(receipt["position_assist_events"]) is int
        and same(receipt["position_assist_events"], 0 if kind == "cold" else 1)
        and type(receipt["fixed_view_aim_events"]) is int
        and (receipt["fixed_view_aim_events"] == 0 if kind == "cold" else receipt["fixed_view_aim_events"] > 0),
        "assist or aim receipt count invalid",
    )
    need(
        plan["resume_checkpoint"] == history["file"]
        and receipt["resume_sha256"] == plan["resume_sha256"] == history["sha256"]
        and receipt["plan_sha256"] == sha(plan_path)
        and receipt["executable_sha256"] == sha(engine)
        and re.fullmatch(r"[0-9a-f]{32}", receipt["run_id"])
        and receipt["run_id"] == plan["receipt_run_id"]
        and receipt["run_id"] != prior["run_id"]
        and receipt["process_id"] != prior["process_id"],
        "phase input or fresh process invalid",
    )
    expected = template(actor, kind)
    for key in ("resume_checkpoint", "resume_sha256", "receipt_run_id"):
        expected[key] = plan[key]
    if kind == "warm":
        # All mutable expectations must derive solely from the actual producer.
        bound = root / "binding-check-plan.json"
        bound.write_text(json.dumps(expected))
        bind(actor, bound, producer)
        expected = doc(bound)
        bound.unlink()
    need(same(plan, expected), "executed plan differs from bound template")
    allowed = {"position_assist", "fixed_view_aim", "weapon_wall_contacts"} if kind == "warm" else set()
    need(
        all(event.get("event") in allowed for event in events if "state" not in event),
        "unexpected non-state trace event",
    )
    need(
        receipt["position_assist_events"] == sum(event.get("event") == "position_assist" for event in events),
        "position assist receipt mismatch",
    )
    kills = history["state"]["player"]["robots_killed_level"]
    initial = steps[0]["state"]
    earned = restored(history["state"])
    earned["combat_observation"]["tracked_objects"] = (
        initial["combat_observation"]["tracked_objects"] if kind == "warm" else []
    )
    need(same(initial, earned), "not exact earned resume")
    if kind == "cold":
        durable = restored(history["state"])
        durable["combat_observation"]["tracked_objects"] = []
        need(
            events == steps
            and receipt["checkpoint_history"] == []
            and receipt["checkpoints"] == {}
            and same(final, durable),
            "cold durable state or zero-frame invalid",
        )
        ordinary(final, kills)
        return
    ordinary(initial, kills)
    for event in steps:
        state = event["state"]
        ordinary(state, state["player"]["robots_killed_level"])
        need(state["player"]["robots_killed_level"] in (kills, kills + 1), "unplanned kill-credit drift")
    first = actor_binding(initial, actor, True)
    need(
        first["damage_since_binding_raw"] == 0
        and first["binding_shields_raw"] == first["actor_state"]["shields_raw"] > 0,
        "actor initial health unbound",
    )
    segment = ACTORS[actor][2]
    need(
        steps[7]["state"]["position"]["segment"] == segment
        and all(x["state"]["position"]["segment"] == segment for x in steps[7:]),
        "assisted-local position changed",
    )
    assists = [event for event in events if event.get("event") == "position_assist"]
    need(
        len(assists) == 1 and assists[0]["step"] == 6 and assists[0]["action"] == "pose_view",
        "local pose assist missing",
    )
    aims = [event for event in events if event.get("event") == "fixed_view_aim"]
    need(
        len(aims) >= (8 if actor == 256 else 2) and receipt["fixed_view_aim_events"] == len(aims),
        "bounded view aim missing",
    )
    owner = initial["combat_observation"]
    owned = [weapon for event in steps for weapon in event["state"]["combat_observation"]["owned_weapons"]]
    need(
        owned
        and all(
            weapon["weapon_id"] == 0
            and weapon["parent_object"] == owner["player_object"]
            and weapon["parent_signature"] == owner["player_signature"]
            for weapon in owned
        ),
        "no owned ordinary Quad Laser shots",
    )
    negative = [
        x
        for x in steps
        if x["state"]["combat_observation"]["tracked_objects"][0].get("actor_state")
        and x["state"]["combat_observation"]["tracked_objects"][0]["actor_state"]["shields_raw"] < 0
    ]
    need(negative, "no same-bound negative health witness")
    witness = negative[0]["state"]
    victim = actor_binding(witness, actor, True)
    need(
        victim["damage_since_binding_raw"] >= first["binding_shields_raw"] + 1
        and witness["player"]["robots_killed_level"] == kills + 1,
        "same-bound damage or kill credit missing",
    )
    need(
        witness["game_time_raw"] > initial["game_time_raw"]
        and witness["game_time_raw"] - initial["game_time_raw"] <= (140 if actor == 256 else 110) * 65536 // 60,
        "kill outside bounded ordinary frames",
    )
    ordinary(final, kills + 1)
    need(
        final["combat_observation"]["owned_weapon_count"] == 0
        and final["game_time_raw"] - witness["game_time_raw"] >= 45 * 65536 // 60 - 2,
        "neutral tail or bolt settlement missing",
    )
    history_out = receipt["checkpoint_history"]
    saved_step = ACTORS[actor][3]
    need(
        len(history_out) == 1
        and history_out[0]["step"] == saved_step
        and re.fullmatch(rf"pumo-player-level7-[0-9]+-step{saved_step}\.sav", history_out[0]["file"])
        and history_out[0]["sha256"] == sha(save(root, history_out[0]["file"]))
        and same(history_out[0]["state"], final),
        "settled versioned save invalid",
    )


def main():
    if len(sys.argv) == 9 and sys.argv[1] == "source":
        source(*(Path(x) for x in sys.argv[2:]))
    elif len(sys.argv) == 5 and sys.argv[1] == "bind":
        bind(int(sys.argv[2]), Path(sys.argv[3]), Path(sys.argv[4]))
    elif len(sys.argv) == 6 and sys.argv[1] == "verify":
        verify(sys.argv[2], Path(sys.argv[3]), Path(sys.argv[4]), Path(sys.argv[5]))
    else:
        raise ValueError("invalid courtyard contract arguments")


if __name__ == "__main__":
    main()
