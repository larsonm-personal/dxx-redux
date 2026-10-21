import copy
import json
import unittest
from pathlib import Path
import tempfile

from nightly_regression_corpus import compare_metadata, compare_route, freeze_oracles, require_unchanged_oracles
from test_matcen_metadata import fixture as matcen_fixture


class CorpusComparisons(unittest.TestCase):
    def setUp(self):
        self.metadata = [
            {
                "status": "ok",
                "target_index": 0,
                "levels": [{"level_num": 1, "level_file": "one.rl2", "segment_count": 10, "travel_time_seconds": 12}],
                "provenance": {"date": "1995-01-01"},
            }
        ]
        self.metadata_summary = [{"name": "mission.zip", "status": "passed"}]
        self.route = {
            "schema": "dxx-guidebot-route-simulation-v1",
            "mission_filename": "one.mn2",
            "target_index": 0,
            "generation": 4,
            "fixed_hz": 60,
            "seed": 1,
            "status": "ok",
            "levels": [
                {
                    "level_num": 1,
                    "level_file": "one.rl2",
                    "status": "ok",
                    "route_input_sha256": "A" * 64,
                    "total_frames": 100,
                    "rng_start": {"state": 1, "calls": 0},
                    "rng_end": {"state": 10, "calls": 20},
                    "objectives": [{"n": "exit", "s": 2}],
                }
            ],
        }
        self.summary = {
            "schema": "dxx-guidebot-route-simulation-batch-v1",
            "mode": "headless",
            "write_regression": False,
            "selected_levels": 1,
            "selected_work_items": ["mission.json|0|1|one.rl2"],
            "infrastructure_failures": [],
        }

    def metadata_result(self, output=None, summary=None):
        return compare_metadata(
            self.metadata,
            self.metadata if output is None else output,
            self.metadata_summary if summary is None else summary,
            "mission.zip",
        )["status"]

    def route_result(self, output=None, summary=None):
        return compare_route(
            self.route,
            self.route if output is None else output,
            self.summary if summary is None else summary,
            "mission.json",
        )["status"]

    def test_metadata_difference_paths_are_bounded_and_do_not_expose_values(self):
        from nightly_regression_corpus import metadata_differences

        left = [{"levels": [{"segment_count": 2, "PRIVATE_KEY": "PRIVATE_VALUE"}]}]
        right = [{"levels": [{"segment_count": 3, "PRIVATE_KEY": "OTHER_SECRET"}]}]
        differences = metadata_differences(left, right)
        self.assertIn({"path": [0, "levels", 0, "segment_count"], "kind": "value"}, differences)
        self.assertNotIn("PRIVATE", str(differences))
        self.assertNotIn("OTHER_SECRET", str(differences))
        self.assertEqual(len(metadata_differences(list(range(100)), [-1] * 100)), 16)
        warnings = metadata_differences({"data_warnings": ["PRIVATE"]}, {"data_warnings": ["SECRET"]})
        self.assertEqual(warnings, [{"path": ["data_warnings", 0], "kind": "value"}])

    def test_route_early_failure_categories_are_bounded_and_private(self):
        from nightly_regression_corpus import route_runner_failure

        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "stderr.log"
            for text, category in (
                ("\x1b[31mD1-in-D2 data directory is missing\x1b[0m PRIVATE_PATH", "route-base-data"),
                ("Game data file matching pinned hash not found: PRIVATE", "route-base-data"),
                ("Assertion PRIVATE failed", "assertion"),
                ("SECRET_UNKNOWN_ERROR", "unclassified-owner-failure"),
                ("D2 HOG directory not found" + "X" * 20000, "unclassified-owner-failure"),
            ):
                path.write_text(text)
                self.assertEqual(route_runner_failure(path, 1), {"exit_code": 1, "categories": [category]})

    def test_inner_worker_failure_diagnostics_are_typed_and_private(self):
        from nightly_regression_corpus import metadata_worker_diagnostics, metadata_owner_failure
        import nightly_retail

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "123.shutdown.json").write_text(
                json.dumps(
                    {
                        "exit_code": -6,
                        "timed_out": False,
                        "stdout_complete": True,
                        "stderr_complete": True,
                        "shutdown_ms": 5,
                        "executable": "PRIVATE_PATH",
                    }
                )
            )
            (root / "123.stderr.log").write_text("Assertion 'PRIVATE_ASSERTION' failed\nPRIVATE_BYTES")
            (root / "123.stdout.log").write_text("PRIVATE_BYTES")
            workers = metadata_worker_diagnostics(root)
            self.assertEqual(workers[0]["exit_code"], -6)
            self.assertEqual(workers[0]["categories"], ["assertion"])
            owner = metadata_owner_failure(
                [
                    {
                        "name": "fixture.zip",
                        "status": "failed",
                        "reason": "metadata worker closed its output unexpectedly with exit code -6; log=PRIVATE_PATH",
                    }
                ],
                "fixture.zip",
            )
            self.assertEqual(owner, {"categories": ["unexpected-eof"], "exit_code": -6})
            self.assertEqual(metadata_owner_failure([], "fixture.zip"), {})
            clean = nightly_retail.public_diagnostics({"workers": workers, "owner_failure": owner})
            self.assertNotIn("PRIVATE", json.dumps(clean))
            self.assertEqual(clean["workers"], workers)
            self.assertEqual(clean["owner_failure"]["categories"], owner["categories"])
            self.assertEqual(clean["owner_failure"]["exit_code"], -6)
            (root / "123.shutdown.json").write_text("invalid PRIVATE_BYTES")
            self.assertEqual(metadata_worker_diagnostics(root)[0]["categories"], ["unclassified-owner-failure"])

    def test_inner_worker_known_signatures_never_return_captured_strings(self):
        from nightly_regression_corpus import worker_failure_categories

        cases = {
            "metadata worker timed out after 3 seconds; log=PRIVATE_PATH": "worker-timeout",
            "AddressSanitizer: heap-use-after-free PRIVATE_BYTES": "address-sanitizer",
            "runtime error: PRIVATE_BYTES": "undefined-behavior",
            "7z extract failed for PRIVATE_PATH": "archive-extraction",
            "Invalid mission descriptor PRIVATE_PATH": "descriptor-invalid",
            "Kotlin metadata worker closed its output unexpectedly": "projection-worker",
        }
        for message, category in cases.items():
            with self.subTest(category=category):
                expected = ["unexpected-eof", category] if category == "projection-worker" else [category]
                self.assertEqual(worker_failure_categories(message), expected)

    def test_mutated_oracle_cannot_be_its_own_expected_output(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "baseline.json"
            path.write_text('{"original": true}')
            snapshot = freeze_oracles([path])
            require_unchanged_oracles(snapshot)
            path.write_text('{"regenerated": true}')
            with self.assertRaises(ValueError):
                require_unchanged_oracles(snapshot)

    def test_wrong_selected_level_never_passes(self):
        result = compare_route(self.route, self.route, self.summary, "mission.json", "mission.json|0|2|other.rl2")
        self.assertEqual(result["status"], "FAIL")

    def test_metadata_exact_and_semantic_dates(self):
        self.assertEqual(self.metadata_result(), "PASS")
        output = copy.deepcopy(self.metadata)
        output[0]["provenance"]["date"] = "2000-01-01"
        self.assertEqual(self.metadata_result(output), "FAIL")
        output = copy.deepcopy(self.metadata)
        output[0]["levels"][0]["travel_time_seconds"] += 1
        self.assertEqual(self.metadata_result(output), "FAIL")

    def test_metadata_missing_skipped_failed_and_shape(self):
        self.assertEqual(self.metadata_result(summary=[]), "BLOCKED")
        for status, expected in [("skipped_large", "BLOCKED"), ("failed", "FAIL")]:
            self.assertEqual(self.metadata_result(summary=[{"name": "mission.zip", "status": status}]), expected)
        self.assertEqual(self.metadata_result(self.metadata[0]), "FAIL")
        self.assertEqual(
            compare_metadata(self.metadata, None, self.metadata_summary, "mission.zip")["status"], "BLOCKED"
        )

    def test_zero_geometry_is_not_a_passing_oracle(self):
        self.metadata[0]["levels"][0]["segment_count"] = 0
        self.assertEqual(self.metadata_result(), "FAIL")

    def test_unbound_matcen_observations_do_not_define_the_nightly_oracle(self):
        capture = matcen_fixture()
        unbound = copy.deepcopy(capture["bindings"][0])
        unbound.update(
            segment=7,
            generated_ordinal=1,
            effective_ordinal=1,
            binding_status="logical_disk_unbound",
            segment_station=4,
        )
        unbound["record_overrides"] = dict(segment=7, station=4, robot_flags=[0, 0])
        capture["bindings"].append(unbound)
        self.metadata[0]["levels"][0]["matcen_regression"] = capture
        output = copy.deepcopy(self.metadata)
        warm = output[0]["levels"][0]["matcen_regression"]
        warm["bindings"][1]["record_overrides"]["robot_flags"] = [8, 0]
        warm["robot_models"].append(dict(type=3, model=4, status="valid_model"))
        saved = copy.deepcopy(output)
        self.assertEqual(self.metadata_result(output), "PASS")
        self.assertEqual(output, saved, "comparison must retain the raw observations")
        # A stale mask can overlap a real disk record; its shared model stays strict.
        warm["bindings"][1]["record_overrides"]["robot_flags"] = [9, 0]
        warm["robot_models"][0]["model"] = 99
        self.assertEqual(self.metadata_result(output), "FAIL")
        for field, value in [
            ("binding_status", "disk_bound"),
            ("effective_ordinal", 0),
            ("station_status", "wrong_station_type"),
            ("segment_station", 9),
        ]:
            changed = copy.deepcopy(saved)
            changed[0]["levels"][0]["matcen_regression"]["bindings"][1][field] = value
            self.assertEqual(self.metadata_result(changed), "FAIL", field)
        changed = copy.deepcopy(saved)
        changed[0]["levels"][0]["matcen_regression"]["disk_records"][0]["robot_flags"] = [4, 0]
        self.assertEqual(self.metadata_result(changed), "FAIL")

    def test_incomplete_matcen_observations_remain_strict(self):
        capture = matcen_fixture()
        capture["capture_status"] = "incomplete"
        self.metadata[0]["levels"][0]["matcen_regression"] = capture
        self.assertEqual(self.metadata_result(), "PASS")
        changed = copy.deepcopy(self.metadata)
        changed[0]["levels"][0]["matcen_regression"]["disk_records"][0]["robot_flags"] = [8, 0]
        self.assertEqual(self.metadata_result(changed), "FAIL")

    def test_route_exact_and_unselected_aggregate(self):
        self.assertEqual(self.route_result(), "PASS")
        output = copy.deepcopy(self.route)
        output["status"] = "partial"
        output["levels"].append({"level_num": 2, "level_file": "two.rl2", "status": "not_run"})
        self.assertEqual(self.route_result(output), "PASS")

    def test_route_non_ok_never_passes(self):
        for status in [
            "failed",
            "route_mismatch",
            "partial",
            "stale",
            "not_run",
            "timeout",
            "unsupported",
            "nondeterministic",
            "infrastructure_error",
            None,
        ]:
            output = copy.deepcopy(self.route)
            output["levels"][0]["status"] = status
            with self.subTest(status=status):
                self.assertEqual(self.route_result(output), "FAIL")

    def test_known_non_ok_outcome_compares_without_confirmed_coverage(self):
        for status in ("partial", "failed", "timeout", "unsupported", "route_mismatch"):
            self.route["levels"][0]["status"] = status
            result = compare_route(self.route, self.route, self.summary, "mission.json")
            self.assertEqual(result["status"], "PASS")
            self.assertTrue(result["comparison_only"])
            self.assertEqual(result["runtime_outcome"], status)
        self.route["levels"][0]["status"] = "not_run"
        self.assertNotEqual(self.route_result(), "PASS")

    def test_engine_mode_header_is_semantic(self):
        self.route["engine_mode"] = "d1_in_d2"
        output = copy.deepcopy(self.route)
        del output["engine_mode"]
        self.assertEqual(self.route_result(output), "FAIL")

    def test_route_requires_full_state(self):
        output = copy.deepcopy(self.route)
        del output["levels"][0]["rng_start"]
        self.assertEqual(self.route_result(output), "BLOCKED")

    def test_route_state_drift(self):
        for field, value in [
            ("total_frames", 101),
            ("rng_end", {"state": 11, "calls": 20}),
            ("objectives", []),
            ("route_input_sha256", "B" * 64),
        ]:
            output = copy.deepcopy(self.route)
            output["levels"][0][field] = value
            self.assertEqual(self.route_result(output), "FAIL")

    def test_route_missing_selection_or_leaf(self):
        for update in [
            {"selected_work_items": []},
            {"selected_levels": 2},
            {"selected_work_items": ["wrong.json|0|1|one.rl2"]},
            {"selected_work_items": ["mission.json|x|1|one.rl2"]},
        ]:
            summary = dict(self.summary, **update)
            self.assertEqual(self.route_result(summary=summary), "BLOCKED")
        self.assertEqual(self.route_result(dict(self.route, levels=[])), "BLOCKED")
        self.assertEqual(compare_route(self.route, self.route, None, "mission.json")["status"], "BLOCKED")

    def test_route_wrong_mode_publication_and_header(self):
        for update in [
            {"write_regression": True},
            {"mode": "headed"},
            {"infrastructure_failures": [{"problem": "missing binary"}]},
        ]:
            self.assertEqual(self.route_result(summary=dict(self.summary, **update)), "FAIL")
        self.assertEqual(self.route_result(dict(self.route, seed=2)), "FAIL")


if __name__ == "__main__":
    unittest.main()
