"""Fail-closed gameplay and report gates for the compact public runtime runner."""

import argparse
import copy
import contextlib
import io
import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import run_compact_public_replays as runner
import check_compact_public_replay_failures as negative


class RuntimeGateTests(unittest.TestCase):
    def test_secondary_rng_negative_requires_exact_causal_abort(self):
        case = {
            "game": "d2",
            "scenario": "secondary-mouse-first",
            "demo": "unused.dximdemo",
            "oracle": "unused.oracle.json",
            "actions": {"first_fire_frame": 2, "initial_ammo": 5},
        }
        report = {
            "status": "fail",
            "backend": "headless",
            "native_only": True,
            "paired": {"status": "not_run"},
            "native_runs": [],
        }
        for repeat in range(2):
            directory = self.root / str(repeat)
            directory.mkdir()
            state, rng, result = (directory / name for name in ("state.jsonl", "rng.jsonl", "result.json"))
            state.write_text(
                "\n".join(
                    json.dumps(row)
                    for row in [
                        {"type": "frame_state", "f": 0},
                        {"type": "frame_state", "f": 1},
                        {
                            "type": "world_boundary",
                            "phase": "aborted",
                            "f": 2,
                            "state": {"players": [{"secondary_ammo": [5]}]},
                        },
                    ]
                )
                + "\n"
            )
            rng.write_text('{"type":"meta"}\n')
            failure = directory / "result.json.failure_test"
            failure.mkdir()
            (failure / "failure.json").write_text(json.dumps({"forced_stop": False, "native_exit_code": 1}))
            report["native_runs"].append(
                {
                    "game": "d2",
                    "scenario": case["scenario"],
                    "repeat": repeat,
                    "status": "fail",
                    "exit_code": 1,
                    "command": ["-Runner", "headless-console"],
                    "artifacts": {"state": str(state), "rng": str(rng), "result": str(result)},
                }
            )
        args = argparse.Namespace(run_mode="headless", corpus=self.root)
        self.assertEqual(len(negative.verify_failure(report, 1, args, [case], "bad-input")), 2)
        failure_path = self.root / "0/result.json.failure_test/failure.json"
        for failure in ({"forced_stop": True, "native_exit_code": 1}, {"forced_stop": False, "native_exit_code": 0}):
            failure_path.write_text(json.dumps(failure))
            with self.assertRaisesRegex(runner.parity.EvidenceError, "exact causal SIM boundary"):
                negative.verify_failure(report, 1, args, [case], "bad-input")
        failure_path.write_text(json.dumps({"forced_stop": False, "native_exit_code": 1}))
        state_path = self.root / "0/state.jsonl"
        original = state_path.read_text()
        for mutation in ("frame", "ammo", "missing-frame", "extra-frame"):
            rows = [json.loads(line) for line in original.splitlines()]
            if mutation == "frame":
                rows[-1]["f"] = 3
            elif mutation == "ammo":
                rows[-1]["state"]["players"][0]["secondary_ammo"][0] = 4
            elif mutation == "missing-frame":
                rows.pop(0)
            else:
                rows.insert(0, {"type": "frame_state", "f": 0})
            state_path.write_text("\n".join(json.dumps(row) for row in rows) + "\n")
            with (
                self.subTest(mutation=mutation),
                self.assertRaisesRegex(runner.parity.EvidenceError, "exact causal SIM boundary"),
            ):
                negative.verify_failure(report, 1, args, [case], "bad-input")
        state_path.write_text(original)
        (self.root / "0/result.json").write_text("{}")
        with self.assertRaisesRegex(runner.parity.EvidenceError, "native boundary"):
            negative.verify_failure(report, 1, args, [case], "bad-input")

    def test_phase_timings_include_failures_without_suppressing_them(self):
        timings = {}
        with mock.patch.object(runner.time, "perf_counter", side_effect=(10.0, 12.5, 20.0, 21.25)):
            with runner.measure_phase(timings, "runner"):
                pass
            with self.assertRaisesRegex(runner.parity.EvidenceError, "wrong oracle"):
                with runner.measure_phase(timings, "fidelity"):
                    raise runner.parity.EvidenceError("wrong oracle")
        self.assertEqual(timings, {"runner": 2.5, "fidelity": 1.25})

    @staticmethod
    def imported_scope(scenario):
        return {
            "role": "required-gameplay" if scenario == "flight" else "diagnostic-only",
            "reason": "Explicit test scope",
        }

    @unittest.skipUnless(os.name == "posix", "POSIX retention uses advisory file locks")
    def test_active_capture_holds_retention_lease(self):
        import fcntl

        args = argparse.Namespace(
            corpus=self.root,
            data_d1=self.root,
            data_d2=self.root,
            exe_d1=self.demo,
            exe_d2=self.demo,
            output=self.root / "active-capture",
        )

        def inspect_lease(args, cases):
            with (args.output / "producer.lock").open("r") as competing:
                with self.assertRaises(BlockingIOError):
                    fcntl.flock(competing.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            return 0

        with (
            mock.patch.object(runner, "load_corpus", return_value=[]),
            mock.patch.object(runner, "validate_assets"),
            mock.patch.object(runner, "_run_cases", side_effect=inspect_lease),
        ):
            self.assertEqual(runner.run(args), 0)

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.demo = self.root / "demo.dximdemo"
        self.actual = self.root / "actual.json"
        self.count = 12
        self.header = {
            "type": "header",
            "game": "d1",
            "mission": "d1",
            "level": 1,
            "difficulty": 2,
            "start_mode": "new_level",
            "frame_count": self.count,
        }
        self.controls = [
            {"type": "frame", "f": i, "input": {"s": {"f": -1092} if i == 0 else {}}} for i in range(self.count)
        ]
        self.write_demo()

        def state(index):
            return {
                "position": {"x": 0 if index == 0 else 131072, "y": 0, "z": 0},
                "player0": {"energy": 100 if index == 0 else 99, "shields": 100},
            }

        self.frames = [{"state": state(i), "diag": {"player_weapon_count": 1}} for i in range(self.count)]
        self.actual.write_text(json.dumps(state(self.count)))

        def world(kind, index, phase=None):
            row = {
                "type": kind,
                "f": index,
                "state": {"death": {"active": 0}, "walls": [{"state": 0 if index < 3 else 1}]},
            }
            if phase:
                row["phase"] = phase
            return row

        self.worlds = (
            [world("world_boundary", 0, "restored")]
            + [world("world_state", i) for i in range(self.count)]
            + [world("world_boundary", self.count, "terminal")]
        )
        self.pickup_target = {"target": 4, "target_id": 1, "target_signature": 5}
        self.objects = [
            {"objects": {"4": {"type": 7, "id": 1, "signature": 5}} if i <= 3 else {}} for i in range(self.count)
        ]

    def write_demo(self):
        self.demo.write_text("\n".join(json.dumps(row) for row in [self.header] + self.controls) + "\n")

    def action(self, scenario, actions, frames=None, worlds=None):
        with (
            mock.patch.object(
                runner.parity, "frames", return_value=iter(copy.deepcopy(self.frames if frames is None else frames))
            ),
            mock.patch.object(
                runner.parity,
                "world_states",
                return_value=iter(copy.deepcopy(self.worlds if worlds is None else worlds)),
            ),
            mock.patch.object(runner.parity, "object_states", return_value=iter(copy.deepcopy(self.objects))),
        ):
            return runner.verify_actions(
                {"game": "d1", "scenario": scenario, "actions": actions}, self.demo, self.demo, self.actual
            )

    def test_backward_thrust_and_delta_controls_count_real_stopping(self):
        self.assertEqual(self.action("flight", {"wall_contact_frames": 11})["wall_contact_frames"], 11)
        self.controls[1]["input"]["s"]["f"] = 0
        self.write_demo()
        with self.assertRaises(runner.parity.EvidenceError):
            self.action("flight", {"wall_contact_frames": 11})

    def test_dead_or_stationary_or_unarmed_motion_cannot_pass(self):
        for mutation in ("dead", "stationary", "weapons", "energy", "claimed_contacts", "missing_world"):
            frames, worlds = copy.deepcopy(self.frames), copy.deepcopy(self.worlds)
            actions = {"wall_contact_frames": 11}
            if mutation == "dead":
                for row in worlds:
                    row["state"]["death"]["active"] = 1
            elif mutation == "stationary":
                for row in frames:
                    row["state"]["position"]["x"] = 131072
            elif mutation == "weapons":
                for row in frames:
                    row["diag"]["player_weapon_count"] = 0
            elif mutation == "energy":
                for row in frames:
                    row["state"]["player0"]["energy"] = 99
            elif mutation == "claimed_contacts":
                actions["wall_contact_frames"] = 0
            else:
                worlds.pop()
            with self.subTest(mutation=mutation), self.assertRaises(runner.parity.EvidenceError):
                self.action("flight", actions, frames, worlds)

    def test_combat_proves_causal_outcome_before_generic_firing_guard(self):
        frames = copy.deepcopy(self.frames)
        for row in frames:
            row["diag"]["player_weapon_count"] = 0
        with mock.patch.object(
            runner.robot_combat, "verify", side_effect=runner.parity.EvidenceError("identified robot remained alive")
        ) as verify:
            with self.assertRaisesRegex(runner.parity.EvidenceError, "identified robot remained alive"):
                self.action("combat", {}, frames)
            verify.assert_called_once()
        with mock.patch.object(runner.robot_combat, "verify", return_value={"score_awarded": 200}) as verify:
            with self.assertRaisesRegex(runner.parity.EvidenceError, "live player weapons"):
                self.action("combat", {}, frames)
            verify.assert_called_once()
        with mock.patch.object(runner.robot_combat, "verify", return_value={"score_awarded": 200}):
            self.assertEqual(self.action("combat", {})["score_awarded"], 200)

    def test_loot_proves_collection_before_generic_firing_guard(self):
        frames = copy.deepcopy(self.frames)
        for row in frames:
            row["diag"]["player_weapon_count"] = 0
        with mock.patch.object(
            runner.robot_loot, "verify", side_effect=runner.parity.EvidenceError("identified drop remained uncollected")
        ) as verify:
            with self.assertRaisesRegex(runner.parity.EvidenceError, "identified drop remained uncollected"):
                self.action("loot", {}, frames)
            verify.assert_called_once()
        with mock.patch.object(runner.robot_loot, "verify", return_value={"laser_level_increase": 1}) as verify:
            with self.assertRaisesRegex(runner.parity.EvidenceError, "live player weapons"):
                self.action("loot", {}, frames)
            verify.assert_called_once()
        with mock.patch.object(runner.robot_loot, "verify", return_value={"laser_level_increase": 1}):
            self.assertEqual(self.action("loot", {})["laser_level_increase"], 1)

    def test_pickup_requires_consecutive_gain_at_exact_next_entry(self):
        frames = copy.deepcopy(self.frames)
        for row in frames[3:]:
            row["state"]["player0"]["shields"] = 110
        self.assertIn(
            2,
            self.action("pickup", {**self.pickup_target, "action_frame": 3, "pickup_gain_frame": 2}, frames)[
                "pickup_gain_frames"
            ],
        )
        for actions, rows in (
            ({"action_frame": 3, "pickup_gain_frame": 3}, frames),
            ({"action_frame": 1, "pickup_gain_frame": 2}, frames),
            ({"action_frame": 3}, frames),
            ({"action_frame": 3, "pickup_gain_frame": -1}, frames),
            ({"action_frame": 3, "pickup_gain_frame": True}, frames),
            ({"action_frame": 3, "pickup_gain_frame": 12}, frames),
            ({"action_frame": 3, "pickup_gain_frame": 2}, self.frames),
        ):
            with self.assertRaises(runner.parity.EvidenceError):
                self.action("pickup", actions, rows)

    def test_pickup_identity_and_retirement_are_exact(self):
        for mutation in ("slot", "id", "signature", "early", "late", "not_original"):
            actions = {**self.pickup_target, "action_frame": 3}
            objects = copy.deepcopy(self.objects)
            if mutation == "slot":
                actions["target"] += 1
            elif mutation == "id":
                actions["target_id"] += 1
            elif mutation == "signature":
                actions["target_signature"] += 1
            elif mutation == "early":
                objects[3]["objects"] = {}
            elif mutation == "late":
                objects[4]["objects"] = objects[3]["objects"]
            else:
                objects[0]["objects"] = {}
            with (
                self.subTest(mutation=mutation),
                mock.patch.object(runner.parity, "object_states", return_value=iter(objects)),
                self.assertRaises(runner.parity.EvidenceError),
            ):
                runner.verify_pickup_target(actions, self.demo, self.header, "d1")

    def test_door_requires_initially_closed_target_and_exact_next_entry(self):
        self.assertEqual(self.action("door", {"target": 0, "action_frame": 2})["door_open_entry_frames"][0], 3)
        for actions in ({"target": 0, "action_frame": 1}, {"target": 99}, {"target": -1}):
            with self.assertRaises(runner.parity.EvidenceError):
                self.action("door", actions)
        worlds = copy.deepcopy(self.worlds)
        worlds[0]["state"]["walls"][0]["state"] = 2
        with self.assertRaises(runner.parity.EvidenceError):
            self.action("door", {"target": 0}, worlds=worlds)

    def paired(self):
        cases, rows = [], []
        for scenario in runner.SCENARIOS:
            name = f"d1-{scenario}.dximdemo"
            (self.root / name).write_text(json.dumps(self.header) + "\n")
            cases.append(
                {
                    "game": "d1",
                    "scenario": scenario,
                    "demo": name,
                    "imported_d1": self.imported_scope(scenario),
                }
            )
            required = (
                "terminal_result",
                "checkpoint_collision_clock",
                "frames",
                "simulation_rng",
                "object_states",
                "world_states",
            )
            checks = {key: {"status": "pass"} for key in required}
            checks["frames"]["frames_compared"] = self.count
            checks["simulation_rng"]["simulation_events_compared"] = 1
            recorded = {
                key: {"status": "incomplete" if key in ("frames", "simulation_rng") else "pass"} for key in required[:4]
            }
            rows.append(
                {
                    "demo": name,
                    "recording_to_native": {"status": "incomplete", "checks": recorded},
                    "native_repeatability": {
                        "status": "pass",
                        "checks": copy.deepcopy(checks),
                    },
                    "native_to_imported": {
                        "status": "pass",
                        "checks": copy.deepcopy(checks),
                    },
                }
            )
        return cases, {"schema": 1, "qualification": "incomplete", "cases": rows}

    def test_only_known_compact_recording_incompleteness_is_accepted(self):
        cases, report = self.paired()
        runner.check_paired_report(report, 2, cases, self.root)
        for mutation in (
            "world",
            "rng",
            "frames",
            "missing",
            "assets",
            "harness",
            "capture",
            "recording_terminal",
            "exit",
        ):
            changed = copy.deepcopy(report)
            case = changed["cases"][0]
            if mutation == "world":
                case["native_to_imported"]["checks"]["world_states"]["status"] = "fail"
            elif mutation == "rng":
                case["native_repeatability"]["checks"]["simulation_rng"]["simulation_events_compared"] = 0
            elif mutation == "frames":
                case["native_repeatability"]["checks"]["frames"]["frames_compared"] -= 1
            elif mutation == "missing":
                changed["cases"].pop()
            elif mutation in ("assets", "harness"):
                changed["asset_error" if mutation == "assets" else "harness_error"] = "changed"
            elif mutation == "capture":
                case["capture_errors"] = {"imported": "failed"}
            elif mutation == "recording_terminal":
                case["recording_to_native"]["checks"]["terminal_result"]["status"] = "fail"
            with self.subTest(mutation=mutation), self.assertRaises(runner.parity.EvidenceError):
                runner.check_paired_report(changed, 1 if mutation == "exit" else 2, cases, self.root)

    def test_imported_scope_is_explicit_and_cannot_remove_required_flight(self):
        cases, report = self.paired()
        future = {"game": "d1", "scenario": "future", "demo": "future.dximdemo"}
        for mutation in (
            "missing",
            "unknown_role",
            "blank_reason",
            "excluded_flight",
            "duplicate_flight",
        ):
            changed = copy.deepcopy(cases)
            if mutation == "missing":
                changed.append(future)
            elif mutation == "unknown_role":
                changed[1]["imported_d1"]["role"] = "optional"
            elif mutation == "blank_reason":
                changed[1]["imported_d1"]["reason"] = " "
            elif mutation == "excluded_flight":
                changed[0]["imported_d1"]["role"] = "excluded"
            else:
                changed.append(changed[0])
            with (
                self.subTest(mutation=mutation),
                self.assertRaises(runner.parity.EvidenceError),
            ):
                runner.imported_d1_scope(changed)
        excluded = {
            **future,
            "imported_d1": {
                "role": "excluded",
                "reason": "Retained unsupported import evidence",
            },
        }
        included, omissions = runner.imported_d1_scope(cases + [excluded])
        self.assertEqual(included, cases)
        self.assertEqual(
            omissions,
            [{"demo": future["demo"], "reason": excluded["imported_d1"]["reason"]}],
        )
        scoped = runner.check_paired_report(report, 2, cases + [excluded], self.root)
        self.assertEqual(scoped["excluded_imported_cases"], omissions)
        changed = copy.deepcopy(report)
        changed["cases"].append({"demo": excluded["demo"]})
        with self.assertRaises(runner.parity.EvidenceError):
            runner.check_paired_report(changed, 2, cases + [excluded], self.root)

    def test_imported_staging_uses_declarations_and_retains_exclusion_on_failure(self):
        cases, _ = self.paired()
        excluded = {
            "game": "d1",
            "scenario": "door-lifecycle",
            "demo": "level-three.dximdemo",
            "imported_d1": {
                "role": "excluded",
                "reason": "Unsupported D2-only wall state",
            },
        }
        # A future case is included when explicitly declared, never silently filtered by its scenario or level
        future = {
            "game": "d1",
            "scenario": "future",
            "demo": "future.dximdemo",
            "imported_d1": self.imported_scope("future"),
        }
        (self.root / future["demo"]).write_text(json.dumps({**self.header, "level": 3}) + "\n")
        output = self.root / "imported"
        output.mkdir()
        args = argparse.Namespace(
            repo=self.root,
            corpus=self.root,
            output=output,
            data_d1=self.root,
            exe_d1=self.demo,
            exe_d2=self.demo,
            pwsh="unused",
            timeout=120,
            minimum_free_gb=0.01,
        )
        report = {}
        with mock.patch.object(runner, "_run", return_value=1):
            runner._run_imported(args, cases + [excluded, future], report)
        self.assertEqual(
            {p.name for p in (output / "d1-corpus").iterdir()},
            {c["demo"] for c in cases + [future]},
        )
        self.assertEqual(report["paired"]["status"], "fail")
        self.assertEqual(
            report["paired"]["excluded_imported_cases"][0]["reason"],
            excluded["imported_d1"]["reason"],
        )

    def test_both_backends_attempt_complete_inventory_and_preserve_failure(self):
        corpus = self.root / "corpus"
        corpus.mkdir()
        cases = []
        for game, scenario in runner.PUBLIC_CASES:
            demo = f"{game}-{scenario}.dximdemo"
            (corpus / demo).write_text("fixture")
            cases.append(
                {
                    "game": game,
                    "scenario": scenario,
                    "demo": demo,
                    "oracle": demo + ".oracle.json",
                    "actions": {},
                    "imported_d1": self.imported_scope(scenario),
                }
            )
        for backend in ("headless", "graphics"):
            output = self.root / backend
            output.mkdir()
            args = argparse.Namespace(
                repo=self.root,
                corpus=corpus,
                output=output,
                data_d1=self.root,
                data_d2=self.root,
                exe_d1=self.demo,
                exe_d2=self.demo,
                run_mode=backend,
                native_only=False,
                pwsh="unused",
                timeout=120,
                minimum_free_gb=0.01,
            )
            with (
                mock.patch.object(runner, "_run", return_value=1) as execute,
                contextlib.redirect_stdout(io.StringIO()),
            ):
                self.assertEqual(runner._run_cases(args, cases), 1)
            self.assertEqual(execute.call_count, 2 * len(cases) + (backend == "graphics"))
            report = json.loads((output / "report.json").read_text())
            self.assertEqual(len(report["native_runs"]), 2 * len(cases))
            self.assertTrue(all(row["status"] == "fail" for row in report["native_runs"]))
            self.assertEqual(
                report["paired"]["status"],
                "fail" if backend == "graphics" else "not_run",
            )
            for row in report["native_runs"]:
                self.assertEqual(row["backend"], backend)
                self.assertEqual(
                    row["command"][row["command"].index("-Runner") + 1],
                    "headless-console" if backend == "headless" else "visual",
                )
                self.assertIn("-StrictComparison", row["command"])
                self.assertIn("-RequireCleanExit", row["command"])
                suffix = row["command"][row["command"].index("-SandboxSuffix") + 1]
                self.assertTrue(suffix.startswith(backend + "-"))
                self.assertNotIn(str(output), suffix)

    def test_same_output_basename_under_distinct_parents_has_distinct_sandbox(self):
        suffixes = []
        for parent in ("first", "second"):
            output = self.root / parent / "wrong-oracle-run"
            output.mkdir(parents=True)
            args = argparse.Namespace(
                repo=self.root,
                corpus=self.root,
                output=output,
                data_d1=self.root,
                data_d2=self.root,
                exe_d1=self.demo,
                exe_d2=self.demo,
                run_mode="headless",
                native_only=True,
                pwsh="unused",
                timeout=120,
                minimum_free_gb=0.01,
            )
            case = {"game": "d1", "scenario": "flight", "demo": self.demo.name, "oracle": "unused.json"}
            with mock.patch.object(runner, "_run", return_value=1), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(runner._run_cases(args, [case]), 1)
            command = json.loads((output / "report.json").read_text())["native_runs"][0]["command"]
            suffixes.append(command[command.index("-SandboxSuffix") + 1])
        self.assertNotEqual(suffixes[0], suffixes[1])

    def test_explicit_native_only_graphics_reports_imported_parity_not_run(self):
        output = self.root / "native-only-graphics"
        output.mkdir()
        args = argparse.Namespace(
            repo=self.root,
            corpus=self.root,
            output=output,
            data_d1=self.root,
            data_d2=self.root,
            exe_d1=self.demo,
            exe_d2=self.demo,
            run_mode="graphics",
            native_only=True,
            pwsh="unused",
            timeout=120,
            minimum_free_gb=0.01,
        )
        case = {"game": "d1", "scenario": "flight", "demo": self.demo.name, "oracle": "unused.json"}
        with (
            mock.patch.object(runner, "_run", return_value=0) as execute,
            mock.patch.object(runner, "_run_imported") as imported,
            mock.patch.object(runner.compact, "verify", return_value={"status": "pass"}),
            mock.patch.object(runner, "verify_actions", return_value={}),
            contextlib.redirect_stdout(io.StringIO()),
        ):
            self.assertEqual(runner._run_cases(args, [case]), 0)
        imported.assert_not_called()
        self.assertEqual(execute.call_count, 2)
        report = json.loads((output / "report.json").read_text())
        self.assertTrue(report["native_only"])
        self.assertEqual(report["paired"]["status"], "not_run")
        self.assertEqual(report["qualification"], "incomplete")

    def scoped_comparison(self, status="pass"):
        return {
            "status": status,
            "frames_compared": self.count,
            "checks": {
                "frames": {"status": status},
                "terminal_result": {"status": "pass"},
                "simulation_rng": {"status": "pass", "simulation_events_compared": 1},
            },
        }

    def test_unqualified_comparative_failure_is_preserved_but_invalid_evidence_fails(self):
        cases, report = self.paired()
        diagnostic = next(row for row in report["cases"] if "door" in row["demo"])
        diagnostic["native_to_imported"]["status"] = "fail"
        diagnostic["native_to_imported"]["checks"]["simulation_rng"]["status"] = "fail"
        path = self.root / "report.json"
        path.write_text(json.dumps(report))

        def compare(path, case):
            result = self.scoped_comparison()
            if "door" in case["demo"]:
                result["status"] = "fail"
                result["checks"]["simulation_rng"]["status"] = "fail"
            return result

        with mock.patch.object(runner.gameplay, "compare_case", side_effect=compare):
            scoped = runner.check_paired_report(report, 1, cases, self.root, path)
        self.assertEqual(scoped["required_paired_cases"], ["d1-flight.dximdemo"])
        self.assertEqual(scoped["status"], "pass")
        row = next(row for row in scoped["cases"] if "door" in row["demo"])
        self.assertFalse(row["required"])
        self.assertEqual(row["scope_role"], "unqualified diagnostic-only")
        self.assertEqual(row["comparison"]["checks"]["simulation_rng"]["status"], "fail")
        for mutation in ("error", "missing", "frames", "native"):
            changed = copy.deepcopy(report)
            if mutation == "native":
                changed["cases"][2]["native_repeatability"]["checks"]["world_states"]["status"] = "fail"

            def broken(path, case):
                result = compare(path, case)
                if "door" in case["demo"]:
                    if mutation == "error":
                        result["checks"]["simulation_rng"]["error"] = "malformed trace"
                    elif mutation == "missing":
                        result["checks"].pop("terminal_result")
                    elif mutation == "frames":
                        result["frames_compared"] -= 1
                return result

            with (
                self.subTest(mutation=mutation),
                mock.patch.object(runner.gameplay, "compare_case", side_effect=broken),
                self.assertRaises(runner.parity.EvidenceError),
            ):
                runner.check_paired_report(changed, 1, cases, self.root, path)

    def test_imported_scope_uses_independent_artifacts_without_relaxing_native(self):
        cases, report = self.paired()
        for case in report["cases"]:
            imported = case["native_to_imported"]
            imported["status"] = "fail"
            for key in ("frames", "object_states", "world_states"):
                imported["checks"][key]["status"] = "fail"
        path = self.root / "report.json"
        path.write_text(json.dumps(report))
        before = path.read_bytes()
        with mock.patch.object(runner.gameplay, "compare_case", return_value=self.scoped_comparison()) as compare:
            scoped = runner.check_paired_report(report, 1, cases, self.root, path)
            self.assertEqual(compare.call_count, len(report["cases"]))
        self.assertEqual(scoped["status"], "pass")
        self.assertEqual(scoped["qualification"], "incomplete")
        self.assertEqual(path.read_bytes(), before)
        self.assertTrue(path.with_name("scoped-gameplay-report.json").is_file())
        for mutation in ("independent", "native", "missing_world", "terminal", "sim"):
            changed = copy.deepcopy(report)
            case = changed["cases"][0]
            if mutation == "native":
                case["native_repeatability"]["checks"]["world_states"]["status"] = "fail"
            elif mutation == "missing_world":
                case["native_to_imported"]["checks"]["world_states"]["status"] = "incomplete"
            elif mutation == "terminal":
                case["native_to_imported"]["checks"]["terminal_result"]["status"] = "fail"
            elif mutation == "sim":
                case["native_to_imported"]["checks"]["simulation_rng"]["status"] = "fail"
            comparison = self.scoped_comparison("fail" if mutation == "independent" else "pass")
            with (
                self.subTest(mutation=mutation),
                mock.patch.object(runner.gameplay, "compare_case", return_value=comparison),
                self.assertRaises(runner.parity.EvidenceError),
            ):
                runner.check_paired_report(changed, 1, cases, self.root, path)

    def test_corpus_requires_complete_unique_plain_filename_pairs(self):
        cases = [
            {
                "game": game,
                "scenario": scenario,
                "demo": f"{game}-{scenario}.dximdemo",
                "oracle": f"{game}-{scenario}.oracle.json",
                "actions": {},
            }
            for game, scenario in runner.PUBLIC_CASES
        ]
        manifest = self.root / "manifest.json"
        imported = {
            "schema": 1,
            "cases": {case["demo"]: self.imported_scope(case["scenario"]) for case in cases if case["game"] == "d1"},
        }

        def valid_pair(demo, oracle):
            game = demo.name[:2]
            level = 2 if "shoot-switch" in demo.name else 3 if game == "d1" and "door-lifecycle" in demo.name else 1
            observations = (
                "combat_observations"
                if "combat" in demo.name or "loot" in demo.name
                else "guidebot_observations"
                if "guidebot-release" in demo.name
                else "door_observations"
                if "door-lifecycle" in demo.name
                else None
            )
            return {"game": game, "level": level}, ({observations: {}} if observations else {})

        with mock.patch.object(runner.compact, "validate", side_effect=valid_pair):
            manifest.write_text(json.dumps({"schema": 1, "cases": cases, "imported_d1": imported}))
            self.assertEqual(len(runner.load_corpus(self.root)), len(runner.PUBLIC_CASES))
            for mutation in (
                "missing",
                "duplicate_case",
                "duplicate_file",
                "unix_path",
                "windows_path",
                "drive_path",
                "missing_actions",
                "wrong_level",
                "d1_switch",
            ):
                changed = copy.deepcopy(cases)
                if mutation == "missing":
                    changed.pop()
                elif mutation == "duplicate_case":
                    changed[-1] = changed[0]
                elif mutation == "duplicate_file":
                    changed[1]["demo"] = changed[0]["demo"]
                elif mutation == "unix_path":
                    changed[0]["demo"] = "../escaped.dximdemo"
                elif mutation == "windows_path":
                    changed[0]["oracle"] = "..\\escaped.json"
                elif mutation == "wrong_level":
                    next(case for case in changed if case["scenario"] == "shoot-switch")["demo"] = "d2-other.dximdemo"
                elif mutation == "d1_switch":
                    next(case for case in changed if case["scenario"] == "shoot-switch")["game"] = "d1"
                elif mutation == "drive_path":
                    changed[0]["demo"] = "C:escaped.dximdemo"
                else:
                    changed[0].pop("actions")
                manifest.write_text(json.dumps({"schema": 1, "cases": changed, "imported_d1": imported}))
                with (
                    self.subTest(mutation=mutation),
                    self.assertRaises(runner.parity.EvidenceError),
                ):
                    runner.load_corpus(self.root)
            for mutation in (
                "missing_case",
                "extra_case",
                "missing_schema",
                "missing_inventory",
                "bool_schema",
                "float_schema",
                "extra_key",
            ):
                changed = copy.deepcopy(imported)
                if mutation == "missing_case":
                    changed["cases"].pop(next(iter(changed["cases"])))
                elif mutation == "extra_case":
                    changed["cases"]["unknown.dximdemo"] = self.imported_scope("unknown")
                elif mutation == "missing_schema":
                    changed.pop("schema")
                elif mutation == "bool_schema":
                    changed["schema"] = True
                elif mutation == "float_schema":
                    changed["schema"] = 1.0
                elif mutation == "extra_key":
                    changed["unknown"] = True
                else:
                    changed = None
                manifest.write_text(json.dumps({"schema": 1, "cases": cases, "imported_d1": changed}))
                with (
                    self.subTest(mutation=mutation),
                    self.assertRaises(runner.parity.EvidenceError),
                ):
                    runner.load_corpus(self.root)


class RealNegativeCorpusTests(unittest.TestCase):
    def test_real_imported_scope_retains_loot_diagnostics_and_required_flight(self):
        source = Path(__file__).parent / "fixtures/public_level_replays"
        included, excluded = runner.imported_d1_scope(runner.load_corpus(source))
        self.assertEqual(
            {case["demo"] for case in included},
            {f"d1-public-{scenario}.dximdemo" for scenario in ("flight", "pickup", "door", "combat", "loot")},
        )
        self.assertEqual(
            {case["demo"] for case in excluded},
            {
                "d1-public-door-lifecycle.dximdemo",
                "d1-public-secondary-release-first.dximdemo",
                "d1-public-secondary-mouse-first.dximdemo",
            },
        )
        self.assertEqual(
            next(case for case in included if case["scenario"] == "loot")["imported_d1"]["role"], "diagnostic-only"
        )
        self.assertEqual(
            next(case for case in included if case["scenario"] == "flight")["imported_d1"]["role"], "required-gameplay"
        )

    def test_combat_cannot_drop_its_immutable_causal_observation_oracle(self):
        source = Path(__file__).parent / "fixtures/public_level_replays"
        cases = runner.load_corpus(source)
        with tempfile.TemporaryDirectory() as root:
            import shutil

            destination = Path(root) / "corpus"
            shutil.copytree(source, destination)
            case = next(row for row in cases if row["scenario"] == "combat")
            path = destination / case["oracle"]
            oracle = json.loads(path.read_bytes())
            oracle.pop("combat_observations")
            path.write_text(json.dumps(oracle))
            with self.assertRaisesRegex(runner.parity.EvidenceError, "immutable live damage/loot"):
                runner.load_corpus(destination)

    def test_door_cannot_drop_its_immutable_geometric_observation_oracle(self):
        source = Path(__file__).parent / "fixtures/public_level_replays"
        cases = runner.load_corpus(source)
        with tempfile.TemporaryDirectory() as root:
            import shutil

            destination = Path(root) / "corpus"
            shutil.copytree(source, destination)
            case = next(row for row in cases if row["scenario"] == "door-lifecycle")
            path = destination / case["oracle"]
            oracle = json.loads(path.read_bytes())
            oracle.pop("door_observations")
            path.write_text(json.dumps(oracle))
            with self.assertRaisesRegex(runner.parity.EvidenceError, "immutable live geometric"):
                runner.load_corpus(destination)

    def test_guidebot_cannot_drop_its_immutable_escort_observation_oracle(self):
        source = Path(__file__).parent / "fixtures/public_level_replays"
        cases = runner.load_corpus(source)
        with tempfile.TemporaryDirectory() as root:
            import shutil

            destination = Path(root) / "corpus"
            shutil.copytree(source, destination)
            case = next(row for row in cases if row["scenario"] == "guidebot-release")
            path = destination / case["oracle"]
            oracle = json.loads(path.read_bytes())
            oracle.pop("guidebot_observations")
            path.write_text(json.dumps(oracle))
            with self.assertRaisesRegex(runner.parity.EvidenceError, "immutable live escort"):
                runner.load_corpus(destination)

    def test_disposable_mutations_are_admitted_without_changing_originals(self):
        source = Path(__file__).parent / "fixtures/public_level_replays"
        cases = runner.load_corpus(source)
        originals = {path.name: path.read_bytes() for path in source.iterdir() if path.is_file()}
        with tempfile.TemporaryDirectory() as root:
            for mutation in ("wrong-oracle", "bad-input"):
                destination = Path(root) / mutation
                negative.corrupt_corpus(source, destination, cases, mutation)
                self.assertEqual(runner.load_corpus(destination), cases)
                for case in cases:
                    expected = json.loads(originals[case["oracle"]])
                    changed = json.loads((destination / case["oracle"]).read_bytes())
                    differences = {key for key in expected if expected[key] != changed[key]}
                    self.assertEqual(
                        differences,
                        {"frame_sha256"} if mutation == "wrong-oracle" else {"compact_sha256"},
                    )
                    original_demo = [json.loads(line) for line in originals[case["demo"]].splitlines()]
                    changed_demo = [json.loads(line) for line in (destination / case["demo"]).read_bytes().splitlines()]
                    self.assertEqual(original_demo[0], changed_demo[0])
                    self.assertEqual(original_demo[-1], changed_demo[-1])
                    if mutation == "wrong-oracle":
                        self.assertEqual(original_demo, changed_demo)
                    else:
                        for before, after in zip(original_demo[1:-1], changed_demo[1:-1]):
                            if case["scenario"] == "loot":
                                if before["f"] == 0 or "f1s" in before["input"].get("s", {}):
                                    before["input"].setdefault("s", {})["f1s"] = 0
                            elif case["scenario"] in ("combat", "guidebot-release"):
                                before["input"].setdefault("s", {})["f1s"] = 0
                                self.assertEqual(after["input"]["s"]["f1s"], 0)
                            elif case["scenario"] == "door-lifecycle":
                                release = next(
                                    row["f"] + 1 for row in case["actions"]["door_events"] if row["stage"] == 2
                                )
                                if before["f"] >= release:
                                    self.assertTrue(all(value == 0 for value in after["input"].get("s", {}).values()))
                                    before["input"] = after["input"]
                            else:
                                self.assertEqual(after["input"], {})
                                before["input"] = {}
                            self.assertEqual(before, after)
        self.assertEqual(
            originals,
            {path.name: path.read_bytes() for path in source.iterdir() if path.is_file()},
        )


if __name__ == "__main__":
    unittest.main()
