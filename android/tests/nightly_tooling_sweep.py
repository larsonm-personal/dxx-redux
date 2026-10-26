#!/usr/bin/env python3
"""Exact-revision nightly batches; artifacts contain data, never executable commands."""

import argparse
import base64
import urllib.request
import urllib.error
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import shutil
import subprocess
import sys
import time
import nightly_retail as retail
from xl_private_assets import OWNER_IDS, available
import nightly_regression_catalog as corpus

VALIDATION_IDS = {
    "master:test_run_all_tests_catalog",
    "master:test_controller_overlay",
    "shareware:test_death#d1",
    "master:test_gradle_unit_tests",
    "extractor:full",
    "audio:strict-fingerprint",
    "engine:ctest-d1",
    "master:test_lan_launch_preparation_public_shareware",
    "master:test_android_music_completion",
    "master:test_launcher_media_controls_generated",
    "python:test_music_spectra.py",
    "shareware:test_auto_level_cheats#d2",
}

PUBLIC_ONLINE_IDS = {"master:test_mp_public_shareware"}
PUBLIC_ONLINE_D2_IDS = {"master:test_mp_public_shareware_d2"}
ONLINE_TRANSPORT_IDS = {
    "master:test_mp_coop_d1_relay",
    "master:test_mp_coop_d2_direct",
    "master:test_mp_coop_d2_initial_fallback",
}
PUBLIC_LAN_IDS = {"master:test_lan_launch_preparation_public_shareware"}
THREE_PLAYER_IDS = {"master:test_lan_three_player_startup"}
THREE_PLAYER_CASES = {"d1/three-player", "d1/three-player-lost", "d2/three-player", "d2/three-player-lost"}
BASIC_USE_IDS = {"master:test_demo_import_public_basic_use", "master:test_lan_launch_preparation_public_shareware"}

FIRST_PLAY_IDS = {
    "master:test_demo_import_public_first_play",
    "master:test_lan_launch_preparation_public_shareware",
    "master:test_lan_host_migration_public_shareware",
    "master:test_gradle_unit_tests",
    "shareware:test_autosave_resume_missing_pilot_unified#d1",
    "shareware:test_autosave_resume_missing_pilot_unified#d2",
    "shareware:test_graphics_lifecycle#d1",
    "shareware:test_graphics_lifecycle#d2",
    "master:test_lan_transport_recovery",
    "shareware:test_pilot_long_hold_delete_unified#d2",
}

GRAPHICS_CAPABILITIES_IDS = {
    "shareware:test_graphics_capabilities#d1",
    "shareware:test_graphics_capabilities#d2",
    "shareware:test_graphics_context_loss#d1",
    "shareware:test_graphics_context_loss#d2",
    "shareware:test_graphics_msaa_allocation_failure#d1",
    "shareware:test_graphics_msaa_allocation_failure#d2",
}

SCHEMA = 1
LANES = ("host", "jvm", "android-single", "android-dual")
TERMINAL = {"PASS", "FAIL", "TIMEOUT"}
COVERED = {"PASS", "FAIL", "TIMEOUT"}
SHA = re.compile(r"^[0-9a-f]{40}$")


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def read(path):
    p = Path(path)
    if p.stat().st_size > 8 * 1024 * 1024:
        raise ValueError("JSON exceeds bounded size")
    return json.loads(p.read_text())


def write(path, value):
    p = Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    tmp = p.with_suffix(p.suffix + ".partial")
    tmp.write_text(json.dumps(value, indent=2) + "\n")
    tmp.replace(p)


def policy_entries(policy):
    if policy.get("schema") != SCHEMA or not isinstance(policy.get("suites"), list):
        raise ValueError("Invalid policy")
    entries = {}
    for entry in policy["suites"] + (corpus.policy_suites() if policy.get("mission_corpus") is True else []):
        ident = entry.get("id")
        if not isinstance(ident, str) or not re.fullmatch(r"[A-Za-z0-9_.:#-]{1,160}", ident) or ident in entries:
            raise ValueError("Invalid/duplicate suite ID")
        if entry.get("lane") not in LANES or entry.get("owner") not in ("master", "host-adapter", "shareware"):
            raise ValueError("Unknown lane/owner")
        if type(entry.get("estimated_seconds")) is not int or not 1 <= entry["estimated_seconds"] <= 2700:
            raise ValueError("Estimate outside nightly budget")
        expected_args = {"suite", "game"} if entry["owner"] == "shareware" else {"suite"}
        if (
            not isinstance(entry.get("args"), dict)
            or set(entry["args"]) != expected_args
            or not isinstance(entry["args"]["suite"], str)
            or not re.fullmatch(r"[A-Za-z0-9_.:-]{1,160}", entry["args"]["suite"])
        ):
            raise ValueError("Only typed exact suite selection is accepted")
        if entry["owner"] == "shareware" and (
            entry["args"]["game"] not in ("d1", "d2") or entry["lane"] != "android-single"
        ):
            raise ValueError("Invalid public gameplay lane/game")
        if entry.get("fixture") not in (None, "retail-base", "retail-missions", "retail-demos", "retail-corpus"):
            raise ValueError("Unknown fixture requirement")
        if entry.get("requires") not in (None, "three_emulators"):
            raise ValueError("Unknown explicit resource requirement")
        if entry.get("requires") == "three_emulators" and entry["lane"] != "android-dual":
            raise ValueError("Three-emulator owners require the multi-device lane")
        if ident in THREE_PLAYER_IDS and (
            entry.get("requires") != "three_emulators"
            or entry.get("category") != "three_player_network"
            or entry["owner"] != "master"
            or entry["args"]["suite"] != "test_lan_three_player_startup"
            or entry["estimated_seconds"] > 900
        ):
            raise ValueError("Invalid bounded three-player owner contract")
        entries[ident] = entry
    return entries


def validate_state(state):
    if state.get("schema") != SCHEMA or not SHA.fullmatch(state.get("target_sha", "")):
        raise ValueError("Invalid ledger schema/revision")
    if not re.fullmatch(r"[0-9a-f]{64}", state.get("catalog_hash", "")) or not isinstance(state.get("records"), dict):
        raise ValueError("Invalid ledger catalog/records")
    if not isinstance(state.get("retryable", {}), dict) or not isinstance(state.get("excluded", {}), dict):
        raise ValueError("Invalid retryable observations")
    for ident, record in state["records"].items():
        if (
            not isinstance(ident, str)
            or record.get("status") not in TERMINAL
            or not isinstance(record.get("run_url"), str)
        ):
            raise ValueError("Invalid ledger receipt")


def fresh_state(sha, catalog_hash):
    return {
        "schema": SCHEMA,
        "target_sha": sha,
        "catalog_hash": catalog_hash,
        "records": {},
        "retryable": {},
        "excluded": {},
    }


def owner_category(entry):
    if entry.get("category"):
        return entry["category"]
    name = entry["id"].lower()
    for category, words in (
        ("extraction-fingerprints", ("extract", "fingerprint", "disc", "cd_")),
        ("music-audio", ("music", "audio", "sound", "redbook", "pcm", "hmp", "fluidsynth")),
        ("metadata-routes", ("metadata", "guidebot", "route", "secret")),
        ("save-replay", ("save", "demo", "replay", "checkpoint", "ordered-input")),
        ("network", ("lan_", "network", "multiplayer", "coop", "manual_ip", "test_mp")),
        ("graphics", ("graphics", "render", "texture", "fov", "msaa")),
    ):
        if any(word in name for word in words):
            return category
    return "tooling"


def plan_batch(sha, catalog, policy, state=None, budget=2700, retail_assets=None):
    if not SHA.fullmatch(sha) or type(budget) is not int or not 1 <= budget <= 2700:
        raise ValueError("Invalid revision/budget")
    entries = policy_entries(policy)
    if catalog.get("schema") != SCHEMA or not isinstance(catalog.get("tests"), list):
        raise ValueError("Invalid master discovery catalog")
    names = {row["name"] for row in catalog["tests"]}
    retail.validate_identity(retail_assets)
    catalog_identity = {"catalog": catalog, "policy": policy, "owners": list(entries.values())}
    if retail_assets:
        catalog_identity["retail_assets"] = retail_assets
    catalog_hash = digest(catalog_identity)
    if state is not None:
        validate_state(state)
    if state is None or (state["target_sha"], state["catalog_hash"]) != (sha, catalog_hash):
        state = fresh_state(sha, catalog_hash)
    if not (set(state["records"]) | set(state.get("retryable", {})) | set(state.get("excluded", {}))) <= set(entries):
        raise ValueError("Ledger contains unknown suite")
    selected, excluded, unavailable = [], list(policy.get("exclusions", [])), []
    admitted = {e["args"]["suite"] for e in entries.values() if e["owner"] == "master"}
    excluded.extend(
        {
            "id": row["name"],
            "reason": "not yet admitted: public prerequisites/host support require qualification",
            "requires": row.get("requires"),
            "manual": row.get("manual"),
        }
        for row in catalog["tests"]
        if row["name"] not in admitted
    )
    used = {lane: 0 for lane in LANES}
    # Untried work precedes repeatedly blocked setup, so one lane cannot starve the rest.
    rings = {}
    for ident, entry in entries.items():
        # Preview campaigns are explicit opt-ins; preserve the scheduled LAN budget
        if ident in PUBLIC_ONLINE_IDS | PUBLIC_ONLINE_D2_IDS | ONLINE_TRANSPORT_IDS:
            continue
        if ident in state["records"] or ident in state.get("excluded", {}):
            continue
        rings.setdefault((entry["lane"], owner_category(entry)), []).append((ident, entry))
    for ring in rings.values():
        ring.sort(
            key=lambda item: (state.get("retryable", {}).get(item[0], {}).get("attempts", 0), digest([sha, item[0]]))
        )
    # Interleave existing owner families just like the runtime sampler's grouped rings.
    # A selection never changes progress; only reconciled execution receipts retire work.
    ordered = [
        item
        for _, item in sorted(
            ((index, item) for ring in rings.values() for index, item in enumerate(ring)),
            key=lambda row: (
                row[0],
                state.get("retryable", {}).get(row[1][0], {}).get("attempts", 0),
                digest([sha, row[1][0]]),
            ),
        )
    ]
    for ident, entry in ordered:
        if entry.get("unavailable_reason"):
            unavailable.append(
                {"id": ident, "reason": entry["unavailable_reason"], "blocker_kind": "target-capability"}
            )
            continue
        if (
            entry.get("fixture") in ("retail-base", "retail-missions", "retail-demos", "retail-corpus")
            and not retail_assets
        ):
            unavailable.append({"id": ident, "reason": "retail configuration unavailable for this run; pending"})
            continue
        if entry.get("fixture") == "retail-corpus" and not retail.corpus_available(retail_assets):
            unavailable.append({"id": ident, "reason": "reviewed mission corpus commit unavailable; pending"})
            continue
        if entry.get("fixture") == "retail-demos" and not retail.demos_available(retail_assets):
            unavailable.append({"id": ident, "reason": "reviewed private demo commit unavailable; pending"})
            continue
        if ident in OWNER_IDS and not available(retail_assets):
            unavailable.append({"id": ident, "reason": "reviewed XL fixture commit unavailable; pending"})
            continue
        if entry["owner"] == "master" and entry["args"]["suite"] not in names:
            excluded.append({"id": ident, "reason": "not present in pinned master catalog"})
            continue
        if ident in state["records"] or ident in state.get("excluded", {}):
            continue
        # Named public leaf evidence must not enter a retail-redacted lane receipt
        # Defer the other group without retiring it; normal retry/ring ordering remains fair
        if (ident in THREE_PLAYER_IDS and retail.lane_requires_retail({"tests": selected}, entry["lane"])) or (
            entry.get("fixture") and any(e["id"] in THREE_PLAYER_IDS and e["lane"] == entry["lane"] for e in selected)
        ):
            continue
        if used[entry["lane"]] + entry["estimated_seconds"] <= budget:
            selected.append(entry)
            used[entry["lane"]] += entry["estimated_seconds"]
    identity = {"target_sha": sha, "catalog_hash": catalog_hash, "tests": selected}
    if retail_assets:
        identity["retail_assets"] = retail_assets
    plan = {
        "schema": SCHEMA,
        **identity,
        "batch_id": digest(identity),
        "budget_seconds": budget,
        "estimated_seconds": sum(used.values()),
        "budget_scope": "per-lane",
        "exclusions": excluded,
        "lanes": [lane for lane in LANES if any(e["lane"] == lane for e in selected)],
    }
    plan["unavailable"] = unavailable
    if retail_assets:
        plan["retail_assets"] = retail_assets
    return plan, state


def select_public_campaign(plan, policy, catalog, campaign):
    """A fixed asset-free sample; never inherits rotating retail selections or progress."""
    campaign_ids = {
        "first-play": FIRST_PLAY_IDS,
        "graphics-capabilities": GRAPHICS_CAPABILITIES_IDS,
        "basic-use": BASIC_USE_IDS,
        "xl-browser": {"master:test_xl_feature_browser"},
        "xl-textures": {"master:test_scoped_static_textures"},
        "lan-smoke": PUBLIC_LAN_IDS,
        "online-preview": PUBLIC_ONLINE_IDS,
        "online-preview-d2": PUBLIC_ONLINE_D2_IDS,
        "online-transports": ONLINE_TRANSPORT_IDS,
        "lan-three-player": THREE_PLAYER_IDS,
    }[campaign]
    entries = policy_entries(policy)
    if not campaign_ids <= entries.keys():
        raise ValueError(f"Missing {campaign} campaign suites")
    names = {row["name"] for row in catalog["tests"]}
    selected = [entry for ident, entry in entries.items() if ident in campaign_ids]
    if any(
        entry.get("fixture") in ("retail-base", "retail-missions", "retail-demos", "retail-corpus")
        for entry in selected
    ):
        raise ValueError(f"{campaign} campaign must remain asset-free")
    if any(
        (entry["owner"] == "master" or campaign == "graphics-capabilities") and entry["args"]["suite"] not in names
        for entry in selected
    ):
        raise ValueError(f"{campaign} owner absent from pinned catalog")
    plan = dict(plan)
    plan.pop("retail_assets", None)
    plan["tests"] = selected
    plan["estimated_seconds"] = sum(entry["estimated_seconds"] for entry in selected)
    plan["lanes"] = [lane for lane in LANES if any(entry["lane"] == lane for entry in selected)]
    plan["batch_id"] = digest({key: plan[key] for key in ("target_sha", "catalog_hash", "tests")})
    validate_plan(plan, policy)
    return plan


def select_xl_originals(plan, policy, catalog):
    if not retail.trusted_context() or not available(plan.get("retail_assets")):
        raise ValueError("Original XL campaign requires trusted cmake and the pinned private revision")
    entries = policy_entries(policy)
    if not OWNER_IDS <= entries.keys() or "test_xl_original_assets" not in {r["name"] for r in catalog["tests"]}:
        raise ValueError("Required original XL owners are missing")
    result = dict(plan)
    result["tests"] = [entries[key] for key in sorted(OWNER_IDS)]
    result["lanes"] = [lane for lane in LANES if any(e["lane"] == lane for e in result["tests"])]
    result["estimated_seconds"] = sum(e["estimated_seconds"] for e in result["tests"])
    result["batch_id"] = digest({key: result[key] for key in ("target_sha", "catalog_hash", "tests", "retail_assets")})
    validate_plan(result, policy)
    return result


def omit_pr_public_lan(plan, policy):
    """The unconditional PR workflow owns this one public LAN test"""
    plan = dict(plan)
    plan["tests"] = [entry for entry in plan["tests"] if entry["id"] not in PUBLIC_LAN_IDS]
    plan["estimated_seconds"] = sum(entry["estimated_seconds"] for entry in plan["tests"])
    plan["lanes"] = [lane for lane in LANES if any(entry["lane"] == lane for entry in plan["tests"])]
    identity = {key: plan[key] for key in ("target_sha", "catalog_hash", "tests")}
    if plan.get("retail_assets"):
        identity["retail_assets"] = plan["retail_assets"]
    plan["batch_id"] = digest(identity)
    validate_plan(plan, policy)
    return plan


def validate_plan(plan, policy):
    entries = policy_entries(policy)
    if plan.get("schema") != SCHEMA or not SHA.fullmatch(plan.get("target_sha", "")):
        raise ValueError("Invalid plan revision/schema")
    if not re.fullmatch(r"[0-9a-f]{64}", plan.get("catalog_hash", "")):
        raise ValueError("Invalid plan catalog hash")
    retail.validate_identity(plan.get("retail_assets"))
    selected = plan.get("tests")
    if not isinstance(selected, list) or len(selected) > 1024:
        raise ValueError("Invalid plan selection")
    seen = set()
    for entry in selected:
        ident = entry.get("id")
        if ident in seen or entries.get(ident) != entry:
            raise ValueError("Unapproved or duplicate plan entry")
        if entry.get("fixture") in ("retail-base", "retail-missions", "retail-demos", "retail-corpus") and not plan.get(
            "retail_assets"
        ):
            raise ValueError("Retail selection lacks pinned data identity")
        if entry.get("fixture") == "retail-demos" and not retail.demos_available(plan.get("retail_assets")):
            raise ValueError("Private demo selection lacks reviewed commit")
        if entry.get("fixture") == "retail-corpus" and not retail.corpus_available(plan.get("retail_assets")):
            raise ValueError("Mission corpus selection lacks reviewed commit")
        if ident in OWNER_IDS and not available(plan.get("retail_assets")):
            raise ValueError("Original XL selection lacks reviewed commit")
        if entry.get("unavailable_reason"):
            raise ValueError("Selected owner requires an unavailable target capability")
        seen.add(ident)
    if any(e["id"] in THREE_PLAYER_IDS and retail.lane_requires_retail(plan, e["lane"]) for e in selected):
        raise ValueError("Three-player evidence requires an asset-free lane batch")
    if plan.get("budget_scope") != "per-lane" or not 1 <= plan.get("budget_seconds", 0) <= 2700:
        raise ValueError("Invalid per-lane budget")
    if any(
        sum(e["estimated_seconds"] for e in selected if e["lane"] == lane) > plan["budget_seconds"] for lane in LANES
    ):
        raise ValueError("Plan exceeds lane budget")
    identity = {key: plan[key] for key in ("target_sha", "catalog_hash", "tests")}
    if plan.get("retail_assets"):
        identity["retail_assets"] = plan["retail_assets"]
    if digest(identity) != plan.get("batch_id"):
        raise ValueError("Plan batch identity mismatch")


def normalize_result(row, entry=None):
    """Never certify a PASS contradicted by the owner's structured evidence."""
    row = dict(row)
    if row.get("status") != "PASS":
        return row
    cases = row.get("cases", [])
    counts = row.get("counts", {})
    if row.get("exit_code") not in (None, 0):
        row.update(status="FAIL", reason="PASS receipt has a nonzero test exit code")
    elif counts.get("failed", 0) or any(case.get("status") in ("FAIL", "TIMEOUT") for case in cases):
        row.update(status="FAIL", reason="PASS receipt contains failed leaf cases")
    elif row.get("attempted_tests") is False or row.get("phase") in ("setup", "preflight"):
        row.update(status="BLOCKED", reason="PASS receipt does not attest to test execution", retryable=True)
    elif counts.get("passed") == 0 or (cases and not any(case.get("status") == "PASS" for case in cases)):
        row.update(status="BLOCKED", reason="No structured leaf case passed", attempted_tests=False, retryable=True)
    if entry and entry["id"] in THREE_PLAYER_IDS and row["status"] == "PASS":
        if (
            len(cases) != len(THREE_PLAYER_CASES)
            or {case.get("id") for case in cases} != THREE_PLAYER_CASES
            or any(case.get("status") != "PASS" for case in cases)
            or row.get("coverage_complete") is not True
        ):
            row.update(status="FAIL", reason="Three-player coverage requires all four named leaf cases to pass")
    if row["status"] != "PASS":
        row["coverage_complete"] = False
    return row


def three_player_result(row, output, sha):
    """Bind the fixed owner's public leaf evidence to this exact execution attempt."""
    row = dict(row)
    path = Path(output) / "three-player" / "three-player-summary.json"
    try:
        summary = read(path)
        if (
            not isinstance(summary, dict)
            or summary.get("schema") != 1
            or summary.get("owner") != "test_lan_three_player_startup"
            or summary.get("revision") != sha
            or summary.get("category") != "three_player_network"
            or not isinstance(summary.get("results"), list)
            or any(not isinstance(case, dict) for case in summary["results"])
        ):
            raise ValueError("Three-player summary provenance mismatch")
        row.update(cases=summary["results"], coverage_complete=summary.get("coverage_complete") is True)
        if row.get("status") == "PASS" and summary.get("passed") is not True:
            row.update(status="FAIL", reason="Three-player summary does not attest to a complete pass")
    except (OSError, ValueError, TypeError) as error:
        if row.get("status") == "PASS":
            row.update(
                status="FAIL", reason="Missing or invalid three-player evidence: " + str(error), coverage_complete=False
            )
    return row


def attempt_identity(plan, lane):
    return {
        **{key: plan[key] for key in ("schema", "target_sha", "catalog_hash", "batch_id")},
        "lane": lane,
        "run_id": os.environ.get("GITHUB_RUN_ID", "local"),
        "run_attempt": os.environ.get("GITHUB_RUN_ATTEMPT", "local"),
        "repository": os.environ.get("GITHUB_REPOSITORY", "local/unpublished"),
    }


def journal_observations(path, sha):
    observations = {}
    if Path(path).exists():
        for line in Path(path).read_text().splitlines():
            try:
                row = json.loads(line)
            except json.JSONDecodeError:
                continue  # Recover completed rows after an interrupted append
            if row.get("commit") != sha:
                raise ValueError("Journal target mismatch")
            prior = observations.get(row["name"])
            if prior and prior.get("status") != "RUNNING" and prior != row:
                raise ValueError("Conflicting terminal journal receipt")
            observations[row["name"]] = row
    return observations


def reconcile(plan, policy, state, receipts):
    validate_plan(plan, policy)
    validate_state(state)
    if any(state[key] != plan[key] for key in ("target_sha", "catalog_hash")):
        raise ValueError("Ledger/plan revision mismatch")
    allowed = {e["id"]: e for e in plan["tests"]}
    result = json.loads(json.dumps(state))
    seen = {}
    for receipt in receipts:
        if (
            any(receipt.get(key) != plan[key] for key in ("target_sha", "catalog_hash", "batch_id"))
            or receipt.get("schema") != SCHEMA
        ):
            raise ValueError("Receipt provenance mismatch")
        lane = receipt.get("lane")
        if receipt.get("retail_assets") != plan.get("retail_assets"):
            raise ValueError("Receipt retail asset identity mismatch")
        receipt = {
            **receipt,
            "results": [normalize_result(row, allowed.get(row.get("id"))) for row in receipt.get("results", [])],
        }
        if retail.lane_requires_retail(plan, lane):
            receipt = retail.public_receipt(receipt)
        for row in receipt.get("results", []):
            ident = row.get("id")
            if ident not in allowed or allowed[ident]["lane"] != lane:
                raise ValueError("Unplanned receipt ID/lane")
            if ident in seen and seen[ident] != row:
                raise ValueError("Conflicting duplicate receipt")
            seen[ident] = row
            if row.get("status") == "BLOCKED":
                if not isinstance(row.get("run_url"), str) or not row["run_url"].startswith("https://github.com/"):
                    raise ValueError("Missing infrastructure run provenance")
                if row.get("retryable") is False and row.get("blocker_kind") == "target-capability":
                    result.setdefault("excluded", {})[ident] = {
                        "reason": str(row.get("reason", ""))[:512],
                        "run_url": row["run_url"],
                    }
                    result.setdefault("retryable", {}).pop(ident, None)
                    continue
                previous = result.setdefault("retryable", {}).get(ident, {})
                result["retryable"][ident] = {
                    "reason": str(row.get("reason", ""))[:512],
                    "run_url": row["run_url"],
                    "attempts": previous.get("attempts", 0) + int(previous.get("run_url") != row["run_url"]),
                }
                continue
            if row.get("status") not in TERMINAL:
                if row.get("status") not in ("RUNNING", "NOT_RUN"):
                    raise ValueError("Unknown result status")
                continue
            if not isinstance(row.get("run_url"), str) or not row["run_url"].startswith("https://github.com/"):
                raise ValueError("Missing run provenance")
            compact = {key: row[key] for key in ("status", "run_url")}
            compact.update(
                comparison_only=row.get("comparison_only") is True,
                runtime_outcome=row.get("runtime_outcome", ""),
                reason=str(row.get("reason", ""))[:512],
                coverage_complete=row.get("coverage_complete") is True,
                attempted_tests=row.get("attempted_tests", True),
                phase=row.get("phase", "tests"),
            )
            cases = row.get("cases", [])
            compact["case_counts"] = {
                status: sum(c.get("status") == status for c in cases) for status in ("PASS", "FAIL", "SKIP", "BLOCKED")
            }
            compact["exclusions"] = [str(x)[:256] for x in row.get("exclusions", [])[:32]]
            if ident.startswith("corpus:"):
                # Many exact route leaves share a run; retain proof without duplicating verbose diagnostics.
                compact = {key: compact[key] for key in ("status", "run_url", "coverage_complete", "reason")}
                if row.get("comparison_only"):
                    compact.update(comparison_only=True, runtime_outcome=row.get("runtime_outcome", ""))
                if cases:
                    compact["case_counts"] = {
                        status: sum(c.get("status") == status for c in cases)
                        for status in ("PASS", "FAIL")
                        if any(c.get("status") == status for c in cases)
                    }
            if row["status"] in ("FAIL", "TIMEOUT"):
                details = retail.public_diagnostics(row)
                workers = details.get("workers", []) + [details.get("owner_failure", {})]
                categories = sorted({category for worker in workers for category in worker.get("categories", [])})
                if categories:
                    compact["failure_categories"] = categories
                exits = [worker["exit_code"] for worker in workers if worker.get("exit_code") not in (None, 0)]
                if exits:
                    compact["worker_exit_codes"] = exits
            prior = result["records"].get(ident)
            if prior and prior != compact:
                raise ValueError("Conflicting completed receipt")
            result["records"][ident] = compact
            result.setdefault("retryable", {}).pop(ident, None)
    return result


def online_failure_summary(output):
    """Return fixed categories only; never export owner exception text or payloads"""
    root = Path(output).resolve()
    paths = sorted((root / "master").glob("test_mp_public_shareware_*.log"))[-1:]
    phase, category = "unknown", "owner-failure"
    for path in paths:
        if not path.is_file() or path.is_symlink() or not path.resolve().is_relative_to(root):
            continue
        with path.open("rb") as stream:
            stream.seek(0, os.SEEK_END)
            stream.seek(max(0, stream.tell() - 131072))
            text = stream.read(131072).decode("utf-8", errors="replace")
        text = re.sub(r"\x1b\[[0-9;]*m", "", text)
        for line in text.splitlines():
            match = re.search(r"--- Phase ([0-9]{1,2}):", line)
            if match:
                phase = "last-numbered-phase-" + match[1]
            if "EXCEPTION:" in line:
                category = "owner-exception"
            elif "FAIL:" in line and category != "owner-exception":
                category = "owner-assertion"
        break
    return f"Online qualification failed: {phase}, {category}; see retained owner log"


def collect(plan, policy, lane, output, blocked_reason=""):
    validate_plan(plan, policy)
    output = Path(output)
    receipt = {key: plan[key] for key in ("schema", "target_sha", "catalog_hash", "batch_id")}
    if plan.get("retail_assets"):
        receipt["retail_assets"] = plan["retail_assets"]
    receipt.update(lane=lane, results=[])
    run_url = (
        os.environ.get("GITHUB_SERVER_URL", "https://github.com")
        + "/"
        + os.environ.get("GITHUB_REPOSITORY", "local/unpublished")
        + "/actions/runs/"
        + os.environ.get("GITHUB_RUN_ID", "local")
    )
    attempt_path = output / "attempt.json"
    attempt = read(attempt_path) if attempt_path.exists() else {}
    if attempt and any(attempt.get(key) != value for key, value in attempt_identity(plan, lane).items()):
        raise ValueError("Execution attempt provenance mismatch")
    processes = attempt.get("processes", {})
    observations = journal_observations(output / "journal.jsonl", plan["target_sha"]) if "master" in processes else {}
    for entry in plan["tests"]:
        if entry["lane"] != lane:
            continue
        if entry["owner"] == "master":
            row = observations.get(entry["args"]["suite"], {})
            status = row.get("status", "NOT_RUN")
            runner_exit = processes.get("master", {}).get("exit_code")
            # A sibling's failure must not overwrite this child's durable result
            exit_code = row.get("exit_code") if type(row.get("exit_code")) is int else runner_exit
            if row:
                try:
                    row = {**row, "master_diagnostics": retail.master_failure_diagnostics(row, output)}
                except (OSError, ValueError):
                    row = {**row, "master_diagnostics": {"log_available": False}}
            if not row and exit_code:
                row = {
                    "reason": f"Master runner exited {exit_code} without this test receipt; see master.log. Remains pending."
                }
            if status == "SKIP":
                status = "BLOCKED"
        else:
            path = output / (entry["id"].replace(":", "_") + ".json")
            process = processes.get(entry["id"], {})
            row = read(path) if process and path.exists() else {}
            worker_id = row.get("worker_diagnostics_id")
            if isinstance(worker_id, str) and re.fullmatch("[0-9a-f]{32}", worker_id):
                from nightly_regression_corpus import metadata_worker_diagnostics

                row["workers"] = metadata_worker_diagnostics(
                    path.with_suffix(".artifacts") / ("metadata-workers-" + worker_id)
                )
            if row and not process.get("completed"):
                row.update(status="NOT_RUN", coverage_complete=False)
            exit_code = process.get("exit_code")
            status = row.get("status", "NOT_RUN")
            if row and (row.get("schema") != 1 or row.get("suite") != entry["args"]["suite"]):
                raise ValueError("Invalid adapter receipt")
        if status == "PASS" and exit_code not in (None, 0):
            row = {
                **row,
                "status": "FAIL",
                "reason": f"Owner process exited {exit_code} despite PASS receipt",
                "coverage_complete": False,
            }
            status = "FAIL"
        row = {**row, "status": status}
        if entry["id"] in THREE_PLAYER_IDS:
            row = three_player_result(row, output, plan["target_sha"])
        row = normalize_result(row, entry)
        status = row["status"]
        if row.get("blocker_kind") == "interrupted":
            status = "NOT_RUN"
        if row.get("attempted_tests") is False and status in ("FAIL", "TIMEOUT"):
            status = "BLOCKED"
        if status == "NOT_RUN" and blocked_reason:
            status = "BLOCKED"
            row = {"reason": blocked_reason}
        if (
            entry["id"] in PUBLIC_ONLINE_IDS | PUBLIC_ONLINE_D2_IDS | ONLINE_TRANSPORT_IDS
            and status in ("FAIL", "TIMEOUT")
            and not row.get("reason")
        ):
            row = {**row, "reason": online_failure_summary(output)}
        if status not in TERMINAL | {"RUNNING", "NOT_RUN", "BLOCKED"}:
            raise ValueError("Invalid collected status")
        receipt["results"].append(
            {
                "id": entry["id"],
                "status": status,
                "reason": row.get("reason", "")[:2048],
                "exit_code": row.get("exit_code", exit_code),
                **retail.public_diagnostics(row),
                "run_url": run_url,
                "exclusions": row.get("exclusions", []),
                "cases": row.get("cases", []),
                "coverage_complete": row.get("coverage_complete", False),
                "comparison_only": row.get("comparison_only", False),
                "runtime_outcome": row.get("runtime_outcome", ""),
                "attempted_tests": row.get("attempted_tests", status in TERMINAL),
                "phase": row.get("phase", "tests" if status in TERMINAL else "setup"),
                "retryable": row.get("retryable", status == "BLOCKED"),
                "blocker_kind": row.get("blocker_kind", "infrastructure" if status == "BLOCKED" else ""),
            }
        )
    if retail.lane_requires_retail(plan, lane):
        receipt = retail.public_receipt(receipt)
    write(output / "results.json", receipt)
    return receipt


def run_command(command, cwd, log, timeout=None):
    process = subprocess.Popen(
        command, cwd=cwd, stdout=log, stderr=subprocess.STDOUT, start_new_session=os.name != "nt"
    )
    try:
        process.wait(timeout=timeout)
        return process
    except BaseException:
        if os.name != "nt":
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
        else:
            process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            if os.name != "nt":
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            else:
                process.kill()
            process.wait(timeout=5)
        if os.name != "nt":
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        raise


def print_public_jvm_failure(plan, lane, output):
    """Expose bounded owner output without exporting retail or other lane logs"""
    if lane != "jvm" or plan.get("retail_assets") or os.environ.get("RETAIL_REQUESTED") == "true":
        return
    selected = [entry["id"] for entry in plan["tests"] if entry["lane"] == lane]
    if selected != ["master:test_gradle_unit_tests"]:
        return
    output = Path(output).resolve()
    paths = [output / "master.log"]
    paths.extend(sorted((output / "master").glob("test_gradle_unit_tests_*.log"))[-2:])
    print("Public JVM failure diagnostics: master:test_gradle_unit_tests")
    for path in paths:
        if not path.is_file() or path.is_symlink() or not path.resolve().is_relative_to(output):
            continue
        with path.open("rb") as stream:
            stream.seek(0, os.SEEK_END)
            stream.seek(max(0, stream.tell() - 65536))
            tail = stream.read(65536).decode("utf-8", errors="replace").splitlines()[-200:]
        # JSON quotes modern commands; escape hashes for the legacy ##[ parser
        print(json.dumps({"file": path.name, "tail": tail}, ensure_ascii=True).replace("#", "\\u0023"))


def execute(plan, policy, lane, repo, output):
    validate_plan(plan, policy)
    repo, output = Path(repo).resolve(), Path(output).resolve()
    actual = subprocess.check_output(["git", "-C", str(repo), "rev-parse", "HEAD"], text=True).strip()
    if actual != plan["target_sha"]:
        raise ValueError("Checkout does not match pinned target")
    output.mkdir(parents=True, exist_ok=True)
    selected = [e for e in plan["tests"] if e["lane"] == lane]
    # Only receipts produced after this attempt begins may be collected
    for name in ("attempt.json", "journal.jsonl", "master-process.json", "results.json", "execution-progress.json"):
        (output / name).unlink(missing_ok=True)
    for entry in selected:
        if entry["owner"] != "master":
            (output / (entry["id"].replace(":", "_") + ".json")).unlink(missing_ok=True)
    three_player_source = repo / "android/temp/lan-three-player-startup/three-player-summary.json"
    three_player_destination = output / "three-player/three-player-summary.json"
    if any(e["id"] in THREE_PLAYER_IDS for e in selected):
        three_player_source.unlink(missing_ok=True)
        three_player_destination.unlink(missing_ok=True)
    attempt = {**attempt_identity(plan, lane), "processes": {}}
    write(output / "attempt.json", attempt)

    def process_evidence(owner, completed=False, exit_code=None):
        attempt["processes"][owner] = {"completed": completed, "exit_code": exit_code}
        write(output / "attempt.json", attempt)

    receipt = {key: plan[key] for key in ("schema", "target_sha", "catalog_hash", "batch_id")}
    if plan.get("retail_assets"):
        receipt["retail_assets"] = plan["retail_assets"]
    receipt.update(lane=lane, results=[])
    run_url = (
        os.environ.get("GITHUB_SERVER_URL", "https://github.com")
        + "/"
        + os.environ.get("GITHUB_REPOSITORY", "local/unpublished")
        + "/actions/runs/"
        + os.environ.get("GITHUB_RUN_ID", "local")
    )

    def save(entry, status, reason="", **detail):
        receipt["results"].append(
            {"id": entry["id"], "status": status, "reason": reason[:2048], "run_url": run_url, **detail}
        )
        write(output / "execution-progress.json", receipt)

    write(output / "execution-progress.json", receipt)
    masters = [e for e in selected if e["owner"] == "master"]
    if masters:
        names = output / "selected.json"
        write(names, [e["args"]["suite"] for e in masters])
        journal = output / "journal.jsonl"
        command = [
            "pwsh",
            "-NoProfile",
            "-File",
            str(repo / "android/run_all_tests.ps1"),
            "-SelectedTestsPath",
            str(names),
            "-EvidenceJournalPath",
            str(journal),
            "-TestBudgetSeconds",
            str(sum(e["estimated_seconds"] for e in masters)),
            "-FullSuite",
            "-FullExtracts",
            "-FullRouteCorpus",
            "-ExtendedGraphics",
            "-ExtendedMultiplayer",
            "-ReportDir",
            str(output / "master"),
        ]
        if lane in ("host", "jvm"):
            command.append("-HostOnly")
        # Master owns setup/cleanup and per-test timeout; outer hosted cancellation remains pending.
        process_evidence("master")
        try:
            with (output / "master.log").open("w") as log:
                process = run_command(command, repo, log)
        finally:
            if any(e["id"] in THREE_PLAYER_IDS for e in masters) and three_player_source.is_file():
                three_player_destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(three_player_source, three_player_destination)
        process_evidence("master", True, process.returncode)
        receipt["runner_failed"] = process.returncode != 0
        if receipt["runner_failed"]:
            print_public_jvm_failure(plan, lane, output)
        observations = journal_observations(journal, actual)
        for entry in masters:
            row = observations.get(entry["args"]["suite"], {})
            status = row.get("status", "NOT_RUN")
            if status == "SKIP":
                status = "BLOCKED"
            if entry["id"] in THREE_PLAYER_IDS:
                row = normalize_result(three_player_result({**row, "status": status}, output, actual), entry)
                status = row["status"]
            save(entry, status, row.get("reason", ""), exit_code=row.get("exit_code"))
    elapsed = 0.0
    for entry in selected:
        if entry["owner"] != "master" and elapsed >= sum(
            e["estimated_seconds"] for e in selected if e["owner"] != "master"
        ):
            save(entry, "NOT_RUN", "test execution budget exhausted")
            continue
        if entry["owner"] == "shareware":
            destination = output / (entry["id"].replace(":", "_") + ".json")
            command = [
                "pwsh",
                "-NoProfile",
                "-File",
                str(repo / "android/helpers/run_shareware_tests.ps1"),
                "-Filter",
                entry["args"]["suite"],
                "-Game",
                entry["args"]["game"],
            ]
            serial = os.environ.get("DXX_TEST_PRIMARY_SERIAL")
            if serial:
                command += ["-Serial", serial]
            # The master intentionally stops its AVDs. The shareware owner restarts
            # its own device; revalidate only the original CI UID grant first.
            start = time.monotonic()
            process_evidence(entry["id"])
            with destination.with_suffix(".log").open("w") as log:
                if os.environ.get("GITHUB_ACTIONS") == "true":
                    refresh = Path(__file__).resolve().parents[1] / "helpers/refresh_nightly_kvm_access.sh"
                    try:
                        setup_failed = run_command(["bash", str(refresh)], repo, log, timeout=60).returncode != 0
                    except subprocess.TimeoutExpired:
                        setup_failed = True
                    if setup_failed:
                        reason = "Approved KVM access refresh failed before shareware owner launch; see owner log"
                        write(
                            destination,
                            {
                                "schema": 1,
                                "suite": entry["args"]["suite"],
                                "status": "BLOCKED",
                                "reason": reason,
                                "attempted_tests": False,
                                "phase": "setup",
                                "retryable": True,
                                "blocker_kind": "infrastructure",
                                "coverage_complete": False,
                            },
                        )
                        process_evidence(entry["id"], True)
                        save(entry, "BLOCKED", reason)
                        receipt["runner_failed"] = True
                        continue
                try:
                    process = run_command(command, repo, log, timeout=max(120, entry["estimated_seconds"] * 2))
                    process_evidence(entry["id"], True, process.returncode)
                    status = "PASS" if process.returncode == 0 else "FAIL"
                except subprocess.TimeoutExpired:
                    process_evidence(entry["id"], True, 124)
                    status = "TIMEOUT"
            elapsed += time.monotonic() - start
            write(
                destination,
                {
                    "schema": 1,
                    "suite": entry["args"]["suite"],
                    "status": status,
                    "reason": "",
                    "elapsed_seconds": time.monotonic() - start,
                    "coverage_complete": False,
                },
            )
            save(entry, status)
            continue
        if entry["owner"] != "host-adapter":
            continue
        if elapsed >= sum(e["estimated_seconds"] for e in selected if e["owner"] == "host-adapter"):
            save(entry, "NOT_RUN", "test execution budget exhausted")
            continue
        destination = output / (entry["id"].replace(":", "_") + ".json")
        command = [
            sys.executable,
            str(Path(__file__).with_name("run_host_test_suites.py")),
            "--suite",
            entry["args"]["suite"],
            "--repo",
            str(repo),
            "--build-root",
            str(output / "build"),
            "--output",
            str(destination),
        ]
        if entry.get("fixture") == "retail-corpus":
            command += ["--timeout-seconds", str(max(600, entry["estimated_seconds"] * 2))]
        start = time.monotonic()
        process_evidence(entry["id"])
        with destination.with_suffix(".log").open("w") as log:
            process = run_command(command, repo, log)
        process_evidence(entry["id"], True, process.returncode)
        receipt["runner_failed"] = receipt.get("runner_failed", False) or process.returncode != 0
        if destination.exists():
            row = read(destination)
            if row.get("suite") != entry["args"]["suite"] or row.get("status") not in ("PASS", "FAIL", "BLOCKED"):
                raise ValueError("Invalid adapter result")
            if row["status"] == "PASS" and process.returncode != 0:
                row.update(status="FAIL", reason=f"Owner process exited {process.returncode} despite PASS receipt")
            row = normalize_result(row)
            elapsed += max(0, float(row.get("elapsed_seconds", time.monotonic() - start)))
            save(entry, row["status"], row.get("reason", ""), exclusions=row.get("exclusions", []))
        else:
            save(entry, "NOT_RUN", "adapter did not produce a terminal receipt")
    return receipt


STATE_BRANCH = "ci/nightly-tooling-state"


def github_api(path, method="GET", payload=None):
    repository = os.environ["GITHUB_REPOSITORY"]
    token = os.environ["GH_TOKEN"]
    request = urllib.request.Request(
        "https://api.github.com/repos/" + repository + "/" + path,
        data=json.dumps(payload).encode() if payload is not None else None,
        method=method,
        headers={
            "Authorization": "Bearer " + token,
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
        },
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)


def remote_state():
    try:
        ref = github_api("git/ref/heads/" + STATE_BRANCH)
    except urllib.error.HTTPError as error:
        if error.code == 404:
            return None, None
        raise
    head = ref["object"]["sha"]
    content = github_api("contents/ledger.json?ref=" + head)
    if content.get("size", 0) > 900000 or content.get("encoding") != "base64":
        raise ValueError("Remote ledger exceeds compact JSON limit")
    state = json.loads(base64.b64decode(content["content"]))
    validate_state(state)
    return head, state


def publish_state(plan, policy, receipts):
    # Dedicated branch only; target and default branches are never mutated.
    for attempt in range(2):
        head, previous = remote_state()
        if previous is None or any(previous[key] != plan[key] for key in ("target_sha", "catalog_hash")):
            previous = fresh_state(plan["target_sha"], plan["catalog_hash"])
        merged = reconcile(plan, policy, previous, receipts)
        if head and merged == previous:
            return merged
        serialized = json.dumps(merged, separators=(",", ":")) + "\n"
        if len(serialized.encode()) > 900000:
            raise ValueError("Ledger exceeds bounded permanent summary size")
        blob = github_api("git/blobs", "POST", {"content": serialized, "encoding": "utf-8"})
        tree = github_api(
            "git/trees",
            "POST",
            {"tree": [{"path": "ledger.json", "mode": "100644", "type": "blob", "sha": blob["sha"]}]},
        )
        commit = github_api(
            "git/commits",
            "POST",
            {
                "message": "Record nightly tooling sweep " + plan["target_sha"],
                "tree": tree["sha"],
                "parents": [head] if head else [],
            },
        )
        try:
            if head:
                github_api("git/refs/heads/" + STATE_BRANCH, "PATCH", {"sha": commit["sha"], "force": False})
            else:
                github_api("git/refs", "POST", {"ref": "refs/heads/" + STATE_BRANCH, "sha": commit["sha"]})
            return merged
        except urllib.error.HTTPError as error:
            if attempt or error.code not in (409, 422):
                raise
    raise RuntimeError("State publication conflict")


def selected_without_pass(plan, state):
    return [entry["id"] for entry in plan["tests"] if state["records"].get(entry["id"], {}).get("status") != "PASS"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("plan")
    for option in ("target-sha", "catalog", "policy", "output", "state-output"):
        p.add_argument("--" + option, required=True)
    p.add_argument("--state")
    p.add_argument("--validation", action="store_true")
    p.add_argument(
        "--campaign",
        choices=(
            "first-play",
            "graphics-capabilities",
            "basic-use",
            "xl-browser",
            "xl-textures",
            "xl-originals",
            "lan-smoke",
            "online-preview",
            "online-preview-d2",
            "online-transports",
            "lan-three-player",
        ),
    )
    p.add_argument("--omit-pr-public-lan", action="store_true")
    p = sub.add_parser("retail-check")
    for option in ("plan", "policy", "lane"):
        p.add_argument("--" + option, required=True)
    p = sub.add_parser("execute")
    for option in ("plan", "policy", "repo", "output"):
        p.add_argument("--" + option, required=True)
    p.add_argument("--lane", choices=LANES, required=True)
    p = sub.add_parser("collect")
    for option in ("plan", "policy", "output"):
        p.add_argument("--" + option, required=True)
    p.add_argument("--lane", choices=LANES, required=True)
    p.add_argument("--blocked-reason", default="")
    p = sub.add_parser("reconcile")
    p.add_argument("--require-selected", action="store_true")
    for option in ("plan", "policy", "state", "output"):
        p.add_argument("--" + option, required=True)
    p.add_argument("--receipts", nargs="*", default=[])
    p = sub.add_parser("fetch-state")
    p.add_argument("--output", required=True)
    p = sub.add_parser("publish")
    for option in ("plan", "policy", "expected-target", "output"):
        p.add_argument("--" + option, required=True)
    p.add_argument("--receipts", nargs="*", default=[])
    args = parser.parse_args()
    if args.command == "fetch-state":
        _, state = remote_state()
        if state is not None:
            write(args.output, state)
    elif args.command == "publish":
        plan = read(args.plan)
        if plan["target_sha"] != args.expected_target:
            raise ValueError("Coordinator target mismatch")
        state = publish_state(plan, read(args.policy), [read(p) for p in args.receipts])
        write(args.output, state)
    elif args.command == "retail-check":
        plan = read(args.plan)
        validate_plan(plan, read(args.policy))
        if (
            not retail.lane_requires_retail(plan, args.lane)
            or not retail.trusted_context()
            or plan.get("retail_assets") != retail.configured_identity()
        ):
            raise ValueError("Private checkout is not authorized by this trusted run and plan")
        print("Verified trusted retail lane and immutable data identity")
    elif args.command == "plan":
        retail_assets = retail.configured_identity()
        if retail.corpus_available(retail_assets):
            controller_sha = subprocess.check_output(
                ["git", "-C", str(corpus.ROOT), "rev-parse", "HEAD"], text=True
            ).strip()
            if controller_sha != args.target_sha:
                raise ValueError(
                    "Corpus controller/catalog differs from exact target; retry the current cmake revision"
                )
        plan, state = plan_batch(
            args.target_sha,
            read(args.catalog),
            read(args.policy),
            read(args.state) if args.state and Path(args.state).exists() else None,
            retail_assets=retail_assets,
        )
        if args.validation:
            state = fresh_state(plan["target_sha"], plan["catalog_hash"])
            validation_ids = VALIDATION_IDS
            plan["tests"] = [e for e in policy_entries(read(args.policy)).values() if e["id"] in validation_ids]
            if {e["id"] for e in plan["tests"]} != validation_ids:
                raise ValueError("Missing representative validation suites")
            plan["estimated_seconds"] = sum(e["estimated_seconds"] for e in plan["tests"])
            plan["lanes"] = [lane for lane in LANES if any(e["lane"] == lane for e in plan["tests"])]
            identity = {key: plan[key] for key in ("target_sha", "catalog_hash", "tests")}
            if plan.get("retail_assets"):
                identity["retail_assets"] = plan["retail_assets"]
            plan["batch_id"] = digest(identity)
        if args.campaign:
            if args.validation:
                raise ValueError("Choose validation or campaign, not both")
            plan = (
                select_xl_originals(plan, read(args.policy), read(args.catalog))
                if args.campaign == "xl-originals"
                else select_public_campaign(plan, read(args.policy), read(args.catalog), args.campaign)
            )
            state = fresh_state(plan["target_sha"], plan["catalog_hash"])
        if args.omit_pr_public_lan:
            if not (args.validation or args.campaign == "first-play"):
                raise ValueError("LAN omission is only for extended PR selections")
            plan = omit_pr_public_lan(plan, read(args.policy))
        validate_plan(plan, read(args.policy))
        write(args.output, plan)
        write(args.state_output, state)
        print(json.dumps({"lanes": plan["lanes"], "count": len(plan["tests"]), "batch_id": plan["batch_id"]}))
    elif args.command == "execute":
        signal.signal(signal.SIGTERM, lambda *_: sys.exit(143))
        result = execute(read(args.plan), read(args.policy), args.lane, args.repo, args.output)
        return int(
            result.get("runner_failed", False) or any(r["status"] in ("FAIL", "TIMEOUT") for r in result["results"])
        )
    elif args.command == "collect":
        collect(read(args.plan), read(args.policy), args.lane, args.output, args.blocked_reason)
    else:
        plan, policy = read(args.plan), read(args.policy)
        state = reconcile(plan, policy, read(args.state), [read(p) for p in args.receipts])
        write(args.output, state)
        summary = {
            "target_sha": state["target_sha"],
            "terminal_owner_attempts": sum(r["status"] in COVERED for r in state["records"].values()),
            "fully_covered_owners": sum(r.get("coverage_complete") is True for r in state["records"].values()),
            "passed_leaf_cases": sum(r.get("case_counts", {}).get("PASS", 0) for r in state["records"].values()),
            "incomplete_owners": [k for k, r in state["records"].items() if r.get("coverage_complete") is not True],
            "failures": [k for k, r in state["records"].items() if r["status"] in ("FAIL", "TIMEOUT")],
            "retryable_infrastructure": state.get("retryable", {}),
            "excluded": plan["exclusions"],
            "target_capability_exclusions": state.get("excluded", {}),
            "unavailable_pending": plan.get("unavailable", []),
        }
        summary["failure_details"] = {
            key: {
                field: row[field]
                for field in ("status", "reason", "failure_categories", "worker_exit_codes")
                if field in row
            }
            for key, row in state["records"].items()
            if row["status"] in ("FAIL", "TIMEOUT")
        }
        pending = (
            set(policy_entries(policy))
            - set(state["records"])
            - set(state.get("excluded", {}))
            - {e["id"] for e in plan["exclusions"]}
        )
        summary["matching_unconfirmed_routes"] = {
            key: row["runtime_outcome"] for key, row in state["records"].items() if row.get("comparison_only")
        }
        summary["pending_owners"] = len(pending)
        summary["outcome"] = (
            "incomplete"
            if pending
            else (
                "exhausted_with_failures"
                if summary["failures"]
                else (
                    "exhausted_with_exclusions"
                    if summary["incomplete_owners"] or summary["excluded"] or state.get("excluded")
                    else "complete_pass"
                )
            )
        )
        summary["selected_without_pass"] = selected_without_pass(plan, state)
        print(json.dumps(summary, indent=2))
        if args.require_selected and summary["selected_without_pass"]:
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
