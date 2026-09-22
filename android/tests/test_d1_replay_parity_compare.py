"""Fail-closed tests for the paired replay evidence comparator."""

import copy
import argparse
import contextlib
import io
import json
import tempfile
import unittest
from unittest import mock
from pathlib import Path

import d1_replay_parity as parity


class ParityTests(unittest.TestCase):
    def setUp(self):
        scratch = Path(__file__).resolve().parents[2] / "temp"
        scratch.mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="d1-parity-unit-", dir=scratch)
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.header = {"game": "d1", "mission": "d1", "level": 14, "difficulty": 0,
                       "frame_count": 2, "start_mode": "save_checkpoint"}
        self.result = {**self.header, "version": 2, "player0": {"shields": 30},
                       "position": {"x": 10}, "level_summary": {"endlevel_completed": True}}

    def trace(self, name, engine="d1"):
        meta = {"type": "meta", **self.header}
        # Both binaries emit input_demo_replay_game/mission in trace metadata
        rows = [meta] + [{"type": "frame_state", "f": i, "ft": 100,
                          "state": {"x": i}, "rng": {"s": 7}, "diag": {"objects": i}}
                         for i in range(2)]
        return self.write(name, rows)

    def write(self, name, rows):
        path = self.root / name
        path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")
        return path

    def mutate(self, path, operation):
        rows = list(parity.records(path))
        operation(rows)
        self.write(path.name, rows)

    def test_explicit_identity_mapping_preserves_all_gameplay(self):
        imported = copy.deepcopy(self.result)
        imported.update(game="d2", mission="descent")
        self.assertEqual(parity.canonical_result(imported, self.header, "d2"), self.result)
        self.assertEqual(imported["game"], "d2")
        imported["mission"] = "counterstrike"
        with self.assertRaises(parity.EvidenceError):
            parity.canonical_result(imported, self.header, "d2")

    def test_full_terminal_comparison_never_substitutes_player_values(self):
        demo = {"header": self.header, "result": self.result}
        actual = copy.deepcopy(self.result)
        actual["player0"]["shields"] = 29
        path = self.root / "actual.json"
        path.write_text(json.dumps(actual))
        report = parity.compare_pair(demo, {"state": self.root / "missing", "rng": self.root / "missing"},
                                     {"result": path, "state": self.root / "missing", "rng": self.root / "missing"},
                                     "d1", recorded=True)
        self.assertEqual(report["checks"]["terminal_result"]["status"], "fail")
        self.assertEqual(report["checks"]["frames"]["status"], "incomplete")
        self.assertEqual(self.result["player0"]["shields"], 30)

    def test_first_frame_mismatch_and_full_lengths(self):
        a, b = self.trace("a"), self.trace("b", "d2")
        self.assertEqual(parity.compare_frames(a, b, self.header, "d2")["status"], "pass")
        self.mutate(b, lambda rows: rows[1]["state"].update(x=7))
        report = parity.compare_frames(a, b, self.header, "d2")
        self.assertEqual(report["frames_compared"], 2)
        self.assertEqual(report["first_differences"]["state"]["frame"], 0)
        self.mutate(b, lambda rows: rows.pop())
        with self.assertRaises(parity.EvidenceError):
            parity.compare_frames(a, b, self.header, "d2")

    def test_wrong_identity_missing_fields_and_extra_frames_fail(self):
        a = self.trace("a")
        for mutation in (lambda rows: rows[0].update(mission="wrong"),
                         lambda rows: rows[1].pop("diag"),
                         lambda rows: rows.append({**rows[-1], "f": 2})):
            b = self.trace("b")
            self.mutate(b, mutation)
            with self.assertRaises(parity.EvidenceError):
                parity.compare_frames(a, b, self.header, "d1")

    def test_all_diagnostics_remain_visible(self):
        a, b = self.trace("a"), self.trace("b")
        self.mutate(b, lambda rows: rows[1]["diag"].update(new_unmapped_field=42))
        report = parity.compare_frames(a, b, self.header, "d1")
        self.assertEqual(report["status"], "fail")
        self.assertIn("diag", report["first_differences"])

    def test_early_diagnostic_difference_does_not_mask_later_fields(self):
        a, b = self.trace("a"), self.trace("b")
        for path in (a, b):
            self.mutate(path, lambda rows: [row["diag"].update(angle=[1, 2, 3]) for row in rows[1:]])
        self.mutate(b, lambda rows: rows[1]["diag"].update(objects=42))
        self.mutate(b, lambda rows: rows[2]["diag"].update(angle=[1, 2, 4], extra=0))
        report = parity.compare_frames(a, b, self.header, "d2")
        self.assertEqual(report["first_differences"]["diag"]["frame"], 0)
        angle = report["diagnostics"]["angle"]
        self.assertEqual(angle["frames_compared"], 2)
        self.assertEqual(angle["frames_different"], 1)
        self.assertEqual(angle["first_difference"]["frame"], 1)
        self.assertEqual(angle["first_difference"]["path"], "$.diag.angle[2]")
        self.assertEqual(report["diagnostics"]["extra"]["first_difference"]["expected"], "<missing>")
        self.assertEqual(report["status"], "fail")

    def test_null_diagnostics_are_not_evidence(self):
        a, b = self.trace("a"), self.trace("b")
        for path in (a, b):
            self.mutate(path, lambda rows: rows[1].update(diag=None))
        with self.assertRaises(parity.EvidenceError):
            parity.compare_frames(a, b, self.header, "d1")

    def rng(self, name):
        return self.write(name, [{"type": "meta", "events": 2, "truncated": False},
                                 {"type": "rand", "seq": 0, "frame": 0, "gt": 100, "call_count": 1,
                                  "state_before": 7, "state_after": 8, "result": 1, "file": "native.c"},
                                 {"type": "rand", "seq": 1, "frame": 1, "gt": 200, "call_count": 2,
                                  "state_before": 8, "state_after": 9, "result": 2}])

    def object_trace(self, name):
        rows = [{"type": "meta", **self.header}]
        for i in range(2):
            rows.append({"type": "object_state", "version": 1, "f": i, "reset": i == 0,
                         "capacity": 10, "slots": {"3": {"type": 5, "id": 11, "signature": i + 7,
                         "orient": [0, 0, i], "weapon": {"hitobj_list": [0, i]}}},
                         "allocator": {"free": [1, 2]}, "segment_heads": [3],
                         "clock": {"game_time": i}, "rng": [{"state": i, "calls": i}]})
        return self.write(name, rows)

    def test_object_deltas_preserve_every_field_and_do_not_mask_later_changes(self):
        a, b = self.object_trace("a"), self.object_trace("b")
        self.assertEqual(parity.compare_objects(a, b, self.header)["status"], "pass")
        self.mutate(b, lambda rows: rows[1]["slots"]["3"].update(extra_engine_field=0))
        self.mutate(b, lambda rows: rows[2]["slots"]["3"]["weapon"].update(hitobj_list=[0, 2]))
        diff = parity.compare_objects(a, b, self.header)["first_field_differences"]
        self.assertEqual(diff["$.object.extra_engine_field"]["frame"], 0)
        self.assertEqual(diff["$.object.weapon.hitobj_list[1]"]["frame"], 1)
        self.assertEqual(diff["$.object.weapon.hitobj_list[1]"]["slot"], 3)
        self.mutate(b, lambda rows: rows[2]["slots"].update({"3": None}))
        self.assertIn("$.object", parity.compare_objects(a, b, self.header)["first_field_differences"])

    def test_object_evidence_requires_complete_ordered_deltas(self):
        for mutation in (lambda rows: rows.pop(), lambda rows: rows[1].update(reset=False),
                         lambda rows: rows[2].update(f=3), lambda rows: rows[2].update(capacity=11),
                         lambda rows: rows[1]["slots"].update({"10": {}}),
                         lambda rows: rows[1]["slots"].update({"2": None}),
                         lambda rows: rows[2].pop("rng")):
            path = self.object_trace("bad")
            self.mutate(path, mutation)
            with self.assertRaises(parity.EvidenceError):
                list(parity.object_states(path, self.header))

    def test_object_arrays_retain_numeric_types(self):
        diff = list(parity.field_differences({"v": [1]}, {"v": [True]}))
        self.assertEqual(diff[0]["path"], "$.v[0]")

    def test_rng_labels_are_mapped_but_order_values_and_counts_are_not(self):
        a, b = self.rng("a"), self.rng("b")
        self.mutate(b, lambda rows: rows[1].update(file="imported.c", func="ported", line=22, stream=0))
        self.assertEqual(parity.compare_rng(a, b)["status"], "pass")
        self.mutate(b, lambda rows: rows[2].update(result=3))
        self.assertEqual(parity.compare_rng(a, b)["first_difference"]["event"], 1)
        self.mutate(b, lambda rows: rows[0].update(truncated=True))
        with self.assertRaises(parity.EvidenceError):
            parity.compare_rng(a, b)

    def test_missing_reference_and_corrupt_json_are_not_passes(self):
        missing = self.root / "missing"
        self.assertEqual(parity.safe_check(lambda: parity.compare_rng(missing, missing))["status"], "incomplete")
        bad = self.root / "broken"
        bad.write_text("{broken}")
        self.assertEqual(parity.safe_check(lambda: list(parity.records(bad)))["status"], "incomplete")

    def test_rng_context_does_not_mask_a_later_value_difference(self):
        a, b = self.rng("a"), self.rng("b")
        self.mutate(b, lambda rows: rows[1].update(ctx_obj=2, ctx_sig=3, ctx_id=4))
        report = parity.compare_rng(a, b)
        self.assertEqual(report["status"], "fail")
        self.assertIsNone(report["first_value_difference"])
        self.mutate(b, lambda rows: rows[2].update(result=3))
        self.assertEqual(parity.compare_rng(a, b)["first_value_difference"]["event"], 1)

    def test_missing_rng_values_and_unknown_streams_are_incomplete(self):
        for mutation in (lambda rows: rows[1].pop("result"),
                         lambda rows: rows[1].update(stream=2)):
            path = self.rng("rng")
            self.mutate(path, mutation)
            with self.assertRaises(parity.EvidenceError):
                list(parity.rng_events(path))

    def test_orchestration_attempts_every_capture_and_archives_failures(self):
        data = self.root / "data"
        data.mkdir()
        for name in ("descent.hog", "descent.pig"):
            (data / name).write_bytes(b"isolated fixture")
        args = argparse.Namespace(repo=Path(__file__).resolve().parents[2], data=data,
                                  output=self.root / "output", native=__file__, imported=__file__,
                                  demo=[self.root / "one.dximdemo", self.root / "two.dximdemo"])
        demo = {"header": self.header, "result": self.result}
        with mock.patch.object(parity, "read_demo", return_value=demo), \
             mock.patch.object(parity, "capture", side_effect=parity.EvidenceError("timeout")) as capture, \
             contextlib.redirect_stdout(io.StringIO()):
            self.assertNotEqual(parity.run(args), 0)
        self.assertEqual(capture.call_count, 6)
        report = json.loads((args.output / "report.json").read_text())
        self.assertEqual(len(report["cases"]), 2)
        self.assertTrue(all(len(case["capture_errors"]) == 3 for case in report["cases"]))
        self.assertEqual(report["qualification"], "incomplete")


if __name__ == "__main__":
    unittest.main()
