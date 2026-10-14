"""Negative save-oracle and configuration-coverage controls, independent of a game process."""

import copy
import itertools
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "helpers"))
from host_gameplay_campaign import FACTORS, TRIPLES, assert_restored
from host_config_campaign import ProbeFailure, covering_cases


class GameplayControls(unittest.TestCase):
    def setUp(self):
        self.saved = {
            "mission": {"name": "test", "version": 1},
            "game": {"level": 1},
            "ship": {"forward": [0, 0, 65536]},
        }
        self.changed = copy.deepcopy(self.saved)
        self.changed["ship"]["forward"] = [0, 20000, 60000]

    def test_real_change_and_exact_restoration_pass(self):
        self.assertEqual(assert_restored(self.saved, self.changed, self.saved)["restored_forward_error"], 0)

    def test_no_actual_change_fails(self):
        with self.assertRaises(ProbeFailure):
            assert_restored(self.saved, self.saved, self.saved)

    def test_restore_that_keeps_changed_state_fails(self):
        with self.assertRaises(ProbeFailure):
            assert_restored(self.saved, self.changed, self.changed)

    def test_wrong_mission_or_level_fails(self):
        wrong = copy.deepcopy(self.saved)
        wrong["game"]["level"] = 2
        with self.assertRaises(ProbeFailure):
            assert_restored(self.saved, self.changed, wrong)

    def test_custom_pairwise_and_high_risk_triples(self):
        rows = covering_cases(20261014, FACTORS, TRIPLES)
        for names in [*itertools.combinations(FACTORS, 2), *TRIPLES]:
            self.assertEqual(
                {tuple(r[n] for n in names) for r in rows}, set(itertools.product(*(FACTORS[n] for n in names)))
            )


if __name__ == "__main__":
    unittest.main()
