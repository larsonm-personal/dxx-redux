"""Capture a full D1 replay and compare its traces to a prior engine capture."""

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import re
import subprocess


def fingerprint(path, compressed=False):
    digest = hashlib.sha256()
    size = 0
    opener = gzip.open if compressed else open
    with opener(path, "rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
            size += len(chunk)
    return {"sha256": digest.hexdigest(), "bytes": size}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", choices=("d1", "d2"), required=True)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--demo", type=Path, required=True)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timeout", type=int, default=300)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    # Never reuse stale results as evidence for a new capture
    if output.exists():
        parser.error("output must be a new directory")
    expected = {}
    for name in ("state.jsonl.gz", "rng.jsonl", "result.json"):
        expected[name] = fingerprint(args.reference / name, name.endswith(".gz"))
    subprocess.run(["pwsh", "-NoProfile", "-File",
                    str(repo / "android/helpers/retain-recent-artifacts.ps1"),
                    "-Artifacts", str(output)], check=True)
    output.mkdir(parents=True)
    command = ["pwsh", "-NoProfile", "-File", str(repo / "android/tests/run_input_demo_replay.ps1"),
               "-DemoPath", str(args.demo.resolve()), "-DataDir", str(args.data.resolve()),
               "-ExecutablePath", str(args.exe.resolve()), "-Runner", "windowed-no-present",
               "-Mode", "accelerated", "-SkipExpectedChecks", "-SandboxSuffix", output.name,
               "-TimeoutSeconds", str(args.timeout), "-ResultCopyPath", str(output / "result.json"),
               "-StateLogPath", str(output / "state.jsonl.gz"), "-RngLogPath", str(output / "rng.jsonl")]
    command += ["-D1InD2"] if args.game == "d2" else ["-Game", "d1"]
    report = {"command": command, "engine": fingerprint(args.exe), "demo": fingerprint(args.demo),
              "reference": str(args.reference.resolve()), "checks": {}}
    with (output / "runner.log").open("w", encoding="utf-8") as log:
        completed = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    report["exit_code"] = completed.returncode
    timing = re.search(r"Elapsed: ([\d.]+)s replay_fps=([\d.]+)",
                       (output / "runner.log").read_text(encoding="utf-8-sig"))
    if timing:
        report["elapsed_seconds"], report["replay_fps"] = map(float, timing.groups())
    for name, reference in expected.items():
        try:
            actual = fingerprint(output / name, name.endswith(".gz"))
            report["checks"][name] = {"equal": actual == reference,
                                     "reference": reference, "actual": actual}
        except (OSError, EOFError) as error:
            report["checks"][name] = {"equal": False, "error": str(error)}
    report["passed"] = completed.returncode == 0 and all(
        check["equal"] for check in report["checks"].values())
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
