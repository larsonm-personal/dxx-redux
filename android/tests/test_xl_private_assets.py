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
import run_xl_navigation_smoke as smoke
import public_contracts as public
import pilot_archive as pilot
import xl_private_assets as assets


class XlPrivateTests(unittest.TestCase):
    def test_reviewed_cohorts_are_independent_of_candidate_requirements(self):
        self.assertEqual(len(runner.HISTORICAL_FAST_TESTS), 111)
        self.assertEqual(
            hashlib.sha256("\n".join(sorted(runner.HISTORICAL_FAST_TESTS)).encode()).hexdigest(),
            "d3a911b9012339b3052d1e7d0b1dbc12c604f7fdef2f9c112253ece1ce1deb86",
        )
        self.assertEqual(len(runner.REVIEWED_FAST_TESTS), 113)
        self.assertEqual(runner.FAST_TESTS, runner.REVIEWED_FAST_TESTS)
        self.assertEqual(runner.REVIEWED_FAST_TESTS - runner.HISTORICAL_FAST_TESTS, runner.REVIEWED_FAST_ADDITIONS)
        for changed in (set(), runner.HISTORICAL_FAST_TESTS, runner.FAST_TESTS | {"unreviewed"}):
            with patch.object(runner, "FAST_TESTS", changed):
                for required in (runner.HISTORICAL_FAST_TESTS, runner.REVIEWED_FAST_TESTS):
                    receipt = {"required_tests": sorted(required), "required_passed": len(required)}
                    self.assertEqual(runner.reviewed_fast_cohort(receipt), required)
                with self.assertRaises(ValueError):
                    runner.reviewed_fast_cohort(
                        {"required_tests": sorted(runner.REVIEWED_FAST_TESTS | {"unreviewed"}), "required_passed": 114}
                    )

    def test_controller_accepts_only_complete_reviewed_fast_cohorts(self):
        additions = {"xl_changing_lights_invalid_rows", "xl_flickers_invalid_rows"}
        self.assertEqual(retail.safe_xl_native_failures(sorted(additions) + ["unknown"]), sorted(additions))
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "native.xml"
            for required in (runner.HISTORICAL_FAST_TESTS, runner.REVIEWED_FAST_TESTS):
                receipt = {
                    "required_tests": sorted(required),
                    "required_passed": len(required),
                    "experimental": {},
                    "level_two_completion_qualified": False,
                    "camera_subchecks": "executed",
                    "format_witnesses": 16,
                    "modern_source_levels": 3,
                    "navigation_smoke_passed": 1,
                    "navigation_anchors_passed": 4,
                    "navigation_smoke_milliseconds": 800,
                }
                root = ET.Element("testsuite")
                for name in sorted(required):
                    ET.SubElement(root, "testcase", name=name, status="run")
                ET.ElementTree(root).write(path)
                self.assertEqual(
                    runner.validate_controller_summary(receipt, path, fast=True)["required_passed"], len(required)
                )
                from nightly_pr_candidate import positive_receipt

                checks = runner.validate_controller_summary(receipt, path, fast=True)
                row = {"id": "host-profile:retail-xl-fast", "status": "PASS", "xl_checks": checks}
                clean = retail.public_diagnostics(row)
                published = {"target_sha": "a" * 40, "results": [{**row, **clean}]}
                self.assertEqual(clean["xl_checks"]["required_tests"], sorted(required))
                self.assertEqual(positive_receipt(published, "a" * 40), 800)
                for status in ("FAIL", "SKIP", "BLOCKED", "TIMEOUT"):
                    with patch.dict(published["results"][0], {"status": status}), self.assertRaises(ValueError):
                        positive_receipt(published, "a" * 40)
                for change in (
                    {"required_tests": sorted(required)[1:]},
                    {"required_tests": sorted(required) + ["unknown"]},
                    {"required_tests": sorted(required) + [sorted(required)[0]]},
                    {"required_tests": None},
                    {"required_tests": [None]},
                    {"required_passed": 113 if len(required) == 111 else 111},
                    {"required_passed": float(len(required))},
                    *(
                        {"required_tests": sorted(runner.HISTORICAL_FAST_TESTS | {name}), "required_passed": 112}
                        for name in additions
                    ),
                ):
                    changed = {**row, "xl_checks": {**checks, **change}}
                    sanitized = retail.public_diagnostics(changed)
                    self.assertNotIn("required_tests", sanitized.get("xl_checks", {}))
                    with self.subTest(publisher=change), self.assertRaises(ValueError):
                        positive_receipt({"target_sha": "a" * 40, "results": [{**changed, **sanitized}]}, "a" * 40)
                if required != runner.FAST_TESTS:
                    with self.assertRaises(ValueError):
                        runner.validate_summary(receipt, fast=True)
                invalid = [
                    {"required_tests": sorted(required) + ["unknown"]},
                    {"required_tests": sorted(required) + [sorted(required)[0]]},
                    {"required_tests": sorted(required)[1:], "required_passed": len(required) - 1},
                    {"required_tests": [None]},
                    {"required_tests": "malformed"},
                    {"required_passed": float(len(required))},
                    {"required_passed": str(len(required))},
                    {"required_passed": True},
                    {"required_passed": len(required) - 1},
                    {"experimental": {"unknown": "passed"}},
                    {"experimental": []},
                    {"level_two_completion_qualified": True},
                    {"camera_subchecks": "missing"},
                    {"format_witnesses": 15},
                    {"modern_source_levels": 2},
                    {"navigation_smoke_passed": 0},
                    {"navigation_anchors_passed": 3},
                    {"navigation_smoke_milliseconds": -1},
                    {"navigation_smoke_milliseconds": 30001},
                ]
                invalid += [
                    {key: value}
                    for key in (
                        "format_witnesses",
                        "modern_source_levels",
                        "navigation_smoke_passed",
                        "navigation_anchors_passed",
                        "navigation_smoke_milliseconds",
                    )
                    for value in (True, "1", float(receipt[key]), None)
                ]
                invalid += [
                    {"required_tests": sorted(runner.HISTORICAL_FAST_TESTS | {name}), "required_passed": 112}
                    for name in additions
                ]
                for change in invalid:
                    with self.subTest(count=len(required), change=change), self.assertRaises(ValueError):
                        runner.validate_controller_summary({**receipt, **change}, path, fast=True)
                for mode in ("failure", "error", "skipped", "disabled", "missing", "duplicate", "unknown"):
                    changed = copy.deepcopy(root)
                    if mode in ("failure", "error", "skipped"):
                        ET.SubElement(changed[0], mode)
                    elif mode == "disabled":
                        changed[0].set("status", "notrun")
                    elif mode == "missing":
                        changed.remove(changed[0])
                    elif mode == "duplicate":
                        changed.append(copy.deepcopy(changed[0]))
                    else:
                        changed[0].set("name", "unknown")
                    ET.ElementTree(changed).write(path)
                    with self.subTest(count=len(required), mode=mode), self.assertRaises(ValueError):
                        runner.validate_controller_summary(receipt, path, fast=True)

    def test_failed_preview_build_retains_only_fixed_stage(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            work = root / "work"
            failure = subprocess.CalledProcessError(2, ["cmake", "private-path"])
            with (
                patch("sys.argv", ["runner", "--repo", str(root), "--work", str(work), "--fast"]),
                patch.object(runner, "verify", return_value={}),
                patch.object(runner.subprocess, "run", side_effect=[subprocess.CompletedProcess([], 0), failure]),
            ):
                with self.assertRaises(subprocess.CalledProcessError):
                    runner.main()
            progress = work / "xl-progress.json"
            self.assertEqual(retail.xl_stage(progress), "preview-build")
            row = {"id": "host-profile:retail-xl-fast", "xl_stage": retail.xl_stage(progress)}
            self.assertEqual(retail.public_diagnostics(row)["xl_stage"], "preview-build")
            for bad in ("private-path", ["preview-build"], None):
                progress.write_text(json.dumps({"stage": bad}))
                self.assertIsNone(retail.xl_stage(progress))
                self.assertNotIn("xl_stage", retail.public_diagnostics({**row, "xl_stage": bad}))
            progress.write_text("x" * 1025)
            self.assertIsNone(retail.xl_stage(progress))
            progress.unlink()
            self.assertIsNone(retail.xl_stage(progress))
            other = root / "other.json"
            other.write_text(json.dumps({"stage": "preview-build"}))
            try:
                progress.symlink_to(other)
            except OSError:
                return  # Symlink creation may require elevated Windows privileges
            self.assertIsNone(retail.xl_stage(progress))

    def test_native_failure_diagnostics_exclude_private_junit_content(self):
        with tempfile.TemporaryDirectory() as temporary:
            report = Path(temporary) / "original-ctest.xml"
            root = ET.Element("testsuite")
            for name, status, child in (
                ("xl_pilot_archive", "run", None),
                ("xl_fog_gpu", "fail", "failure"),
                ("xl_pog_gpu", "notrun", "skipped"),
                ("PRIVATE_NAME", "fail", "failure"),
            ):
                case = ET.SubElement(root, "testcase", name=name, status=status)
                ET.SubElement(case, "system-out").text = "PRIVATE_OUTPUT"
                if child:
                    ET.SubElement(case, child, message="PRIVATE_MESSAGE")
            ET.ElementTree(root).write(report)
            expected = ["xl_fog_gpu", "xl_pog_gpu"]
            self.assertEqual(retail.xl_native_failures(report), expected)
            row = {"id": "host-profile:retail-xl-fast", "xl_native_failures": expected + ["PRIVATE_NAME", {}]}
            clean = retail.public_diagnostics(row)
            self.assertEqual(clean["xl_native_failures"], expected)
            self.assertNotIn("PRIVATE", json.dumps(clean))
            self.assertEqual(retail.safe_xl_native_failures("xl_fog_gpu"), [])
            self.assertEqual(retail.safe_xl_native_failures(["xl_fog_gpu"] * 1000), [])
            for bad in (b"broken", b"<!DOCTYPE testsuite><testsuite/>", b"x" * (16 * 1024 * 1024 + 1)):
                report.write_bytes(bad)
                self.assertEqual(retail.xl_native_failures(report), [])
            report.unlink()
            self.assertEqual(retail.xl_native_failures(report), [])
            other = Path(temporary) / "private.xml"
            ET.ElementTree(root).write(other)
            try:
                report.symlink_to(other)
            except OSError:
                return
            self.assertEqual(retail.xl_native_failures(report), [])

    def test_original_pilot_inventory_remains_complete_and_unqualified(self):
        levels = [
            {"name": f"fixture-{i}.rl2", "inventory": {"status": "inspection_only", "playable": False}}
            for i in range(30)
        ]
        levels[0]["name"] = "d2leva-1.rl2"
        levels[0]["inventory"].update({key: [0] * count for key, count in pilot.PILOT_COUNTS.items()})
        report = {"playable": False, "levels": levels}
        pilot.validate_inventory(report)
        for key in pilot.PILOT_COUNTS:
            changed = copy.deepcopy(report)
            changed["levels"][0]["inventory"][key].pop()
            with self.subTest(count=key), self.assertRaises(ValueError):
                pilot.validate_inventory(changed)
        for index in range(30):
            for field, value in (("status", "rejected"), ("playable", True), ("playable", None)):
                changed = copy.deepcopy(report)
                changed["levels"][index]["inventory"][field] = value
                with self.subTest(level=index, field=field, value=value), self.assertRaises(ValueError):
                    pilot.validate_inventory(changed)
        for mode in ("missing", "extra", "duplicate-pilot", "absent-pilot", "qualified"):
            changed = copy.deepcopy(report)
            if mode == "missing":
                changed["levels"].pop()
            elif mode == "extra":
                changed["levels"].append(copy.deepcopy(levels[-1]))
            elif mode == "duplicate-pilot":
                changed["levels"][1] = copy.deepcopy(levels[0])
            elif mode == "absent-pilot":
                changed["levels"][0]["name"] = "other.rl2"
            else:
                changed["playable"] = True
            with self.subTest(mode=mode), self.assertRaises(ValueError):
                pilot.validate_inventory(changed)

    def test_public_source_selection_rejects_omissions_private_tests_and_skips(self):
        self.assertEqual(len(public.PUBLIC_TESTS), 49)
        self.assertEqual(
            public.GPU_TESTS, {"xl_fog_gpu", "xl_fog_gpu_direct", "xl_fog_gpu_copy16_fallback", "xl_pog_gpu"}
        )
        oof_public = {"xl_oof_metadata_reference", "xl_oof_render_reference", "xl_oof_source_pins"}
        self.assertEqual(public.PUBLIC_TESTS - runner.FAST_TESTS, oof_public)
        self.assertEqual(len(runner.FAST_TESTS), 113)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            oracle = root / "oracle"
            oracle.write_bytes(b"fixture executable")
            reference = root / "reference"
            for name in ("models/oofread.cpp", "render/oofrender.cpp", "3d/oof2model.cpp"):
                path = reference / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("Fixture path only; the CMake pin controls verify actual source bytes")
            report = root / "public.xml"
            for gpu, audio in ((False, False), (True, False), (False, True), (True, True)):
                required = (
                    public.PUBLIC_TESTS
                    | (public.GPU_TESTS if gpu else set())
                    | (public.AUDIO_TESTS if audio else set())
                )
                listing = {
                    "tests": [
                        {
                            "name": name,
                            "command": ["cmake"]
                            + [
                                "-D" + argument + "=" + str(oracle)
                                for argument in runner.SOURCE_ARGUMENTS.get(name, ())
                            ],
                        }
                        for name in sorted(required)
                    ]
                }
                pin = next(test for test in listing["tests"] if test["name"] == "xl_oof_source_pins")
                pin["command"].append("-DREFERENCE=" + str(reference))
                public.validate_registration(listing, gpu=gpu, audio=audio)
                for name in oof_public:
                    for mode in ("omitted", "duplicate", "same-length-duplicate"):
                        changed = copy.deepcopy(listing)
                        selected = next(test for test in changed["tests"] if test["name"] == name)
                        if mode == "omitted":
                            changed["tests"].remove(selected)
                        elif mode == "duplicate":
                            changed["tests"].append(copy.deepcopy(selected))
                        else:
                            changed["tests"][0] = copy.deepcopy(selected)
                        with self.subTest(name=name, mode=mode), self.assertRaises(ValueError):
                            public.validate_registration(changed, gpu=gpu, audio=audio)
                for mode in ("omitted", "empty", "absent", "file", "incomplete", "duplicate"):
                    changed = copy.deepcopy(listing)
                    selected = next(test for test in changed["tests"] if test["name"] == "xl_oof_source_pins")
                    selected["command"] = ["cmake"]
                    if mode != "omitted":
                        value = {
                            "empty": "",
                            "absent": str(root / "absent"),
                            "file": str(oracle),
                            "incomplete": str(root),
                            "duplicate": str(reference),
                        }[mode]
                        selected["command"].append("-DREFERENCE=" + value)
                        if mode == "duplicate":
                            selected["command"].append("-DREFERENCE=" + value)
                    with self.subTest(reference=mode), self.assertRaises(ValueError):
                        public.validate_registration(changed, gpu=gpu, audio=audio)
                for tests in (
                    listing["tests"][1:],
                    listing["tests"] + [{"name": "xl_preview_lifecycle"}],
                    listing["tests"] + [listing["tests"][0]],
                ):
                    with self.assertRaises(ValueError):
                        public.validate_registration({"tests": tests}, gpu=gpu, audio=audio)
                missing = copy.deepcopy(listing)
                for test in missing["tests"]:
                    if test["name"] in runner.SOURCE_ARGUMENTS:
                        test["command"] = ["cmake"]
                with self.assertRaises(ValueError):
                    public.validate_registration(missing, gpu=gpu, audio=audio)
                suite = ET.Element("testsuite")
                for name in sorted(required):
                    case = ET.SubElement(suite, "testcase", name=name, status="run")
                if name == "xl_projectiles":
                    ET.SubElement(case, "system-out").text = "XL_CAMERA_SUBCHECKS: not_run_missing_addon"
                ET.ElementTree(suite).write(report)
                self.assertEqual(public.require_result(report, 0, gpu=gpu, audio=audio), len(required))
                with self.assertRaises(ValueError):
                    public.require_result(report, 8, gpu=gpu, audio=audio)
                for name in oof_public:
                    for mode in ("missing", "duplicate", "same-length-duplicate", "skipped", "failure", "error"):
                        changed = copy.deepcopy(suite)
                        selected = next(case for case in changed if case.get("name") == name)
                        if mode == "missing":
                            changed.remove(selected)
                        elif mode == "duplicate":
                            changed.append(copy.deepcopy(selected))
                        elif mode == "same-length-duplicate":
                            changed[0] = copy.deepcopy(selected)
                        else:
                            ET.SubElement(selected, mode)
                        ET.ElementTree(changed).write(report)
                        with self.subTest(name=name, mode=mode), self.assertRaises(ValueError):
                            public.require_result(report, 0, gpu=gpu, audio=audio)
                for mode in ("missing", "duplicate", "disabled", "skipped", "failure", "error"):
                    changed = copy.deepcopy(suite)
                    if mode == "missing":
                        changed.remove(changed[0])
                    elif mode == "duplicate":
                        changed.append(copy.deepcopy(changed[0]))
                    elif mode == "disabled":
                        changed[0].set("status", "disabled")
                    else:
                        ET.SubElement(changed[0], mode)
                    ET.ElementTree(changed).write(report)
                    with self.subTest(gpu=gpu, mode=mode), self.assertRaises(ValueError):
                        public.require_result(report, 0, gpu=gpu, audio=audio)

    def test_fast_registration_requires_built_source_oracles(self):
        with tempfile.TemporaryDirectory() as temporary:
            oracle = Path(temporary) / "oracle"
            oracle.write_bytes(b"fixture executable")
            listing = {
                "tests": [
                    {
                        "name": name,
                        "command": ["cmake"]
                        + ["-D" + argument + "=" + str(oracle) for argument in runner.SOURCE_ARGUMENTS.get(name, ())],
                    }
                    for name in sorted(runner.FAST_TESTS)
                ]
            }
            runner.validate_registration(listing, fast=True)
            self.assertTrue(set(runner.SOURCE_ARGUMENTS) <= runner.FAST_TESTS)
            for test in listing["tests"]:
                for argument in runner.SOURCE_ARGUMENTS.get(test["name"], ()):
                    original = test["command"][:]
                    prefix = "-D" + argument + "="
                    for mode in ("omitted", "empty", "absent", "duplicate"):
                        test["command"] = [value for value in original if not value.startswith(prefix)]
                        if mode != "omitted":
                            test["command"].append(
                                prefix
                                + {"empty": "", "absent": str(oracle) + "-absent", "duplicate": str(oracle)}[mode]
                            )
                        if mode == "duplicate":
                            test["command"].append(prefix + str(oracle))
                        with (
                            self.subTest(name=test["name"], argument=argument, mode=mode),
                            self.assertRaises(ValueError),
                        ):
                            runner.validate_registration(listing, fast=True)
                    test["command"] = original
            for tests in (listing["tests"][1:], listing["tests"] + [listing["tests"][0]]):
                with self.assertRaises(ValueError):
                    runner.validate_registration({"tests": tests}, fast=True)

    def test_fast_receipts_require_every_executed_control_and_no_campaign_exemption(self):
        self.assertEqual(len(runner.FAST_TESTS), 113)
        self.assertFalse(runner.FAST_TESTS & runner.CAMPAIGN_TESTS)
        self.assertTrue(
            {
                "xl_modern_runtime_layouts",
                "xl_preview_lifecycle",
                "xl_stock_loader_probe",
                "xl_fog_gpu",
                "xl_fog_gpu_direct",
                "xl_fog_gpu_copy16_fallback",
                "xl_pog_native",
                "xl_pog_gpu",
                "xl_boss_groups",
                "xl_modern_generators_27",
                "xl_level_messages",
                "xl_level_text_contract",
                "xl_level_text_reference",
                "xl_texture_actions",
                "xl_countdown_runtime",
                "xl_disarm_runtime",
            }
            <= runner.FAST_TESTS
        )
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "native.xml"
            suite = ET.Element("testsuite")
            for name in sorted(runner.FAST_TESTS):
                case = ET.SubElement(suite, "testcase", name=name, status="run")
                if name == "xl_projectiles":
                    ET.SubElement(case, "system-out").text = "XL_CAMERA_SUBCHECKS: not_run_missing_addon"
            ET.ElementTree(suite).write(path)
            receipt = runner.native_receipt(path, 0, fast=True)
            receipt.update(
                required_tests=sorted(runner.FAST_TESTS),
                format_witnesses=16,
                modern_source_levels=3,
                navigation_smoke_passed=1,
                navigation_anchors_passed=4,
                navigation_smoke_milliseconds=800,
            )
            self.assertEqual(runner.validate_summary(receipt, fast=True)["required_passed"], 113)
            for mode in ("omitted", "duplicate", "disabled", "skipped", "failed", "experimental"):
                changed = copy.deepcopy(suite)
                first = changed[0]
                if mode == "omitted":
                    changed.remove(first)
                elif mode == "duplicate":
                    changed.append(copy.deepcopy(first))
                elif mode == "disabled":
                    first.set("status", "disabled")
                elif mode in ("skipped", "failed"):
                    ET.SubElement(first, "skipped" if mode == "skipped" else "failure", message="Failed")
                else:
                    ET.SubElement(changed, "testcase", name="xl_level_two_keys", status="run")
                ET.ElementTree(changed).write(path)
                with self.subTest(mode=mode), self.assertRaises(ValueError):
                    runner.native_receipt(path, 0, fast=True)
            for key, value in (
                ("required_passed", 40),
                ("required_tests", sorted(runner.FAST_TESTS)[1:]),
                ("experimental", {"xl_level_two_keys": "failed"}),
                ("format_witnesses", 9),
                ("modern_source_levels", 2),
                ("navigation_smoke_passed", 0),
                ("navigation_smoke_milliseconds", -1),
                ("navigation_smoke_milliseconds", 30001),
                ("navigation_smoke_milliseconds", "private"),
            ):
                with self.subTest(field=key), self.assertRaises(ValueError):
                    runner.validate_summary({**receipt, key: value}, fast=True)

    def test_format21_requires_strict_inspection_without_playability(self):
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            witness = {"level_version": 21, "game_version": 40, "hog_member": "test.rl2"}
            for mode in ("decoded", "unsupported", "malformed", "wrong-version", "nonzero", "playable", "empty"):
                with self.subTest(mode=mode):
                    inventory = {"status": "inspection_only", "level_version": 21, "game_version": 40}
                    if mode in ("unsupported", "malformed"):
                        inventory = {
                            "status": "rejected",
                            "reason": "unsupported level/game version pair 21/40"
                            if mode == "unsupported"
                            else "invalid packed light index",
                        }
                    if mode == "wrong-version":
                        inventory["level_version"] = 23

                    def inspect(command, **kwargs):
                        Path(command[2]).write_text(
                            json.dumps(
                                {
                                    "playable": mode == "playable",
                                    "levels": [] if mode == "empty" else [{"inventory": inventory}],
                                }
                            )
                        )
                        return subprocess.CompletedProcess(
                            command, 1 if mode in ("unsupported", "malformed", "nonzero") else 0
                        )

                    with patch.object(runner.subprocess, "run", side_effect=inspect):
                        if mode == "decoded":
                            self.assertEqual(
                                runner.format_results(Path("inspect"), work, work, {"format_witnesses": [witness]}), 1
                            )
                        else:
                            with self.assertRaises(ValueError):
                                runner.format_results(Path("inspect"), work, work, {"format_witnesses": [witness]})

    def test_format34_preserves_the_original_packed_light_rejection(self):
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            witness = {"level_version": 15, "game_version": 34, "hog_member": "test.rl2"}
            for mode in ("decoded", "rejected", "unsupported-format", "playable"):
                inventory = {"status": "inspection_only", "level_version": 15, "game_version": 34}
                if mode != "decoded":
                    inventory = {
                        "status": "rejected",
                        "reason": "unsupported level/game version pair 15/34"
                        if mode == "unsupported-format"
                        else "invalid packed light index 2 in 15/34: segment=63 side=0 count=2592 first=37 available=1284",
                    }

                def inspect(command, **kwargs):
                    Path(command[2]).write_text(
                        json.dumps({"playable": mode == "playable", "levels": [{"inventory": inventory}]})
                    )
                    return subprocess.CompletedProcess(command, 0 if mode == "decoded" else 1)

                with patch.object(runner.subprocess, "run", side_effect=inspect):
                    if mode == "rejected":
                        self.assertEqual(
                            runner.format_results(Path("inspect"), work, work, {"format_witnesses": [witness]}), 1
                        )
                    else:
                        with self.assertRaises(ValueError):
                            runner.format_results(Path("inspect"), work, work, {"format_witnesses": [witness]})

    def test_modern_original_outcomes_cannot_drop_rejections_or_source_comparisons(self):
        decoded = {
            "boilpnt.rl2": (23, 1241),
            "control2.rl2": (27, 3389),
            "canyon.rl2": (27, 2139),
            "canyon2.rl2": (27, 2125),
            "watrwrks.rl2": (27, 3188),
        }
        rows = [
            {
                "name": name,
                "inventory": {
                    "status": "inspection_only",
                    "level_version": version,
                    "game_version": 40,
                    "segments": [None] * segments,
                },
            }
            for name, (version, segments) in decoded.items()
        ]
        reactor = next(row["inventory"] for row in rows if row["name"] == "control2.rl2")
        reactor.update(playable=False, reactor_authored_count=26, reactor_targets=[[0, 0] for _ in range(10)])
        rows += [
            {"name": name, "inventory": {"status": "rejected", "reason": reason}}
            for name, reason in {
                "control.rl2": "invalid packed light index 0",
                "subzero.rl2": "offset 923615: unsupported movement layout 255 at object 4",
            }.items()
        ]
        for mode in (
            "valid",
            "missing-rejection",
            "duplicate",
            "changed-version",
            "wrong-rejection",
            "empty-source",
            "reactor-old-rejection",
            "reactor-raw",
            "reactor-short",
            "reactor-target-changed",
            "reactor-bool",
            "reactor-dict",
            "reactor-playable",
            "reactor-empty-source",
            "reactor-source-failure",
        ):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as temporary:
                work = Path(temporary)
                current = copy.deepcopy(rows)
                if mode == "missing-rejection":
                    current.pop()
                elif mode == "duplicate":
                    current[-1] = current[-2]
                elif mode == "changed-version":
                    current[1]["inventory"]["level_version"] = 25
                elif mode == "wrong-rejection":
                    current[-1]["inventory"]["reason"] = "unsupported level version 27"
                reactor = next(row["inventory"] for row in current if row["name"] == "control2.rl2")
                if mode == "reactor-old-rejection":
                    reactor.update(status="rejected", reason="invalid reactor trigger count")
                elif mode == "reactor-raw":
                    reactor["reactor_authored_count"] = 10
                elif mode == "reactor-short":
                    reactor["reactor_targets"].pop()
                elif mode == "reactor-target-changed":
                    reactor["reactor_targets"][5] = [0, 1]
                elif mode == "reactor-bool":
                    reactor["reactor_targets"][0][0] = False
                elif mode == "reactor-dict":
                    reactor["reactor_targets"] = {}
                elif mode == "reactor-playable":
                    reactor["playable"] = True
                calls = []

                def execute(command, **kwargs):
                    calls.append(command)
                    if len(command) == 3:
                        Path(command[2]).write_text(json.dumps({"playable": False, "levels": current}))
                        return subprocess.CompletedProcess(command, 1)
                    if mode == "reactor-source-failure" and command[3] == "control2.rl2":
                        raise subprocess.CalledProcessError(1, command)
                    segments = decoded[command[3]][1]
                    output = (
                        ""
                        if mode == "empty-source" or (mode == "reactor-empty-source" and command[3] == "control2.rl2")
                        else f"1 levels, {segments} segments, {segments * 6} sides, 10000 comparisons; 5000 vertex brightness maxima"
                    )
                    return subprocess.CompletedProcess(command, 0, stdout=output)

                with patch.object(runner.subprocess, "run", side_effect=execute):
                    if mode == "valid":
                        self.assertEqual(runner.modern_results(Path("inspect"), Path("reference"), work, work), (7, 3))
                        self.assertEqual(len(calls), 5)
                        self.assertEqual(
                            {command[3] for command in calls[1:]},
                            {"canyon.rl2", "canyon2.rl2", "watrwrks.rl2", "control2.rl2"},
                        )
                    else:
                        with self.assertRaises((ValueError, subprocess.CalledProcessError)):
                            runner.modern_results(Path("inspect"), Path("reference"), work, work)

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
        for mode in ("valid", "missing", "bad-hash", "collision", "case-collision", "valid-modern", "bad-modern"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary) / "source"
                destination = Path(temporary) / "stage"
                (root / "missions/d2xxl").mkdir(parents=True)
                (root / "missions/d2xxl/manifest.json").write_text("{}")
                level = struct.pack("<IiiiHH", 0x504C564C, 15, 0, 16, 0x6705, 33)
                hog = b"DHF" + struct.pack("<13si", b"level.rl2", len(level)) + level
                files = {"d2-xl.hog": hog, "d2-xl.mn2": b"name=synthetic"}
                modern = mode.endswith("modern")
                modern_hog = b"D2X synthetic modern original"

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
                if modern:
                    data["original_mission_archives"].append(
                        {"path": "missions/boilpnt.7z", **identity(b"modern archive")}
                    )
                    (root / "missions/boilpnt.7z").write_bytes(b"modern archive")
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
                    if source.name == "boilpnt.7z":
                        target.mkdir()
                        (target / "boilpnt.hog").write_bytes(modern_hog if mode == "valid-modern" else b"changed")
                        return
                    if source.name != "D2-XL.7z":
                        return original_extract(source, target, seven_zip=seven_zip)
                    target.mkdir()
                    for name, content in files.items():
                        (target / name).write_bytes(content)

                pins = {name: (len(content), hashlib.sha256(content).hexdigest()) for name, content in files.items()}
                with (
                    patch.object(assets, "manifest", return_value=data),
                    patch.object(assets, "D2_FILES", pins),
                    patch.object(
                        assets,
                        "MODERN_FILES",
                        {"modern/boilpnt.hog": (len(modern_hog), hashlib.sha256(modern_hog).hexdigest())},
                    ),
                    patch.object(assets, "extract_archive", side_effect=extract),
                ):
                    if mode in ("valid", "valid-modern"):
                        assets.stage_inputs(root, destination, None, modern=modern)
                        self.assertEqual((destination / "witnesses/level.rl2").read_bytes(), level)
                        assets.verify(destination, modern=modern)
                        with self.assertRaises(ValueError):
                            assets.stage_inputs(root, destination, None)
                        self.assertTrue(destination.exists())
                        if modern:
                            (destination / "modern/boilpnt.hog").unlink()
                            with self.assertRaises(OSError):
                                assets.verify(destination, modern=True)
                    else:
                        with self.assertRaises(ValueError):
                            assets.stage_inputs(root, destination, None, modern=modern)
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

    def test_public_fast_receipt_exposes_only_fixed_counts(self):
        row = {
            "id": assets.FAST_OWNER_ID,
            "xl_checks": {
                "required_passed": 96,
                "format_witnesses": 16,
                "modern_source_levels": 3,
                "camera_subchecks": "not_run_missing_addon",
                "private": "raw input must not escape",
            },
        }
        clean = retail.public_diagnostics(row)
        self.assertEqual(
            clean["xl_checks"],
            {
                "required_passed": 96,
                "format_witnesses": 16,
                "modern_source_levels": 3,
                "camera_subchecks": "not_run_missing_addon",
            },
        )
        self.assertNotIn("raw input", json.dumps(clean))
        row["xl_checks"].update(
            navigation_smoke_passed=1, navigation_anchors_passed=4, navigation_smoke_milliseconds=800
        )
        self.assertEqual(retail.public_diagnostics(row)["xl_checks"]["navigation_smoke_milliseconds"], 800)
        for invalid in ("private", -1, 30001, True):
            row["xl_checks"]["navigation_smoke_milliseconds"] = invalid
            self.assertNotIn("navigation_smoke_milliseconds", retail.public_diagnostics(row)["xl_checks"])
        for invalid in (True, -1, 1001, "private text"):
            changed = copy.deepcopy(row)
            changed["xl_checks"]["required_passed"] = invalid
            self.assertNotIn("xl_checks", retail.public_diagnostics(changed))
        self.assertNotIn("xl_checks", retail.public_diagnostics({**row, "id": "unrelated"}))

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
                    + (
                        "<system-out>XL_CAMERA_SUBCHECKS: not_run_missing_addon</system-out>"
                        if name == "xl_projectiles"
                        else ""
                    )
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
            self.assertEqual(receipt(passed, 0)["required_passed"], 94)
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
                        case = ET.SubElement(suite, "testcase", name=name, status="run")
                if name == "xl_projectiles":
                    ET.SubElement(case, "system-out").text = "XL_CAMERA_SUBCHECKS: not_run_missing_addon"
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
            self.assertEqual({e["id"] for e in chosen["tests"]}, assets.ORIGINAL_OWNER_IDS)
            self.assertEqual(set(chosen["lanes"]), {"host", "android-single"})
            fast = sweep.select_xl_originals(plan, policy, {"tests": []}, fast=True)
            self.assertEqual({e["id"] for e in fast["tests"]}, {assets.FAST_OWNER_ID})
            self.assertEqual(fast["lanes"], ["host"])
            self.assertLess(fast["estimated_seconds"], chosen["estimated_seconds"])
            with self.assertRaises(ValueError):
                sweep.select_xl_originals(plan, policy, {"tests": []})
            with patch.dict(os.environ, {"GITHUB_EVENT_NAME": "pull_request"}):
                with self.assertRaises(ValueError):
                    sweep.select_xl_originals(plan, policy, catalog)
                with self.assertRaises(ValueError):
                    sweep.select_xl_originals(plan, policy, catalog, fast=True)
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


class NavigationSmokeTests(unittest.TestCase):
    def test_sampling_is_reproducible_and_covers_pool(self):
        chosen = [smoke.select(seed) for seed in range(100)]
        self.assertEqual(chosen, [smoke.select(seed) for seed in range(100)])
        self.assertEqual({mission for mission, _ in chosen}, set(smoke.POOL))
        for mission, level in chosen:
            self.assertIn(level, smoke.POOL[mission]["levels"])

    def test_only_successful_requested_xl_route_is_accepted(self):
        mission, level = smoke.select(318)
        ordinal, filename, version, game, target = level
        route = {
            "schema": "dxx-guidebot-route-confirmation-v1",
            "status": "confirmed",
            "mission": mission,
            "level": ordinal,
            "seed": 1,
            "fixed_hz": 60,
            "frames": 90,
            "verification_goal": {"kind": "segment", "segment": target},
            "last_actor_state": {"segment": target},
            "objectives": [{"kind": "unexplored"}],
        }
        proof = {
            "mission": mission,
            "level": ordinal,
            "level_file": filename,
            "xl_active": True,
            "level_version": version,
            "game_version": game,
            "segments": 200,
            "visited_segments": 8,
        }
        smoke.validate(route, proof, mission, level)
        for field, values in {
            "status": ["failed", "timeout", "unsupported", "partial", "inspection_only"],
            "mission": ["other"],
            "level": [ordinal + 1],
            "seed": [2],
            "frames": [0, 1801],
            "objectives": [[]],
            "verification_goal": [{"kind": "segment", "segment": target + 1}],
            "last_actor_state": [{"segment": target + 1}],
        }.items():
            for value in values:
                changed = copy.deepcopy(route)
                changed[field] = value
                with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                    smoke.validate(changed, proof, mission, level)
        for field, value in {
            "xl_active": False,
            "visited_segments": 2,
            "segments": 0,
            "level_file": "other.rl2",
            "level_version": 8,
            "game_version": 32,
            "mission": "other",
            "level": 9,
        }.items():
            changed = dict(proof, **{field: value})
            with self.subTest(field=field), self.assertRaises(ValueError):
                smoke.validate(route, changed, mission, level)


class ReadinessTests(unittest.TestCase):
    def test_score_never_counts_rejections_or_unknowns_as_passes(self):
        import xl_readiness as readiness

        outcomes = [
            "loaded_ticked",
            "loaded_ticked",
            "unsupported_format",
            "load_failed",
            "simulation_failed",
            "crash",
            "timeout",
            "load_failed",
        ]
        rows = [{"outcome": value} for value in outcomes]
        self.assertEqual(readiness.summary(rows)["readiness_percent"], 25.0)
        for unknown in ("untested", "infrastructure_error"):
            rows[-1]["outcome"] = unknown
            self.assertIsNone(readiness.summary(rows)["readiness_percent"])
            self.assertEqual(readiness.summary(rows)["denominator"], 8)
            self.assertEqual(readiness.summary(rows)["passed"], 2)

    def test_native_load_tick_identity_and_outcomes(self):
        import xl_readiness as readiness

        case = {"mission": "sample", "level": 2, "level_file": "two.rl2"}
        proof = {
            "schema": "dxx-xl-load-tick-v1",
            "phase": "loaded_ticked",
            **case,
            "frames": 120,
            "segments": 40,
            "fixed_hz": 60,
            "seed": 1,
            "assisted": False,
        }
        self.assertEqual(readiness.classify(0, proof, "", case), "loaded_ticked")
        for key, value in (("frames", 0), ("mission", "wrong"), ("level", 1), ("assisted", True)):
            self.assertNotEqual(readiness.classify(0, {**proof, key: value}, "", case), "loaded_ticked")
        self.assertEqual(readiness.classify(-11, proof, "", case), "crash")
        self.assertEqual(readiness.classify(None, proof, "", case), "timeout")
        self.assertEqual(
            readiness.classify(1, {"phase": "level_load"}, "unsupported level version 99", case), "unsupported_format"
        )
        self.assertEqual(readiness.classify(1, {"phase": "level_load"}, "missing resource", case), "load_failed")
        self.assertEqual(readiness.classify(0, {}, "", case), "infrastructure_error")

    def test_advertised_inventory_keeps_secrets_and_unusual_names(self):
        import xl_readiness as readiness

        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "sample.mn2"
            path.write_text("d2x-name = sample\nnum_levels=2\nget3d!ii.rl2\n46&22.rl2\nnum_secrets=1\nsecret.rl2,1\n")
            self.assertEqual([r["level"] for r in readiness.levels(path)], [1, 2, -1])
            path.write_text("num_levels=1\n../escape.rl2\n")
            with self.assertRaises(ValueError):
                readiness.levels(path)

    def test_frozen_cohort_cannot_drop_failure_or_forge_target(self):
        import xl_readiness as readiness
        import xl_readiness_report as reporter

        catalog = json.loads(Path(__file__).with_name("xl_readiness_inventory.json").read_text())
        cases = {r["id"]: r for r in catalog["cases"]}
        rows = [
            {**cases[key], "outcome": "load_failed", "assisted_navigation": "not_run"} for key in catalog["selected"]
        ]
        receipt = {
            "schema": "dxx-xl-readiness-v1",
            "target_sha": "a" * 40,
            "asset_commit": readiness.ASSET_COMMIT,
            "rows": rows,
        }
        self.assertEqual(reporter.validate(receipt, catalog, "a" * 40)["readiness_percent"], 0)
        with self.assertRaises(ValueError):
            reporter.validate({**receipt, "rows": rows[:-1]}, catalog, "a" * 40)
        with self.assertRaises(ValueError):
            reporter.validate(receipt, catalog, "b" * 40)
        self.assertTrue(readiness.NEW_CAMPAIGNS <= {Path(cases[key]["archive"]).name for key in catalog["selected"]})
        self.assertEqual(len(catalog["archives"]), 23)
        workflow = Path(__file__).resolve().parents[2] / ".github/workflows/xl-pr-assets.yml"
        lane = workflow.read_text().split("  readiness:", 1)[1]
        self.assertLess(lane.index("Revalidate pinned candidate"), lane.index("Check out pinned uploaded"))
        self.assertLess(lane.index("rm -rf retail-input"), lane.index("Build candidate"))

    def test_staging_preserves_original_bytes_without_duplicate_descriptors(self):
        import xl_readiness as readiness

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / "sample.zip"
            descriptor = b"num_levels=1\none.rl2\n"
            with zipfile.ZipFile(archive, "w") as stream:
                stream.writestr("missions/single/sample.mn2", descriptor)
                stream.writestr("missions/single/sample.hog", b"original HOG bytes")
                stream.writestr("missions/single/other.mn2", descriptor)
                stream.writestr("mods/sample/sounds/test.wav", b"original sound bytes")
            record = {"path": archive.name, "size": archive.stat().st_size, "sha256": readiness.digest(archive)}
            case = {
                "descriptor": "missions/single/sample.mn2",
                "descriptor_sha256": hashlib.sha256(descriptor).hexdigest(),
                "level": 1,
                "level_file": "one.rl2",
            }
            extra = readiness.stage_case(root, record, case, root / "work", None)
            self.assertEqual(list(extra.rglob("*.mn2")), [extra / "missions/sample.mn2"])
            self.assertEqual((extra / "missions/sample.mn2").read_bytes(), descriptor)
            self.assertEqual((extra / "missions/sample.hog").read_bytes(), b"original HOG bytes")
            self.assertEqual((extra / "mods/sample/sounds/test.wav").read_bytes(), b"original sound bytes")

    def test_auxiliary_failure_does_not_erase_loaded_ticked(self):
        import xl_readiness as readiness
        import xl_readiness_report as reporter

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            case = {
                "id": "sample.zip/sample.mn2:1",
                "archive": "sample.zip",
                "mission": "sample",
                "descriptor": "sample.mn2",
                "level": 1,
                "level_file": "one.rl2",
            }
            catalog = {
                "asset_commit": readiness.ASSET_COMMIT,
                "selection_seed": readiness.SEED,
                "selected": [case["id"]],
                "cases": [case],
                "archives": [{"path": "sample.zip"}],
            }

            def execute(command, **kwargs):
                if "-xl-readiness-proof" in command:
                    readiness.write(
                        command[command.index("-xl-readiness-proof") + 1],
                        {
                            "schema": "dxx-xl-load-tick-v1",
                            "phase": "loaded_ticked",
                            "mission": "sample",
                            "level": 1,
                            "level_file": "one.rl2",
                            "frames": 120,
                            "segments": 40,
                            "fixed_hz": 60,
                            "seed": 1,
                            "assisted": False,
                            "xl_active": False,
                        },
                    )
                    return subprocess.CompletedProcess(command, 0)
                raise OSError("auxiliary executable failed")

            with (
                patch.object(readiness, "stage_case", return_value=root),
                patch.object(readiness.subprocess, "run", side_effect=execute),
            ):
                receipt = readiness.run(root / "engine", root, root, catalog, root / "run", None, "a" * 40)
            row = receipt["rows"][0]
            self.assertEqual((row["outcome"], row["assisted_navigation"]), ("loaded_ticked", "infrastructure_error"))
            result = reporter.validate(receipt, catalog, "a" * 40)
            self.assertEqual(result["readiness_percent"], 100)
            self.assertIn("0 XL-active, 1 classic/imported", reporter.markdown(receipt, catalog, result))
            row["xl_active"] = "arbitrary private text"
            with self.assertRaises(ValueError):
                reporter.validate(receipt, catalog, "a" * 40)

    def test_all_four_anchors_run_and_failure_does_not_reroll(self):
        import run_xl_navigation_smoke as smoke

        with patch.object(smoke, "run", return_value={"seconds": 0.1}) as run:
            self.assertEqual(smoke.run_anchors("engine", "stage", "data", "work")["anchors_passed"], 4)
            self.assertEqual(
                {(call.kwargs["case"][0], call.kwargs["case"][1][0]) for call in run.call_args_list},
                {("d2-xl", 1), ("d2-xl", 3), ("obsrv-xl", 1), ("neptunxl", 1)},
            )
        with patch.object(smoke, "run", side_effect=ValueError("failed")) as run:
            with self.assertRaises(ValueError):
                smoke.run_anchors("engine", "stage", "data", "work")
            self.assertEqual(run.call_count, 1)


if __name__ == "__main__":
    unittest.main()
