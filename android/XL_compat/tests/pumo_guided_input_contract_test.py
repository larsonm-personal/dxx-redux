"""Asset-free controls for the pinned Guided input policy and object boundary."""

import copy
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import pumo_guided_input_contract as contract


class GuidedInputContractTest(unittest.TestCase):
    def setUp(self):
        player = {
            "secondary_ammo": [7, 0, 0, 0, 0, 0, 4, 0, 0, 0],
            "secondary_weapon": 0,
            "cloaked": True,
            "has_red_key": True,
            "invulnerable": False,
            "shields": 100,
            "energy": 100,
            "score": 10,
        }
        self.saved = {
            "sha256": "a" * 64,
            "state": {
                "player": player,
                "game_time_raw": 100,
            },
        }
        guided = {
            "object": 55,
            "signature": 123,
            "weapon_id": 37,
            "lifeleft_raw": 50,
            "parent_object": 0,
            "parent_signature": 0,
            "position_fixed": [1, 2, 3],
            "orient_forward_fixed": [1, 2, 3],
            "orient_right_fixed": [1, 2, 3],
            "orient_up_fixed": [1, 2, 3],
        }
        weapon = {
            "object": 55,
            "signature": 123,
            "weapon_id": 37,
            "radius_raw": 100093,
            "parent_type": 4,
            "parent_object": 0,
            "parent_signature": 0,
            "position_fixed": [1, 2, 3],
            "velocity_fixed": [10, 20, 30],
        }
        self.state = {
            "current_level_num": 7,
            "player_dead": False,
            "cheats_enabled": False,
            "player": copy.deepcopy(player),
            "position": {"segment": 458},
            "xl_pilot": {"remaining_objectives": 1},
            "secondary_weapon_definition": {"weapon_id": 37},
            "combat_observation": {
                "guided_control": guided,
                "player_object": 0,
                "player_signature": 0,
                "owned_weapon_count": 1,
                "owned_weapons": [weapon],
                "owned_weapons_truncated": False,
            },
        }
        self.state["player"]["secondary_ammo"][6] = 3
        self.state["player"]["secondary_weapon"] = 6
        self.initial = copy.deepcopy(self.state)

    def test_approved_templates_and_source_clock(self):
        for kind in ("neutral", "yaw"):
            actual = contract.expected_plan(None, kind, self.saved)
            self.assertEqual(actual["steps"][5]["expect"]["game_time_raw"], 65636)
            self.assertEqual(len(actual["steps"]), 24)
            self.assertEqual(actual["steps"][4]["timeout_ms"], 500)
        self.assertEqual(
            contract.expected_plan(None, "yaw", self.saved)["steps"][13],
            {"action": "send_axis", "axis": 2, "axis_value": 0.2},
        )

    def test_action_policy_rejects_extra_aim_and_changed_timing(self):
        original = contract.doc
        for mutation in (
            lambda plan: plan["steps"].insert(12, {"action": "face_view", "object": 55}),
            lambda plan: plan["steps"][4].update(timeout_ms=501),
            lambda plan: plan["steps"][13].update(axis_value=0.3),
            lambda plan: plan.update(simulation_fps=30),
            lambda plan: plan.update(realtime=True),
        ):
            with self.subTest(mutation=mutation):
                template = original(Path(contract.__file__).with_name("pumo_guided_input_yaw.json"))
                mutation(template)
                with patch.object(contract, "doc", return_value=template), self.assertRaises(ValueError):
                    contract.expected_plan(None, "yaw", self.saved)

    def test_live_object_and_forged_boundaries(self):
        contract.object_witness(self.state, self.saved, self.initial)
        for path, value in (
            (("player", "secondary_ammo"), [7, 0, 0, 0, 0, 0, 4, 0, 0, 0]),
            (("combat_observation", "guided_control", "signature"), 124),
            (("combat_observation", "owned_weapons", 0, "parent_object"), 42),
            (("xl_pilot", "remaining_objectives"), 0),
        ):
            state = copy.deepcopy(self.state)
            target = state
            for key in path[:-1]:
                target = target[key]
            target[path[-1]] = value
            with self.subTest(path=path), self.assertRaises(ValueError):
                contract.object_witness(state, self.saved, self.initial)

    def test_coordinated_forged_player_parent_rejected(self):
        source = copy.deepcopy(self.state)
        forged = copy.deepcopy(self.state)
        forged["combat_observation"]["player_signature"] = 999
        forged["combat_observation"]["guided_control"]["parent_signature"] = 999
        forged["combat_observation"]["owned_weapons"][0]["parent_signature"] = 999
        contract.object_witness(forged, self.saved, forged)
        with self.assertRaises(ValueError):
            contract.restored_identity(forged, source)

    def test_only_guided_motion_is_ignored_by_pair_projection(self):
        other = copy.deepcopy(self.state)
        other["combat_observation"]["guided_control"]["orient_forward_fixed"][0] += 1
        self.assertEqual(contract.remove_motion(self.state), contract.remove_motion(other))
        other["combat_observation"]["guided_control"]["signature"] += 1
        self.assertNotEqual(contract.remove_motion(self.state), contract.remove_motion(other))
        other = copy.deepcopy(self.state)
        other["xl_pilot"]["remaining_objectives"] = 0
        self.assertNotEqual(contract.remove_motion(self.state), contract.remove_motion(other))
        other = copy.deepcopy(self.state)
        other["player"]["score"] += 1
        self.assertNotEqual(contract.remove_motion(self.state), contract.remove_motion(other))

    def test_forward_velocity_requires_near_alignment(self):
        contract.forward_velocity_aligned([100, 0, 0], [100, 1, 0])
        with self.assertRaises(ValueError):
            contract.forward_velocity_aligned([100, 0, 0], [1, 100, 0])
        with self.assertRaises(ValueError):
            contract.forward_velocity_aligned([100, 0, 0], [-100, 0, 0])

    def test_positive_yaw_uses_missile_local_right_across_world_orientations(self):
        # Both fixtures describe the same stock heading turn after rotating the
        # entire missile basis. In the second, world Y decreases under yaw.
        for forward, right, up in (
            ((0, 0, 1000), (1000, 0, 0), (0, 1000, 0)),
            ((0, 0, 1000), (0, -1000, 0), (1000, 0, 0)),
            ((1000, 0, 0), (0, 1000, 0), (0, 0, 1000)),
        ):
            turned = tuple(f + r // 10 for f, r in zip(forward, right))
            velocity = tuple(3 * f for f in forward)
            turned_velocity = tuple(3 * f for f in turned)
            contract.positive_local_yaw(forward, right, up, turned, velocity, turned_velocity)
            contract.forward_velocity_aligned(turned, turned_velocity)
            for bad_forward, bad_velocity, bad_right, bad_up in (
                (
                    tuple(f - r // 10 for f, r in zip(forward, right)),
                    tuple(3 * (f - r // 10) for f, r in zip(forward, right)),
                    right,
                    up,
                ),
                (forward, velocity, right, up),
                (turned, turned_velocity, tuple(-r for r in right), up),
                (turned, turned_velocity, up, up),
                (turned, turned_velocity, forward, up),
                (turned, turned_velocity, right, tuple(u // 1000 for u in up)),
            ):
                with self.assertRaises(ValueError):
                    contract.positive_local_yaw(forward, bad_right, bad_up, bad_forward, velocity, bad_velocity)

    def test_positive_yaw_rejects_up_dominated_and_forged_velocity(self):
        forward, right, up = (0, 0, 1000), (1000, 0, 0), (0, 1000, 0)
        for turned, moved in (
            ((100, 200, 1000), (300, 600, 3000)),
            ((100, 0, 1000), (0, 0, 3000)),
            ((0, 0, 1000), (300, 0, 3000)),
        ):
            with self.assertRaises(ValueError):
                contract.positive_local_yaw(forward, right, up, turned, (0, 0, 3000), moved)

    def test_empty_terminal_source_rejected(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / "guided_load-result.json"
            path.write_text("{}")
            with (
                patch.object(contract, "sha", return_value=contract.FROZEN_ENGINE_SHA256),
                self.assertRaises(ValueError),
            ):
                contract.source(path, Path(root) / "engine")


if __name__ == "__main__":
    unittest.main()
