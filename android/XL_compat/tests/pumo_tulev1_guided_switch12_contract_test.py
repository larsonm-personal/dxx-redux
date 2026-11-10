"""Asset-free source and provenance forgeries for earned Guided switch12."""

import copy
import json
import tempfile
import unittest
from pathlib import Path

import pumo_tulev1_guided_switch12_contract as contract


def state(object_id=9, signature=31):
    guide = {
        "object": object_id,
        "signature": signature,
        "weapon_id": 37,
        "parent_object": 0,
        "parent_signature": 0,
        "lifeleft_raw": 90,
    }
    owned = dict(guide, parent_type=4)
    return {
        "current_level_num": 7,
        "game_time_raw": 100,
        "player_dead": False,
        "cheats_enabled": False,
        "player": {"shields": 60, "invulnerable": False},
        "combat_observation": {
            "player_object": 0,
            "player_signature": 0,
            "guided_control": guide,
            "owned_weapon_count": 1,
            "owned_weapons_truncated": False,
            "owned_weapons": [owned],
        },
    }


def final_state():
    value = state()
    value["combat_observation"]["guided_control"] = None
    value["combat_observation"]["owned_weapon_count"] = 0
    value["combat_observation"]["owned_weapons"] = []
    value["combat_observation"]["owned_weapons_truncated"] = False
    value["xl_pilot"] = {
        "wall_triggers": {"12": {"operated": True, "shot": 1}},
        "operation_counts": {"12": 1},
        "operation_sequence": 1,
        "trigger_activated": {"12": True},
        "doorways": {"201": {"type": 4, "state": 0}, "202": {"type": 4, "state": 0}},
    }
    return value


def witnesses():
    launch = state()
    operation = copy.deepcopy(launch)
    operation["game_time_raw"] = 130
    operation["xl_pilot"] = {
        "wall_triggers": {"12": {"operated": True, "shot": 1}},
        "operation_counts": {"12": 1},
        "operation_sequence": 1,
    }
    tail = final_state()
    events = [{"step": i, "action": "introspect", "state": copy.deepcopy(launch)} for i in range(15)]
    events.append({"step": 15, "action": "send_button", "state": launch})
    events.append({"step": 16, "action": "assert", "state": copy.deepcopy(launch)})
    events.append(
        {
            "event": "successful_wall_operation",
            "index": 12,
            "shot": 1,
            "sequence": 1,
            "state": operation,
        }
    )
    events.append({"event": "trigger_change", "state": copy.deepcopy(tail)})
    events.append(
        {
            "event": "weapon_wall_contacts",
            "result": {
                "kind": "actual_contacts_reaching_effect_blowup_gate",
                "truncated": False,
                "contact_count_lower_bound": 1,
                "contacts": [
                    {
                        "object": 9,
                        "signature": 31,
                        "weapon_id": 37,
                        "parent_object": 0,
                        "parent_signature": 0,
                        "level": 7,
                        "wall": 204,
                        "trigger": 12,
                        "overlay_raw": 414,
                        "eclip": 72,
                        "hit_segment": 1538,
                        "hit_side": 2,
                        "blew_up": 1,
                    }
                ],
            },
        }
    )
    return events, tail


class GuidedSwitch12ContractTest(unittest.TestCase):
    def test_source_only_control_and_settled_postlude(self):
        warm = contract.template("warm")
        cold = contract.template("cold")
        self.assertEqual(len(warm["steps"]), 1861)
        self.assertEqual(cold["steps"], [{"action": "introspect"}])
        self.assertEqual(warm["steps"][6], {"action": "select_secondary", "slot": 1})
        self.assertEqual(warm["steps"][10]["action"], "pose_view")
        self.assertEqual(
            warm["steps"][13:16:2],
            [
                {"action": "send_button", "button": 101, "held": 1},
                {"action": "send_button", "button": 101, "held": 0},
            ],
        )
        self.assertEqual(
            warm["steps"][-2:],
            [
                {
                    "action": "set_debug",
                    "field": "android_game_request",
                    "value": "quick_save",
                },
                {"action": "introspect"},
            ],
        )
        self.assertEqual([s["action"] for s in warm["steps"]].count("pose_view"), 1)
        self.assertEqual([s["action"] for s in warm["steps"]].count("select_secondary"), 1)
        self.assertFalse(any(s["action"] in {"load", "save", "face_view", "set_invulnerable"} for s in warm["steps"]))
        self.assertNotIn("resume_checkpoint", warm)
        self.assertNotIn("resume_sha256", warm)
        self.assertNotIn("game_time_raw", warm["checkpoint_expect"])
        self.assertNotIn("player.shields", warm["checkpoint_expect"])
        self.assertNotIn("player.energy", warm["checkpoint_expect"])
        self.assertNotIn("game_time_raw", warm["steps"][16]["expect"])
        self.assertNotIn("game_time_raw", warm["steps"][18]["expect"])
        self.assertTrue(warm["observe_weapon_wall_contacts"])

    def test_runtime_source_binding_and_forged_inputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            plan_path = root / "warm-plan.json"
            producer = root / "guard255-result.json"
            plan = contract.template("warm")
            plan.update(resume_checkpoint="earned.sav", resume_sha256="a" * 64)
            source = {
                "checkpoint_history": [
                    {
                        "step": 44,
                        "state": {
                            "game_time_raw": 120,
                            "player": {"shields": 70, "energy": 80},
                        },
                    }
                ]
            }
            plan_path.write_text(json.dumps(plan))
            producer.write_text(json.dumps(source))
            contract.bind(plan_path, producer)
            bound = contract.doc(plan_path)
            self.assertEqual(bound["checkpoint_expect"]["game_time_raw"], 120)
            self.assertEqual(bound["steps"][0]["expect"]["player.shields"], 70)
            for change in (
                lambda x: x["checkpoint_history"][0].update(step=43),
                lambda x: x["checkpoint_history"].append(copy.deepcopy(x["checkpoint_history"][0])),
            ):
                forged = copy.deepcopy(source)
                change(forged)
                producer.write_text(json.dumps(forged))
                plan_path.write_text(json.dumps(plan))
                with self.assertRaises(ValueError):
                    contract.bind(plan_path, producer)

    def test_live_identity_hp_pointer_and_operation_forgeries(self):
        good = state()
        self.assertEqual(contract.identity(good), (9, 31))
        for mutate in (
            lambda s: s["combat_observation"]["guided_control"].update(signature=32),
            lambda s: s["combat_observation"]["owned_weapons"][0].update(parent_type=2),
            lambda s: s["combat_observation"].update(owned_weapon_count=0),
            lambda s: s["combat_observation"]["guided_control"].update(lifeleft_raw=0),
        ):
            forged = copy.deepcopy(good)
            mutate(forged)
            with self.assertRaises(ValueError):
                contract.identity(forged)
        events, _ = witnesses()
        self.assertEqual(contract.flight(events, [e for e in events if "step" in e])[0], (9, 31))
        for mutate in (
            lambda es: es[16]["state"]["player"].update(shields=59),
            lambda es: es[16]["state"]["combat_observation"]["guided_control"].update(signature=32),
            lambda es: es[17].update(index=11),
            lambda es: es[17].update(sequence=2),
            lambda es: es[17].update(shot=True),
            lambda es: es[16]["state"]["player"].update(invulnerable=0),
            lambda es: es[16]["state"]["player"].update(invulnerable=True),
        ):
            forged = copy.deepcopy(events)
            mutate(forged)
            with self.assertRaises(ValueError):
                contract.flight(forged, [e for e in forged if "step" in e])

    def test_relative_timing_forgeries(self):
        events, _ = witnesses()
        steps = [e for e in events if "step" in e]
        source = {"game_time_raw": 90}
        steps[15]["state"]["game_time_raw"] = 100
        # The synthetic end state is independent of private measurements.
        steps.extend({"step": i, "state": {"game_time_raw": 90 + 820 * 65536 // 60}} for i in range(17, 1859))
        contract.relative_timing(source, steps, events[17]["state"])
        for forged_origin in (91, True):
            with self.assertRaises(ValueError):
                contract.relative_timing({"game_time_raw": forged_origin}, steps, events[17]["state"])

    def test_contact_effect_and_door_forgeries(self):
        events, final = witnesses()
        contract.target(events, final, (9, 31), (0, 0))
        for mutate in (
            lambda es, f: es[-1]["result"]["contacts"][0].update(signature=32),
            lambda es, f: es[-1]["result"]["contacts"][0].update(trigger=13),
            lambda es, f: es[-1]["result"]["contacts"][0].update(blew_up=True),
            lambda es, f: es[-1]["result"]["contacts"][0].update(overlay_raw=413),
            lambda es, f: es[-1]["result"].update(truncated=True),
            lambda es, f: es.append(copy.deepcopy(es[-1])),
            lambda es, f: f["xl_pilot"]["doorways"]["201"].update(type=3),
            lambda es, f: f["xl_pilot"]["doorways"]["201"].update(state=False),
            lambda es, f: f["xl_pilot"]["wall_triggers"]["12"].update(shot=0),
            lambda es, f: f["combat_observation"].update(
                guided_control=state()["combat_observation"]["guided_control"]
            ),
        ):
            forged_events, forged_final = copy.deepcopy((events, final))
            mutate(forged_events, forged_final)
            with self.assertRaises(ValueError):
                contract.target(forged_events, forged_final, (9, 31), (0, 0))

    def test_settled_save_state_forgeries(self):
        settled = final_state()
        contract.settled_observation(settled)
        for mutate in (
            lambda s: s["combat_observation"].update(guided_control=state()["combat_observation"]["guided_control"]),
            lambda s: s["combat_observation"].update(owned_weapon_count=True),
            lambda s: s["combat_observation"].update(owned_weapons_truncated=True),
            lambda s: s["combat_observation"].update(owned_weapons=[state()["combat_observation"]["owned_weapons"][0]]),
        ):
            forged = copy.deepcopy(settled)
            mutate(forged)
            with self.assertRaises(ValueError):
                contract.settled_observation(forged)

    def test_cold_full_persistence_forgeries(self):
        source = final_state()
        source.update(
            position={"segment": 647},
            level_scene={"powerups": [20]},
            reactor_countdown=None,
            simulation_rng={"state": 77, "call_count": 8},
        )
        cold = copy.deepcopy(source)
        cold["simulation_rng"]["call_count"] = 0
        cold["xl_pilot"]["operation_counts"] = {}
        cold["xl_pilot"]["operation_sequence"] = 0
        cold["xl_pilot"]["trigger_activated"] = {}
        contract.cold_persistence(cold, source)
        for mutate in (
            lambda s: s["position"].update(segment=648),
            lambda s: s["level_scene"]["powerups"].append(21),
            lambda s: s["simulation_rng"].update(state=78),
            lambda s: s["simulation_rng"].update(state_available=False),
            lambda s: s["simulation_rng"].update(call_count=1),
            lambda s: s["xl_pilot"]["wall_triggers"]["12"].update(shot=0),
            lambda s: s["xl_pilot"]["doorways"]["202"].update(type=3),
        ):
            forged = copy.deepcopy(cold)
            mutate(forged)
            with self.assertRaises(ValueError):
                contract.cold_persistence(forged, source)


if __name__ == "__main__":
    unittest.main()
