"""Adapter contracts without executing target native, PowerShell, or device tests."""

import json
import subprocess
import sys
import tempfile
import unittest
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
            self.assertTrue(
                argv[argv.index(flag) + 1].endswith("game_data/shareware/" + game)
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
