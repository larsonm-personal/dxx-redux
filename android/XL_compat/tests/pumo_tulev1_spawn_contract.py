"""Asset-free, source-bound checks for the bounded original-spawn Tulev1 route."""

import hashlib
import json
import math
import re
import stat
import sys
from itertools import pairwise
from pathlib import Path

ENGINE_SHA = "dece6cf76b0f5608bb586ee3143426fb3c0b909b86a73be85effc7a0a5d1d5db"
ORDER = ("door127", "door211", "guided137", "door138", "load")
GATES = {
    "door127": (1183, 1200, 1150, 127, 126),
    "door211": (1150, 1160, 1181, 211, 210),
    "door138": (1277, 1210, 689, 138, 139),
}
ACTIONS = (
    "assert",
    "face_view",
    "assert",
    "send_axis",
    "wait_for",
    "send_axis",
    "assert",
    "set_debug",
    "introspect",
)


def need(value, message):
    if not value:
        raise ValueError(message)


def read(path, limit=16 << 20):
    info = path.lstat()
    need(
        stat.S_ISREG(info.st_mode) and 0 < info.st_size <= limit,
        f"unsafe or empty {path.name}",
    )
    return path.read_bytes()


def sha(path):
    return hashlib.sha256(read(path, 64 << 20)).hexdigest()


def doc(path):
    return json.loads(read(path))


def save(root, name):
    need(
        type(name) is str and re.fullmatch(r"[A-Za-z0-9_.-]+\.sav", name),
        "unsafe save name",
    )
    return root / "preview-user" / name


def number(value):
    return type(value) in (int, float) and math.isfinite(value)


def integer(value):
    return type(value) is int and value >= 0


def live(s):
    p = s["player"]
    need(
        type(s["current_level_num"]) is int
        and s["current_level_num"] == 7
        and s["player_dead"] is False
        and s["cheats_enabled"] is False
        and p["invulnerable"] is False
        and number(p["shields"])
        and number(p["energy"])
        and 0 < p["shields"] <= 200
        and 0 < p["energy"] <= 200,
        "not live at level 7",
    )
    need(
        p["has_blue_key"] is False
        and p["has_red_key"] is False
        and p["has_gold_key"] is False
        and p["cloaked"] is False
        and type(p["laser_level"]) is int
        and p["laser_level"] == 0
        and p["quad_lasers"] is False
        and type(p["primary_weapon_flags"]) is int
        and p["primary_weapon_flags"] == 65
        and type(p["secondary_weapon_flags"]) is int
        and all(integer(x) for x in p["secondary_ammo"])
        and integer(p["vulcan_ammo"]),
        "unauthorized original capability",
    )
    return p


def inventory(p):
    return (p["secondary_ammo"], p["secondary_weapon_flags"], p["vulcan_ammo"])


def source(path, entry, engine):
    root = path.parent
    need(
        path.name == "main-result.json"
        and entry == save(root, "sophodo-main-level7.sav"),
        "wrong warm source",
    )
    r, plan = doc(path), doc(root / "main-executed-plan.json")
    need(
        type(r["receipt_version"]) is int
        and type(r["steps"]) is int
        and type(r["initial_level_setup"]) is int
        and type(r["final_level"]) is int,
        "receipt numeric schema invalid",
    )
    need(
        r["receipt_version"] == 2
        and r["result"] == "PASS"
        and r["steps"] == 144
        and r["initial_level_setup"] == 6
        and r["final_level"] == 7
        and len(r["transitions"]) == 1
        and r["transitions"][0]["from"] == 6
        and r["transitions"][0]["to"] == 7
        and r["resumed_earned_checkpoint"] is True,
        "not actual Sophodo transition",
    )
    need(
        all(type(r["transitions"][0][field]) is int for field in ("from", "to")),
        "source transition type invalid",
    )
    warm = r["checkpoint_history"][-1]
    need(type(warm["step"]) is int, "warm step type invalid")
    need(
        warm["step"] == 139
        and warm["sha256"] == sha(entry)
        and "7" in r["checkpoints"]
        and r["checkpoints"]["7"]["sha256"] == warm["sha256"]
        and warm["file"] != r["checkpoints"]["7"]["file"]
        and re.fullmatch(r"pumo-player-level7-[0-9]+-step139\.sav", warm["file"])
        and warm["sha256"] == sha(save(root, warm["file"]))
        and r["checkpoints"]["7"]["file"] == "pumo-player-level7.sav"
        and r["run_id"] == plan["receipt_run_id"]
        and r["plan_sha256"] == sha(root / "main-executed-plan.json")
        and r["executable_sha256"] == sha(engine) == ENGINE_SHA,
        "warm provenance mismatch",
    )
    p = live(warm["state"])
    for index in (127, 126, 211, 210, 138, 139):
        wall = warm["state"]["xl_pilot"]["doorways"][str(index)]
        need(
            all(
                type(wall[field]) is int
                for field in ("type", "keys", "flags", "state", "clip")
            ),
            "source door type invalid",
        )
        need(
            (wall["type"], wall["keys"], wall["flags"], wall["state"], wall["clip"])
            == (2, 1, 16, 0, 30),
            f"wrong source AUTO door {index}",
        )
    need(
        type(warm["state"]["position"]["segment"]) is int
        and warm["state"]["position"]["segment"] == 1183
        and p["secondary_ammo"][6] == 0
        and p["secondary_weapon_flags"] == 1
        and p["shields"] == p["energy"] == 100,
        "wrong original spawn state",
    )
    return warm


def events_at(root, name):
    events = [
        json.loads(line) for line in read(root / f"{name}-trace.jsonl").splitlines()
    ]
    steps = [e for e in events if "step" in e and "state" in e]
    return events, steps


def phase(name, root, engine, producer):
    need(name in ORDER, "unknown phase")
    r = doc(root / f"{name}-result.json")
    plan_path = root / f"{name}-executed-plan.json"
    plan = doc(plan_path)
    final = doc(root / f"{name}-state.json")
    events, steps = events_at(root, name)
    count = 1 if name == "load" else 9
    need(
        type(r["receipt_version"]) is int
        and type(r["steps"]) is int
        and type(r["initial_level_setup"]) is int
        and type(r["final_level"]) is int,
        "receipt numeric schema invalid",
    )
    need(
        r["receipt_version"] == 2
        and r["result"] == "PASS"
        and r["steps"] == count
        and r["initial_level_setup"] == r["final_level"] == 7
        and r["resumed_earned_checkpoint"] is True
        and r["transitions"] == []
        and type(r["process_id"]) is int
        and r["process_id"] > 0,
        "phase result invalid",
    )
    need(
        re.fullmatch("[0-9a-f]{32}", r["run_id"])
        and r["run_id"] == plan["receipt_run_id"]
        and r["plan_sha256"] == sha(plan_path)
        and r["executable_sha256"] == sha(engine) == ENGINE_SHA
        and r["resume_sha256"]
        == plan["resume_sha256"]
        == sha(save(root, plan["resume_checkpoint"])),
        "phase input identity invalid",
    )
    template = doc(Path(__file__).with_name(f"pumo_tulev1_spawn_{name}.json"))
    expected_plan = dict(template)
    expected_plan.update(
        resume_checkpoint=plan["resume_checkpoint"],
        resume_sha256=plan["resume_sha256"],
        receipt_run_id=plan["receipt_run_id"],
    )
    need(plan == expected_plan, "executed plan differs from checked-in template")
    need(all(type(e["step"]) is int for e in steps), "trace step type invalid")
    need(
        [e["step"] for e in steps] == list(range(count))
        and [e["action"] for e in steps]
        == ([ACTIONS[i] for i in range(count)] if count == 9 else ["introspect"])
        and [x["action"] for x in plan["steps"]] == [e["action"] for e in steps],
        "native PRE-action indexing or plan changed",
    )
    need(
        type(r["position_assist_events"]) is int
        and type(r["fixed_view_aim_events"]) is int,
        "assist counter type invalid",
    )
    initial = steps[0]["state"]
    need(final == steps[-1]["state"], "final introspection differs")
    prior_name = ORDER[ORDER.index(name) - 1] if name != ORDER[0] else None
    prior = (
        doc(root.parent / prior_name / f"{prior_name}-result.json")[
            "checkpoint_history"
        ][0]
        if prior_name
        else source(
            producer,
            producer.parent / "preview-user" / "sophodo-main-level7.sav",
            engine,
        )
    )
    need(r["resume_sha256"] == prior["sha256"], "not immediate saved predecessor")
    pp, ip, fp = live(prior["state"]), live(initial), live(final)
    need(
        all(
            type(x) is int
            for x in (
                initial["game_time_raw"],
                prior["state"]["game_time_raw"],
                initial["simulation_rng"]["state"],
                prior["state"]["simulation_rng"]["state"],
                initial["simulation_rng"]["call_count"],
            )
        ),
        "restored clock or RNG type invalid",
    )
    need(
        initial["game_time_raw"] == prior["state"]["game_time_raw"]
        and initial["simulation_rng"]["state_available"] is True
        and initial["simulation_rng"]["state"]
        == prior["state"]["simulation_rng"]["state"]
        and initial["simulation_rng"]["call_count"] == 0
        and ip["shields"] == pp["shields"]
        and ip["energy"] == pp["energy"]
        and initial["position"] == prior["state"]["position"]
        and inventory(ip) == inventory(pp)
        and ip["primary_weapon"] == pp["primary_weapon"]
        and ip["secondary_weapon"] == pp["secondary_weapon"]
        and ip["primary_weapon_flags"] == pp["primary_weapon_flags"]
        and ip == pp,
        "loaded predecessor differs",
    )
    assist = [e for e in events if e.get("event") == "position_assist"]
    need(
        r["position_assist_events"] == len(assist) == (0 if name == "load" else 1),
        "assist count invalid",
    )
    if name == "load":
        need(
            plan["steps"] == [{"action": "introspect"}]
            and len(events) == 1
            and r["checkpoint_history"] == []
            and r["checkpoints"] == {}
            and r["fixed_view_aim_events"] == 0,
            "cold load not read-only",
        )
        for field in ("game_time_raw", "position"):
            need(final[field] == prior["state"][field], f"cold {field} mismatch")
        need(
            final["simulation_rng"]["state_available"] is True
            and final["simulation_rng"]["state"]
            == prior["state"]["simulation_rng"]["state"]
            and final["simulation_rng"]["call_count"] == 0
            and final["player"] == prior["state"]["player"],
            "cold state mismatch",
        )
        return
    need(
        r["fixed_view_aim_events"] == 0
        and type(assist[0]["step"]) is int
        and assist[0]["step"] == 1
        and assist[0]["action"] == "face_view"
        and plan["steps"][3]["axis"] == plan["steps"][5]["axis"] == 1
        and plan["steps"][3]["axis_value"] == -0.2
        and plan["steps"][5]["axis_value"] == 0
        and plan["steps"][7]
        == {
            "action": "set_debug",
            "field": "android_game_request",
            "value": "quick_save",
        },
        "ordinary action sequence changed",
    )
    h = r["checkpoint_history"]
    need(len(h) == 1 and type(h[0]["step"]) is int, "save step type invalid")
    need(
        len(h) == 1
        and h[0]["step"] == 7
        and re.fullmatch(r"pumo-player-level7-[0-9]+-step7\.sav", h[0]["file"])
        and h[0]["file"] != plan["resume_checkpoint"]
        and h[0]["sha256"] == sha(save(root, h[0]["file"]))
        and set(r["checkpoints"]) == {"7"}
        and r["checkpoints"]["7"]["file"] == "pumo-player-level7.sav"
        and r["checkpoints"]["7"]["sha256"] == h[0]["sha256"]
        and sha(save(root, "pumo-player-level7.sav")) == h[0]["sha256"]
        and h[0]["state"] == final,
        "unique immutable checkpoint missing",
    )
    staged, crossed = steps[2]["state"], steps[6]["state"]
    sp, cp = live(staged), live(crossed)
    resources = [ip, sp, cp, fp]
    need(
        inventory(sp) == inventory(ip) and inventory(cp) == inventory(fp),
        "unexpected inventory change before or after contact",
    )
    need(
        all(
            b["shields"] <= a["shields"] and b["energy"] <= a["energy"]
            for a, b in pairwise(resources)
        ),
        "unexplained shield or energy increase",
    )
    need(
        all(
            type(s["position"]["segment"]) is int
            for s in (initial, staged, crossed, final)
        ),
        "segment type invalid",
    )
    if name in GATES:
        origin, pose, target, wall, back = GATES[name]
        side = {"door127": 2, "door211": 2, "door138": 0}[name]
        need(
            initial["position"]["segment"] == origin
            and plan["steps"][1]["segment"] == pose
            and plan["steps"][1]["side"] == side
            and plan["steps"][1]["distance"] == 8
            and staged["position"]["segment"] == pose
            and crossed["position"]["segment"] == target
            and final["position"]["segment"] == target,
            "gate geometry or crossing invalid",
        )
        for index in (wall, back):
            w0 = staged["xl_pilot"]["doorways"][str(index)]
            w1 = crossed["xl_pilot"]["doorways"][str(index)]
            need(
                all(
                    type(w[field]) is int
                    for w in (w0, w1)
                    for field in ("type", "keys", "flags", "state")
                ),
                "door numeric schema invalid",
            )
            need(
                (w0["type"], w0["keys"], w0["flags"], w0["state"]) == (2, 1, 16, 0)
                and w1["type"] == 2
                and w1["keys"] == 1
                and w1["flags"] == 18
                and w1["state"] == 2,
                f"original AUTO door {index} not opened and crossed",
            )
        need(inventory(fp) == inventory(ip), "gate changed secondary inventory")
    else:
        need(
            initial["position"]["segment"] == 1181
            and plan["steps"][1]["segment"] == 1276
            and plan["steps"][1]["side"] == 0
            and staged["position"]["segment"] == 1276
            and crossed["position"]["segment"] == 1277
            and final["position"]["segment"] == 1277,
            "pickup setup changed",
        )

        def present(s):
            return [p for p in s["level_scene"]["powerups"] if p["object"] == 137]

        need(
            len(present(staged)) == 1
            and present(staged)[0]["id"] == 40
            and present(staged)[0]["segment"] == 1277
            and not present(crossed)
            and not present(final),
            "original Guided137 not collected",
        )
        before, after = ip["secondary_ammo"], fp["secondary_ammo"]
        need(
            len(before) == len(after) == 10
            and before[6] == 0
            and after[6] == 1
            and all(after[i] == before[i] for i in range(10) if i != 6)
            and fp["secondary_weapon_flags"] == ip["secondary_weapon_flags"] | 64
            and fp["vulcan_ammo"] == ip["vulcan_ammo"]
            and fp["primary_weapon"] == ip["primary_weapon"]
            and fp["secondary_weapon"] == ip["secondary_weapon"],
            "Guided slot 6 gain not exact",
        )


def main():
    if sys.argv[1] == "producer":
        source(*(Path(p) for p in sys.argv[2:5]))
    elif sys.argv[1] == "phase":
        phase(sys.argv[2], *(Path(p) for p in sys.argv[3:6]))
    else:
        raise ValueError("mode")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, IndexError, TypeError, OSError) as error:
        sys.exit(str(error))
