"""Capture native input phases, replay both backends, and require causal negatives."""

import argparse
import copy
import json
import os
from pathlib import Path
import subprocess

import compact_public_replay as compact
import d1_replay_parity as parity
import ordered_public_replay as ordered
from run_compact_public_replays import validate_assets


def require(condition, message):
    if not condition:
        raise parity.EvidenceError(message)


def verify_dispatch_capture(path):
    rows = list(parity.records(path))
    batches = ordered.validate(rows[0], rows[1:-2], rows[-2])
    require(len(batches) == 9, "Dispatch probe must retain eight frames and input tail")
    require(batches[0]["pre_rng"]["c"] == 0 and batches[0]["post_rng"]["c"] > 0,
            "First input must draw SIM RNG before frame zero")
    require(batches[0]["ops"][0]["ft"] == 1092 and batches[0]["frame_time"] == 3276,
            "Input timestep must differ from the later simulation timestep")
    require([op["k"] for op in batches[2]["ops"]] == ["clear", "clear"],
            "Held-input and empty native flushes must remain explicit")
    require([op["k"] for op in batches[3]["ops"]] == ["command", "reset", "sample", "reset", "sample", "command"],
            "Commands must retain their positions before and after native dispatch")
    require([op["k"] for op in batches[4]["ops"]] == ["reset"], "Handled input must retain reset-only dispatch")
    require(batches[6]["ops"] == [], "No dispatch must encode an explicit empty batch")
    require(batches[-1]["ops"] and all(op["ft"] == 1638 for op in batches[-1]["ops"])
            and batches[-1]["post_rng"]["c"] > batches[-1]["pre_rng"]["c"],
            "Terminal input must retain its native timestep and SIM effects")
    return rows


def replay(args, game, mode, demo, directory):
    directory.mkdir(parents=True)
    exe = getattr(args, mode + "_" + game)
    cmd = [args.pwsh, "-NoProfile", "-File", str(args.repo / "android/tests/run_input_demo_replay.ps1"),
           "-DataEdition", "shareware", "-ExecutablePath", str(exe), "-Game", game,
           "-DataDir", str(getattr(args, "data_" + game)), "-DemoPath", str(demo),
           "-Runner", "headless-console" if mode == "headless" else "visual", "-Mode", "accelerated",
           "-StrictComparison", "-RequireCleanExit", "-StateLogPath", str(directory / "state.jsonl.gz"),
           "-RngLogPath", str(directory / "rng.jsonl"), "-ResultCopyPath", str(directory / "result.json"),
           "-TimeoutSeconds", str(args.timeout), "-MinimumFreeSpaceGB", str(args.minimum_free_gb),
           "-SandboxSuffix", "ordered-" + game + "-" + mode + "-" + directory.name]
    env = dict(os.environ)
    if mode == "headless":
        env.pop("DISPLAY", None)
        env.pop("WAYLAND_DISPLAY", None)
        env["SDL_VIDEODRIVER"] = "invalid"
    with (directory / "runner.log").open("w") as log:
        status = subprocess.run(cmd, cwd=args.repo, env=env, stdout=log, stderr=subprocess.STDOUT,
                                timeout=args.timeout + 90).returncode
    if status:
        archives = list(directory.glob("result.json.failure_*/failure.json"))
        require(len(archives) == 1, "Failed input must retain one complete native failure report")
        failure = json.loads(archives[0].read_text())
        require(failure.get("forced_stop") is False and failure.get("native_exit_code") is not None,
                "Negative input must exit naturally, not hang or be killed")
    return status


def run(args):
    require(not args.output.exists(), "Use a new ordered-input output directory")
    args.output.mkdir(parents=True)
    report = []
    for game in ("d1", "d2"):
        data = getattr(args, "data_" + game)
        validate_assets(game, data)
        capture = args.output / (game + "-capture")
        capture.mkdir()
        with (capture / "capture.log").open("w") as log:
            subprocess.run([str(getattr(args, "capture_" + game)), "--public-level-replay", str(data), "ordered-dispatch"],
                           cwd=capture, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=args.timeout)
        demo = capture / "capture.dximdemo"
        rows = verify_dispatch_capture(demo)
        header = rows[0]
        for mode in ("headless", "graphics"):
            directory = args.output / (game + "-" + mode + "-positive")
            require(replay(args, game, mode, demo, directory) == 0, "Native ordered dispatch replay failed")
            require(compact._frame_hashes(demo, header, recorded=True) == compact._frame_hashes(directory / "state.jsonl.gz", header),
                    "Native ordered dispatch frame state/RNG/diagnostics differ")
            require(parity.compare_rng(capture / "capture.dximdemo.rngtrace.jsonl", directory / "rng.jsonl")["status"] == "pass",
                    "Native ordered dispatch SIM events differ")
            report.append({"game": game, "backend": mode, "case": "positive", "status": "pass"})
            for mutation, boundary in (("clear", 2), ("first-input", 0), ("tail", 8)):
                changed = copy.deepcopy(rows)
                if mutation == "clear":
                    changed[3]["dispatch"]["ops"] = []
                elif mutation == "first-input":
                    # Leave final released controls intact but remove the first flare
                    changed[1]["dispatch"]["ops"] = changed[1]["dispatch"]["ops"][2:]
                else:
                    changed[-2]["dispatch"]["ops"] = []
                negative = args.output / (game + "-" + mode + "-" + mutation + ".dximdemo")
                negative.write_bytes(b"".join(compact.canonical(row) + b"\n" for row in changed))
                destination = negative.with_suffix("")
                require(replay(args, game, mode, negative, destination) != 0, "Corrupt ordered input was accepted")
                actual_rows = list(parity.records(destination / "state.jsonl.gz"))
                aborted = [row for row in actual_rows if row.get("type") == "object_boundary" and row.get("phase") == "aborted"]
                require(len(aborted) == 1 and aborted[0]["f"] == boundary
                        and sum(row.get("type") == "frame_state" for row in actual_rows) == boundary
                        and not (destination / "result.json").exists(),
                        "Negative did not abort before simulation at the intended ordered-input boundary")
                report.append({"game": game, "backend": mode, "case": mutation, "status": "pass"})
        (args.output / "report.json").write_text(json.dumps({"status": "pass", "runs": report}, indent=2) + "\n")
    print("PASS: native ordered input, first-input RNG, flushes, tail, pause/live-input isolation and causal negatives in D1/D2")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for role in ("capture", "headless", "graphics", "data"):
        for game in ("d1", "d2"):
            parser.add_argument("--" + role + "-" + game, type=Path, required=True)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--pwsh", default="pwsh")
    parser.add_argument("--timeout", type=int, default=60)
    parser.add_argument("--minimum-free-gb", type=float, default=4)
    run(parser.parse_args())


if __name__ == "__main__":
    main()
