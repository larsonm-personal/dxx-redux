"""Source-only earned ordinary paired blast-wall and settled-save owner."""

import json
import re
import sys
from pathlib import Path

import pumo_tulev1_guided_switch12_contract as guided

need = guided.need
same = guided.same
integer = guided.integer
doc = guided.doc
sha = guided.sha
save = guided.save
read = guided.read
decode = guided.decode
restored = guided.restored


def template(phase):
    return doc(Path(__file__).with_name(f"pumo_tulev1_blast136_{phase}.json"))


def source(path, engine, guard, quad, ordinary, blue):
    need(path.name == "warm-result.json", "not a Guided switch12 producer")
    return guided.warm(path.parent, engine, guard, quad, ordinary, blue)


def bind(plan_path, producer_path):
    plan = doc(plan_path)
    producer = doc(producer_path)
    need(len(producer["checkpoint_history"]) == 1, "missing Guided producer save")
    state = producer["checkpoint_history"][0]["state"]
    expected = template("warm")
    for field in ("resume_checkpoint", "resume_sha256"):
        expected[field] = plan[field]
    need(same(plan, expected), "unbound blast template altered")
    for field, value in (
        ("game_time_raw", state["game_time_raw"]),
        ("player.shields", state["player"]["shields"]),
        ("player.energy", state["player"]["energy"]),
    ):
        plan["checkpoint_expect"][field] = value
        plan["steps"][0]["expect"][field] = value
    plan_path.write_text(json.dumps(plan, indent=4) + "\n")


def bundle(phase, root, engine, source_state=None):
    receipt = doc(root / f"{phase}-result.json")
    plan_path = root / f"{phase}-executed-plan.json"
    plan = doc(plan_path)
    final = doc(root / f"{phase}-state.json")
    events = [decode(line) for line in read(root / f"{phase}-trace.jsonl").splitlines()]
    steps = [event for event in events if "step" in event and "state" in event]
    expected = template(phase)
    for field in ("resume_checkpoint", "resume_sha256", "receipt_run_id"):
        expected[field] = plan[field]
    if phase == "warm":
        need(source_state is not None, "missing actual source state")
        for field, value in (
            ("game_time_raw", source_state["game_time_raw"]),
            ("player.shields", source_state["player"]["shields"]),
            ("player.energy", source_state["player"]["energy"]),
        ):
            expected["checkpoint_expect"][field] = value
            expected["steps"][0]["expect"][field] = value
    need(same(plan, expected), "executed plan differs from bound template")
    count = 16 if phase == "warm" else 1
    for field, value in (
        ("receipt_version", 2),
        ("steps", count),
        ("initial_level_setup", 7),
        ("final_level", 7),
        ("position_assist_events", 1 if phase == "warm" else 0),
        ("fixed_view_aim_events", 0),
    ):
        need(same(receipt[field], value), f"invalid {field}")
    need(
        receipt["result"] == "PASS"
        and receipt["resumed_earned_checkpoint"] is True
        and receipt["transitions"] == []
        and receipt["rendered"] is False
        and integer(receipt["process_id"])
        and receipt["process_id"] > 0,
        "native phase did not pass",
    )
    need(
        re.fullmatch(r"[0-9a-f]{32}", receipt["run_id"])
        and receipt["run_id"] == plan["receipt_run_id"]
        and receipt["plan_sha256"] == sha(plan_path)
        and receipt["executable_sha256"] == sha(engine) == guided.quad.ENGINE_SHA
        and receipt["resume_sha256"] == plan["resume_sha256"] == sha(save(root, plan["resume_checkpoint"])),
        "phase input or receipt pin invalid",
    )
    need(
        same([event["step"] for event in steps], list(range(count)))
        and same([event["action"] for event in steps], [step["action"] for step in plan["steps"]])
        and same(final, steps[-1]["state"]),
        "trace or final state inconsistent",
    )
    allowed = {"position_assist", "weapon_wall_contacts"} if phase == "warm" else set()
    need(
        all(
            (set(event) == {"step", "action", "state", "simulation_fps"} and same(event["simulation_fps"], 60))
            if "step" in event and "state" in event
            else event.get("event") in allowed
            for event in events
        ),
        "unplanned trace event",
    )
    assists = [event for event in events if event.get("event") == "position_assist"]
    if phase == "warm":
        assisted_local_pose(assists, steps)
    else:
        need(not assists, "unexpected cold position assist")
    return receipt, plan, final, events, steps


def assisted_local_pose(assists, steps):
    need(
        len(assists) == 1
        and same(assists[0]["step"], 6)
        and assists[0]["action"] == "face_view"
        and same(assists[0]["position"], steps[7]["state"]["position"]),
        "local face_view assist mismatch",
    )


def paired(state, flags):
    pilot = state["xl_pilot"]
    need(
        all(
            same(pilot["doorways"][str(i)]["type"], 1)
            and same(pilot["doorways"][str(i)]["clip"], 6)
            and same(pilot["doorways"][str(i)]["flags"], flags)
            for i in (136, 137)
        )
        and all(
            same(pilot["doorways"][str(i)]["type"], 4) and same(pilot["doorways"][str(i)]["state"], 0)
            for i in (201, 202)
        )
        and pilot["wall_triggers"]["12"]["operated"] is True
        and same(pilot["wall_triggers"]["12"]["shot"], 1),
        "blast pair or prior switch12 state invalid",
    )


def ordinary(state):
    player = state["player"]
    need(
        state["player_dead"] is False
        and state["cheats_enabled"] is False
        and player["invulnerable"] is False
        and player["quad_lasers"] is True
        and same(player["primary_weapon"], 0)
        and same(player["laser_level"], 0)
        and player["has_blue_key"] is True
        and player["has_red_key"] is False
        and player["has_gold_key"] is False
        and same(player["secondary_ammo"], [7, 0, 0, 0, 0, 0, 0, 0, 0, 0]),
        "not ordinary post-Guided Quad Laser actor",
    )


def actual_contacts(result):
    need(
        result["kind"] == "actual_contacts_reaching_effect_blowup_gate"
        and integer(result["contact_count_lower_bound"])
        and result["contact_count_lower_bound"] >= 1
        and type(result["contacts"]) is list
        and 1 <= len(result["contacts"]) <= result["contact_count_lower_bound"]
        and integer(result["contact_limit"])
        and result["contact_limit"] >= len(result["contacts"])
        and type(result["truncated"]) is bool
        and (
            result["contact_count_lower_bound"] > len(result["contacts"])
            if result["truncated"]
            else result["contact_count_lower_bound"] == len(result["contacts"])
        ),
        "actual contact report invalid",
    )


def warm(root, engine, producer, guard, quad, ordinary_path, blue):
    previous, saved = source(producer, engine, guard, quad, ordinary_path, blue)
    receipt, plan, final, events, steps = bundle("warm", root, engine, saved["state"])
    need(
        plan["resume_checkpoint"] == saved["file"]
        and receipt["resume_sha256"] == saved["sha256"]
        and receipt["run_id"] != previous["run_id"]
        and receipt["process_id"] != previous["process_id"],
        "not fresh actual Guided switch12 continuation",
    )
    expected = restored(saved["state"])
    expected["combat_observation"]["tracked_objects"] = steps[0]["state"]["combat_observation"]["tracked_objects"]
    need(same(steps[0]["state"], expected), "source save did not cold-load")
    for index in (0, 7, 8, 10, 11, 13, 14, 15):
        ordinary(steps[index]["state"])
    paired(steps[0]["state"], 16)
    paired(steps[7]["state"], 16)
    paired(steps[11]["state"], 17)
    paired(steps[13]["state"], 17)
    paired(final, 17)
    need(
        guided.number(final["player"]["energy"])
        and 0 < final["player"]["energy"] < saved["state"]["player"]["energy"]
        and guided.number(final["player"]["shields"])
        and 0 < final["player"]["shields"] <= saved["state"]["player"]["shields"],
        "ordinary fire or health continuity invalid",
    )
    need(
        all(
            state["xl_pilot"]["wall_triggers"]["14"]["operated"] is False
            and same(state["xl_pilot"]["operation_sequence"], steps[0]["state"]["xl_pilot"]["operation_sequence"])
            for state in (step["state"] for step in steps)
        ),
        "blast owner operated a later switch",
    )
    need(
        all(same(steps[i]["state"]["position"]["segment"], 1152) for i in range(7, 16))
        and same(steps[0]["state"]["position"]["segment"], 648),
        "blast proof has unauthorized crossing",
    )
    # Trace states are sampled before their indexed actions; step 11 follows wait_for firing.
    origin = steps[10]["state"]["game_time_raw"]
    effect = steps[11]["state"]["game_time_raw"]
    settled = steps[13]["state"]["game_time_raw"]
    need(
        integer(origin)
        and integer(effect)
        and integer(settled)
        and 0 < effect - origin <= 180 * 65536 // 60
        and 0 < settled - effect <= 60 * 65536 // 60
        and same(steps[13]["state"]["combat_observation"]["owned_weapon_count"], 0),
        "blast or bolt settlement outside bounded normal frames",
    )
    contacts = [event for event in events if event.get("event") == "weapon_wall_contacts"]
    need(len(contacts) == 1, "actual contact observation missing")
    result = contacts[0]["result"]
    actual_contacts(result)
    owner = steps[0]["state"]["combat_observation"]
    for hit in result["contacts"]:
        need(
            same(hit["weapon_id"], 0)
            and same(hit["parent_object"], owner["player_object"])
            and same(hit["parent_signature"], owner["player_signature"])
            and same(hit["level"], 7)
            and same(hit["wall"], 136)
            and same(hit["hit_segment"], 1151)
            and same(hit["hit_side"], 0),
            "sampled contact not player Quad Laser on wall136",
        )
    history = receipt["checkpoint_history"]
    need(
        len(history) == 1
        and same(history[0]["step"], 14)
        and re.fullmatch(r"pumo-player-level7-[0-9]+-step14\.sav", history[0]["file"])
        and history[0]["sha256"] == sha(save(root, history[0]["file"]))
        and same(history[0]["state"], steps[14]["state"])
        and receipt["checkpoints"]["7"]["sha256"] == history[0]["sha256"]
        and sha(save(root, receipt["checkpoints"]["7"]["file"])) == history[0]["sha256"],
        "not a settled versioned blast save",
    )
    need(same(steps[14]["state"]["combat_observation"]["owned_weapon_count"], 0), "save has live bolts")
    return receipt, history[0]


def cold(root, engine, warm_path, producer, guard, quad, ordinary_path, blue):
    previous, saved = warm(warm_path.parent, engine, producer, guard, quad, ordinary_path, blue)
    receipt, plan, final, events, _steps = bundle("cold", root, engine)
    need(
        plan["resume_checkpoint"] == saved["file"]
        and receipt["resume_sha256"] == saved["sha256"]
        and receipt["process_id"] != previous["process_id"]
        and receipt["run_id"] != previous["run_id"]
        and len(events) == 1
        and receipt["checkpoint_history"] == []
        and receipt["checkpoints"] == {},
        "cold was not fresh zero-frame no-save process",
    )
    for key in (
        "player",
        "position",
        "level_scene",
        "current_level_num",
        "player_dead",
        "cheats_enabled",
        "reactor_countdown",
    ):
        need(same(final[key], saved["state"][key]), f"cold persistent {key} differs")
    need(
        same(final["game_time_raw"], saved["state"]["game_time_raw"])
        and same(final["simulation_rng"], dict(saved["state"]["simulation_rng"], call_count=0)),
        "cold clock or RNG differs",
    )
    excluded = {"operation_counts", "operation_sequence", "trigger_activated"}
    need(
        same(
            {k: v for k, v in final["xl_pilot"].items() if k not in excluded},
            {k: v for k, v in saved["state"]["xl_pilot"].items() if k not in excluded},
        ),
        "cold wall/trigger persistence differs",
    )
    paired(final, 17)
    need(same(final["combat_observation"]["owned_weapon_count"], 0), "cold has owned bolts")


def main():
    if len(sys.argv) == 8 and sys.argv[1] == "source":
        source(*(Path(x) for x in sys.argv[2:]))
    elif len(sys.argv) == 4 and sys.argv[1] == "bind":
        bind(Path(sys.argv[2]), Path(sys.argv[3]))
    elif len(sys.argv) == 9 and sys.argv[1] == "warm":
        warm(*(Path(x) for x in sys.argv[2:]))
    elif len(sys.argv) == 10 and sys.argv[1] == "cold":
        cold(*(Path(x) for x in sys.argv[2:]))
    else:
        raise ValueError("invalid blast136 contract arguments")


if __name__ == "__main__":
    try:
        main()
    except (KeyError, TypeError, ValueError, IndexError) as error:
        raise ValueError(f"invalid blast136 evidence: {error}") from error
