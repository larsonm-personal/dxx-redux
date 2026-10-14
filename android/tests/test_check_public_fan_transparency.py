"""Offline adversarial tests for the bounded fan fault diagnostic."""

import contextlib
import copy
import gzip
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import check_public_fan_transparency as fan
import d1_replay_parity as parity
import ordered_public_replay as ordered


def example(schema):
    if schema is int:
        return 0
    if isinstance(schema, dict):
        return {k: example(v) for k, v in schema.items() if k != "*"}
    if isinstance(schema, tuple):
        return [example(v) for v in schema]
    return []


def result(header):
    return {
        "version": 2,
        **{k: header[k] for k in ("game", "mission", "level", "difficulty", "frame_count")},
        "game_time64": header["frame_count"] * 4096,
        "player0": {
            **dict.fromkeys(
                "energy shields score lives laser_level primary_weapon secondary_weapon flags hostages".split(), 0
            ),
            "primary_ammo": [0] * 16,
            "secondary_ammo": [0] * 16,
        },
        "position": dict.fromkeys("segment x y z forward_x forward_y forward_z".split(), 0),
        "level_summary": {
            **dict.fromkeys("robots_alive robots_killed hostages_remaining powerups_remaining".split(), 0),
            "control_center_destroyed": False,
            "endlevel_completed": False,
        },
    }


def capture(no_fire=False, delay=0):
    c = {
        "game": "d1",
        "scenario": "fan-transparency",
        "capture_role": "native_d1_reference",
        "public_replay_admission": False,
        "pixel_observation": True,
        "armed_frame": 299,
        "request_frame": 299 + delay,
        "no_fire": no_fire,
        "fire_delay": delay,
        "frames": 324,
        "flare_slot": -1 if no_fire else 77,
        "flare_signature": -1 if no_fire else 179,
        "flare_radius": 26709,
        "hold": [1, 2, 3],
        "aim": [4, 5, 6],
        "hit_center": [8209978, -3153217, 30042966],
        "observations": [],
    }
    for f in range(324):
        phase = 1 if f == 301 else 0
        row = {"f": f, "fan_frame": phase, "fan_bitmap": 823 + phase, "fan_timer": 2156}
        if not no_fire and f >= 299 + delay:
            passed = not delay and f >= 300
            row["flare"] = {
                "slot": 77,
                "signature": 179,
                "flags": 0,
                "lifeleft": 270000 - max(0, f - 299) * 4096,
                "parent_num": 0,
                "parent_signature": 1,
                "physics_flags": 16,
                "position": fan.FAULT["native_position"] if passed else [8207005, -3163843, 30042452],
                "segment": 142 if passed else 135,
                "size": 26709,
                "velocity": [0, 0, 0] if delay and f >= 301 else fan.FAULT["native_velocity"],
            }
        if f >= 299:
            pixel = {
                "point": c["hit_center"],
                "u": -16895,
                "v": 35329,
                "x": 47,
                "y": 34,
                "pixel": 255 if phase == 0 else 59,
                "native_crossable": int(phase == 0),
                "effect_frame": phase,
                "wall_bitmap": 823 + phase,
            }
            row["pixel_probe"] = pixel
            row["collision_pixel_probe"] = {**pixel, "point": fan.FAULT["position"], "u": -17056, "v": 35286}
        c["observations"].append(copy.deepcopy(row))
    return c


def raw_flare(flare):
    if flare is None:
        return None
    o = example(parity.OBJECT_SCHEMA)
    o.update(
        signature=179,
        next=-1,
        type=5,
        id=9,
        control_type=9,
        movement_type=1,
        size=26709,
        flags=0,
        lifeleft=flare["lifeleft"],
        segnum=flare["segment"],
        pos=flare["position"],
    )
    o["physics"] = {**example(parity.PHYSICS_SCHEMA), "velocity": flare["velocity"], "flags": 16}
    o["weapon"] = {
        **dict.fromkeys(
            "parent_type parent_num parent_signature creation_time last_hitobj track_goal multiplier creation_framecount".split(),
            0,
        ),
        "parent_type": 4,
        "parent_signature": flare["parent_signature"],
        "hitobj_list": [0] * 1000,
    }
    return o


def player(signature=1):
    return {**example(parity.OBJECT_SCHEMA), "signature": signature, "type": 4, "control_type": 4, "lifeleft": 100000}


def world(engine="d1"):
    state = example(parity.WORLD_SCHEMA)
    state["exploding_walls"] = {"capacity": 10, "slots": {}}
    ai = state["ai"]
    ai["local_capacity"] = 1000
    ai["local_default"].update(last_see_time=0, last_attack_time=0, wait_time=0)
    ai["boss"].update(
        Boss_hit_this_frame=0,
        Boss_been_hit=0,
        teleport_segments=[0] * 100,
        Num_boss_gate_segs=0,
        gate_segments=[0] * 100,
    )
    for k, n in (("paths", 2500), ("cloak", 8), ("awareness", 64)):
        ai[k] = [example(parity.WORLD_SCHEMA["ai"][k][0]) for _ in range(n)]
    for k in ("goal_angles", "delta_angles", "goal_state", "achieved_state"):
        ai["local_default"][k] = [example(parity.AI_LOCAL_SCHEMA[k][0]) for _ in range(10)]
    state["segments"] = [example(parity.WORLD_SCHEMA["segments"][0])]
    state["effects"] = [example(parity.WORLD_SCHEMA["effects"][0]) for _ in range(4)]
    if engine == "d2":
        ai.update(Believed_player_seg=0, Ai_last_missile_camera=-1)
        ai["local_default"]["d1_saved"] = {
            k: ai["local_default"].pop(k) for k in ("last_see_time", "last_attack_time", "wait_time")
        }
        ai["local_default"].update(next_action_time=0, next_fire2=0)
        ai["path_runtime"]["last_buddy_polish_path_tick"] = 0
        for item in ai["cloak"]:
            item["last_segment"] = 0
        ai["boss"].pop("Boss_hit_this_frame")
        ai["boss"].pop("Boss_been_hit")
        ai["boss"].update(Boss_hit_time=0, d1_hit_pending=0, d1_been_hit=0)
    parity.validate_world_state(state, engine)
    return state


class FanTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.capture = capture()
        self.header = {
            "type": "header",
            "version": 5,
            "game": "d1",
            "mission": "d1",
            "level": 1,
            "difficulty": 0,
            "frame_count": 324,
            "start_mode": "new_level",
            "rng_mode": "lcg_state",
        }
        self.event = {
            "type": "rand",
            "frame": 301,
            "gt": 1232896,
            "call_count": 346,
            "ctx_obj": 77,
            "ctx_sig": 179,
            "ctx_id": 9,
            "state_before": 2301841099,
            "state_after": 2296157864,
            "result": 2268,
        }
        self.frames = []
        for f in range(324):
            self.frames.append(
                {
                    "type": "frame",
                    "f": f,
                    "ft": 4096,
                    "input": {},
                    "rng": dict(fan.FAULT["rng_before"] if f <= 301 else fan.FAULT["rng_after"]),
                    "dispatch": {
                        "pre_rng": "=",
                        "ops": [{"k": "sample", "ft": 4096, "s": {}, "p": {"fl": int(f == 299)}}],
                    },
                    "state": {},
                    "diag": {k: [0] * n if n else 0 for k, n in parity.FRAME_DIAGNOSTIC_FIELDS.items()},
                }
            )
        self.tail = {
            "type": "input_tail",
            "f": 324,
            "input": {},
            "rng": dict(fan.FAULT["rng_after"]),
            "dispatch": {"pre_rng": "=", "ops": []},
        }
        self.paths = {
            k: self.root / name
            for k, name in (
                ("capture", "capture.json"),
                ("full", "full.dximdemo"),
                ("compact", "probe.dximdemo"),
                ("provenance", "provenance.json"),
                ("rng", "rng.jsonl"),
            )
        }
        self.write_sources()
        self.oracle = fan.make_oracle(*(self.paths[k] for k in ("capture", "full", "compact", "provenance", "rng")))
        self.demo = fan.read_demo(self.paths["compact"])
        self.native, self.imported = self.decoded_pair()

    @staticmethod
    def write_rows(path, rows):
        raw = b"".join(fan.strict.encoded(row) for row in rows)
        path.write_bytes(gzip.compress(raw) if path.suffix == ".gz" else raw)

    def write_sources(self):
        self.paths["capture"].write_bytes(fan.strict.encoded(self.capture))
        self.paths["provenance"].write_bytes(
            fan.strict.encoded(
                {
                    "status": "synthetic offline evidence",
                    "engine_inputs_unchanged": True,
                    "engine_source_revision": "a" * 40,
                    "executable_sha256": "b" * 64,
                }
            )
        )
        self.write_rows(
            self.paths["full"],
            [self.header, *self.frames, self.tail, {"type": "result", "result": result(self.header)}],
        )
        frames, tail = ordered.compact(self.header, self.frames, self.tail)
        for row in frames:
            row.pop("state", None)
            row.pop("diag", None)
        self.write_rows(
            self.paths["compact"], [self.header, *frames, tail, {"type": "result", "result": result(self.header)}]
        )
        self.write_rows(
            self.paths["rng"], [{"type": "meta", "events": 1, "truncated": False}, {**self.event, "seq": 0}]
        )

    def decoded_pair(self):
        samples, effects = [], []
        for f in range(325):
            row = self.capture["observations"][max(0, f - 1)]
            if f == 299:
                row = self.capture["observations"][299]
            samples.append(
                {
                    "flare": raw_flare(row.get("flare")),
                    "player": player(),
                    "clock": {"game_time": (f + 1) * 4096, "frame_time": 4096, "d_tick_count": f + 1},
                    "rng": [{"available": 1, "state": 2301841099, "calls": 345}],
                }
            )
            effects.append(
                {"frame_count": row["fan_frame"], "wall_bitmap": row["fan_bitmap"], "time_left": row["fan_timer"]}
            )
        native = {
            "samples": samples,
            "effects": effects,
            "frames": 324,
            "phase": "terminal",
            "sha256": "a" * 64,
            **{
                k: copy.deepcopy(self.oracle["recording_checks"][k])
                for k in ("full_frame_sha256", "gameplay_frame_sha256")
            },
        }
        imported = copy.deepcopy(native)
        imported.update(frames=301, phase="aborted", sha256="b" * 64)
        for k in ("samples", "effects"):
            imported[k] = imported[k][:302]
        for k in ("full_frame_sha256", "gameplay_frame_sha256"):
            imported[k] = imported[k][:301]
        imported["full_frame_sha256"][0] = "c" * 64
        for sample in imported["samples"]:
            sample["player"]["signature"] = 0
            if sample["flare"]:
                sample["flare"]["weapon"]["parent_signature"] = 0
        after = imported["samples"][-1]
        after["flare"].update(next=75, segnum=135, pos=copy.deepcopy(fan.FAULT["position"]))
        after["flare"]["physics"]["velocity"] = [0, 0, 0]
        after["clock"] = {"game_time": 1232896, "frame_time": 4096, "d_tick_count": 301}
        after["rng"] = [{"available": 1, "state": 2296157864, "calls": 346}]
        imported["effects"][-1] = {"frame_count": 1, "wall_bitmap": 824, "time_left": 2156}
        return native, imported

    def compare(self):
        return fan.compare_evidence(
            self.oracle,
            self.demo,
            self.native,
            self.imported,
            ([self.event], fan.FAULT["rng_after"]),
            ([{**self.event, "frame": 300}], fan.FAULT["rng_after"]),
        )

    def test_native_only_bounded_oracle_and_exact_companion(self):
        self.assertLessEqual(len(fan.strict.encoded(self.oracle)), 100000)
        self.assertEqual(fan.validate_companion(self.oracle, self.paths["compact"]), self.demo)
        for key, source in (
            ("capture_sha256", "capture"),
            ("full_recording_sha256", "full"),
            ("compact_sha256", "compact"),
            ("capture_provenance_sha256", "provenance"),
            ("capture_rng_sha256", "rng"),
        ):
            self.assertEqual(self.oracle["source"][key], parity.digest(self.paths[source]))
        self.paths["compact"].write_text(self.paths["compact"].read_text() + "\n")
        with self.assertRaisesRegex(parity.EvidenceError, "bytes"):
            fan.validate_companion(self.oracle, self.paths["compact"])

    def test_fault_success_keeps_raw_parity_failure(self):
        report = self.compare()
        self.assertEqual(report["status"], "known_fault_reproduced")
        self.assertFalse(report["public_replay_admission"])
        self.assertEqual(report["imported_completion"], {"phase": "aborted", "frames": 301, "expected_frames": 324})
        self.assertEqual(report["raw_comparison"]["status"], "fail")
        self.assertEqual(report["raw_comparison"]["full_diagnostics"]["status"], "fail")
        self.assertTrue(
            any(
                d["path"] == "$.flare.weapon.parent_signature"
                for d in report["raw_comparison"]["objects_and_fan"]["differences"]
            )
        )

    def test_native_native_pair_never_reproduces_fault(self):
        self.imported = copy.deepcopy(self.native)
        with self.assertRaises(parity.EvidenceError):
            self.compare()

    def test_wrong_phase_magnitude_abort_clock_prefix_and_rng_rejected(self):
        changes = [
            lambda: self.imported["effects"][-1].update(frame_count=0),
            lambda: self.imported["effects"][-1].update(wall_bitmap=823),
            lambda: self.imported["samples"][-1]["flare"]["pos"].__setitem__(0, 1),
            lambda: self.imported["samples"][-1]["flare"]["physics"].update(velocity=[1, 0, 0]),
            lambda: self.imported["samples"][-1]["clock"].update(game_time=1236992),
            lambda: self.imported["samples"][-1]["rng"][0].update(calls=345),
            lambda: self.imported["gameplay_frame_sha256"].__setitem__(2, "f" * 64),
            lambda: self.imported["samples"][300]["flare"].update(size=26710),
            lambda: self.imported["samples"][299]["player"].update(signature=5),
            lambda: self.imported.update(phase="terminal"),
            lambda: self.imported.update(frames=302),
        ]
        for change in changes:
            with self.subTest(change=change):
                self.native, self.imported = self.decoded_pair()
                change()
                with self.assertRaises(parity.EvidenceError):
                    self.compare()

    def test_reused_missing_dead_and_wrong_reference_rejected(self):
        valid = {"0": player(0), "77": copy.deepcopy(self.imported["samples"][300]["flare"])}
        changes = [
            lambda x: x.pop("77"),
            lambda x: x["77"].update(signature=180),
            lambda x: x["77"].update(flags=2),
            lambda x: x["77"].update(id=10),
            lambda x: x["77"]["weapon"].update(parent_signature=1),
            lambda x: x["77"]["weapon"].update(parent_num=1),
        ]
        for change in changes:
            objects = copy.deepcopy(valid)
            change(objects)
            with self.subTest(change=change), self.assertRaises(parity.EvidenceError):
                fan.select(objects, self.capture, 300)

    def test_missing_fixed_pixel_wrong_point_uv_or_native_source_rejected(self):
        changes = [
            lambda c: c.update(game="d2"),
            lambda c: c.update(capture_role="imported_d1_diagnostic"),
            lambda c: c.update(pixel_observation=False),
            lambda c: c["observations"][300].pop("collision_pixel_probe"),
            lambda c: c["observations"][300]["collision_pixel_probe"].update(u=1),
            lambda c: c["observations"][300]["collision_pixel_probe"].update(pixel=59),
            lambda c: c["observations"][300]["flare"].update(parent_signature=0),
            lambda c: c["observations"].pop(),
        ]
        for change in changes:
            value = copy.deepcopy(self.capture)
            change(value)
            with self.subTest(change=change), self.assertRaises(parity.EvidenceError):
                fan.validate_capture(value)

    def test_duplicate_nonfinite_and_truncated_json_fail_closed(self):
        path = self.root / "bad.jsonl"
        for raw in ('{"type":"meta","type":"meta"}\n', '{"x":NaN}\n', '{"x":1e999}\n', '{"x":Infinity}\n', '{"x":'):
            path.write_text(raw)
            with self.subTest(raw=raw), self.assertRaises(parity.EvidenceError):
                list(fan.strict.strict_records(path))
        zipped = self.root / "bad.jsonl.gz"
        zipped.write_bytes(gzip.compress(b'{"x":1}\n')[:-5])
        with self.assertRaises(parity.EvidenceError):
            list(fan.strict.strict_records(zipped))

    def test_arbitrary_failure_exit_and_abort_text_never_pass(self):
        log = self.root / "run.log"
        for exit_code, text in (
            (1, "segmentation fault"),
            (2, "Input demo replay stopped: carried SIM RNG differs before input at frame 301"),
            (1, "Input demo replay stopped: carried SIM RNG differs before input at frame 302"),
            (
                1,
                "Input demo replay stopped: carried SIM RNG differs before input at frame 301\nInput demo replay stopped: another failure",
            ),
        ):
            log.write_text(text)
            with self.subTest(exit_code=exit_code, text=text), self.assertRaises(parity.EvidenceError):
                fan.compare_pair(
                    self.oracle,
                    self.paths["compact"],
                    self.root / "absent",
                    self.root / "absent",
                    self.paths["rng"],
                    self.paths["rng"],
                    self.root / "absent",
                    log,
                    exit_code,
                )

    def test_raw_rng_event_count_state_and_reseed_rejected(self):
        original = [{"type": "meta", "events": 1, "truncated": False}, {**self.event, "seq": 0}]
        changes = [
            lambda r: r[0].update(truncated=True),
            lambda r: r[0].update(events=2),
            lambda r: r[1].update(seq=1),
            lambda r: r[1].update(state_after=1),
            lambda r: r[1].update(type="srand", seed=1),
            lambda r: r[1].update(call_count=True),
        ]
        for change in changes:
            rows = copy.deepcopy(original)
            change(rows)
            self.write_rows(self.paths["rng"], rows)
            with self.subTest(change=change), self.assertRaises(parity.EvidenceError):
                fan.rng_trace(self.paths["rng"], self.demo, 324)

    def test_causal_decoder_compares_all_controls_beyond_flare(self):
        variants = []
        for no_fire, delay in ((False, 0), (True, 0), (False, 1)):
            c = capture(no_fire, delay)
            d = copy.deepcopy(self.demo)
            for b in d["batches"]:
                for op in b["ops"]:
                    op["p"]["fl"] = int(not no_fire and b["f"] == 299 + delay)
            variants.append(({"metadata": c, "source": {}}, d))
        with mock.patch.object(fan, "validate_companion", side_effect=[v[1] for v in variants]):
            value = fan.causal_gate(variants[0][0], "p", variants[1][0], "n", variants[2][0], "d")
        self.assertEqual(value["status"], "causal_controls_verified")
        variants[2][1]["batches"][20]["ops"][0]["s"]["f"] = 1
        with (
            mock.patch.object(fan, "validate_companion", side_effect=[v[1] for v in variants]),
            self.assertRaises(parity.EvidenceError),
        ):
            fan.causal_gate(variants[0][0], "p", variants[1][0], "n", variants[2][0], "d")

    def trace_rows(self, engine="d1", aborted=False):
        decoded = self.imported if aborted else self.native
        state = world(engine)
        rows = [
            {
                "type": "meta",
                **{k: self.header[k] for k in ("game", "mission", "level", "difficulty", "frame_count", "start_mode")},
            }
        ]

        def storage(f, phase=None):
            sample = decoded["samples"][f]
            objects = {"0": sample["player"]}
            if phase != "restored" and sample["flare"]:
                objects["77"] = sample["flare"]
            row = example(parity.OBJECT_STORAGE_SCHEMA)
            row.update(
                type="object_boundary" if phase else "object_state",
                version=2,
                f=f,
                reset=bool(phase or f == 0),
                capacity=1000,
                slots=copy.deepcopy(objects),
                clock=sample["clock"],
                rng=sample["rng"],
            )
            row["rng"] = row["rng"] + [{"available": 1, "state": 1, "calls": 0}]
            row["allocator"].update(
                num_objects=len(objects),
                highest_object_index=77 if "77" in objects else 0,
                free_obj_list=list(range(1000)),
            )
            if phase:
                row["phase"] = phase
            return row

        def boundary(f, phase):
            rows.append(storage(f, phase))
            w = copy.deepcopy(state)
            w["effects"][3].update(decoded["effects"][f])
            rows.append({"type": "world_boundary", "version": 10, "f": f, "phase": phase, "reset": True, "state": w})

        boundary(0, "restored")
        for f in range(decoded["frames"]):
            rows.append({**copy.deepcopy(self.frames[f]), "type": "frame_state"})
            rows.append(storage(f))
            w = copy.deepcopy(state) if f == 0 else {"effects": copy.deepcopy(state["effects"])}
            w["effects"][3].update(decoded["effects"][f])
            rows.append({"type": "world_state", "version": 10, "f": f, "reset": f == 0, "state": w})
        boundary(decoded["frames"], decoded["phase"])
        return rows

    def test_lossless_trace_decoder_preserves_aborted_boundary(self):
        path = self.root / "trace.jsonl.gz"
        self.write_rows(path, self.trace_rows("d2", True))
        decoded = fan.read_trace(path, self.oracle, "d2", True)
        self.assertEqual(decoded["phase"], "aborted")
        self.assertEqual(decoded["samples"][-1]["flare"]["pos"], fan.FAULT["position"])
        with self.assertRaises(parity.EvidenceError):
            list(parity.world_states(path, self.header, "d2"))

    def test_missing_duplicate_reset_or_truncated_raw_trace_rejected(self):
        base = self.trace_rows("d2", True)
        path = self.root / "trace.jsonl"
        cases = [
            base[:-1],
            base[:8] + base[9:],
            base[:9] + [base[8]] + base[9:],
            [base[0], base[0], *base[1:]],
            copy.deepcopy(base),
            copy.deepcopy(base),
        ]
        cases[-2][-1]["phase"] = "terminal"
        cases[-1][7]["reset"] = True
        for rows in cases:
            self.write_rows(path, rows)
            with self.subTest(rows=len(rows)), self.assertRaises(parity.EvidenceError):
                fan.read_trace(path, self.oracle, "d2", True)

    def test_rng_context_difference_is_only_exact_bound_player_signature(self):
        left = {**self.event, "ctx_obj": 0, "ctx_id": 0, "ctx_sig": 1}
        right = {**left, "ctx_sig": 0}
        fan.rng_value_prefix([left], [right])
        for key, value in (("ctx_sig", 2), ("ctx_obj", 77), ("result", 2269), ("frame", 300)):
            broken = {**right, key: value}
            with self.subTest(key=key), self.assertRaises(parity.EvidenceError):
                fan.rng_value_prefix([left], [broken])

    def test_collision_probe_requires_exact_pixel_geometry_and_rng(self):
        lines = [
            "frame=300 gt=1232896 kind=trans_wall_pixel obj=77 sig=179 id=9 seg=135 seg=135 side=4 face=0 tmap=83 tmap2=329 wall=5 u=-17056 v=35286 bmx=47 bmy=34 bm_w=64 bm_h=64 direct=59 gpixel=59 transparent=0 p=(8208474,-3156161,30042682)",
            "frame=300 gt=1232896 kind=physics_fvi_fate obj=77 sig=179 id=9 fate=wall start_seg=135 hit_side=4 hit_side_seg=135 child=142 wall=5 size=26709 parent=0 parent_sig=0 target=(8216206,-2905300,30439592) vel=(129480,4201080,6646920)",
            "frame=300 gt=1232896 kind=awareness_rng_gate obj=77 sig=179 id=9 source=collide_weapon_wall source_obj=77 aux_obj=135 calls=345->346 state=2301841099->2296157864 added=1",
        ]
        path = self.root / "probe.log"
        text = "\n".join(lines)
        path.write_text(text)
        self.assertEqual(len(fan.probe_evidence(path)["target_rows"]), 3)
        variants = [
            "\n".join(lines[:-1]),
            text + "\n" + lines[0],
            text.replace("gpixel=59", "gpixel=255"),
            text.replace("bmx=47", "bmx=46"),
            text.replace("sig=179", "sig=178"),
            text.replace("345->346", "345->347"),
            text.replace("seg=135 seg=135", "seg=135 seg=134"),
        ]
        for value in variants:
            path.write_text(value)
            with self.subTest(value=value), self.assertRaises(parity.EvidenceError):
                fan.probe_evidence(path)

    def test_non_neutral_next_input_or_extra_flare_rejected(self):
        for frame, group, field in ((301, "s", "f"), (301, "p", "fl"), (300, "p", "f1")):
            demo = copy.deepcopy(self.demo)
            demo["batches"][frame]["ops"][0][group][field] = 1
            with self.subTest(frame=frame, field=field), self.assertRaises(parity.EvidenceError):
                fan.check_actions(self.capture, demo)

    def test_oracle_provenance_is_revalidated(self):
        changes = {
            "engine_inputs_unchanged": False,
            "engine_source_revision": "unqualified source",
            "executable_sha256": "not a digest",
            "status": " ",
        }
        for key, value in changes.items():
            with self.subTest(field=key):
                oracle = copy.deepcopy(self.oracle)
                oracle["provenance"][key] = value
                with self.assertRaises(parity.EvidenceError):
                    fan.validate_oracle(oracle)
        oracle = copy.deepcopy(self.oracle)
        oracle["provenance"]["unexpected"] = True
        with self.assertRaises(parity.EvidenceError):
            fan.validate_oracle(oracle)

    def test_cli_enforces_report_byte_budget(self):
        output = self.root / "oversized-report.json"
        argv = ["create-oracle"]
        for flag, key in (
            ("native-capture", "capture"),
            ("native-demo", "full"),
            ("compact-demo", "compact"),
            ("provenance", "provenance"),
            ("native-rng", "rng"),
        ):
            argv.extend(["--" + flag, str(self.paths[key])])
        argv.extend(["--output", str(output)])
        with (
            mock.patch.object(fan, "MAX_REPORT_BYTES", 16),
            mock.patch.object(fan, "make_oracle", return_value={"oversized": "x" * 32}),
            mock.patch("sys.stderr"),
        ):
            self.assertEqual(fan.main(argv), 2)
        result = json.loads(output.read_text())
        self.assertEqual(result["status"], "invalid")
        self.assertIn("output byte budget", result["error"])

    def test_cli_invalidates_stale_success_output(self):
        output = self.root / "output.json"
        output.write_text('{"status":"known_fault_reproduced"}')
        with contextlib.redirect_stderr(io.StringIO()):
            code = fan.main(
                [
                    "create-oracle",
                    "--native-capture",
                    str(self.root / "missing"),
                    "--native-demo",
                    str(self.paths["full"]),
                    "--compact-demo",
                    str(self.paths["compact"]),
                    "--provenance",
                    str(self.paths["provenance"]),
                    "--native-rng",
                    str(self.paths["rng"]),
                    "--output",
                    str(output),
                ]
            )
        self.assertEqual(code, 2)
        self.assertEqual(json.loads(output.read_text())["status"], "invalid")


if __name__ == "__main__":
    unittest.main()
