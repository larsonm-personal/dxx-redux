"""Asset-free negative checks for the paired blast-wall owner."""

import copy
import unittest

import pumo_tulev1_blast136_contract as blast


class Blast136ContractTest(unittest.TestCase):
    def test_public_templates_are_bounded_and_source_only(self):
        warm = blast.template("warm")
        cold = blast.template("cold")
        self.assertNotIn("resume_checkpoint", warm)
        self.assertNotIn("resume_sha256", warm)
        self.assertEqual(warm["steps"][6], {"action": "face_view", "segment": 1152, "side": 0, "distance": 8.0})
        self.assertEqual(warm["steps"][9], {"action": "send_button", "button": 100, "held": 1})
        self.assertEqual(warm["steps"][10]["timeout_ms"], 3000)
        self.assertEqual(warm["steps"][11], {"action": "send_button", "button": 100, "held": 0})
        self.assertEqual(warm["steps"][12]["timeout_ms"], 1000)
        self.assertEqual([step["action"] for step in cold["steps"]], ["introspect"])
        self.assertNotIn("set_debug", [step["action"] for step in cold["steps"]])

    def test_pair_rejects_unblasted_partner_and_lost_switch(self):
        state = {
            "xl_pilot": {
                "doorways": {str(i): {"type": 1, "clip": 6, "flags": 17} for i in (136, 137)},
                "wall_triggers": {"12": {"operated": True, "shot": 1}},
            }
        }
        for i in (201, 202):
            state["xl_pilot"]["doorways"][str(i)] = {"type": 4, "state": 0}
        blast.paired(state, 17)
        for path, value in (
            (("doorways", "136", "flags"), 16),
            (("doorways", "137", "flags"), 16),
            (("doorways", "136", "type"), 4),
            (("wall_triggers", "12", "operated"), False),
        ):
            altered = copy.deepcopy(state)
            field = altered["xl_pilot"]
            for key in path[:-1]:
                field = field[key]
            field[path[-1]] = value
            with self.subTest(path=path), self.assertRaises(ValueError):
                blast.paired(altered, 17)

    def test_face_view_assist_uses_actual_trace_action(self):
        position = {"segment": 1152}
        assist = {"event": "position_assist", "step": 6, "action": "face_view", "position": position}
        steps = [{"state": {"position": {"segment": 648}}} for _ in range(8)]
        steps[7]["state"]["position"] = position
        blast.assisted_local_pose([assist], steps)
        for field, value in (("action", "pose_view"), ("step", 7), ("position", {"segment": 1151})):
            altered = copy.deepcopy(assist)
            altered[field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                blast.assisted_local_pose([altered], steps)

    def test_contact_metadata_rejects_contradictions(self):
        report = {
            "kind": "actual_contacts_reaching_effect_blowup_gate",
            "contact_count_lower_bound": 3,
            "contact_limit": 2,
            "truncated": True,
            "contacts": [{"wall": 136}, {"wall": 136}],
        }
        blast.actual_contacts(report)
        for field, value in (
            ("contact_count_lower_bound", 1),
            ("contact_limit", 1),
            ("truncated", False),
            ("truncated", 1),
        ):
            altered = copy.deepcopy(report)
            altered[field] = value
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                blast.actual_contacts(altered)

    def test_actor_rejects_power_grant_and_safety_assist(self):
        state = {
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
                "secondary_ammo": [7, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            },
        }
        blast.ordinary(state)
        for field, value in (("invulnerable", True), ("laser_level", 4), ("quad_lasers", False)):
            altered = copy.deepcopy(state)
            altered["player"][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                blast.ordinary(altered)


if __name__ == "__main__":
    unittest.main()
