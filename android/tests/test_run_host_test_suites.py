"""Adapter contracts without executing target native, PowerShell, or device tests."""

import copy
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
import test_secret_area_liquid_obsidian as missions


class HostAdapterTests(unittest.TestCase):
    def setUp(self):
        # Exercise Linux-only contracts with mocked subprocesses on every CI host.
        self.enterContext(patch.object(adapter.sys, "platform", "linux"))
        self.enterContext(patch.object(adapter.signal, "SIGKILL", 9, create=True))

    def test_nested_corpus_metadata_stops_before_build_or_flattening(self):
        from nightly_regression_catalog import CORPUS_COMMIT
        for profile in ("metadata",):
            with self.subTest(profile=profile), tempfile.TemporaryDirectory() as directory:
                root = Path(directory).resolve()
                baseline = root / "game_data/mission_files/variant.json"
                baseline.parent.mkdir(parents=True)
                baseline.write_text(json.dumps([{"mission_path": "REBIRTH/mission.mn2"}]))
                baseline.with_suffix(".zip").touch()
                baseline.with_suffix(".simulation.json").write_text("[]")
                suite = {"id": "corpus:fixture", "kind": "retail-corpus", "profile": profile,
                         "baseline": "variant.json", "coverage_scope": "synthetic"}
                with patch.dict(adapter.os.environ, {"RETAIL_CHECKOUT_COMMIT": CORPUS_COMMIT}), patch.object(
                        adapter.subprocess, "Popen") as launch:
                    code = adapter.run_suite(suite, root, root / "build", root / "receipt.json", 30)
                receipt = json.loads((root / "receipt.json").read_text())
                self.assertEqual(code, 2)
                self.assertEqual(receipt["blocker_kind"], "target-capability")
                self.assertFalse(receipt["retryable"])
                self.assertFalse(receipt["attempted_tests"])
                self.assertTrue(receipt["assets"]["archive"])
                self.assertTrue(receipt["assets"]["route_oracle"])
                launch.assert_not_called()

    def test_exact_nested_corpus_target_stages_only_its_payload_before_build(self):
        from nightly_regression_catalog import CORPUS_COMMIT
        for target in (1, 0):
            with self.subTest(target=target), tempfile.TemporaryDirectory() as directory:
                root = Path(directory).resolve()
                baseline = root / "game_data/mission_files/fixture.json"
                baseline.parent.mkdir(parents=True)
                rows = [{"target_index": i, "mission_filename": "m.mn2", "mission_path": str(i) + "/m.mn2",
                         "levels": [{"level_num": 1, "level_file": "one.rl2"}]} for i in (0, 1)]
                baseline.write_text(json.dumps(rows))
                baseline.with_suffix(".simulation.json").write_text("[]")
                with zipfile.ZipFile(baseline.with_suffix(".zip"), "w") as archive:
                    for i in (1, 0):
                        archive.writestr(str(i) + "/m.mn2", "name=fixture")
                        archive.writestr(str(i) + "/m.hog", str(i))
                suite = {"id": "corpus:fixture", "kind": "retail-corpus", "profile": "route",
                         "baseline": "fixture.json", "target_index": target, "level_num": 1,
                         "level_file": "one.rl2", "coverage_scope": "synthetic"}
                with (patch.dict(adapter.os.environ, {"RETAIL_CHECKOUT_COMMIT": CORPUS_COMMIT}),
                      patch.object(adapter, "verify_retail_stage"), patch.object(adapter.subprocess, "Popen") as launch):
                    launch.return_value.wait.return_value = 9
                    self.assertEqual(adapter.run_suite(suite, root, root / "build", root / "receipt.json", 30), 2)
                self.assertEqual((root / "receipt.artifacts/exact-mission/missions/m.hog").read_text(), str(target))
                receipt = json.loads((root / "receipt.json").read_text())
                self.assertEqual(receipt["blocker_kind"], "metadata-cli-setup")
                self.assertFalse(receipt["coverage_complete"])

    def test_corpus_setup_uses_host_jvm_and_reports_static_failure_stage(self):
        from nightly_regression_catalog import CORPUS_COMMIT
        for profile in ("metadata", "route"):
            for failure_at, kind in ((0, "metadata-cli-setup"),
                                     (1, "corpus-worker-configure"),
                                     (2, "corpus-worker-build"),
                                     (3, "corpus-worker-configure"),
                                     (4, "corpus-worker-build")):
                with (self.subTest(profile=profile, failure_at=failure_at),
                      tempfile.TemporaryDirectory() as directory):
                    root = Path(directory).resolve()
                    baseline = root / "game_data/mission_files/fixture.json"
                    baseline.parent.mkdir(parents=True)
                    baseline.write_text(json.dumps([{"mission_path": "fixture.mn2", "levels": [{"level_num": 1, "level_file": "one.rl2"}]}]))
                    baseline.with_suffix(".zip").touch()
                    baseline.with_suffix(".simulation.json").write_text("[]")
                    suite = {"id": "corpus:fixture", "kind": "retail-corpus", "profile": profile,
                             "baseline": "fixture.json", "target_index": 0, "level_num": 1, "level_file": "one.rl2", "coverage_scope": "synthetic"}
                    with (patch.dict(adapter.os.environ, {"RETAIL_CHECKOUT_COMMIT": CORPUS_COMMIT}),
                          patch.object(adapter, "verify_retail_stage"),
                          patch("nightly_corpus_stage.stage_route", return_value=root / "stage"),
                          patch.object(adapter.subprocess, "Popen") as launch):
                        launch.return_value.wait.side_effect = [0] * failure_at + [9]
                        code = adapter.run_suite(suite, root, root / "build", root / "receipt.json", 30)
                    receipt = json.loads((root / "receipt.json").read_text())
                    self.assertEqual(launch.call_args_list[0].args[0], [
                        "bash", str(root / "android/gradlew"), "-p", str(root / "android"),
                        "-PhostJvmOnly=true", ":mission-metadata-cli:installDist", "--console=plain"])
                    self.assertEqual(launch.call_count, failure_at + 1)
                    self.assertEqual(code, 2)
                    self.assertEqual(receipt["phase"], "setup")
                    self.assertEqual(receipt["blocker_kind"], kind)
                    self.assertEqual(receipt["commands"][-1]["exit_code"], 9)
                    self.assertEqual(receipt["commands"][-1]["outcome"], "completed")
                    self.assertIn("stdout_bytes", receipt["commands"][-1])
                    self.assertIn("stderr_bytes", receipt["commands"][-1])
                    self.assertEqual(receipt["exit_code"], 9)
                    self.assertTrue(receipt["retryable"])
                    self.assertFalse(receipt["attempted_tests"])
                    self.assertFalse(receipt["coverage_complete"])
                    self.assertEqual(receipt["cases"], [])

    def test_d1_corpus_route_uses_verified_automatic_combined_data_and_retains_early_failure(self):
        from nightly_regression_catalog import CORPUS_COMMIT
        import nightly_retail

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            baseline = root / "game_data/mission_files/fixture.json"
            baseline.parent.mkdir(parents=True)
            baseline.write_text(
                json.dumps(
                    [
                        {
                            "game": "d1",
                            "mission_filename": "fixture.msn",
                            "levels": [{"level_num": 1, "level_file": "one.rdl"}],
                        }
                    ]
                )
            )
            baseline.with_suffix(".zip").touch()
            baseline.with_suffix(".simulation.json").write_text("[]")
            suite = {
                "id": "corpus:fixture",
                "kind": "retail-corpus",
                "profile": "route",
                "baseline": "fixture.json",
                "target_index": 0,
                "level_num": 1,
                "level_file": "one.rdl",
                "identity": "fixture.json|0|1|one.rdl",
                "coverage_scope": "synthetic",
            }
            with (
                patch.dict(adapter.os.environ, {"RETAIL_CHECKOUT_COMMIT": CORPUS_COMMIT}),
                patch.object(adapter, "verify_retail_stage"),
                patch("nightly_corpus_stage.stage_route", return_value=root / "stage"),
                patch.object(adapter.subprocess, "check_output", return_value="a" * 40),
                patch.object(adapter.subprocess, "Popen") as launch,
            ):

                def process(argv, **kwargs):
                    if "-HeadlessExecutable" in argv:
                        self.assertNotIn("-D1InD2HogDir", argv)
                        self.assertEqual(argv[argv.index("-HogDir") + 1], str(root / "game_data/retail-ci/d2"))
                        kwargs["stderr"].write("D1-in-D2 data directory is missing PRIVATE_PATH SECRET")
                    return launch.return_value

                launch.side_effect = process
                launch.return_value.wait.side_effect = [0] * 5 + [1]
                code = adapter.run_suite(suite, root, root / "build", root / "receipt.json", 30)
            receipt = json.loads((root / "receipt.json").read_text())
            self.assertEqual(launch.call_count, 6)
            self.assertEqual(code, 2)
            self.assertFalse(receipt["coverage_complete"])
            clean = nightly_retail.public_diagnostics(receipt)
            self.assertFalse(clean["assets"]["route_summary"])
            self.assertEqual(clean["owner_failure"]["categories"], ["route-base-data"])
            self.assertEqual(clean["owner_failure"]["exit_code"], 1)
            self.assertNotIn("PRIVATE", str(clean))
            self.assertNotIn("SECRET", str(clean))

    def test_real_nonzero_process_retains_both_streams_and_typed_receipt(self):
        suite = {"id": "fixture", "kind": "python-script", "path": "fixture.py", "coverage_scope": "fixture"}
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            (root / "fixture.py").write_text(
                "import sys; print('stdout-marker'); print('stderr-marker', file=sys.stderr); sys.exit(7)"
            )
            code = adapter.run_suite(suite, root, root / "build", root / "out.json", 30)
            row = json.loads((root / "out.json").read_text())
            self.assertEqual(code, 1)
            self.assertEqual(row["commands"][-1]["exit_code"], 7)
            self.assertGreater(row["commands"][-1]["stdout_bytes"], 0)
            self.assertGreater(row["commands"][-1]["stderr_bytes"], 0)
            self.assertIn("stdout-marker", Path(row["log"]).read_text())
            self.assertIn("stderr-marker", Path(row["log"]).read_text())

    @unittest.skipUnless(sys.platform == "linux", "Real process-group timeout requires Linux")
    def test_real_timeout_retains_partial_output_and_signal_exit(self):
        suite = {"id": "fixture", "kind": "python-script", "path": "fixture.py", "coverage_scope": "fixture"}
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            (root / "fixture.py").write_text("import time; print('before-timeout', flush=True); time.sleep(30)")
            self.assertEqual(adapter.run_suite(suite, root, root / "build", root / "out.json", 1), 1)
            row = json.loads((root / "out.json").read_text())
            command = row["commands"][-1]
            self.assertEqual(command["outcome"], "timeout")
            self.assertLess(command["exit_code"], 0)
            self.assertGreater(command["stdout_bytes"], 0)
            self.assertFalse(row["coverage_complete"])

    def test_launch_exception_survives_private_projection(self):
        import nightly_retail

        suite = adapter.load_suites()["python:test_music_spectra.py"]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            source = root / suite["path"]
            source.parent.mkdir(parents=True)
            source.write_text("")
            with (
                patch.object(adapter.importlib.util, "find_spec", return_value=object()),
                patch.object(adapter.subprocess, "Popen", side_effect=OSError("PRIVATE_MARKER")),
            ):
                adapter.run_suite(suite, root, root / "build", root / "receipt.json", 30)
            row = json.loads((root / "receipt.json").read_text())
            self.assertEqual(row["commands"][-1]["outcome"], "exception")
            clean = nightly_retail.public_diagnostics(row)
            self.assertNotIn("PRIVATE_MARKER", json.dumps(clean))
            self.assertEqual(clean["commands"][-1]["outcome"], "exception")

    def test_spectral_suite_blocks_missing_matplotlib_before_tests(self):
        suite = adapter.load_suites()["python:test_music_spectra.py"]
        self.assertEqual(set(suite["python_modules"]), {"numpy", "scipy", "imageio_ffmpeg", "matplotlib"})
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            with (
                patch.object(
                    adapter.importlib.util,
                    "find_spec",
                    side_effect=lambda name: None if name == "matplotlib" else object(),
                ),
                patch.object(adapter.subprocess, "Popen") as launch,
            ):
                code = adapter.run_suite(suite, root, root / "build", root / "receipt.json", 30)
            receipt = json.loads((root / "receipt.json").read_text())
            self.assertEqual(code, 2)
            self.assertEqual(receipt["status"], "BLOCKED")
            self.assertEqual(receipt["reason"], "Missing prerequisites: matplotlib")
            self.assertEqual(receipt["phase"], "preflight")
            self.assertFalse(receipt["attempted_tests"])
            self.assertFalse(receipt["coverage_complete"])
            launch.assert_not_called()

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
                    "retail-corpus",
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

    def test_only_blocked_leaves_are_blocked(self):
        for cases in ([{"status": "BLOCKED"}],
                      [{"status": "BLOCKED"}, {"status": "SKIP"}]):
            with self.subTest(cases=cases):
                self.assertEqual(adapter.classify_cases(cases)[0], "BLOCKED")
                self.assertEqual(adapter.count_cases(cases)["passed"], 0)

    def test_partial_structured_counts_preserve_success_and_exclusions(self):
        cases = ([{"status": "PASS"}] * 54
                 + [{"status": "BLOCKED"}] * 2 + [{"status": "SKIP"}])
        self.assertEqual(adapter.classify_cases(cases)[0], "PASS")
        self.assertEqual(adapter.count_cases(cases), {
            "total": 57, "passed": 54, "failed": 0, "skipped": 1, "excluded": 3,
        })

    def test_unittest_receipts_require_successful_leaves(self):
        modules = [
            ("", "BLOCKED", 2, 0, 0, 0),
            ("    @unittest.skip('fixture absent')\n"
             "    def test_skip(self): pass\n", "BLOCKED", 2, 0, 1, 0),
            ("    @unittest.expectedFailure\n"
             "    def test_expected(self): self.fail('known')\n",
             "BLOCKED", 2, 0, 1, 0),
            ("    def test_pass(self): pass\n", "PASS", 0, 1, 0, 0),
            ("    def test_pass(self): pass\n"
             "    @unittest.skip('fixture absent')\n"
             "    def test_skip(self): pass\n", "PASS", 0, 1, 1, 0),
            ("    def test_fail(self): self.fail('broken')\n", "FAIL", 1, 0, 0, 1),
        ]
        for body, status, expected_code, passed, skipped, failed in modules:
            with self.subTest(body=body), tempfile.TemporaryDirectory() as directory:
                root = Path(directory).resolve()
                (root / "test_fixture.py").write_text(
                    "import unittest\nclass Cases(unittest.TestCase):\n"
                    + (body or "    pass\n")
                )
                suite = {
                    "id": "fixture", "kind": "python-unittest",
                    "path": "test_fixture.py", "coverage_scope": "fixture",
                }
                code = adapter.run_suite(
                    suite, root, root / "build", root / "receipt.json", 30
                )
                receipt = json.loads((root / "receipt.json").read_text())
                self.assertEqual(code, expected_code)
                self.assertEqual(receipt["status"], status)
                self.assertEqual(receipt["counts"], {
                    "total": passed + skipped + failed, "passed": passed,
                    "failed": failed, "skipped": skipped, "excluded": skipped,
                })
                self.assertEqual(receipt["coverage_complete"],
                                 status == "PASS" and not skipped)

    def test_script_owner_pass_does_not_claim_unknown_leaf_coverage(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            (root / "fixture.py").write_text("assert 1 + 1 == 2\n")
            suite = {
                "id": "fixture", "kind": "python-script",
                "path": "fixture.py", "coverage_scope": "fixture",
            }
            code = adapter.run_suite(
                suite, root, root / "build", root / "receipt.json", 30
            )
            receipt = json.loads((root / "receipt.json").read_text())
            self.assertEqual(code, 0)
            self.assertEqual(receipt["status"], "PASS")
            self.assertTrue(receipt["attempted_tests"])
            self.assertFalse(receipt["retryable"])
            self.assertFalse(receipt["coverage_complete"])
            self.assertNotIn("counts", receipt)
            self.assertEqual(receipt["exclusions"], [])
            self.assertEqual(receipt["cases"], [])
            self.assertIn("Aggregate", receipt["reason"])

    def test_strict_fingerprint_owner_has_no_fabricated_leaf_count(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            for relative, content in (
                ("android/app/src/main/cpp/extract/CMakeLists.txt", "project(fake)"),
                ("android/tests/test_fingerprint_match_strict.py", "# --matcher"),
                ("android/app/src/main/assets/known_albums.jsonc", "{}"),
            ):
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content)

            class Process:
                def __init__(self, *args, **kwargs):
                    pass

                def wait(self, **kwargs):
                    return 0

            with (
                patch.object(adapter.shutil, "which", return_value="tool"),
                patch.object(adapter.subprocess, "Popen", Process),
            ):
                code = adapter.run_suite(
                    adapter.load_suites()["audio:strict-fingerprint"], root,
                    root / "build", root / "receipt.json", 30,
                )
            receipt = json.loads((root / "receipt.json").read_text())
            self.assertEqual(code, 0)
            self.assertEqual(receipt["status"], "PASS")
            self.assertTrue(receipt["attempted_tests"])
            self.assertFalse(receipt["retryable"])
            self.assertFalse(receipt["coverage_complete"])
            self.assertEqual(receipt["cases"], [])
            self.assertEqual(receipt["exclusions"], [])
            self.assertNotIn("counts", receipt)

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
        self.assertEqual(receipt["status"], "PASS")
        self.assertEqual(receipt["cases"], [])

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
                self.assertFalse(receipt["coverage_complete"])
                self.assertTrue(receipt["attempted_tests"])
                self.assertEqual(receipt["cases"], [])
                self.assertNotIn("counts", receipt)

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
                self.assertEqual(receipt["cases"], [])
                self.assertFalse(receipt["coverage_complete"])
                if owner_code == 2:
                    self.assertTrue(receipt["retryable"])
                    self.assertEqual(len(receipt["exclusions"]), 1)
                    self.assertEqual(receipt["exclusions"][0]["status"], "BLOCKED")
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

    def test_expansion_profiles_require_mission_stage(self):
        suites = adapter.load_suites()
        expansions = [
            suite
            for suite in suites.values()
            if suite.get("fixture") == "retail-missions"
        ]
        self.assertEqual(
            {suite["profile"] for suite in expansions},
            {"retail-vertigo-metadata", "retail-obsidian-liquid", "retail-xl-originals"},
        )
        for suite in expansions:
            self.assertEqual(suite["games"], [] if suite["profile"] == "retail-xl-originals" else ["d2"])
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
                self.assertEqual(calls[1][-3:], [
                    "dxx-redux-d2-headless-metadata", "dxx-redux-d2-metadata-worker",
                    "dxx-redux-d2-headless-route",
                ])
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
                        "--worker",
                        str(root / "build/engine-d2/main/dxx-redux-d2-metadata-worker"),
                        "--route-exe",
                        str(root / "build/engine-d2/main/dxx-redux-d2-headless-route"),
                        "--vertigo-dir",
                        str(root / "game_data/retail-ci/missions"),
                    ],
                )
                self.assertFalse(receipt["coverage_complete"])
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


class MissionLifecycleOwnerTests(unittest.TestCase):
    DESCRIPTOR = "name = Synthetic\nnum_levels = 2\none.rl2\ntwo.rl2\nnum_secrets = 1\nsecret.rl2,1\n"
    WORKER = '''import json, sys
mode = sys.argv[1]
for index, line in enumerate(sys.stdin, 1):
    request = json.loads(line)
    with open('requests.jsonl', 'a') as log: log.write(line)
    level = dict.fromkeys(('segment_count', 'texture_count', 'wall_count', 'trigger_count',
                          'object_count', 'robots', 'hostages', 'powerups', 'reactors'), 1)
    level.update(status='ok', level_num=request['level_num'], level_name='Synthetic',
                 level_file=request['level_file'])
    report = dict(schema='dxx-level-metadata-v1', request_id=request['request_id'], status='ok',
                  game='d2', source=request['source_name'], mission_filename=request['mission_name'],
                  levels=[level])
    if mode == 'missing': sys.exit(0)
    if mode == 'foreign': report['request_id'] = 'foreign'
    if mode == 'mission': report['mission_filename'] = 'foreign'
    if mode == 'level': level['level_num'] = 77
    if mode == 'filename': level['level_file'] = 'foreign.rl2'
    if mode == 'empty': report['levels'] = []
    if mode == 'failed': level['status'] = 'error'
    if mode == 'restart': report['worker_restart_required'] = True
    if mode == 'zero': level['texture_count'] = 0
    if mode == 'base-count' and index > 1 and request['mission_name'] == 'd2':
        level['object_count'] = 99
    print('DXXMETA\\t' + json.dumps(report), flush=True)
    if mode == 'duplicate': print('DXXMETA\\t' + json.dumps(report), flush=True)
if mode == 'trailing': print('DXXMETA\\t' + json.dumps(report), flush=True)
if mode == 'nonzero': sys.exit(1)
'''

    def test_descriptor_counts_and_secret_entries_are_complete(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "synthetic.mn2"
            path.write_text(self.DESCRIPTOR + "\x1a", encoding="ascii")
            self.assertEqual(missions.mission_levels(path),
                             [(1, "one.rl2"), (2, "two.rl2"), (-1, "secret.rl2")])
            malformed = (
                self.DESCRIPTOR.replace("num_levels = 2", "num_levels = -1"),
                self.DESCRIPTOR.replace("num_levels = 2", "num_levels = 101"),
                self.DESCRIPTOR.replace("num_levels = 2", "num_levels = 0"),
                self.DESCRIPTOR.replace("num_levels = 2", "num_levels = two"),
                self.DESCRIPTOR + "num_levels = 1\nother.rl2\n",
                self.DESCRIPTOR.replace("num_secrets = 1\nsecret.rl2,1\n", ""),
                self.DESCRIPTOR.replace("secret.rl2,1\n", ""),
                self.DESCRIPTOR.replace("secret.rl2,1", "secret.rl2,3"),
                self.DESCRIPTOR.replace("secret.rl2,1", "secret.rl2"),
                self.DESCRIPTOR.replace("two.rl2", "ONE.RL2"),
                self.DESCRIPTOR.replace("one.rl2", "../one.rl2"),
            )
            for descriptor in malformed:
                with self.subTest(descriptor=descriptor), self.assertRaises(AssertionError):
                    path.write_text(descriptor)
                    missions.mission_levels(path)

    def test_one_worker_sequence_rejects_missing_foreign_duplicate_and_mismatched_results(self):
        popen = subprocess.Popen
        modes = ("ok", "missing", "foreign", "mission", "level", "filename", "empty",
                 "failed", "restart", "zero", "base-count", "duplicate", "trailing", "nonzero", "timeout")
        for mode in modes:
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                worker = root / "worker.py"
                worker.write_text(self.WORKER)
                for name in ("d2x", "obsidian"):
                    (root / (name + ".mn2")).write_text(self.DESCRIPTOR)
                processes = []

                def launch(argv, *, worker=worker, mode=mode, processes=processes, **kwargs):
                    process = popen([sys.executable, str(worker), mode], **kwargs)
                    processes.append(process)
                    return process

                with patch.object(missions.subprocess, "Popen", side_effect=launch), patch("builtins.print"):
                    if mode == "ok":
                        missions.check_loading(worker, root, (("d2x", root), ("obsidian", root)), root)
                        requests = [json.loads(line) for line in (root / "requests.jsonl").read_text().splitlines()]
                        self.assertEqual([(request["mission_name"], request["level_num"]) for request in requests],
                                         [("d2", 1), ("d2x", 1), ("d2x", 2), ("d2x", -1), ("d2", 1),
                                          ("obsidian", 1), ("obsidian", 2), ("obsidian", -1), ("d2", 1)])
                        self.assertEqual(["extra_data_dir" in request for request in requests],
                                         [False, True, True, True, False, True, True, True, False])
                    elif mode == "timeout":
                        with (patch.object(missions.queue.Queue, "get", side_effect=missions.queue.Empty),
                              self.assertRaises(TimeoutError)):
                            missions.check_loading(worker, root, (("d2x", root), ("obsidian", root)), root)
                    else:
                        with self.assertRaises(AssertionError):
                            missions.check_loading(worker, root, (("d2x", root), ("obsidian", root)), root)
                self.assertEqual(len(processes), 1)
                self.assertIsNotNone(processes[0].poll())

    @staticmethod
    def route_report(mission, *, restored=False):
        report = {
            "schema": "dxx-guidebot-route-confirmation-v1", "mission": mission,
            "level": 1, "status": "confirmed", "frames": 12, "seed": 1,
            "fixed_hz": 60, "difficulty": 2, "radius": {"player": 42, "effective": 42},
            "objectives": [{"kind": "exit", "activation_kind": "enter_exit", "frame": 12}],
        }
        if restored:
            report["start_state"] = {"kind": "saved_world", "segment": 3,
                                     "key_flags": 6, "position_fixed": [1, 2, 3]}
        else:
            report["route_confirmation"] = {"status": "confirmed"}
        return report

    def test_route_requires_current_json_success_identity_and_native_confirmation(self):
        failures = (
            ("absent", None), ("status", "failed"), ("mission", "wrong"), ("level", 2),
            ("frames", 0), ("objectives", []), ("route_confirmation", {"status": "failed"}),
            ("radius", {"player": 42, "effective": 1}),
        )
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "route.json"
            success = subprocess.CompletedProcess([], 0, "ROUTE-CONFIRM OK", "")
            report = self.route_report("d2x")
            output.write_text(json.dumps(report))
            missions.validate_route(success, output, "d2x")
            # A PASS-looking JSON and stdout cannot rescue a failing process
            with self.assertRaises(AssertionError):
                missions.validate_route(subprocess.CompletedProcess([], 1, success.stdout, ""), output, "d2x")
            for key, value in failures:
                with self.subTest(key=key), self.assertRaises((AssertionError, FileNotFoundError, IndexError)):
                    changed = copy.deepcopy(report)
                    if key == "absent":
                        output.unlink()
                    else:
                        changed[key] = value
                        output.write_text(json.dumps(changed))
                    missions.validate_route(success, output, "d2x")
            output.write_text(json.dumps(report))
            with self.assertRaises(KeyError):
                missions.validate_route(success, output, "d2x", restored=True)
            restored = self.route_report("d2x", restored=True)
            output.write_text(json.dumps(restored))
            missions.validate_route(success, output, "d2x", restored=True)
            for key in ("segment", "key_flags"):
                with self.subTest(default_state=key), self.assertRaises(AssertionError):
                    changed = copy.deepcopy(restored)
                    changed["start_state"][key] = 0
                    output.write_text(json.dumps(changed))
                    missions.validate_route(success, output, "d2x", restored=True)

    def test_checkpoint_negative_requires_the_expected_guard_and_no_success_artifact(self):
        guard = "ROUTE-CONFIRM FAIL checkpoint restore or mission/level mismatch"
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "rejected.json"
            missions.validate_route(subprocess.CompletedProcess([], 1, "", guard), output,
                                    "d2x", rejected=True)
            for code, log in ((0, guard), (2, guard), (-11, guard), (1, "mission load failed"),
                              (1, guard + "\nROUTE-CONFIRM OK")):
                with self.subTest(code=code, log=log), self.assertRaises(AssertionError):
                    missions.validate_route(subprocess.CompletedProcess([], code, "", log), output,
                                            "d2x", rejected=True)
            output.write_text(json.dumps(self.route_report("d2x")))
            with self.assertRaises(AssertionError):
                missions.validate_route(subprocess.CompletedProcess([], 1, "", guard), output,
                                        "d2x", rejected=True)

    def test_route_sequence_saves_restores_rejects_mismatches_and_preserves_checkpoint(self):
        for fault in (None, "missing-save", "changed-save"):
            with self.subTest(fault=fault), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                calls = []
                installed = tuple((name, root / (name + "-input") / "missions")
                                  for name in ("d2x", "obsidian"))

                def run(argv, *, calls=calls, fault=fault, root=root, **kwargs):
                    calls.append(argv)
                    self.assertEqual(kwargs["timeout"], 60)
                    target = argv[argv.index("-mission") + 1]
                    output = Path(argv[argv.index("-route-confirm-json-out") + 1])
                    # Route discovery mounts the parent of the descriptor directory
                    self.assertEqual(argv[argv.index("-extra-dir") + 1],
                                     str((root / (output.parent.name + "-input")).resolve()))
                    save = Path(argv[argv.index("-route-confirm-user-dir") + 1]) / "level1.sav"
                    phase = output.stem
                    if phase.startswith("wrong-"):
                        return subprocess.CompletedProcess(argv, 1, "", "ROUTE-CONFIRM FAIL checkpoint restore or mission/level mismatch")
                    if phase == "start" and fault != "missing-save":
                        save.write_bytes(b"synthetic checkpoint")
                    if phase == "restore" and fault == "changed-save":
                        save.write_bytes(b"modified checkpoint")
                    output.write_text(json.dumps(self.route_report(target, restored=phase == "restore")))
                    return subprocess.CompletedProcess(argv, 0, "ROUTE-CONFIRM OK", "")

                with patch.object(missions.subprocess, "run", side_effect=run), patch("builtins.print"):
                    if fault:
                        with self.assertRaises(AssertionError):
                            missions.check_routes(root / "native", root, installed, root)
                    else:
                        missions.check_routes(root / "native", root, installed, root)
                        self.assertEqual(len(calls), 8)
                        self.assertEqual([argv[argv.index("-mission") + 1] for argv in calls],
                                         ["d2x", "d2x", "d2x", "d2", "obsidian", "obsidian", "obsidian", "d2"])
                        self.assertEqual([argv[argv.index("-level") + 1] for argv in calls],
                                         ["1", "1", "2", "1"] * 2)

    def test_route_requires_native_mission_directory_layout_before_launch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(missions.subprocess, "run") as run, self.assertRaises(AssertionError):
                missions.check_routes(root / "native", root, (("d2x", root),), root)
            run.assert_not_called()

    def test_extended_arguments_are_all_or_none_and_default_remains_liquid_only(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "liquid.json"
            base = ["--exe", "metadata", "--hogdir", str(root), "--mission-dir", str(root),
                    "--output", str(output)]
            for flags in (["--worker", "worker"], ["--route-exe", "route"],
                          ["--vertigo-dir", str(root)], ["--worker", "worker", "--route-exe", "route"]):
                with self.subTest(flags=flags), patch("sys.stderr"), patch.object(missions.subprocess, "run") as run:
                    with self.assertRaises(SystemExit) as error:
                        missions.main(base + flags)
                    self.assertEqual(error.exception.code, 2)
                    run.assert_not_called()

            def liquid_run(argv, **kwargs):
                output.write_text("{}")
                return subprocess.CompletedProcess(argv, 0, "", "")

            with (patch.object(missions.subprocess, "run", side_effect=liquid_run) as run,
                  patch.object(missions, "validate") as validate,
                  patch.object(missions, "check_loading") as loading,
                  patch.object(missions, "check_routes") as routes, patch("builtins.print")):
                missions.main(base)
            run.assert_called_once()
            validate.assert_called_once_with({})
            loading.assert_not_called()
            routes.assert_not_called()

    def test_expanded_estimate_matches_policy_without_changing_the_lane_budget(self):
        suite = adapter.load_suites()["host-profile:retail-obsidian-liquid"]
        policy = json.loads(Path(__file__).with_name("nightly_tooling_policy.json").read_text())
        entry = next(item for item in policy["suites"] if item["id"] == suite["id"])
        self.assertEqual(suite["estimated_seconds"], 600)
        self.assertEqual(entry["estimated_seconds"], suite["estimated_seconds"])
        self.assertEqual(entry["fixture"], "retail-missions")
        self.assertLessEqual(entry["estimated_seconds"], 2700)
        self.assertIn("Vertigo/Obsidian loading and checkpoint regressions", suite["coverage_scope"])

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
    def test_ctest_null_and_excluded_only_receipts_cannot_pass(self):
        for leaves, expected_code, passed, excluded, skipped in [
            ("", 2, 0, 0, 0),
            ('<testcase name="skip"><skipped/></testcase>', 2, 0, 1, 1),
            ('<testcase name="blocked"><system-out>SKIP: sample media not present'
             '</system-out></testcase>', 2, 0, 1, 0),
            ('<testcase name="pass"/><testcase name="blocked"><system-out>'
             'SKIP: sample media not present</system-out></testcase>', 0, 1, 1, 0),
        ]:
            with self.subTest(leaves=leaves), tempfile.TemporaryDirectory() as directory:
                root = Path(directory).resolve()
                suite = adapter.load_suites()["cmake:etc2-limits"]
                source = root / suite["source"]
                source.mkdir(parents=True)
                (source / "CMakeLists.txt").touch()

                class Process:
                    def __init__(self, argv, **kwargs):
                        if argv[0] == "ctest":
                            Path(argv[argv.index("--output-junit") + 1]).write_text(
                                "<testsuite>" + leaves + "</testsuite>"
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
                receipt = json.loads((root / "receipt.json").read_text())
                self.assertEqual(code, expected_code)
                self.assertEqual(receipt["status"], "PASS" if passed else "BLOCKED")
                self.assertFalse(receipt["coverage_complete"])
                self.assertEqual(receipt["counts"], {
                    "total": passed + excluded, "passed": passed, "failed": 0,
                    "skipped": skipped, "excluded": excluded,
                })

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
                        durable = json.loads((root / "out.json").read_text())
                        if not durable["attempted_tests"]:
                            raise AssertionError("Running test launch was not persisted")
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
                command = data['commands'][-1]
                self.assertEqual(command['outcome'], 'timeout' if isinstance(interruption, subprocess.TimeoutExpired) else 'cancelled')
                self.assertIn('duration_seconds', command)
                self.assertIn('stdout_bytes', command)
                self.assertIn('stderr_bytes', command)


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
