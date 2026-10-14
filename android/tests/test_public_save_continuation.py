"""Synthetic rejection checks for the narrow ordinary-save contract."""

import copy
import json
import tempfile
import unittest
from pathlib import Path

import public_save_continuation as save


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
        self.restored[0]["clock"]["game_time"] = 0
        self.restored[1]["globals"]["GameTime64"] = 0

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
            lambda o, w: o["clock"].update(game_time=393120),
            lambda o, w: w["globals"].update(GameTime64=393120),
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
