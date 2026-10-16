"""Synthetic controls only: no private bytes, credentials or network."""

import argparse
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import nightly_retail as retail
import nightly_tooling_sweep as sweep
import private_input_demos as demos
import run_host_test_suites as adapter
from test_nightly_retail import ENV


class PrivateDemoTests(unittest.TestCase):
    def test_manifest_has_only_the_two_originals(self):
        self.assertEqual([case["level"] for case in demos.CASES], [7, 9])
        self.assertEqual([case["frames"] for case in demos.CASES], [2161, 7372])
        self.assertTrue(all(len(case["sha256"]) == 64 for case in demos.CASES))

    def test_missing_and_changed_bytes_fail_before_parsing_or_copy(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaises(OSError):
                demos.validate(root)
            for case in demos.CASES:
                (root / case["name"]).write_text("{}")
            with self.assertRaisesRegex(ValueError, "identity"):
                demos.stage(root, root / "stage")
            self.assertFalse((root / "stage").exists())

    def test_terminal_oracle_catches_missing_truncated_and_changed_state(self):
        oracle = {"frame_count": 10, "player0": {"score": 9}, "position": {"x": 3}}
        demos.assert_expected(oracle, oracle)
        for actual in (
            {},
            {**oracle, "frame_count": 9},
            {**oracle, "player0": {"score": 8}},
        ):
            with self.assertRaises(ValueError):
                demos.assert_expected(oracle, actual)

    def test_oracle_rejects_boolean_float_and_array_type_substitutions(self):
        for actual in (True, 1.0):
            with self.assertRaises(TypeError):
                demos.assert_expected({"frame_count": 1}, {"frame_count": actual})
            with self.assertRaises(TypeError):
                demos.assert_expected([1], [actual])

    def test_command_never_weakens_original_oracle(self):
        args = argparse.Namespace(
            pwsh="pwsh",
            repo=Path("/repo"),
            demos=Path("/private"),
            data=Path("/data"),
            exe=Path("/exe"),
            timeout=300,
            minimum_free_gb=0.01,
        )
        command = demos.command(args, demos.CASES[0], Path("/results/one.json"))
        for flag in (
            "-inputdemo-replay",
            "-inputdemo-actual-result",
        ):
            self.assertIn(flag, command)
        self.assertEqual(command[0], str(Path("/exe")))
        self.assertNotIn("pwsh", command)
        self.assertNotIn("-SkipExpectedChecks", command)

    def test_admission_requires_exact_private_commit_and_trusted_entry(self):
        entry = {
            "id": "host-profile:retail-input-demos",
            "owner": "host-adapter",
            "lane": "host",
            "args": {"suite": "host-profile:retail-input-demos"},
            "estimated_seconds": 1200,
            "fixture": "retail-demos",
        }
        policy = {"schema": 1, "suites": [entry], "exclusions": []}
        catalog = {"schema": 1, "tests": []}
        for commit in ("", "b" * 40, demos.ASSETS_COMMIT):
            identity = retail.configured_identity({**ENV, "RETAIL_ASSETS_COMMIT": commit})
            plan, _ = sweep.plan_batch("a" * 40, catalog, policy, None, retail_assets=identity)
            self.assertEqual(bool(plan["tests"]), commit == demos.ASSETS_COMMIT)
            sweep.validate_plan(plan, policy)
            if plan["tests"]:
                self.assertTrue(retail.lane_requires_retail(plan, "host"))
                plan["retail_assets"]["commit"] = "b" * 40
                with self.assertRaisesRegex(ValueError, "reviewed commit"):
                    sweep.validate_plan(plan, policy)
        self.assertIsNone(
            retail.configured_identity(
                {
                    **ENV,
                    "RETAIL_ASSETS_COMMIT": demos.ASSETS_COMMIT,
                    "GITHUB_EVENT_NAME": "pull_request",
                }
            )
        )

    def test_host_profile_uses_headless_target_and_private_stage(self):
        suite = adapter.load_suites()["host-profile:retail-input-demos"]
        self.assertEqual(suite["target_suffix"], "-headless")
        self.assertEqual(suite["fixture"], "retail-demos")
        args = adapter.retail_profile_arguments(suite, Path("/repo"), Path("/build"), Path("/work"))
        self.assertIn(str(Path("/repo/game_data/retail-ci/demos")), args)
        self.assertIn(str(Path("/build/engine-d2/main/dxx-redux-d2-headless")), args)

    def test_native_failure_controls_and_both_cases_attempted(self):
        scenarios = (
            "pass",
            "nonzero",
            "timeout",
            "missing",
            "malformed",
            "truncated",
            "mismatch",
            "repeat",
        )
        for scenario in scenarios:
            with (
                self.subTest(scenario=scenario),
                tempfile.TemporaryDirectory() as directory,
            ):
                args = argparse.Namespace(
                    output=Path(directory) / "output",
                    demos=Path("/private"),
                    data=Path("/data"),
                    exe=Path("/exe"),
                    timeout=10,
                    minimum_free_gb=0,
                )
                calls = []

                def launch(command, scenario=scenario, calls=calls, **kwargs):
                    calls.append(command)
                    self.assertNotIn("pwsh", command)
                    self.assertNotIn("-inputdemo-state-log", command)
                    self.assertTrue(Path(kwargs["env"]["HOME"]).is_dir())
                    self.assertEqual(kwargs["timeout"], 10)
                    if scenario == "timeout":
                        raise demos.subprocess.TimeoutExpired(command, 10)
                    actual = {"frame_count": 10, "player0": {"score": 9}, "extra": 1}
                    if scenario == "truncated":
                        actual["frame_count"] = 9
                    if scenario == "mismatch":
                        actual["player0"]["score"] = 8
                    if scenario == "repeat":
                        actual["extra"] = len(calls) % 2
                    if scenario != "missing":
                        result = Path(command[command.index("-inputdemo-actual-result") + 1])
                        result.write_text("{" if scenario == "malformed" else json.dumps(actual))
                    return argparse.Namespace(returncode=1 if scenario == "nonzero" else 0)

                oracle = {"frame_count": 10, "player0": {"score": 9}}
                with (
                    patch.object(demos, "validate", return_value=[oracle] * 2),
                    patch.object(demos.subprocess, "run", side_effect=launch),
                ):
                    self.assertEqual(demos.run(args), 0 if scenario == "pass" else 1)
                    self.assertEqual(len(calls), 4)
                report = json.loads((args.output / "summary.json").read_text())
                self.assertEqual(report["status"], "PASS" if scenario == "pass" else "FAIL")


if __name__ == "__main__":
    unittest.main()
