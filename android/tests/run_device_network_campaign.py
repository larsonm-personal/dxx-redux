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
        base = ["-Game", game] + (["-MissionFile", mission] if mission else [])
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
            "client-rewind": ["-ClientRewind"],
        }
        if content != "d1d2":
            scenarios["host-migration"] = ["-HostMigration"]
        if content != "d2":
            scenarios["level-transition"] = ["-D1LevelTransition"]
        for name, flags in scenarios.items():
            result.append({"id": f"{content}-{name}", "kind": "suite", "args": base + flags})
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
                    "-SecretWorld",
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
    return result


class Device:
    def __init__(self, adb, serial, output):
        self.adb = adb
        self.serial = serial
        self.output = output
        self.snapshot_number = 0
        self.automation_number = 0
        self.fault_number = 0

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
            args += [flag, key, str(value).lower() if isinstance(value, bool) else str(value)]
        return self.shell(*args)

    def mp(self, command, **extras):
        return self.broadcast("MP_COMMAND", command=command, **extras)

    def snapshot(self, setup=False, lobby=False):
        name = "mp_introspect.json" if lobby else "setup_introspect.json" if setup else "introspect.json"
        self.shell("run-as", PACKAGE, "rm", "-f", f"files/{name}")
        if lobby:
            self.mp("introspect")
        else:
            self.broadcast("SETUP_INTROSPECT" if setup else "INTROSPECT", **({"lightweight": True} if setup else {}))
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
        self.wake()
        self.shell("am", "force-stop", PACKAGE)
        self.shell("am", "start", "-n", f"{PACKAGE}/{ACTIVITY}")
        wait_for("launcher ready", lambda: self.snapshot(setup=True), 40)

    def wake(self):
        self.shell("input", "keyevent", "KEYCODE_WAKEUP")
        time.sleep(0.7)
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

    def automate(self, steps, wait=True):
        self.automation_number += 1
        name = f"campaign-{self.serial}-{self.automation_number}.jsonc"
        path = self.output / name
        if isinstance(steps, Path):
            shutil.copyfile(steps, path)
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

    def launch(self, case, host=True, address=None):
        args = {
            "game": case["game"],
            "mission": case["mission"],
            "mp_mode": "host" if host else "join",
            "mode": "coop",
            "max_players": 2,
            "difficulty": 0,
            "level_num": 1,
            "callsign": "ChaosRP" if host else "ChaosS21",
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
            print(f"{utc()} holding {target.serial} {fault}, {remaining:.1f}s remaining", flush=True)
            time.sleep(min(20, remaining))
    finally:
        if fault == "wifi":
            target.shell("svc", "wifi", "enable")
        if fault in ("home", "screen"):
            target.wake()
            activity = ACTIVITY if launcher else "com.dxxredux.app.MainActivity"
            target.shell("am", "start", "-n", f"{PACKAGE}/{activity}", "-f", "0x20000000")
        if stopped_pid:
            target.shell("run-as", PACKAGE, "kill", "-CONT", stopped_pid, check=False)
        timeline["restore_command_utc"] = utc()
        timeline["actual_seconds"] = round(time.monotonic() - started, 3)
        write_json(target.output / f"{target.serial}-fault-{target.fault_number:03d}.json", timeline)
    if fault == "wifi":
        wait_for(
            "Wi-Fi reconnect", lambda: "inet " in target.shell("ip", "-4", "addr", "show", "wlan0", check=False), 40
        )
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
        print(f"{utc()} reconnect cycle {cycle}, outage={duration}s", flush=True)
        timed_fault({"fault": "kill", "duration": duration}, client)
        client.start_setup()
        client.launch(case, host=False, address=host.ip())
        verify_traffic(devices)
        states = pair_states(devices)
        for state in states:
            connected = [player for player in state["multiplayer"]["players"] if player["connected"]]
            if len(connected) != 2 or len({player["callsign"] for player in connected}) != 2:
                raise RuntimeError("Reconnect left duplicate pilots or ghost slots")
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


def lobby_bidirectional(host, client, label):
    for ready in (True, False, True):
        client.mp("lan_set_ready", ready=ready)
        wait_for(
            "host receives ready change",
            lambda: any(
                player["callsign"] == "ChaosS21" and player["ready"] == ready for player in lobby_state(host)["players"]
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


def run_suite(case, devices, directory):
    env = dict(os.environ, DXX_TEST_PACKAGE=PACKAGE)
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
            process.wait(timeout=900)
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
    write_json(args.output / "manifest.json", {"utc": utc(), "serials": serials, "package": PACKAGE, "cases": selected})
    results = []
    for iteration in range(1, args.repeat + 1):
        for case in selected:
            directory = args.output / f"{iteration:02d}-{case['id']}"
            directory.mkdir(exist_ok=False)
            devices = [Device(args.adb, serial, directory) for serial in serials]
            print(f"{utc()} START {case['id']} iteration={iteration}", flush=True)
            started = time.monotonic()
            result = {"case": case, "iteration": iteration, "started_utc": utc(), "status": "FAIL"}
            loggers = []
            with contextlib.ExitStack() as stack:
                try:
                    for device in devices:
                        if device.call("get-state") != "device":
                            raise RuntimeError(f"Device not online: {device.serial}")
                        device.shell("run-as", PACKAGE, "pwd")
                        device.wake()
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
                    elif case["kind"] == "churn":
                        churn(case, devices)
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
