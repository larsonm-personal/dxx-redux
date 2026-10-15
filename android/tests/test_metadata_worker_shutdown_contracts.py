"""Portable child/contract checks; actual PowerShell helper execution belongs to CI."""
from pathlib import Path
import subprocess
import sys
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
CHILD = ROOT / "android/tests/fixtures/metadata_shutdown_child.py"


class ShutdownChildTests(unittest.TestCase):
    def run_child(self, mode, timeout=10):
        process = subprocess.Popen([sys.executable, str(CHILD), mode], stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        started = time.monotonic()
        timed_out = False
        try:
            stdout, stderr = process.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            process.kill()
            stdout, stderr = process.communicate(timeout=5)
        return process.returncode, timed_out, stdout, stderr, time.monotonic() - started

    def test_slow_graceful_eof_exceeds_former_two_seconds(self):
        code, timed_out, stdout, stderr, elapsed = self.run_child("slow")
        self.assertEqual(code, 0)
        self.assertFalse(timed_out)
        self.assertGreaterEqual(elapsed, 2)
        self.assertIn(b"shutdown stdout", stdout)
        self.assertIn(b"shutdown diagnostic", stderr)

    def test_nonzero_exit_retains_both_streams(self):
        code, timed_out, stdout, stderr, _ = self.run_child("nonzero")
        self.assertEqual(code, 23)
        self.assertFalse(timed_out)
        self.assertIn(b"shutdown stdout", stdout)
        self.assertIn(b"shutdown diagnostic", stderr)

    def test_timeout_is_bounded_and_reaped(self):
        code, timed_out, _, _, elapsed = self.run_child("timeout", 0.2)
        self.assertNotEqual(code, 0)
        self.assertTrue(timed_out)
        self.assertLess(elapsed, 5)

    def test_both_pipes_drain_without_truncation(self):
        code, timed_out, stdout, stderr, _ = self.run_child("flood")
        self.assertEqual(code, 0)
        self.assertFalse(timed_out)
        self.assertGreaterEqual(len(stdout), 262144)
        self.assertGreaterEqual(len(stderr), 262144)


class ShutdownSourceContracts(unittest.TestCase):
    def test_real_helper_drains_before_wait_and_bounds_completion(self):
        source = (ROOT / "android/helpers/host_metadata_worker.ps1").read_text()
        stop = source[source.index("function Stop-MetadataWorkerProcess {"):]
        self.assertIn("$TimeoutMilliseconds = 30000", stop)
        self.assertLess(stop.index("ReadToEndAsync()"), stop.index("WaitForExit($TimeoutMilliseconds)"))
        for fragment in ("$OutputTask.Wait(5000)", "$Worker.ErrorTask.Wait(5000)",
                         "if ($stdoutComplete)", "if ($stderrComplete)", "$exitCode -ne 0",
                         "if (-not $IgnoreFailure) { throw $message }", "$Worker.Process = $null"):
            self.assertIn(fragment, stop)
        self.assertIn("-OutputTask $outputTask", (ROOT / "android/tests/test_native_metadata_worker.ps1").read_text())
        self.assertEqual(source.count("foreach ($key in $replacement.Keys)"), 2)

    def test_pid_stage_and_complete_logs_are_retained(self):
        helper = (ROOT / "android/helpers/host_metadata_worker.ps1").read_text()
        runner = (ROOT / "android/tests/run_engine_sanitizers.ps1").read_text()
        for suffix in (".start.json", ".shutdown.json", ".stdout.log", ".stderr.log"):
            self.assertIn(suffix, helper)
        for fragment in ("DXX_SANITIZER_STAGE", "DXX_METADATA_WORKER_DIAGNOSTICS_DIR", "diagnostic-index.json",
                         "public-worker-shutdown-contract", "test_host_metadata_worker.ps1"):
            self.assertIn(fragment, runner)
        self.assertIn("$_.Name -like 'asan.*' -and $_.Length -gt 0", runner)
        self.assertNotIn("detect_leaks=0", runner)
        workflow = (ROOT / ".github/workflows/engine-smoke.yml").read_text()
        section = workflow[workflow.index("- name: Keep sanitizer diagnostics"):]
        self.assertIn("if: always()", section)
        self.assertIn("android/temp/public-sanitizers-ci/", section)


if __name__ == "__main__":
    unittest.main()
