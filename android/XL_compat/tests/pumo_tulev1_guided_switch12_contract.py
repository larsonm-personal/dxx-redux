"""Source-only earned Guided switch12 flight and settled-save owner."""

import json
import re
import sys
from itertools import pairwise
from pathlib import Path

import pumo_tulev1_quad_guard255_contract as quad

need = quad.need
same = quad.same
integer = quad.integer
number = quad.guard269.predecessor.number
doc = quad.doc
sha = quad.sha
save = quad.save
read = quad.read
decode = quad.decode
restored = quad.restored


def template(phase):
    return doc(Path(__file__).with_name(f"pumo_tulev1_guided_switch12_{phase}.json"))


def source(path, engine, quad_path, guard_path, blue_path):
    need(path.name == "guard255-result.json", "not the guard255 producer")
    return quad.combat(path.parent, engine, quad_path, guard_path, blue_path)


def bind(plan_path, source_path):
    plan = doc(plan_path)
    producer = doc(source_path)
    history = producer["checkpoint_history"]
    need(
        source_path.name == "guard255-result.json" and len(history) == 1 and same(history[0]["step"], 44),
        "not an actual versioned guard255 producer",
    )
    state = history[0]["state"]
    expected = template("warm")
    expected["resume_checkpoint"] = plan["resume_checkpoint"]
    expected["resume_sha256"] = plan["resume_sha256"]
    need(same(plan, expected), "unbound warm template altered")
    # The actual producer owns these values; no private observed constants enter source.
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
    # The full 820-frame trace exceeds the shared 16 MiB default; keep an explicit cap.
    limit = 128 << 20 if phase == "warm" else 16 << 20
    events = [decode(line) for line in read(root / f"{phase}-trace.jsonl", limit).splitlines()]
    steps = [event for event in events if "step" in event and "state" in event]
    expected = template(phase)
    for key in ("resume_checkpoint", "resume_sha256", "receipt_run_id"):
        expected[key] = plan[key]
    if phase == "warm":
        need(source_state is not None, "missing source state")
        for field, value in (
            ("game_time_raw", source_state["game_time_raw"]),
            ("player.shields", source_state["player"]["shields"]),
            ("player.energy", source_state["player"]["energy"]),
        ):
            expected["checkpoint_expect"][field] = value
            expected["steps"][0]["expect"][field] = value
    need(same(plan, expected), "executed plan differs from bound public template")
    count = {"warm": 1861, "cold": 1}[phase]
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
        "native process did not pass",
    )
    need(
        re.fullmatch(r"[0-9a-f]{32}", receipt["run_id"])
        and receipt["run_id"] == plan["receipt_run_id"]
        and receipt["plan_sha256"] == sha(plan_path)
        and receipt["executable_sha256"] == sha(engine) == quad.ENGINE_SHA
        and receipt["resume_sha256"] == plan["resume_sha256"] == sha(save(root, plan["resume_checkpoint"])),
        "receipt or input pin invalid",
    )
    need(
        same([event["step"] for event in steps], list(range(count)))
        and same(
            [event["action"] for event in steps],
            [step["action"] for step in plan["steps"]],
        )
        and same(final, steps[-1]["state"]),
        "step trace/final state mismatch",
    )
    allowed = {
        "warm": {
            "position_assist",
            "successful_wall_operation",
            "trigger_change",
            "weapon_wall_contacts",
        },
        "cold": set(),
    }[phase]
    need(
        all(
            (set(event) == {"step", "action", "state", "simulation_fps"} and same(event["simulation_fps"], 60))
            if "step" in event and "state" in event
            else event.get("event") in allowed
            for event in events
        ),
        "unexpected trace event",
    )
    assists = [event for event in events if event.get("event") == "position_assist"]
    need(
        (
            len(assists) == 1
            and same(assists[0]["step"], 10)
            and assists[0]["action"] == "pose_view"
            and same(assists[0]["position"], steps[11]["state"]["position"])
        )
        if phase == "warm"
        else not assists,
        "position assist mismatch",
    )
    return receipt, plan, final, events, steps


def identity(state):
    observed = state["combat_observation"]
    guided = observed["guided_control"]
    need(
        type(guided) is dict
        and same(guided["weapon_id"], 37)
        and same(guided["parent_object"], observed["player_object"])
        and same(guided["parent_signature"], observed["player_signature"])
        and integer(guided["object"])
        and integer(guided["signature"])
        and integer(guided["lifeleft_raw"])
        and guided["lifeleft_raw"] > 0
        and same(observed["owned_weapon_count"], 1)
        and observed["owned_weapons_truncated"] is False,
        "not a live player-owned Guide",
    )
    owned = observed["owned_weapons"]
    need(
        len(owned) == 1
        and same(owned[0]["object"], guided["object"])
        and same(owned[0]["signature"], guided["signature"])
        and same(owned[0]["weapon_id"], 37)
        and same(owned[0]["parent_type"], 4)
        and same(owned[0]["parent_object"], guided["parent_object"])
        and same(owned[0]["parent_signature"], guided["parent_signature"]),
        "guided pointer does not bind owned missile",
    )
    return guided["object"], guided["signature"]


def flight(events, steps):
    launch = steps[15]["state"]
    first = identity(launch)
    hp = steps[12]["state"]["player"]["shields"]
    operation = next(event for event in events if event.get("event") == "successful_wall_operation")
    before = []
    for event in events:
        if event is operation:
            break
        if "state" in event and (("step" in event) or event.get("event") == "trigger_change"):
            before.append(event["state"])
    clocks = []
    launched = False
    for state in before:
        launched = launched or state is launch
        need(
            state["player_dead"] is False
            and state["cheats_enabled"] is False
            and state["player"]["invulnerable"] is False,
            "forbidden safety assist",
        )
        if launched:
            need(
                identity(state) == first and same(state["player"]["shields"], hp),
                "guided identity or HP continuity broken",
            )
            clocks.append(state["game_time_raw"])
    need(
        clocks and all(b >= a for a, b in pairwise(clocks)),
        "guided observations missing or out of time order",
    )
    # A reported gap is an observation limit, never a claim about hidden frames.
    boundary_clock = operation["state"]["game_time_raw"]
    need(
        integer(boundary_clock) and boundary_clock >= clocks[-1],
        "operation clock out of order",
    )
    max_gap = max([boundary_clock - clocks[-1], *(b - a for a, b in pairwise(clocks))])
    need(
        same(operation["index"], 12) and same(operation["shot"], 1) and same(operation["sequence"], 1),
        "wrong switch operation provenance",
    )
    return first, max_gap, hp


def relative_timing(source_state, steps, operation_state):
    origin = source_state["game_time_raw"]
    launch = steps[15]["state"]["game_time_raw"]
    operated = operation_state["game_time_raw"]
    end = steps[1858]["state"]["game_time_raw"]
    need(
        integer(origin)
        and integer(launch)
        and integer(operated)
        and integer(end)
        and 0 < launch - origin < operated - origin < end - origin
        and same(end - origin, 820 * 65536 // 60),
        "relative 820-frame timing mismatch",
    )


def target(events, final, first, owner):
    contacts = [event for event in events if event.get("event") == "weapon_wall_contacts"]
    operations = [event for event in events if event.get("event") == "successful_wall_operation"]
    changes = [event for event in events if event.get("event") == "trigger_change"]
    need(
        len(contacts) == len(operations) == len(changes) == 1,
        "missing or repeated target effects",
    )
    result = contacts[0]["result"]
    need(
        result["kind"] == "actual_contacts_reaching_effect_blowup_gate"
        and result["truncated"] is False
        and same(result["contact_count_lower_bound"], 1)
        and len(result["contacts"]) == 1,
        "contact evidence incomplete",
    )
    hit = result["contacts"][0]
    need(
        same((hit["object"], hit["signature"]), first)
        and same(hit["weapon_id"], 37)
        and same(hit["parent_object"], owner[0])
        and same(hit["parent_signature"], owner[1])
        and same(hit["level"], 7)
        and same(hit["wall"], 204)
        and same(hit["trigger"], 12)
        and same(hit["overlay_raw"], 414)
        and same(hit["eclip"], 72)
        and same(hit["hit_segment"], 1538)
        and same(hit["hit_side"], 2)
        and same(hit["blew_up"], 1),
        "wrong missile or switch12 contact",
    )
    state = operations[0]["state"]
    need(
        state["xl_pilot"]["wall_triggers"]["12"]["operated"] is True
        and same(state["xl_pilot"]["wall_triggers"]["12"]["shot"], 1)
        and same(state["xl_pilot"]["operation_counts"]["12"], 1)
        and same(state["xl_pilot"]["operation_sequence"], 1)
        and changes[0]["state"]["xl_pilot"]["trigger_activated"]["12"] is True,
        "wrong switch12 effect",
    )
    for snapshot in (final,):
        pilot = snapshot["xl_pilot"]
        need(
            all(
                same(pilot["doorways"][str(i)]["type"], 4) and same(pilot["doorways"][str(i)]["state"], 0)
                for i in (201, 202)
            ),
            "gates 201/202 not OPEN",
        )
    need(
        final["xl_pilot"]["wall_triggers"]["12"]["operated"] is True
        and same(final["xl_pilot"]["wall_triggers"]["12"]["shot"], 1)
        and same(final["xl_pilot"]["operation_counts"]["12"], 1)
        and same(final["xl_pilot"]["operation_sequence"], 1)
        and final["combat_observation"]["guided_control"] is None
        and same(final["combat_observation"]["owned_weapon_count"], 0)
        and number(final["player"]["shields"])
        and final["player"]["shields"] > 0
        and final["player"]["invulnerable"] is False,
        "settled final switch state invalid",
    )


def settled_observation(state):
    observed = state["combat_observation"]
    need(
        observed["guided_control"] is None
        and same(observed["owned_weapon_count"], 0)
        and observed["owned_weapons_truncated"] is False
        and same(observed["owned_weapons"], []),
        "Guided missile or pointer still in flight at save",
    )


def warm(root, engine, source_path, quad_path, guard_path, blue_path):
    previous, prior = source(source_path, engine, quad_path, guard_path, blue_path)
    receipt, plan, final, events, steps = bundle("warm", root, engine, prior["state"])
    need(
        plan["resume_checkpoint"] == prior["file"]
        and receipt["resume_sha256"] == prior["sha256"]
        and receipt["process_id"] != previous["process_id"]
        and receipt["run_id"] != previous["run_id"],
        "not fresh actual guard255 continuation",
    )
    expected = restored(prior["state"])
    expected["combat_observation"]["tracked_objects"] = steps[0]["state"]["combat_observation"]["tracked_objects"]
    need(same(steps[0]["state"], expected), "guard255 save cold-load mismatch")
    first, max_gap, _hp = flight(events, steps)
    operation = next(event for event in events if event.get("event") == "successful_wall_operation")
    relative_timing(prior["state"], steps, operation["state"])
    observed = steps[15]["state"]["combat_observation"]
    target(
        events,
        steps[1858]["state"],
        first,
        (observed["player_object"], observed["player_signature"]),
    )
    history = receipt["checkpoint_history"]
    settled_observation(steps[1858]["state"])
    need(
        len(history) == 1
        and same(history[0]["step"], 1859)
        and re.fullmatch(r"pumo-player-level7-[0-9]+-step1859\.sav", history[0]["file"])
        and history[0]["sha256"] == sha(save(root, history[0]["file"]))
        and same(history[0]["state"], steps[1859]["state"])
        and receipt["checkpoints"]["7"]["sha256"] == history[0]["sha256"]
        and sha(save(root, receipt["checkpoints"]["7"]["file"])) == history[0]["sha256"],
        "settled versioned save invalid",
    )
    settled_observation(history[0]["state"])
    settled_observation(final)
    print(f"Guided switch12 observed continuity max gap: {max_gap} raw ticks")
    return receipt, history[0]


def cold(root, engine, warm_path, source_path, quad_path, guard_path, blue_path):
    previous, saved = warm(warm_path.parent, engine, source_path, quad_path, guard_path, blue_path)
    receipt, plan, final, events, _steps = bundle("cold", root, engine)
    need(
        plan["resume_checkpoint"] == saved["file"]
        and receipt["resume_sha256"] == saved["sha256"]
        and receipt["process_id"] != previous["process_id"]
        and receipt["run_id"] != previous["run_id"]
        and len(events) == 1
        and receipt["checkpoint_history"] == []
        and receipt["checkpoints"] == {},
        "cold must be a fresh zero-frame no-save process",
    )
    cold_persistence(final, saved["state"])


def cold_persistence(final, source_state):
    for key in (
        "player",
        "position",
        "level_scene",
        "current_level_num",
        "player_dead",
        "cheats_enabled",
        "reactor_countdown",
    ):
        need(same(final[key], source_state[key]), f"cold persistent {key} differs")
    expected_rng = dict(source_state["simulation_rng"], call_count=0)
    need(
        same(final["game_time_raw"], source_state["game_time_raw"]) and same(final["simulation_rng"], expected_rng),
        "cold clock/RNG persistence differs",
    )
    excluded = {"operation_counts", "operation_sequence", "trigger_activated"}
    need(
        same(
            {k: v for k, v in final["xl_pilot"].items() if k not in excluded},
            {k: v for k, v in source_state["xl_pilot"].items() if k not in excluded},
        ),
        "full persistent xl_pilot differs",
    )
    need(
        final["xl_pilot"]["wall_triggers"]["12"]["operated"] is True
        and same(final["xl_pilot"]["wall_triggers"]["12"]["shot"], 1)
        and all(
            same(final["xl_pilot"]["doorways"][str(i)]["type"], 4)
            and same(final["xl_pilot"]["doorways"][str(i)]["state"], 0)
            for i in (201, 202)
        )
        and final["combat_observation"]["guided_control"] is None
        and same(final["combat_observation"]["owned_weapon_count"], 0),
        "cold switch effect or settled missile differs",
    )


def main():
    if len(sys.argv) == 4 and sys.argv[1] == "bind":
        bind(Path(sys.argv[2]), Path(sys.argv[3]))
    elif len(sys.argv) == 7 and sys.argv[1] == "source":
        source(*(Path(x) for x in sys.argv[2:]))
    elif len(sys.argv) == 8 and sys.argv[1] == "warm":
        warm(Path(sys.argv[2]), Path(sys.argv[3]), *(Path(x) for x in sys.argv[4:]))
    elif len(sys.argv) == 9 and sys.argv[1] == "cold":
        cold(Path(sys.argv[2]), Path(sys.argv[3]), *(Path(x) for x in sys.argv[4:]))
    else:
        raise ValueError("invalid guided switch12 contract arguments")


if __name__ == "__main__":
    try:
        main()
    except (KeyError, TypeError, ValueError, IndexError) as error:
        raise ValueError(f"invalid guided switch12 evidence: {error}") from error
