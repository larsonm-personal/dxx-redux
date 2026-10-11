"""Exact scoped comparison must preserve diagnostic failure and reject gameplay loss."""

import copy
import json
import shutil
import unittest

import compact_public_gameplay_parity as gameplay
import test_compact_public_replay as fixtures


class GameplayScopeTests(unittest.TestCase):
    def setUp(self):
        self.fixture = fixtures.CompactReplayTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        f = self.fixture
        self.native = {"state": f.trace, "rng": f.rng, "result": f.actual}
        self.imported = {key: f.root / ("imported-" + path.name) for key, path in self.native.items()}
        for key, path in self.imported.items():
            path.write_bytes(self.native[key].read_bytes())
        self.imported["result"].write_text(json.dumps({**f.result, "game": "d2", "mission": "descent"}))
        self.rows = copy.deepcopy(f.trace_rows)
        scalar = next(key for key, value in self.rows[1]["diag"].items() if type(value) is int)
        self.rows[1]["diag"][scalar] += 1
        f.write(self.imported["state"], self.rows)
        self.original = {
            key: {"status": "fail" if key in ("frames", "object_states", "world_states") else "pass"}
            for key in (
                "frames",
                "object_states",
                "world_states",
                "terminal_result",
                "simulation_rng",
                "checkpoint_collision_clock",
            )
        }
        self.original["frames"]["first_differences"] = {"diag": {"frame": 0, "path": "$.diag.signature"}}

    def compare(self):
        return gameplay.compare_traces(self.fixture.header, self.native, self.imported, self.original)

    def test_diagnostic_difference_is_preserved_as_unqualified(self):
        before = self.imported["state"].read_bytes()
        original = copy.deepcopy(self.original)
        result = self.compare()
        self.assertEqual(result["status"], "pass")
        self.assertEqual(result["qualification"], "incomplete")
        self.assertEqual(result["frames_compared"], 3)
        self.assertEqual(result["diagnostic_frames_different"], 1)
        self.assertEqual(result["not_qualified"], ["frame.diag", "object_states", "world_states"])
        self.assertEqual(result["original_comparison_statuses"]["frames"], "fail")
        self.assertEqual(result["original_frame_first_differences"], original["frames"]["first_differences"])
        self.assertEqual(self.original, original)
        self.assertEqual(self.imported["state"].read_bytes(), before)

    def test_gameplay_rng_terminal_and_missing_frames_fail(self):
        f = self.fixture
        for mutation in ("player", "rng", "missing", "diag_missing", "rng_event", "terminal"):
            rows = copy.deepcopy(self.rows)
            f.write(self.imported["rng"], f.events)
            self.imported["result"].write_text(json.dumps({**f.result, "game": "d2", "mission": "descent"}))
            if mutation == "player":
                rows[1]["state"]["player0"] = {"energy": 0}
            elif mutation == "rng":
                rows[1]["rng"]["s"] += 1
            elif mutation == "missing":
                rows.pop()
            elif mutation == "diag_missing":
                rows[1].pop("diag")
            elif mutation == "rng_event":
                events = copy.deepcopy(f.events)
                events[1]["result"] += 1
                f.write(self.imported["rng"], events)
            else:
                self.imported["result"].write_text(
                    json.dumps({**f.result, "game": "d2", "mission": "descent", "player0": {"energy": 1}})
                )
            f.write(self.imported["state"], rows)
            with self.subTest(mutation=mutation):
                result = self.compare()
                self.assertEqual(result["status"], "fail")
                self.assertIsNotNone(result["first_difference"])

    def test_launch_inventory_is_checked_and_original_report_untouched(self):
        f = self.fixture
        root = f.root / "paired"
        root.mkdir()
        original = {"demo": str(f.full), "native_to_imported": {"status": "fail", "checks": self.original}}
        report = root / "report.json"
        report.write_text(json.dumps({"schema": 1, "qualification": "incomplete", "cases": [original]}))
        launches = []
        for directory_name, paths in (("native-a", self.native), ("imported", self.imported)):
            directory = root / f.full.stem / directory_name
            directory.mkdir(parents=True)
            artifacts = {}
            for key, filename in (("state", "state.jsonl"), ("rng", "rng.jsonl"), ("result", "result.json")):
                target = directory / filename
                shutil.copyfile(paths[key], target)
                artifacts[key] = str(target)
            launch = directory / "launch.json"
            launch.write_text(json.dumps({"exit_code": 0, "artifacts": artifacts}))
            launches.append(launch)
        before = report.read_bytes()
        self.assertEqual(gameplay.compare_case(report, original)["status"], "pass")
        self.assertEqual(report.read_bytes(), before)
        baseline = json.loads(launches[1].read_text())
        for mutation in ("exit", "outside", "missing", "name"):
            changed = copy.deepcopy(baseline)
            if mutation == "exit":
                changed["exit_code"] = 1
            elif mutation == "outside":
                changed["artifacts"]["state"] = str(self.native["state"])
            elif mutation == "missing":
                changed["artifacts"].pop("rng")
            else:
                changed["artifacts"]["state"] = str(launches[1])
            launches[1].write_text(json.dumps(changed))
            with self.subTest(mutation=mutation), self.assertRaises(gameplay.parity.EvidenceError):
                gameplay.compare_case(report, original)


if __name__ == "__main__":
    unittest.main()
