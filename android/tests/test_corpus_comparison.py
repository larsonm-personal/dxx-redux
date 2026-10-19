"""Asset-free contract tests; never launch native producers or their supervisor."""

import copy
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import corpus_comparison as c


class ComparisonTests(unittest.TestCase):
    def setUp(self):
        self.settings = c.config()
        self.identity = "0|1|one.rl2"
        self.entry = {"baseline": "one.json", "levels": [self.identity], "available": True, "shard": 0}

    def outcome(self, status="ok"):
        raw = {"status": "confirmed" if status == "ok" else status, "frames": 100}
        return {
            "levels": {self.identity: {"status": status, "total_frames": 100}},
            "engine": {self.identity: [copy.deepcopy(raw), copy.deepcopy(raw)]},
            "process": {self.identity: [{"exit_code": 0, "timed_out": False}] * 2},
        }

    def classification(self, before, after):
        return c.compare(self.entry, before, after)[0]["classification"]

    def test_gate_requires_exact_trusted_context_and_asset_pin(self):
        env = {
            "GITHUB_REPOSITORY": c.REPOSITORY,
            "GITHUB_REF": "refs/heads/cmake",
            "GITHUB_WORKFLOW_REF": c.WORKFLOW,
            "GITHUB_EVENT_NAME": "workflow_dispatch",
            "RETAIL_ASSETS_COMMIT": self.settings["assets"],
        }
        c.gate(self.settings, env)
        for key in env:
            with self.subTest(key=key), self.assertRaises(ValueError):
                c.gate(self.settings, {**env, key: "untrusted"})

    def test_config_requires_immutable_revisions_and_reviewed_settings(self):
        for key, value in (("base", "cmake"), ("candidate", "main"), ("assets", "latest"), ("repeat", 1)):
            with patch.object(c, "read", return_value={**self.settings, key: value}), self.assertRaises(ValueError):
                c.config()

    def test_improvement_regression_and_unmodified_failure_are_distinct(self):
        self.assertEqual("improvement", self.classification(self.outcome("failed"), self.outcome()))
        self.assertEqual("regression", self.classification(self.outcome(), self.outcome("timeout")))
        self.assertEqual("unchanged", self.classification(self.outcome("failed"), self.outcome("failed")))
        self.assertEqual("unchanged", self.classification(self.outcome("unsupported"), self.outcome("unsupported")))

    def test_repeat_disagreement_and_missing_repeat_fail_closed(self):
        after = self.outcome()
        after["engine"][self.identity][1]["frames"] = 101
        self.assertEqual("nondeterministic", self.classification(self.outcome(), after))
        after = self.outcome("failed")
        after["process"][self.identity] = [{"exit_code": 0, "timed_out": False}, {"exit_code": 2, "timed_out": False}]
        self.assertEqual("nondeterministic", self.classification(self.outcome("failed"), after))
        after["process"][self.identity].pop()
        self.assertEqual("incomplete", self.classification(self.outcome("failed"), after))
        after = self.outcome()
        after["engine"][self.identity].pop()
        self.assertEqual("incomplete", self.classification(self.outcome(), after))

    def test_partial_or_failed_process_cannot_be_a_clean_improvement(self):
        for receipt in ({"exit_code": 1, "timed_out": False}, {"exit_code": 0, "timed_out": True}):
            after = self.outcome()
            after["process"][self.identity] = [receipt] * 2
            self.assertEqual("incomplete", self.classification(self.outcome("failed"), after))
        before = self.outcome("failed")
        before["process"][self.identity] = [{"exit_code": 1, "timed_out": False}] * 2
        self.assertEqual("incomplete", self.classification(before, self.outcome()))
        before = self.outcome("timeout")
        before["process"][self.identity] = [{"exit_code": -1, "timed_out": True}] * 2
        before["engine"][self.identity] = []
        self.assertEqual("incomplete", self.classification(before, self.outcome()))
        before = self.outcome("failed")
        before["engine"][self.identity].pop()
        self.assertEqual("incomplete", self.classification(before, self.outcome()))
        after = self.outcome()
        after["engine"][self.identity] = [{"status": "timeout"}] * 2
        row = c.compare(self.entry, self.outcome("failed"), after)[0]
        self.assertEqual("incomplete", row["classification"])
        self.assertEqual("timeout", row["raw_after"][0]["status"])
        after = self.outcome()
        after["incomplete"] = "selection_or_oracle_contract_changed"
        self.assertEqual("incomplete", self.classification(self.outcome("failed"), after))

    def test_semantic_differences_are_retained(self):
        for field, value in (
            ("total_frames", 101),
            ("rng_end", {"state": 42}),
            ("objectives", [{"n": "exit", "s": 4}]),
        ):
            after = self.outcome()
            after["levels"][self.identity][field] = value
            self.assertEqual("changed", self.classification(self.outcome(), after))
        after = self.outcome()
        after["metadata"] = {self.identity: {"segment_count": 4}}
        self.assertTrue(c.compare(self.entry, self.outcome(), after)[0]["metadata_changed"])

    def test_missing_native_result_and_phase_failure_are_incomplete(self):
        self.assertEqual("incomplete", self.classification(self.outcome(), {}))
        for field, value in (("route_exit_code", 1), ("incomplete", "metadata_failed")):
            after = self.outcome()
            after[field] = value
            self.assertEqual("incomplete", self.classification(self.outcome(), after))
        self.assertEqual("incomplete", self.classification(self.outcome(), self.outcome("infrastructure_error")))

    def test_uncovered_and_duplicate_logical_identities(self):
        self.entry["available"] = False
        self.assertEqual("uncovered", self.classification({}, {}))
        level = {"level_num": 1, "level_file": "one.rl2"}
        with self.assertRaises(ValueError):
            c.levels({"levels": [level, level]})

    def aggregate_fixture(self, root):
        entries = [{**self.entry, "baseline": f"{i}.json", "shard": i} for i in range(4)]
        manifest = {"settings": self.settings, "entries": entries}
        for i, entry in enumerate(entries):
            folder = root / str(i)
            c.write(folder / "manifest.json", manifest)
            c.write(folder / "receipt.json", {"shard": i, "finished": True, "manifest_sha256": c.digest(manifest)})
            c.write(
                folder / "rows.json",
                [{"identity": entry["baseline"] + "|" + self.identity, "classification": "unchanged"}],
            )

    def test_aggregate_requires_all_shards_and_all_assigned_levels(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.aggregate_fixture(root)
            self.assertEqual(4, c.aggregate(root, self.settings)["unchanged"])
            c.write(root / "0/rows.json", [])
            with self.assertRaises(ValueError):
                c.aggregate(root, self.settings)
            self.aggregate_fixture(root)
            (root / "0/receipt.json").unlink()
            with self.assertRaises(ValueError):
                c.aggregate(root, self.settings)

    def test_aggregate_rejects_changed_manifest_and_duplicate_results(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.aggregate_fixture(root)
            c.write(root / "0/manifest.json", {})
            with self.assertRaises(ValueError):
                c.aggregate(root, self.settings)
            self.aggregate_fixture(root)
            rows = c.read(root / "0/rows.json")
            c.write(root / "0/rows.json", rows * 2)
            with self.assertRaises(ValueError):
                c.aggregate(root, self.settings)

    def test_manifest_preserves_missing_and_duplicate_inputs(self):
        with tempfile.TemporaryDirectory() as temp:
            base, candidate = Path(temp) / "base", Path(temp) / "candidate"
            for repo in (base, candidate):
                for name in ("one", "two", "missing"):
                    path = repo / "game_data/mission_files" / (name + ".json")
                    document = {
                        "mission_filename": name + ".mn2",
                        "levels": [{"level_num": 1, "level_file": "one.rl2", "route_input_sha256": "A" * 64}],
                    }
                    c.write(path, document)
                    c.write(path.with_suffix(".simulation.json"), document)
                    if name != "missing":
                        path.with_suffix(".zip").write_bytes(b"same archive")
            inventory = {"matches_tested_edition": True, "manifest_sha256": "retail"}
            with patch.object(c, "retail_inventory", return_value=inventory):
                result = c.manifest(base, candidate, self.settings)
                self.assertEqual(3, len(result["entries"]))
                self.assertEqual(1, sum(not e["available"] for e in result["entries"]))
                self.assertEqual([["one.json", "two.json"]], result["duplicate_archives"])
                self.assertEqual(3, len(result["duplicate_route_inputs"][0]))
                (candidate / "game_data/mission_files/one.zip").write_bytes(b"changed")
                with self.assertRaises(ValueError):
                    c.manifest(base, candidate, self.settings)

    def test_collection_uses_fresh_inputs_and_preserves_raw_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            entry = {**self.entry, "extension": ".zip"}
            baseline = root / "repo/game_data/mission_files/one.json"
            c.write(baseline, {"untouched": True})
            baseline.with_suffix(".zip").write_bytes(b"fixture")
            document = {"levels": [{"level_num": 1, "level_file": "one.rl2", "status": "timeout"}]}

            def producer(repo, script, arguments, log):
                output = Path(arguments[arguments.index("-OutputRoot") + 1])
                self.assertNotIn("-WriteRegression", arguments)
                if script == "regenerate_all_mission_metadata_host.ps1":
                    self.assertIn("-NoRegressionCopy", arguments)
                    c.write(output / "one.json", document)
                    c.write(output / "summary.json", [{"status": "passed", "metadata_json": str(output / "one.json")}])
                else:
                    scratch = Path(arguments[arguments.index("-MissionMetadataRoot") + 1])
                    self.assertEqual(document, c.read(scratch / "one.json"))
                    self.assertEqual("2", arguments[arguments.index("-Repeat") + 1])
                    self.assertEqual("1", arguments[arguments.index("-SampleFraction") + 1])
                    c.write(
                        output / "summary.json",
                        {
                            "write_regression": False,
                            "changed_files": [],
                            "selected_work_items": ["one.json|" + self.identity],
                        },
                    )
                    c.write(output / "results/one.simulation.json", document)
                    for repeat in (1, 2):
                        path = output / "results" / f"one.json_0_1_one.rl2_run_{repeat}.json"
                        c.write(path, {"status": "timeout", "frames": 123, "problem": "route stalled"})
                        c.write(Path(str(path) + ".process.json"), {"exit_code": 2, "timed_out": False})
                        c.write(
                            Path(str(path) + ".launch.json"),
                            {"arguments": ["-route-confirm-timeout-seconds", "90"], "timeout_seconds": 180},
                        )
                return 0

            with patch.object(c, "producer", side_effect=producer):
                result = c.collect(root / "repo", entry, self.settings, root / "work", root / "evidence")
            self.assertEqual(2, len(result["engine"][self.identity]))
            self.assertEqual("route stalled", result["engine"][self.identity][0]["problem"])
            self.assertEqual(90, result["budgets"][self.identity][0]["simulation_seconds"])
            self.assertEqual({"untouched": True}, c.read(baseline))

    def test_workflow_is_manual_and_artifacts_are_allowlisted(self):
        workflow = (c.ROOT / ".github/workflows/corpus-comparison.yml").read_text()
        self.assertIn("workflow_dispatch:", workflow)
        self.assertNotIn("pull_request:", workflow)
        self.assertIn("environment: retail-ci", workflow)
        self.assertIn(c.WORKFLOW, workflow)
        self.assertIn("path: evidence/**/*.json", workflow)
        self.assertNotIn("WriteRegression", workflow)
        self.assertNotIn("inputs:", workflow)


if __name__ == "__main__":
    unittest.main()
