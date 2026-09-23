"""Capture a prepared, private GOG DOSBox session without desktop plugins.

Windows only. Input is directed only to the window of the process launched here.
Foreground SendInput remains optional. The unmodified DOSBox recorder is the reference.
"""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--session', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=60)
    parser.add_argument('--format', choices=('midi', 'opl'), default='midi')
    parser.add_argument('--input', choices=('window', 'foreground'), default='window')
    parser.add_argument('--startup-escape-at', type=int, nargs='*', default=[],
                        help='Seconds after launch to dismiss original intro movies')
    args = parser.parse_args()
    if sys.platform != 'win32' or not 20 <= args.seconds <= 600:
        parser.error('Windows and a duration of 20..600 seconds are required')
    if any(second < 0 or second >= 20 for second in args.startup_escape_at):
        parser.error('Startup Escape times must be in 0..19 seconds')
    root = args.session.resolve()
    exe = root / 'game/DOSBOX/DOSBox.exe'
    config = root / 'capture.conf'
    captures = root / 'captures'
    if not exe.is_file() or not config.is_file() or not captures.is_dir():
        parser.error('Expected an existing session from dos_midi_capture.py')
    text = config.read_text(encoding='ascii')
    if '\npause\n' not in text or str(captures) not in text:
        parser.error('Expected private capture path and startup pause')
    extension = '*.dro' if args.format == 'opl' else '*.mid'
    capture_key = 0x76 if args.format == 'opl' else 0x77
    before = set(captures.glob(extension))
    user = ctypes.WinDLL('user32', use_last_error=True)
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.SetForegroundWindow.argtypes = [wintypes.HWND]
    user.GetForegroundWindow.restype = wintypes.HWND
    user.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
    user.AttachThreadInput.argtypes = [wintypes.DWORD, wintypes.DWORD, wintypes.BOOL]
    user.BringWindowToTop.argtypes = [wintypes.HWND]
    user.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    user.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    user.PostMessageW.restype = wintypes.BOOL
    user.MapVirtualKeyW.argtypes = [wintypes.UINT, wintypes.UINT]
    user.MapVirtualKeyW.restype = wintypes.UINT
    environment = dict(os.environ)
    if args.input == 'window':
        environment['SDL_VIDEODRIVER'] = 'windib'
    process = subprocess.Popen([str(exe), '-conf', str(config)], cwd=root, env=environment,
                               creationflags=subprocess.CREATE_NO_WINDOW)
    window = None
    report = {'pid': process.pid, 'seconds': args.seconds, 'format': args.format,
              'input': args.input, 'startup_escape_at': args.startup_escape_at, 'completed': False,
              'dosbox_sha256': hashlib.sha256(exe.read_bytes()).hexdigest(),
              'config_sha256': hashlib.sha256(config.read_bytes()).hexdigest()}

    class Keyboard(ctypes.Structure):
        _fields_ = [('vk', wintypes.WORD), ('scan', wintypes.WORD),
                    ('flags', wintypes.DWORD), ('time', wintypes.DWORD),
                    ('extra', ctypes.c_size_t)]

    class Payload(ctypes.Union):
        _fields_ = [('keyboard', Keyboard), ('padding', ctypes.c_byte * (32 if ctypes.sizeof(ctypes.c_void_p) == 8 else 24))]

    class Input(ctypes.Structure):
        _fields_ = [('type', wintypes.DWORD), ('payload', Payload)]

    user.SendInput.argtypes = [wintypes.UINT, ctypes.POINTER(Input), ctypes.c_int]
    user.SendInput.restype = wintypes.UINT

    def owned():
        owner = wintypes.DWORD()
        user.GetWindowThreadProcessId(window, ctypes.byref(owner))
        if process.poll() is not None or owner.value != process.pid:
            raise RuntimeError('Capture window no longer belongs to the launched process')

    def chord(keys):
        owned()
        if args.input == 'window':
            # SDL 1.2 windib processes these messages into its own modifier state
            # No global keystrokes or foreground activation are needed
            pressed = []

            def post(key, release):
                owned()
                flags = 1 | (user.MapVirtualKeyW(key, 0) << 16)
                if release:
                    flags |= (1 << 30) | (1 << 31)
                if not user.PostMessageW(window, 0x101 if release else 0x100, key, flags):
                    raise ctypes.WinError(ctypes.get_last_error())

            try:
                for key in keys:
                    post(key, False)
                    pressed.append(key)
                    time.sleep(0.08)
            finally:
                for key in reversed(pressed):
                    post(key, True)
                    time.sleep(0.08)
            time.sleep(0.2)
            return
        user.ShowWindow(window, 9)
        foreground = user.GetForegroundWindow()
        foreground_thread = user.GetWindowThreadProcessId(foreground, None) if foreground else 0
        own_thread = ctypes.windll.kernel32.GetCurrentThreadId()
        attached = foreground_thread and user.AttachThreadInput(own_thread, foreground_thread, True)
        try:
            user.BringWindowToTop(window)
            user.SetForegroundWindow(window)
        finally:
            if attached:
                user.AttachThreadInput(own_thread, foreground_thread, False)
        time.sleep(0.2)
        pressed = []

        def send(key, release):
            event = Input(type=1, payload=Payload(keyboard=Keyboard(vk=key, flags=2 if release else 0)))
            if user.SendInput(1, ctypes.byref(event), ctypes.sizeof(event)) != 1:
                raise ctypes.WinError(ctypes.get_last_error())

        try:
            for key in keys:
                owned()
                if user.GetForegroundWindow() != window:
                    raise RuntimeError('Refusing keyboard input: DOSBox is not the foreground window')
                send(key, False)
                pressed.append(key)
                time.sleep(0.08)
        finally:
            for key in reversed(pressed):
                send(key, True)
                time.sleep(0.08)
        time.sleep(0.2)

    try:
        for _ in range(100):
            found = []

            @callback_type
            def visit(hwnd, _):
                owner = wintypes.DWORD()
                user.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
                name = ctypes.create_unicode_buffer(512)
                user.GetWindowTextW(hwnd, name, len(name))
                if owner.value == process.pid and name.value.startswith('DOSBox 0.74,'):
                    found.append(hwnd)
                return True

            user.EnumWindows(visit, 0)
            if len(found) == 1:
                window = found[0]
                break
            if process.poll() is not None:
                raise RuntimeError('DOSBox exited before creating its window')
            time.sleep(0.1)
        if window is None:
            raise RuntimeError('No unique DOSBox window appeared')
        time.sleep(2)
        chord([0x11, 0x12, capture_key])  # Ctrl+Alt+F7/F8: arm OPL/MIDI recording
        chord([0x20])  # Space: release the prepared startup pause
        print(f'Launched private DOSBox and armed {args.format} capture', flush=True)
        for elapsed in range(args.seconds):
            if process.poll() is not None:
                raise RuntimeError('DOSBox exited while recording')
            if elapsed in args.startup_escape_at:
                chord([0x1b])
            time.sleep(1)
            if elapsed == 19 and not (set(captures.glob(extension)) - before):
                raise RuntimeError('No capture appeared within 20 seconds')
        chord([0x11, 0x12, capture_key])
        files = set(captures.glob(extension)) - before
        if len(files) != 1:
            raise RuntimeError(f'Expected one new capture, found {len(files)}')
        path = files.pop()
        sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tests'))
        if args.format == 'opl':
            from fm_feasibility.opl_trace import read_dro, note_states
            writes, metadata = read_dro(path)
            ons, offs = note_states(writes)
            duration, notes = metadata['duration_ms'], len(ons)
            report.update(register_writes=len(writes), key_offs=len(offs), opl=metadata)
        else:
            from midi_diff import read_midi
            events, duration = read_midi(path)
            notes = sum(e.status & 0xf0 == 0x90 and e.data[1] > 0 for e in events)
            report['channel_and_sysex_messages'] = len(events)
        if duration < 10000 or notes < 20:
            raise RuntimeError('Capture is too short or contains too few notes')
        report.update(completed=True, capture=str(path), duration_ms=duration,
                      capture_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                      note_ons=notes)
        print(json.dumps(report, indent=2), flush=True)
    except BaseException as error:
        report['error'] = str(error)
        if window and process.poll() is None:
            try:
                chord([0x11, 0x74])  # Ctrl+F5: DOSBox's own diagnostic screenshot
            except (RuntimeError, OSError) as capture_error:
                report['diagnostic_error'] = str(capture_error)
        raise
    finally:
        try:
            if window and process.poll() is None:
                chord([0x11, 0x78])  # Ctrl+F9: exit this DOSBox only
            process.wait(timeout=5)
        except (RuntimeError, OSError, subprocess.TimeoutExpired) as error:
            report['shutdown_error'] = str(error)
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
            (root / 'unattended-capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
