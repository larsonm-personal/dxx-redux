"""Capture native input phases, replay both backends, and require causal negatives."""

import argparse
import copy
import json
import os
from pathlib import Path
import subprocess

import compact_public_replay as compact
import d1_replay_parity as parity
import ordered_public_replay as ordered
import public_secondary_replay as secondary
from run_compact_public_replays import validate_assets, verify_actions


def require(condition, message):
    if not condition:
        raise parity.EvidenceError(message)


def isolated_native_env(directory, mode=None):
    # Native Unix PhysFS searches HOME, independently of the working directory
    home = directory / "home"
    home.mkdir()
    env = dict(os.environ)
    env["HOME"] = str(home.resolve())
    if mode == "headless":
        env.pop("DISPLAY", None)
        env.pop("WAYLAND_DISPLAY", None)
        env["SDL_VIDEODRIVER"] = "invalid"
    return env


def verify_dispatch_capture(path):
    rows = list(parity.records(path))
    batches = ordered.validate(rows[0], rows[1:-2], rows[-2])
    require(len(batches) == 9, "Dispatch probe must retain eight frames and input tail")
    require(
        batches[0]["pre_rng"]["c"] == 0 and batches[0]["post_rng"]["c"] > 0,
        "First input must draw SIM RNG before frame zero",
    )
    require(
        batches[0]["ops"][0]["ft"] == 1092 and batches[0]["frame_time"] == 3276,
        "Input timestep must differ from the later simulation timestep",
    )
    require(
        [op["k"] for op in batches[2]["ops"]] == ["clear", "clear"],
        "Held-input and empty native flushes must remain explicit",
    )
    require(
        [op["k"] for op in batches[3]["ops"]] == ["command", "reset", "sample", "reset", "sample", "command"],
        "Commands must retain their positions before and after native dispatch",
    )
    require([op["k"] for op in batches[4]["ops"]] == ["reset"], "Handled input must retain reset-only dispatch")
    require(batches[6]["ops"] == [], "No dispatch must encode an explicit empty batch")
    require(
        batches[-1]["ops"]
        and all(op["ft"] == 1638 for op in batches[-1]["ops"])
        and batches[-1]["post_rng"]["c"] > batches[-1]["pre_rng"]["c"],
        "Terminal input must retain its native timestep and SIM effects",
    )
    return rows


def replay(args, game, mode, demo, directory, *, headless_console_output=1):
    directory.mkdir(parents=True)
    exe = getattr(args, mode + "_" + game)
    cmd = [
        args.pwsh,
        "-NoProfile",
        "-File",
        str(args.repo / "android/tests/run_input_demo_replay.ps1"),
        "-DataEdition",
        "shareware",
        "-ExecutablePath",
        str(exe),
        "-Game",
        game,
        "-DataDir",
        str(getattr(args, "data_" + game)),
        "-DemoPath",
        str(demo),
        "-Runner",
        "headless-console" if mode == "headless" else "visual",
        "-Mode",
        "accelerated",
        "-StrictComparison",
        "-RequireCleanExit",
        "-HeadlessConsoleOutput",
        str(headless_console_output),
        "-StateLogPath",
        str(directory / "state.jsonl.gz"),
        "-RngLogPath",
        str(directory / "rng.jsonl"),
        "-ResultCopyPath",
        str(directory / "result.json"),
        "-TimeoutSeconds",
        str(args.timeout),
        "-MinimumFreeSpaceGB",
        str(args.minimum_free_gb),
        "-SandboxSuffix",
        "ordered-" + game + "-" + mode + "-" + directory.name,
    ]
    env = dict(os.environ)
    inherited_home = directory / "inherited-home"
    profile = inherited_home / ("." + game + "x-redux")
    profile.mkdir(parents=True)
    stale = {
        profile / ".plr": b"stale external pilot must never be opened",
        profile / ".plx": b"stale external preferences",
        profile / "descent.cfg": b"ResolutionX=1024\nResolutionY=768\n",
    }
    for path, value in stale.items():
        path.write_bytes(value)
    env["HOME"] = str(inherited_home.resolve())
    if mode == "headless":
        env.pop("DISPLAY", None)
        env.pop("WAYLAND_DISPLAY", None)
        env["SDL_VIDEODRIVER"] = "invalid"
    with (directory / "runner.log").open("w") as log:
        status = subprocess.run(
            cmd, cwd=args.repo, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=args.timeout + 90
        ).returncode
    require(
        all(path.read_bytes() == value for path, value in stale.items()),
        "Replay modified inherited stale pilot/config files",
    )
    if status:
        archives = list(directory.glob("result.json.failure_*/failure.json"))
        require(len(archives) == 1, "Failed input must retain one complete native failure report")
        failure = json.loads(archives[0].read_text())
        require(
            failure.get("forced_stop") is False and failure.get("native_exit_code") is not None,
            "Negative input must exit naturally, not hang or be killed",
        )
    return status


def verify_secondary_orders(args, game, report):
    for scenario in secondary.SCENARIOS:
        capture = args.output / (game + "-" + scenario + "-capture")
        capture.mkdir()
        with (capture / "capture.log").open("w") as log:
            subprocess.run(
                [
                    str(getattr(args, "capture_" + game)),
                    "--public-level-replay",
                    str(getattr(args, "data_" + game)),
                    scenario,
                ],
                cwd=capture,
                env=isolated_native_env(capture),
                stdout=log,
                stderr=subprocess.STDOUT,
                check=True,
                timeout=args.timeout,
            )
        demo = capture / "capture.dximdemo"
        rows = list(parity.records(demo))
        actions = secondary.capture_actions(json.loads((capture / "capture.json").read_text()))
        case = {"scenario": scenario, "actions": actions}
        for mode in ("headless", "graphics"):
            destination = args.output / (game + "-" + mode + "-" + scenario)
            require(replay(args, game, mode, demo, destination) == 0, "Native SDL order replay failed")
            require(
                compact._frame_hashes(demo, rows[0], recorded=True)
                == compact._frame_hashes(destination / "state.jsonl.gz", rows[0]),
                "Native SDL order full state/RNG/diagnostics differ",
            )
            require(
                parity.compare_rng(capture / "capture.dximdemo.rngtrace.jsonl", destination / "rng.jsonl")["status"]
                == "pass",
                "Native SDL order SIM events differ",
            )
            secondary.verify(
                case,
                demo,
                rows[0],
                list(parity.frames(destination / "state.jsonl.gz", rows[0], game)),
                json.loads((destination / "result.json").read_text()),
            )
            report.append({"game": game, "backend": mode, "case": scenario, "status": "pass"})
            if scenario != "secondary-mouse-first":
                continue
            for mutation in ("swap-dispatch", "delete-dispatch"):
                changed = copy.deepcopy(rows)
                ops = changed[14]["dispatch"]["ops"]
                changed[14]["dispatch"]["ops"] = ops[2:] + ops[:2] if mutation == "swap-dispatch" else ops[2:]
                negative = args.output / (game + "-" + mode + "-" + mutation + ".dximdemo")
                negative.write_bytes(b"".join(compact.canonical(row) + b"\n" for row in changed))
                target = negative.with_suffix("")
                require(replay(args, game, mode, negative, target) != 0, "Changed SDL dispatch was accepted")
                states = list(parity.records(target / "state.jsonl.gz"))
                if mutation == "swap-dispatch":
                    require(
                        any(
                            row.get("type") == "object_boundary" and row.get("phase") == "aborted" and row["f"] == 13
                            for row in states
                        )
                        and not (target / "result.json").exists(),
                        "Swapped dispatch must fail the real residual boundary",
                    )
                elif game == "d1":
                    actual = json.loads((target / "result.json").read_text())
                    require(
                        sum(row.get("type") == "frame_state" for row in states) == 25
                        and actual["player0"]["secondary_ammo"][0] == 4,
                        "Deleted mouse dispatch must cause the real missing second shot",
                    )
                else:
                    # D2's missile also draws SIM RNG, so its input boundary detects
                    # the missing shot before another simulation frame can run
                    aborted = [
                        row for row in states if row.get("type") == "world_boundary" and row.get("phase") == "aborted"
                    ]
                    require(
                        len(aborted) == 1
                        and aborted[0]["f"] == 13
                        and aborted[0]["state"]["players"][0]["secondary_ammo"][0] == 4
                        and not (target / "result.json").exists(),
                        "Deleted D2 mouse dispatch must fail at the missing shot RNG boundary",
                    )
                report.append({"game": game, "backend": mode, "case": mutation, "status": "pass"})


def view_events(path, recorded=False):
    """Include input-tail observations; ignore replay's pre-restore startup UI."""
    result = []
    restored = recorded
    for row in parity.records(path):
        if row.get("type") == "object_boundary" and row.get("phase") == "restored":
            restored = True
        events = row.get("events", []) if recorded else [row]
        for event in events:
            if restored and event.get("kind" if recorded else "type") == "input_view":
                result.append({key: value for key, value in event.items() if key not in ("kind", "type")})
    return result


def verify_modal_capture(demo, capture_report):
    rows = list(parity.records(demo))
    batches = ordered.validate(rows[0], rows[1:-2], rows[-2])
    require(
        len(batches) == 17 and rows[0].get("rear_view") and rows[0].get("view_projection"),
        "Modal capture needs sixteen frames and initial view state",
    )
    checks = {
        "native_f3_status_bar",
        "native_f3_fullscreen",
        "native_f3_full_cockpit_restore",
        "native_shift_f3_noop",
        "sticky_same_batch_tap",
        "sticky_toggle_off",
        "sticky_native_clock_hold_release",
        "nonsticky_same_batch_tap",
        "nonsticky_toggle_off",
        "nonsticky_held_callback_release",
        "movement_and_fire_before_map",
        "map_tap_toggle",
        "map_escape",
        "map_fire_is_not_gameplay_fire",
        "1100_redraws_without_operation_growth",
        "map_long_hold_preserves_native_toggle",
        "unarmed_rear_hold_through_map",
        "armed_rear_hold_through_map",
    }
    if rows[0]["game"] == "d2":
        checks.update(("native_marker_drop", "native_confirmed_marker_delete"))
        require(
            capture_report["marker"]["deleted"] and capture_report["marker"]["signature"] >= 0,
            "Native marker deletion must identify its original object",
        )
    tail_active = capture_report["scenario"] == "ordered-modal-tail"
    if tail_active:
        checks.add("active_map_terminal_tail")
    require(checks <= set(capture_report["checks"]), "Modal capture omitted a required native action")
    require(
        capture_report["redraw_count"] >= 1100 and capture_report["terminal_automap"] == int(tail_active),
        "Modal redraw or terminal state evidence is incomplete",
    )
    observations = capture_report["observations"]
    before = next(row for row in observations if row["phase"] == "map_open")
    after = next(row for row in observations if row["phase"] == "after_redraws")
    require(
        before["game_time"] == after["game_time"]
        and before["ammo"] == after["ammo"]
        and before["energy"] == after["energy"],
        "Modal redraws advanced gameplay",
    )
    actions = [op["action"] for batch in batches for op in batch["ops"] if op["k"] == "rear_view"]
    require({"enter", "toggle_off", "arm", "release_off"} <= set(actions), "Native rear decisions are missing")
    require(
        any(op["ft"] != batch["frame_time"] for batch in batches[:-1] for op in batch["ops"]),
        "Modal input must retain its native interval independently of simulation",
    )
    require(view_events(demo, recorded=True), "Modal capture must retain view/map/marker observations")
    return rows


def verify_modal_orders(args, game, report):
    native = args.output / (game + "-recording-off-native")
    native.mkdir()
    with (native / "capture.log").open("w") as log:
        subprocess.run(
            [
                str(getattr(args, "capture_" + game)),
                "--public-level-replay",
                str(getattr(args, "data_" + game)),
                "marker-delete-native",
            ],
            cwd=native,
            env=isolated_native_env(native),
            stdout=log,
            stderr=subprocess.STDOUT,
            check=True,
            timeout=args.timeout,
        )
    facts = json.loads((native / "capture.json").read_text())
    require(
        facts["recording"] is False and facts["public_replay_admission"] is False and len(facts["rear_cases"]) >= 16,
        "Recording-off native reducer coverage is incomplete",
    )
    require(
        len(facts["cockpit_reverse_cases"]) == 3
        and facts["projection_guard"]["cancelled"]
        and facts["projection_guard"]["restored"],
        "Recording reliability guard coverage is incomplete",
    )
    if game == "d2":
        helpers, discovery = facts["missile_helpers"], facts["missile_map"]
        require(
            helpers["supported"]
            and len(helpers["selectors"]) == 18
            and len(helpers["geometry"]) == 240
            and discovery["supported"]
            and len(discovery["cases"]) == 20
            and all(case["combined_matches_renderer"] for case in discovery["cases"])
            and any(
                case["extra_segments"] > 0 and not case["main_only_matches_renderer"] for case in discovery["cases"]
            )
            and len(discovery["unsupported"]) == 4
            and all(case["capture_canceled"] and case["live_state_unchanged"] for case in discovery["unsupported"]),
            "Default missile-camera discovery and causal main-only negative are incomplete",
        )
        require(
            len(facts["marker_cases"]) == 40
            and all(case["other_slots_unchanged"] for case in facts["marker_cases"])
            and {case["player"] for case in facts["marker_cases"]} == {0, 1},
            "Native absolute-slot marker deletion changed other players' behavior",
        )
    report.append({"game": game, "backend": "native-capture", "case": "recording-off-reducers", "status": "pass"})
    for scenario in ("ordered-modal", "ordered-modal-tail"):
        capture = args.output / (game + "-" + scenario + "-capture")
        capture.mkdir()
        with (capture / "capture.log").open("w") as log:
            subprocess.run(
                [
                    str(getattr(args, "capture_" + game)),
                    "--public-level-replay",
                    str(getattr(args, "data_" + game)),
                    scenario,
                ],
                cwd=capture,
                env=isolated_native_env(capture),
                stdout=log,
                stderr=subprocess.STDOUT,
                check=True,
                timeout=args.timeout,
            )
        demo = capture / "capture.dximdemo"
        rows = verify_modal_capture(demo, json.loads((capture / "capture.json").read_text()))
        expected_view = view_events(demo, recorded=True)
        for mode in ("headless", "graphics"):
            destination = args.output / (game + "-" + mode + "-" + scenario)
            require(replay(args, game, mode, demo, destination) == 0, "Native modal replay failed")
            require(
                compact._frame_hashes(demo, rows[0], recorded=True)
                == compact._frame_hashes(destination / "state.jsonl.gz", rows[0]),
                "Native modal full state/RNG/diagnostics differ",
            )
            require(
                parity.compare_rng(capture / "capture.dximdemo.rngtrace.jsonl", destination / "rng.jsonl")["status"]
                == "pass",
                "Native modal SIM events differ",
            )
            require(
                expected_view == view_events(destination / "state.jsonl.gz"),
                "Native modal view/map/marker observations differ",
            )
            report.append({"game": game, "backend": mode, "case": scenario, "status": "pass"})
            if scenario != "ordered-modal":
                continue
            mutations = ["rear-arm", "map-depths", "map-controls", "map-clear", "projection", "cockpit"]
            if game == "d2":
                mutations.extend(("marker-delete", "marker-identity"))
            for mutation in mutations:
                changed = copy.deepcopy(rows)
                found = mutation == "projection"
                if found:
                    changed[0]["view_projection"]["aspect"] += 1
                for row in [] if found else changed[1:-1]:
                    ops = row["dispatch"]["ops"]
                    for index, op in enumerate(ops):
                        matches = (
                            mutation == "cockpit"
                            and op["k"] == "cockpit"
                            or mutation == "rear-arm"
                            and op["k"] == "rear_view"
                            and op["action"] == "arm"
                            or mutation == "map-depths"
                            and op["k"] == "automap"
                            and op["phase"] == "depths"
                            or mutation == "map-controls"
                            and op["k"] == "controls"
                            and row.get("f") == 8
                            or mutation == "map-clear"
                            and op["k"] == "clear"
                            and row.get("f") == 8
                            or mutation in ("marker-delete", "marker-identity")
                            and op["k"] == "command"
                            and op["event"]["command"] == "delete_marker"
                        )
                        if matches:
                            if mutation == "marker-identity":
                                op["event"]["expected_signature"] += 1
                            else:
                                del ops[index]
                            found = True
                            break
                    if found:
                        break
                require(found, "Modal negative has no real target operation: " + mutation)
                negative = args.output / (game + "-" + mode + "-modal-" + mutation + ".dximdemo")
                negative.write_bytes(b"".join(compact.canonical(row) + b"\n" for row in changed))
                target = negative.with_suffix("")
                status = replay(args, game, mode, negative, target)
                state_path = target / "state.jsonl.gz"
                actual_rows = list(parity.records(state_path)) if state_path.exists() else []
                if mutation in ("map-depths", "projection"):
                    require(
                        status != 0
                        and not any(row.get("type") == "object_boundary" for row in actual_rows)
                        and not (target / "result.json").exists(),
                        "Broken map lifecycle must fail before world restoration",
                    )
                elif mutation in ("rear-arm", "marker-identity"):
                    require(
                        status != 0
                        and not (target / "result.json").exists()
                        and any(
                            row.get("type") == "object_boundary" and row.get("phase") == "aborted"
                            for row in actual_rows
                        ),
                        "Changed modal decision/identity must abort native execution",
                    )
                else:
                    # Native actual results are evidence, never acceptance: the independent
                    # full state and view oracle must reject even an otherwise clean exit
                    require(
                        status != 0
                        or compact._frame_hashes(demo, rows[0], recorded=True)
                        != compact._frame_hashes(state_path, rows[0])
                        or expected_view != view_events(state_path),
                        "Removed modal effect passed every independent oracle",
                    )
                report.append({"game": game, "backend": mode, "case": "modal-" + mutation, "status": "pass"})


def verify_live_loot_no_collect(args, report):
    """Complete a genuine altered-input capture before exercising the original loot gate."""
    corpus = args.repo / "android/tests/fixtures/public_level_replays"
    manifest = json.loads((corpus / "manifest.json").read_text())
    case = next(case for case in manifest["cases"] if case["game"] == "d1" and case["scenario"] == "loot")
    original_demo = corpus / case["demo"]
    original_header, oracle = compact.validate(original_demo, corpus / case["oracle"])
    original = list(parity.records(original_demo))
    original_batches = ordered.validate(original_header, original[1:-2], original[-2])
    scenario = "generated-loot-no-collect"
    capture = args.output / ("d1-" + scenario + "-capture")
    capture.mkdir()
    env = isolated_native_env(capture)
    env["DXX_PUBLIC_CAPTURE_LOOT_NO_COLLECT"] = "1"
    with (capture / "capture.log").open("w") as log:
        subprocess.run(
            [str(args.capture_d1), "--public-level-replay", str(args.data_d1), "loot"],
            cwd=capture,
            env=env,
            stdout=log,
            stderr=subprocess.STDOUT,
            check=True,
            timeout=args.timeout,
        )
    live = json.loads((capture / "capture.json").read_text())
    demo = capture / "capture.dximdemo"
    rows = list(parity.records(demo))
    header = rows[0]
    batches = ordered.validate(header, rows[1:-2], rows[-2])
    actions = case["actions"]
    drop = actions["loot_frame"]
    require(
        live["scenario"] == scenario
        and live["public_replay_admission"] is False
        and live["frame_count"] == header["frame_count"] == original_header["frame_count"]
        and live["no_collect_after_frame"] == drop,
        "Generated no-collect capture must remain outside admission and finish the original horizon",
    )
    require(
        all(live[key] == value for key, value in actions.items() if key not in ("pickup_frame", "pickup_gain_frame"))
        and live["pickup_frame"] == live["pickup_gain_frame"] == -1
        and live["score"] == actions["target_score"]
        and all(observation["laser_level"] == 0 for observation in live["observations"]),
        "Live no-collect capture must retain the original kill/drop with no inventory gain",
    )
    require(batches[: drop + 1] == original_batches[: drop + 1], "Loot input/RNG prefix changed before physical drop")
    for batch in batches[drop + 1 : -1]:
        require(
            not any(batch["residual"]["s"].values())
            and not any(batch["residual"]["p"].values())
            and len(batch["ops"]) == 1
            and batch["ops"][0]["k"] == "sample"
            and not any(batch["ops"][0]["s"].values())
            and not any(batch["ops"][0]["p"].values()),
            "No-collect capture must release movement and fire after the actual physical drop",
        )
    own_hashes = compact._frame_hashes(demo, header, recorded=True)
    require(
        own_hashes[: drop + 2] == oracle["frame_sha256"][: drop + 2],
        "Live no-collect state/RNG/diagnostics must match the unchanged oracle through physical drop",
    )
    failure = "The exact new laser pickup must spawn, persist, then retire at collection"
    for mode in ("headless", "graphics"):
        destination = args.output / ("d1-" + mode + "-" + scenario)
        require(replay(args, "d1", mode, demo, destination) == 0, "Live no-collect replay did not complete naturally")
        trace = destination / "state.jsonl.gz"
        actual_path = destination / "result.json"
        actual = json.loads(actual_path.read_text())
        require(
            own_hashes == compact._frame_hashes(trace, header),
            "Live no-collect replay full state/RNG/diagnostics differ from its own live capture",
        )
        require(
            parity.compare_rng(capture / "capture.dximdemo.rngtrace.jsonl", destination / "rng.jsonl")["status"]
            == "pass"
            and actual == rows[-1]["result"],
            "Live no-collect replay SIM events or terminal result differ from its own live capture",
        )
        terminal_objects = terminal_world = None
        worlds = []
        for row in parity.world_states(trace, header, "d1"):
            if row["type"] == "world_state":
                worlds.append(dict(row["state"]))
            elif row["phase"] == "terminal":
                if row["type"] == "object_boundary":
                    terminal_objects = row["slots"]
                else:
                    terminal_world = row["state"]
        require(
            terminal_objects is not None and terminal_world is not None, "No-collect replay lacks terminal evidence"
        )
        worlds.append(terminal_world)
        pickup = terminal_objects[str(actions["pickup"])]
        expected_pickup = live["remaining_pickup"]
        require(
            expected_pickup["slot"] == actions["pickup"]
            and (pickup["type"], pickup["id"], pickup["signature"]) == (7, 3, actions["pickup_signature"])
            and all(pickup[key] == expected_pickup[key] for key in ("type", "id", "signature", "flags", "lifeleft"))
            and not pickup["flags"] & 2
            and pickup["lifeleft"] > 0
            and all(
                world["players"][0]["laser_level"] == 0
                and world["players"][0]["primary_weapon"] == 0
                and world["players"][0]["primary_weapon_flags"] == 1
                and world["players"][0]["score"] == actions["target_score"] * int(index > actions["action_frame"])
                and world["death"]["active"] == 0
                for index, world in enumerate(worlds)
            ),
            "Completed replay must preserve the exact live uncollected pickup and original score without inventory gain",
        )
        try:
            verify_actions(case, demo, trace, actual_path)
        except parity.EvidenceError as error:
            require(
                str(error) == failure,
                "Unchanged loot gate failed before the missed-collection requirement: " + str(error),
            )
        else:
            raise parity.EvidenceError("Unchanged corpus loot requirement accepted a completed no-collect replay")
        report.append(
            {
                "game": "d1",
                "backend": mode,
                "case": scenario,
                "status": "pass",
                "public_replay_admission": False,
                "frame_count": header["frame_count"],
                "capture_revision": header["git_version"],
                "release_after_frame": drop,
                "pickup": actions["pickup"],
                "pickup_signature": actions["pickup_signature"],
                "own_live_oracle": "pass",
                "semantic_gate_error": failure,
            }
        )


def run(args):
    require(not args.output.exists(), "Use a new ordered-input output directory")
    args.output.mkdir(parents=True)
    report = []
    for game in ("d1",) if args.loot_no_collect_only else ("d1", "d2"):
        data = getattr(args, "data_" + game)
        validate_assets(game, data)
        if args.loot_no_collect_only:
            verify_live_loot_no_collect(args, report)
            continue
        if args.modal_only:
            verify_modal_orders(args, game, report)
            (args.output / "report.json").write_text(
                json.dumps({"status": "running", "complete": False, "runs": report}, indent=2) + "\n"
            )
            continue
        capture = args.output / (game + "-capture")
        capture.mkdir()
        with (capture / "capture.log").open("w") as log:
            subprocess.run(
                [str(getattr(args, "capture_" + game)), "--public-level-replay", str(data), "ordered-dispatch"],
                cwd=capture,
                env=isolated_native_env(capture),
                stdout=log,
                stderr=subprocess.STDOUT,
                check=True,
                timeout=args.timeout,
            )
        demo = capture / "capture.dximdemo"
        rows = verify_dispatch_capture(demo)
        header = rows[0]
        for mode in ("headless", "graphics"):
            directory = args.output / (game + "-" + mode + "-positive")
            require(replay(args, game, mode, demo, directory) == 0, "Native ordered dispatch replay failed")
            require(
                compact._frame_hashes(demo, header, recorded=True)
                == compact._frame_hashes(directory / "state.jsonl.gz", header),
                "Native ordered dispatch frame state/RNG/diagnostics differ",
            )
            require(
                parity.compare_rng(capture / "capture.dximdemo.rngtrace.jsonl", directory / "rng.jsonl")["status"]
                == "pass",
                "Native ordered dispatch SIM events differ",
            )
            report.append({"game": game, "backend": mode, "case": "positive", "status": "pass"})
            for mutation, boundary in (
                ("clear", 2),
                ("first-input", 0),
                ("tail", 8),
                ("rng-carry-state", 6),
                ("rng-carry-count", 6),
                ("rng-tail-count", 8),
            ):
                changed = copy.deepcopy(rows)
                if mutation == "clear":
                    changed[3]["dispatch"]["ops"] = []
                elif mutation == "first-input":
                    # Leave final released controls intact but remove the first flare
                    changed[1]["dispatch"]["ops"] = changed[1]["dispatch"]["ops"][2:]
                elif mutation == "tail":
                    changed[-2]["dispatch"]["ops"] = []
                else:
                    # Change both ends coherently: repeated reseeding would conceal
                    # this carry error, especially the input-only tail count
                    target = changed[-2] if mutation == "rng-tail-count" else changed[7]
                    field = "s" if mutation == "rng-carry-state" else "c"
                    target["dispatch"]["pre_rng"][field] += 1
                    target["rng"][field] += 1
                negative = args.output / (game + "-" + mode + "-" + mutation + ".dximdemo")
                negative.write_bytes(b"".join(compact.canonical(row) + b"\n" for row in changed))
                destination = negative.with_suffix("")
                require(replay(args, game, mode, negative, destination) != 0, "Corrupt ordered input was accepted")
                actual_rows = list(parity.records(destination / "state.jsonl.gz"))
                aborted = [
                    row for row in actual_rows if row.get("type") == "object_boundary" and row.get("phase") == "aborted"
                ]
                require(
                    len(aborted) == 1
                    and aborted[0]["f"] == boundary
                    and sum(row.get("type") == "frame_state" for row in actual_rows) == boundary
                    and not (destination / "result.json").exists(),
                    "Negative did not abort before simulation at the intended ordered-input boundary",
                )
                report.append({"game": game, "backend": mode, "case": mutation, "status": "pass"})
            for mutation in ("unknown-operation", "invalid-command"):
                changed = copy.deepcopy(rows)
                if mutation == "unknown-operation":
                    changed[1]["dispatch"]["ops"][0]["k"] = "unknown"
                else:
                    command = (
                        {
                            "kind": "direct_command",
                            "command": "guidebot_goal",
                            "special_key": 2147483647,
                            "from_menu": False,
                        }
                        if game == "d2"
                        else {
                            "kind": "direct_command",
                            "command": "select_weapon_exact",
                            "weapon_class": 0,
                            "weapon_index": 999,
                        }
                    )
                    changed[1]["dispatch"]["ops"].append({"k": "command", "ft": 1092, "event": command})
                negative = args.output / (game + "-" + mode + "-" + mutation + ".dximdemo")
                negative.write_bytes(b"".join(compact.canonical(row) + b"\n" for row in changed))
                destination = negative.with_suffix("")
                require(replay(args, game, mode, negative, destination) != 0, "Malformed ordered input was accepted")
                state_path = destination / "state.jsonl.gz"
                require(
                    not (destination / "result.json").exists()
                    and (
                        not state_path.exists()
                        or not any(row.get("type") == "object_boundary" for row in parity.records(state_path))
                    ),
                    "Malformed input must fail before restoring or mutating the game world",
                )
                report.append({"game": game, "backend": mode, "case": mutation, "status": "pass"})
            # A real filesystem write failure must not produce a native OK verdict
            blocked = args.output / (game + "-" + mode + "-unwritable-result")
            blocked.mkdir()
            result_directory = blocked / "result.json"
            result_directory.mkdir()
            command = [
                str(getattr(args, mode + "_" + game)),
                "-hogdir",
                str(data),
                "-notitles" if game == "d1" else "-nomovies",
                "-nomusic",
                "-nosound",
                "-inputdemo-replay",
                str(demo),
                "-inputdemo-actual-result",
                str(result_directory),
                "-inputdemo-exit",
            ]
            if mode == "graphics":
                command.extend(["-window", "-nonicefps"])
            else:
                command.extend(["-headless-console-output", "1"])
            env = isolated_native_env(blocked, mode)
            with (blocked / "native.log").open("w") as log:
                outcome = subprocess.run(
                    command, cwd=blocked, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=args.timeout
                )
            require(
                outcome.returncode != 0 and "HEADLESS-RUN OK" not in (blocked / "native.log").read_text(),
                "Native executable reported success without writing its terminal result",
            )
            report.append({"game": game, "backend": mode, "case": "unwritable-result", "status": "pass"})
        if game == "d1":
            verify_live_loot_no_collect(args, report)
        verify_secondary_orders(args, game, report)
        verify_modal_orders(args, game, report)
        (args.output / "report.json").write_text(
            json.dumps({"status": "running", "complete": False, "runs": report}, indent=2) + "\n"
        )
    expected_runs = 2 if args.loot_no_collect_only else 38 if args.modal_only else 96
    require(len(report) == expected_runs, "Native qualification did not execute its complete expected matrix")
    (args.output / "report.json").write_text(
        json.dumps({"status": "pass", "complete": True, "expected_runs": expected_runs, "runs": report}, indent=2)
        + "\n"
    )
    if args.loot_no_collect_only:
        print(
            "PASS: completed live D1 no-collect replay matches its own oracle and fails the unchanged collection gate on both backends"
        )
    elif args.modal_only:
        print("PASS: native rear/map/marker effects and causal negatives in D1/D2 on both backends")
    else:
        print("PASS: native ordered input, RNG, flushes, tail, modal effects, isolation and causal negatives in D1/D2")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for role in ("capture", "headless", "graphics", "data"):
        for game in ("d1", "d2"):
            parser.add_argument("--" + role + "-" + game, type=Path, required=True)
    focus = parser.add_mutually_exclusive_group()
    focus.add_argument(
        "--modal-only", action="store_true", help="Run only native rear-view/automap cases while debugging"
    )
    focus.add_argument(
        "--loot-no-collect-only", action="store_true", help="Run only the completed live D1 loot semantic negative"
    )
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--pwsh", default="pwsh")
    parser.add_argument("--timeout", type=int, default=60)
    parser.add_argument("--minimum-free-gb", type=float, default=4)
    args = parser.parse_args()
    existed = args.output.exists()
    try:
        run(args)
    except Exception as error:
        if not existed and args.output.exists():
            path = args.output / "report.json"
            result = json.loads(path.read_text()) if path.exists() else {"runs": []}
            result.update(status="fail", complete=False, error=str(error))
            path.write_text(json.dumps(result, indent=2) + "\n")
        raise


if __name__ == "__main__":
    main()
