"""Expanded cohort and bounded measurement integration controls."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import xl_readiness as readiness
import xl_readiness_expanded as expanded


class ExpandedReadinessTests(unittest.TestCase):
    def test_failed_measurement_cannot_publish_a_checkpoint_as_success(self):
        args = argparse.Namespace(publish_status=True, target_sha="a" * 40)
        with (
            patch.dict(expanded.os.environ, {"READINESS_RESULT": "failure"}),
            patch.object(expanded.reporter.candidate, "verify") as verify,
            patch.object(expanded.reporter.candidate, "status") as status,
        ):
            self.assertEqual(expanded.publish(args), 2)
            verify.assert_called_once_with(args.target_sha)
            self.assertEqual(status.call_args.args[1], "error")

    def test_frozen_coverage_preserves_originals_and_all_archives(self):
        path = expanded.HERE / "xl_readiness_inventory.json"
        frozen = expanded.cohort(path)
        self.assertEqual(frozen, json.loads((expanded.HERE / "xl_readiness_expanded_cohort.json").read_text()))
        catalog = json.loads(path.read_text())
        cases = {r["id"]: r for r in catalog["cases"]}
        ids = [r["id"] for r in frozen["selected"]]
        self.assertEqual(len(set(ids)), 64)
        self.assertEqual(ids[:8], catalog["selected"])
        self.assertEqual(len({cases[key]["archive"] for key in ids}), 23)
        self.assertTrue(
            all(r["id"] in ids for r in catalog["cases"] if Path(r["archive"]).name in readiness.NEW_CAMPAIGNS)
        )
        self.assertEqual(sum(r["reason"] == "additional secret level" for r in frozen["selected"]), 12)

    def arguments(self, root, budget=360):
        return argparse.Namespace(
            catalog=expanded.HERE / "xl_readiness_inventory.json",
            cohort=expanded.HERE / "xl_readiness_expanded_cohort.json",
            work=root / "results",
            data=root,
            assets=root,
            engine=Path(sys.executable),
            target_sha="a" * 40,
            seven_zip=None,
            budget_seconds=budget,
        )

    def test_exhausted_budget_keeps_all_unstarted_cases(self):
        with tempfile.TemporaryDirectory() as temporary:
            args = self.arguments(Path(temporary), budget=0.1)
            with (
                patch.object(readiness, "verify_resources", return_value=readiness.resource_identity()),
                patch.object(expanded.subprocess, "Popen") as launch,
            ):
                self.assertEqual(expanded.measure(args), 2)
                launch.assert_not_called()
            receipt = json.loads((args.work / "results.json").read_text())
            self.assertTrue(receipt["budget_exhausted"])
            self.assertEqual(receipt["summary"]["denominator"], 64)
            self.assertEqual(receipt["summary"]["counts"], {"untested": 64})
            self.assertIsNone(receipt["summary"]["readiness_percent"])

    def test_real_worker_receipts_keep_failures_and_separate_original_subtotal(self):
        # Real child processes emulate only the worker protocol. Original-content
        # qualification uses the actual ordinary engine in separate retained runs.
        worker = """
import json,sys
from pathlib import Path
args=sys.argv[1:]
def arg(name): return args[args.index(name)+1]
c=json.loads(Path(arg('--catalog')).read_text())
key=c['selected'][0]; row=next(r for r in c['cases'] if r['id']==key)
row.update(outcome='load_failed', assisted_navigation='not_run', frames=0, xl_active=None)
report={'schema':'dxx-xl-readiness-v1','target_sha':arg('--target-sha'),
        'asset_commit':c['asset_commit'],'selection_seed':c['selection_seed'],
        'inventory_levels':len(c['cases']),'inventory_archives':len(c['archives']),
        'resources':json.loads(sys.argv[-1]),'rows':[row]}
p=Path(arg('--work'));p.mkdir();(p/'results.json').write_text(json.dumps(report))
"""
        real_popen = subprocess.Popen

        def launch(command, **kwargs):
            return real_popen(
                [sys.executable, "-c", worker, *command[2:], json.dumps(readiness.resource_identity())], **kwargs
            )

        with tempfile.TemporaryDirectory() as temporary:
            args = self.arguments(Path(temporary))
            with (
                patch.object(readiness, "verify_resources", return_value=readiness.resource_identity()),
                patch.object(expanded.subprocess, "Popen", side_effect=launch),
            ):
                self.assertEqual(expanded.measure(args), 0)
            receipt = json.loads((args.work / "results.json").read_text())
            self.assertEqual(receipt["summary"]["counts"], {"load_failed": 64})
            self.assertEqual(receipt["summary"]["readiness_percent"], 0)
            self.assertEqual(receipt["original_eight_summary"]["denominator"], 8)
            self.assertFalse(receipt["budget_exhausted"])
            self.assertFalse(list(args.work.glob("case-*")))
            self.assertEqual(expanded.validate(receipt, args.catalog, args.cohort, args.target_sha)["passed"], 0)
            receipt["rows"].pop()
            with self.assertRaises(ValueError):
                expanded.validate(receipt, args.catalog, args.cohort, args.target_sha)

    def test_changed_selection_is_rejected_before_launch(self):
        with tempfile.TemporaryDirectory() as temporary:
            args = self.arguments(Path(temporary))
            value = json.loads(args.cohort.read_text())
            value["selected"].pop()
            args.cohort = Path(temporary) / "changed.json"
            args.cohort.write_text(json.dumps(value))
            with self.assertRaises(ValueError):
                expanded.measure(args)
            self.assertFalse(args.work.exists())

    def test_active_worker_budget_cancellation_is_not_a_compatibility_failure(self):
        real_popen = subprocess.Popen
        processes = []

        def launch(command, **kwargs):
            if command[0] == "taskkill":
                return real_popen(command, **kwargs)
            process = real_popen([sys.executable, "-c", "import time; time.sleep(30)"], **kwargs)
            processes.append(process)
            return process

        # Give the worker its normal admission window, then simulate staging
        # consuming that budget. The supervisor must terminate the live worker.
        clock = iter([0, 0, 0, 359.9, 360.1])
        with tempfile.TemporaryDirectory() as temporary:
            args = self.arguments(Path(temporary))
            with (
                patch.object(readiness, "verify_resources", return_value=readiness.resource_identity()),
                patch.object(expanded.subprocess, "Popen", side_effect=launch),
                patch.object(expanded.time, "monotonic", side_effect=lambda: next(clock, 360.1)),
            ):
                self.assertEqual(expanded.measure(args), 2)
            self.assertEqual(len(processes), 1)
            self.assertIsNotNone(processes[0].poll())
            receipt = json.loads((args.work / "results.json").read_text())
            self.assertTrue(receipt["budget_exhausted"])
            self.assertEqual(receipt["summary"]["counts"], {"untested": 64})
            self.assertFalse(list(args.work.glob("case-*")))


if __name__ == "__main__":
    unittest.main()
