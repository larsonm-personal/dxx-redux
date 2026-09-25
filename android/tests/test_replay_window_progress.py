"""Windows integration: fast replay progress and native D1 pause/resume."""

import argparse
import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import re
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", choices=("d1", "d2"), required=True)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--demo", type=Path, required=True)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    subprocess.run(["pwsh", "-NoProfile", "-File", str(repo / "android/helpers/retain-recent-artifacts.ps1"),
                    "-Artifacts", str(output)], check=True)
    output.mkdir(parents=True, exist_ok=False)
    suffix = output.name
    sandbox = repo / "temp/input_demo_runtime_wrapper" / args.game / (args.demo.stem + "__" + suffix)
    expected_exe = sandbox / (args.game + "x-redux.exe")
    command = ["pwsh", "-NoProfile", "-File", str(repo / "android/tests/run_input_demo_replay.ps1"),
               "-DemoPath", str(args.demo.resolve()), "-DataDir", str(args.data.resolve()),
               "-ExecutablePath", str(args.exe.resolve()), "-Runner", "fast", "-Mode", "accelerated",
               "-SkipExpectedChecks", "-KeepSandbox", "-SandboxSuffix", suffix,
               "-TimeoutSeconds", "240", "-ResultCopyPath", str(output / "result.json"),
               "-StateLogPath", str(output / "state.jsonl.gz"), "-RngLogPath", str(output / "rng.jsonl")]
    command += ["-D1InD2"] if args.game == "d2" else ["-Game", "d1"]
    user = ctypes.WinDLL("user32", use_last_error=True)
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    user.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR,
                                                ctypes.POINTER(wintypes.DWORD)]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]

    def find_window():
        found = []

        @callback_type
        def visit(hwnd, unused):
            pid = wintypes.DWORD()
            user.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            handle = kernel.OpenProcess(0x1000, False, pid.value)
            if handle:
                try:
                    path, size = ctypes.create_unicode_buffer(32768), wintypes.DWORD(32768)
                    if kernel.QueryFullProcessImageNameW(handle, 0, path, ctypes.byref(size)):
                        if Path(path.value) == expected_exe:
                            title = ctypes.create_unicode_buffer(256)
                            user.GetWindowTextW(hwnd, title, len(title))
                            if title.value:
                                found.append((hwnd, title.value))
                finally:
                    kernel.CloseHandle(handle)
            return True

        user.EnumWindows(visit, 0)
        return found

    def pause_key(hwnd):
        # Send only to this test's isolated engine window; never focus another app
        for message, flags in ((0x100, 1 | (0x45 << 16)), (0x101, 1 | (0x45 << 16) | (3 << 30))):
            if not user.PostMessageW(hwnd, message, 0x13, flags):
                raise ctypes.WinError(ctypes.get_last_error())

    report = {"command": command, "sandbox": str(sandbox), "events": []}
    phase, paused_frame, pause_time = "running", None, None
    first_frame = None
    with (output / "runner.log").open("w") as log:
        process = subprocess.Popen(command, cwd=repo, stdout=log, stderr=subprocess.STDOUT)
        while process.poll() is None:
            for hwnd, title in find_window():
                match = re.match(r"Input replay: (\d+) / (\d+)", title)
                if not match:
                    continue
                frame = int(match[1])
                # D2 deliberately filters the Pause key during replay
                if args.game == "d2":
                    if first_frame is None:
                        first_frame = frame
                        report["events"].append({"action": "progress_started", "title": title})
                    elif phase != "progressed" and frame > first_frame:
                        report["events"].append({"action": "progress_advanced", "title": title})
                        phase = "progressed"
                    continue
                if phase == "running" and frame >= 2:
                    report["events"].append({"action": "pause", "title": title})
                    pause_key(hwnd)
                    phase = "pause_requested"
                elif phase == "pause_requested" and "waiting for dialog" in title:
                    paused_frame, pause_time = frame, time.monotonic()
                    report["events"].append({"action": "dialog_visible", "title": title})
                    phase = "paused"
                elif phase == "paused" and time.monotonic() - pause_time >= 1:
                    report["pause_frame_unchanged"] = frame == paused_frame
                    pause_key(hwnd)
                    phase = "resume_requested"
                elif phase == "resume_requested" and "waiting for dialog" not in title and frame > paused_frame:
                    report["events"].append({"action": "resumed", "title": title})
                    phase = "resumed"
            time.sleep(0.1)
        report["exit_code"] = process.returncode
    interaction_ok = phase == "progressed" if args.game == "d2" else phase == "resumed" and report.get("pause_frame_unchanged")
    report["status"] = "pass" if interaction_ok and process.returncode == 0 else "fail"
    (output / "window-report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
