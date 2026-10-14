#!/usr/bin/env python3
# TEST-SUPPORT: owner=test_headless_process_pool
"""Check supervisor prerequisites and real pool cleanup after exit or parent death."""

import contextlib
import ctypes
import errno
import importlib.util
import io
import json
import os
from pathlib import Path
import select
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest import mock

REPO = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "process_lifetime_linux", REPO / "android/helpers/process_lifetime_linux.py"
)
SUPERVISOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SUPERVISOR)


@unittest.skipUnless(sys.platform == "linux", "Linux supervisor prerequisites")
class SupervisorPreflightTests(unittest.TestCase):
    def setUp(self):
        self.patches = contextlib.ExitStack()
        self.addCleanup(self.patches.close)
        self.patches.enter_context(mock.patch.object(SUPERVISOR.os, "getppid", return_value=42))
        self.pidfd_open = self.patches.enter_context(mock.patch.object(SUPERVISOR.os, "pidfd_open", return_value=101))
        self.close = self.patches.enter_context(mock.patch.object(SUPERVISOR.os, "close"))
        self.path = self.patches.enter_context(mock.patch.object(SUPERVISOR, "Path"))
        self.read_text = self.path.return_value.read_text
        self.read_text.return_value = ""
        self.libc = self.patches.enter_context(mock.patch.object(SUPERVISOR.ctypes, "CDLL"))
        self.libc.return_value.prctl.return_value = 0
        self.launch = self.patches.enter_context(mock.patch.object(SUPERVISOR.subprocess, "Popen"))
        self.cleanup = self.patches.enter_context(mock.patch.object(SUPERVISOR, "stop_children"))
        self.patches.enter_context(mock.patch.object(SUPERVISOR.sys, "argv", ["supervisor", "42", "--", "workload"]))
        self.stderr = self.patches.enter_context(contextlib.redirect_stderr(io.StringIO()))

    def assert_unsupported(self, detail):
        self.assertEqual(SUPERVISOR.main(), 125)
        self.assertIn("Linux child supervisor: unsupported Linux host:", self.stderr.getvalue())
        self.assertIn(detail, self.stderr.getvalue())
        self.assertNotIn("Traceback", self.stderr.getvalue())
        self.launch.assert_not_called()
        self.cleanup.assert_not_called()
        self.close.assert_called_with(101)

    def test_missing_children_interface_does_not_launch_workload(self):
        self.read_text.side_effect = FileNotFoundError(errno.ENOENT, "No such file or directory")
        self.assert_unsupported("cannot read child process IDs")
        self.path.assert_called_once_with(f"/proc/self/task/{os.getpid()}/children")
        self.read_text.assert_called_once_with(encoding="ascii")
        self.libc.assert_not_called()

    def test_unreadable_children_interface_does_not_launch_workload(self):
        self.read_text.side_effect = PermissionError(errno.EACCES, "Permission denied")
        self.assert_unsupported("Permission denied")

    def test_invalid_children_encoding_does_not_launch_workload(self):
        self.read_text.side_effect = UnicodeDecodeError("ascii", b"\xff", 0, 1, "invalid byte")
        self.assert_unsupported("cannot read child process IDs")

    def test_malformed_child_ids_do_not_launch_workload(self):
        for content in ("not-a-pid", "0", "-1", "+1", "1.0", "12 broken"):
            with self.subTest(content=content):
                self.read_text.return_value = content
                self.assert_unsupported("invalid child process ID")

    def test_unavailable_libc_does_not_launch_workload(self):
        self.libc.side_effect = OSError("libc unavailable")
        self.assert_unsupported("child subreaper support is required")

    def test_missing_prctl_does_not_launch_workload(self):
        self.libc.return_value = SimpleNamespace()
        self.assert_unsupported("child subreaper support is required")

    def test_denied_subreaper_does_not_launch_workload(self):
        self.libc.return_value.prctl.return_value = -1
        with mock.patch.object(SUPERVISOR.ctypes, "get_errno", return_value=errno.EPERM):
            self.assert_unsupported("child subreaper support is required")
        self.libc.return_value.prctl.assert_called_once_with(36, 1, 0, 0, 0)

    def test_supported_host_preflights_before_launch_and_preserves_exit(self):
        self.read_text.return_value = "12 34\n"
        child = SimpleNamespace(pid=123, returncode=None)

        def launch(command, *, start_new_session):
            self.read_text.assert_called_once_with(encoding="ascii")
            self.libc.return_value.prctl.assert_called_once_with(36, 1, 0, 0, 0)
            self.assertEqual(command, ["workload"])
            self.assertTrue(start_new_session)
            return child

        self.launch.side_effect = launch
        self.pidfd_open.side_effect = [101, 102]
        with (
            mock.patch.object(SUPERVISOR.selectors, "DefaultSelector") as selector,
            mock.patch.object(
                SUPERVISOR.os, "waitid", return_value=SimpleNamespace(si_status=7, si_code=os.CLD_EXITED)
            ),
        ):
            selector.return_value.__enter__.return_value.select.return_value = [(SimpleNamespace(data="child"), 1)]
            self.assertEqual(SUPERVISOR.main(), 7)
        self.launch.assert_called_once()
        self.cleanup.assert_called_once_with(123)
        self.assertEqual(child.returncode, 0)
        self.assertEqual(self.close.call_args_list, [mock.call(102), mock.call(101)])
        self.assertEqual(self.stderr.getvalue(), "")


@unittest.skipUnless(sys.platform == "linux", "Linux pidfd/subreaper integration")
class ProcessLifetimeTests(unittest.TestCase):
    def exercise(self, kill_owner):
        # Reap the supervisor after killing its parent instead of leaving it to PID 1
        self.assertEqual(ctypes.CDLL(None).prctl(36, 1, 0, 0, 0), 0)
        descriptors = []
        owner = None
        with tempfile.TemporaryDirectory(prefix="process lifetime-", dir=REPO / "android/temp") as temporary:
            root = Path(temporary)
            state = root / "children.json"
            worker = root / "worker.py"
            worker.write_text("""import json, os, subprocess, sys, time
from pathlib import Path
state = Path(sys.argv[1])
if sys.argv[2] == 'grandchild':
    state.with_suffix('.ready').write_text('ready')
    time.sleep(60)
else:
    child = subprocess.Popen([sys.executable, __file__, str(state), 'grandchild'], start_new_session=True)
    while not state.with_suffix('.ready').exists(): time.sleep(0.01)
    pending = state.with_suffix('.pending')
    pending.write_text(json.dumps([os.getpid(), child.pid, os.getppid()]))
    pending.replace(state)
    print('supervised stdout', flush=True)
    print('supervised stderr', file=sys.stderr, flush=True)
    if sys.argv[2] == 'normal': sys.exit(7)
    time.sleep(60)
""")
            launcher = root / "owner.ps1"
            helper = str(REPO / "android/helpers/headless_process_pool.ps1").replace("'", "''")
            launcher.write_text(
                """param($PythonPath, $WorkerPath, $StatePath, $Mode)
$ErrorActionPreference = 'Stop'
"""
                + f". '{helper}'\n"
                + """$fixtureResult = @{ Exit = -1 }
$task = [pscustomobject]@{ FilePath = $PythonPath; Arguments = @($WorkerPath, $StatePath, $Mode); WorkingDirectory = $PSScriptRoot; TimeoutSeconds = 60 }
Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted {
    param($task, $result)
    $fixtureResult.Exit = $result.ExitCode
    Write-Output $result.StandardOutput
    [Console]::Error.Write($result.StandardError)
}
exit $fixtureResult.Exit
"""
            )
            try:
                owner = subprocess.Popen(
                    [
                        shutil.which("pwsh"),
                        "-NoProfile",
                        "-File",
                        str(launcher),
                        sys.executable,
                        str(worker),
                        str(state),
                        "hold" if kill_owner else "normal",
                    ],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                )
                deadline = time.monotonic() + 15
                while not state.exists():
                    if owner.poll() is not None or time.monotonic() >= deadline:
                        self.fail(f"fixture did not start: {owner.communicate(timeout=5)}")
                    time.sleep(0.02)
                children = json.loads(state.read_text())
                for child in children:
                    try:
                        descriptors.append(os.pidfd_open(child))
                    except ProcessLookupError:
                        pass
                if kill_owner:
                    owner.kill()
                stdout, stderr = owner.communicate(timeout=15)
                self.assertEqual(owner.returncode, -signal.SIGKILL if kill_owner else 7, stderr)
                if not kill_owner:
                    self.assertIn("supervised stdout", stdout)
                    self.assertIn("supervised stderr", stderr)
                for descriptor in descriptors:
                    self.assertTrue(select.select([descriptor], [], [], 5)[0], "owned process survived")
                # Adopted supervisor zombies are ours to reap in the forced-kill case
                for child in children:
                    try:
                        os.waitpid(child, 0)
                    except ChildProcessError:
                        pass
            finally:
                for descriptor in descriptors:
                    try:
                        signal.pidfd_send_signal(descriptor, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    os.close(descriptor)
                if owner is not None:
                    if owner.poll() is None:
                        owner.kill()
                    owner.communicate(timeout=10)
                while True:
                    try:
                        if os.waitpid(-1, os.WNOHANG)[0] == 0:
                            break
                    except ChildProcessError:
                        break

    def test_normal_exit_reaps_detached_grandchild_and_preserves_output(self):
        self.exercise(False)

    def test_sigkill_of_runner_stops_detached_descendants(self):
        self.exercise(True)


if __name__ == "__main__":
    unittest.main()
