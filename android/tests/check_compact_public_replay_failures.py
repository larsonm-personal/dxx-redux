"""Prove real replay comparison failures through the complete regression wrappers."""

import argparse
import hashlib
import os
import shutil
import subprocess
from pathlib import Path

import compact_public_replay as compact
import d1_replay_parity as parity
import public_incoming_robot_hits as incoming
import run_compact_public_replays as runner
import public_door_replay as doors


def corrupt_corpus(source, destination, cases, mutation):
    """Mutate disposable copies only; admission hashes are not gameplay oracles."""
    shutil.copytree(source, destination)
    for case in cases:
        demo, oracle = (destination / case[key] for key in ("demo", "oracle"))
        expected = compact._json(oracle.read_bytes())
        if mutation == "wrong-oracle":
            digest = expected["frame_sha256"][0]
            expected["frame_sha256"][0] = ("1" if digest[0] == "0" else "0") + digest[1:]
        elif mutation in ("bad-input", "no-collect"):
            if mutation == "no-collect" and case["scenario"] != "loot":
                continue
            records = [compact._json(line) for line in demo.read_bytes().splitlines() if line.strip()]
            release = next((row["f"] + 1 for row in case["actions"].get("door_events", []) if row["stage"] == 2), None)
            held_keys = sorted({key for row in records[1:-1] for key in row["input"].get("s", {})})
            for record in records[1:-1]:
                if mutation == "no-collect":
                    if record["f"] > case["actions"]["loot_frame"]:
                        record["input"] = {"s": {key: 0 for key in ("f", "x", "z", "h", "p", "f1s")}} if record["f"] == case["actions"]["loot_frame"] + 1 else {}
                elif case["scenario"] == "loot":
                    # Sparse held input: clear every fire transition without bloating the bounded replay
                    if record["f"] == 0 or "f1s" in record["input"].get("s", {}):
                        record["input"].setdefault("s", {})["f1s"] = 0
                elif case["scenario"] in ("combat", "guidebot-release"):
                    # Preserve the recorded flight/aim route but remove every primary shot
                    record["input"].setdefault("s", {})["f1s"] = 0
                elif case["scenario"] == "door-lifecycle":
                    # Reach and obstruct the door normally, then remove the clearance controls
                    if record["f"] >= release:
                        record["input"] = {"s": {key: 0 for key in held_keys}} if record["f"] == release else {}
                else:
                    # With no controls from frame zero the ship cannot follow any route
                    record["input"] = {}
            raw = b"".join(compact.canonical(record) + b"\n" for record in records)
            demo.write_bytes(raw)
            expected["compact_sha256"] = hashlib.sha256(raw).hexdigest()
        else:
            raise parity.EvidenceError("Unknown negative-control mutation")
        parity.write_json(oracle, expected)
    # Negative controls must reach the engine, not fail input admission
    if runner.load_corpus(destination) != cases:
        raise parity.EvidenceError("Negative corpus changed its scenario inventory")



def verify_loot_negative(actions, header, state, actual_result, mutation):
    """Require the intended live outcome, not just rejection of a damaged trace."""
    terminal_objects, terminal_world = None, None
    for row in parity.world_states(state, header, "d1"):
        if row.get("phase") == "terminal":
            if row["type"] == "object_boundary":
                terminal_objects = row["slots"]
            elif row["type"] == "world_boundary":
                terminal_world = row["state"]
    if terminal_objects is None or terminal_world is None:
        raise parity.EvidenceError("Loot negative lacks complete terminal world/object evidence")
    player = terminal_world["players"][0]
    if player["laser_level"] != 0 or actual_result["player0"]["laser_level"] != 0:
        raise parity.EvidenceError("Loot negative unexpectedly gained the laser upgrade")
    pickup = terminal_objects.get(str(actions["pickup"]))
    exact_pickup = pickup is not None and tuple(pickup[key] for key in ("type", "id", "signature")) == (7, 3, actions["pickup_signature"])
    if mutation == "no-collect":
        if not (exact_pickup and not pickup["flags"] & 2 and pickup["lifeleft"] > 0
                and player["shields"] >= 0 and terminal_world["death"]["active"] == 0
                and player["score"] == actual_result["player0"]["score"] == 500):
            raise parity.EvidenceError("No-collect must leave the exact drop unconsumed and the player alive after the kill")
    else:
        carrier = terminal_objects.get(str(actions["target"]))
        if not (carrier is not None and tuple(carrier[key] for key in ("type", "id", "signature")) == (2, actions["target_id"], actions["target_signature"])
                and carrier["shields"] >= 0 and not carrier["flags"] & 1 and not exact_pickup):
            raise parity.EvidenceError("No-fire must leave the original carrier alive without its drop")
        if any(row.get("type") == "robot_loot" and row.get("robot_signature") == actions["target_signature"] for row in parity.records(state)):
            raise parity.EvidenceError("No-fire unexpectedly resolved the carrier's loot")


def verify_failure(report, exit_code, args, cases, mutation):
    identities = {(case["game"], case["scenario"], repeat) for case in cases for repeat in range(2)}
    rows = report.get("native_runs", [])
    if (
        exit_code != 1
        or report.get("status") != "fail"
        or report.get("backend") != args.run_mode
        or report.get("native_only") is not True
        or report.get("paired", {}).get("status") != "not_run"
        or len(rows) != len(identities)
        or {(row.get("game"), row.get("scenario"), row.get("repeat")) for row in rows} != identities
    ):
        raise parity.EvidenceError("Negative run did not fail through the complete expected runtime inventory")
    evidence = []
    for row in rows:
        case = next(case for case in cases if (case["game"], case["scenario"]) == (row["game"], row["scenario"]))
        demo, oracle = (args.corpus / case[key] for key in ("demo", "oracle"))
        state, rng, actual = (Path(row["artifacts"][key]) for key in ("state", "rng", "result"))
        command = row["command"]
        causal_rejection = None
        door_rejection = None
        incoming_rejection = None
        expected_runner = "headless-console" if args.run_mode == "headless" else "visual"
        if row.get("status") != "fail" or command[command.index("-Runner") + 1] != expected_runner:
            raise parity.EvidenceError("Negative run accepted a scene or selected the wrong backend")
        if not all(path.is_file() and path.stat().st_size for path in (state, rng, actual)):
            raise parity.EvidenceError("Negative run lacks completed real-engine result/state/RNG artifacts")
        if mutation == "wrong-oracle":
            if (
                row.get("exit_code") != 0
                or row.get("error") != "Replay frame state, RNG or diagnostics differ from live capture"
            ):
                raise parity.EvidenceError("Wrong expected output did not fail at the live-frame comparison")
            # Matched positive using the exact same runtime evidence and original oracle
            compact.verify(case["game"], demo, oracle, state, rng, actual)
            runner.verify_actions(case, demo, state, actual)
        else:
            # A load/start/process failure is not a successful negative control
            log = actual.with_name("runner.log").read_text(encoding="utf-8")
            if row.get("exit_code") != 1 or "Result compare failed" not in log:
                raise parity.EvidenceError("Changed controls did not fail the real terminal comparison")
            header = next(parity.records(demo))
            if len(list(parity.frames(state, header, case["game"]))) != header["frame_count"]:
                raise parity.EvidenceError("Changed-input replay did not run every frame")
            actual_result = compact._json(actual.read_bytes())
            parity.validate_identity(actual_result, header, case["game"])
            recorded_result = list(parity.records(demo))[-1]["result"]
            if compact.sha(actual_result) == compact.sha(recorded_result):
                raise parity.EvidenceError("Changed controls did not change the actual terminal result")
            if case["scenario"] in ("combat", "loot", "guidebot-release"):
                try:
                    runner.verify_actions(case, demo, state, actual)
                except parity.EvidenceError as error:
                    causal_rejection = str(error)
                else:
                    raise parity.EvidenceError("Changed combat, loot or Guide-Bot controls still passed their causal action gate")
                if case["scenario"] == "combat":
                    states = [row["state"] for row in parity.frames(state, header, case["game"])] + [actual_result]
                    try:
                        incoming.verify_trace(header, states, state)
                    except parity.EvidenceError as error:
                        incoming_rejection = str(error)
                    else:
                        raise parity.EvidenceError("Changed combat controls still passed the incoming projectile gate")
                if (
                    case["scenario"] == "guidebot-release"
                    and causal_rejection != "Both cage barriers must blast at the exact declared post-frame boundary"
                ):
                    raise parity.EvidenceError("Guide-Bot no-fire controls did not fail the actual cage-blast proof")
            if case["scenario"] == "door-lifecycle":
                states = [row["state"] for row in parity.frames(state, header, case["game"])] + [actual_result]
                try:
                    doors.verify(case["actions"], header, states, state)
                except parity.EvidenceError as error:
                    door_rejection = str(error)
                else:
                    raise parity.EvidenceError("Door without clearance controls still passed its lifecycle gate")
            if case["scenario"] == "loot":
                verify_loot_negative(case["actions"], header, state, actual_result, mutation)
            if mutation == "no-collect":
                end = case["actions"]["loot_frame"] + 2
                expected = compact._json(oracle.read_bytes())
                if compact._frame_hashes(state, header)[:end] != expected["frame_sha256"][:end]:
                    raise parity.EvidenceError("No-collect control changed gameplay before the original physical drop")
                if actual_result["player0"]["laser_level"] != 0 or actual_result["player0"]["score"] != 500:
                    raise parity.EvidenceError("No-collect control did not preserve the kill while preventing inventory gain")
                if "exact new laser pickup" not in (causal_rejection or ""):
                    raise parity.EvidenceError("No-collect control failed for a reason other than physical collection")
        evidence.append(
            {
                "game": row["game"],
                "scenario": row["scenario"],
                "repeat": row["repeat"],
                "passed": True,
                **(
                    {
                        (
                            "guidebot_causal_rejection"
                            if case["scenario"] == "guidebot-release"
                            else "combat_causal_rejection"
                        ): causal_rejection
                    }
                    if causal_rejection is not None
                    else {}
                ),
                **({"door_lifecycle_rejection": door_rejection} if door_rejection is not None else {}),
                **({"incoming_hit_rejection": incoming_rejection} if incoming_rejection is not None else {}),
            }
        )
    return evidence


def run(args):
    cases = runner.load_corpus(args.corpus)
    originals = {path.name: parity.digest(path) for path in args.corpus.iterdir() if path.is_file()}
    args.output.mkdir(parents=True, exist_ok=False)
    with (args.output / "producer.lock").open("w", encoding="ascii") as lease:
        if os.name == "posix":
            import fcntl

            fcntl.flock(lease.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        return _run_controls(args, cases, originals)


def _run_controls(args, cases, originals):
    report = {"backend": args.run_mode, "status": "pending", "case_count": len(cases), "controls": []}
    parity.write_json(args.output / "report.json", report)
    for mutation in ("wrong-oracle", "bad-input"):
        changed = args.output / (mutation + "-corpus")
        corrupt_corpus(args.corpus, changed, cases, mutation)
        output = args.output / (mutation + "-run")
        command = [
            args.pwsh,
            "-NoProfile",
            "-File",
            str(args.repo / "android/tests/test_input_demo_regressions.ps1"),
            "-Corpus",
            "public",
            "-NativeOnly",
            "-RunMode",
            args.run_mode,
            "-DemoRoot",
            str(changed),
            "-D1Exe",
            str(args.exe_d1),
            "-D2Exe",
            str(args.exe_d2),
            "-D1DataDir",
            str(args.data_d1),
            "-D2DataDir",
            str(args.data_d2),
            "-ResultArchiveRoot",
            str(output),
            "-TimeoutSeconds",
            str(args.timeout),
            "-MinimumFreeSpaceGB",
            str(args.minimum_free_gb),
        ]
        item = {"mutation": mutation, "command": command, "status": "fail"}
        report["controls"].append(item)
        parity.write_json(args.output / "report.json", report)
        try:
            # Negative controls exercise native backends, not unrelated imported probes
            limit = 2 * len(cases) * (args.timeout + 120) + 120
            item["outer_exit_code"] = runner._run(command, args.output / (mutation + ".log"), limit, args.repo)
            original = compact._json((output / "report.json").read_bytes())
            item["evidence"] = verify_failure(original, item["outer_exit_code"], args, cases, mutation)
            item["status"] = "pass"
        except (OSError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
            item["error"] = str(error)
        parity.write_json(args.output / "report.json", report)
    # The extra loot-specific negative uses the same native/replay wrappers, twice,
    # without rerunning every unrelated positive scene a third time
    loot_cases = [case for case in cases if case["scenario"] == "loot"]
    if loot_cases:
        changed = args.output / "no-collect-corpus"
        corrupt_corpus(args.corpus, changed, cases, "no-collect")
        local = argparse.Namespace(**vars(args))
        local.corpus, local.output, local.native_only = changed, args.output / "no-collect-run", True
        local.output.mkdir()
        item = {"mutation": "no-collect", "scope": "authored loot only", "status": "fail"}
        report["controls"].append(item)
        try:
            code = runner._run_cases(local, loot_cases)
            observed = compact._json((local.output / "report.json").read_bytes())
            item["evidence"] = verify_failure(observed, code, args, loot_cases, "no-collect")
            item["status"] = "pass"
        except (OSError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
            item["error"] = str(error)
        parity.write_json(args.output / "report.json", report)
    report["original_corpus_unchanged"] = originals == {
        path.name: parity.digest(path) for path in args.corpus.iterdir() if path.is_file()
    }
    report["status"] = (
        "pass"
        if report["original_corpus_unchanged"] and all(item["status"] == "pass" for item in report["controls"])
        else "fail"
    )
    parity.write_json(args.output / "report.json", report)
    print(f"{report['status'].upper()}: real-engine negative controls ({args.run_mode}): {args.output / 'report.json'}")
    return 0 if report["status"] == "pass" else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repo", "corpus", "output", "data-d1", "data-d2", "exe-d1", "exe-d2"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--run-mode", choices=("headless", "graphics"), required=True)
    parser.add_argument("--pwsh", default="pwsh")
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--minimum-free-gb", type=float, default=4)
    args = parser.parse_args()
    if not 1 <= args.timeout <= 3600 or not 0.01 <= args.minimum_free_gb <= 1048576:
        parser.error("Invalid timeout or disk reserve")
    for key, value in vars(args).items():
        if isinstance(value, Path):
            setattr(args, key, value.resolve())
    return run(args)


if __name__ == "__main__":
    raise SystemExit(main())
