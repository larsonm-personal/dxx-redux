"""Fail-closed comparisons for existing, non-publishing regression owners.

Inputs are decoded JSON from an exact target checkout and a fresh owner output
folder. No baseline is updated. Projected metadata has no volatile generation
field: dates, timing, RNG, objectives and every other semantic field are kept.
Unbound matcen masks and models used exclusively by them are retained in raw
output but excluded from semantic baseline comparison because they depend on
previously loaded levels.
"""

import copy
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "helpers"))
from compare_matcen_metadata import MatcenMismatch, baseline_capture


def _metadata_baseline(value):
    result = copy.deepcopy(value)
    for mission in _missions(result) or []:
        for level in mission.get("levels", []):
            if "matcen_regression" in level:
                capture = level["matcen_regression"]
                # Incomplete/rejected captures have no validated observations to
                # exclude. Retain their full strict comparison and admission state.
                if (
                    isinstance(capture, dict)
                    and capture.get("capture_status") == "complete"
                    and capture.get("admission") == "loaded"
                ):
                    level["matcen_regression"] = baseline_capture(capture)
    return result


def freeze_oracles(paths):
    return {path: path.read_bytes() if path.is_file() else None for path in paths}


def require_unchanged_oracles(snapshot):
    if freeze_oracles(snapshot) != snapshot:
        raise ValueError("Regression owner modified its committed oracle")


def _receipt(status, reason, **details):
    return dict(status=status, reason=reason, **details)


def _missions(value):
    if isinstance(value, dict):
        return [value]
    if isinstance(value, list) and value and all(isinstance(x, dict) for x in value):
        return value
    return None


# Field names are fixed schema vocabulary, never mission values or arbitrary keys.
DIAGNOSTIC_FIELDS = frozenset(('levels', 'status', 'segment_count', 'level_num', 'level_file',
    'matcen_regression', 'capture_status', 'admission', 'robots', 'models', 'objects',
    'vertices', 'segments', 'walls', 'triggers', 'data_warnings', 'game', 'target_index', 'mission_path',
    'mission_filename', 'robot_count', 'object_count', 'version', 'reactor', 'secret_exit'))


def metadata_differences(expected, actual, path=()):
    differences = []

    def visit(left, right, at):
        if len(differences) >= 16 or left == right:
            return
        if type(left) is not type(right):
            differences.append({"path": list(at), "kind": "type"})
        elif isinstance(left, dict):
            for key in sorted(left.keys() | right.keys()):
                child = at + (key if key in DIAGNOSTIC_FIELDS else "field",)
                if key not in left or key not in right:
                    if len(differences) < 16:
                        differences.append({"path": list(child), "kind": "missing-field"})
                else:
                    visit(left[key], right[key], child)
        elif isinstance(left, list):
            if len(left) != len(right):
                differences.append({"path": list(at), "kind": "length"})
            for index, (a, b) in enumerate(zip(left, right)):
                visit(a, b, at + (index,))
        else:
            differences.append({"path": list(at), "kind": "value"})

    visit(expected, actual, path)
    return differences


WORKER_CATEGORIES = frozenset(('assertion', 'address-sanitizer', 'undefined-behavior',
    'segmentation-fault', 'cpp-exception', 'worker-timeout', 'unexpected-eof',
    'worker-shutdown', 'archive-extraction', 'descriptor-invalid', 'variant-selection',
    'projection-worker', 'unclassified-owner-failure'))


def worker_failure_categories(text):
    # Match source-defined signatures; never return captured text from a log.
    patterns = (
        ("assertion", r"(?i)assertion[^\n]{0,2048}failed|assert failed"),
        ("address-sanitizer", r"AddressSanitizer:"),
        ("undefined-behavior", r"UndefinedBehaviorSanitizer:|runtime error:"),
        ("segmentation-fault", r"(?i)segmentation fault"),
        ("cpp-exception", r"terminate called after throwing"),
        ("worker-timeout", r"metadata (?:archive )?worker timed out"),
        ("unexpected-eof", r"metadata worker closed its output unexpectedly"),
        ("worker-shutdown", r"Metadata worker shutdown failed"),
        ("archive-extraction", r"7z extract failed"),
        ("descriptor-invalid", r"Invalid mission descriptor"),
        ("variant-selection", r"Shared mission archive variant selection"),
        ("projection-worker", r"Kotlin metadata worker (?:exited|closed)"),
    )
    return [name for name, pattern in patterns if re.search(pattern, text)]


def metadata_owner_failure(summary, archive_name):
    if not isinstance(summary, list):
        return {}
    records = [row for row in summary if isinstance(row, dict) and row.get("name") == archive_name]
    if len(records) != 1 or records[0].get("status") != "failed":
        return {}
    reason = str(records[0].get("reason", ""))[:16384]
    result = {"categories": worker_failure_categories(reason) or ["unclassified-owner-failure"]}
    exit_match = re.search(
        r"metadata (?:archive )?worker (?:closed its output unexpectedly with exit code|exited) (-?\d{1,10})", reason
    )
    if exit_match:
        result["exit_code"] = int(exit_match[1])
    return result


def metadata_worker_diagnostics(root):
    root = Path(root)
    if not root.is_dir() or root.is_symlink():
        return []

    def tail(path):
        if not path.is_file() or path.is_symlink():
            return ""
        with path.open("rb") as stream:
            stream.seek(max(0, path.stat().st_size - 131072))
            return stream.read(131072).decode("utf-8", errors="replace")

    rows = []
    prefixes = sorted(
        {path.name.rsplit(".", 2)[0] for pattern in ("*.start.json", "*.shutdown.json") for path in root.glob(pattern)}
    )
    roles = {"dxx-redux-d1-metadata-worker": "d1-metadata", "dxx-redux-d2-metadata-worker": "d2-metadata"}
    for prefix in prefixes[:16]:
        path = root / (prefix + ".shutdown.json")
        row = {"shutdown_missing": not path.is_file()}
        data = {}
        if path.is_file():
            if path.is_symlink() or path.stat().st_size > 16384:
                continue
            try:
                data = json.loads(path.read_text(encoding="utf-8-sig"))
                if not isinstance(data, dict):
                    raise ValueError("Invalid shutdown record")
            except (ValueError, OSError):
                rows.append({"categories": ["unclassified-owner-failure"]})
                continue
        row.update({key: data[key] for key in ("exit_code", "shutdown_ms") if type(data.get(key)) is int})
        row.update(
            {
                key: data[key]
                for key in ("timed_out", "stdout_complete", "stderr_complete")
                if type(data.get(key)) is bool
            }
        )
        row["worker"] = roles.get(Path(str(data.get("executable", ""))).name, "other")
        categories = set()
        for stream in ("stdout", "stderr"):
            output = root / (prefix + "." + stream + ".log")
            row[stream + "_categories"] = worker_failure_categories(tail(output))
            categories.update(row[stream + "_categories"])
            if output.is_file() and not output.is_symlink():
                row[stream + "_bytes"] = output.stat().st_size
        if data.get("timed_out") is True:
            categories.add("worker-timeout")
        if data.get("exit_code") not in (None, 0) and not categories:
            categories.add("unclassified-owner-failure")
        row["categories"] = sorted(categories)
        rows.append(row)
    return rows


def compare_metadata(baseline, generated, summary, archive_name):
    """Compare full projected archive JSON and its mandatory owner receipt.

    summary is regenerate_all_mission_metadata_host.ps1's summary.json array.
    Missing output/selection is BLOCKED; executed failures or semantic drift FAIL.
    """
    if baseline is None or generated is None or not isinstance(summary, list):
        return _receipt("BLOCKED", "Missing metadata baseline, generated output or summary")
    records = [r for r in summary if isinstance(r, dict) and r.get("name") == archive_name]
    if len(records) != 1:
        return _receipt("BLOCKED", "Selected archive missing or duplicated in owner summary")
    status = records[0].get("status")
    if status == "failed":
        return _receipt("FAIL", "Metadata owner reported failed")
    if status != "passed":
        return _receipt("BLOCKED", "Metadata owner did not execute selected archive successfully")
    expected, actual = _missions(baseline), _missions(generated)
    if expected is None or actual is None:
        return _receipt("BLOCKED", "Invalid or empty projected mission metadata")
    if any(
        not isinstance(m.get("levels"), list)
        or not m["levels"]
        or any(type(level.get("segment_count")) is not int or level["segment_count"] <= 0 for level in m["levels"])
        for m in actual
    ):
        return _receipt("FAIL", "Generated mission contains absent or empty level geometry")
    if any(m.get("status") != "ok" for m in actual):
        return _receipt("FAIL", "Generated mission metadata contains a non-ok outcome")
    if any(m.get("status") != "ok" for m in expected):
        return _receipt("BLOCKED", "Baseline mission metadata is not an ok oracle")
    # Preserve shape and semantic values/order; unbound robot masks are observations.
    try:
        expected_projection, actual_projection = _metadata_baseline(baseline), _metadata_baseline(generated)
        matches = expected_projection == actual_projection
    except (MatcenMismatch, KeyError, TypeError, ValueError) as error:
        return _receipt("FAIL", "Invalid matcen comparison evidence", private_error=str(error))
    if not matches:
        return _receipt("FAIL", "Full projected metadata differs from committed baseline",
                        mismatches=metadata_differences(expected_projection, actual_projection))
    return _receipt("PASS", "Projected metadata semantics match committed baseline", missions=len(actual))


def _selected_level(document, target, number, filename):
    missions = _missions(document)
    if missions is None:
        return None
    matches = []
    for mission in missions:
        if mission.get("target_index", 0) != target:
            continue
        levels = mission.get("levels")
        if not isinstance(levels, list):
            continue
        for level in levels:
            if isinstance(level, dict) and level.get("level_num") == number and level.get("level_file") == filename:
                matches.append((mission, level))
    return matches[0] if len(matches) == 1 else None


def compare_route(baseline, generated, summary, relative_metadata, expected_identity=None):
    """Compare only selected normalized simulation leaves, never aggregate success.

    baseline/generated are the matching *.simulation.json documents; summary is
    the route owner's summary.json. relative_metadata must be its exact path
    relative to MissionMetadataRoot. All selected identities must belong to it.
    Parent must use a fresh output directory and omit -WriteRegression.
    """
    if baseline is None:
        return _receipt(
            "BLOCKED", "Committed route oracle is absent", retryable=False, blocker_kind="target-capability"
        )
    if generated is None or not isinstance(summary, dict):
        return _receipt("BLOCKED", "Missing route baseline, generated output or summary")
    if summary.get("schema") != "dxx-guidebot-route-simulation-batch-v1":
        return _receipt("BLOCKED", "Missing or unsupported route summary schema")
    if summary.get("write_regression") is not False:
        return _receipt("FAIL", "Route owner was not explicitly non-publishing")
    if summary.get("mode") != "headless":
        return _receipt("FAIL", "Route owner did not use canonical headless mode")
    if summary.get("infrastructure_failures"):
        return _receipt("FAIL", "Route owner reported infrastructure failures")
    selected = summary.get("selected_work_items")
    if (
        not isinstance(selected, list)
        or len(selected) != 1
        or not all(isinstance(s, str) for s in selected)
        or len(set(selected)) != len(selected)
        or summary.get("selected_levels") != len(selected)
    ):
        return _receipt("BLOCKED", "Missing, duplicated or inconsistent selected route work items")
    checked = []
    for identity in selected:
        if expected_identity is not None and identity != expected_identity:
            return _receipt("FAIL", "Owner executed a different route work item")
        pieces = identity.split("|")
        if len(pieces) != 4 or pieces[0] != relative_metadata:
            return _receipt("BLOCKED", "Selected route identity does not match requested metadata")
        try:
            target, number = int(pieces[1]), int(pieces[2])
        except ValueError:
            return _receipt("BLOCKED", "Malformed selected route identity")
        expected = _selected_level(baseline, target, number, pieces[3])
        actual = _selected_level(generated, target, number, pieces[3])
        if expected is None:
            return _receipt(
                "BLOCKED",
                "Selected committed route oracle is absent or duplicated",
                identity=identity,
                retryable=False,
                blocker_kind="target-capability",
            )
        if actual is None:
            return _receipt("BLOCKED", "Selected generated route leaf is absent or duplicated", identity=identity)
        expected_mission, expected_level = expected
        actual_mission, actual_level = actual
        # A completed known non-ok outcome may match an oracle, but is not confirmed route coverage.
        outcome = actual_level.get("status")
        if outcome not in ("ok", "partial", "failed", "timeout", "unsupported", "route_mismatch"):
            return _receipt(
                "FAIL", "Selected route outcome is not ok", identity=identity, outcome=actual_level.get("status")
            )
        if expected_level.get("status") != outcome:
            return _receipt(
                "FAIL",
                "Runtime route outcome differs from committed baseline",
                identity=identity,
                runtime_outcome=outcome,
            )
        for field in ("schema", "mission_filename", "target_index", "generation", "fixed_hz", "seed"):
            if field not in expected_mission or field not in actual_mission:
                return _receipt("BLOCKED", "Missing canonical route header: " + field)
            if expected_mission[field] != actual_mission[field]:
                return _receipt("FAIL", "Canonical route header differs: " + field, identity=identity)
        for field in ("route_input_sha256", "rng_start", "rng_end", "objectives", "total_frames"):
            if field not in expected_level or field not in actual_level:
                return _receipt("BLOCKED", "Missing canonical route state: " + field, identity=identity)
        header = lambda mission: {k: v for k, v in mission.items() if k not in ("levels", "status", "level_counts")}
        if header(expected_mission) != header(actual_mission):
            return _receipt("FAIL", "Canonical mission header differs", identity=identity)
        if expected_level != actual_level:
            return _receipt("FAIL", "Selected route state differs from committed baseline", identity=identity)
        checked.append(identity)
    # Aggregate status/counts include intentionally unselected not_run leaves.
    return _receipt(
        "PASS",
        "Selected route state matches committed baseline",
        selected=checked,
        runtime_outcome=outcome,
        comparison_only=outcome != "ok",
    )
