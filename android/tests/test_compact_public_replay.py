"""Compact admission and exact replay evidence checks, without game assets."""

import copy
import hashlib
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import compact_public_replay as compact


class CompactReplayTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.header = {
            "type": "header",
            "version": 4,
            "game": "d1",
            "mission": "d1",
            "build_number": 1,
            "git_version": "unit",
            "arch": "x86_64",
            "level": 1,
            "difficulty": 2,
            "start_mode": "new_level",
            "rng_mode": "lcg_state",
            "frame_count": 3,
        }
        self.result = {"game": "d1", "mission": "d1", "difficulty": 2, "frame_count": 3, "player0": {"energy": 99}}
        diag = {key: [0] * length if length else 0 for key, length in compact.parity.FRAME_DIAGNOSTIC_FIELDS.items()}
        self.rows = (
            [self.header]
            + [
                {
                    "type": "frame",
                    "f": index,
                    **({"ft": 1092} if index == 0 else {}),
                    "input": {"s": {"forward_thrust_time": 1092}},
                    "rng": {"s": 7, "c": index},
                    "state": {"position": {"x": index, "y": 0, "z": 0}},
                    "diag": copy.deepcopy(diag),
                }
                for index in range(3)
            ]
            + [{"type": "result", "result": self.result}]
        )
        self.provenance = {
            "source_revision": "a" * 40,
            "capture_executable_sha256": "b" * 64,
            "capture_source_sha256": "c" * 64,
            "asset_sha256": compact.pinned_assets("d1"),
        }
        self.full = self.root / "capture.dximdemo"
        self.rng = self.root / "capture.rngtrace.jsonl"
        self.demo = self.root / "compact.dximdemo"
        self.oracle = self.root / "compact.oracle.json"
        self.trace = self.root / "state.jsonl"
        self.actual = self.root / "actual.json"
        self.write(self.full, self.rows)
        self.events = [
            {"type": "meta", "events": 1, "truncated": False},
            {
                "type": "rand",
                "seq": 0,
                "stream": 0,
                "frame": 0,
                "gt": 1092,
                "call_count": 1,
                "state_before": 7,
                "state_after": 8,
                "result": 1,
            },
        ]
        self.write(self.rng, self.events)
        self.trace_rows = [{**self.header, "type": "meta", "diag_version": compact.parity.FRAME_DIAGNOSTIC_VERSION}] + [
            {**copy.deepcopy(row), "type": "frame_state"} for row in self.rows[1:-1]
        ]
        self.write(self.trace, self.trace_rows)
        self.actual.write_bytes(compact.canonical(self.result))

    def write(self, path, rows):
        path.write_bytes(b"".join(compact.canonical(row) + b"\n" for row in rows))

    def pack(self):
        return compact.pack(self.full, self.rng, self.demo, self.oracle, self.provenance)

    def test_round_trip_preserves_all_except_frame_observations(self):
        original = self.full.read_bytes()
        oracle = self.pack()
        packed = [json.loads(line) for line in self.demo.read_text().splitlines()]
        expected = [
            {key: value for key, value in row.items() if row["type"] != "frame" or key not in ("state", "diag")}
            for row in self.rows
        ]
        self.assertEqual(packed, expected)
        self.assertEqual(self.full.read_bytes(), original)
        self.assertEqual(oracle["full_recording_sha256"], hashlib.sha256(original).hexdigest())
        self.assertLessEqual(self.demo.stat().st_size + self.oracle.stat().st_size, 100000)
        self.assertEqual(
            compact.verify("d1", self.demo, self.oracle, self.trace, self.rng, self.actual),
            {"status": "pass", "frames_compared": 3, "simulation_events_compared": 1},
        )
        with self.assertRaises(compact.parity.EvidenceError):
            self.pack()

    def test_d2_public_identity_and_assets(self):
        self.header.update(game="d2", mission="d2demo")
        self.result.update(game="d2", mission="d2demo")
        self.provenance["asset_sha256"] = compact.pinned_assets("d2")
        self.write(self.full, self.rows)
        self.pack()
        header, oracle = compact.validate(self.demo, self.oracle)
        self.assertEqual(header["game"], "d2")
        self.assertIn("d2demo.dem", oracle["provenance"]["asset_sha256"])

    def test_only_reviewed_diagnostic_events_are_removed(self):
        event = {"kind": "probe_weapon_life", "objnum": 12, "lifeleft": 999}
        path_event = {"kind": "weapon_robot_path", "weapon_obj": 12, "robot_obj": 13}
        self.rows[1]["events"] = [event, path_event]
        self.write(self.full, self.rows)
        oracle = self.pack()
        self.assertEqual(
            oracle["removed_telemetry"],
            {
                "counts": {"probe_weapon_life": 1, "weapon_robot_path": 1},
                "events": 2,
                "sha256": compact.sha([{"f": 0, "event": event}, {"f": 0, "event": path_event}]),
                "compared_on_replay": False,
            },
        )
        compact.validate(self.demo, self.oracle)
        self.assertEqual(json.loads(self.demo.read_text().splitlines()[1])["events"], [])
        self.demo.unlink()
        self.oracle.unlink()
        for kind in ("direct_command", "unknown_probe", "weapon_robot_path_unreviewed"):
            self.rows[1]["events"] = [{"kind": kind}]
            self.write(self.full, self.rows)
            with self.subTest(kind=kind), self.assertRaises(compact.parity.EvidenceError):
                self.pack()

    def test_rejects_saved_nonstandard_and_malformed_recordings(self):
        mutations = (
            "save_mode",
            "save_field",
            "checkpoint",
            "nested_data",
            "extra_type",
            "duplicate_header",
            "early_result",
            "sequence",
            "empty",
            "too_many",
            "mission",
            "missing_diag",
            "invalid_diag",
            "unknown_frame",
            "missing_ft",
            "duplicate_json",
        )
        for mutation in mutations:
            rows = copy.deepcopy(self.rows)
            if mutation == "save_mode":
                rows[0]["start_mode"] = "save_checkpoint"
            elif mutation == "save_field":
                rows[0]["start_save"] = "old.sav"
            elif mutation == "checkpoint":
                rows.insert(1, {"type": "checkpoint", "data": "AAAA"})
            elif mutation == "nested_data":
                rows[1]["events"] = [{"payload": {"data": "AAAA", "encoding": "base64"}}]
            elif mutation == "extra_type":
                rows[1]["type"] = "world_state"
            elif mutation == "duplicate_header":
                rows.insert(1, rows[0])
            elif mutation == "early_result":
                rows[1], rows[-1] = rows[-1], rows[1]
            elif mutation == "sequence":
                rows[2]["f"] = 0
            elif mutation == "empty":
                rows[0]["frame_count"] = 0
            elif mutation == "too_many":
                rows[0]["frame_count"] = 1001
            elif mutation == "mission":
                rows[0]["mission"] = "retail"
            elif mutation == "missing_diag":
                rows[1]["diag"].pop(next(iter(rows[1]["diag"])))
            elif mutation == "invalid_diag":
                rows[1]["diag"][next(iter(rows[1]["diag"]))] = None
            elif mutation == "unknown_frame":
                rows[1]["payload"] = "unknown"
            elif mutation == "missing_ft":
                rows[1].pop("ft")
            self.write(self.full, rows)
            if mutation == "duplicate_json":
                self.full.write_bytes(self.full.read_bytes().replace(b'"version":4', b'"version":4,"version":4'))
            with self.subTest(mutation=mutation), self.assertRaises(compact.parity.EvidenceError):
                self.pack()
            self.assertFalse(self.demo.exists())

    def test_combined_size_limit_applies_to_pack_and_validate(self):
        with mock.patch.object(compact, "MAX_BYTES", 1), self.assertRaises(compact.parity.EvidenceError):
            self.pack()
        self.assertFalse(self.demo.exists())
        self.pack()
        self.oracle.write_bytes(self.oracle.read_bytes() + b" " * 100000)
        with self.assertRaises(compact.parity.EvidenceError):
            compact.validate(self.demo, self.oracle)

    def test_provenance_and_strict_oracle(self):
        self.provenance["asset_sha256"]["descent.hog"] = "0" * 64
        with self.assertRaises(compact.parity.EvidenceError):
            self.pack()
        self.provenance["asset_sha256"] = compact.pinned_assets("d1")
        expected = self.pack()
        for mutation in ("extra", "missing", "hash", "count", "type", "save", "asset"):
            changed = copy.deepcopy(expected)
            if mutation == "extra":
                changed["notes"] = "not hash-only"
            elif mutation == "missing":
                changed.pop("terminal_sha256")
            elif mutation == "hash":
                changed["frame_sha256"][0] = "not a digest"
            elif mutation == "count":
                changed["frame_sha256"].pop()
            elif mutation == "type":
                changed["simulation_rng_events"] = True
            elif mutation == "save":
                changed["provenance"]["checkpoint"] = "hidden"
            else:
                changed["provenance"]["asset_sha256"] = {}
            self.oracle.write_bytes(compact.canonical(changed))
            with self.subTest(mutation=mutation), self.assertRaises(compact.parity.EvidenceError):
                compact.validate(self.demo, self.oracle)

    def test_every_observation_and_terminal_are_exact(self):
        self.pack()
        for field in ("state", "rng", "diag", "ft", "missing", "extra"):
            rows = copy.deepcopy(self.trace_rows)
            if field == "state":
                rows[1][field]["position"]["x"] += 1
            elif field == "rng":
                rows[1][field]["s"] += 1
            elif field == "diag":
                rows[1][field][next(iter(rows[1][field]))] += 1
            elif field == "ft":
                rows[1][field] += 1
            elif field == "missing":
                rows.pop()
            else:
                rows.append(rows[-1])
            self.write(self.trace, rows)
            with self.subTest(field=field), self.assertRaises(compact.parity.EvidenceError):
                compact.verify("d1", self.demo, self.oracle, self.trace, self.rng, self.actual)
        self.write(self.trace, self.trace_rows)
        changed = copy.deepcopy(self.events)
        changed[1]["result"] += 1
        self.write(self.rng, changed)
        with self.assertRaises(compact.parity.EvidenceError):
            compact.verify("d1", self.demo, self.oracle, self.trace, self.rng, self.actual)
        self.write(self.rng, self.events)
        self.actual.write_bytes(compact.canonical({**self.result, "extra": 1}))
        with self.assertRaises(compact.parity.EvidenceError):
            compact.verify("d1", self.demo, self.oracle, self.trace, self.rng, self.actual)


if __name__ == "__main__":
    unittest.main()
