"""One reviewed, non-publishing before/after corpus experiment.

Only this controller writes reports. Target producers retain their native watchdogs
and process supervision. No archive, executable, launch file or log is exported.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

from nightly_regression_catalog import baselines, stage_archives
from nightly_regression_corpus import _metadata_baseline, freeze_oracles, require_unchanged_oracles
from retail_ci_assets import baseline as retail_baseline, inventory as retail_inventory

ROOT = Path(__file__).resolve().parents[2]
CONFIG = ROOT / "android/tests/corpus_comparison.json"
REPOSITORY = "larsonm-personal/dxx-redux"
WORKFLOW = REPOSITORY + "/.github/workflows/corpus-comparison.yml@refs/heads/cmake"


def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def write(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def file_hash(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def config():
    value = read(CONFIG)
    if set(value) != {"base", "candidate", "assets", "shards", "repeat", "speed_percent", "workers"}:
        raise ValueError("Unexpected experiment configuration")
    if any(not re.fullmatch("[0-9a-f]{40}", value[key]) for key in ("base", "candidate", "assets")):
        raise ValueError("Revisions must be immutable")
    if (value["shards"], value["repeat"], value["speed_percent"], value["workers"]) != (4, 2, 160, 2):
        raise ValueError("Experiment settings changed; review controller and tests")
    return value


def gate(settings, env):
    expected = {
        "GITHUB_REPOSITORY": REPOSITORY,
        "GITHUB_REF": "refs/heads/cmake",
        "GITHUB_WORKFLOW_REF": WORKFLOW,
        "GITHUB_EVENT_NAME": "workflow_dispatch",
        "RETAIL_ASSETS_COMMIT": settings["assets"],
    }
    if any(env.get(key) != value for key, value in expected.items()):
        raise ValueError("Comparison requires the trusted manual workflow and configured asset pin")


def verify_revision(repo, expected):
    actual = subprocess.check_output(["git", "-C", str(repo), "rev-parse", "HEAD"], text=True).strip()
    if actual != expected:
        raise ValueError("Checkout revision differs from reviewed experiment")


def levels(document):
    result = {}
    for mission in document if isinstance(document, list) else [document]:
        for level in mission["levels"]:
            key = f"{mission.get('target_index', 0)}|{level['level_num']}|{level['level_file']}"
            if key in result:
                raise ValueError("Duplicate logical level identity")
            result[key] = level
    return result


def manifest(base, candidate, settings):
    retail = [retail_inventory(repo / "game_data/retail-ci", retail_baseline(ROOT)) for repo in (base, candidate)]
    if (
        any(item["matches_tested_edition"] is not True for item in retail)
        or retail[0]["manifest_sha256"] != retail[1]["manifest_sha256"]
    ):
        raise ValueError("Retail inputs differ from the reviewed edition")
    entries = []
    route_duplicates = {}
    loads = [0] * settings["shards"]
    for relative in sorted(baselines(base).values()):
        left, right = base / "game_data/mission_files" / relative, candidate / "game_data/mission_files" / relative
        if not right.is_file() or left.read_bytes() != right.read_bytes():
            raise ValueError("Comparison requires identical committed corpus inputs")
        simulation = left.with_suffix(".simulation.json")
        other_simulation = right.with_suffix(".simulation.json")
        if simulation.exists() != other_simulation.exists() or (
            simulation.exists() and simulation.read_bytes() != other_simulation.read_bytes()
        ):
            raise ValueError("Committed simulation references differ")
        if simulation.exists():
            for identity, level in levels(read(simulation)).items():
                if level.get("route_input_sha256"):
                    route_duplicates.setdefault(level["route_input_sha256"], []).append(relative + "|" + identity)
        archive = next(
            (left.with_suffix(ext) for ext in (".zip", ".7z", ".rar") if left.with_suffix(ext).is_file()), None
        )
        builtin = left.name in ("FirstStrike.json", "Counterstrike.json")
        archive_hash = file_hash(archive) if archive else None
        if archive and file_hash(right.with_suffix(archive.suffix)) != archive_hash:
            raise ValueError("Staged archive bytes differ")
        shard = min(range(len(loads)), key=lambda index: loads[index])
        expected = levels(read(left))
        available = builtin or archive is not None
        if available:
            loads[shard] += len(expected)
        entries.append(
            {
                "baseline": relative,
                "levels": sorted(expected),
                "metadata_sha256": file_hash(left),
                "simulation_sha256": file_hash(simulation) if simulation.exists() else None,
                "archive_sha256": archive_hash,
                "extension": archive.suffix if archive else None,
                "available": available,
                "shard": shard,
                "uncovered_reason": None if available else "archive_or_cd_source_not_staged",
            }
        )
    if set(baselines(base).values()) != set(baselines(candidate).values()):
        raise ValueError("Candidate changes corpus membership")
    duplicates = {}
    for entry in entries:
        if entry["archive_sha256"]:
            duplicates.setdefault(entry["archive_sha256"], []).append(entry["baseline"])
    return {
        "settings": settings,
        "retail_manifest_sha256": retail[0]["manifest_sha256"],
        "entries": entries,
        "duplicate_archives": [group for group in duplicates.values() if len(group) > 1],
        "duplicate_route_inputs": [group for group in route_duplicates.values() if len(group) > 1],
    }


def run(repo, arguments, log):
    with log.open("wb") as stream:
        return subprocess.run(arguments, cwd=repo, stdout=stream, stderr=subprocess.STDOUT, check=False).returncode


def producer(repo, script, arguments, log):
    return run(
        repo,
        ["pwsh", "-NoProfile", "-NonInteractive", "-File", str(repo / "android/helpers" / script), *arguments],
        log,
    )


def collect(repo, entry, settings, work, evidence):
    """Run one archive against fresh projected metadata; canonical JSON is never a destination."""
    relative = entry["baseline"]
    baseline = repo / "game_data/mission_files" / relative
    work.mkdir(parents=True)
    evidence.mkdir(parents=True)
    metadata_output = work / "metadata"

    def binary(game, suffix):
        return str(repo / ("build" + game) / "main" / ("dxx-redux-" + game + suffix))

    args = [
        "-NoBuild",
        "-NoRegressionCopy",
        "-MaxParallel",
        "1",
        "-OutputRoot",
        str(metadata_output),
        "-D1Executable",
        binary("d1", "-metadata-worker"),
        "-D2Executable",
        binary("d2", "-metadata-worker"),
        "-CdSourceIds",
        "__none__",
    ]
    if entry["extension"]:
        args += ["-ArchivePaths", str(baseline.with_suffix(entry["extension"]))]
    else:
        args += ["-BuiltInOnly", "-IncludeBuiltIn" + baseline.stem]
    result = {
        "metadata_exit_code": producer(repo, "regenerate_all_mission_metadata_host.ps1", args, work / "metadata.log"),
        "levels": {},
        "metadata": {},
        "engine": {},
        "process": {},
        "budgets": {},
    }
    summary = metadata_output / "summary.json"
    records = read(summary) if summary.is_file() else []
    records = records if isinstance(records, list) else [records]
    if result["metadata_exit_code"] or len(records) != 1 or records[0].get("status") != "passed":
        result["incomplete"] = "metadata_producer_failed_or_skipped"
        return result
    generated = Path(records[0]["metadata_json"])
    if not generated.resolve().is_relative_to(metadata_output.resolve()):
        raise ValueError("Producer metadata escaped output directory")
    document = read(generated)
    result["metadata"] = levels(document)
    write(evidence / "metadata.json", document)
    scratch = work / "input"
    staged = scratch / relative
    write(staged, document)
    if entry["extension"]:
        shutil.copyfile(baseline.with_suffix(entry["extension"]), staged.with_suffix(entry["extension"]))
    route = work / "route"
    result["route_exit_code"] = producer(
        repo,
        "regenerate_all_guidebot_simulations.ps1",
        [
            "-Mode",
            "Headless",
            "-NoBuild",
            "-ExactMissionJson",
            "-MissionJson",
            relative,
            "-MissionMetadataRoot",
            str(scratch),
            "-HeadlessExecutable",
            binary("d2", "-headless-route"),
            "-HogDir",
            str(repo / "game_data/retail-ci/d2"),
            "-D1InD2HogDir",
            str(repo / "game_data/retail-ci/d1"),
            "-SampleFraction",
            "1",
            "-Repeat",
            str(settings["repeat"]),
            "-TestSpeedPercent",
            str(settings["speed_percent"]),
            "-MaxParallel",
            str(settings["workers"]),
            "-OutputRoot",
            str(route),
        ],
        work / "route.log",
    )
    route_summary = route / "summary.json"
    summary = read(route_summary) if route_summary.is_file() else {}
    result["selection"] = summary.get("selected_work_items", [])
    if (
        summary.get("write_regression") is not False
        or summary.get("changed_files")
        or set(result["selection"]) != {relative + "|" + key for key in entry["levels"]}
    ):
        result["incomplete"] = "selection_or_oracle_contract_changed"
    result_file = route / "results" / baseline.with_suffix(".simulation.json").name
    if result_file.is_file():
        result["levels"] = levels(read(result_file))
    for identity in entry["levels"]:
        safe = re.sub(r"[^A-Za-z0-9_.-]+", "_", relative + "|" + identity)
        result["engine"][identity], result["process"][identity], result["budgets"][identity] = [], [], []
        for repeat in range(1, settings["repeat"] + 1):
            path = route / "results" / f"{safe}_run_{repeat}.json"
            if path.is_file():
                raw = read(path)
                # Preserve pre-normalization failure, objectives and RNG without arbitrary log/launch files
                receipt = {
                    key: raw[key]
                    for key in ("status", "problem", "frames", "objectives", "rng_start", "rng_end")
                    if key in raw
                }
                result["engine"][identity].append(receipt)
            launch = Path(str(path) + ".launch.json")
            if launch.is_file():
                launched = read(launch)
                arguments = launched["arguments"]
                position = arguments.index("-route-confirm-timeout-seconds")
                result["budgets"][identity].append(
                    {
                        "simulation_seconds": int(arguments[position + 1]),
                        "watchdog_seconds": launched["timeout_seconds"],
                    }
                )
            process = Path(str(path) + ".process.json")
            if process.is_file():
                raw = read(process)
                result["process"][identity].append({key: raw.get(key) for key in ("exit_code", "timed_out")})
    return result


def compare(entry, before, after):
    rows = []
    identities = sorted(set(entry["levels"]) | set(before.get("levels", {})) | set(after.get("levels", {})))
    for identity in identities:
        a, b = before.get("levels", {}).get(identity), after.get("levels", {}).get(identity)
        engines = [side.get("engine", {}).get(identity, []) for side in (before, after)]
        processes = [side.get("process", {}).get(identity, []) for side in (before, after)]
        unstable = any(len(runs) == 2 and runs[0] != runs[1] for runs in engines + processes)
        status = "uncovered" if not entry["available"] else "incomplete" if a is None or b is None else "unchanged"
        if a is not None and b is not None and entry["available"]:
            if unstable or "nondeterministic" in (a["status"], b["status"]):
                status = "nondeterministic"
            elif any(
                value["status"] not in ("ok", "failed", "timeout", "unsupported", "route_mismatch") for value in (a, b)
            ):
                status = "incomplete"
            elif a["status"] == "ok" and b["status"] != "ok":
                status = "regression"
            elif a["status"] != "ok" and b["status"] == "ok":
                status = "improvement"
            elif a != b:
                status = "changed"
            for value, runs, receipts in zip((a, b), engines, processes):
                if value["status"] != "unsupported" and len(receipts) != 2:
                    status = "incomplete"
                if value["status"] == "ok" and (len(runs) != 2 or any(r.get("status") != "confirmed" for r in runs)):
                    status = "incomplete"
            if identity not in entry["levels"] or any(
                side.get("incomplete") or side.get("route_exit_code", 0) for side in (before, after)
            ):
                status = "incomplete"
        rows.append(
            {
                "identity": entry["baseline"] + "|" + identity,
                "classification": status,
                "before": a,
                "after": b,
                "raw_before": engines[0],
                "raw_after": engines[1],
                "process_before": before.get("process", {}).get(identity, []),
                "process_after": after.get("process", {}).get(identity, []),
                "budgets_before": before.get("budgets", {}).get(identity, []),
                "budgets_after": after.get("budgets", {}).get(identity, []),
                "metadata_changed": _metadata_baseline({"levels": [before.get("metadata", {}).get(identity, {})]})
                != _metadata_baseline({"levels": [after.get("metadata", {}).get(identity, {})]}),
            }
        )
    return rows


def execute(args, settings):
    gate(settings, os.environ)
    for name in ("base", "candidate", "assets"):
        verify_revision(getattr(args, name), settings[name])
    args.output.mkdir(parents=True, exist_ok=False)
    for name in ("base", "candidate"):
        stage_archives(args.assets, getattr(args, name), settings["assets"])
    inputs = manifest(args.base, args.candidate, settings)
    write(args.output / "manifest.json", inputs)
    rows, phases = [], []
    frozen = freeze_oracles(
        [p for name in ("base", "candidate") for p in (getattr(args, name) / "game_data/mission_files").rglob("*.json")]
    )
    try:
        for entry in inputs["entries"]:
            if entry["shard"] != args.shard:
                continue
            key = digest(entry["baseline"])[:20]
            outcomes = []
            for name in ("base", "candidate"):
                outcome = {}
                if entry["available"]:
                    work = args.work / key / name
                    try:
                        outcome = collect(getattr(args, name), entry, settings, work, args.output / key / name)
                    except (OSError, ValueError, KeyError) as error:
                        outcome = {"incomplete": type(error).__name__}
                    finally:
                        require_unchanged_oracles(frozen)
                        if (
                            entry["extension"]
                            and file_hash(
                                getattr(args, name)
                                / "game_data/mission_files"
                                / Path(entry["baseline"]).with_suffix(entry["extension"])
                            )
                            != entry["archive_sha256"]
                        ):
                            raise ValueError("Producer modified its input archive")
                        shutil.rmtree(work, ignore_errors=True)
                    write(args.output / key / (name + ".json"), outcome)
                outcomes.append(outcome)
            rows.extend(compare(entry, *outcomes))
            phases.append(
                {
                    "baseline": entry["baseline"],
                    "phases": [
                        {
                            key: value
                            for key, value in outcome.items()
                            if key in ("incomplete", "metadata_exit_code", "route_exit_code", "selection")
                        }
                        for outcome in outcomes
                    ],
                }
            )
            write(args.output / "rows.json", rows)
        require_unchanged_oracles(frozen)
        if manifest(args.base, args.candidate, settings) != inputs:
            raise ValueError("Inputs changed during collection")
        write(
            args.output / "receipt.json",
            {"shard": args.shard, "manifest_sha256": digest(inputs), "phases": phases, "finished": True},
        )
    finally:
        require_unchanged_oracles(frozen)


def aggregate(root, settings):
    receipts = [read(p) for p in root.glob("*/receipt.json")]
    if len(receipts) != settings["shards"] or {r["shard"] for r in receipts} != set(range(settings["shards"])):
        raise ValueError("Missing or duplicated shard receipt")
    if len({r["manifest_sha256"] for r in receipts}) != 1 or not all(r["finished"] for r in receipts):
        raise ValueError("Shard inputs differ or execution was interrupted")
    manifests = [read(p) for p in root.glob("*/manifest.json")]
    if len(manifests) != settings["shards"] or any(
        digest(m) != receipts[0]["manifest_sha256"] or m["settings"] != settings for m in manifests
    ):
        raise ValueError("Missing or altered input manifest")
    rows = []
    for path in root.glob("*/receipt.json"):
        receipt = read(path)
        shard_rows = read(path.parent / "rows.json")
        expected_shard = {
            e["baseline"] + "|" + identity
            for e in manifests[0]["entries"]
            if e["shard"] == receipt["shard"]
            for identity in e["levels"]
        }
        if not expected_shard.issubset({r["identity"] for r in shard_rows}):
            raise ValueError("Shard omitted its assigned levels")
        rows.extend(shard_rows)
    expected = {entry["baseline"] + "|" + identity for entry in manifests[0]["entries"] for identity in entry["levels"]}
    if not expected.issubset({r["identity"] for r in rows}):
        raise ValueError("Result ledger omitted catalog levels")
    if len({r["identity"] for r in rows}) != len(rows):
        raise ValueError("Duplicate result identity")
    counts = {
        kind: sum(r["classification"] == kind for r in rows)
        for kind in ("unchanged", "changed", "improvement", "regression", "nondeterministic", "uncovered", "incomplete")
    }
    result = {
        "settings": settings,
        "counts": counts,
        "qualification": "manual_review_required",
        "all_catalog_covered": not counts["uncovered"] and not counts["incomplete"],
        "metadata_changes": sum(bool(r.get("metadata_changed")) for r in rows),
        "rows": rows,
    }
    write(root / "comparison.json", result)
    return counts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("gate", "run", "aggregate"))
    for name in ("base", "candidate", "assets", "output", "work"):
        parser.add_argument("--" + name, type=Path)
    parser.add_argument("--shard", type=int, choices=range(4))
    args = parser.parse_args()
    required = (
        ("base", "candidate", "assets", "output", "work", "shard")
        if args.command == "run"
        else ("output",)
        if args.command == "aggregate"
        else ()
    )
    for name in required:
        if getattr(args, name) is None:
            parser.error("--" + name + " is required")
    settings = config()
    if args.command == "gate":
        gate(settings, os.environ)
        with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as stream:
            for key in ("base", "candidate", "assets"):
                stream.write(key + "=" + settings[key] + "\n")
    elif args.command == "run":
        execute(args, settings)
    else:
        counts = aggregate(args.output, settings)
        print(json.dumps(counts, sort_keys=True))
        if counts["regression"] or counts["nondeterministic"] or counts["incomplete"]:
            raise SystemExit(1)


if __name__ == "__main__":
    main()
