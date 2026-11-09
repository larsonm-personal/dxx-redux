"""Asset-free negative controls for the early Tulev1 combat witness."""

import copy
import unittest

import pumo_tulev1_early_contract as contract


class Tulev1EarlyWitnessTests(unittest.TestCase):
    def test_full_combat_trace_uses_pre_action_snapshots(self):
        def snapshot(shields, damage, exists, kills):
            actor = {
                "object": 256,
                "expected_id": 28,
                "expected_type": 2,
                "expected_signature": 257,
                "expected_level": 7,
                "observed_level": 7,
                "expected_load_generation": 2,
                "observed_load_generation": 2,
                "exists": exists,
                "binding_shields_raw": 12435456,
                "damage_since_binding_raw": damage,
                "actor_state": {"shields_raw": shields} if exists else None,
            }
            return {
                "combat_observation": {"load_generation": 2, "tracked_objects": [actor]},
                "player": {"robots_killed_level": kills},
            }

        events = [
            {"step": step, "action": action, "state": snapshot(*values)}
            for step, (action, values) in enumerate(
                (
                    ("assert", (12435456, 0, True, 0)),
                    ("send_button", (12435456, 0, True, 0)),
                    ("wait_for", (12435456, 0, True, 0)),
                    ("wait_for", (11091968, 1343488, True, 0)),
                    ("send_button", (None, None, False, 1)),
                )
            )
        ]
        contract.combat_trace_witness(events)
        for step, field, value in ((3, "damage_since_binding_raw", 1), (4, "exists", True)):
            changed = copy.deepcopy(events)
            changed[step]["state"]["combat_observation"]["tracked_objects"][0][field] = value
            with self.assertRaises(contract.WitnessError):
                contract.combat_trace_witness(changed)
        changed = copy.deepcopy(events)
        changed[4]["state"]["player"]["robots_killed_level"] = 0
        with self.assertRaises(contract.WitnessError):
            contract.combat_trace_witness(changed)

    def test_damage_is_relative_to_actual_live_binding(self):
        pre = {
            "exists": True,
            "expected_signature": 257,
            "expected_load_generation": 2,
            "binding_shields_raw": 12435456,
            "damage_since_binding_raw": 0,
            "actor_state": {"shields_raw": 12435456},
        }
        damaged = copy.deepcopy(pre)
        damaged["actor_state"]["shields_raw"] = 11091968
        damaged["damage_since_binding_raw"] = 1343488
        defeated = copy.deepcopy(damaged)
        defeated["exists"] = False
        defeated["actor_state"] = None
        defeated["damage_since_binding_raw"] = None
        contract.combat_damage(pre, damaged, defeated)
        for mutation in (
            lambda a, b, c: b["actor_state"].update(shields_raw=12435456),
            lambda a, b, c: b.update(expected_signature=258),
            lambda a, b, c: b.update(expected_load_generation=3),
            lambda a, b, c: b.update(binding_shields_raw=13107200),
            lambda a, b, c: b.update(binding_shields_raw=True),
            lambda a, b, c: a.update(binding_shields_raw=13107200),
            lambda a, b, c: a.update(damage_since_binding_raw=1),
            lambda a, b, c: b.update(damage_since_binding_raw=0),
            lambda a, b, c: b.update(damage_since_binding_raw=1),
            lambda a, b, c: c.update(binding_shields_raw=13107200),
            lambda a, b, c: c.update(exists=True),
            lambda a, b, c: c.update(damage_since_binding_raw=1343488),
        ):
            a, b, c = copy.deepcopy((pre, damaged, defeated))
            mutation(a, b, c)
            with self.assertRaises(contract.WitnessError):
                contract.combat_damage(a, b, c)

    def test_producer_requires_typed_sophodo_transition(self):
        receipt = {"initial_level_setup": 6, "final_level": 7, "transitions": [{"from": 6, "to": 7}]}
        contract.producer_progression(receipt)
        for field, value in (
            ("initial_level_setup", True),
            ("initial_level_setup", 7),
            ("final_level", "7"),
            ("final_level", 8),
            ("transitions", []),
            ("transitions", [{"from": 7, "to": 7}]),
            ("transitions", [{"from": "6", "to": 7}]),
            ("transitions", [{"from": 6, "to": True}]),
            ("transitions", [{"from": 6, "to": 7}, {"from": 7, "to": 8}]),
        ):
            changed = copy.deepcopy(receipt)
            changed[field] = value
            with self.assertRaises(contract.WitnessError):
                contract.producer_progression(changed)
        changed = copy.deepcopy(receipt)
        del changed["transitions"]
        with self.assertRaises(KeyError):
            contract.producer_progression(changed)

    def test_binding_rejects_identity_or_generation_change(self):
        item = {
            "object": 256,
            "expected_id": 28,
            "expected_type": 2,
            "expected_level": 7,
            "observed_level": 7,
            "expected_load_generation": 2,
            "observed_load_generation": 2,
        }
        state = {"combat_observation": {"load_generation": 2, "tracked_objects": [item]}}
        contract.bound(state)
        for field, value in (
            ("expected_id", 29),
            ("expected_level", 6),
            ("observed_load_generation", 3),
        ):
            changed = copy.deepcopy(state)
            changed["combat_observation"]["tracked_objects"][0][field] = value
            with self.assertRaises(contract.WitnessError):
                contract.bound(changed)

    def test_operation_order_and_types(self):
        events = [
            {
                "event": "successful_wall_operation",
                "index": index,
                "sequence": sequence,
                "shot": 0,
                "state": {"current_level_num": 7},
            }
            for sequence, index in enumerate((64, 63, 61), 1)
        ]
        expected = [(64, 1, 0), (63, 2, 0), (61, 3, 0)]
        contract.operations(events, expected)
        for field, value in (("index", 65), ("shot", True), ("sequence", 2)):
            changed = copy.deepcopy(events)
            changed[0][field] = value
            with self.assertRaises(contract.WitnessError):
                contract.operations(changed, expected)


if __name__ == "__main__":
    unittest.main()
