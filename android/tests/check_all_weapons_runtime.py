"""Qualify bounded native all-weapons captures without publishing or resealing evidence.

Use existing clean-revision builds and an SDL display (Xvfb is sufficient).
Native D1 scourge/bigred and native D2 honestbob candidates replay through both
backends. Imported D1 is a live inventory diagnostic only. Retail asset playback
and Android device execution are explicitly outside this qualification.
"""

import argparse
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

import compact_public_replay as compact
import d1_replay_parity as parity
import ordered_public_replay as ordered
from capture_public_d1_fault_probes import clean_stamp, digest, git
from check_ordered_input_runtime import isolated_native_env, replay, require
from run_compact_public_replays import validate_assets

FIXTURE = "android/tests/public_all_weapons_fixture.hpp"
RECORDER = "android/app/src/main/cpp/shared/input_demo_recorder.cpp"
CHEAT = "android/app/src/main/cpp/shared/weapon_cheat_shared.c"
RECIPES = (
    FIXTURE,
    RECORDER,
    CHEAT,
    "android/app/src/main/cpp/shared/weapon_cheat_shared.h",
    "android/app/src/main/cpp/shared/input_demo_dispatch.h",
    "android/app/src/main/cpp/shared/input_demo_direct_command_policy.c",
    "android/app/src/main/cpp/shared/input_demo_fixture.cpp",
    "android/tests/test_upstream_compat.cpp",
    "android/tests/public_level_replay_fixture.hpp",
    "android/tests/check_all_weapons_runtime.py",
    "android/tests/compact_public_replay.py",
    "android/tests/ordered_public_replay.py",
    *(
        f"{game}/main/{name}"
        for game in ("d1", "d2")
        for name in ("gamecntl.c", "input_demo_hooks.c", "CMakeLists.txt")
    ),
)
CASES = (
    ("d1-scourge", "d1", "d1", "d1_scourge"),
    ("d1-bigred", "d1", "d1", "d1_bigred"),
    ("d2-honestbob", "d2", "d2", "d2_honestbob"),
    ("imported-d1", "d2", "d1", "d1_scourge"),
)
GRANT_CODES = {"d1_scourge": 1, "d1_bigred": 2, "d2_honestbob": 3}
PHASES = ("first_grant", "between_grants", "repeated_grant", "spent_inventory", "later_grant", "tail_grant")
MAX_RAW_BYTES = 64 * 1024 * 1024
MAX_REPORT_BYTES = 2 * 1024 * 1024
SCOPE = (
    "Native public-shareware SDL captures and headed/headless replay only; "
    "D1-in-D2 is live diagnostic evidence, not compact replay admission. "
    "No retail assets were played and no Android device was tested. "
    "Recording-off retail policy branches do not establish retail asset playback. "
    "Publication requires separate review."
)


def source_identity(repo, revision):
    require(re.fullmatch(r"[0-9a-f]{40}", revision) is not None, "--revision must be the exact 40-character clean HEAD")
    require(git(repo, "rev-parse", "HEAD").decode().strip() == revision, "Source HEAD differs from --revision")
    require(
        not git(repo, "status", "--porcelain", "--untracked-files=all").strip(),
        "Source is dirty; commit the final recipes before qualification",
    )
    hashes = {}
    for name in RECIPES:
        raw = (repo / name).read_bytes()
        require(
            raw == git(repo, "show", f"{revision}:{name}"), f"Source recipe differs from the exact revision: {name}"
        )
        hashes[name] = hashlib.sha256(raw).hexdigest()
    return hashes


def build_identity(repo, build, game, revision, executables):
    """Bind all three binaries to their native source and real compile recipes."""
    cache = {}
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        match = re.match(r"([^#/:][^:]*):[^=]+=(.*)$", line)
        if match:
            cache[match[1]] = match[2]
    require(
        Path(cache.get("CMAKE_HOME_DIRECTORY", "")).resolve() == repo / game,
        "Build belongs to another source tree or game",
    )
    require(
        cache.get("CMAKE_GENERATOR") in ("Ninja", "Unix Makefiles"),
        "Use an existing single-config Ninja or Unix Makefiles build",
    )
    if "DXX_INPUT_DEMO_GIT_VERSION" in cache:
        clean_stamp(cache["DXX_INPUT_DEMO_GIT_VERSION"], revision)
    targets = {
        "capture": "test_upstream_compat",
        "graphics": f"dxx-redux-{game}",
        "headless": f"dxx-redux-{game}-headless",
    }
    wanted = {role: {RECORDER, CHEAT, f"{game}/main/gamecntl.c", f"{game}/main/input_demo_hooks.c"} for role in targets}
    wanted["capture"].add("android/tests/test_upstream_compat.cpp")
    found = {role: set() for role in targets}
    stamps = set()
    for row in compact._json((build / "compile_commands.json").read_bytes()):
        arguments = row.get("arguments") or shlex.split(row["command"])
        output = row.get("output", "")
        if "-o" in arguments:
            output = arguments[arguments.index("-o") + 1]
        roles = [role for role, target in targets.items() if f"CMakeFiles/{target}.dir/" in output.replace("\\", "/")]
        if not roles:
            continue
        role = roles[0]
        source = (Path(row["directory"]) / row["file"]).resolve()
        names = [name for name in wanted[role] if source == repo / name]
        if not names:
            continue
        defines = [arg for arg in arguments if arg.startswith("-DDXX_INPUT_DEMO_GIT_VERSION=")]
        require(len(defines) == 1, "Native compile recipe lacks a unique recorder stamp")
        require(
            any(arg.startswith("-DDXX_BUILD_DESCENT_II") for arg in arguments) == (game == "d2"),
            "Native compile recipe has the wrong engine profile",
        )
        require(
            any(arg.startswith("-DDXX_HEADLESS_CONSOLE") for arg in arguments) == (role != "graphics"),
            "Compile recipe has the wrong replay backend",
        )
        stamp = defines[0].split("=", 1)[1].strip('"')
        clean_stamp(stamp, revision)
        stamps.add(stamp)
        found[role].add(names[0])
    require(found == wanted and len(stamps) == 1, "Missing or inconsistent native fixture/replay compile recipes")
    stamp = stamps.pop()
    binaries = {}
    for role, target in targets.items():
        exe = executables[role]
        require(not exe.is_symlink() and exe.is_file() and os.access(exe, os.X_OK), f"Missing executable: {exe}")
        filename = f"{game}x-redux" if role == "graphics" else target
        require(
            exe.resolve() in {(build / "main" / (filename + suffix)).resolve() for suffix in ("", ".exe")},
            "Executable must be the selected build's original target",
        )
        require(stamp.encode() + b"\0" in exe.read_bytes(), "Executable lacks the expected clean source stamp")
        binaries[role] = {"path": str(exe), "sha256": digest(exe)}
    return {"stamp": stamp, "compile_commands_sha256": digest(build / "compile_commands.json"), "executables": binaries}


def integer(value, label, minimum=0, maximum=2**31 - 1):
    require(type(value) is int and minimum <= value <= maximum, f"Invalid {label}")


def inventory(value, game):
    fields = {
        "primary_flags",
        "secondary_flags",
        "primary_ammo",
        "secondary_ammo",
        "energy",
        "laser_level",
        "flags",
        "score",
        "cheats_enabled",
        "primary_selected",
        "secondary_selected",
    }
    require(isinstance(value, dict) and set(value) == fields, "Incomplete inventory observation")
    for key in fields - {"primary_ammo", "secondary_ammo"}:
        integer(value[key], "inventory." + key)
    for key in ("primary_ammo", "secondary_ammo"):
        require(
            isinstance(value[key], list) and len(value[key]) == (5 if game == "d1" else 10),
            "Wrong native ammo capacity",
        )
        for number in value[key]:
            integer(number, "inventory." + key, maximum=65535)


def expected_grant(game, grant):
    """Synchronized with weapon_cheat_shared.c and each engine's weapon.c tables."""
    count = 5 if game == "d1" else 10
    primary = [0] * count
    primary[1] = 1568 if grant == "d2_honestbob" else 784
    secondary = [20, 10, 10] + [0] * (count - 3)
    if grant == "d1_bigred":
        secondary[3:5] = [5, 5]
    if grant == "d2_honestbob":
        primary[6] = 1568
        secondary = [20, 10, 10, 5, 0, 20, 20, 15, 0, 0]
    return {
        "primary_flags": 0xFCCF if grant == "d2_honestbob" else 0xFF if grant == "d1_bigred" else 0xE7,
        "secondary_flags": 0xFCEF if grant == "d2_honestbob" else 0xFF if grant == "d1_bigred" else 0xE7,
        "primary_ammo": primary,
        "secondary_ammo": secondary,
        "energy": 200 * 65536,
        "laser_level": 5 if grant == "d2_honestbob" else 3,
        "cheats_enabled": 1,
        "score": 0,
    }


def grant_operations(rows):
    return [
        (index, operation)
        for index, row in enumerate(rows[1:-1], 1)
        for operation in row["dispatch"]["ops"]
        if operation["k"] == "command" and operation["event"]["command"] == "grant_all_weapons"
    ]


def verify_capture(directory, case, revision, stamp):
    name, game, profile, grant = case
    raw = directory / "capture.dximdemo"
    report_path = directory / "capture.json"
    require(
        raw.stat().st_size <= MAX_RAW_BYTES and report_path.stat().st_size <= MAX_REPORT_BYTES,
        "Live evidence exceeds its byte budget",
    )
    rows = [compact._json(line) for line in raw.read_bytes().splitlines() if line.strip()]
    require(len(rows) == 43, "Capture must contain forty frames, input tail and result")
    for row in rows:
        compact._no_save(row)
    header = rows[0]
    mission = "descent" if name == "imported-d1" else compact.PUBLIC_MISSIONS[game]
    require(
        all(
            header.get(key) == value
            for key, value in {
                "type": "header",
                "version": 5,
                "game": game,
                "mission": mission,
                "start_mode": "new_level",
                "level": 1,
                "frame_count": 40,
                "rng_mode": "lcg_state",
            }.items()
        ),
        "Capture is not the exact ordinary new-level scenario",
    )
    clean_stamp(header.get("git_version", ""), revision)
    require(header["git_version"] == stamp, "Live recorder stamp differs from the verified executable")
    require(rows[-1].get("type") == "result", "Live capture is missing its terminal result")
    parity.validate_identity(rows[-1]["result"], header, game)
    batches = ordered.validate(header, rows[1:-2], rows[-2])
    grants = grant_operations(rows)
    require(
        [rows[index]["f"] for index, _ in grants] == [2, 2, 34, 40]
        and all(op["event"]["grant"] == grant for _, op in grants),
        "Recording lost repeated or later ordered grants",
    )
    enables = [
        (index, op)
        for index, row in enumerate(rows[1:-1], 1)
        for op in row["dispatch"]["ops"]
        if op["k"] == "command" and op["event"]["command"] == "enable_cheats"
    ]
    require(len(enables) == (1 if game == "d1" else 0), "Native cheat enable command was lost or invented")
    ops = batches[2]["ops"]
    positions = [
        index for index, op in enumerate(ops) if op["k"] == "command" and op["event"]["command"] == "grant_all_weapons"
    ]
    require(
        any(op["k"] in ("sample", "controls") and op["p"].get("f2", 0) for op in ops[positions[0] + 1 : positions[1]]),
        "Missing native secondary fire between same-batch grants",
    )
    if enables:
        require(
            enables[0][0] == 3 and ops.index(enables[0][1]) < positions[0], "D1 enable must precede the first grant"
        )
    report = compact._json(report_path.read_bytes())
    expected_identity = {
        "game": game,
        "mission": mission,
        "scenario": "all-weapons-bigred" if grant == "d1_bigred" else "all-weapons",
        "profile": profile,
        "grant": GRANT_CODES[grant],
        "native_code": "bigred" if grant == "d1_bigred" else "scourge" if game == "d1" else "honestbob",
        "frames": 40,
        "post_capture_policy_checks": "pass",
        "retail_assets_playback": False,
        "android_device_run": False,
    }
    require(
        all(type(report.get(key)) is type(value) and report[key] == value for key, value in expected_identity.items()),
        "Wrong live capture identity or scope declaration",
    )
    observations = report.get("observations", [])
    require(
        [row.get("phase") for row in observations] == list(PHASES)
        and [row.get("frame") for row in observations] == [2, 2, 2, 34, 34, 40],
        "Missing ordered inventory phases",
    )
    for state in [report["initial"], report["terminal"], *(row["inventory"] for row in observations)]:
        inventory(state, game)
    initial = report["initial"]
    require(
        all(
            initial[key] == value
            for key, value in {
                "primary_flags": 1,
                "secondary_flags": 1,
                "energy": 100 * 65536,
                "laser_level": 0,
                "cheats_enabled": 0,
                "primary_selected": 0,
                "secondary_selected": 0,
            }.items()
        ),
        "Initial inventory was granted or altered before recording",
    )
    require(
        initial["primary_ammo"] == [0] * len(initial["primary_ammo"])
        and initial["secondary_ammo"] == [5] + [0] * (len(initial["secondary_ammo"]) - 1),
        "Initial ammo is not the ordinary new-level loadout",
    )
    states = {row["phase"]: row["inventory"] for row in observations}
    expected = expected_grant(game, grant)
    for phase in ("first_grant", "repeated_grant", "later_grant", "tail_grant"):
        require(
            all(states[phase][key] == value for key, value in expected.items()) and states[phase]["flags"] & 1024,
            "Grant inventory differs from its faithful native mask/ammo/laser policy",
        )
    between = copy.deepcopy(states["first_grant"])
    between["secondary_ammo"][0] -= 1
    require(
        states["between_grants"] == between, "Same-batch fire did not spend exactly one missile before replenishment"
    )
    require(
        states["spent_inventory"]["energy"] < expected["energy"]
        or (grant == "d2_honestbob" and states["spent_inventory"]["primary_ammo"][1] < expected["primary_ammo"][1]),
        "Later grant did not replenish spent inventory",
    )
    shots = report.get("shots")
    require(isinstance(shots, list) and 0 < len(shots) <= 1000, "Missing bounded live projectile signatures")
    signatures = set()
    for shot in shots:
        require(
            isinstance(shot, dict) and set(shot) == {"frame", "object", "signature", "weapon_id", "selected"},
            "Malformed live projectile observation",
        )
        for key, value in shot.items():
            integer(value, "shot." + key)
        require(
            shot["frame"] < 40 and shot["object"] < 1000 and shot["signature"] not in signatures,
            "Invalid or reused observed projectile signature",
        )
        signatures.add(shot["signature"])
    # D1 uses the slot-12 firing table but creates projectile 20; D2 creates 12
    spread = [shot for shot in shots if shot["weapon_id"] == (20 if profile == "d1" else 12) and shot["selected"] == 2]
    gauss = [shot for shot in shots if shot["weapon_id"] == 32 and shot["selected"] == 6]
    require(
        spread and (bool(gauss) == (profile == "d2")), "Missing actual granted Spreadfire/Gauss projectile identities"
    )
    terminal = rows[-1]["result"]["player0"]
    require(
        terminal["energy"] == report["terminal"]["energy"] // 65536,
        "Terminal integer energy differs from live fixed-point inventory",
    )
    for key in ("laser_level", "flags", "score"):
        require(terminal[key] == report["terminal"][key], "Terminal inventory differs from the live result")
    for key in ("primary_ammo", "secondary_ammo"):
        require(
            terminal[key][: len(report["terminal"][key])] == report["terminal"][key]
            and not any(terminal[key][len(report["terminal"][key]) :]),
            "Terminal ammo differs from the live result",
        )
    require((directory / "capture.dximdemo.rngtrace.jsonl").is_file(), "Missing live SIM RNG evidence")
    return rows, {
        "initial": initial,
        "observations": observations,
        "terminal": report["terminal"],
        "spreadfire": spread,
        "gauss": gauss,
        "all_projectiles": shots,
        "ordered_grant_frames": [2, 2, 34, 40],
        "post_capture_policy_checks": "pass",
    }


def mutate(rows, mutation):
    """Change only declared executable input; preserve live terminal and all RNG."""
    changed = copy.deepcopy(rows)
    grants = grant_operations(changed)
    require(len(grants) == 4, "Negative control requires the exact four live grants")
    if mutation == "missing-final-grant":
        index, operation = grants[-1]
        require(
            changed[index]["type"] == "input_tail" and changed[index]["f"] == 40,
            "Final grant moved from its native input tail",
        )
        changed[index]["dispatch"]["ops"].remove(operation)
    else:
        for _, operation in grants:
            current = operation["event"]["grant"]
            if mutation == "wrong-profile":
                replacement = "d1_scourge" if current == "d2_honestbob" else "d2_honestbob"
            else:
                require(
                    mutation == "wrong-variant" and current in ordered.D1_ALL_WEAPONS_GRANTS, "Unknown negative variant"
                )
                replacement = "d1_bigred" if current == "d1_scourge" else "d1_scourge"
            operation["event"]["grant"] = replacement
    require(
        changed[0] == rows[0] and changed[-1] == rows[-1],
        "Negative must preserve original identity and terminal expectation",
    )
    return changed


def verify_negative(directory, demo, oracle, mutation, game, status):
    """Compare negative outputs to the original oracle, not the mutated input hash."""
    require(status != 0, "Native runner accepted a negative all-weapons control")
    failures = list(directory.glob("result.json.failure_*/failure.json"))
    require(len(failures) == 1, "Negative lacks its native failure archive")
    failure = compact._json(failures[0].read_bytes())
    require(
        failure.get("forced_stop") is False and type(failure.get("native_exit_code")) is int,
        "Negative must exit naturally",
    )
    if mutation == "wrong-profile":
        logs = "\n".join(path.read_text(errors="replace") for path in failures[0].parent.glob("*.txt"))
        reason = (
            "unsupported all-weapons grant for fixture game"
            if game == "d1"
            else "all-weapons grant differs from the active gameplay profile"
        )
        require(reason in logs, "Wrong-profile control failed without its precise native grant rejection")
        return {
            "status": "pass",
            "native_exit_code": failure["native_exit_code"],
            "evidence": "native grant/profile rejection",
        }
    state, rng, result = (directory / name for name in ("state.jsonl.gz", "rng.jsonl", "result.json"))
    require(
        all(path.is_file() for path in (state, rng, result)),
        "Semantic negative must retain complete frame/RNG/terminal outputs",
    )
    header, expected = compact.validate(demo, oracle)
    hashes = compact._frame_hashes(state, header)
    actual = compact._json(result.read_bytes())
    parity.validate_identity(actual, header, game)
    require(
        compact.sha(actual) != expected["terminal_sha256"], "Negative failed without changing live terminal evidence"
    )
    if mutation == "missing-final-grant":
        require(hashes == expected["frame_sha256"], "Missing tail grant must preserve every live simulation frame")
        baseline = list(parity.records(demo))[-1]["result"]["player0"]
        player = actual["player0"]
        require(
            player["energy"] < baseline["energy"] or player["primary_ammo"][1] < baseline["primary_ammo"][1],
            "Missing final grant did not preserve the spent resources",
        )
    try:
        compact.verify(game, demo, oracle, state, rng, result)
    except parity.EvidenceError as error:
        return {
            "status": "pass",
            "native_exit_code": failure["native_exit_code"],
            "first_different_frame": next(
                (index for index, pair in enumerate(zip(hashes, expected["frame_sha256"])) if pair[0] != pair[1]), None
            ),
            "original_oracle_rejection": str(error),
        }
    raise parity.EvidenceError("Negative unexpectedly matched the immutable live oracle")


def write_report(output, report):
    (output / "report.json").write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")


def run(args):
    require(0 < args.timeout <= 600, "Per-process timeout must be 1..600 seconds")
    require(math.isfinite(args.minimum_free_gb) and args.minimum_free_gb >= 0, "Invalid minimum free space")
    source = source_identity(args.repo, args.revision)
    builds = {}
    for game in ("d1", "d2"):
        validate_assets(game, getattr(args, "data_" + game))
        builds[game] = build_identity(
            args.repo,
            getattr(args, "build_" + game),
            game,
            args.revision,
            {role: getattr(args, role + "_" + game) for role in ("capture", "graphics", "headless")},
        )
    # Never reuse an output directory or replace previously sealed companions
    args.output.mkdir(parents=True, exist_ok=False)
    report = {
        "schema": 1,
        "status": "in_progress",
        "complete": False,
        "source_revision": args.revision,
        "source_recipe_sha256": source,
        "builds": builds,
        "reviewed_scope_statement": SCOPE,
        "retail_assets_playback": False,
        "android_device_run": False,
        "published": False,
        "cases": [],
        "runs": [],
    }
    write_report(args.output, report)
    try:
        for case in CASES:
            name, game, _, grant = case
            directory = args.output / (name + "-capture")
            directory.mkdir()
            data = args.data_d1 if name == "imported-d1" else getattr(args, "data_" + game)
            command = (
                [str(getattr(args, "capture_" + game)), "--public-level-replay", str(data), "all-weapons-bigred"]
                if grant == "d1_bigred"
                else [str(getattr(args, "capture_" + game)), "--public-all-weapons", str(data)]
            )
            if name == "imported-d1":
                command.append("--imported-d1")
            environment = {
                key: value for key, value in isolated_native_env(directory).items() if not key.startswith("DXX_PUBLIC_")
            }
            with (directory / "capture.log").open("w") as log:
                subprocess.run(
                    command,
                    cwd=directory,
                    env=environment,
                    stdout=log,
                    stderr=subprocess.STDOUT,
                    check=True,
                    timeout=args.timeout,
                )
            _, evidence = verify_capture(directory, case, args.revision, builds[game]["stamp"])
            entry = {
                "case": name,
                "game": game,
                "capture": str(directory.relative_to(args.output)),
                "scope": "live-diagnostic-only" if name == "imported-d1" else "native-compact-candidate",
                "evidence": evidence,
            }
            report["cases"].append(entry)
            if name == "imported-d1":
                write_report(args.output, report)
                continue
            demo, oracle = (args.output / (name + suffix) for suffix in (".dximdemo", ".oracle.json"))
            compact.pack(
                directory / "capture.dximdemo",
                directory / "capture.dximdemo.rngtrace.jsonl",
                demo,
                oracle,
                {
                    "source_revision": args.revision,
                    "capture_executable_sha256": builds[game]["executables"]["capture"]["sha256"],
                    "capture_source_sha256": source[FIXTURE],
                    "asset_sha256": compact.pinned_assets(game),
                },
            )
            compact.validate(demo, oracle)
            immutable = {
                path: digest(path)
                for path in (
                    demo,
                    oracle,
                    directory / "capture.dximdemo",
                    directory / "capture.dximdemo.rngtrace.jsonl",
                    directory / "capture.json",
                )
            }
            entry.update(
                demo=demo.name, oracle=oracle.name, demo_bytes=demo.stat().st_size, oracle_bytes=oracle.stat().st_size
            )
            rows = list(parity.records(demo))
            for mode in ("headless", "graphics"):
                destination = args.output / f"{name}-{mode}"
                require(replay(args, game, mode, demo, destination) == 0, "Native all-weapons replay failed")
                verdict = compact.verify(
                    game,
                    demo,
                    oracle,
                    destination / "state.jsonl.gz",
                    destination / "rng.jsonl",
                    destination / "result.json",
                )
                report["runs"].append({"case": name, "backend": mode, "control": "original", **verdict})
                write_report(args.output, report)
                mutations = (
                    ("wrong-profile", "wrong-variant", "missing-final-grant")
                    if game == "d1"
                    else ("wrong-profile", "missing-final-grant")
                )
                for mutation in mutations:
                    negative = args.output / f"{name}-{mode}-{mutation}.dximdemo"
                    negative.write_bytes(b"".join(compact.canonical(row) + b"\n" for row in mutate(rows, mutation)))
                    target = negative.with_suffix("")
                    status = replay(args, game, mode, negative, target)
                    verdict = verify_negative(target, demo, oracle, mutation, game, status)
                    require(
                        all(digest(path) == value for path, value in immutable.items()),
                        "A live capture or sealed oracle changed during qualification",
                    )
                    report["runs"].append(
                        {
                            "case": name,
                            "backend": mode,
                            "control": mutation,
                            "negative_input_sha256": digest(negative),
                            "oracle_sha256": digest(oracle),
                            "oracle_resealed": False,
                            **verdict,
                        }
                    )
                    write_report(args.output, report)
        require(source_identity(args.repo, args.revision) == source, "Source changed during qualification")
        for game in ("d1", "d2"):
            validate_assets(game, getattr(args, "data_" + game))
            require(
                build_identity(
                    args.repo,
                    getattr(args, "build_" + game),
                    game,
                    args.revision,
                    {role: getattr(args, role + "_" + game) for role in ("capture", "graphics", "headless")},
                )
                == builds[game],
                "Build or executable changed during qualification",
            )
        report.update(status="pass", complete=True)
    except Exception as error:
        report.update(status="fail", complete=False, error=str(error))
        raise
    finally:
        report["artifacts"] = {
            str(path.relative_to(args.output)): {"bytes": path.stat().st_size, "sha256": digest(path)}
            for path in sorted(args.output.rglob("*"))
            if path.is_file() and path.name != "report.json"
        }
        write_report(args.output, report)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=lambda value: Path(value).resolve(), default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=lambda value: Path(value).resolve(), required=True)
    parser.add_argument("--revision", required=True, help="Exact clean source commit, 40 lowercase hex characters")
    for game in ("d1", "d2"):
        for role in ("build", "capture", "graphics", "headless", "data"):
            parser.add_argument(f"--{role}-{game}", type=lambda value: Path(value).resolve(), required=True)
    parser.add_argument("--pwsh", default="pwsh")
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--minimum-free-gb", type=float, default=4)
    try:
        result = run(parser.parse_args())
    except (parity.EvidenceError, OSError, subprocess.SubprocessError, ValueError, KeyError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    print(f"PASS: {len(result['cases'])} live cases, {len(result['runs'])} native replay controls; candidates only")
    return 0


if __name__ == "__main__":
    sys.exit(main())
