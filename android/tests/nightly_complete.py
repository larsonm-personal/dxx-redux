"""Every-night admitted unattended coverage, bounded shards and positive receipts."""

import argparse
import json
import os
from pathlib import Path

import nightly_tooling_sweep as sweep
import nightly_retail as retail
import nightly_candidate as candidate


def plan_all(sha, catalog, policy, retail_assets=None):
    full, _ = sweep.plan_batch(sha, catalog, policy, retail_assets=retail_assets, complete=True)
    admitted = sweep.policy_entries(policy)
    frozen_policy = dict(policy, mission_corpus=False, suites=list(admitted.values()))
    groups = {}
    for entry in full["tests"]:
        # Public evidence must never share a private fixture executor or artifact
        key = entry["lane"], bool(entry.get("fixture"))
        groups.setdefault(key, []).append(entry)
    batches = []
    for key, entries in sorted(groups.items()):
        budget = 5400 if key[0] == "host" else 2700
        bins = []
        for entry in sorted(entries, key=lambda e: (-e["estimated_seconds"], e["id"])):
            bucket = next(
                (b for b in bins if sum(e["estimated_seconds"] for e in b) + entry["estimated_seconds"] <= budget), None
            )
            if bucket is None:
                bucket = []
                bins.append(bucket)
            bucket.append(entry)
        for bucket in bins:
            plan = dict(
                full,
                budget_seconds=budget,
                tests=bucket,
                # Global accounting belongs only in the manifest, not every shard
                exclusions=[],
                unavailable=[],
                lanes=[key[0]],
                estimated_seconds=sum(e["estimated_seconds"] for e in bucket),
            )
            if not key[1]:
                plan.pop("retail_assets", None)
            identity = {k: plan[k] for k in ("target_sha", "catalog_hash", "tests")}
            if plan.get("retail_assets"):
                identity["retail_assets"] = plan["retail_assets"]
            plan["batch_id"] = sweep.digest(identity)
            sweep.validate_plan(plan, frozen_policy)
            batches.append(plan)
    if len(batches) > 256:
        raise ValueError("Complete nightly exceeds GitHub matrix limit; partition the workflow before deployment")
    selected = {e["id"] for p in batches for e in p["tests"]}
    accounted = selected | {r["id"] for r in full["unavailable"]} | {r["id"] for r in full["exclusions"]}
    if set(sweep.policy_entries(policy)) - accounted:
        raise ValueError("Complete nightly silently omitted an admitted owner")
    return {
        "schema": 1,
        "target_sha": sha,
        "batches": batches,
        "exclusions": full["exclusions"],
        "unavailable": full["unavailable"],
        "selected_count": len(selected),
        "estimated_test_seconds": sum(p["estimated_seconds"] for p in batches),
        "estimated_lane_seconds": {
            lane: sum(p["estimated_seconds"] for p in batches if p["lanes"] == [lane]) for lane in sweep.LANES
        },
    }


def report(manifest, policy, receipts):
    policy = dict(policy, mission_corpus=False, suites=list(sweep.policy_entries(policy).values()))
    batches = manifest["batches"]
    ids = {p["batch_id"] for p in batches}
    if any(r.get("batch_id") not in ids for r in receipts):
        raise ValueError("Receipt from an unplanned shard")
    records, missing, incomplete = {}, [], []
    for plan in batches:
        state = sweep.reconcile(
            plan,
            policy,
            sweep.fresh_state(plan["target_sha"], plan["catalog_hash"]),
            [r for r in receipts if r.get("batch_id") == plan["batch_id"]],
        )
        records.update(state["records"])
        missing.extend(sweep.selected_without_pass(plan, state))
        incomplete.extend(
            e["id"] for e in plan["tests"] if state["records"].get(e["id"], {}).get("coverage_complete") is not True
        )
    pending = [row for row in manifest["unavailable"] if row.get("blocker_kind") != "target-capability"]
    return {
        "target_sha": manifest["target_sha"],
        "selected_owners": manifest["selected_count"],
        "passed_owners": sum(r["status"] == "PASS" for r in records.values()),
        "selected_without_pass": sorted(missing),
        "incomplete_owners": sorted(incomplete),
        "unavailable_pending": manifest["unavailable"],
        "excluded": manifest["exclusions"],
        "outcome": (
            "incomplete"
            if missing or incomplete or pending
            else "passed_with_exclusions"
            if manifest["unavailable"] or manifest["exclusions"]
            else "complete_pass"
        ),
    }


def write_artifact(path, value):
    sweep.write(path, value)
    # Fail planning before upload if a producer exceeds the consumer's read bound
    sweep.read(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("plan", "report"))
    args = parser.parse_args()
    policy = sweep.read(Path(__file__).with_name("nightly_tooling_policy.json"))
    if args.command == "plan":
        sha = os.environ["NIGHTLY_TARGET_SHA"]
        candidate.verify(sha)
        manifest = plan_all(sha, sweep.read("plan/catalog.json"), policy, retail.configured_identity())
        write_artifact("plan/complete.json", manifest)
        rows = []
        for index, plan in enumerate(manifest["batches"]):
            shard = f"shard-{index:03d}"
            write_artifact(f"plan/{shard}.json", plan)
            rows.append({"shard": shard, "lane": plan["lanes"][0], "retail": bool(plan.get("retail_assets"))})
        with open(os.environ["GITHUB_OUTPUT"], "a") as output:
            values = {
                "target_sha": sha,
                "lanes": sorted({r["lane"] for r in rows}),
                "android_lanes": [r["lane"] for r in rows if r["lane"] != "host"],
                "host_matrix": [r for r in rows if r["lane"] == "host"],
                "android_matrix": [r for r in rows if r["lane"] != "host"],
                "retail_lanes": sorted({r["lane"] for r in rows if r["retail"]}),
                "retail_commit": (retail.configured_identity() or {}).get("commit", ""),
                "count": manifest["selected_count"],
            }
            for key, value in values.items():
                output.write(f"{key}={json.dumps(value) if isinstance(value, list) else value}\n")
        return 0
    receipts = [sweep.read(p) for p in Path("receipts").glob("*/results.json")]
    summary = report(sweep.read("plan/complete.json"), policy, receipts)
    summary["supplemental_jobs"] = {
        key: os.environ.get(key, "missing")
        for key in (
            "PUMO_STAGE_RESULT",
            "RELEASE_COMPILE_RESULT",
            "HOST_RESULT",
            "ANDROID_RESULT",
            "EXECUTION_STACK_RESULT",
        )
    }
    if any(value != "success" for value in summary["supplemental_jobs"].values()):
        summary["outcome"] = "incomplete"
    write_artifact("summary.json", summary)
    print(json.dumps(summary, indent=2))
    return int(summary["outcome"] == "incomplete")


if __name__ == "__main__":
    raise SystemExit(main())
