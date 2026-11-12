"""Supplemental assisted render-clock qualification; never a readiness result."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time

import d1_replay_parity as parity

REVISION = "2576a98b5de310e3ac7c9777df2677d1627b8bae"
PINS = {
    "render/renderframe.cpp": {
        "56f059e9f02903cece0da2b95acbb2469bd65c9046d0a8539ee0708c632e3bbf",
        "9d09dbc8597c8c02ed738c73ba89f964bfcede1730eef54feb10f20a66f5e7c1",
    },
    "render/automap.cpp": {
        "19b7128b48dadea220a085018e0fd3a2ff31d59fa04d60fce78d4bb1f5877be8",
        "4b00b69943bc6dba010d84f280da266afc68cfccde104ff724b5c7c4eb0b6d1b",
    },
    "weapons/omega.cpp": {
        "76f003622dcbac54dbd533fd4dfa8537c7ceceed0c75c5310c83b7b075cfb61d",
        "40cfe360e3ced7b5a5625009c419d1a2f4ac4b69d99ef457c68d1bb5bec6743c",
    },
    "gameio/loadgame.cpp": {
        "5d7ff36d6121411e0250be94353bbe3268bdd02b9bd35c6161951d879fcecc55",
        "4b9cb1f149c9c64929f1cfef7183ad9ff541604a79331454c3205ec89c477f68",
    },
}
# Source trace: renderframe.cpp:348/735; automap.cpp:504; loadgame.cpp:913
# An end-level inner RenderFrame contributes in addition to GameRenderFrame.
EVENTS = (
    ("simulation_only", 0),
    ("main", 1),
    ("auxiliary_mono", 0),
    ("automap", 1),
    ("endlevel_main", 2),
    ("endlevel_auxiliary", 1),
    ("simulation_after_render", 0),
    ("main_again", 1),
    ("restored", 0),
    ("main_after_restore", 1),
)


def verify_reference(root):
    found = {}
    for name, allowed in PINS.items():
        actual = hashlib.sha256((root / name).read_bytes()).hexdigest()
        if actual not in allowed:
            raise ValueError(f"Render-clock reference differs: {name}")
        found[name] = actual
    return found


def passed(code, proof, mission, level, xl):
    keys = {
        "schema",
        "assisted",
        "complete",
        "events",
        "renderer",
        "xl_active",
        "mission",
        "level",
        "reset_count",
        "shot_charge",
        "shot_delay",
    }
    if (
        type(code) is not int
        or code != 0
        or not isinstance(proof, dict)
        or set(proof) != keys
        or proof["schema"] != "xl-render-clock-v1"
        or proof["assisted"] is not True
        or proof["complete"] is not True
        or proof["xl_active"] is not xl
        or proof["mission"] != mission
        or type(proof["level"]) is not int
        or proof["level"] != level
        or not isinstance(proof["renderer"], str)
        or not proof["renderer"].strip()
        or type(proof["reset_count"]) is not int
        or proof["reset_count"] != 0
        or type(proof["shot_charge"]) is not int
        or proof["shot_charge"] != 32768 - (1092 if xl else 3276)
        or type(proof["shot_delay"]) is not int
        or proof["shot_delay"] != (1 if xl else 3276)
        or not isinstance(proof["events"], list)
        or len(proof["events"]) != len(EVENTS)
    ):
        return False
    count = 0
    for row, (event, delta) in zip(proof["events"], EVENTS):
        if event == "restored":
            count = 1 if xl else 0
        if (
            not isinstance(row, dict)
            or set(row) != {"event", "before", "after", "charge"}
            or row["event"] != event
            or type(row["before"]) is not int
            or type(row["after"]) is not int
            or row["before"] != count
            or row["after"] != count + (delta if xl else 0)
            or type(row["charge"]) is not int
            or row["charge"] != 32768 + (273 if xl and row["after"] not in (0, 1) else 0)
        ):
            return False
        count = row["after"]
    return True


def time_combat_passed(proof, damage_rows, clocks):
    """Require native player-projectile attribution at the observed slowed frame."""
    try:
        target = proof["combat"]
        fields = (
            "object",
            "signature",
            "robot_id",
            "player",
            "player_signature",
            "primary_weapon",
            "shields_before",
            "shields_after",
            "first_damage_frame",
            "firing_frames",
            "aiming_frames",
        )
        if any(type(target[key]) is not int for key in fields):
            return False
        if (
            any(target[key] < 0 for key in ("object", "signature", "robot_id", "player", "player_signature"))
            or target["primary_weapon"] != 0
            or target["shields_before"] <= 0
            or target["shields_after"] >= target["shields_before"]
            or target["firing_frames"] != 84
            or not 0 <= target["aiming_frames"] < 90
            or type(proof["recorded_frames"]) is not int
            or not 0 <= target["first_damage_frame"] < proof["recorded_frames"]
            or type(proof["warmup_frames"]) is not int
            or proof["warmup_frames"] != 66
            or type(proof["inventory_assists"]) is not int
            or proof["inventory_assists"] != 0
            or type(proof["assists_during_recording"]) is not int
            or proof["assists_during_recording"] != 0
            or len(clocks) != 2
        ):
            return False
        for index, name in enumerate(("world_speed_at_damage", "player_speed_at_damage")):
            if (
                len(clocks[index]) != 4
                or type(target[name]) not in (int, float)
                or not 1 < target[name] <= 3
                or clocks[index][0] != target[name]
            ):
                return False
        expected = {
            "f": target["first_damage_frame"],
            "robot": target["object"],
            "robot_id": target["robot_id"],
            "robot_signature": target["signature"],
            "parent": target["player"],
            "parent_signature": target["player_signature"],
            "parent_type": 4,
        }
        return any(
            isinstance(row, dict)
            and row.get("type") == "robot_damage"
            and type(row.get("version")) is int
            and row.get("version") == 1
            and all(type(row.get(key)) is int and row[key] == value for key, value in expected.items())
            and all(type(row.get(key)) is int for key in ("old_shields", "new_shields", "damage"))
            and row["damage"] > 0
            and row["old_shields"] > row["new_shields"]
            and row["new_shields"] < target["shields_before"]
            for row in damage_rows
        )
    except (KeyError, TypeError):
        return False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("engine", "data", "extra", "work", "reference-root"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--mission", required=True)
    parser.add_argument("--level", required=True, type=int)
    parser.add_argument("--xl", action="store_true")
    parser.add_argument(
        "--time-equipment", action="store_true", help="Exercise the existing ordinary time-pickup/control/save probe"
    )
    args = parser.parse_args()
    pins = verify_reference(args.reference_root)
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    (work / "user").mkdir()
    proof_path = work / "proof.json"
    engine = args.engine.resolve()
    command = [
        str(engine),
        "-mission",
        args.mission,
        "-level",
        str(args.level),
        "-hogdir",
        str(args.data.resolve()),
        "-extra-dir",
        str(args.extra.resolve()),
        "-route-confirm-user-dir",
        str(work / "user"),
        "-route-confirm-json-out",
        str(work / "unused-route.json"),
        "-xl-time-equipment-proof" if args.time_equipment else "-xl-render-clock-proof",
        str(proof_path),
        "-nosound",
        "-nomusic",
    ]
    with (work / "engine.log").open("wb") as log:
        started = time.monotonic()
        try:
            code = subprocess.run(
                command, cwd=work, stdout=log, stderr=subprocess.STDOUT, timeout=45, check=False
            ).returncode
        except subprocess.TimeoutExpired:
            code = None
        elapsed = time.monotonic() - started
    proof = json.loads(proof_path.read_text()) if proof_path.exists() else {}
    recordings = []
    if args.time_equipment:
        required_events = {
            "visible_slow_motion_pickup",
            "visible_bullet_time_pickup",
            "earned_original_pickups",
            "ordinary_slow_motion",
            "slow_motion_thrust",
            "bullet_time",
            "bullet_time_thrust",
        }
        success = (
            code == 0
            and proof.get("schema") == "xl-time-equipment-gameplay-v1"
            and proof.get("complete") is True
            and proof.get("assisted") is True
            and proof.get("cold_resume") is False
            and proof.get("mission") == args.mission
            and proof.get("level") == args.level
            and args.xl
            and required_events <= {event.get("event") for event in proof.get("events", [])}
        )
        for demo in sorted((work / "user").rglob("*.dximdemo")):
            with demo.open(encoding="utf-8") as stream:
                header = json.loads(next(stream))
            success = success and header.get("version") == 6 and header.get("start_mode") == "save_checkpoint"
            recordings.append(
                {
                    "path": str(demo),
                    "sha256": hashlib.sha256(demo.read_bytes()).hexdigest(),
                    "frame_count": header.get("frame_count"),
                    "live_replay_qualified": False,
                }
            )
        success = success and len(recordings) == 1
        if success:
            try:
                trace = Path(str(proof_path) + ".capture.jsonl.gz")
                damage = [row for row in parity.records(trace) if row.get("type") == "robot_damage"]
                frame = proof.get("combat", {}).get("first_damage_frame")
                # World rows describe the start of a frame; damage is inside it.
                # The next boundary observes the factors after that simulation.
                clocks = next(
                    (
                        row["state"]["xl_time"]["slow_motion"]["clocks"]
                        for row in parity.world_states(trace, header, "d2")
                        if row["type"] == "world_state" and row["f"] == frame + 1
                    ),
                    None,
                )
                success = proof["combat_trace_verified"] = time_combat_passed(proof, damage, clocks)
            except (OSError, ValueError, KeyError, TypeError) as error:
                proof["combat_trace_error"] = str(error)
                success = False
    else:
        success = passed(code, proof, args.mission, args.level, args.xl)
    result = {
        "passed": success,
        "exit_code": code,
        "elapsed_seconds": elapsed,
        "proof": proof,
        "reference_revision": REVISION,
        "reference_hashes": pins,
        "engine_sha256": hashlib.sha256(engine.read_bytes()).hexdigest(),
        "official_readiness_status_updated": False,
    }
    if args.time_equipment:
        result["recordings"] = recordings
    (work / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"passed": success, "exit_code": code, "events": len(proof.get("events", []))}))
    return 0 if success else 1


if __name__ == "__main__":
    raise SystemExit(main())
