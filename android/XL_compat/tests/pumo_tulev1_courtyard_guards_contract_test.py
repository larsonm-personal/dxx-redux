"""Asset-free negative checks for the earned courtyard guard owner."""

import copy
import json
import tempfile
import unittest
from pathlib import Path

import pumo_tulev1_courtyard_guards_contract as courtyard


class CourtyardContractTest(unittest.TestCase):
    def cold_fixture(self, root):
        root = Path(root)
        source = root / "source"
        phase = root / "phase"
        (source / "preview-user").mkdir(parents=True)
        phase.mkdir()
        engine = root / "engine"
        engine.write_bytes(b"pinned fixture engine")
        doors = {str(i): {"type": 5} for i in (55, 56, 57, 58)}
        doors.update({str(i): {"flags": 17} for i in (136, 137)})
        saved = {
            "current_level_num": 7,
            "game_time_raw": 1000,
            "simulation_rng": {"call_count": 9, "seed": 1},
            "player_dead": False,
            "cheats_enabled": False,
            "player": {
                "invulnerable": False,
                "quad_lasers": True,
                "primary_weapon": 0,
                "laser_level": 0,
                "has_blue_key": True,
                "has_red_key": False,
                "has_gold_key": False,
                "secondary_ammo": [7] + [0] * 9,
                "shields": 50,
                "robots_killed_level": 3,
            },
            "xl_pilot": {
                "remaining_objectives": 1,
                "wall_triggers": {"6": {"operated": False}},
                "doorways": doors,
                "operation_counts": {},
                "operation_sequence": 2,
                "trigger_activated": {"6": True},
            },
            "combat_observation": {"guided_control": None, "tracked_objects": [{"object": 256}]},
        }
        filename = "pumo-player-level7-1-step44.sav"
        save_path = source / "preview-user" / filename
        save_path.write_bytes(b"fixture save")
        source_receipt = {
            "result": "PASS",
            "run_id": "a" * 32,
            "process_id": 1,
            "checkpoint_history": [{"step": 44, "file": filename, "sha256": courtyard.sha(save_path), "state": saved}],
        }
        producer = source / "guard256-result.json"
        producer.write_text(json.dumps(source_receipt))
        final = courtyard.restored(saved)
        final["combat_observation"]["tracked_objects"] = []
        plan = courtyard.template(256, "cold")
        plan.update(resume_checkpoint=filename, resume_sha256=courtyard.sha(save_path), receipt_run_id="b" * 32)
        plan_path = phase / "guard256_cold-executed-plan.json"
        plan_path.write_text(json.dumps(plan))
        receipt = {
            "result": "PASS",
            "receipt_version": 2,
            "resumed_earned_checkpoint": True,
            "initial_level_setup": 7,
            "final_level": 7,
            "rendered": False,
            "transitions": [],
            "steps": 1,
            "position_assist_events": 0,
            "fixed_view_aim_events": 0,
            "resume_sha256": courtyard.sha(save_path),
            "plan_sha256": courtyard.sha(plan_path),
            "executable_sha256": courtyard.sha(engine),
            "run_id": "b" * 32,
            "process_id": 2,
            "checkpoint_history": [],
            "checkpoints": {},
        }
        receipt_path = phase / "guard256_cold-result.json"
        receipt_path.write_text(json.dumps(receipt))
        state_path = phase / "guard256_cold-state.json"
        state_path.write_text(json.dumps(final))
        row = {"step": 0, "action": "introspect", "state": final, "simulation_fps": 60}
        trace_path = phase / "guard256_cold-trace.jsonl"
        trace_path.write_text(json.dumps(row) + "\n")
        return phase, engine, producer, receipt_path, state_path, trace_path

    def test_full_cold_verify_and_fail_closed_mutations(self):
        with tempfile.TemporaryDirectory() as temporary:
            phase, engine, producer, receipt_path, state_path, trace_path = self.cold_fixture(temporary)
            courtyard.verify("guard256_cold", phase, engine, producer)
            receipt = json.loads(receipt_path.read_text())
            row = json.loads(trace_path.read_text())
            for field, value in (
                ("position_assist_events", 1),
                ("fixed_view_aim_events", 1),
                ("checkpoint_history", [{}]),
                ("process_id", 1),
                ("run_id", "a" * 32),
            ):
                bad = copy.deepcopy(receipt)
                bad[field] = value
                receipt_path.write_text(json.dumps(bad))
                with self.subTest(field=field), self.assertRaises((ValueError, KeyError)):
                    courtyard.verify("guard256_cold", phase, engine, producer)
            receipt_path.write_text(json.dumps(receipt))
            for field, value in (("simulation_fps", 59), ("simulation_fps", None), ("extra", 1)):
                bad = copy.deepcopy(row)
                bad[field] = value
                trace_path.write_text(json.dumps(bad) + "\n")
                with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                    courtyard.verify("guard256_cold", phase, engine, producer)
            trace_path.write_text(json.dumps(row) + "\n")
            altered = copy.deepcopy(row["state"])
            altered["xl_pilot"]["doorways"]["55"]["type"] = 4
            state_path.write_text(json.dumps(altered))
            trace_path.write_text(json.dumps(dict(row, state=altered)) + "\n")
            with self.assertRaises(ValueError):
                courtyard.verify("guard256_cold", phase, engine, producer)
            with self.assertRaises(ValueError):
                courtyard.verify("guard305_cold", phase, engine, producer)

    def test_public_phases_are_bounded_and_do_not_embed_private_resume(self):
        for actor in (256, 305):
            warm = courtyard.template(actor, "warm")
            cold = courtyard.template(actor, "cold")
            for key in ("resume_checkpoint", "resume_sha256"):
                self.assertNotIn(key, warm)
                self.assertNotIn(key, cold)
            self.assertEqual(warm["track_objects"], [actor])
            self.assertEqual(warm["steps"][6]["segment"], 449 if actor == 256 else 450)
            self.assertEqual(
                warm["steps"][-1], {"action": "set_debug", "field": "android_game_request", "value": "quick_save"}
            )
            self.assertEqual(cold["steps"], [{"action": "introspect"}])
        self.assertEqual(len(courtyard.template(256, "warm")["steps"]), 45)
        self.assertEqual(len(courtyard.template(305, "warm")["steps"]), 20)
        self.assertEqual(courtyard.template(305, "warm")["steps"][13]["timeout_ms"], 2483)

    def test_original_actor_binding_rejects_signature_generation_and_damage_identity(self):
        item = {
            "object": 256,
            "expected_id": 28,
            "expected_signature": 257,
            "expected_type": 2,
            "expected_level": 7,
            "observed_level": 7,
            "expected_load_generation": 2,
            "observed_load_generation": 2,
            "exists": True,
            "current_slot": {"id": 28, "signature": 257, "type": 2},
            "actor_state": {"shields_raw": 10},
            "binding_shields_raw": 10,
            "damage_since_binding_raw": 0,
        }
        state = {"combat_observation": {"load_generation": 2, "tracked_objects": [item]}}
        courtyard.actor_binding(state, 256, True)
        for path, value in (
            ("expected_signature", 999),
            ("expected_load_generation", 1),
            ("object", 305),
            ("exists", False),
        ):
            bad = copy.deepcopy(state)
            bad["combat_observation"]["tracked_objects"][0][path] = value
            with self.subTest(path=path), self.assertRaises(ValueError):
                courtyard.actor_binding(bad, 256, True)
        bad = copy.deepcopy(state)
        bad["combat_observation"]["tracked_objects"][0]["current_slot"]["signature"] = 999
        with self.assertRaises(ValueError):
            courtyard.actor_binding(bad, 256, True)

    def test_ordinary_rejects_gate_switch_weapon_and_death(self):
        pilot = {
            "remaining_objectives": 1,
            "wall_triggers": {"6": {"operated": False}},
            "doorways": {str(i): {"type": 5} for i in (55, 56, 57, 58)},
        }
        pilot["doorways"].update({str(i): {"flags": 17} for i in (136, 137)})
        state = {
            "current_level_num": 7,
            "player_dead": False,
            "cheats_enabled": False,
            "player": {
                "invulnerable": False,
                "quad_lasers": True,
                "primary_weapon": 0,
                "laser_level": 0,
                "has_blue_key": True,
                "has_red_key": False,
                "has_gold_key": False,
                "secondary_ammo": [7] + [0] * 9,
                "shields": 50,
                "robots_killed_level": 2,
            },
            "xl_pilot": pilot,
            "combat_observation": {"guided_control": None},
        }
        courtyard.ordinary(state, 2)
        for field, value in (
            ("primary_weapon", 1),
            ("quad_lasers", False),
            ("invulnerable", True),
            ("has_blue_key", False),
        ):
            bad = copy.deepcopy(state)
            bad["player"][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                courtyard.ordinary(bad, 2)
        for mutation in (
            lambda x: x["xl_pilot"]["wall_triggers"]["6"].update(operated=True),
            lambda x: x["xl_pilot"]["doorways"]["55"].update(type=4),
            lambda x: x.update(player_dead=True),
        ):
            bad = copy.deepcopy(state)
            mutation(bad)
            with self.assertRaises(ValueError):
                courtyard.ordinary(bad, 2)


if __name__ == "__main__":
    unittest.main()
