"""Prove real replay comparison failures through the complete regression wrappers."""

import argparse
import hashlib
import os
import shutil
import subprocess
from pathlib import Path

import compact_public_replay as compact
import d1_replay_parity as parity
import run_compact_public_replays as runner


def corrupt_corpus(source, destination, cases, mutation):
    """Mutate disposable copies only; admission hashes are not gameplay oracles."""
    shutil.copytree(source, destination)
    for case in cases:
        demo, oracle = (destination / case[key] for key in ("demo", "oracle"))
        expected = compact._json(oracle.read_bytes())
        if mutation == "wrong-oracle":
            digest = expected["frame_sha256"][0]
            expected["frame_sha256"][0] = ("1" if digest[0] == "0" else "0") + digest[1:]
        elif mutation == "bad-input":
            records = [compact._json(line) for line in demo.read_bytes().splitlines() if line.strip()]
            for record in records[1:-1]:
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
        evidence.append({"game": row["game"], "scenario": row["scenario"], "repeat": row["repeat"], "passed": True})
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
