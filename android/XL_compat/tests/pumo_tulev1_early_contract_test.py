"""Asset-free negative controls for the early Tulev1 combat witness."""

import copy
import json
import unittest
from pathlib import Path

import pumo_tulev1_early_contract as contract


class Tulev1EarlyWitnessTests(unittest.TestCase):
    def test_restored_guided_inventory_rejects_forged_persistence(self):
        state = {
            "current_level_num": 7,
            "player_dead": False,
            "cheats_enabled": False,
            "position": {"segment": 458},
            "game_time_raw": 555959,
            "simulation_rng": {
                "state_available": True,
                "state": 61446058,
                "call_count": 0,
            },
            "player": {
                "shields": 96.61917114257812,
                "energy": 81.8350830078125,
                "invulnerable": False,
                "secondary_ammo": [7, 0, 0, 0, 0, 0, 4, 0, 0, 0],
                "secondary_weapon_flags": 65,
                "secondary_weapon": 0,
                "vulcan_ammo": 264,
                "lives": 3,
                "score": 36050,
                "primary_weapon": 0,
                "primary_weapon_flags": 65,
                "robots_killed_level": 1,
                "has_blue_key": False,
                "has_red_key": True,
                "has_gold_key": False,
                "cloaked": True,
            },
            "secondary_weapon_definition": {"selected_slot": 0, "weapon_id": 8},
            "combat_observation": {"guided_control": None},
            "secondary_fire": {"count": 0, "held": 0},
            "xl_pilot": {"operation_sequence": 0, "remaining_objectives": 1},
        }
        contract.restored_inventory(state, state)
        for section, key, value in (
            ("player", "secondary_ammo", [7, 0, 0, 0, 0, 0, 3, 0, 0, 0]),
            ("player", "secondary_weapon_flags", 1),
            ("player", "shields", 1),
            ("position", "segment", 457),
            ("simulation_rng", "state", 1),
            ("simulation_rng", "call_count", 1),
            ("secondary_fire", "count", 1),
            ("xl_pilot", "operation_sequence", 1),
        ):
            bad = copy.deepcopy(state)
            bad[section][key] = value
            with self.assertRaises(contract.WitnessError):
                contract.restored_inventory(state, bad)

    def test_guided_predecessor_rejects_stale_input(self):
        latest = {"sha256": "a" * 64}
        contract.earned_predecessor(latest, {"resume_sha256": "a" * 64})
        with self.assertRaises(contract.WitnessError):
            contract.earned_predecessor(latest, {"resume_sha256": "b" * 64})

    def test_guided_pickup_full_trace_and_negative_controls(self):
        here = Path(__file__).parent
        # Minimal fixture mirrors the observed 0-to-4 slot-6 pickup without private saves.
        player = {
            "secondary_ammo": [7, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            "secondary_weapon_flags": 1,
            "secondary_weapon": 0,
            "energy": 81.8350830078125,
            "shields": 96.61917114257812,
            "vulcan_ammo": 264,
            "lives": 3,
            "score": 36050,
            "primary_weapon": 0,
            "primary_weapon_flags": 65,
            "robots_killed_level": 1,
            "has_blue_key": False,
            "has_red_key": True,
            "has_gold_key": False,
            "invulnerable": False,
        }
        pre = {
            "position": {"segment": 1149},
            "level_scene": {"powerups": [{"object": 140, "id": 41, "segment": 458}]},
            "xl_pilot": {
                "wall_triggers": {"6": {"operated": True}},
                "doorways": {str(w): {"type": 4} for w in (55, 56, 57, 58)},
                "remaining_objectives": 1,
                "operation_sequence": 3,
            },
            "player": player,
            "secondary_weapon_definition": {"selected_slot": 0, "weapon_id": 8},
            "combat_observation": {"guided_control": None},
            "secondary_fire": {"count": 0, "held": 0},
        }
        post = copy.deepcopy(pre)
        post["position"]["segment"] = 458
        post["level_scene"]["powerups"] = []
        post["player"]["secondary_ammo"][6] = 4
        post["player"]["secondary_weapon_flags"] = 65
        staged = copy.deepcopy(pre)
        staged["position"]["segment"] = 457
        moving = copy.deepcopy(staged)
        trace = [
            {"step": n, "action": action, "state": copy.deepcopy(state)}
            for n, (action, state) in enumerate(
                zip(
                    (
                        "assert",
                        "face_view",
                        "assert",
                        "send_axis",
                        "wait_for",
                        "send_axis",
                        "assert",
                        "set_debug",
                    ),
                    (pre, pre, staged, staged, moving, post, post, post),
                )
            )
        ]
        plan = json.loads(
            (here / "pumo_tulev1_early_guided_inventory.json").read_text()
        )
        contract.guided_inventory_witness(trace, plan)
        for step, path, value in (
            (7, ("player", "secondary_ammo", 6), 3),
            (7, ("player", "secondary_ammo", 2), 1),
            (7, ("player", "secondary_weapon_flags"), 129),
            (0, ("level_scene", "powerups"), []),
            (
                0,
                ("level_scene", "powerups"),
                [{"object": 140, "id": 40, "segment": 458}],
            ),
            (2, ("level_scene", "powerups"), []),
            (
                7,
                ("level_scene", "powerups"),
                [x for x in pre["level_scene"]["powerups"] if x["object"] == 140],
            ),
            (7, ("combat_observation", "guided_control"), {"object": 140}),
            (7, ("secondary_fire", "count"), 1),
            (7, ("xl_pilot", "operation_sequence"), 4),
        ):
            changed = copy.deepcopy(trace)
            target = changed[step]["state"]
            for key in path[:-1]:
                target = target[key]
            target[path[-1]] = value
            with self.assertRaises(contract.WitnessError):
                contract.guided_inventory_witness(changed, plan)
        bad = copy.deepcopy(plan)
        bad["steps"][4]["timeout_ms"] = 10000
        with self.assertRaises(contract.WitnessError):
            contract.guided_inventory_witness(trace, bad)
        for extra in ({"face_object": 140}, {"face_interval_ms": 10}):
            bad = copy.deepcopy(plan)
            bad["steps"][4].update(extra)
            with self.assertRaises(contract.WitnessError):
                contract.guided_inventory_witness(trace, bad)
        bad = copy.deepcopy(plan)
        bad["steps"].append({"action": "send_button", "button": "secondary"})
        with self.assertRaises(contract.WitnessError):
            contract.guided_inventory_witness(trace, bad)
        bad = copy.deepcopy(plan)
        bad["flight_claim"] = True
        with self.assertRaises(contract.WitnessError):
            contract.guided_inventory_witness(trace, bad)

    def test_guided_receipt_rejects_aim_events(self):
        contract.guided_receipt_controls({"fixed_view_aim_events": 0})
        for value in (1, True):
            with self.assertRaises(contract.WitnessError):
                contract.guided_receipt_controls({"fixed_view_aim_events": value})

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
                "combat_observation": {
                    "load_generation": 2,
                    "tracked_objects": [actor],
                },
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
        # First parent run used 0/2/3 as if snapshots followed actions;
        # step 2 is still pre-hit and must not satisfy the damage witness.
        with self.assertRaises(contract.WitnessError):
            contract.combat_damage(
                contract.bound(events[0]["state"]),
                contract.bound(events[2]["state"]),
                contract.bound(events[3]["state"]),
            )
        for step, field, value in (
            (3, "damage_since_binding_raw", 1),
            (4, "exists", True),
        ):
            changed = copy.deepcopy(events)
            changed[step]["state"]["combat_observation"]["tracked_objects"][0][
                field
            ] = value
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
        receipt = {
            "initial_level_setup": 6,
            "final_level": 7,
            "transitions": [{"from": 6, "to": 7}],
        }
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
        state = {
            "combat_observation": {"load_generation": 2, "tracked_objects": [item]}
        }
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
