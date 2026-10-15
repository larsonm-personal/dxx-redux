"""Regenerate static admission accounting; never infer runtime passes from policy."""

import argparse
import hashlib
import json
from collections import Counter, defaultdict
from pathlib import Path


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def reconcile(root):
    historical = read(root / "nightly_source_reconciliation.json")
    ownership = {
        r["path"]: r for r in read(root / "nightly_source_ownership.json")["files"]
    }
    policy_path = root / "nightly_tooling_policy.json"
    policy = read(policy_path)
    admitted = {r["id"] for r in policy["suites"]}
    by_name = defaultdict(list)
    for row in policy["suites"]:
        by_name[row["args"]["suite"]].append(row["id"])
    by_path = defaultdict(list)
    for row in read(root / "host_test_suites.json")["suites"]:
        if row["id"] in admitted and row.get("path"):
            by_path[row["path"]].append(row["id"])
    owners = []
    for audit_name in ("host_owner_audit.json", "android_owner_audit.json"):
        for row in read(root / audit_name)["suites"]:
            name = row.get("name") or row["id"].removeprefix("master:")
            ids = sorted(set(by_name[name] + by_path[row["path"]]))
            owners.append(
                {
                    "name": name,
                    "path": row["path"],
                    "audit_file": audit_name,
                    "disposition": row["disposition"],
                    "selection_ids": ids,
                    "admission": "selected-profile" if ids else "audited-not-selected",
                }
            )
    assert len(owners) == len({r["name"] for r in owners}) == 346
    owner_by_path = {r["path"]: r for r in owners}
    owner_by_name = {r["name"]: r for r in owners}
    files = []
    for old in historical["files"]:
        row = dict(old)
        mapped = ownership[row["path"]]
        row.update(
            ownership_status=mapped["ownership_status"], dispatcher=mapped["dispatcher"]
        )
        owner = owner_by_path.get(row["path"])
        child = owner_by_name.get(Path(row.get("declared_owner") or "").stem)
        ids = sorted(
            set(
                by_path[row["path"]]
                + [i for i in row["selection_ids"] if i in admitted]
            )
        )
        if owner:
            ids = owner["selection_ids"]
            row["classification"] = (
                "directly-selectable" if ids else "audited-not-selected"
            )
            row["audit_reference"] = owner["audit_file"] + "#" + owner["name"]
            row["reason"] = (
                "Owner audited: "
                + owner["disposition"]
                + ". Only listed policy profiles are admitted; see exact prerequisites in audit."
            )
            row["ownership_status"] = "audited-owner"
        elif child:
            ids = child["selection_ids"]
            row["classification"] = (
                "child-of-admitted-owner"
                if ids
                else "child-of-audited-unselected-owner"
            )
            row["audit_reference"] = child["audit_file"] + "#" + child["name"]
            row["reason"] = (
                "Declared child ownership is known; eligibility follows the explicitly selected owner profile, not all variants."
            )
        elif ids:
            row["classification"] = (
                "directly-selectable"
                if by_path[row["path"]]
                else "child-of-admitted-owner"
            )
        elif row["classification"] not in ("support/non-test", "excluded"):
            row["classification"] = (
                "known-ownership-unselected"
                if mapped["ownership_status"] != "unresolved"
                else "still-unmapped"
            )
            row["reason"] = (
                "Dispatcher/source ownership is known, but no reviewed selected-profile mapping."
                if mapped["ownership_status"] != "unresolved"
                else "No authoritative owner or runnable profile mapping yet; not an unsupported or licensed-media claim."
            )
        if row["ownership_status"] == "unresolved" and ids:
            row["ownership_status"] = "selected-entrypoint"
        row["selection_ids"] = ids
        files.append(row)
    ctests = []
    for old in historical["ctest_mentions"]:
        row = dict(old)
        project = row["path"]
        if "/fluidsynth_quality/" in project:
            row.update(
                selection_ids=["cmake:fluidsynth-contracts"],
                classification="child-of-admitted-owner",
                reason="Original standalone contract project; verified public SoundFont provisioned by adapter.",
            )
        elif "/etc2tool/" in project:
            row.update(
                selection_ids=["cmake:etc2-limits"],
                classification="child-of-admitted-owner",
                reason="Original standalone limit test project; build dependencies provisioned by adapter.",
            )
        elif "/fm_quality/" in project:
            row.update(
                selection_ids=["extractor:full"],
                classification="same-source-different-build",
                reason="Equivalent test sources execute through extractor CMake; this separate legacy build configuration is not qualified.",
            )
        elif "/pcm_ring/" in project:
            row.update(
                selection_ids=[],
                classification="known-android-owner",
                reason="test_pcm_ring.ps1 builds with Android NDK and runs via ADB; source ownership is known, this standalone CMake invocation is not admitted as a host test.",
            )
        ctests.append(row)
    counts = dict(sorted(Counter(r["classification"] for r in files).items()))
    return {
        "schema": 2,
        "source_commit": historical["source_commit"],
        "policy_sha256": hashlib.sha256(policy_path.read_bytes()).hexdigest(),
        "selection_count": len(admitted),
        "candidate_file_count": len(files),
        "file_classification_counts": counts,
        "audited_master_owner_count": len(owners),
        "owner_admission_counts": dict(Counter(r["admission"] for r in owners)),
        "ownership_counts": dict(Counter(r["ownership_status"] for r in files)),
        "runtime_qualification": "Not inferred. Per-run receipts and test cases are authoritative; this file contains static source/admission evidence only.",
        "historical_snapshot": "nightly_source_reconciliation.json",
        "ctest_mention_count": len(ctests),
        "ctest_classification_counts": dict(
            Counter(r["classification"] for r in ctests)
        ),
        "owners": owners,
        "files": files,
        "ctest_mentions": ctests,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--tests-root", type=Path, default=Path(__file__).resolve().parent
    )
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = reconcile(args.tests_root)
    output = args.output or args.tests_root / "nightly_current_reconciliation.json"
    # One complete record per line keeps the checked-in evidence diffable.
    arrays = {key: result.pop(key) for key in ("owners", "files", "ctest_mentions")}
    text = json.dumps(result, indent=2)[:-2]
    for key, rows in arrays.items():
        text += ",\n  " + json.dumps(key) + ": [\n"
        text += (
            ",\n".join("    " + json.dumps(row, ensure_ascii=False) for row in rows)
            + "\n  ]"
        )
    output.write_text(text + "\n}\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
