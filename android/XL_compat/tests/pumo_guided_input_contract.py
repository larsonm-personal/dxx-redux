"""Asset-free, source-bound ordinary Guided launch and first-turn receipt contract."""

import copy
import hashlib
import json
import re
import sys
from pathlib import Path

from pumo_tulev1_early_contract import (
    checkpoint,
    doc,
    live,
    need,
    phase,
    restored_inventory,
    sha,
    trace,
)

FROZEN_ENGINE_SHA256 = "dece6cf76b0f5608bb586ee3143426fb3c0b909b86a73be85effc7a0a5d1d5db"


def source(path, engine):
    need(sha(engine) == FROZEN_ENGINE_SHA256, "unreviewed Guided engine binary")
    need(
        path.name == "guided_inventory-result.json" and path.parent.name == "guided_inventory",
        "wrong guided inventory producer",
    )
    root = path.parent
    need(root.parent.name == "pumo-tulev1-early-player", "wrong early producer owner")
    phase("guided_inventory", root, engine, root.parent.parent / "pumo-sophodo-player/main-result.json")
    result = doc(path)
    plan_path = root / "guided_inventory-executed-plan.json"
    plan = doc(plan_path, 1 << 20)
    history = result["checkpoint_history"]
    need(
        result["receipt_version"] == 2
        and result["result"] == "PASS"
        and result["steps"] == 8
        and result["initial_level_setup"] == result["final_level"] == 7
        and result["resumed_earned_checkpoint"] is True
        and result["transitions"] == []
        and result["position_assist_events"] == 1
        and result["fixed_view_aim_events"] == 0
        and type(result["process_id"]) is int
        and result["process_id"] > 0
        and re.fullmatch(r"[0-9a-f]{32}", result["run_id"])
        and plan["receipt_run_id"] == result["run_id"]
        and result["plan_sha256"] == sha(plan_path, 1 << 20)
        and result["executable_sha256"] == sha(engine)
        and result["resume_sha256"]
        == plan["resume_sha256"]
        == sha(checkpoint(root, plan["resume_checkpoint"]), 4 << 20)
        and len(history) == 1
        and history[0]["step"] == 7
        and result["checkpoints"]["7"]["sha256"] == history[0]["sha256"]
        and history[0]["sha256"] == sha(checkpoint(root, history[0]["file"]), 4 << 20)
        and doc(root / "guided_inventory-state.json", 1 << 20) == history[0]["state"],
        "guided inventory producer identity or versioned save invalid",
    )
    saved = history[0]["state"]
    live(saved)
    need(
        saved["player"]["cloaked"] is True
        and saved["player"]["has_red_key"] is True
        and saved["player"]["secondary_ammo"][6] == 4
        and saved["player"]["secondary_weapon_flags"] == 65
        and saved["player"]["secondary_weapon"] == 0
        and saved["position"]["segment"] == 458
        and saved["xl_pilot"]["remaining_objectives"] == 1
        and saved["combat_observation"]["guided_control"] is None
        and saved["combat_observation"]["player_object"] == 0
        and saved["combat_observation"]["player_signature"] == 0
        and saved["simulation_rng"]["state_available"] is True,
        "guided inventory source boundaries invalid",
    )
    return history[0]


# Reviewed packet policy. These canonical hashes are independent of plan files and
# cover every action, axis, button, timeout, assertion, and top-level control.
APPROVED_TEMPLATES = {
    "neutral": "344ea0838b8c61aa7ca909e741ddd4d92c3ffd0fb413a553723523968c70a5dc",
    "yaw": "13c434dcf594dd33d66821572abadf568db6599e5b0733f0fc6d5be51c4c6741",
}


def expected_plan(root, kind, saved):
    template = doc(Path(__file__).with_name(f"pumo_guided_input_{kind}.json"))
    canonical = json.dumps(template, sort_keys=True, separators=(",", ":")).encode()
    need(hashlib.sha256(canonical).hexdigest() == APPROVED_TEMPLATES[kind], f"{kind}: reviewed action template changed")
    template["resume_checkpoint"] = "tulev1-guided_inventory-level7.sav"
    template["resume_sha256"] = saved["sha256"]
    template["steps"][5]["expect"]["game_time_raw"] = saved["state"]["game_time_raw"] + 65536
    return template


def remove_motion(state):
    value = copy.deepcopy(state)
    guided = value["combat_observation"]["guided_control"]
    for key in ("orient_forward_fixed", "orient_right_fixed", "orient_up_fixed", "position_fixed"):
        guided.pop(key)
    for weapon in value["combat_observation"]["owned_weapons"]:
        for key in ("velocity_fixed", "position_fixed"):
            weapon.pop(key)
    return value


def object_witness(state, saved, initial):
    live(state)
    player = state["player"]
    source_player = saved["state"]["player"]
    need(
        player["secondary_ammo"][6] == 3
        and all(player["secondary_ammo"][n] == source_player["secondary_ammo"][n] for n in range(10) if n != 6),
        "forged ammo change",
    )
    for field in source_player.keys() - {"secondary_ammo", "secondary_weapon"}:
        need(player[field] == source_player[field], f"changed player resource {field}")
    need(
        player["secondary_weapon"] == 6
        and player["cloaked"] is True
        and player["has_red_key"] is True
        and state["position"]["segment"] == initial["position"]["segment"] == 458
        and state["xl_pilot"] == initial["xl_pilot"]
        and state["xl_pilot"]["remaining_objectives"] == 1
        and state["combat_observation"]["player_object"] == initial["combat_observation"]["player_object"]
        and state["combat_observation"]["player_signature"] == initial["combat_observation"]["player_signature"]
        and state["secondary_weapon_definition"]["weapon_id"] == 37,
        "player/world/identity boundary changed",
    )
    obs = state["combat_observation"]
    guided = obs["guided_control"]
    need(
        guided is not None
        and guided["weapon_id"] == 37
        and guided["lifeleft_raw"] > 0
        and guided["parent_object"] == obs["player_object"]
        and guided["parent_signature"] == obs["player_signature"]
        and obs["owned_weapon_count"] == len(obs["owned_weapons"]) == 1
        and obs["owned_weapons_truncated"] is False,
        "live local Guided object invalid",
    )
    weapon = obs["owned_weapons"][0]
    need(
        weapon["weapon_id"] == 37
        and weapon["object"] == guided["object"]
        and weapon["signature"] == guided["signature"]
        and weapon["parent_type"] == 4
        and weapon["parent_object"] == obs["player_object"]
        and weapon["parent_signature"] == obs["player_signature"]
        and weapon["radius_raw"] > 0
        and weapon["position_fixed"] == guided["position_fixed"],
        "Guided ownership/signature/size invalid",
    )
    return guided, weapon


def restored_identity(initial, saved_state):
    need(
        initial["combat_observation"]["player_object"] == saved_state["combat_observation"]["player_object"]
        and initial["combat_observation"]["player_signature"] == saved_state["combat_observation"]["player_signature"],
        "forged restored player identity",
    )


def run(root, kind, saved, engine):
    result = doc(root / f"{kind}-result.json")
    plan_path = root / f"{kind}-executed-plan.json"
    plan = doc(plan_path, 1 << 20)
    events = trace(root / f"{kind}-trace.jsonl")
    final = doc(root / f"{kind}-state.json", 1 << 20)
    expected = expected_plan(root, kind, saved)
    need(
        plan.pop("receipt_run_id", None) == result["run_id"]
        and plan == expected
        and re.fullmatch(r"[0-9a-f]{32}", result["run_id"])
        and result["receipt_version"] == 2
        and result["result"] == "PASS"
        and result["steps"] == len(events) == 24
        and result["initial_level_setup"] == result["final_level"] == 7
        and result["resumed_earned_checkpoint"] is True
        and result["rendered"] is False
        and result["position_assist_events"] == result["fixed_view_aim_events"] == 0
        and result["operation_sequence"] == 0
        and result["transitions"] == []
        and result["checkpoints"] == {}
        and result["checkpoint_history"] == []
        and type(result["process_id"]) is int
        and result["process_id"] > 0
        and result["plan_sha256"] == sha(plan_path, 1 << 20)
        and result["executable_sha256"] == sha(engine)
        and result["resume_sha256"]
        == saved["sha256"]
        == expected["resume_sha256"]
        == sha(checkpoint(root, expected["resume_checkpoint"]), 4 << 20),
        f"{kind}: plan, process, engine or receipt mismatch",
    )
    need(
        all(
            e["step"] == i and e["action"] == expected["steps"][i]["action"] and e["simulation_fps"] == 60
            for i, e in enumerate(events)
        )
        and final == doc(root / "player-state.json", 1 << 20)
        and final == events[23]["state"],
        f"{kind}: trace/final mismatch",
    )
    initial = events[0]["state"]
    restored_inventory(saved["state"], initial)
    need(
        initial["player"] == saved["state"]["player"],
        f"{kind}: source inventory or player resources changed on restore",
    )
    restored_identity(initial, saved["state"])
    need(
        initial["game_time_raw"] == saved["state"]["game_time_raw"]
        and initial["simulation_rng"]["state"] == saved["state"]["simulation_rng"]["state"]
        and initial["position"] == saved["state"]["position"]
        and events[5]["state"]["game_time_raw"] == saved["state"]["game_time_raw"] + 65536
        and events[5]["state"]["secondary_fire"]["next_delta_raw"] == 0
        and events[5]["state"]["secondary_fire"]["count"] == 0,
        f"{kind}: restored start or 1-second rearm invalid",
    )
    for i in range(8, 24):
        state = events[i]["state"]
        guided, _ = object_witness(state, saved, initial)
        need(
            guided["signature"] == events[8]["state"]["combat_observation"]["guided_control"]["signature"],
            f"{kind}: Guided signature changed",
        )
    need(
        final["secondary_fire"]["held"] == 0
        and {p.name for p in (root / "preview-user").glob("*.sav")} == {expected["resume_checkpoint"]},
        f"{kind}: fire held or produced save",
    )
    return result, events


def forward_velocity_aligned(forward, velocity):
    need(
        len(forward) == len(velocity) == 3 and all(type(x) is int for x in forward + velocity),
        "invalid Guided forward/velocity vector",
    )
    dot = sum(a * b for a, b in zip(forward, velocity))
    forward_sq = sum(a * a for a in forward)
    velocity_sq = sum(b * b for b in velocity)
    need(
        dot > 0 and forward_sq > 0 and velocity_sq > 0 and dot * dot * 10000 >= 9801 * forward_sq * velocity_sq,
        "Guided velocity not aligned with stock forward orientation",
    )


def positive_local_yaw(neutral_forward, neutral_right, neutral_up, yaw_forward, neutral_velocity, yaw_velocity):
    # Stock controls.c applies positive heading about the missile's local up axis.
    # World X/Y signs depend on the inherited orientation and are not a yaw oracle.
    vectors = (neutral_forward, neutral_right, neutral_up, yaw_forward, neutral_velocity, yaw_velocity)
    need(all(len(v) == 3 and all(type(x) is int for x in v) for v in vectors), "invalid Guided local-frame vectors")
    dot = lambda a, b: sum(x * y for x, y in zip(a, b))
    square = lambda a: dot(a, a)
    cross = lambda a, b: (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
    basis = (neutral_forward, neutral_right, neutral_up)
    need(
        all(square(v) > 0 for v in basis)
        # A stock orientation matrix has equal-length unit axes. Preserve that
        # scale so a shortened up vector cannot hide pitch in the projection.
        and all(abs(square(a) - square(b)) * 100 < square(a) for a, b in ((basis[0], basis[1]), (basis[0], basis[2])))
        and all(
            dot(a, b) ** 2 * 10000 < square(a) * square(b)
            for a, b in ((basis[0], basis[1]), (basis[0], basis[2]), (basis[1], basis[2]))
        )
        and dot(cross(neutral_right, neutral_up), neutral_forward) > 0,
        "Guided neutral orientation basis invalid",
    )
    for before, after in ((neutral_forward, yaw_forward), (neutral_velocity, yaw_velocity)):
        delta = tuple(y - x for x, y in zip(before, after))
        right = dot(delta, neutral_right)
        need(right > 0 and abs(dot(delta, neutral_up)) < right, "stock positive local yaw response invalid")


def pair(neutral, yaw, producer_path, engine):
    saved = source(producer_path, engine)
    nr, ne = run(neutral, "neutral", saved, engine)
    yr, ye = run(yaw, "yaw", saved, engine)
    need(
        nr["process_id"] != yr["process_id"]
        and nr["process_id"] != doc(producer_path)["process_id"]
        and yr["process_id"] != doc(producer_path)["process_id"]
        and nr["run_id"] != yr["run_id"],
        "paired processes/nonces reused",
    )
    for i in range(14):
        need(ne[i]["state"] == ye[i]["state"], f"pre-yaw step {i} changed")
    for i in (14, 15, 16, 17, 18, 19, 20, 21, 22, 23):
        need(remove_motion(ne[i]["state"]) == remove_motion(ye[i]["state"]), f"non-Guided state diverged at {i}")
    for i in (15, 23):
        n = ne[i]["state"]["combat_observation"]["guided_control"]
        y = ye[i]["state"]["combat_observation"]["guided_control"]
        nv = ne[i]["state"]["combat_observation"]["owned_weapons"][0]["velocity_fixed"]
        yv = ye[i]["state"]["combat_observation"]["owned_weapons"][0]["velocity_fixed"]
        nf, yf = n["orient_forward_fixed"], y["orient_forward_fixed"]
        forward_velocity_aligned(nf, nv)
        forward_velocity_aligned(yf, yv)
        need(
            n["object"] == y["object"] and n["signature"] == y["signature"],
            f"stock positive yaw response invalid at {i}",
        )
        positive_local_yaw(nf, n["orient_right_fixed"], n["orient_up_fixed"], yf, nv, yv)
    print("PASS: source-bound ordinary Guided launch and positive-yaw first turn")


def main():
    try:
        if len(sys.argv) == 4 and sys.argv[1] == "source":
            print(source(Path(sys.argv[2]), Path(sys.argv[3]))["state"]["game_time_raw"])
        elif len(sys.argv) == 6 and sys.argv[1] == "pair":
            pair(*(Path(x) for x in sys.argv[2:]))
        else:
            raise ValueError("usage: source producer engine | pair neutral yaw producer engine")
    except (ValueError, KeyError, IndexError, OSError, TypeError) as e:
        print(f"FAIL: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
