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


def validate_lifecycle_report(report, metadata, game, mission, level, difficulty, dispatch=True):
    """Validate the opt-in native lifecycle report without manufacturing an oracle.

    Internal captures remain schema 1 here. Imported D1 source action masks are
    absent from that capture, so their completeness cannot be certified here.
    A structurally valid cap-only or empty-mask report still requires review.
    """
    preflight(metadata, game)
    actual = report["baseline"]
    bindings = preflight(actual, game)
    try:
        comparison = compare_capture(metadata, actual)
    except MatcenMismatch as exc:
        raise EvidenceError("lifecycle load differs from frozen metadata") from exc
    require(type(difficulty) is int and 0 <= difficulty <= 4, "invalid lifecycle difficulty")
    for key, expected in dict(
        schema=2,
        scope="controlled_authored_trigger_station_lifecycle",
        game=game,
        mission=mission,
        level=level,
        difficulty=difficulty,
        rng_seed=1234,
        frame_time=1092,
        frames_per_attempt=2400,
        attempts=4,
        dispatch_requested=dispatch,
        observed_runtime_links_preserved=True,
        disk_original_links_verified=False,
        comparison_performed=False,
    ).items():
        require(report[key] == expected, "incorrect lifecycle report " + key)
    require(
        game != "d2" or not any(t["flags"] & 128 for t in actual["triggers"]),
        "imported D1 capture lacks source action masks for complete lifecycle validation",
    )
    triggers = [t for t in actual["triggers"] if (t["flags"] & 64 if game == "d1" else t["type"] == 2)]
    cases = report["cases"]
    require(
        [c["trigger"] for c in cases] == [t["index"] for t in triggers],
        "missing/reordered lifecycle cases",
    )
    executed = total_births = 0
    review = not cases or bool(comparison["unbound_requires_review"])

    def check_vector(value):
        require(
            isinstance(value, list) and len(value) == 3 and all(type(n) is int for n in value),
            "missing or invalid lifecycle vector",
        )

    for case, trigger in zip(cases, triggers):
        require(
            case["dispatch_requested"] is dispatch and case["load_snapshot"] == actual,
            "lifecycle case control or fresh load changed",
        )
        links = case["links"]
        targets = [{k: link[k] for k in ("ordinal", "segment", "side")} for link in links]
        require(
            targets == trigger["links"] and len(links) == trigger["declared_link_count"],
            "lifecycle targets changed, reordered, or omitted duplicates",
        )
        require(
            case["runtime_type"] == trigger["type"] and case["runtime_flags"] == trigger["flags"],
            "lifecycle runtime trigger changed",
        )
        if case["status"] == "skipped":
            require(
                isinstance(case.get("skip_reasons"), list) and bool(case["skip_reasons"]),
                "lifecycle skip lacks reasons",
            )
            require(
                not any(k in case for k in ("rounds", "initial", "birth_count")),
                "skipped lifecycle claims execution",
            )
            review = True
            continue
        require(
            case["status"] == "executed" and not case.get("skip_reasons") and bool(links),
            "invalid lifecycle execution",
        )
        repeatable = not (trigger["flags"] & (32 if game == "d1" else 6))
        if game == "d1":
            repeatable = repeatable and not (trigger["flags"] & ~(64 | 16 | 32))
        require(
            repeatable,
            "one-shot, disabled, or mixed trigger executed in repeatable lifecycle",
        )
        centers = {}
        for link in links:
            require(
                link["status"] == "eligible" and link["segment"] in bindings,
                "unsafe lifecycle target",
            )
            binding = bindings[link["segment"]]
            require(
                binding["binding_status"] == "disk_bound"
                and 0 <= link["center"] < actual["runtime_counts"]["robot_centers"]
                and 0 <= link["station"] < 128
                and link["center"] == binding["effective_ordinal"]
                and link["station"] == binding["station"]["index"],
                "wrong lifecycle target mapping",
            )
            require(
                link["nonempty_mask"] is bool(mask_types(binding["effective_record"]["robot_flags"])),
                "wrong lifecycle mask claim",
            )
            centers.setdefault(link["center"], binding)
        unlimited = game == "d2" and difficulty == 4
        require(
            case["unlimited_native_lives"] is unlimited,
            "wrong highest-difficulty life rule",
        )
        initial = case["initial"]
        expected_trigger = dict(
            type=trigger["type"],
            flags=trigger["flags"],
            num_links=len(targets),
            links=targets,
        )

        def check_snapshot(value, activations, enabled, frame, births):
            player = value["player"]
            require(
                all(type(player.get(k)) is int for k in ("number", "object", "segment")),
                "missing or invalid lifecycle player identity",
            )
            for key in ("position", "forward", "right", "up"):
                check_vector(player.get(key))
            require(
                all(type(value.get(k)) is int for k in ("game_time", "robot_total", "robot_kills", "robot_limit")),
                "missing or invalid lifecycle counter",
            )
            require(
                value["trigger"] == expected_trigger and value["player"] == initial["player"],
                "lifecycle trigger or player changed",
            )
            require(
                value["game_time"] == frame * 1092
                and value["robot_total"] == initial["robot_total"] + births
                and value["robot_kills"] == initial["robot_kills"]
                and value["robot_limit"] == initial["robot_limit"],
                "lifecycle time, cap, or kill counters changed",
            )
            require(
                all(type(value[k]) is int and value[k] >= 0 for k in ("sim_state", "sim_calls")),
                "invalid lifecycle RNG observation",
            )
            require(
                [s["ordinal"] for s in value["centers"]] == list(centers),
                "wrong lifecycle state order",
            )
            for state in value["centers"]:
                binding = centers[state["ordinal"]]
                require(
                    state["station"] == binding["station"]["index"] and state["segment"] == binding["segment"],
                    "wrong lifecycle state mapping",
                )
                require(
                    all(
                        type(state[k]) is int
                        for k in (
                            "enabled",
                            "lives",
                            "capacity",
                            "timer",
                            "flag",
                            "disable_time",
                            "activation_count",
                        )
                    ),
                    "invalid lifecycle station state",
                )
                require(
                    state["enabled"] == enabled
                    and state["activation_count"] == activations
                    and state["lives"] == 3 - (0 if unlimited else activations),
                    "wrong lifecycle native life/activation state",
                )

        require(
            initial["sim_calls"] == 0 and initial["sim_state"] == 1234,
            "lifecycle was not seeded once at start",
        )
        check_snapshot(initial, 0, 0, 0, 0)
        rounds = case["rounds"]
        require(
            [r["attempt"] for r in rounds] == [1, 2, 3, 4],
            "missing/reordered lifecycle attempts",
        )
        observed_births = 0
        previous = initial
        signatures = set()

        def check_objects(rows):
            for row in rows:
                require(
                    type(row["signature"]) is int and row["signature"] >= 0 and row["signature"] not in signatures,
                    "duplicate or invalid lifecycle signature",
                )
                require(
                    type(row["object"]) is int and row["object"] >= 0,
                    "invalid lifecycle object slot",
                )
                signatures.add(row["signature"])

        check_objects(case["cleared_authored_robots"])
        for attempt, round_ in enumerate(rounds, 1):
            offset = (attempt - 1) * 2400
            activated = dispatch and (unlimited or attempt <= 3)
            count_before = (attempt - 1 if unlimited else min(attempt - 1, 3)) if dispatch else 0
            count_after = count_before + int(activated)
            before, after, end = (round_[key] for key in ("before_dispatch", "after_dispatch", "after_frames"))
            require(
                round_["frame_offset"] == offset and before == previous,
                "lifecycle sequence reset or gap",
            )
            check_snapshot(before, count_before, 0, offset, observed_births)
            check_snapshot(after, count_after, int(activated), offset, observed_births)
            require(
                after["sim_calls"] == before["sim_calls"] and after["sim_state"] == before["sim_state"],
                "lifecycle dispatch changed RNG",
            )
            require(
                round_["dispatch_result"] == (0 if dispatch else -1),
                "wrong lifecycle dispatch result",
            )
            if attempt == 1:
                require(
                    round_["after_active_retrigger"] == after
                    and round_["active_retrigger_result"] == (0 if dispatch else -1),
                    "active retrigger changed state",
                )
            lights, births = round_["activation_lights"], round_["births"]
            require(
                [light["segment"] for light in lights]
                == ([b["segment"] for b in centers.values()] if activated else []),
                "wrong lifecycle activation lights",
            )
            check_objects(lights)
            check_objects(births)
            for light in lights:
                require(type(light.get("segment")) is int, "missing or invalid lifecycle light segment")
                check_vector(light.get("position"))
            require(
                [b["spawn_ordinal"] for b in births] == list(range(observed_births, observed_births + len(births))),
                "wrong lifecycle birth order",
            )
            last_frame = -1
            for birth in births:
                require(
                    all(
                        type(birth.get(k)) is int
                        for k in (
                            "frame",
                            "absolute_frame",
                            "spawn_ordinal",
                            "ordinal",
                            "station",
                            "segment",
                            "type",
                            "shields",
                        )
                    ),
                    "missing or invalid lifecycle birth fields",
                )
                require(
                    activated
                    and type(birth["frame"]) is int
                    and 0 <= birth["frame"]
                    and last_frame <= birth["frame"] < 2400
                    and birth["absolute_frame"] == offset + birth["frame"],
                    "invalid lifecycle birth frame",
                )
                last_frame = birth["frame"]
                require(
                    birth["ordinal"] in centers,
                    "lifecycle birth from unrequested center",
                )
                binding = centers[birth["ordinal"]]
                require(
                    birth["station"] == binding["station"]["index"] and birth["segment"] == binding["segment"],
                    "wrong lifecycle birth origin",
                )
                require(
                    birth["type"] in mask_types(binding["effective_record"]["robot_flags"]),
                    "lifecycle birth outside mapped mask",
                )
                check_vector(birth.get("position"))
            observed_births += len(births)
            check_snapshot(end, count_after, 0, offset + 2400, observed_births)
            require(end["sim_calls"] >= after["sim_calls"], "lifecycle RNG counter reset")
            if not activated:
                require(
                    end["sim_calls"] == after["sim_calls"] and end["sim_state"] == after["sim_state"],
                    "inactive lifecycle consumed RNG",
                )
            cap_frame = round_["first_cap_frame"]
            require(
                type(cap_frame) is int and -1 <= cap_frame < 2400 and round_["global_cap_observed"] is (cap_frame >= 0),
                "invalid lifecycle cap timing",
            )
            require(
                round_["first_cap_phase"]
                in (("before_station_update", "after_station_update") if cap_frame >= 0 else ("not_observed",)),
                "invalid lifecycle cap phase",
            )
            require(
                (end["robot_total"] - end["robot_kills"] >= end["robot_limit"]) == (cap_frame >= 0),
                "lifecycle cap claim disagrees with native counters",
            )
            disabled = round_["first_all_disabled_frame"]
            require(
                type(disabled) is int and (0 <= disabled < 2400 if activated else disabled == -1),
                "lifecycle did not complete naturally",
            )
            outcomes = round_["production_outcomes"]
            require(
                [o["ordinal"] for o in outcomes] == list(centers),
                "missing lifecycle outcomes",
            )
            for outcome in outcomes:
                produced = sum(b["ordinal"] == outcome["ordinal"] for b in births)
                nonempty = bool(mask_types(centers[outcome["ordinal"]]["effective_record"]["robot_flags"]))
                status = (
                    "no_dispatch_control"
                    if not dispatch
                    else "native_lives_exhausted"
                    if not activated
                    else "empty_mask"
                    if not nonempty
                    else "produced"
                    if produced
                    else "inconclusive_zero_births_with_global_cap"
                )
                require(
                    outcome["births"] == produced and outcome["status"] == status,
                    "wrong lifecycle outcome",
                )
                require(not produced or nonempty, "empty lifecycle mask produced births")
                require(
                    status != "inconclusive_zero_births_with_global_cap" or cap_frame >= 0,
                    "missing lifecycle births without cap evidence",
                )
                review = review or status in (
                    "empty_mask",
                    "inconclusive_zero_births_with_global_cap",
                )
            previous = end
        require(case["birth_count"] == observed_births, "wrong lifecycle birth count")
        executed += 1
        total_births += observed_births
    require(report["passed"] is bool(executed), "empty lifecycle coverage claimed a pass")
    require(
        report["execution_summary"]
        == dict(
            matcen_triggers=len(cases),
            executed=executed,
            skipped=len(cases) - executed,
            births=total_births,
            status="no_matcen_triggers"
            if not cases
            else "all_cases_skipped"
            if not executed
            else "native_invariants_checked",
        ),
        "wrong lifecycle execution summary",
    )
    return "review-required" if review or not executed or (dispatch and not total_births) else "passed"


def normalize_lifecycle_trace(report):
    """Semantic cross-engine comparison only; same-engine repeats stay exact.

    Exclude repeated load metadata and explicitly incidental allocation identities.
    Preserve ordered/duplicate targets, all states, masks via metadata comparison,
    RNG, counters, positions, frames, types, birth order, and outcome classifications.
    """
    trace = copy.deepcopy(report)
    trace.pop("baseline", None)
    for case in trace["cases"]:
        case.pop("load_snapshot", None)
        for row in case.get("cleared_authored_robots", []):
            row.pop("object", None)
            row.pop("signature", None)
        snapshots = [case["initial"]] if "initial" in case else []
        for round_ in case.get("rounds", []):
            snapshots.extend(
                round_[key]
                for key in (
                    "before_dispatch",
                    "after_dispatch",
                    "after_active_retrigger",
                    "after_frames",
                )
                if key in round_
            )
            for key in ("activation_lights", "births"):
                for row in round_[key]:
                    row.pop("object", None)
                    row.pop("signature", None)
        for snapshot in snapshots:
            snapshot["player"].pop("object", None)
    return trace


def compare_lifecycle_reports(
    historical,
    historical_repeat,
    current,
    current_repeat,
    historical_control,
    current_control,
    metadata,
    game,
    mission,
    level,
    difficulty,
):
    """Compare independent executions; never generate or update expected traces."""
    statuses = [
        validate_lifecycle_report(report, metadata, game, mission, level, difficulty, dispatch)
        for report, dispatch in (
            (historical, True),
            (historical_repeat, True),
            (current, True),
            (current_repeat, True),
            (historical_control, False),
            (current_control, False),
        )
    ]
    require(
        historical == historical_repeat and current == current_repeat,
        "non-deterministic lifecycle repeat",
    )
    require(
        all(
            report["execution_summary"]["executed"] > 0
            for report in (historical, current, historical_control, current_control)
        ),
        "no executed lifecycle comparison",
    )
    require(
        historical["execution_summary"]["births"] > 0 and current["execution_summary"]["births"] > 0,
        "no births in active lifecycle comparison",
    )
    require(
        normalize_lifecycle_trace(historical) == normalize_lifecycle_trace(current),
        "lifecycle trace differs from historical execution",
    )
    require(
        normalize_lifecycle_trace(historical_control) == normalize_lifecycle_trace(current_control),
        "lifecycle no-dispatch traces differ",
    )
    for active, control in (
        (historical, historical_control),
        (current, current_control),
    ):
        fields = (
            "trigger",
            "status",
            "links",
            "skip_reasons",
            "initial",
            "cleared_authored_robots",
        )
        require(
            [{k: c.get(k) for k in fields} for c in active["cases"]]
            == [{k: c.get(k) for k in fields} for c in control["cases"]],
            "lifecycle control changed setup or eligibility",
        )
    return "review-required" if "review-required" in statuses else "passed"


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
