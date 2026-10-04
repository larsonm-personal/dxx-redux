#!/usr/bin/env python3
"""Paired physical-device LAN campaign with durable per-case evidence.

Provision com.dxxredux.app.nsdtest and owned game data first. The normal app is
never reset. Use --list to review cases, then --case patterns to run a subset.
All timestamps in artifacts are UTC; all durations use a monotonic clock.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import contextlib
import datetime as dt
import fnmatch
import json
import os
from pathlib import Path
import re
import shutil
import shlex
import signal
import subprocess
import time
import traceback


ROOT = Path(__file__).resolve().parents[2]
PACKAGE = "com.dxxredux.app.nsdtest"
ACTIVITY = "com.dxxredux.app.SetupActivity"


class DeviceUnavailable(RuntimeError):
    """A device needs user attention before a gameplay case can run."""


def utc():
    return dt.datetime.now(dt.timezone.utc).isoformat()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def cases():
    result = []
    for content, game, mission in (("d1", "d1", ""), ("d2", "d2", "d2"), ("d1d2", "d2", "descent")):
        base = ["-Game", game] + (["-MissionFile", mission] if content == "d1d2" else [])
        scenarios = {
            "baseline": [],
            "briefing-host-loss": ["-Briefings", "-BriefingFailure", "host"],
            "briefing-client-loss": ["-Briefings", "-BriefingFailure", "client"],
            "briefing-release-host-loss": ["-Briefings", "-BriefingFailure", "host", "-BriefingFailureRelease"],
            "briefing-release-client-loss": ["-Briefings", "-BriefingFailure", "client", "-BriefingFailureRelease"],
            "briefing-rejoin": ["-Briefings", "-BriefingCase", "rejoin"],
            "first-join-host-loss": ["-Briefings", "-BriefingCase", "first_join", "-BriefingJoinAction", "host_loss"],
            "first-join-cancel": ["-Briefings", "-BriefingCase", "first_join", "-BriefingJoinAction", "cancel"],
            "first-join-late": [
                "-Briefings",
                "-BriefingCase",
                "first_join",
                "-BriefingJoinDelaySeconds",
                "105",
                "-BriefingJoinAction",
                "skip",
            ],
            "transfer-transition": ["-JoinTransitionCase", "transfer"],
            "scores-join": ["-JoinTransitionCase", "scores"],
            "restore-client-loss": ["-RestoreFailure", "client"],
            "restore-host-loss": ["-RestoreFailure", "host"],
            "restore-sync-stall": ["-RestoreFailure", "sync_stalled"],
            "saved-cancel-churn": [
                "-SavedLateJoin",
                "-SavedLateJoinCancelCount",
                "3",
                "-SavedLateJoinRejoinCount",
                "2",
            ],
            "saved-level5-return": ["-SavedLateJoin", "-InitialLevel", "5", "-SavedLateJoinRejoinCount", "2"],
            "saved-status-return": [
                "-SavedLateJoin",
                "-SavedLateJoinRejoinCount",
                "1",
                "-RestoreStatus",
                "-RestoreStatusFunctionalOnly",
            ],
            "saved-partial-cancel-return": [
                "-SavedLateJoin",
                "-SavedLateJoinAbortTransfer",
                "-SavedLateJoinRejoinCount",
                "1",
            ],
            "saved-checkpoint-first-return": ["-SavedLateJoin", "-RestoreStatus", "-RestoreStatusFunctionalOnly"],
            "saved-client-rewind": ["-SavedLateJoin", "-ClientRewind"],
            "client-rewind": ["-ClientRewind"],
        }
        for failure in ("client", "host", "sync_stalled", "load_client", "load_host"):
            scenarios[f"restore-{failure}-resume"] = ["-RestoreFailure", failure, "-RestoreLossResume"]
        if content != "d1d2":
            scenarios["host-migration"] = ["-HostMigration"]
            for failure in ("load_client", "load_host"):
                scenarios[f"restore-{failure}-rehost"] = ["-RestoreFailure", failure, "-RestoreFailureRehost"]
            scenarios["restore-options-rehost"] = [
                "-RestoreFailure",
                "load_client",
                "-RestoreFailureRehost",
                "-RestoreFailureOptions",
            ]
        if content != "d2":
            scenarios["level-transition"] = ["-D1LevelTransition"]
            scenarios["saved-level-transition"] = ["-SavedLateJoin", "-D1LevelTransition"]
        for name, flags in scenarios.items():
            result.append({"id": f"{content}-{name}", "kind": "suite", "args": base + flags})
        if content != "d1d2":
            result.append(
                {"id": f"{content}-loading-background", "kind": "loading-background", "game": game, "mission": mission}
            )
            result.append(
                {
                    "id": f"{content}-client-native-rehost",
                    "kind": "client-native-rehost",
                    "game": game,
                    "mission": mission,
                }
            )
        for action in ("cancel", "stop", "expire"):
            result.append(
                {
                    "id": f"{content}-approval-{action}-retry",
                    "kind": "admission",
                    "game": game,
                    "mission": mission,
                    "action": action,
                    "cycles": 3,
                }
            )
        for ui in ("menu", "automap"):
            result.append(
                {
                    "id": f"{content}-approval-{ui}-expiry",
                    "kind": "admission",
                    "game": game,
                    "mission": mission,
                    "action": "expire",
                    "ui": ui,
                    "cycles": 3,
                }
            )
        for action in ("launcher", "native") if content != "d1d2" else ("launcher",):
            result.append(
                {
                    "id": f"{content}-unavailable-host-{action}-recovery",
                    "kind": "offline-recovery",
                    "game": game,
                    "mission": mission,
                    "action": action,
                }
            )
        if content != "d1d2":
            result.append(
                {
                    "id": f"{content}-native-abort-advertisement",
                    "kind": "native-abort-advertisement",
                    "game": game,
                    "mission": mission,
                }
            )
            result.append(
                {
                    "id": f"{content}-background-deadline",
                    "kind": "dormancy-expiry",
                    "game": game,
                    "mission": mission,
                    "duration": 1200,
                }
            )
        result.append(
            {
                "id": f"{content}-renamed-returns",
                "kind": "renamed-returns",
                "game": game,
                "mission": mission,
                "cycles": 6,
            }
        )
        result.append(
            {
                "id": f"{content}-repeated-host-swaps",
                "kind": "host-swaps",
                "game": game,
                "mission": mission,
                "cycles": 4,
            }
        )
        for role in ("host", "client"):
            if content != "d1d2":
                result.append(
                    {
                        "id": f"{content}-{role}-death-autosave",
                        "kind": "death-autosave",
                        "game": game,
                        "mission": mission,
                        "role": role,
                    }
                )
            for ui in ("menu", "automap"):
                for action in ("soak", "peer-loss"):
                    result.append(
                        {
                            "id": f"{content}-{role}-{ui}-{action}",
                            "kind": "ui-session",
                            "game": game,
                            "mission": mission,
                            "role": role,
                            "ui": ui,
                            "action": action,
                            "duration": 90,
                        }
                    )
        result.append({"id": f"{content}-client-churn", "kind": "churn", "game": game, "mission": mission})
        for role, fault, duration in (
            ("host", "home", 35),
            ("client", "screen", 70),
            ("host", "wifi", 3),
            ("client", "wifi", 12),
            ("client", "stop", 35),
        ):
            result.append(
                {
                    "id": f"{content}-briefing-{role}-{fault}-{duration}",
                    "kind": "briefing-lifecycle",
                    "game": game,
                    "mission": mission,
                    "role": role,
                    "fault": fault,
                    "duration": duration,
                    "briefings": True,
                }
            )
        for role in ("host", "client"):
            for fault, durations in (
                ("home", (3, 35)),
                ("screen", (5, 70)),
                ("wifi", (3, 12, 35)),
                ("stop", (3, 12, 35)),
                ("native-kill", (1,)),
                ("leave", (1,)),
            ):
                for duration in durations:
                    result.append(
                        {
                            "id": f"{content}-{role}-{fault}-{duration}",
                            "kind": "lifecycle",
                            "game": game,
                            "mission": mission,
                            "role": role,
                            "fault": fault,
                            "duration": duration,
                        }
                    )
            for duration in (1, 8, 35):
                result.append(
                    {
                        "id": f"{content}-{role}-kill-rejoin-{duration}",
                        "kind": "lifecycle",
                        "game": game,
                        "mission": mission,
                        "role": role,
                        "fault": "kill",
                        "duration": duration,
                    }
                )
    for phase in ("freezing", "capturing", "loading", "committed", "release"):
        result.append(
            {
                "id": f"d2-secret-loss-{phase}",
                "kind": "suite",
                "args": [
                    "-Game",
                    "d2",
                    "-InitialLevel",
                    "8",
                    "-AllowSecretWarps",
                    "-NoCoopQol",
                    "-SecretDisconnectPhase",
                    phase,
                ],
            }
        )
    for role in ("host", "client"):
        for fault, duration in (("screen", 75), ("home", 35), ("wifi", 12), ("wifi", 145), ("kill", 15)):
            result.append(
                {
                    "id": f"lobby-{role}-{fault}-{duration}",
                    "kind": "lobby",
                    "role": role,
                    "fault": fault,
                    "duration": duration,
                }
            )
    for action in ("replace", "rename", "rename-after-loss", "launch-cancel", "repeat-join"):
        result.append({"id": f"lobby-sequence-{action}", "kind": "lobby-sequence", "action": action})
    result.append({"id": "lobby-client-background-rejoin", "kind": "background-rejoin"})
    result.append({"id": "lobby-role-turnover", "kind": "lobby-role-turnover", "cycles": 24})
    result.append(
        {
            "id": "d2-background-menu-deadline",
            "kind": "dormancy-expiry",
            "game": "d2",
            "mission": "d2",
            "duration": 1200,
            "ui": "menu",
        }
    )
    result.append(
        {
            "id": "d1-background-menu-deadline",
            "kind": "dormancy-expiry",
            "game": "d1",
            "mission": "",
            "duration": 1200,
            "ui": "menu",
        }
    )
    result.append(
        {
            "id": "d2-background-deadline-reset",
            "kind": "dormancy-reset",
            "game": "d2",
            "mission": "d2",
            "duration": 1190,
        }
    )
    for role in ("host", "client"):
        result.append(
            {
                "id": f"lobby-{role}-route-retry",
                "kind": "lobby-route-retry",
                "role": role,
                "fault": "wifi",
                "duration": 145,
            }
        )
    return result


class Device:
    def __init__(self, adb, serial, output, background_lobby=False):
        self.adb = adb
        self.serial = serial
        self.output = output
        self.snapshot_number = 0
        self.automation_number = 0
        self.fault_number = 0
        self.background_lobby = background_lobby

    def call(self, *args, timeout=20, check=True):
        started = time.monotonic()
        proc = subprocess.run(
            [self.adb, "-s", self.serial, *map(str, args)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
            check=False,
        )
        with (self.output / f"{self.serial}-commands.jsonl").open("a", encoding="utf-8") as log:
            log.write(
                json.dumps(
                    {
                        "utc": utc(),
                        "seconds": round(time.monotonic() - started, 3),
                        "args": args,
                        "exit": proc.returncode,
                        "stderr": proc.stderr[-2000:],
                    }
                )
                + "\n"
            )
        if check and proc.returncode:
            raise RuntimeError(f"{self.serial}: {args}: {proc.stderr or proc.stdout}")
        return proc.stdout.strip()

    def shell(self, *args, **kwargs):
        return self.call("shell", *args, **kwargs)

    def broadcast(self, action, **extras):
        args = ["am", "broadcast", "-a", f"com.dxxredux.{action}", "-p", PACKAGE]
        for key, value in extras.items():
            flag = "--ez" if isinstance(value, bool) else "--ei" if isinstance(value, int) else "--es"
            # adb shell joins arguments into a remote command; preserve empty strings and spaces
            encoded = str(value).lower() if isinstance(value, bool) else shlex.quote(str(value))
            args += [flag, key, encoded]
        return self.shell(*args)

    def mp(self, command, **extras):
        return self.broadcast("MP_COMMAND", command=command, **extras)

    def snapshot(self, setup=False, lobby=False, full_setup=False):
        name = "mp_introspect.json" if lobby else "setup_introspect.json" if setup else "introspect.json"
        self.shell("run-as", PACKAGE, "rm", "-f", f"files/{name}")
        if lobby:
            self.mp("introspect")
        else:
            self.broadcast(
                "SETUP_INTROSPECT" if setup else "INTROSPECT",
                **({"lightweight": not full_setup} if setup else {}),
            )
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            raw = self.shell("run-as", PACKAGE, "cat", f"files/{name}", check=False)
            try:
                state = json.loads(raw)
                self.snapshot_number += 1
                write_json(
                    self.output / f"{self.serial}-{self.snapshot_number:05d}-{name}",
                    {"captured_utc": utc(), "state": state},
                )
                return state
            except json.JSONDecodeError:
                time.sleep(0.15)
        return None

    def start_setup(self):
        if not self.background_lobby:
            self.wake()
        self.shell("am", "force-stop", PACKAGE)
        self.shell(
            "am",
            "start",
            "-a",
            "android.intent.action.MAIN",
            "-c",
            "android.intent.category.LAUNCHER",
            "-n",
            f"{PACKAGE}/{ACTIVITY}",
        )
        wait_for("launcher ready", lambda: self.snapshot(setup=True), 40)

    def wake(self):
        self.shell("input", "keyevent", "KEYCODE_WAKEUP")
        time.sleep(0.7)
        policy = self.shell("dumpsys", "window", "policy")
        if re.search(r"\bshowing=true\b", policy) and re.search(r"\bsecure=true\b", policy):
            raise DeviceUnavailable(f"{self.serial}: unlock the device normally before continuing")
        self.shell("wm", "dismiss-keyguard")
        # WAKEUP alone does not refresh user activity when the display is already on
        self.shell("input", "keyevent", "KEYCODE_SHIFT_LEFT")
        for _ in range(5):
            time.sleep(0.3)
            policy = self.shell("dumpsys", "window", "policy")
            if not re.search(r"\bshowing=true\b", policy):
                return
            if re.search(r"\bsecure=true\b", policy):
                raise DeviceUnavailable(f"{self.serial}: unlock the device normally before continuing")
            self.shell("wm", "dismiss-keyguard")
        raise DeviceUnavailable(f"{self.serial}: lock screen did not dismiss")

    def signal_game(self, signal):
        pid = self.shell("pidof", f"{PACKAGE}:game", check=False)
        if not re.fullmatch(r"\d+", pid):
            raise RuntimeError(f"Expected one diagnostic game process, got {pid!r}")
        self.shell("run-as", PACKAGE, "kill", f"-{signal}", pid)
        return pid

    def ip(self):
        text = self.shell("ip", "-4", "addr", "show", "wlan0", check=False)
        match = re.search(r"\binet (\d+\.\d+\.\d+\.\d+)", text)
        if not match:
            raise RuntimeError(f"No Wi-Fi IP for {self.serial}")
        return match.group(1)

    def automate(self, steps, wait=True, game=None):
        self.automation_number += 1
        name = f"campaign-{self.serial}-{self.automation_number}.jsonc"
        path = self.output / name
        if isinstance(steps, Path):
            parsed = subprocess.run(
                [
                    "pwsh",
                    "-NoProfile",
                    "-Command",
                    (
                        ". './android/helpers/test_helpers.ps1'; "
                        "Get-Content -Raw (Resolve-TestScript -ScriptPath $env:DXX_CAMPAIGN_JSONC_FILE "
                        "-GameId $env:DXX_CAMPAIGN_GAME)"
                        if game
                        else ". './android/helpers/jsonc.ps1'; "
                        "ConvertTo-Json -InputObject (Read-JsoncFile -Path $env:DXX_CAMPAIGN_JSONC_FILE) -Depth 30"
                    ),
                ],
                cwd=ROOT,
                env=dict(os.environ, DXX_CAMPAIGN_JSONC_FILE=str(steps), DXX_CAMPAIGN_GAME=game or ""),
                capture_output=True,
                text=True,
                encoding="utf-8",
                timeout=20,
                check=True,
            )
            resolved = json.loads(parsed.stdout)
            if any("when" in step for step in resolved):
                raise ValueError("Conditional fixture requires an explicit game for Resolve-TestScript")
            write_json(path, resolved)
        else:
            write_json(path, steps)
        self.call("push", str(path), f"/data/local/tmp/{name}")
        self.shell("run-as", PACKAGE, "cp", f"/data/local/tmp/{name}", f"files/{name}")
        self.shell("run-as", PACKAGE, "rm", "-f", "files/automation_result.json")
        self.broadcast("AUTOMATE", script=name)
        if wait:

            def result():
                raw = self.shell("run-as", PACKAGE, "cat", "files/automation_result.json", check=False)
                try:
                    obj = json.loads(raw)
                except json.JSONDecodeError:
                    return False
                if obj.get("result") == "FAIL":
                    raise RuntimeError(f"Automation failed: {obj}")
                return obj.get("result") == "PASS"

            wait_for("automation completes", result, 30)
            for filename in ("automation_result.json", "automation_log.jsonl"):
                raw = self.shell("run-as", PACKAGE, "cat", f"files/{filename}", check=False)
                (self.output / f"{self.serial}-{self.automation_number}-{filename}").write_text(
                    raw + "\n", encoding="utf-8"
                )

    def launch(self, case, host=True, address=None, callsign=None):
        args = {
            "game": case["game"],
            "mission": case["mission"],
            "mp_mode": "host" if host else "join",
            "mode": "coop",
            "max_players": 2,
            "difficulty": 0,
            "level_num": 1,
            "callsign": callsign or ("ChaosRP" if host else "ChaosS21"),
            "coop_briefings": case.get("briefings", False),
        }
        if address:
            args["host_addr"] = address
        self.mp("lan_launch", **args)

    def collect(self):
        for file in (
            "automation_result.json",
            "automation_log.jsonl",
            "introspect.json",
            "introspect_ui.json",
            "setup_introspect.json",
            "mp_introspect.json",
        ):
            raw = self.shell("run-as", PACKAGE, "cat", f"files/{file}", check=False)
            (self.output / f"{self.serial}-final-{file}").write_text(raw + "\n", encoding="utf-8")
        for name, args in (
            ("exit-info", ("dumpsys", "activity", "exit-info", PACKAGE)),
            ("battery", ("dumpsys", "battery")),
            ("processes", ("pidof", PACKAGE, f"{PACKAGE}:game")),
            ("lock-policy", ("dumpsys", "window", "policy")),
        ):
            (self.output / f"{self.serial}-{name}.txt").write_text(
                self.shell(*args, check=False) + "\n", encoding="utf-8"
            )


def wait_for(label, predicate, timeout, interval=0.5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        result = predicate()
        if result:
            return result
        time.sleep(interval)
    raise TimeoutError(label)


def pair_states(devices):
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        return list(pool.map(lambda device: device.snapshot(), devices))


def healthy_pair(states):
    return all(
        state
        and state.get("in_game")
        and state.get("is_network")
        and not state.get("time_paused")
        and state.get("multiplayer", {}).get("num_connected") == 2
        for state in states
    ) and (states[0].get("current_level_num") == states[1].get("current_level_num"))


def verify_traffic(devices):
    def joined():
        states = pair_states(devices)
        for device, state in zip(devices, states, strict=True):
            if (
                state
                and state.get("multiplayer", {}).get("i_am_master")
                and state["multiplayer"].get("join_request_pending")
            ):
                device.automate([{"action": "key", "key": "f6", "post_delay_ms": 200}])
        return states if healthy_pair(states) else None

    first = wait_for("healthy two-player gameplay", joined, 65)
    seen = [False, False]

    def advances():
        current = pair_states(devices)
        if not healthy_pair(current):
            return False
        for index in (0, 1):
            slot = current[1 - index]["multiplayer"]["my_player_num"]
            seen[index] |= (
                first[index]["multiplayer"]["last_pdata_received"][slot]
                != current[index]["multiplayer"]["last_pdata_received"][slot]
            )
        if current[0]["multiplayer"]["master_player_num"] != current[1]["multiplayer"]["master_player_num"]:
            raise RuntimeError("Peers disagree about the host")
        if current[0].get("coop_world_visit", {}).get("active") != current[1].get("coop_world_visit", {}).get("active"):
            raise RuntimeError("Peers disagree about the world visit")
        return all(seen)

    wait_for("bidirectional PDATA advances", advances, 15)


def hold_and_observe(devices, seconds):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        pair_states(devices)
        time.sleep(min(2, max(0, deadline - time.monotonic())))


def timed_fault(case, target, launcher=False):
    """Keep ADB introspection latency from extending the requested outage."""
    fault = case["fault"]
    started = time.monotonic()
    timeline = {"requested_seconds": case["duration"], "start_utc": utc(), "fault": fault}
    keep_lobby_visible = launcher and fault == "wifi" and not target.background_lobby
    timeline["screen_activity_during_outage"] = keep_lobby_visible
    stopped_pid = None
    target.fault_number += 1
    if fault == "wifi":
        timeline["ip_before"] = target.ip()
    try:
        if fault == "wifi":
            target.shell("svc", "wifi", "disable")
            wait_for(
                "Wi-Fi address removed",
                lambda: "inet " not in target.shell("ip", "-4", "addr", "show", "wlan0", check=False),
                15,
            )
        elif fault == "home":
            target.shell("input", "keyevent", "KEYCODE_HOME")
        elif fault == "screen":
            target.shell("input", "keyevent", "KEYCODE_SLEEP")
        elif fault == "kill":
            target.shell("am", "force-stop", PACKAGE)
        elif fault == "native-kill":
            target.signal_game("KILL")
        elif fault == "stop":
            stopped_pid = target.signal_game("STOP")
        elif fault == "leave":
            target.automate([{"action": "enter_launcher"}], wait=False)
        deadline = time.monotonic() + case["duration"]
        timeline["injection_command_seconds"] = round(time.monotonic() - started, 3)
        while (remaining := deadline - time.monotonic()) > 0:
            if keep_lobby_visible:
                # Isolate route loss from a second, unintended screen-timeout fault
                target.shell("input", "keyevent", "KEYCODE_SHIFT_LEFT")
            print(f"{utc()} holding {target.serial} {fault}, {remaining:.1f}s remaining", flush=True)
            time.sleep(min(20, remaining))
    finally:
        if fault == "wifi":
            # Idle Samsung firmware can defer reassociation until the display wakes
            # WAKEUP preserves the secure keyguard and does not return to an app
            target.shell("input", "keyevent", "KEYCODE_WAKEUP")
            target.shell("svc", "wifi", "enable")
        if fault in ("home", "screen"):
            target.wake()
            target.shell("monkey", "-p", PACKAGE, "-c", "android.intent.category.LAUNCHER", "1")
            if not launcher:
                time.sleep(1)
                target.shell("input", "keyevent", "KEYCODE_BACK")
                time.sleep(2)
        if stopped_pid:
            target.shell("run-as", PACKAGE, "kill", "-CONT", stopped_pid, check=False)
        timeline["restore_command_utc"] = utc()
        timeline["actual_seconds"] = round(time.monotonic() - started, 3)
        write_json(target.output / f"{target.serial}-fault-{target.fault_number:03d}.json", timeline)
    if fault == "wifi":
        try:
            wait_for(
                "Wi-Fi reconnect", lambda: "inet " in target.shell("ip", "-4", "addr", "show", "wlan0", check=False), 40
            )
        except TimeoutError as exc:
            raise DeviceUnavailable(f"{target.serial}: Wi-Fi did not reconnect after being re-enabled") from exc
        timeline["ip_after"] = target.ip()
        timeline["reassociated_utc"] = utc()
        timeline["outage_through_reassociation_seconds"] = round(time.monotonic() - started, 3)
        write_json(target.output / f"{target.serial}-fault-{target.fault_number:03d}.json", timeline)
    return timeline


def start_pair(case, devices):
    host, client = devices
    for device in devices:
        device.start_setup()
    host.launch(case)
    wait_for(
        "host network lobby",
        lambda: (
            (s and s.get("is_network") and s.get("multiplayer", {}).get("num_connected") == 1)
            if (s := host.snapshot())
            else False
        ),
        60,
    )
    client.launch(case, host=False, address=host.ip())
    verify_traffic(devices)
    verify_controls(devices)


def verify_controls(devices):
    for device in devices:
        before = device.snapshot()
        device.automate(ROOT / "android/game_scripts/test_device_network_controls.jsonc")
        after = device.snapshot()
        if not before or not after or not before.get("position") or not after.get("position"):
            raise RuntimeError("No flight state for control verification")
        if sum(abs(after["position"][key] - before["position"][key]) for key in ("fvec_x", "fvec_y", "fvec_z")) < 0.01:
            raise RuntimeError("Flight input was consumed but ship orientation did not change")


def lifecycle(case, devices):
    start_pair(case, devices)
    host, client = devices
    target = host if case["role"] == "host" else client
    peer = client if target is host else host
    fault = case["fault"]
    write_json(target.output / "fault-start.json", {"utc": utc(), "case": case, "states": pair_states(devices)})
    timing = timed_fault(case, target)
    elapsed_outage = timing.get("outage_through_reassociation_seconds", case["duration"])
    needs_rejoin = fault in ("kill", "native-kill", "leave") or fault in ("wifi", "stop") and elapsed_outage > 15
    if needs_rejoin:
        if target is host or elapsed_outage > 15:
            survivor = wait_for(
                "survivor becomes solo host",
                lambda: (
                    (
                        s
                        if s
                        and s.get("multiplayer", {}).get("num_connected") == 1
                        and s["multiplayer"].get("i_am_master")
                        else None
                    )
                    if (s := peer.snapshot())
                    else None
                ),
                50,
            )
            write_json(target.output / "surviving-host.json", survivor)
        target.start_setup()
        target.mp("lan_discover", callsign="ChaosRP" if target is host else "ChaosS21")
        target.mp("lan_join_ip", host_addr=peer.ip())
    verify_traffic(devices)
    hold_and_observe(devices, 8)
    verify_traffic(devices)
    verify_controls(devices)


def churn(case, devices):
    start_pair(case, devices)
    host, client = devices
    # Cross both sides of the 15-second timeout and exceed the eight native slots
    for cycle, duration in enumerate((0.25, 1, 3, 8, 16, 0.25, 8, 1, 16, 3), start=1):
        join_path = "launcher IP discovery" if cycle % 2 else "direct game join"
        print(f"{utc()} reconnect cycle {cycle}, outage={duration}s, path={join_path}", flush=True)
        timed_fault({"fault": "kill", "duration": duration}, client)
        client.start_setup()
        if cycle % 2:
            client.mp("lan_discover", callsign="ChaosS21")
            client.mp("lan_join_ip", host_addr=host.ip())
        else:
            client.launch(case, host=False, address=host.ip())
        verify_traffic(devices)
        states = pair_states(devices)
        for state in states:
            connected = [player for player in state["multiplayer"]["players"] if player["connected"]]
            if len(connected) != 2 or len({player["callsign"] for player in connected}) != 2:
                raise RuntimeError("Reconnect left duplicate pilots or ghost slots")
        verify_controls(devices)


def solo_host(device, timeout=45):
    def ready():
        state = device.snapshot()
        mp = state.get("multiplayer", {}) if state else {}
        return state if mp.get("num_connected") == 1 and mp.get("i_am_master") else None

    return wait_for("survivor is the sole host", ready, timeout)


def join_from_launcher(client, host, callsign):
    client.start_setup()
    client.mp("lan_discover", callsign=callsign)
    client.mp("lan_join_ip", host_addr=host.ip())


def exit_to_launcher(device):
    device.automate([{"action": "enter_launcher"}], wait=False)
    wait_for(
        "native process exits to launcher",
        lambda: not device.shell("pidof", f"{PACKAGE}:game", check=False),
        25,
    )
    state = wait_for("launcher returns", lambda: device.snapshot(setup=True, full_setup=True), 25)
    if state.get("game_running") is not False or state.get("has_returnable_game_activity") is not False:
        raise RuntimeError("Exited game still marked as returnable")


def offline_recovery(case, devices):
    device, absent_host = devices
    absent_host.shell("am", "force-stop", PACKAGE)
    device.start_setup()
    launcher_pid = device.shell("pidof", PACKAGE)
    device.launch(case, host=False, address=absent_host.ip())
    native_pid = wait_for("native join starts", lambda: device.shell("pidof", f"{PACKAGE}:game", check=False), 30)
    # The initial host-info poll has a fixed 30-second deadline and blocks native introspection
    time.sleep(35)
    state = device.snapshot()
    if not state or state.get("in_game") or state.get("menu", {}).get("type") != "newmenu":
        raise RuntimeError("Unavailable host did not return to a usable menu")
    if case["action"] == "native":
        device.automate(ROOT / "android/game_scripts/test_device_network_native_host.jsonc", game=case["game"])
        assert_process(device, native_pid)
    else:
        exit_to_launcher(device)
        device.launch(case)
        solo_host(device)
        device.automate(ROOT / "android/game_scripts/test_coop_late_join_start.jsonc")
        if device.shell("pidof", f"{PACKAGE}:game") == native_pid:
            raise RuntimeError("A replacement game reused native globals")
    state = solo_host(device)
    if not state.get("in_game") or not state.get("is_network"):
        raise RuntimeError("Fresh host did not enter network gameplay")
    exit_to_launcher(device)
    if device.shell("pidof", PACKAGE) != launcher_pid:
        raise RuntimeError("Failed-join recovery restarted the launcher process")


def assert_process(device, expected):
    actual = device.shell("pidof", f"{PACKAGE}:game", check=False)
    if actual != expected:
        raise RuntimeError(f"Surviving engine was replaced: {device.serial} {expected} -> {actual}")


def admission(case, devices):
    host, client = devices
    for device in devices:
        device.start_setup()
    host.launch(case)
    solo_host(host)
    host.automate(ROOT / "android/game_scripts/test_coop_late_join_start.jsonc")
    host_pid = host.shell("pidof", f"{PACKAGE}:game")
    if case.get("ui"):
        host.automate(ROOT / f"android/game_scripts/test_device_network_{case['ui']}_open.jsonc")
    rounds = []
    for cycle in range(case["cycles"]):
        callsign = f"Wait{cycle + 1}"
        client.start_setup()
        client.launch(case, host=False, address=host.ip(), callsign=callsign)

        def requested():
            state = host.snapshot()
            mp = state.get("multiplayer", {}) if state else {}
            return state if mp.get("join_request_pending") and mp.get("join_request_callsign") == callsign else None

        requested_state = wait_for(f"fresh approval for {callsign}", requested, 35)
        write_json(host.output / f"approval-{cycle + 1}-pending.json", requested_state)
        started = time.monotonic()
        if case["action"] == "cancel":
            client.automate(ROOT / "android/game_scripts/test_device_network_cancel_join.jsonc")
        elif case["action"] == "stop":
            client.shell("am", "force-stop", PACKAGE)

        def retired():
            state = host.snapshot()
            return state if state and not state["multiplayer"]["join_request_pending"] else None

        retired_state = wait_for("abandoned approval retires", retired, 18)
        if retired_state["multiplayer"]["num_connected"] != 1:
            raise RuntimeError("Unapproved client occupied a live slot")
        if case["action"] == "expire":
            wait_for(
                "ignored join returns from pending admission",
                lambda: (s and not s.get("join_wait", {}).get("active")) if (s := client.snapshot()) else False,
                20,
            )
        assert_process(host, host_pid)
        rounds.append({"callsign": callsign, "action": case["action"], "retired_seconds": time.monotonic() - started})
        write_json(host.output / "approval-rounds.json", rounds)
    if case.get("ui"):
        host.automate(ROOT / "android/game_scripts/test_device_network_ui_close.jsonc")
    join_from_launcher(client, host, "ReturnOK")
    verify_traffic(devices)
    verify_controls(devices)
    assert_process(host, host_pid)
    states = pair_states(devices)
    if any(len(state["multiplayer"]["players"]) > 2 for state in states):
        raise RuntimeError("Cancelled admissions left extra native player slots")


def renamed_returns(case, devices):
    start_pair(case, devices)
    host, client = devices
    host_pid = host.shell("pidof", f"{PACKAGE}:game")
    for cycle in range(case["cycles"]):
        callsign = ("R2JOIN" if cycle % 2 else "R2Join") + str(cycle + 1)
        print(f"{utc()} returning under pilot name {callsign}", flush=True)
        client.shell("am", "force-stop", PACKAGE)
        solo_host(host)
        join_from_launcher(client, host, callsign)
        verify_traffic(devices)
        verify_controls(devices)
        assert_process(host, host_pid)
        for state in pair_states(devices):
            active = [p for p in state["multiplayer"]["players"] if p["connected"]]
            if sorted(p["callsign"].lower() for p in active) != sorted(("chaosrp", callsign.lower())):
                raise RuntimeError("Renamed return displaced or duplicated the wrong pilot")


def client_native_rehost(case, devices):
    start_pair(case, devices)
    original_host, former_client = devices
    native_pid = former_client.shell("pidof", f"{PACKAGE}:game")
    former_client.automate(ROOT / "android/game_scripts/test_device_network_abort_game.jsonc")
    solo_host(original_host)
    former_client.automate(ROOT / "android/game_scripts/test_device_network_native_host.jsonc", game=case["game"])
    assert_process(former_client, native_pid)
    join_from_launcher(original_host, former_client, "ReturnHost")
    verify_traffic(devices)
    verify_controls(devices)
    assert_process(former_client, native_pid)
    write_json(former_client.output / "ordinary-client-rehost.json", pair_states(devices))


def loading_background(case, devices):
    start_pair(case, devices)
    failures = []
    for device in devices:
        try:
            device.automate(ROOT / "android/game_scripts/test_device_loading_background.jsonc")
        except RuntimeError as exc:
            failures.append(f"{device.serial}: {exc}")
    if failures:
        raise RuntimeError("; ".join(failures))
    verify_traffic(devices)
    verify_controls(devices)


def host_swaps(case, devices):
    start_pair(case, devices)
    # Keep combat deaths from obscuring the migration and advertisement checks
    for device in devices:
        device.automate([{"action": "set_debug", "field": "clear_robots", "value": "true"}])
    host, client = devices
    names = {host.serial: "ChaosRP", client.serial: "ChaosS21"}
    advertisements = []
    for cycle in range(case["cycles"]):
        print(f"{utc()} host swap {cycle + 1}, departing={host.serial}", flush=True)
        survivor_pid = client.shell("pidof", f"{PACKAGE}:game")
        host.shell("am", "force-stop", PACKAGE)
        solo_host(client)
        host.start_setup()
        host.mp("lan_discover", callsign=names[host.serial])
        address = client.ip()

        def migrated_advertisement():
            return next(
                (
                    item
                    for item in lobby_state(host)["discovered"]
                    if item["host_address"] == address
                    and item["status"] == "in_game"
                    and item["host_port"] == 42425
                    and item["game"] == case["game"]
                ),
                None,
            )

        advertisement = wait_for("migrated host advertises its current endpoint", migrated_advertisement, 30)
        advertisements.append({"cycle": cycle + 1, "host": client.serial, "advertisement": advertisement})
        write_json(host.output / "migration-advertisements.json", advertisements)
        host.mp("lan_join_ip", host_addr=address)
        verify_traffic(devices)
        verify_controls(devices)
        assert_process(client, survivor_pid)
        host, client = client, host
    client.shell("am", "force-stop", PACKAGE)
    solo_host(host)
    client.start_setup()
    client.mp("lan_discover", callsign="Observer")
    address = host.ip()
    wait_for(
        "final migrated host is visible before abort",
        lambda: any(item["host_address"] == address for item in lobby_state(client)["discovered"]),
        30,
    )
    host_pid = host.shell("pidof", f"{PACKAGE}:game")
    host.automate(ROOT / "android/game_scripts/test_device_network_abort_game.jsonc")
    wait_for(
        "aborted migrated host advertisement retires",
        lambda: not any(item["host_address"] == address for item in lobby_state(client)["discovered"]),
        40,
    )
    assert_process(host, host_pid)
    write_json(host.output / "migration-abort-observer.json", lobby_state(client))


def death_autosave(case, devices):
    start_pair(case, devices)
    host, client = devices
    for device in devices:
        device.automate([{"action": "set_debug", "field": "clear_robots", "value": "true"}])
    target = host if case["role"] == "host" else client
    native_pid = target.shell("pidof", f"{PACKAGE}:game")
    target.automate(ROOT / "android/game_scripts/test_device_network_death_wait.jsonc")
    hold_and_observe(devices, 45)
    states = pair_states(devices)
    write_json(target.output / "death-after-autosave-interval.json", states)
    if any(not state or state.get("time_paused") for state in states):
        raise RuntimeError("Death through autosave interval left a paused or missing engine")
    verify_traffic(devices)
    peer = client if target is host else host
    peer.shell("am", "force-stop", PACKAGE)
    solo_host(target)
    join_from_launcher(peer, target, "ChaosS21" if peer is client else "ChaosRP")
    verify_traffic(devices)
    write_json(target.output / "death-host-admitted-returning-peer.json", pair_states(devices))
    target.automate(ROOT / "android/game_scripts/test_device_network_respawn.jsonc")
    verify_traffic(devices)
    verify_controls(devices)
    assert_process(target, native_pid)


def ui_session(case, devices):
    start_pair(case, devices)
    original_host, original_client = devices
    owner = original_host if case["role"] == "host" else original_client
    peer = original_client if owner is original_host else original_host
    owner_pid = owner.shell("pidof", f"{PACKAGE}:game")
    fixture = (
        "test_device_network_menu_open.jsonc" if case["ui"] == "menu" else "test_device_network_automap_open.jsonc"
    )
    owner.automate(ROOT / "android/game_scripts" / fixture)
    write_json(owner.output / "ui-open.json", pair_states(devices))
    if case["action"] == "peer-loss":
        peer.shell("am", "force-stop", PACKAGE)
        write_json(owner.output / "ui-survivor.json", solo_host(owner))
        deadline = time.monotonic() + 15
    else:
        deadline = time.monotonic() + case["duration"]
    while time.monotonic() < deadline:
        state = owner.snapshot()
        if not state or not state.get("is_network"):
            raise RuntimeError("Menu owner lost its network session")
        if case["ui"] == "automap" and not state.get("automap_active"):
            raise RuntimeError("Automap closed unexpectedly")
        if case["ui"] == "menu" and state.get("game_window_is_front"):
            raise RuntimeError("Game menu closed unexpectedly")
        if case["action"] == "soak":
            peer_state = peer.snapshot()
            if (
                state["multiplayer"]["num_connected"] != 2
                or not peer_state
                or peer_state.get("multiplayer", {}).get("num_connected") != 2
            ):
                raise RuntimeError("A healthy peer was dropped while a menu was open")
        time.sleep(min(3, max(0, deadline - time.monotonic())))
    owner.automate(ROOT / "android/game_scripts/test_device_network_ui_close.jsonc")
    assert_process(owner, owner_pid)
    if case["action"] == "peer-loss":
        join_from_launcher(peer, owner, "ChaosS21" if peer is original_client else "ChaosRP")
    verify_traffic(devices)
    verify_controls(devices)


def briefing_lifecycle(case, devices):
    host, client = devices
    for device in devices:
        device.start_setup()
    host.launch(case)
    wait_for("host network lobby", lambda: (s and s.get("is_network")) if (s := host.snapshot()) else False, 60)
    client.launch(case, host=False, address=host.ip())

    def reading():
        states = pair_states(devices)
        return (
            states
            if all(
                state and state.get("coop_briefing", {}).get("phase") == 6 and state["coop_briefing"].get("presenting")
                for state in states
            )
            else None
        )

    before = wait_for("both players reading the briefing", reading, 70)
    write_json(host.output / "briefing-before-fault.json", before)
    generation = before[0]["coop_briefing"]["generation"]
    target = host if case["role"] == "host" else client
    timing = timed_fault(case, target)
    host_after = host.snapshot()
    if not host_after or host_after["coop_briefing"]["generation"] != generation:
        raise RuntimeError("Lifecycle interruption changed the host briefing generation")
    if host_after["coop_briefing"]["seconds_remaining"] > before[0]["coop_briefing"]["seconds_remaining"]:
        raise RuntimeError("Lifecycle interruption extended the original briefing deadline")
    reconnect = (
        case["fault"] == "stop" or case["fault"] == "wifi" and timing["outage_through_reassociation_seconds"] > 15
    )
    if reconnect:
        if target is host:
            raise RuntimeError("Host-loss briefing cases use the dedicated suite oracle")
        client.start_setup()
        client.launch(case, host=False, address=host.ip())
        wait_for(
            "returning client waits for original briefing",
            lambda: (s and s.get("join_wait", {}).get("active")) if (s := client.snapshot()) else False,
            40,
        )
    else:
        after = wait_for("both original readers recover", reading, 10)
        if any(state["coop_briefing"]["participants"] != 3 for state in after):
            raise RuntimeError("A live backgrounded reader was removed from the briefing")
        client.automate([{"action": "set_debug", "field": "coop_briefing_action", "value": "skip"}])
    host.automate([{"action": "set_debug", "field": "coop_briefing_action", "value": "skip"}])
    verify_traffic(devices)
    verify_controls(devices)


def lobby_state(device):
    state = device.snapshot(lobby=True)
    if not state or "lan" not in state:
        raise RuntimeError("Build lacks LAN introspection or launcher is unresponsive")
    return state["lan"]


def lobby_join(host, client):
    client.mp("lan_discover", callsign="ChaosS21")
    client.mp("lan_join_ip", host_addr=host.ip())
    wait_for("client joined lobby", lambda: lobby_state(client).get("joined_lobby_id"), 40)
    wait_for(
        "host sees two connected players",
        lambda: len(p := lobby_state(host)["players"]) == 2 and all(player["connected"] for player in p),
        20,
    )


def lobby_bidirectional(host, client, label, callsign="ChaosS21"):
    for ready in (True, False, True):
        client.mp("lan_set_ready", ready=ready)
        wait_for(
            "host receives ready change",
            lambda: any(
                player["callsign"] == callsign and player["ready"] == ready for player in lobby_state(host)["players"]
            ),
            15,
        )
    # These messages are between the two local test pilots, with no external service
    for sender, receiver, suffix in ((host, client, "host"), (client, host, "client")):
        marker = f"campaign-{label}-{suffix}-{time.monotonic_ns()}"
        sender.mp("lan_send_chat", text=marker)
        wait_for(
            "directed chat delivered",
            lambda: any(message["text"] == marker for message in lobby_state(receiver)["chat"]),
            15,
        )


def lobby_case(case, devices):
    host, client = devices
    for device in devices:
        device.start_setup()
    host.mp("lan_host_lobby", callsign="ChaosRP", game="d2", mission="d2", mode="coop", max_players=2)
    wait_for("LAN host created", lambda: lobby_state(host)["hosting"], 15)
    lobby_join(host, client)
    lobby_bidirectional(host, client, "before")
    before_id = lobby_state(client)["joined_lobby_id"]
    target = host if case["role"] == "host" else client
    timed_fault(case, target, launcher=True)
    if case["fault"] == "kill":
        target.start_setup()
        if target is host:
            host.mp("lan_host_lobby", callsign="ChaosRP", game="d2", mission="d2", mode="coop", max_players=2)
            client.mp("lan_stop_lobby")
        lobby_join(host, client)
        if target is host and lobby_state(client)["joined_lobby_id"] == before_id:
            raise RuntimeError("Host restart reused the retired lobby identity")
    elif case["fault"] == "wifi" and case["duration"] > 130:
        state = lobby_state(client)
        write_json(target.output / "after-lobby-expiry.json", state)
        if state["joined_lobby_id"]:
            raise RuntimeError("Client retained lobby after peer timeout and reconnect grace")
        lobby_join(host, client)
    wait_for(
        "lobby returns to two connected players",
        lambda: len(p := lobby_state(host)["players"]) == 2 and all(player["connected"] for player in p),
        35,
    )
    lobby_bidirectional(host, client, "after")


def lobby_role_turnover(case, devices):
    for device in devices:
        device.start_setup()
    pids = {device.serial: device.shell("pidof", PACKAGE) for device in devices}
    host, client = devices
    seen_ids = set()
    rounds = []
    content = (("d1", ""), ("d2", "d2"), ("d2", "descent"))
    for cycle in range(case["cycles"]):
        game, mission = content[cycle % len(content)]
        print(f"{utc()} lobby turnover={cycle + 1} host={host.serial} game={game}/{mission}", flush=True)
        host.mp("lan_host_lobby", callsign="ChaosRP", game=game, mission=mission, mode="coop", max_players=2)
        wait_for("turnover host exists", lambda: lobby_state(host)["hosting"], 15)
        lobby_join(host, client)
        lobby_id = lobby_state(client)["joined_lobby_id"]
        if lobby_id in seen_ids:
            raise RuntimeError("A fresh lobby reused a retired identity")
        seen_ids.add(lobby_id)
        lobby_bidirectional(host, client, f"turnover-{cycle + 1}")
        # Vary whether the guest leaves before or after the host closes the room
        if cycle % 2:
            host.mp("lan_stop_lobby")
            client.mp("lan_leave_lobby")
        else:
            client.mp("lan_leave_lobby")
            host.mp("lan_stop_lobby")
        client.mp("lan_stop_lobby")
        for device in devices:
            if device.shell("pidof", PACKAGE) != pids[device.serial]:
                raise RuntimeError("Lobby turnover replaced an app process")
            state = lobby_state(device)
            if state["hosting"] or state["joined_lobby_id"] or state["launch_pending"] or state["players"]:
                raise RuntimeError("Closed lobby retained membership or launch state")
        rounds.append(
            {
                "utc": utc(),
                "cycle": cycle + 1,
                "host": host.serial,
                "game": game,
                "mission": mission,
                "lobby_id": lobby_id,
            }
        )
        write_json(host.output / "lobby-turnover-rounds.json", rounds)
        host, client = client, host


def lobby_route_retry(case, devices):
    host, client = devices
    for device in devices:
        device.start_setup()
    host.mp("lan_host_lobby", callsign="ChaosRP", game="d2", mission="d2", mode="coop", max_players=2)
    wait_for("LAN host created", lambda: lobby_state(host)["hosting"], 15)
    lobby_join(host, client)
    lobby_bidirectional(host, client, "before-route-loss")
    pids = [device.shell("pidof", PACKAGE) for device in devices]
    timed_fault(case, host if case["role"] == "host" else client, launcher=True)
    if lobby_state(client)["joined_lobby_id"]:
        raise RuntimeError("Client retained lobby after full reconnect grace")
    observations = []
    joined = False
    for attempt in range(1, 4):
        client.mp("lan_discover", callsign="ChaosS21")
        client.mp("lan_join_ip", host_addr=host.ip())
        deadline = time.monotonic() + 40
        while time.monotonic() < deadline:
            state = lobby_state(client)
            observation = {"utc": utc(), "attempt": attempt, "client_lobby": state}
            for sender, receiver, label in ((host, client, "host_to_client"), (client, host, "client_to_host")):
                observation[label] = sender.shell("ping", "-c", "1", "-W", "1", receiver.ip(), check=False)
            observations.append(observation)
            write_json(host.output / "route-retry-observations.json", observations)
            print(f"{utc()} route retry={attempt} joined={bool(state['joined_lobby_id'])}", flush=True)
            if state["joined_lobby_id"]:
                joined = True
                break
            time.sleep(4)
        if joined:
            break
        time.sleep(10)
    if not joined:
        raise RuntimeError("Three ordinary Join attempts failed after Wi-Fi restoration")
    for device, pid in zip(devices, pids, strict=True):
        if device.shell("pidof", PACKAGE) != pid:
            raise RuntimeError("Launcher process changed during route recovery")
    wait_for(
        "host sees recovered client",
        lambda: len(p := lobby_state(host)["players"]) == 2 and all(player["connected"] for player in p),
        20,
    )
    lobby_bidirectional(host, client, "after-route-retry")


def lobby_sequence(case, devices, launch=True):
    host, client = devices
    for device in devices:
        device.start_setup()
    host.mp("lan_host_lobby", callsign="ChaosRP", game="d2", mission="d2", mode="coop", max_players=2)
    wait_for("host lobby exists", lambda: lobby_state(host)["hosting"], 15)
    lobby_join(host, client)
    lobby_bidirectional(host, client, "before")
    old_id = lobby_state(client)["joined_lobby_id"]
    client_pid = client.shell("pidof", PACKAGE)
    callsign = "ChaosS21"
    action = case["action"]
    if action in ("replace", "launch-cancel"):
        if action == "launch-cancel":
            host.mp("lan_start_game", level_num=1, difficulty=0)
            wait_for("client has pending lobby launch", lambda: lobby_state(client)["launch_pending"], 20)
            client.mp("launch_game")
            time.sleep(2)
            if client.shell("pidof", f"{PACKAGE}:game", check=False):
                raise RuntimeError("Client entered engine before host preparation confirmation")
            client.mp("lan_stop_lobby")
            client.mp("lan_discover", callsign=callsign)
        host.mp("lan_stop_lobby")
        host.mp("lan_host_lobby", callsign="ChaosRP", game="d2", mission="d2", mode="coop", max_players=2)
        wait_for("replacement host is listening", lambda: lobby_state(host)["hosting"], 15)
        # Preserve the client's process and, for replace, its old joined membership
        client.mp("lan_join_ip", host_addr=host.ip())
        wait_for(
            "client joins replacement lobby identity",
            lambda: (
                (state["joined_lobby_id"] and state["joined_lobby_id"] != old_id)
                if (state := lobby_state(client))
                else False
            ),
            40,
        )
    elif action in ("rename", "rename-after-loss"):
        client.mp("lan_leave_lobby" if action == "rename" else "lan_stop_lobby")
        wait_for(
            "old pilot is no longer connected",
            lambda: sum(p["connected"] for p in lobby_state(host)["players"]) == 1,
            20,
        )
        callsign = "R2New"
        client.mp("lan_discover", callsign=callsign)
        client.mp("lan_join_ip", host_addr=host.ip())
    else:
        for cycle in range(8):
            client.mp("lan_join_ip", host_addr=host.ip())
            time.sleep(0.25 if cycle % 2 else 1)
            state = lobby_state(client)
            if state["joined_lobby_id"] != old_id:
                raise RuntimeError("Repeated join replaced a healthy lobby identity")
    wait_for(
        "lobby has exactly the expected pair",
        lambda: (
            sorted(p["callsign"] for p in lobby_state(host)["players"] if p["connected"])
            == sorted(("ChaosRP", callsign))
        ),
        30,
    )
    if client.shell("pidof", PACKAGE) != client_pid:
        raise RuntimeError("Lobby recovery unexpectedly restarted the client process")
    lobby_bidirectional(host, client, "after", callsign)
    if not launch:
        if action == "launch-cancel":
            state = client.snapshot(setup=True, full_setup=True)
            if not state or state.get("launch_preparation", {}).get("active") or state.get("game_running"):
                raise RuntimeError("Cancelled lobby preparation left a pending or running game")
        return
    host.mp("lan_start_game", level_num=1, difficulty=0)
    wait_for("host has a launch event", lambda: lobby_state(host)["launch_pending"], 20)
    wait_for("client has a launch event", lambda: lobby_state(client)["launch_pending"], 20)
    client.mp("launch_game")
    time.sleep(1)
    host.mp("launch_game")
    verify_traffic(devices)
    verify_controls(devices)


def lobby_background_rejoin(devices):
    host, client = devices
    for device in devices:
        device.start_setup()
    host.mp("lan_host_lobby", callsign="ChaosRP", game="d2", mission="d2", mode="coop", max_players=2)
    wait_for("host lobby exists", lambda: lobby_state(host)["hosting"], 15)
    lobby_join(host, client)
    client_pid = client.shell("pidof", PACKAGE)
    client.mp("lan_leave_lobby")
    wait_for("client leaves old lobby", lambda: len(lobby_state(host)["players"]) == 1, 15)
    client.shell("input", "keyevent", "KEYCODE_HOME")
    # Let Android's foreground-start allowance expire before an artificial queued join
    time.sleep(16)
    client.mp("lan_discover", callsign="ChaosS21")
    client.mp("lan_join_ip", host_addr=host.ip())
    wait_for("background client rejoins", lambda: lobby_state(client).get("joined_lobby_id"), 35)
    if client.shell("pidof", PACKAGE) != client_pid:
        raise RuntimeError("Background lobby action crashed or restarted the launcher")
    before = client.shell("dumpsys", "activity", "services", PACKAGE)
    (client.output / "service-before-resume.txt").write_text(before, encoding="utf-8")
    client.wake()
    client.shell("monkey", "-p", PACKAGE, "-c", "android.intent.category.LAUNCHER", "1")
    wait_for("launcher returns to foreground", lambda: client.snapshot(setup=True), 20)
    client.mp("lan_notify_resumed")

    def service_started():
        state = client.shell("dumpsys", "activity", "services", PACKAGE)
        (client.output / "service-after-resume.txt").write_text(state, encoding="utf-8")
        return "MultiplayerForegroundService" in state and "isForeground=true" in state

    wait_for("LAN foreground service resumes", service_started, 15)
    lobby_bidirectional(host, client, "after-background-rejoin")


def advertised_solo_host(case, devices, reset=True):
    host, observer = devices
    if reset:
        host.start_setup()
        host.broadcast("SETUP_COMMAND", command="write_bool_pref", key="dlog_dormancy_enabled", value=True)
        host.start_setup()
        observer.start_setup()
    host.mp(
        "lan_host_lobby", callsign="ChaosRP", game=case["game"], mission=case["mission"], mode="coop", max_players=2
    )
    wait_for("host lobby exists", lambda: lobby_state(host)["hosting"], 15)
    lobby_join(host, observer)
    lobby_bidirectional(host, observer, "before-native-lifecycle")
    host.mp("lan_start_game", level_num=1, difficulty=0)
    wait_for("host launch is prepared", lambda: lobby_state(host)["launch_pending"], 20)
    # The second pilot leaves before entering the engine; the host continues alone
    observer.mp("lan_leave_lobby")
    observer.mp("lan_stop_lobby")
    host.mp("launch_game")
    solo_host(host)
    host.automate(ROOT / "android/game_scripts/test_coop_late_join_start.jsonc")
    host_pid = host.shell("pidof", f"{PACKAGE}:game")
    # Keep the observer's normal LAN service active instead of relying on idle
    # discovery, which Android may deny network access while the screen is locked
    observer.start_setup()
    observer.mp("lan_host_lobby", callsign="Observer", game="d2", mission="d2", mode="coop", max_players=2)
    wait_for("observer waiting lobby exists", lambda: lobby_state(observer)["hosting"], 15)
    service = observer.shell("dumpsys", "activity", "services", PACKAGE)
    (observer.output / f"{observer.serial}-observer-service-{observer.snapshot_number:05d}.txt").write_text(
        service, encoding="utf-8"
    )
    if "isForeground=true" not in service:
        raise DeviceUnavailable("Observer's normal lobby foreground service did not start")
    address = host.ip()
    wait_for(
        "running host is advertised",
        lambda: any(
            item["host_address"] == address and item["status"] == "in_game"
            for item in lobby_state(observer)["discovered"]
        ),
        30,
    )
    return host_pid, address


def native_abort_advertisement(case, devices):
    host, observer = devices
    host_pid, address = advertised_solo_host(case, devices)
    host.automate(ROOT / "android/game_scripts/test_device_network_abort_game.jsonc")
    observations = []
    deadline = time.monotonic() + 40
    while time.monotonic() < deadline:
        hosted = lobby_state(host)
        advertised = lobby_state(observer)
        observations.append({"utc": utc(), "host": hosted, "observer": advertised, "native": host.snapshot()})
        write_json(host.output / "native-abort-observations.json", observations)
        if not hosted["hosting"] and not any(item["host_address"] == address for item in advertised["discovered"]):
            assert_process(host, host_pid)
            break
        time.sleep(3)
    else:
        raise RuntimeError("Aborted native game remained advertised from its main menu")
    # Rehosting in the surviving native process must acquire a fresh game lease
    host.automate(ROOT / "android/game_scripts/test_device_network_native_host.jsonc", game=case["game"])
    assert_process(host, host_pid)
    service = host.shell("dumpsys", "activity", "services", PACKAGE)
    (host.output / "native-rehost-service.txt").write_text(service, encoding="utf-8")
    if "isForeground=true" not in service:
        raise RuntimeError("Native menu rehosting did not reacquire the foreground service")
    host.shell("input", "keyevent", "KEYCODE_HOME")
    print(f"{utc()} native rehost background soak 90s", flush=True)
    time.sleep(90)
    service = host.shell("dumpsys", "activity", "services", PACKAGE)
    (host.output / "native-rehost-service-background.txt").write_text(service, encoding="utf-8")
    if "isForeground=true" not in service:
        raise RuntimeError("Native menu rehost lost its service while backgrounded")
    host.wake()
    host.shell("monkey", "-p", PACKAGE, "-c", "android.intent.category.LAUNCHER", "1")
    state = host.snapshot()
    write_json(host.output / "native-rehost-after-background.json", state)
    assert_process(host, host_pid)
    if not state or not state.get("in_game") or not state.get("is_network"):
        raise RuntimeError("Native menu rehost did not survive ordinary backgrounding")
    host.automate(ROOT / "android/game_scripts/test_device_network_abort_game.jsonc")


def dormancy_expiry(case, devices):
    host, observer = devices
    host_pid, address = advertised_solo_host(case, devices)
    launcher_pid = host.shell("pidof", PACKAGE)
    if case.get("ui"):
        host.automate(ROOT / f"android/game_scripts/test_device_network_{case['ui']}_open.jsonc")
    host.shell("input", "keyevent", "KEYCODE_HOME")
    started = time.monotonic()
    observations = []
    while time.monotonic() - started < case["duration"] + 20:
        elapsed = time.monotonic() - started
        state = host.snapshot()
        observations.append(
            {
                "utc": utc(),
                "elapsed_seconds": elapsed,
                "state": state,
                "observer": lobby_state(observer),
            }
        )
        write_json(host.output / "background-deadline-observations.json", observations)
        if elapsed < case["duration"] - 10:
            assert_process(host, host_pid)
            if state and not state.get("is_network"):
                raise RuntimeError("Host disconnected before the background deadline")
        print(
            f"{utc()} background deadline elapsed={elapsed:.1f}s network={state.get('is_network') if state else None}",
            flush=True,
        )
        time.sleep(min(20, max(0, case["duration"] + 20 - (time.monotonic() - started))))
    advertised = lobby_state(observer)
    hosted = lobby_state(host)
    write_json(host.output / "advertisement-after-deadline.json", advertised)
    write_json(host.output / "host-lobby-after-deadline.json", hosted)
    host.wake()
    host.shell("monkey", "-p", PACKAGE, "-c", "android.intent.category.LAUNCHER", "1")
    state = host.snapshot()
    if not state:
        host.mp("tap_button", text="Return to Game")
        state = wait_for("native menu resumes after deadline", lambda: host.snapshot(), 20)
    write_json(host.output / "native-after-deadline.json", state)
    assert_process(host, host_pid)
    write_json(
        host.output / "native-process-after-deadline.json",
        {"expected_pid": host_pid, "actual_pid": host.shell("pidof", f"{PACKAGE}:game")},
    )
    if state.get("is_network") or state.get("in_game"):
        raise RuntimeError("Background deadline did not leave cooperative gameplay")
    if not any(
        item.get("text", "").lower().startswith("multiplayer") for item in state.get("menu", {}).get("items", [])
    ):
        raise RuntimeError("Background deadline did not return to the native main menu")
    exit_to_launcher(host)
    advertised_solo_host(case, devices, reset=False)
    if not solo_host(host).get("in_game"):
        raise RuntimeError("Could not host again after background expiry")
    if host.shell("pidof", PACKAGE) != launcher_pid:
        raise RuntimeError("Post-expiry hosting replaced the launcher process")
    if hosted["hosting"] or any(item["host_address"] == address for item in advertised["discovered"]):
        raise RuntimeError("Expired native host remained advertised before returning to the launcher")


def dormancy_reset(case, devices):
    host, observer = devices
    host_pid, _ = advertised_solo_host(case, devices)
    host.shell("input", "keyevent", "KEYCODE_HOME")
    started = time.monotonic()
    wait_for(
        "native Activity acknowledges background before reset test",
        lambda: (
            (s and s.get("android_lifecycle", {}).get("observed_visibility") == "background")
            if (s := host.snapshot())
            else False
        ),
        5,
    )
    observations = []
    while time.monotonic() - started < case["duration"]:
        state = host.snapshot()
        observations.append(
            {
                "utc": utc(),
                "elapsed_seconds": time.monotonic() - started,
                "native": state,
                "observer": lobby_state(observer),
            }
        )
        write_json(host.output / "deadline-reset-observations.json", observations)
        if not state or not state.get("is_network"):
            raise RuntimeError("Host disconnected before foreground reset")
        if state.get("android_lifecycle", {}).get("observed_visibility") != "background":
            raise RuntimeError("Deadline reset precondition lost: native Activity was not backgrounded")
        print(f"{utc()} waiting for foreground reset elapsed={time.monotonic() - started:.1f}s", flush=True)
        time.sleep(min(20, max(0, case["duration"] - (time.monotonic() - started))))
    reset_began = time.monotonic() - started
    host.wake()
    host.shell("monkey", "-p", PACKAGE, "-c", "android.intent.category.LAUNCHER", "1")
    time.sleep(2)
    resumed = host.snapshot()
    write_json(
        host.output / "deadline-reset-resumed.json",
        {"utc": utc(), "reset_began_elapsed_seconds": reset_began, "native": resumed},
    )
    if not resumed or not resumed.get("in_game") or not resumed.get("is_network"):
        raise RuntimeError("Return before timeout failed to preserve the session")
    if resumed.get("android_lifecycle", {}).get("observed_visibility") != "foreground":
        raise RuntimeError("Deadline reset precondition missed: native Activity did not reach foreground")
    assert_process(host, host_pid)
    host.shell("input", "keyevent", "KEYCODE_HOME")
    wait_for(
        "native Activity backgrounds again after reset",
        lambda: (
            (s and s.get("android_lifecycle", {}).get("observed_visibility") == "background")
            if (s := host.snapshot())
            else False
        ),
        5,
    )
    deadline = time.monotonic() + 40
    while time.monotonic() < deadline:
        state = host.snapshot()
        observations.append(
            {
                "utc": utc(),
                "elapsed_seconds": time.monotonic() - started,
                "native": state,
                "observer": lobby_state(observer),
            }
        )
        write_json(host.output / "deadline-reset-observations.json", observations)
        if not state or not state.get("in_game") or not state.get("is_network"):
            raise RuntimeError("Old background deadline disconnected the resumed session")
        assert_process(host, host_pid)
        time.sleep(3)
    host.wake()
    host.shell("monkey", "-p", PACKAGE, "-c", "android.intent.category.LAUNCHER", "1")
    host.automate([{"action": "introspect"}])


def run_suite(case, devices, directory):
    started = time.time()
    env = dict(os.environ, DXX_TEST_PACKAGE=PACKAGE, DXX_CAMPAIGN_EVIDENCE_DIR=str(directory))
    command = [
        "pwsh",
        "-NoProfile",
        "-File",
        str(ROOT / "android/tests/test_lan.ps1"),
        "-HostDevice",
        devices[0].serial,
        "-JoinDevice",
        devices[1].serial,
        "-SkipBuild",
        *case["args"],
    ]
    write_json(directory / "command.json", command)
    with (directory / "runner.log").open("w", encoding="utf-8") as output:
        process = subprocess.Popen(
            command,
            cwd=ROOT,
            env=env,
            stdout=output,
            stderr=subprocess.STDOUT,
            start_new_session=os.name != "nt",
            creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0,
        )
        try:
            deadline = time.monotonic() + 900
            while process.poll() is None:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise subprocess.TimeoutExpired(command, 900)
                try:
                    process.wait(timeout=min(10, remaining))
                except subprocess.TimeoutExpired:
                    # Faulted peers can wait in the launcher for several minutes
                    # Refresh ordinary user activity without changing lock settings
                    for device in devices:
                        if not device.shell("pidof", f"{PACKAGE}:game", check=False):
                            device.shell("input", "keyevent", "KEYCODE_SHIFT_LEFT")
        finally:
            if process.poll() is None:
                if os.name == "nt":
                    subprocess.run(
                        ["taskkill", "/PID", str(process.pid), "/T", "/F"], capture_output=True, timeout=15, check=False
                    )
                else:
                    os.killpg(process.pid, signal.SIGKILL)
                process.wait(timeout=15)
    for name in ("lan_test_log.txt", "lan_emu1_logcat.txt", "lan_emu2_logcat.txt"):
        source = ROOT / "temp" / name
        if source.exists():
            shutil.copy2(source, directory / name)
    for source in (ROOT / "temp").glob("coop-*.json"):
        if source.stat().st_mtime >= started:
            shutil.copy2(source, directory / source.name)
    if process.returncode:
        raise RuntimeError(f"LAN suite exited {process.returncode}; see runner.log")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--host")
    parser.add_argument("--client")
    parser.add_argument("--adb", default=shutil.which("adb") or r"C:\local\android-sdk\platform-tools\adb.exe")
    parser.add_argument("--case", action="append", default=[])
    parser.add_argument("--output", type=Path)
    parser.add_argument("--reverse", action="store_true")
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--not-before", help="ISO UTC design deadline; execution refuses to start earlier")
    parser.add_argument("--stop-on-failure", action="store_true")
    parser.add_argument("--lobby-only", action="store_true", help="Verify lobby recovery without launching gameplay")
    parser.add_argument(
        "--background-lobby-serial",
        help="Skip waking/keyguard dismissal during this device's launcher setup; requires --lobby-only",
    )
    args = parser.parse_args()
    selected = [
        case
        for case in cases()
        if not args.case or any(fnmatch.fnmatchcase(case["id"], pattern) for pattern in args.case)
    ]
    if args.list:
        print(json.dumps(selected, indent=2))
        return 0
    if not args.host or not args.client or args.host == args.client or not args.output:
        parser.error("distinct --host and --client plus --output are required")
    if not selected:
        parser.error("no cases selected")
    if args.lobby_only and any(
        case["kind"] not in ("lobby", "lobby-sequence", "background-rejoin", "lobby-route-retry", "lobby-role-turnover")
        for case in selected
    ):
        parser.error("--lobby-only requires lobby cases")
    if args.background_lobby_serial and (
        not args.lobby_only or args.background_lobby_serial not in (args.host, args.client)
    ):
        parser.error("--background-lobby-serial must select one test device and requires --lobby-only")
    if args.not_before and dt.datetime.now(dt.timezone.utc) < dt.datetime.fromisoformat(args.not_before):
        parser.error("design interval has not ended")
    args.output = args.output.resolve()
    if not args.output.is_relative_to(ROOT / "android/temp"):
        parser.error("output must be under android/temp")
    subprocess.run(
        [
            "pwsh",
            "-NoProfile",
            "-File",
            str(ROOT / "android/helpers/retain-recent-artifacts.ps1"),
            "-Artifacts",
            str(args.output),
        ],
        cwd=ROOT,
        check=True,
    )
    args.output.mkdir(parents=True, exist_ok=True)
    serials = [args.host, args.client]
    if args.reverse:
        serials.reverse()
    if args.background_lobby_serial == serials[1] and any(case["kind"] == "background-rejoin" for case in selected):
        parser.error("background-rejoin needs an unlocked client for its final foreground recovery check")
    write_json(
        args.output / "manifest.json",
        {
            "utc": utc(),
            "serials": serials,
            "package": PACKAGE,
            "cases": selected,
            "lobby_only": args.lobby_only,
            "background_lobby_serial": args.background_lobby_serial,
        },
    )
    results = []
    for iteration in range(1, args.repeat + 1):
        for case in selected:
            directory = args.output / f"{iteration:02d}-{case['id']}"
            directory.mkdir(exist_ok=False)
            devices = [
                Device(args.adb, serial, directory, serial == args.background_lobby_serial) for serial in serials
            ]
            print(f"{utc()} START {case['id']} iteration={iteration}", flush=True)
            started = time.monotonic()
            result = {"case": case, "iteration": iteration, "started_utc": utc(), "status": "FAIL"}
            result["lobby_only"] = args.lobby_only
            loggers = []
            with contextlib.ExitStack() as stack:
                try:
                    for device in devices:
                        if device.call("get-state") != "device":
                            raise RuntimeError(f"Device not online: {device.serial}")
                        device.shell("run-as", PACKAGE, "pwd")
                        if not device.background_lobby:
                            device.wake()
                        (directory / f"{device.serial}-lock-before.txt").write_text(
                            device.shell("dumpsys", "window", "policy"), encoding="utf-8"
                        )
                        log = stack.enter_context(
                            (directory / f"{device.serial}-logcat.txt").open("w", encoding="utf-8")
                        )
                        device.call("logcat", "-c")
                        loggers.append(
                            subprocess.Popen(
                                [args.adb, "-s", device.serial, "logcat", "-v", "threadtime"],
                                stdout=log,
                                stderr=subprocess.STDOUT,
                            )
                        )
                    if case["kind"] == "suite":
                        run_suite(case, devices, directory)
                    elif case["kind"] == "lobby":
                        lobby_case(case, devices)
                    elif case["kind"] == "lobby-route-retry":
                        lobby_route_retry(case, devices)
                    elif case["kind"] == "lobby-role-turnover":
                        lobby_role_turnover(case, devices)
                    elif case["kind"] == "churn":
                        churn(case, devices)
                    elif case["kind"] == "admission":
                        admission(case, devices)
                    elif case["kind"] == "offline-recovery":
                        offline_recovery(case, devices)
                    elif case["kind"] == "renamed-returns":
                        renamed_returns(case, devices)
                    elif case["kind"] == "host-swaps":
                        host_swaps(case, devices)
                    elif case["kind"] == "client-native-rehost":
                        client_native_rehost(case, devices)
                    elif case["kind"] == "loading-background":
                        loading_background(case, devices)
                    elif case["kind"] == "death-autosave":
                        death_autosave(case, devices)
                    elif case["kind"] == "ui-session":
                        ui_session(case, devices)
                    elif case["kind"] == "lobby-sequence":
                        lobby_sequence(case, devices, launch=not args.lobby_only)
                    elif case["kind"] == "background-rejoin":
                        lobby_background_rejoin(devices)
                    elif case["kind"] == "dormancy-expiry":
                        dormancy_expiry(case, devices)
                    elif case["kind"] == "native-abort-advertisement":
                        native_abort_advertisement(case, devices)
                    elif case["kind"] == "dormancy-reset":
                        dormancy_reset(case, devices)
                    elif case["kind"] == "briefing-lifecycle":
                        briefing_lifecycle(case, devices)
                    else:
                        lifecycle(case, devices)
                    result["status"] = "PASS"
                except (Exception, KeyboardInterrupt) as exc:
                    result["error"] = str(exc)
                    result["traceback"] = traceback.format_exc()
                    if isinstance(exc, KeyboardInterrupt):
                        result["status"] = "INTERRUPTED"
                    elif isinstance(exc, DeviceUnavailable):
                        result["status"] = "ENVIRONMENT_BLOCKED"
                finally:
                    for device in devices:
                        try:
                            device.collect()
                            device.shell("am", "force-stop", PACKAGE)
                        except Exception as exc:
                            result.setdefault("cleanup_errors", []).append(str(exc))
                    for logger in loggers:
                        logger.terminate()
                        logger.wait(timeout=10)
            result["seconds"] = round(time.monotonic() - started, 2)
            result["ended_utc"] = utc()
            write_json(directory / "result.json", result)
            results.append(result)
            write_json(args.output / "results.json", results)
            print(f"{utc()} {result['status']} {case['id']} {result['seconds']}s {result.get('error', '')}", flush=True)
            if (
                result["status"] in ("INTERRUPTED", "ENVIRONMENT_BLOCKED")
                or args.stop_on_failure
                and result["status"] != "PASS"
            ):
                return 1
    return 0 if all(result["status"] == "PASS" for result in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
