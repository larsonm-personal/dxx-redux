"""Door qualification rejects missing obstruction and incorrect native lifecycle behavior."""

import copy
import unittest
from unittest import mock

import public_door_replay as door


class DoorEvidenceTests(unittest.TestCase):
    def prepare(self, game="d1"):
        profile = door.PROFILES[game]
        self.game = game
        self.count = 260
        self.header = {
            "game": game,
            "level": profile["level"],
            "difficulty": 2,
            "start_mode": "new_level",
            "frame_count": self.count,
        }
        stages = (
            [(0, 0), (1, 20), (2, 150)]
            + ([(3, 160), (4, 165)] if game == "d2" else [(4, 160)])
            + [(5, 230), (6, 240), (7, 259)]
        )
        self.actions = {
            "target": profile["target"],
            "door_back": profile["door_back"],
            "action_frame": 10,
            "door_events": [{"stage": stage, "f": frame} for stage, frame in stages],
        }
        self.worlds, self.objects, self.states, self.probes = [], [], [], []
        for frame in range(self.count + 1):
            opened = 16 <= frame <= 220 or frame >= 253
            if frame <= 10 or 231 <= frame <= 240:
                front, back = 0, 0
            elif frame <= 15 or 241 <= frame <= 252:
                front, back = 1, 1
            elif game == "d1" and 120 <= frame <= 160:
                front, back = 3, 2
            elif (game == "d1" and 161 <= frame <= 230) or (
                game == "d2" and (161 <= frame <= 164 or 221 <= frame <= 230)
            ):
                front, back = 3, 3
            elif game == "d2" and 165 <= frame <= 167:
                front, back = 1, 1
            else:
                front, back = 2, 2
            walls = [{} for _ in range(profile["door_back"] + 1)]
            for side, state, index in (("front", front, profile["target"]), ("back", back, profile["door_back"])):
                walls[index] = {
                    "type": 2,
                    "state": state,
                    "flags": 16 + 2 * opened,
                    "keys": 1,
                    "segnum": profile[side + "_segment"],
                    "sidenum": profile[side + "_side"],
                }
            active = (
                []
                if front == 0
                else [
                    {
                        "n_parts": 1,
                        "front": [profile["target"], -1],
                        "back": [profile["door_back"], -1],
                        "time": 0 if game == "d1" and 120 <= frame <= 160 else 400000 + frame * 3276,
                    }
                ]
            )
            self.worlds.append(
                {
                    "walls": walls,
                    "doors": active,
                    "players": [{"objnum": 0, "shields": 6553600}],
                    "death": {"active": 0},
                }
            )
            player = {
                "type": 4,
                "id": 0,
                "signature": 1,
                "segnum": profile["back_segment"] if frame == self.count else profile["front_segment"],
                "pos": [0, 0, 0],
                "size": 310325,
            }
            self.objects.append({"0": player})
            self.states.append({"player0": {"shields": 100}})
            blockers = []
            if 16 <= frame <= 151 or game == "d2" and frame in (164, 165):
                blockers.append(
                    {
                        "slot": 0,
                        "type": 4,
                        "id": 0,
                        "signature": 1,
                        "segment": player["segnum"],
                        "position": player["pos"],
                        "radius": player["size"],
                    }
                )
            self.probes.append(
                {
                    "type": "door_probe",
                    "version": 1,
                    "f": frame,
                    "phase": "terminal" if frame == self.count else "frame",
                    "front": profile["target"],
                    "back": profile["door_back"],
                    "front_doorway": 7 if opened else 2,
                    "back_doorway": 7 if opened else 2,
                    "blockers": blockers,
                }
            )

    def setUp(self):
        self.prepare()

    def verify(self):
        records = [{"type": "world_boundary", "phase": "restored", "state": self.worlds[0]}]
        records += [{"type": "world_state", "f": index, "state": world} for index, world in enumerate(self.worlds[:-1])]
        records += [
            {"type": "world_boundary", "phase": "terminal", "state": self.worlds[-1]},
            {"type": "object_boundary", "phase": "terminal", "slots": self.objects[-1]},
        ]
        with (
            mock.patch.object(
                door.parity, "object_states", return_value=iter({"objects": objects} for objects in self.objects[:-1])
            ),
            mock.patch.object(door.parity, "world_states", return_value=iter(records)),
            mock.patch.object(door.parity, "records", return_value=iter(self.probes)),
        ):
            return door.verify(self.actions, self.header, self.states, "unused")

    def test_d1_freezes_closing_then_closes_reopens_and_traverses(self):
        self.assertEqual(self.verify()["player_obstruction_frames"], 20)

    def test_d2_waits_and_reverses_on_reentry(self):
        self.prepare("d2")
        self.assertEqual(self.verify()["door_obstruction_reversal_frame"], 164)

    def test_wrong_spawn_or_target(self):
        for field, value in (("level", 1), ("difficulty", 0), ("start_mode", "save")):
            with self.subTest(field=field):
                old = self.header[field]
                self.header[field] = value
                with self.assertRaises(door.parity.EvidenceError):
                    self.verify()
                self.header[field] = old
        self.actions["target"] += 1
        with self.assertRaises(door.parity.EvidenceError):
            self.verify()

    def test_requires_original_closed_wall_pair(self):
        self.worlds[0]["walls"][72]["state"] = 1
        with self.assertRaisesRegex(door.parity.EvidenceError, "start closed"):
            self.verify()

    def test_missing_player_obstruction_is_not_open_door_coverage(self):
        self.probes[140]["blockers"] = []
        with self.assertRaisesRegex(door.parity.EvidenceError, "physically obstruct"):
            self.verify()

    def test_fake_blocker_pose_or_identity_is_rejected(self):
        self.probes[140]["blockers"][0]["signature"] = 5
        with self.assertRaisesRegex(door.parity.EvidenceError, "exact live object"):
            self.verify()

    def test_both_collision_faces_are_checked(self):
        self.probes[140]["back_doorway"] = 2
        with self.assertRaisesRegex(door.parity.EvidenceError, "Both collision flags"):
            self.verify()

    def test_d1_cannot_advance_timer_while_obstructed(self):
        self.worlds[140]["doors"][0]["time"] = 1
        with self.assertRaisesRegex(door.parity.EvidenceError, "freeze closing"):
            self.verify()

    def test_d2_cannot_close_during_waiting_obstruction(self):
        self.prepare("d2")
        self.worlds[140]["walls"][76]["state"] = 3
        with self.assertRaisesRegex(door.parity.EvidenceError, "D2 must wait"):
            self.verify()

    def test_clearance_requires_no_remaining_blockers(self):
        self.probes[161]["blockers"] = copy.deepcopy(self.probes[140]["blockers"])
        with self.assertRaisesRegex(door.parity.EvidenceError, "Removing obstruction"):
            self.verify()

    def test_d2_reversal_requires_player_reentry(self):
        self.prepare("d2")
        self.probes[164]["blockers"] = []
        with self.assertRaisesRegex(door.parity.EvidenceError, "reversal requires"):
            self.verify()

    def test_both_faces_must_finish_closed(self):
        self.worlds[231]["walls"][73]["state"] = 3
        with self.assertRaisesRegex(door.parity.EvidenceError, "fully close"):
            self.verify()

    def test_d1_impact_fireball_created_after_close_is_not_a_closing_obstruction(self):
        obj = {"type": 1, "id": 32, "signature": 2, "segnum": 38, "pos": [0, 0, 0], "size": 196608}
        self.objects[231]["95"] = obj
        self.probes[231]["blockers"] = [
            {
                "slot": 95,
                "type": 1,
                "id": 32,
                "signature": 2,
                "segment": 38,
                "position": obj["pos"],
                "radius": obj["size"],
            }
        ]
        self.assertEqual(self.verify()["door_closed_frame"], 230)

    def test_completed_close_must_retire_active_door(self):
        self.worlds[231]["doors"] = copy.deepcopy(self.worlds[230]["doors"])
        with self.assertRaisesRegex(door.parity.EvidenceError, "retire its active-door"):
            self.verify()

    def test_missing_reopen_is_rejected(self):
        self.worlds[241]["walls"][72]["state"] = 0
        with self.assertRaisesRegex(door.parity.EvidenceError, "reopen through normal"):
            self.verify()

    def test_traversal_requires_the_player_on_far_side(self):
        self.objects[-1]["0"]["segnum"] = 75
        with self.assertRaisesRegex(door.parity.EvidenceError, "traverse"):
            self.verify()

    def test_dead_player_cannot_supply_obstruction_coverage(self):
        self.worlds[140]["death"]["active"] = 1
        with self.assertRaisesRegex(door.parity.EvidenceError, "surviving player"):
            self.verify()

    def test_missing_probe_cannot_be_inferred(self):
        self.probes.pop(140)
        with self.assertRaisesRegex(door.parity.EvidenceError, "continuous geometric"):
            self.verify()


if __name__ == "__main__":
    unittest.main()
