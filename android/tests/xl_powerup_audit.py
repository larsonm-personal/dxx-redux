"""Qualify original Subzero powerup initialization, save/restore and controlled pickup.

Requires the developer metadata executable and authentic extracted Boiling Point.
This diagnostic is separate from ordinary sampled readiness and campaign completion.
"""

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import time

from xl_texture_audit import digest, inventory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--extra", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True, help="Fresh isolated output directory")
    args = parser.parse_args()
    engine, runtime, source, work = (value.resolve() for value in (args.engine, args.runtime, args.extra, args.work))
    repo = Path(__file__).resolve().parents[2]
    if not engine.is_file() or not runtime.is_dir() or work.exists():
        parser.error("Use an existing engine/runtime and a fresh output directory")
    if any(work.is_relative_to(path) or path.is_relative_to(work) for path in (source, runtime, engine)):
        parser.error("Output must not overlap input paths")
    if not all((source / name).is_dir() for name in ("missions", "mods")):
        parser.error("Extra input must contain missions/ and mods/")
    hog = source / "missions/boilpnt.hog"
    profiles = {
        "f0c7363c992bdcf0222873e63bcf7fff699c6f53ca28ab8ea9437f953b190ce2": "frozen-cohort",
        "228dc2c6f4b8fbd4ef108966c43ee127007295d4215ac233b8e4ca40abae700c": "private-owner",
    }
    profile = profiles.get(digest(hog)) if hog.is_file() else None
    if not profile:
        parser.error("Requires a pinned Boiling Point HOG with the original Subzero member")
    if any(path.is_symlink() for root in (source, runtime) for path in root.rglob("*")):
        parser.error("Input trees must contain regular files")
    subprocess.run(
        [
            "pwsh" if shutil.which("pwsh") else "powershell",
            "-NoProfile",
            "-File",
            str(repo / "android/helpers/retain-recent-artifacts.ps1"),
            "-Artifacts",
            str(work),
        ],
        cwd=repo,
        check=True,
    )
    work.mkdir(parents=True)
    extra = work / "extra"
    for name in ("missions", "mods"):
        shutil.copytree(source / name, extra / name)
    before = {name: inventory(extra / name) for name in ("missions", "mods")}
    runtime_before = inventory(runtime)
    argv = [
        str(engine),
        "-sentinel-powerup-test",
        "1",
        "-hogdir",
        str(runtime),
        "-extra-dir",
        str(extra),
        "-userdir",
        str(extra / "preview-user"),
        "-nosound",
        "-nomusic",
    ]
    record = {
        "argv": argv,
        "engine_sha256": digest(engine),
        "runner_sha256": digest(Path(__file__)),
        "source_head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repo, text=True).strip(),
        "source_status": subprocess.check_output(["git", "status", "--porcelain"], cwd=repo, text=True),
        "input_sha256": before,
        "runtime_sha256": runtime_before,
        "timeout_seconds": 120,
        "qualification": "Original powerup lifecycle with controlled pickup; not sampled readiness",
        "input_profile": profile,
    }
    start = time.monotonic()
    with (work / "engine.log").open("wb") as log:
        try:
            record["exit_code"] = subprocess.run(
                argv, cwd=work, stdout=log, stderr=subprocess.STDOUT, timeout=120
            ).returncode
        except subprocess.TimeoutExpired:
            record["exit_code"] = "timeout"
    record["elapsed_seconds"] = round(time.monotonic() - start, 3)
    record["inputs_unchanged"] = before == {
        name: inventory(extra / name) for name in before
    } and runtime_before == inventory(runtime)
    report = extra / "sentinel-powerup.json"
    record["controls_complete"] = False
    if report.is_file():
        result = json.loads(report.read_text())
        record["report_sha256"] = digest(report)
        record["controls_complete"] = (
            result.get("mission") == "boilpnt"
            and result.get("level") == 4
            and result.get("ordinary_frames") == 120
            and all(
                result.get(key) == 1
                for key in (
                    "poisoned_prior_unions",
                    "disk_restore_semantic_equal",
                    "stationary",
                    "controlled_pickup",
                    "future_equal",
                    "consumed_restore_equal",
                )
            )
        )
    record["passed"] = record["exit_code"] == 0 and record["inputs_unchanged"] and record["controls_complete"]
    (work / "result.json").write_text(json.dumps(record, indent=2) + "\n")
    print(
        json.dumps(
            {
                key: record[key]
                for key in ("passed", "exit_code", "elapsed_seconds", "inputs_unchanged", "controls_complete")
            },
            indent=2,
        )
    )
    return 0 if record["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
