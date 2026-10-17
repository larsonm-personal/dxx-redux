#!/usr/bin/env python3
"""Compare one explicit native authored station probe with a frozen historical trace.

No mission discovery, baseline generation, baseline updates, or full-gameplay claim.
Metadata can be a raw complete capture or an existing mission metadata document.
It proves captured robotmaker joins/model status, not safety of arbitrary level data.
Exit 0: all cases executed and matched; 2: matched but needs skip/cap/empty review;
1: invalid evidence, startup blocked, native failure, or trace mismatch.
"""

import argparse
import copy
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "helpers"))
from compare_matcen_metadata import compare_capture, MatcenMismatch


class EvidenceError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise EvidenceError(message)


def mask_types(words):
    require(isinstance(words, list) and len(words) in (1, 2), "missing or invalid mask words")
    require(all(type(w) is int and -(1 << 31) <= w < (1 << 32) for w in words), "invalid mask word")
    return [i * 32 + bit for i, word in enumerate(words) for bit in range(32) if word & (1 << bit)]


def preflight(metadata, game):
    """Fail closed on missing evidence before invoking a StartNewGame process."""
    require(metadata["schema_version"] == 1 and metadata["capture_status"] == "complete", "incomplete metadata")
    require(
        metadata["admission"] == "loaded" and metadata["scope"] == "fresh_level_load", "metadata was not a loaded level"
    )
    require(metadata["trigger_representation"] == "decoded_disk_before_runtime_linking", "unexpected trigger capture")
    limits, counts = metadata["engine_limits"], metadata["runtime_counts"]
    for key in ("segments", "robot_centers", "stations", "robot_types", "polygon_models"):
        require(type(limits[key]) is int and limits[key] > 0, "missing physical engine limit")
        require(type(counts[key]) is int and 0 <= counts[key] <= limits[key], "unsafe runtime count")
    generated, disk = metadata["generated_count"], metadata["disk_count"]
    require(type(generated) is int and 0 <= generated <= limits["robot_centers"], "unsafe generated count")
    require(
        type(disk) is int and 0 <= disk <= limits["robot_centers"] and metadata["declared_disk_count"] == disk,
        "unsafe disk count",
    )
    authored, bindings = metadata["authored_segments"], metadata["runtime_bindings"]
    require(len(authored) == len(bindings) == generated, "missing binding coverage")
    require([a["generated_ordinal"] for a in authored] == list(range(generated)), "authored ordinal coverage")
    require([b["generated_ordinal"] for b in bindings] == list(range(generated)), "binding ordinal coverage")
    require(len({a["segment"] for a in authored}) == generated, "duplicate authored segment")
    require(len(metadata["disk_records"]) == disk, "missing disk records")
    require([r["ordinal"] for r in metadata["disk_records"]] == list(range(disk)), "disk ordinal coverage")
    for a, b in zip(authored, bindings):
        require(
            type(a["segment"]) is int and 0 <= a["segment"] < counts["segments"] and b["segment"] == a["segment"],
            "unsafe segment join",
        )
        ordinal = b["effective_ordinal"]
        require(type(ordinal) is int and 0 <= ordinal < limits["robot_centers"], "unsafe physical ordinal")
        require(
            b["binding_status"] == ("disk_bound" if ordinal < disk else "logical_disk_unbound"),
            "unsafe or inconsistent binding status",
        )
        r, station = b["effective_record"], b["station"]
        require(b["station_status"] == "matching_station", "unsafe station status")
        require(type(station["index"]) is int and 0 <= station["index"] < counts["stations"], "unsafe station index")
        require(station["type"] == 4 and station["segment"] == b["segment"], "unsafe station segment/type")
        require(station["index"] == b["segment_station"] == r["station"], "unsafe station join")
        require(type(r["segment"]) is int, "invalid redundant record backlink")
        types = mask_types(r["robot_flags"])
        require(all(t < counts["robot_types"] for t in types), "unsafe robot type")
        require(len(r["robot_flags"]) == (1 if game == "d1" else 2), "wrong game mask width")
        robots = b["eligible_robots"]
        require([v["type"] for v in robots] == types, "missing exact robot model coverage")
        require(
            all(
                v["status"] == "valid_model" and type(v["model"]) is int and 0 <= v["model"] < counts["polygon_models"]
                for v in robots
            ),
            "unsafe robot model",
        )
    require(
        [t["index"] for t in metadata["triggers"]] == list(range(len(metadata["triggers"]))),
        "trigger identity coverage",
    )
    return {b["segment"]: b for b in bindings}


def validate_report(report, metadata, game, mission, level, dispatch=True):
    preflight(metadata, game)
    actual = report["baseline"]
    bindings = preflight(actual, game)
    try:
        comparison = compare_capture(metadata, actual)
    except MatcenMismatch as exc:
        raise EvidenceError("load snapshot differs from frozen metadata") from exc
    for key, expected in dict(
        schema=1,
        scope="controlled_authored_trigger_station_loop",
        game=game,
        mission=mission,
        level=level,
        difficulty=2,
        rng_seed=1234,
        frame_time=1092,
        frames=2400,
        original_links_preserved=True,
        passed=True,
    ).items():
        require(report[key] == expected, "incorrect report " + key)
    triggers = [t for t in actual["triggers"] if (t["flags"] & 64 if game == "d1" else t["type"] == 2)]
    cases = report["cases"]
    require([c["trigger"] for c in cases] == [t["index"] for t in triggers], "missing/reordered trigger cases")
    review = not cases or bool(comparison["unbound_requires_review"])
    for case, trigger in zip(cases, triggers):
        require(case["dispatch_requested"] is dispatch, "wrong dispatch control")
        require(case["load_snapshot"] == actual, "case snapshot differs")
        links = case["links"]
        require(
            [{k: x[k] for k in ("ordinal", "segment", "side")} for x in links] == trigger["links"],
            "authored links changed",
        )
        require(len(links) == trigger["declared_link_count"], "incomplete link coverage")
        if case["status"] == "skipped":
            require(
                isinstance(case.get("skip_reasons"), list) and bool(case["skip_reasons"]),
                "skipped without explicit reason",
            )
            known_reasons = {
                "unsupported_mixed_action_trigger",
                "authored_trigger_disabled",
                "invalid_station_count",
                "unsafe_station_loop_segment",
                "unexpected_pre_enabled_station",
                "incomplete_or_overflowed_disk_capture",
                "invalid_or_empty_link_count",
                "invalid_segment",
                "invalid_side",
                "not_robotmaker_segment",
                "physical_out_of_bounds",
                "runtime_count_unbound",
                "logical_disk_unbound",
                "invalid_station",
                "wrong_station_type",
                "station_segment_binding_mismatch",
                "player_obstructs_target",
                "invalid_robot_mask",
                "invalid_robot_model",
            }
            for reason in case["skip_reasons"]:
                if isinstance(reason, str):
                    require(reason in known_reasons, "unknown skip reason")
                else:
                    require(reason["reason"] in known_reasons, "unknown link skip reason")
                    require(type(reason["link"]) is int and 0 <= reason["link"] < len(links), "invalid skipped link")
                    require(links[reason["link"]]["status"] == reason["reason"], "skip reason disagrees with link")
            require(
                not any(k in case for k in ("births", "after_dispatch", "production_outcomes")),
                "skipped case claims execution",
            )
            review = True
            continue
        require(case["status"] == "executed" and not case.get("skip_reasons"), "invalid execution status")
        require(bool(links), "empty links cannot execute")
        centers = {}
        for link in links:
            require(link["status"] == "eligible", "executed ineligible link")
            require(link["segment"] in bindings, "link has no captured binding")
            b = bindings[link["segment"]]
            require(b["binding_status"] == "disk_bound", "logical unbound link executed")
            require(0 <= link["station"] < 128, "station exceeds creator encoding")
            require(link["center"] < actual["runtime_counts"]["robot_centers"], "runtime count unbound link executed")
            require(
                link["center"] == b["effective_ordinal"] and link["station"] == b["station"]["index"],
                "wrong mapped target",
            )
            require(link["nonempty_mask"] is bool(mask_types(b["effective_record"]["robot_flags"])), "wrong mask claim")
            centers.setdefault(link["center"], b)
        for field in ("before_dispatch", "after_dispatch", "after_frames"):
            states = case[field]
            require([s["ordinal"] for s in states] == list(centers), "wrong station state order")
            for state in states:
                b = centers[state["ordinal"]]
                require(
                    state["station"] == b["station"]["index"] and state["segment"] == b["segment"],
                    "wrong state mapping",
                )
                require(
                    state["activation_count"] == (int(dispatch) if field != "before_dispatch" else 0),
                    "wrong activation count",
                )
        for before, after in zip(case["before_dispatch"], case["after_dispatch"]):
            require(after["lives"] == before["lives"] - int(dispatch), "wrong native life consumption")
        require(
            [x["segment"] for x in case["activation_lights"]]
            == ([b["segment"] for b in centers.values()] if dispatch else []),
            "wrong activation light order",
        )
        signatures = []
        for field in ("activation_lights", "births", "cleared_authored_robots"):
            rows = case[field]
            require(
                all(type(row["signature"]) is int and row["signature"] >= 0 for row in rows), "invalid object signature"
            )
            require(len({row["signature"] for row in rows}) == len(rows), "duplicate object signatures")
            if field != "activation_lights":
                require(all(type(row["object"]) is int and row["object"] >= 0 for row in rows), "invalid object slot")
            signatures.extend(row["signature"] for row in rows)
        require(len(set(signatures)) == len(signatures), "object signatures overlap across phases")
        births = case["births"]
        require(dispatch or not births, "no-dispatch produced births")
        require([b["spawn_ordinal"] for b in births] == list(range(len(births))), "wrong birth order")
        last_frame = -1
        for birth in births:
            require(type(birth["frame"]) is int and last_frame <= birth["frame"] < 2400, "invalid birth frame")
            last_frame = birth["frame"]
            require(birth["ordinal"] in centers, "birth from unrequested center")
            b = centers[birth["ordinal"]]
            require(
                birth["station"] == b["station"]["index"] and birth["segment"] == b["segment"], "wrong birth origin"
            )
            require(birth["type"] in mask_types(b["effective_record"]["robot_flags"]), "birth outside mapped mask")
        outcomes = case["production_outcomes"]
        require([o["ordinal"] for o in outcomes] == list(centers), "missing production outcomes")
        for outcome in outcomes:
            count = sum(b["ordinal"] == outcome["ordinal"] for b in births)
            nonempty = bool(mask_types(centers[outcome["ordinal"]]["effective_record"]["robot_flags"]))
            status = (
                "no_dispatch_control"
                if not dispatch
                else "empty_mask"
                if not nonempty
                else "produced"
                if count
                else "native_global_cap"
            )
            require(outcome["births"] == count and outcome["status"] == status, "wrong production outcome")
            require(not count or (dispatch and nonempty), "empty mask produced birth")
            require(
                status != "native_global_cap" or case["global_cap_observed"] is True,
                "nonempty mask produced no births without native cap",
            )
            review = review or (dispatch and status in ("empty_mask", "native_global_cap"))
    return "review-required" if review else "passed"


def normalize_trace(report):
    """Drop duplicate metadata and incidental object/signature allocation identities.

    Array order, frames, types, masks, origins, shields and all station state remain.
    Report validation still checks allocation identity integrity.
    """
    trace = copy.deepcopy(report)
    trace.pop("baseline", None)
    for case in trace["cases"]:
        case.pop("load_snapshot", None)
        for field in ("births", "activation_lights", "cleared_authored_robots"):
            for row in case.get(field, []):
                row.pop("object", None)
                row.pop("signature", None)
    return trace


def expand_expected(expected, metadata):
    report = copy.deepcopy(expected)
    report.setdefault("baseline", metadata)
    for case in report["cases"]:
        case.setdefault("load_snapshot", metadata)
        signature = 0
        for field in ("activation_lights", "births", "cleared_authored_robots"):
            for row in case.get(field, []):
                row.setdefault("signature", signature)
                if field != "activation_lights":
                    row.setdefault("object", 0)
                signature += 1
    return report


def compare_reports(first, repeat, control, expected, metadata, game, mission, level):
    status = validate_report(first, metadata, game, mission, level)
    validate_report(repeat, metadata, game, mission, level)
    validate_report(control, metadata, game, mission, level, False)
    validate_report(expand_expected(expected, metadata), metadata, game, mission, level)
    require(first == repeat, "non-deterministic repeated trace")
    require(normalize_trace(first) == normalize_trace(expected), "trace differs from frozen historical expectation")
    require(
        [(c["trigger"], c["status"], c["links"], c.get("skip_reasons")) for c in first["cases"]]
        == [(c["trigger"], c["status"], c["links"], c.get("skip_reasons")) for c in control["cases"]],
        "control changed case eligibility",
    )
    return status


def select_metadata(document, game, mission, level):
    """Select exactly one existing native mission/level; never silently choose a match."""
    if isinstance(document, dict) and "schema_version" in document:
        return document
    missions = document if isinstance(document, list) else [document]
    matches = []
    for entry in missions:
        name = entry.get("mission_filename", "").casefold()
        if name.endswith((".msn", ".mn2")):
            name = name.rsplit(".", 1)[0]
        if entry.get("game") == game and name == mission.casefold():
            matches.extend(row for row in entry.get("levels", []) if row.get("level_num") == level)
    require(len(matches) == 1, "missing or ambiguous mission/level metadata")
    require("matcen_regression" in matches[0], "selected level lacks frozen capture")
    return matches[0]["matcen_regression"]


def verify_input_tree(root, files):
    """Prove exact file membership and bytes without publishing local paths."""
    require(isinstance(files, dict), "missing input file manifest")
    require(root.is_dir() and not root.is_symlink(), "input root is missing or symlinked")
    for name, digest in files.items():
        require(isinstance(name, str) and bool(name) and "\\" not in name and ":" not in name, "invalid manifest path")
        path = PurePosixPath(name)
        require(
            not path.is_absolute() and ".." not in path.parts and path.as_posix() == name and name != ".",
            "unsafe manifest path",
        )
        require(
            isinstance(digest, str) and len(digest) == 64 and all(c in "0123456789abcdef" for c in digest),
            "invalid manifest hash",
        )
    observed = {}
    for path in root.rglob("*"):
        require(not path.is_symlink(), "symlink in input tree")
        if path.is_dir():
            continue
        require(path.is_file(), "non-regular input file")
        name = path.relative_to(root).as_posix()
        require(name in files, "unexpected input file")
        digest = hashlib.sha256()
        with path.open("rb") as source:
            for block in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(block)
        observed[name] = digest.hexdigest()
    require(observed == files, "missing or changed input bytes")


def verify_inputs(manifest, game, mission, level, base, extra):
    require(
        (manifest["game"], manifest["mission"], manifest["level"]) == (game, mission, level),
        "input manifest identity mismatch",
    )
    verify_input_tree(base, manifest["base_files"])
    verify_input_tree(extra, manifest["extra_files"])


def run(args):
    metadata = select_metadata(json.loads(args.metadata.read_text()), args.game, args.mission, args.level)
    expected = json.loads(args.expected.read_text())
    inputs = json.loads(args.inputs.read_text())
    # Complete both checks before any native process can reach StartNewGame.
    preflight(metadata, args.game)
    validate_report(expand_expected(expected, metadata), metadata, args.game, args.mission, args.level)
    output = args.output_dir.resolve()
    require(
        not any(
            path.resolve() == output or output in path.resolve().parents
            for path in (args.metadata, args.expected, args.inputs)
        ),
        "output overlaps frozen evidence",
    )
    require(
        not any(output == path.resolve() or path.resolve() in output.parents for path in (args.base, args.extra)),
        "output overlaps input resources",
    )
    require(args.timeout > 0, "timeout must be positive")
    verify_inputs(inputs, args.game, args.mission, args.level, args.base, args.extra)
    output.mkdir(parents=True, exist_ok=True)
    runs = []
    for label in ("first", "repeat", "no-dispatch"):
        home = output / label
        home.mkdir()  # Refuse stale captures and accidental baseline overwrites.
        env = dict(os.environ, HOME=str(home), SDL_VIDEODRIVER="invalid", SDL_AUDIODRIVER="dummy")
        env.pop("DISPLAY", None)
        env.pop("WAYLAND_DISPLAY", None)
        cmd = [
            str(args.executable.resolve()),
            "--authored-matcen-stations",
            str(args.base.resolve()),
            str(args.extra.resolve()),
            args.mission,
            str(args.level),
            "capture.json",
        ]
        if label == "no-dispatch":
            cmd.append("--no-dispatch")
        # Local diagnostics may contain private paths; do not publish native.log.
        verify_inputs(inputs, args.game, args.mission, args.level, args.base, args.extra)
        with (home / "native.log").open("w") as log:
            result = subprocess.run(
                cmd, cwd=home, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=args.timeout, check=False
            )
        require(result.returncode == 0, "native fixture failed: " + label)
        runs.append(json.loads((home / "capture.json").read_text()))
    status = compare_reports(*runs, expected, metadata, args.game, args.mission, args.level)
    summary = {
        "status": status,
        "cases": len(runs[0]["cases"]),
        "executed": sum(c["status"] == "executed" for c in runs[0]["cases"]),
        "skipped": sum(c["status"] == "skipped" for c in runs[0]["cases"]),
    }
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary))
    return 0 if status == "passed" else 2


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for flag in ("executable", "base", "extra", "metadata", "expected", "inputs", "output-dir"):
        parser.add_argument("--" + flag, required=True, type=Path)
    parser.add_argument("--game", choices=("d1", "d2"), required=True)
    parser.add_argument("--mission", required=True)
    parser.add_argument("--level", required=True, type=int)
    parser.add_argument("--timeout", type=float, default=60)
    args = parser.parse_args(argv)
    try:
        return run(args)
    except (EvidenceError, OSError, KeyError, TypeError, ValueError, subprocess.TimeoutExpired):
        # Do not echo exceptions that can embed private paths or resource contents.
        print(
            json.dumps(
                {"status": "failed", "reason": "invalid evidence, unsafe startup, native failure, or trace mismatch"}
            )
        )
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
