"""Asset-free tests for required XL fixtures, receipts and trusted selection."""

import hashlib
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

import nightly_retail as retail
import nightly_tooling_sweep as sweep
import run_xl_originals as runner
import xl_private_assets as assets


class XlPrivateTests(unittest.TestCase):
    def test_hash_missing_tamper_and_links_fail(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "input"
            record = {"size": 3, "sha256": hashlib.sha256(b"abc").hexdigest()}
            with self.assertRaises((ValueError, OSError)):
                assets.checked(source, record)
            source.write_bytes(b"abc")
            assets.checked(source, record)
            source.write_bytes(b"abd")
            with self.assertRaises(ValueError):
                assets.checked(source, record)
            if os.name != "nt":
                link = root / "linked"
                link.symlink_to(source)
                with self.assertRaises(ValueError):
                    assets.hog_member(link, "level.rl2")

    def test_hog_bounds_duplicate_paths_and_missing_members(self):
        def member(name, content=b"abc", size=None):
            return struct.pack("<13si", name.encode(), len(content) if size is None else size) + content

        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "fixture.hog"
            path.write_bytes(b"DHF" + member("level.rl2"))
            self.assertEqual(assets.hog_member(path, "level.rl2"), b"abc")
            invalid = [
                b"DHF" + member("level.rl2", size=4),
                b"DHF" + member("level.rl2", size=-1),
                b"DHF" + member("../x"),
                b"DHF" + member("level.rl2") + member("LEVEL.RL2"),
                b"DHFx",
            ]
            for content in invalid:
                path.write_bytes(content)
                with self.assertRaises(ValueError):
                    assets.hog_member(path, "level.rl2")
            path.write_bytes(b"DHF" + member("other.rl2"))
            with self.assertRaises(ValueError):
                assets.hog_member(path, "level.rl2")

    def test_manifest_identity_and_complete_pairs_required(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "manifest.json"
            data = {
                "missing": [],
                "mismatched": [],
                "format_witnesses": [{"level_version": a, "game_version": b} for a, b in sorted(assets.PAIRS)],
            }
            path.write_text(json.dumps(data))
            with self.assertRaises(ValueError):
                assets.manifest(path)
            with patch.object(assets, "MANIFEST_SHA256", hashlib.sha256(path.read_bytes()).hexdigest()):
                self.assertEqual(assets.manifest(path), data)
            data["format_witnesses"].pop()
            path.write_text(json.dumps(data))
            with patch.object(assets, "MANIFEST_SHA256", hashlib.sha256(path.read_bytes()).hexdigest()):
                with self.assertRaises(ValueError):
                    assets.manifest(path)

    def test_receipt_requires_exact_non_skipped_execution(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "junit.xml"
            path.write_text('<testsuite><testcase name="required"/></testsuite>')
            runner.require_junit(path, {"required"})
            for body in (
                "",
                '<testcase name="other"/>',
                '<testcase name="required"/><testcase name="required"/>',
                *('<testcase name="required"><' + tag + "/></testcase>" for tag in ("skipped", "failure", "error")),
            ):
                path.write_text("<testsuite>" + body + "</testsuite>")
                with self.assertRaises(ValueError):
                    runner.require_junit(path, {"required"})

    def test_private_campaign_requires_trust_pin_and_both_owners(self):
        policy = sweep.read(Path(__file__).with_name("nightly_tooling_policy.json"))
        catalog = {
            "schema": 1,
            "tests": [{"name": e["args"]["suite"]} for e in policy["suites"] if e["owner"] == "master"],
        }
        env = {
            "GITHUB_REPOSITORY": retail.REPOSITORY,
            "GITHUB_REF": "refs/heads/cmake",
            "GITHUB_WORKFLOW_REF": retail.WORKFLOW,
            "GITHUB_EVENT_NAME": "workflow_dispatch",
            "RETAIL_ASSETS_COMMIT": assets.ASSETS_COMMIT,
        }
        with patch.dict(os.environ, env, clear=True):
            plan, _ = sweep.plan_batch("a" * 40, catalog, policy, retail_assets=retail.configured_identity())
            chosen = sweep.select_xl_originals(plan, policy, catalog)
            self.assertEqual({e["id"] for e in chosen["tests"]}, assets.OWNER_IDS)
            self.assertEqual(set(chosen["lanes"]), {"host", "android-single"})
            with self.assertRaises(ValueError):
                sweep.select_xl_originals(plan, policy, {"tests": []})
            with patch.dict(os.environ, {"GITHUB_EVENT_NAME": "pull_request"}):
                with self.assertRaises(ValueError):
                    sweep.select_xl_originals(plan, policy, catalog)
            with patch.dict(os.environ, {"RETAIL_ASSETS_COMMIT": "b" * 40}):
                plan, _ = sweep.plan_batch("a" * 40, catalog, policy, retail_assets=retail.configured_identity())
                self.assertFalse(assets.selected(plan))
                with self.assertRaises(ValueError):
                    sweep.select_xl_originals(plan, policy, catalog)
        self.assertTrue(retail.corpus_available({"commit": assets.ASSETS_COMMIT}))
        self.assertTrue(retail.demos_available({"commit": assets.ASSETS_COMMIT}))


if __name__ == "__main__":
    unittest.main()
