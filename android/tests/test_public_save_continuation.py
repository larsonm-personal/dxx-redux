"""Synthetic rejection checks for the narrow ordinary-save contract."""

import copy
import json
import re
import tempfile
import unittest
from unittest import mock
from pathlib import Path

import public_save_continuation as save


class RuntimeRunnerRegistrationTests(unittest.TestCase):
    def test_report_ignores_runtime_directories_without_trace_files(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for case in ("d2-countdown", "d2-robot-death", "d1-epoch-ordinary", "d2-epoch-load",
                         "d2-timer-seismic", "imported-d1-timer-pickup"):
                (root / case).mkdir()
                (root / case / "exit.txt").write_text("0")
            with mock.patch.object(save, "qualify_case", return_value={"status": "pass"}) as qualify:
                with mock.patch("sys.argv", ["public_save_continuation.py", "--output", temp]):
                    self.assertEqual(save.main(), 0)
            self.assertEqual([call.args[0].name for call in qualify.call_args_list],
                             ["d1-flight", "d2-flight", "d1-door"])

    def test_active_runtime_cases_use_staged_d2_assets_and_checked_process(self):
        runner = Path(__file__).with_suffix(".ps1").read_text()
        for scenario in ("countdown", "robot-death"):
            entry = f"@{{Name = 'd2-{scenario}'; Exe = $d2ExePath; Assets = $d2Path; Scenario = '{scenario}'; Mode = '--active-save-runtime' }}"
            self.assertIn(entry, runner)
        self.assertIn("$traceArgs = @($mode, $case.Assets, $case.Scenario)", runner)
        self.assertIn("$case.ContainsKey('Mode')", runner)
        self.assertIn("Assert-SharewareTestDataDirectory -Config $config -DataDir $staged", runner)
        self.assertLess(runner.index("Assert-SharewareTestDataDirectory"), runner.index("foreach ($case in $cases)"))
        self.assertIn("$testProcess.WaitForExit($TimeoutSeconds * 1000)", runner)
        self.assertIn("if ($testProcess.ExitCode -ne 0) { throw", runner)
        workflow = Path(__file__).parents[2] / ".github/workflows/public-save-integrity.yml"
        self.assertIn("-File android/tests/test_public_save_continuation.ps1", workflow.read_text())


class SaveRuntimeExtensionRunnerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.runner = Path(__file__).with_suffix(".ps1").read_text()
        cls.entries = re.findall(r"@\{Name = '([^']+)'; ([^\n]+) \}", cls.runner)
        cls.cases = dict(cls.entries)

    def assert_case(self, name, game, scenario, mode, assets=None, work=None, extra=None):
        entry = self.cases[name]
        self.assertIn(f"Exe = ${game}ExePath; Assets = ${assets or game}Path;", entry)
        self.assertIn(f"Scenario = {scenario}; Mode = '--{mode}'", entry)
        if work:
            self.assertIn(f"WorkDirectory = '{work}'", entry)
            self.assertLess(list(self.cases).index(work), list(self.cases).index(name))
        else:
            self.assertNotIn("WorkDirectory", entry)
        if extra:
            self.assertIn(f"ExtraArguments = @('{extra}')", entry)
        else:
            self.assertNotIn("ExtraArguments", entry)

    def test_guided_edge_cases_and_separate_process_restores(self):
        for scenario in ("active", "released", "multiple", "stale", "parent", "none", "format"):
            self.assert_case(f"d2-guided-{scenario}", "d2", f"'{scenario}'", "guided-save-runtime")
        for scenario in ("active", "released"):
            self.assert_case(
                f"d2-guided-load-{scenario}", "d2", f"'load-{scenario}'", "guided-save-runtime",
                work=f"d2-guided-{scenario}",
            )

    def test_wiggle_controls_and_cold_loads_share_producer_directory(self):
        for game in ("d1", "d2"):
            for scenario in ("normal", "no-wiggle"):
                name = f"{game}-wiggle-{scenario}"
                self.assert_case(name, game, f"'{scenario}'", "wiggle-save-runtime")
                self.assert_case(f"{name}-load", game, "'load'", "wiggle-save-runtime", work=name)
        self.assert_case("d2-wiggle-level-record", "d2", "'level-record'", "wiggle-save-runtime")
        self.assert_case(
            "d2-wiggle-level-load", "d2", "'level-load'", "wiggle-save-runtime",
            work="d2-wiggle-level-record",
        )

    def test_fusion_optional_timer_matrix_in_both_engines(self):
        for game in ("d1", "d2"):
            for scenario in ("idle", "due", "past", "soon", "future", "inactive-charged", "nonfusion"):
                self.assert_case(f"{game}-fusion-{scenario}", game, f"'{scenario}'", "fusion-save-runtime")

    def test_native_d1_producer_feeds_imported_d1_consumer(self):
        producer = "d1-translation-record"
        consumer = "d2-translation-load"
        self.assert_case(producer, "d1", "'.'", "d1-save-translation-runtime")
        self.assert_case(
            consumer, "d2", f"(Join-Path $outputPath '{producer}')", "d1-save-translation-runtime",
            assets="d1",
        )
        self.assertLess(list(self.cases).index(producer), list(self.cases).index(consumer))

    def test_epoch_matrix_and_cold_loads_share_producer_directory(self):
        for game in ("d1", "d2"):
            for scenario in ("ordinary", "high", "repeat", "endian", "malformed", "replay", "save"):
                self.assert_case(f"{game}-epoch-{scenario}", game, f"'{scenario}'", "save-epoch-runtime")
            self.assert_case(
                f"{game}-epoch-load", game, "'load'", "save-epoch-runtime", work=f"{game}-epoch-save",
            )
        for scenario in ("ai-phase", "secret", "flash"):
            self.assert_case(f"d2-epoch-{scenario}", "d2", f"'{scenario}'", "save-epoch-runtime")
        self.assertEqual(sum("Mode = '--save-epoch-runtime'" in entry for entry in self.cases.values()), 19)

    def test_timer_matrix_selects_native_and_imported_assets(self):
        self.assert_case("d1-timer-cooldowns", "d1", "'cooldowns'", "timer-save-runtime")
        for scenario in ("pickup", "cooldowns", "seismic", "ambient", "seismic-level"):
            self.assert_case(f"d2-timer-{scenario}", "d2", f"'{scenario}'", "timer-save-runtime")
        for scenario in ("pickup", "cooldowns"):
            self.assert_case(
                f"imported-d1-timer-{scenario}", "d2", f"'{scenario}'", "timer-save-runtime",
                assets="d1", extra="--imported-d1",
            )
        self.assertEqual(sum("Mode = '--timer-save-runtime'" in entry for entry in self.cases.values()), 8)

    def test_optional_arguments_follow_scenario_before_quoting(self):
        base = "$traceArgs = @($mode, $case.Assets, $case.Scenario)"
        extra = "if ($case.ContainsKey('ExtraArguments')) { $traceArgs += @($case.ExtraArguments) }"
        quote = "$quotedArgs = @($traceArgs | ForEach-Object"
        self.assertIn(extra, self.runner)
        self.assertLess(self.runner.index(base), self.runner.index(extra))
        self.assertLess(self.runner.index(extra), self.runner.index(quote))

    def test_cases_keep_bounded_process_checks_and_distinct_logs(self):
        self.assertEqual(len(self.entries), 67, "Keep the original 40 processes plus 19 epoch and 8 timer processes")
        self.assertEqual(len(self.entries), len(self.cases), "Each process needs its own log directory")
        for game, scenario, _ in save.CASES:
            self.assertIn(f"{game}-{scenario}", self.cases)
        for required in (
            "$directory = Join-Path $outputPath $case.Name",
            "$workingDirectory = if ($case.ContainsKey('WorkDirectory')) { Join-Path $outputPath $case.WorkDirectory } else { $directory }",
            "Test-Path -LiteralPath $workingDirectory -PathType Container",
            "$mode = if ($case.ContainsKey('Mode')) { $case.Mode } else { '--public-save-continuation' }",
            "$traceArgs = @($mode, $case.Assets, $case.Scenario)",
            "WorkingDirectory = $workingDirectory; PassThru = $true",
            'RedirectStandardOutput = "$directory/stdout.log"; RedirectStandardError = "$directory/stderr.log"',
            "$testProcess.WaitForExit($TimeoutSeconds * 1000)",
            "Stop-Process -Id $testProcess.Id -Force",
            'Set-Content "$directory/exit.txt" $testProcess.ExitCode',
            "if ($testProcess.ExitCode -ne 0) { throw",
        ):
            self.assertIn(required, self.runner)
        self.assertLess(self.runner.index("Assert-SharewareTestDataDirectory"), self.runner.index("foreach ($case in $cases)"))


class SavePersistenceTests(unittest.TestCase):
    def setUp(self):
        player = {key: 0 for key in save.PLAYER_FIELDS}
        player.update(primary_ammo=[0] * 5, secondary_ammo=[5, 0, 0, 0, 0], energy=100, shields=80)
        obj = {key: 0 for key in save.OBJECT_FIELDS}
        obj.update(
            type=5,
            signature=4,
            control_type=9,
            movement_type=1,
            pos=[1, 2, 3],
            orient=[[1, 0, 0], [0, 1, 0], [0, 0, 1]],
            physics={"velocity": [3, 2, 1]},
        )
        obj["weapon"] = {key: 0 for key in save.WEAPON_FIELDS}
        obj["weapon"].update(parent_type=4, parent_num=0, parent_signature=1, hitobj_list=[0, 1, 0])
        objects = {
            "slots": {"2": obj},
            "capacity": 1000,
            "allocator": {"signature_seed": 4, "free_obj_list": [3, 4, 5]},
            "clock": {"d_tick_count": 120, "game_time": 393120},
            "rng": [{"available": 1, "state": 10, "calls": 96}, {"available": 1, "state": 20, "calls": 1860}],
        }
        world = {
            "players": [player],
            "globals": {"GameTime64": 393120},
            "tick": {"count": 120, "step": 0, "timer": 0},
            "walls": [{"type": 2, "state": 2}],
            "doors": [{"n_parts": 1, "front": [0, -1], "back": [1, -1], "time": 6}],
            "triggers": [{"flags": 4}],
        }
        self.before = (objects, world)
        self.saved = copy.deepcopy(self.before)
        self.restored = copy.deepcopy(self.before)
        self.restored[0]["rng"][0]["calls"] = 0

    def test_exact_bounded_contract(self):
        save.assert_persistence(self.before, self.saved, self.restored)

    def test_player_promises_reject_each_changed_field(self):
        for field in save.PLAYER_FIELDS:
            with self.subTest(field=field):
                restored = copy.deepcopy(self.restored)
                value = restored[1]["players"][0][field]
                restored[1]["players"][0][field] = value + [1] if isinstance(value, list) else value + 1
                with self.assertRaises(save.EvidenceError):
                    save.assert_persistence(self.before, self.saved, restored)

    def test_object_promises_reject_each_changed_field(self):
        for field in save.OBJECT_FIELDS:
            with self.subTest(field=field):
                restored = copy.deepcopy(self.restored)
                restored[0]["slots"]["2"][field] = -42
                with self.assertRaises(save.EvidenceError):
                    save.assert_persistence(self.before, self.saved, restored)

    def test_weapon_parent_and_hit_storage(self):
        for field in save.WEAPON_FIELDS:
            with self.subTest(field=field):
                restored = copy.deepcopy(self.restored)
                restored[0]["slots"]["2"]["weapon"][field] = -42
                with self.assertRaises(save.EvidenceError):
                    save.assert_persistence(self.before, self.saved, restored)

    def test_allocator_tick_world_and_velocity(self):
        for mutate in (
            lambda o, w: o["allocator"]["free_obj_list"].reverse(),
            lambda o, w: o["clock"].update(d_tick_count=121),
            lambda o, w: o["slots"]["2"]["physics"].update(velocity=[0, 0, 0]),
            lambda o, w: w["tick"].update(timer=1),
            lambda o, w: w["walls"][0].update(state=0),
            lambda o, w: w["doors"][0].update(time=7),
            lambda o, w: w["triggers"][0].update(flags=0),
            lambda o, w: o["slots"].clear(),
        ):
            restored = copy.deepcopy(self.restored)
            mutate(*restored)
            with self.assertRaises(save.EvidenceError):
                save.assert_persistence(self.before, self.saved, restored)

    def test_rng_states_counters_and_clock_are_distinct_promises(self):
        for mutate in (
            lambda o, w: o["rng"][0].update(state=11),
            lambda o, w: o["rng"][1].update(state=21),
            lambda o, w: o["rng"][0].update(available=0),
            lambda o, w: o["rng"][0].update(calls=96),
            lambda o, w: o["rng"][1].update(calls=0),
            lambda o, w: o["clock"].update(game_time=0),
            lambda o, w: w["globals"].update(GameTime64=0),
        ):
            restored = copy.deepcopy(self.restored)
            mutate(*restored)
            with self.assertRaises(save.EvidenceError):
                save.assert_persistence(self.before, self.saved, restored)

    def test_save_itself_cannot_change_promised_state(self):
        self.saved[1]["players"][0]["energy"] -= 1
        with self.assertRaises(save.EvidenceError):
            save.assert_persistence(self.before, self.saved, self.restored)

    def test_missing_promised_field(self):
        del self.restored[0]["slots"]["2"]["weapon"]["hitobj_list"]
        with self.assertRaises(save.EvidenceError):
            save.assert_persistence(self.before, self.saved, self.restored)

    def test_relative_sim_draws_not_absolute_counter(self):
        left, right = copy.deepcopy(self.saved[0]), copy.deepcopy(self.restored[0])
        left["rng"][0]["calls"] += 3
        right["rng"][0]["calls"] += 3
        save.assert_flight_step(left, right, self.saved[0], self.restored[0], self.saved[1], self.restored[1])
        right["rng"][0]["calls"] += 1
        with self.assertRaises(save.EvidenceError):
            save.assert_flight_step(left, right, self.saved[0], self.restored[0], self.saved[1], self.restored[1])

    def test_flight_rng_availability_and_player_outcomes(self):
        for mutate in (
            lambda o, w: o["rng"][0].update(available=0),
            lambda o, w: o["rng"][1].update(state=21),
            lambda o, w: o["rng"][1].update(calls=1861),
            lambda o, w: w["players"][0].update(score=1),
        ):
            right = copy.deepcopy(self.restored)
            mutate(*right)
            with self.assertRaises(save.EvidenceError):
                save.assert_flight_step(
                    self.saved[0], right[0], self.saved[0], self.restored[0], self.saved[1], right[1]
                )

    def test_terminal_only_object_and_rng_mutations(self):
        previous = {key: value for key, value in self.saved[0].items() if key != "slots"}
        previous["objects"] = self.saved[0]["slots"]
        terminal = dict(self.saved[0], type="object_boundary", version=2, f=60, reset=True, phase="terminal")
        save.assert_terminal_objects(previous, terminal)
        for mutate in (
            lambda row: row["slots"]["2"].update(shields=1),
            lambda row: row["rng"][0].update(state=11),
            lambda row: row["rng"][1].update(calls=1861),
            lambda row: row["allocator"]["free_obj_list"].reverse(),
            lambda row: row["clock"].update(game_time=0),
            lambda row: row.update(unexpected_state=1),
        ):
            changed = copy.deepcopy(terminal)
            mutate(changed)
            with self.assertRaises(save.EvidenceError):
                save.assert_terminal_objects(previous, changed)

    def test_terminal_only_world_mutations(self):
        previous = self.saved[1]
        terminal = {
            "type": "world_boundary",
            "version": 10,
            "f": 60,
            "reset": True,
            "phase": "terminal",
            "state": copy.deepcopy(previous),
        }
        save.assert_terminal_world(previous, terminal)
        for mutate in (
            lambda row: row["state"]["players"][0].update(shields=79),
            lambda row: row["state"]["players"][0]["secondary_ammo"].append(1),
            lambda row: row["state"]["doors"][0].update(time=7),
            lambda row: row["state"].update(unexpected_state=1),
        ):
            changed = copy.deepcopy(terminal)
            mutate(changed)
            with self.assertRaises(save.EvidenceError):
                save.assert_terminal_world(previous, changed)

    def test_raw_trajectory_difference_is_not_hidden_by_promise(self):
        self.restored[0]["segment_heads"] = [4, 3]
        self.saved[0]["segment_heads"] = [3, 4]
        save.assert_persistence(self.before, self.saved, self.restored)
        self.assertTrue(list(save.field_differences(self.saved[0], self.restored[0])))

    def test_door_checkpoint_must_be_active_and_armed(self):
        save.assert_door_witness(*self.saved, {"target": 0, "action_frame": 12})
        for mutate in (
            lambda o, w: w["doors"].clear(),
            lambda o, w: w["players"][0].update(shields=6553600),
            lambda o, w: w["walls"][0].update(state=0),
            lambda o, w: o["slots"]["2"].update(type=2),
        ):
            state = copy.deepcopy(self.saved)
            mutate(*state)
            with self.assertRaises(save.EvidenceError):
                save.assert_door_witness(*state, {"target": 0, "action_frame": 12})

    def test_controls_missing_duplicate_reordered_or_inactive_fail(self):
        rows = [
            {"type": "continuation_input", "version": 1, "f": i, "frame_time": 2184, "forward": 2184, "fire_primary": 1}
            for i in range(2)
        ]
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "trace.jsonl"

            def check(values):
                path.write_text("\n".join(json.dumps(row) for row in values))
                save.inputs(path, "flight", 2)

            check(rows)
            for values in (rows[:1], rows + rows[:1], rows[::-1], [dict(row, fire_primary=0) for row in rows]):
                with self.assertRaises(save.EvidenceError):
                    check(values)


if __name__ == "__main__":
    unittest.main()
