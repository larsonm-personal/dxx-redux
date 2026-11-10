"""Asset-free contract for an assisted-local, ordinary guard269 kill.

The predecessor owns its versioned blue-key save. This owner never claims
connected travel through the level. Native observations remain local artifacts.
"""

import copy
import re
import sys
from pathlib import Path

import pumo_tulev1_master5_contract as predecessor

need = predecessor.need
same = predecessor.same
integer = predecessor.integer
number = predecessor.number
doc = predecessor.doc
sha = predecessor.sha
save = predecessor.save
AMMO = predecessor.AMMO
ENGINE_SHA = predecessor.ENGINE_SHA


def template(phase):
    return doc(Path(__file__).with_name(f"pumo_tulev1_guard269_{phase}.json"))


def source(path, engine):
    need(path.name == "blue-result.json", "source is not the blue phase")
    cloak = path.parent.parent / "cloak" / "cloak-result.json"
    predecessor.phase("blue", path.parent, engine, cloak)
    receipt, saved = predecessor.producer(path, engine, "blue")
    return receipt, saved


def bundle(phase, root, engine):
    receipt = doc(root / f"{phase}-result.json")
    plan_path = root / f"{phase}-executed-plan.json"
    plan = doc(plan_path)
    final = doc(root / f"{phase}-state.json")
    events = [predecessor.decode(line) for line in predecessor.read(root / f"{phase}-trace.jsonl").splitlines()]
    steps = [event for event in events if "step" in event and "state" in event]
    count = 42 if phase == "ordinary" else 1
    expected = template(phase)
    for field in ("resume_checkpoint", "resume_sha256", "receipt_run_id"):
        expected[field] = plan[field]
    need(same(plan, expected), "executed plan differs from checked-in template")
    for field, value in (
        ("receipt_version", 2),
        ("steps", count),
        ("initial_level_setup", 7),
        ("final_level", 7),
        ("fixed_view_aim_events", 0),
        ("position_assist_events", 1 if phase == "ordinary" else 0),
    ):
        need(same(receipt[field], value), f"invalid receipt {field}")
    need(
        receipt["result"] == "PASS"
        and receipt["resumed_earned_checkpoint"] is True
        and receipt["transitions"] == []
        and receipt["rendered"] is False
        and integer(receipt["process_id"])
        and receipt["process_id"] > 0,
        "not a successful isolated native process",
    )
    need(
        type(receipt["run_id"]) is str
        and re.fullmatch(r"[0-9a-f]{32}", receipt["run_id"])
        and receipt["run_id"] == plan["receipt_run_id"]
        and receipt["plan_sha256"] == sha(plan_path)
        and receipt["executable_sha256"] == sha(engine) == ENGINE_SHA
        and receipt["resume_sha256"] == plan["resume_sha256"] == sha(save(root, plan["resume_checkpoint"])),
        "receipt or executable identity invalid",
    )
    need(
        same([event["step"] for event in steps], list(range(count)))
        and same([event["action"] for event in steps], [step["action"] for step in plan["steps"]])
        and same(final, steps[-1]["state"]),
        "native trace or final state mismatch",
    )
    allowed = {"position_assist", "weapon_wall_contacts"} if phase == "ordinary" else set()
    need(
        all(
            set(event) == {"step", "action", "state", "simulation_fps"} and same(event["simulation_fps"], 60)
            if "step" in event and "state" in event
            else event.get("event") in allowed
            for event in events
        ),
        "unexpected trace event",
    )
    return receipt, plan, final, events, steps


def actor(state):
    player = predecessor.live(state, cloak=True, blue=True)
    need(state["combat_observation"]["guided_control"] is None, "Guided control in flight")
    need(player["secondary_weapon_flags"] == 65 and same(player["secondary_ammo"], AMMO), "Guided inventory changed")
    return player


def tracked(state):
    values = state["combat_observation"]["tracked_objects"]
    need(type(values) is list and len(values) == 1, "guard tracker missing")
    item = values[0]
    need(
        same(item["object"], 269)
        and same(item["expected_id"], 28)
        and same(item["expected_type"], 2)
        and same(item["expected_signature"], 270)
        and same(item["expected_level"], state["current_level_num"])
        and same(item["observed_level"], state["current_level_num"])
        and integer(item["expected_load_generation"])
        and item["expected_load_generation"] == item["observed_load_generation"]
        and same(item["binding_shields_raw"], 200 * 65536)
        and integer(item["damage_since_binding_raw"])
        and type(item["exists"]) is bool,
        "guard tracker identity invalid",
    )
    if item["exists"]:
        need(
            same(item["current_slot"], {"id": 28, "signature": item["expected_signature"], "type": 2}),
            "tracked guard replaced",
        )
        need(
            type(item["actor_state"]["shields_raw"]) is int
            and item["damage_since_binding_raw"] == item["binding_shields_raw"] - item["actor_state"]["shields_raw"],
            "guard HP or bound damage inconsistent",
        )
    return item


def guard_killed(bound, last):
    need(
        last["exists"] is True
        and last["damage_since_binding_raw"] > bound["binding_shields_raw"]
        and last["actor_state"]["shields_raw"] < 0,
        "tracked guard did not actually die with negative shields",
    )


def checkpoint(root, receipt, final):
    history = receipt["checkpoint_history"]
    need(type(history) is list and len(history) == 1, "one versioned combat save required")
    saved = history[0]
    need(
        same(saved["step"], 41)
        and type(saved["file"]) is str
        and re.fullmatch(r"pumo-player-level7-[0-9]+-step41\.sav", saved["file"])
        and saved["sha256"] == sha(save(root, saved["file"]))
        and same(saved["state"], final),
        "combat save witness invalid",
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


def ordinary(root, engine, source_path):
    prior_receipt, prior = source(source_path, engine)
    receipt, plan, final, events, steps = bundle("ordinary", root, engine)
    need(
        plan["resume_checkpoint"] == prior["file"]
        and receipt["resume_sha256"] == prior["sha256"]
        and receipt["process_id"] != prior_receipt["process_id"]
        and receipt["run_id"] != prior_receipt["run_id"],
        "not fresh from actual blue save",
    )
    states = [step["state"] for step in steps]
    expected_loaded = predecessor.restored(prior["state"])
    expected_loaded["combat_observation"]["tracked_objects"] = states[0]["combat_observation"]["tracked_objects"]
    need(same(states[0], expected_loaded), "blue state not cold loaded exactly")
    initial = states[0]
    baseline = actor(initial)
    bound = tracked(initial)
    need(
        bound["exists"]
        and bound["damage_since_binding_raw"] == 0
        and bound["actor_state"]["shields_raw"] == bound["binding_shields_raw"]
        and integer(baseline["robots_killed_level"]),
        "guard not alive at binding",
    )
    need(
        same(states[8]["position"]["segment"], 647) and same(states[8]["combat_observation"]["owned_weapon_count"], 0),
        "unsafe local aim source",
    )
    assists = [event for event in events if event.get("event") == "position_assist"]
    need(
        len(assists) == 1
        and assists[0]["step"] == 7
        and assists[0]["action"] == "pose_view"
        and same(assists[0]["position"], states[8]["position"]),
        "local pose not declared",
    )
    identities = set()
    from itertools import pairwise

    for before, after in pairwise(states):
        need(
            after["game_time_raw"] >= before["game_time_raw"]
            and after["player"]["shields"] <= before["player"]["shields"]
            and after["player"]["energy"] <= before["player"]["energy"]
            and after["player"]["robots_killed_level"] >= before["player"]["robots_killed_level"],
            "clock, resources or kill credit regressed",
        )
    for index, state in enumerate(states):
        player = actor(state)
        guard = tracked(state)
        need(
            guard["expected_signature"] == bound["expected_signature"]
            and guard["expected_load_generation"] == bound["expected_load_generation"]
            and guard["binding_shields_raw"] == bound["binding_shields_raw"],
            "guard binding changed",
        )
        need(
            same(state["xl_pilot"]["wall_triggers"], initial["xl_pilot"]["wall_triggers"])
            and same(state["xl_pilot"]["operation_sequence"], initial["xl_pilot"]["operation_sequence"])
            and same(state["xl_pilot"]["remaining_objectives"], initial["xl_pilot"]["remaining_objectives"]),
            "ordinary combat unexpectedly operated a trigger",
        )
        need(
            state["game_time_raw"] >= initial["game_time_raw"]
            and player["shields"] <= baseline["shields"]
            and player["energy"] <= baseline["energy"]
            and player["robots_killed_level"] >= baseline["robots_killed_level"]
            and state["position"]["segment"] == (1315 if index < 8 else 647),
            "clock, resources or local geometry invalid",
        )
        observation = state["combat_observation"]
        need(
            observation["owned_weapons_truncated"] is False
            and observation["owned_weapon_count"] == len(observation["owned_weapons"]),
            "owned laser observer truncated or inconsistent",
        )
        for weapon in observation["owned_weapons"]:
            need(
                weapon["weapon_id"] == 0
                and weapon["parent_type"] == 4
                and weapon["parent_object"] == observation["player_object"]
                and weapon["parent_signature"] == observation["player_signature"],
                "unowned or non-Laser shot",
            )
    for ordinal in range(14):
        state = states[11 + 2 * ordinal]
        need(
            state["game_time_raw"] == initial["game_time_raw"] + (61 + 15 * ordinal) * 65536 // 60,
            "laser cadence changed",
        )
        weapons = state["combat_observation"]["owned_weapons"]
        new = set()
        for weapon in weapons:
            if weapon["weapon_id"] != 0 or weapon["lifeleft_raw"] <= 0:
                continue
            need(
                weapon["parent_type"] == 4
                and weapon["parent_object"] == state["combat_observation"]["player_object"]
                and weapon["parent_signature"] == state["combat_observation"]["player_signature"],
                "Laser not owned by player",
            )
            identity = (weapon["object"], weapon["signature"])
            if identity not in identities:
                new.add(identity)
        need(len(new) == 2, "ordinary paired Laser burst missing or extra shot")
        identities.update(new)
    need(len(identities) == 28, "fourteen independent paired bursts not witnessed")
    need(
        final["game_time_raw"] == initial["game_time_raw"] + 301 * 65536 // 60
        and final["player"]["energy"] < states[8]["player"]["energy"]
        and final["player"]["shields"] > 0
        and final["player"]["robots_killed_level"] == baseline["robots_killed_level"] + 1
        and final["primary_fire"]["held"] == 0
        and final["secondary_fire"]["held"] == 0
        and final["combat_observation"]["owned_weapon_count"] == 0,
        "ordinary kill or settled live save missing",
    )
    last = tracked(final)
    guard_killed(bound, last)
    contacts = [event["result"] for event in events if event.get("event") == "weapon_wall_contacts"]
    need(
        len(contacts) == 1
        and contacts[0]["kind"] == "actual_contacts_reaching_effect_blowup_gate"
        and integer(contacts[0]["contact_count_lower_bound"])
        and len(contacts[0]["contacts"]) <= contacts[0]["contact_count_lower_bound"],
        "wall-contact observer malformed",
    )
    # This observation may be truncated; never equate it with a complete contact list.
    need(
        type(contacts[0]["truncated"]) is bool
        and (
            contacts[0]["contact_count_lower_bound"] > len(contacts[0]["contacts"])
            if contacts[0]["truncated"]
            else contacts[0]["contact_count_lower_bound"] == len(contacts[0]["contacts"])
        ),
        "contact truncation not classified",
    )
    saved = checkpoint(root, receipt, final)
    need(saved["file"] != prior["file"] and saved["sha256"] != prior["sha256"], "combat save is not newly versioned")
    return receipt, saved


def cold(root, engine, source_path, blue_source):
    prior_receipt, prior = ordinary(source_path.parent, engine, blue_source)
    receipt, plan, final, events, steps = bundle("cold", root, engine)
    need(
        plan["resume_checkpoint"] == prior["file"]
        and receipt["resume_sha256"] == prior["sha256"]
        and receipt["process_id"] != prior_receipt["process_id"]
        and receipt["run_id"] != prior_receipt["run_id"]
        and len(events) == 1
        and receipt["checkpoint_history"] == []
        and receipt["checkpoints"] == {},
        "not a distinct zero-step, no-save restore",
    )
    expected = predecessor.restored(prior["state"])
    # Tracking is a per-process request, not persisted state. The cold plan
    # does not request a fresh binding of the killed robot.
    expected["combat_observation"]["tracked_objects"] = []
    need(same(final, expected), "cold restore differs beyond documented process-local resets")
    actor(final)


def main():
    if len(sys.argv) == 4 and sys.argv[1] == "source":
        source(Path(sys.argv[2]), Path(sys.argv[3]))
    elif len(sys.argv) == 6 and sys.argv[1] == "ordinary":
        ordinary(Path(sys.argv[2]), Path(sys.argv[3]), Path(sys.argv[4]))
    elif len(sys.argv) == 6 and sys.argv[1] == "cold":
        cold(Path(sys.argv[2]), Path(sys.argv[3]), Path(sys.argv[4]), Path(sys.argv[5]))
    else:
        raise ValueError("usage: source RESULT ENGINE | ordinary/cold ROOT ENGINE SOURCE_RESULT BLUE_RESULT")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, IndexError, TypeError, OSError) as error:
        sys.exit(str(error))
