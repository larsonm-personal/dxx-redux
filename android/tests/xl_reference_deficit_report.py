"""Compare a full-survey failure list to ordinary Redux reruns and original XL observations.

Supply observations for one original archive per invocation. Each observation is
a child directory produced by xl_reference_campaign.py using the same private
installation. Raw paths and game files stay in that installation.
"""

import argparse
from collections import Counter, defaultdict
from pathlib import Path

from prepare_xl_campaign_reference import REVISION
from xl_readiness import digest, write
from xl_readiness_full_report import read, summarize
from xl_reference_campaign import classify


def compare(args):
    catalog = read(args.catalog)
    source = read(args.source_groups)
    archive_sha = digest(args.archive)
    archives = [a for a in catalog["archives"] if a["sha256"] == archive_sha]
    if len(archives) != 1 or source["inventory_sha256"] != digest(args.catalog):
        raise ValueError("Archive or source survey does not match the inventory")
    archive = archives[0]
    cases = {case["id"]: case for case in catalog["cases"] if case["archive"] == archive["path"]}
    failures = [row for row in source["failures"] if row["id"] in cases]
    expected = {row["id"] for row in failures}
    rerun = summarize(args.redux_work, args.catalog)
    measured = read(args.redux_work / "results.json")
    outcomes = {row["id"]: row["outcome"] for row in measured["rows"]}
    if not expected or len(failures) != len(expected) or expected != set(outcomes):
        raise ValueError("Redux rerun must cover each source failure in this archive exactly once")

    reference_sha = digest(args.reference_engine)
    selection = {(Path(cases[i]["descriptor"]), cases[i]["level"]): i for i in expected}
    original = {}
    for receipt in sorted(args.reference_runs.glob("*/result.json")):
        result = read(receipt)
        relative = Path("missions") / result["descriptor"]
        ordinal = result["ordinal"]
        ident = selection.get((relative, ordinal))
        if ident is None or ident in original:
            raise ValueError(f"Unexpected or duplicate original observation at {receipt}")
        case = cases[ident]
        descriptor = (args.installation / relative).resolve()
        trace = receipt.with_name("reference.tsv")
        trace_hash = digest(trace) if trace.exists() else None
        if (
            result["schema"] != "xl-original-campaign-probe-v1"
            or result["reference_revision"] != REVISION
            or result["engine_sha256"] != reference_sha
            or result["descriptor_sha256"] != case["descriptor_sha256"]
            or digest(descriptor) != case["descriptor_sha256"]
            or result["advertised_file"].casefold() != case["level_file"].casefold()
            or result["receipt_sha256"] != trace_hash
            or result["timed_out"] != (result["exit_code"] is None)
        ):
            raise ValueError(f"Original input or receipt identity differs for {ident}")
        events = (
            [line.split("\t") for line in trace.read_text(encoding="utf-8", errors="replace").splitlines()]
            if trace.exists()
            else []
        )
        observation = classify(result["exit_code"], events, descriptor, case["level_file"], ordinal)
        if observation != result["observation"]:
            raise ValueError(f"Original result differs from its 120-frame trace for {ident}")
        original[ident] = dict(observation=observation, exit_code=result["exit_code"], receipt_sha256=trace_hash)
    if original.keys() != expected:
        raise ValueError("Missing original observations for this archive")

    current = {row["id"]: row["first_blocker"] for row in rerun["failures"]}
    groups = defaultdict(Counter)
    rows = []
    for row in failures:
        ident = row["id"]
        groups[row["first_blocker"]][original[ident]["observation"]] += 1
        rows.append(
            dict(**row, redux_rerun=outcomes[ident], rerun_blocker=current.get(ident), original=original[ident])
        )
    return dict(
        schema="xl-reference-deficit-comparison-v1",
        archive=archive,
        inventory_sha256=source["inventory_sha256"],
        source_survey_sha256=digest(args.source_groups),
        source_target=source["target_sha"],
        source_engine=source["engine_sha256"],
        redux_rerun_target=rerun["target_sha"],
        redux_rerun_engine=rerun["engine_sha256"],
        original_revision=REVISION,
        original_engine=reference_sha,
        original_outcomes=Counter(row["observation"] for row in original.values()),
        by_source_blocker=dict(groups),
        rows=rows,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in (
        "source-groups",
        "redux-work",
        "archive",
        "installation",
        "reference-engine",
        "reference-runs",
        "output",
    ):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--catalog", type=Path, default=Path(__file__).with_name("xl_readiness_inventory.json"))
    args = parser.parse_args()
    report = compare(args)
    write(args.output, report)
    print(dict(report["original_outcomes"]))


if __name__ == "__main__":
    main()
