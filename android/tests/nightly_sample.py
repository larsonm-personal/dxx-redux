"""Resume a bounded nightly verification sample; traversal is not SHA coverage."""

import argparse
import base64
import hashlib
import json
import os
import re
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
    excluded = state.get("runtime_excluded", {})
    if not isinstance(excluded, dict):
        raise ValueError("Invalid runtime exclusion ledger")
    for ident, row in excluded.items():
        if (
            not isinstance(ident, str)
            or not isinstance(row, dict)
            or not isinstance(row.get("signature"), str)
            or len(row["signature"]) != 64
            or not isinstance(row.get("reason"), str)
            or not sweep.SHA.fullmatch(row.get("target_sha", ""))
        ):
            raise ValueError("Invalid runtime exclusion receipt")
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


def reconcile_catalog(state, entries, capability=None):
    state = json.loads(json.dumps(state or fresh_traversal()))
    validate_traversal(state)
    signatures = {entry["id"]: sweep.digest(entry) for entry in entries}
    # Removed/changed owners lose their traversal credit; additions enter the same hash ring
    state["done"] = {ident: row for ident, row in state["done"].items() if signatures.get(ident) == row["signature"]}
    state["failures"] = {ident: row for ident, row in state["failures"].items() if ident in signatures}
    definitions = {entry["id"]: entry for entry in entries}
    state["runtime_excluded"] = {
        ident: row
        for ident, row in state.get("runtime_excluded", {}).items()
        if ident in definitions and row.get("signature") == exclusion_signature(definitions[ident], capability)
    }
    return state


def capability_context(sha, retail_assets):
    root = Path(__file__).parent
    controllers = ("nightly_regression_catalog.py", "run_host_test_suites.py", "../helpers/retail_ci_assets.py")
    return {
        "target_sha": sha,
        "retail_assets": retail_assets,
        "controller_capabilities": {
            name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in controllers
        },
    }


def exclusion_signature(entry, capability):
    return sweep.digest({"entry": entry, "capability": capability})


def plan_sample(
    sha, catalog, policy, previous=None, retail_assets=None, now=None, core_ids=CORE_IDS, host_workers=1, refill=False
):
    if type(host_workers) is not int or not 1 <= host_workers <= 16:
        raise ValueError("Host concurrency must be between one and sixteen")
    inventory, _ = sweep.plan_batch(sha, catalog, policy, retail_assets=retail_assets, complete=True)
    entries = {e["id"]: e for e in inventory["tests"]}
    if not core_ids <= entries.keys():
        raise ValueError("Required quick core is unavailable in the trusted catalog")
    extras = [SUPPLEMENTAL["release-compile"]]
    if retail_assets:
        extras.append(SUPPLEMENTAL["pumo-stage"])
    sampled = [e for e in entries.values() if e["id"] not in core_ids]
    capability = capability_context(sha, retail_assets)
    state = reconcile_catalog(previous, sampled + list(SUPPLEMENTAL.values()), capability)
    eligible = {e["id"] for e in sampled + extras}
    # Wrap after every eligible owner was attempted or has a scoped capability exclusion
    # Historical failures survive the wrap; none of this is current-revision coverage
    if eligible and eligible <= (state["done"].keys() | state["runtime_excluded"].keys()):
        state["cycle"] += 1
        state["runtime_excluded"] = {}
        state["done"] = {key: value for key, value in state["done"].items() if key not in eligible}
    ordered = hash_order(sampled)
    selected = [entries[key] for key in sorted(core_ids)]
    used = {lane: sum(e["estimated_seconds"] for e in selected if e["lane"] == lane) for lane in sweep.LANES}
    pending_order = {}
    host_groups = [[e for e in selected if e["lane"] == "host"]] if host_workers > 1 else []
    for lane in sweep.LANES:
        pending = [
            e
            for e in ordered
            if e["lane"] == lane and e["id"] not in state["done"] and e["id"] not in state["runtime_excluded"]
        ]
        pending_order[lane] = [e["id"] for e in pending]
        if lane == "host" and host_workers > 1:
            for entry in pending:
                private = retail.lane_requires_retail({"tests": [entry]}, "host")
                group = next(
                    (
                        group
                        for group in host_groups
                        if (not group or retail.lane_requires_retail({"tests": group}, "host") == private)
                        and sum(e["estimated_seconds"] for e in group) + entry["estimated_seconds"] <= 2700
                    ),
                    None,
                )
                if group is None:
                    if len(host_groups) >= host_workers:
                        break
                    group = []
                    host_groups.append(group)
                group.append(entry)
                selected.append(entry)
                used[lane] += entry["estimated_seconds"]
            continue
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
        budget_seconds=max(2700, max(value for lane, value in used.items() if lane != "host" or host_workers == 1)),
    )
    identity = {key: plan[key] for key in ("target_sha", "catalog_hash", "tests")}
    if plan.get("retail_assets"):
        identity["retail_assets"] = plan["retail_assets"]
    plan["batch_id"] = sweep.digest(identity)
    now = int(time.time()) if now is None else now
    plan["sampling"] = {
        "schema": SCHEMA,
        "core_ids": sorted(core_ids),
        "host_workers": host_workers,
        "host_shards": [[e["id"] for e in group] for group in host_groups if group],
        "cycle": state["cycle"],
        "order": "Get-RuntimeHashOrder SHA256(name), grouped interleave",
        "pending_order": pending_order,
        "supplemental": extra,
        "started_unix": now,
        "deadline_unix": now + 3000,
        "eligible_owners": len(entries),
        "traversed_owners": len(state["done"]),
        "runtime_excluded": state["runtime_excluded"],
        "selected_sample_ids": [e["id"] for e in selected if e["id"] not in core_ids],
    }
    if refill:
        reserve_refills(plan, entries)
    sweep.validate_plan(plan, policy)
    return plan, state


def reconcile_sample(plan, policy, previous, receipts, supplemental_result="", run_id="local"):
    # Reconcile against a FRESH exact-SHA coverage state, never traversal from another revision
    if plan.get("reservation"):
        policy = materialized_policy(policy)
    expected = {shard["batch_id"]: shard for shard in shard_plans(plan).values()}
    normalized = []
    for receipt in receipts:
        if plan.get("reservation"):
            source = expected.get(receipt.get("batch_id")) if receipt.get("lane") == "host" and expected else plan
            if source is None:
                raise ValueError("Unknown host shard receipt provenance")
            validate_refill_receipt(source, policy, receipt)
        if expected and receipt.get("lane") == "host":
            shard = expected.get(receipt.get("batch_id"))
            if shard is None:
                raise ValueError("Unknown host shard receipt provenance")
            sweep.reconcile(shard, policy, sweep.fresh_state(shard["target_sha"], shard["catalog_hash"]), [receipt])
            receipt = dict(receipt, batch_id=plan["batch_id"])
        normalized.append(receipt)
    coverage = sweep.reconcile(plan, policy, sweep.fresh_state(plan["target_sha"], plan["catalog_hash"]), normalized)
    entries = sweep.policy_entries(policy)
    capability = capability_context(plan["target_sha"], plan.get("retail_assets"))
    state = reconcile_catalog(previous, list(entries.values()) + list(SUPPLEMENTAL.values()), capability)
    for ident, row in coverage.get("excluded", {}).items():
        state["runtime_excluded"][ident] = {
            "signature": exclusion_signature(entries[ident], capability),
            "reason": row["reason"],
            "run_url": row["run_url"],
            "target_sha": plan["target_sha"],
            "run_id": run_id,
        }
    core = set(plan["sampling"]["core_ids"])
    for ident, row in coverage["records"].items():
        if ident in core or row.get("attempted_tests") is False or row.get("phase") != "tests":
            continue
        state["runtime_excluded"].pop(ident, None)
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
    # This value is the trusted execution step outcome, never the overall job result
    # Setup-only failures leave that step skipped and remain undone
    if extra and supplemental_result in ("success", "failure"):
        status = "PASS" if supplemental_result == "success" else "FAIL"
        state["done"][extra] = {
            "signature": sweep.digest(SUPPLEMENTAL[extra]),
            "target_sha": plan["target_sha"],
            "status": status,
            "run_id": run_id,
        }
        if status == "FAIL":
            state["failures"][extra] = state["done"][extra]
        else:
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
            previous["runtime_excluded"] = {}
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


def shard_plans(plan):
    if plan.get("reservation"):
        return reservation_shards(plan)
    shards = {}
    for index, ids in enumerate(plan["sampling"].get("host_shards", [])):
        shard = dict(
            plan, tests=[e for e in plan["tests"] if e["id"] in ids], lanes=["host"], exclusions=[], unavailable=[]
        )
        shard["sampling"] = dict(plan["sampling"], host_shards=[], host_workers=1, pending_order={})
        shard = phase_plan(shard, "all")
        shards[f"shard-{index:03d}"] = shard
    return shards


def phase_plan(plan, phase):
    result = dict(plan)
    core = set(plan["sampling"]["core_ids"])
    result["tests"] = [e for e in plan["tests"] if phase == "all" or (e["id"] in core) == (phase == "core")]
    result["sample_phase"] = phase == "sample"
    result["estimated_seconds"] = sum(e["estimated_seconds"] for e in result["tests"])
    identity = {key: result[key] for key in ("target_sha", "catalog_hash", "tests")}
    if result.get("retail_assets"):
        identity["retail_assets"] = result["retail_assets"]
    result["batch_id"] = sweep.digest(identity)
    return result


def collect_sample(plan, policy, lane, output, blocked_reason=""):
    if plan.get("reservation"):
        return collect_refills(plan, policy, lane, output, blocked_reason)
    receipt = {key: plan[key] for key in ("schema", "target_sha", "catalog_hash", "batch_id")}
    if plan.get("retail_assets"):
        receipt["retail_assets"] = plan["retail_assets"]
    receipt.update(lane=lane, results=[])
    for phase in ("core", "sample"):
        rows = sweep.collect(phase_plan(plan, phase), policy, lane, output / phase, blocked_reason)
        receipt["results"].extend(rows["results"])
    write_artifact(output / "results.json", receipt)
    return receipt


RESERVATION_SCHEMA = "dxx-nightly-reservation-v1"


def materialized_policy(policy):
    # Enumerate immutable controller baselines once, not once per recovered child
    if policy.get("mission_corpus") is True:
        return dict(policy, mission_corpus=False, suites=list(sweep.policy_entries(policy).values()))
    return policy


def controller_identity():
    # Pin executable admission/collection policy, not candidate code or local paths
    root = Path(__file__).parent
    return sweep.digest(
        {
            "workflow_sha": os.environ.get("GITHUB_WORKFLOW_SHA", "local"),
            "files": {
                name: hashlib.sha256((root / name).read_bytes()).hexdigest()
                for name in (
                    "nightly_sample.py",
                    "nightly_tooling_sweep.py",
                    "nightly_retail.py",
                    "run_host_test_suites.py",
                )
            },
        }
    )


def reserve_refills(plan, entries):
    sampling = plan["sampling"]
    groups = [list(ids) for ids in sampling["host_shards"]]
    if not groups:
        groups = [[e["id"] for e in plan["tests"] if e["lane"] == "host"]]
    selected = {e["id"] for e in plan["tests"]}
    for ident in sampling["pending_order"]["host"]:
        if ident in selected:
            continue
        private = retail.lane_requires_retail({"tests": [entries[ident]]}, "host")
        compatible = [
            group
            for group in groups
            if not group
            or all(retail.lane_requires_retail({"tests": [entries[item]]}, "host") == private for item in group)
        ]
        if not compatible:
            if len(groups) >= sampling["host_workers"]:
                break
            groups.append([])
            compatible = [groups[-1]]
        group = min(compatible, key=lambda group: sum(entries[item]["estimated_seconds"] for item in group))
        group.append(ident)
        selected.add(ident)
    queues = {
        f"shard-{i:03d}" if sampling["host_workers"] > 1 else "host": group for i, group in enumerate(groups) if group
    }
    for lane in sweep.LANES:
        if lane == "host":
            continue
        queue = [e["id"] for e in plan["tests"] if e["lane"] == lane]
        for ident in sampling["pending_order"][lane]:
            if ident in selected:
                continue
            # Preserve the next-undone cursor at an incompatible provisioning boundary
            combined = [entries[item] for item in queue + [ident]]
            if any(e["id"] in sweep.THREE_PLAYER_IDS for e in combined) and retail.lane_requires_retail(
                {"tests": combined}, lane
            ):
                break
            queue.append(ident)
            selected.add(ident)
        if queue:
            queues[lane] = queue
    plan["tests"] = [entries[ident] for queue in queues.values() for ident in queue]
    plan["lanes"] = [lane for lane in sweep.LANES if any(e["lane"] == lane for e in plan["tests"])]
    plan["estimated_seconds"] = sum(e["estimated_seconds"] for e in plan["tests"])
    sampling["selection_scope"] = "immutable-refill-reservation"
    sampling["host_shards"] = [queue for key, queue in queues.items() if key.startswith("shard-")]
    sampling["selected_sample_ids"] = [e["id"] for e in plan["tests"] if e["id"] not in sampling["core_ids"]]
    plan["reservation"] = {
        "schema": RESERVATION_SCHEMA,
        "controller": controller_identity(),
        "queues": queues,
        "core_ids": sampling["core_ids"],
        "started_unix": sampling["started_unix"],
        "deadline_unix": sampling["deadline_unix"],
    }
    plan["batch_id"] = sweep.batch_identity(plan)


def validate_reservation(plan):
    reservation = plan["reservation"]
    if (
        not isinstance(reservation, dict)
        or reservation.get("schema") != RESERVATION_SCHEMA
        or reservation.get("controller") != controller_identity()
    ):
        raise ValueError("Reservation controller identity mismatch")
    start, deadline = reservation.get("started_unix"), reservation.get("deadline_unix")
    if type(start) is not int or type(deadline) is not int or deadline - start != 3000:
        raise ValueError("Invalid reservation wall deadline")
    entries = {e["id"]: e for e in plan["tests"]}
    queues = reservation.get("queues")
    if not isinstance(queues, dict) or len(queues) > 19:
        raise ValueError("Invalid reservation queues")
    seen = []
    host_queues = 0
    for name, ids in queues.items():
        if not isinstance(name, str) or (
            name not in sweep.LANES and not re.fullmatch(r"shard-00[0-9]|shard-01[0-5]", name)
        ):
            raise ValueError("Invalid reservation shard name")
        lane = "host" if name.startswith("shard-") else name
        if lane not in sweep.LANES or not isinstance(ids, list) or not ids:
            raise ValueError("Invalid reservation queue")
        if lane == "host":
            host_queues += 1
        for ident in ids:
            if ident not in entries or entries[ident]["lane"] != lane:
                raise ValueError("Reservation owner/lane mismatch")
        if lane == "host" and len({retail.lane_requires_retail({"tests": [entries[i]]}, lane) for i in ids}) > 1:
            raise ValueError("Reservation mixes public and private host owners")
        seen.extend(ids)
    if "host" in queues and host_queues != 1:
        raise ValueError("Global and isolated host queues cannot be mixed")
    if host_queues > 16 or len(seen) != len(set(seen)) or set(seen) != set(entries):
        raise ValueError("Reservation contains missing or duplicate shard owners")
    core = reservation.get("core_ids")
    if not isinstance(core, list) or len(core) != len(set(core)) or any(not isinstance(i, str) for i in core):
        raise ValueError("Invalid reservation core")
    for queue in queues.values():
        if [i for i in queue if i in core] != queue[: sum(i in core for i in queue)]:
            raise ValueError("Reservation must execute core before sampled owners")
    sampling = plan.get("sampling", {})
    if any(sampling.get(key) != reservation[key] for key in ("core_ids", "started_unix", "deadline_unix")):
        raise ValueError("Reservation sampling metadata mismatch")


def reservation_shards(plan):
    shards = {}
    for name, ids in plan["reservation"]["queues"].items():
        if not name.startswith("shard-"):
            continue
        shard = dict(
            plan, tests=[e for e in plan["tests"] if e["id"] in ids], lanes=["host"], exclusions=[], unavailable=[]
        )
        shard["sampling"] = dict(plan["sampling"], host_shards=[], host_workers=1, pending_order={})
        shard["reservation"] = dict(plan["reservation"], queues={"host": ids}, parent_batch_id=plan["batch_id"])
        shard["estimated_seconds"] = sum(e["estimated_seconds"] for e in shard["tests"])
        shard["batch_id"] = sweep.batch_identity(shard)
        shards[name] = shard
    return shards


def refill_plans(plan, lane):
    queues = plan["reservation"]["queues"]
    if lane not in queues:
        if lane == "host" and any(key.startswith("shard-") for key in queues):
            raise ValueError("Execute an isolated host shard, not the global reservation")
        return []
    ids = queues[lane]
    core = set(plan["reservation"]["core_ids"])
    batches = [[ident for ident in ids if ident in core]] + [[ident] for ident in ids if ident not in core]
    result = []
    entries = {e["id"]: e for e in plan["tests"]}
    for selected in batches:
        if not selected:
            continue
        child = {key: value for key, value in plan.items() if key != "reservation"}
        child.update(
            tests=[entries[ident] for ident in selected],
            lanes=[lane],
            sample_phase=selected[0] not in core,
            refill_parent={"batch_id": plan["batch_id"], "index": len(result)},
        )
        child["sampling"] = {
            key: plan["sampling"][key] for key in ("schema", "cycle", "core_ids", "started_unix", "deadline_unix")
        }
        child["estimated_seconds"] = sum(e["estimated_seconds"] for e in child["tests"])
        child["budget_seconds"] = max(2700, child["estimated_seconds"])
        child["batch_id"] = sweep.batch_identity(child)
        result.append(child)
    return result


def validate_refill_receipt(plan, policy, receipt):
    policy = materialized_policy(policy)
    sweep.validate_plan(plan, policy)
    if any(receipt.get(key) != plan[key] for key in ("target_sha", "catalog_hash", "batch_id")):
        raise ValueError("Refill reservation provenance mismatch")
    if receipt.get("lane") not in plan["lanes"]:
        raise ValueError("Refill receipt lane was not reserved")
    children = refill_plans(plan, receipt["lane"])
    admitted = receipt.get("refill_batches")
    if not isinstance(admitted, list) or admitted != [child["batch_id"] for child in children[: len(admitted)]]:
        raise ValueError("Refill admissions are not a unique ordered prefix")
    rows = receipt.get("results", [])
    if receipt.get("provisioning_blocked") is True:
        if admitted or any(row.get("status") != "BLOCKED" or row.get("attempted_tests") is not False for row in rows):
            raise ValueError("Provisioning failure cannot supply test evidence")
        return
    expected_ids = [e["id"] for child in children[: len(admitted)] for e in child["tests"]]
    if [row.get("id") for row in rows] != expected_ids:
        raise ValueError("Refill receipt contains unadmitted or duplicate owners")
    offset = 0
    for child in children[: len(admitted)]:
        # Exact ordered IDs were checked above; slice each row once
        selected = rows[offset : offset + len(child["tests"])]
        offset += len(selected)
        if any(row.get("refill_batch_id") != child["batch_id"] for row in selected):
            raise ValueError("Refill child receipt provenance mismatch")
        child_receipt = dict(receipt, batch_id=child["batch_id"], results=selected)
        sweep.reconcile(child, policy, sweep.fresh_state(child["target_sha"], child["catalog_hash"]), [child_receipt])


def collect_refill_child(child, policy, lane, directory):
    if sweep.read(directory / "admission.json") != child["refill_parent"]:
        raise ValueError("Invalid refill admission journal")
    if sweep.read(directory / "plan.json") != child:
        raise ValueError("Invalid refill child plan provenance")
    attempt = directory / "attempt.json"
    if attempt.exists():
        identity = sweep.read(attempt)
        if not isinstance(identity, dict) or any(
            identity.get(key) != value for key, value in sweep.attempt_identity(child, lane).items()
        ):
            raise ValueError("Invalid refill attempt provenance")
    return sweep.collect(child, policy, lane, directory)


def collect_refills(plan, policy, lane, output, blocked_reason="", expected_receipts=None):
    integrity_error = output / "refill-integrity-error.json"
    if integrity_error.exists():
        raise ValueError("Refill evidence integrity failure requires investigation")
    policy = materialized_policy(policy)
    receipt = {key: plan[key] for key in ("schema", "target_sha", "catalog_hash", "batch_id")}
    if plan.get("retail_assets"):
        receipt["retail_assets"] = plan["retail_assets"]
    receipt.update(lane=lane, results=[], refill_batches=[])
    gap = False
    for index, child in enumerate(refill_plans(plan, lane)):
        directory = output / "refill" / f"{index:04d}"
        admission = directory / "admission.json"
        if not admission.exists():
            gap = True
            continue
        if gap:
            raise ValueError("Invalid refill admission journal")
        collected = collect_refill_child(child, policy, lane, directory)
        if expected_receipts is not None and expected_receipts.get(child["batch_id"]) != sweep.digest(collected):
            write_artifact(integrity_error, {"schema": 1, "reason": "completed-evidence-changed"})
            raise ValueError("Completed refill evidence changed after collection")
        receipt["refill_batches"].append(child["batch_id"])
        receipt["results"].extend(dict(row, refill_batch_id=child["batch_id"]) for row in collected["results"])
    if expected_receipts is not None and receipt["refill_batches"] != list(expected_receipts):
        write_artifact(integrity_error, {"schema": 1, "reason": "completed-evidence-disappeared"})
        raise ValueError("Completed refill evidence disappeared after collection")
    if blocked_reason and not receipt["refill_batches"]:
        receipt["results"] = sweep.collect(plan, policy, lane, output / "unstarted", blocked_reason)["results"]
        receipt["provisioning_blocked"] = True
    validate_refill_receipt(plan, policy, receipt)
    write_artifact(output / "results.json", receipt)
    return receipt


def execute_refills(plan, policy, lane, output):
    policy = materialized_policy(policy)
    sweep.validate_plan(plan, policy)
    started = time.monotonic()
    allowance = plan["reservation"]["deadline_unix"] - time.time() - 120

    def remaining():
        return min(allowance - (time.monotonic() - started), plan["reservation"]["deadline_unix"] - time.time() - 120)

    if (output / "refill").exists():
        raise ValueError("Refill execution requires a fresh output directory; recover existing receipts instead")
    failed = 0
    expected_receipts = {}
    try:
        for index, child in enumerate(refill_plans(plan, lane)):
            if remaining() <= 0:
                break
            directory = output / "refill" / f"{index:04d}"
            write_artifact(directory / "plan.json", child)
            write_artifact(directory / "admission.json", child["refill_parent"])
            command = [
                sys.executable,
                str(Path(sweep.__file__).resolve()),
                "execute",
                "--plan",
                str(directory / "plan.json"),
                "--policy",
                str(Path(__file__).with_name("nightly_tooling_policy.json").resolve()),
                "--lane",
                lane,
                "--repo",
                "target",
                "--output",
                str(directory),
                "--build-root",
                str(output / "build"),
            ]
            timed_out = False
            try:
                budget = remaining()
                if budget <= 0:
                    break
                failed |= int(sweep.run_command(command, Path.cwd(), sys.stdout, timeout=budget).returncode != 0)
            except subprocess.TimeoutExpired:
                # Exhausting the shared sampling allowance is not itself a test
                # failure. The interrupted owner remains pending in its receipt;
                # preserve any failure already observed from an earlier owner
                print("Nightly deadline reached; interrupted owner remains pending")
                timed_out = True
            finally:
                try:
                    collected = collect_refill_child(child, policy, lane, directory)
                    sweep.reconcile(
                        child, policy, sweep.fresh_state(child["target_sha"], child["catalog_hash"]), [collected]
                    )
                    expected_receipts[child["batch_id"]] = sweep.digest(collected)
                except Exception:
                    # Admission was durably written by this controller. Its observed
                    # loss/corruption cannot be recovered as an unadmitted crash tail
                    write_artifact(
                        output / "refill-integrity-error.json",
                        {"schema": 1, "reason": "live-child-evidence-invalid", "batch_id": child["batch_id"]},
                    )
                    raise
            if timed_out:
                break
            current = collected["results"]
            # Stop on a setup/incomplete attempt instead of replaying a shared prerequisite
            # A scoped capability exclusion may advance the queue, but grants no coverage
            if any(
                not (
                    row["status"] in sweep.TERMINAL
                    and row.get("attempted_tests") is True
                    and row.get("phase") == "tests"
                )
                and not (
                    row["status"] == "BLOCKED"
                    and row.get("retryable") is False
                    and row.get("blocker_kind") == "target-capability"
                )
                for row in current
            ):
                failed = 1
                break
    finally:
        # Child evidence is durable after each attempt. Re-read the full prefix once
        # at completion/cancellation; never trust cached rows for publication
        try:
            collect_refills(plan, policy, lane, output, expected_receipts=expected_receipts)
        except Exception:
            marker = output / "refill-integrity-error.json"
            if not marker.exists():
                write_artifact(marker, {"schema": 1, "reason": "final-evidence-invalid"})
            raise
    return failed


def sample_status_ids(plan, coverage):
    # Partition this selection only; historical exclusions are not current receipts
    groups = {key: [] for key in ("pass_full", "pass_partial", "fail", "timeout", "excluded", "blocked", "pending")}
    for ident in plan["sampling"]["selected_sample_ids"]:
        row = coverage["records"].get(ident)
        if row:
            status = row["status"].lower()
            if status == "pass":
                status = "pass_full" if row.get("coverage_complete") is True else "pass_partial"
        elif ident in coverage.get("excluded", {}):
            status = "excluded"
        elif ident in coverage.get("retryable", {}):
            status = "blocked"
        else:
            status = "pending"
        groups[status].append(ident)
    return groups


def display_summary(summary):
    # The complete evidence remains in the artifact, outside GitHub's step-summary cap
    display = {
        key: summary[key]
        for key in (
            "target_sha",
            "batch_id",
            "core_without_pass",
            "current_sha_passed_owners",
            "current_sha_fully_covered_owners",
            "supplemental",
            "outcome",
        )
    }
    display.update(
        sample_status_counts={key: len(ids) for key, ids in summary["sample_status_ids"].items()},
        recorded_failures=len(summary["failures"]),
        historical_failures_not_current_sha_coverage=len(summary["historical_failures_not_current_sha_coverage"]),
        persisted_capability_exclusions_not_coverage=len(summary["runtime_excluded_not_coverage"]),
        details_artifact="nightly-sweep-summary / summary.json",
    )
    for field in ("sample_admitted_ids", "sample_unadmitted_ids"):
        if field in summary:
            display[field.removesuffix("_ids") + "_count"] = len(summary[field])
    return display


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
        plan, state = plan_sample(
            sha,
            sweep.read("plan/catalog.json"),
            policy,
            previous,
            retail.configured_identity(),
            host_workers=16,
            refill=True,
        )
        shards = shard_plans(plan)
        for name, shard in shards.items():
            sweep.validate_plan(shard, policy)
            write_artifact("plan/" + name + ".json", shard)
        write_artifact(args.plan, plan)
        write_artifact("plan/traversal.json", state)
        with open(os.environ["GITHUB_OUTPUT"], "a") as output:
            values = {
                "target_sha": sha,
                "lanes": plan["lanes"],
                "android_lanes": [lane for lane in plan["lanes"] if lane != "host"],
                "host_matrix": [
                    {"shard": name, "lane": "host", "retail": retail.lane_requires_retail(shard, "host")}
                    for name, shard in shards.items()
                ]
                or [{"shard": "", "lane": "host", "retail": retail.lane_requires_retail(plan, "host")}],
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
        if plan.get("reservation"):
            return execute_refills(plan, policy, args.lane, Path("results"))
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
    missing_core = [
        ident
        for ident in core
        if records.get(ident, {}).get("status") != "PASS"
        or (
            records.get(ident, {}).get("coverage_complete") is not True
            and not (
                ident == "master:test_gradle_unit_tests"
                and records.get(ident, {}).get("core_scope") == "fixture-independent-gradle-v1"
                and records.get(ident, {}).get("core_coverage_complete") is True
            )
        )
    ]
    failures = [ident for ident, row in records.items() if row["status"] in ("FAIL", "TIMEOUT")]
    statuses = sample_status_ids(plan, coverage)
    summary = {
        "target_sha": plan["target_sha"],
        "batch_id": plan["batch_id"],
        "sampling": plan["sampling"],
        "core_without_pass": missing_core,
        "current_sha_records": records,
        "failures": failures,
        "sample_pending": statuses["pending"],
        "sample_status_ids": statuses,
        "sample_blocked_not_coverage": coverage.get("retryable", {}),
        "unavailable": plan["unavailable"],
        "excluded": plan["exclusions"],
        "historical_failures_not_current_sha_coverage": state["failures"],
        "runtime_excluded_not_coverage": state["runtime_excluded"],
        "current_sha_passed_owners": sum(r["status"] == "PASS" for r in records.values()),
        "current_sha_fully_covered_owners": sum(
            r["status"] == "PASS" and r.get("coverage_complete") is True for r in records.values()
        ),
        "supplemental": {"selected": extra, "result": result if extra else "not-selected"},
        "outcome": "failed" if missing_core or failures or (extra and result != "success") else "sample_pass",
    }
    if plan.get("reservation"):
        admitted = {
            row["id"] for receipt in receipts for row in receipt.get("results", []) if row.get("refill_batch_id")
        }
        summary["sample_admitted_ids"] = [
            ident for ident in plan["sampling"]["selected_sample_ids"] if ident in admitted
        ]
        summary["sample_unadmitted_ids"] = [
            ident for ident in plan["sampling"]["selected_sample_ids"] if ident not in admitted
        ]
    write_artifact("summary.json", summary)
    write_artifact("summary-state.json", coverage)
    display = display_summary(summary)
    write_artifact("summary-display.json", display)
    print(json.dumps(display, indent=2))
    return int(summary["outcome"] == "failed")


if __name__ == "__main__":
    raise SystemExit(main())
