#!/usr/bin/env python3
"""Run an extractor with bounded time, diagnostics, and output."""

import argparse
import os
import signal
import shutil
import subprocess
import sys
import threading
import time


def measure_tree(root, max_files, max_file_bytes, max_total_bytes):
    files = 0
    total = 0
    if not os.path.isdir(root):
        return files, total
    for dir_path, _, names in os.walk(root, followlinks=False):
        for name in names:
            path = os.path.join(dir_path, name)
            size = os.lstat(path).st_size
            files += 1
            total += size
            if files > max_files:
                raise RuntimeError(f"extractor exceeded {max_files} output files")
            if size > max_file_bytes:
                raise RuntimeError(f"extractor output file exceeded {max_file_bytes} bytes")
            if total > max_total_bytes:
                raise RuntimeError(f"extractor output exceeded {max_total_bytes} bytes")
    if shutil.disk_usage(root).free < 50 * 1024 * 1024:
        raise RuntimeError("extractor exhausted the 50 MiB free-space headroom")
    return files, total


def stop_process_tree(process):
    if process.poll() is not None:
        return
    if os.name == "nt":
        subprocess.run(
            ["taskkill", "/PID", str(process.pid), "/T", "/F"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
    else:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def run_bounded(command, output_dir, timeout_seconds, max_files, max_file_bytes,
                max_total_bytes, max_diagnostic_bytes):
    diagnostics = bytearray()
    diagnostic_exceeded = threading.Event()
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        start_new_session=os.name != "nt",
    )

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
    finally:
        if failure:
            stop_process_tree(process)
        output_thread.join(timeout=5)
        process.stdout.close()

    sys.stdout.buffer.write(diagnostics)
    if failure:
        print(failure, file=sys.stderr)
        return 1
    if diagnostic_exceeded.is_set():
        print(f"extractor diagnostics exceeded {max_diagnostic_bytes} bytes", file=sys.stderr)
        return 1
    try:
        measure_tree(output_dir, max_files, max_file_bytes, max_total_bytes)
    except RuntimeError as error:
        print(error, file=sys.stderr)
        return 1
    return process.returncode


def main(argv):
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
    return run_bounded(
        command,
        args.output_dir,
        args.timeout_seconds,
        args.max_files,
        args.max_file_bytes,
        args.max_total_bytes,
        args.max_diagnostic_bytes,
    )


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
