"""Incoming qualification rejects unowned shots, wall losses and corrupted boundaries."""

import copy
import unittest
from unittest import mock

import public_incoming_robot_hits as incoming


class IncomingHitTests(unittest.TestCase):
    def setUp(self):
        self.header = {"game": "d1", "frame_count": 9}
        self.profile = {"robot": (8, 3, 9), "weapon_id": 5, "damage": 393216, "hits": ((4, 2, 68, 76), (7, 5, 66, 78))}
        self.objects, self.worlds, self.states = [], [], []
        self.events = []
        for frame, birth, slot, signature in self.profile["hits"]:
            old = (100 if frame == 4 else 94) * 65536
            self.events.append(
                dict(
                    type="player_weapon_damage",
                    version=1,
                    f=frame,
                    player=0,
                    player_id=0,
                    player_signature=1,
                    weapon=slot,
                    weapon_id=5,
                    weapon_signature=signature,
                    parent=8,
                    parent_type=2,
                    parent_signature=9,
                    old_shields=old,
                    new_shields=old - 393216,
                    damage=393216,
                )
            )
        for index in range(10):
            shields = (100 if index <= 4 else 94 if index <= 7 else 88) * 65536
            slots = {
                "0": dict(type=4, id=0, signature=1, flags=0, shields=shields),
                "8": dict(type=2, id=3, signature=9, flags=int(index >= 6), shields=-1 if index >= 6 else 100),
            }
            for frame, birth, slot, signature in self.profile["hits"]:
                if birth <= index <= frame + 1:
                    slots[str(slot)] = dict(
                        type=5,
                        id=5,
                        signature=signature,
                        control_type=9,
                        flags=2 if index == frame + 1 else 0,
                        lifeleft=65536,
                        shields=393216,
                        weapon=dict(parent_num=8, parent_type=2, parent_signature=9, multiplier=65536),
                    )
            self.objects.append(slots)
            self.worlds.append(dict(players=[dict(objnum=0, shields=shields)], death=dict(active=0)))
            self.states.append(dict(player0=dict(shields=shields // 65536)))
        self.restored_objects = copy.deepcopy(self.objects[0])
        self.restored_world = copy.deepcopy(self.worlds[0])

    def verify(self):
        with mock.patch.dict(incoming.PROFILES, {"d1": self.profile}):
            return incoming.verify(
                self.header,
                self.states,
                self.objects,
                self.worlds,
                self.restored_objects,
                self.restored_world,
                self.events,
            )

    def test_real_hits_include_live_shot_arriving_after_its_owner_dies(self):
        result = self.verify()
        self.assertEqual(result["incoming_hit_frames"], [4, 7])
        self.assertEqual(result["incoming_shield_loss"], 12)
        self.assertEqual(result["incoming_robot_projectile_hits"], 2)

    def test_rejects_corrupt_event_identity_damage_frame_or_schema(self):
        for key, value in (
            ("f", 5),
            ("f", True),
            ("version", 2),
            ("player", 1),
            ("player_id", 1),
            ("player_signature", 2),
            ("weapon", 66),
            ("weapon_id", 6),
            ("weapon_signature", 77),
            ("parent", 7),
            ("parent_type", 4),
            ("parent_signature", 10),
            ("damage", 1),
            ("old_shields", 99 * 65536),
            ("new_shields", 93 * 65536),
            ("extra", 0),
        ):
            with self.subTest(key=key, value=value):
                self.setUp()
                self.events[0][key] = value
                with self.assertRaises(incoming.parity.EvidenceError):
                    self.verify()

    def test_rejects_absent_duplicate_reordered_or_wall_only_events(self):
        for mode in ("missing", "duplicate", "reordered", "wall"):
            with self.subTest(mode=mode):
                self.setUp()
                if mode == "missing":
                    self.events.pop()
                elif mode == "duplicate":
                    self.events.append(copy.deepcopy(self.events[0]))
                elif mode == "reordered":
                    self.events.reverse()
                else:
                    for row in self.events:
                        row["type"] = "wall_damage"
                with self.assertRaises(incoming.parity.EvidenceError):
                    self.verify()

    def test_rejects_corrupt_live_projectile_or_owner(self):
        for key, value in (
            ("signature", 77),
            ("id", 6),
            ("type", 2),
            ("control_type", 0),
            ("flags", 1),
            ("flags", 2),
            ("flags", 4),
            ("flags", 32),
            ("lifeleft", 0),
            ("shields", 1),
        ):
            with self.subTest(key=key, value=value):
                self.setUp()
                self.objects[3]["68"][key] = value
                with self.assertRaises(incoming.parity.EvidenceError):
                    self.verify()
        for key, value in (("parent_num", 7), ("parent_type", 4), ("parent_signature", 10), ("multiplier", 1)):
            with self.subTest(key=key):
                self.setUp()
                self.objects[3]["68"]["weapon"][key] = value
                with self.assertRaises(incoming.parity.EvidenceError):
                    self.verify()
        for key, value in (("id", 4), ("signature", 10), ("shields", -1), ("flags", 1)):
            with self.subTest(robot=key):
                self.setUp()
                self.objects[2]["8"][key] = value
                with self.assertRaises(incoming.parity.EvidenceError):
                    self.verify()

    def test_rejects_ownership_corruption_at_the_impact_boundary(self):
        for key, value in (("parent_num", 7), ("parent_type", 4), ("parent_signature", 10), ("multiplier", 1)):
            with self.subTest(key=key):
                self.setUp()
                self.objects[5]["68"]["weapon"][key] = value
                with self.assertRaises(incoming.parity.EvidenceError):
                    self.verify()

    def test_rejects_staged_missing_or_unretired_projectiles(self):
        for mode in ("restored", "early", "missing", "live_after_impact", "unretired"):
            with self.subTest(mode=mode):
                self.setUp()
                weapon = copy.deepcopy(self.objects[2]["68"])
                if mode == "restored":
                    self.restored_objects["68"] = weapon
                elif mode == "early":
                    self.objects[1]["68"] = weapon
                elif mode == "missing":
                    del self.objects[3]["68"]
                elif mode == "live_after_impact":
                    self.objects[5]["68"]["flags"] = 0
                else:
                    self.objects[6]["68"] = weapon
                with self.assertRaises(incoming.parity.EvidenceError):
                    self.verify()

    def test_rejects_rounded_raw_or_unexplained_shield_loss_and_dead_player(self):
        for mode in ("frame", "world", "mirror", "later_mirror", "unexplained", "death", "identity", "missing"):
            with self.subTest(mode=mode):
                self.setUp()
                if mode == "frame":
                    self.states[5]["player0"]["shields"] += 1
                elif mode == "world":
                    self.worlds[5]["players"][0]["shields"] += 1
                elif mode == "mirror":
                    self.objects[5]["0"]["shields"] += 1
                elif mode == "later_mirror":
                    self.objects[6]["0"]["shields"] += 1
                elif mode == "unexplained":
                    self.worlds[3]["players"][0]["shields"] -= 65536
                    self.states[3]["player0"]["shields"] -= 1
                elif mode == "death":
                    self.worlds[5]["death"]["active"] = 1
                elif mode == "identity":
                    self.objects[5]["0"]["signature"] += 1
                else:
                    self.worlds.pop()
                with self.assertRaises(incoming.parity.EvidenceError):
                    self.verify()


if __name__ == "__main__":
    unittest.main()
