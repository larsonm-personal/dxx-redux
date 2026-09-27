#!/usr/bin/env python3
"""Run one owned child tree and clean it up when the requesting process exits."""

import ctypes
import os
from pathlib import Path
import selectors
import signal
import subprocess
import sys
import time


def stop_children(leader):
    # The leader has not been reaped, so its process-group ID cannot be reused
    try:
        os.killpg(leader, signal.SIGKILL)
    except ProcessLookupError:
        pass
    deadline = time.monotonic() + 5
    children_path = Path(f"/proc/self/task/{os.getpid()}/children")
    while True:
        # Subreaper adoption also finds descendants that created another session
        children = [int(value) for value in children_path.read_text().split()]
        if not children:
            return
        for child in children:
            try:
                descriptor = os.pidfd_open(child)
            except ProcessLookupError:
                continue
            try:
                signal.pidfd_send_signal(descriptor, signal.SIGKILL)
            except ProcessLookupError:
                pass
            finally:
                os.close(descriptor)
        while True:
            try:
                reaped, _ = os.waitpid(-1, os.WNOHANG)
            except ChildProcessError:
                break
            if reaped == 0:
                break
        if time.monotonic() >= deadline:
            raise RuntimeError("owned descendants did not exit after SIGKILL")
        time.sleep(0.01)


def run(parent, command):
    if os.getppid() != parent:
        raise RuntimeError("requesting process exited before supervisor startup")
    parent_descriptor = os.pidfd_open(parent)
    child_descriptor = None
    child = None
    try:
        if os.getppid() != parent:
            raise RuntimeError("requesting process exited during supervisor startup")
        libc = ctypes.CDLL(None, use_errno=True)
        libc.prctl.argtypes = [ctypes.c_int] + [ctypes.c_ulong] * 4
        # PR_SET_CHILD_SUBREAPER, from linux/prctl.h
        if libc.prctl(36, 1, 0, 0, 0) != 0:
            raise OSError(ctypes.get_errno(), "cannot become a child subreaper")
        child = subprocess.Popen(command, start_new_session=True)
        child_descriptor = os.pidfd_open(child.pid)
        with selectors.DefaultSelector() as events:
            events.register(parent_descriptor, selectors.EVENT_READ, "parent")
            events.register(child_descriptor, selectors.EVENT_READ, "child")
            while True:
                ready = events.select(timeout=0.05)
                if ready:
                    break
                # An active worker may wait for an orphaned grandchild to exit
                # Reap adopted zombies now, preserving the main child's status
                children_path = Path(f"/proc/self/task/{os.getpid()}/children")
                for adopted in children_path.read_text().split():
                    adopted = int(adopted)
                    if adopted != child.pid:
                        try:
                            os.waitpid(adopted, os.WNOHANG)
                        except ChildProcessError:
                            pass
            if any(key.data == "parent" for key, _ in ready):
                return 143
            # Observe without reaping until group cleanup has completed
            result = os.waitid(os.P_PID, child.pid, os.WEXITED | os.WNOWAIT)
            return result.si_status if result.si_code == os.CLD_EXITED else 128 + result.si_status
    finally:
        try:
            if child is not None:
                stop_children(child.pid)
                # stop_children reaped the process; avoid subprocess destructor polling
                child.returncode = 0
        finally:
            if child_descriptor is not None:
                os.close(child_descriptor)
            os.close(parent_descriptor)


if __name__ == "__main__":
    try:
        if len(sys.argv) < 4 or sys.argv[2] != "--":
            raise ValueError("usage: process_lifetime_linux.py PARENT_PID -- COMMAND [ARG...]")
        sys.exit(run(int(sys.argv[1]), sys.argv[3:]))
    except Exception as error:
        print(f"Linux child supervisor: {error}", file=sys.stderr)
        sys.exit(125)
