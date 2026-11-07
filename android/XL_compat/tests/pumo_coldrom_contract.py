"""Bounded original Coldrom native witness checks; never reads original assets."""

import argparse
import hashlib
import json
import re
import stat
from pathlib import Path


class WitnessError(ValueError):
    pass


def need(ok, why):
    if not ok:
        raise WitnessError(why)


def exact_number(value):
    return type(value) in (int, float)


MAX_PLAN_BYTES = 1 << 20
MAX_RECEIPT_BYTES = 4 << 20
MAX_STATE_BYTES = 1 << 20
MAX_TRACE_BYTES = 16 << 20
MAX_TRACE_LINE_BYTES = 1 << 18
MAX_TRACE_EVENTS = 10000
MAX_SAVE_BYTES = 4 << 20
MAX_ENGINE_BYTES = 64 << 20
HEX32 = re.compile(r"[0-9a-f]{32}\Z")
HEX64 = re.compile(r"[0-9a-f]{64}\Z")


def regular_file(path, limit):
    info = path.lstat()
    need(
        stat.S_ISREG(info.st_mode) and info.st_size <= limit,
        f"unsafe or oversized file: {path.name}",
    )
    return path


def sha(path, limit=MAX_SAVE_BYTES):
    regular_file(path, limit)
    digest = hashlib.sha256()
    total = 0
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(65536), b""):
            total += len(chunk)
            need(total <= limit, f"file grew beyond cap: {path.name}")
            digest.update(chunk)
    return digest.hexdigest()


def bounded_json(path, limit):
    regular_file(path, limit)
    with path.open("rb") as source:
        data = source.read(limit + 1)
    need(len(data) <= limit, f"file grew beyond cap: {path.name}")
    return json.loads(data)


def bounded_text(path, limit):
    regular_file(path, limit)
    with path.open("rb") as source:
        data = source.read(limit + 1)
    need(len(data) <= limit, f"file grew beyond cap: {path.name}")
    return data.decode("utf-8")


def bounded_trace(path):
    regular_file(path, MAX_TRACE_BYTES)
    events = []
    total = 0
    with path.open("rb") as source:
        for line in source:
            total += len(line)
            need(
                total <= MAX_TRACE_BYTES and len(line) <= MAX_TRACE_LINE_BYTES,
                "trace byte or line cap exceeded",
            )
            need(len(events) < MAX_TRACE_EVENTS, "trace event cap exceeded")
            events.append(json.loads(line))
    need(events, "empty trace")
    return events


def safe_checkpoint(root, name):
    need(
        type(name) is str
        and name
        and Path(name).name == name
        and name not in (".", "..")
        and "/" not in name
        and "\\" not in name,
        "unsafe checkpoint name",
    )
    path = root / "preview-user" / name
    regular_file(path, MAX_SAVE_BYTES)
    return path


def state_at(trace, step):
    matches = [
        event["state"]
        for event in trace
        if event.get("step") == step and "state" in event
    ]
    need(len(matches) == 1, f"step {step} state missing or duplicate")
    return matches[0]


def bound(state, object_number, signature, generation=None):
    obs = state["combat_observation"]
    need(obs["owned_weapons_truncated"] is False, "owned weapon observation truncated")
    matches = [
        item for item in obs["tracked_objects"] if item["object"] == object_number
    ]
    need(len(matches) == 1, f"actor {object_number} binding missing or duplicate")
    item = matches[0]
    need(
        type(item["expected_signature"]) is int
        and item["expected_signature"] == signature,
        f"actor {object_number} signature changed",
    )
    need(type(item["exists"]) is bool, "actor binding exists type invalid")
    need(
        type(state["current_level_num"]) is int and state["current_level_num"] == 14,
        "combat observation left level 14",
    )
    need(
        type(obs["load_generation"]) is int,
        "combat observation generation type invalid",
    )
    need(
        type(item["expected_level"]) is int
        and type(item["observed_level"]) is int
        and item["expected_level"]
        == item["observed_level"]
        == state["current_level_num"],
        "actor level binding invalidated",
    )
    need(
        type(item["expected_load_generation"]) is int
        and type(item["observed_load_generation"]) is int
        and item["expected_load_generation"]
        == item["observed_load_generation"]
        == obs["load_generation"],
        "actor load binding invalidated",
    )
    if generation is not None:
        need(
            obs["load_generation"] == generation,
            "actor binding changed generation during combat",
        )
    need(
        (item["actor_state"] is None) == (item["exists"] is False),
        "actor state/existence inconsistent",
    )
    return item


def decreased(before, after, object_number, signature, generation=None):
    first = bound(before, object_number, signature, generation)
    last = bound(after, object_number, signature, generation)
    need(first["exists"] is True and last["exists"] is True, "damage actor disappeared")
    need(
        exact_number(first["actor_state"]["shields_raw"])
        and exact_number(last["actor_state"]["shields_raw"])
        and last["actor_state"]["shields_raw"] < first["actor_state"]["shields_raw"],
        f"actor {object_number} did not take new damage",
    )


def destroyed(state, object_number, signature, reactor=False, generation=None):
    item = bound(state, object_number, signature, generation)
    if item["exists"] is False:
        return
    need(reactor, f"guard {object_number} was not removed")
    actor = item["actor_state"]
    need(
        exact_number(actor["shields_raw"])
        and actor["shields_raw"] < 0
        and type(actor["flags"]) is int
        and actor["flags"] & 4 == 4,
        "persistent reactor is not destroyed",
    )


def original_operations(trace, indices):
    operations = [
        event for event in trace if event.get("event") == "successful_wall_operation"
    ]
    selected = {}
    for index in indices:
        matches = [event for event in operations if event.get("index") == index]
        need(len(matches) == 1, f"original operation {index} absent or duplicate")
        event = matches[0]
        state = event["state"]
        player = state["combat_observation"]["player_object"]
        trigger = state["xl_pilot"]["wall_triggers"][str(index)]
        need(
            type(player) is int
            and type(trigger["actor"]) is int
            and trigger["actor"] == player,
            f"operation {index} not player-owned",
        )
        need(
            type(event["sequence"]) is int and type(trigger["operated"]) is bool,
            f"operation {index} types invalid",
        )
        need(
            type(event["shot"]) is int
            and type(trigger["shot"]) is int
            and event["shot"] == trigger["shot"] == 0,
            f"operation {index} not collision-dispatched",
        )
        need(trigger["operated"] is True, f"trigger {index} not operated")
        selected[index] = event
    sequence = lambda index: selected[index]["sequence"]
    if 4 in selected:
        need(
            sequence(4)
            < sequence(20)
            < sequence(21)
            < sequence(22)
            < sequence(5)
            < sequence(6),
            "boost/master operation order invalid",
        )
        need(
            all(sequence(i) < sequence(i + 1) for i in range(6, 19)),
            "boost chain order invalid",
        )
        need(sequence(18) < sequence(19), "exit preceded boost 18")
        master = selected[5]["state"]
        need(
            master["reactor_countdown"]["total_seconds"] == 99,
            "master did not set 99 seconds",
        )
        exit_state = selected[19]["state"]
        need(
            exit_state["observation_boundary"] == "pre_exit"
            and exit_state["current_level_num"] == 14
            and exit_state["xl_pilot"]["remaining_objectives"] == 0,
            "missing original pre-exit state",
        )
    return selected


def fusion_witness(trace):
    before = state_at(trace, 6)
    charged = state_at(trace, 9)
    released = state_at(trace, 12)
    for state in (before, charged, released):
        need(
            state["player_dead"] is False and state["cheats_enabled"] is False,
            "Fusion pilot invalid",
        )
        need(state["player"]["primary_weapon_flags"] == 17, "Fusion inventory changed")
    need(
        type(before["primary_fire"]["held"]) is int
        and before["primary_fire"]["held"] == 0,
        "Fusion idle held type/value",
    )
    need(
        type(charged["primary_fire"]["held"]) is int
        and charged["primary_fire"]["held"] == 1,
        "Fusion held type/value",
    )
    need(
        type(released["primary_fire"]["held"]) is int
        and released["primary_fire"]["held"] == 0,
        "Fusion release held type/value",
    )
    need(before["primary_fire"]["fusion_charge_raw"] == 0, "Fusion started charged")
    need(charged["primary_fire"]["fusion_charge_raw"] > 0, "Fusion did not charge")
    need(released["primary_fire"]["fusion_charge_raw"] == 0, "Fusion did not release")
    need(
        before["player"]["energy"]
        > charged["player"]["energy"]
        > released["player"]["energy"]
        > 0,
        "Fusion energy economy absent",
    )
    before_owned = before["combat_observation"]["owned_weapons"]
    after_owned = released["combat_observation"]["owned_weapons"]
    need(
        not before_owned
        and not released["combat_observation"]["owned_weapons_truncated"],
        "Fusion projectile boundary invalid",
    )
    player = released["combat_observation"]
    witnesses = [weapon for weapon in after_owned if weapon["weapon_id"] == 14]
    need(len(witnesses) >= 1, "real Fusion projectile missing")
    need(
        len({(w["object"], w["signature"]) for w in witnesses}) == len(witnesses),
        "projectile identity duplicated",
    )
    for weapon in witnesses:
        need(
            weapon["parent_object"] == player["player_object"]
            and weapon["parent_signature"] == player["player_signature"]
            and weapon["parent_type"] == 4
            and before["game_time_raw"]
            <= weapon["creation_time_raw"]
            <= released["game_time_raw"]
            and weapon["lifeleft_raw"] > 0,
            "Fusion projectile is not a live player-owned shot from this run",
        )


def gold_witness(trace):
    before = state_at(trace, 0)
    after = state_at(trace, 5)
    need(
        before["current_level_num"] == after["current_level_num"] == 5,
        "gold level changed",
    )
    need(
        before["player"]["primary_weapon_flags"]
        == after["player"]["primary_weapon_flags"]
        == 17,
        "Fusion inventory lost",
    )
    need(
        before["player"]["has_gold_key"] is False
        and after["player"]["has_gold_key"] is True,
        "gold key not acquired",
    )
    need(
        before["xl_pilot"]["wall_triggers"]["17"]["operated"] is False,
        "gold trigger already operated",
    )
    need(
        after["xl_pilot"]["wall_triggers"]["17"]["operated"] is True,
        "gold trigger not operated",
    )
    need(before["xl_pilot"]["doorways"]["34"]["type"] == 5, "gold door already open")
    need(after["xl_pilot"]["doorways"]["34"]["type"] == 4, "gold door not open")
    key = lambda state: [
        item
        for item in state["level_scene"]["powerups"]
        if item["object"] == 28 and item["id"] == 6
    ]
    need(
        len(key(before)) == 1 and not key(after),
        "real original gold powerup not consumed",
    )
    need(
        after["player"]["shields"] > 0 and after["player_dead"] is False,
        "gold pilot dead",
    )
    original_operations(trace, [17])


def phase_files(owner, phase):
    root = owner / phase
    result_file = root / f"{phase}-result.json"
    plan_file = root / f"{phase}-executed-plan.json"
    state_file = root / f"{phase}-state.json"
    trace_file = root / f"{phase}-trace.jsonl"
    need(
        sha(result_file, MAX_RECEIPT_BYTES)
        == sha(root / "player-result.json", MAX_RECEIPT_BYTES),
        "stale copied result",
    )
    need(
        sha(state_file, MAX_STATE_BYTES)
        == sha(root / "player-state.json", MAX_STATE_BYTES),
        "stale copied state",
    )
    need(
        sha(trace_file, MAX_TRACE_BYTES)
        == sha(root / "player-trace.jsonl", MAX_TRACE_BYTES),
        "stale copied trace",
    )
    receipt = bounded_json(result_file, MAX_RECEIPT_BYTES)
    plan = bounded_json(plan_file, MAX_PLAN_BYTES)
    state = bounded_json(state_file, MAX_STATE_BYTES)
    trace = bounded_trace(trace_file)
    need(
        type(receipt) is dict and type(plan) is dict and type(state) is dict,
        "phase JSON object missing",
    )
    need(receipt["result"] == "PASS", f"{phase} phase failed")
    need(
        type(receipt["run_id"]) is str and HEX32.fullmatch(receipt["run_id"]),
        "native run ID invalid",
    )
    need(
        type(plan["receipt_run_id"]) is str
        and receipt["run_id"] == plan["receipt_run_id"],
        "native run nonce mismatch",
    )
    for field in ("plan_sha256", "resume_sha256", "executable_sha256"):
        need(
            type(receipt[field]) is str and HEX64.fullmatch(receipt[field]),
            f"{field} malformed",
        )
    need(
        receipt["plan_sha256"] == sha(plan_file, MAX_PLAN_BYTES),
        f"{phase} executed plan mismatch",
    )
    need(
        plan["resume_checkpoint"] == f"coldrom-{phase}-earned-entry.sav",
        "phase resumed an output-alias or wrong entry name",
    )
    entry = safe_checkpoint(root, plan["resume_checkpoint"])
    need(
        receipt["resume_sha256"] == plan["resume_sha256"] == sha(entry),
        f"{phase} input mismatch",
    )
    engine = Path(bounded_text(owner / "engine-path.txt", 4096).strip())
    need(
        receipt["executable_sha256"] == sha(engine, MAX_ENGINE_BYTES),
        "engine identity mismatch",
    )
    need(
        type(receipt["steps"]) is int and receipt["steps"] == len(plan["steps"]),
        "step count mismatch",
    )
    need(
        type(receipt["checkpoint_history"]) is list
        and len(receipt["checkpoint_history"]) <= 8,
        "checkpoint count invalid",
    )
    seen = set()
    for saved in receipt["checkpoint_history"]:
        need(
            type(saved["step"]) is int and 0 <= saved["step"] < receipt["steps"],
            "checkpoint producer step invalid",
        )
        path = safe_checkpoint(root, saved["file"])
        need(
            saved["file"] not in seen
            and type(saved["sha256"]) is str
            and HEX64.fullmatch(saved["sha256"])
            and saved["sha256"] == sha(path),
            "checkpoint bytes invalid",
        )
        seen.add(saved["file"])
    return receipt, plan, trace


TEMPLATE_ACTIONS = {
    "fusion": [
        "assert",
        "face_view",
        "assert",
        "select_primary",
        "assert",
        "wait_for",
        "assert",
        "send_button",
        "wait_ms",
        "assert",
        "send_button",
        "wait_for",
        "assert",
        "introspect",
    ],
    "main": [
        "assert",
        "wait_for",
        "face_view",
        "assert",
        "send_button",
        "wait_for",
        "wait_for",
        "send_button",
        "wait_ms",
        "assert",
        "assert",
        "set_debug",
        "introspect",
        "assert",
        "wait_for",
        "face_view",
        "assert",
        "send_button",
        "wait_for",
        "wait_for",
        "send_button",
        "wait_ms",
        "assert",
        "assert",
        "set_debug",
        "introspect",
        "assert",
        "wait_for",
        "pose_view",
        "assert",
        "send_button",
        "wait_ms",
        "wait_for",
        "send_button",
        "wait_ms",
        "assert",
        "set_debug",
        "introspect",
        "assert",
        "wait_for",
        "assert",
        "face_view",
        "assert",
        "send_axis",
        "wait_for",
        "wait_for",
        "wait_for",
        "wait_for",
        "wait_for",
        "wait_for",
        "assert",
        "wait_for",
        "assert",
        "set_debug",
        "introspect",
    ],
    "cold": [
        "assert",
        "wait_for",
        "assert",
        "face_view",
        "assert",
        "send_axis",
        "wait_for",
        "wait_for",
        "wait_for",
        "wait_for",
        "wait_for",
        "wait_for",
        "assert",
        "wait_for",
        "assert",
        "set_debug",
        "introspect",
    ],
    "gold": [
        "assert",
        "face_view",
        "send_axis",
        "wait_for",
        "send_axis",
        "assert",
        "set_debug",
        "introspect",
    ],
}


def template_witness(name, plan):
    actions = TEMPLATE_ACTIONS[name]
    need(
        type(plan) is dict
        and type(plan.get("steps")) is list
        and len(plan["steps"]) == len(actions),
        f"{name} template step count invalid",
    )
    need(
        type(plan.get("initial_level")) is int
        and plan["initial_level"] == (5 if name == "gold" else 14),
        f"{name} initial level invalid",
    )
    need(
        "resume_checkpoint" not in plan and "resume_sha256" not in plan,
        f"{name} template contains baked save",
    )
    expect = plan.get("checkpoint_expect")
    need(type(expect) is dict, f"{name} checkpoint_expect missing")
    need(
        expect.get("current_level_num") == plan["initial_level"]
        and expect.get("player.primary_weapon_flags") == 17,
        f"{name} entry state invalid",
    )
    need(type(expect.get("position.segment")) is int, f"{name} entry segment missing")
    if name in ("fusion", "main"):
        need(
            expect.get("position.segment") == 375
            and expect.get("player.primary_weapon") == 0
            and expect.get("player.energy") == 100
            and expect.get("xl_pilot.remaining_objectives") == 1,
            f"{name} pickup entry invalid",
        )
        need(
            expect.get("player_dead") is False
            and expect.get("cheats_enabled") is False
            and expect.get("player.invulnerable") is False,
            f"{name} entry safety invalid",
        )
    if name == "main":
        need(
            expect.get("player.shields") == 100
            and plan["steps"][28].get("heading") == 16384
            and plan["steps"][31].get("ms") == 1000,
            "main first-second reactor input invalid",
        )
    if name == "cold":
        need(
            expect.get("position.segment") == 698
            and expect.get("reactor_countdown.active") is True
            and expect.get("xl_pilot.remaining_objectives") == 0,
            "cold reactor entry invalid",
        )
    if name == "gold":
        need(
            expect.get("position.segment") == 260
            and expect.get("player.energy") == 100
            and expect.get("player.shields") == 100
            and expect.get("xl_pilot.remaining_objectives") == 1,
            "gold branch entry invalid",
        )
    need(
        [step.get("action") for step in plan["steps"]] == actions,
        f"{name} action shape invalid",
    )
    for step in plan["steps"]:
        action = step["action"]
        if action == "wait_for":
            need(
                type(step.get("timeout_ms")) is int
                and 0 < step["timeout_ms"] <= 18000
                and type(step.get("expect")) is dict,
                f"{name} wait bound invalid",
            )
        elif action == "wait_ms":
            need(
                type(step.get("ms")) is int and 0 < step["ms"] <= 1000,
                f"{name} wait_ms bound invalid",
            )
        elif action == "send_button":
            need(
                step.get("button") == 100
                and type(step.get("pressed")) is int
                and step["pressed"] in (0, 1),
                f"{name} fire input invalid",
            )
        elif action == "set_debug":
            need(
                step.get("field") == "android_game_request"
                and step.get("value") == "quick_save",
                f"{name} debug action invalid",
            )
    return len(actions)


def templates_witness(directory):
    counts = {}
    for name in TEMPLATE_ACTIONS:
        plan = bounded_json(directory / f"pumo_coldrom_{name}.json", MAX_PLAN_BYTES)
        counts[name] = template_witness(name, plan)
    return {"result": "WITNESSED", "phase": "templates", "counts": counts}


def producer_witness(producer_path, entry, engine):
    root = producer_path.parent
    need(
        producer_path.name == "coldrom-result.json",
        "wrong Coldrom producer result path",
    )
    need(
        entry.parent == root / "preview-user"
        and entry.name == "pumo-player-level14.sav",
        "wrong configured pickup entry path",
    )
    for member, limit in (
        ("result.json", MAX_RECEIPT_BYTES),
        ("state.json", MAX_STATE_BYTES),
        ("trace.jsonl", MAX_TRACE_BYTES),
    ):
        need(
            sha(root / f"coldrom-{member}", limit)
            == sha(root / f"player-{member}", limit),
            "stale Coldrom producer copy",
        )
    receipt = bounded_json(producer_path, MAX_RECEIPT_BYTES)
    plan_file = root / "coldrom-executed-plan.json"
    plan = bounded_json(plan_file, MAX_PLAN_BYTES)
    need(
        receipt["receipt_version"] == 2
        and receipt["result"] == "PASS"
        and receipt["steps"] == 15,
        "pickup producer result invalid",
    )
    need(
        receipt["initial_level_setup"] == receipt["final_level"] == 14
        and receipt["transitions"] == [],
        "pickup producer level invalid",
    )
    need(receipt["resumed_earned_checkpoint"] is True, "pickup was not earned-resumed")
    need(
        type(receipt["run_id"]) is str
        and HEX32.fullmatch(receipt["run_id"])
        and receipt["run_id"] == plan["receipt_run_id"],
        "pickup producer nonce invalid",
    )
    need(
        type(receipt["plan_sha256"]) is str
        and HEX64.fullmatch(receipt["plan_sha256"])
        and receipt["plan_sha256"] == sha(plan_file, MAX_PLAN_BYTES),
        "pickup producer plan invalid",
    )
    engine_hash = sha(engine, MAX_ENGINE_BYTES)
    need(receipt["executable_sha256"] == engine_hash, "pickup producer engine mismatch")
    need(
        plan["initial_level"] == 14 and len(plan["steps"]) == 15,
        "pickup executed actions invalid",
    )
    need(
        type(receipt["resume_sha256"]) is str
        and HEX64.fullmatch(receipt["resume_sha256"]),
        "pickup resume hash invalid",
    )
    need(
        plan["resume_sha256"]
        == receipt["resume_sha256"]
        == sha(safe_checkpoint(root, plan["resume_checkpoint"])),
        "pickup entry save invalid",
    )
    need(
        len(receipt["checkpoint_history"]) == 1
        and receipt["checkpoint_history"][0]["step"] == 10,
        "pickup checkpoint producer step invalid",
    )
    saved = receipt["checkpoint_history"][0]
    need(
        saved["sha256"]
        == sha(safe_checkpoint(root, saved["file"]))
        == receipt["checkpoints"]["14"]["sha256"]
        == sha(regular_file(entry, MAX_SAVE_BYTES)),
        "pickup checkpoint bytes invalid",
    )
    for state in (saved["state"], receipt["checkpoints"]["14"]["state"]):
        need(
            state["current_level_num"] == 14
            and state["player_dead"] is False
            and state["cheats_enabled"] is False,
            "pickup checkpoint state invalid",
        )
        need(
            state["player"]["invulnerable"] is False
            and state["player"]["primary_weapon_flags"] == 17,
            "pickup inventory/cheat state invalid",
        )

    alternate = bounded_json(root / "alternate-result.json", MAX_RECEIPT_BYTES)
    alternate_plan_file = root / "alternate-executed-plan.json"
    alternate_plan = bounded_json(alternate_plan_file, MAX_PLAN_BYTES)
    need(
        alternate["receipt_version"] == 2
        and alternate["result"] == "PASS"
        and alternate["steps"] == 14,
        "Metillis alternate result invalid",
    )
    need(
        alternate["initial_level_setup"] == 4 and alternate["final_level"] == 14,
        "Metillis alternate levels invalid",
    )
    need(
        len(alternate["transitions"]) == 1
        and alternate["transitions"][0]["from"] == 4
        and alternate["transitions"][0]["to"] == 14,
        "Metillis alternate transition invalid",
    )
    need(
        alternate["resumed_earned_checkpoint"] is True,
        "Metillis alternate not earned-resumed",
    )
    need(
        type(alternate["run_id"]) is str
        and HEX32.fullmatch(alternate["run_id"])
        and alternate["run_id"] == alternate_plan["receipt_run_id"]
        and alternate["run_id"] != receipt["run_id"],
        "Metillis alternate nonce invalid",
    )
    need(
        alternate["plan_sha256"] == sha(alternate_plan_file, MAX_PLAN_BYTES)
        and alternate["executable_sha256"] == engine_hash,
        "Metillis alternate source invalid",
    )
    need(
        len(alternate["checkpoint_history"]) == 1
        and alternate["checkpoint_history"][0]["step"] == 9,
        "Metillis alternate checkpoint producer invalid",
    )
    need(
        alternate["checkpoints"]["14"]["sha256"]
        == receipt["resume_sha256"]
        == alternate["checkpoint_history"][0]["sha256"]
        == sha(safe_checkpoint(root, alternate["checkpoint_history"][0]["file"])),
        "Metillis alternate-to-pickup chain invalid",
    )
    need(
        alternate["resume_sha256"]
        == alternate_plan["resume_sha256"]
        == sha(safe_checkpoint(root, alternate_plan["resume_checkpoint"])),
        "Metillis alternate input save invalid",
    )
    main = bounded_json(root / "main-result.json", MAX_RECEIPT_BYTES)
    need(
        main["result"] == "PASS"
        and main["checkpoints"]["4"]["sha256"] == alternate["resume_sha256"],
        "Metillis main-to-alternate chain invalid",
    )
    return {
        "result": "WITNESSED",
        "phase": "producer",
        "run_id": receipt["run_id"],
        "entry_sha256": sha(entry),
    }


PRESERVED_PLAYER_FIELDS = (
    "shields",
    "energy",
    "primary_weapon",
    "primary_weapon_flags",
    "vulcan_ammo",
    "has_blue_key",
    "has_red_key",
    "has_gold_key",
    "cloaked",
    "invulnerable",
    "lives",
    "score",
    "robots_killed_level",
)
PRESERVED_COUNTDOWN_FIELDS = ("active", "total_seconds", "timer")


def preserved_entry(saved, restored):
    need(
        saved["current_level_num"] == restored["current_level_num"],
        "resumed level changed",
    )
    for field in PRESERVED_PLAYER_FIELDS:
        need(
            type(saved["player"][field]) is type(restored["player"][field])
            and saved["player"][field] == restored["player"][field],
            f"resumed player {field} changed",
        )
    for field in PRESERVED_COUNTDOWN_FIELDS:
        need(
            type(saved["reactor_countdown"][field])
            is type(restored["reactor_countdown"][field])
            and saved["reactor_countdown"][field]
            == restored["reactor_countdown"][field],
            f"resumed countdown {field} changed",
        )
    for state in (saved, restored):
        need(
            state["simulation_rng"]["state_available"] is True
            and type(state["simulation_rng"]["state"]) is int,
            "resumed RNG unavailable",
        )
    need(
        saved["simulation_rng"]["state"] == restored["simulation_rng"]["state"],
        "resumed RNG state changed",
    )
    # The process-local RNG call counter legitimately starts again after load.


def route_witness(receipt, trace, main=False):
    indices = [4, 5, *range(6, 23)]
    operations = original_operations(trace, indices)
    need(
        len(receipt["transitions"]) == 1
        and receipt["transitions"][0]["from"] == 14
        and receipt["transitions"][0]["to"] == 5,
        "not a normal 14-to-5 transition",
    )
    need(operations[19]["state"]["player_dead"] is False, "exit pilot dead")


def main_witness(receipt, trace):
    signatures = {}
    initial = state_at(trace, 0)
    for number in (103, 104, 125):
        item = [
            item
            for item in initial["combat_observation"]["tracked_objects"]
            if item["object"] == number
        ]
        need(
            len(item) == 1 and item[0]["exists"] is True,
            f"initial actor {number} not bound",
        )
        signatures[number] = item[0]["expected_signature"]
    need(initial["player"]["primary_weapon_flags"] == 17, "main not Fusion branch")
    initial_generation = initial["combat_observation"]["load_generation"]
    for number, signature in signatures.items():
        bound(initial, number, signature, initial_generation)
    for start, number in ((0, 103), (13, 104)):
        pre = state_at(trace, start + 3)
        damage = state_at(trace, start + 6)
        final = state_at(trace, start + 9)
        decreased(pre, damage, number, signatures[number], initial_generation)
        destroyed(final, number, signatures[number], generation=initial_generation)
        need(final["player"]["shields"] > 0, f"guard {number} pilot dead")
    pre = state_at(trace, 29)
    after_second = state_at(trace, 32)
    final = state_at(trace, 35)
    need(
        type(pre["primary_fire"]["held"]) is int
        and type(after_second["primary_fire"]["held"]) is int
        and pre["primary_fire"]["held"] == 0
        and after_second["primary_fire"]["held"] == 1,
        "reactor normal held-fire window missing",
    )
    need(
        pre["game_time_raw"] < after_second["game_time_raw"],
        "reactor first-second interval absent",
    )
    need(
        pre["player"]["energy"] > after_second["player"]["energy"] > 0,
        "reactor normal-fire energy use absent",
    )
    # Other in-flight player shots may exist; this is a player-only reactor damage witness, not per-shot attribution.
    decreased(pre, after_second, 125, signatures[125], initial_generation)
    destroyed(final, 125, signatures[125], reactor=True, generation=initial_generation)
    need(
        final["xl_pilot"]["remaining_objectives"] == 0
        and final["reactor_countdown"]["active"] is True,
        "reactor objective/countdown absent",
    )
    need(final["reactor_countdown"]["total_seconds"] > 0, "reactor countdown invalid")
    need(
        final["player"]["shields"] > 0 and final["player"]["energy"] > 0,
        "reactor pilot resources invalid",
    )
    need(len(receipt["checkpoint_history"]) == 4, "main checkpoint count invalid")
    need(
        [saved["step"] for saved in receipt["checkpoint_history"]] == [11, 24, 36, 53],
        "main checkpoint steps invalid",
    )
    reactor_saved = receipt["checkpoint_history"][2]["state"]
    need(
        reactor_saved["xl_pilot"]["remaining_objectives"] == 0
        and reactor_saved["reactor_countdown"]["active"] is True,
        "saved reactor state invalid",
    )
    route_witness(receipt, trace, main=True)
    need(
        receipt["checkpoint_history"][3]["state"]["player"]["primary_weapon_flags"]
        == 17,
        "warm destination lost Fusion",
    )


def verify(owner, phase):
    receipt, _plan, trace = phase_files(owner, phase)
    expected = {
        "fusion": (14, 14, 0),
        "main": (55, 5, 1),
        "cold": (17, 5, 1),
        "gold": (8, 5, 0),
    }[phase]
    need(
        (receipt["steps"], receipt["final_level"], len(receipt["transitions"]))
        == expected,
        "phase contract changed",
    )
    if phase == "fusion":
        fusion_witness(trace)
    elif phase == "main":
        main_witness(receipt, trace)
    elif phase == "cold":
        route_witness(receipt, trace)
        main_receipt = bounded_json(
            owner / "main" / "main-result.json", MAX_RECEIPT_BYTES
        )
        need(
            receipt["resume_sha256"] == main_receipt["checkpoint_history"][2]["sha256"],
            "cold input not main reactor save",
        )
        preserved_entry(
            main_receipt["checkpoint_history"][2]["state"], state_at(trace, 0)
        )
        need(
            receipt["run_id"] != main_receipt["run_id"],
            "cold not fresh process",
        )
    else:
        gold_witness(trace)
        main_receipt = bounded_json(
            owner / "main" / "main-result.json", MAX_RECEIPT_BYTES
        )
        need(
            receipt["resume_sha256"] == main_receipt["checkpoint_history"][3]["sha256"],
            "gold input not warm main destination",
        )
        preserved_entry(
            main_receipt["checkpoint_history"][3]["state"], state_at(trace, 0)
        )
    return {
        "result": "WITNESSED",
        "phase": phase,
        "run_id": receipt["run_id"],
        "plan_sha256": receipt["plan_sha256"],
        "resume_sha256": receipt["resume_sha256"],
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--phase",
        choices=("templates", "producer", "fusion", "main", "cold", "gold"),
        required=True,
    )
    parser.add_argument("--owner", type=Path)
    parser.add_argument("--producer", type=Path)
    parser.add_argument("--entry", type=Path)
    parser.add_argument("--engine", type=Path)
    parser.add_argument("--templates", type=Path)
    args = parser.parse_args()
    if args.phase == "templates":
        need(args.templates is not None, "templates directory missing")
        result = templates_witness(args.templates)
    elif args.phase == "producer":
        need(
            args.producer is not None
            and args.entry is not None
            and args.engine is not None,
            "producer arguments missing",
        )
        result = producer_witness(args.producer, args.entry, args.engine)
    else:
        need(args.owner is not None, "phase owner missing")
        result = verify(args.owner, args.phase)
    print(json.dumps(result, sort_keys=True))
