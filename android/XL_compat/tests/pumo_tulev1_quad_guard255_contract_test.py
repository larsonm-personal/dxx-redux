"""Asset-free forgery controls for assisted-local Quad and guard255 owner."""

import copy
import json
import tempfile
import unittest
from pathlib import Path

import pumo_tulev1_guard269_contract_test as prior_test
import pumo_tulev1_quad_guard255_contract as contract


def state():
    value = prior_test.state()
    value["player"]["quad_lasers"] = True
    value["player"]["robots_killed_level"] = 1
    item = value["combat_observation"]["tracked_objects"][0]
    item["object"] = 255
    item["expected_signature"] = 256
    item["current_slot"]["signature"] = 256
    return value


class QuadGuard255ContractTest(unittest.TestCase):
    def test_bounded_public_templates(self):
        quad = contract.template("quad")
        combat = contract.template("guard255")
        cold = contract.template("cold")
        self.assertEqual(len(quad["steps"]), 9)
        self.assertEqual(len(combat["steps"]), 45)
        self.assertEqual(cold["steps"], [{"action": "introspect"}])
        self.assertEqual(combat["track_objects"], [255])
        self.assertTrue(combat["observe_weapon_wall_contacts"])
        self.assertEqual(combat["steps"][6]["action"], "pose_view")
        self.assertEqual(combat["steps"][8], {"action": "send_button", "button": 100, "held": 1})
        self.assertEqual(combat["steps"][25], {"action": "send_button", "button": 100, "held": 0})
        self.assertEqual(
            combat["steps"][44],
            {
                "action": "set_debug",
                "field": "android_game_request",
                "value": "quick_save",
            },
        )
        self.assertEqual([s["action"] for s in combat["steps"]].count("wait_for"), 8)
        self.assertEqual([s["action"] for s in combat["steps"]].count("introspect"), 18)
        self.assertEqual([s["ms"] for s in combat["steps"] if s["action"] == "wait_ms"], [67] * 9)
        self.assertTrue(
            all(
                s.get("face_object") == 255 and s.get("face_interval_ms") == 50
                for s in combat["steps"]
                if s["action"] == "wait_for"
            )
        )
        self.assertEqual(
            [s["expect"]["game_time_raw"]["gte"] for s in combat["steps"] if s["action"] == "wait_for"],
            [0] * 8,
        )
        self.assertFalse(
            any(
                s["action"] in {"load", "save", "face_view", "select_secondary", "set_invulnerable"}
                for s in combat["steps"]
            )
        )
        for plan in (quad, combat, cold):
            self.assertNotIn("resume_checkpoint", plan)
            self.assertNotIn("resume_sha256", plan)
            self.assertNotIn("player.shields", plan.get("checkpoint_expect", {}))

    def test_source_relative_view_binding_rejects_forged_producer(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            plan_path = root / "guard255-plan.json"
            producer = root / "quad-result.json"
            plan = contract.template("guard255")
            plan.update(resume_checkpoint="source.sav", resume_sha256="a" * 64)
            receipt = {"checkpoint_history": [{"step": 7, "state": {"game_time_raw": 1200000}}]}
            plan_path.write_text(json.dumps(plan))
            producer.write_text(json.dumps(receipt))
            contract.bind(plan_path, producer)
            bound = contract.doc(plan_path)
            self.assertEqual(
                [bound["steps"][9 + 2 * i]["expect"]["game_time_raw"]["gte"] for i in range(8)],
                [1200000 + (1 + 15 * i) * 65536 // 60 for i in range(8)],
            )
            for replacement in (-1, "clock", True):
                forged = copy.deepcopy(receipt)
                forged["checkpoint_history"][0]["state"]["game_time_raw"] = replacement
                producer.write_text(json.dumps(forged))
                plan_path.write_text(json.dumps(plan))
                with (
                    self.subTest(replacement=replacement),
                    self.assertRaises(ValueError),
                ):
                    contract.bind(plan_path, producer)

    def test_actor_rejects_quad_or_guided_forgery(self):
        good = state()
        contract.actor(good, True)
        for group, field, replacement in [
            ("player", "quad_lasers", False),
            ("player", "secondary_ammo", [7, 0, 0, 0, 0, 0, 0, 0, 0, 0]),
            ("player", "invulnerable", True),
            ("combat_observation", "guided_control", {"object": 6}),
        ]:
            forged = copy.deepcopy(good)
            forged[group][field] = replacement
            with self.subTest(field=field), self.assertRaises(ValueError):
                contract.actor(forged, True)

    def test_tracker_identity_and_immutable_negative_damage(self):
        good = state()
        self.assertEqual(contract.tracked(good)["object"], 255)
        killed = copy.deepcopy(good)
        item = killed["combat_observation"]["tracked_objects"][0]
        item["damage_since_binding_raw"] = 201 * 65536
        item["actor_state"]["shields_raw"] = -65536
        contract.damage_witness([good, killed])
        for field, replacement in [
            ("object", 269),
            ("expected_id", 27),
            ("expected_signature", 270),
            ("binding_shields_raw", 199 * 65536),
            ("exists", 1),
            ("current_slot", {"id": 28, "signature": 270, "type": 2}),
        ]:
            forged = copy.deepcopy(good)
            forged["combat_observation"]["tracked_objects"][0][field] = replacement
            with self.subTest(field=field), self.assertRaises(ValueError):
                contract.tracked(forged)
        disappeared = copy.deepcopy(good)
        disappeared["combat_observation"]["tracked_objects"][0]["exists"] = False
        with self.assertRaises(ValueError):
            contract.damage_witness([good, disappeared])
        contract.damage_witness([good, killed, disappeared])
        contract.final_nonliving(contract.tracked(killed))
        contract.final_nonliving(contract.tracked(disappeared))
        with self.assertRaises(ValueError):
            contract.final_nonliving(contract.tracked(good))

    def test_fixed_view_cadence_and_target_identity_forgeries(self):
        target = state()["combat_observation"]["tracked_objects"][0]
        target["actor_state"]["should_be_dead"] = False
        first = 1200000
        events = [
            {
                "event": "fixed_view_aim",
                "forward_fixed": [1, 0, 0],
                "game_time_raw": first + (0 if i == 0 else 1 + 3 * (i - 1)) * 65536 // 60,
                "target": 255,
                "target_state": copy.deepcopy(target),
            }
            for i in range(36)
        ]
        contract.aim_witness(events, first, 2)
        for index, field, replacement in [
            (2, "game_time_raw", first),
            (3, "target", 269),
            (4, "expected_signature", 270),
            (5, "current_slot", {"id": 28, "signature": 270, "type": 2}),
            (6, "shields_raw", 0),
            (7, "should_be_dead", True),
            (0, "binding_shields_raw", 199 * 65536),
        ]:
            forged = copy.deepcopy(events)
            if field in {"game_time_raw", "target"}:
                forged[index][field] = replacement
            elif field in {"shields_raw", "should_be_dead"}:
                forged[index]["target_state"]["actor_state"][field] = replacement
            else:
                forged[index]["target_state"][field] = replacement
            with self.subTest(field=field), self.assertRaises(ValueError):
                contract.aim_witness(forged, first, 2)

    def test_versioned_save_alias_and_hash_forgeries(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "preview-user").mkdir()
            version = "pumo-player-level7-123-step44.sav"
            alias = "pumo-player-level7.sav"
            for name in (version, alias):
                (root / "preview-user" / name).write_bytes(b"fixture")
            digest = contract.sha(root / "preview-user" / version)
            final = state()
            saved = {"step": 44, "file": version, "sha256": digest, "state": final}
            receipt = {
                "checkpoint_history": [saved],
                "checkpoints": {"7": {"file": alias, "sha256": digest, "state": final}},
            }
            contract.checkpoint(root, receipt, final, 44)
            for mutator in (
                lambda r: r["checkpoint_history"][0].update(file=alias),
                lambda r: r["checkpoint_history"][0].update(sha256="0" * 64),
                lambda r: r["checkpoints"]["7"].update(file=version),
                lambda r: r["checkpoint_history"][0].update(state={}),
            ):
                forged = copy.deepcopy(receipt)
                mutator(forged)
                with self.assertRaises(ValueError):
                    contract.checkpoint(root, forged, final, 44)


if __name__ == "__main__":
    unittest.main()
