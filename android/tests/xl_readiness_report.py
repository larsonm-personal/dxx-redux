"""Trusted, bounded publication of sampled readiness, independently of regression CI."""

import argparse
import hashlib
import html
import json
import os
from pathlib import Path

import nightly_pr_candidate as candidate
from xl_readiness import ASSET_COMMIT, NEW_CAMPAIGNS, OUTCOMES, summary

CONTEXT = "D2X readiness (sampled load/tick)"


def validate(receipt, catalog, sha):
    if not isinstance(receipt, dict) or not isinstance(catalog, dict):
        raise TypeError("Readiness receipt and catalog must be objects")
    if (
        receipt.get("schema") != "dxx-xl-readiness-v1"
        or receipt.get("target_sha") != sha
        or receipt.get("asset_commit") != ASSET_COMMIT
        or catalog.get("asset_commit") != ASSET_COMMIT
        or receipt.get("selection_seed") != catalog["selection_seed"]
        or receipt.get("inventory_levels") != len(catalog["cases"])
        or receipt.get("inventory_archives") != len(catalog["archives"])
    ):
        raise ValueError("Readiness identity mismatch")
    rows = receipt.get("rows")
    if (
        not isinstance(rows, list)
        or not all(isinstance(row, dict) for row in rows)
        or [r.get("id") for r in rows] != catalog["selected"]
    ):
        raise ValueError("Frozen cohort membership changed")
    cases = {c["id"]: c for c in catalog["cases"]}
    for row in rows:
        if row.get("outcome") not in OUTCOMES or row.get("assisted_navigation") not in (
            "not_run",
            "confirmed",
            "unconfirmed",
            "crash",
            "timeout",
            "infrastructure_error",
        ):
            raise ValueError("Invalid case outcome")
        if row["outcome"] != "untested":
            if any(row.get(k) != cases[row["id"]][k] for k in ("mission", "level", "level_file", "archive")):
                raise ValueError("Case identity mismatch")
        if row.get("xl_active") is not None and type(row["xl_active"]) is not bool:
            raise ValueError("Invalid runtime classification")
        if row["outcome"] == "loaded_ticked" and (row.get("frames") != 120 or type(row.get("xl_active")) is not bool):
            raise ValueError("Missing bounded ticking evidence")
    return summary(rows)


def expected_failures_match(receipt, catalog, baseline, catalog_digest, measurement_result):
    """Waive only the reviewed per-case gaps, never missing or failed measurement jobs."""
    if not isinstance(baseline, dict):
        raise TypeError("Readiness baseline must be an object")
    if (
        measurement_result != "success"
        or baseline.get("schema") != "dxx-xl-readiness-expected-v1"
        or baseline.get("asset_commit") != ASSET_COMMIT
        or baseline.get("catalog_sha256") != catalog_digest
        or baseline.get("selection_seed") != catalog["selection_seed"]
    ):
        return False
    expected = baseline.get("rows")
    if (
        not isinstance(expected, list)
        or not all(isinstance(row, dict) for row in expected)
        or [r.get("id") for r in expected] != catalog["selected"]
    ):
        return False
    # A passing case may not be traded for a new failing one at the same percentage.
    # Improvements are welcome, but still require normal bounded ticking validation.
    for row, prior in zip(receipt["rows"], expected):
        if prior.get("outcome") not in ("loaded_ticked", "load_failed"):
            return False
        if row["outcome"] == "loaded_ticked":
            continue
        if type(row.get("frames")) is not int:
            return False
        if prior["outcome"] != "load_failed" or any(
            key not in row or key not in prior or row[key] != prior[key]
            for key in ("outcome", "frames", "xl_active", "assisted_navigation")
        ):
            return False
        # v1 receipts have no root-cause fingerprint. Do not silently ignore one
        # if a future producer adds it; the reviewed baseline must be updated.
        if row.get("failure_signature") != prior.get("failure_signature"):
            return False
    return summary(receipt["rows"])["complete"]


def markdown(receipt, catalog, result, expected=False):
    title = f"D2X readiness: {result['readiness_percent']}%" if result["complete"] else "D2X readiness: incomplete"
    new_rows = [r for r in receipt["rows"] if Path(r.get("archive", r["id"].split("/")[0])).name in NEW_CAMPAIGNS]
    new_passed = sum(r["outcome"] == "loaded_ticked" for r in new_rows)
    xl_passed = sum(r["outcome"] == "loaded_ticked" and r.get("xl_active") is True for r in receipt["rows"])
    classic_passed = sum(r["outcome"] == "loaded_ticked" and r.get("xl_active") is False for r in receipt["rows"])
    lines = [
        "## " + title,
        "",
        f"**{result['passed']}/{result['denominator']} sampled levels confirmed**: original load plus 120 unassisted frames at 60 Hz, without death or level transition.",
        "",
        f"**Newly uploaded XL campaigns: {new_passed}/{len(new_rows)} confirmed.** Passing runtime paths: {xl_passed} XL-active, {classic_passed} classic/imported. Failed loads have unknown runtime classification; the overall percentage must not be interpreted as XL-specific compatibility.",
        "",
        f"Fixed cohort covers {len(catalog['selected'])}/{len(catalog['cases'])} advertised levels from {len(catalog['archives'])} uploaded archives. This is not a random population estimate, full playability score, rendering test, or campaign completion claim.",
        "",
        "Unsupported formats, load/simulation failures, crashes and timeouts are unsuccessful compatibility outcomes. Infrastructure errors and untested cases make the percentage incomplete. Assisted navigation is separate and does not raise this score.",
        "",
        "| Case | Load/tick outcome | Runtime path | Assisted navigation |",
        "|---|---|---|---|",
    ]
    for row in receipt["rows"]:
        identity = html.escape(row["id"]).replace("|", "\\|").replace("`", "")
        runtime = (
            "XL-active"
            if row.get("xl_active") is True
            else ("classic/imported" if row.get("xl_active") is False else "unknown")
        )
        lines.append(f"| {identity} | {row['outcome']} | {runtime} | {row['assisted_navigation']} |")
    lines += [
        "",
        (
            "Expected-failure baseline matched: the reporting job may succeed, but readiness remains unsuccessful until 8/8. This does not make asset smoke or regression tests pass."
            if expected
            else "Expected-failure baseline not applied: unsuccessful or incomplete readiness remains a reporting failure."
        ),
        "The temporary baseline compares exact cases and published load/tick classifications, frames, runtime classification and navigation omission. Current load_failed receipts have no root-cause fingerprint, so a different underlying load error in the same case cannot be distinguished. Changed published signatures are not waived.",
        "",
        "Camera addon subchecks are not covered by this load/tick survey. The separate regression receipt explicitly reports camera subcheck execution or omission.",
        "",
        "Cohort IDs and pinned archive identities are committed in `android/tests/xl_readiness_inventory.json`. Failed cases stay selected across commits; only an explicit reviewed cohort revision may change membership.",
        "",
    ]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--expected-failures", type=Path, help="Trusted controller baseline, never candidate input")
    parser.add_argument("--measurement-result", default=os.environ.get("READINESS_RESULT"))
    parser.add_argument("--local-sha", help="Render locally without GitHub access or status mutation")
    args = parser.parse_args()
    sha = args.local_sha or os.environ["CANDIDATE_SHA"]
    candidate.STATUS_CONTEXT = CONTEXT
    if not args.local_sha:
        candidate.verify(sha)
    try:
        if args.receipt.is_symlink() or args.receipt.stat().st_size > 1024 * 1024:
            raise ValueError("Readiness receipt exceeds its bound")
        catalog = json.loads(args.catalog.read_text())
        receipt = json.loads(args.receipt.read_text())
        result = validate(receipt, catalog, sha)
        expected = False
        if args.expected_failures:
            expected = expected_failures_match(
                receipt,
                catalog,
                json.loads(args.expected_failures.read_text()),
                hashlib.sha256(args.catalog.read_bytes()).hexdigest(),
                args.measurement_result,
            )
        text = markdown(receipt, catalog, result, expected)
        print(text)
        if os.environ.get("GITHUB_STEP_SUMMARY"):
            with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as stream:
                stream.write(text)
        complete = result["complete"]
        description = (
            f"D2X readiness {result['readiness_percent']}% ({result['passed']}/{result['denominator']}; sampled load/tick)"
            if complete
            else f"D2X readiness incomplete ({result['passed']}/{result['denominator']} confirmed; infrastructure/untested)"
        )
        measurement_ok = not args.expected_failures or args.measurement_result == "success"
        if not measurement_ok:
            description = "D2X readiness unavailable: measurement job did not succeed"
        if not args.local_sha:
            candidate.status(
                sha,
                "success" if measurement_ok and complete and result["passed"] == result["denominator"] else "failure",
                description,
            )
        return 0 if (measurement_ok and complete and result["passed"] == result["denominator"]) or expected else 1
    except (OSError, ValueError, KeyError, TypeError):
        if not args.local_sha:
            candidate.status(sha, "error", "D2X readiness unavailable: missing or invalid measurement receipt")
        raise


if __name__ == "__main__":
    raise SystemExit(main())
