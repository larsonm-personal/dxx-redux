"""Exercise host storage admission without deleting anything on the test host."""

import os
import shlex
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

    def run_retire(self, *, linked=False, missing_adb=False, **overrides):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            hosted = root / "hosted-android"
            hosted.mkdir()
            alias = root / "hosted-alias"
            alias.symlink_to(hosted, target_is_directory=True)
            if linked:
                hosted.rmdir()
                target = root / "other-android"
                target.mkdir()
                hosted.symlink_to(target, target_is_directory=True)
            sdk = root / "managed-sdk"
            (sdk / "platform-tools").mkdir(parents=True)
            adb = sdk / "platform-tools/adb"
            adb.touch()
            adb.chmod(0o755)
            if missing_adb:
                adb.unlink()
            ndk = root / "managed-ndk"
            ndk.mkdir()
            calls = root / "calls"
            script = root / "storage.sh"
            script.write_text(SCRIPT.read_text().replace("/usr/local/lib/android", str(hosted)))
            for name, body in {
                "df": "echo 'Filesystem Blocks Used Available Capacity Mounted'\necho 'mock 99999999 0 99999999 0% /'\n",
                "du": "exit 0\n",
                "sudo": f"printf '%s\\n' \"$*\" >> {shlex.quote(str(calls))}\n",
            }.items():
                command = root / name
                command.write_text("#!/bin/sh\n" + body)
                command.chmod(0o755)
            env = dict(os.environ)
            env.update(
                PATH=f"{root}:{env['PATH']}",
                GITHUB_WORKSPACE=str(root / "workspace"),
                RUNNER_TEMP=str(root / "temp"),
                GITHUB_ACTIONS="true",
                RUNNER_ENVIRONMENT="github-hosted",
                RUNNER_OS="Linux",
                ImageOS="ubuntu24",
                DXX_NIGHTLY_SDK_CACHE=str(sdk),
                ANDROID_HOME=str(sdk),
                ANDROID_SDK_ROOT=str(sdk),
                ANDROID_NDK_ROOT=str(ndk),
                ANDROID_NDK_HOME=str(ndk),
                ANDROID_NDK_LATEST_HOME=str(ndk),
                JAVA_HOME=str(root / "jdk"),
            )
            env.update(
                {
                    key: value.replace("{hosted}", str(hosted)).replace("{alias}", str(alias))
                    for key, value in overrides.items()
                }
            )
            result = subprocess.run(
                ["bash", str(script), "retire-hosted-sdk", "20"],
                env=env,
                text=True,
                capture_output=True,
                check=False,
            )
            # The mock only records commands; no host or fixture tree is removed
            self.assertTrue(hosted.is_dir())
            return result, calls.read_text().splitlines() if calls.exists() else [], str(hosted)

    def test_replaced_sdk_cleanup_is_fixed_and_opt_in(self):
        result, calls, hosted = self.run_retire()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(calls, [f"rm -rf -- {hosted}"])

    def test_replaced_sdk_cleanup_rejects_active_locations(self):
        for variable in (
            "GITHUB_WORKSPACE",
            "RUNNER_TEMP",
            "ANDROID_HOME",
            "ANDROID_SDK_ROOT",
            "ANDROID_NDK_ROOT",
            "ANDROID_NDK_HOME",
            "ANDROID_NDK_LATEST_HOME",
            "DXX_NIGHTLY_SDK_CACHE",
            "JAVA_HOME",
        ):
            with self.subTest(variable=variable):
                result, calls, _ = self.run_retire(**{variable: "{hosted}"})
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(calls, [])

    def test_replaced_sdk_cleanup_preserves_link(self):
        result, calls, _ = self.run_retire(linked=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(calls, [])

    def test_replaced_sdk_cleanup_resolves_active_alias(self):
        result, calls, _ = self.run_retire(ANDROID_NDK_HOME="{alias}/ndk")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(calls, [])

    def test_replaced_sdk_cleanup_requires_provisioned_adb(self):
        result, calls, _ = self.run_retire(missing_adb=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(calls, [])

    def test_replaced_sdk_cleanup_refuses_self_hosted_runner(self):
        result, calls, _ = self.run_retire(RUNNER_ENVIRONMENT="self-hosted")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(calls, [])

    def test_replaced_sdk_cleanup_requires_managed_sdk(self):
        result, calls, _ = self.run_retire(DXX_NIGHTLY_SDK_CACHE="")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(calls, [])

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
