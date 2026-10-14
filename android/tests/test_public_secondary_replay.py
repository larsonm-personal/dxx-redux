"""Independent action gates reject lost ordering and changed two-tap outcomes."""

import copy
import json
from pathlib import Path
import tempfile
import unittest

import public_secondary_replay as secondary


class SecondaryTests(unittest.TestCase):
    def fixture(self, root, game, scenario):
        case = {"scenario": scenario, "actions": secondary.actions_for(scenario)}
        header = {"type": "header", "version": 5, "game": game, "frame_count": 25, "level": 1}
        rows = [header]
        frames = []
        for index in range(25):
            held = index in (2, 12)
            state = {"f2s": int(held)}
            ops = [{"k": "reset", "ft": 3276}, {"k": "sample", "ft": 3276, "s": state}]
            if index == 13:
                ops = [
                    {"k": "reset", "ft": 3276},
                    {"k": "sample", "ft": 3276, "s": {"f2s": int(scenario.endswith("mouse-first"))}},
                    {"k": "reset", "ft": 3276},
                    {"k": "sample", "ft": 3276, "s": {}},
                ]
            rows.append(
                {
                    "type": "frame",
                    "f": index,
                    "ft": 3276,
                    "input": {"s": state},
                    "rng": {"s": 1234, "c": 0},
                    "dispatch": {"pre_rng": "=", "ops": ops},
                }
            )
            ammo = 5 if index < 2 else 4 if index < 13 else case["actions"]["terminal_ammo"]
            frames.append(
                {
                    "ft": 3276,
                    "state": {"player0": {"secondary_ammo": [ammo]}},
                    "diag": {"player_weapon_count": 5 - ammo},
                }
            )
        rows.extend(
            [
                {
                    "type": "input_tail",
                    "f": 25,
                    "input": {},
                    "rng": {"s": 1234, "c": 0},
                    "dispatch": {"pre_rng": "=", "ops": []},
                },
                {"type": "result", "result": {}},
            ]
        )
        demo = root / (game + scenario + ".dximdemo")
        demo.write_text("\n".join(json.dumps(row) for row in rows) + "\n")
        return case, demo, header, frames, copy.deepcopy(frames[-1]["state"])

    def test_both_games_and_both_orders_require_exact_native_outcomes(self):
        with tempfile.TemporaryDirectory() as root:
            for game in ("d1", "d2"):
                for scenario in secondary.SCENARIOS:
                    args = self.fixture(Path(root), game, scenario)
                    result = secondary.verify(*args)
                    self.assertEqual(result["secondary_shots"], 5 - args[0]["actions"]["terminal_ammo"])

    def test_wrong_frame_or_terminal_ammo_is_rejected(self):
        with tempfile.TemporaryDirectory() as root:
            for target in ("frame", "terminal", "actions", "timestep"):
                args = self.fixture(Path(root), "d1", "secondary-release-first")
                if target == "frame":
                    args[3][13]["state"]["player0"]["secondary_ammo"][0] = 3
                elif target == "terminal":
                    args[4]["player0"]["secondary_ammo"][0] = 3
                elif target == "actions":
                    args[0]["actions"]["second_press_ticks_early"] = 0
                else:
                    args[3][12]["ft"] += 1
                with self.subTest(target=target), self.assertRaises(secondary.parity.EvidenceError):
                    secondary.verify(*args)

    def test_swapped_or_deleted_dispatch_is_rejected_even_with_unchanged_oracle(self):
        with tempfile.TemporaryDirectory() as root:
            for mutation in ("swap", "delete"):
                args = self.fixture(Path(root), "d2", "secondary-mouse-first")
                rows = list(secondary.parity.records(args[1]))
                ops = rows[14]["dispatch"]["ops"]
                rows[14]["dispatch"]["ops"] = ops[2:] + ops[:2] if mutation == "swap" else ops[2:]
                args[1].write_text("\n".join(json.dumps(row) for row in rows) + "\n")
                with self.subTest(mutation=mutation), self.assertRaises(secondary.parity.EvidenceError):
                    secondary.verify(*args)


if __name__ == "__main__":
    unittest.main()
