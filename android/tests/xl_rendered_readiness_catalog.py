"""Run a supplemental windowed survey of unmodified, hash-pinned mission cases."""

import argparse
from itertools import groupby
from pathlib import Path
import tempfile

from xl_readiness import digest, stage_case, verify_resources, write
from xl_readiness_full import select_catalog_cases
from xl_rendered_readiness import run_probe


def measure(args):
    catalog, cases = select_catalog_cases(args.catalog, args.archive, args.case)
    archives = {row["path"]: row for row in catalog["archives"]}
    engine_hash = digest(args.engine)
    identity = {
        "schema": "dxx-xl-windowed-catalog-v1",
        "target_sha": args.target_sha,
        "engine_sha256": engine_hash,
        "inventory_sha256": digest(args.catalog),
        "resources": verify_resources(args.data),
        "selected": [case["id"] for case in cases],
    }
    args.work.mkdir(parents=True, exist_ok=False)
    write(args.work / "survey.json", identity)
    rows = []
    for archive_name, group in groupby(cases, key=lambda case: case["archive"]):
        with tempfile.TemporaryDirectory(dir=args.work.parent, prefix="xl-windowed-cache-") as cache:
            for case in group:
                receipt = args.work / str(len(rows))
                row = {
                    "id": case["id"],
                    "mission": case["mission"],
                    "level": case["level"],
                    "level_file": case["level_file"],
                    "entered_gameplay": False,
                    "survived_120_frames": False,
                }
                try:
                    with tempfile.TemporaryDirectory(dir=args.work.parent, prefix="xl-windowed-case-") as area:
                        extra = stage_case(args.assets, archives[archive_name], case, Path(area), args.seven_zip, cache)
                        result = run_probe(
                            args.engine,
                            args.data,
                            extra,
                            receipt,
                            case["mission"],
                            case["level"],
                            case["level_file"],
                            engine_hash,
                            quiet=True,
                            timeout=args.timeout,
                        )
                    proof = result["proof"]
                    row.update(
                        entered_gameplay=result["entered_gameplay"],
                        survived_120_frames=result["passed"],
                        phase=proof.get("phase"),
                        exit_code=result["exit_code"],
                        frames=proof.get("frames"),
                        rendered_frames=proof.get("rendered_frames"),
                    )
                    if "stop" in proof:
                        row["stop"] = proof["stop"]
                except (OSError, RuntimeError, ValueError) as error:
                    receipt.mkdir(exist_ok=True)
                    row["measurement_error"] = str(error)
                write(receipt / "case.json", row)
                rows.append(row)
                print(f"{len(rows)}/{len(cases)} {row.get('phase') or 'unverified'} {case['id']}", flush=True)
    if digest(args.engine) != engine_hash or verify_resources(args.data) != identity["resources"]:
        raise ValueError("Windowed survey inputs changed during measurement")
    summary = {
        "measured": len(rows),
        "entered_gameplay": sum(r["entered_gameplay"] for r in rows),
        "survived_120_frames": sum(r["survived_120_frames"] for r in rows),
    }
    write(args.work / "results.json", {**identity, "rows": rows, "summary": summary})
    return int(summary["survived_120_frames"] != len(cases))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("engine", "assets", "data", "catalog", "work"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--seven-zip", type=Path)
    parser.add_argument("--target-sha", required=True)
    parser.add_argument("--archive", help="Select an archive by basename")
    parser.add_argument("--case", action="append", help="Select an exact inventory ID (repeatable)")
    parser.add_argument("--timeout", type=int, default=120, help="Wall-clock timeout per flight in seconds")
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    return measure(args)


if __name__ == "__main__":
    raise SystemExit(main())
