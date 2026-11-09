"""Source-only contract for the incremental, assisted-local level 7 supplies owner."""

import copy
from collections import Counter
import json
import math
import re
import struct
import sys
from pathlib import Path

import pumo_tulev1_courtyard_guards_contract as courtyard

need = courtyard.need
same = courtyard.same
doc = courtyard.doc
sha = courtyard.sha
save = courtyard.save
decode = courtyard.decode
read = courtyard.read
restored = courtyard.restored

STAGES = ("switch6", "cloak113", "guided140")
STEPS = {"switch6": 199, "cloak113": 145, "guided140": 328}
SAVE_STEPS = {"switch6": 196, "cloak113": 143, "guided140": 326}


def phase_parts(phase):
    stage, kind = phase.rsplit("_", 1)
    need(stage in STAGES and kind in ("warm", "cold"), "unknown supplies phase")
    return stage, kind


def template(phase):
    phase_parts(phase)
    return doc(Path(__file__).with_name(f"pumo_tulev1_supplies_{phase}.json"))


def source(courtyard_root, engine, blast, guided_producer, guided_guard, quad, guard, blue):
    """Revalidate the registered courtyard producer and its public ancestry."""
    courtyard.source(blast, engine, guided_producer, guided_guard, quad, guard, blue)
    first = courtyard_root / "guard256" / "guard256-result.json"
    second = courtyard_root / "guard305" / "guard305-result.json"
    courtyard.verify("guard256", courtyard_root / "guard256", engine, blast)
    courtyard.verify("guard256_cold", courtyard_root / "guard256_cold", engine, first)
    courtyard.verify("guard305", courtyard_root / "guard305", engine, first)
    courtyard.verify("guard305_cold", courtyard_root / "guard305_cold", engine, second)
    courtyard.source_save(second)


def producer_save(path, phase):
    stage, kind = phase_parts(phase)
    if stage == "switch6" and kind == "warm":
        need(path.name == "guard305-result.json", "not registered guard305 producer")
        return courtyard.source_save(path)
    expected = f"{stage}_warm-result.json" if kind == "cold" else None
    if expected is None:
        expected_stage = STAGES[STAGES.index(stage) - 1]
        expected = f"{expected_stage}_cold-result.json"
    need(path.name == expected, "not immediate supplies predecessor")
    receipt = doc(path)
    history = receipt["checkpoint_history"]
    need(receipt["result"] == "PASS" and len(history) == 1, "predecessor save absent")
    item = history[0]
    prior_stage = stage if kind == "cold" else STAGES[STAGES.index(stage) - 1]
    expected_step = SAVE_STEPS[prior_stage] if kind == "cold" else 1
    need(
        same(item["step"], expected_step)
        and re.fullmatch(rf"pumo-player-level7-[0-9]+-step{expected_step}\.sav", item["file"])
        and item["sha256"] == sha(save(path.parent, item["file"])),
        "predecessor versioned save invalid",
    )
    return receipt, item


def binding(phase, plan, producer):
    stage, kind = phase_parts(phase)
    _receipt, history = producer_save(producer, phase)
    expected = template(phase)
    for key in ("resume_checkpoint", "resume_sha256", "receipt_run_id"):
        if key in plan:
            expected[key] = plan[key]
    need(same(plan, expected), "unbound supplies template changed")
    state = history["state"]
    for field, value in (
        ("game_time_raw", state["game_time_raw"]),
        ("player.shields", state["player"]["shields"]),
        ("player.energy", state["player"]["energy"]),
        ("position.segment", state["position"]["segment"]),
    ):
        plan["checkpoint_expect"][field] = value
    if kind == "warm":
        plan["steps"][0]["expect"].update(plan["checkpoint_expect"])
        if stage in ("cloak113", "guided140"):
            pose = next(step for step in plan["steps"] if step["action"] == "pose_view")
            need(pose["position_fixed"] == [0, 0, 0], "same-position setup placeholder changed")
            pose["position_fixed"] = [round(state["position"][axis] * 65536) for axis in ("x", "y", "z")]
            need(pose["segment"] == state["position"]["segment"], "same-position setup segment changed")
    return plan


def bind(phase, plan_path, producer):
    plan = binding(phase, doc(plan_path), producer)
    plan_path.write_text(json.dumps(plan, indent=4) + "\n")


def items(state):
    return {item["object"]: item for item in state["level_scene"]["powerups"]}


def saved_object_table(raw, state):
    """Find the unique version-47 serialized object table from the player slot.

    d2/main/state.c writes DGSS/version at 2577 and the object count then
    consecutive object_rw entries at 2742-2758. d2/main/object.h defines
    object_rw at 326: signature, type/id, segment, position, and size.
    The 264-byte packed layout and field offsets are scoped to version 47
    (android_save_format.h). No object-table location or count is copied from
    a prior run. This is only a test oracle.
    """
    need(
        len(raw) >= 8 and raw[:4] == b"DGSS" and struct.unpack_from("<i", raw, 4)[0] == 47,
        "unsupported saved object format",
    )
    position = struct.pack("<3i", *(round(state["position"][axis] * 65536) for axis in ("x", "y", "z")))
    candidates = []
    cursor = 0
    while True:
        found = raw.find(position, cursor)
        if found < 0:
            break
        cursor = found + 1
        base = found - 18
        if base < 4:
            continue
        count = struct.unpack_from("<i", raw, base - 4)[0]
        if not 1 <= count <= 32768 or base + count * 264 > len(raw):
            continue
        if raw[base + 4] != 4 or struct.unpack_from("<h", raw, base + 14)[0] != state["position"]["segment"]:
            continue
        candidates.append((base, count))
    need(len(candidates) == 1, "saved object table not uniquely identified")
    return candidates[0]


def saved_actor_radii(path, state):
    """Decode source actor identities and radii, including distant actors."""
    raw = path.read_bytes()
    base, count = saved_object_table(raw, state)
    actors = {}
    for slot in range(count):
        offset = base + slot * 264
        if raw[offset + 4] != 2:
            continue
        radius = struct.unpack_from("<i", raw, offset + 66)[0]
        need(0 < radius < 100 * 65536, "saved actor radius invalid")
        actors[slot] = (
            raw[offset + 5],
            radius,
            struct.unpack_from("<i", raw, offset)[0],
            struct.unpack_from("<i", raw, offset + 70)[0],
        )
    return actors


def actor_hulls_clear(steps, producer, history):
    radii = saved_actor_radii(save(producer.parent, history["file"]), history["state"])
    need(all(slot in radii for slot in (162, 214)), "source corridor actors missing")
    baseline = {}
    for event in steps:
        state = event["state"]
        player = state["position"]
        player_xyz = [player[axis] for axis in ("x", "y", "z")]
        player_radius = player["radius_raw"] / 65536

        def hull_clearance(position_fixed, radius_raw):
            actor_xyz = [coordinate / 65536 for coordinate in position_fixed]
            return math.dist(player_xyz, actor_xyz) - player_radius - radius_raw / 65536

        scene = {actor["object"]: actor for actor in state["level_scene"]["robots"]}
        tracked = state["combat_observation"]["tracked_objects"]
        need({actor["object"] for actor in tracked} == {162, 214}, "corridor actor bindings missing")
        for actor in tracked:
            slot = actor["object"]
            health = actor["actor_state"]
            need(
                actor["exists"] is True
                and actor["expected_type"] == actor["current_slot"]["type"] == 2
                and actor["expected_id"] == actor["current_slot"]["id"] == radii[slot][0]
                and actor["expected_signature"] == actor["current_slot"]["signature"] == radii[slot][2]
                and actor["damage_since_binding_raw"] == 0
                and health["shields_raw"] == radii[slot][3] > 0
                and not health["should_be_dead"],
                "corridor actor identity or health changed",
            )
            identity = (actor["expected_signature"], health["shields_raw"])
            if slot in baseline:
                need(baseline[slot] == identity, "corridor actor damaged or rebound")
            else:
                baseline[slot] = identity
            need(hull_clearance(health["position_fixed"], radii[slot][1]) > 60, "tracked actor full hull too close")
            scene_actor = scene.get(slot)
            if scene_actor is not None:
                need(
                    scene_actor["id"] == radii[slot][0] and scene_actor["position_fixed"] == health["position_fixed"],
                    "scene actor identity or position drifted",
                )
        for slot, actor in scene.items():
            need(slot in radii and actor["id"] == radii[slot][0], "new or wrong scene actor")
            need(
                hull_clearance(actor["position_fixed"], radii[slot][1]) > 60,
                "player and scene actor full hulls too close",
            )


def common(state):
    pilot = state["xl_pilot"]
    player = state["player"]
    need(
        state["current_level_num"] == 7
        and not state["player_dead"]
        and not state["cheats_enabled"]
        and player["shields"] > 0
        and not player["invulnerable"]
        and player["quad_lasers"]
        and player["has_blue_key"]
        and not player["has_gold_key"]
        and not player["has_red_key"]
        and same(player["robots_killed_level"], 4)
        and same(pilot["remaining_objectives"], 1)
        and state["combat_observation"]["guided_control"] is None,
        "ordinary supplies state drifted",
    )


def cold_save_parity(source_history, successor_history, producer_root, successor_root):
    """Compare whole saves, except source-defined inactive-slot canonicalization.

    d2/main/state.c writes object_rw.rtype.pobj_info.alt_textures at +260
    for RT_POLYOBJ. Both state_restore_segment_object_links() and its relink
    fallback set that field to -1 on restored slots. In version-47 object_rw,
    type +4 is OBJ_NONE=255 and render_type +12 is RT_POLYOBJ=1
    (d2/main/object.h, android_save_format.h). Only zero to -1 at that exact
    field of an inactive polymodel slot is permitted. This is not a general
    inactive-slot or active-object normalization.
    """
    source_path = save(producer_root, source_history["file"])
    successor_path = save(successor_root, successor_history["file"])
    left, right = source_path.read_bytes(), successor_path.read_bytes()
    need(sha(source_path) == source_history["sha256"], "cold source save hash changed")
    need(sha(successor_path) == successor_history["sha256"], "cold successor save hash changed")
    need(len(left) == len(right), "cold successor save length changed")
    source_base, source_count = saved_object_table(left, source_history["state"])
    target_base, target_count = saved_object_table(right, successor_history["state"])
    need((source_base, source_count) == (target_base, target_count), "cold object table changed")
    allowed = set()
    for slot in range(source_count):
        offset = source_base + slot * 264
        field = offset + 260
        if (
            left[offset + 4] == right[offset + 4] == 255
            and left[offset + 12] == right[offset + 12] == 1
            and left[field : field + 4] == b"\0" * 4
            and right[field : field + 4] == b"\xff" * 4
        ):
            allowed.update(range(field, field + 4))
    need(
        all(index in allowed for index, (before, after) in enumerate(zip(left, right)) if before != after),
        "cold successor changed hidden save bytes",
    )


def trace_events_allowed(events, stage, kind):
    expected = (
        {
            "switch6": {
                "position_assist": 1,
                "successful_wall_operation": 1,
                "trigger_change": 1,
                "weapon_wall_contacts": 1,
                "player_physics_fvi": 1,
            },
            "cloak113": {"position_assist": 1, "player_physics_fvi": 1},
            "guided140": {"position_assist": 1},
        }[stage]
        if kind == "warm"
        else {}
    )
    observed = Counter(event["event"] for event in events if "event" in event)
    need(
        observed == expected and all("event" in event or ("step" in event and "state" in event) for event in events),
        "unexpected or missing non-state trace event",
    )


def verify(phase, root, engine, producer):
    stage, kind = phase_parts(phase)
    prior, history = producer_save(producer, phase)
    receipt = doc(root / f"{phase}-result.json")
    plan_path = root / f"{phase}-executed-plan.json"
    plan = doc(plan_path)
    expected = binding(
        phase,
        template(phase) | {k: plan[k] for k in ("resume_checkpoint", "resume_sha256", "receipt_run_id")},
        producer,
    )
    need(same(plan, expected), "executed supplies plan changed")
    final = doc(root / f"{phase}-state.json")
    events = [decode(line) for line in read(root / f"{phase}-trace.jsonl").splitlines()]
    trace_events_allowed(events, stage, kind)
    steps = [event for event in events if "step" in event and "state" in event]
    count = STEPS[stage] if kind == "warm" else 3
    need(
        receipt["result"] == "PASS"
        and receipt["receipt_version"] == 2
        and receipt["resumed_earned_checkpoint"] is True
        and receipt["initial_level_setup"] == receipt["final_level"] == 7
        and receipt["rendered"] is False
        and receipt["transitions"] == []
        and receipt["steps"] == len(steps) == count
        and [event["step"] for event in steps] == list(range(count))
        and [event["action"] for event in steps] == [x["action"] for x in plan["steps"]]
        and same(final, steps[-1]["state"]),
        "supplies receipt or trace invalid",
    )
    need(
        all(
            set(event) == {"step", "action", "state", "simulation_fps"} and same(event["simulation_fps"], 60)
            for event in steps
        ),
        "trace row or rate invalid",
    )
    need(
        receipt["resume_sha256"] == plan["resume_sha256"] == history["sha256"]
        and plan["resume_checkpoint"] == history["file"]
        and receipt["plan_sha256"] == sha(plan_path)
        and receipt["executable_sha256"] == sha(engine)
        and re.fullmatch(r"[0-9a-f]{32}", receipt["run_id"])
        and receipt["run_id"] == plan["receipt_run_id"] != prior["run_id"]
        and receipt["process_id"] != prior["process_id"],
        "input, producer, or process identity invalid",
    )
    assists = [event for event in events if event.get("event") == "position_assist"]
    need(
        receipt["position_assist_events"] == len(assists) == (1 if kind == "warm" else 0)
        and receipt["fixed_view_aim_events"] == 0,
        "unplanned setup or aim assist",
    )
    initial = steps[0]["state"]
    earned = restored(history["state"])
    earned["combat_observation"]["tracked_objects"] = (
        initial["combat_observation"]["tracked_objects"] if kind == "warm" and stage != "switch6" else []
    )
    need(same(initial, earned), "not exact earned predecessor resume")
    initial_items = items(initial)
    for event in steps:
        common(event["state"])
        pilot = event["state"]["player"]
        baseline = initial["player"]
        changing = (
            {"switch6": {"energy"}, "cloak113": {"cloaked"}, "guided140": {"secondary_ammo"}}[stage]
            if kind == "warm"
            else set()
        )
        need(
            all(same(pilot[key], value) for key, value in baseline.items() if key not in changing),
            "unexpected health or inventory change",
        )
        if kind == "warm":
            if stage == "switch6":
                need(
                    baseline["energy"] - 1 <= pilot["energy"] <= baseline["energy"],
                    "ordinary shot energy outside bounded decrease",
                )
            elif stage == "cloak113":
                need(pilot["cloaked"] in (False, True), "cloak status invalid")
            else:
                ammo = pilot["secondary_ammo"]
                need(
                    ammo[:6] == baseline["secondary_ammo"][:6]
                    and ammo[6] in (0, 4)
                    and ammo[7:] == baseline["secondary_ammo"][7:],
                    "unplanned secondary ammo change",
                )
        expected_items = copy.deepcopy(initial_items)
        if kind == "warm" and stage == "cloak113":
            expected_items.pop(113, None)
        elif kind == "warm" and stage == "guided140":
            expected_items.pop(140, None)
        need(items(event["state"]) in (initial_items, expected_items), "unplanned stock item consumption or creation")
    saved = receipt["checkpoint_history"]
    expected_step = SAVE_STEPS[stage] if kind == "warm" else 1
    need(
        len(saved) == 1
        and saved[0]["step"] == expected_step
        and re.fullmatch(rf"pumo-player-level7-[0-9]+-step{expected_step}\.sav", saved[0]["file"])
        and saved[0]["sha256"] == sha(save(root, saved[0]["file"]))
        and same(saved[0]["state"], final)
        and receipt["checkpoints"].get("7", {}).get("sha256")
        == sha(root / "preview-user" / receipt["checkpoints"]["7"]["file"]),
        "phase versioned save invalid",
    )
    if kind == "cold":
        cold_save_parity(history, saved[0], producer.parent, root)
        need(
            not any(event["action"] == "wait_ms" for event in steps)
            and saved[0]["state"]["game_time_raw"] == initial["game_time_raw"]
            and final["game_time_raw"] == initial["game_time_raw"]
            and same(final, initial),
            "cold no-frame resave invalid",
        )
        return
    need(len(assists) == 1 and assists[0]["action"] == "pose_view", "local setup missing")
    start_items = items(initial)
    end_items = items(final)
    if stage == "switch6":
        operations = [event for event in events if event.get("event") == "successful_wall_operation"]
        owned = [weapon for event in steps for weapon in event["state"]["combat_observation"]["owned_weapons"]]
        owner = initial["combat_observation"]
        need(
            len(operations) == 1
            and operations[0]["index"] == 6
            and operations[0]["shot"] == 1
            and owned
            and all(
                weapon["weapon_id"] == 0
                and weapon["parent_object"] == owner["player_object"]
                and weapon["parent_signature"] == owner["player_signature"]
                for weapon in owned
            )
            and initial["xl_pilot"]["wall_triggers"]["6"]["operated"] is False
            and final["xl_pilot"]["wall_triggers"]["6"]["operated"] is True
            and final["xl_pilot"]["wall_triggers"]["6"]["shot"] == 1
            and all(final["xl_pilot"]["doorways"][str(i)]["type"] == 4 for i in (55, 56, 57, 58))
            and 113 in start_items
            and 140 in start_items
            and 113 in end_items
            and 140 in end_items
            and all(event["state"]["position"]["segment"] == 450 for event in steps),
            "ordinary switch6 shot or gates invalid",
        )
        need(final["player"]["energy"] < initial["player"]["energy"], "ordinary switch6 shot did not consume energy")
    elif stage == "cloak113":
        actor_hulls_clear(steps, producer, history)
        need(
            113 in start_items
            and start_items[113]["id"] == 23
            and 113 not in end_items
            and 140 in end_items
            and initial["player"]["cloaked"] is False
            and final["player"]["cloaked"] is True
            and initial["position"]["segment"] == 450
            and final["position"]["segment"] == 457,
            "ordinary Cloak113 pickup invalid",
        )
    else:
        actor_hulls_clear(steps, producer, history)
        need(
            140 in start_items
            and start_items[140]["id"] == 41
            and 140 not in end_items
            and initial["position"]["segment"] == 457
            and final["position"]["segment"] == 458
            and initial["player"]["secondary_ammo"][6] == 0
            and final["player"]["secondary_ammo"][6] == 4
            and initial["player"]["secondary_weapon"] == final["player"]["secondary_weapon"] == 0
            and final["player"]["cloaked"] is True,
            "ordinary Guided140 pickup invalid",
        )
        transitions = [(a["state"], b["state"]) for a, b in zip(steps, steps[1:])]
        need(
            any(
                before["position"]["segment"] == 457
                and 140 in items(before)
                and before["player"]["secondary_ammo"][6] == 0
                and after["position"]["segment"] == 458
                and 140 not in items(after)
                and after["player"]["secondary_ammo"][6] == 4
                for before, after in transitions
            ),
            "same-tick segment entry and stock pickup not witnessed",
        )


def main():
    if len(sys.argv) == 10 and sys.argv[1] == "source":
        source(*(Path(x) for x in sys.argv[2:]))
    elif len(sys.argv) == 5 and sys.argv[1] == "bind":
        bind(sys.argv[2], Path(sys.argv[3]), Path(sys.argv[4]))
    elif len(sys.argv) == 6 and sys.argv[1] == "verify":
        verify(sys.argv[2], Path(sys.argv[3]), Path(sys.argv[4]), Path(sys.argv[5]))
    else:
        raise ValueError("invalid supplies contract arguments")


if __name__ == "__main__":
    main()
