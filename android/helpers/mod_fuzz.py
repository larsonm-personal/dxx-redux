"""Bounded, serial Android mod campaigns with durable evidence and exact replay"""

import argparse
import copy
import hashlib
import itertools
import json
import os
from pathlib import Path
import random
import re
import subprocess
import sys
import time

REPO = Path(__file__).resolve().parents[2]
SCHEMA = 1


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def write_json(path, value):
    path = Path(path)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


def append_record(path, value):
    with Path(path).open("a", encoding="utf-8") as stream:
        stream.write(json.dumps(value, sort_keys=True) + "\n")
        stream.flush()
        os.fsync(stream.fileno())


def records(path):
    if not Path(path).exists():
        return []
    result = []
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        try:
            result.append(json.loads(line))
        except json.JSONDecodeError:
            # An interrupted final append is not a completed case
            continue
    return result


def compatible(mode, packs, variants=True):
    if mode == "mac-d1":
        return [n for n in packs if n in ("d1xr-hires.dxa", "d1xr-mac-demo-sounds.dxa")]
    game = "d2" if mode in ("d2", "vertigo", "demo-d2") else "d1"
    return [
        n
        for n in packs
        if (n.startswith(game + "-") or n.startswith("uu" + game) or (game == "d1" and n == "d1xr-hires.dxa"))
        and (variants or not re.search(r"-(128|512)-", n))
        and (not mode.startswith("demo-") or not n.startswith("uud"))
    ]


def hog_names(path):
    # Read only the archive directory; mission/level interpretation stays in the engine
    names = []
    with Path(path).open("rb") as stream:
        if stream.read(3) != b"DHF":
            raise ValueError(f"Not a HOG archive: {path}")
        while header := stream.read(17):
            if len(header) != 17:
                raise ValueError(f"Truncated HOG directory: {path}")
            names.append(header[:13].split(b"\0")[0].decode("ascii").lower())
            stream.seek(int.from_bytes(header[13:], "little"), 1)
    return names


def levels_for(mode, fixture):
    assets = {a["file"].lower(): a["path"] for a in fixture["assets"]}
    if mode == "vertigo":
        descriptor = Path(assets["d2x.mn2"]).read_text(encoding="latin1")
        match = re.search(r"num_levels\s*=\s*(\d+)\s*\n", descriptor, re.I)
        if not match:
            raise ValueError("Vertigo descriptor has no level list")
        names = descriptor[match.end() :].splitlines()[: int(match[1])]
        archive = hog_names(assets["d2x.hog"])
        if any(n.strip().lower() not in archive for n in names):
            raise ValueError("Vertigo level list disagrees with archive")
        return [n.strip().lower() for n in names]
    hog = assets.get("descent2.hog") if mode == "d2" else assets.get("descent.hog")
    if mode == "demo-d2":
        hog = assets.get("d2demo.hog")
    names = hog_names(hog)
    if mode == "d2":
        from dos_midi_capture import extract_member

        descriptor = extract_member(Path(hog), "d2.mn2").decode("latin1")
        match = re.search(r"num_levels\s*=\s*(\d+)\s*\n", descriptor, re.I)
        if not match:
            raise ValueError("Counterstrike descriptor has no level list")
        levels = [n.strip().lower() for n in descriptor[match.end() :].splitlines()[: int(match[1])]]
        if len(levels) != int(match[1]) or any(n not in names for n in levels):
            raise ValueError("Counterstrike level list disagrees with archive")
        return levels
    if mode == "demo-d2":
        return sorted(n for n in names if re.fullmatch(r"d2leva-\d\.sl2", n))
    return sorted(n for n in names if re.fullmatch(r"level\d+\.(rdl|sdl)", n))


def schedule(config, seed):
    packs = [p["filename"] for p in config["packs"]]
    fixtures = config["fixtures"]
    sequence = 0

    def case(mode, mods, phase, level=1, transition="menus", diagnostic=False):
        nonlocal sequence
        sequence += 1
        return dict(
            schema=SCHEMA,
            id=f"{sequence:05d}-{phase}-{mode}-L{level}",
            seed=seed,
            generator="python-random-v1",
            mode=mode,
            mods=list(mods),
            phase=phase,
            level=level,
            transition=transition,
            diagnostic=diagnostic,
            play_seconds=12,
            timeout_seconds=240,
        )

    for mode in sorted(fixtures):
        yield case(mode, [], "baseline", diagnostic=True)
    for mode in sorted(fixtures):
        for mod in compatible(mode, packs):
            yield case(mode, [mod], "singleton", transition="toggle", diagnostic=True)
        enabled = compatible(mode, packs, variants=False)
        yield case(mode, enabled, "all-on", 2 if mode == "vertigo" else 1, "toggle", True)
    for variant, order in (("author", "none"), ("extras", "none"), ("extras", "uud-first"), ("extras", "hires-first")):
        item = case("d2", ["uud2tp-textures.dxa"], "authored", transition="authored", diagnostic=True)
        item.update(
            external_test="test_uud_mission_gameplay",
            arguments=["-Variant", variant, "-HighResOrder", order],
            timeout_seconds=1800,
        )
        yield item
    for name in ("test_mod_save_toggle", "test_startup_restore_failure", "test_uud2sp"):
        item = case("d2", [], "regression", transition=name, diagnostic=True)
        item.update(external_test=name, arguments=[], timeout_seconds=1200)
        yield item
    for mode in ("d1", "d2", "d1-in-d2", "vertigo"):
        enabled = compatible(mode, packs, variants=False)
        for pair in itertools.combinations(enabled, 2):
            yield case(mode, pair, "pair", 2 if mode == "vertigo" else 1, "toggle", True)
        yield case(mode, enabled[::-1], "reverse", 2 if mode == "vertigo" else 1, "reorder", True)
    for mode in ("d1", "d2", "d1-in-d2", "vertigo", "demo-d1", "demo-d2", "mac-d1"):
        enabled = compatible(mode, packs, variants=False)
        for level, _ in enumerate(config["levels"][mode], 1):
            yield case(mode, enabled, "level", level, "menus")
    for mode in ("d2", "vertigo"):
        yield case(mode, compatible(mode, packs, variants=False), "secret", transition="secret")
    rng = random.Random(seed)
    previous = None
    while True:
        mode = rng.choice(["d1", "d2", "d1-in-d2", "vertigo", "vertigo"])
        candidates = compatible(mode, packs)
        resolution = rng.choice(("128", "256", "512"))
        candidates = [n for n in candidates if "textures-ktx2" not in n or f"-{resolution}-" in n]
        enabled = [n for n in candidates if rng.random() < 0.7]
        rng.shuffle(enabled)
        level = rng.randrange(1, len(config["levels"][mode]) + 1)
        current = case(
            mode,
            enabled,
            "random",
            level,
            rng.choice(("toggle", "reorder", "menus", "reentry", "switch", "abort")),
            rng.random() < 0.05,
        )
        if previous is not None and sequence % 11 == 0:
            repeated = copy.deepcopy(previous)
            repeated["id"] = current["id"] + "-repeat"
            repeated["repeat_of"] = previous["id"]
            yield repeated
        else:
            yield current
            previous = current


def setup(command, **args):
    return dict(action="setup_command", command=command, args=args, post_delay_ms=100)


def installed_candidates(config, case):
    packs = [p["filename"] for p in config["packs"]]
    return packs if case.get("shared_set") else compatible(case["mode"], packs)


def effective_case(case):
    result = copy.deepcopy(case)
    result["mods"] = compatible(case["mode"], case["mods"])
    return result


def shared_schedule(config, seed):
    enabled = [p["filename"] for p in config["packs"] if p["recommended"] and "mac-demo" not in p["filename"]]
    for index, (transition, mode) in enumerate(
        itertools.product(("toggle", "switch", "reorder", "menus"), ("d1", "d2", "d1-in-d2", "vertigo")), 1
    ):
        yield dict(
            schema=SCHEMA,
            id=f"shared-{index:02d}-{transition}-{mode}",
            seed=seed,
            generator="shared-retail-v1",
            mode=mode,
            mods=[] if transition == "menus" else enabled,
            phase="shared",
            shared_set=True,
            level=1,
            transition=transition,
            diagnostic=True,
            play_seconds=12,
            timeout_seconds=360,
            resume_observation_seconds=10,
        )


def asset_assertions(case):
    # Check each side of a game switch, including the D1 namespace in the D2 engine
    mods = effective_case(case)["mods"]
    checks = [dict(action="assert", expect={"mod_usage.enabled": True})]
    for name in mods:
        if "textures-ktx2" in name:
            checks.append(
                dict(
                    action="assert",
                    expect={"texture_packs.packs": {"contains_object": {"name": name, "mipmapped": {"gt": 0}}}},
                )
            )
    sounds = [name for name in mods if "sounds" in name]
    if sounds:
        checks.append(
            dict(
                action="assert",
                expect={
                    "mod_usage.sounds": {
                        "contains_object": {"source": {"contains": sounds[0]}, "resident": True, "played": True}
                    }
                },
            )
        )
    other_game = "d2" if case["mode"] in ("d1", "d1-in-d2") else "d1"
    for name in compatible(other_game, case["mods"]):
        if "sounds" in name:
            checks.append(dict(action="assert", expect={"mod_usage.sounds": {"not_contains": name}}))
    return checks


def wait(field, value, seconds=30):
    return dict(action="wait_for", field=field, value=str(value).lower(), timeout_ms=seconds * 1000)


def launch_steps(config, case):
    fixture = config["fixtures"][case["mode"]]
    result = []
    copying = False
    for original in config["template"]:
        if original.get("action") == "enter_game":
            copying = True
        if not copying:
            continue
        if original.get("when", fixture["game"]) != fixture["game"]:
            continue
        encoded = json.dumps(original).replace("${MODE}", "d1-in-d2" if case["mode"] == "d1-in-d2" else fixture["game"])
        step = json.loads(encoded.replace("${MISSION}", fixture["mission"]))
        step.pop("when", None)
        result.append(step)
        if step.get("action") == "select_mission":
            result.append(dict(action="key", key="backspace", post_delay_ms=100))
            result.extend(dict(action="key", key=digit, post_delay_ms=100) for digit in str(case["level"]))
        if step.get("action") == "wait_for" and step.get("field") == "in_game":
            break
    result.extend(
        [
            wait("game_window_is_front", True, 15),
            dict(action="assert", expect={"current_level_num": case["level"], "in_game": True}),
        ]
    )
    return result


def script_for(config, case, manifest):
    fixture = config["fixtures"][case["mode"]]
    installed = [row["filename"] for row in sorted(manifest.get("mods", []), key=lambda row: row["order"])]
    candidates = installed_candidates(config, case)
    info = dict(_standalone=False, _owner="test_mod_fuzz", games=[fixture["game"]], _deps=fixture["deps"])
    steps = [
        {"_info": info},
        dict(action="enter_launcher"),
        setup("create_set", name=fixture["set"]),
        setup("switch_set", name=fixture["set"]),
        setup("write_bool_pref", key="show_resume_offer", value=True),
        setup("write_bool_pref", key="skip_intro_movie", value=False),
        setup("write_bool_pref", key="dlog_game logs_enabled", value=case["diagnostic"]),
        setup("write_bool_pref", key="dlog_texture_enabled", value=case.get("texture_diagnostics", False)),
    ]
    for name in candidates:
        if name not in installed:
            steps.append(setup("import_mod", filename=name, path="mod-fuzz-import/" + name))
            installed.append(name)
    desired = case["mods"] + [n for n in candidates if n not in case["mods"]]
    if set(installed) != set(candidates):
        raise ValueError("Unexpected installed archive in dedicated fuzz set")
    for index, name in enumerate(desired):
        if installed[index] != name:
            other = installed.index(name)
            steps.append(setup("swap_mod_order", first=installed[index], second=name))
            installed[index], installed[other] = installed[other], installed[index]
    for name in candidates:
        steps.append(setup("set_mod_enabled", filename=name, enabled=False))
    for name in case["mods"]:
        steps.append(setup("set_mod_enabled", filename=name, enabled=True))
    steps.append(setup("clear_pilot_files"))
    steps += launch_steps(config, case)
    steps += [
        dict(action="send_axis", axis=2, axis_value=0.25),
        dict(action="send_button", button=100, held=1),
        dict(action="wait_ms", ms=1000),
        dict(action="send_button", button=100, pressed=0),
        dict(action="send_axis", axis=2, axis_value=0.0),
        dict(action="wait_ms", ms=(case["play_seconds"] - 1) * 1000),
        dict(action="assert", expect={"in_game": True, "idle_saver.game_time": {"gt": 8}}),
    ]
    if case["transition"] in ("menus", "reentry"):
        steps += [
            dict(action="key", key="tab"),
            wait("automap_active", True, 10),
            dict(action="key", key="escape"),
            wait("automap_active", False, 10),
            dict(action="set_debug", field="android_game_request", value="pause", post_delay_ms=300),
            dict(action="key", key="escape"),
            wait("game_window_is_front", True, 10),
        ]
    if case["transition"] in ("toggle", "reorder"):
        steps.append(dict(action="set_debug", field="player_energy", value="117"))
        for enabled in (False, True):
            steps.append(dict(action="enter_launcher"))
            if case["transition"] == "reorder" and len(case["mods"]) > 1:
                steps.append(setup("swap_mod_order", first=case["mods"][0], second=case["mods"][-1]))
            else:
                steps.extend(setup("set_mod_enabled", filename=name, enabled=enabled) for name in case["mods"])
            steps += [
                wait("resume_offer_enabled", True, 15),
                dict(action="tap_button", text="Load Last Save", launches_game=True),
                wait("in_game", True, 45),
                dict(
                    action="assert", expect={"current_level_num": case["level"], "player.energy": {"range": [116, 117]}}
                ),
            ]
            if case.get("resume_observation_seconds", 0):
                steps.append(dict(action="wait_ms", ms=case["resume_observation_seconds"] * 1000))
    if case["transition"] == "secret":
        for transition in ("secret_enter", "secret_exit", "secret_revisit", "secret_exit"):
            steps += [
                dict(action="set_debug", field="periodic_checkpoint", value=transition),
                wait("in_game", True),
                dict(action="introspect"),
            ]
    if case["transition"] in ("reentry", "switch"):
        alternate = copy.deepcopy(case)
        alternate["level"] = case["level"] % len(config["levels"][case["mode"]]) + 1
        if case["transition"] == "switch":
            alternate["mode"] = "d2" if fixture["game"] == "d1" else "d1"
            alternate["level"] = 1
        for destination in (alternate, case):
            steps.append(dict(action="enter_launcher"))
            # The retail fixture includes both base games; use its existing installed list
            steps += launch_steps(config, destination)
            if case.get("shared_set"):
                steps += [
                    dict(action="send_button", button=100, held=1),
                    dict(action="wait_ms", ms=1000),
                    dict(action="send_button", button=100, pressed=0),
                ]
                steps += asset_assertions(destination)
    if case["transition"] == "abort":
        steps += [
            dict(action="key", key="escape"),
            dict(action="select", text="Abort Game", timeout_ms=10000),
            wait("current_level_num", 0, 15),
        ]
        new_game = launch_steps(config, case)
        start = next(i for i, step in enumerate(new_game) if step.get("text") == "New game")
        steps += new_game[start:]
    steps += [
        dict(action="send_button", button=100, held=1),
        dict(action="wait_ms", ms=300),
        dict(action="send_button", button=100, pressed=0),
    ]
    if case.get("controlled_failure"):
        steps.append(dict(action="assert", expect={"current_level_num": 9999}))
    if case.get("shared_set"):
        steps += asset_assertions(case)
    steps.append(dict(action="introspect"))
    return steps


def classify(code, text, timed_out=False):
    if re.search(r"Fatal signal \d+|FATAL EXCEPTION:|Abort message:", text):
        return "crash"
    if "ANR in " in text:
        return "anr"
    if timed_out or "Timed out" in text or "FAIL: Timeout" in text:
        return "timeout"
    if code == 2 or "RESULT: SKIP" in text:
        return "missing_prerequisite"
    if "device offline" in text or "device not found" in text:
        return "infrastructure_failure"
    return "passed" if code == 0 else "assertion_failure"


def failure_signature(outcome, text):
    if outcome == "assertion_failure":
        lines = re.findall(r"^controller:[^\n]*", text, re.M)
        if not lines:
            lines = re.findall(r"(?:ASSERT_FAIL:|SCRIPT_RESULT: FAIL)[^\n]*", text)
    else:
        lines = re.findall(r"(?:Abort message:|#00 pc|FATAL EXCEPTION:)[^\n]*", text)
    # Preserve relative native PCs; unrelated Android service failures are not test failures
    normalized = [re.sub(r"0x[0-9a-fA-F]+|\b[0-9a-f]{32}\b|\b\d{2}:\d{2}:\d{2}\b", "<id>", line) for line in lines]
    stable = "\n".join(sorted(set(normalized)))
    return hashlib.sha256((outcome + "\n" + stable).encode()).hexdigest()


def minimize(case, reproduces, limit=6):
    result = copy.deepcopy(case)
    attempts = 0
    for name in list(result["mods"]):
        if attempts >= limit:
            break
        candidate = copy.deepcopy(result)
        candidate["mods"].remove(name)
        attempts += 1
        if reproduces(candidate):
            result = candidate
    return result


def verify_owners(state, case):
    if not case["diagnostic"]:
        return
    usage = state.get("mod_usage", {})
    if not usage.get("enabled"):
        raise AssertionError("Requested diagnostic sampling is disabled")
    sounds = usage.get("sounds", [])
    sound_packs = [name for name in case["mods"] if "sounds" in name]
    if sound_packs:
        if not sounds or not all(row.get("resident") for row in sounds):
            raise AssertionError("Enabled sound replacements are not resident")
        if not any(row.get("played") and Path(row["source"]).name == sound_packs[0] for row in sounds):
            raise AssertionError("Highest-priority sound pack never reached playback")
    elif sounds:
        raise AssertionError("Sound replacements survived while disabled")
    for name in case["mods"]:
        if name.startswith("uud") and "tp-textures" in name:
            presentation = usage.get("presentation", [])
            if not any(row.get("kind") == "metadata" for row in presentation):
                raise AssertionError("Enabled UUD metadata was not applied")
            if not any(row.get("kind") == "bitmap_patch" and Path(row["source"]).name == name for row in presentation):
                raise AssertionError("Enabled UUD indexed bitmap replacements were not loaded")
        if "textures-ktx2" in name:
            entries = state.get("texture_packs", {}).get("packs", [])
            if not any(row["name"] == name and row.get("mipmapped", 0) > 0 for row in entries):
                raise AssertionError(f"High-resolution pack was not rendered: {name}")
        if name == "d1xr-hires.dxa":
            if not any(
                row.get("kind") == "font" and Path(row["source"]).name == name for row in usage.get("presentation", [])
            ):
                raise AssertionError("Enabled D1 font replacement was not loaded")


class Campaign:
    def __init__(self, config, output, adb):
        self.config, self.output, self.adb_path = config, Path(output), adb
        self.env = dict(os.environ, ANDROID_SERIAL=config["serial"], DXX_TEST_PACKAGE=config["package"])
        self.journal = self.output / "journal.jsonl"

    def adb(self, *args, timeout=15, binary=False):
        response = subprocess.run(
            [self.adb_path, "-s", self.config["serial"], *args],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
        )
        if response.returncode:
            raise RuntimeError(response.stderr.decode(errors="replace") or response.stdout.decode(errors="replace"))
        return response.stdout if binary else response.stdout.decode(errors="replace").strip()

    def private(self, path):
        result = self.adb("exec-out", "run-as", self.config["package"], "cat", path)
        if result.startswith(("cat:", "run-as:")):
            raise RuntimeError(result)
        return result

    def manifest(self, mode):
        try:
            return json.loads(
                self.private(
                    f"files/imported/sets/{self.config['fixtures'][mode]['set']}/.content/mods/mod_manifest.json"
                )
            )
        except RuntimeError as error:
            if "No such file" in str(error):
                return {"mods": []}
            raise

    def capture(self, directory, failed):
        errors = []
        for source, name in (
            ("files/introspect.json", "introspect.json"),
            ("files/automation_result.json", "automation-result.json"),
            ("files/automation_log.jsonl", "automation-log.jsonl"),
        ):
            try:
                (directory / name).write_text(self.private(source), encoding="utf-8")
            except (RuntimeError, subprocess.TimeoutExpired) as error:
                errors.append(str(error))
        try:
            (directory / "logcat.txt").write_text(self.adb("logcat", "-d", timeout=20), encoding="utf-8")
            (directory / "battery.txt").write_text(self.adb("shell", "dumpsys", "battery"), encoding="utf-8")
            if failed:
                names = self.adb("shell", "run-as", self.config["package"], "find", "files", "-type", "f").splitlines()
                wanted = [
                    n
                    for n in names
                    if re.search(
                        r"(crash|tombstone|debuglog|automation_|mod_manifest\.json|\.sg[0-9a-f]|\.plr|\.plx)", n
                    )
                    and re.fullmatch(r"[a-zA-Z0-9_./ -]+", n)
                ]
                write_json(directory / "private-artifact-paths.json", wanted)
                # Only this disposable package is readable; never pull the user's Play saves
                if wanted:
                    (directory / "private-evidence.tar").write_bytes(
                        self.adb(
                            "exec-out",
                            "run-as",
                            self.config["package"],
                            "tar",
                            "-cf",
                            "-",
                            *wanted,
                            timeout=45,
                            binary=True,
                        )
                    )
                (directory / "exit-info.txt").write_text(
                    self.adb("shell", "dumpsys", "activity", "exit-info", self.config["package"]), encoding="utf-8"
                )
        except (RuntimeError, subprocess.TimeoutExpired) as error:
            errors.append(str(error))
        write_json(directory / "capture-errors.json", errors)

    def run_case(self, case, deadline):
        directory = self.output / case["id"]
        directory.mkdir(exist_ok=True)
        write_json(directory / "case.json", case)
        append_record(self.journal, dict(event="planned", case=case, time=time.time()))
        started = time.monotonic()
        append_record(self.journal, dict(event="started", id=case["id"], time=time.time()))
        code, timed_out = 1, False
        try:
            if case.get("external_test"):
                allowed = (
                    "test_uud_mission_gameplay",
                    "test_mod_save_toggle",
                    "test_startup_restore_failure",
                    "test_uud2sp",
                )
                if case["external_test"] not in allowed:
                    raise ValueError("Replay requests an unknown external fixture")
                args = [
                    "pwsh",
                    "-NoProfile",
                    "-NonInteractive",
                    "-File",
                    str(REPO / "android/tests" / (case["external_test"] + ".ps1")),
                    "-Serial",
                    self.config["serial"],
                    *case["arguments"],
                ]
            else:
                script = script_for(self.config, case, self.manifest(case["mode"]))
                prefixes = case.get("prefix_cases", [])
                if prefixes:
                    if len(prefixes) > 4 or any(p.get("prefix_cases") or p.get("external_test") for p in prefixes):
                        raise ValueError("Replay prefix must contain at most four plain cases")
                    sets = [self.config["fixtures"][p["mode"]]["set"] for p in [*prefixes, case]]
                    if len(sets) != len(set(sets)):
                        raise ValueError("Replay prefix cases require distinct file sets")
                    parts = [script_for(self.config, p, self.manifest(p["mode"])) for p in prefixes] + [script]
                    info = copy.deepcopy(script[0])
                    info["_info"]["_deps"] = [dep for part in parts for dep in part[0]["_info"]["_deps"]]
                    info["_info"]["games"] = sorted({game for part in parts for game in part[0]["_info"]["games"]})
                    script = [info] + [step for part in parts for step in part[1:]]
                write_json(directory / "steps.jsonc", script)
                args = [
                    "pwsh",
                    "-NoProfile",
                    "-NonInteractive",
                    "-File",
                    str(REPO / "android/helpers/run_test.ps1"),
                    "-ScriptName",
                    str(directory / "steps.jsonc"),
                    "-Game",
                    self.config["fixtures"][case["mode"]]["game"],
                    "-Serial",
                    self.config["serial"],
                    "-DataEdition",
                    "retail",
                    "-LeaveRunning",
                    "-TimeoutSeconds",
                    str(case["timeout_seconds"] - 30),
                ]
            write_json(directory / "command.json", args)
            with (directory / "runner.log").open("wb") as log:
                process = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT, env=self.env, cwd=REPO)
                try:
                    code = process.wait(timeout=max(1, min(case["timeout_seconds"], deadline - time.monotonic())))
                except subprocess.TimeoutExpired:
                    timed_out = True
                    if os.name == "nt":
                        subprocess.run(
                            ["taskkill", "/PID", str(process.pid), "/T", "/F"],
                            capture_output=True,
                            timeout=15,
                            check=False,
                        )
                    else:
                        process.kill()
                    process.wait(timeout=10)
            self.capture(directory, code != 0 or timed_out)
            if code == 0 and not case.get("external_test"):
                actual = self.manifest(case["mode"])
                write_json(directory / "manifest.json", actual)
                rows = actual["mods"]
                expected = installed_candidates(self.config, case)
                if set(r["filename"] for r in rows) != set(expected):
                    raise AssertionError("Disabled archives disappeared from installed manifest")
                root = f"files/imported/sets/{self.config['fixtures'][case['mode']]['set']}/.content/mods"
                files = self.adb("shell", "run-as", self.config["package"], "ls", "-l", root)
                (directory / "installed-files.txt").write_text(files, encoding="utf-8")
                sizes = {}
                for line in files.splitlines():
                    fields = line.split()
                    if len(fields) >= 8 and fields[0].startswith("-") and fields[4].isdigit():
                        sizes[fields[-1]] = int(fields[4])
                if any(sizes.get(name, 0) <= 0 for name in expected):
                    raise AssertionError("A disabled installed archive is missing or empty")
                if set(r["filename"] for r in rows if r["enabled"]) != set(case["mods"]):
                    raise AssertionError("Effective enabled flags differ from the case")
                state = read_json(directory / "introspect.json")
                if not state.get("in_game"):
                    raise AssertionError("No playable final game state")
                mounted = {Path(n.replace("\\", "/")).name for n in state.get("mounted_mods", [])}
                expected_mounts = case["mods"]
                if case.get("shared_set") and self.config["fixtures"][case["mode"]]["game"] == "d1":
                    expected_mounts = effective_case(case)["mods"]
                if mounted != set(expected_mounts):
                    raise AssertionError(f"Mounted mods disagree: {mounted} vs {expected_mounts}")
                # Loaded/played evidence is retained separately from mere mounting
                write_json(
                    directory / "owners.json",
                    dict(
                        textures=state.get("level_scene", {}).get("mod_texture_references", {}),
                        usage=state.get("mod_usage", {}),
                    ),
                )
                verify_owners(state, effective_case(case) if case.get("shared_set") else case)
        except (RuntimeError, AssertionError, ValueError, OSError, subprocess.TimeoutExpired) as error:
            code = 1
            (directory / "controller-error.txt").write_text("controller: " + str(error), encoding="utf-8")
            self.capture(directory, True)
        combined = "\n".join(p.read_text(encoding="utf-8", errors="replace") for p in directory.glob("*.txt"))
        if (directory / "runner.log").exists():
            combined += "\n" + (directory / "runner.log").read_text(encoding="utf-8", errors="replace")
        outcome = classify(code, combined, timed_out)
        result = dict(
            event="completed",
            id=case["id"],
            outcome=outcome,
            elapsed_seconds=round(time.monotonic() - started, 3),
            phase=case["phase"],
            mode=case["mode"],
            level=case["level"],
            mods=case["mods"],
            transition=case["transition"],
            signature=failure_signature(outcome, combined),
            time=time.time(),
        )
        write_json(directory / "result.json", result)
        append_record(self.journal, result)
        # Evidence is flushed before any recovery action
        self.adb("shell", "am", "force-stop", self.config["package"])
        print(f"{case['id']}: {outcome} ({result['elapsed_seconds']:.1f}s)", flush=True)
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inventory", required=True)
    parser.add_argument("--adb", required=True)
    parser.add_argument("--seed", type=int, default=20261011)
    parser.add_argument("--hours", type=float, default=8)
    parser.add_argument("--profile", choices=("smoke", "overnight", "shared"), default="smoke")
    parser.add_argument("--resume", action="store_true")
    parser.add_argument("--replay")
    parser.add_argument("--controlled-failure", action="store_true")
    args = parser.parse_args()
    config = read_json(args.inventory)
    if not config["serial"].startswith("emulator-") and config["package"] != "com.dxxredux.app.nsdtest":
        raise ValueError("Physical campaigns require the isolated diagnostic package")
    output = Path(args.inventory).resolve().parent
    if args.profile == "shared":
        retail = config["fixtures"]["vertigo"]
        for mode in ("d1", "d2", "d1-in-d2", "vertigo"):
            fixture = config["fixtures"][mode]
            fixture["set"] = "mod-fuzz-shared-retail"
            fixture["deps"] = copy.deepcopy(retail["deps"])
            for dep in fixture["deps"]:
                dep["target"] = "files/imported/sets/mod-fuzz-shared-retail"
    generate = shared_schedule if args.profile == "shared" else schedule
    config["levels"] = {mode: levels_for(mode, fixture) for mode, fixture in config["fixtures"].items()}
    if any(not levels for levels in config["levels"].values()):
        raise ValueError("A required fixture has no verified level inventory")
    write_json(output / "level-inventory.json", config["levels"])
    systematic = list(itertools.takewhile(lambda case: case["phase"] != "random", generate(config, args.seed)))
    write_json(output / "systematic-plan.json", systematic)
    campaign = Campaign(config, output, args.adb)
    completed = {r["id"]: r for r in records(campaign.journal) if r.get("event") == "completed"}
    if completed and not args.resume:
        raise ValueError("Existing journal requires --resume")
    prior_seconds = sum(r["elapsed_seconds"] for r in completed.values())
    started = time.monotonic()
    deadline = started + max(0, args.hours * 3600 - prior_seconds)
    original = campaign.adb("shell", "settings", "get", "global", "stay_on_while_plugged_in")
    power_file = output / "power-original.json"
    if args.resume and power_file.exists():
        original = read_json(power_file)["original"]
    else:
        write_json(power_file, dict(original=original))
    failures = []
    reproduced = {r.get("signature") for r in completed.values() if r.get("phase") == "repro"}
    campaign.adb("shell", "svc", "power", "stayon", "true")
    try:
        cases = [read_json(args.replay)] if args.replay else generate(config, args.seed)
        for case in cases:
            if args.replay:
                case["replay_of"] = case["id"]
                case["id"] += f"-replay-{len(completed):05d}"
            if args.profile == "smoke" and not args.replay:
                if case["phase"] == "authored":
                    break
                if case["phase"] not in ("baseline", "all-on"):
                    continue
            if case["id"] in completed:
                continue
            if time.monotonic() + 60 >= deadline:
                break
            if args.controlled_failure:
                case["controlled_failure"] = True
            result = campaign.run_case(case, deadline)
            completed[case["id"]] = result
            if result["outcome"] != "passed":
                failures.append(result)
            if (
                args.profile == "overnight"
                and not case.get("external_test")
                and result["outcome"] in ("crash", "anr", "assertion_failure")
                and result["signature"] not in reproduced
            ):
                reproduced.add(result["signature"])
                attempt = 0

                def reproduce(candidate):
                    nonlocal attempt
                    if time.monotonic() + candidate["timeout_seconds"] + 60 >= deadline:
                        return False
                    attempt += 1
                    candidate = copy.deepcopy(candidate)
                    candidate["id"] = case["id"] + f"-repro-{attempt}"
                    candidate["phase"] = "repro"
                    retry = campaign.run_case(candidate, deadline)
                    completed[candidate["id"]] = retry
                    return retry["outcome"] == result["outcome"] and retry["signature"] == result["signature"]

                if reproduce(case):
                    minimal = minimize(case, reproduce, limit=4)
                    write_json(output / case["id"] / "reduced-case.json", minimal)
            report = dict(
                schema=SCHEMA,
                seed=args.seed,
                profile=args.profile,
                budget_seconds=args.hours * 3600,
                elapsed_seconds=prior_seconds + time.monotonic() - started,
                completed=list(completed.values()),
                outcomes={
                    kind: sum(r["outcome"] == kind for r in completed.values())
                    for kind in sorted({r["outcome"] for r in completed.values()})
                },
                finished=False,
                systematic_pending=[case["id"] for case in systematic if case["id"] not in completed],
            )
            write_json(output / "summary.json", report)
            if args.controlled_failure or result["outcome"] == "infrastructure_failure":
                break
            latest = list(completed.values())[-3:]
            if len(latest) == 3 and all(r["outcome"] == "timeout" for r in latest):
                break
        report = dict(read_json(output / "summary.json")) if (output / "summary.json").exists() else dict(completed=[])
        report.update(
            finished=True,
            elapsed_seconds=prior_seconds + time.monotonic() - started,
            deadline_reached=time.monotonic() + 60 >= deadline,
            completed=list(completed.values()),
            systematic_pending=[case["id"] for case in systematic if case["id"] not in completed],
            outcomes={
                kind: sum(r["outcome"] == kind for r in completed.values())
                for kind in sorted({r["outcome"] for r in completed.values()})
            },
        )
        write_json(output / "summary.json", report)
    finally:
        if original == "null":
            campaign.adb("shell", "settings", "delete", "global", "stay_on_while_plugged_in")
        else:
            campaign.adb("shell", "settings", "put", "global", "stay_on_while_plugged_in", original)
    if args.controlled_failure:
        if len(failures) != 1 or failures[0]["outcome"] != "assertion_failure":
            return 1
        directory = output / failures[0]["id"]
        evidence = read_json(directory / "automation-result.json")
        return 0 if "9999" in json.dumps(evidence) and (directory / "private-evidence.tar").stat().st_size > 0 else 1
    return 1 if any(result["outcome"] != "passed" for result in completed.values()) else 0


if __name__ == "__main__":
    sys.exit(main())
