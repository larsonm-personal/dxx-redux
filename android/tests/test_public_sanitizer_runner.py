#!/usr/bin/env python3
# TEST-SUPPORT: owner=test_native_host_unit_tests
"""Run the public profile against recording stubs, without retail assets or a compiler."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
PWSH = shutil.which("pwsh")


@unittest.skipUnless(PWSH and os.name != "nt", "Requires PowerShell on Linux")
class PublicSanitizerRunner(unittest.TestCase):
    def run_profile(self, *options, failing="", finding=False, fatal=False):
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            tests = repo / "android/tests"
            helpers = repo / "android/helpers"
            tests.mkdir(parents=True)
            helpers.mkdir()
            shutil.copy(ROOT / "android/tests/run_engine_sanitizers.ps1", tests)
            (helpers / "retain-recent-artifacts.ps1").write_text("param($Artifacts)\n")
            (helpers / "test_host_platform.ps1").write_text("""
function git { return 'fixture-revision' }
function Test-RegressionWindowsHost { return $false }
function Get-RegressionHostExecutableNames { param($BaseName) return @($BaseName, "$BaseName.exe") }
function Join-RegressionPath { return [IO.Path]::Combine([string[]]$args) }
function Resolve-RegressionCMakePath { param($RepoRoot, $BuildDir) return "$RepoRoot/cmake" }
""")
            (helpers / "shareware_test_data.ps1").write_text("""
function Resolve-SharewareTestData { param($RepoRoot, $Game) return "$RepoRoot/public-$Game" }
""")
            (repo / "run-linux-build.sh").write_text("""#!/bin/bash
set -eu
printf '%s\\n' "$*" >> "$FIXTURE_BUILD_LOG"
while [ "$#" -gt 0 ]; do
  if [ "$1" = --target ]; then game="$2"; fi
  shift
done
mkdir -p "build${game}-asan/main"
printf 'DXX_SANITIZERS:STRING=address\\n' > "build${game}-asan/CMakeCache.txt"
""")
            ctest = repo / "ctest"
            ctest.write_text('#!/bin/bash\nprintf \'%s\\n\' "$*" >> "$FIXTURE_CTEST_LOG"\n')
            ctest.chmod(0o755)
            for name in (
                "test_input_demo_runtime_smoke",
                "test_secret_area_baseline",
                "test_native_metadata_worker",
                "test_metadata_level_headers",
                "test_headless_config_read_bounds",
            ):
                location = helpers if name == "test_headless_config_read_bounds" else tests
                (location / (name + ".ps1")).write_text("""
@{ name = [IO.Path]::GetFileNameWithoutExtension($PSCommandPath); arguments = @($args); asan = $env:ASAN_OPTIONS; lsan = $env:LSAN_OPTIONS } |
    ConvertTo-Json -Compress | Add-Content -LiteralPath $env:FIXTURE_STAGE_LOG
if ($env:FIXTURE_FAIL -eq [IO.Path]::GetFileNameWithoutExtension($PSCommandPath)) { exit 23 }
if ($env:FIXTURE_FINDING -eq '1') {
    $env:FIXTURE_DIAGNOSTIC | Set-Content ($env:FIXTURE_OUTPUT + '/asan.123')
}
exit 0
""")
            output = repo / "reports"
            env = dict(
                os.environ,
                FIXTURE_OUTPUT=str(output),
                ASAN_OPTIONS="detect_leaks=0:halt_on_error=0",
                LSAN_OPTIONS="suppressions=/tmp/hostile-suppressions:exitcode=0",
                FIXTURE_FAIL=failing,
                FIXTURE_FINDING="1" if finding or fatal else "0",
                FIXTURE_DIAGNOSTIC="LeakSanitizer has encountered a fatal error"
                if fatal
                else "ERROR: AddressSanitizer: heap-buffer-overflow",
                FIXTURE_STAGE_LOG=str(repo / "stages.jsonl"),
                FIXTURE_BUILD_LOG=str(repo / "build.log"),
                FIXTURE_CTEST_LOG=str(repo / "ctest.log"),
            )
            result = subprocess.run(
                [
                    PWSH,
                    "-NoProfile",
                    "-File",
                    str(tests / "run_engine_sanitizers.ps1"),
                    "-OutputRoot",
                    str(output),
                    *options,
                ],
                cwd=repo,
                env=env,
                text=True,
                capture_output=True,
                timeout=60,
            )
            records = (
                [json.loads(line) for line in (repo / "stages.jsonl").read_text().splitlines()]
                if (repo / "stages.jsonl").exists()
                else []
            )
            builds = (repo / "build.log").read_text() if (repo / "build.log").exists() else ""
            ctests = (repo / "ctest.log").read_text() if (repo / "ctest.log").exists() else ""
            return result, records, builds, ctests

    def test_public_d2_selects_only_instrumented_public_workloads(self):
        result, records, builds, ctests = self.run_profile("-Category", "Public", "-Game", "d2")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("--sanitizer address --build-type Debug", builds)
        self.assertNotIn("--target d1", builds)
        self.assertIn("--parallel 1", ctests)
        self.assertEqual(len(records), 8)
        configs = [r for r in records if r["name"] == "test_headless_config_read_bounds"]
        self.assertEqual(len(configs), 3)
        for record in records:
            self.assertNotIn("detect_leaks=0", record["asan"])
            self.assertIn("halt_on_error=1", record["asan"])
            self.assertIn("log_path=", record["asan"])
            self.assertEqual(record["lsan"], "exitcode=23")
            args = record["arguments"]
            if record["name"] == "test_input_demo_runtime_smoke":
                self.assertEqual(args[args.index("-TimeoutSeconds") + 1], "240")
            else:
                self.assertNotIn("-TimeoutSeconds", args)
            self.assertTrue(any("buildd2-asan/main" in a for a in args), record)
            if record in configs:
                self.assertEqual(args[args.index("-Mission") + 1], "d2demo")
            else:
                self.assertEqual(args[args.index("-DataEdition") + 1], "shareware")
        self.assertTrue(any("-CheckProfileSwitch" in r["arguments"] for r in records))
        headers = next(r for r in records if r["name"] == "test_metadata_level_headers")
        self.assertIn("-NoBuild", headers["arguments"])
        self.assertIn("-KeepArtifacts", headers["arguments"])

    def test_public_d1_does_not_require_d2_or_config_routes(self):
        result, records, builds, _ = self.run_profile("-Category", "Public", "-Game", "d1")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(len(records), 4)
        self.assertNotIn("--target d2", builds)
        replay = next(record for record in records if record["name"] == "test_input_demo_runtime_smoke")
        args = replay["arguments"]
        self.assertEqual(args[args.index("-TimeoutSeconds") + 1], "240")

    def test_stage_failure_still_runs_remaining_categories(self):
        result, records, _, _ = self.run_profile(
            "-Category", "Public", "-Game", "d1", failing="test_secret_area_baseline"
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(len(records), 4)

    def test_swallowed_sanitizer_diagnostic_fails_suite(self):
        result, records, _, _ = self.run_profile("-Category", "Public", "-Game", "d1", finding=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(len(records), 4)

    def test_swallowed_sanitizer_runtime_fatal_fails_suite(self):
        result, records, _, _ = self.run_profile("-Category", "Public", "-Game", "d1", fatal=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(len(records), 4)


if __name__ == "__main__":
    unittest.main()
