"""Asset-free positive and mutation tests for registered switch14 ownership."""

import copy
import json
import tempfile
import unittest
from pathlib import Path

import pumo_tulev1_switch14_contract as switch14


class Switch14ContractTest(unittest.TestCase):
    def fixture(self, directory, kind="warm"):
        root = Path(directory)
        (root / "preview-user").mkdir(parents=True, exist_ok=True)
        step = 1 if kind == "warm" else 21
        path = root / "preview-user" / f"pumo-player-level7-1-step{step}.sav"
        path.write_bytes(b"synthetic switch14 predecessor")
        state = {
            "game_time_raw": 100000,
            "position": {"segment": 458},
            "player": {
                "shields": 60,
                "energy": 80,
                "secondary_ammo": [7, 0, 0, 0, 0, 0, 4, 0, 0, 0],
                "robots_killed_level": 4,
            },
            "combat_observation": {"player_object": 0},
            "xl_pilot": {"wall_triggers": {"14": {"operated": False}}},
        }
        receipt = root / (
            "guided140_cold-result.json" if kind == "warm" else "warm-result.json"
        )
        receipt.write_text(
            json.dumps(
                {
                    "result": "PASS",
                    "checkpoint_history": [
                        {
                            "step": step,
                            "file": path.name,
                            "sha256": switch14.sha(path),
                            "state": state,
                        }
                    ],
                }
            )
        )
        return receipt

    def test_templates_are_bounded_and_no_source_bytes(self):
        warm, cold = switch14.template("warm"), switch14.template("cold")
        self.assertEqual(len(warm["steps"]), 23)
        self.assertEqual(len(cold["steps"]), 3)
        self.assertEqual(warm["steps"][6], switch14.POSE)
        self.assertEqual(
            [(s["ms"]) for s in warm["steps"] if s["action"] == "wait_ms"],
            [1000, 1, 2100],
        )
        self.assertEqual(
            [
                (s["button"], s["held"])
                for s in warm["steps"]
                if s["action"] == "send_button"
            ],
            [(101, 1), (101, 0)],
        )
        self.assertEqual(
            [s["action"] for s in cold["steps"]], ["assert", "set_debug", "introspect"]
        )
        self.assertEqual(sum(s["action"] == "pose_view" for s in warm["steps"]), 1)
        for step in warm["steps"] + cold["steps"]:
            self.assertNotIn(
                step["action"],
                {"grant", "fire_trigger", "damage_object", "freeze_actor"},
            )
        self.assertNotIn("resume_sha256", warm)
        self.assertNotIn("game_time_raw", warm["steps"][0]["expect"])

    def test_dynamic_registered_binding_and_mutations(self):
        with tempfile.TemporaryDirectory() as directory:
            producer = self.fixture(directory)
            history = json.loads(producer.read_text())["checkpoint_history"][0]
            plan = switch14.template("warm")
            plan.update(
                resume_checkpoint=history["file"], resume_sha256=history["sha256"]
            )
            bound = switch14.binding("warm", copy.deepcopy(plan), producer)
            self.assertEqual(bound["steps"][0]["expect"]["game_time_raw"], 100000)
            self.assertEqual(
                bound["steps"][16]["expect"]["player.secondary_ammo"][6], 3
            )
            self.assertEqual(bound["steps"][11]["expect"]["player.shields"], 60)
            changed = copy.deepcopy(plan)
            changed["steps"][18]["ms"] = 1100
            with self.assertRaises(ValueError):
                switch14.binding("warm", changed, producer)
            changed = copy.deepcopy(plan)
            changed["steps"][6]["segment"] = 458
            with self.assertRaises(ValueError):
                switch14.binding("warm", changed, producer)
            data = json.loads(producer.read_text())
            data["checkpoint_history"][0]["sha256"] = "0" * 64
            producer.write_text(json.dumps(data))
            with self.assertRaises(ValueError):
                switch14.binding("warm", plan, producer)

    def test_wrong_producer_or_source_state_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            producer = self.fixture(directory)
            with self.assertRaises(ValueError):
                switch14.producer_save(producer, "cold")
            data = json.loads(producer.read_text())
            data["checkpoint_history"][0]["state"]["player"]["secondary_ammo"][6] = 0
            producer.write_text(json.dumps(data))
            with self.assertRaises(ValueError):
                switch14.producer_save(producer, "warm")

    def test_actor_identity_and_corridor_guard(self):
        actors = {x: (20, 65536, x + 1, 10 * 65536) for x in switch14.TRACKED}

        def actor(slot):
            return {
                "object": slot,
                "exists": True,
                "expected_type": 2,
                "expected_id": 20,
                "expected_signature": slot + 1,
                "current_slot": {"type": 2, "id": 20, "signature": slot + 1},
                "damage_since_binding_raw": 0,
                "expected_level": 7,
                "observed_level": 7,
                "expected_load_generation": 2,
                "observed_load_generation": 2,
                "binding_shields_raw": 10 * 65536,
                "actor_state": {
                    "shields_raw": 10 * 65536,
                    "should_be_dead": False,
                    "segment": 1543,
                },
            }

        state = {
            "combat_observation": {
                "load_generation": 2,
                "tracked_objects": [actor(x) for x in switch14.TRACKED],
            }
        }
        switch14.actor_guards(state, actors)
        for mutation in (
            lambda s: s["combat_observation"]["tracked_objects"].pop(),
            lambda s: s["combat_observation"]["tracked_objects"].append(
                copy.deepcopy(s["combat_observation"]["tracked_objects"][0])
            ),
            lambda s: s["combat_observation"]["tracked_objects"][0][
                "actor_state"
            ].update(segment=1544),
            lambda s: s["combat_observation"]["tracked_objects"][0].update(
                damage_since_binding_raw=1
            ),
        ):
            changed = copy.deepcopy(state)
            mutation(changed)
            with self.assertRaises(ValueError):
                switch14.actor_guards(changed, actors)

    def test_contact_owner_wall_and_operation_mutations(self):
        shot = {
            "object": 140,
            "signature": 200,
            "parent_object": 0,
            "parent_signature": 1,
            "weapon_id": 37,
            "lifeleft_raw": 1,
        }
        owner = {"player_object": 0, "player_signature": 1}
        hit = {
            "level": 7,
            "object": 140,
            "signature": 200,
            "weapon_id": 37,
            "parent_object": 0,
            "parent_signature": 1,
            "hit_segment": 1544,
            "hit_side": 2,
            "wall": 205,
            "trigger": 14,
            "base": 41,
            "overlay_raw": 414,
            "eclip": 72,
            "effect_dest": 413,
            "blew_up": 1,
        }
        switch14.contact_guard(hit, shot, owner)
        for key, value in (
            ("object", 141),
            ("signature", 201),
            ("parent_signature", 2),
            ("wall", 204),
            ("overlay_raw", 413),
            ("eclip", 71),
            ("effect_dest", 412),
            ("blew_up", 0),
        ):
            changed = hit.copy()
            changed[key] = value
            with self.assertRaises(ValueError):
                switch14.contact_guard(changed, shot, owner)
        initial = {
            "xl_pilot": {
                "doorways": {str(i): {"type": 5} for i in switch14.GATES}
                | {"200": {"type": 5}},
                "wall_triggers": {"14": {"operated": False}, "15": {"operated": False}},
            }
        }
        final = {
            "xl_pilot": {
                "doorways": {str(i): {"type": 4} for i in switch14.GATES}
                | {"200": {"type": 5}},
                "wall_triggers": {"14": {"operated": True}, "15": {"operated": False}},
                "operation_sequence": 1,
            },
            "combat_observation": {"guided_control": None},
        }
        op = {"index": 14, "shot": 1, "sequence": 1}
        switch14.operation_guard(initial, final, [op])
        for ops in ([], [op, op], [dict(op, index=15)]):
            with self.assertRaises(ValueError):
                switch14.operation_guard(initial, final, ops)
        for mutate in (
            lambda f: f["xl_pilot"]["doorways"]["183"].update(type=5),
            lambda f: f["xl_pilot"]["doorways"]["200"].update(type=4),
            lambda f: f["xl_pilot"]["doorways"].update({"201": {"type": 5}}),
            lambda f: f["xl_pilot"]["wall_triggers"]["15"].update(operated=True),
        ):
            changed = copy.deepcopy(final)
            mutate(changed)
            with self.assertRaises(ValueError):
                switch14.operation_guard(initial, changed, [op])

    def test_complete_trace_envelope_mutations(self):
        plan = switch14.template("warm")
        state = {"synthetic": True}
        rows = [
            {"step": i, "action": step["action"], "state": state, "simulation_fps": 60}
            for i, step in enumerate(plan["steps"])
        ]
        events = rows + [
            {"event": name}
            for name in (
                "position_assist",
                "successful_wall_operation",
                "trigger_change",
                "weapon_wall_contacts",
            )
        ]
        receipt = {
            "result": "PASS",
            "receipt_version": 2,
            "resumed_earned_checkpoint": True,
            "initial_level_setup": 7,
            "final_level": 7,
            "rendered": False,
            "transitions": [],
            "steps": len(rows),
            "position_assist_events": 1,
            "fixed_view_aim_events": 0,
        }
        switch14.trace_guard("warm", receipt, plan, state, events)
        mutations = [
            events[1:],
            events + [rows[0]],
            [dict(events[0], step=1)] + events[1:],
            [dict(events[0], action="wait_ms")] + events[1:],
            [dict(events[0], simulation_fps=59)] + events[1:],
            events[:-1],
            events + [{"event": "trigger_change"}],
            events + [{"event": "position_assist"}],
        ]
        for changed in mutations:
            with self.subTest(changed=changed[:1]), self.assertRaises(ValueError):
                switch14.trace_guard("warm", receipt, plan, state, changed)
        for field, value in (
            ("result", "FAIL"),
            ("steps", 22),
            ("resumed_earned_checkpoint", False),
            ("position_assist_events", 2),
            ("fixed_view_aim_events", 1),
        ):
            changed = dict(receipt, **{field: value})
            with self.subTest(field=field), self.assertRaises(ValueError):
                switch14.trace_guard("warm", changed, plan, state, events)

    def test_native_shaped_save_alias_and_mutations(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile = root / "preview-user"
            profile.mkdir()
            source_file = "pumo-player-level7-1-step1.sav"
            versioned = "pumo-player-level7-2-step21.sav"
            state = {"synthetic": "saved state"}
            (profile / source_file).write_bytes(b"source")
            for name in (versioned, "pumo-player-level7.sav", "pumo-player-quick.sav"):
                (profile / name).write_bytes(b"normal saved bytes")
            digest = switch14.sha(profile / versioned)
            history = [
                {"step": 21, "file": versioned, "sha256": digest, "state": state}
            ]
            receipt = {
                "checkpoints": {
                    "7": {
                        "file": "pumo-player-level7.sav",
                        "sha256": digest,
                        "state": state,
                    }
                }
            }

            def check(result=receipt, saved=history):
                switch14.normal_save_guard(root, result, saved, state, 21, source_file)

            check()
            for change in (
                lambda r, h: r["checkpoints"]["7"].update(file=versioned),
                lambda r, h: r["checkpoints"]["7"].update(sha256="0" * 64),
                lambda r, h: r["checkpoints"]["7"].update(state={"wrong": True}),
                lambda r, h: h[0].update(sha256="0" * 64),
                lambda r, h: h[0].update(file=source_file),
            ):
                changed_receipt, changed_history = (
                    copy.deepcopy(receipt),
                    copy.deepcopy(history),
                )
                change(changed_receipt, changed_history)
                with self.assertRaises(ValueError):
                    check(changed_receipt, changed_history)
            (profile / "pumo-player-quick.sav").unlink()
            with self.assertRaises(FileNotFoundError):
                check()
            (profile / "pumo-player-quick.sav").write_bytes(b"wrong")
            with self.assertRaises(ValueError):
                check()

    def test_release_step_owns_first_guide_birth(self):
        def state(ammo, shot, count):
            return {
                "player": {"secondary_ammo": [0, 0, 0, 0, 0, 0, ammo]},
                "combat_observation": {
                    "guided_control": shot,
                    "owned_weapon_count": count,
                },
            }

        shot = {"object": 140, "signature": 1135, "weapon_id": 37}
        rows = [{"state": state(4, None, 0)} for _ in range(15)]
        rows += [{"state": state(3, shot, 1)}, {"state": state(3, shot, 1)}]
        switch14.shot_timing_guard(rows)
        for index, replacement in (
            (14, state(3, shot, 1)),
            (15, state(2, shot, 2)),
            (15, state(3, None, 0)),
            (16, state(2, shot, 2)),
        ):
            changed = copy.deepcopy(rows)
            changed[index]["state"] = replacement
            with self.assertRaises(ValueError):
                switch14.shot_timing_guard(changed)


if __name__ == "__main__":
    unittest.main()
