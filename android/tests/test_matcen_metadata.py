"""Asset-free negative controls for the historical matcen metadata comparator."""
import copy
import importlib.util
from pathlib import Path
import unittest

path = Path(__file__).parents[1] / "helpers" / "compare_matcen_metadata.py"
spec = importlib.util.spec_from_file_location("compare_matcen_metadata", path)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


def fixture():
    record = {"ordinal": 0, "segment": -1, "station": 9, "robot_flags": [5, 0], "hit_points": 12, "interval": 34}
    return {"schema_version": 1, "capture_status": "complete", "admission": "loaded", "generated_count": 1,
            "disk_count": 1, "declared_disk_count": 1,
            "authored_segments": [{"segment": 3, "generated_ordinal": 0, "serialized_matcen": 7, "serialized_station": 9}],
            "disk_records": [record], "triggers": [{"index": 2, "links": [{"segment": 3}, {"segment": 3}, {"segment": 5}]}],
            "runtime_bindings": [{"segment": 3, "generated_ordinal": 0, "effective_ordinal": 0, "segment_station": 2,
                                  "binding_status": "disk_bound", "station_status": "matching_station", "station": {"index": 2, "segment": 3, "type": 4},
                                  "eligible_robots": [{"type": 0, "model": 1, "status": "valid_model"}, {"type": 2, "model": 3, "status": "valid_model"}],
                                  "effective_record": {**record, "station": 2}}]}


class MatcenMetadataTests(unittest.TestCase):
    def test_same_capture_and_redundant_backlink_normalization(self):
        a, b = fixture(), fixture()
        b["runtime_bindings"][0]["effective_record"]["segment"] = 3
        self.assertTrue(m.compare_capture(a, b)["defined_bindings_match"])

    def test_changed_defined_identities_and_robot_data_rejected(self):
        for field, value in (("effective_ordinal", 1), ("segment_station", 9), ("segment", 4), ("generated_ordinal", 1), ("binding_status", "logical_disk_unbound"), ("eligible_robots", [])):
            with self.subTest(field=field):
                b = fixture(); b["runtime_bindings"][0][field] = value
                with self.assertRaises(m.MatcenMismatch): m.compare_capture(fixture(), b)
        for field, value in (("robot_flags", [6, 0]), ("hit_points", 0), ("interval", 0)):
            b = fixture(); b["runtime_bindings"][0]["effective_record"][field] = value
            with self.assertRaises(m.MatcenMismatch): m.compare_capture(fixture(), b)
        b = fixture(); b["runtime_bindings"][0]["station"]["segment"] = 9
        with self.assertRaises(m.MatcenMismatch): m.compare_capture(fixture(), b)

    def test_ordered_trigger_target_and_duplicate_loss_rejected(self):
        for mutate in (lambda links: links.pop(0), lambda links: links.reverse(), lambda links: links[0].update(segment=8)):
            b = fixture(); mutate(b["triggers"][0]["links"])
            with self.assertRaises(m.MatcenMismatch): m.compare_capture(fixture(), b)

    def test_incomplete_rejected_and_empty_capture_fail(self):
        for mutate in (lambda b: b.update(admission="rejected_or_missing"), lambda b: b.update(capture_status="incomplete"), lambda b: b.update(runtime_bindings=[]), lambda b: b.update(disk_records=[]), lambda b: b.update(schema_version=True)):
            b = fixture(); mutate(b)
            with self.assertRaises(m.MatcenMismatch): m.compare_capture(fixture(), b)

    def test_unbound_globals_are_not_golden_robot_masks(self):
        a, b = fixture(), fixture()
        for value in (a, b): value["runtime_bindings"][0]["binding_status"] = "logical_disk_unbound"
        b["runtime_bindings"][0]["effective_record"]["robot_flags"] = [123, 0]
        result = m.compare_capture(a, b)
        self.assertEqual(len(result["unbound_requires_review"]), 1)

    def test_contradictory_join_and_duplicate_identity_rejected(self):
        b = fixture(); b["runtime_bindings"][0]["effective_record"]["station"] = 9
        with self.assertRaises(m.MatcenMismatch): m.compare_capture(fixture(), b)
        b = fixture(); b["generated_count"] = 2
        b["runtime_bindings"] *= 2; b["authored_segments"] *= 2
        with self.assertRaises(m.MatcenMismatch): m.compare_capture(b, b)

    def test_missing_reference_or_actual_is_not_success(self):
        with self.assertRaises(m.MatcenMismatch): m.compare_documents({"game": "d1", "levels": []}, {"game": "d1", "levels": []})
        reference = {"game": "d1", "mission_filename": "m.msn", "levels": [{"level_num": 1, "level_file": "l.rdl", "matcen_regression": fixture()}]}
        with self.assertRaises(m.MatcenMismatch): m.compare_documents(reference, {"game": "d1", "levels": []})
        wrong_game = copy.deepcopy(reference); wrong_game["game"] = "d2"
        with self.assertRaises(m.MatcenMismatch): m.compare_documents(reference, wrong_game)
        actual = copy.deepcopy(reference); actual["levels"][0].pop("matcen_regression")
        with self.assertRaises(m.MatcenMismatch): m.compare_documents(reference, actual)


if __name__ == "__main__": unittest.main()
