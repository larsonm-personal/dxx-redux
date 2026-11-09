"""Asset-free negative controls for the guard269 ordinary-combat owner."""

import copy
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import pumo_tulev1_guard269_contract as contract
import pumo_tulev1_master5_contract_test as predecessor_test


def state():
    value = predecessor_test.state(647)
    value["player"]["cloaked"] = True
    value["player"]["has_blue_key"] = True
    value["player"]["robots_killed_level"] = 0
    value["combat_observation"]["tracked_objects"] = [
        {
            "object": 269,
            "expected_id": 28,
            "expected_type": 2,
            "expected_signature": 270,
            "expected_level": 7,
            "observed_level": 7,
            "expected_load_generation": 2,
            "observed_load_generation": 2,
            "binding_shields_raw": 200 * 65536,
            "damage_since_binding_raw": 0,
            "exists": True,
            "current_slot": {"id": 28, "signature": 270, "type": 2},
            "actor_state": {"shields_raw": 200 * 65536},
        }
    ]
    return value


class Guard269ContractTest(unittest.TestCase):
    def test_canonical_plan_is_bounded_ordinary_input(self):
        plan = contract.template("ordinary")
        actions = plan["steps"]
        self.assertEqual(len(actions), 42)
        self.assertEqual(plan["track_objects"], [269])
        self.assertEqual(plan["simulation_fps"], 60)
        self.assertFalse(plan["realtime"])
        self.assertTrue(plan["observe_weapon_wall_contacts"])
        self.assertEqual(actions[7]["action"], "pose_view")
        self.assertEqual(actions[7]["segment"], 647)
        self.assertEqual(actions[8], {"action": "introspect"})
        self.assertEqual(actions[9], {"action": "send_button", "button": 100, "held": 1})
        self.assertEqual(actions[38], {"action": "send_button", "button": 100, "held": 0})
        self.assertEqual(actions[-1], {"action": "set_debug", "field": "android_game_request", "value": "quick_save"})
        self.assertEqual(sum(step.get("ms", 0) for step in actions), 4793)
        self.assertEqual(
            [step["ms"] for step in actions if step["action"] == "wait_ms"], [1000, 1] + [234] * 13 + [750]
        )
        self.assertEqual([step["action"] for step in actions].count("introspect"), 16)
        self.assertFalse(
            any(
                step["action"] in {"wait_for", "select_secondary", "select_primary", "load", "save", "face_view"}
                for step in actions
            )
        )
        self.assertNotIn("resume_sha256", plan)
        self.assertNotIn("resume_checkpoint", plan)
        self.assertEqual(contract.template("cold")["steps"], [{"action": "introspect"}])

    def test_kill_requires_bound_target_with_negative_shields(self):
        bound = contract.tracked(state())
        killed = copy.deepcopy(bound)
        killed["damage_since_binding_raw"] = 205 * 65536
        killed["actor_state"]["shields_raw"] = -5 * 65536
        contract.guard_killed(bound, killed)
        for replacement in (
            {"exists": False},
            {"damage_since_binding_raw": 200 * 65536},
            {"actor_state": {"shields_raw": 0}},
            {"actor_state": {"shields_raw": 1}},
        ):
            with self.subTest(replacement=replacement):
                forged = copy.deepcopy(killed)
                forged.update(replacement)
                with self.assertRaises(ValueError):
                    contract.guard_killed(bound, forged)

    def test_valid_actor_and_tracker(self):
        value = state()
        self.assertEqual(contract.actor(value)["robots_killed_level"], 0)
        self.assertEqual(contract.tracked(value)["object"], 269)

    def test_reject_guard_identity_or_provenance_forgeries(self):
        changes = [
            ("object", 268),
            ("object", 269.0),
            ("expected_id", 28.0),
            ("expected_type", 2.0),
            ("expected_level", 7.0),
            ("observed_level", 7.0),
            ("exists", 1),
            ("expected_id", 29),
            ("expected_type", 3),
            ("expected_signature", 0),
            ("expected_signature", 271),
            ("expected_signature", True),
            ("expected_level", 6),
            ("observed_level", 6),
            ("expected_load_generation", 1),
            ("observed_load_generation", 1),
            ("binding_shields_raw", 0),
            ("binding_shields_raw", -1),
            ("binding_shields_raw", 199 * 65536),
            ("damage_since_binding_raw", -1),
            ("damage_since_binding_raw", 1),
            ("current_slot", {"id": 28, "signature": 54, "type": 2}),
            ("actor_state", {"shields_raw": "dead"}),
        ]
        for field, replacement in changes:
            with self.subTest(field=field, replacement=replacement):
                forged = state()
                forged["combat_observation"]["tracked_objects"][0][field] = replacement
                with self.assertRaises((ValueError, KeyError, TypeError)):
                    contract.tracked(forged)

    def test_reject_duplicate_or_missing_tracker(self):
        for replacement in ([], [state()["combat_observation"]["tracked_objects"][0]] * 2):
            forged = state()
            forged["combat_observation"]["tracked_objects"] = replacement
            with self.assertRaises(ValueError):
                contract.tracked(forged)

    def test_reject_guided_and_resource_grants(self):
        variants = [
            ("combat_observation", "guided_control", {"object": 3}),
            ("player", "invulnerable", True),
            ("player", "has_blue_key", False),
            ("player", "secondary_ammo", [7, 0, 0, 0, 0, 0, 0, 0, 0, 0]),
            ("player", "secondary_weapon_flags", 64),
            ("player", "shields", 0),
            ("player", "cloaked", False),
        ]
        for group, field, replacement in variants:
            with self.subTest(field=field):
                forged = state()
                forged[group][field] = replacement
                with self.assertRaises(ValueError):
                    contract.actor(forged)

    def test_versioned_save_rejects_alias_and_state_forgeries(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "preview-user").mkdir()
            version = "pumo-player-level7-123-step41.sav"
            alias = "pumo-player-level7.sav"
            for filename in (version, alias):
                (root / "preview-user" / filename).write_bytes(b"fixture")
            final = state()
            digest = contract.sha(root / "preview-user" / version)
            saved = {"step": 41, "file": version, "sha256": digest, "state": final}
            receipt = {
                "checkpoint_history": [saved],
                "checkpoints": {"7": {"file": alias, "sha256": digest, "state": final}},
            }
            self.assertEqual(contract.checkpoint(root, receipt, final), saved)
            variants = [
                ("checkpoint_history", 0, "step", 40),
                ("checkpoint_history", 0, "file", alias),
                ("checkpoint_history", 0, "sha256", "0" * 64),
                ("checkpoint_history", 0, "state", {}),
                ("checkpoints", "7", "file", version),
                ("checkpoints", "7", "sha256", "0" * 64),
                ("checkpoints", "7", "state", {}),
            ]
            for group, index, field, replacement in variants:
                with self.subTest(group=group, field=field):
                    forged = copy.deepcopy(receipt)
                    forged[group][index][field] = replacement
                    with self.assertRaises(ValueError):
                        contract.checkpoint(root, forged, final)
            for history in ([], [saved, saved]):
                with self.subTest(length=len(history)):
                    forged = copy.deepcopy(receipt)
                    forged["checkpoint_history"] = history
                    with self.assertRaises(ValueError):
                        contract.checkpoint(root, forged, final)

    def test_bundle_rejects_receipt_and_trace_forgeries(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "preview-user").mkdir()
            engine = root / "engine"
            engine.write_bytes(b"fixture engine")
            filename = "source.sav"
            (root / "preview-user" / filename).write_bytes(b"fixture source")
            plan = contract.template("ordinary")
            plan.update(
                {
                    "resume_checkpoint": filename,
                    "resume_sha256": contract.sha(root / "preview-user" / filename),
                    "receipt_run_id": "a" * 32,
                }
            )
            plan_path = root / "ordinary-executed-plan.json"
            plan_path.write_text(json.dumps(plan) + "\n")
            final = state()
            (root / "ordinary-state.json").write_text(json.dumps(final) + "\n")
            trace = [
                {"step": index, "action": step["action"], "state": final, "simulation_fps": 60}
                for index, step in enumerate(plan["steps"])
            ]
            trace_path = root / "ordinary-trace.jsonl"
            receipt = {
                "receipt_version": 2,
                "steps": 42,
                "initial_level_setup": 7,
                "final_level": 7,
                "fixed_view_aim_events": 0,
                "position_assist_events": 1,
                "result": "PASS",
                "resumed_earned_checkpoint": True,
                "transitions": [],
                "rendered": False,
                "process_id": 42,
                "run_id": "a" * 32,
                "plan_sha256": contract.sha(plan_path),
                "executable_sha256": contract.ENGINE_SHA,
                "resume_sha256": plan["resume_sha256"],
            }
            result_path = root / "ordinary-result.json"
            real_sha = contract.sha

            def fixture_sha(path):
                return contract.ENGINE_SHA if path == engine else real_sha(path)

            def write():
                result_path.write_text(json.dumps(receipt) + "\n")
                trace_path.write_text("".join(json.dumps(event) + "\n" for event in trace))

            with patch.object(contract, "sha", side_effect=fixture_sha):
                write()
                contract.bundle("ordinary", root, engine)
                variants = [
                    ("result", "FAIL"),
                    ("steps", 41),
                    ("process_id", 0),
                    ("run_id", "b" * 32),
                    ("plan_sha256", "0" * 64),
                    ("executable_sha256", "0" * 64),
                    ("resume_sha256", "0" * 64),
                    ("rendered", True),
                    ("position_assist_events", 0),
                    ("resumed_earned_checkpoint", False),
                ]
                for field, replacement in variants:
                    with self.subTest(field=field):
                        original = receipt[field]
                        receipt[field] = replacement
                        write()
                        with self.assertRaises(ValueError):
                            contract.bundle("ordinary", root, engine)
                        receipt[field] = original
                write()
                for forged in (
                    trace[:-1],
                    [dict(trace[0], action="load")] + trace[1:],
                    [dict(trace[0], simulation_fps=30)] + trace[1:],
                    trace[:-1] + [dict(trace[-1], state={})],
                    [dict(trace[0], step=1)] + trace[1:],
                    trace + [{"event": "successful_wall_operation"}],
                ):
                    with self.subTest(trace_forgery=len(forged)):
                        trace_path.write_text("".join(json.dumps(event) + "\n" for event in forged))
                        with self.assertRaises(ValueError):
                            contract.bundle("ordinary", root, engine)

    def test_source_requires_blue_phase_and_uses_predecessor_witness(self):
        with self.assertRaises(ValueError):
            contract.source(Path("wrong-result.json"), Path("engine"))
        with (
            patch.object(contract.predecessor, "phase") as phase,
            patch.object(contract.predecessor, "producer", return_value=({}, {})),
        ):
            contract.source(Path("root/blue/blue-result.json"), Path("engine"))
            phase.assert_called_once_with(
                "blue", Path("root/blue"), Path("engine"), Path("root/cloak/cloak-result.json")
            )


if __name__ == "__main__":
    unittest.main()
