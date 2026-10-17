#!/usr/bin/env python3
"""Source-only negative controls; no private mission assets or native executable."""

import argparse
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import run_authored_matcen_stations as runner


def evidence(dispatch=True):
    record = {"segment": 8, "station": 2, "robot_flags": [2], "hit_points": 1, "interval": 1}
    metadata = {
        "schema_version": 1,
        "capture_status": "complete",
        "admission": "loaded",
        "scope": "fresh_level_load",
        "trigger_representation": "decoded_disk_before_runtime_linking",
        "generated_count": 1,
        "disk_count": 1,
        "declared_disk_count": 1,
        "engine_limits": {
            "segments": 9000,
            "robot_centers": 20,
            "stations": 140,
            "robot_types": 30,
            "polygon_models": 500,
        },
        "runtime_counts": {"segments": 10, "robot_centers": 1, "stations": 3, "robot_types": 3, "polygon_models": 4},
        "authored_segments": [{"segment": 8, "generated_ordinal": 0}],
        "disk_records": [dict(record, ordinal=0)],
        "runtime_bindings": [
            {
                "segment": 8,
                "generated_ordinal": 0,
                "effective_ordinal": 0,
                "binding_status": "disk_bound",
                "station_status": "matching_station",
                "segment_station": 2,
                "station": {"index": 2, "segment": 8, "type": 4},
                "effective_record": record,
                "eligible_robots": [{"type": 1, "model": 2, "status": "valid_model"}],
            }
        ],
        "triggers": [
            {
                "index": 0,
                "type": 0,
                "flags": 64,
                "declared_link_count": 1,
                "links": [{"ordinal": 0, "segment": 8, "side": 1}],
            }
        ],
    }
    before = {"ordinal": 0, "station": 2, "segment": 8, "activation_count": 0, "lives": 3}
    after = dict(before, activation_count=int(dispatch), lives=3 - int(dispatch))
    case = {
        "trigger": 0,
        "runtime_type": 0,
        "runtime_flags": 64,
        "status": "executed",
        "dispatch_requested": dispatch,
        "load_snapshot": copy.deepcopy(metadata),
        "links": [
            {
                "ordinal": 0,
                "segment": 8,
                "side": 1,
                "center": 0,
                "station": 2,
                "nonempty_mask": True,
                "status": "eligible",
            }
        ],
        "before_dispatch": [before],
        "after_dispatch": [after],
        "after_frames": [copy.deepcopy(after)],
        "activation_lights": [{"segment": 8, "signature": 10}] if dispatch else [],
        "births": [
            {
                "frame": 40,
                "spawn_ordinal": 0,
                "ordinal": 0,
                "station": 2,
                "segment": 8,
                "type": 1,
                "signature": 11,
                "object": 3,
                "shields": 10,
            }
        ]
        if dispatch
        else [],
        "production_outcomes": [
            {"ordinal": 0, "births": int(dispatch), "status": "produced" if dispatch else "no_dispatch_control"}
        ],
        "global_cap_observed": False,
        "cleared_authored_robots": [],
    }
    report = {
        "schema": 1,
        "scope": "controlled_authored_trigger_station_loop",
        "game": "d1",
        "mission": "synthetic",
        "level": 1,
        "difficulty": 2,
        "rng_seed": 1234,
        "frame_time": 1092,
        "frames": 2400,
        "original_links_preserved": True,
        "passed": True,
        "baseline": metadata,
        "cases": [case],
    }
    return metadata, report


class AuthoredMatcenReportTests(unittest.TestCase):
    def setUp(self):
        self.metadata, self.report = evidence()

    def validate(self):
        return runner.validate_report(self.report, self.metadata, "d1", "synthetic", 1)

    def test_positive_and_compact_historical_comparison(self):
        expected = runner.normalize_trace(self.report)
        self.assertNotIn("baseline", expected)
        self.assertNotIn("load_snapshot", expected["cases"][0])
        self.assertEqual(
            "passed",
            runner.compare_reports(
                self.report,
                copy.deepcopy(self.report),
                evidence(False)[1],
                expected,
                self.metadata,
                "d1",
                "synthetic",
                1,
            ),
        )

    def test_no_spawn_cannot_be_its_own_passing_baseline(self):
        case = self.report["cases"][0]
        case["births"] = []
        case["production_outcomes"][0].update(births=0, status="native_global_cap")
        with self.assertRaisesRegex(runner.EvidenceError, "no births"):
            self.validate()

    def test_documented_native_cap_requires_review(self):
        case = self.report["cases"][0]
        case["births"] = []
        case["production_outcomes"][0].update(births=0, status="native_global_cap")
        case["global_cap_observed"] = True
        self.assertEqual("review-required", self.validate())

    def test_wrong_mapping_rejected(self):
        self.report["cases"][0]["births"][0]["station"] = 3
        with self.assertRaisesRegex(runner.EvidenceError, "origin"):
            self.validate()

    def test_wrong_robot_mask_rejected(self):
        self.report["cases"][0]["births"][0]["type"] = 2
        with self.assertRaisesRegex(runner.EvidenceError, "mapped mask"):
            self.validate()

    def test_invalid_frame_rejected(self):
        self.report["cases"][0]["births"][0]["frame"] = 2400
        with self.assertRaisesRegex(runner.EvidenceError, "frame"):
            self.validate()

    def test_missing_case_rejected(self):
        self.report["cases"] = []
        with self.assertRaisesRegex(runner.EvidenceError, "trigger cases"):
            self.validate()

    def test_changed_link_order_rejected(self):
        self.report["cases"][0]["links"][0]["ordinal"] = 1
        with self.assertRaisesRegex(runner.EvidenceError, "links changed"):
            self.validate()

    def test_skipped_execution_misclaim_rejected(self):
        case = self.report["cases"][0]
        case.update(status="skipped", skip_reasons=["authored_trigger_disabled"])
        with self.assertRaisesRegex(runner.EvidenceError, "claims execution"):
            self.validate()

    def test_skip_requires_reason_and_review(self):
        case = self.report["cases"][0]
        for k in ("births", "after_dispatch", "production_outcomes"):
            del case[k]
        case["status"] = "skipped"
        with self.assertRaisesRegex(runner.EvidenceError, "explicit reason"):
            self.validate()
        case["skip_reasons"] = ["authored_trigger_disabled"]
        self.assertEqual("review-required", self.validate())
        case["skip_reasons"] = ["arbitrary_excuse"]
        with self.assertRaisesRegex(runner.EvidenceError, "unknown skip"):
            self.validate()

    def test_no_trigger_links_needs_review(self):
        self.metadata["triggers"] = []
        self.report["cases"] = []
        self.assertEqual("review-required", self.validate())

    def test_missing_mask_model_coverage_blocks(self):
        self.metadata["runtime_bindings"][0]["eligible_robots"] = []
        with self.assertRaisesRegex(runner.EvidenceError, "model coverage"):
            runner.preflight(self.metadata, "d1")

    def test_missing_bindings_blocks(self):
        self.metadata["runtime_bindings"] = []
        with self.assertRaisesRegex(runner.EvidenceError, "coverage"):
            runner.preflight(self.metadata, "d1")

    def test_wrong_station_join_blocks(self):
        self.metadata["runtime_bindings"][0]["station"]["segment"] = 9
        with self.assertRaisesRegex(runner.EvidenceError, "station segment"):
            runner.preflight(self.metadata, "d1")

    def test_logical_unbound_physically_safe_can_preflight(self):
        self.metadata.update(disk_count=0, declared_disk_count=0, disk_records=[])
        self.metadata["runtime_bindings"][0]["binding_status"] = "logical_disk_unbound"
        runner.preflight(self.metadata, "d1")
        self.report["cases"][0]["load_snapshot"] = copy.deepcopy(self.metadata)
        with self.assertRaisesRegex(runner.EvidenceError, "logical unbound"):
            self.validate()

    def test_historical_difference_rejected_even_when_repeat_matches(self):
        expected = copy.deepcopy(self.report)
        expected["cases"][0]["births"][0]["frame"] += 1
        with self.assertRaisesRegex(runner.EvidenceError, "historical"):
            runner.compare_reports(
                self.report, self.report, evidence(False)[1], expected, self.metadata, "d1", "synthetic", 1
            )

    def test_no_dispatch_birth_rejected(self):
        control = evidence(False)[1]
        control["cases"][0]["births"] = copy.deepcopy(self.report["cases"][0]["births"])
        with self.assertRaisesRegex(runner.EvidenceError, "no-dispatch"):
            runner.validate_report(control, self.metadata, "d1", "synthetic", 1, False)

    def test_missing_metadata_never_starts_native(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(runner.subprocess, "run") as native:
            root = Path(directory)
            args = argparse.Namespace(
                metadata=root / "missing.json",
                expected=root / "expected.json",
                game="d1",
                mission="synthetic",
                level=1,
                inputs=root / "inputs.json",
            )
            with self.assertRaises(OSError):
                runner.run(args)
            native.assert_not_called()

    def test_incomplete_metadata_never_starts_native(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(runner.subprocess, "run") as native:
            root = Path(directory)
            self.metadata["capture_status"] = "overflow"
            (root / "metadata.json").write_text(json.dumps(self.metadata))
            (root / "expected.json").write_text(json.dumps(self.report))
            (root / "inputs.json").write_text("{}")
            args = argparse.Namespace(
                metadata=root / "metadata.json",
                expected=root / "expected.json",
                game="d1",
                mission="synthetic",
                level=1,
                inputs=root / "inputs.json",
            )
            with self.assertRaises(runner.EvidenceError):
                runner.run(args)
            native.assert_not_called()

    def test_physical_out_of_bounds_blocks(self):
        self.metadata["runtime_bindings"][0]["effective_ordinal"] = 20
        with self.assertRaisesRegex(runner.EvidenceError, "physical ordinal"):
            runner.preflight(self.metadata, "d1")

    def test_runner_keeps_inputs_and_uses_three_isolated_homes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            metadata_path, expected_path = root / "metadata.json", root / "expected.json"
            metadata_path.write_text(json.dumps(self.metadata))
            expected_path.write_text(json.dumps(runner.normalize_trace(self.report)))
            original = (metadata_path.read_bytes(), expected_path.read_bytes())
            (root / "base").mkdir()
            (root / "extra").mkdir()
            (root / "inputs.json").write_text(
                json.dumps({"game": "d1", "mission": "synthetic", "level": 1, "base_files": {}, "extra_files": {}})
            )
            args = argparse.Namespace(
                metadata=metadata_path,
                expected=expected_path,
                game="d1",
                mission="synthetic",
                level=1,
                output_dir=root / "output",
                executable=root / "native",
                base=root / "base",
                extra=root / "extra",
                timeout=60,
                inputs=root / "inputs.json",
            )
            commands = []

            def execute(cmd, **kwargs):
                commands.append((cmd, kwargs))
                report = evidence("--no-dispatch" not in cmd)[1]
                (kwargs["cwd"] / "capture.json").write_text(json.dumps(report))
                return argparse.Namespace(returncode=0)

            with patch.object(runner.subprocess, "run", side_effect=execute), patch("builtins.print"):
                self.assertEqual(0, runner.run(args))
            self.assertEqual(3, len(commands))
            self.assertEqual(["first", "repeat", "no-dispatch"], [k["cwd"].name for _, k in commands])
            self.assertEqual([False, False, True], ["--no-dispatch" in c for c, _ in commands])
            self.assertEqual(original, (metadata_path.read_bytes(), expected_path.read_bytes()))
            self.assertNotIn(str(root), (args.output_dir / "summary.json").read_text())
            args.output_dir = root
            with patch.object(runner.subprocess, "run") as native:
                with self.assertRaisesRegex(runner.EvidenceError, "overlaps"):
                    runner.run(args)
                native.assert_not_called()

    def test_mission_document_selection_is_unique(self):
        document = [
            {
                "game": "d1",
                "mission_filename": "SYNTHETIC.MSN",
                "levels": [{"level_num": 1, "level_file": "level.rdl", "matcen_regression": self.metadata}],
            }
        ]
        self.assertEqual(self.metadata, runner.select_metadata(document, "d1", "synthetic", 1))
        with self.assertRaisesRegex(runner.EvidenceError, "missing or ambiguous"):
            runner.select_metadata(document, "d2", "synthetic", 1)
        with self.assertRaisesRegex(runner.EvidenceError, "missing or ambiguous"):
            runner.select_metadata(document * 2, "d1", "synthetic", 1)
        del document[0]["levels"][0]["matcen_regression"]
        with self.assertRaisesRegex(runner.EvidenceError, "lacks frozen capture"):
            runner.select_metadata(document, "d1", "synthetic", 1)

    def test_redundant_runtime_backlink_normalization_allowed(self):
        reference = copy.deepcopy(self.metadata)
        reference["runtime_bindings"][0]["effective_record"]["segment"] = 9001
        self.assertEqual("passed", runner.validate_report(self.report, reference, "d1", "synthetic", 1))

    def test_defined_mask_change_rejected_against_frozen_reference(self):
        reference = copy.deepcopy(self.metadata)
        reference["runtime_bindings"][0]["effective_record"]["robot_flags"] = [4]
        reference["runtime_bindings"][0]["eligible_robots"] = [{"type": 2, "model": 3, "status": "valid_model"}]
        with self.assertRaisesRegex(runner.EvidenceError, "frozen metadata"):
            runner.validate_report(self.report, reference, "d1", "synthetic", 1)

    def test_incidental_ids_normalized_only_for_history(self):
        expected = runner.normalize_trace(self.report)
        self.report["cases"][0]["births"][0].update(signature=99, object=8)
        control = evidence(False)[1]
        self.assertEqual(
            "passed",
            runner.compare_reports(
                self.report, copy.deepcopy(self.report), control, expected, self.metadata, "d1", "synthetic", 1
            ),
        )
        repeat = copy.deepcopy(self.report)
        repeat["cases"][0]["births"][0]["signature"] = 100
        with self.assertRaisesRegex(runner.EvidenceError, "non-deterministic"):
            runner.compare_reports(self.report, repeat, control, expected, self.metadata, "d1", "synthetic", 1)

    def test_duplicate_signatures_rejected(self):
        self.report["cases"][0]["births"][0]["signature"] = 10
        with self.assertRaisesRegex(runner.EvidenceError, "signatures overlap"):
            self.validate()

    def test_actual_station_remap_rejected(self):
        reference = copy.deepcopy(self.metadata)
        binding = self.metadata["runtime_bindings"][0]
        binding["station"]["index"] = binding["segment_station"] = binding["effective_record"]["station"] = 1
        with self.assertRaisesRegex(runner.EvidenceError, "frozen metadata"):
            runner.validate_report(self.report, reference, "d1", "synthetic", 1)

    def test_wrong_edition_rejected(self):
        self.report["game"] = "d2"
        with self.assertRaisesRegex(runner.EvidenceError, "report game"):
            self.validate()

    def test_missing_actual_capture_rejected(self):
        del self.report["baseline"]
        with self.assertRaises(KeyError):
            self.validate()

    def test_missing_physical_limits_rejected(self):
        del self.metadata["engine_limits"]
        with self.assertRaises(KeyError):
            runner.preflight(self.metadata, "d1")

    def test_robot_model_runtime_bound_rejected(self):
        self.metadata["runtime_bindings"][0]["eligible_robots"][0]["model"] = 4
        with self.assertRaisesRegex(runner.EvidenceError, "unsafe robot model"):
            runner.preflight(self.metadata, "d1")

    def test_physical_station_safe_but_creator_encoding_unavailable(self):
        self.metadata["runtime_counts"]["stations"] = 130
        binding = self.metadata["runtime_bindings"][0]
        binding["station"]["index"] = binding["segment_station"] = binding["effective_record"]["station"] = 129
        runner.preflight(self.metadata, "d1")
        self.report["cases"][0]["load_snapshot"] = copy.deepcopy(self.metadata)
        self.report["cases"][0]["links"][0]["station"] = 129
        with self.assertRaisesRegex(runner.EvidenceError, "creator encoding"):
            self.validate()

    def test_generated_and_runtime_counts_are_independent_for_orphans(self):
        self.metadata.update(disk_count=0, declared_disk_count=0, disk_records=[])
        self.metadata["runtime_counts"]["robot_centers"] = 0
        self.metadata["runtime_bindings"][0]["binding_status"] = "logical_disk_unbound"
        runner.preflight(self.metadata, "d1")

    def test_extra_disk_records_do_not_require_extra_generated_centers(self):
        self.metadata.update(disk_count=2, declared_disk_count=2)
        self.metadata["runtime_counts"]["robot_centers"] = 2
        self.metadata["disk_records"].append(dict(self.metadata["disk_records"][0], ordinal=1))
        runner.preflight(self.metadata, "d1")

    def test_input_bytes_are_exact(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            file = root / "level.rdl"
            file.write_bytes(b"original mission bytes")
            files = {"level.rdl": hashlib.sha256(file.read_bytes()).hexdigest()}
            runner.verify_input_tree(root, files)
            file.write_bytes(b"changed mission bytes")
            with self.assertRaisesRegex(runner.EvidenceError, "changed input bytes"):
                runner.verify_input_tree(root, files)
            file.unlink()
            with self.assertRaisesRegex(runner.EvidenceError, "missing or changed"):
                runner.verify_input_tree(root, files)
            file.write_bytes(b"original mission bytes")
            (root / "extra.rdl").write_bytes(b"other")
            with self.assertRaisesRegex(runner.EvidenceError, "unexpected input file"):
                runner.verify_input_tree(root, files)

    def test_symlink_and_unsafe_manifest_paths_blocked(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            file = root / "level.rdl"
            file.write_bytes(b"original")
            digest = hashlib.sha256(file.read_bytes()).hexdigest()
            for name in (
                "../escape",
                "/absolute",
                "a/../level.rdl",
                "./level.rdl",
                "a//b",
                "a\\b",
                "C:/foo",
                "file:stream",
            ):
                with self.assertRaises(runner.EvidenceError):
                    runner.verify_input_tree(root, {name: digest})
            try:
                (root / "linked.rdl").symlink_to(file)
            except OSError as exc:
                self.skipTest(f"host cannot create file symlinks: {exc}")
            with self.assertRaisesRegex(runner.EvidenceError, "symlink"):
                runner.verify_input_tree(root, {"level.rdl": digest, "linked.rdl": digest})

    def test_wrong_input_identity_rejected(self):
        with self.assertRaisesRegex(runner.EvidenceError, "identity"):
            runner.verify_inputs(
                {"game": "d2", "mission": "synthetic", "level": 1}, "d1", "synthetic", 1, Path("."), Path(".")
            )


def lifecycle_evidence(dispatch=True, game="d1", difficulty=2, duplicate=False, cap=False):
    metadata, legacy = evidence(dispatch)
    trigger = metadata["triggers"][0]
    if game == "d2":
        trigger.update(type=2, flags=0)
        metadata["disk_records"][0]["robot_flags"] = [2, 0]
        metadata["runtime_bindings"][0]["effective_record"]["robot_flags"] = [2, 0]
    case = {
        key: copy.deepcopy(legacy["cases"][0][key])
        for key in (
            "trigger",
            "dispatch_requested",
            "runtime_type",
            "runtime_flags",
            "status",
            "links",
            "cleared_authored_robots",
        )
    }
    case.update(
        runtime_type=trigger["type"],
        runtime_flags=trigger["flags"],
        trigger_format="native_" + game,
    )
    if game == "d1":
        case["source_flags"] = trigger["flags"]
    if duplicate:
        trigger["links"].append(dict(trigger["links"][0], ordinal=1))
        trigger["declared_link_count"] = 2
        case["links"].append(dict(case["links"][0], ordinal=1))
    case["load_snapshot"] = copy.deepcopy(metadata)
    unlimited = game == "d2" and difficulty == 4
    state = dict(
        ordinal=0,
        station=2,
        segment=8,
        enabled=0,
        lives=3,
        capacity=0,
        timer=0,
        flag=0,
        disable_time=0,
        activation_count=0,
    )
    initial = dict(
        centers=[state],
        trigger=dict(
            type=trigger["type"],
            flags=trigger["flags"],
            num_links=trigger["declared_link_count"],
            links=copy.deepcopy(trigger["links"]),
        ),
        game_time=0,
        sim_state=1234,
        sim_calls=0,
        robot_total=10,
        robot_kills=0,
        robot_limit=10 if cap else 25,
        player=dict(
            number=0,
            object=0,
            segment=0,
            position=[1, 2, 3],
            forward=[0, 0, 65536],
            right=[65536, 0, 0],
            up=[0, 65536, 0],
        ),
    )
    case.update(initial=copy.deepcopy(initial), unlimited_native_lives=unlimited, rounds=[])
    previous, births = initial, 0
    for attempt in range(1, 5):
        activated = dispatch and (unlimited or attempt <= 3)
        before = copy.deepcopy(previous)
        after = copy.deepcopy(before)
        if activated:
            center = after["centers"][0]
            center.update(
                enabled=1,
                lives=center["lives"] - int(not unlimited),
                activation_count=center["activation_count"] + 1,
                timer=65536000,
                capacity=(difficulty + 3) * 65536,
                disable_time=(30 - 2 * difficulty) * 65536,
            )
        lights = [dict(signature=20 + attempt * 3, object=5, segment=8, position=[4, 5, 6])] if activated else []
        born = (
            [
                dict(
                    frame=40,
                    absolute_frame=(attempt - 1) * 2400 + 40,
                    spawn_ordinal=births,
                    ordinal=0,
                    station=2,
                    segment=8,
                    position=[7, 8, 9],
                    type=1,
                    signature=21 + attempt * 3,
                    object=6,
                    shields=100,
                )
            ]
            if activated and not cap
            else []
        )
        births += len(born)
        end = copy.deepcopy(after)
        end.update(
            game_time=attempt * 2400 * 1092,
            robot_total=10 + births,
            sim_calls=after["sim_calls"] + len(born) * 10,
            sim_state=after["sim_state"] + len(born) * 123,
        )
        end["centers"][0]["enabled"] = 0
        status = (
            "no_dispatch_control"
            if not dispatch
            else "native_lives_exhausted"
            if not activated
            else "inconclusive_zero_births_with_global_cap"
            if cap
            else "produced"
        )
        round_ = dict(
            attempt=attempt,
            frame_offset=(attempt - 1) * 2400,
            before_dispatch=before,
            dispatch_result=0 if dispatch else -1,
            after_dispatch=after,
            activation_lights=lights,
            births=born,
            after_frames=end,
            production_outcomes=[dict(ordinal=0, births=len(born), status=status)],
            global_cap_observed=cap,
            first_cap_frame=0 if cap else -1,
            first_cap_phase="before_station_update" if cap else "not_observed",
            first_all_disabled_frame=1700 if activated else -1,
        )
        if attempt == 1:
            round_.update(
                after_active_retrigger=copy.deepcopy(after),
                active_retrigger_result=0 if dispatch else -1,
            )
        case["rounds"].append(round_)
        previous = end
    case["birth_count"] = births
    report = dict(
        schema=2,
        scope="controlled_authored_trigger_station_lifecycle",
        game=game,
        mission="synthetic",
        level=1,
        difficulty=difficulty,
        rng_seed=1234,
        frame_time=1092,
        frames_per_attempt=2400,
        attempts=4,
        dispatch_requested=dispatch,
        observed_runtime_links_preserved=True,
        disk_original_links_verified=False,
        comparison_performed=False,
        passed=True,
        baseline=metadata,
        cases=[case],
        execution_summary=dict(
            matcen_triggers=1,
            executed=1,
            skipped=0,
            births=births,
            status="native_invariants_checked",
        ),
    )
    return metadata, report


class AuthoredMatcenLifecycleTests(unittest.TestCase):
    def setUp(self):
        self.metadata, self.report = lifecycle_evidence(duplicate=True)
        self.control = lifecycle_evidence(False, duplicate=True)[1]

    def validate(self, report=None):
        return runner.validate_lifecycle_report(report or self.report, self.metadata, "d1", "synthetic", 1, 2)

    def compare(self, current=None, repeat=None):
        current = current or self.report
        return runner.compare_lifecycle_reports(
            self.report,
            copy.deepcopy(self.report),
            current,
            repeat or copy.deepcopy(current),
            self.control,
            copy.deepcopy(self.control),
            self.metadata,
            "d1",
            "synthetic",
            1,
            2,
        )

    def test_every_native_difficulty_and_no_dispatch(self):
        for game in ("d1", "d2"):
            for difficulty in range(5):
                for dispatch in (False, True):
                    with self.subTest(game=game, difficulty=difficulty, dispatch=dispatch):
                        metadata, report = lifecycle_evidence(dispatch, game, difficulty)
                        self.assertEqual(
                            "passed",
                            runner.validate_lifecycle_report(
                                report,
                                metadata,
                                game,
                                "synthetic",
                                1,
                                difficulty,
                                dispatch,
                            ),
                        )
        self.assertEqual("passed", self.compare())

    def test_semantically_changed_birth_is_not_normalized_away(self):
        for field, value in (
            ("position", [7, 8, 10]),
            ("shields", 101),
            ("type", 2),
            ("station", 1),
            ("ordinal", 1),
            ("segment", 7),
        ):
            with self.subTest(field=field):
                changed = copy.deepcopy(self.report)
                changed["cases"][0]["rounds"][0]["births"][0][field] = value
                with self.assertRaises(runner.EvidenceError):
                    self.compare(changed)
        changed = copy.deepcopy(self.report)
        birth = changed["cases"][0]["rounds"][0]["births"][0]
        birth["frame"] += 1
        birth["absolute_frame"] += 1
        with self.assertRaisesRegex(runner.EvidenceError, "differs from historical"):
            self.compare(changed)

    def test_missing_birth_and_duplicate_link_rejected(self):
        for remove in (
            lambda c: c["rounds"][0]["births"].pop(),
            lambda c: c["links"].pop(),
        ):
            changed = copy.deepcopy(self.report)
            remove(changed["cases"][0])
            with self.assertRaises(runner.EvidenceError):
                self.compare(changed)

    def test_negative_first_birth_frame_rejected(self):
        changed = copy.deepcopy(self.report)
        changed["cases"][0]["rounds"][0]["births"][0].update(frame=-1, absolute_frame=-1)
        with self.assertRaisesRegex(runner.EvidenceError, "birth frame"):
            self.validate(changed)

    def test_metadata_mask_and_target_changes_rejected(self):
        for change in (
            lambda r: r["baseline"]["disk_records"][0].update(robot_flags=[4]),
            lambda r: r["cases"][0]["links"][0].update(side=2),
            lambda r: r["cases"][0]["links"].reverse(),
        ):
            changed = copy.deepcopy(self.report)
            change(changed)
            with self.assertRaises(runner.EvidenceError):
                self.compare(changed)

    def test_exact_repeats_keep_allocation_ids(self):
        changed = copy.deepcopy(self.report)
        changed["cases"][0]["rounds"][0]["births"][0]["signature"] = 999
        self.assertEqual(
            runner.normalize_lifecycle_trace(changed),
            runner.normalize_lifecycle_trace(self.report),
        )
        self.assertEqual("passed", self.compare(changed))
        with self.assertRaisesRegex(runner.EvidenceError, "non-deterministic lifecycle repeat"):
            self.compare(self.report, changed)

    def test_active_retrigger_counters_and_round_resets_rejected(self):
        for change in (
            lambda c: c["rounds"][0]["after_active_retrigger"]["centers"][0].update(lives=1),
            lambda c: c["rounds"][1]["before_dispatch"].update(sim_calls=0),
            lambda c: c["rounds"][1]["after_frames"].update(robot_kills=1),
            lambda c: c["rounds"].pop(),
        ):
            changed = copy.deepcopy(self.report)
            change(changed["cases"][0])
            with self.assertRaises(runner.EvidenceError):
                self.compare(changed)

    def test_cap_only_evidence_is_inconclusive_not_a_parity_pass(self):
        metadata, report = lifecycle_evidence(cap=True)
        control = lifecycle_evidence(False, cap=True)[1]
        self.assertEqual(
            "review-required",
            runner.validate_lifecycle_report(report, metadata, "d1", "synthetic", 1, 2),
        )
        with self.assertRaisesRegex(runner.EvidenceError, "no births"):
            runner.compare_lifecycle_reports(
                report,
                copy.deepcopy(report),
                copy.deepcopy(report),
                copy.deepcopy(report),
                control,
                copy.deepcopy(control),
                metadata,
                "d1",
                "synthetic",
                1,
                2,
            )
        report["cases"][0]["rounds"][0]["first_cap_frame"] = -1
        with self.assertRaisesRegex(runner.EvidenceError, "cap timing"):
            runner.validate_lifecycle_report(report, metadata, "d1", "synthetic", 1, 2)

    def test_all_skipped_and_no_matcen_are_not_execution_passes(self):
        for empty in (False, True):
            metadata, report = lifecycle_evidence()
            if empty:
                metadata["triggers"] = []
                report["cases"] = []
            else:
                case = report["cases"][0]
                for key in ("initial", "rounds", "birth_count"):
                    del case[key]
                case.update(
                    status="skipped",
                    skip_reasons=["one_shot_outside_repeatable_lifecycle_scope"],
                )
            report["passed"] = False
            report["execution_summary"] = dict(
                matcen_triggers=0 if empty else 1,
                executed=0,
                skipped=0 if empty else 1,
                births=0,
                status="no_matcen_triggers" if empty else "all_cases_skipped",
            )
            self.assertEqual(
                "review-required",
                runner.validate_lifecycle_report(report, metadata, "d1", "synthetic", 1, 2),
            )
            report["passed"] = True
            with self.assertRaisesRegex(runner.EvidenceError, "empty lifecycle coverage"):
                runner.validate_lifecycle_report(report, metadata, "d1", "synthetic", 1, 2)

    def test_incomplete_imported_trigger_capture_fails_closed(self):
        metadata, report = lifecycle_evidence(game="d2")
        metadata["triggers"][0]["flags"] |= 128
        with self.assertRaisesRegex(runner.EvidenceError, "lacks source action masks"):
            runner.validate_lifecycle_report(report, metadata, "d2", "synthetic", 1, 2)

    def test_shared_semantic_field_omissions_fail_closed(self):
        for missing in ("birth_shields", "light_position", "player_position", "player_forward", "player_number"):
            with self.subTest(missing=missing):
                active, control = copy.deepcopy(self.report), copy.deepcopy(self.control)
                for report in (active, control):
                    for case in report["cases"]:
                        snapshots = [case["initial"]]
                        for round_ in case["rounds"]:
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
                            if missing == "birth_shields":
                                for birth in round_["births"]:
                                    del birth["shields"]
                            if missing == "light_position":
                                for light in round_["activation_lights"]:
                                    del light["position"]
                        if missing.startswith("player_"):
                            for snapshot in snapshots:
                                del snapshot["player"][missing.removeprefix("player_")]
                with self.assertRaises(runner.EvidenceError):
                    runner.compare_lifecycle_reports(
                        active,
                        copy.deepcopy(active),
                        copy.deepcopy(active),
                        copy.deepcopy(active),
                        control,
                        copy.deepcopy(control),
                        self.metadata,
                        "d1",
                        "synthetic",
                        1,
                        2,
                    )


if __name__ == "__main__":
    unittest.main()
