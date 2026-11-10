"""Asset-free positive and forged-receipt controls for the spawn owner."""

import copy
import json
import tempfile
from pathlib import Path

import pumo_tulev1_spawn_contract as c


def put(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value) + "\n")


def state(segment):
    return {
        "current_level_num": 7,
        "player_dead": False,
        "cheats_enabled": False,
        "game_time_raw": 100,
        "simulation_rng": {"state_available": True, "state": 42, "call_count": 0},
        "position": {"segment": segment},
        "player": {
            "invulnerable": False,
            "shields": 100,
            "energy": 100,
            "has_blue_key": False,
            "has_red_key": False,
            "has_gold_key": False,
            "cloaked": False,
            "laser_level": 0,
            "quad_lasers": False,
            "primary_weapon_flags": 65,
            "primary_weapon": 0,
            "secondary_weapon": 0,
            "secondary_weapon_flags": 1,
            "secondary_ammo": [7, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            "vulcan_ammo": 0,
        },
        "xl_pilot": {"doorways": {}},
        "level_scene": {"powerups": []},
    }


def check_reject(fn, text):
    try:
        fn()
    except (ValueError, KeyError, IndexError, TypeError):
        return
    raise AssertionError(f"accepted forgery: {text}")


def test():
    with tempfile.TemporaryDirectory() as temporary:
        base = Path(temporary)
        engine = base / "engine"
        engine.write_bytes(b"asset-free engine identity")
        c.ENGINE_SHA = c.sha(engine)
        source = base / "sophodo"
        entry = source / "preview-user" / "sophodo-main-level7.sav"
        entry.parent.mkdir(parents=True)
        entry.write_bytes(b"public synthetic warm save")
        plan_path = source / "main-executed-plan.json"
        put(plan_path, {"receipt_run_id": "a" * 32})
        warm_state = state(1183)
        for index in (127, 126, 211, 210, 138, 139):
            warm_state["xl_pilot"]["doorways"][str(index)] = {
                "type": 2,
                "keys": 1,
                "flags": 16,
                "state": 0,
                "clip": 30,
            }
        versioned = source / "preview-user" / "pumo-player-level7-123-step139.sav"
        versioned.write_bytes(entry.read_bytes())
        alias = source / "preview-user" / "pumo-player-level7.sav"
        alias.write_bytes(entry.read_bytes())
        warm = {
            "step": 139,
            "file": versioned.name,
            "sha256": c.sha(entry),
            "state": warm_state,
        }
        producer = {
            "receipt_version": 2,
            "result": "PASS",
            "steps": 144,
            "initial_level_setup": 6,
            "final_level": 7,
            "transitions": [{"from": 6, "to": 7}],
            "resumed_earned_checkpoint": True,
            "run_id": "a" * 32,
            "plan_sha256": c.sha(plan_path),
            "executable_sha256": c.sha(engine),
            "checkpoint_history": [warm],
            "checkpoints": {"7": {"file": alias.name, "sha256": c.sha(entry)}},
        }
        put(source / "main-result.json", producer)
        c.source(source / "main-result.json", entry, engine)
        altered = copy.deepcopy(producer)
        altered["checkpoint_history"][-1]["state"]["position"]["segment"] = 458
        put(source / "main-result.json", altered)
        check_reject(
            lambda: c.source(source / "main-result.json", entry, engine),
            "old assisted segment",
        )
        put(source / "main-result.json", producer)
        phase = base / "door127"
        input_save = phase / "preview-user" / "input.sav"
        input_save.parent.mkdir(parents=True)
        input_save.write_bytes(entry.read_bytes())
        out_save = phase / "preview-user" / "pumo-player-level7-123-step7.sav"
        out_save.write_bytes(b"public synthetic successor")
        (phase / "preview-user" / "pumo-player-level7.sav").write_bytes(
            out_save.read_bytes()
        )
        staged, crossed = state(1200), state(1150)
        for index in ("127", "126"):
            staged["xl_pilot"]["doorways"][index] = {
                "type": 2,
                "keys": 1,
                "flags": 16,
                "state": 0,
                "clip": 30,
            }
            crossed["xl_pilot"]["doorways"][index] = {
                "type": 2,
                "keys": 1,
                "flags": 18,
                "state": 2,
                "clip": 30,
            }
        states = [
            state(1183),
            state(1183),
            staged,
            staged,
            staged,
            crossed,
            crossed,
            crossed,
            crossed,
        ]
        actions = list(c.ACTIONS)
        plan = c.doc(Path(c.__file__).with_name("pumo_tulev1_spawn_door127.json"))
        plan.update(
            receipt_run_id="b" * 32,
            resume_checkpoint=input_save.name,
            resume_sha256=c.sha(input_save),
        )
        executed = phase / "door127-executed-plan.json"
        put(executed, plan)
        receipt = {
            "receipt_version": 2,
            "result": "PASS",
            "steps": 9,
            "initial_level_setup": 7,
            "final_level": 7,
            "resumed_earned_checkpoint": True,
            "transitions": [],
            "process_id": 123,
            "run_id": "b" * 32,
            "plan_sha256": c.sha(executed),
            "executable_sha256": c.sha(engine),
            "resume_sha256": c.sha(input_save),
            "position_assist_events": 1,
            "fixed_view_aim_events": 0,
            "checkpoint_history": [
                {
                    "step": 7,
                    "file": out_save.name,
                    "sha256": c.sha(out_save),
                    "state": crossed,
                }
            ],
            "checkpoints": {
                "7": {"file": "pumo-player-level7.sav", "sha256": c.sha(out_save)}
            },
        }
        result_path = phase / "door127-result.json"
        put(result_path, receipt)
        put(phase / "door127-state.json", crossed)
        events = [
            {"step": index, "action": action, "state": states[index]}
            for index, action in enumerate(actions)
        ]
        events.insert(2, {"event": "position_assist", "step": 1, "action": "face_view"})
        trace = phase / "door127-trace.jsonl"
        trace.write_text("".join(json.dumps(e) + "\n" for e in events))
        c.phase("door127", phase, engine, source / "main-result.json")
        bad = copy.deepcopy(receipt)
        bad["resume_sha256"] = "0" * 64
        put(result_path, bad)
        check_reject(
            lambda: c.phase("door127", phase, engine, source / "main-result.json"),
            "forged predecessor",
        )
        bad_plan = copy.deepcopy(plan)
        bad_plan["steps"][4]["timeout_ms"] = 99999
        put(executed, bad_plan)
        bad = copy.deepcopy(receipt)
        bad["plan_sha256"] = c.sha(executed)
        put(result_path, bad)
        check_reject(
            lambda: c.phase("door127", phase, engine, source / "main-result.json"),
            "weakened timing",
        )
        put(executed, plan)
        put(result_path, receipt)
        bad_events = copy.deepcopy(events)
        bad_events[7]["state"]["player"]["shields"] = 101
        trace.write_text("".join(json.dumps(e) + "\n" for e in bad_events))
        check_reject(
            lambda: c.phase("door127", phase, engine, source / "main-result.json"),
            "forged health increase",
        )
        bad_events = copy.deepcopy(events)
        bad_events[3]["state"]["position"]["segment"] = 1200.0
        trace.write_text("".join(json.dumps(e) + "\n" for e in bad_events))
        check_reject(
            lambda: c.phase("door127", phase, engine, source / "main-result.json"),
            "float segment",
        )
        bad_events = copy.deepcopy(events)
        bad_events[0]["state"]["simulation_rng"]["state"] = 999
        trace.write_text("".join(json.dumps(e) + "\n" for e in bad_events))
        check_reject(
            lambda: c.phase("door127", phase, engine, source / "main-result.json"),
            "forged restored RNG",
        )
        put(result_path, receipt)
        bad_events = copy.deepcopy(events)
        bad_events[7]["state"]["xl_pilot"]["doorways"]["127"]["flags"] = 16
        trace.write_text("".join(json.dumps(e) + "\n" for e in bad_events))
        check_reject(
            lambda: c.phase("door127", phase, engine, source / "main-result.json"),
            "not opened",
        )
        bad_events = copy.deepcopy(events)
        bad_events[7]["state"]["position"]["segment"] = 1200
        trace.write_text("".join(json.dumps(e) + "\n" for e in bad_events))
        check_reject(
            lambda: c.phase("door127", phase, engine, source / "main-result.json"),
            "no physical crossing",
        )


if __name__ == "__main__":
    test()
    print("spawn contract: asset-free positive and forgery controls passed")
