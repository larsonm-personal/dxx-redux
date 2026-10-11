#!/usr/bin/env python3
# TEST-SUPPORT: owner=test_native_host_unit_tests
"""Exercise the real Linux runner in isolated repositories with recording tool shims."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
BASH = shutil.which("bash")
REVISION = "1234567890abcdef1234567890abcdef12345678"
TOOL_SHIM = r"""
import json
import os
from pathlib import Path
import sys

args = sys.argv[1:]
name = Path(sys.argv[0]).name
if name == "git":
    print(os.environ["FIXTURE_REVISION"])
    sys.exit(0)
if name == "ctest":
    stage = "ctest"
    build_dir = Path(args[args.index("--test-dir") + 1])
elif "--build" in args:
    stage = "build"
    build_dir = Path(args[args.index("--build") + 1])
else:
    stage = "configure"
    build_dir = Path(args[args.index("-B") + 1])
with open(os.environ["FIXTURE_LOG"], "a", encoding="utf-8") as log:
    log.write(json.dumps({
        "stage": stage, "tool": sys.argv[0], "args": args,
        "cwd": os.getcwd(), "build_dir": str(build_dir),
        "audio": os.environ.get("SDL_AUDIODRIVER"),
        "video": os.environ.get("SDL_VIDEODRIVER"),
    }) + "\n")
if os.environ.get("FIXTURE_FAIL_STAGE") == stage:
    print("fixture " + stage + " failure", file=sys.stderr)
    sys.exit(23)
if stage == "configure":
    build_dir.mkdir(parents=True, exist_ok=True)
elif stage == "ctest":
    if os.environ.get("FIXTURE_NO_TESTS") == "1":
        print("No tests were found!!!", file=sys.stderr)
        sys.exit(8 if "--no-tests=error" in args else 0)
    Path(args[args.index("--output-junit") + 1]).write_text(
        '<testsuite tests="1" failures="0"/>\n', encoding="utf-8"
    )
"""


@unittest.skipUnless(sys.platform.startswith("linux") and BASH, "requires Linux and bash")
class LinuxBuildCliTest(unittest.TestCase):
    def setUp(self):
        temp_root = ROOT / "android" / "temp"
        temp_root.mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="linux_cli_", dir=temp_root)
        self.addCleanup(self.temporary.cleanup)
        self.work = Path(self.temporary.name)
        self.repo = self.work / "fixture repo"
        self.repo.mkdir()
        self.runner = self.repo / "run-linux-build.sh"
        shutil.copyfile(ROOT / "run-linux-build.sh", self.runner)
        for game in ("d1", "d2"):
            (self.repo / game).mkdir()
        helpers = self.repo / "android" / "get_deps" / "helpers"
        helpers.mkdir(parents=True)
        (helpers / "platform.sh").write_text('assert_dependency_disk_space() { [[ "$2" == 4 ]]; }\n', encoding="utf-8")
        self.log = self.work / "calls.jsonl"
        self.bin = self.work / "host tools"
        self.bin.mkdir()
        # Keep PATH isolated so a missing tool can never fall through to a real build
        for name in ("cat", "dirname", "head", "tr", "sed", "find", "sort", "rm", "mkdir"):
            executable = shutil.which(name)
            self.assertIsNotNone(executable, name)
            (self.bin / name).symlink_to(executable)
        for name in ("cmake", "ctest", "ninja", "git"):
            self.write_tool(self.bin / name)
        self.env = dict(os.environ)
        self.env.update(
            PATH=str(self.bin),
            FIXTURE_LOG=str(self.log),
            FIXTURE_REVISION=REVISION,
            SDL_AUDIODRIVER="fixture-audio",
            SDL_VIDEODRIVER="fixture-video",
        )
        for name in ("FIXTURE_FAIL_STAGE", "FIXTURE_NO_TESTS", "BASH_ENV", "ENV"):
            self.env.pop(name, None)

    def write_tool(self, path):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(f"#!{sys.executable}\n{TOOL_SHIM}", encoding="utf-8")
        path.chmod(0o755)
        return path

    def run_cli(self, *args, success=True, env=None):
        self.log.unlink(missing_ok=True)
        result = subprocess.run(
            [BASH, str(self.runner), *args],
            cwd=self.work,
            env={**self.env, **(env or {})},
            capture_output=True,
            text=True,
            timeout=15,
            check=False,
        )
        output = result.stdout + result.stderr
        if success:
            self.assertEqual(result.returncode, 0, output)
        else:
            self.assertNotEqual(result.returncode, 0, output)
            self.assertNotIn("Linux build complete", output)
            self.assertNotIn("Linux build and tests complete", output)
        self.calls = (
            [json.loads(line) for line in self.log.read_text(encoding="utf-8").splitlines()]
            if self.log.exists()
            else []
        )
        return result

    def assert_option(self, call, option, value):
        self.assertIn(option, call["args"])
        self.assertEqual(call["args"][call["args"].index(option) + 1], value)

    def seed_stamps(self):
        stamps = self.repo / "temp" / "input_demo_build_stamps"
        stamps.mkdir(parents=True)
        for name in ("d1", "d2", "d1-asan", "d2-asan"):
            (stamps / f"{name}.stamp").write_text(f"original-{name}", encoding="utf-8")
        return {path: path.read_bytes() for path in stamps.iterdir()}

    def test_build_only_defaults_and_replay_stamps(self):
        self.run_cli()
        self.assertEqual([c["stage"] for c in self.calls], ["configure", "build"] * 2)
        for game, (configure, build) in zip(("d1", "d2"), (self.calls[:2], self.calls[2:])):
            self.assertEqual(configure["build_dir"], str(self.repo / f"build{game}"))
            self.assertIn("CMAKE_BUILD_TYPE=RelWithDebInfo", configure["args"])
            self.assertNotIn("BUILD_TESTING=ON", configure["args"])
            self.assertIn("DXX_SANITIZERS=", configure["args"])
            self.assert_option(build, "--config", "RelWithDebInfo")
            self.assertEqual(build["args"][-1], "--parallel")
            self.assertEqual(build["audio"], "fixture-audio")
            stamp = self.repo / "temp" / "input_demo_build_stamps" / f"{game}.stamp"
            self.assertEqual(stamp.read_text(encoding="utf-8"), REVISION)

    def test_test_mode_runs_both_serially_with_reports_and_preserves_stamps(self):
        stamps = self.seed_stamps()
        self.run_cli("--test")
        self.assertEqual([c["stage"] for c in self.calls], ["configure", "build", "ctest"] * 2)
        for offset, game in ((0, "d1"), (3, "d2")):
            configure, build, ctest = self.calls[offset : offset + 3]
            build_dir = self.repo / f"build{game}-test"
            self.assertEqual(configure["build_dir"], str(build_dir))
            self.assert_option(configure, "-S", game)
            for definition in (
                "CMAKE_BUILD_TYPE=Debug",
                "BUILD_TESTING=ON",
                "CMAKE_C_FLAGS_DEBUG=-O0 -g1",
                "CMAKE_CXX_FLAGS_DEBUG=-O0 -g1",
                "OPENGL=ON",
                "SDLMIXER=ON",
                "UDP=ON",
                "OpenGL_GL_PREFERENCE=LEGACY",
            ):
                self.assertIn(definition, configure["args"])
            self.assert_option(build, "--config", "Debug")
            self.assert_option(build, "--parallel", "4")
            self.assert_option(ctest, "-C", "Debug")
            self.assert_option(ctest, "--parallel", "1")
            self.assert_option(ctest, "--timeout", "120")
            self.assert_option(ctest, "--test-dir", str(build_dir))
            self.assertIn("--output-on-failure", ctest["args"])
            self.assertIn("--no-tests=error", ctest["args"])
            self.assert_option(ctest, "--output-junit", str(build_dir / "ctest-results.xml"))
            self.assertTrue((build_dir / "ctest-results.xml").is_file())
            self.assertEqual((ctest["audio"], ctest["video"]), ("dummy", "dummy"))
        self.assertEqual({path: path.read_bytes() for path in stamps}, stamps)

    def test_single_target_explicit_jobs_and_multi_config(self):
        self.run_cli(
            "--test", "--target", "d2", "--jobs", "2", "--generator", "Ninja Multi-Config", "--build-type", "Debug"
        )
        self.assertEqual(len(self.calls), 3)
        self.assert_option(self.calls[0], "-G", "Ninja Multi-Config")
        self.assert_option(self.calls[1], "--config", "Debug")
        self.assert_option(self.calls[1], "--parallel", "2")
        self.assert_option(self.calls[2], "-C", "Debug")
        self.assertFalse((self.repo / "buildd1-test").exists())

    def test_sanitizer_test_tree_and_clean_are_isolated(self):
        stamps = self.seed_stamps()
        sentinels = []
        for name in (
            "buildd1",
            "buildd2",
            "buildd1-test",
            "buildd2-test",
            "buildd1-asan",
            "buildd2-asan",
            "buildd1-test-asan",
            "buildd2-test-asan",
        ):
            tree = self.repo / name
            tree.mkdir()
            sentinel = tree / "keep.txt"
            sentinel.write_text("fixture", encoding="utf-8")
            sentinels.append(sentinel)
        self.run_cli("--test", "--target", "d2", "--sanitizer", "address", "--clean")
        self.assertIn("DXX_SANITIZERS=address", self.calls[0]["args"])
        self.assertEqual(self.calls[0]["build_dir"], str(self.repo / "buildd2-test-asan"))
        for sentinel in sentinels:
            self.assertEqual(sentinel.exists(), sentinel.parent.name != "buildd2-test-asan")
        self.assertEqual({path: path.read_bytes() for path in stamps}, stamps)

    def test_build_only_accepts_release_and_sanitizer(self):
        self.run_cli("--target", "d1", "--sanitizer", "address", "--build-type", "Release", "--jobs", "3")
        self.assertEqual([c["stage"] for c in self.calls], ["configure", "build"])
        self.assertEqual(self.calls[0]["build_dir"], str(self.repo / "buildd1-asan"))
        self.assertIn("CMAKE_BUILD_TYPE=Release", self.calls[0]["args"])
        self.assertIn("BUILD_TESTING=ON", self.calls[0]["args"])
        self.assert_option(self.calls[1], "--parallel", "3")
        self.assertTrue((self.repo / "temp/input_demo_build_stamps/d1-asan.stamp").is_file())

    def test_each_failure_stops_before_the_next_game(self):
        for stage, stages in (
            ("configure", ["configure"]),
            ("build", ["configure", "build"]),
            ("ctest", ["configure", "build", "ctest"]),
        ):
            with self.subTest(stage=stage):
                result = self.run_cli("--test", success=False, env={"FIXTURE_FAIL_STAGE": stage})
                self.assertEqual([c["stage"] for c in self.calls], stages)
                self.assertTrue(all("buildd1-test" in c["build_dir"] for c in self.calls))
                self.assertIn(f"fixture {stage} failure", result.stderr)
                self.assertFalse((self.repo / "temp/input_demo_build_stamps").exists())

    def test_no_discovered_tests_is_an_error(self):
        result = self.run_cli("--test", success=False, env={"FIXTURE_NO_TESTS": "1"})
        self.assertEqual(result.returncode, 8)
        self.assertEqual([c["stage"] for c in self.calls], ["configure", "build", "ctest"])

    def test_selected_cmake_prefers_sibling_ctest_and_handles_spaces(self):
        tools = self.work / "selected cmake tools"
        cmake = self.write_tool(tools / "cmake")
        ctest = self.write_tool(tools / "ctest")
        ninja = self.write_tool(tools / "custom ninja")
        self.run_cli("--test", "--target", "d1", "--cmake", str(cmake), "--ninja", str(ninja))
        self.assertEqual(self.calls[0]["tool"], str(cmake))
        self.assertEqual(self.calls[1]["tool"], str(cmake))
        self.assertEqual(self.calls[2]["tool"], str(ctest))
        self.assertIn(f"CMAKE_MAKE_PROGRAM={ninja}", self.calls[0]["args"])
        self.assertTrue(all(c["cwd"] == str(self.repo) for c in self.calls))

    def test_selected_cmake_can_fall_back_to_path_ctest(self):
        cmake = self.write_tool(self.work / "selected tools" / "cmake")
        self.run_cli("--test", "--target", "d1", "--cmake", str(cmake))
        self.assertEqual(self.calls[2]["tool"], str(self.bin / "ctest"))

    def test_relative_tool_paths_are_resolved_before_configuring(self):
        tools = self.repo / "relative tools"
        for name in ("cmake", "ctest", "custom ninja"):
            self.write_tool(tools / name)
        self.run_cli(
            "--test", "--target", "d1", "--cmake", "relative tools/cmake", "--ninja", "relative tools/custom ninja"
        )
        self.assertEqual(self.calls[0]["tool"], str(tools / "cmake"))
        self.assertIn(f"CMAKE_MAKE_PROGRAM={tools / 'custom ninja'}", self.calls[0]["args"])
        self.assertEqual(self.calls[2]["tool"], str(tools / "ctest"))

    def test_managed_sdk_cmake_and_sibling_ctest_discovery(self):
        (self.bin / "cmake").unlink()
        dependency_base = self.work / "managed dependencies"
        tools = dependency_base / "android-sdk/cmake/3.31.6/bin"
        self.write_tool(tools / "cmake")
        self.write_tool(tools / "ctest")
        (self.repo / "dependency_base.txt").write_text(f"  {dependency_base}  \n", encoding="utf-8")
        self.run_cli("--test", "--target", "d1")
        self.assertEqual(self.calls[0]["tool"], str(tools / "cmake"))
        self.assertEqual(self.calls[2]["tool"], str(tools / "ctest"))

    def test_missing_ctest_fails_before_build_but_build_only_does_not_need_it(self):
        (self.bin / "ctest").unlink()
        result = self.run_cli("--test", success=False)
        self.assertIn("ctest not found", result.stderr)
        self.assertEqual(self.calls, [])
        self.run_cli("--target", "d1")
        self.assertEqual([c["stage"] for c in self.calls], ["configure", "build"])

    def test_list_tools_and_help_do_not_build(self):
        for args in (("--test", "--list-tools"), ("--list-tools",), ("--help",)):
            with self.subTest(args=args):
                result = self.run_cli(*args)
                self.assertEqual(self.calls, [])
                if "--test" in args:
                    self.assertIn(f"ctest: {self.bin / 'ctest'}", result.stdout)

    def test_missing_and_empty_option_values_are_friendly_errors(self):
        for option in ("--target", "--build-type", "--sanitizer", "--jobs", "--generator", "--cmake", "--ninja"):
            for suffix in ((), ("",), ("--test",)):
                with self.subTest(option=option, suffix=suffix):
                    result = self.run_cli(option, *suffix, success=False)
                    self.assertIn(f"Missing value for {option}", result.stderr)
                    self.assertNotIn("unbound variable", result.stderr)
                    self.assertEqual(self.calls, [])

    def test_invalid_arguments_fail_before_any_tool(self):
        cases = [("--unknown",), ("--target", "d3"), ("--sanitizer", "thread")]
        cases += [("--jobs", value) for value in ("0", "-1", "1.5", "many", "2 3", "1e2")]
        cases += [("--test", "--build-type", value) for value in ("Release", "RelWithDebInfo", "MinSizeRel")]
        cases += [
            ("--cmake", str(self.work / "missing")),
            ("--cmake", str(self.work)),
            ("--ninja", str(self.work / "missing")),
            ("--ninja", str(self.work)),
        ]
        for args in cases:
            with self.subTest(args=args):
                self.run_cli(*args, success=False)
                self.assertEqual(self.calls, [])


if __name__ == "__main__":
    unittest.main()
