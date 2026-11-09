"""Independent bounded Tulev1 early-route receipts; never reads original assets."""

import hashlib
import json
import re
import stat
import sys
from pathlib import Path


class WitnessError(ValueError):
    pass


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
    actions = ["assert", "send_button", "wait_for", "wait_for", "send_button"]
    need(
        all(
            any(e.get("step") == index and e.get("action") == action for e in events)
            for index, action in enumerate(actions)
        ),
        "robot256 combat action trace incomplete",
    )
    # Trace snapshots precede their numbered action. Step 3 sees first damage
    # from step 2; step 4 sees defeat from step 3.
    pre, damaged, defeated = (bound(state_at(events, n)) for n in (0, 3, 4))
    combat_damage(pre, damaged, defeated)
    need(
        state_at(events, 4)["player"]["robots_killed_level"]
        == state_at(events, 0)["player"]["robots_killed_level"] + 1,
        "robot256 defeat did not increment level kills",
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


def phase(name, root, engine, producer_path):
    steps = {
        "switch": 8,
        "combat": 8,
        "cloak": 7,
        "red_key": 8,
        "red_door": 8,
        "master": 12,
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
    order = list(steps)
    previous = order[order.index(name) - 1] if name != "switch" else None
    if previous is None:
        prior = doc(producer_path)["checkpoint_history"][-1]
    else:
        prior = doc(root.parent / previous / f"{previous}-result.json")["checkpoint_history"][0]
    need(
        prior["sha256"] == result["resume_sha256"],
        "phase did not resume its immediate earned predecessor",
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
        "invulnerable",
    ):
        need(
            prior["state"]["player"][field] == start_player[field],
            f"earned resource changed during phase load: {field}",
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
    expected_assists = 0 if name == "combat" else 2 if name == "master" else 1
    need(
        result["position_assist_events"] == len(assists) == expected_assists,
        "undisclosed position assist",
    )
    if name == "switch":
        need(
            assists[0]["step"] == 1
            and assists[0]["action"] == "pose_view"
            and initial["position"]["segment"] == 1183
            and state_at(events, 2)["position"]["segment"] == 450
            and final["xl_pilot"]["wall_triggers"]["6"]["operated"] is True
            and all(final["xl_pilot"]["doorways"][str(w)]["type"] == 4 for w in (55, 56, 57, 58))
            and not end_player["cloaked"]
            and not end_player["has_red_key"],
            "switch6 opening incomplete",
        )
        operations(events, [(6, 1, 1)])
    elif name == "combat":
        combat_trace_witness(events)
        need(
            end_player["robots_killed_level"] == start_player["robots_killed_level"] + 1
            and final["position"]["segment"] == 450
            and result["fixed_view_aim_events"] > 0,
            "robot256 live-HP damage or defeat not established",
        )
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
    else:
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
    need(
        not end_player["has_blue_key"] and not end_player["has_gold_key"],
        "unearned key grant",
    )


def main():
    if len(sys.argv) == 5 and sys.argv[1] == "producer":
        producer(Path(sys.argv[2]), Path(sys.argv[3]), Path(sys.argv[4]))
    elif len(sys.argv) == 6 and sys.argv[1] == "phase":
        phase(sys.argv[2], Path(sys.argv[3]), Path(sys.argv[4]), Path(sys.argv[5]))
    else:
        raise WitnessError("expected producer or phase arguments")
    print("Tulev1 early witness PASS")


if __name__ == "__main__":
    try:
        main()
    except (WitnessError, KeyError, IndexError, TypeError, ValueError, OSError) as exc:
        raise SystemExit(f"Tulev1 early witness FAIL: {exc}")
