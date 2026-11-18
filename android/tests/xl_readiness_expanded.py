"""Fixed expanded original-content survey; compatibility failures remain visible."""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time

import xl_readiness as readiness
import xl_readiness_report as reporter

HERE = Path(__file__).resolve().parent
SEED = "d2x-readiness-expanded-v1"
# Nominations come from the original descriptor inventory and structural metadata,
# never from observed engine outcomes. Distinct package contexts remain distinct.
NOMINATIONS = (
    ("po2.7z/missions/single/pluton2.mn2:1", "full-HAM/custom resource package"),
    ("bahagad.7z/missions/single/bahaeasy.mn2:1", "alternate easy mission descriptor"),
    ("maximum.7z/missions/single/maxlink.mn2:1", "multiplayer mission descriptor"),
    ("saturn.7z/SATURN_E.MN2:4", "texture and robot-generator density"),
    ("harqyjia.7z/harqyjia.MN2:4", "trigger density"),
    ("revodrav.7z/missions/single/revodrav.msn:20", "D1 trigger density"),
    ("diehard.7z/missions/single/diehard.mn2:4", "object density"),
    ("levelpack.7z/missions/single/alioth.mn2:1", "large geometry and wall count"),
    ("levelpack.7z/missions/kruel/complexo.mn2:1", "large geometry in separate package directory"),
    ("levelpack.7z/missions/cp5670/lsrbase3.mn2:1", "object density in separate package directory"),
    ("levelpack.7z/missions/single/ewithin.mn2:4", "large campaign geometry"),
    ("levelpack.7z/missions/single/ewithin.mn2:-2", "secret-level wall density"),
    ("ironblade.7z/missions/geld/ironblad.msn:2", "D1 robot-generator density"),
)


def rank(value):
    return hashlib.sha256((SEED + ":" + value).encode()).hexdigest()


def cohort(catalog_path):
    catalog = json.loads(catalog_path.read_text())
    cases = {r["id"]: r for r in catalog["cases"]}
    nominations = dict(NOMINATIONS)
    if not nominations.keys() <= cases.keys():
        raise ValueError("Unknown nominated original IDs: " + str(nominations.keys() - cases.keys()))
    selected = dict.fromkeys(catalog["selected"], "original eight-case baseline")
    for key, row in cases.items():
        if Path(row["archive"]).name in readiness.NEW_CAMPAIGNS:
            selected.setdefault(key, "complete focal campaign")
    for archive in catalog["archives"]:
        if any(cases[key]["archive"] == archive["path"] for key in selected):
            continue
        candidates = [key for key, row in cases.items() if row["archive"] == archive["path"] and key not in nominations]
        selected[min(candidates, key=rank)] = "additional archive representative"
    if len(selected) != 39:
        raise ValueError("Archive/focal selection changed")
    secrets = sorted(
        (key for key, row in cases.items() if row["level"] < 0 and key not in selected and key not in nominations),
        key=rank,
    )
    for key in secrets[:12]:
        selected[key] = "additional secret level"
    for key, reason in NOMINATIONS:
        if key in selected:
            raise ValueError("Nomination overlaps fixed strata: " + key)
        selected[key] = reason
    if len(selected) != 64:
        raise ValueError("Expanded cohort must contain 64 unique entries")
    return {
        "schema": "dxx-xl-expanded-cohort-v1",
        "selection_seed": SEED,
        "inventory_sha256": readiness.digest(catalog_path),
        "asset_commit": readiness.ASSET_COMMIT,
        "original_selected": catalog["selected"],
        "selected": [{"id": key, "reason": reason} for key, reason in selected.items()],
    }


def stop(process):
    if process.poll() is not None:
        return
    if os.name == "nt":
        subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"], capture_output=True, check=True)
    else:
        os.killpg(process.pid, signal.SIGKILL)
    process.wait()


def catalog_view(catalog_path, cohort_path):
    frozen = json.loads(cohort_path.read_text())
    if frozen != cohort(catalog_path):
        raise ValueError("Expanded cohort differs from its frozen selection")
    catalog = json.loads(catalog_path.read_text())
    return {**catalog, "selection_seed": SEED, "selected": [r["id"] for r in frozen["selected"]]}


def validate(receipt, catalog_path, cohort_path, sha):
    catalog = catalog_view(catalog_path, cohort_path)
    result = reporter.validate(receipt, catalog, sha, require_resources=True)
    budget = receipt.get("budget_seconds")
    elapsed = receipt.get("elapsed_seconds")
    if (
        receipt.get("cohort_sha256") != readiness.digest(cohort_path)
        or type(budget) not in (int, float)
        or not math.isfinite(budget)
        or not 0 < budget <= 600
        or type(elapsed) not in (int, float)
        or not math.isfinite(elapsed)
        or elapsed < 0
        or type(receipt.get("budget_exhausted")) is not bool
        or any(r["assisted_navigation"] != "not_run" for r in receipt["rows"])
    ):
        raise ValueError("Invalid expanded measurement identity or budget")
    original = json.loads(catalog_path.read_text())["selected"]
    baseline = readiness.summary([r for r in receipt["rows"] if r["id"] in original])
    if receipt.get("summary") != result or receipt.get("original_eight_summary") != baseline:
        raise ValueError("Expanded summary differs from actual outcomes")
    return result


def publish(args):
    if args.publish_status:
        reporter.candidate.verify(args.target_sha)
        reporter.candidate.STATUS_CONTEXT = "D2X readiness (expanded 64 load/tick)"
        if os.environ.get("READINESS_RESULT") != "success":
            reporter.candidate.status(
                args.target_sha, "error", "Expanded readiness unavailable: measurement job did not succeed"
            )
            return 2
    if args.receipt.is_symlink() or args.receipt.stat().st_size > 1024 * 1024:
        raise ValueError("Invalid expanded receipt file")
    receipt = json.loads(args.receipt.read_text())
    result = validate(receipt, args.catalog, args.cohort, args.target_sha)
    text = [
        "## Expanded D2X readiness",
        "",
        f"{result['passed']}/64 original levels confirmed; complete={result['complete']}.",
        f"Probe time: {receipt['elapsed_seconds']} seconds; budget: {receipt['budget_seconds']} seconds; exhausted={receipt['budget_exhausted']}.",
        "Original eight-case CI status remains separate. These are load/120-frame checks, not campaign or rendering qualification.",
        "No expected-failure waiver or passing-case substitution is applied.",
        "",
        "| Case | Outcome |",
        "|---|---|",
    ]
    for row in receipt["rows"]:
        identity = reporter.html.escape(row["id"]).replace("|", "\\|").replace("`", "")
        text.append(f"| {identity} | {row['outcome']} |")
    output = "\n".join(text) + "\n"
    print(output)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as stream:
            stream.write(output)
    if args.publish_status:
        reporter.candidate.status(
            args.target_sha,
            "success" if result["complete"] and result["passed"] == 64 else "failure",
            f"Expanded readiness {result['passed']}/64; " + ("complete" if result["complete"] else "incomplete"),
        )
    # Measurement integrity is separate from compatibility; the distinct status
    # above remains unsuccessful for every failing or untested case.
    return 0 if result["complete"] else 2


def measure(args):
    frozen = json.loads(args.cohort.read_text())
    if frozen != cohort(args.catalog):
        raise ValueError("Expanded cohort differs from its frozen selection")
    catalog = catalog_view(args.catalog, args.cohort)
    args.work.mkdir(parents=True, exist_ok=False)
    resources = readiness.verify_resources(args.data)
    started = time.monotonic()
    rows = [{"id": key, "outcome": "untested", "assisted_navigation": "not_run"} for key in catalog["selected"]]
    report = {
        "schema": "dxx-xl-readiness-v1",
        "target_sha": args.target_sha,
        "asset_commit": readiness.ASSET_COMMIT,
        "resources": resources,
        "selection_seed": SEED,
        "inventory_levels": len(catalog["cases"]),
        "inventory_archives": len(catalog["archives"]),
        "cohort_sha256": readiness.digest(args.cohort),
        "rows": rows,
        "budget_seconds": args.budget_seconds,
        "budget_exhausted": False,
        "limitations": "Fixed expanded load/tick survey; no assisted routes, rendering or campaign qualification",
    }

    def checkpoint():
        report["summary"] = readiness.summary(rows)
        report["original_eight_summary"] = readiness.summary(
            [r for r in rows if r["id"] in frozen["original_selected"]]
        )
        report["elapsed_seconds"] = round(time.monotonic() - started, 3)
        readiness.write(args.work / "results.json", report)

    checkpoint()
    with tempfile.TemporaryDirectory(prefix="xl-readiness-cache-", dir=args.work.parent) as cache:
        for index, row in enumerate(rows):
            remaining = args.budget_seconds - (time.monotonic() - started)
            # Do not knowingly start a primary probe without its full watchdog window.
            if remaining < 45:
                report["budget_exhausted"] = True
                break
            with tempfile.TemporaryDirectory(prefix="case-", dir=args.work) as temporary:
                root = Path(temporary)
                one = {**catalog, "selected": [row["id"]]}
                readiness.write(root / "catalog.json", one)
                command = [
                    sys.executable,
                    str(HERE / "xl_readiness.py"),
                    "run",
                    "--catalog",
                    str(root / "catalog.json"),
                    "--engine",
                    str(args.engine),
                    "--assets",
                    str(args.assets),
                    "--data",
                    str(args.data),
                    "--work",
                    str(root / "run"),
                    "--target-sha",
                    args.target_sha,
                    "--require-resources",
                    "--no-assisted-routes",
                    "--unpack-cache",
                    cache,
                ]
                if args.seven_zip:
                    command += ["--seven-zip", str(args.seven_zip)]
                with (root / "worker.log").open("wb") as log:
                    process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
                    try:
                        code = process.wait(timeout=max(0.001, args.budget_seconds - (time.monotonic() - started)))
                    except subprocess.TimeoutExpired:
                        stop(process)
                        report["budget_exhausted"] = True
                        code = None
                    finally:
                        stop(process)
                evidence = args.work / str(index)
                evidence.mkdir()
                for source in [root / "worker.log", root / "run/0/engine.log", root / "run/0/load.json"]:
                    if source.is_file():
                        with source.open("rb") as stream:
                            stream.seek(max(0, source.stat().st_size - 65536))
                            (evidence / source.name).write_bytes(stream.read(65536))
                if code is None:
                    # Budget cancellation is not an engine watchdog failure or a pass.
                    break
                result = root / "run/results.json"
                if code not in (0, 2) or not result.exists():
                    row.update(next(c for c in catalog["cases"] if c["id"] == row["id"]))
                    row["outcome"] = "infrastructure_error"
                else:
                    receipt = json.loads(result.read_text())
                    reporter.validate(receipt, one, args.target_sha, require_resources=True)
                    row.update(receipt["rows"][0])
            checkpoint()
    readiness.verify_resources(args.data)
    checkpoint()
    print(
        json.dumps(
            {
                "summary": report["summary"],
                "elapsed_seconds": report["elapsed_seconds"],
                "budget_exhausted": report["budget_exhausted"],
            },
            sort_keys=True,
        )
    )
    return 0 if report["summary"]["complete"] else 2


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("freeze", "prepare", "run", "report"))
    parser.add_argument("--catalog", type=Path, default=HERE / "xl_readiness_inventory.json")
    parser.add_argument("--cohort", type=Path, default=HERE / "xl_readiness_expanded_cohort.json")
    for name in ("engine", "assets", "data", "work", "seven-zip"):
        parser.add_argument("--" + name, type=Path)
    parser.add_argument("--target-sha")
    parser.add_argument("--resources", type=Path)
    parser.add_argument("--receipt", type=Path)
    parser.add_argument("--publish-status", action="store_true")
    parser.add_argument(
        "--budget-seconds",
        type=float,
        default=360,
        help="Probe budget; default reserves four minutes of a ten-minute job for setup",
    )
    args = parser.parse_args()
    if args.command == "freeze":
        value = cohort(args.catalog)
        if not args.cohort.exists() or json.loads(args.cohort.read_text()) != value:
            args.cohort.write_text(json.dumps(value, indent=4, sort_keys=True) + "\n")
        return 0
    if args.command == "prepare":
        if not all((args.assets, args.data, args.resources, args.work)):
            parser.error("prepare requires assets, data, resources and work")
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "catalog.json"
            readiness.write(path, catalog_view(args.catalog, args.cohort))
            return subprocess.call(
                [
                    sys.executable,
                    str(HERE / "xl_readiness.py"),
                    "prepare",
                    "--catalog",
                    str(path),
                    "--assets",
                    str(args.assets),
                    "--data",
                    str(args.data),
                    "--resources",
                    str(args.resources),
                    "--work",
                    str(args.work),
                ]
            )
    if args.command == "report":
        if not args.receipt or not args.target_sha:
            parser.error("report requires receipt and target-sha")
        return publish(args)
    if not math.isfinite(args.budget_seconds) or not 0 < args.budget_seconds <= 600:
        parser.error("Probe budget must be positive and at most 600 seconds")
    if not all((args.engine, args.assets, args.data, args.work, args.target_sha)):
        parser.error("run requires engine, assets, data, work and target-sha")
    return measure(args)


if __name__ == "__main__":
    raise SystemExit(main())
