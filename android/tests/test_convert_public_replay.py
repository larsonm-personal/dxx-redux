"""Bounded conversion candidates retain the original expected files byte-for-byte."""

import copy
import json
import unittest
from unittest import mock

import compact_public_replay as compact
import convert_public_replay as converter
import test_compact_public_replay as fixture_tests


class ConversionTests(unittest.TestCase):
    def setUp(self):
        self.fixture = fixture_tests.CompactReplayTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        fixture = self.fixture
        for row in fixture.rows[1:-1]:
            row["input"] = {"s": {"f": 1092}}
        fixture.write(fixture.full, fixture.rows)
        fixture.pack()
        fixture.write(
            fixture.trace,
            fixture.trace_rows
            + [{"type": "object_boundary", "phase": "terminal", "rng": [{"available": 1, "state": 8, "calls": 1}]}],
        )
        self.original_demo, self.original_oracle = fixture.demo.read_bytes(), fixture.oracle.read_bytes()

    def convert(self):
        f = self.fixture
        return converter.convert(f.demo, f.oracle, f.trace, f.rng, f.actual, f.root / "candidates", "a" * 40)

    def test_three_whole_recording_schedules_preserve_all_oracles(self):
        f = self.fixture
        report = self.convert()
        self.assertEqual([row["schedule"] for row in report["candidates"]], list(converter.SCHEDULES))
        self.assertFalse(report["historical_event_order_recovered"])
        self.assertEqual(report["status"], "unverified-candidates")
        for row in report["candidates"]:
            header, expected = compact.validate(row["demo"], f.oracle)
            self.assertEqual(header["version"], 5)
            self.assertEqual(expected, compact._json(self.original_oracle))
            compact.verify("d1", row["demo"], f.oracle, f.trace, f.rng, f.actual)
        self.assertEqual(f.demo.read_bytes(), self.original_demo)
        self.assertEqual(f.oracle.read_bytes(), self.original_oracle)
        with self.assertRaisesRegex(compact.parity.EvidenceError, "new output"):
            self.convert()

    def test_baseline_mismatch_creates_no_candidates(self):
        self.fixture.actual.write_text('{"game":"d1","mission":"d1","difficulty":2,"frame_count":3}')
        with self.assertRaises(compact.parity.EvidenceError):
            self.convert()
        self.assertFalse((self.fixture.root / "candidates").exists())
        self.assertEqual(self.fixture.oracle.read_bytes(), self.original_oracle)

    def test_changed_oracle_or_input_breaks_conversion_binding(self):
        from pathlib import Path

        f = self.fixture
        row = self.convert()["candidates"][0]
        candidate = Path(row["demo"])
        original = candidate.read_bytes()
        candidate.write_bytes(original + b"\n")
        with self.assertRaisesRegex(compact.parity.EvidenceError, "conversion provenance"):
            compact.validate(candidate, f.oracle)
        candidate.write_bytes(original)
        f.oracle.write_bytes(self.original_oracle + b"\n")
        with self.assertRaisesRegex(compact.parity.EvidenceError, "conversion provenance"):
            compact.validate(candidate, f.oracle)

    def test_size_ineligible_is_separate_from_semantic_failure(self):
        f = self.fixture
        _, rows = compact._records(f.demo, False)
        size = len(
            b"".join(
                compact.canonical(row) + b"\n"
                for row in converter.candidate_rows(rows, "sample-only", {"s": 8, "c": 1})
            )
        )
        # Keep old admission unconstrained, but apply a smaller candidate-output cap
        real_validate = compact.validate

        def validate(*args):
            with mock.patch.object(compact, "MAX_BYTES", 100000):
                return real_validate(*args)

        with (
            mock.patch.object(compact, "validate", side_effect=validate),
            mock.patch.object(compact, "MAX_BYTES", size),
        ):
            report = self.convert()
        self.assertEqual(len(report["candidates"]), 1)
        self.assertEqual(len(report["size_ineligible"]), 2)
        self.assertTrue(all(row["semantic_verdict"] == "not-run" for row in report["size_ineligible"]))

    def test_schedule_never_searches_per_frame_and_preserves_tail(self):
        _, rows = compact._records(self.fixture.demo, False)
        rows[1]["input"] = {"s": {"f2s": 1}, "p": {"f2": 1}}
        rows[2]["input"] = {"s": {"f2s": 0}}
        rows[3]["input"] = {}
        original = copy.deepcopy(rows)
        output = converter.candidate_rows(rows, "prior-held-then-current", {"s": 8, "c": 1})
        self.assertEqual([op["k"] for op in output[2]["dispatch"]["ops"]], ["sample", "reset", "sample"])
        self.assertEqual(output[-2]["dispatch"]["ops"], [])
        self.assertNotIn("f2", output[1]["input"].get("p", {}))
        self.assertEqual(rows, original)
        for schedule in ("guess", ""):
            with self.assertRaises(compact.parity.EvidenceError):
                converter.candidate_rows(rows, schedule, {"s": 8, "c": 1})


if __name__ == "__main__":
    unittest.main()
