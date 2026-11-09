"""Asset-free synthetic checks for the registered supplies source contract."""

import copy
import json
import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import pumo_tulev1_supplies_contract as supplies


class SuppliesContractTest(unittest.TestCase):
    def fixture(self, root, name, step, state=None):
        root = Path(root)
        (root / "preview-user").mkdir(parents=True)
        state = state or {
            "game_time_raw": 900,
            "position": {"segment": 450, "x": 2.5, "y": -1.25, "z": 3.0},
            "player": {"shields": 60, "energy": 80},
        }
        filename = f"pumo-player-level7-1-step{step}.sav"
        saved = root / "preview-user" / filename
        saved.write_bytes(b"synthetic fixture save")
        path = root / name
        path.write_text(
            json.dumps(
                {
                    "result": "PASS",
                    "process_id": 1,
                    "run_id": "a" * 32,
                    "checkpoint_history": [
                        {
                            "step": step,
                            "file": filename,
                            "sha256": supplies.sha(saved),
                            "state": state,
                        }
                    ],
                }
            )
        )
        return path

    def test_templates_use_only_declared_controls(self):
        for stage in supplies.STAGES:
            warm = supplies.template(f"{stage}_warm")
            cold = supplies.template(f"{stage}_cold")
            self.assertEqual(len(warm["steps"]), supplies.STEPS[stage])
            self.assertEqual([step["action"] for step in cold["steps"]], ["assert", "set_debug", "introspect"])
            self.assertEqual(sum(step["action"] == "pose_view" for step in warm["steps"]), 1)
            self.assertEqual(sum(step["action"] == "set_debug" for step in warm["steps"]), 1)
            self.assertEqual(sum(step["action"] == "wait_ms" for step in cold["steps"]), 0)
            for step in warm["steps"] + cold["steps"]:
                self.assertNotIn(step["action"], {"grant", "fire_trigger", "damage_object", "freeze_actor"})
                if step["action"] == "set_debug":
                    self.assertEqual((step["field"], step["value"]), ("android_game_request", "quick_save"))
        for stage in ("cloak113", "guided140"):
            pose = next(step for step in supplies.template(f"{stage}_warm")["steps"] if step["action"] == "pose_view")
            self.assertEqual(pose["position_fixed"], [0, 0, 0])

    def test_same_position_binding_uses_actual_predecessor(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            producer = self.fixture(root, "switch6_cold-result.json", 1)
            plan = supplies.template("cloak113_warm")
            prior = json.loads(producer.read_text())["checkpoint_history"][0]
            plan.update(resume_checkpoint=prior["file"], resume_sha256=prior["sha256"])
            bound = supplies.binding("cloak113_warm", copy.deepcopy(plan), producer)
            pose = next(step for step in bound["steps"] if step["action"] == "pose_view")
            self.assertEqual(pose["position_fixed"], [163840, -81920, 196608])
            self.assertEqual(bound["checkpoint_expect"]["game_time_raw"], 900)
            changed = copy.deepcopy(plan)
            changed["steps"][0]["expect"]["player.shields"] = 99
            with self.assertRaises(ValueError):
                supplies.binding("cloak113_warm", changed, producer)

    def test_immediate_predecessor_and_save_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            producer = self.fixture(root, "cloak113_cold-result.json", 1)
            supplies.producer_save(producer, "guided140_warm")
            with self.assertRaises(ValueError):
                supplies.producer_save(producer, "cloak113_warm")
            changed = json.loads(producer.read_text())
            changed["checkpoint_history"][0]["sha256"] = "0" * 64
            producer.write_text(json.dumps(changed))
            with self.assertRaises(ValueError):
                supplies.producer_save(producer, "guided140_warm")

    def test_actor_parser_rejects_unidentified_table(self):
        with tempfile.TemporaryDirectory() as directory:
            saved = Path(directory) / "synthetic.sav"
            saved.write_bytes(b"not a serialized actor table")
            with self.assertRaises(ValueError):
                supplies.saved_actor_radii(saved, {"position": {"segment": 7, "x": 0, "y": 0, "z": 0}})

    def test_actor_parser_reads_all_synthetic_slots(self):
        with tempfile.TemporaryDirectory() as directory:
            saved = Path(directory) / "synthetic.sav"
            base = 40
            raw = bytearray(base + 215 * 264)
            raw[:4] = b"DGSS"
            struct.pack_into("<i", raw, 4, 47)
            struct.pack_into("<i", raw, base - 4, 215)
            raw[base + 4] = 4
            struct.pack_into("<h", raw, base + 14, 7)
            struct.pack_into("<3i", raw, base + 18, 65536, 2 * 65536, -3 * 65536)
            for slot, ident, radius in ((162, 8, 5 * 65536), (214, 20, 8 * 65536)):
                offset = base + slot * 264
                raw[offset + 4] = 2
                raw[offset + 5] = ident
                struct.pack_into("<i", raw, offset, slot + 1)
                struct.pack_into("<i", raw, offset + 66, radius)
                struct.pack_into("<i", raw, offset + 70, 20 * 65536)
            saved.write_bytes(raw)
            state = {"position": {"segment": 7, "x": 1, "y": 2, "z": -3}}
            self.assertEqual(
                supplies.saved_actor_radii(saved, state),
                {162: (8, 5 * 65536, 163, 20 * 65536), 214: (20, 8 * 65536, 215, 20 * 65536)},
            )
            saved.write_bytes(raw + raw[base - 4 :])
            with self.assertRaises(ValueError):
                supplies.saved_actor_radii(saved, state)
            bounded = bytearray(raw)
            struct.pack_into("<i", bounded, base - 4, 32769)
            saved.write_bytes(bounded)
            with self.assertRaises(ValueError):
                supplies.saved_actor_radii(saved, state)
            saved.write_bytes(raw)
            raw[base + 214 * 264 + 66 : base + 214 * 264 + 70] = b"\0" * 4
            saved.write_bytes(raw)
            with self.assertRaises(ValueError):
                supplies.saved_actor_radii(saved, state)
            struct.pack_into("<i", raw, 4, 46)
            saved.write_bytes(raw)
            with self.assertRaises(ValueError):
                supplies.saved_actor_radii(saved, state)

    def test_actor_clearance_rejects_missing_identity_and_collision(self):
        def tracked(slot, ident, signature, x):
            return {
                "object": slot,
                "exists": True,
                "expected_type": 2,
                "expected_id": ident,
                "expected_signature": signature,
                "current_slot": {"type": 2, "id": ident, "signature": signature},
                "damage_since_binding_raw": 0,
                "actor_state": {
                    "shields_raw": 10 * 65536,
                    "should_be_dead": False,
                    "position_fixed": [x * 65536, 0, 0],
                },
            }

        state = {
            "position": {"x": 0, "y": 0, "z": 0, "radius_raw": 65536},
            "level_scene": {
                "robots": [
                    {"object": 162, "id": 8, "position_fixed": [100 * 65536, 0, 0]},
                    {"object": 214, "id": 20, "position_fixed": [200 * 65536, 0, 0]},
                ]
            },
            "combat_observation": {"tracked_objects": [tracked(162, 8, 163, 100), tracked(214, 20, 215, 200)]},
        }
        radii = {162: (8, 65536, 163, 10 * 65536), 214: (20, 65536, 215, 10 * 65536)}
        with patch.object(supplies, "saved_actor_radii", return_value=radii):
            supplies.actor_hulls_clear(
                [{"state": state}], Path("producer.json"), {"file": "synthetic.sav", "state": {}}
            )
            changed = copy.deepcopy(state)
            changed["combat_observation"]["tracked_objects"].pop()
            with self.assertRaises(ValueError):
                supplies.actor_hulls_clear(
                    [{"state": changed}], Path("producer.json"), {"file": "synthetic.sav", "state": {}}
                )

            changed = copy.deepcopy(state)
            changed["level_scene"]["robots"][0]["position_fixed"][0] = 40 * 65536
            with self.assertRaises(ValueError):
                supplies.actor_hulls_clear(
                    [{"state": changed}], Path("producer.json"), {"file": "synthetic.sav", "state": {}}
                )
            changed = copy.deepcopy(state)
            changed["level_scene"]["robots"].pop(0)
            changed["combat_observation"]["tracked_objects"][0]["actor_state"]["position_fixed"][0] = 40 * 65536
            with self.assertRaises(ValueError):
                supplies.actor_hulls_clear(
                    [{"state": changed}], Path("producer.json"), {"file": "synthetic.sav", "state": {}}
                )
            changed = copy.deepcopy(state)
            changed["combat_observation"]["tracked_objects"][0]["actor_state"]["shields_raw"] -= 1
            with self.assertRaises(ValueError):
                supplies.actor_hulls_clear(
                    [{"state": changed}], Path("producer.json"), {"file": "synthetic.sav", "state": {}}
                )

    def test_cold_save_parity_catches_hidden_byte_change(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            before, after = root / "before", root / "after"
            (before / "preview-user").mkdir(parents=True)
            (after / "preview-user").mkdir(parents=True)
            base, count = 40, 3  # synthetic table; no run-derived offsets or slots
            raw = bytearray(base + count * 264 + 12)
            raw[:4] = b"DGSS"
            struct.pack_into("<i", raw, 4, 47)
            struct.pack_into("<i", raw, base - 4, count)
            raw[base + 4] = 4
            struct.pack_into("<h", raw, base + 14, 7)
            struct.pack_into("<3i", raw, base + 18, 65536, 0, 0)
            raw[base + 264 + 4] = 2
            slot = base + 2 * 264
            raw[slot + 4] = 255
            raw[slot + 12] = 1
            source_path, target_path = before / "preview-user/synthetic.sav", after / "preview-user/synthetic.sav"
            source_path.write_bytes(raw)
            state = {"position": {"segment": 7, "x": 1, "y": 0, "z": 0}}
            source = {"file": source_path.name, "sha256": supplies.sha(source_path), "state": state}

            def check(mutated, accepted=False):
                target_path.write_bytes(mutated)
                successor = {"file": target_path.name, "sha256": supplies.sha(target_path), "state": state}
                if accepted:
                    supplies.cold_save_parity(source, successor, before, after)
                else:
                    with self.assertRaises(ValueError):
                        supplies.cold_save_parity(source, successor, before, after)

            check(raw, accepted=True)
            normalized = bytearray(raw)
            struct.pack_into("<i", normalized, slot + 260, -1)
            check(normalized, accepted=True)
            changed = bytearray(normalized)
            changed[-1] = 1  # hidden payload outside the object table
            check(changed)
            changed = bytearray(normalized)
            changed[slot + 70] = 1  # another field in the same inactive slot
            check(changed)
            changed = bytearray(normalized)
            changed[slot + 4] = 2  # a live object cannot claim the exception
            check(changed)
            changed = bytearray(normalized)
            changed[slot + 12] = 0  # wrong render union
            check(changed)
            changed = bytearray(raw)
            struct.pack_into("<i", changed, slot + 260, -2)  # wrong value
            check(changed)
            changed = bytearray(raw)
            struct.pack_into("<i", changed, base + 264 + 260, -1)  # live-slot field
            check(changed)
            check(normalized + b"x")  # extra byte
            check(normalized[:-1])  # missing byte
            changed = bytearray(normalized)
            struct.pack_into("<i", changed, 4, 46)  # unsupported version
            check(changed)
            changed = bytearray(normalized)
            struct.pack_into("<i", changed, base - 4, count + 1)  # count drift
            check(changed)
            check(normalized + normalized[base - 4 :])  # ambiguous table
            target_path.write_bytes(normalized)
            wrong_hash = {"file": target_path.name, "sha256": "0" * 64, "state": state}
            with self.assertRaises(ValueError):
                supplies.cold_save_parity(source, wrong_hash, before, after)

    def test_unplanned_trace_events_are_rejected(self):
        supplies.trace_events_allowed([], "guided140", "cold")
        with self.assertRaises(ValueError):
            supplies.trace_events_allowed([{"event": "actor_freeze"}], "guided140", "cold")
        expected = [{"event": "position_assist"}]
        supplies.trace_events_allowed(expected, "guided140", "warm")
        with self.assertRaises(ValueError):
            supplies.trace_events_allowed(expected + [{"event": "actor_damage"}], "guided140", "warm")
        with self.assertRaises(ValueError):
            supplies.trace_events_allowed(expected + [{"other": "unclassified"}], "guided140", "warm")


if __name__ == "__main__":
    unittest.main()
