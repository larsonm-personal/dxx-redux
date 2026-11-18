"""On-demand original level census using the ordinary fixed-frame readiness probe."""

import argparse
import json
from pathlib import Path
import shutil
import tempfile

import xl_readiness as readiness
from xl_readiness_report import validate, validate_rows


def measure(args):
    catalog = json.loads(args.catalog.read_text())
    local = catalog.get("schema") == "dxx-xl-local-inventory-v1"
    if local:
        if catalog["asset_commit"] is not None:
            raise ValueError("Local inventories use per-archive hashes")
    elif catalog["asset_commit"] != readiness.ASSET_COMMIT:
        raise ValueError("Unexpected pinned inventory revision")
    cases = catalog["cases"]
    if args.archive:
        cases = [row for row in cases if Path(row["archive"]).name == args.archive]
    if args.case:
        requested = set(args.case)
        cases = [row for row in cases if row["id"] in requested]
        if len(cases) != len(requested):
            raise ValueError("Unknown survey case")
    if not cases or len({row["id"] for row in cases}) != len(cases):
        raise ValueError("Empty or duplicate survey selection")
    identity = {
        "schema": "dxx-xl-full-survey-v1",
        "target_sha": args.target_sha,
        "engine_sha256": readiness.digest(args.engine),
        "inventory_sha256": readiness.digest(args.catalog),
        "asset_commit": catalog["asset_commit"],
        "resources": readiness.verify_resources(args.data),
        "selected": [row["id"] for row in cases],
    }
    args.work.mkdir(parents=True, exist_ok=False)
    readiness.write(args.work / "survey.json", identity)
    rows = []
    with tempfile.TemporaryDirectory(dir=args.work.parent, prefix="xl-full-cache-") as cache:
        for index, case in enumerate(cases):
            with tempfile.TemporaryDirectory(dir=args.work.parent, prefix="xl-full-case-") as temp:
                work = Path(temp) / "run"
                selected = {**catalog, "selected": [case["id"]]}
                result = readiness.run(
                    args.engine,
                    args.assets,
                    args.data,
                    selected,
                    work,
                    args.seven_zip,
                    args.target_sha,
                    require_resources=True,
                    assisted_routes=False,
                    unpack_cache=cache,
                )
                if local:
                    validate_rows(result["rows"], selected)
                else:
                    validate(result, selected, args.target_sha, require_resources=True)
                row = result["rows"][0]
                receipt = args.work / str(index)
                receipt.mkdir()
                for name in ("load.json", "engine.log"):
                    source = work / "0" / name
                    if source.is_file():
                        shutil.copyfile(source, receipt / name)
                readiness.write(receipt / "result.json", row)
                rows.append(row)
                print(f"{index + 1}/{len(cases)} {row['outcome']} {row['id']}", flush=True)
    readiness.verify_resources(args.data)
    summary = readiness.summary(rows)
    readiness.write(args.work / "results.json", {**identity, "rows": rows, "summary": summary})
    failures = [row for row in rows if row["outcome"] != "loaded_ticked"]
    readiness.write(args.work / "failures.json", failures)
    return int(bool(failures))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("engine", "assets", "data", "work"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--target-sha", required=True)
    parser.add_argument("--catalog", type=Path, default=Path(__file__).with_name("xl_readiness_inventory.json"))
    parser.add_argument("--seven-zip", type=Path)
    parser.add_argument("--archive", help="Survey only the named archive")
    parser.add_argument("--case", action="append", help="Survey a specific inventory ID (repeatable)")
    return measure(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
