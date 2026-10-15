"""Adapter contracts without executing target native, PowerShell, or device tests."""

import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
import warnings
import zipfile
from pathlib import Path
from unittest.mock import patch

import run_host_test_suites as adapter
import test_fingerprint_match_strict as strict


class HostAdapterTests(unittest.TestCase):
    def setUp(self):
        # Exercise Linux-only contracts with mocked subprocesses on every CI host.
        self.enterContext(patch.object(adapter.sys, "platform", "linux"))
        self.enterContext(patch.object(adapter.signal, "SIGKILL", 9, create=True))

    def test_registry_ids_are_unique_and_no_arbitrary_commands(self):
        suites = adapter.load_suites()
        self.assertGreater(len(suites), 70)
        for key, suite in suites.items():
            self.assertEqual(key, suite["id"])
            self.assertNotIn("command", suite)
            self.assertIn(
                suite["kind"],
                {
                    "python-unittest",
                    "python-script",
                    "extractor",
                    "configured-ctest",
                    "public-powershell",
                    "retail-powershell",
                    "retail-python",
                    "standalone-cmake",
                },
            )

    def test_unsupported_windows_host_blocks_before_launch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            suite = {"id": "fixture", "coverage_scope": "fixture"}
            with (
                patch.object(adapter.sys, "platform", "win32"),
                patch.object(adapter.subprocess, "Popen") as launch,
            ):
                code = adapter.run_suite(
                    suite, root, root / "build", root / "receipt.json", 10
                )
            self.assertEqual(code, 2)
            launch.assert_not_called()
            receipt = json.loads((root / "receipt.json").read_text())
            self.assertFalse(receipt["attempted_tests"])
            self.assertIn("Linux only", receipt["reason"])

    def test_hmp_owned_by_ctest_not_unconfigured_discovery(self):
        self.assertNotIn("python:test_hmp_playback.py", adapter.load_suites())

    def test_failure_dominates_media_gap(self):
        status, exclusions = adapter.classify_cases(
            [{"status": "FAIL"}, {"status": "BLOCKED"}]
        )
        self.assertEqual(status, "FAIL")
        self.assertEqual(len(exclusions), 1)

    def test_partial_media_gap_completes_owner_but_retains_exclusion(self):
        status, exclusions = adapter.classify_cases(
            [{"status": "PASS"}, {"status": "BLOCKED"}]
        )
        self.assertEqual(status, "PASS")
        self.assertEqual(len(exclusions), 1)

    def test_empty_suite_is_blocked(self):
        self.assertEqual(adapter.classify_cases([])[0], "BLOCKED")

    def test_only_explicit_skips_is_blocked(self):
        self.assertEqual(adapter.classify_cases([{"status": "SKIP"}])[0], "BLOCKED")

    def test_target_path_traversal_rejected(self):
        with tempfile.TemporaryDirectory() as directory, self.assertRaises(ValueError):
            adapter.target_file(Path(directory), "../outside.py")

    def test_ctest_failure_skip_and_internal_media_are_distinct(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "result.xml"
            path.write_text("""<testsuite>
<testcase name="passed"/><testcase name="failed"><failure/></testcase>
<testcase name="skipped"><skipped/></testcase>
<testcase name="hfs"><system-out>SKIP: sample media not present</system-out></testcase>
<testcase name="gog"><system-out>skip missing fixture: installer.exe</system-out></testcase>
</testsuite>""")
            cases = adapter.read_ctest_results(path)
            self.assertEqual(
                [c["status"] for c in cases],
                ["PASS", "FAIL", "SKIP", "BLOCKED", "BLOCKED"],
            )

    def test_missing_tool_blocks_without_launch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            suite = {"id": "fake", "coverage_scope": "fake", "executables": ["missing"]}
            with (
                patch.object(adapter.shutil, "which", return_value=None),
                patch.object(adapter.subprocess, "Popen") as popen,
            ):
                code = adapter.run_suite(
                    suite, root, root / "build", root / "result.json", 10
                )
            self.assertEqual(code, 2)
            popen.assert_not_called()
            self.assertFalse(
                json.loads((root / "result.json").read_text())["coverage_complete"]
            )

    def test_old_commit_light_profile_blocks_without_full_download(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            source = root / "android/app/src/main/cpp/extract"
            source.mkdir(parents=True)
            (source / "CMakeLists.txt").write_text("project(old)")
            suite = {
                "id": "extractor:light",
                "kind": "extractor",
                "profile": "light",
                "coverage_scope": "light",
            }
            with patch.object(adapter.subprocess, "Popen") as popen:
                code = adapter.run_suite(
                    suite, root, root / "build", root / "result.json", 10
                )
            self.assertEqual(code, 2)
            popen.assert_not_called()
            data = json.loads((root / "result.json").read_text())
            self.assertFalse(data["retryable"])
            self.assertEqual(data["blocker_kind"], "target-capability")

    def test_verified_soundfont_helper_is_present_in_target_source(self):
        repo = Path(__file__).resolve().parents[2]
        helper = repo / "android/get_deps/helpers/get_soundfont.sh"
        self.assertTrue(
            helper.is_file(), "Full checkout must contain the existing SoundFont helper"
        )
        source = helper.read_text(encoding="utf-8")
        self.assertIn('verify_sha256 "$TMPFILE" "$SOUNDFONT_SHA256"', source)
        self.assertIn('DEST="$ASSETS_DIR/gm.sf2"', source)
        for relative in (
            "tool_versions.conf",
            "helpers/platform.sh",
            "helpers/verify_sha256.sh",
        ):
            self.assertTrue((repo / "android/get_deps" / relative).is_file())

    def test_extractor_soundfont_setup_is_full_only_and_failure_is_retryable(self):
        for profile, setup_code in (("full", 0), ("full", 7), ("light", 0)):
            with (
                self.subTest(profile=profile, setup_code=setup_code),
                tempfile.TemporaryDirectory() as directory,
            ):
                root = Path(directory).resolve()
                source = root / "android/app/src/main/cpp/extract"
                source.mkdir(parents=True)
                (source / "CMakeLists.txt").write_text("# DXX_EXTRACT_DEPENDENCY_LIGHT")
                calls = []

                class Process:
                    def __init__(self, argv, calls=calls, **kwargs):
                        self.argv = argv
                        calls.append(argv)
                        if argv[0] == "ctest":
                            Path(argv[argv.index("--output-junit") + 1]).write_text(
                                '<testsuite><testcase name="one"/></testsuite>'
                            )

                    def wait(self, setup_code=setup_code, **kwargs):
                        return setup_code if self.argv[0] == "bash" else 0

                suite = {
                    "id": "extractor:" + profile,
                    "kind": "extractor",
                    "profile": profile,
                    "coverage_scope": profile,
                }
                with patch.object(adapter.subprocess, "Popen", Process):
                    code = adapter.run_suite(
                        suite, root, root / "build", root / "result.json", 10
                    )
                result = json.loads((root / "result.json").read_text())
                if profile == "full":
                    self.assertEqual(
                        calls[0],
                        [
                            "bash",
                            str(root / "android/get_deps/helpers/get_soundfont.sh"),
                        ],
                    )
                else:
                    self.assertFalse(any(argv[0] == "bash" for argv in calls))
                if setup_code:
                    self.assertEqual(code, 2)
                    self.assertEqual(len(calls), 1)
                    self.assertEqual(result["status"], "BLOCKED")
                    self.assertTrue(result["retryable"])
                    self.assertFalse(result["attempted_tests"])
                    self.assertEqual(result["cases"], [])
                else:
                    self.assertEqual(code, 0)
                    self.assertEqual(calls[-1][0], "ctest")

    def test_native_lazy_setup_preserves_debug_and_serial_environment(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            (root / "d1").mkdir()
            (root / "d1/CMakeLists.txt").write_text("project(fake)")
            calls = []

            class Process:
                def __init__(self, argv, **kwargs):
                    calls.append((argv, kwargs))
                    if argv[0] == "ctest":
                        Path(argv[argv.index("--output-junit") + 1]).write_text(
                            '<testsuite><testcase name="one"/></testsuite>'
                        )

                def wait(self, **kwargs):
                    return 0

            suite = {
                "id": "engine:ctest-d1",
                "kind": "configured-ctest",
                "build_subdirectory": "engine-d1",
                "coverage_scope": "native",
            }
            with patch.object(adapter.subprocess, "Popen", Process):
                code = adapter.run_suite(
                    suite, root, root / "build", root / "result.json", 10
                )
            self.assertEqual(code, 0)
            self.assertIn("-DCMAKE_BUILD_TYPE=Debug", calls[0][0])
            self.assertIn("-DBUILD_TESTING=ON", calls[0][0])
            self.assertIn("-DCMAKE_C_FLAGS_DEBUG=-O0 -g1", calls[0][0])
            self.assertEqual(calls[-1][0][calls[-1][0].index("--parallel") + 1], "1")
            self.assertEqual(calls[-1][1]["env"]["SDL_AUDIODRIVER"], "dummy")
            self.assertEqual(
                calls[-1][1]["env"]["TMPDIR"], str(root / "result.artifacts/tmp")
            )
            self.assertEqual(calls[-1][1]["env"]["TEMP"], calls[-1][1]["env"]["TMPDIR"])
            result = json.loads((root / "result.json").read_text())
            self.assertIn("setup_seconds", result)
            self.assertEqual(result["cases"][0]["id"], "one")


class PublicProfileTests(unittest.TestCase):
    def setUp(self):
        # Exercise Linux-only contracts with mocked subprocesses on every CI host.
        self.enterContext(patch.object(adapter.sys, "platform", "linux"))
        self.enterContext(patch.object(adapter.signal, "SIGKILL", 9, create=True))

    def run_profile(self, profile, setup_failure=False):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            suite = adapter.load_suites()["host-profile:" + profile]
            script = root / suite["path"]
            script.parent.mkdir(parents=True)
            script.touch()
            calls = []

            class Process:
                def __init__(self, argv, **kwargs):
                    calls.append((argv, kwargs))

                def wait(self, **kwargs):
                    return 9 if setup_failure else 0

            with (
                patch.object(adapter.shutil, "which", return_value="tool"),
                patch.object(adapter.subprocess, "Popen", Process),
            ):
                code = adapter.run_suite(
                    suite, root, root / "build", root / "receipt.json", 30
                )
            receipt = json.loads((root / "receipt.json").read_text())
            return code, receipt, calls

    def test_imported_profile_has_explicit_worker_and_public_assets(self):
        code, receipt, calls = self.run_profile("metadata-imported-d1")
        self.assertEqual(code, 0)
        self.assertIn("dxx-redux-d2-metadata-worker", calls[1][0])
        self.assertIn("-Download", calls[2][0])
        self.assertIn("-CheckProfileSwitch", calls[3][0])
        self.assertIn("-NonInteractive", calls[3][0])
        self.assertIn("shareware", calls[3][0])
        self.assertFalse(receipt["coverage_complete"])
        self.assertTrue(receipt["attempted_tests"])
        self.assertEqual(receipt["cases"][0]["status"], "PASS")

    def test_header_profile_builds_both_workers_and_disables_inner_build(self):
        code, _receipt, calls = self.run_profile("metadata-headers")
        self.assertEqual(code, 0)
        self.assertIn("dxx-redux-d1-metadata-worker", calls[1][0])
        self.assertIn("dxx-redux-d2-metadata-worker", calls[3][0])
        self.assertIn("-NoBuild", calls[-1][0])
        self.assertIn("-D1Exe", calls[-1][0])
        self.assertIn("-D2Exe", calls[-1][0])

    def test_visual_profile_uses_virtual_display_not_dummy_video(self):
        code, _receipt, calls = self.run_profile("public-flight")
        self.assertEqual(code, 0)
        self.assertEqual(calls[-1][0][:2], ["xvfb-run", "-a"])
        self.assertEqual(calls[-1][1]["env"].get("SDL_VIDEODRIVER"), "x11")
        argv = calls[-1][0]
        self.assertEqual(Path(argv[argv.index("-D1Exe") + 1]).name, "d1x-redux")
        self.assertEqual(Path(argv[argv.index("-D2Exe") + 1]).name, "d2x-redux")

    def test_benchmark_policy_does_not_build_or_claim_performance(self):
        code, receipt, calls = self.run_profile("benchmark-policy")
        self.assertEqual(code, 0)
        self.assertEqual(len(calls), 1)
        self.assertEqual(calls[0][0][-1], "-PolicySelfTest")
        self.assertFalse(receipt["coverage_complete"])
        self.assertIn("performance", receipt["exclusions"][0]["reason"])

    def test_setup_failure_is_retryable_and_has_no_assertion_receipt(self):
        code, receipt, calls = self.run_profile("metadata-d1", setup_failure=True)
        self.assertEqual(code, 2)
        self.assertEqual(len(calls), 1)
        self.assertTrue(receipt["retryable"])
        self.assertFalse(receipt["attempted_tests"])
        self.assertEqual(receipt["cases"], [])

    def test_unknown_profile_does_not_construct_an_arbitrary_command(self):
        with self.assertRaises(ValueError):
            adapter.public_profile_arguments(
                {"profile": "untrusted"}, Path("repo"), Path("build"), Path("work")
            )

    def test_public_save_and_pickup_bind_existing_fixture_binaries(self):
        for profile, flags in (
            ("public-save", ("-D1Exe", "-D2Exe")),
            ("public-pickup", ("-NativeExe", "-ImportedExe")),
        ):
            with self.subTest(profile=profile):
                code, _receipt, calls = self.run_profile(profile)
                self.assertEqual(code, 0)
                argv = calls[-1][0]
                for flag in flags:
                    self.assertEqual(
                        Path(argv[argv.index(flag) + 1]).name, "test_upstream_compat"
                    )
                self.assertIn("-Output", argv)
                self.assertEqual(argv[:2], ["xvfb-run", "-a"])
                if profile == "public-pickup":
                    self.assertIn("shareware", argv)
                    self.assertIn("pickup", argv)

    def test_public_matcen_is_native_with_explicit_verified_inputs(self):
        code, receipt, calls = self.run_profile("public-matcen")
        self.assertEqual(code, 0)
        self.assertTrue(receipt["attempted_tests"])
        argv = calls[-1][0]
        self.assertEqual(argv[0], "pwsh")
        for game in ("d1", "d2"):
            flag = "-" + game.upper() + "Exe"
            self.assertEqual(
                Path(argv[argv.index(flag) + 1]).name, "test_upstream_compat"
            )
            flag = "-" + game.upper() + "DataDir"
            self.assertEqual(
                Path(argv[argv.index(flag) + 1]).parts[-3:],
                ("game_data", "shareware", game),
            )
        self.assertTrue(any("-Download" in command for command, _ in calls))

    def test_secret_and_shareware_profiles_cannot_succeed_on_missing_assets(self):
        for profile, required in (
            ("public-secret-baseline", "-RequireAssets"),
            ("shareware-data", "-RequireData"),
        ):
            with self.subTest(profile=profile):
                code, _receipt, calls = self.run_profile(profile)
                self.assertEqual(code, 0)
                self.assertIn(required, calls[-1][0])
                self.assertTrue(any("-Download" in argv for argv, _ in calls))

    def test_ordered_and_all_weapons_build_all_backends_with_compile_provenance(self):
        for profile in ("public-ordered-input", "public-all-weapons"):
            with self.subTest(profile=profile):
                code, _receipt, calls = self.run_profile(profile)
                self.assertEqual(code, 0)
                self.assertIn("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", calls[0][0])
                for index, game in ((1, "d1"), (3, "d2")):
                    argv = calls[index][0]
                    self.assertEqual(
                        argv[argv.index("--target") + 1 :],
                        [
                            "test_upstream_compat",
                            "dxx-redux-" + game,
                            "dxx-redux-" + game + "-headless",
                        ],
                    )
                argv = calls[-1][0]
                self.assertEqual(argv[:2], ["xvfb-run", "-a"])
                if profile == "public-ordered-input":
                    for flag in (
                        "-D1Exe",
                        "-D2Exe",
                        "-D1Headless",
                        "-D2Headless",
                        "-D1Graphics",
                        "-D2Graphics",
                    ):
                        self.assertIn(flag, argv)
                else:
                    self.assertIn("-D1BuildDir", argv)
                    self.assertIn("-D2BuildDir", argv)
                    self.assertNotIn(
                        "-Revision", argv
                    )  # Existing owner binds exact clean HEAD itself.


class RetailProfileTests(unittest.TestCase):
    def setUp(self):
        self.enterContext(patch.object(adapter.sys, "platform", "linux"))
        self.enterContext(patch.object(adapter.shutil, "which", return_value="tool"))

    def run_profile(self, profile, *, owner_code=0, setup_code=0):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            suite = adapter.load_suites()["host-profile:" + profile]
            script = root / suite["path"]
            script.parent.mkdir(parents=True)
            script.touch()
            calls = []
            events = []

            class Process:
                def __init__(self, argv, **kwargs):
                    self.argv = argv
                    calls.append(argv)
                    events.append(argv[0])

                def wait(self, **kwargs):
                    return setup_code if self.argv[0] == "cmake" else owner_code

            with (
                patch.object(
                    adapter,
                    "verify_retail_stage",
                    side_effect=lambda repo, **kwargs: events.append("verify"),
                ) as verify,
                patch.object(adapter, "prepare_obsidian_mission") as prepare,
                patch.object(adapter.subprocess, "Popen", Process),
            ):
                code = adapter.run_suite(
                    suite, root, root / "build", root / "receipt.json", 30
                )
                if suite.get("fixture") == "retail-missions":
                    verify.assert_called_once_with(root, include_missions=True)
                else:
                    verify.assert_called_once_with(root)
                if profile == "retail-obsidian-liquid":
                    prepare.assert_called_once_with(root, root / "receipt.artifacts")
                else:
                    prepare.assert_not_called()
            receipt = json.loads((root / "receipt.json").read_text())
            self.assertEqual(events[0], "verify")
            return code, receipt, calls, root

    def test_base_retail_registry_is_four_fixed_owners_and_portable_controls(self):
        suites = adapter.load_suites()
        self.assertEqual(
            {
                suite["profile"]
                for suite in suites.values()
                if suite.get("fixture") == "retail-base"
            },
            {
                "retail-secret-baseline",
                "retail-counterstrike-route",
                "retail-metadata-headers",
                "retail-guidebot-timeout",
            },
        )
        for suite in suites.values():
            if suite.get("fixture") == "retail-base":
                self.assertEqual(suite["fixture"], "retail-base")
                self.assertFalse(suite["public_data"])
                self.assertFalse(suite["virtual_display"])
                self.assertEqual(suite["executables"], ["pwsh", "cmake"])
        for name in ("test_retail_ci_assets.py", "test_nightly_retail.py"):
            self.assertEqual(suites["python:" + name]["kind"], "python-unittest")
            self.assertEqual(suites["python:" + name]["executables"], [])

    def test_retail_baseline_and_headers_bind_both_games_without_inner_builds(self):
        for profile, suffix, output_flag, required in (
            (
                "retail-secret-baseline",
                "-headless-metadata",
                "-OutputDir",
                "-RequireAssets",
            ),
            ("retail-metadata-headers", "-metadata-worker", "-OutputDirectory", "-NoBuild"),
        ):
            with self.subTest(profile=profile):
                code, receipt, calls, root = self.run_profile(profile)
                self.assertEqual(code, 0)
                self.assertEqual(len(calls), 5)
                argv = calls[-1]
                self.assertEqual(argv[argv.index("-DataEdition") + 1], "retail")
                self.assertEqual(argv[argv.index("-Game") + 1], "both")
                for index, game in enumerate(("d1", "d2")):
                    name = "dxx-redux-" + game + suffix
                    self.assertEqual(calls[index * 2 + 1][-1], name)
                    flag = "-" + game.upper() + "Exe"
                    self.assertEqual(
                        argv[argv.index(flag) + 1],
                        str(root / ("build/engine-" + game) / "main" / name),
                    )
                    flag = "-" + game.upper() + "DataDir"
                    self.assertEqual(
                        argv[argv.index(flag) + 1],
                        str(root / "game_data/retail-ci" / game),
                    )
                self.assertIn(output_flag, argv)
                self.assertIn(required, argv)
                self.assertNotIn("-UpdateBaseline", argv)
                self.assertNotIn("-BuildBeforeRun", argv)
                self.assertFalse(any("-Download" in command for command in calls))
                self.assertTrue(receipt["coverage_complete"])
                self.assertTrue(receipt["attempted_tests"])
                self.assertEqual(len(receipt["cases"]), 1)

    def test_counterstrike_route_keeps_existing_bounded_owner(self):
        code, _receipt, calls, root = self.run_profile("retail-counterstrike-route")
        self.assertEqual(code, 0)
        self.assertEqual(len(calls), 3)
        self.assertEqual(calls[1][-1], "dxx-redux-d2-headless-metadata")
        self.assertEqual(
            calls[-1][5:],
            [
                "-Exe",
                str(root / "build/engine-d2/main/dxx-redux-d2-headless-metadata"),
                "-DataDir",
                str(root / "game_data/retail-ci/d2"),
                "-OutputPath",
                str(root / "receipt.artifacts/counterstrike-route.json"),
            ],
        )
        repo = Path(__file__).resolve().parents[2]
        source = (
            repo / "android/tests/test_counterstrike_level2_trigger21_route.ps1"
        ).read_text(encoding="utf-8")
        self.assertIn("TimeoutSeconds = 300", source)

    def test_guidebot_uses_native_assertion_and_cannot_pass_exit_two(self):
        for owner_code, expected in ((0, "PASS"), (1, "FAIL"), (2, "BLOCKED")):
            with self.subTest(owner_code=owner_code):
                code, receipt, calls, root = self.run_profile(
                    "retail-guidebot-timeout", owner_code=owner_code
                )
                self.assertEqual(code, owner_code)
                self.assertEqual(calls[1][-1], "dxx-redux-d2-headless-route")
                self.assertEqual(
                    calls[-1][5:],
                    [
                        "-NoBuild",
                        "-HeadlessExecutable",
                        str(root / "build/engine-d2/main/dxx-redux-d2-headless-route"),
                        "-HogDir",
                        str(root / "game_data/retail-ci/d2"),
                    ],
                )
                self.assertEqual(receipt["status"], expected)
                self.assertEqual(receipt["cases"][0]["status"], expected)
                self.assertEqual(receipt["coverage_complete"], owner_code == 0)
                if owner_code == 2:
                    self.assertTrue(receipt["retryable"])
                    self.assertEqual(receipt["exclusions"], receipt["cases"])
        repo = Path(__file__).resolve().parents[2]
        source = (
            repo / "android/tests/test_guidebot_simulation_timeout_policy.ps1"
        ).read_text(encoding="utf-8")
        self.assertIn("Assert-Equal 61 $engineResult.frames", source)
        self.assertIn("Assert-Equal 'timeout' $engineResult.status", source)
        self.assertIn("if ($LASTEXITCODE -ne 2)", source)
        self.assertIn("exit 2", source[:source.index("Assert-Equal 61")])

    def test_build_failure_is_retryable_without_assertion_credit(self):
        code, receipt, calls, _root = self.run_profile(
            "retail-secret-baseline", setup_code=9
        )
        self.assertEqual(code, 2)
        self.assertEqual(len(calls), 1)
        self.assertEqual(receipt["phase"], "setup")
        self.assertTrue(receipt["retryable"])
        self.assertFalse(receipt["attempted_tests"])
        self.assertFalse(receipt["coverage_complete"])
        self.assertEqual(receipt["cases"], [])

    def test_missing_mismatched_and_changed_pin_stages_block_before_build(self):
        control = Path(__file__).resolve().parents[2]
        baseline = (control / "android/helpers/standard_game_data.ps1").read_text(
            encoding="utf-8"
        )
        for failure in ("missing", "mismatched", "changed-pins"):
            with (
                self.subTest(failure=failure),
                tempfile.TemporaryDirectory() as directory,
            ):
                root = Path(directory).resolve()
                suite = adapter.load_suites()["host-profile:retail-guidebot-timeout"]
                script = root / suite["path"]
                script.parent.mkdir(parents=True)
                script.touch()
                pins = root / "android/helpers/standard_game_data.ps1"
                pins.parent.mkdir(parents=True)
                pins.write_text(baseline, encoding="utf-8")
                if failure == "changed-pins":
                    pins.write_text(
                        baseline.replace("f1abf516", "f2abf516"), encoding="utf-8"
                    )
                if failure == "mismatched":
                    files = {
                        "d1": ("descent.hog", "descent.pig"),
                        "d2": (
                            "descent2.hog",
                            "descent2.ham",
                            "descent2.s22",
                            "groupa.pig",
                            "alien1.pig",
                            "alien2.pig",
                            "fire.pig",
                            "ice.pig",
                            "water.pig",
                        ),
                    }
                    for game, names in files.items():
                        stage = root / "game_data/retail-ci" / game
                        stage.mkdir(parents=True)
                        for name in names:
                            (stage / name).write_bytes(b"Synthetic mismatch only")
                with patch.object(adapter.subprocess, "Popen") as launch:
                    code = adapter.run_suite(
                        suite, root, root / "build", root / "receipt.json", 10
                    )
                self.assertEqual(code, 2)
                launch.assert_not_called()
                receipt = json.loads((root / "receipt.json").read_text())
                self.assertEqual(receipt["status"], "BLOCKED")
                self.assertEqual(receipt["phase"], "setup")
                self.assertEqual(receipt["blocker_kind"], "retail-fixture")
                self.assertTrue(receipt["retryable"])
                self.assertFalse(receipt["attempted_tests"])
                self.assertFalse(receipt["coverage_complete"])
                self.assertEqual(receipt["cases"], [])

    def test_unknown_retail_profile_cannot_supply_arguments(self):
        with self.assertRaises(ValueError):
            adapter.retail_profile_arguments(
                {"profile": "arbitrary"}, Path("repo"), Path("build"), Path("work")
            )

    def test_two_expansion_profiles_require_mission_stage(self):
        suites = adapter.load_suites()
        expansions = [
            suite
            for suite in suites.values()
            if suite.get("fixture") == "retail-missions"
        ]
        self.assertEqual(
            {suite["profile"] for suite in expansions},
            {"retail-vertigo-metadata", "retail-obsidian-liquid"},
        )
        for suite in expansions:
            self.assertEqual(suite["games"], ["d2"])
            self.assertFalse(suite["public_data"])
            self.assertFalse(suite["virtual_display"])
        registry = json.loads(adapter.REGISTRY.read_text(encoding="utf-8"))
        self.assertNotIn(
            "android/tests/test_secret_area_liquid_obsidian.py",
            {entry["path"] for entry in registry["unsupported_entrypoints"]},
        )

    def test_vertigo_profile_uses_reused_worker_and_exact_staged_missions(self):
        code, receipt, calls, root = self.run_profile("retail-vertigo-metadata")
        self.assertEqual(code, 0)
        self.assertEqual(len(calls), 3)
        self.assertEqual(calls[1][-1], "dxx-redux-d2-metadata-worker")
        self.assertEqual(Path(calls[-1][4]).name, "test_vertigo_metadata.ps1")
        self.assertEqual(
            calls[-1][5:],
            [
                "-NoBuild",
                "-Worker",
                str(root / "build/engine-d2/main/dxx-redux-d2-metadata-worker"),
                "-DataDir",
                str(root / "game_data/retail-ci/d2"),
                "-VertigoDir",
                str(root / "game_data/retail-ci/missions"),
            ],
        )
        self.assertTrue(receipt["attempted_tests"])

    def test_obsidian_profile_keeps_assertions_and_native_owner_timeout(self):
        for owner_code in (0, 1):
            with self.subTest(owner_code=owner_code):
                code, receipt, calls, root = self.run_profile(
                    "retail-obsidian-liquid", owner_code=owner_code
                )
                self.assertEqual(code, owner_code)
                self.assertEqual(len(calls), 3)
                self.assertEqual(calls[1][-1], "dxx-redux-d2-headless-metadata")
                # Isolated Python ignores PYTHONOPTIMIZE and user site hooks
                self.assertEqual(calls[-1][:2], [sys.executable, "-I"])
                self.assertEqual(
                    Path(calls[-1][2]).name, "test_secret_area_liquid_obsidian.py"
                )
                self.assertEqual(
                    calls[-1][3:],
                    [
                        "--exe",
                        str(root / "build/engine-d2/main/dxx-redux-d2-headless-metadata"),
                        "--hogdir",
                        str(root / "game_data/retail-ci/d2"),
                        "--mission-dir",
                        str(root / "receipt.artifacts/obsidian-stage"),
                        "--output",
                        str(root / "receipt.artifacts/obsidian-liquid.json"),
                    ],
                )
                self.assertEqual(receipt["coverage_complete"], owner_code == 0)
        repo = Path(__file__).resolve().parents[2]
        owner = (repo / "android/tests/test_secret_area_liquid_obsidian.py").read_text()
        self.assertIn("timeout=60", owner)
        self.assertIn('assert level["secret_count"] == 6', owner)
        self.assertIn("{(154,), (282,)}", owner)

    def test_expansion_setup_errors_never_launch_builds_or_assertions(self):
        for profile, failing_step, error in (
            ("retail-vertigo-metadata", "verify", ValueError("wrong mission pin")),
            ("retail-obsidian-liquid", "verify", OSError("missing mission file")),
            ("retail-obsidian-liquid", "extract", zipfile.BadZipFile("invalid ZIP")),
            ("retail-obsidian-liquid", "extract", ValueError("wrong member digest")),
        ):
            with (
                self.subTest(profile=profile, error=str(error)),
                tempfile.TemporaryDirectory() as directory,
            ):
                root = Path(directory).resolve()
                suite = adapter.load_suites()["host-profile:" + profile]
                script = root / suite["path"]
                script.parent.mkdir(parents=True)
                script.touch()
                with (
                    patch.object(
                        adapter, "verify_retail_stage",
                        side_effect=error if failing_step == "verify" else None,
                    ),
                    patch.object(
                        adapter, "prepare_obsidian_mission",
                        side_effect=error if failing_step == "extract" else None,
                    ),
                    patch.object(adapter.subprocess, "Popen") as launch,
                ):
                    code = adapter.run_suite(
                        suite, root, root / "build", root / "receipt.json", 10
                    )
                self.assertEqual(code, 2)
                launch.assert_not_called()
                receipt = json.loads((root / "receipt.json").read_text())
                self.assertEqual(receipt["phase"], "setup")
                self.assertEqual(receipt["status"], "BLOCKED")
                self.assertTrue(receipt["retryable"])
                self.assertFalse(receipt["attempted_tests"])
                self.assertEqual(receipt["cases"], [])


class ObsidianExtractionTests(unittest.TestCase):
    def setUp(self):
        self.temp = self.enterContext(tempfile.TemporaryDirectory())
        self.root = Path(self.temp).resolve()
        self.work = self.root / "work"
        self.work.mkdir()
        self.archive = self.root / "game_data/retail-ci/missions/obsidian.zip"
        self.archive.parent.mkdir(parents=True)
        self.contents = {
            "obsidian.hog": b"Synthetic mission HOG, not licensed content",
            "obsidian.mn2": b"Synthetic mission descriptor, not licensed content",
        }
        self.pins = {
            name: (len(data), hashlib.sha256(data).hexdigest())
            for name, data in self.contents.items()
        }

    def write_archive(self, entries=None):
        with warnings.catch_warnings(), zipfile.ZipFile(self.archive, "w") as archive:
            warnings.simplefilter("ignore", UserWarning)
            for name, data in entries or self.contents.items():
                archive.writestr(name, data)

    def prepare(self):
        with patch.object(adapter, "OBSIDIAN_MEMBERS", self.pins):
            adapter.prepare_obsidian_mission(self.root, self.work)

    def test_exact_real_member_pins_are_preserved(self):
        self.assertEqual(
            adapter.OBSIDIAN_MEMBERS,
            {
                "obsidian.hog": (
                    35385307,
                    "32ee5f893718c2191975c826a6f95f116658b143b69bbf2aa73a9455174508db",
                ),
                "obsidian.mn2": (
                    338,
                    "1bf9a8cc9882a67356e16f08e06cb82923ca920710bf30d245cd7be9829bdde6",
                ),
            },
        )

    def test_only_two_members_are_installed_under_native_missions_directory(self):
        self.write_archive([
            *self.contents.items(), ("obsidian.bat", b"never execute or copy")
        ])
        self.prepare()
        stage = self.work / "obsidian-stage"
        self.assertEqual(
            sorted(
                path.relative_to(stage).as_posix()
                for path in stage.rglob("*") if path.is_file()
            ),
            ["missions/obsidian.hog", "missions/obsidian.mn2"],
        )
        for name, data in self.contents.items():
            self.assertEqual((stage / "missions" / name).read_bytes(), data)
        with self.assertRaises(FileExistsError):
            self.prepare()

    def test_duplicate_case_alias_and_unsafe_paths_are_rejected(self):
        for name in (
            "../outside", "/outside", "folder/file", "folder\\file", "C:outside",
            "obsidian.hog", "OBSIDIAN.HOG",
        ):
            with self.subTest(name=name):
                self.write_archive([*self.contents.items(), (name, b"unsafe")])
                with self.assertRaises(ValueError):
                    self.prepare()
                self.assertFalse((self.work / "obsidian-stage").exists())

    def test_missing_member_and_size_mismatch_are_rejected_before_extraction(self):
        for entries in (
            [("obsidian.hog", self.contents["obsidian.hog"])],
            [
                ("obsidian.hog", b"wrong size"),
                ("obsidian.mn2", self.contents["obsidian.mn2"]),
            ],
        ):
            with self.subTest(entries=entries):
                self.write_archive(entries)
                with self.assertRaises(ValueError):
                    self.prepare()
                self.assertFalse((self.work / "obsidian-stage").exists())

    def test_same_size_hash_mismatch_removes_partial_stage(self):
        entries = list(self.contents.items())
        entries[-1] = ("obsidian.mn2", b"x" * len(self.contents["obsidian.mn2"]))
        self.write_archive(entries)
        with self.assertRaises(ValueError):
            self.prepare()
        self.assertFalse((self.work / "obsidian-stage").exists())

    def test_symlink_zip_member_and_excessive_member_count_are_rejected(self):
        link = zipfile.ZipInfo("obsidian.hog")
        link.create_system = 3
        link.external_attr = (0o120777 << 16)
        self.write_archive([
            (link, self.contents["obsidian.hog"]),
            ("obsidian.mn2", self.contents["obsidian.mn2"]),
        ])
        with self.assertRaises(ValueError):
            self.prepare()
        self.write_archive([
            *self.contents.items(), *((f"extra{i}", b"") for i in range(31))
        ])
        with self.assertRaises(ValueError):
            self.prepare()
        self.assertFalse((self.work / "obsidian-stage").exists())


class StandaloneCMakeTests(unittest.TestCase):
    def test_existing_ctest_projects_build_exact_targets_and_keep_leaf_receipts(self):
        for name, target, public_soundfont in (
            ("etc2-limits", "test_etc2_limits", False),
            ("fluidsynth-contracts", "fluid_render", True),
        ):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory).resolve()
                suite = adapter.load_suites()["cmake:" + name]
                source = root / suite["source"]
                source.mkdir(parents=True)
                (source / "CMakeLists.txt").touch()
                calls = []

                class Process:
                    def __init__(self, argv, observed=calls, **kwargs):
                        observed.append(argv)
                        if argv[0] == "ctest":
                            Path(argv[argv.index("--output-junit") + 1]).write_text(
                                '<testsuite><testcase name="existing-contract"/></testsuite>'
                            )

                    def wait(self, **kwargs):
                        return 0

                with (
                    patch.object(adapter.sys, "platform", "linux"),
                    patch.object(adapter.shutil, "which", return_value="tool"),
                    patch.object(adapter.subprocess, "Popen", Process),
                ):
                    code = adapter.run_suite(
                        suite, root, root / "build", root / "receipt.json", 30
                    )
                self.assertEqual(code, 0)
                self.assertEqual(calls[0][0] == "bash", public_soundfont)
                if public_soundfont:
                    self.assertTrue(calls[0][1].endswith("get_soundfont.sh"))
                build = next(argv for argv in calls if "--build" in argv)
                self.assertEqual(build[build.index("--target") + 1], target)
                self.assertEqual(calls[-1][0], "ctest")
                receipt = json.loads((root / "receipt.json").read_text())
                self.assertEqual(receipt["cases"][0]["id"], "existing-contract")
                self.assertTrue(receipt["coverage_complete"])


class EvidenceAndCancellationTests(unittest.TestCase):
    def setUp(self):
        # Exercise Linux-only contracts with mocked subprocesses on every CI host.
        self.enterContext(patch.object(adapter.sys, "platform", "linux"))
        self.enterContext(patch.object(adapter.signal, "SIGKILL", 9, create=True))

    def test_expected_failure_and_unexpected_success_remain_visible(self):
        class Cases(unittest.TestCase):
            @unittest.expectedFailure
            def test_expected(self):
                self.fail("known failure")

            @unittest.expectedFailure
            def test_unexpected_success(self):
                pass

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            suite = unittest.defaultTestLoader.loadTestsFromTestCase(Cases)
            with patch.object(
                unittest.defaultTestLoader, "discover", return_value=suite
            ):
                code = adapter.unittest_worker(root, "fixture.py", root / "cases.json")
            data = json.loads((root / "cases.json").read_text())
            self.assertEqual(code, 1)
            self.assertEqual(len(data["skipped"]), 1)
            self.assertEqual(len(data["failures"]), 1)
            self.assertEqual(data["passed"], [])

    def test_sigterm_unwinds_once(self):
        with (
            patch.object(adapter.signal, "signal") as install,
            self.assertRaises(adapter.SuiteCancelled),
        ):
            adapter.handle_termination(adapter.signal.SIGTERM, None)
        install.assert_called_once_with(adapter.signal.SIGTERM, adapter.signal.SIG_IGN)

    def test_timeout_reaps_group_even_after_parent_exits(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            (root / "fixture.py").touch()

            class Process:
                pid = 4321
                calls = 0

                def __init__(self, *args, **kwargs):
                    pass

                def wait(self, **kwargs):
                    self.calls += 1
                    if self.calls == 1:
                        raise subprocess.TimeoutExpired("fixture", 1)
                    return 0

            suite = {
                "id": "fixture",
                "kind": "python-script",
                "path": "fixture.py",
                "coverage_scope": "fixture",
            }
            with (
                patch.object(adapter.subprocess, "Popen", Process),
                patch.object(adapter.os, "killpg", create=True) as kill,
            ):
                code = adapter.run_suite(
                    suite, root, root / "build", root / "out.json", 1
                )
            self.assertEqual(code, 1)
            self.assertEqual(
                [call.args[1] for call in kill.call_args_list],
                [adapter.signal.SIGTERM, adapter.signal.SIGKILL],
            )

    def test_setup_timeout_and_cancellation_are_retryable_without_coverage(self):
        for interruption in [
            subprocess.TimeoutExpired("cmake", 1),
            adapter.SuiteCancelled("cancel"),
        ]:
            with (
                self.subTest(interruption=type(interruption).__name__),
                tempfile.TemporaryDirectory() as directory,
            ):
                root = Path(directory).resolve()
                (root / "d1").mkdir()
                (root / "d1/CMakeLists.txt").write_text("project(fake)")

                class Process:
                    pid = 4321
                    calls = 0

                    def __init__(self, *args, error=interruption, **kwargs):
                        self.error = error

                    def wait(self, **kwargs):
                        self.calls += 1
                        if self.calls == 1:
                            raise self.error
                        return 0

                suite = {
                    "id": "engine:ctest-d1",
                    "kind": "configured-ctest",
                    "build_subdirectory": "engine-d1",
                    "coverage_scope": "native",
                }
                with (
                    patch.object(adapter.subprocess, "Popen", Process),
                    patch.object(adapter.os, "killpg", create=True),
                ):
                    code = adapter.run_suite(
                        suite, root, root / "build", root / "out.json", 1
                    )
                data = json.loads((root / "out.json").read_text())
                self.assertEqual(code, 2)
                self.assertEqual(data["status"], "BLOCKED")
                self.assertTrue(data["retryable"])
                self.assertFalse(data["attempted_tests"])
                self.assertFalse(data["coverage_complete"])
                self.assertEqual(data["cases"], [])


class FingerprintPortableCliTests(unittest.TestCase):
    def test_explicit_matcher_and_fixture_preserve_all_assertions(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            binary = root / "fingerprint_match"
            binary.touch()
            fixture = root / "known_albums.jsonc"
            fixture.write_text('{"chromaprint": "fixture"}')
            results = [
                subprocess.CompletedProcess([], 0, "", "Loaded 1 entries"),
                subprocess.CompletedProcess([], 0, '{"score":1}', ""),
                subprocess.CompletedProcess([], 0, '{"score":1}', ""),
                subprocess.CompletedProcess([], 0, "", ""),
                subprocess.CompletedProcess([], 1, "", ""),
                subprocess.CompletedProcess([], 1, "", ""),
            ]
            with (
                patch.object(
                    sys,
                    "argv",
                    ["strict", "--matcher", str(binary), "--known-discs", str(fixture)],
                ),
                patch.object(strict, "run_matcher", side_effect=results) as run,
                patch.object(strict, "MATCHER", binary),
            ):
                strict.main()
            self.assertEqual(run.call_count, 6)
            self.assertEqual(len(run.call_args_list[-1].args[0]), 4097)

    def test_explicit_missing_matcher_fails_before_launch(self):
        with (
            patch.object(sys, "argv", ["strict", "--matcher", "/does/not/exist"]),
            patch.object(strict, "run_matcher") as run,
            patch.object(strict, "MATCHER", Path("unused")),
            self.assertRaises(SystemExit),
        ):
            strict.main()
        run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
