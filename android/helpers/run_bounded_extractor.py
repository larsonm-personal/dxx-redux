#!/usr/bin/env python3
"""Run an extractor with bounded time, diagnostics, and output."""

import argparse
import ctypes
import os
import signal
import shutil
import stat
import subprocess
import sys
import threading
import time


def _add_file(files, total, logical, allocated, limits):
    max_files, max_file_bytes, max_total_bytes = limits
    accounted = max(logical, allocated)
    files += 1
    total += accounted
    if files > max_files:
        raise RuntimeError(f"extractor exceeded {max_files} output files")
    if logical > max_file_bytes:
        raise RuntimeError(
            f"extractor output file exceeded {max_file_bytes} logical bytes")
    if total > max_total_bytes:
        raise RuntimeError(f"extractor output exceeded {max_total_bytes} accounted bytes")
    return files, total


def _measure_posix_tree(root, limits, strict):
    no_follow = getattr(os, "O_NOFOLLOW", 0)
    directory = getattr(os, "O_DIRECTORY", 0)
    root_flags = os.O_RDONLY | directory | no_follow
    try:
        root_fd = os.open(root, root_flags)
    except FileNotFoundError:
        if strict:
            raise RuntimeError("extractor output directory disappeared") from None
        return 0, 0
    except OSError as error:
        raise RuntimeError(f"extractor output root is unsafe: {error}") from None

    files = 0
    total = 0
    root_device = None

    def walk(directory_fd):
        nonlocal files, total
        try:
            entries = list(os.scandir(directory_fd))
        except OSError as error:
            raise RuntimeError(f"cannot enumerate extractor output: {error}") from None
        for entry in entries:
            try:
                before = os.stat(entry.name, dir_fd=directory_fd, follow_symlinks=False)
            except FileNotFoundError:
                if strict:
                    raise RuntimeError("extractor output changed during validation") from None
                continue
            mode = before.st_mode
            try:
                if stat.S_ISDIR(mode):
                    child_fd = os.open(entry.name, root_flags, dir_fd=directory_fd)
                    try:
                        after = os.fstat(child_fd)
                        if (before.st_dev, before.st_ino) != (after.st_dev, after.st_ino):
                            raise RuntimeError("extractor output changed during validation")
                        if after.st_dev != root_device:
                            raise RuntimeError("extractor output crosses a filesystem boundary")
                        walk(child_fd)
                    finally:
                        os.close(child_fd)
                    continue
                if not stat.S_ISREG(mode):
                    raise RuntimeError("extractor produced a link or special file")
                if before.st_nlink != 1:
                    raise RuntimeError("extractor produced a multiply-linked file")
                file_fd = os.open(
                    entry.name, os.O_RDONLY | no_follow | getattr(os, "O_NONBLOCK", 0),
                    dir_fd=directory_fd)
                try:
                    after = os.fstat(file_fd)
                    if ((before.st_dev, before.st_ino) != (after.st_dev, after.st_ino)
                            or not stat.S_ISREG(after.st_mode) or after.st_nlink != 1):
                        raise RuntimeError("extractor output changed during validation")
                    if after.st_dev != root_device:
                        raise RuntimeError("extractor output crosses a filesystem boundary")
                    allocated = after.st_blocks * 512
                    if strict and after.st_size and allocated < after.st_size:
                        raise RuntimeError("extractor produced a sparse output file")
                    files, total = _add_file(
                        files, total, after.st_size, allocated, limits)
                finally:
                    os.close(file_fd)
            except FileNotFoundError:
                if strict:
                    raise RuntimeError("extractor output changed during validation") from None
            except OSError as error:
                raise RuntimeError(f"extractor output is unsafe: {error}") from None

    try:
        root_info = os.fstat(root_fd)
        root_device = root_info.st_dev
        if not stat.S_ISDIR(root_info.st_mode):
            raise RuntimeError("extractor output root is not a regular directory")
        walk(root_fd)
        if os.fstatvfs(root_fd).f_bavail * os.fstatvfs(root_fd).f_frsize < 50 * 1024 * 1024:
            raise RuntimeError("extractor exhausted the 50 MiB free-space headroom")
        return files, total
    finally:
        os.close(root_fd)


def measure_tree(root, max_files, max_file_bytes, max_total_bytes, strict=False):
    limits = max_files, max_file_bytes, max_total_bytes
    try:
        if os.name == "nt":
            return _measure_windows_tree(root, limits, strict)
        return _measure_posix_tree(root, limits, strict)
    except RuntimeError:
        raise
    except OSError as error:
        raise RuntimeError(f"cannot validate extractor output: {error}") from None


class PosixProcessGroup:
    def __init__(self, command):
        self.process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )

    def terminate(self):
        try:
            os.killpg(self.process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()


if os.name == "nt":
    from ctypes import wintypes

    class JobBasicLimitInformation(ctypes.Structure):
        _fields_ = [
            ("per_process_user_time_limit", ctypes.c_longlong),
            ("per_job_user_time_limit", ctypes.c_longlong),
            ("limit_flags", wintypes.DWORD),
            ("minimum_working_set_size", ctypes.c_size_t),
            ("maximum_working_set_size", ctypes.c_size_t),
            ("active_process_limit", wintypes.DWORD),
            ("affinity", ctypes.c_size_t),
            ("priority_class", wintypes.DWORD),
            ("scheduling_class", wintypes.DWORD),
        ]

    class IoCounters(ctypes.Structure):
        _fields_ = [(name, ctypes.c_ulonglong) for name in (
            "read_operation_count", "write_operation_count", "other_operation_count",
            "read_transfer_count", "write_transfer_count", "other_transfer_count",
        )]

    class JobExtendedLimitInformation(ctypes.Structure):
        _fields_ = [
            ("basic_limit_information", JobBasicLimitInformation),
            ("io_info", IoCounters),
            ("process_memory_limit", ctypes.c_size_t),
            ("job_memory_limit", ctypes.c_size_t),
            ("peak_process_memory_used", ctypes.c_size_t),
            ("peak_job_memory_used", ctypes.c_size_t),
        ]

    class JobBasicAccountingInformation(ctypes.Structure):
        _fields_ = [
            ("total_user_time", ctypes.c_longlong),
            ("total_kernel_time", ctypes.c_longlong),
            ("this_period_total_user_time", ctypes.c_longlong),
            ("this_period_total_kernel_time", ctypes.c_longlong),
            ("total_page_fault_count", wintypes.DWORD),
            ("total_processes", wintypes.DWORD),
            ("active_processes", wintypes.DWORD),
            ("total_terminated_processes", wintypes.DWORD),
        ]

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateJobObjectW.argtypes = (ctypes.c_void_p, wintypes.LPCWSTR)
    kernel32.CreateJobObjectW.restype = wintypes.HANDLE
    kernel32.SetInformationJobObject.argtypes = (
        wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD)
    kernel32.SetInformationJobObject.restype = wintypes.BOOL
    kernel32.AssignProcessToJobObject.argtypes = (wintypes.HANDLE, wintypes.HANDLE)
    kernel32.AssignProcessToJobObject.restype = wintypes.BOOL
    kernel32.TerminateJobObject.argtypes = (wintypes.HANDLE, wintypes.UINT)
    kernel32.TerminateJobObject.restype = wintypes.BOOL
    kernel32.QueryInformationJobObject.argtypes = (
        wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD,
        ctypes.POINTER(wintypes.DWORD))
    kernel32.QueryInformationJobObject.restype = wintypes.BOOL
    kernel32.CloseHandle.argtypes = (wintypes.HANDLE,)
    kernel32.CloseHandle.restype = wintypes.BOOL
    kernel32.CreateFileW.argtypes = (
        wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
        wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE)
    kernel32.CreateFileW.restype = wintypes.HANDLE
    kernel32.GetFileInformationByHandle.argtypes = (
        wintypes.HANDLE, ctypes.c_void_p)
    kernel32.GetFileInformationByHandle.restype = wintypes.BOOL
    kernel32.GetFinalPathNameByHandleW.argtypes = (
        wintypes.HANDLE, wintypes.LPWSTR, wintypes.DWORD, wintypes.DWORD)
    kernel32.GetFinalPathNameByHandleW.restype = wintypes.DWORD
    kernel32.GetFileType.argtypes = (wintypes.HANDLE,)
    kernel32.GetFileType.restype = wintypes.DWORD
    kernel32.GetCompressedFileSizeW.argtypes = (
        wintypes.LPCWSTR, ctypes.POINTER(wintypes.DWORD))
    kernel32.GetCompressedFileSizeW.restype = wintypes.DWORD
    kernel32.FindFirstStreamW.argtypes = (
        wintypes.LPCWSTR, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD)
    kernel32.FindFirstStreamW.restype = wintypes.HANDLE
    kernel32.FindNextStreamW.argtypes = (wintypes.HANDLE, ctypes.c_void_p)
    kernel32.FindNextStreamW.restype = wintypes.BOOL
    kernel32.FindClose.argtypes = (wintypes.HANDLE,)
    kernel32.FindClose.restype = wintypes.BOOL
    kernel32.DeviceIoControl.argtypes = (
        wintypes.HANDLE, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD,
        ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD),
        ctypes.c_void_p)
    kernel32.DeviceIoControl.restype = wintypes.BOOL

    class ByHandleFileInformation(ctypes.Structure):
        _fields_ = [
            ("file_attributes", wintypes.DWORD),
            ("creation_time", wintypes.FILETIME),
            ("last_access_time", wintypes.FILETIME),
            ("last_write_time", wintypes.FILETIME),
            ("volume_serial_number", wintypes.DWORD),
            ("file_size_high", wintypes.DWORD),
            ("file_size_low", wintypes.DWORD),
            ("number_of_links", wintypes.DWORD),
            ("file_index_high", wintypes.DWORD),
            ("file_index_low", wintypes.DWORD),
        ]

    class Win32FindStreamData(ctypes.Structure):
        _fields_ = [
            ("stream_size", ctypes.c_longlong),
            ("stream_name", wintypes.WCHAR * 296),
        ]


def _windows_handle(path):
    access = 0x0080
    share = 0x00000001 | 0x00000002 | 0x00000004
    open_existing = 3
    flags = 0x00200000 | 0x02000000
    handle = kernel32.CreateFileW(path, access, share, None, open_existing, flags, None)
    if handle == wintypes.HANDLE(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    return handle


def _windows_information(handle):
    information = ByHandleFileInformation()
    if not kernel32.GetFileInformationByHandle(handle, ctypes.byref(information)):
        raise ctypes.WinError(ctypes.get_last_error())
    return information


def _windows_final_path(handle):
    size = kernel32.GetFinalPathNameByHandleW(handle, None, 0, 0)
    if not size:
        raise ctypes.WinError(ctypes.get_last_error())
    buffer = ctypes.create_unicode_buffer(size + 1)
    if not kernel32.GetFinalPathNameByHandleW(handle, buffer, len(buffer), 0):
        raise ctypes.WinError(ctypes.get_last_error())
    return buffer.value.rstrip("\\/")


def _windows_allocated_size(path):
    high = wintypes.DWORD()
    ctypes.set_last_error(0)
    low = kernel32.GetCompressedFileSizeW(path, ctypes.byref(high))
    if low == 0xFFFFFFFF and ctypes.get_last_error():
        raise ctypes.WinError(ctypes.get_last_error())
    return (high.value << 32) | low


def _assert_no_windows_streams(path):
    data = Win32FindStreamData()
    ctypes.set_last_error(0)
    handle = kernel32.FindFirstStreamW(path, 0, ctypes.byref(data), 0)
    if handle == wintypes.HANDLE(-1).value:
        error = ctypes.get_last_error()
        if error in (1, 38, 50):
            return
        raise ctypes.WinError(error)
    try:
        while True:
            if data.stream_name != "::$DATA":
                raise RuntimeError("extractor produced an alternate data stream")
            if not kernel32.FindNextStreamW(handle, ctypes.byref(data)):
                error = ctypes.get_last_error()
                if error == 38:
                    break
                raise ctypes.WinError(error)
    finally:
        kernel32.FindClose(handle)


def _measure_windows_tree(root, limits, strict):
    if not os.path.lexists(root):
        if strict:
            raise RuntimeError("extractor output directory disappeared")
        return 0, 0
    root = os.path.abspath(root)
    files = 0
    total = 0
    seen = set()
    try:
        root_handle = _windows_handle(root)
    except OSError as error:
        raise RuntimeError(f"extractor output root is unsafe: {error}") from None
    try:
        root_info = _windows_information(root_handle)
        directory_attribute = 0x00000010
        reparse_attribute = 0x00000400
        sparse_attribute = 0x00000200
        if (not root_info.file_attributes & directory_attribute
                or root_info.file_attributes & reparse_attribute):
            raise RuntimeError("extractor output root is not a regular directory")
        root_final = _windows_final_path(root_handle)
        root_prefix = root_final + "\\"
        _assert_no_windows_streams(root)

        def walk(path):
            nonlocal files, total
            try:
                entries = list(os.scandir(path))
            except FileNotFoundError:
                if strict:
                    raise RuntimeError("extractor output changed during validation") from None
                return
            for entry in entries:
                try:
                    handle = _windows_handle(entry.path)
                except FileNotFoundError:
                    if strict:
                        raise RuntimeError("extractor output changed during validation") from None
                    continue
                except OSError as error:
                    raise RuntimeError(f"extractor output is unsafe: {error}") from None
                try:
                    info = _windows_information(handle)
                    final_path = _windows_final_path(handle)
                    if not final_path.casefold().startswith(root_prefix.casefold()):
                        raise RuntimeError("extractor output escapes its physical root")
                    if info.volume_serial_number != root_info.volume_serial_number:
                        raise RuntimeError("extractor output crosses a filesystem boundary")
                    if info.file_attributes & reparse_attribute:
                        raise RuntimeError("extractor produced a reparse point")
                    identity = (
                        info.volume_serial_number,
                        info.file_index_high,
                        info.file_index_low,
                    )
                    if identity in seen:
                        raise RuntimeError("extractor output repeats a physical identity")
                    seen.add(identity)
                    _assert_no_windows_streams(entry.path)
                    if info.file_attributes & directory_attribute:
                        walk(entry.path)
                        continue
                    if kernel32.GetFileType(handle) != 0x0001:
                        raise RuntimeError("extractor produced a special file")
                    if info.number_of_links != 1:
                        raise RuntimeError("extractor produced a multiply-linked file")
                    if strict and info.file_attributes & sparse_attribute:
                        raise RuntimeError("extractor produced a sparse output file")
                    logical = (info.file_size_high << 32) | info.file_size_low
                    allocated = _windows_allocated_size(entry.path)
                    files, total = _add_file(files, total, logical, allocated, limits)
                finally:
                    kernel32.CloseHandle(handle)
        walk(root)
        if shutil.disk_usage(root).free < 50 * 1024 * 1024:
            raise RuntimeError("extractor exhausted the 50 MiB free-space headroom")
        return files, total
    finally:
        kernel32.CloseHandle(root_handle)


class WindowsJob:
    KILL_ON_JOB_CLOSE = 0x00002000
    EXTENDED_LIMIT_INFORMATION = 9
    BASIC_ACCOUNTING_INFORMATION = 1

    def __init__(self, command):
        self.handle = None
        self.process = None
        if os.name != "nt":
            raise RuntimeError("Windows Job Objects are unavailable")
        handle = kernel32.CreateJobObjectW(None, None)
        if not handle:
            raise ctypes.WinError(ctypes.get_last_error())
        self.handle = handle
        limits = JobExtendedLimitInformation()
        limits.basic_limit_information.limit_flags = self.KILL_ON_JOB_CLOSE
        if not kernel32.SetInformationJobObject(
                handle, self.EXTENDED_LIMIT_INFORMATION,
                ctypes.byref(limits), ctypes.sizeof(limits)):
            self.close()
            raise ctypes.WinError(ctypes.get_last_error())
        gate_command = [
            sys.executable, "-I", os.path.realpath(__file__), "--child-gate", "--", *command]
        try:
            self.process = subprocess.Popen(
                gate_command,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
            )
            if not kernel32.AssignProcessToJobObject(handle, int(self.process._handle)):
                raise ctypes.WinError(ctypes.get_last_error())
            self.process.stdin.write(b"1")
            self.process.stdin.close()
        except BaseException:
            if self.process is not None:
                self.process.kill()
                self.process.wait()
            self.close()
            raise

    def close(self):
        if self.handle is not None:
            kernel32.CloseHandle(self.handle)
            self.handle = None

    def terminate(self):
        if self.handle is None:
            return
        cleanup_error = None
        if not kernel32.TerminateJobObject(self.handle, 1):
            cleanup_error = ctypes.WinError(ctypes.get_last_error())
        accounting = JobBasicAccountingInformation()
        deadline = time.monotonic() + 5
        while True:
            if not kernel32.QueryInformationJobObject(
                    self.handle, self.BASIC_ACCOUNTING_INFORMATION,
                    ctypes.byref(accounting), ctypes.sizeof(accounting), None):
                cleanup_error = cleanup_error or ctypes.WinError(
                    ctypes.get_last_error())
                break
            if accounting.active_processes == 0:
                break
            if time.monotonic() >= deadline:
                cleanup_error = cleanup_error or RuntimeError(
                    "extractor process tree did not terminate")
                break
            time.sleep(0.01)
        if self.process is not None:
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        self.close()
        if cleanup_error is not None:
            raise cleanup_error


def start_owned_process(command):
    if os.name == "nt":
        return WindowsJob(command)
    return PosixProcessGroup(command)


def run_child_gate(command):
    if os.name != "nt":
        return 125
    if sys.stdin.buffer.read(1) != b"1":
        return 125
    return subprocess.run(command).returncode


class ProcessCancelled(Exception):
    pass


def run_bounded(command, output_dir, timeout_seconds, max_files, max_file_bytes,
                max_total_bytes, max_diagnostic_bytes):
    diagnostics = bytearray()
    diagnostic_exceeded = threading.Event()
    owner = start_owned_process(command)
    process = owner.process

    def drain_output():
        read_chunk = getattr(process.stdout, "read1", process.stdout.read)
        while True:
            chunk = read_chunk(65536)
            if not chunk:
                return
            remaining = max_diagnostic_bytes - len(diagnostics)
            if remaining > 0:
                diagnostics.extend(chunk[:remaining])
            if len(chunk) > remaining:
                diagnostic_exceeded.set()

    output_thread = threading.Thread(target=drain_output, daemon=True)
    output_thread.start()
    deadline = time.monotonic() + timeout_seconds
    failure = None
    return_code = None
    try:
        while process.poll() is None:
            if diagnostic_exceeded.is_set():
                failure = f"extractor diagnostics exceeded {max_diagnostic_bytes} bytes"
                break
            if time.monotonic() >= deadline:
                failure = f"extractor exceeded {timeout_seconds} seconds"
                break
            try:
                measure_tree(output_dir, max_files, max_file_bytes, max_total_bytes)
            except RuntimeError as error:
                failure = str(error)
                break
            time.sleep(0.1)
        return_code = process.returncode
    finally:
        owner.terminate()
        output_thread.join(timeout=5)
        if output_thread.is_alive():
            failure = failure or "extractor process tree retained its diagnostic pipe"
        else:
            process.stdout.close()

    sys.stdout.buffer.write(diagnostics)
    if failure:
        print(failure, file=sys.stderr)
        return 1
    if diagnostic_exceeded.is_set():
        print(f"extractor diagnostics exceeded {max_diagnostic_bytes} bytes", file=sys.stderr)
        return 1
    try:
        measure_tree(
            output_dir, max_files, max_file_bytes, max_total_bytes, strict=True)
    except RuntimeError as error:
        print(error, file=sys.stderr)
        return 1
    return return_code


def main(argv):
    if argv[1:2] == ["--child-gate"]:
        command = argv[3:] if argv[2:3] == ["--"] else argv[2:]
        return run_child_gate(command) if command else 125
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--timeout-seconds", type=float, required=True)
    parser.add_argument("--max-files", type=int, required=True)
    parser.add_argument("--max-file-bytes", type=int, required=True)
    parser.add_argument("--max-total-bytes", type=int, required=True)
    parser.add_argument("--max-diagnostic-bytes", type=int, required=True)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args(argv[1:])
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command:
        parser.error("an extractor command is required after --")
    if min(args.timeout_seconds, args.max_files, args.max_file_bytes,
           args.max_total_bytes, args.max_diagnostic_bytes) <= 0:
        parser.error("all limits must be positive")
    previous_term_handler = None
    if os.name != "nt" and threading.current_thread() is threading.main_thread():
        previous_term_handler = signal.getsignal(signal.SIGTERM)

        def cancel_process(_signum, _frame):
            raise ProcessCancelled()

        signal.signal(signal.SIGTERM, cancel_process)
    try:
        return run_bounded(
            command,
            args.output_dir,
            args.timeout_seconds,
            args.max_files,
            args.max_file_bytes,
            args.max_total_bytes,
            args.max_diagnostic_bytes,
        )
    except ProcessCancelled:
        print("extractor supervision cancelled", file=sys.stderr)
        return 130
    finally:
        if previous_term_handler is not None:
            signal.signal(signal.SIGTERM, previous_term_handler)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
