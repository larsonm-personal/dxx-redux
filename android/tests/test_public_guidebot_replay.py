"""Reject fake cage releases and replacement Guide-Bots in public replay evidence."""

import copy
import unittest
from unittest import mock

import public_guidebot_replay as guidebot
import compact_public_replay as compact


class GuidebotEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.header = dict(game="d2", mission="d2demo", level=1, difficulty=2, start_mode="new_level", frame_count=6)
        self.actions = {**guidebot.PROFILE, "damage_frame": 0, "blast_frame": 1, "action_frame": 2, "exit_frame": 3}
        self.objects, self.worlds = [], []
        for index in range(7):
            bot = dict(type=2, id=33, signature=29, shields=1000 * 65536, flags=0, segnum=224 if index <= 3 else 222)
            self.objects.append({"28": bot})
            walls = [{} for _ in range(76)]
            for wall, seg, side in ((74, 222, 4), (75, 224, 5)):
                walls[wall] = dict(
                    segnum=seg,
                    sidenum=side,
                    type=1,
                    flags=int(index > 1),
                    hps=(100 if index == 0 else 50 if index == 1 else -1) * 65536,
                )
            self.worlds.append(
                {
                    "walls": walls,
                    "players": [
                        dict(
                            score=0,
                            num_kills_level=0,
                            num_kills_total=0,
                            laser_level=0,
                            primary_weapon=0,
                            primary_weapon_flags=1,
                            shields=100,
                        )
                    ],
                    "death": {"active": 0},
                    "ai": {"escort": {"Buddy_objnum": 28, "Buddy_allowed_to_talk": int(index > 2)}},
                }
            )

    def verify(self):
        world_rows = [
            {"type": "world_boundary", "phase": "restored", "state": self.worlds[0]},
            {"type": "object_boundary", "phase": "restored", "slots": self.objects[0]},
        ]
        world_rows += [{"type": "world_state", "state": row} for row in self.worlds[:-1]]
        world_rows += [
            {"type": "object_boundary", "phase": "terminal", "slots": self.objects[-1]},
            {"type": "world_boundary", "phase": "terminal", "state": self.worlds[-1]},
        ]
        with (
            mock.patch.object(
                guidebot.parity, "object_states", return_value=[{"objects": row} for row in self.objects[:-1]]
            ),
            mock.patch.object(guidebot.parity, "world_states", return_value=world_rows),
        ):
            return guidebot.verify(self.actions, self.header, [{}] * 7, "unused")

    def test_real_release(self):
        self.assertEqual(self.verify()["guidebot_exited_frame"], 3)

    def test_rejects_false_outcomes(self):
        for mutation in (
            "cage_intact",
            "one_side_intact",
            "replaced_bot",
            "dead_bot",
            "never_exited",
            "early_release",
            "boolean_release",
            "no_damage",
            "wrong_damage_frame",
            "wrong_level",
        ):
            with self.subTest(mutation=mutation):
                self.setUp()
                if mutation == "cage_intact":
                    for world in self.worlds:
                        for wall in (74, 75):
                            world["walls"][wall]["flags"] = 0
                elif mutation == "one_side_intact":
                    self.worlds[-1]["walls"][75]["flags"] = 0
                elif mutation == "replaced_bot":
                    self.objects[-1]["28"]["signature"] = 100
                elif mutation == "dead_bot":
                    self.objects[-1]["28"]["flags"] = 1
                elif mutation == "never_exited":
                    for slots in self.objects:
                        slots["28"]["segnum"] = 224
                elif mutation == "early_release":
                    self.worlds[2]["ai"]["escort"]["Buddy_allowed_to_talk"] = 1
                elif mutation == "boolean_release":
                    self.worlds[-1]["ai"]["escort"]["Buddy_allowed_to_talk"] = True
                elif mutation == "no_damage":
                    self.worlds[1]["walls"][74]["hps"] = 100 * 65536
                elif mutation == "wrong_damage_frame":
                    self.actions["damage_frame"] = 1
                elif mutation == "wrong_level":
                    self.header["level"] = 2
                with self.assertRaises(guidebot.parity.EvidenceError):
                    self.verify()

    def test_missing_native_release_state(self):
        del self.worlds[-1]["ai"]["escort"]
        with self.assertRaises(guidebot.parity.EvidenceError):
            self.verify()


class GuidebotObservationTests(unittest.TestCase):
    def events(self):
        return [
            {
                "type": "guidebot_observation",
                "version": 1,
                "f": frame,
                **{key: frame for key in compact.GUIDEBOT_FIELDS},
            }
            for frame in range(3)
        ]

    def test_native_post_frame_observations_equal_independent_capture(self):
        events = self.events()
        native = [{"type": "world_state", "f": 0, "state": {}}]
        for frame, event in enumerate(events, 1):
            native.append(
                {
                    "type": "world_state" if frame < 3 else "world_boundary",
                    "phase": "terminal",
                    "f": frame,
                    "state": {"ai": {"escort": {key: event[key] for key in compact.GUIDEBOT_FIELDS}}},
                }
            )
        with (
            mock.patch.object(compact.parity, "records", return_value=events),
            mock.patch.object(compact.parity, "world_states", return_value=native),
        ):
            captured = compact.guidebot_observations("capture")
            self.assertEqual(captured, compact.guidebot_observations("native", {"game": "d2"}))
            native[-1]["state"]["ai"]["escort"]["Escort_goal_index"] += 1
            self.assertNotEqual(captured, compact.guidebot_observations("native", {"game": "d2"}))

    def test_observation_schema_cannot_hide_missing_or_reordered_fields(self):
        for mutation in ("missing", "boolean", "reordered", "extra"):
            events = self.events()
            if mutation == "missing":
                del events[0]["Buddy_objnum"]
            elif mutation == "boolean":
                events[0]["version"] = True
            elif mutation == "reordered":
                events.reverse()
            else:
                events[0]["fabricated"] = 1
            with (
                self.subTest(mutation=mutation),
                mock.patch.object(compact.parity, "records", return_value=events),
                self.assertRaises(compact.parity.EvidenceError),
            ):
                compact.guidebot_observations("capture")


if __name__ == "__main__":
    unittest.main()
