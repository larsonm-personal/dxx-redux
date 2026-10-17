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


def compact(metadata):
    return runner.compact_capture(metadata, "d1")


class AuthoredMatcenReportTests(unittest.TestCase):
    def setUp(self):
        self.metadata, self.report = evidence()

    def validate(self):
        return runner.validate_report(self.report, compact(self.metadata), "d1", "synthetic", 1)

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
                compact(self.metadata),
                "d1",
                "synthetic",
                1,
            ),
        )

    def test_compact_projection_retains_sparse_trigger_ids_and_duplicate_links(self):
        trigger = self.metadata["triggers"][0]
        trigger["index"] = 1
        trigger["links"] *= 2
        trigger["declared_link_count"] = 2
        self.metadata["triggers"].insert(0, dict(trigger, index=0, flags=0))
        projected = compact(self.metadata)
        self.assertEqual([1], [t["index"] for t in projected["triggers"]])
        self.assertEqual(trigger["links"], projected["triggers"][0]["links"])
        runner.preflight(projected, "d1")
        runner.preflight(self.metadata, "d1")
        self.metadata["triggers"].pop(0)
        with self.assertRaisesRegex(runner.EvidenceError, "trigger identity"):
            runner.preflight(self.metadata, "d1")

    def test_invalid_bindings_survive_projection_and_require_review(self):
        for status in ("invalid_segment", "physical_out_of_bounds", "invalid_station"):
            with self.subTest(status=status):
                raw = copy.deepcopy(self.metadata)
                binding = raw["runtime_bindings"][0]
                binding.pop("station")
                if status == "invalid_station":
                    binding["station_status"] = status
                else:
                    binding["binding_status"] = status
                    for key in ("effective_record", "eligible_robots", "station_status"):
                        binding.pop(key)
                    if status == "invalid_segment":
                        binding.pop("effective_ordinal")
                    else:
                        binding["effective_ordinal"] = 20
                projected = compact(raw)
                resolved = runner.resolve_binding(projected, projected["bindings"][0])
                self.assertEqual({}, resolved["station"])
                self.assertEqual(1, len(runner.compare_capture(projected, projected)["unbound_requires_review"]))
                with self.assertRaises((runner.EvidenceError, KeyError)):
                    runner.preflight(projected, "d1")

    def test_converted_d1_trigger_uses_source_flags(self):
        self.assertTrue(runner.is_matcen_trigger({"type": 0, "flags": 0, "source_flags": 64}, "d2"))
        self.assertFalse(runner.is_matcen_trigger({"type": 2, "flags": 0, "source_flags": 1}, "d2"))

    def test_compact_projection_keeps_only_deltas_and_shared_models(self):
        projected = compact(self.metadata)
        binding = projected["bindings"][0]
        self.assertNotIn("record_overrides", binding)
        self.assertNotIn("station_overrides", binding)
        self.assertNotIn("eligible_robots", binding)
        self.assertEqual(self.metadata["runtime_bindings"][0]["eligible_robots"], projected["robot_models"])
        self.metadata["runtime_bindings"][0]["effective_record"]["interval"] = 2
        self.assertEqual({"interval": 2}, compact(self.metadata)["bindings"][0]["record_overrides"])
        self.metadata.update(disk_records=[], disk_count=0, declared_disk_count=0)
        self.assertEqual(
            self.metadata["runtime_bindings"][0]["effective_record"],
            compact(self.metadata)["bindings"][0]["record_overrides"],
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
                self.report, self.report, evidence(False)[1], expected, compact(self.metadata), "d1", "synthetic", 1
            )

    def test_no_dispatch_birth_rejected(self):
        control = evidence(False)[1]
        control["cases"][0]["births"] = copy.deepcopy(self.report["cases"][0]["births"])
        with self.assertRaisesRegex(runner.EvidenceError, "no-dispatch"):
            runner.validate_report(control, compact(self.metadata), "d1", "synthetic", 1, False)

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
            (root / "metadata.json").write_text(json.dumps(compact(self.metadata)))
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
            with self.assertRaises((runner.EvidenceError, runner.MatcenMismatch)):
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
            metadata_path.write_text(json.dumps(compact(self.metadata)))
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
        self.assertEqual("passed", runner.validate_report(self.report, compact(reference), "d1", "synthetic", 1))

    def test_defined_mask_change_rejected_against_frozen_reference(self):
        reference = copy.deepcopy(self.metadata)
        reference["runtime_bindings"][0]["effective_record"]["robot_flags"] = [4]
        reference["runtime_bindings"][0]["eligible_robots"] = [{"type": 2, "model": 3, "status": "valid_model"}]
        with self.assertRaisesRegex(runner.EvidenceError, "frozen metadata"):
            runner.validate_report(self.report, compact(reference), "d1", "synthetic", 1)

    def test_incidental_ids_normalized_only_for_history(self):
        expected = runner.normalize_trace(self.report)
        self.report["cases"][0]["births"][0].update(signature=99, object=8)
        control = evidence(False)[1]
        self.assertEqual(
            "passed",
            runner.compare_reports(
                self.report, copy.deepcopy(self.report), control, expected, compact(self.metadata), "d1", "synthetic", 1
            ),
        )
        repeat = copy.deepcopy(self.report)
        repeat["cases"][0]["births"][0]["signature"] = 100
        with self.assertRaisesRegex(runner.EvidenceError, "non-deterministic"):
            runner.compare_reports(self.report, repeat, control, expected, compact(self.metadata), "d1", "synthetic", 1)

    def test_duplicate_signatures_rejected(self):
        self.report["cases"][0]["births"][0]["signature"] = 10
        with self.assertRaisesRegex(runner.EvidenceError, "signatures overlap"):
            self.validate()

    def test_actual_station_remap_rejected(self):
        reference = copy.deepcopy(self.metadata)
        binding = self.metadata["runtime_bindings"][0]
        binding["station"]["index"] = binding["segment_station"] = binding["effective_record"]["station"] = 1
        with self.assertRaisesRegex(runner.EvidenceError, "frozen metadata"):
            runner.validate_report(self.report, compact(reference), "d1", "synthetic", 1)

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


if __name__ == "__main__":
    unittest.main()
