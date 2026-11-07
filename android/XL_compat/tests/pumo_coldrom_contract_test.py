"""Asset-free positive and negative controls for Coldrom native witnesses."""

import copy
import json
import tempfile
import unittest
from pathlib import Path

import pumo_coldrom_contract as contract


def actor(number, signature, shields=100, exists=True, flags=0):
    return {
        "object": number,
        "expected_signature": signature,
        "expected_level": 14,
        "observed_level": 14,
        "expected_load_generation": 2,
        "observed_load_generation": 2,
        "exists": exists,
        "actor_state": {"shields_raw": shields, "flags": flags} if exists else None,
    }


def state(actors=()):
    return {
        "current_level_num": 14,
        "combat_observation": {
            "load_generation": 2,
            "owned_weapons_truncated": False,
            "owned_weapon_count": 0,
            "owned_weapons": [],
            "player_object": 0,
            "player_signature": 0,
            "tracked_objects": list(actors),
        },
    }


class ColdromWitnessTests(unittest.TestCase):
    def test_all_bounded_plan_templates(self):
        directory = Path(__file__).parent
        result = contract.templates_witness(directory)
        self.assertEqual(
            result["counts"], {"fusion": 14, "main": 55, "cold": 17, "gold": 8}
        )
        for name in ("fusion", "main", "cold", "gold"):
            plan = json.loads((directory / f"pumo_coldrom_{name}.json").read_text())
            missing_entry = copy.deepcopy(plan)
            del missing_entry["checkpoint_expect"]
            with self.assertRaises(contract.WitnessError):
                contract.template_witness(name, missing_entry)
            wrong_action = copy.deepcopy(plan)
            wrong_action["steps"][0]["action"] = "grant"
            with self.assertRaises(contract.WitnessError):
                contract.template_witness(name, wrong_action)
            wrong_count = copy.deepcopy(plan)
            wrong_count["steps"].pop()
            with self.assertRaises(contract.WitnessError):
                contract.template_witness(name, wrong_count)

    def test_bound_damage_and_reactor_death(self):
        first = state([actor(125, 126, 900)])
        damaged = state([actor(125, 126, 880)])
        contract.decreased(first, damaged, 125, 126)
        contract.destroyed(
            state([actor(125, 126, -2, flags=5)]), 125, 126, reactor=True
        )
        contract.destroyed(
            state([actor(125, 126, exists=False)]), 125, 126, reactor=True
        )
        with self.assertRaises(contract.WitnessError):
            contract.decreased(first, first, 125, 126)
        with self.assertRaises(contract.WitnessError):
            contract.decreased(first, state([actor(125, 127, 880)]), 125, 126)
        with self.assertRaises(contract.WitnessError):
            contract.destroyed(damaged, 125, 126, reactor=True)
        with self.assertRaises(contract.WitnessError):
            contract.destroyed(state([actor(103, 104, 0)]), 103, 104)
        wrong_generation = state([actor(125, 126, exists=False)])
        wrong_generation["combat_observation"]["load_generation"] = 3
        with self.assertRaises(contract.WitnessError):
            contract.destroyed(wrong_generation, 125, 126, reactor=True, generation=2)
        wrong_level = state([actor(125, 126, exists=False)])
        wrong_level["current_level_num"] = 5
        with self.assertRaises(contract.WitnessError):
            contract.destroyed(wrong_level, 125, 126, reactor=True, generation=2)
        mismatched_binding = state([actor(125, 126, exists=False)])
        mismatched_binding["combat_observation"]["tracked_objects"][0][
            "observed_load_generation"
        ] = 3
        with self.assertRaises(contract.WitnessError):
            contract.destroyed(mismatched_binding, 125, 126, reactor=True, generation=2)
        mismatched_level = state([actor(125, 126, exists=False)])
        mismatched_level["combat_observation"]["tracked_objects"][0][
            "observed_level"
        ] = 5
        with self.assertRaises(contract.WitnessError):
            contract.destroyed(mismatched_level, 125, 126, reactor=True, generation=2)
        missing_state = state([actor(125, 126, exists=False)])
        missing_state["combat_observation"]["tracked_objects"][0]["actor_state"] = {
            "shields_raw": 0
        }
        with self.assertRaises(contract.WitnessError):
            contract.destroyed(missing_state, 125, 126, reactor=True, generation=2)

    def test_original_player_collision_order(self):
        indices = [4, 20, 21, 22, 5, *range(6, 20)]
        trace = []
        for sequence, index in enumerate(indices):
            current = state()
            current["xl_pilot"] = {
                "wall_triggers": {
                    str(index): {"actor": 0, "shot": 0, "operated": True}
                },
                "remaining_objectives": 0,
            }
            current["reactor_countdown"] = {"total_seconds": 99}
            current["current_level_num"] = 14
            current["observation_boundary"] = "pre_exit" if index == 19 else "normal"
            trace.append(
                {
                    "event": "successful_wall_operation",
                    "index": index,
                    "sequence": sequence,
                    "shot": 0,
                    "state": current,
                }
            )
        contract.original_operations(trace, indices)
        wrong_actor = copy.deepcopy(trace)
        wrong_actor[0]["state"]["xl_pilot"]["wall_triggers"]["4"]["actor"] = 7
        with self.assertRaises(contract.WitnessError):
            contract.original_operations(wrong_actor, indices)
        wrong_order = copy.deepcopy(trace)
        wrong_order[5]["sequence"] = -1
        with self.assertRaises(contract.WitnessError):
            contract.original_operations(wrong_order, indices)
        with self.assertRaises(contract.WitnessError):
            contract.original_operations(trace[:-1], indices)
        wrong_shot = copy.deepcopy(trace)
        wrong_shot[0]["shot"] = 1
        with self.assertRaises(contract.WitnessError):
            contract.original_operations(wrong_shot, indices)

    def test_live_fusion_projectile(self):
        before = state()
        before.update(
            player_dead=False,
            cheats_enabled=False,
            player={"primary_weapon_flags": 17, "energy": 100},
            primary_fire={"held": 0, "fusion_charge_raw": 0},
            game_time_raw=10,
        )
        charged = copy.deepcopy(before)
        charged["player"]["energy"] = 98
        charged["primary_fire"] = {"held": 1, "fusion_charge_raw": 16384}
        charged["game_time_raw"] = 20
        released = copy.deepcopy(charged)
        released["player"]["energy"] = 97
        released["primary_fire"] = {"held": 0, "fusion_charge_raw": 0}
        released["game_time_raw"] = 21
        projectile = {
            "object": 2,
            "signature": 3,
            "weapon_id": 14,
            "parent_object": 0,
            "parent_signature": 0,
            "parent_type": 4,
            "creation_time_raw": 21,
            "lifeleft_raw": 100,
        }
        released["combat_observation"]["owned_weapons"] = [projectile]
        trace = [
            {"step": step, "state": current}
            for step, current in ((6, before), (9, charged), (12, released))
        ]
        contract.fusion_witness(trace)
        missing = copy.deepcopy(trace)
        missing[-1]["state"]["combat_observation"]["owned_weapons"] = []
        with self.assertRaises(contract.WitnessError):
            contract.fusion_witness(missing)
        wrong_parent = copy.deepcopy(trace)
        wrong_parent[-1]["state"]["combat_observation"]["owned_weapons"][0][
            "parent_signature"
        ] = 9
        with self.assertRaises(contract.WitnessError):
            contract.fusion_witness(wrong_parent)
        wrong_held_type = copy.deepcopy(trace)
        wrong_held_type[1]["state"]["primary_fire"]["held"] = True
        with self.assertRaises(contract.WitnessError):
            contract.fusion_witness(wrong_held_type)

    def test_original_gold_powerup(self):
        before = state()
        before.update(
            current_level_num=5,
            player_dead=False,
            player={"primary_weapon_flags": 17, "has_gold_key": False, "shields": 100},
            xl_pilot={
                "wall_triggers": {"17": {"actor": -1, "shot": 0, "operated": False}},
                "doorways": {"34": {"type": 5}},
            },
            level_scene={"powerups": [{"object": 28, "id": 6}]},
        )
        after = copy.deepcopy(before)
        after["player"]["has_gold_key"] = True
        after["xl_pilot"]["wall_triggers"]["17"] = {
            "actor": 0,
            "shot": 0,
            "operated": True,
        }
        after["xl_pilot"]["doorways"]["34"]["type"] = 4
        after["level_scene"]["powerups"] = []
        trace = [
            {"step": 0, "state": before},
            {
                "event": "successful_wall_operation",
                "index": 17,
                "sequence": 1,
                "shot": 0,
                "state": after,
            },
            {"step": 5, "state": after},
        ]
        contract.gold_witness(trace)
        missing = copy.deepcopy(trace)
        missing[-1]["state"]["level_scene"]["powerups"] = [{"object": 28, "id": 6}]
        with self.assertRaises(contract.WitnessError):
            contract.gold_witness(missing)

    def test_preserved_save_state(self):
        saved = {
            "current_level_num": 14,
            "player": {
                "shields": 56.8,
                "energy": 84.8,
                "primary_weapon": 0,
                "primary_weapon_flags": 17,
                "vulcan_ammo": 0,
                "has_blue_key": False,
                "has_red_key": False,
                "has_gold_key": False,
                "cloaked": False,
                "invulnerable": False,
                "lives": 3,
                "score": 100,
                "robots_killed_level": 2,
            },
            "reactor_countdown": {
                "active": True,
                "total_seconds": 90,
                "timer": 5896055,
            },
            "simulation_rng": {
                "state_available": True,
                "state": 12345,
                "call_count": 100,
            },
        }
        restored = copy.deepcopy(saved)
        restored["simulation_rng"]["call_count"] = 0
        contract.preserved_entry(saved, restored)
        for section, field, wrong in (
            ("player", "shields", 100),
            ("reactor_countdown", "timer", 0),
            ("simulation_rng", "state", 999),
        ):
            changed = copy.deepcopy(restored)
            changed[section][field] = wrong
            with self.assertRaises(contract.WitnessError):
                contract.preserved_entry(saved, changed)

    def test_predecessor_fixture_chain(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            previews = root / "preview-user"
            previews.mkdir()
            engine = root / "engine"
            engine.write_bytes(b"public engine")
            entry = previews / "pumo-player-level14.sav"
            pickup_versioned = previews / "pickup-step10.sav"
            alternate_entry = previews / "coldrom-entry.sav"
            alternate_versioned = previews / "alternate-step9.sav"
            reactor_entry = previews / "metillis-reactor-earned.sav"
            for path, data in (
                (entry, b"pickup"),
                (pickup_versioned, b"pickup"),
                (alternate_entry, b"alternate"),
                (alternate_versioned, b"alternate"),
                (reactor_entry, b"reactor"),
            ):
                path.write_bytes(data)
            checkpoint_state = {
                "current_level_num": 14,
                "player_dead": False,
                "cheats_enabled": False,
                "player": {"invulnerable": False, "primary_weapon_flags": 17},
            }
            cold_plan = {
                "initial_level": 14,
                "steps": [{}] * 15,
                "resume_checkpoint": alternate_entry.name,
                "resume_sha256": contract.sha(alternate_entry),
                "receipt_run_id": "a" * 32,
            }
            alternate_plan = {
                "resume_checkpoint": reactor_entry.name,
                "resume_sha256": contract.sha(reactor_entry),
                "receipt_run_id": "b" * 32,
            }
            (root / "coldrom-executed-plan.json").write_text(json.dumps(cold_plan))
            (root / "alternate-executed-plan.json").write_text(
                json.dumps(alternate_plan)
            )
            alternate = {
                "receipt_version": 2,
                "result": "PASS",
                "steps": 14,
                "initial_level_setup": 4,
                "final_level": 14,
                "transitions": [{"from": 4, "to": 14}],
                "resumed_earned_checkpoint": True,
                "run_id": "b" * 32,
                "plan_sha256": contract.sha(root / "alternate-executed-plan.json"),
                "executable_sha256": contract.sha(engine),
                "resume_sha256": contract.sha(reactor_entry),
                "checkpoint_history": [
                    {
                        "step": 9,
                        "file": alternate_versioned.name,
                        "sha256": contract.sha(alternate_versioned),
                    }
                ],
                "checkpoints": {"14": {"sha256": contract.sha(alternate_entry)}},
            }
            receipt = {
                "receipt_version": 2,
                "result": "PASS",
                "steps": 15,
                "initial_level_setup": 14,
                "final_level": 14,
                "transitions": [],
                "resumed_earned_checkpoint": True,
                "run_id": "a" * 32,
                "plan_sha256": contract.sha(root / "coldrom-executed-plan.json"),
                "executable_sha256": contract.sha(engine),
                "resume_sha256": contract.sha(alternate_entry),
                "checkpoint_history": [
                    {
                        "step": 10,
                        "file": pickup_versioned.name,
                        "sha256": contract.sha(pickup_versioned),
                        "state": checkpoint_state,
                    }
                ],
                "checkpoints": {
                    "14": {"sha256": contract.sha(entry), "state": checkpoint_state}
                },
            }
            (root / "main-result.json").write_text(
                json.dumps(
                    {
                        "result": "PASS",
                        "checkpoints": {"4": {"sha256": contract.sha(reactor_entry)}},
                    }
                )
            )
            (root / "alternate-result.json").write_text(json.dumps(alternate))
            producer = root / "coldrom-result.json"
            producer.write_text(json.dumps(receipt))
            (root / "player-result.json").write_text(json.dumps(receipt))
            for member, data in (("state.json", "{}\n"), ("trace.jsonl", "{}\n")):
                (root / f"coldrom-{member}").write_text(data)
                (root / f"player-{member}").write_text(data)
            contract.producer_witness(producer, entry, engine)
            wrong_nonce = {**receipt, "run_id": "c" * 32}
            producer.write_text(json.dumps(wrong_nonce))
            (root / "player-result.json").write_text(json.dumps(wrong_nonce))
            with self.assertRaises(contract.WitnessError):
                contract.producer_witness(producer, entry, engine)
            producer.write_text(json.dumps(receipt))
            (root / "player-result.json").write_text(json.dumps(receipt))
            wrong_flags = copy.deepcopy(receipt)
            wrong_flags["checkpoints"]["14"]["state"]["player"][
                "primary_weapon_flags"
            ] = 65
            producer.write_text(json.dumps(wrong_flags))
            (root / "player-result.json").write_text(json.dumps(wrong_flags))
            with self.assertRaises(contract.WitnessError):
                contract.producer_witness(producer, entry, engine)
            producer.write_text(json.dumps(receipt))
            (root / "player-result.json").write_text(json.dumps(receipt))
            wrong_alternate = copy.deepcopy(alternate)
            wrong_alternate["checkpoints"]["14"]["sha256"] = "0" * 64
            (root / "alternate-result.json").write_text(json.dumps(wrong_alternate))
            with self.assertRaises(contract.WitnessError):
                contract.producer_witness(producer, entry, engine)

    def test_input_and_checkpoint_hashes(self):
        with tempfile.TemporaryDirectory() as directory:
            owner = Path(directory)
            engine = owner / "engine"
            engine.write_bytes(b"engine")
            (owner / "engine-path.txt").write_text(str(engine))
            root = owner / "gold"
            (root / "preview-user").mkdir(parents=True)
            entry = root / "preview-user" / "coldrom-gold-earned-entry.sav"
            saved = root / "preview-user" / "earned.sav"
            entry.write_bytes(b"entry")
            saved.write_bytes(b"earned")
            plan = {
                "receipt_run_id": "a" * 32,
                "resume_checkpoint": entry.name,
                "resume_sha256": contract.sha(entry),
                "steps": [{"action": "introspect"}],
            }
            (root / "gold-executed-plan.json").write_text(json.dumps(plan))
            receipt = {
                "result": "PASS",
                "plan_sha256": contract.sha(root / "gold-executed-plan.json"),
                "resume_sha256": contract.sha(entry),
                "executable_sha256": contract.sha(engine),
                "steps": 1,
                "run_id": "a" * 32,
                "checkpoint_history": [
                    {"step": 0, "file": saved.name, "sha256": contract.sha(saved)}
                ],
            }
            result = root / "gold-result.json"
            result.write_text(json.dumps(receipt))
            (root / "player-result.json").write_text(json.dumps(receipt))
            (root / "gold-state.json").write_text("{}\n")
            (root / "player-state.json").write_text("{}\n")
            (root / "gold-trace.jsonl").write_text("{}\n")
            (root / "player-trace.jsonl").write_text("{}\n")
            alias = root / "preview-user" / "pumo-player-level5.sav"
            alias.write_bytes(b"old generic alias")
            alias.write_bytes(b"new same-level quick-save")
            contract.phase_files(owner, "gold")
            alias_plan = {
                **plan,
                "resume_checkpoint": alias.name,
                "resume_sha256": contract.sha(alias),
            }
            (root / "gold-executed-plan.json").write_text(json.dumps(alias_plan))
            alias_receipt = {
                **receipt,
                "plan_sha256": contract.sha(root / "gold-executed-plan.json"),
                "resume_sha256": contract.sha(alias),
            }
            result.write_text(json.dumps(alias_receipt))
            (root / "player-result.json").write_text(json.dumps(alias_receipt))
            with self.assertRaises(contract.WitnessError):
                contract.phase_files(owner, "gold")
            (root / "gold-executed-plan.json").write_text(json.dumps(plan))
            result.write_text(json.dumps(receipt))
            (root / "player-result.json").write_text(json.dumps(receipt))
            (root / "player-trace.jsonl").write_text('{"stale":true}\n')
            with self.assertRaises(contract.WitnessError):
                contract.phase_files(owner, "gold")
            (root / "player-trace.jsonl").write_text("{}\n")
            (root / "player-result.json").write_text('{"stale":true}\n')
            with self.assertRaises(contract.WitnessError):
                contract.phase_files(owner, "gold")
            (root / "player-result.json").write_text(json.dumps(receipt))
            (root / "player-state.json").write_text('{"stale":true}\n')
            with self.assertRaises(contract.WitnessError):
                contract.phase_files(owner, "gold")
            (root / "player-state.json").write_text("{}\n")
            oversized = " " * (contract.MAX_TRACE_LINE_BYTES + 1) + "\n"
            (root / "gold-trace.jsonl").write_text(oversized)
            (root / "player-trace.jsonl").write_text(oversized)
            with self.assertRaises(contract.WitnessError):
                contract.phase_files(owner, "gold")
            (root / "gold-trace.jsonl").write_text("{}\n")
            (root / "player-trace.jsonl").write_text("{}\n")
            bad_plan = {**plan, "receipt_run_id": "b" * 32}
            (root / "gold-executed-plan.json").write_text(json.dumps(bad_plan))
            bad_receipt = {
                **receipt,
                "plan_sha256": contract.sha(root / "gold-executed-plan.json"),
            }
            result.write_text(json.dumps(bad_receipt))
            (root / "player-result.json").write_text(json.dumps(bad_receipt))
            with self.assertRaises(contract.WitnessError):
                contract.phase_files(owner, "gold")
            bad_plan = {**plan, "resume_checkpoint": "../coldrom-gold-earned-entry.sav"}
            (root / "gold-executed-plan.json").write_text(json.dumps(bad_plan))
            bad_receipt = {
                **receipt,
                "plan_sha256": contract.sha(root / "gold-executed-plan.json"),
            }
            result.write_text(json.dumps(bad_receipt))
            (root / "player-result.json").write_text(json.dumps(bad_receipt))
            with self.assertRaises(contract.WitnessError):
                contract.phase_files(owner, "gold")
            (root / "gold-executed-plan.json").write_text(json.dumps(plan))
            bad = copy.deepcopy(receipt)
            bad["resume_sha256"] = "0" * 64
            result.write_text(json.dumps(bad))
            (root / "player-result.json").write_text(json.dumps(bad))
            with self.assertRaises(contract.WitnessError):
                contract.phase_files(owner, "gold")
            result.write_text(json.dumps(receipt))
            (root / "player-result.json").write_text(json.dumps(receipt))
            saved.write_bytes(b"wrong checkpoint")
            with self.assertRaises(contract.WitnessError):
                contract.phase_files(owner, "gold")


if __name__ == "__main__":
    unittest.main()
