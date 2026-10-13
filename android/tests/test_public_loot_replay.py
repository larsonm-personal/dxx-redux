"""Authored loot admission rejects phantom drops, wrong ownership and no collection."""

import copy
import unittest
from unittest import mock

import public_loot_replay as loot
import check_compact_public_replay_failures as negative


class LootEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.header = {
            "game": "d1",
            "mission": "d1",
            "level": 1,
            "difficulty": 2,
            "start_mode": "new_level",
            "frame_count": 7,
        }
        self.actions = dict(
            loot.CARRIER,
            damage_frame=1,
            action_frame=2,
            loot_frame=3,
            retirement_frame=4,
            pickup_gain_frame=5,
            pickup_frame=6,
            pickup=90,
            pickup_signature=100,
        )
        self.objects, self.worlds, self.states = [], [], []
        for index in range(8):
            objects = {"0": {"type": 4, "id": 0, "signature": 1}}
            if index <= 4:
                objects["39"] = {
                    "type": 2,
                    "id": 20,
                    "signature": 40,
                    "flags": int(index > 2),
                    "shields": 4587520 if index < 2 else 3932160 if index < 3 else -1,
                    "contains_type": 7,
                    "contains_id": 3,
                    "contains_count": 1,
                }
            if 3 < index <= 6:
                objects["90"] = {
                    "type": 7,
                    "id": 3,
                    "signature": 100,
                    "flags": 2 * int(index > 5),
                    "lifeleft": 1073741823,
                }
            player = {
                "objnum": 0,
                "score": 500 * int(index > 2),
                "laser_level": int(index > 5),
                "primary_weapon": 0,
                "primary_weapon_flags": 1,
                "shields": 100,
                "num_kills_level": int(index > 0) + int(index > 2),
                "num_kills_total": int(index > 0) + int(index > 2),
            }
            self.objects.append(objects)
            self.worlds.append(
                {
                    "players": [player],
                    "death": {"active": 0},
                    "weapons": {"delayed_primary_autoselect_weapon_index": -1},
                }
            )
            self.states.append({"player0": {key: player[key] for key in ("score", "laser_level", "primary_weapon")}})
        self.events = []
        for frame, old, new in ((1, 4587520, 3932160), (2, 3932160, -1)):
            died = int(new < 0)
            self.events.append(
                {
                    "type": "robot_damage",
                    "version": 1,
                    "f": frame,
                    "robot": 39,
                    "robot_id": 20,
                    "robot_signature": 40,
                    "weapon": 91,
                    "weapon_id": 0,
                    "weapon_signature": 101 + frame,
                    "parent": 0,
                    "parent_type": 4,
                    "parent_signature": 1,
                    "old_shields": old,
                    "new_shields": new,
                    "damage": old - new,
                    "died": died,
                    "score_value": 500,
                    "score": 500 * died,
                    "kills_level": 1 + died,
                    "kills_total": 1 + died,
                }
            )
        self.events.append(
            {
                "type": "robot_loot",
                "version": 1,
                "f": 3,
                "robot": 39,
                "robot_id": 20,
                "robot_signature": 40,
                "definition_type": 7,
                "definition_id": 2,
                "definition_count": 1,
                "definition_probability": 3,
                "chance_roll": -1,
                "count_roll": -1,
                "contains_type": 7,
                "contains_id": 3,
                "contains_count": 1,
                "created": 90,
                "created_type": 7,
                "created_id": 3,
                "created_signature": 100,
            }
        )

    def verify(self):
        worlds = [
            {"type": "world_boundary", "phase": "restored", "state": self.worlds[0]},
            {"type": "object_boundary", "phase": "restored", "slots": self.objects[0]},
        ]
        worlds += [{"type": "world_state", "state": world} for world in self.worlds[:-1]]
        worlds += [
            {"type": "world_boundary", "phase": "terminal", "state": self.worlds[-1]},
            {"type": "object_boundary", "phase": "terminal", "slots": self.objects[-1]},
        ]
        with (
            mock.patch.object(
                loot.parity, "object_states", return_value=iter({"objects": row} for row in self.objects[:-1])
            ),
            mock.patch.object(loot.parity, "world_states", return_value=iter(worlds)),
            mock.patch.object(loot.parity, "records", return_value=iter(self.events)),
        ):
            return loot.verify(self.actions, self.header, self.states, "unused")

    def reject(self):
        with self.assertRaises(loot.parity.EvidenceError):
            self.verify()

    def test_authored_carrier_drop_and_collection(self):
        result = self.verify()
        self.assertEqual(result["laser_level_increase"], 1)
        self.assertEqual(result["uncredited_kills_before_carrier"], 1)

    def test_wrong_carrier(self):
        self.actions["target"] = 38
        self.reject()

    def test_staged_contents(self):
        self.objects[0]["39"]["contains_count"] = 0
        self.reject()

    def test_no_fire(self):
        self.events = self.events[-1:]
        self.reject()

    def test_damage_not_player_owned(self):
        self.events[0]["parent_type"] = 2
        self.reject()

    def test_damage_signature_reused(self):
        self.events[0]["parent_signature"] = 99
        self.reject()

    def test_ram_damage_is_not_a_laser_hit(self):
        self.events[0]["weapon_id"] = -1
        self.reject()

    def test_unexplained_shield_loss(self):
        self.objects[1]["39"]["shields"] -= 1
        self.reject()

    def test_false_score(self):
        self.events[1]["score"] = 0
        self.reject()

    def test_invented_chance_roll(self):
        self.events[-1]["chance_roll"] = 0
        self.reject()

    def test_wrong_drop(self):
        self.events[-1]["created_id"] = 2
        self.reject()

    def test_phantom_physical_drop(self):
        del self.objects[4]["90"]
        self.reject()

    def test_old_pickup_is_not_robot_loot(self):
        self.objects[0]["7"] = copy.deepcopy(self.objects[4]["90"])
        self.reject()

    def test_no_collection(self):
        self.objects[7]["90"] = copy.deepcopy(self.objects[6]["90"])
        self.reject()

    def test_drop_expires_instead_of_being_collected(self):
        self.objects[5]["90"]["lifeleft"] = 0
        self.reject()

    def test_drop_not_marked_consumed_at_gain(self):
        self.objects[6]["90"]["flags"] = 0
        self.reject()

    def test_other_laser_pickup_explains_gain(self):
        for index in range(6):
            self.objects[index]["7"] = {"type": 7, "id": 3, "signature": 8, "flags": 0, "lifeleft": 1073741823}
        self.reject()

    def test_delayed_unrelated_drop_retirement(self):
        self.actions["pickup_gain_frame"] = 4
        self.reject()

    def test_collection_without_inventory_effect(self):
        self.worlds[-1]["players"][0]["laser_level"] = 0
        self.reject()

    def test_unrequested_primary_autoselection(self):
        self.worlds[-1]["players"][0]["primary_weapon"] = 2
        self.reject()

    def test_queued_primary_autoselection(self):
        self.worlds[-1]["weapons"]["delayed_primary_autoselect_weapon_index"] = 2
        self.reject()

    def test_terminal_result_inventory_disagrees(self):
        self.states[-1]["player0"]["laser_level"] = 0
        self.reject()

    def test_death_during_collection(self):
        self.worlds[-1]["death"]["active"] = 1
        self.reject()


class LootNegativeEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.actions = dict(loot.CARRIER, pickup=90, pickup_signature=100)
        self.pickup = {"type": 7, "id": 3, "signature": 100, "flags": 0, "lifeleft": 1073741823}
        self.carrier = {"type": 2, "id": 20, "signature": 40, "flags": 0, "shields": 4587520}
        self.objects = {"90": self.pickup}
        self.player = {"laser_level": 0, "score": 500, "shields": 100}
        self.world = {"players": [self.player], "death": {"active": 0}}
        self.result = {"player0": {"laser_level": 0, "score": 500}}
        self.events = []

    def verify(self, mutation="no-collect", omitted=None):
        boundaries = [
            {"type": "object_boundary", "phase": "terminal", "slots": self.objects},
            {"type": "world_boundary", "phase": "terminal", "state": self.world},
        ]
        with (
            mock.patch.object(
                negative.parity, "world_states", return_value=iter(row for row in boundaries if row["type"] != omitted)
            ),
            mock.patch.object(negative.parity, "records", return_value=iter(self.events)),
        ):
            negative.verify_loot_negative(self.actions, {}, "unused", self.result, mutation)

    def reject(self, mutation="no-collect", omitted=None):
        with self.assertRaises(loot.parity.EvidenceError):
            self.verify(mutation, omitted)

    def test_no_collect_requires_real_unconsumed_drop(self):
        self.verify()
        original = copy.deepcopy(self.pickup)
        for key, value in (("type", 2), ("id", 2), ("signature", 101), ("flags", 2), ("lifeleft", 0)):
            with self.subTest(key=key):
                self.objects["90"] = {**original, key: value}
                self.reject()
        del self.objects["90"]
        self.reject()

    def test_no_collect_requires_living_player(self):
        self.world["death"]["active"] = 1
        self.reject()
        self.world["death"]["active"] = 0
        self.player["shields"] = -1
        self.reject()

    def test_no_collect_requires_actual_kill_score_in_both_observations(self):
        for player in (self.player, self.result["player0"]):
            with self.subTest(player=player):
                player["score"] = 0
                self.reject()
                player["score"] = 500

    def test_negatives_reject_inventory_gain_in_either_observation(self):
        for mutation in ("no-collect", "bad-input"):
            self.objects = {"90": self.pickup} if mutation == "no-collect" else {"39": self.carrier}
            for player in (self.player, self.result["player0"]):
                with self.subTest(mutation=mutation, player=player):
                    player["laser_level"] = 1
                    self.reject(mutation)
                    player["laser_level"] = 0

    def test_negatives_require_both_terminal_boundaries(self):
        for mutation in ("no-collect", "bad-input"):
            self.objects = {"90": self.pickup} if mutation == "no-collect" else {"39": self.carrier}
            for omitted in ("object_boundary", "world_boundary"):
                with self.subTest(mutation=mutation, omitted=omitted):
                    self.reject(mutation, omitted)

    def test_negatives_propagate_malformed_world_evidence(self):
        with mock.patch.object(negative.parity, "world_states", side_effect=loot.parity.EvidenceError("truncated")):
            with self.assertRaisesRegex(loot.parity.EvidenceError, "truncated"):
                negative.verify_loot_negative(self.actions, {}, "unused", self.result, "no-collect")

    def test_no_fire_requires_living_original_carrier(self):
        self.objects = {"39": self.carrier}
        self.verify("bad-input")
        original = copy.deepcopy(self.carrier)
        for key, value in (("type", 7), ("id", 19), ("signature", 41), ("flags", 1), ("shields", -1)):
            with self.subTest(key=key):
                self.objects["39"] = {**original, key: value}
                self.reject("bad-input")
        del self.objects["39"]
        self.reject("bad-input")

    def test_no_fire_rejects_physical_carrier_drop(self):
        self.objects["39"] = self.carrier
        self.reject("bad-input")

    def test_no_fire_rejects_carrier_loot_resolution_but_preserves_unrelated_events(self):
        self.objects = {"39": self.carrier}
        self.events = [{"type": "robot_loot", "robot_signature": 46, "created": -1}]
        self.verify("bad-input")
        self.events.append({"type": "robot_loot", "robot_signature": 40, "created": -1})
        self.reject("bad-input")


if __name__ == "__main__":
    unittest.main()
