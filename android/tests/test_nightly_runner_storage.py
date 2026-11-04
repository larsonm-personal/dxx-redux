"""Exercise host storage admission without deleting anything on the test host."""

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "helpers/nightly_runner_storage.sh"


@unittest.skipUnless(sys.platform == "linux", "Hosted Ubuntu storage helper")
class RunnerStorageTest(unittest.TestCase):
    def run_storage(self, mode, minimum, free_kib, **overrides):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            commands = {
                "df": f"echo 'Filesystem 1024-blocks Used Available Capacity Mounted'\necho 'mock 99999999 0 {free_kib} 0% /'\n",
                "du": "exit 0\n",
                "sudo": "echo 'UNEXPECTED DESTRUCTIVE COMMAND' >&2\nexit 99\n",
            }
            for name, body in commands.items():
                executable = root / name
                executable.write_text("#!/bin/sh\n" + body)
                executable.chmod(0o755)
            env = dict(os.environ)
            env.update(
                PATH=f"{root}:{env['PATH']}",
                GITHUB_WORKSPACE=directory,
                RUNNER_TEMP=directory,
                GITHUB_ACTIONS="true",
                RUNNER_ENVIRONMENT="self-hosted",
                RUNNER_OS="Linux",
                ImageOS="ubuntu24",
            )
            env.update(overrides)
            result = subprocess.run(
                ["bash", str(SCRIPT), mode, str(minimum)],
                env=env,
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertNotIn("UNEXPECTED DESTRUCTIVE COMMAND", result.stderr)
            return result

    def test_cleanup_refuses_non_hosted_runner(self):
        result = self.run_storage("prepare", 20, 30 * 1024 * 1024)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("requires a GitHub-hosted Ubuntu", result.stdout)

    def test_cleanup_refuses_local_machine(self):
        result = self.run_storage("prepare", 20, 30 * 1024 * 1024, GITHUB_ACTIONS="")
        self.assertNotEqual(result.returncode, 0)

    def test_check_requires_full_budget(self):
        result = self.run_storage("check", 10, 10 * 1024 * 1024 - 1)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("requires 10 GiB", result.stdout)

    def test_exact_budget_is_admitted_without_cleanup(self):
        self.assertEqual(self.run_storage("check", 10, 10 * 1024 * 1024).returncode, 0)

    def test_final_report_works_even_when_full(self):
        self.assertEqual(self.run_storage("report", 0, 0).returncode, 0)

    def test_invalid_budget_fails(self):
        self.assertNotEqual(self.run_storage("check", "invalid", 99999999).returncode, 0)


if __name__ == "__main__":
    unittest.main()
