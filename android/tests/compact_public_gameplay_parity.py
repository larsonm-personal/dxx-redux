"""Separate gameplay-state/RNG evidence; never upgrades full paired qualification."""

import copy
import itertools
import json
from pathlib import Path

import d1_replay_parity as parity

COMPARED_FIELDS = ["frame.f", "frame.ft", "frame.state", "frame.rng", "terminal_result", "simulation_rng_events"]
NOT_QUALIFIED = ["frame.diag", "object_states", "world_states"]


def compare_traces(header, native, imported, original_checks):
    """Compare unchanged frame gameplay values, retaining excluded evidence status.

    Terminal identity uses the existing canonical_result game/mission naming map.
    SIM event comparison uses the existing rng_events source-label interpretation.
    No frame state, RNG value or object signature is normalized here.
    """
    result = {
        "status": "fail",
        "qualification": "incomplete",
        "compared_fields": list(COMPARED_FIELDS),
        "not_qualified": list(NOT_QUALIFIED),
        "frames_compared": 0,
        "first_difference": None,
        "diagnostic_frames_different": 0,
        "original_comparison_statuses": {key: value.get("status", "missing") for key, value in original_checks.items()},
        "original_frame_first_differences": copy.deepcopy(
            original_checks.get("frames", {}).get("first_differences", {})
        ),
        "checks": {},
    }
    if (
        header.get("game") != "d1"
        or header.get("mission") != "d1"
        or header.get("level") != 1
        or header.get("start_mode") != "new_level"
        or type(header.get("frame_count")) is not int
        or not 0 < header["frame_count"] <= 1000
    ):
        raise parity.EvidenceError("Gameplay scope requires an ordinary public D1 level-one replay")

    def frame_check():
        first = None
        for a, b in itertools.zip_longest(
            parity.frames(native["state"], header, "d1"), parity.frames(imported["state"], header, "d2")
        ):
            if a is None or b is None:
                raise parity.EvidenceError("Missing paired gameplay frame")
            if parity.difference(a["diag"], b["diag"]):
                result["diagnostic_frames_different"] += 1
            for key in ("f", "ft", "state", "rng"):
                difference = parity.difference(a[key], b[key], "$." + key)
                if first is None and difference:
                    first = {"frame": result["frames_compared"], **difference}
            result["frames_compared"] += 1
        if result["frames_compared"] != header["frame_count"]:
            raise parity.EvidenceError("Incomplete paired gameplay frame evidence")
        return {
            "status": "fail" if first else "pass",
            "first_difference": first,
            "frames_compared": result["frames_compared"],
        }

    def terminal_check():
        left = json.loads(Path(native["result"]).read_text())
        right = json.loads(Path(imported["result"]).read_text())
        difference = parity.difference(
            parity.canonical_result(left, header, "d1"), parity.canonical_result(right, header, "d2")
        )
        return {"status": "fail" if difference else "pass", "first_difference": difference}

    for name, check in (
        ("frames", frame_check),
        ("terminal_result", terminal_check),
        ("simulation_rng", lambda: parity.compare_rng(native["rng"], imported["rng"])),
    ):
        try:
            observed = check()
            if name == "simulation_rng" and observed.get("simulation_events_compared", 0) <= 0:
                raise parity.EvidenceError("Missing nonempty paired SIM event evidence")
        except (OSError, ValueError, KeyError, TypeError) as error:
            observed = {"status": "fail", "error": str(error)}
        result["checks"][name] = observed
        if result["first_difference"] is None and observed["status"] != "pass":
            result["first_difference"] = {
                "check": name,
                **(
                    {"difference": observed["first_difference"]}
                    if observed.get("first_difference")
                    else {"error": observed.get("error", "comparison failed")}
                ),
            }
    if all(check["status"] == "pass" for check in result["checks"].values()):
        result["status"] = "pass"
    return result


def compare_case(report_path, case):
    """Read a case from an existing paired report and its verified launch artifacts.

    case is either the exact original case dictionary or its demo basename.
    This function returns separate evidence and never writes or changes a gate.
    """
    report_path = Path(report_path).resolve()
    report = json.loads(report_path.read_text())
    if (
        report.get("schema") != 1
        or report.get("qualification") != "incomplete"
        or "asset_error" in report
        or "harness_error" in report
    ):
        raise parity.EvidenceError("Original paired report is invalid or has integrity failures")
    name = Path(case["demo"] if isinstance(case, dict) else case).name
    matches = [row for row in report.get("cases", []) if Path(row.get("demo", "")).name == name]
    if len(matches) != 1 or (isinstance(case, dict) and case != matches[0]):
        raise parity.EvidenceError("No unique unchanged original paired case")
    original = matches[0]
    if "error" in original or "capture_errors" in original:
        raise parity.EvidenceError("Original paired case has capture errors")
    demo = parity.read_demo(Path(original["demo"]))
    paths = {}
    for engine, directory_name in (("native", "native-a"), ("imported", "imported")):
        directory = report_path.parent / Path(name).stem / directory_name
        launch = json.loads((directory / "launch.json").read_text())
        if (
            type(launch.get("exit_code")) is not int
            or launch["exit_code"] != 0
            or "error" in launch
            or "harness_error" in launch
        ):
            raise parity.EvidenceError("Paired gameplay launch did not complete successfully")
        artifacts = launch.get("artifacts", {})
        if set(artifacts) != {"state", "rng", "result"}:
            raise parity.EvidenceError("Incomplete paired gameplay artifact inventory")
        resolved = {}
        for key, filenames in (
            ("state", {"state.jsonl", "state.jsonl.gz"}),
            ("rng", {"rng.jsonl", "rng.jsonl.gz"}),
            ("result", {"result.json"}),
        ):
            path = Path(artifacts[key]).resolve()
            if path.parent != directory or path.name not in filenames or not path.is_file():
                raise parity.EvidenceError("Paired gameplay artifact escaped its declared capture or is missing")
            resolved[key] = path
        paths[engine] = resolved
    relationship = original.get("native_to_imported", {})
    checks = relationship.get("checks", {})
    required = {
        "frames",
        "terminal_result",
        "simulation_rng",
        "object_states",
        "world_states",
        "checkpoint_collision_clock",
    }
    if set(checks) != required:
        raise parity.EvidenceError("Original paired comparison inventory is incomplete")
    result = compare_traces(demo["header"], paths["native"], paths["imported"], checks)
    result.update(
        original_report=str(report_path),
        demo=original["demo"],
        original_relationship_status=relationship.get("status"),
        artifacts={engine: {key: str(path) for key, path in artifacts.items()} for engine, artifacts in paths.items()},
    )
    return result
