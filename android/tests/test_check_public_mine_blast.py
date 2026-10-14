"""Synthetic adversarial evidence for the live proximity-mine checker."""

import contextlib
import copy
import gzip
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import check_public_mine_blast as mine
import d1_replay_parity as parity
import ordered_public_replay as ordered


def example(schema):
    if schema is int:
        return 0
    if isinstance(schema, dict):
        return {key: example(value) for key, value in schema.items() if key != "*"}
    if isinstance(schema, tuple):
        return [example(value) for value in schema]
    return []


class MineTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.header = {
            "type": "header",
            "version": 5,
            "game": "d1",
            "mission": "d1",
            "level": 3,
            "difficulty": 0,
            "frame_count": 6,
            "start_mode": "new_level",
            "rng_mode": "lcg_state",
        }
        self.terminal = {
            "version": 2,
            **{k: self.header[k] for k in ("game", "mission", "level", "difficulty", "frame_count")},
            "game_time64": 24576,
            "player0": {
                k: 0
                for k in [
                    "energy",
                    "shields",
                    "score",
                    "lives",
                    "laser_level",
                    "primary_weapon",
                    "secondary_weapon",
                    "flags",
                    "hostages",
                ]
            },
            "position": {
                k: 0
                for k in [
                    "segment",
                    "x",
                    "y",
                    "z",
                    "forward_x",
                    "forward_y",
                    "forward_z",
                ]
            },
            "level_summary": {
                k: 0
                for k in [
                    "robots_alive",
                    "robots_killed",
                    "hostages_remaining",
                    "powerups_remaining",
                ]
            },
        }
        self.terminal["player0"].update(primary_ammo=[0] * 16, secondary_ammo=[0] * 16)
        self.terminal["level_summary"].update(control_center_destroyed=False, endlevel_completed=False)
        self.capture = {
            "frames": 6,
            "mine": 2,
            "mine_signature": 323,
            "mine_pickup": 1,
            "mine_pickup_frame": 1,
            "mine_drop_frame": 2,
            "request_frame": 3,
            "shot_frame": 3,
            "negative_no_missile": False,
            "initial_player": {"slot": 0, "signature": 1, "type": 4},
            "scenario": "mine-blast",
            "game": "d1",
            "damage_frame_scope": "first_any_player_damage",
            "observations": [],
        }
        self.mine_identity = {
            "slot": 2,
            "signature": 323,
            "type": 5,
            "id": 16,
            "flags": 0,
            "parent": {"type": 4, "slot": 0, "signature": 1, "creation_time": 8192},
        }
        for f in range(6):
            live = f >= 2
            self.capture["observations"].append(
                {
                    "f": f,
                    "mine_live": live,
                    "mine_identity": copy.deepcopy(self.mine_identity) if live else None,
                    "mine_position": [f + 1, 2, 3] if live else None,
                    "mine_velocity": [4, 5, 6] if live else None,
                }
            )
        self.frames = []
        for f in range(6):
            self.frames.append(
                {
                    "type": "frame",
                    "f": f,
                    "ft": 4096,
                    "input": {},
                    "rng": {"s": 1234, "c": 0},
                    "dispatch": {
                        "pre_rng": "=",
                        "ops": [
                            {
                                "k": "sample",
                                "ft": 4096,
                                "s": {"f2s": int(f == 3)},
                                "p": {"db": int(f == 2)},
                            }
                        ],
                    },
                    "state": {},
                    "diag": {k: [0] * n if n else 0 for k, n in parity.FRAME_DIAGNOSTIC_FIELDS.items()},
                }
            )
        self.tail = {
            "type": "input_tail",
            "f": 6,
            "input": {},
            "rng": {"s": 1234, "c": 0},
            "dispatch": {"pre_rng": "=", "ops": []},
        }
        self.capture_path = self.root / "capture.json"
        self.full_path = self.root / "capture.dximdemo.gz"
        self.compact_path = self.root / "probe.dximdemo"
        self.provenance_path = self.root / "provenance.json"
        self.native_path = self.root / "native.jsonl"
        self.imported_path = self.root / "imported.jsonl"
        self.rng_path = self.root / "rng.jsonl"
        self.rng_path.write_text(json.dumps({"type": "meta", "events": 0, "truncated": False}) + "\n")
        self.native_result = self.root / "native-result.json"
        self.imported_result = self.root / "imported-result.json"
        self.provenance = {
            "status": "synthetic unit evidence only",
            "engine_inputs_unchanged": True,
            "engine_source_revision": "a" * 40,
            "executable_sha256": "b" * 64,
        }
        self.write_sources()
        self.native_rows = self.trace()
        self.imported_rows = self.trace(0)
        self.write_jsonl(self.native_path, self.native_rows)
        self.write_jsonl(self.imported_path, self.imported_rows)
        self.native_result.write_text(json.dumps(self.terminal))
        self.imported_result.write_text(json.dumps({**self.terminal, "game": "d2", "mission": "descent"}))
        self.oracle = self.create()

    def write_sources(self):
        self.capture_path.write_text(json.dumps(self.capture))
        self.provenance_path.write_text(json.dumps(self.provenance))
        self.write_jsonl(
            self.full_path,
            [
                self.header,
                *self.frames,
                self.tail,
                {"type": "result", "result": self.terminal},
            ],
        )
        frames, tail = ordered.compact(self.header, self.frames, self.tail)
        for row in frames:
            row.pop("state", None)
            row.pop("diag", None)
        self.write_jsonl(
            self.compact_path,
            [self.header, *frames, tail, {"type": "result", "result": self.terminal}],
        )

    @staticmethod
    def write_jsonl(path, rows):
        raw = b"".join(mine.strict.encoded(row) for row in rows)
        path.write_bytes(gzip.compress(raw) if path.suffix == ".gz" else raw)

    def create(self):
        return mine.make_oracle(
            self.capture_path,
            self.full_path,
            self.compact_path,
            self.provenance_path,
            self.rng_path,
        )

    def object(self, signature, objtype):
        obj = example(parity.OBJECT_SCHEMA)
        obj.update(
            signature=signature,
            type=objtype,
            lifeleft=100000,
            movement_type=1,
            physics=example(parity.PHYSICS_SCHEMA),
        )
        return obj

    def storage(self, f, kind, parent_sig=1, phase=None):
        player = self.object(parent_sig, 4)
        player["control_type"] = 4
        objects = {"0": player}
        if phase != "restored" and f >= 2:
            weapon = self.object(323, 5)
            weapon.update(id=16, control_type=9, pos=[f, 2, 3])
            weapon["physics"]["velocity"] = [4, 5, 6]
            weapon["weapon"] = {
                key: 0
                for key in [
                    "parent_type",
                    "parent_num",
                    "parent_signature",
                    "creation_time",
                    "last_hitobj",
                    "track_goal",
                    "multiplier",
                    "creation_framecount",
                ]
            }
            weapon["weapon"].update(
                parent_type=4,
                parent_num=0,
                parent_signature=parent_sig,
                creation_time=8192,
                creation_framecount=2,
                hitobj_list=[-1] * 1000,
            )
            objects["2"] = weapon
        row = example(parity.OBJECT_STORAGE_SCHEMA)
        row.update(
            type=kind,
            version=2,
            f=f,
            reset=(kind == "object_boundary" or f == 0),
            capacity=1000,
            slots=objects,
        )
        row["allocator"].update(
            num_objects=len(objects),
            highest_object_index=2 if "2" in objects else 0,
            free_obj_list=list(range(1000)),
        )
        if phase:
            row["phase"] = phase
        return row

    def trace(self, signature=1):
        return [
            {
                "type": "meta",
                **{
                    k: self.header[k]
                    for k in (
                        "game",
                        "mission",
                        "level",
                        "difficulty",
                        "frame_count",
                        "start_mode",
                    )
                },
            },
            self.storage(0, "object_boundary", signature, "restored"),
            *[self.storage(f, "object_state", signature) for f in range(6)],
            *[{**copy.deepcopy(row), "type": "frame_state"} for row in self.frames],
            self.storage(6, "object_boundary", signature, "terminal"),
        ]

    def compare(self):
        return mine.compare_pair(
            self.oracle,
            self.compact_path,
            self.native_path,
            self.imported_path,
            self.native_result,
            self.imported_result,
            self.rng_path,
            self.rng_path,
        )

    def test_oracle_is_bounded_native_only_and_source_hashes_are_actual(self):
        self.assertLessEqual(len(mine.strict.encoded(self.oracle)), 100000)
        self.assertEqual(
            self.oracle["source"],
            {
                "capture_sha256": parity.digest(self.capture_path),
                "full_recording_sha256": parity.digest(self.full_path),
                "compact_sha256": parity.digest(self.compact_path),
                "capture_provenance_sha256": parity.digest(self.provenance_path),
                "capture_rng_sha256": parity.digest(self.rng_path),
            },
        )
        for name in ("initial_player", "damage_frame_scope"):
            broken = copy.deepcopy(self.capture)
            del broken[name]
            with self.subTest(name=name), self.assertRaises(parity.EvidenceError):
                mine.validate_capture(broken)
        broken = copy.deepcopy(self.capture)
        del broken["observations"][2]["mine_identity"]
        with self.assertRaises(parity.EvidenceError):
            mine.validate_capture(broken)

    def test_post_frame_alignment_and_terminal_boundary(self):
        report = self.compare()
        self.assertEqual(report["native"]["status"], "pass")
        self.assertEqual(report["native"]["samples_compared"], 4)
        self.assertEqual(report["imported"]["physical_trajectory"]["status"], "pass")
        self.assertEqual(report["imported"]["parent_binding"]["status"], "pass")
        self.assertEqual(report["status"], "fail")
        self.assertEqual(
            report["raw_mine"]["first_mismatch"]["path"],
            "$.mine.weapon.parent_signature",
        )
        self.assertEqual(report["imported"]["live_identity"]["first_mismatch"]["actual"], 0)
        self.imported_rows[-1]["slots"]["2"]["pos"][0] += 1
        self.write_jsonl(self.imported_path, self.imported_rows)
        report = self.compare()
        self.assertEqual(
            report["imported"]["physical_trajectory"]["first_mismatch"]["capture_frame"],
            5,
        )

    def test_summary_pass_does_not_hide_moved_mine(self):
        self.imported_rows[6]["slots"]["2"]["physics"]["velocity"] = [90, 91, 92]
        self.imported_rows[6]["slots"]["2"]["pos"][0] += 8
        self.write_jsonl(self.imported_path, self.imported_rows)
        report = self.compare()
        self.assertTrue(all(c["status"] == "pass" for c in report["summary"].values()))
        physical = report["imported"]["physical_trajectory"]
        self.assertEqual(physical["status"], "fail")
        self.assertEqual(physical["first_mismatch"]["trace_frame"], 4)
        self.assertEqual(physical["first_mismatch"]["capture_frame"], 3)
        self.assertEqual(report["status"], "fail")
        self.assertTrue(any(d["path"] == "$.mine.physics.velocity[0]" for d in report["raw_mine"]["differences"]))

    def test_no_concussion_control_preserves_physical_pass_and_raw_identity_fail(self):
        self.capture.update(negative_no_missile=True, shot_frame=-1)
        self.frames[3]["dispatch"]["ops"][0]["s"]["f2s"] = 0
        self.write_sources()
        self.oracle = self.create()
        report = self.compare()
        self.assertTrue(report["negative_no_missile"])
        self.assertEqual(report["imported"]["physical_trajectory"]["status"], "pass")
        self.assertEqual(report["raw_mine"]["status"], "fail")

    def test_missing_dead_reused_wrong_type_and_wrong_parent_rejected(self):
        mutations = [
            lambda row: row["slots"].__setitem__("2", None),
            lambda row: row["slots"]["2"].update(flags=2),
            lambda row: row["slots"]["2"].update(lifeleft=0),
            lambda row: row["slots"]["2"].update(signature=324),
            lambda row: row["slots"]["2"].update(id=17),
            lambda row: row["slots"]["2"]["weapon"].update(parent_signature=1),
            lambda row: row["slots"]["2"]["weapon"].update(parent_num=7),
            lambda row: row["slots"]["0"].update(type=5),
        ]
        for change in mutations:
            rows = copy.deepcopy(self.imported_rows)
            change(rows[6])
            self.write_jsonl(self.imported_path, rows)
            with self.subTest(change=change), self.assertRaises(parity.EvidenceError):
                self.compare()

    def test_missing_duplicate_out_of_order_frames_or_boundaries_rejected(self):
        versions = [
            self.imported_rows[:-1],
            self.imported_rows[0:1] + self.imported_rows[2:],
            self.imported_rows[:5] + self.imported_rows[6:],
            self.imported_rows[:5] + self.imported_rows[4:],
            [*self.imported_rows, self.imported_rows[-1]],
            [*self.imported_rows[:-2], self.imported_rows[-1], self.imported_rows[-2]],
        ]
        for rows in versions:
            self.write_jsonl(self.imported_path, rows)
            with (
                self.subTest(length=len(rows)),
                self.assertRaises(parity.EvidenceError),
            ):
                self.compare()

    def test_corrupt_ambiguous_and_truncated_jsonl_rejected(self):
        valid = self.imported_path.read_bytes()
        for suffix in (
            b'{"type":"bad", "type":"other"}\n',
            b'{"number":NaN}\n',
            b'{"number":1e999}\n',
            b"[]\n",
            b'{"unterminated":',
            b'{"type":"nonsense"}\n',
            b"{}\n",
            b'{"type":"result","result":{}}\n',
        ):
            self.imported_path.write_bytes(valid + suffix)
            with self.subTest(suffix=suffix), self.assertRaises(parity.EvidenceError):
                self.compare()
        path = self.root / "corrupt.jsonl.gz"
        path.write_bytes(gzip.compress(valid)[:-6])
        with self.assertRaises(parity.EvidenceError):
            mine.read_trace(path, self.oracle, "d2")

    def test_control_and_source_tampering_rejected(self):
        self.compact_path.write_bytes(self.compact_path.read_bytes() + b"\n")
        with self.assertRaisesRegex(parity.EvidenceError, "provenance"):
            self.compare()
        self.write_sources()
        rows = list(mine.strict.strict_records(self.compact_path))
        rows[1]["dispatch"]["ops"][0]["s"] = {"f": 1}
        self.write_jsonl(self.compact_path, rows)
        with self.assertRaisesRegex(parity.EvidenceError, "Compaction changed"):
            self.create()

    def test_invalid_live_lifetime_and_parent_rejected(self):
        for mutate in (
            lambda row: row.update(mine_live=False),
            lambda row: row["mine_identity"].update(signature=324),
            lambda row: row["mine_identity"].update(flags=2),
            lambda row: row["mine_identity"]["parent"].update(signature=0),
            lambda row: row["mine_position"].__setitem__(0, True),
        ):
            capture = copy.deepcopy(self.capture)
            mutate(capture["observations"][3])
            with self.assertRaises(parity.EvidenceError):
                mine.validate_capture(capture)

    def test_oracle_size_and_missing_result_fields_rejected(self):
        broken = copy.deepcopy(self.oracle)
        broken["provenance"]["status"] = "x" * 100000
        with self.assertRaisesRegex(parity.EvidenceError, "budget"):
            mine.validate_oracle(broken)
        self.imported_result.write_text(json.dumps({"game": "d2"}))
        with self.assertRaises(parity.EvidenceError):
            self.compare()

    def causal_fixture(self):
        self.frames[3]["input"] = {"s": {"f2s": 1}}
        self.frames[4]["input"] = {"s": {"f2s": 0}}
        self.write_sources()
        self.oracle = self.create()
        self.imported_rows[6]["slots"]["2"]["pos"] = [12, 10, 11]
        self.imported_rows[6]["slots"]["2"]["physics"]["velocity"] = [90, 91, 92]
        self.write_jsonl(self.imported_path, self.imported_rows)
        positive = self.compare()
        positive_oracle = self.oracle
        positive_demo = self.root / "positive.dximdemo"
        positive_demo.write_bytes(self.compact_path.read_bytes())
        self.capture.update(negative_no_missile=True, shot_frame=-1)
        self.frames[3]["input"] = {"s": {"f2s": 0}}
        self.frames[3]["dispatch"]["ops"][0]["s"]["f2s"] = 0
        self.write_sources()
        self.oracle = self.create()
        self.write_jsonl(self.imported_path, self.trace(0))
        negative = self.compare()
        signature = {
            "frames": 6,
            "pickup_frame": 1,
            "drop_frame": 2,
            "shot_frame": 3,
            "first_trace_frame": 4,
            "expected_position": [4, 2, 3],
            "actual_position": [12, 10, 11],
            "expected_velocity": [4, 5, 6],
            "actual_velocity": [90, 91, 92],
        }
        return [
            positive_oracle,
            positive_demo,
            positive,
            self.oracle,
            self.compact_path,
            negative,
        ], signature

    def test_exact_causal_gate_retains_failed_raw_verdicts(self):
        args, signature = self.causal_fixture()
        with mock.patch.dict(mine.KNOWN_MINE_FAULT, signature, clear=True):
            report = mine.causal_gate(*args)
        self.assertEqual(report["status"], "known_fault_reproduced", report["failures"])
        self.assertEqual(report["raw_parity_status"], {"positive": "fail", "negative": "fail"})
        self.assertEqual(len(report["controls"]["input_differences"]), 2)

    def test_causal_gate_rejects_absent_early_wrong_impulse_and_native_failures(self):
        args, signature = self.causal_fixture()
        changes = [
            lambda a: a[2]["imported"].update(physical_trajectory=mine.result([])),
            lambda a: a[2]["imported"]["physical_trajectory"]["differences"][0].update(trace_frame=3),
            lambda a: a[2]["imported"]["physical_trajectory"]["differences"][0].update(actual=13),
            lambda a: a[2]["native"]["physical_trajectory"].update(status="fail"),
            lambda a: a[5]["imported"]["physical_trajectory"].update(status="fail"),
            lambda a: a[2].pop("simulation_rng"),
            lambda a: a[2].update(frames_compared=5),
            lambda a: a[2]["simulation_rng"]["native"]["actual"].update(sha256="0" * 64),
            lambda a: a[2]["summary"]["imported"].update(status="fail"),
        ]
        for change in changes:
            broken = copy.deepcopy(args)
            change(broken)
            with (
                self.subTest(change=change),
                mock.patch.dict(mine.KNOWN_MINE_FAULT, signature, clear=True),
            ):
                self.assert_causal_rejected(broken)

    def assert_causal_rejected(self, args):
        try:
            report = mine.causal_gate(*args)
        except parity.EvidenceError:
            return
        self.assertEqual(report["status"], "fail")

    def test_causal_report_structure_rejects_forged_consistency_and_types(self):
        args, signature = self.causal_fixture()
        changes = [
            lambda r: r["imported"]["physical_trajectory"].update(mismatch_count=0, first_mismatch=None),
            lambda r: r["imported"]["physical_trajectory"].update(mismatch_count=True),
            lambda r: r["imported"]["physical_trajectory"]["differences"][0].update(trace_frame=True),
            lambda r: r.pop("trace_sha256"),
            lambda r: r["trace_sha256"].update(native="wrong"),
            lambda r: r.update(simulation_rng=None),
            lambda r: r["raw_simulation_rng"].update(
                status="fail",
                mismatch_count=-1,
                first_mismatch=None,
                differences=[{"path": "garbage.ctx_sig", "expected": 1, "actual": 0}],
            ),
            lambda r: r.update(
                raw_simulation_rng=mine.result([{"path": "garbage.ctx_sig", "expected": 1, "actual": 0}])
            ),
            lambda r: r.update(
                raw_simulation_rng=mine.result(
                    [
                        {
                            "path": "$.simulation_rng[0].ctx_sig",
                            "expected": 1,
                            "actual": 0,
                        }
                    ]
                )
            ),
            lambda r: r.update(raw_simulation_rng=mine.result([1])),
            lambda r: r["raw_mine"].update(first_mismatch=None),
        ]
        for change in changes:
            broken = copy.deepcopy(args)
            change(broken[2])
            with (
                self.subTest(change=change),
                mock.patch.dict(mine.KNOWN_MINE_FAULT, signature, clear=True),
                self.assertRaises(parity.EvidenceError),
            ):
                mine.causal_gate(*broken)

    def test_causal_gate_rejects_changed_native_recording_prefix(self):
        args, signature = self.causal_fixture()
        for field in ("full_frame_sha256", "gameplay_frame_sha256"):
            broken = copy.deepcopy(args)
            broken[0]["recording_checks"][field][0] = "0" * 64
            broken[2]["oracle_sha256"] = mine.sha(broken[0])
            with (
                self.subTest(field=field),
                mock.patch.dict(mine.KNOWN_MINE_FAULT, signature, clear=True),
            ):
                report = mine.causal_gate(*broken)
            self.assertEqual(report["status"], "fail")
            self.assertIn(
                "native recorded history differs before concussion: " + field,
                report["failures"],
            )

    def test_causal_gate_rejects_changed_preintervention_rng_prefix(self):
        args, signature = self.causal_fixture()
        rows = list(mine.strict.strict_records(args[1]))
        rows[2]["dispatch"]["pre_rng"] = {"s": 5678, "c": 0}
        self.write_jsonl(args[1], rows)
        args[0]["source"]["compact_sha256"] = parity.digest(args[1])
        args[0]["controls_sha256"] = mine.sha(mine.read_demo(args[1])["batches"])
        args[2]["oracle_sha256"] = mine.sha(args[0])
        args[2]["source"] = copy.deepcopy(args[0]["source"])
        with mock.patch.dict(mine.KNOWN_MINE_FAULT, signature, clear=True):
            report = mine.causal_gate(*args)
        self.assertEqual(report["status"], "fail")
        self.assertIn("native input RNG history differs before concussion", report["failures"])

    def test_cli_invalid_input_overwrites_stale_known_fault_output(self):
        args, signature = self.causal_fixture()
        paths = []
        for index, value in enumerate(args):
            if isinstance(value, Path):
                paths.append(value)
            else:
                path = self.root / f"causal-{index}.json"
                path.write_bytes(mine.strict.encoded(value))
                paths.append(path)
        output = self.root / "causal-output.json"
        flags = (
            "positive-oracle",
            "positive-demo",
            "positive-report",
            "negative-oracle",
            "negative-demo",
            "negative-report",
        )
        command = [
            "causal",
            *[arg for flag, path in zip(flags, paths) for arg in ("--" + flag, str(path))],
            "--output",
            str(output),
        ]
        for content in (
            '{"broken":',
            json.dumps({**args[2], "simulation_rng": None}),
            json.dumps({**args[2], "raw_simulation_rng": mine.result([1])}),
        ):
            paths[2].write_text(content)
            output.write_text('{"status":"known_fault_reproduced"}')
            stderr = io.StringIO()
            with (
                self.subTest(content=content[:40]),
                contextlib.redirect_stdout(io.StringIO()),
                contextlib.redirect_stderr(stderr),
                mock.patch.dict(mine.KNOWN_MINE_FAULT, signature, clear=True),
            ):
                self.assertEqual(mine.main(command), 2)
            invalid = json.loads(output.read_text())
            self.assertEqual(invalid["status"], "invalid")
            self.assertEqual(json.loads(stderr.getvalue())["status"], "invalid")
            self.assertNotIn("known_fault_reproduced", output.read_text())

    def test_causal_gate_rejects_unrelated_control_change(self):
        args, signature = self.causal_fixture()
        self.frames[4]["dispatch"]["ops"][0]["s"]["f"] = 1
        self.write_sources()
        args[3] = self.create()
        args[5]["oracle_sha256"] = mine.sha(args[3])
        args[5]["source"] = args[3]["source"]
        with mock.patch.dict(mine.KNOWN_MINE_FAULT, signature, clear=True):
            report = mine.causal_gate(*args)
        self.assertEqual(report["status"], "fail")
        self.assertIn(
            "inputs differ beyond withholding the declared concussion",
            report["failures"],
        )

    def test_reseed_broken_rng_chain_and_missing_gameplay_frame_rejected(self):
        event = {
            "type": "srand",
            "seq": 0,
            "frame": 1,
            "gt": 4096,
            "call_count": 1,
            "state_before": 1234,
            "state_after": 1234,
            "seed": 1234,
        }
        self.write_jsonl(self.rng_path, [{"type": "meta", "events": 1, "truncated": False}, event])
        with self.assertRaisesRegex(parity.EvidenceError, "reseeded"):
            mine.analyze_rng(self.rng_path, mine.read_demo(self.compact_path))
        event.update(type="rand", state_before=55, result=0)
        self.write_jsonl(self.rng_path, [{"type": "meta", "events": 1, "truncated": False}, event])
        with self.assertRaisesRegex(parity.EvidenceError, "chain"):
            mine.analyze_rng(self.rng_path, mine.read_demo(self.compact_path))
        self.write_jsonl(
            self.imported_path,
            [r for r in self.imported_rows if not (r["type"] == "frame_state" and r["f"] == 5)],
        )
        with self.assertRaisesRegex(parity.EvidenceError, "gameplay frames"):
            mine.read_trace(self.imported_path, self.oracle, "d2")


if __name__ == "__main__":
    unittest.main()
