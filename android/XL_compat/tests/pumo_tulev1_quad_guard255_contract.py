"""Asset-free assisted-local Quad pickup and ordinary guard255 combat contract."""

import json
import re
import sys
from pathlib import Path

import pumo_tulev1_guard269_contract as guard269

need = guard269.need
same = guard269.same
integer = guard269.integer
doc = guard269.doc
sha = guard269.sha
save = guard269.save
read = guard269.predecessor.read
decode = guard269.predecessor.decode
restored = guard269.predecessor.restored
ENGINE_SHA = guard269.ENGINE_SHA


def template(phase):
    return doc(Path(__file__).with_name(f"pumo_tulev1_quad_guard255_{phase}.json"))


def bind(plan_path, producer_path):
    """Bind fixed relative view windows to the actual Quad producer clock."""
    plan = doc(plan_path)
    producer = doc(producer_path)
    need(
        producer_path.name == "quad-result.json"
        and len(producer["checkpoint_history"]) == 1
        and same(producer["checkpoint_history"][0]["step"], 7),
        "not a unique actual Quad producer",
    )
    clock = producer["checkpoint_history"][0]["state"]["game_time_raw"]
    need(integer(clock), "Quad source clock invalid")
    expected = template("guard255")
    expected["resume_checkpoint"] = plan["resume_checkpoint"]
    expected["resume_sha256"] = plan["resume_sha256"]
    need(same(plan, expected), "unbound guard plan was altered")
    for ordinal in range(8):
        plan["steps"][9 + 2 * ordinal]["expect"]["game_time_raw"]["gte"] = clock + (1 + 15 * ordinal) * 65536 // 60
    plan_path.write_text(json.dumps(plan, indent=4) + "\n")


def source(path, engine, blue):
    need(path.name == "ordinary-result.json", "not guard269 ordinary producer")
    receipt, saved = guard269.ordinary(path.parent, engine, blue)
    return receipt, saved


def bundle(phase, root, engine):
    receipt = doc(root / f"{phase}-result.json")
    plan_path = root / f"{phase}-executed-plan.json"
    plan = doc(plan_path)
    final = doc(root / f"{phase}-state.json")
    events = [decode(line) for line in read(root / f"{phase}-trace.jsonl").splitlines()]
    steps = [event for event in events if "step" in event and "state" in event]
    count = {"quad": 9, "guard255": 45, "cold": 1}[phase]
    expected = template(phase)
    for field in ("resume_checkpoint", "resume_sha256", "receipt_run_id"):
        expected[field] = plan[field]
    if phase == "guard255":
        for ordinal in range(8):
            step = 9 + 2 * ordinal
            clock = plan["steps"][step]["expect"]["game_time_raw"]["gte"]
            need(type(clock) is int, "bound view clock is not an integer")
            expected["steps"][step]["expect"]["game_time_raw"]["gte"] = clock
    need(same(plan, expected), "executed plan differs from public template")
    for field, value in (
        ("receipt_version", 2),
        ("steps", count),
        ("initial_level_setup", 7),
        ("final_level", 7),
        ("position_assist_events", 0 if phase == "cold" else 1),
        ("fixed_view_aim_events", 36 if phase == "guard255" else 0),
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
        type(receipt["run_id"]) is str
        and re.fullmatch(r"[0-9a-f]{32}", receipt["run_id"])
        and receipt["run_id"] == plan["receipt_run_id"]
        and receipt["plan_sha256"] == sha(plan_path)
        and receipt["executable_sha256"] == sha(engine) == ENGINE_SHA
        and receipt["resume_sha256"] == plan["resume_sha256"] == sha(save(root, plan["resume_checkpoint"])),
        "receipt or pinned executable identity mismatch",
    )
    need(
        same([event["step"] for event in steps], list(range(count)))
        and same(
            [event["action"] for event in steps],
            [step["action"] for step in plan["steps"]],
        )
        and same(final, steps[-1]["state"]),
        "trace or final state inconsistent",
    )
    allowed = {
        "quad": {"position_assist"},
        "guard255": {"position_assist", "fixed_view_aim", "weapon_wall_contacts"},
        "cold": set(),
    }[phase]
    need(
        all(
            set(event) == {"step", "action", "state", "simulation_fps"} and same(event["simulation_fps"], 60)
            if "step" in event and "state" in event
            else event.get("event") in allowed
            for event in events
        ),
        "unplanned trace event",
    )
    assists = [event for event in events if event.get("event") == "position_assist"]
    assist_step = {"quad": 1, "guard255": 6}.get(phase)
    need(
        (
            not assists
            if phase == "cold"
            else len(assists) == 1
            and assists[0]["step"] == assist_step
            and assists[0]["action"] == "pose_view"
            and same(assists[0]["position"], steps[assist_step + 1]["state"]["position"])
        ),
        "assisted local pose differs",
    )
    return receipt, plan, final, events, steps


def actor(state, quad):
    player = state["player"]
    need(player["quad_lasers"] is quad, "Quad state differs")
    normalized = dict(state)
    normalized["player"] = dict(player, quad_lasers=False)
    guard269.actor(normalized)
    return player


def checkpoint(root, receipt, final, step):
    history = receipt["checkpoint_history"]
    need(
        type(history) is list and len(history) == 1,
        "one actual versioned save required",
    )
    saved = history[0]
    need(
        same(saved["step"], step)
        and type(saved["file"]) is str
        and re.fullmatch(rf"pumo-player-level7-[0-9]+-step{step}\.sav", saved["file"])
        and saved["sha256"] == sha(save(root, saved["file"]))
        and same(saved["state"], final),
        "versioned save invalid",
    )
    alias = receipt["checkpoints"]
    need(
        set(alias) == {"7"}
        and alias["7"]["file"] == "pumo-player-level7.sav"
        and alias["7"]["file"] != saved["file"]
        and alias["7"]["sha256"] == saved["sha256"] == sha(save(root, alias["7"]["file"]))
        and same(alias["7"]["state"], final),
        "mutable alias substituted for versioned save",
    )
    return saved


def pickup(root, engine, source_path, blue):
    previous, prior = source(source_path, engine, blue)
    receipt, plan, final, _events, steps = bundle("quad", root, engine)
    need(
        plan["resume_checkpoint"] == prior["file"]
        and receipt["resume_sha256"] == prior["sha256"]
        and receipt["process_id"] != previous["process_id"]
        and receipt["run_id"] != previous["run_id"],
        "Quad did not consume actual guard269 save in fresh process",
    )
    states = [event["state"] for event in steps]
    expected = restored(prior["state"])
    expected["combat_observation"]["tracked_objects"] = []
    need(same(states[0], expected), "guard269 save cold-load mismatch")
    before = actor(states[0], False)
    need(
        any(p == {"object": 61, "id": 12, "segment": 1156} for p in states[0]["level_scene"]["powerups"]),
        "Quad61 absent before local pickup",
    )
    need(
        states[2]["position"]["segment"] == 1156
        and states[2]["game_time_raw"] == states[0]["game_time_raw"]
        and actor(states[2], False),
        "pose did not precede ordinary pickup",
    )
    need(
        states[5]["game_time_raw"] > states[2]["game_time_raw"] and actor(states[5], True),
        "Quad not ordinarily acquired on normal tick",
    )
    for state in states[5:]:
        player = actor(state, True)
        need(
            player["robots_killed_level"] == before["robots_killed_level"]
            and player["shields"] <= before["shields"]
            and player["energy"] <= before["energy"]
            and not any(p["object"] == 61 for p in state["level_scene"]["powerups"])
            and state["combat_observation"]["owned_weapon_count"] == 0,
            "pickup grant, resurrected item or combat side effect",
        )
    need(
        all(
            same(
                state["xl_pilot"]["wall_triggers"],
                states[0]["xl_pilot"]["wall_triggers"],
            )
            and same(
                state["xl_pilot"]["remaining_objectives"],
                states[0]["xl_pilot"]["remaining_objectives"],
            )
            and same(
                state["xl_pilot"]["operation_sequence"],
                states[0]["xl_pilot"]["operation_sequence"],
            )
            and same(
                state["xl_pilot"]["trigger_activated"],
                states[0]["xl_pilot"]["trigger_activated"],
            )
            for state in states
        ),
        "pickup operated objective",
    )
    saved = checkpoint(root, receipt, final, 7)
    need(
        saved["sha256"] != prior["sha256"] and saved["file"] != prior["file"],
        "Quad save not distinct",
    )
    return receipt, saved


def tracked(state):
    values = state["combat_observation"]["tracked_objects"]
    need(type(values) is list and len(values) == 1, "guard255 tracker missing")
    item = values[0]
    need(
        same(item["object"], 255)
        and same(item["expected_id"], 28)
        and same(item["expected_type"], 2)
        and same(item["expected_signature"], 256)
        and same(item["expected_level"], 7)
        and same(item["observed_level"], 7)
        and integer(item["expected_load_generation"])
        and item["expected_load_generation"] == item["observed_load_generation"]
        and same(item["binding_shields_raw"], 200 * 65536)
        and integer(item["damage_since_binding_raw"])
        and type(item["exists"]) is bool,
        "guard255 binding invalid",
    )
    if item["exists"]:
        need(
            same(item["current_slot"], {"id": 28, "signature": 256, "type": 2})
            and type(item["actor_state"]["shields_raw"]) is int
            and item["damage_since_binding_raw"] == item["binding_shields_raw"] - item["actor_state"]["shields_raw"],
            "guard255 slot or damage inconsistent",
        )
    return item


def damage_witness(states):
    witnesses = [tracked(state) for state in states]
    bound = witnesses[0]
    need(
        bound["exists"]
        and bound["damage_since_binding_raw"] == 0
        and bound["actor_state"]["shields_raw"] == bound["binding_shields_raw"],
        "guard255 not alive at binding",
    )
    need(
        all(
            w["expected_load_generation"] == bound["expected_load_generation"]
            and w["binding_shields_raw"] == bound["binding_shields_raw"]
            for w in witnesses
        ),
        "guard255 tracker rebound",
    )
    need(
        any(
            w["exists"]
            and w["damage_since_binding_raw"] > bound["binding_shields_raw"]
            and w["actor_state"]["shields_raw"] < 0
            for w in witnesses
        ),
        "no immutable negative-shield guard255 damage witness",
    )
    return witnesses


def aim_witness(events, first, generation):
    faces = [event for event in events if event.get("event") == "fixed_view_aim"]
    need(len(faces) == 36, "fixed view assist count differs")
    for ordinal, event in enumerate(faces):
        frame = 0 if ordinal == 0 else 1 + 3 * (ordinal - 1)
        target = event["target_state"]
        need(
            set(event) == {"event", "forward_fixed", "game_time_raw", "target", "target_state"}
            and same(event["target"], 255)
            and same(event["game_time_raw"], first + frame * 65536 // 60)
            and same(target["object"], 255)
            and same(target["expected_id"], 28)
            and same(target["expected_type"], 2)
            and same(target["expected_signature"], 256)
            and same(target["expected_level"], 7)
            and same(target["observed_level"], 7)
            and same(target["expected_load_generation"], generation)
            and same(target["observed_load_generation"], generation)
            and target["exists"] is True
            and same(target["current_slot"], {"id": 28, "signature": 256, "type": 2})
            and type(target["actor_state"]["shields_raw"]) is int
            and target["actor_state"]["shields_raw"] > 0
            and target["actor_state"]["should_be_dead"] is False
            and integer(target["binding_shields_raw"])
            and 0 < target["binding_shields_raw"] <= 200 * 65536
            and (ordinal != 0 or target["binding_shields_raw"] == 200 * 65536)
            and integer(target["damage_since_binding_raw"])
            and target["damage_since_binding_raw"]
            == target["binding_shields_raw"] - target["actor_state"]["shields_raw"],
            "fixed view assist target or cadence forged",
        )


def final_nonliving(item):
    need(
        not item["exists"]
        or (item["damage_since_binding_raw"] > item["binding_shields_raw"] and item["actor_state"]["shields_raw"] < 0),
        "final guard255 is still living",
    )


def combat(root, engine, source_path, guard_path, blue):
    previous, prior = pickup(source_path.parent, engine, guard_path, blue)
    receipt, plan, final, events, steps = bundle("guard255", root, engine)
    need(
        plan["resume_checkpoint"] == prior["file"]
        and receipt["resume_sha256"] == prior["sha256"]
        and receipt["process_id"] != previous["process_id"]
        and receipt["run_id"] != previous["run_id"],
        "guard255 did not consume actual Quad save in fresh process",
    )
    states = [event["state"] for event in steps]
    expected = restored(prior["state"])
    expected["combat_observation"]["tracked_objects"] = states[0]["combat_observation"]["tracked_objects"]
    need(same(states[0], expected), "Quad save cold-load mismatch")
    baseline = actor(states[0], True)
    damage_witness(states)
    aim_witness(
        events,
        states[0]["game_time_raw"],
        tracked(states[0])["expected_load_generation"],
    )
    first = states[0]["game_time_raw"]
    need(
        all(
            plan["steps"][9 + 2 * ordinal]["expect"]["game_time_raw"]["gte"] == first + (1 + 15 * ordinal) * 65536 // 60
            for ordinal in range(8)
        ),
        "bound view windows differ from source-relative cadence",
    )
    identities = set()
    for index, state in enumerate(states):
        player = actor(state, True)
        need(
            state["game_time_raw"] >= first
            and player["shields"] > 0
            and player["shields"] <= baseline["shields"]
            and player["energy"] <= baseline["energy"]
            and player["robots_killed_level"] >= baseline["robots_killed_level"]
            and state["position"]["segment"] == (1156 if index < 7 else 647)
            and same(
                state["xl_pilot"]["wall_triggers"],
                states[0]["xl_pilot"]["wall_triggers"],
            )
            and same(
                state["xl_pilot"]["remaining_objectives"],
                states[0]["xl_pilot"]["remaining_objectives"],
            )
            and same(
                state["xl_pilot"]["operation_sequence"],
                states[0]["xl_pilot"]["operation_sequence"],
            )
            and same(
                state["xl_pilot"]["trigger_activated"],
                states[0]["xl_pilot"]["trigger_activated"],
            ),
            "combat state or objective invalid",
        )
        observation = state["combat_observation"]
        need(
            observation["owned_weapons_truncated"] is False
            and observation["owned_weapon_count"] == len(observation["owned_weapons"]),
            "owned shot observer incomplete",
        )
        for weapon in observation["owned_weapons"]:
            need(
                weapon["weapon_id"] == 0
                and weapon["parent_type"] == 4
                and weapon["parent_object"] == observation["player_object"]
                and weapon["parent_signature"] == observation["player_signature"],
                "shot not player-owned Laser",
            )
    for ordinal in range(8):
        state = states[10 + ordinal * 2]
        need(
            state["game_time_raw"] == first + (1 + 15 * ordinal) * 65536 // 60,
            "Quad burst cadence changed",
        )
        new = set()
        for weapon in state["combat_observation"]["owned_weapons"]:
            identity = weapon["object"], weapon["signature"]
            if weapon["lifeleft_raw"] > 0 and identity not in identities:
                new.add(identity)
        need(len(new) == 4, "ordinary four-bolt Quad burst missing")
        identities.update(new)
    need(len(identities) == 32, "eight independent Quad bursts not witnessed")
    need(
        all(
            states[27 + 2 * ordinal]["game_time_raw"] == first + (111 + 5 * ordinal) * 65536 // 60
            for ordinal in range(9)
        ),
        "nine neutral tail snapshots did not advance five frames each",
    )
    final_nonliving(tracked(final))
    need(
        final["game_time_raw"] == first + 151 * 65536 // 60
        and final["player"]["robots_killed_level"] == baseline["robots_killed_level"] + 1
        and final["primary_fire"]["held"] == 0
        and final["secondary_fire"]["held"] == 0
        and final["combat_observation"]["owned_weapon_count"] == 0,
        "guard255 kill credit or settled save missing",
    )
    contacts = [event["result"] for event in events if event.get("event") == "weapon_wall_contacts"]
    need(
        len(contacts) == 1
        and contacts[0]["kind"] == "actual_contacts_reaching_effect_blowup_gate"
        and integer(contacts[0]["contact_count_lower_bound"])
        and len(contacts[0]["contacts"]) <= contacts[0]["contact_count_lower_bound"]
        and type(contacts[0]["truncated"]) is bool
        and (
            contacts[0]["contact_count_lower_bound"] > len(contacts[0]["contacts"])
            if contacts[0]["truncated"]
            else contacts[0]["contact_count_lower_bound"] == len(contacts[0]["contacts"])
        ),
        "contact witness malformed or truncation concealed",
    )
    saved = checkpoint(root, receipt, final, 44)
    need(
        saved["sha256"] != prior["sha256"] and saved["file"] != prior["file"],
        "guard255 save not distinct",
    )
    return receipt, saved


def cold(root, engine, source_path, quad_path, guard_path, blue):
    previous, prior = combat(source_path.parent, engine, quad_path, guard_path, blue)
    receipt, plan, final, events, _steps = bundle("cold", root, engine)
    need(
        plan["resume_checkpoint"] == prior["file"]
        and receipt["resume_sha256"] == prior["sha256"]
        and receipt["process_id"] != previous["process_id"]
        and receipt["run_id"] != previous["run_id"]
        and len(events) == 1
        and receipt["checkpoint_history"] == []
        and receipt["checkpoints"] == {},
        "cold restore is not a fresh no-frame no-save process",
    )
    expected = restored(prior["state"])
    expected["combat_observation"]["tracked_objects"] = []
    need(same(final, expected), "cold gameplay state differs beyond process-local resets")
    actor(final, True)


def main():
    if len(sys.argv) == 4 and sys.argv[1] == "bind":
        bind(Path(sys.argv[2]), Path(sys.argv[3]))
    elif len(sys.argv) == 5 and sys.argv[1] == "source":
        source(Path(sys.argv[2]), Path(sys.argv[3]), Path(sys.argv[4]))
    elif len(sys.argv) == 7 and sys.argv[1] == "quad":
        pickup(Path(sys.argv[2]), Path(sys.argv[3]), Path(sys.argv[5]), Path(sys.argv[6]))
    elif len(sys.argv) == 7 and sys.argv[1] == "guard255":
        combat(
            Path(sys.argv[2]),
            Path(sys.argv[3]),
            Path(sys.argv[4]),
            Path(sys.argv[5]),
            Path(sys.argv[6]),
        )
    elif len(sys.argv) == 8 and sys.argv[1] == "cold":
        cold(
            Path(sys.argv[2]),
            Path(sys.argv[3]),
            Path(sys.argv[4]),
            Path(sys.argv[5]),
            Path(sys.argv[6]),
            Path(sys.argv[7]),
        )
    else:
        raise ValueError("invalid Quad/guard255 contract arguments")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, IndexError, TypeError, OSError) as error:
        sys.exit(str(error))
