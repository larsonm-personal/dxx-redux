"""Combat admission rejects mere firing, wrong actors, invented awards and fake loot."""

import copy
import unittest
from unittest import mock

import public_robot_combat_replay as combat


class CombatEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.header = {
            "game": "d1",
            "mission": "d1",
            "level": 1,
            "difficulty": 2,
            "start_mode": "new_level",
            "frame_count": 6,
        }
        self.profile = {
            "target": 8,
            "target_id": 3,
            "target_signature": 9,
            "target_shields": 2500,
            "target_score": 100,
            "loot": (7, 2, 1, 8),
        }
        self.actions = {key: value for key, value in self.profile.items() if key != "loot"}
        self.actions.update(damage_frame=1, action_frame=2, loot_frame=3, retirement_frame=4, loot_count=0)
        self.objects, self.worlds, self.states = [], [], []
        for index in range(7):
            slots = {"0": {"type": 4, "id": 0, "signature": 1}}
            if index <= 4:
                slots["8"] = {
                    "type": 2,
                    "id": 3,
                    "signature": 9,
                    "shields": 2500 if index < 2 else 1500 if index < 3 else -500,
                    "flags": int(index > 2),
                    "contains_count": 0,
                }
            self.objects.append(slots)
            killed = int(index > 2)
            self.worlds.append(
                {
                    "players": [
                        {
                            "objnum": 0,
                            "score": 100 * killed,
                            "num_kills_level": killed,
                            "num_kills_total": killed,
                            "laser_level": 0,
                            "primary_weapon": 0,
                            "primary_weapon_flags": 1,
                            "shields": 100,
                        }
                    ],
                    "death": {"active": 0},
                }
            )
            self.states.append({"player0": {"score": 100 * killed}})
        self.hits = []
        for frame, old, new in ((1, 2500, 1500), (2, 1500, -500)):
            died = int(new < 0)
            self.hits.append(
                {
                    "type": "robot_damage",
                    "version": 1,
                    "f": frame,
                    "robot": 8,
                    "robot_id": 3,
                    "robot_signature": 9,
                    "weapon": 100,
                    "weapon_id": 0,
                    "weapon_signature": 200 + frame,
                    "parent": 0,
                    "parent_type": 4,
                    "parent_signature": 1,
                    "old_shields": old,
                    "new_shields": new,
                    "damage": old - new,
                    "died": died,
                    "score_value": 100,
                    "score": 100 * died,
                    "kills_level": died,
                    "kills_total": died,
                }
            )
        self.loot = {
            "type": "robot_loot",
            "version": 1,
            "f": 3,
            "robot": 8,
            "robot_id": 3,
            "robot_signature": 9,
            "definition_type": 7,
            "definition_id": 2,
            "definition_count": 1,
            "definition_probability": 8,
            "chance_roll": 30000,
            "count_roll": -1,
            "contains_type": -1,
            "contains_id": -1,
            "contains_count": 0,
            "created": -1,
            "created_type": -1,
            "created_id": -1,
            "created_signature": -1,
        }
        self.events = self.hits + [self.loot]

    def verify(self):
        worlds = [
            {"type": "world_boundary", "phase": "restored", "state": self.worlds[0]},
            {"type": "object_boundary", "phase": "restored", "slots": self.objects[0]},
        ]
        worlds += [{"type": "world_state", "f": i, "state": state} for i, state in enumerate(self.worlds[:-1])]
        worlds += [
            {"type": "world_boundary", "phase": "terminal", "state": self.worlds[-1]},
            {"type": "object_boundary", "phase": "terminal", "slots": self.objects[-1]},
        ]
        with (
            mock.patch.object(combat.incoming, "verify", return_value={}),
            mock.patch.dict(combat.TARGETS, {"d1": self.profile}),
            mock.patch.object(
                combat.parity, "object_states", return_value=iter({"objects": row} for row in self.objects[:-1])
            ),
            mock.patch.object(combat.parity, "world_states", return_value=iter(worlds)),
            mock.patch.object(combat.parity, "records", return_value=iter(self.events)),
        ):
            return combat.verify(self.actions, self.header, self.states, "unused")

    def test_identified_projectile_damage_death_score_and_failed_loot_roll(self):
        result = self.verify()
        self.assertEqual(result["score_awarded"], 100)
        self.assertEqual(result["player_projectile_hits"], 2)
        self.assertEqual(result["physical_loot_objects"], 0)

    def test_actual_successful_loot_roll_requires_physical_object(self):
        self.loot.update(
            chance_roll=0,
            count_roll=0,
            contains_type=7,
            contains_id=2,
            contains_count=1,
            created=100,
            created_type=7,
            created_id=2,
            created_signature=300,
        )
        self.actions["loot_count"] = 1
        for index in range(4, 7):
            self.objects[index]["100"] = {"type": 7, "id": 2, "signature": 300}
        self.assertEqual(self.verify()["physical_loot_objects"], 1)
        del self.objects[4]["100"]
        with self.assertRaisesRegex(combat.parity.EvidenceError, "physical loot"):
            self.verify()

    def test_rejects_firing_without_real_damage_observations(self):
        self.events = [self.loot]
        with self.assertRaises(combat.parity.EvidenceError):
            self.verify()

    def test_rejects_wrong_identity_damage_or_award(self):
        for key, value in (
            ("robot", 9),
            ("robot_signature", 10),
            ("parent", 1),
            ("parent_type", 2),
            ("parent_signature", 0),
            ("weapon_id", 1),
            ("damage", 1),
            ("old_shields", 2000),
            ("died", 1),
            ("score_value", 200),
            ("score", 1),
            ("kills_level", 1),
            ("f", True),
        ):
            with self.subTest(key=key):
                self.setUp()
                self.hits[0][key] = value
                with self.assertRaises(combat.parity.EvidenceError):
                    self.verify()

    def test_rejects_wrong_boundaries_original_robot_or_player(self):
        for key in (
            "damage_frame",
            "action_frame",
            "loot_frame",
            "retirement_frame",
            "target_signature",
            "target_score",
        ):
            with self.subTest(key=key):
                self.setUp()
                self.actions[key] += 1
                with self.assertRaises(combat.parity.EvidenceError):
                    self.verify()
        self.setUp()
        self.objects[0]["8"]["shields"] -= 1
        with self.assertRaises(combat.parity.EvidenceError):
            self.verify()
        self.setUp()
        self.worlds[4]["death"]["active"] = 1
        with self.assertRaises(combat.parity.EvidenceError):
            self.verify()

    def test_rejects_duplicate_hits_missing_death_or_duplicate_loot(self):
        for change in ("hit", "death", "loot"):
            with self.subTest(change=change):
                self.setUp()
                if change == "hit":
                    self.events.insert(0, copy.deepcopy(self.hits[0]))
                elif change == "death":
                    self.events.remove(self.hits[-1])
                else:
                    self.events.append(copy.deepcopy(self.loot))
                with self.assertRaises(combat.parity.EvidenceError):
                    self.verify()

    def test_rejects_unjustified_loot_and_phantom_drop(self):
        for key, value in (
            ("definition_probability", 16),
            ("chance_roll", 0),
            ("count_roll", 0),
            ("created", 100),
            ("contains_count", 1),
            ("robot_signature", 10),
        ):
            with self.subTest(key=key):
                self.setUp()
                self.loot[key] = value
                with self.assertRaises(combat.parity.EvidenceError):
                    self.verify()
        self.setUp()
        self.objects[4]["100"] = {"type": 7, "id": 2, "signature": 300}
        with self.assertRaises(combat.parity.EvidenceError):
            self.verify()

    def test_rejects_collateral_score_kill_or_wrong_terminal_score(self):
        for key in ("score", "num_kills_level", "num_kills_total"):
            with self.subTest(key=key):
                self.setUp()
                self.worlds[-1]["players"][0][key] += 1
                with self.assertRaises(combat.parity.EvidenceError):
                    self.verify()
        self.setUp()
        self.states[-1]["player0"]["score"] += 1
        with self.assertRaises(combat.parity.EvidenceError):
            self.verify()

    def test_rejects_unknown_fields_and_boolean_numbers(self):
        self.loot["unreviewed"] = 1
        with self.assertRaises(combat.parity.EvidenceError):
            self.verify()
        self.setUp()
        self.actions["loot_count"] = False
        with self.assertRaises(combat.parity.EvidenceError):
            self.verify()


if __name__ == "__main__":
    unittest.main()
