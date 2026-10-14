"""Authored switch admission rejects firing, unlock-only and traversal-only evidence."""

import copy
import unittest
from unittest import mock

import public_switch_replay as switch


class SwitchEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.header = {"game": "d2", "mission": "d2demo", "level": 2, "frame_count": 6}
        self.actions = {**switch.SWITCH, "action_frame": 2, "passage_frame": 4}
        walls = [{} for _ in range(155)]
        walls[116] = {"segnum": 85, "sidenum": 4, "type": 2, "state": 0, "keys": 1, "flags": 24}
        walls[117] = {"segnum": 235, "sidenum": 5, "type": 2, "state": 0, "keys": 1, "flags": 16}
        walls[154] = {"segnum": 84, "sidenum": 2, "trigger": 17}
        triggers = [{} for _ in range(18)]
        triggers[17] = {"type": 7, "num_links": 2, "segments": [85, 235], "sides": [4, 5]}
        segments = [{} for _ in range(85)]
        segments[84] = {"sides": [{}, {}, {"overlay": 406, "wall": 154}]}
        initial = {"walls": walls, "triggers": triggers, "segments": segments, "death": {"active": 0}}
        self.worlds = [{"type": "world_boundary", "phase": "restored", "f": 0, "state": copy.deepcopy(initial)}]
        for index in range(7):
            state = copy.deepcopy(initial)
            if index >= 3:
                state["walls"][116]["flags"] = 16
                state["segments"][84]["sides"][2]["overlay"] = 410
            if index >= 4:
                for side in (116, 117):
                    state["walls"][side].update(state=2, flags=18)
            row = {"type": "world_state", "f": index, "state": state}
            if index == 6:
                row.update(type="world_boundary", phase="terminal")
            self.worlds.append(row)
        self.states = [{"position": {"segment": segment}} for segment in (217, 213, 84, 84, 85, 235, 235)]
        self.events = [
            {
                "type": "trigger_activation",
                "version": 1,
                "f": 2,
                "segment": 84,
                "side": 2,
                "trigger": 17,
                "shot": 1,
                "activator": 0,
                "activator_type": 4,
                "activator_signature": 0,
            }
        ]

    def verify(self):
        with (
            mock.patch.object(switch.parity, "world_states", return_value=iter(self.worlds)),
            mock.patch.object(switch.parity, "records", return_value=iter(self.events)),
        ):
            return switch.verify(self.actions, self.header, self.states, "unused")

    def test_exact_authored_action_and_traversal(self):
        self.assertEqual(
            self.verify(), {"projectile_trigger_frame": 2, "both_doors_opening_frame": 4, "passage_frame": 4}
        )

    def test_passage_at_terminal_boundary_is_real_coverage(self):
        self.header["frame_count"] = 5
        self.worlds.pop()
        self.worlds[-1].update(type="world_boundary", phase="terminal")
        self.states.pop()
        self.assertEqual(self.verify()["passage_frame"], 4)

    def test_rejects_wrong_or_unverified_activation(self):
        for mutation in (
            "missing",
            "duplicate",
            "frame",
            "segment",
            "side",
            "shot",
            "activator",
            "activator_type",
            "activator_signature",
            "bool",
        ):
            with self.subTest(mutation=mutation):
                self.setUp()
                if mutation == "missing":
                    self.events.clear()
                elif mutation == "duplicate":
                    self.events *= 2
                elif mutation == "frame":
                    self.events[0]["f"] += 1
                elif mutation == "bool":
                    self.events[0]["shot"] = True
                else:
                    self.events[0][mutation] += 1
                with self.assertRaises(switch.parity.EvidenceError):
                    self.verify()

    def test_rejects_wrong_world_outcomes(self):
        for mutation in (
            "initial_lock",
            "reverse_initial",
            "early_unlock",
            "late_unlock",
            "overlay",
            "link",
            "one_door",
            "blocked",
            "dead",
            "missing_world",
        ):
            with self.subTest(mutation=mutation):
                self.setUp()
                if mutation == "initial_lock":
                    self.worlds[0]["state"]["walls"][116]["flags"] = 16
                elif mutation == "reverse_initial":
                    self.worlds[0]["state"]["walls"][117]["flags"] = 24
                elif mutation == "early_unlock":
                    self.worlds[3]["state"]["walls"][116]["flags"] = 16
                elif mutation == "late_unlock":
                    self.worlds[4]["state"]["walls"][116]["flags"] = 24
                elif mutation == "overlay":
                    self.worlds[4]["state"]["segments"][84]["sides"][2]["overlay"] = 406
                elif mutation == "link":
                    self.worlds[0]["state"]["triggers"][17]["segments"][1] = 234
                elif mutation == "one_door":
                    for row in self.worlds:
                        row["state"]["walls"][117]["state"] = 0
                elif mutation == "blocked":
                    self.worlds[6]["state"]["walls"][117]["flags"] = 16
                elif mutation == "dead":
                    self.worlds[2]["state"]["death"]["active"] = 1
                else:
                    self.worlds.pop()
                with self.assertRaises(switch.parity.EvidenceError):
                    self.verify()

    def test_preopened_passage_cannot_qualify_unlock_and_traversal(self):
        for row in self.worlds[2:]:
            for side in (116, 117):
                row["state"]["walls"][side]["state"] = 2
                row["state"]["walls"][side]["flags"] |= 2
        with self.assertRaises(switch.parity.EvidenceError):
            self.verify()

    def test_malformed_world_is_a_named_evidence_failure(self):
        for mutation in ("walls", "triggers", "segments"):
            with self.subTest(mutation=mutation):
                self.setUp()
                self.worlds[0]["state"][mutation] = []
                with self.assertRaises(switch.parity.EvidenceError):
                    self.verify()

    def test_rejects_bypass_wrong_scene_and_untraversed_passage(self):
        for mutation in (
            "wrong_game",
            "wrong_level",
            "wrong_wall",
            "bool_frame",
            "wrong_order",
            "no_crossing",
            "earlier_bypass",
            "wrong_origin",
        ):
            with self.subTest(mutation=mutation):
                self.setUp()
                if mutation == "wrong_game":
                    self.header["game"] = "d1"
                elif mutation == "wrong_level":
                    self.header["level"] = 1
                elif mutation == "wrong_wall":
                    self.actions["front_wall"] = 117
                elif mutation == "bool_frame":
                    self.actions["action_frame"] = True
                elif mutation == "wrong_order":
                    self.actions["passage_frame"] = 2
                elif mutation == "no_crossing":
                    self.states[5]["position"]["segment"] = 85
                elif mutation == "earlier_bypass":
                    self.states[2]["position"]["segment"] = 235
                else:
                    self.states[0]["position"]["segment"] = 216
                with self.assertRaises(switch.parity.EvidenceError):
                    self.verify()


if __name__ == "__main__":
    unittest.main()
