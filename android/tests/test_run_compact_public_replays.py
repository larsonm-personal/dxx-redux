"""Fail-closed gameplay and report gates for the compact public runtime runner."""

import argparse
import copy
import contextlib
import io
import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import run_compact_public_replays as runner
import check_compact_public_replay_failures as negative
import ordered_public_replay as ordered


class EarlyAbortTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    @staticmethod
    def schema_value(schema):
        if schema is int:
            return 0
        if isinstance(schema, dict):
            return {key: EarlyAbortTests.schema_value(value) for key, value in schema.items() if key != "*"}
        if isinstance(schema, tuple):
            return [EarlyAbortTests.schema_value(value) for value in schema]
        return []

    def world(self):
        parity = runner.parity
        state = self.schema_value(parity.WORLD_SCHEMA)
        state["exploding_walls"]["capacity"] = 10
        ai = state["ai"]
        ai.update(local_capacity=1000, Believed_player_seg=-1, Ai_last_missile_camera=-1)
        ai["path_runtime"]["last_buddy_polish_path_tick"] = 0
        ai["boss"].update(Boss_hit_time=0, d1_hit_pending=0, d1_been_hit=0, teleport_segments=[0] * 100)
        for key, count in (("paths", 2500), ("cloak", 8), ("awareness", 64)):
            ai[key] = [self.schema_value(parity.WORLD_SCHEMA["ai"][key][0]) for _ in range(count)]
        for row in ai["cloak"]:
            row["last_segment"] = -1
        local = ai["local_default"]
        local.update(
            next_action_time=0,
            next_fire2=0,
            d1_saved=dict.fromkeys("last_see_time last_attack_time wait_time".split(), 0),
        )
        for key in ("goal_angles", "delta_angles", "goal_state", "achieved_state"):
            local[key] = [self.schema_value(parity.AI_LOCAL_SCHEMA[key][0]) for _ in range(10)]
        player = self.schema_value(parity.WORLD_SCHEMA["players"][0])
        player.update(shields=100 * 65536, energy=100 * 65536, primary_ammo=[0] * 10, secondary_ammo=[5] + [0] * 9)
        state["players"] = [player]
        return state

    def fixture(self, phase="post-input"):
        frame = 2 if phase == "post-input" else 1
        scenario = "secondary-mouse-first" if phase == "post-input" else "flight"
        case = {
            "game": "d2",
            "scenario": scenario,
            "demo": "test.dximdemo",
            "oracle": "test.oracle.json",
            "actions": {"first_fire_frame": 2, "initial_ammo": 5},
        }
        header = {
            "game": "d2",
            "mission": "d2demo",
            "difficulty": 2,
            "frame_count": 3,
            "level": 1,
            "start_mode": "new_level",
        }
        seeds = [100]
        for _ in range(3):
            seeds.append((seeds[-1] * 0x41C64E6D + 0x3039) & 0xFFFFFFFF)
        batches = [
            {"pre_rng": {"s": seeds[i], "c": i}, "post_rng": {"s": seeds[i], "c": i}, "frame_time": 1} for i in range(3)
        ]
        if phase == "post-input":
            batches[frame]["post_rng"] = {"s": seeds[frame + 1], "c": frame + 1}
        else:
            batches[frame]["pre_rng"] = {"s": seeds[frame + 1], "c": frame + 1}
        world = self.world()
        ship = self.schema_value(runner.parity.OBJECT_SCHEMA)
        ship.update(type=4, signature=1, movement_type=1, physics=self.schema_value(runner.parity.PHYSICS_SCHEMA))

        def boundary(kind, at, phase_name):
            row = {
                "type": kind,
                "version": 2 if kind == "object_boundary" else 10,
                "f": at,
                "phase": phase_name,
                "reset": True,
            }
            if kind == "object_boundary":
                row.update(self.schema_value(runner.parity.OBJECT_STORAGE_SCHEMA))
                row.update(
                    capacity=1000,
                    slots={"0": copy.deepcopy(ship)},
                    rng=[{"available": 1, "state": seeds[at], "calls": at}, {"available": 1, "state": 99, "calls": 0}],
                )
                row["allocator"].update(num_objects=1, free_obj_list=list(range(1000)))
                row["clock"].update(game_time=at, frame_time=1 if at else 0)
            else:
                row["state"] = copy.deepcopy(world)
                row["state"]["globals"].update(GameTime64=at, FrameTime=1 if at else 0)
            return row

        rows = [
            {
                "type": "meta",
                "version": 1,
                "source": "replay",
                "diag_version": runner.parity.FRAME_DIAGNOSTIC_VERSION,
                **header,
            }
        ]
        rows += [boundary(kind, 0, "restored") for kind in ("object_boundary", "world_boundary")]
        for i in range(frame):
            for kind in ("object", "world"):
                snapshot = boundary(kind + "_boundary", i + 1, "unused")
                snapshot.update(type=kind + "_state", f=i, reset=i == 0)
                del snapshot["phase"]
                rows.append(snapshot)
            state = self.schema_value(negative.PARTIAL_FRAME_SCHEMA)
            state["game_time64"] = i + 1
            state["player0"].update(energy=100, shields=100, secondary_ammo=[5] + [0] * 15)
            state["level_summary"].update(control_center_destroyed=False, endlevel_completed=False)
            diag = {key: [0] * length if length else 0 for key, length in runner.parity.FRAME_DIAGNOSTIC_FIELDS.items()}
            rows.append(
                {
                    "type": "frame_state",
                    "source": "replay",
                    "f": i,
                    "ft": 1,
                    "rng": batches[i]["post_rng"],
                    "diag": diag,
                    "state": state,
                }
            )
        rows += [boundary(kind, frame, "aborted") for kind in ("object_boundary", "world_boundary")]
        events = [{"type": "meta", "version": 1, "events": frame, "truncated": False}]
        for i in range(frame):
            events.append(
                {
                    "type": "rand",
                    "seq": i,
                    "frame": i,
                    "gt": i + 1,
                    "call_count": i + 1,
                    "state_before": seeds[i],
                    "state_after": seeds[i + 1],
                    "result": (seeds[i + 1] >> 16) & 0x7FFF,
                }
            )
        directory = self.root / phase
        directory.mkdir(exist_ok=True)
        state, rng, result = (directory / name for name in ("state.jsonl", "rng.jsonl", "result.json"))
        failure = directory / "result.json.failure_test"
        failure.mkdir(exist_ok=True)
        runner.parity.write_json(failure / "failure.json", {"forced_stop": False, "native_exit_code": 1})
        reason = (
            f"ordered input SIM RNG differs at frame {frame}"
            if phase == "post-input"
            else f"carried SIM RNG differs before input at frame {frame}"
        )
        (directory / "runner.log").write_text(f"Input demo replay stopped: {reason}\n")
        self.write_rows(state, rows)
        self.write_rows(rng, events)
        row = {
            "exit_code": 1,
            "command": [],
            "artifacts": {"state": str(state), "rng": str(rng), "result": str(result)},
        }
        return case, header, batches, rows, events, row

    @staticmethod
    def write_rows(path, rows):
        path.write_text("".join(json.dumps(row) + "\n" for row in rows))

    def verify(self, fixture, phase="post-input"):
        case, header, batches, _, _, row = fixture
        with mock.patch.object(negative, "verify_declared_mutation", return_value=(header, {}, [], batches)):
            return negative.verify_early_abort(
                row, None, case, "bad-input", negative.early_abort_spec(case, "bad-input")
            )

    def test_exact_carry_and_post_input_proofs_pass(self):
        for phase in ("carry", "post-input"):
            with self.subTest(phase=phase):
                result = self.verify(self.fixture(phase), phase)
                self.assertEqual(result["phase"], phase)
                self.assertEqual(result["rng"]["count"], 1 if phase == "carry" else 2)

    def test_windowed_native_diagnostic_archive(self):
        fixture = self.fixture()
        directory = Path(fixture[-1]["artifacts"]["result"]).parent
        console = directory / "runner.log"
        native = directory / "result.json.failure_test/gamelog.txt"
        reason = console.read_text()
        native.write_text("12:00:00 " + reason)
        self.verify(fixture)
        console.write_text("Windowed native process exited with code 1\n")
        self.verify(fixture)
        for diagnostic in (reason + reason, "Input demo replay stopped: invalid input\n"):
            with self.subTest(diagnostic=diagnostic):
                native.write_text(diagnostic)
                with self.assertRaisesRegex(runner.parity.EvidenceError, "native diagnostic"):
                    self.verify(fixture)

    def test_wrong_boundaries_process_outcomes_and_effects_reject(self):
        for mutation in (
            "frame",
            "missing-boundary",
            "missing-frame",
            "extra-frame",
            "terminal",
            "reason",
            "exit-zero",
            "native-zero",
            "force",
            "result",
            "archived-result",
            "ammo",
        ):
            with self.subTest(mutation=mutation):
                fixture = self.fixture()
                *_, rows, events, run = fixture
                directory = Path(run["artifacts"]["result"]).parent
                if mutation == "frame":
                    rows[-1]["f"] = 3
                elif mutation == "missing-boundary":
                    rows.pop(-2)
                elif mutation == "missing-frame":
                    rows[:] = [r for r in rows if not (r["type"] == "frame_state" and r["f"] == 0)]
                elif mutation == "extra-frame":
                    rows.insert(-2, next(r for r in rows if r["type"] == "frame_state"))
                elif mutation == "terminal":
                    rows[-1]["phase"] = "terminal"
                elif mutation == "reason":
                    (directory / "runner.log").write_text("Input demo replay stopped: invalid input\n")
                elif mutation == "exit-zero":
                    run["exit_code"] = 0
                elif mutation in ("native-zero", "force"):
                    runner.parity.write_json(
                        directory / "result.json.failure_test/failure.json",
                        {"forced_stop": mutation == "force", "native_exit_code": 0 if mutation == "native-zero" else 1},
                    )
                elif mutation == "result":
                    Path(run["artifacts"]["result"]).write_text("{}")
                elif mutation == "archived-result":
                    (directory / "result.json.failure_test/result.actual.json").write_text("{}")
                else:
                    rows[-1]["state"]["players"][0]["secondary_ammo"][0] = 4
                self.write_rows(Path(run["artifacts"]["state"]), rows)
                with self.assertRaises(runner.parity.EvidenceError):
                    self.verify(fixture)
                # Each subcase starts without any fabricated terminal file
                Path(run["artifacts"]["result"]).unlink(missing_ok=True)
                (directory / "result.json.failure_test/result.actual.json").unlink(missing_ok=True)
        fixture = self.fixture("carry")
        fixture[3][-2]["slots"]["0"]["physics"]["thrust"][2] = 1
        self.write_rows(Path(fixture[-1]["artifacts"]["state"]), fixture[3])
        with self.assertRaisesRegex(runner.parity.EvidenceError, "player thrust"):
            self.verify(fixture, "carry")

    def test_stripped_payloads_sources_and_wrong_clocks_reject(self):
        for mutation in (
            "object-payload",
            "world-payload",
            "frame-payload",
            "object-count",
            "object-delete",
            "meta-source",
            "frame-source",
            "clock",
            "position",
            "inventory",
            "summary",
        ):
            with self.subTest(mutation=mutation):
                fixture = self.fixture()
                rows, run = fixture[3], fixture[-1]
                if mutation.endswith("payload"):
                    kind = mutation.split("-")[0] + "_state"
                    row = next(row for row in rows if row["type"] == kind)
                    if kind == "object_state":
                        row.pop("slots")
                    else:
                        row["state"] = {}
                elif mutation == "object-count":
                    next(row for row in rows if row["type"] == "object_state")["allocator"]["num_objects"] = 2
                elif mutation == "object-delete":
                    [row for row in rows if row["type"] == "object_state"][1]["slots"]["999"] = None
                elif mutation == "meta-source":
                    rows[0]["source"] = "record"
                elif mutation == "frame-source":
                    next(row for row in rows if row["type"] == "frame_state")["source"] = "record"
                elif mutation == "clock":
                    rows[-2]["clock"]["game_time"] += 1
                else:
                    state = next(row for row in rows if row["type"] == "frame_state")["state"]
                    if mutation == "position":
                        state["position"]["x"] = 1
                    elif mutation == "inventory":
                        state["player0"]["secondary_ammo"][0] = 4
                    else:
                        del state["level_summary"]["endlevel_completed"]
                self.write_rows(Path(run["artifacts"]["state"]), rows)
                with self.assertRaises(runner.parity.EvidenceError):
                    self.verify(fixture)

    def test_missing_reordered_forged_rng_or_wrong_endpoint_reject(self):
        for mutation in (
            "missing",
            "reordered",
            "duplicate-seq",
            "bool-seq",
            "truncated",
            "before",
            "after",
            "count",
            "result",
            "srand-count",
            "context",
            "time",
            "endpoint",
            "extra-batch",
        ):
            with self.subTest(mutation=mutation):
                fixture = self.fixture()
                _, _, _, rows, events, run = fixture
                if mutation == "missing":
                    events.pop()
                elif mutation == "reordered":
                    events[1], events[2] = events[2], events[1]
                elif mutation == "duplicate-seq":
                    events[2]["seq"] = 0
                elif mutation == "bool-seq":
                    events[2]["seq"] = True
                elif mutation == "truncated":
                    events[0]["truncated"] = True
                elif mutation in ("before", "after"):
                    events[1]["state_" + mutation] += 1
                elif mutation == "count":
                    events[1]["call_count"] += 1
                elif mutation == "result":
                    events[1]["result"] += 1
                elif mutation == "srand-count":
                    events[1].update(type="srand", seed=events[1]["state_after"])
                elif mutation == "context":
                    events[1]["frame"] = 1
                elif mutation == "time":
                    events[1]["gt"] = 100
                elif mutation == "endpoint":
                    rows[-2]["rng"][0]["calls"] += 1
                else:
                    last = events[-1]
                    state = (last["state_after"] * 0x41C64E6D + 0x3039) & 0xFFFFFFFF
                    events.append(
                        {
                            **last,
                            "seq": 2,
                            "frame": 2,
                            "gt": 2,
                            "call_count": 3,
                            "state_before": last["state_after"],
                            "state_after": state,
                            "result": (state >> 16) & 0x7FFF,
                        }
                    )
                    events[0]["events"] += 1
                self.write_rows(Path(run["artifacts"]["state"]), rows)
                self.write_rows(Path(run["artifacts"]["rng"]), events)
                with self.assertRaises(runner.parity.EvidenceError):
                    self.verify(fixture)

    def test_srand_preserves_call_count_and_rand_resumes_from_seed(self):
        fixture = self.fixture("carry")
        _, _, batches, rows, events, run = fixture
        seed = 777
        state = (seed * 0x41C64E6D + 0x3039) & 0xFFFFFFFF
        events = [
            {"type": "meta", "version": 1, "events": 2, "truncated": False},
            {
                "type": "srand",
                "seq": 0,
                "frame": 0,
                "gt": 1,
                "call_count": 0,
                "state_before": 100,
                "state_after": seed,
                "seed": seed,
            },
            {
                "type": "rand",
                "seq": 1,
                "frame": 0,
                "gt": 1,
                "call_count": 1,
                "state_before": seed,
                "state_after": state,
                "result": (state >> 16) & 0x7FFF,
            },
        ]
        rows[-2]["rng"][0].update(state=state, calls=1)
        self.write_rows(Path(run["artifacts"]["state"]), rows)
        self.write_rows(Path(run["artifacts"]["rng"]), events)
        self.assertEqual(self.verify(fixture, "carry")["rng"]["state"], state)

    def test_no_collect_requires_original_drop_prefix_and_live_uncollected_identity(self):
        case = {
            "game": "d1",
            "scenario": "loot",
            "actions": {"loot_frame": 1, "target_score": 500, "target": 39, "pickup": 79, "pickup_signature": 301},
        }
        frames = [{"f": i, "ft": 1, "state": {"score": 500}, "rng": {}, "diag": {}} for i in range(3)]
        expected = {"frame_sha256": [runner.compact.sha(row) for row in frames]}
        world = {"players": [{"objnum": 0, "shields": 1, "score": 500, "laser_level": 0}], "death": {"active": 0}}
        ship = {"type": 4, "signature": 1}
        pickup = {"type": 7, "id": 3, "signature": 301, "flags": 0, "lifeleft": 100}
        boundaries = {
            ("object_boundary", "restored"): {"slots": {"0": ship}},
            ("object_boundary", "aborted"): {"slots": {"0": ship, "79": pickup}},
            ("world_boundary", "restored"): {"state": world},
            ("world_boundary", "aborted"): {"state": world},
        }
        label = negative.verify_partial_effect(case, "no-collect", frames, boundaries, expected)
        self.assertEqual(label, "pre-collection divergence; no completed non-collection claim")
        for mutation in (
            "prefix",
            "short",
            "signature",
            "type",
            "id",
            "consumed",
            "expired",
            "score",
            "gain",
            "carrier",
        ):
            with self.subTest(mutation=mutation):
                changed, prefix = copy.deepcopy(boundaries), copy.deepcopy(frames)
                objects = changed["object_boundary", "aborted"]["slots"]
                player = changed["world_boundary", "aborted"]["state"]["players"][0]
                if mutation == "prefix":
                    prefix[0]["state"]["score"] = 0
                elif mutation == "short":
                    prefix.pop()
                elif mutation in ("signature", "type", "id"):
                    objects["79"][mutation] += 1
                elif mutation == "consumed":
                    objects["79"]["flags"] = 2
                elif mutation == "expired":
                    objects["79"]["lifeleft"] = 0
                elif mutation == "score":
                    player["score"] = 0
                elif mutation == "gain":
                    player["laser_level"] = 1
                else:
                    objects["39"] = {"type": 2}
                with self.assertRaises(runner.parity.EvidenceError):
                    negative.verify_partial_effect(case, "no-collect", prefix, changed, expected)

    def test_repeat_rng_evidence_must_match(self):
        case, _, _, _, _, row = self.fixture("carry")
        row.update(game="d2", scenario="flight", status="fail", command=["-Runner", "headless-console"])
        report = {
            "status": "fail",
            "backend": "headless",
            "native_only": True,
            "paired": {"status": "not_run"},
            "native_runs": [{**row, "repeat": i} for i in range(2)],
        }
        args = argparse.Namespace(run_mode="headless", corpus=self.root)
        with mock.patch.object(negative, "verify_early_abort", return_value={"rng": {"count": 1}}):
            self.assertEqual(len(negative.verify_failure(report, 1, args, [case], "bad-input")), 2)
        with mock.patch.object(
            negative, "verify_early_abort", side_effect=[{"rng": {"count": 1}}, {"rng": {"count": 2}}]
        ):
            with self.assertRaisesRegex(runner.parity.EvidenceError, "Repeated partial"):
                negative.verify_failure(report, 1, args, [case], "bad-input")


class RuntimeGateTests(unittest.TestCase):
    def test_phase_timings_include_failures_without_suppressing_them(self):
        timings = {}
        with mock.patch.object(runner.time, "perf_counter", side_effect=(10.0, 12.5, 20.0, 21.25)):
            with runner.measure_phase(timings, "runner"):
                pass
            with self.assertRaisesRegex(runner.parity.EvidenceError, "wrong oracle"):
                with runner.measure_phase(timings, "fidelity"):
                    raise runner.parity.EvidenceError("wrong oracle")
        self.assertEqual(timings, {"runner": 2.5, "fidelity": 1.25})

    @staticmethod
    def imported_scope(scenario):
        return {
            "role": "required-gameplay" if scenario == "flight" else "diagnostic-only",
            "reason": "Explicit test scope",
        }

    @unittest.skipUnless(os.name == "posix", "POSIX retention uses advisory file locks")
    def test_active_capture_holds_retention_lease(self):
        import fcntl

        args = argparse.Namespace(
            corpus=self.root,
            data_d1=self.root,
            data_d2=self.root,
            exe_d1=self.demo,
            exe_d2=self.demo,
            output=self.root / "active-capture",
        )

        def inspect_lease(args, cases):
            with (args.output / "producer.lock").open("r") as competing:
                with self.assertRaises(BlockingIOError):
                    fcntl.flock(competing.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            return 0

        with (
            mock.patch.object(runner, "load_corpus", return_value=[]),
            mock.patch.object(runner, "validate_assets"),
            mock.patch.object(runner, "_run_cases", side_effect=inspect_lease),
        ):
            self.assertEqual(runner.run(args), 0)

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.demo = self.root / "demo.dximdemo"
        self.actual = self.root / "actual.json"
        self.count = 12
        self.header = {
            "type": "header",
            "game": "d1",
            "mission": "d1",
            "level": 1,
            "difficulty": 2,
            "start_mode": "new_level",
            "frame_count": self.count,
        }
        self.controls = [
            {"type": "frame", "f": i, "input": {"s": {"f": -1092} if i == 0 else {}}} for i in range(self.count)
        ]
        self.write_demo()

        def state(index):
            return {
                "position": {"x": 0 if index == 0 else 131072, "y": 0, "z": 0},
                "player0": {"energy": 100 if index == 0 else 99, "shields": 100},
            }

        self.frames = [{"state": state(i), "diag": {"player_weapon_count": 1}} for i in range(self.count)]
        self.actual.write_text(json.dumps(state(self.count)))

        def world(kind, index, phase=None):
            row = {
                "type": kind,
                "f": index,
                "state": {"death": {"active": 0}, "walls": [{"state": 0 if index < 3 else 1}]},
            }
            if phase:
                row["phase"] = phase
            return row

        self.worlds = (
            [world("world_boundary", 0, "restored")]
            + [world("world_state", i) for i in range(self.count)]
            + [world("world_boundary", self.count, "terminal")]
        )
        self.pickup_target = {"target": 4, "target_id": 1, "target_signature": 5}
        self.objects = [
            {"objects": {"4": {"type": 7, "id": 1, "signature": 5}} if i <= 3 else {}} for i in range(self.count)
        ]

    def write_demo(self):
        self.demo.write_text("\n".join(json.dumps(row) for row in [self.header] + self.controls) + "\n")

    def action(self, scenario, actions, frames=None, worlds=None):
        with (
            mock.patch.object(
                runner.parity, "frames", return_value=iter(copy.deepcopy(self.frames if frames is None else frames))
            ),
            mock.patch.object(
                runner.parity,
                "world_states",
                return_value=iter(copy.deepcopy(self.worlds if worlds is None else worlds)),
            ),
            mock.patch.object(runner.parity, "object_states", return_value=iter(copy.deepcopy(self.objects))),
        ):
            return runner.verify_actions(
                {"game": "d1", "scenario": scenario, "actions": actions}, self.demo, self.demo, self.actual
            )

    def test_backward_thrust_and_delta_controls_count_real_stopping(self):
        self.assertEqual(self.action("flight", {"wall_contact_frames": 11})["wall_contact_frames"], 11)
        self.controls[1]["input"]["s"]["f"] = 0
        self.write_demo()
        with self.assertRaises(runner.parity.EvidenceError):
            self.action("flight", {"wall_contact_frames": 11})

    def test_dead_or_stationary_or_unarmed_motion_cannot_pass(self):
        for mutation in ("dead", "stationary", "weapons", "energy", "claimed_contacts", "missing_world"):
            frames, worlds = copy.deepcopy(self.frames), copy.deepcopy(self.worlds)
            actions = {"wall_contact_frames": 11}
            if mutation == "dead":
                for row in worlds:
                    row["state"]["death"]["active"] = 1
            elif mutation == "stationary":
                for row in frames:
                    row["state"]["position"]["x"] = 131072
            elif mutation == "weapons":
                for row in frames:
                    row["diag"]["player_weapon_count"] = 0
            elif mutation == "energy":
                for row in frames:
                    row["state"]["player0"]["energy"] = 99
            elif mutation == "claimed_contacts":
                actions["wall_contact_frames"] = 0
            else:
                worlds.pop()
            with self.subTest(mutation=mutation), self.assertRaises(runner.parity.EvidenceError):
                self.action("flight", actions, frames, worlds)

    def test_combat_proves_causal_outcome_before_generic_firing_guard(self):
        frames = copy.deepcopy(self.frames)
        for row in frames:
            row["diag"]["player_weapon_count"] = 0
        with mock.patch.object(
            runner.robot_combat, "verify", side_effect=runner.parity.EvidenceError("identified robot remained alive")
        ) as verify:
            with self.assertRaisesRegex(runner.parity.EvidenceError, "identified robot remained alive"):
                self.action("combat", {}, frames)
            verify.assert_called_once()
        with mock.patch.object(runner.robot_combat, "verify", return_value={"score_awarded": 200}) as verify:
            with self.assertRaisesRegex(runner.parity.EvidenceError, "live player weapons"):
                self.action("combat", {}, frames)
            verify.assert_called_once()
        with mock.patch.object(runner.robot_combat, "verify", return_value={"score_awarded": 200}):
            self.assertEqual(self.action("combat", {})["score_awarded"], 200)

    def test_loot_proves_collection_before_generic_firing_guard(self):
        frames = copy.deepcopy(self.frames)
        for row in frames:
            row["diag"]["player_weapon_count"] = 0
        with mock.patch.object(
            runner.robot_loot, "verify", side_effect=runner.parity.EvidenceError("identified drop remained uncollected")
        ) as verify:
            with self.assertRaisesRegex(runner.parity.EvidenceError, "identified drop remained uncollected"):
                self.action("loot", {}, frames)
            verify.assert_called_once()
        with mock.patch.object(runner.robot_loot, "verify", return_value={"laser_level_increase": 1}) as verify:
            with self.assertRaisesRegex(runner.parity.EvidenceError, "live player weapons"):
                self.action("loot", {}, frames)
            verify.assert_called_once()
        with mock.patch.object(runner.robot_loot, "verify", return_value={"laser_level_increase": 1}):
            self.assertEqual(self.action("loot", {})["laser_level_increase"], 1)

    def test_pickup_requires_consecutive_gain_at_exact_next_entry(self):
        frames = copy.deepcopy(self.frames)
        for row in frames[3:]:
            row["state"]["player0"]["shields"] = 110
        self.assertIn(
            2,
            self.action("pickup", {**self.pickup_target, "action_frame": 3, "pickup_gain_frame": 2}, frames)[
                "pickup_gain_frames"
            ],
        )
        for actions, rows in (
            ({"action_frame": 3, "pickup_gain_frame": 3}, frames),
            ({"action_frame": 1, "pickup_gain_frame": 2}, frames),
            ({"action_frame": 3}, frames),
            ({"action_frame": 3, "pickup_gain_frame": -1}, frames),
            ({"action_frame": 3, "pickup_gain_frame": True}, frames),
            ({"action_frame": 3, "pickup_gain_frame": 12}, frames),
            ({"action_frame": 3, "pickup_gain_frame": 2}, self.frames),
        ):
            with self.assertRaises(runner.parity.EvidenceError):
                self.action("pickup", actions, rows)

    def test_pickup_identity_and_retirement_are_exact(self):
        for mutation in ("slot", "id", "signature", "early", "late", "not_original"):
            actions = {**self.pickup_target, "action_frame": 3}
            objects = copy.deepcopy(self.objects)
            if mutation == "slot":
                actions["target"] += 1
            elif mutation == "id":
                actions["target_id"] += 1
            elif mutation == "signature":
                actions["target_signature"] += 1
            elif mutation == "early":
                objects[3]["objects"] = {}
            elif mutation == "late":
                objects[4]["objects"] = objects[3]["objects"]
            else:
                objects[0]["objects"] = {}
            with (
                self.subTest(mutation=mutation),
                mock.patch.object(runner.parity, "object_states", return_value=iter(objects)),
                self.assertRaises(runner.parity.EvidenceError),
            ):
                runner.verify_pickup_target(actions, self.demo, self.header, "d1")

    def test_door_requires_initially_closed_target_and_exact_next_entry(self):
        self.assertEqual(self.action("door", {"target": 0, "action_frame": 2})["door_open_entry_frames"][0], 3)
        for actions in ({"target": 0, "action_frame": 1}, {"target": 99}, {"target": -1}):
            with self.assertRaises(runner.parity.EvidenceError):
                self.action("door", actions)
        worlds = copy.deepcopy(self.worlds)
        worlds[0]["state"]["walls"][0]["state"] = 2
        with self.assertRaises(runner.parity.EvidenceError):
            self.action("door", {"target": 0}, worlds=worlds)

    def paired(self):
        cases, rows = [], []
        for scenario in runner.SCENARIOS:
            name = f"d1-{scenario}.dximdemo"
            (self.root / name).write_text(json.dumps(self.header) + "\n")
            cases.append(
                {
                    "game": "d1",
                    "scenario": scenario,
                    "demo": name,
                    "imported_d1": self.imported_scope(scenario),
                }
            )
            required = (
                "terminal_result",
                "checkpoint_collision_clock",
                "frames",
                "simulation_rng",
                "object_states",
                "world_states",
            )
            checks = {key: {"status": "pass"} for key in required}
            checks["frames"]["frames_compared"] = self.count
            checks["simulation_rng"]["simulation_events_compared"] = 1
            recorded = {
                key: {"status": "incomplete" if key in ("frames", "simulation_rng") else "pass"} for key in required[:4]
            }
            rows.append(
                {
                    "demo": name,
                    "recording_to_native": {"status": "incomplete", "checks": recorded},
                    "native_repeatability": {
                        "status": "pass",
                        "checks": copy.deepcopy(checks),
                    },
                    "native_to_imported": {
                        "status": "pass",
                        "checks": copy.deepcopy(checks),
                    },
                }
            )
        return cases, {"schema": 1, "qualification": "incomplete", "cases": rows}

    def test_only_known_compact_recording_incompleteness_is_accepted(self):
        cases, report = self.paired()
        runner.check_paired_report(report, 2, cases, self.root)
        for mutation in (
            "world",
            "rng",
            "frames",
            "missing",
            "assets",
            "harness",
            "capture",
            "recording_terminal",
            "exit",
        ):
            changed = copy.deepcopy(report)
            case = changed["cases"][0]
            if mutation == "world":
                case["native_to_imported"]["checks"]["world_states"]["status"] = "fail"
            elif mutation == "rng":
                case["native_repeatability"]["checks"]["simulation_rng"]["simulation_events_compared"] = 0
            elif mutation == "frames":
                case["native_repeatability"]["checks"]["frames"]["frames_compared"] -= 1
            elif mutation == "missing":
                changed["cases"].pop()
            elif mutation in ("assets", "harness"):
                changed["asset_error" if mutation == "assets" else "harness_error"] = "changed"
            elif mutation == "capture":
                case["capture_errors"] = {"imported": "failed"}
            elif mutation == "recording_terminal":
                case["recording_to_native"]["checks"]["terminal_result"]["status"] = "fail"
            with self.subTest(mutation=mutation), self.assertRaises(runner.parity.EvidenceError):
                runner.check_paired_report(changed, 1 if mutation == "exit" else 2, cases, self.root)

    def test_imported_scope_is_explicit_and_cannot_remove_required_flight(self):
        cases, report = self.paired()
        future = {"game": "d1", "scenario": "future", "demo": "future.dximdemo"}
        for mutation in (
            "missing",
            "unknown_role",
            "blank_reason",
            "excluded_flight",
            "duplicate_flight",
        ):
            changed = copy.deepcopy(cases)
            if mutation == "missing":
                changed.append(future)
            elif mutation == "unknown_role":
                changed[1]["imported_d1"]["role"] = "optional"
            elif mutation == "blank_reason":
                changed[1]["imported_d1"]["reason"] = " "
            elif mutation == "excluded_flight":
                changed[0]["imported_d1"]["role"] = "excluded"
            else:
                changed.append(changed[0])
            with (
                self.subTest(mutation=mutation),
                self.assertRaises(runner.parity.EvidenceError),
            ):
                runner.imported_d1_scope(changed)
        excluded = {
            **future,
            "imported_d1": {
                "role": "excluded",
                "reason": "Retained unsupported import evidence",
            },
        }
        included, omissions = runner.imported_d1_scope(cases + [excluded])
        self.assertEqual(included, cases)
        self.assertEqual(
            omissions,
            [{"demo": future["demo"], "reason": excluded["imported_d1"]["reason"]}],
        )
        scoped = runner.check_paired_report(report, 2, cases + [excluded], self.root)
        self.assertEqual(scoped["excluded_imported_cases"], omissions)
        changed = copy.deepcopy(report)
        changed["cases"].append({"demo": excluded["demo"]})
        with self.assertRaises(runner.parity.EvidenceError):
            runner.check_paired_report(changed, 2, cases + [excluded], self.root)

    def test_imported_staging_uses_declarations_and_retains_exclusion_on_failure(self):
        cases, _ = self.paired()
        excluded = {
            "game": "d1",
            "scenario": "door-lifecycle",
            "demo": "level-three.dximdemo",
            "imported_d1": {
                "role": "excluded",
                "reason": "Unsupported D2-only wall state",
            },
        }
        # A future case is included when explicitly declared, never silently filtered by its scenario or level
        future = {
            "game": "d1",
            "scenario": "future",
            "demo": "future.dximdemo",
            "imported_d1": self.imported_scope("future"),
        }
        (self.root / future["demo"]).write_text(json.dumps({**self.header, "level": 3}) + "\n")
        output = self.root / "imported"
        output.mkdir()
        args = argparse.Namespace(
            repo=self.root,
            corpus=self.root,
            output=output,
            data_d1=self.root,
            exe_d1=self.demo,
            exe_d2=self.demo,
            pwsh="unused",
            timeout=120,
            minimum_free_gb=0.01,
        )
        report = {}
        with mock.patch.object(runner, "_run", return_value=1):
            runner._run_imported(args, cases + [excluded, future], report)
        self.assertEqual(
            {p.name for p in (output / "d1-corpus").iterdir()},
            {c["demo"] for c in cases + [future]},
        )
        self.assertEqual(report["paired"]["status"], "fail")
        self.assertEqual(
            report["paired"]["excluded_imported_cases"][0]["reason"],
            excluded["imported_d1"]["reason"],
        )

    def test_both_backends_attempt_complete_inventory_and_preserve_failure(self):
        corpus = self.root / "corpus"
        corpus.mkdir()
        cases = []
        for game, scenario in runner.PUBLIC_CASES:
            demo = f"{game}-{scenario}.dximdemo"
            (corpus / demo).write_text("fixture")
            cases.append(
                {
                    "game": game,
                    "scenario": scenario,
                    "demo": demo,
                    "oracle": demo + ".oracle.json",
                    "actions": {},
                    "imported_d1": self.imported_scope(scenario),
                }
            )
        for backend in ("headless", "graphics"):
            output = self.root / backend
            output.mkdir()
            args = argparse.Namespace(
                repo=self.root,
                corpus=corpus,
                output=output,
                data_d1=self.root,
                data_d2=self.root,
                exe_d1=self.demo,
                exe_d2=self.demo,
                run_mode=backend,
                native_only=False,
                pwsh="unused",
                timeout=120,
                minimum_free_gb=0.01,
            )
            with (
                mock.patch.object(runner, "_run", return_value=1) as execute,
                contextlib.redirect_stdout(io.StringIO()),
            ):
                self.assertEqual(runner._run_cases(args, cases), 1)
            self.assertEqual(execute.call_count, 2 * len(cases) + (backend == "graphics"))
            report = json.loads((output / "report.json").read_text())
            self.assertEqual(len(report["native_runs"]), 2 * len(cases))
            self.assertTrue(all(row["status"] == "fail" for row in report["native_runs"]))
            self.assertEqual(
                report["paired"]["status"],
                "fail" if backend == "graphics" else "not_run",
            )
            for row in report["native_runs"]:
                self.assertEqual(row["backend"], backend)
                self.assertEqual(
                    row["command"][row["command"].index("-Runner") + 1],
                    "headless-console" if backend == "headless" else "visual",
                )
                self.assertIn("-StrictComparison", row["command"])
                self.assertIn("-RequireCleanExit", row["command"])
                if backend == "headless":
                    self.assertEqual(row["command"][row["command"].index("-HeadlessConsoleOutput") + 1], "2")
                suffix = row["command"][row["command"].index("-SandboxSuffix") + 1]
                self.assertTrue(suffix.startswith(backend + "-"))
                self.assertNotIn(str(output), suffix)

    def test_same_output_basename_under_distinct_parents_has_distinct_sandbox(self):
        suffixes = []
        for parent in ("first", "second"):
            output = self.root / parent / "wrong-oracle-run"
            output.mkdir(parents=True)
            args = argparse.Namespace(
                repo=self.root,
                corpus=self.root,
                output=output,
                data_d1=self.root,
                data_d2=self.root,
                exe_d1=self.demo,
                exe_d2=self.demo,
                run_mode="headless",
                native_only=True,
                pwsh="unused",
                timeout=120,
                minimum_free_gb=0.01,
            )
            case = {"game": "d1", "scenario": "flight", "demo": self.demo.name, "oracle": "unused.json"}
            with mock.patch.object(runner, "_run", return_value=1), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(runner._run_cases(args, [case]), 1)
            command = json.loads((output / "report.json").read_text())["native_runs"][0]["command"]
            suffixes.append(command[command.index("-SandboxSuffix") + 1])
        self.assertNotEqual(suffixes[0], suffixes[1])

    def test_explicit_native_only_graphics_reports_imported_parity_not_run(self):
        output = self.root / "native-only-graphics"
        output.mkdir()
        args = argparse.Namespace(
            repo=self.root,
            corpus=self.root,
            output=output,
            data_d1=self.root,
            data_d2=self.root,
            exe_d1=self.demo,
            exe_d2=self.demo,
            run_mode="graphics",
            native_only=True,
            pwsh="unused",
            timeout=120,
            minimum_free_gb=0.01,
        )
        case = {"game": "d1", "scenario": "flight", "demo": self.demo.name, "oracle": "unused.json"}
        with (
            mock.patch.object(runner, "_run", return_value=0) as execute,
            mock.patch.object(runner, "_run_imported") as imported,
            mock.patch.object(runner.compact, "verify", return_value={"status": "pass"}),
            mock.patch.object(runner, "verify_actions", return_value={}),
            contextlib.redirect_stdout(io.StringIO()),
        ):
            self.assertEqual(runner._run_cases(args, [case]), 0)
        imported.assert_not_called()
        self.assertEqual(execute.call_count, 2)
        report = json.loads((output / "report.json").read_text())
        self.assertTrue(report["native_only"])
        self.assertEqual(report["paired"]["status"], "not_run")
        self.assertEqual(report["qualification"], "incomplete")

    def scoped_comparison(self, status="pass"):
        return {
            "status": status,
            "frames_compared": self.count,
            "checks": {
                "frames": {"status": status},
                "terminal_result": {"status": "pass"},
                "simulation_rng": {"status": "pass", "simulation_events_compared": 1},
            },
        }

    def test_unqualified_comparative_failure_is_preserved_but_invalid_evidence_fails(self):
        cases, report = self.paired()
        diagnostic = next(row for row in report["cases"] if "door" in row["demo"])
        diagnostic["native_to_imported"]["status"] = "fail"
        diagnostic["native_to_imported"]["checks"]["simulation_rng"]["status"] = "fail"
        path = self.root / "report.json"
        path.write_text(json.dumps(report))

        def compare(path, case):
            result = self.scoped_comparison()
            if "door" in case["demo"]:
                result["status"] = "fail"
                result["checks"]["simulation_rng"]["status"] = "fail"
            return result

        with mock.patch.object(runner.gameplay, "compare_case", side_effect=compare):
            scoped = runner.check_paired_report(report, 1, cases, self.root, path)
        self.assertEqual(scoped["required_paired_cases"], ["d1-flight.dximdemo"])
        self.assertEqual(scoped["status"], "pass")
        row = next(row for row in scoped["cases"] if "door" in row["demo"])
        self.assertFalse(row["required"])
        self.assertEqual(row["scope_role"], "unqualified diagnostic-only")
        self.assertEqual(row["comparison"]["checks"]["simulation_rng"]["status"], "fail")
        for mutation in ("error", "missing", "frames", "native"):
            changed = copy.deepcopy(report)
            if mutation == "native":
                changed["cases"][2]["native_repeatability"]["checks"]["world_states"]["status"] = "fail"

            def broken(path, case):
                result = compare(path, case)
                if "door" in case["demo"]:
                    if mutation == "error":
                        result["checks"]["simulation_rng"]["error"] = "malformed trace"
                    elif mutation == "missing":
                        result["checks"].pop("terminal_result")
                    elif mutation == "frames":
                        result["frames_compared"] -= 1
                return result

            with (
                self.subTest(mutation=mutation),
                mock.patch.object(runner.gameplay, "compare_case", side_effect=broken),
                self.assertRaises(runner.parity.EvidenceError),
            ):
                runner.check_paired_report(changed, 1, cases, self.root, path)

    def test_imported_scope_uses_independent_artifacts_without_relaxing_native(self):
        cases, report = self.paired()
        for case in report["cases"]:
            imported = case["native_to_imported"]
            imported["status"] = "fail"
            for key in ("frames", "object_states", "world_states"):
                imported["checks"][key]["status"] = "fail"
        path = self.root / "report.json"
        path.write_text(json.dumps(report))
        before = path.read_bytes()
        with mock.patch.object(runner.gameplay, "compare_case", return_value=self.scoped_comparison()) as compare:
            scoped = runner.check_paired_report(report, 1, cases, self.root, path)
            self.assertEqual(compare.call_count, len(report["cases"]))
        self.assertEqual(scoped["status"], "pass")
        self.assertEqual(scoped["qualification"], "incomplete")
        self.assertEqual(path.read_bytes(), before)
        self.assertTrue(path.with_name("scoped-gameplay-report.json").is_file())
        for mutation in ("independent", "native", "missing_world", "terminal", "sim"):
            changed = copy.deepcopy(report)
            case = changed["cases"][0]
            if mutation == "native":
                case["native_repeatability"]["checks"]["world_states"]["status"] = "fail"
            elif mutation == "missing_world":
                case["native_to_imported"]["checks"]["world_states"]["status"] = "incomplete"
            elif mutation == "terminal":
                case["native_to_imported"]["checks"]["terminal_result"]["status"] = "fail"
            elif mutation == "sim":
                case["native_to_imported"]["checks"]["simulation_rng"]["status"] = "fail"
            comparison = self.scoped_comparison("fail" if mutation == "independent" else "pass")
            with (
                self.subTest(mutation=mutation),
                mock.patch.object(runner.gameplay, "compare_case", return_value=comparison),
                self.assertRaises(runner.parity.EvidenceError),
            ):
                runner.check_paired_report(changed, 1, cases, self.root, path)

    def test_corpus_requires_complete_unique_plain_filename_pairs(self):
        cases = [
            {
                "game": game,
                "scenario": scenario,
                "demo": f"{game}-{scenario}.dximdemo",
                "oracle": f"{game}-{scenario}.oracle.json",
                "actions": {},
            }
            for game, scenario in runner.PUBLIC_CASES
        ]
        manifest = self.root / "manifest.json"
        imported = {
            "schema": 1,
            "cases": {case["demo"]: self.imported_scope(case["scenario"]) for case in cases if case["game"] == "d1"},
        }

        def valid_pair(demo, oracle):
            game = demo.name[:2]
            level = 2 if "shoot-switch" in demo.name else 3 if game == "d1" and "door-lifecycle" in demo.name else 1
            observations = (
                "combat_observations"
                if "combat" in demo.name or "loot" in demo.name
                else "guidebot_observations"
                if "guidebot-release" in demo.name
                else "door_observations"
                if "door-lifecycle" in demo.name
                else None
            )
            return {"game": game, "level": level}, ({observations: {}} if observations else {})

        with mock.patch.object(runner.compact, "validate", side_effect=valid_pair):
            manifest.write_text(json.dumps({"schema": 1, "cases": cases, "imported_d1": imported}))
            self.assertEqual(len(runner.load_corpus(self.root)), len(runner.PUBLIC_CASES))
            for mutation in (
                "missing",
                "duplicate_case",
                "duplicate_file",
                "unix_path",
                "windows_path",
                "drive_path",
                "missing_actions",
                "wrong_level",
                "d1_switch",
            ):
                changed = copy.deepcopy(cases)
                if mutation == "missing":
                    changed.pop()
                elif mutation == "duplicate_case":
                    changed[-1] = changed[0]
                elif mutation == "duplicate_file":
                    changed[1]["demo"] = changed[0]["demo"]
                elif mutation == "unix_path":
                    changed[0]["demo"] = "../escaped.dximdemo"
                elif mutation == "windows_path":
                    changed[0]["oracle"] = "..\\escaped.json"
                elif mutation == "wrong_level":
                    next(case for case in changed if case["scenario"] == "shoot-switch")["demo"] = "d2-other.dximdemo"
                elif mutation == "d1_switch":
                    next(case for case in changed if case["scenario"] == "shoot-switch")["game"] = "d1"
                elif mutation == "drive_path":
                    changed[0]["demo"] = "C:escaped.dximdemo"
                else:
                    changed[0].pop("actions")
                manifest.write_text(json.dumps({"schema": 1, "cases": changed, "imported_d1": imported}))
                with (
                    self.subTest(mutation=mutation),
                    self.assertRaises(runner.parity.EvidenceError),
                ):
                    runner.load_corpus(self.root)
            for mutation in (
                "missing_case",
                "extra_case",
                "missing_schema",
                "missing_inventory",
                "bool_schema",
                "float_schema",
                "extra_key",
            ):
                changed = copy.deepcopy(imported)
                if mutation == "missing_case":
                    changed["cases"].pop(next(iter(changed["cases"])))
                elif mutation == "extra_case":
                    changed["cases"]["unknown.dximdemo"] = self.imported_scope("unknown")
                elif mutation == "missing_schema":
                    changed.pop("schema")
                elif mutation == "bool_schema":
                    changed["schema"] = True
                elif mutation == "float_schema":
                    changed["schema"] = 1.0
                elif mutation == "extra_key":
                    changed["unknown"] = True
                else:
                    changed = None
                manifest.write_text(json.dumps({"schema": 1, "cases": cases, "imported_d1": changed}))
                with (
                    self.subTest(mutation=mutation),
                    self.assertRaises(runner.parity.EvidenceError),
                ):
                    runner.load_corpus(self.root)


class RealNegativeCorpusTests(unittest.TestCase):
    def test_capture_manifest_preserves_every_reviewed_imported_scope(self):
        import capture_compact_public_replays as capture

        repo = Path(__file__).resolve().parents[2]
        source = repo / "android/tests/fixtures/public_level_replays"
        original = json.loads((source / "manifest.json").read_text())
        actual = capture.candidate_manifest(repo, original["cases"])
        self.assertEqual(actual, original)
        selected = [case for case in original["cases"] if case["scenario"] == "combat"]
        actual = capture.candidate_manifest(repo, selected)
        self.assertEqual(actual["cases"], selected)
        self.assertEqual(
            actual["imported_d1"],
            {
                "schema": 1,
                "cases": {"d1-public-combat.dximdemo": original["imported_d1"]["cases"]["d1-public-combat.dximdemo"]},
            },
        )
        with self.assertRaises(KeyError):
            capture.candidate_manifest(repo, [{"game": "d1", "demo": "undeclared.dximdemo"}])

    def test_real_imported_scope_retains_loot_diagnostics_and_required_flight(self):
        source = Path(__file__).parent / "fixtures/public_level_replays"
        included, excluded = runner.imported_d1_scope(runner.load_corpus(source))
        self.assertEqual(
            {case["demo"] for case in included},
            {f"d1-public-{scenario}.dximdemo" for scenario in ("flight", "pickup", "door", "combat", "loot")},
        )
        self.assertEqual(
            {case["demo"] for case in excluded},
            {
                "d1-public-door-lifecycle.dximdemo",
                "d1-public-secondary-release-first.dximdemo",
                "d1-public-secondary-mouse-first.dximdemo",
            },
        )
        self.assertEqual(
            next(case for case in included if case["scenario"] == "loot")["imported_d1"]["role"], "diagnostic-only"
        )
        self.assertEqual(
            next(case for case in included if case["scenario"] == "flight")["imported_d1"]["role"], "required-gameplay"
        )

    def assert_missing_observation_rejected(self, scenarios, field, message):
        source = Path(__file__).parent / "fixtures/public_level_replays"
        cases = runner.load_corpus(source)
        bindings = set()
        for case in cases:
            if case["scenario"] not in scenarios:
                continue
            with self.subTest(game=case["game"], scenario=case["scenario"]), tempfile.TemporaryDirectory() as root:
                import shutil

                destination = Path(root) / "corpus"
                shutil.copytree(source, destination)
                demo, path = (destination / case[key] for key in ("demo", "oracle"))
                original_oracle = path.read_bytes()
                oracle = json.loads(original_oracle)
                self.assertEqual(oracle["compact_sha256"], runner.parity.digest(demo))
                self.assertFalse(demo.with_suffix(".migration.json").exists())
                bindings.add("live")
                oracle.pop(field)
                runner.parity.write_json(path, oracle)
                _, admitted = runner.compact.validate(demo, path)
                self.assertNotIn(field, admitted)
                with self.assertRaisesRegex(runner.parity.EvidenceError, message):
                    runner.load_corpus(destination)
                self.assertEqual((source / case["oracle"]).read_bytes(), original_oracle)
        return bindings

    def test_combat_cannot_drop_its_immutable_causal_observation_oracle(self):
        self.assertEqual(
            self.assert_missing_observation_rejected(
                {"combat", "loot"}, "combat_observations", "immutable live damage/loot"
            ),
            {"live"},
        )

    def test_door_cannot_drop_its_immutable_geometric_observation_oracle(self):
        self.assertEqual(
            self.assert_missing_observation_rejected(
                {"door-lifecycle"}, "door_observations", "immutable live geometric"
            ),
            {"live"},
        )

    def test_guidebot_cannot_drop_its_immutable_escort_observation_oracle(self):
        self.assertEqual(
            self.assert_missing_observation_rejected(
                {"guidebot-release"}, "guidebot_observations", "immutable live escort"
            ),
            {"live"},
        )

    def assert_changed_ordered_controls(self, case, original_demo, changed_demo):
        original = ordered.validate(original_demo[0], original_demo[1:-2], original_demo[-2])
        changed = ordered.validate(changed_demo[0], changed_demo[1:-2], changed_demo[-2])
        self.assertEqual(len(original), len(changed))
        self.assertTrue(changed[-1]["is_tail"])
        self.assertEqual(changed[-1]["f"], original_demo[0]["frame_count"])
        self.assertNotEqual(
            runner.compact.sha([batch["ops"] for batch in original]),
            runner.compact.sha([batch["ops"] for batch in changed]),
            "Bad-input mutation must change executed operations, including resolved aliases",
        )
        release = next((row["f"] + 1 for row in case["actions"].get("door_events", []) if row["stage"] == 2), None)
        for before, after in zip(original, changed):
            expected = copy.deepcopy(before)
            scenario = case["scenario"]
            if scenario == "door-lifecycle" and before["f"] < release:
                self.assertEqual(before, after)
                continue
            controls = [expected["residual"]] + [op for op in expected["ops"] if op["k"] in ("sample", "controls")]
            for controls_at_dispatch in controls:
                if scenario in ("loot", "combat", "guidebot-release"):
                    # Remove firing while preserving the original flight/aim and all other inputs
                    controls_at_dispatch["s"]["f1s"] = 0
                    controls_at_dispatch["p"]["f1"] = 0
                else:
                    for field in ("s", "p"):
                        controls_at_dispatch[field] = dict.fromkeys(controls_at_dispatch[field], 0)
            # This includes each operation's kind/order/time, commands, pre/post RNG and tail boundary
            self.assertEqual(expected, after)

    def test_disposable_mutations_are_admitted_without_changing_originals(self):
        source = Path(__file__).parent / "fixtures/public_level_replays"
        cases = runner.load_corpus(source)
        originals = {path.name: path.read_bytes() for path in source.iterdir() if path.is_file()}
        with tempfile.TemporaryDirectory() as root:
            for mutation in ("wrong-oracle", "bad-input"):
                destination = Path(root) / (mutation + "-corpus")
                negative.corrupt_corpus(source, destination, cases, mutation)
                self.assertEqual(runner.load_corpus(destination), cases)
                for case in cases:
                    with self.subTest(mutation=mutation, game=case["game"], scenario=case["scenario"]):
                        demo_path, oracle_path = (destination / case[key] for key in ("demo", "oracle"))
                        expected = json.loads(originals[case["oracle"]])
                        changed = json.loads(oracle_path.read_bytes())
                        expected_oracle = copy.deepcopy(expected)
                        expected_oracle["compact_sha256"] = runner.parity.digest(demo_path)
                        if mutation == "wrong-oracle":
                            digest = expected_oracle["frame_sha256"][0]
                            expected_oracle["frame_sha256"][0] = ("1" if digest[0] == "0" else "0") + digest[1:]
                        self.assertEqual(changed, expected_oracle)
                        # Disposable controls bind directly to their own explicitly mutated input
                        self.assertEqual(changed["compact_sha256"], runner.parity.digest(demo_path))
                        _, admitted = runner.compact.validate(demo_path, oracle_path)
                        self.assertEqual(admitted, changed)
                        migration_name = demo_path.with_suffix(".migration.json").name
                        self.assertNotIn(migration_name, originals)
                        self.assertFalse((destination / migration_name).exists())
                        original_demo = [json.loads(line) for line in originals[case["demo"]].splitlines()]
                        changed_demo = [json.loads(line) for line in demo_path.read_bytes().splitlines()]
                        self.assertEqual(len(original_demo), len(changed_demo))
                        self.assertEqual(original_demo[0], changed_demo[0])
                        self.assertEqual(original_demo[-1], changed_demo[-1])
                        self.assertEqual(changed_demo[-2]["type"], "input_tail")
                        if mutation == "wrong-oracle":
                            self.assertEqual(originals[case["demo"]], demo_path.read_bytes())
                            continue
                        self.assertNotEqual(originals[case["demo"]], demo_path.read_bytes())
                        self.assert_changed_ordered_controls(case, original_demo, changed_demo)
                        spec = negative.early_abort_spec(case, mutation)
                        if spec is not None:
                            phase, frame = spec
                            args = argparse.Namespace(corpus=source, output=Path(root))
                            negative.verify_declared_mutation(
                                args, case, ["-DemoPath", str(demo_path)], mutation, frame, phase
                            )
                            with self.assertRaisesRegex(runner.parity.EvidenceError, "unintended recording"):
                                negative.verify_declared_mutation(
                                    args, case, ["-DemoPath", str(source / case["demo"])], mutation, frame, phase
                                )
                        # Also pin the wire-level delta, not just its decoded gameplay meaning
                        for before, after in zip(copy.deepcopy(original_demo[1:-1]), changed_demo[1:-1]):
                            if case["scenario"].startswith("secondary-"):
                                before["input"] = {"s": {"f2s": 0}}
                                for operation in before["dispatch"]["ops"]:
                                    if operation["k"] == "sample":
                                        operation["s"] = {}
                                        operation.pop("p", None)
                            elif case["scenario"] == "loot":
                                if before["f"] == 0 or "f1s" in before["input"].get("s", {}):
                                    before["input"].setdefault("s", {})["f1s"] = 0
                            elif case["scenario"] in ("combat", "guidebot-release"):
                                before["input"].setdefault("s", {})["f1s"] = 0
                            elif case["scenario"] == "door-lifecycle":
                                release = next(
                                    row["f"] + 1 for row in case["actions"]["door_events"] if row["stage"] == 2
                                )
                                held_keys = {key for row in original_demo[1:-1] for key in row["input"].get("s", {})}
                                if before["f"] >= release:
                                    before["input"] = (
                                        {"s": dict.fromkeys(held_keys, 0)} if before["f"] == release else {}
                                    )
                            else:
                                before["input"] = {}
                            self.assertEqual(before, after)
        self.assertEqual(
            originals,
            {path.name: path.read_bytes() for path in source.iterdir() if path.is_file()},
        )


if __name__ == "__main__":
    unittest.main()
