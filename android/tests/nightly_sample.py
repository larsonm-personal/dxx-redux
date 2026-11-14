"""Resume a bounded nightly verification sample; traversal is not SHA coverage."""

import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import urllib.error

import nightly_candidate as candidate
import nightly_retail as retail
import nightly_tooling_sweep as sweep

CORE_IDS = frozenset(
    {
        "master:test_run_all_tests_catalog",
        "master:test_download_verification",
        "engine:ctest-d1",
        "master:test_gradle_unit_tests",
        "master:test_controller_overlay",
    }
)
SCHEMA = "dxx-nightly-traversal-v1"
STATE_FILE = "nightly-sample.json"
SUPPLEMENTAL = {
    "pumo-stage": {"id": "pumo-stage", "lane": "supplemental", "estimated_seconds": 1800},
    "release-compile": {"id": "release-compile", "lane": "supplemental", "estimated_seconds": 1800},
}


def name_hash(name):
    return hashlib.sha256(name.encode("utf-8")).hexdigest()


def category(entry):
    for name, ids in (
        ("online-d1", sweep.PUBLIC_ONLINE_IDS),
        ("online-d2", sweep.PUBLIC_ONLINE_D2_IDS),
        ("transports", sweep.ONLINE_TRANSPORT_IDS),
        ("three-player", sweep.THREE_PLAYER_IDS),
    ):
        if entry["id"] in ids:
            return name
    return sweep.owner_category(entry)


def hash_order(entries):
    # Same raw-name SHA256 ordering and grouped interleave as Get-RuntimeHashOrder
    # in runtime_targeted_sampling.ps1 (menu T); never invoke regeneration writers
    groups = {}
    for entry in entries:
        groups.setdefault(entry["lane"] + "|" + category(entry), []).append(entry)
    rings = [
        sorted(groups[key], key=lambda e: (name_hash(e["id"]), e["id"]))
        for key in sorted(groups, key=lambda key: (name_hash(key), key))
    ]
    return [ring[index] for index in range(max(map(len, rings), default=0)) for ring in rings if index < len(ring)]


def fresh_traversal():
    return {"schema": SCHEMA, "cycle": 0, "done": {}, "failures": {}}


def validate_traversal(state):
    if (
        state.get("schema") != SCHEMA
        or type(state.get("cycle")) is not int
        or state["cycle"] < 0
        or not isinstance(state.get("done"), dict)
        or not isinstance(state.get("failures"), dict)
    ):
        raise ValueError("Invalid nightly traversal state")
    for ident, row in state["done"].items():
        if (
            not isinstance(ident, str)
            or not isinstance(row, dict)
            or not isinstance(row.get("signature"), str)
            or len(row["signature"]) != 64
            or row.get("status") not in sweep.TERMINAL
            or not sweep.SHA.fullmatch(row.get("target_sha", ""))
        ):
            raise ValueError("Invalid completed traversal receipt")


def reconcile_catalog(state, entries):
    state = json.loads(json.dumps(state or fresh_traversal()))
    validate_traversal(state)
    signatures = {entry["id"]: sweep.digest(entry) for entry in entries}
    # Removed/changed owners lose their traversal credit; additions enter the same hash ring
    state["done"] = {ident: row for ident, row in state["done"].items() if signatures.get(ident) == row["signature"]}
    state["failures"] = {ident: row for ident, row in state["failures"].items() if ident in signatures}
    return state


def plan_sample(sha, catalog, policy, previous=None, retail_assets=None, now=None, core_ids=CORE_IDS):
    inventory, _ = sweep.plan_batch(sha, catalog, policy, retail_assets=retail_assets, complete=True)
    entries = {e["id"]: e for e in inventory["tests"]}
    if not core_ids <= entries.keys():
        raise ValueError("Required quick core is unavailable in the trusted catalog")
    extras = [SUPPLEMENTAL["release-compile"]]
    if retail_assets:
        extras.append(SUPPLEMENTAL["pumo-stage"])
    sampled = [e for e in entries.values() if e["id"] not in core_ids]
    state = reconcile_catalog(previous, sampled + list(SUPPLEMENTAL.values()))
    eligible = {e["id"] for e in sampled + extras}
    # A completed traversal wraps only when every currently eligible owner was actually attempted
    # Historical failures survive the wrap; none of this is current-revision coverage
    if eligible and eligible <= state["done"].keys():
        state["cycle"] += 1
        state["done"] = {key: value for key, value in state["done"].items() if key not in eligible}
    ordered = hash_order(sampled)
    selected = [entries[key] for key in sorted(core_ids)]
    used = {lane: sum(e["estimated_seconds"] for e in selected if e["lane"] == lane) for lane in sweep.LANES}
    pending_order = {}
    for lane in sweep.LANES:
        pending = [e for e in ordered if e["lane"] == lane and e["id"] not in state["done"]]
        pending_order[lane] = [e["id"] for e in pending]
        for entry in pending:
            # Do not jump past the next undone owner merely because a later one is cheaper
            if used[lane] + entry["estimated_seconds"] > 2700 and any(
                e["lane"] == lane and e["id"] not in core_ids for e in selected
            ):
                break
            if (entry["id"] in sweep.THREE_PLAYER_IDS and retail.lane_requires_retail({"tests": selected}, lane)) or (
                entry.get("fixture") and any(e["id"] in sweep.THREE_PLAYER_IDS and e["lane"] == lane for e in selected)
            ):
                break
            selected.append(entry)
            used[lane] += entry["estimated_seconds"]
    extra = next((e["id"] for e in hash_order(extras) if e["id"] not in state["done"]), "")
    plan = dict(
        inventory,
        tests=selected,
        lanes=[lane for lane in sweep.LANES if used[lane]],
        estimated_seconds=sum(used.values()),
        budget_seconds=max(2700, max(used.values())),
    )
    identity = {key: plan[key] for key in ("target_sha", "catalog_hash", "tests")}
    if plan.get("retail_assets"):
        identity["retail_assets"] = plan["retail_assets"]
    plan["batch_id"] = sweep.digest(identity)
    now = int(time.time()) if now is None else now
    plan["sampling"] = {
        "schema": SCHEMA,
        "core_ids": sorted(core_ids),
        "cycle": state["cycle"],
        "order": "Get-RuntimeHashOrder SHA256(name), grouped interleave",
        "pending_order": pending_order,
        "supplemental": extra,
        "started_unix": now,
        "deadline_unix": now + 3000,
        "eligible_owners": len(entries),
        "traversed_owners": len(state["done"]),
        "selected_sample_ids": [e["id"] for e in selected if e["id"] not in core_ids],
    }
    sweep.validate_plan(plan, policy)
    return plan, state


def reconcile_sample(plan, policy, previous, receipts, supplemental_result="", run_id="local"):
    # Reconcile against a FRESH exact-SHA coverage state, never traversal from another revision
    coverage = sweep.reconcile(plan, policy, sweep.fresh_state(plan["target_sha"], plan["catalog_hash"]), receipts)
    entries = sweep.policy_entries(policy)
    state = reconcile_catalog(previous, list(entries.values()) + list(SUPPLEMENTAL.values()))
    core = set(plan["sampling"]["core_ids"])
    for ident, row in coverage["records"].items():
        if ident in core or row.get("attempted_tests") is False or row.get("phase") != "tests":
            continue
        state["done"][ident] = {
            "signature": sweep.digest(entries[ident]),
            "target_sha": plan["target_sha"],
            "status": row["status"],
            "run_id": run_id,
        }
        if row["status"] in ("FAIL", "TIMEOUT"):
            state["failures"][ident] = state["done"][ident]
        elif row["status"] == "PASS":
            state["failures"].pop(ident, None)
    extra = plan["sampling"]["supplemental"]
    # A failed setup-only supplemental job has no execution receipt and must remain undone
    if extra and supplemental_result == "success":
        state["done"][extra] = {
            "signature": sweep.digest(SUPPLEMENTAL[extra]),
            "target_sha": plan["target_sha"],
            "status": "PASS",
            "run_id": run_id,
        }
        state["failures"].pop(extra, None)
    state["last_target_sha"] = plan["target_sha"]
    state["last_batch_id"] = plan["batch_id"]
    return state, coverage


def read_remote():
    try:
        head = sweep.github_api("git/ref/heads/" + sweep.STATE_BRANCH)["object"]["sha"]
    except urllib.error.HTTPError as error:
        if error.code == 404:
            return None, fresh_traversal()
        raise
    try:
        content = sweep.github_api("contents/" + STATE_FILE + "?ref=" + head)
    except urllib.error.HTTPError as error:
        if error.code == 404:
            return head, fresh_traversal()
        raise
    if content.get("size", 0) > 900000 or content.get("encoding") != "base64":
        raise ValueError("Traversal ledger exceeds compact JSON limit")
    state = json.loads(base64.b64decode(content["content"]))
    validate_traversal(state)
    return head, state


def write_artifact(path, value):
    sweep.write(path, value)
    sweep.read(path)


def publish(plan, policy, receipts, supplemental_result):
    # Only this trusted, isolated job has write credentials; preserve the legacy ledger file
    for attempt in range(2):
        head, previous = read_remote()
        # Reapply the planner's cycle transition, without adopting any untrusted progress artifact
        if previous["cycle"] != plan["sampling"]["cycle"]:
            if previous["cycle"] + 1 != plan["sampling"]["cycle"]:
                raise ValueError("Traversal cycle changed during execution")
            previous["cycle"] += 1
            selected_pool = {item for values in plan["sampling"]["pending_order"].values() for item in values}
            previous["done"] = {
                key: value
                for key, value in previous["done"].items()
                if key not in selected_pool and key not in SUPPLEMENTAL
            }
        state, _ = reconcile_sample(plan, policy, previous, receipts, supplemental_result, os.environ["GITHUB_RUN_ID"])
        serialized = json.dumps(state, separators=(",", ":")) + "\n"
        if len(serialized.encode()) > 900000:
            raise ValueError("Traversal ledger exceeds compact JSON limit")
        blob = sweep.github_api("git/blobs", "POST", {"content": serialized, "encoding": "utf-8"})
        tree_args = {"tree": [{"path": STATE_FILE, "mode": "100644", "type": "blob", "sha": blob["sha"]}]}
        if head:
            tree_args["base_tree"] = sweep.github_api("git/commits/" + head)["tree"]["sha"]
        tree = sweep.github_api("git/trees", "POST", tree_args)
        commit = sweep.github_api(
            "git/commits",
            "POST",
            {
                "message": "Record nightly sample traversal " + plan["target_sha"],
                "tree": tree["sha"],
                "parents": [head] if head else [],
            },
        )
        try:
            if head:
                sweep.github_api(
                    "git/refs/heads/" + sweep.STATE_BRANCH, "PATCH", {"sha": commit["sha"], "force": False}
                )
            else:
                sweep.github_api("git/refs", "POST", {"ref": "refs/heads/" + sweep.STATE_BRANCH, "sha": commit["sha"]})
            return state
        except urllib.error.HTTPError as error:
            if attempt or error.code not in (409, 422):
                raise
    raise RuntimeError("Traversal publication conflict")


def phase_plan(plan, phase):
    result = dict(plan)
    core = set(plan["sampling"]["core_ids"])
    result["tests"] = [e for e in plan["tests"] if (e["id"] in core) == (phase == "core")]
    result["sample_phase"] = phase == "sample"
    result["estimated_seconds"] = sum(e["estimated_seconds"] for e in result["tests"])
    identity = {key: result[key] for key in ("target_sha", "catalog_hash", "tests")}
    if result.get("retail_assets"):
        identity["retail_assets"] = result["retail_assets"]
    result["batch_id"] = sweep.digest(identity)
    return result


def collect_sample(plan, policy, lane, output, blocked_reason=""):
    receipt = {key: plan[key] for key in ("schema", "target_sha", "catalog_hash", "batch_id")}
    if plan.get("retail_assets"):
        receipt["retail_assets"] = plan["retail_assets"]
    receipt.update(lane=lane, results=[])
    for phase in ("core", "sample"):
        rows = sweep.collect(phase_plan(plan, phase), policy, lane, output / phase, blocked_reason)
        receipt["results"].extend(rows["results"])
    write_artifact(output / "results.json", receipt)
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("plan", "report", "publish", "execute", "collect", "check-deadline"))
    parser.add_argument("--plan", default="plan/plan.json")
    parser.add_argument("--lane", choices=sweep.LANES)
    parser.add_argument("--blocked-reason", default="")
    args = parser.parse_args()
    policy = sweep.read(Path(__file__).with_name("nightly_tooling_policy.json"))
    if args.command == "plan":
        sha = os.environ["NIGHTLY_TARGET_SHA"]
        candidate.verify(sha)
        _, previous = read_remote()
        plan, state = plan_sample(sha, sweep.read("plan/catalog.json"), policy, previous, retail.configured_identity())
        write_artifact(args.plan, plan)
        write_artifact("plan/traversal.json", state)
        with open(os.environ["GITHUB_OUTPUT"], "a") as output:
            values = {
                "target_sha": sha,
                "lanes": plan["lanes"],
                "android_lanes": [lane for lane in plan["lanes"] if lane != "host"],
                "host_matrix": [{"shard": "", "lane": "host", "retail": retail.lane_requires_retail(plan, "host")}],
                "android_matrix": [
                    {"shard": "", "lane": lane, "retail": retail.lane_requires_retail(plan, lane)}
                    for lane in plan["lanes"]
                    if lane != "host"
                ],
                "retail_lanes": [lane for lane in plan["lanes"] if retail.lane_requires_retail(plan, lane)],
                "retail_commit": (plan.get("retail_assets") or {}).get("commit", ""),
                "supplemental": plan["sampling"]["supplemental"],
                "count": len(plan["tests"]),
            }
            for key, value in values.items():
                output.write(f"{key}={json.dumps(value) if isinstance(value, list) else value}\n")
        return 0
    plan = sweep.read(args.plan)
    sweep.validate_plan(plan, policy)
    if args.command == "check-deadline":
        return int(time.time() >= plan["sampling"]["deadline_unix"] - 120)
    if args.command in ("execute", "collect"):
        if not args.lane:
            raise ValueError("Execution requires a lane")
        if args.command == "collect":
            collect_sample(plan, policy, args.lane, Path("results"), args.blocked_reason)
            return 0
        failed = 0
        for phase in ("core", "sample"):
            subplan = phase_plan(plan, phase)
            if not any(e["lane"] == args.lane for e in subplan["tests"]):
                continue
            path = Path("plan") / (phase + ".json")
            write_artifact(path, subplan)
            remaining = plan["sampling"]["deadline_unix"] - time.time() - 120
            command = [
                sys.executable,
                str(Path(sweep.__file__).resolve()),
                "execute",
                "--plan",
                str(path),
                "--policy",
                str(Path(__file__).with_name("nightly_tooling_policy.json").resolve()),
                "--lane",
                args.lane,
                "--repo",
                "target",
                "--output",
                "results/" + phase,
            ]
            try:
                if remaining <= 0:
                    break
                failed |= int(sweep.run_command(command, Path.cwd(), sys.stdout, timeout=remaining).returncode != 0)
            except subprocess.TimeoutExpired:
                print("Nightly deadline reached; unfinished owners remain pending")
                break
            finally:
                collect_sample(plan, policy, args.lane, Path("results"))
        return failed
    receipts = [sweep.read(p) for p in Path("receipts").glob("*/results.json")]
    extra = plan["sampling"]["supplemental"]
    result = os.environ.get("PUMO_STAGE_RESULT" if extra == "pumo-stage" else "RELEASE_COMPILE_RESULT", "")
    if args.command == "publish":
        if plan["target_sha"] != os.environ["TARGET_SHA"]:
            raise ValueError("Publication target mismatch")
        write_artifact("published-state.json", publish(plan, policy, receipts, result))
        return 0
    state, coverage = reconcile_sample(
        plan, policy, sweep.read("plan/traversal.json"), receipts, result, os.environ.get("GITHUB_RUN_ID", "local")
    )
    core = plan["sampling"]["core_ids"]
    records = coverage["records"]
    missing_core = [ident for ident in core if records.get(ident, {}).get("status") != "PASS"]
    failures = [ident for ident, row in records.items() if row["status"] in ("FAIL", "TIMEOUT")]
    summary = {
        "target_sha": plan["target_sha"],
        "batch_id": plan["batch_id"],
        "sampling": plan["sampling"],
        "core_without_pass": missing_core,
        "current_sha_records": records,
        "failures": failures,
        "sample_pending": [ident for ident in plan["sampling"]["selected_sample_ids"] if ident not in records],
        "unavailable": plan["unavailable"],
        "excluded": plan["exclusions"],
        "historical_failures_not_current_sha_coverage": state["failures"],
        "supplemental": {"selected": extra, "result": result if extra else "not-selected"},
        "outcome": "failed" if missing_core or failures or (extra and result != "success") else "sample_pass",
    }
    write_artifact("summary.json", summary)
    write_artifact("summary-state.json", coverage)
    print(json.dumps(summary, indent=2))
    return int(summary["outcome"] == "failed")


if __name__ == "__main__":
    raise SystemExit(main())
