"""Trusted, bounded publication of sampled readiness, independently of regression CI."""

import argparse
import html
import json
import os
from pathlib import Path

import nightly_pr_candidate as candidate
from xl_readiness import ASSET_COMMIT, NEW_CAMPAIGNS, OUTCOMES, summary

CONTEXT = "D2X readiness (sampled load/tick)"


def validate(receipt, catalog, sha):
    if (
        receipt.get("schema") != "dxx-xl-readiness-v1"
        or receipt.get("target_sha") != sha
        or receipt.get("asset_commit") != ASSET_COMMIT
    ):
        raise ValueError("Readiness identity mismatch")
    rows = receipt.get("rows")
    if not isinstance(rows, list) or [r.get("id") for r in rows] != catalog["selected"]:
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


def markdown(receipt, catalog, result):
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
        text = markdown(receipt, catalog, result)
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
        if not args.local_sha:
            candidate.status(
                sha, "success" if complete and result["passed"] == result["denominator"] else "failure", description
            )
        return 0 if complete and result["passed"] == result["denominator"] else 1
    except (OSError, ValueError, KeyError, TypeError):
        if not args.local_sha:
            candidate.status(sha, "error", "D2X readiness unavailable: missing or invalid measurement receipt")
        raise


if __name__ == "__main__":
    raise SystemExit(main())
