"""Expanded cohort and bounded measurement integration controls."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile
from unittest.mock import patch

import xl_readiness as readiness
import xl_readiness_expanded as expanded
import xl_readiness_full as full
import xl_readiness_local_inventory as local


class ExpandedReadinessTests(unittest.TestCase):
    def test_full_survey_preserves_variant_and_secret_case_identities(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            downloads = root / "downloads"
            downloads.mkdir()
            archive, record, _, _ = self.staged_fixture(downloads)
            readiness.write(root / "pinned.json", {"archives": [record]})
            with zipfile.ZipFile(downloads / "variant.zip", "w") as package:
                package.writestr("single/control.mn2", "num_levels=1\nfirst.rl2\nnum_secrets=1\nhidden.rl2,1\n")
                package.writestr("broken.msn", "num_levels=0\n")
            index = root / "index"
            local.inventory(downloads, root / "pinned.json", index, None)
            catalog = json.loads((index / "inventory.json").read_text())
            self.assertEqual([case["level"] for case in catalog["cases"]], [1, -1])
            self.assertEqual(json.loads((index / "issues.json").read_text())[0]["descriptor"], "broken.msn")
            self.assertEqual(json.loads((index / "duplicates.json").read_text())[0]["path"], archive.name)
            selected = catalog["selected"][1]
            args = argparse.Namespace(
                catalog=index / "inventory.json",
                archive=None,
                case=[selected],
                engine=Path(sys.executable),
                assets=downloads,
                data=root,
                target_sha="a" * 40,
                work=root / "survey",
                seven_zip=None,
            )
            # A real process without engine output must produce an unsuccessful receipt
            with patch.object(readiness, "verify_resources", return_value=readiness.resource_identity()):
                self.assertEqual(full.measure(args), 1)
            receipt = json.loads((args.work / "results.json").read_text())
            self.assertEqual(receipt["selected"], [selected])
            self.assertEqual(receipt["summary"]["counts"], {"infrastructure_error": 1})
            self.assertEqual(json.loads((args.work / "failures.json").read_text()), receipt["rows"])
            self.assertEqual(receipt["rows"][0]["level_file"], "hidden.rl2")
            self.assertTrue((args.work / "0/engine.log").exists())
            import xl_readiness_full_report as full_report

            grouped = full_report.summarize(args.work, args.catalog)
            self.assertEqual(grouped["groups"], {"infrastructure_error": 1})
            self.assertEqual(grouped["failures"][0]["id"], selected)
            args.case = []
            with self.assertRaisesRegex(ValueError, "Empty or duplicate"):
                full.measure(args)
            readiness.write(args.work / "0/result.json", {**receipt["rows"][0], "frames": 120})
            with self.assertRaisesRegex(ValueError, "Case receipt differs"):
                full_report.summarize(args.work, args.catalog)

    def test_full_survey_distinguishes_a_loose_filename_proof_from_early_simulation_stop(self):
        import xl_readiness_full_report as full_report

        row = {"outcome": "simulation_failed", "level_file": "bay32.rdl"}
        proof = {"phase": "loaded_ticked", "frames": 120, "level_file": "missions/BAY32.rdl"}
        self.assertEqual(full_report.first_blocker(row, proof, ""), "Resolved level filename differs")
        self.assertEqual(
            full_report.first_blocker(row, {"phase": "simulation_failed", "frames": 82}, ""), "Simulation stopped"
        )

    def staged_fixture(self, root):
        archive = root / "fixture.zip"
        descriptor = b"d2x-name = Cache control\nnum_levels = 2\nfirst.rl2\nsecond.rl2\n"
        with zipfile.ZipFile(archive, "w") as output:
            output.writestr("missions/single/control.mn2", descriptor)
            output.writestr("missions/single/control.hog", b"original mission bytes")
            output.writestr("mods/control/textures/wall.tga", b"original package texture")
            output.writestr("missions/single/unrelated.mn2", b"num_levels = 1\nother.rl2\n")
        record = {"path": archive.name, "size": archive.stat().st_size, "sha256": readiness.digest(archive)}
        case = {
            "descriptor": "missions/single/control.mn2",
            "descriptor_sha256": readiness.hashlib.sha256(descriptor).hexdigest(),
            "level": 1,
            "level_file": "first.rl2",
        }
        cache = root / "cache"
        cache.mkdir()
        return archive, record, case, cache

    def test_cached_extraction_keeps_full_package_and_fresh_writable_cases(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive, record, case, cache = self.staged_fixture(root)
            ordinary = readiness.stage_case(root, record, case, root / "ordinary", None)
            expected = readiness.extraction_identity(ordinary)
            with patch.object(readiness, "extract_archive", wraps=readiness.extract_archive) as extract:
                first = readiness.stage_case(root, record, case, root / "first", None, cache)
                self.assertEqual(readiness.extraction_identity(first), expected)
                (first / "missions/control.hog").write_bytes(b"engine writes belong only to this case")
                (first / "mods/control/textures/wall.tga").unlink()
                second = readiness.stage_case(
                    root, record, {**case, "level": 2, "level_file": "second.rl2"}, root / "second", None, cache
                )
                self.assertEqual(readiness.extraction_identity(second), expected)
                self.assertEqual(extract.call_count, 1)
            self.assertEqual(readiness.digest(archive), record["sha256"])
            self.assertFalse((root / "first/unpack").exists())
            self.assertFalse((root / "second/unpack").exists())

    def test_linked_cached_payload_is_not_reused(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive, record, case, cache = self.staged_fixture(root)
            readiness.stage_case(root, record, case, root / "first", None, cache)
            payload = cache / record["sha256"] / "unpack/missions/single/control.hog"
            target = root / "linked-input"
            target.write_bytes(payload.read_bytes())
            payload.unlink()
            try:
                payload.symlink_to(target)
            except OSError as error:
                self.skipTest(f"Symbolic link creation unavailable: {error}")
            with self.assertRaises(ValueError):
                readiness.stage_case(root, record, case, root / "second", None, cache)

    def test_changed_source_or_cached_bytes_fail_closed(self):
        for change in ("source", "changed", "added", "missing", "identity", "partial"):
            with self.subTest(change=change), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                archive, record, case, cache = self.staged_fixture(root)
                readiness.stage_case(root, record, case, root / "first", None, cache)
                entry = cache / record["sha256"]
                payload = entry / "unpack/missions/single/control.hog"
                if change == "source":
                    archive.write_bytes(b"changed pinned archive")
                elif change == "changed":
                    payload.write_bytes(b"changed cached bytes")
                elif change == "added":
                    (entry / "unpack/extra.ham").write_bytes(b"unexpected resource")
                elif change == "missing":
                    payload.unlink()
                elif change == "identity":
                    (entry / "identity.json").write_text("{}")
                else:
                    entry.rename(entry.with_name(entry.name + ".building"))
                with patch.object(readiness, "extract_archive") as extract:
                    with self.assertRaises((ValueError, OSError)):
                        readiness.stage_case(root, record, case, root / "second", None, cache)
                    extract.assert_not_called()

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
            self.assertFalse(list(args.work.parent.glob("xl-readiness-cache-*")))
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
            self.assertFalse(list(args.work.parent.glob("xl-readiness-cache-*")))


if __name__ == "__main__":
    unittest.main()
