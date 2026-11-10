"""Asset-free positive and mutation contracts for registered switch13 ownership."""

import copy
import json
import struct
import tempfile
import unittest
from pathlib import Path

import pumo_tulev1_switch13_contract as switch13


class Switch13ContractTest(unittest.TestCase):
    def fixture(self, directory, kind="warm"):
        root = Path(directory)
        (root / "preview-user").mkdir(parents=True, exist_ok=True)
        step = 1 if kind == "warm" else 17
        saved = root / "preview-user" / f"pumo-player-level7-1-step{step}.sav"
        saved.write_bytes(b"synthetic predecessor")
        state = {
            "current_level_num": 7,
            "game_time_raw": 100000,
            "position": {"segment": 1547},
            "player": {
                "shields": 60,
                "energy": 80,
                "secondary_weapon": 6,
                "secondary_ammo": [7, 0, 0, 0, 0, 0, 3, 0, 0, 0],
                "robots_killed_level": 4,
            },
            "secondary_fire": {"next_delta_raw": -1},
            "combat_observation": {"player_object": 0},
            "xl_pilot": {
                "wall_triggers": {"13": {"operated": False}, "14": {"operated": True}},
                "doorways": {str(w): {"type": 4} for w in switch13.PRIOR_GATES}
                | {str(w): {"type": 5} for w in switch13.GATES},
            },
        }
        receipt = root / ("cold-result.json" if kind == "warm" else "warm-result.json")
        receipt.write_text(
            json.dumps(
                {
                    "result": "PASS",
                    "checkpoint_history": [
                        {"step": step, "file": saved.name, "sha256": switch13.sha(saved), "state": state}
                    ],
                }
            )
        )
        return receipt

    def test_templates_are_bounded_and_clean(self):
        warm, cold = switch13.template("warm"), switch13.template("cold")
        self.assertEqual(len(warm["steps"]), 19)
        self.assertEqual(len(cold["steps"]), 3)
        self.assertEqual(warm["steps"][6], switch13.POSE)
        self.assertEqual([s["ms"] for s in warm["steps"] if s["action"] == "wait_ms"], [1, 2100])
        self.assertEqual(
            [(s["button"], s["held"]) for s in warm["steps"] if s["action"] == "send_button"], [(101, 1), (101, 0)]
        )
        self.assertEqual([s["action"] for s in cold["steps"]], ["assert", "set_debug", "introspect"])
        self.assertEqual(sum(s["action"] == "pose_view" for s in warm["steps"]), 1)
        self.assertEqual(warm["track_objects"], list(switch13.TRACKED))
        self.assertTrue(warm["observe_weapon_wall_contacts"])
        for step in warm["steps"] + cold["steps"]:
            self.assertNotIn(
                step["action"],
                {
                    "grant",
                    "fire_trigger",
                    "damage_object",
                    "freeze_actor",
                    "quick_load",
                    "diagnostic_weapon_rays",
                    "face_view",
                },
            )
        self.assertNotIn("resume_sha256", warm)
        self.assertNotIn("game_time_raw", warm["steps"][0]["expect"])

    def test_dynamic_registered_binding_and_mutations(self):
        with tempfile.TemporaryDirectory() as directory:
            producer = self.fixture(directory)
            history = json.loads(producer.read_text())["checkpoint_history"][0]
            plan = switch13.template("warm")
            plan.update(resume_checkpoint=history["file"], resume_sha256=history["sha256"])
            bound = switch13.binding("warm", copy.deepcopy(plan), producer)
            self.assertEqual(bound["steps"][0]["expect"]["game_time_raw"], 100000)
            self.assertEqual(bound["steps"][12]["expect"]["player.secondary_ammo"][6], 2)
            self.assertEqual(bound["steps"][7]["expect"]["player.shields"], 60)
            for mutation in (
                lambda p: p["steps"][14].update(ms=1100),
                lambda p: p["steps"][6].update(segment=1547),
                lambda p: p.update(qualification="ASSISTED_EARNED_SWITCH13"),
            ):
                changed = copy.deepcopy(plan)
                mutation(changed)
                with self.assertRaises(ValueError):
                    switch13.binding("warm", changed, producer)
            data = json.loads(producer.read_text())
            data["checkpoint_history"][0]["sha256"] = "0" * 64
            producer.write_text(json.dumps(data))
            with self.assertRaises(ValueError):
                switch13.binding("warm", plan, producer)

    def test_wrong_producer_or_prerequisites_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            producer = self.fixture(directory)
            with self.assertRaises(ValueError):
                switch13.producer_save(producer, "cold")
            original = json.loads(producer.read_text())
            mutations = (
                lambda x: x["player"]["secondary_ammo"].__setitem__(6, 0),
                lambda x: x["secondary_fire"].update(next_delta_raw=1),
                lambda x: x["xl_pilot"]["wall_triggers"]["13"].update(operated=True),
                lambda x: x["xl_pilot"]["doorways"]["201"].update(type=5),
                lambda x: x["xl_pilot"]["doorways"]["193"].update(type=4),
            )
            for mutate in mutations:
                changed = copy.deepcopy(original)
                mutate(changed["checkpoint_history"][0]["state"])
                producer.write_text(json.dumps(changed))
                with self.assertRaises(ValueError):
                    switch13.producer_save(producer, "warm")

    def test_pose_stays_on_screened_local_up_axis(self):
        point = switch13.POSE_POINT
        up = [x / 65536 for x in switch13.BASIS["orient_up_fixed"]]

        def position(delta):
            return dict(zip(("x", "y", "z"), (point[i] + delta[i] for i in range(3))))

        self.assertTrue(switch13.pose_envelope(position([0.6 * x for x in up])))
        self.assertFalse(switch13.pose_envelope(position([0.8 * x for x in up])))
        self.assertFalse(switch13.pose_envelope(position([0.1, 0, 0])))

    def test_actor_identity_and_health_mutations(self):
        actor_ids = {210: 20, 282: 29, 283: 29, 314: 52}
        actors = {slot: (actor_ids[slot], 65536, slot + 1, 10 * 65536) for slot in switch13.TRACKED}

        def actor(slot):
            return {
                "object": slot,
                "exists": True,
                "expected_type": 2,
                "expected_id": actor_ids[slot],
                "expected_signature": slot + 1,
                "current_slot": {"type": 2, "id": actor_ids[slot], "signature": slot + 1},
                "damage_since_binding_raw": 0,
                "expected_level": 7,
                "observed_level": 7,
                "expected_load_generation": 2,
                "observed_load_generation": 2,
                "binding_shields_raw": 10 * 65536,
                "actor_state": {"shields_raw": 10 * 65536, "should_be_dead": False, "segment": 1543},
            }

        state = {"combat_observation": {"load_generation": 2, "tracked_objects": [actor(x) for x in switch13.TRACKED]}}
        switch13.actor_guards(state, actors)
        for mutate in (
            lambda s: s["combat_observation"]["tracked_objects"].pop(),
            lambda s: s["combat_observation"]["tracked_objects"][0]["actor_state"].update(segment=1537),
            lambda s: s["combat_observation"]["tracked_objects"][0]["actor_state"].update(shields_raw=1),
            lambda s: s["combat_observation"]["tracked_objects"][0].update(damage_since_binding_raw=1),
        ):
            changed = copy.deepcopy(state)
            mutate(changed)
            with self.assertRaises(ValueError):
                switch13.actor_guards(changed, actors)

    def test_contact_and_operation_mutations(self):
        shot = {"object": 140, "signature": 200, "parent_object": 0, "parent_signature": 1, "weapon_id": 37}
        owner = {"player_object": 0, "player_signature": 1}
        hit = {
            "level": 7,
            "object": 140,
            "signature": 200,
            "weapon_id": 37,
            "parent_object": 0,
            "parent_signature": 1,
            "hit_segment": 1537,
            "hit_side": 2,
            "wall": 203,
            "trigger": 13,
            "base": 137,
            "overlay_raw": 414,
            "eclip": 72,
            "effect_dest": 413,
            "blew_up": 1,
        }
        switch13.contact_guard(hit, shot, owner)
        for key, value in (
            ("object", 141),
            ("signature", 201),
            ("parent_signature", 2),
            ("wall", 205),
            ("base", 41),
            ("eclip", 71),
            ("blew_up", 0),
        ):
            changed = dict(hit, **{key: value})
            with self.assertRaises(ValueError):
                switch13.contact_guard(changed, shot, owner)
        initial = {
            "xl_pilot": {
                "operation_sequence": 1,
                "doorways": {str(i): {"type": 5} for i in switch13.GATES} | {"201": {"type": 4}},
                "wall_triggers": {"13": {"operated": False}, "14": {"operated": True}},
            }
        }
        final = {
            "xl_pilot": {
                "operation_sequence": 2,
                "doorways": {str(i): {"type": 4} for i in switch13.GATES} | {"201": {"type": 4}},
                "wall_triggers": {"13": {"operated": True}, "14": {"operated": True}},
            },
            "combat_observation": {"guided_control": None},
        }
        op = {"index": 13, "shot": 1, "sequence": 2}
        switch13.operation_guard(initial, final, [op])
        for change in (
            lambda f, o: o.append(dict(op)),
            lambda f, o: o[0].update(index=14),
            lambda f, o: f["xl_pilot"]["doorways"]["193"].update(type=5),
            lambda f, o: f["xl_pilot"]["doorways"]["201"].update(type=5),
            lambda f, o: f["xl_pilot"]["wall_triggers"]["14"].update(operated=False),
            lambda f, o: f["xl_pilot"].update(operation_sequence=3),
        ):
            f, o = copy.deepcopy(final), [copy.deepcopy(op)]
            change(f, o)
            with self.assertRaises(ValueError):
                switch13.operation_guard(initial, f, o)

    def test_complete_native_shaped_trace_mutations(self):
        plan = switch13.template("warm")
        state = {"synthetic": True}
        rows = [
            {"step": i, "action": step["action"], "state": state, "simulation_fps": 60}
            for i, step in enumerate(plan["steps"])
        ]
        events = rows + [
            {"event": name}
            for name in ("position_assist", "successful_wall_operation", "trigger_change", "weapon_wall_contacts")
        ]
        receipt = {
            "result": "PASS",
            "receipt_version": 2,
            "resumed_earned_checkpoint": True,
            "initial_level_setup": 7,
            "final_level": 7,
            "rendered": False,
            "transitions": [],
            "steps": 19,
            "position_assist_events": 1,
            "fixed_view_aim_events": 0,
        }
        switch13.trace_guard("warm", receipt, plan, state, events)
        for changed in (
            events[1:],
            events + [rows[0]],
            [dict(events[0], step=1)] + events[1:],
            [dict(events[0], simulation_fps=59)] + events[1:],
            events[:-1],
            events + [{"event": "trigger_change"}],
            events + [{"event": "position_assist"}],
        ):
            with self.assertRaises(ValueError):
                switch13.trace_guard("warm", receipt, plan, state, changed)
        for field, value in (
            ("result", "FAIL"),
            ("steps", 18),
            ("position_assist_events", 2),
            ("fixed_view_aim_events", 1),
        ):
            with self.assertRaises(ValueError):
                switch13.trace_guard("warm", dict(receipt, **{field: value}), plan, state, events)

    def test_all_saved_actor_and_pickup_slots_guarded(self):
        def packet(path, player_position, changed=None, distant=False):
            count = 415 if distant else 315
            base = 128
            data = bytearray(base + count * 264 + 4)
            data[:4] = b"DGSS"
            struct.pack_into("<i", data, 4, 47)
            struct.pack_into("<i", data, base - 4, count)
            data[base + 4] = 4
            struct.pack_into("<h", data, base + 14, 1547)
            struct.pack_into("<3i", data, base + 18, *player_position)
            for slot, kind, ident in ((5, 7, 23), (210, 2, 20), (282, 2, 29), (283, 2, 29), (314, 2, 52)):
                at = base + slot * 264
                struct.pack_into("<i", data, at, slot + 1)
                data[at + 4] = kind
                data[at + 5] = ident
                struct.pack_into("<h", data, at + 14, 1537 if kind == 7 else 1518)
                struct.pack_into("<3i", data, at + 18, 100, 200, 300)
                struct.pack_into("<i", data, at + 66, 65536)
                struct.pack_into("<i", data, at + 70, 10 * 65536)
            if distant:
                at = base + 414 * 264
                struct.pack_into("<i", data, at, 1111)
                data[at + 4 : at + 6] = bytes((7, 1))
                data[at + 11] = 1
                struct.pack_into("<h", data, at + 14, 447)
                struct.pack_into("<3i", data, at + 18, -32000000, 12000000, -13000000)
                struct.pack_into("<3i", data, at + 74, -32000000, 12000000, -13000000)
                struct.pack_into("<2i", data, at + 66, 196608, 1310720)
                struct.pack_into("<i", data, at + 90, 1000000)
                struct.pack_into("<3i", data, at + 94, 32768, 0, 0)
                struct.pack_into("<2i", data, at + 118, 65536, 512)
                struct.pack_into("<H", data, at + 156, 4)
            struct.pack_into("<i", data, base + count * 264, 258)
            if changed:
                changed(data, base)
            path.write_bytes(data)

        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "source.sav"
            output = Path(directory) / "output.sav"
            position = (65536, 131072, 196608)
            state = {"position": {"segment": 1547, "x": 1.0, "y": 2.0, "z": 3.0}}
            packet(source, position)
            packet(output, position)
            switch13.saved_world_guard(source, state, output, state)
            for mutate in (
                lambda d, b: d.__setitem__(b + 5 * 264 + 4, 255),
                lambda d, b: struct.pack_into("<i", d, b + 210 * 264 + 70, 1),
                lambda d, b: struct.pack_into("<i", d, b + 314 * 264, 2),
                lambda d, b: struct.pack_into("<h", d, b + 5 * 264 + 14, 1536),
                lambda d, b: struct.pack_into("<i", d, b + 5 * 264 + 18, 500),
            ):
                packet(output, position, mutate)
                with self.assertRaises(ValueError):
                    switch13.saved_world_guard(source, state, output, state)

            source_state = dict(state, game_time_raw=100000)
            final_state = dict(state, game_time_raw=231072)
            packet(source, position, distant=True)

            def ordinary_motion(data, base):
                at = base + 414 * 264
                struct.pack_into("<3i", data, at + 18, -32000000 + 65536, 12000000, -13000000)
                struct.pack_into("<3i", data, at + 74, -32000000 + 65536, 12000000, -13000000)
                struct.pack_into("<i", data, at + 90, 868928)
                struct.pack_into("<3i", data, at + 94, 26214, 0, 0)

            packet(output, position, ordinary_motion, distant=True)
            switch13.saved_world_guard(source, source_state, output, final_state)
            for mutate in (
                lambda d, b: struct.pack_into("<i", d, b + 414 * 264 + 18, -32000000 + 10 * 65536),
                lambda d, b: d.__setitem__(b + 414 * 264 + 4, 255),
                lambda d, b: d.__setitem__(b + 414 * 264 + 5, 2),
                lambda d, b: struct.pack_into("<i", d, b + 414 * 264, 1112),
                lambda d, b: struct.pack_into("<h", d, b + 414 * 264 + 14, 1537),
                lambda d, b: struct.pack_into("<i", d, b + 414 * 264 + 70, 1),
                lambda d, b: struct.pack_into("<i", d, b + 414 * 264 + 90, 1000000),
                lambda d, b: struct.pack_into("<i", d, b + 414 * 264 + 74, 0),
                lambda d, b: struct.pack_into("<H", d, b + 414 * 264 + 156, 0),
                lambda d, b: struct.pack_into("<3i", d, b + 414 * 264 + 94, 10 * 65536, 0, 0),
            ):

                def changed(data, base, mutate=mutate):
                    ordinary_motion(data, base)
                    mutate(data, base)

                packet(output, position, changed, distant=True)
                with self.assertRaises(ValueError):
                    switch13.saved_world_guard(source, source_state, output, final_state)

            # Strict cold is a separate normal save with no simulation tick.
            packet(output, position, distant=True)
            switch13.saved_world_guard(source, source_state, output, source_state)
            for mutate in (
                lambda d, b: struct.pack_into("<i", d, b + 414 * 264 + 18, -32000000 + 1),
                lambda d, b: struct.pack_into("<i", d, b + 414 * 264 + 94, 32769),
                lambda d, b: struct.pack_into("<i", d, b + 414 * 264 + 74, -32000000 + 1),
                lambda d, b: struct.pack_into("<i", d, b + 414 * 264 + 90, 999999),
                lambda d, b: d.__setitem__(b + 414 * 264 + 4, 255),
            ):
                packet(output, position, mutate, distant=True)
                with self.assertRaises(ValueError):
                    switch13.saved_world_guard(source, source_state, output, source_state)
            packet(output, position, distant=True)
            with self.assertRaises(ValueError):
                switch13.saved_world_guard(source, source_state, output, dict(source_state, game_time_raw=99999))

    def test_native_shaped_normal_save_alias(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile = root / "preview-user"
            profile.mkdir()
            original = "pumo-player-level7-1-step1.sav"
            versioned = "pumo-player-level7-2-step17.sav"
            state = {"synthetic": "saved state"}
            (profile / original).write_bytes(b"source")
            for name in (versioned, "pumo-player-level7.sav", "pumo-player-quick.sav"):
                (profile / name).write_bytes(b"normal saved bytes")
            digest = switch13.sha(profile / versioned)
            history = [{"step": 17, "file": versioned, "sha256": digest, "state": state}]
            receipt = {"checkpoints": {"7": {"file": "pumo-player-level7.sav", "sha256": digest, "state": state}}}
            switch13.switch14.normal_save_guard(root, receipt, history, state, 17, original)
            for mutate in (
                lambda r, h: r["checkpoints"]["7"].update(sha256="0" * 64),
                lambda r, h: h[0].update(file=original),
                lambda r, h: h[0].update(sha256="0" * 64),
            ):
                r, h = copy.deepcopy(receipt), copy.deepcopy(history)
                mutate(r, h)
                with self.assertRaises(ValueError):
                    switch13.switch14.normal_save_guard(root, r, h, state, 17, original)


if __name__ == "__main__":
    unittest.main()
