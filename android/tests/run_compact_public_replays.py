"""Run compact public scenes twice through the existing headless or visual replay backend."""

import argparse
import hashlib
import os
import shutil
import subprocess
from pathlib import Path

import compact_public_replay as compact
import compact_public_gameplay_parity as gameplay
import d1_replay_parity as parity
import public_switch_replay as shoot_switch
import public_robot_combat_replay as robot_combat
import public_door_replay as door_lifecycle
import public_loot_replay as robot_loot

SCENARIOS = ("flight", "pickup", "door", "combat")
PUBLIC_CASES = {(game, scenario): 1 for game in compact.PUBLIC_MISSIONS for scenario in SCENARIOS}
PUBLIC_CASES[("d2", "shoot-switch")] = 2
PUBLIC_CASES[("d1", "door-lifecycle")] = 3
PUBLIC_CASES[("d2", "door-lifecycle")] = 1
PUBLIC_CASES[("d1", "loot")] = 1


def load_corpus(directory):
    manifest = compact._json((directory / "manifest.json").read_bytes())
    if not isinstance(manifest, dict) or manifest.get("schema") != 1 or not isinstance(manifest.get("cases"), list):
        raise parity.EvidenceError("Invalid public level replay manifest")
    cases = manifest["cases"]
    imported = manifest.get("imported_d1", {})
    if (
        not isinstance(imported, dict)
        or set(imported) != {"schema", "cases"}
        or type(imported.get("schema")) is not int
        or imported.get("schema") != 1
        or not isinstance(imported.get("cases"), dict)
        or set(imported["cases"]) != {case.get("demo") for case in cases if case.get("game") == "d1"}
    ):
        raise parity.EvidenceError("Every D1 recording requires explicit imported scope")
    cases = [
        {**case, "imported_d1": imported["cases"][case["demo"]]} if case.get("game") == "d1" else case for case in cases
    ]
    imported_d1_scope(cases)
    expected = set(PUBLIC_CASES)
    if len(cases) != len(expected) or {(row.get("game"), row.get("scenario")) for row in cases} != expected:
        raise parity.EvidenceError(f"Public corpus requires its exact {len(expected)} unique game/scenario pairs")
    filenames = set()
    for case in cases:
        if not isinstance(case.get("actions"), dict):
            raise parity.EvidenceError("Missing declared gameplay actions")
        for key in ("demo", "oracle"):
            name = case.get(key)
            if (
                not isinstance(name, str)
                or not name
                or name in (".", "..")
                or "/" in name
                or "\\" in name
                or ":" in name
                or Path(name).name != name
                or name in filenames
            ):
                raise parity.EvidenceError("Corpus companions must have unique plain filenames")
            filenames.add(name)
        header, oracle = compact.validate(directory / case["demo"], directory / case["oracle"])
        if ("combat_observations" in oracle) != (case["scenario"] in ("combat", "loot")):
            raise parity.EvidenceError("Combat cases require their immutable live damage/loot observation oracle")
        if ("door_observations" in oracle) != (case["scenario"] == "door-lifecycle"):
            raise parity.EvidenceError("Door lifecycle cases require immutable live geometric observation oracles")
        if header["game"] != case["game"] or header["level"] != PUBLIC_CASES[(case["game"], case["scenario"])]:
            raise parity.EvidenceError("Manifest game or authored scenario level disagrees with its recording")
    return cases


def imported_d1_scope(cases):
    included, excluded, required = [], [], []
    for case in cases:
        if case.get("game") != "d1":
            continue
        declaration = case.get("imported_d1", {})
        if (
            not isinstance(declaration, dict)
            or set(declaration) != {"role", "reason"}
            or declaration["role"] not in ("required-gameplay", "diagnostic-only", "excluded")
            or not isinstance(declaration["reason"], str)
            or not declaration["reason"].strip()
        ):
            raise parity.EvidenceError("Invalid explicit imported D1 case scope")
        role = declaration["role"]
        if role == "excluded":
            excluded.append({"demo": case["demo"], "reason": declaration["reason"]})
        else:
            included.append(case)
        if role == "required-gameplay":
            required.append(case)
        if (case.get("scenario") == "flight") != (role == "required-gameplay"):
            raise parity.EvidenceError("Imported D1 flight must retain its required gameplay scope")
    if len(required) != 1:
        raise parity.EvidenceError("Paired scope requires exactly one D1 flight case")
    return included, excluded


def validate_assets(game, directory):
    expected = compact.pinned_assets(game)
    files = list(directory.iterdir())
    names = {path.name.lower(): path for path in files if path.is_file()}
    if len(names) != len(files) or set(names) - (set(expected) | {".extraction-complete.json"}):
        raise parity.EvidenceError("Ambiguous or unpinned public data directory")
    for name, digest in expected.items():
        if name not in names or parity.digest(names[name]) != digest:
            raise parity.EvidenceError(f"Public data digest differs: {game}/{name}")


def _position(state):
    return tuple(state["position"][axis] for axis in ("x", "y", "z"))


def _distance_squared(a, b):
    return sum((x - y) ** 2 for x, y in zip(a, b))


def verify_pickup_target(actions, state_trace, header, game):
    for key in ("target", "target_id", "target_signature"):
        if type(actions.get(key)) is not int or actions[key] < 0:
            raise parity.EvidenceError("Pickup target requires its original slot, powerup ID and signature")
    slot = str(actions["target"])

    def matches(value):
        # OBJ_POWERUP is the same engine object type in D1 and D2
        return (
            value is not None
            and value["type"] == 7
            and value["id"] == actions["target_id"]
            and value["signature"] == actions["target_signature"]
        )

    present = [matches(row["objects"].get(slot)) for row in parity.object_states(state_trace, header, game)]
    retirement = actions["action_frame"]
    if retirement + 1 == len(present):
        terminal = None
        for row in parity.world_states(state_trace, header, game):
            if row["type"] == "object_boundary" and row["phase"] == "terminal":
                terminal = matches(row["slots"].get(slot))
        if terminal is None:
            raise parity.EvidenceError("Missing terminal pickup object boundary")
        present.append(terminal)
    if not present[0] or not present[retirement] or present[retirement + 1]:
        raise parity.EvidenceError("Selected original pickup did not retire at its exact declared boundary")


def verify_actions(case, demo, state_trace, actual_result):
    header = next(parity.records(demo))
    frames = list(parity.frames(state_trace, header, case["game"]))
    terminal = compact._json(Path(actual_result).read_bytes())
    states = [frame["state"] for frame in frames] + [terminal]
    positions = [_position(state) for state in states]
    if max(_distance_squared(positions[0], point) for point in positions) <= 65536**2:
        raise parity.EvidenceError("Declared public action contains no movement beyond one world unit")
    player = [state["player0"] for state in states]
    energy_drop = any(b["energy"] < a["energy"] for a, b in zip(player, player[1:]))
    weapons = max(frame["diag"]["player_weapon_count"] for frame in frames)
    scenario, actions = case["scenario"], case["actions"]
    action_frame = actions.get("action_frame")
    if action_frame is not None and (type(action_frame) is not int or not -1 <= action_frame < len(frames)):
        raise parity.EvidenceError("Invalid declared action frame")
    # Prove the identified kill before the generic firing guard, so no-fire combat
    # controls exercise causal robot outcomes rather than just weapon presence
    combat_actions = robot_combat.verify(actions, header, states, state_trace) if scenario == "combat" else None
    loot_actions = robot_loot.verify(actions, header, states, state_trace) if scenario == "loot" else None
    if scenario in ("flight", "door", "shoot-switch", "combat", "loot") and (weapons <= 0 or not energy_drop):
        raise parity.EvidenceError("Replay did not create live player weapons and spend energy")
    result = {
        "movement": True,
        "max_player_weapons": weapons,
        "energy_fell": energy_drop,
    }
    if scenario == "door-lifecycle":
        result.update(door_lifecycle.verify(actions, header, states, state_trace))
        return result
    if scenario == "loot":
        result.update(loot_actions)
        return result
    if scenario == "shoot-switch":
        result.update(shoot_switch.verify(actions, header, states, state_trace))
        return result
    if scenario == "combat":
        result.update(combat_actions)
        return result
    if scenario == "pickup":
        gain_frame = actions.get("pickup_gain_frame")
        if (
            type(gain_frame) is not int
            or not 0 <= gain_frame < len(frames)
            or action_frame is None
            or gain_frame > action_frame
        ):
            raise parity.EvidenceError("Pickup requires a valid gain frame no later than object retirement")
        gains = [
            index
            for index, (a, b) in enumerate(zip(player, player[1:]))
            if b["energy"] > a["energy"] or b["shields"] > a["shields"]
        ]
        if gain_frame not in gains:
            raise parity.EvidenceError("No energy/shield pickup gain at the declared post-frame boundary")
        verify_pickup_target(actions, state_trace, header, case["game"])
        result["pickup_gain_frames"] = gains
        result["pickup_target_retired_frame"] = action_frame
        return result
    target = actions.get("target")
    if scenario == "door" and (type(target) is not int or target < 0):
        raise parity.EvidenceError("Missing declared door wall index")
    worlds, terminal_world = {}, None
    initial_door = None
    for row in parity.world_states(state_trace, header, case["game"]):
        if row["type"] == "object_boundary":
            continue
        state = row["state"]
        observation = {"alive": state["death"]["active"] == 0}
        if scenario == "door":
            if target >= len(state["walls"]):
                raise parity.EvidenceError("Declared target wall is absent from replay")
            observation["door"] = state["walls"][target]["state"]
        if row["type"] == "world_state":
            worlds[row["f"]] = observation
        elif row["phase"] == "terminal":
            terminal_world = observation
        elif scenario == "door":
            initial_door = observation["door"]
    if len(worlds) != len(frames) or terminal_world is None:
        raise parity.EvidenceError("Missing complete action world evidence")
    worlds[len(frames)] = terminal_world
    if scenario == "door":
        openings = [index for index in range(1, len(frames) + 1) if worlds[index]["door"] in (1, 2)]
        if (
            initial_door != 0
            or worlds[0]["door"] != 0
            or not openings
            or (action_frame is not None and action_frame + 1 not in openings)
        ):
            raise parity.EvidenceError("Initially closed target door did not open at the declared post-frame boundary")
        result["door_open_entry_frames"] = openings
    else:
        held_forward = 0
        contacts = 0
        controls = [row for row in parity.records(demo) if row.get("type") == "frame"]
        for index, frame in enumerate(controls):
            held_forward = frame["input"].get("s", {}).get("f", held_forward)
            if (
                abs(held_forward) > 0
                and worlds[index]["alive"]
                and worlds[index + 1]["alive"]
                and player[index]["shields"] >= 0
                and player[index + 1]["shields"] >= 0
                and _distance_squared(positions[index], positions[index + 1]) < 655**2
            ):
                contacts += 1
        if contacts < 10 or type(actions.get("wall_contact_frames")) is not int or actions["wall_contact_frames"] < 10:
            raise parity.EvidenceError("Insufficient live, thrusting wall-contact frames")
        result["wall_contact_frames"] = contacts
    return result


def check_paired_report(report, exit_code, cases, corpus, report_path=None):
    if (
        exit_code not in (1, 2)
        or report.get("qualification") != "incomplete"
        or report.get("schema") != 1
        or "asset_error" in report
        or "harness_error" in report
    ):
        raise parity.EvidenceError("Strict paired public D1 comparison failed or changed its qualification boundary")
    included, excluded = imported_d1_scope(cases)
    expected = {case["demo"]: case for case in included}
    paired_required = [name for name, case in expected.items() if case.get("scenario") == "flight"]
    if len(paired_required) != 1:
        raise parity.EvidenceError("Paired scope requires exactly one D1 flight case")
    rows = report.get("cases", [])
    if len(rows) != len(expected) or {Path(row.get("demo", "")).name for row in rows} != set(expected):
        raise parity.EvidenceError("Paired D1 cases are incomplete or duplicated")
    required = {
        "terminal_result",
        "checkpoint_collision_clock",
        "frames",
        "simulation_rng",
        "object_states",
        "world_states",
    }
    scoped = {
        "schema": 1,
        "status": "pending",
        "qualification": "incomplete",
        "scope": "exact gameplay-state/SIM-RNG imported D1 flight parity",
        "required_paired_cases": paired_required,
        "unqualified_paired_cases": sorted(set(expected) - set(paired_required)),
        "excluded_imported_cases": excluded,
        "not_qualified": list(gameplay.NOT_QUALIFIED),
        "original_report": str(report_path),
        "original_exit_code": exit_code,
        "cases": [],
    }
    original_failure = False
    for case in rows:
        if "error" in case or "capture_errors" in case:
            raise parity.EvidenceError("Paired D1 capture contains errors")
        is_required = Path(case["demo"]).name in paired_required
        frame_count = next(parity.records(corpus / Path(case["demo"]).name))["frame_count"]
        recording = case.get("recording_to_native", {})
        recorded_checks = recording.get("checks", {})
        if (
            recording.get("status") != "incomplete"
            or set(recorded_checks) != required - {"object_states", "world_states"}
            or recorded_checks["terminal_result"].get("status") != "pass"
            or recorded_checks["checkpoint_collision_clock"].get("status") != "pass"
            or any(recorded_checks[key].get("status") != "incomplete" for key in ("frames", "simulation_rng"))
        ):
            raise parity.EvidenceError("Unexpected compact recording-to-native paired result")
        for name in ("native_repeatability", "native_to_imported"):
            relationship = case.get(name, {})
            checks = relationship.get("checks", {})
            if set(checks) != required:
                raise parity.EvidenceError(f"Paired {name} comparison inventory is incomplete")
            if name == "native_repeatability" or report_path is None:
                if relationship.get("status") != "pass" or any(row.get("status") != "pass" for row in checks.values()):
                    raise parity.EvidenceError(f"Strict paired {name} comparison is not passing")
            else:
                if any(row.get("status") not in ("pass", "fail") or "error" in row for row in checks.values()):
                    raise parity.EvidenceError("Imported evidence is missing or malformed")
                if checks["checkpoint_collision_clock"].get("status") != "pass" or (
                    is_required
                    and any(checks[key].get("status") != "pass" for key in ("terminal_result", "simulation_rng"))
                ):
                    raise parity.EvidenceError("Required imported gameplay checks failed")
                failed = any(row["status"] == "fail" for row in checks.values())
                original_failure |= failed
                if relationship.get("status") != ("fail" if failed else "pass"):
                    raise parity.EvidenceError("Original imported relationship status is inconsistent")
            if (
                checks["frames"].get("frames_compared") != frame_count
                or checks["simulation_rng"].get("simulation_events_compared", 0) <= 0
            ):
                raise parity.EvidenceError("Paired comparison lacks complete frame/SIM evidence")
        if report_path is not None:
            try:
                comparison = gameplay.compare_case(report_path, case)
            except (OSError, ValueError, KeyError, TypeError) as error:
                comparison = {"status": "fail", "error": str(error)}
            scoped["cases"].append(
                {
                    "demo": case["demo"],
                    "required": is_required,
                    "scope_role": "required flight comparison" if is_required else "unqualified diagnostic-only",
                    "comparison": comparison,
                }
            )
            parity.write_json(Path(report_path).with_name("scoped-gameplay-report.json"), scoped)
            comparison_checks = comparison.get("checks", {})
            malformed = (
                "error" in comparison
                or set(comparison_checks) != {"frames", "terminal_result", "simulation_rng"}
                or any(
                    row.get("status") not in ("pass", "fail") or "error" in row for row in comparison_checks.values()
                )
                or comparison.get("frames_compared") != frame_count
                or comparison_checks.get("simulation_rng", {}).get("simulation_events_compared", 0) <= 0
                or comparison.get("status")
                != ("fail" if any(row.get("status") == "fail" for row in comparison_checks.values()) else "pass")
            )
            if malformed or (is_required and comparison.get("status") != "pass"):
                scoped["status"] = "fail"
                parity.write_json(Path(report_path).with_name("scoped-gameplay-report.json"), scoped)
                raise parity.EvidenceError("Independent exact gameplay-state/SIM-RNG comparison failed")
    if exit_code != (1 if original_failure else 2):
        raise parity.EvidenceError("Original paired exit code disagrees with preserved comparison statuses")
    scoped["status"] = "pass"
    if report_path is not None:
        parity.write_json(Path(report_path).with_name("scoped-gameplay-report.json"), scoped)
    return scoped


def _run(command, log, timeout, cwd):
    with log.open("w", encoding="utf-8") as stream:
        return subprocess.run(
            command, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT, timeout=timeout, check=False
        ).returncode


def run(args):
    cases = load_corpus(args.corpus)
    for game in compact.PUBLIC_MISSIONS:
        validate_assets(game, getattr(args, f"data_{game}"))
        if not getattr(args, f"exe_{game}").is_file():
            raise parity.EvidenceError(f"Missing {game} executable")
    args.output.mkdir(parents=True, exist_ok=False)
    with (args.output / "producer.lock").open("w", encoding="ascii") as lease:
        if os.name == "posix":
            import fcntl

            fcntl.flock(lease.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        return _run_cases(args, cases)


def _run_cases(args, cases):
    sandbox_token = hashlib.sha256(str(args.output).encode("utf-8")).hexdigest()[:16]
    report = {
        "schema": 1,
        "qualification": "incomplete",
        "backend": args.run_mode,
        "native_only": args.native_only,
        "case_count": len(cases),
        "expected_native_runs": 2 * len(cases),
        "native_runs": [],
        "paired": {"status": "pending"},
    }
    for case in cases:
        demo, oracle = (args.corpus / case[key] for key in ("demo", "oracle"))
        for repeat in range(2):
            directory = args.output / f"{case['game']}-{case['scenario']}-{repeat}"
            directory.mkdir()
            state, rng, actual = (directory / name for name in ("state.jsonl.gz", "rng.jsonl", "result.json"))
            item = {
                "game": case["game"],
                "scenario": case["scenario"],
                "repeat": repeat,
                "backend": args.run_mode,
                "status": "fail",
                "artifacts": {"state": str(state), "rng": str(rng), "result": str(actual)},
            }
            command = [
                args.pwsh,
                "-NoProfile",
                "-File",
                str(args.repo / "android/tests/run_input_demo_replay.ps1"),
                "-DataEdition",
                "shareware",
                "-ExecutablePath",
                str(getattr(args, f"exe_{case['game']}")),
                "-Game",
                case["game"],
                "-DataDir",
                str(getattr(args, f"data_{case['game']}")),
                "-DemoPath",
                str(demo),
                "-Runner",
                "headless-console" if args.run_mode == "headless" else "visual",
                "-Mode",
                "accelerated",
                "-StrictComparison",
                "-RequireCleanExit",
                "-StateLogPath",
                str(state),
                "-RngLogPath",
                str(rng),
                "-ResultCopyPath",
                str(actual),
                "-TimeoutSeconds",
                str(args.timeout),
                "-MinimumFreeSpaceGB",
                str(args.minimum_free_gb),
                "-SandboxSuffix",
                args.run_mode + "-" + sandbox_token + "-" + directory.name,
            ]
            item["command"] = command
            try:
                item["exit_code"] = _run(command, directory / "runner.log", args.timeout + 120, args.repo)
                if item["exit_code"]:
                    raise parity.EvidenceError("Replay runner failed; inspect runner.log")
                item["fidelity"] = compact.verify(case["game"], demo, oracle, state, rng, actual)
                item["actions"] = verify_actions(case, demo, state, actual)
                item["status"] = "pass"
            except (OSError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
                item["error"] = str(error)
            report["native_runs"].append(item)
            parity.write_json(args.output / "report.json", report)
    if args.run_mode == "graphics" and not args.native_only:
        _run_imported(args, cases, report)
    else:
        report["paired"] = {
            "status": "not_run",
            "qualification": "incomplete",
            "reason": "Native-only qualification; imported D1 parity requires the separate full graphics pass",
        }
    report["status"] = (
        "pass"
        if len(report["native_runs"]) == report["expected_native_runs"]
        and all(row["status"] == "pass" for row in report["native_runs"])
        and (args.run_mode == "headless" or args.native_only or report["paired"]["status"] == "pass")
        else "fail"
    )
    parity.write_json(args.output / "report.json", report)
    print(
        f"{report['status'].upper()}: compact public replay report {args.output / 'report.json'}; full simulation qualification remains incomplete"
    )
    return 0 if report["status"] == "pass" else 1


def _run_imported(args, cases, report):
    included, excluded = imported_d1_scope(cases)
    report["paired"] = {"status": "pending", "excluded_imported_cases": excluded}
    staged = args.output / "d1-corpus"
    staged.mkdir()
    for case in included:
        shutil.copyfile(args.corpus / case["demo"], staged / case["demo"])
    paired = args.output / "paired-d1"
    command = [
        args.pwsh,
        "-NoProfile",
        "-File",
        str(args.repo / "android/tests/test_d1_replay_parity.ps1"),
        "-DataEdition",
        "shareware",
        "-DataDir",
        str(args.data_d1),
        "-DemoDirectory",
        str(staged),
        "-NativeExe",
        str(args.exe_d1),
        "-ImportedExe",
        str(args.exe_d2),
        "-OutputPath",
        str(paired),
        "-TimeoutSeconds",
        str(args.timeout),
        "-MinimumFreeSpaceGB",
        str(args.minimum_free_gb),
    ]
    try:
        code = _run(
            command,
            args.output / "paired-runner.log",
            3 * len(included) * (args.timeout + 120) + 120,
            args.repo,
        )
        report["paired"].update(status="fail", exit_code=code, original_report=str(paired / "report.json"))
        original = compact._json((paired / "report.json").read_bytes())
        scope = check_paired_report(original, code, cases, args.corpus, paired / "report.json")
        report["paired"].update(
            status="pass",
            scope=scope["scope"],
            qualification="incomplete",
            not_qualified=scope["not_qualified"],
            required_paired_cases=scope["required_paired_cases"],
            unqualified_paired_cases=scope["unqualified_paired_cases"],
            scoped_report=str(paired / "scoped-gameplay-report.json"),
        )
    except (OSError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
        report["paired"].update(status="fail", error=str(error))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repo", "corpus", "output", "data-d1", "data-d2", "exe-d1", "exe-d2"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument(
        "--native-only", action="store_true", help="Omit imported parity and report it explicitly as not run"
    )
    parser.add_argument("--run-mode", choices=("headless", "graphics"), default="headless")
    parser.add_argument("--pwsh", default="pwsh")
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--minimum-free-gb", type=float, default=4)
    args = parser.parse_args()
    if not 1 <= args.timeout <= 3600 or not 0.01 <= args.minimum_free_gb <= 1048576:
        parser.error("Invalid timeout or minimum disk reserve")
    for key, value in vars(args).items():
        if isinstance(value, Path):
            setattr(args, key, value.resolve())
    return run(args)


if __name__ == "__main__":
    raise SystemExit(main())
