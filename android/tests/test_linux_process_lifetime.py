#!/usr/bin/env python3
# TEST-SUPPORT: owner=test_headless_process_pool
"""Exercise the real PowerShell pool across normal exit and abrupt parent death."""

import ctypes
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
import unittest

REPO = Path(__file__).resolve().parents[2]


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
