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
    def test_registry_ids_are_unique_and_no_arbitrary_commands(self):
        suites = adapter.load_suites()
        self.assertGreater(len(suites), 70)
        for key, suite in suites.items():
            self.assertEqual(key, suite["id"])
            self.assertNotIn("command", suite)
            self.assertIn(
                suite["kind"],
                {"python-unittest", "python-script", "extractor", "configured-ctest"},
            )

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
            root = Path(directory)
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
            root = Path(directory)
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

    def test_native_lazy_setup_preserves_debug_and_serial_environment(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
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


class EvidenceAndCancellationTests(unittest.TestCase):
    def test_expected_failure_and_unexpected_success_remain_visible(self):
        class Cases(unittest.TestCase):
            @unittest.expectedFailure
            def test_expected(self):
                self.fail("known failure")

            @unittest.expectedFailure
            def test_unexpected_success(self):
                pass

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
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
            root = Path(directory)
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
                patch.object(adapter.os, "killpg") as kill,
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
                root = Path(directory)
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
                    patch.object(adapter.os, "killpg"),
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
            root = Path(directory)
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
