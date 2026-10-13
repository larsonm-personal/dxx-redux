"""Capture fresh compact candidates; never update checked-in expectations automatically."""

import argparse
import hashlib
import itertools
import json
import subprocess
from pathlib import Path

import compact_public_replay as compact
from run_compact_public_replays import validate_assets, PUBLIC_CASES
import public_switch_replay as shoot_switch


def capture(args):
    args.output.mkdir(parents=True, exist_ok=False)
    cases = []
    for game in ("d1", "d2"):
        exe = getattr(args, "capture_" + game)
        data = getattr(args, "data_" + game)
        hashes = compact.pinned_assets(game)
        validate_assets(game, data)
        provenance = {
            "source_revision": args.engine_revision,
            "capture_executable_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
            "capture_source_sha256": hashlib.sha256(
                (args.repo / "android/tests/public_level_replay_fixture.hpp").read_bytes()
            ).hexdigest(),
            "asset_sha256": hashes,
        }
        for candidate_game, scenario in PUBLIC_CASES:
            if candidate_game != game or (args.scenario and scenario != args.scenario):
                continue
            directory = args.output / f"{game}-{scenario}"
            directory.mkdir()
            with (directory / "capture.log").open("w") as log:
                subprocess.run(
                    [str(exe), "--public-level-replay", str(data), scenario],
                    cwd=directory,
                    stdout=log,
                    stderr=subprocess.STDOUT,
                    check=True,
                    timeout=120,
                )
            report = json.loads((directory / "capture.json").read_text())
            if scenario == "flight" and report["wall_contact_frames"] < 10:
                raise compact.parity.EvidenceError("Capture did not contact solid geometry under thrust")
            if scenario != "flight" and report["action_frame"] < 0:
                raise compact.parity.EvidenceError("Capture did not reach its declared real action")
            stem = f"{game}-public-{scenario}"
            compact.pack(
                directory / "capture.dximdemo",
                directory / "capture.dximdemo.rngtrace.jsonl",
                args.output / (stem + ".dximdemo"),
                args.output / (stem + ".oracle.json"),
                provenance,
                door_trace=directory / "capture-observation.jsonl" if scenario == "door-lifecycle" else None,
                combat_trace=directory / "capture-observation.jsonl" if scenario == "combat" else None,
            )
            actions = {
                key: report[key]
                for key in ("target", "target_id", "target_signature", "action_frame", "wall_contact_frames")
            }
            if scenario == "door-lifecycle":
                actions.update({key: report[key] for key in ("door_back", "door_events")})
            if scenario == "shoot-switch":
                events = [
                    row
                    for row in compact.parity.records(directory / "capture-observation.jsonl")
                    if row.get("type") == "trigger_activation" and row.get("trigger") == 17
                ]
                if len(events) != 1 or events[0].get("shot") != 1 or events[0].get("f") != report["action_frame"]:
                    raise compact.parity.EvidenceError("Live capture lacks a successful projectile-trigger observation")
                actions.update(shoot_switch.SWITCH, passage_frame=report["passage_frame"])
            if scenario == "combat":
                observations = list(compact.parity.records(directory / "capture-observation.jsonl"))
                hits = [row for row in observations if row.get("type") == "robot_damage"]
                loots = [row for row in observations if row.get("type") == "robot_loot"]
                if not hits or len(loots) != 1 or sum(row["died"] for row in hits) != 1:
                    raise compact.parity.EvidenceError("Live combat lacks identified projectile death and loot resolution")
                actions.update({key: report[key] for key in ("damage_frame", "retirement_frame", "target_shields", "target_score")})
                actions.update(loot_frame=loots[0]["f"], loot_count=int(loots[0]["created"] >= 0))
            if scenario == "pickup":
                observations = report["observations"]
                gains = [
                    row["f"]
                    for previous, row in itertools.pairwise(observations)
                    if row["energy"] > previous["energy"] or row["shields"] > previous["shields"]
                ]
                if not gains:
                    raise compact.parity.EvidenceError("Live pickup capture has no measured resource gain")
                actions["pickup_gain_frame"] = gains[0]
            cases.append(
                {
                    "game": game,
                    "scenario": scenario,
                    "demo": stem + ".dximdemo",
                    "oracle": stem + ".oracle.json",
                    "actions": actions,
                }
            )
    (args.output / "manifest.json").write_text(json.dumps({"schema": 1, "cases": cases}, indent=4) + "\n")
    print("Captured candidates only; replay and repeated-capture checks are required before admission")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repo", "output", "capture-d1", "capture-d2", "data-d1", "data-d2"):
        parser.add_argument("--" + name, required=True, type=lambda value: Path(value).resolve())
    parser.add_argument("--engine-revision", required=True)
    parser.add_argument("--scenario", choices=sorted({scenario for _, scenario in PUBLIC_CASES}))
    capture(parser.parse_args())


if __name__ == "__main__":
    main()
