"""Run original-content texture ownership controls in a fresh isolated directory.

Requires the developer metadata engine and already extracted authentic assets.
This is a lifecycle diagnostic, not sampled readiness or campaign qualification.
"""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(root):
    return {path.relative_to(root).as_posix(): digest(path) for path in sorted(root.rglob("*")) if path.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument(
        "--extra", type=Path, required=True, help="Extracted missions/ and mods/ directories; copied before use"
    )
    parser.add_argument(
        "--work", type=Path, required=True, help="Fresh output directory beneath the repository temp/ directory"
    )
    parser.add_argument("--mode", choices=("clock", "vclip", "recovery", "guns", "afterburner"), required=True)
    parser.add_argument("--reference", type=Path, help="Pinned xl_hxm_reference executable, required for guns mode")
    parser.add_argument("--outgoing", choices=("d2", "wshock20"), default="d2")
    parser.add_argument("--timeout", type=int, default=120)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    engine, runtime, source, work = (path.resolve() for path in (args.engine, args.runtime, args.extra, args.work))
    if not engine.is_file() or not runtime.is_dir():
        parser.error("Engine file and runtime directory must exist")
    if args.mode == "guns" and (not args.reference or not args.reference.is_file()):
        parser.error("Guns mode requires the pinned xl_hxm_reference executable")
    if not all((source / name).is_dir() for name in ("missions", "mods")):
        parser.error("Extra input must contain missions/ and mods/")
    if args.timeout <= 0 or work.exists() or not work.is_relative_to(repo / "temp"):
        parser.error("Use a positive timeout and a fresh output path beneath repository temp/")
    if any(work.is_relative_to(path) or path.is_relative_to(work) for path in (source, runtime)):
        parser.error("Output and authentic input directories must not overlap")
    for folder in (source / "missions", source / "mods", runtime):
        if any(path.is_symlink() for path in folder.rglob("*")):
            parser.error("Input trees must contain regular files, not symbolic links")
    shell = "pwsh" if shutil.which("pwsh") else "powershell"
    subprocess.run(
        [
            shell,
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
    mission, level = ("wshock20", "6") if args.mode in ("vclip", "afterburner") else ("boilpnt", "7")
    output = extra / "texture-dependency-audit.json"
    argv = [
        str(engine),
        "-texture-dependency-audit",
        mission,
        "-audit-level",
        level,
        "-audit-output",
        str(output),
        "-hogdir",
        str(runtime),
        "-extra-dir",
        str(extra),
        "-userdir",
        str(extra / "preview-user"),
        "-nosound",
        "-nomusic",
    ]
    if args.mode == "recovery":
        argv += ["-audit-live-recovery", "1", "-audit-outgoing", args.outgoing]
    elif args.mode == "guns":
        argv += ["-audit-gun-points", "1"]
    elif args.mode == "afterburner":
        argv += ["-audit-afterburner-strip", "1"]
    else:
        argv += ["-audit-selected-mission", "1", "-audit-native-" + args.mode, "1"]
        if args.mode == "clock":
            argv += ["-audit-native-pixels", "1"]
    record = {
        "argv": argv,
        "source_head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repo, text=True).strip(),
        "source_status": subprocess.check_output(["git", "status", "--porcelain"], cwd=repo, text=True),
        "engine_sha256": digest(engine),
        "runner_sha256": digest(Path(__file__)),
        "input_sha256": before,
        "runtime_sha256": runtime_before,
        "timeout_seconds": args.timeout,
        "qualification": "Texture ownership diagnostic; not readiness or clean replay",
    }
    start = time.monotonic()
    with (work / "engine.log").open("wb") as log:
        try:
            record["exit_code"] = subprocess.run(
                argv, cwd=work, stdout=log, stderr=subprocess.STDOUT, timeout=args.timeout
            ).returncode
        except subprocess.TimeoutExpired:
            record["exit_code"] = "timeout"
    record["elapsed_seconds"] = round(time.monotonic() - start, 3)
    record["inputs_unchanged"] = before == {
        name: inventory(extra / name) for name in before
    } and runtime_before == inventory(runtime)
    record["report_present"] = output.is_file()
    record["requested_controls_present"] = False
    if output.is_file():
        record["report_sha256"] = digest(output)
        try:
            report = json.loads(output.read_text(encoding="utf-8"))
            if args.mode == "recovery":
                record["requested_controls_present"] = (
                    report.get("outgoing") == args.outgoing
                    and report.get("incoming") == "boilpnt:7"
                    and report.get("ordinary_frames") == 120
                    and report.get("equal_count_independence") == 1
                    and {row["case"] for row in report.get("rejections", [])}
                    == {"malformed_priority_override", "missing_set"}
                    and all(row.get("valid_recovery") == 1 for row in report["rejections"])
                    and all(
                        report.get("save_source_controls", {}).get(key) == 1
                        for key in (
                            "original",
                            "changed_priority",
                            "malformed_rejected",
                            "live_state_unchanged",
                            "recovered",
                        )
                    )
                    and report.get("save_source_controls", {}).get("source_count", 0) > 0
                )
            elif args.mode == "guns":
                record["requested_controls_present"] = (
                    report.get("mission") == "boilpnt"
                    and report.get("level") == 7
                    and report.get("gun_point_cases", 0) > 0
                    and report.get("fixed_header_cases", 0) > 0
                    and report.get("sim_rng_unchanged") == 1
                )
            elif args.mode == "afterburner":
                record["requested_controls_present"] = (
                    report.get("mission") == "wshock20"
                    and report.get("level") == 6
                    and report.get("blobs") == report.get("retired_blobs") == 6
                    and report.get("ticks") == 120
                    and report.get("selected_frames") == list(range(5))
                    and report.get("pixel_comparisons", 0) > 0
                    and report.get("image") == "mods/wshock20/textures/exp23#0.tga"
                )
            else:
                record["requested_controls_present"] = (
                    report.get("dependency_walk") == "complete"
                    and bool(report.get("native_" + args.mode + "_controls"))
                    and bool(report.get("selected_native_" + args.mode + "_controls"))
                    and (args.mode != "clock" or bool(report.get("native_pixel_controls")))
                )
        except (ValueError, TypeError, KeyError) as error:
            record["report_error"] = str(error)
    if args.mode == "guns":
        record["reference_exit_code"] = "not_run"
        if record["exit_code"] == 0 and record["requested_controls_present"]:
            reference = args.reference.resolve()
            samples = extra / "gun-points.txt"
            record["reference_sha256"] = digest(reference)
            record["samples_sha256"] = digest(samples)
            record["reference_argv"] = [str(reference), "--guns", str(samples)]
            with (work / "reference.log").open("wb") as log:
                try:
                    record["reference_exit_code"] = subprocess.run(
                        record["reference_argv"], cwd=work, stdout=log, stderr=subprocess.STDOUT, timeout=args.timeout
                    ).returncode
                except subprocess.TimeoutExpired:
                    record["reference_exit_code"] = "timeout"
    (work / "receipt.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    passed = record["exit_code"] == 0 and record["inputs_unchanged"] and record["requested_controls_present"]
    if args.mode == "guns":
        passed = passed and record["reference_exit_code"] == 0
    print(f"Texture {args.mode} audit: {'PASS' if passed else 'FAIL'}; receipt: {work / 'receipt.json'}")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
