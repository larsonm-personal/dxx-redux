#!/usr/bin/env python3
"""Bounded real-menu configuration campaigns for the opt-in desktop probe.

Only public data directories explicitly supplied by the caller are used. Each
case owns HOME and its probe/log directory. Settings assertions do not imply
visual or audible correctness; those need their own output oracles.
"""

import argparse
import hashlib
import itertools
import json
import os
from pathlib import Path
import random
import re
import signal
import shutil
import subprocess
import time

KEYS = {
    "enter": 13,
    "escape": 27,
    "space": 32,
    "backspace": 8,
    "up": 273,
    "down": 274,
    "right": 275,
    "left": 276,
    "home": 278,
    "end": 279,
    "tab": 9,
    "print": 316,
    **{f"f{i}": 281 + i for i in range(1, 13)},
}
FATAL = re.compile(
    r"AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|Segmentation fault|Assertion .*failed|Sanitizer:DEADLYSIGNAL"
)


class ProbeFailure(RuntimeError):
    pass


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def freeze_binary(executable, directory):
    source = Path(executable).resolve()
    destination = Path(directory) / source.name
    before = hashlib.sha256(source.read_bytes()).hexdigest()
    shutil.copy2(source, destination)
    digest = hashlib.sha256(destination.read_bytes()).hexdigest()
    after = hashlib.sha256(source.read_bytes()).hexdigest()
    if before != digest or digest != after or not os.access(destination, os.X_OK):
        raise ProbeFailure("executable changed during snapshot; wait for its build to finish")
    write_json(Path(directory) / "binary.json", {"source": str(source), "snapshot": str(destination), "sha256": digest})
    return destination


def normalized(text):
    return " ".join(text.lower().replace("...", "").split())


class HostSession:
    def __init__(self, executable, data, directory, game="d1", timeout=180, config=None):
        self.executable = Path(executable).resolve()
        self.data = Path(data).resolve()
        self.directory = Path(directory).resolve()
        self.game = game
        self.timeout = timeout
        self.directory.mkdir(parents=True, exist_ok=True)
        self.home = self.directory / "home"
        self.profile = self.home / f".{game}x-redux"
        self.profile.mkdir(parents=True, exist_ok=True)
        self.probe = self.directory / "probe"
        self.probe.mkdir(exist_ok=True)
        self.trace = self.directory / "trace.jsonl"
        self.process = None
        self.sequence = 0
        self.launch_number = 0
        self.deadline = time.monotonic() + timeout
        if config is not None:
            (self.profile / "descent.cfg").write_text("".join(f"{k}={v}\n" for k, v in config.items()))

    def record(self, action, **details):
        with self.trace.open("a") as output:
            output.write(json.dumps({"action": action, **details}, sort_keys=True) + "\n")

    def start(self):
        self.launch_number += 1
        self.clean_exit = False
        self.sequence = 0
        for name in ("snapshot.json", "command.txt"):
            (self.probe / name).unlink(missing_ok=True)
        env = dict(os.environ, HOME=str(self.home), DXX_HOST_PROBE_DIR=str(self.probe))
        env.setdefault("SDL_AUDIODRIVER", "dummy")
        env.setdefault("LIBGL_ALWAYS_SOFTWARE", "1")
        self.log_path = self.directory / f"engine-{self.launch_number}.log"
        self.log = self.log_path.open("w")
        args = [
            str(self.executable),
            "-hogdir",
            str(self.data),
            "-window",
            "-notitles",
            "-maxfps",
            "60",
            "-pilot",
            "player",
        ]
        self.record("launch", args=args, home=str(self.home))
        self.process = subprocess.Popen(
            args, cwd=self.directory, env=env, stdout=self.log, stderr=subprocess.STDOUT, start_new_session=True
        )
        state = self.wait(lambda s: s["kind"] in ("menu", "listbox"), "first semantic menu snapshot", timeout=30)
        if "pilot" in normalized(state["subtitle"]) and state["kind"] == "menu":
            self.key("enter")
        state = self.wait(lambda s: self.has_item(s, "options", exact=True), "main menu", timeout=25)
        if state.get("engine") != self.game:
            raise ProbeFailure(f"actual engine {state.get('engine')!r} does not match requested {self.game!r}")
        return self

    def health(self):
        if time.monotonic() >= self.deadline:
            raise ProbeFailure("case deadline exceeded")
        if self.process.poll() is not None:
            raise ProbeFailure(f"unexpected engine exit {self.process.returncode}; see {self.log_path}")
        log = self.log_path.read_text(errors="replace")
        match = FATAL.search(log)
        if match:
            raise ProbeFailure(f"fatal engine diagnostic: {match.group(0)}")

    def snapshot(self):
        self.health()
        try:
            return json.loads((self.probe / "snapshot.json").read_text())
        except (OSError, ValueError):
            return None

    def wait(self, predicate, label, timeout=12):
        until = min(self.deadline, time.monotonic() + timeout)
        state = None
        while time.monotonic() < until:
            state = self.snapshot()
            if state is not None and predicate(state):
                self.record("assert", label=label, snapshot=state)
                return state
            time.sleep(0.03)
        raise ProbeFailure(f"timeout waiting for {label}; last={state}")

    def key(self, name, modifiers=0, hold_ms=0):
        if not 0 <= hold_ms <= 2000:
            raise ValueError("held input must be bounded to 0..2000 milliseconds")
        value = KEYS.get(name, ord(name) if len(name) == 1 else None)
        if value is None:
            raise ValueError(f"unknown SDL key {name}")
        old = self.snapshot()
        self.sequence += 1
        temporary = self.probe / "command.tmp"
        temporary.write_text(f"{self.sequence} {value} {modifiers} {hold_ms}\n")
        temporary.replace(self.probe / "command.txt")
        self.record("key", key=name, modifiers=modifiers, hold_ms=hold_ms, sequence=self.sequence)
        # A frame after acknowledgement excludes a snapshot taken before dispatch
        return self.wait(
            lambda s: s["sequence"] >= self.sequence and s["frame"] > (old or {}).get("frame", 0) + 1,
            f"input {name} acknowledged",
        )

    @staticmethod
    def has_item(state, text, exact=False):
        wanted = normalized(text)
        return any(
            normalized(i["text"]) == wanted if exact else wanted in normalized(i["text"])
            for i in state.get("items", [])
        )

    def select(self, text, exact=False, section=None):
        state = self.wait(lambda s: s["kind"] in ("menu", "listbox") and bool(s["items"]), "selectable menu")
        matches = [
            i
            for i, item in enumerate(state["items"])
            if (normalized(item["text"]) == normalized(text) if exact else normalized(text) in normalized(item["text"]))
        ]
        if section is not None:
            current_section = ""
            scoped = []
            for index, item in enumerate(state["items"]):
                if item.get("type") == 4 and item["text"].strip():
                    current_section = item["text"]
                if index in matches and normalized(section) in normalized(current_section):
                    scoped.append(index)
            matches = scoped
        if len(matches) != 1:
            raise ProbeFailure(f"menu item {text!r} in section {section!r} matched {matches}; items={state['items']}")
        target = matches[0]
        identity = (state["kind"], state["title"], state["subtitle"])
        choices = [i for i, item in enumerate(state["items"]) if item.get("type") != 4]
        if target not in choices:
            raise ProbeFailure(f"item {text!r} is not selectable")
        for _ in range(len(state["items"]) + 2):
            if state["selected"] == target:
                return state
            current = choices.index(state["selected"])
            forward = (choices.index(target) - current) % len(choices)
            backward = (current - choices.index(target)) % len(choices)
            # Native listboxes clamp at either end; ordinary menus wrap
            direction = (
                ("down" if target > state["selected"] else "up")
                if state["kind"] == "listbox"
                else ("down" if forward <= backward else "up")
            )
            self.key(direction)
            state = self.wait(
                lambda s: (s["kind"], s["title"], s["subtitle"]) == identity and bool(s["items"]),
                "same menu after selection input",
            )
        raise ProbeFailure(f"cannot select observed item {text!r}")

    def open_menu(self, text, exact=False):
        before = self.select(text, exact)
        self.key("enter")
        return self.wait(
            lambda s: (
                (s["kind"], s["title"], s["subtitle"], [i["text"] for i in s["items"]])
                != (before["kind"], before["title"], before["subtitle"], [i["text"] for i in before["items"]])
            ),
            f"open {text}",
        )

    def enter_options(self):
        return self.open_menu("options", exact=True)

    def set_item(self, text, value, exact=False, section=None):
        state = self.select(text, exact, section)
        item = state["items"][state["selected"]]
        if item["type"] in (2, 3):
            if item["value"] != value:
                self.key("space")
        elif item["type"] in (5, 7):
            if not item["min"] <= value <= item["max"]:
                raise ValueError(f"value {value} outside observed range {item}")
            for _ in range(abs(item["value"] - value)):
                self.key("right" if item["value"] < value else "left")
        else:
            raise ProbeFailure(f"unsupported setting widget {item}")
        return self.wait(
            lambda s: bool(s["items"]) and s["selected"] >= 0 and s["items"][s["selected"]].get("value") == value,
            f"widget {text}={value}",
        )

    def back_to_options(self):
        self.key("escape")
        state = self.wait(
            lambda s: self.has_item(s, "sound effects & music") or "requires restart" in normalized(s["subtitle"]),
            "options or graphics restart notice",
        )
        if "requires restart" in normalized(state["subtitle"]):
            self.key("enter")
        return self.wait(lambda s: self.has_item(s, "sound effects & music"), "return to options")

    def assert_config(self, expected):
        return self.wait(
            lambda s: all(s["config"].get(k) == v for k, v in expected.items()),
            "effective config " + json.dumps(expected, sort_keys=True),
        )

    def assert_player(self, expected):
        return self.wait(
            lambda s: all(s["player"].get(k) == v for k, v in expected.items()),
            "effective player " + json.dumps(expected, sort_keys=True),
        )

    def start_level(self, mission=None, level=1, difficulty="Trainee", version=None):
        self.open_menu("new game", exact=True)
        for _ in range(12):
            state = self.wait(lambda s: True, "current start route")
            if state["kind"] == "game":
                break
            if state["kind"] == "listbox":
                if not mission:
                    raise ProbeFailure("mission picker requires an explicit observed mission name")
                self.select(mission)
                self.key("enter")
            elif self.has_item(state, difficulty, exact=True):
                self.select(difficulty, exact=True)
                self.key("enter")
            elif state["kind"] == "menu" and "starting level" in normalized(state["subtitle"]):
                self.key("backspace")
                for character in str(level):
                    self.key(character)
                self.key("enter")
            elif state["kind"] == "other":
                # The only unclassified window on this bounded start route is a briefing/movie
                self.key("escape")
            else:
                raise ProbeFailure(f"unrecognized start-level route: {state}")
        result = self.wait(
            lambda s: s["kind"] == "game" and s["game"]["level"] == level and s["world"]["game_time"] >= 32768,
            "loaded level and advancing simulation",
            timeout=30,
        )
        if mission and normalized(mission).removeprefix("d1: ") not in normalized(result["mission"]["name"]):
            raise ProbeFailure(f"wrong actual mission name: {result['mission']}")
        if version is not None and result["mission"]["version"] != version:
            raise ProbeFailure(f"wrong actual mission version: {result['mission']}")
        return result

    def enter_game_options(self):
        self.key("f2")
        return self.wait(lambda s: self.has_item(s, "sound effects & music"), "in-game options")

    def automap_roundtrip(self):
        self.key("tab")
        self.wait(lambda s: s["kind"] == "automap", "actual automap active")
        self.key("escape")
        return self.wait(lambda s: s["kind"] == "game", "automap closed to game")

    def save_first_slot(self, name="configqa"):
        save = self.profile / "player.sg0"
        previous = (save.stat().st_mtime_ns, hashlib.sha256(save.read_bytes()).hexdigest()) if save.exists() else None
        self.key("f2", modifiers=0x300)
        self.wait(lambda s: normalized(s["subtitle"]) == "save game", "save slot menu")
        self.key("home")
        self.key("enter")
        for character in name:
            self.key(character)
        self.last_saved_state = self.wait(lambda s: normalized(s["subtitle"]) == "save game", "frozen pre-save state")
        self.key("enter")
        self.wait(lambda s: s["kind"] == "game", "save completed")
        save = self.profile / "player.sg0"
        if not save.exists() or save.stat().st_size < 100:
            raise ProbeFailure("save action did not create a nonempty first slot")
        current = (save.stat().st_mtime_ns, hashlib.sha256(save.read_bytes()).hexdigest())
        if current == previous:
            raise ProbeFailure("save action left the previous slot unchanged")
        self.record(
            "save_file", name=str(save), size=save.stat().st_size, sha256=hashlib.sha256(save.read_bytes()).hexdigest()
        )
        return save

    def load_first_slot(self, name="configqa"):
        self.key("f3", modifiers=0x300)
        self.wait(lambda s: self.has_item(s, name, exact=True), "saved slot visible")
        self.select(name, exact=True)
        self.key("enter")
        state = self.wait(
            lambda s: self.has_item(s, "yes", exact=True) or s["kind"] == "game", "restore prompt or loaded game"
        )
        if self.has_item(state, "yes", exact=True):
            self.select("yes", exact=True)
            self.key("enter")
        return self.wait(
            lambda s: s["kind"] == "game" and s["world"]["game_time"] >= 8192,
            "restored level and advancing simulation",
            timeout=30,
        )

    def leave_game(self):
        self.key("escape")
        self.wait(lambda s: self.has_item(s, "abort game", exact=True), "game pause menu")
        self.select("abort game", exact=True)
        self.key("enter")
        state = self.wait(
            lambda s: self.has_item(s, "new game", exact=True) or self.has_item(s, "yes", exact=True),
            "abort prompt or main menu",
        )
        if self.has_item(state, "yes", exact=True):
            self.select("yes", exact=True)
            self.key("enter")
        return self.wait(
            lambda s: self.has_item(s, "new game", exact=True) and not s["game"]["active"], "game closed to main menu"
        )

    def close(self):
        if not self.process:
            raise ProbeFailure("no launched process to close")
        if self.process.poll() is not None:
            if getattr(self, "clean_exit", False):
                return
            raise ProbeFailure(f"unexpected engine exit before quit: {self.process.returncode}")
        try:
            self.wait(lambda s: self.has_item(s, "quit", exact=True), "main menu before quit", timeout=3)
            self.select("quit", exact=True)
            # Exit may precede key acknowledgement; write the final key directly
            self.sequence += 1
            command = self.probe / "command.tmp"
            command.write_text(f"{self.sequence} 13 0\n")
            command.replace(self.probe / "command.txt")
            self.process.wait(timeout=10)
            if self.process.returncode != 0:
                raise ProbeFailure(f"engine quit exit {self.process.returncode}")
            final_log = self.log_path.read_text(errors="replace")
            match = FATAL.search(final_log)
            if match:
                raise ProbeFailure(f"fatal diagnostic during quit: {match.group(0)}")
            self.clean_exit = True
            self.record("clean_exit", returncode=self.process.returncode)
        finally:
            self.stop()

    def stop(self):
        if self.process and self.process.poll() is None:
            os.killpg(self.process.pid, signal.SIGTERM)
            try:
                self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                os.killpg(self.process.pid, signal.SIGKILL)
                self.process.wait(timeout=3)
        if hasattr(self, "log"):
            self.log.close()


FACTORS = {
    "DigiVolume": [0, 4, 8],
    "MusicVolume": [0, 4, 8],
    "MusicType": [0, 1],
    "ReverseStereo": [0, 1],
    "TexFilt": [0, 1, 2],
    "VSync": [0, 1],
    "GammaLevel": [0, 8, 16],
    "MouseControlStyle": [0, 1, 2],
}


def covering_cases(seed, factors=None, triples=None):
    """Deterministic greedy pairwise with explicitly requested high-risk triples."""
    rng = random.Random(seed)
    factors = FACTORS if factors is None else factors
    triples = [("MusicType", "TexFilt", "ReverseStereo")] if triples is None else triples
    names = list(factors)
    rows = list(itertools.product(*factors.values()))
    rng.shuffle(rows)
    groups = list(itertools.combinations(range(len(names)), 2))
    groups += [tuple(names.index(name) for name in triple) for triple in triples]

    def interactions(row):
        return {(g, tuple(row[i] for i in g)) for g in groups}

    candidates = [(row, interactions(row)) for row in rows]
    remaining = set().union(*(covered for _, covered in candidates))
    selected = []
    while remaining:
        row, covered = max(candidates, key=lambda pair: len(pair[1] & remaining))
        remaining -= covered
        selected.append(dict(zip(names, row)))
        candidates.remove((row, covered))
    return selected


def apply_settings(session, values):
    session.open_menu("sound effects & music")
    session.set_item("fx volume", values["DigiVolume"])
    session.set_item("music volume", values["MusicVolume"], exact=True)
    session.set_item("reverse stereo", values["ReverseStereo"])
    session.set_item("no music" if values["MusicType"] == 0 else "built-in/addon music", 1, exact=True)
    session.back_to_options()
    session.assert_config({k: values[k] for k in ("DigiVolume", "MusicVolume", "MusicType", "ReverseStereo")})
    session.open_menu("graphics options")
    session.set_item(["None (Classical)", "Bilinear", "Trilinear"][values["TexFilt"]], 1, exact=True)
    session.set_item("VSync", values["VSync"], exact=True)
    session.set_item("brightness", values["GammaLevel"])
    session.back_to_options()
    session.assert_config({k: values[k] for k in ("TexFilt", "VSync", "GammaLevel")})
    session.open_menu("controls")
    session.set_item(["Rebirth", "FlightSim", "Old school"][values["MouseControlStyle"]], 1, exact=True)
    session.back_to_options()
    session.assert_player({"MouseControlStyle": values["MouseControlStyle"]})


def assert_reopened_widgets(session, values):
    groups = [
        (
            "sound effects & music",
            [
                ("FX Volume", values["DigiVolume"]),
                ("music volume", values["MusicVolume"]),
                ("Reverse Stereo", values["ReverseStereo"]),
                ("no music" if values["MusicType"] == 0 else "built-in/addon music", 1),
            ],
        ),
        (
            "graphics options",
            [
                (["None (Classical)", "Bilinear", "Trilinear"][values["TexFilt"]], 1),
                ("VSync", values["VSync"]),
                ("Brightness", values["GammaLevel"]),
            ],
        ),
        ("controls", [(["Rebirth", "FlightSim", "Old school"][values["MouseControlStyle"]], 1)]),
    ]
    for menu, widgets in groups:
        session.open_menu(menu)
        for text, value in widgets:
            session.select(text, exact=True)
            session.wait(
                lambda state: (
                    bool(state["items"])
                    and state["selected"] >= 0
                    and state["items"][state["selected"]].get("value") == value
                ),
                f"reopened widget {text}={value}",
            )
        session.back_to_options()


def run_case(executable, data, directory, game, values, timeout=240, mission=None, mission_version=None, level=1):
    session = HostSession(
        executable,
        data,
        directory,
        game,
        timeout,
        config={"ResolutionX": 640, "ResolutionY": 480, "WindowMode": 1, "MusicType": 0},
    )
    try:
        session.start()
        if mission:
            session.start_level(mission=mission, level=level, version=mission_version)
        original = session.wait(lambda state: bool(state.get("config")), "baseline effective settings")
        baseline = {k: original["player" if k == "MouseControlStyle" else "config"][k] for k in FACTORS}
        session.enter_game_options() if mission else session.enter_options()
        apply_settings(session, values)
        assert_reopened_widgets(session, values)
        session.key("escape")
        if mission:
            session.automap_roundtrip()
            session.leave_game()
        session.close()
        saved = (session.profile / "descent.cfg").read_text()
        session.record("persisted_config", content=saved)
        parsed = dict(line.split("=", 1) for line in saved.splitlines() if "=" in line)
        for key, value in values.items():
            if key != "MouseControlStyle" and parsed.get(key) != str(value):
                raise ProbeFailure(f"persisted {key}: {parsed.get(key)!r}, expected {value}")
        session.start()
        if mission:
            session.start_level(mission=mission, level=level, version=mission_version)
        session.assert_config({k: v for k, v in values.items() if k != "MouseControlStyle"})
        session.assert_player({"MouseControlStyle": values["MouseControlStyle"]})
        session.enter_game_options() if mission else session.enter_options()
        apply_settings(session, baseline)
        session.key("escape")
        if mission:
            session.leave_game()
        session.close()
        return {
            "status": "pass",
            "oracle": "menu widgets, effective config, disk persistence, relaunch, A-B-A",
            "values": values,
            "mission": mission,
            "mission_version": mission_version,
            "level": level if mission else None,
        }
    finally:
        session.stop()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True)
    parser.add_argument("--data", required=True)
    parser.add_argument("--game", choices=("d1", "d2"), required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--seed", type=int, default=20261014)
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--hours", type=float, default=1)
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--mission")
    parser.add_argument("--mission-version", type=int, choices=(1, 2))
    parser.add_argument("--level", type=int, default=1)
    parser.add_argument("--replay", help="JSON containing one values object")
    args = parser.parse_args()
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=False)
    cases = [json.loads(Path(args.replay).read_text())["values"]] if args.replay else covering_cases(args.seed)
    if args.limit:
        cases = cases[: args.limit]
    write_json(
        output / "plan.json",
        {
            "seed": args.seed,
            "game": args.game,
            "cases": cases,
            "coverage": "pairwise plus music/filter/stereo triples",
            "scope": "native desktop public data",
            "mission": args.mission,
            "mission_version": args.mission_version,
            "level": args.level if args.mission else None,
        },
    )
    if args.prepare_only:
        return 0
    executable = freeze_binary(args.executable, output)
    deadline = time.monotonic() + args.hours * 3600
    failures = 0
    executed = 0
    for number, values in enumerate(cases):
        if time.monotonic() >= deadline:
            break
        directory = output / f"case-{number:04d}"
        directory.mkdir()
        write_json(directory / "case.json", {"seed": args.seed, "game": args.game, "values": values})
        started = time.monotonic()
        try:
            result = run_case(
                executable,
                args.data,
                directory,
                args.game,
                values,
                timeout=min(240, deadline - time.monotonic()),
                mission=args.mission,
                mission_version=args.mission_version,
                level=args.level,
            )
        except OSError as error:
            result = {"status": "blocked", "error": str(error), "values": values}
            failures += 1
        except (ProbeFailure, ValueError, subprocess.SubprocessError) as error:
            result = {"status": "failed", "error": str(error), "values": values}
            failures += 1
        executed += 1
        result.update(case=number, elapsed=round(time.monotonic() - started, 3))
        write_json(directory / "result.json", result)
        with (output / "journal.jsonl").open("a") as journal:
            journal.write(json.dumps(result, sort_keys=True) + "\n")
        print(json.dumps(result, sort_keys=True), flush=True)
        # Stop setup-wide repeated failures; failures remain replayable
        if result["status"] == "blocked" or (failures >= 3 and number < 3):
            break
    complete = executed == len(cases)
    write_json(
        output / "summary.json",
        {
            "status": "failed" if failures else ("complete" if complete else "incomplete"),
            "planned": len(cases),
            "executed": executed,
            "passed": executed - failures,
            "failed_or_blocked": failures,
            "complete": complete,
            "seed": args.seed,
            "limited_subset": bool(args.limit),
        },
    )
    return 1 if failures else (0 if complete else 2)


if __name__ == "__main__":
    raise SystemExit(main())
