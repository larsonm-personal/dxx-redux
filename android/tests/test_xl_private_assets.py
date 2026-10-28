"""Asset-free tests for required XL fixtures, receipts and trusted selection."""

import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import xml.etree.ElementTree as ET
import tempfile
import unittest
import zipfile
import copy
from unittest.mock import patch

import nightly_retail as retail
import nightly_tooling_sweep as sweep
import run_xl_originals as runner
import xl_private_assets as assets


class XlPrivateTests(unittest.TestCase):
    def test_original_gpu_trace_requires_unique_real_witnesses(self):
        rows = [
            {
                "context": context,
                "visit": visit,
                "owner": owner,
                "correct": True,
                "original": owner == "a",
                "red": 228 if owner == "a" else 10,
                "path": "mods/pilot/textures/CEIL002.TGA",
            }
            for context in (0, 1)
            for visit, owner in enumerate(("a", "b", "stock", "a"))
        ]
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "d2.json"
            path.write_text(json.dumps(rows))
            assets.require_original_gpu_trace(path)
            invalid = [rows[:-1], rows + [rows[0]], [rows[0]] + rows[:-1]]
            for field, value in (
                ("original", False),
                ("correct", False),
                ("path", "global/ceil002.tga"),
                ("red", 210),
            ):
                changed = copy.deepcopy(rows)
                changed[0][field] = value
                invalid.append(changed)
            invalid.append([{**row, "original": False} for row in rows])
            for data in invalid:
                path.write_text(json.dumps(data))
                with self.assertRaises(ValueError):
                    assets.require_original_gpu_trace(path)

    def test_staging_zip_members_collisions_and_rollback(self):
        for mode in ("valid", "missing", "bad-hash", "collision", "case-collision"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary) / "source"
                destination = Path(temporary) / "stage"
                (root / "missions/d2xxl").mkdir(parents=True)
                (root / "missions/d2xxl/manifest.json").write_text("{}")
                level = struct.pack("<IiiiHH", 0x504C564C, 15, 0, 16, 0x6705, 33)
                hog = b"DHF" + struct.pack("<13si", b"level.rl2", len(level)) + level
                files = {"d2-xl.hog": hog, "d2-xl.mn2": b"name=synthetic"}

                def identity(content):
                    return {
                        "size": len(content),
                        "sha256": hashlib.sha256(content).hexdigest(),
                    }

                data = {
                    "archive": {"path": "pack.7z", **identity(b"pack")},
                    "archives": [],
                    "original_mission_archives": [{"path": "missions/D2-XL.7z", **identity(b"original")}],
                    "format_witnesses": [
                        {
                            "archive_member": "missions/single/d2-xl.hog",
                            "hog_member": "level.rl2",
                            "level_version": 15,
                            "game_version": 33,
                            **identity(level),
                        }
                    ],
                }
                (root / "pack.7z").write_bytes(b"pack")
                (root / "missions/D2-XL.7z").write_bytes(b"original")
                for index in range(2 if mode in ("collision", "case-collision") else 1):
                    name = "bundle-" + str(index) + ".zip"
                    content = b"first" if index == 0 else b"conflicting"
                    member_name = "Resource.bin" if mode == "case-collision" and index else "resource.bin"
                    with zipfile.ZipFile(root / name, "w") as archive:
                        if mode != "missing":
                            archive.writestr(member_name, content)
                    member = {"path": member_name, **identity(content)}
                    if mode == "bad-hash":
                        member["sha256"] = "0" * 64
                    data["archives"].append(
                        {
                            "path": name,
                            **identity((root / name).read_bytes()),
                            "members": [member],
                        }
                    )
                original_extract = assets.extract_archive

                def extract(source, target, seven_zip=None):
                    if source.name != "D2-XL.7z":
                        return original_extract(source, target, seven_zip=seven_zip)
                    target.mkdir()
                    for name, content in files.items():
                        (target / name).write_bytes(content)

                pins = {name: (len(content), hashlib.sha256(content).hexdigest()) for name, content in files.items()}
                with (
                    patch.object(assets, "manifest", return_value=data),
                    patch.object(assets, "D2_FILES", pins),
                    patch.object(assets, "extract_archive", side_effect=extract),
                ):
                    if mode == "valid":
                        assets.stage_inputs(root, destination, None)
                        self.assertEqual((destination / "witnesses/level.rl2").read_bytes(), level)
                        assets.verify(destination)
                        with self.assertRaises(ValueError):
                            assets.stage_inputs(root, destination, None)
                        self.assertTrue(destination.exists())
                    else:
                        with self.assertRaises(ValueError):
                            assets.stage_inputs(root, destination, None)
                        self.assertFalse(destination.exists())
                self.assertEqual((root / "pack.7z").read_bytes(), b"pack")

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

    def test_public_experimental_status_is_visible_and_sanitized(self):
        base = dict(schema=1, target_sha="a" * 40, catalog_hash="b" * 64, batch_id="test", lane="linux")
        for status in ("passed", "failed"):
            row = dict(
                id="host-profile:retail-xl-originals",
                status="PASS",
                phase="tests",
                reason="private output",
                experimental_acceptance={"xl_level_two_keys": status, "private": "must not escape"},
            )
            result = retail.public_receipt(dict(base, results=[row]))["results"][0]
            self.assertEqual(result["experimental_acceptance"], {"xl_level_two_keys": status})
            self.assertNotIn("must not escape", json.dumps(result))
            row["id"] = "unrelated"
            self.assertNotIn("experimental_acceptance", retail.public_receipt(dict(base, results=[row]))["results"][0])

    def test_experimental_outcome_survives_collection_and_reconciliation(self):
        entry = dict(
            id="host-profile:retail-xl-originals",
            owner="host-adapter",
            lane="host",
            args={"suite": "host-profile:retail-xl-originals"},
            estimated_seconds=10,
        )
        policy = {"schema": 1, "suites": [entry]}
        identity = retail.configured_identity(
            {
                "GITHUB_REPOSITORY": retail.REPOSITORY,
                "GITHUB_REF": "refs/heads/cmake",
                "GITHUB_WORKFLOW_REF": retail.WORKFLOW,
                "GITHUB_EVENT_NAME": "workflow_dispatch",
                "RETAIL_ASSETS_COMMIT": assets.ASSETS_COMMIT,
            }
        )
        plan, state = sweep.plan_batch("a" * 40, {"schema": 1, "tests": []}, policy, retail_assets=identity)
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            sweep.write(
                output / "attempt.json",
                {
                    **sweep.attempt_identity(plan, "host"),
                    "processes": {entry["id"]: {"completed": True, "exit_code": 0}},
                },
            )
            sweep.write(
                output / "host-profile_retail-xl-originals.json",
                {
                    "schema": 1,
                    "suite": entry["id"],
                    "status": "PASS",
                    "exit_code": 0,
                    "experimental_acceptance": {"xl_level_two_keys": "failed"},
                },
            )
            receipt = sweep.collect(plan, policy, "host", output)
            expected = {"xl_level_two_keys": "failed"}
            self.assertEqual(receipt["results"][0]["experimental_acceptance"], expected)
            reconciled = sweep.reconcile(plan, policy, state, [receipt])
            self.assertEqual(reconciled["records"][entry["id"]]["experimental_acceptance"], expected)

    def test_native_experimental_policy(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "native.xml"
            marker = "XL_EXPERIMENTAL_ACCEPTANCE: xl_level_two_keys route_incomplete"

            def receipt(experimental, code=8, required_failure=False):
                body = "".join(
                    '<testcase name="'
                    + name
                    + '" status="run">'
                    + ('<failure message="Failed"/>' if required_failure else "")
                    + "</testcase>"
                    for name in sorted(runner.CORE_TESTS)
                )
                path.write_text("<testsuite>" + body + experimental + "</testsuite>")
                return runner.native_receipt(path, code)

            failed = (
                '<testcase name="xl_level_two_keys" status="fail"><failure message="Failed"/>'
                "<system-out>" + marker + "</system-out></testcase>"
            )
            passed = '<testcase name="xl_level_two_keys" status="run"/>'
            self.assertEqual(receipt(failed)["experimental"], {"xl_level_two_keys": "failed"})
            self.assertEqual(receipt(passed, 0)["required_passed"], 70)
            self.assertFalse(receipt(passed, 0)["level_two_completion_qualified"])
            # CTest can exit zero when disabled tests are present
            disabled_required = ET.parse(path)
            disabled_required.find(".//testcase").set("status", "disabled")
            disabled_required.write(path)
            with self.assertRaises(ValueError):
                runner.native_receipt(path, 0)
            with self.assertRaises(ValueError):
                receipt('<testcase name="xl_level_two_keys" status="disabled"/>', 0)
            invalid = [
                "",
                failed + failed,
                failed.replace(marker, ""),
                failed.replace(marker, marker + "\n" + marker),
                '<testcase name="xl_level_two_keys" status="disabled"/>',
                '<testcase name="xl_level_two_keys" status="notrun"/>',
                passed.replace("/>", "><skipped/></testcase>"),
                passed.replace("/>", "><error/></testcase>"),
                *[
                    failed.replace('message="Failed"', 'message="' + kind + '"')
                    for kind in ("Timeout", "SEGFAULT", "BAD_COMMAND")
                ],
            ]
            for body, code, required_failure in (
                *[(body, 8, False) for body in invalid],
                (failed, 0, False),
                (passed, 8, False),
                (failed, 8, True),
            ):
                with self.subTest(body=body, code=code, required_failure=required_failure):
                    with self.assertRaises(ValueError):
                        receipt(body, code, required_failure)

    def test_private_route_failure_classification(self):
        helper = Path(__file__).resolve().parents[1] / "XL_compat/tests/experimental_acceptance.cmake"
        marker = "XL_EXPERIMENTAL_ACCEPTANCE: xl_level_two_keys route_incomplete"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            script = root / "classify.cmake"
            script.write_text(
                'include("' + helper.as_posix() + '")\nxl_report_experimental_route_failure("${RESULT}" "${FIXTURE}")\n'
            )
            blue = "Level two ordinary blue key: frames=20 alive=1 checkpoint=restored\n"
            red = "Level two route result: key=5 frames=30 dead=1 shields=-1 complete=0\n"
            error = "XL lifecycle FAILED: level-two ordinary key route incomplete\n"
            for result, output, errors, expected in (
                ("1", blue + red, error, True),
                ("Process terminated due to timeout", blue + red, error, False),
                ("Subprocess aborted", blue + red, error, False),
                ("1", blue + red, "XL lifecycle FAILED: different assertion\n", False),
                ("1", blue + red, error + error, False),
                ("1", red, error, False),
                ("1", blue, error, False),
                ("1", blue + red + red, error, False),
                ("0", blue + red, error, False),
            ):
                with self.subTest(result=result, output=output, errors=errors):
                    (root / "preview.log").write_text(output)
                    (root / "preview-error.log").write_text(errors)
                    run = subprocess.run(
                        ["cmake", "-DRESULT=" + result, "-DFIXTURE=" + root.as_posix(), "-P", str(script)],
                        capture_output=True,
                        text=True,
                        timeout=10,
                        check=True,
                    )
                    self.assertEqual(marker in run.stderr, expected)

    def test_actual_ctest_nonexecution_and_timeout_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "CTestTestfile.cmake").write_text(
                "add_test(timeout cmake -E sleep 2)\nset_tests_properties(timeout PROPERTIES TIMEOUT 0.1)\n"
                "add_test(disabled cmake -E true)\nset_tests_properties(disabled PROPERTIES DISABLED TRUE)\n"
                'add_test(missing "' + (root / "absent-command").as_posix() + '")\n'
            )
            report = root / "ctest.xml"
            run = subprocess.run(
                ["ctest", "--test-dir", str(root), "--output-junit", str(report)], capture_output=True, timeout=15
            )
            self.assertEqual(run.returncode, 8)
            actual = ET.parse(report).getroot().findall(".//testcase")
            self.assertEqual({c.get("name") for c in actual}, {"timeout", "disabled", "missing"})
            for case in actual:
                with self.subTest(kind=case.get("name")):
                    case.set("name", "xl_level_two_keys")
                    suite = ET.Element("testsuite")
                    for name in runner.CORE_TESTS:
                        ET.SubElement(suite, "testcase", name=name, status="run")
                    suite.append(case)
                    ET.ElementTree(suite).write(report)
                    with self.assertRaises(ValueError):
                        runner.native_receipt(report, 8)

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
                plan, _ = sweep.plan_batch(
                    "a" * 40,
                    catalog,
                    policy,
                    retail_assets=retail.configured_identity(),
                )
                self.assertFalse(assets.selected(plan))
                with self.assertRaises(ValueError):
                    sweep.select_xl_originals(plan, policy, catalog)
        self.assertTrue(retail.corpus_available({"commit": assets.ASSETS_COMMIT}))
        self.assertTrue(retail.demos_available({"commit": assets.ASSETS_COMMIT}))


if __name__ == "__main__":
    unittest.main()
