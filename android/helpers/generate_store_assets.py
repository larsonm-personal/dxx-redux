#!/usr/bin/env python3
"""Capture real Android store assets on the dedicated DxxStoreAssets emulator."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import time
import re
import xml.etree.ElementTree as ET
import zipfile
import uuid

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = "com.dxxredux.app"


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


class Capture:
    def __init__(self, output, serial="emulator-5580"):
        self.output = Path(output).resolve()
        self.output.mkdir(parents=True, exist_ok=True)
        self.serial = serial
        sdk = Path(os.environ.get("ANDROID_HOME", "C:/local/android-sdk"))
        self.adb = sdk / "platform-tools" / ("adb.exe" if os.name == "nt" else "adb")
        name = self.call("emu", "avd", "name")
        if "DxxStoreAssets" not in name.splitlines():
            raise RuntimeError("Capture requires the dedicated DxxStoreAssets AVD")

    def call(self, *args, binary=False, check=True):
        result = subprocess.run(
            [str(self.adb), "-s", self.serial, *map(str, args)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=120,
            check=False,
        )
        if check and result.returncode:
            raise RuntimeError(
                result.stderr.decode("utf-8", errors="replace") + result.stdout.decode("utf-8", errors="replace")
            )
        return result.stdout if binary else result.stdout.decode("utf-8", errors="replace").strip()

    def shell(self, *args, **kwargs):
        return self.call("shell", shlex.join(map(str, args)), **kwargs)

    def private(self, *args, **kwargs):
        return self.shell("run-as", PACKAGE, *args, **kwargs)

    def broadcast(self, action, **extras):
        args = ["am", "broadcast", "-a", "com.dxxredux." + action, "-p", PACKAGE]
        for key, value in extras.items():
            kind = "--ez" if isinstance(value, bool) else "--ei" if isinstance(value, int) else "--es"
            args += [kind, key, str(value).lower() if isinstance(value, bool) else str(value)]
        result = self.shell(*args)
        if "result=1" in result:
            raise RuntimeError(result)
        return result

    def command(self, command, **extras):
        return self.broadcast("SETUP_COMMAND", command=command, **extras)

    def stage(self, source, destination):
        remote = "/data/local/tmp/store-assets-stage"
        self.call("push", source, remote)
        self.private("mkdir", "-p", str(Path(destination).parent).replace("\\", "/"))
        self.private("cp", remote, destination)
        self.shell("rm", "-f", remote)

    def state(self, setup=False):
        path = "files/" + ("setup_introspect.json" if setup else "introspect.json")
        self.private("rm", "-f", path)
        self.broadcast("SETUP_INTROSPECT" if setup else "INTROSPECT")
        for _ in range(30):
            raw = self.private("cat", path, check=False)
            if raw:
                return json.loads(raw)
            time.sleep(0.1)
        raise ValueError("No fresh introspection response")

    def wait(self, predicate, setup=False, timeout=60):
        deadline = time.monotonic() + timeout
        last = None
        while time.monotonic() < deadline:
            try:
                last = self.state(setup)
                if predicate(last):
                    return last
            except (ValueError, subprocess.CalledProcessError):
                pass
            time.sleep(0.3)
        write_json(self.output / "wait-timeout.json", last)
        raise RuntimeError("Timed out waiting for app state; see wait-timeout.json")

    def launcher(self, **extras):
        self.shell("am", "force-stop", PACKAGE)
        args = ["am", "start", "-W", "-n", PACKAGE + "/.SetupActivity"]
        for key, value in extras.items():
            args += ["--es", key, value]
        self.shell(*args)
        return self.wait(lambda s: "buttons" in s, setup=True)

    def automate(self, steps, setup=False, name="steps", timeout=90):
        run_id = name + "-" + uuid.uuid4().hex[:8]
        path = self.output / "scripts" / (name + ".json")
        write_json(
            path,
            [{"_info": {"_standalone": False, "_owner": "generate-store-assets.ps1", "games": ["d1", "d2"]}}, *steps],
        )
        self.stage(path, "files/store-assets.json")
        self.private("rm", "-f", "files/automation_result.json")
        self.broadcast("SETUP_AUTOMATE" if setup else "AUTOMATE", script="store-assets.json", run_id=run_id)
        if len(steps) == 1 and steps[0]["action"] == "enter_launcher":
            # This standalone game script has no suspended launcher executor.
            # The launcher discards its continuation token; verify the save/UI.
            return self.wait(
                lambda s: (
                    not s.get("game_running")
                    and s.get("running_game_pid") == -1
                    and s.get("resume_offer_enabled")
                    and s.get("resume_candidate")
                ),
                setup=True,
            )
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            raw = self.private("cat", "files/automation_result.json", check=False)
            if raw:
                result = json.loads(raw)
                if result.get("run_id") != run_id:
                    time.sleep(0.1)
                    continue
                if result.get("result") not in ("PASS", "FAIL"):
                    time.sleep(0.25)
                    continue
                write_json(self.output / "scripts" / (name + "-result.json"), result)
                log = self.private("cat", "files/automation_log.jsonl", check=False)
                (self.output / "scripts" / (name + "-log.jsonl")).write_text(log, encoding="utf-8")
                if result.get("result") != "PASS":
                    raise RuntimeError(f"Automation failed: {result}")
                return result
            time.sleep(0.25)
        raise RuntimeError(f"Automation timed out: {name}")

    def screenshot(self, name):
        path = self.output / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(self.call("exec-out", "screencap", "-p", binary=True))
        return path

    def start_audio(self):
        before = time.monotonic()
        device = float(self.shell("cat", "/proc/uptime").split()[0])
        self.audio_clock_offset = (before + time.monotonic()) / 2 - device
        self.step("set_debug", field="capture_audio", value="start")
        self.audio_active = True
        state = self.state()
        self.audio_state = {key: state.get(key, {}) for key in ("audio", "music")}

    def stop_audio(self, video):
        if not getattr(self, "audio_active", False):
            return
        self.step("set_debug", field="capture_audio", value="stop")
        self.audio_active = False
        metadata = json.loads(self.private("cat", "files/store-audio.json"))
        path = video.with_suffix(".wav")
        path.write_bytes(self.call("exec-out", "run-as", PACKAGE, "cat", "files/store-audio.wav", binary=True))
        metadata["source"] = str(path.relative_to(self.output))
        if not metadata.get("music_stem"):
            raise RuntimeError("Capture APK must support isolated engine music")
        music = video.with_suffix(".music.wav")
        music.write_bytes(self.call("exec-out", "run-as", PACKAGE, "cat", "files/store-music.wav", binary=True))
        metadata["music_source"] = str(music.relative_to(self.output))
        metadata["video_offset"] = metadata["start_monotonic_seconds"] + self.audio_clock_offset - self.record_started
        metadata["engine_audio"] = self.audio_state
        write_json(video.with_suffix(".audio.json"), metadata)

    def start_recording(self, name, host=False, audio=True):
        if audio:
            self.start_audio()
        self.record_host = host
        if not host:
            remote = "/sdcard/store-assets.mp4"
            self.shell("rm", "-f", remote, remote + ".pid")
            command = f"echo $$ > {remote}.pid; exec screenrecord --verbose --size 2400x1080 --bit-rate 16000000 --time-limit 180 {remote}"
            self.record_log = (self.output / (name + "-screenrecord.log")).open("wb")
            self.record_process = subprocess.Popen(
                [str(self.adb), "-s", self.serial, "shell", command],
                stdout=self.record_log,
                stderr=subprocess.STDOUT,
            )
            self.record_started = time.monotonic()
            self.record_name = name
            previous = self.record_started
            deadline = previous + 20
            while time.monotonic() < deadline:
                size = self.shell("stat", "-c", "%s", remote, check=False)
                now = time.monotonic()
                if size.isdigit() and int(size) > 64:
                    # Bound the first encoded frame between file-size polls,
                    # rather than counting potentially slow codec startup
                    self.record_started = (previous + now) / 2
                    break
                if self.record_process.poll() is not None:
                    self.record_log.close()
                    raise RuntimeError("screenrecord failed during codec startup; see capture log")
                previous = now
                time.sleep(0.1)
            else:
                pid = self.shell("cat", remote + ".pid")
                if pid.isdigit():
                    self.shell("kill", "-2", pid, check=False)
                self.record_process.wait(timeout=20)
                self.record_log.close()
                raise RuntimeError("screenrecord produced no frames in 20 seconds")
            return self.record_started
        # Host recording uses the physical portrait framebuffer. Rotate it in
        # the editor; requesting landscape here stretches the rotated pixels.
        self.record_path = self.output / "raw" / (name + ".webm")
        self.record_path.parent.mkdir(parents=True, exist_ok=True)
        self.record_started = time.monotonic()
        self.record_name = name
        result = self.call(
            "emu",
            "screenrecord",
            "start",
            "--fps",
            "60",
            "--size",
            "1080x2400",
            "--bit-rate",
            "16000000",
            "--time-limit",
            "180",
            self.record_path,
        )
        if "KO" in result:
            raise RuntimeError("Host recording failed: " + result)
        time.sleep(0.4)
        return self.record_started

    def stop_recording(self):
        if not self.record_host:
            pid = self.shell("cat", "/sdcard/store-assets.mp4.pid")
            if pid.isdigit() and self.record_process.poll() is None:
                self.shell("kill", "-2", pid)
            self.record_process.wait(timeout=20)
            self.record_log.close()
            path = self.output / "raw" / (self.record_name + ".mp4")
            path.parent.mkdir(parents=True, exist_ok=True)
            self.call("pull", "/sdcard/store-assets.mp4", path)
            self.stop_audio(path)
            if path.stat().st_size < 1024:
                raise RuntimeError("screenrecord produced empty media; see capture log")
            return path
        result = self.call("emu", "screenrecord", "stop")
        if "KO" in result:
            raise RuntimeError("Host recording stop failed: " + result)
        time.sleep(0.5)
        if not self.record_path.exists() or not self.record_path.stat().st_size:
            raise RuntimeError("Host recording produced no media")
        self.stop_audio(self.record_path)
        return self.record_path

    def step(self, action, setup=False, **fields):
        return self.automate([{"action": action, **fields}], setup, name="step-" + action)

    def tap(self, text):
        return self.step("tap_button", setup=True, text=text, post_delay_ms=250)

    def ui_nodes(self):
        self.shell("uiautomator", "dump", "/sdcard/store-assets-ui.xml")
        root = ET.fromstring(self.shell("cat", "/sdcard/store-assets-ui.xml"))
        return list(root.iter("node"))

    def ui_tap(self, label):
        nodes = self.ui_nodes()
        matches = [n for n in nodes if label in (n.get("text"), n.get("content-desc"), n.get("resource-id"))]
        if not matches:
            raise RuntimeError(
                f"System picker item {label!r} missing: " + str([(n.get("text"), n.get("content-desc")) for n in nodes])
            )
        bounds = list(map(int, re.findall(r"\d+", matches[0].get("bounds"))))
        self.shell("input", "tap", (bounds[0] + bounds[2]) // 2, (bounds[1] + bounds[3]) // 2)

    def pilot_menu(self, game, mark=None):
        self.wait(lambda s: s.get("screen_mode") == "menu" or s.get("intro_active"), timeout=45)
        self.step("skip_intro", timeout_ms=20000, post_delay_ms=200)
        state = self.wait(lambda s: bool(s.get("menu")), timeout=30)
        menu = state["menu"]
        if mark:
            self.start_audio()
            mark("Pilot name")
            time.sleep(0.5)
        if "Enter your pilot" in menu.get("subtitle", ""):
            self.step("select", text="Ok", post_delay_ms=300) if game == "d2" else self.step(
                "key", key="enter", post_delay_ms=300
            )
        elif "pilot" in (menu.get("title", "") + menu.get("subtitle", "")).lower():
            self.step("select", text="player", post_delay_ms=300)

    def campaign(self, game, briefing=False, mark=None):
        self.step("select", text="New game", post_delay_ms=300)
        if mark:
            mark("Campaign")
        self.step("select_mission", text="Counterstrike" if game == "d2" else "First Strike", post_delay_ms=300)
        self.step("select", text="Ok", post_delay_ms=300) if game == "d2" else self.step(
            "key", key="enter", post_delay_ms=300
        )
        if mark:
            mark("Choose difficulty")
        self.step("select", text="Rookie", post_delay_ms=300)
        if mark:
            mark("Difficulty selected")
        if briefing:
            self.wait(lambda s: s.get("screen_advance_kind") == "briefing", timeout=20)
            # Retail D2 normally chooses ambient hum here. For this requested
            # MIDI-backed preview, ask the engine to play its own briefing song
            self.step("music_control", operation="briefing")
            for page in range(1, 4):
                self.step("key", key="space", post_delay_ms=300)
                if mark:
                    mark(f"Briefing page {page}")
                time.sleep(0.4)
                if mark:
                    self.screenshot(f"briefing/page-{page}-later.png")
                if page < 3:
                    self.step("key", key="space", post_delay_ms=300)
        if game == "d1":
            self.step("key", key="escape", post_delay_ms=300)
        else:
            self.step("skip_briefing", timeout_ms=15000, post_delay_ms=300)
        return self.wait(lambda s: s.get("in_game") and s.get("game_window_is_front"), timeout=20)

    def seed(self):
        for game in ("d1", "d2"):
            self.launcher()
            self.command("launch", game=game)
            self.pilot_menu(game)
            self.campaign(game)
            self.automate([{"action": "enter_launcher"}], name="seed-" + game + "-save")
        self.tap("Game Preferences")
        self.tap("Restore defaults")
        self.tap("Confirm")
        self.tap("< Back")
        self.launcher_stills()

    def launcher_stills(self):
        self.launcher()
        self.step("scroll", setup=True, direction="up", count=8)
        state = self.state(True)
        if not state.get("resume_candidate", {}).get("has_thumbnail"):
            raise RuntimeError("Launcher requires a real populated save with thumbnail")
        write_json(self.output / "launcher/top-state.json", state)
        self.screenshot("launcher/01-top.png")
        self.step("scroll", setup=True, direction="down", count=8)
        self.screenshot("launcher/02-bottom.png")
        self.tap("Save Explorer")
        self.screenshot("launcher/03-save-explorer.png")
        self.launcher()

    def opening(self):
        self.launcher()
        self.command("clear_pilot_files")
        archive = self.output / "Descent-Game-Data.zip"
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as stream:
            # Read only the staged, verified assets; importing this archive goes
            # through the same system picker and app importer as a user's files
            for asset in json.loads((self.output / "game-data.json").read_text()):
                data = self.call(
                    "exec-out", "run-as", PACKAGE, "cat", "files/store-input/" + asset["name"], binary=True
                )
                stream.writestr(asset["name"], data)
        self.call("push", archive, "/sdcard/Download/Descent-Game-Data.zip")
        self.shell(
            "am",
            "broadcast",
            "-a",
            "android.intent.action.MEDIA_SCANNER_SCAN_FILE",
            "-d",
            "file:///sdcard/Download/Descent-Game-Data.zip",
        )
        self.shell("wm", "user-rotation", "lock", "1")
        self.shell("wm", "fixed-to-user-rotation", "enabled")
        self.launcher()
        self.step("scroll", setup=True, direction="up", count=8)
        self.start_recording("opening", audio=False)
        beats = []

        def mark(label):
            beats.append({"label": label, "wall": time.monotonic() - self.record_started})
            if label.startswith("Briefing page"):
                page = label.rsplit(" ", 1)[1]
                write_json(self.output / f"briefing/page-{page}-state.json", self.state())
                self.screenshot(f"briefing/page-{page}.png")

        try:
            mark("Launcher")
            time.sleep(0.5)
            self.tap("Select Game Files or Archive")
            self.tap("Pick One or More Files")
            mark("Pick game data")
            nodes = self.ui_nodes()
            if not any(n.get("text") == "Descent-Game-Data.zip" for n in nodes):
                self.ui_tap("Show roots")
                self.ui_tap("Downloads")
            mark("File picker ready")
            time.sleep(0.4)
            self.ui_tap("Descent-Game-Data.zip")
            self.wait(lambda s: s.get("d2", {}).get("ready") and not s.get("scanning", False), setup=True)
            mark("Imported game data")
            self.step("scroll", setup=True, direction="down", count=8)
            mark("Launcher bottom")
            time.sleep(0.4)
            self.tap("Launch Descent 2")
            mark("Launch Descent 2")
            self.pilot_menu("d2", mark=mark)
            mark("Created pilot")
            self.campaign("d2", briefing=True, mark=mark)
            mark("In level")
            self.automate(
                [
                    {"action": "set_debug", "field": "show_robot_hostage_counts", "value": "true"},
                    {"action": "set_debug", "field": "show_boss_health_bar", "value": "true"},
                    {"action": "wait_ms", "ms": 1000},
                ],
                name="opening-final",
            )
            mark("End")
        finally:
            path = self.stop_recording()
            write_json(self.output / "opening.json", {"source": str(path.relative_to(self.output)), "beats": beats})
            self.shell("wm", "fixed-to-user-rotation", "default")
            self.shell("wm", "user-rotation", "lock", "0")

    def flyout(self, audio_pass=False):
        self.launcher()
        self.command("launch", game="d1")
        self.pilot_menu("d1")
        self.campaign("d1")
        name = "flyout-audio" if audio_pass else "flyout"
        rate = 1 if audio_pass else 8
        self.start_recording(name, host=not audio_pass, audio=audio_pass)
        samples = []
        try:
            self.step("trigger_endlevel", value="from_exit_tunnel", capture_slowdown=rate)
            state = self.wait(lambda s: s.get("endlevel_sequence", 0) > 0, timeout=10)
            deadline = time.monotonic() + 120
            while time.monotonic() < deadline:
                state = self.state()
                samples.append(
                    {
                        "wall": time.monotonic() - self.record_started,
                        "phase": state.get("endlevel_sequence", 0),
                        "screen": state.get("screen_advance_kind"),
                    }
                )
                if not state.get("endlevel_sequence"):
                    break
                time.sleep(0.08)
            else:
                raise RuntimeError("Fly-out did not finish")
        finally:
            path = self.stop_recording()
            write_json(
                self.output / (name + ".json"),
                {
                    "source": str(path.relative_to(self.output)),
                    "samples": samples,
                    "playback_rate": rate,
                    "transpose": None if audio_pass else "cclock",
                },
            )
        if not audio_pass:
            self.flyout(audio_pass=True)

    def replay(self, source, video_only=False):
        source = Path(source).resolve()
        name = source.stem
        previous = None
        if video_only:
            previous = json.loads((self.output / "demos" / name / "capture.json").read_text())
        frames = [0.0]
        frame_time = 0
        with source.open(encoding="utf-8") as stream:
            header = json.loads(next(stream))
            for line in stream:
                record = json.loads(line)
                if record["type"] == "frame":
                    frame_time = record.get("ft", frame_time)
                    frames.append(frames[-1] + frame_time / 65536)
        self.launcher(input_demo_replay="/data/user/0/" + PACKAGE + "/files/store-demo.dximdemo")
        self.stage(source, "files/store-demo.dximdemo")
        self.private("rm", "-f", "files/store-demo.dximdemo.actual.json")
        self.call("logcat", "-c")
        self.command("launch", game=header["game"])
        state = self.wait(lambda s: s.get("input_demo", {}).get("replaying"), timeout=45)
        # Replay intentionally restores simulation options from its recording.
        # Apply only visual helpers which its pilot-less startup leaves disabled.
        self.automate(
            [
                {"action": "set_debug", "field": "show_robot_hostage_counts", "value": "true"},
                {"action": "set_debug", "field": "show_boss_health_bar", "value": "true"},
            ],
            name=name + "-presentation",
        )
        state = self.state()
        if not state["hud_layout"]["show_robot_hostage_counts"]:
            raise RuntimeError("Replay HUD counters were not enabled")
        write_json(self.output / "demos" / name / "start-state.json", state)
        manifest = {
            "source": str(source.relative_to(ROOT)),
            "sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "duration": frames[-1],
            "game": header["game"],
            "level": header["level"],
            "samples": [],
            "screenshots": [],
            "parts": [],
            "simulation_fps": (len(frames) - 1) / frames[-1],
            "video_capture": "guest-vfr-no-screenshots" if video_only else "guest-vfr-with-screenshots",
        }
        if video_only:
            manifest["screenshots"] = previous["screenshots"]
        else:
            filename = f"demos/{name}/0000.0s.png"
            self.screenshot(filename)
            manifest["screenshots"].append(
                {"file": filename, "demo_seconds": frames[state["input_demo"]["replay_frame"]], "target_seconds": 0}
            )
        part = 0
        self.start_recording(name + f"-{part:02d}")
        next_sample = 10.0
        deadline = time.monotonic() + frames[-1] * 2 + 60
        try:
            while time.monotonic() < deadline:
                state = self.state()
                wall = time.monotonic() - self.record_started
                replay = state.get("input_demo", {})
                if not replay.get("replaying"):
                    break
                frame = replay["replay_frame"]
                elapsed = frames[min(frame, len(frames) - 1)]
                manifest["samples"].append(
                    {
                        "part": part,
                        "wall": wall,
                        "frame": frame,
                        "demo_seconds": elapsed,
                        "endlevel": state.get("endlevel_sequence", 0),
                    }
                )
                if elapsed >= next_sample:
                    if not video_only:
                        filename = f"demos/{name}/{next_sample:06.1f}s.png"
                        self.screenshot(filename)
                        manifest["screenshots"].append(
                            {"file": filename, "demo_seconds": elapsed, "target_seconds": next_sample}
                        )
                    print(f"{name}: {elapsed:.1f}/{frames[-1]:.1f}s", flush=True)
                    next_sample += 10
                if wall > 165:
                    manifest["parts"].append(str(self.stop_recording().relative_to(self.output)))
                    part += 1
                    self.start_recording(name + f"-{part:02d}")
                time.sleep(0.15)
            else:
                raise RuntimeError("Demo capture exceeded its timeout")
        finally:
            manifest["parts"].append(str(self.stop_recording().relative_to(self.output)))
            log = self.call("logcat", "-d")
            (self.output / "demos" / name / "logcat.txt").write_text(log, encoding="utf-8")
            result = self.private("cat", "files/store-demo.dximdemo.actual.json", check=False)
            if result:
                manifest["replay_result"] = json.loads(result)
            write_json(self.output / "demos" / name / "capture.json", manifest)
        return manifest

    def prepare(self, apk):
        if not apk.exists():
            apk = ROOT / "android/app/build/intermediates/apk/debug/app-debug.apk"
        self.call("install", "-r", "-t", apk)
        self.shell("wm", "size", "1080x2400")
        self.shell("wm", "density", "420")
        self.shell("wm", "fixed-to-user-rotation", "default")
        self.shell("wm", "user-rotation", "lock", "0")
        self.shell("settings", "put", "system", "screen_off_timeout", "2147483647")
        self.shell("settings", "put", "system", "accelerometer_rotation", "0")
        self.shell("settings", "put", "system", "user_rotation", "0")
        self.shell("settings", "put", "secure", "immersive_mode_confirmations", "confirmed")
        self.shell("settings", "put", "global", "sysui_demo_allowed", "1")
        for extras in (("clock", "hhmm", "0941"), ("notifications", "visible", "false"), ("battery", "level", "100")):
            self.shell(
                "am",
                "broadcast",
                "-a",
                "com.android.systemui.demo",
                "--es",
                "command",
                extras[0],
                "--es",
                extras[1],
                extras[2],
            )
        self.shell("input", "keyevent", "82")
        self.launcher()
        # Use the repository's existing content-hash index, never embed game data
        entries = {}
        for line in (ROOT / "game_data/game_data_index.txt").read_text(encoding="utf-8-sig").splitlines():
            if len(line) > 66 and not line.startswith("#"):
                digest, relative = line.split(None, 1)
                entries.setdefault(Path(relative).name.lower(), (digest, ROOT / relative))
        names = [
            "descent.hog",
            "descent.pig",
            "descent2.hog",
            "descent2.ham",
            "descent2.s22",
            "groupa.pig",
            "alien1.pig",
            "alien2.pig",
            "fire.pig",
            "ice.pig",
            "water.pig",
            "robots-h.mvl",
        ]
        # Robot movies are not in older repository indexes. Discover the owned
        # full-game library locally and verify its known retail content hash
        if "robots-h.mvl" not in entries:
            digest = "f491f078308a310b53bb46477b916f9de4cce9358a77b733e31b1bce86135b0a"
            for source in (ROOT / "game_data").rglob("*"):
                if source.name.lower() == "robots-h.mvl" and hashlib.sha256(source.read_bytes()).hexdigest() == digest:
                    entries["robots-h.mvl"] = (digest, source)
                    break
            else:
                raise RuntimeError("Full D2 ROBOTS-H.MVL required under game_data for animated briefings")
        assets = []
        for name in names:
            digest, source = entries[name]
            actual = hashlib.sha256(source.read_bytes()).hexdigest()
            if actual != digest:
                raise RuntimeError(f"Game data hash mismatch: {source}")
            self.stage(source, "files/store-input/" + name)
            self.command("import_files", path="/data/user/0/" + PACKAGE + "/files/store-input/" + name)
            assets.append({"name": name, "sha256": actual})
        write_json(self.output / "game-data.json", assets)
        state = self.wait(lambda s: s.get("d1", {}).get("ready") and s.get("d2", {}).get("ready"), setup=True)
        write_json(self.output / "prepared.json", state)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "action",
        choices=[
            "all",
            "prepare",
            "state",
            "screenshot",
            "script",
            "replay",
            "seed",
            "opening",
            "flyout",
            "compose",
            "review",
            "validate",
        ],
    )
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--serial", default="emulator-5580")
    parser.add_argument("--apk", type=Path, default=ROOT / "android/app/build/outputs/apk/debug/app-debug.apk")
    parser.add_argument("--setup", action="store_true")
    parser.add_argument("--name", default="capture.png")
    parser.add_argument("--script", type=Path)
    parser.add_argument("--demo", type=Path)
    parser.add_argument("--all-demos", action="store_true")
    parser.add_argument(
        "--video-only", action="store_true", help="Recapture replay video using existing screenshot manifests"
    )
    parser.add_argument("--recipe", type=Path)
    args = parser.parse_args()
    if args.action in ("compose", "review", "validate"):
        from store_asset_media import compose, review, validate

        if args.action == "compose":
            print(compose(args.output, args.recipe))
        elif args.action == "review":
            review(args.output)
        else:
            print(json.dumps(validate(args.output), indent=2))
        return
    capture = Capture(args.output, args.serial)
    if args.action == "all":
        from store_asset_media import compose, validate

        capture.shell("am", "force-stop", PACKAGE)
        capture.call("install", "-r", "-t", args.apk)
        capture.shell("pm", "clear", PACKAGE)
        capture.prepare(args.apk)
        write_json(
            args.output / "build.json",
            {
                "apk_sha256": hashlib.sha256(args.apk.read_bytes()).hexdigest(),
                "git_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                "git_status": subprocess.check_output(["git", "status", "--short"], cwd=ROOT, text=True),
                "device": capture.shell("getprop", "ro.build.fingerprint"),
                "resolution": capture.shell("wm", "size"),
            },
        )
        print("Seeding saves and capturing launcher", flush=True)
        capture.seed()
        print("Recording launcher-to-level flow", flush=True)
        capture.opening()
        print("Recording D1 fly-out", flush=True)
        capture.flyout()
        demo_dir = ROOT / "android/regression_demos"
        demos = (
            sorted(demo_dir.glob("*.dximdemo"))
            if args.all_demos
            else [
                demo_dir / "d1_descent_level5_20260616_202713.dximdemo",
                demo_dir / "d1_descent_level18_20260618_202117.dximdemo",
                demo_dir / "d2_descent2_level9_20260511_192804.dximdemo",
            ]
        )
        for demo in demos:
            print("Capturing " + demo.name, flush=True)
            capture.replay(demo)
        compose(args.output, args.recipe)
        validate(args.output)
        capture.launcher()
    elif args.action == "prepare":
        capture.prepare(args.apk)
    elif args.action == "state":
        state = capture.state(args.setup)
        write_json(args.output / "state.json", state)
        print(json.dumps(state, indent=2))
    elif args.action == "screenshot":
        print(capture.screenshot(args.name))
    elif args.action == "replay":
        capture.replay(args.demo, video_only=args.video_only)
    elif args.action in ("seed", "opening", "flyout"):
        getattr(capture, args.action)()
    else:
        capture.automate(json.loads(args.script.read_text()), args.setup, args.script.stem)


if __name__ == "__main__":
    main()
