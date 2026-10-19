"""Asset-free negative controls for the historical matcen metadata comparator."""

import copy
import importlib.util
import json
from pathlib import Path
import unittest

path = Path(__file__).parents[1] / "helpers" / "compare_matcen_metadata.py"
spec = importlib.util.spec_from_file_location("compare_matcen_metadata", path)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
normalizer_spec = importlib.util.spec_from_file_location("normalize_json", path.with_name("normalize_json.py"))
normalizer = importlib.util.module_from_spec(normalizer_spec)
normalizer_spec.loader.exec_module(normalizer)


def fixture():
    record = {"ordinal": 0, "segment": -1, "station": 9, "robot_flags": [5, 0], "hit_points": 12, "interval": 34}
    return {
        "schema_version": 2,
        "capture_status": "complete",
        "admission": "loaded",
        "declared_disk_count": 1,
        "disk_records": [record],
        "robot_models": [
            {"type": 0, "model": 1, "status": "valid_model"},
            {"type": 2, "model": 3, "status": "valid_model"},
        ],
        "triggers": [{"index": 2, "links": [{"segment": 3}, {"segment": 3}, {"segment": 5}]}],
        "bindings": [
            {
                "segment": 3,
                "generated_ordinal": 0,
                "serialized_matcen": 7,
                "serialized_station": 9,
                "effective_ordinal": 0,
                "segment_station": 2,
                "binding_status": "disk_bound",
                "station_status": "matching_station",
                "record_overrides": {"station": 2},
            }
        ],
    }


class MatcenMetadataTests(unittest.TestCase):
    def test_publication_preserves_compact_matcen_rows_and_values(self):
        value = {"levels": [{"label": 'escaped " brace } and newline\n', "matcen_regression": fixture()}]}
        pretty = json.dumps(value, indent=2)
        compact = normalizer.format_json_text(pretty, False, mission_metadata=True)
        self.assertEqual(json.loads(compact), value)
        self.assertEqual(normalizer.format_json_text(compact, False, mission_metadata=True), compact)
        self.assertLess(len(compact.splitlines()), len(pretty.splitlines()) // 2)
        self.assertIn('"robot_flags": [5, 0]', compact)
        self.assertIn('"links": [{"segment": 3}, {"segment": 3}, {"segment": 5}]', compact)
        self.assertEqual(normalizer.format_json_text(pretty, False), pretty + "\n")

    def test_same_capture_and_redundant_backlink_normalization(self):
        a, b = fixture(), fixture()
        b["bindings"][0]["record_overrides"]["segment"] = 3
        self.assertTrue(m.compare_capture(a, b)["defined_bindings_match"])

    def test_changed_defined_identities_and_robot_data_rejected(self):
        for field, value in (
            ("effective_ordinal", 1),
            ("segment_station", 9),
            ("segment", 4),
            ("generated_ordinal", 1),
            ("binding_status", "logical_disk_unbound"),
        ):
            with self.subTest(field=field):
                b = fixture()
                b["bindings"][0][field] = value
                with self.assertRaises(m.MatcenMismatch):
                    m.compare_capture(fixture(), b)
        for field, value in (("robot_flags", [6, 0]), ("hit_points", 0), ("interval", 0)):
            b = fixture()
            b["bindings"][0]["record_overrides"][field] = value
            with self.assertRaises(m.MatcenMismatch):
                m.compare_capture(fixture(), b)
        b = fixture()
        b["bindings"][0]["station_overrides"] = {"segment": 9}
        with self.assertRaises(m.MatcenMismatch):
            m.compare_capture(fixture(), b)

    def test_shared_model_table_order_is_not_binding_order(self):
        a, b = fixture(), fixture()
        b["robot_models"].reverse()
        self.assertTrue(m.compare_capture(a, b)["defined_bindings_match"])

    def test_changed_or_missing_shared_robot_model_rejected(self):
        for models in ([], [{"type": 0, "model": 99, "status": "valid_model"}]):
            b = fixture()
            b["robot_models"] = models
            with self.assertRaises(m.MatcenMismatch):
                m.compare_capture(fixture(), b)

    def test_v1_metadata_is_not_accepted(self):
        b = fixture()
        b["schema_version"] = 1
        with self.assertRaises(m.MatcenMismatch):
            m.compare_capture(b, b)

    def test_ordered_trigger_target_and_duplicate_loss_rejected(self):
        for mutate in (
            lambda links: links.pop(0),
            lambda links: links.reverse(),
            lambda links: links[0].update(segment=8),
        ):
            b = fixture()
            mutate(b["triggers"][0]["links"])
            with self.assertRaises(m.MatcenMismatch):
                m.compare_capture(fixture(), b)

    def test_incomplete_rejected_and_empty_capture_fail(self):
        for mutate in (
            lambda b: b.update(admission="rejected_or_missing"),
            lambda b: b.update(capture_status="incomplete"),
            lambda b: b.update(bindings=[]),
            lambda b: b.update(disk_records=[]),
            lambda b: b.update(schema_version=True),
        ):
            b = fixture()
            mutate(b)
            with self.assertRaises(m.MatcenMismatch):
                m.compare_capture(fixture(), b)

    def test_unbound_globals_are_not_golden_robot_masks(self):
        a, b = fixture(), fixture()
        for value in (a, b):
            value["bindings"][0]["binding_status"] = "logical_disk_unbound"
        b["bindings"][0]["record_overrides"]["robot_flags"] = [123, 0]
        result = m.compare_capture(a, b)
        self.assertEqual(len(result["unbound_requires_review"]), 1)

    def test_unsafe_station_remains_review_required(self):
        a, b = fixture(), fixture()
        for value in (a, b):
            value["bindings"][0]["station_status"] = "wrong_station_type"
            value["bindings"][0]["station_overrides"] = {"type": 1}
        b["robot_models"][0]["model"] = 99
        self.assertEqual(1, len(m.compare_capture(a, b)["unbound_requires_review"]))

    def test_unrelated_shared_models_do_not_change_defined_binding(self):
        a, b = fixture(), fixture()
        b["robot_models"].append({"type": 9, "model": 99, "status": "valid_model"})
        self.assertTrue(m.compare_capture(a, b)["defined_bindings_match"])

    def test_contradictory_join_and_duplicate_identity_rejected(self):
        b = fixture()
        b["bindings"][0]["station_overrides"] = {"index": 9}
        with self.assertRaises(m.MatcenMismatch):
            m.compare_capture(fixture(), b)
        b = fixture()
        b["bindings"] *= 2
        with self.assertRaises(m.MatcenMismatch):
            m.compare_capture(b, b)

    def test_missing_reference_or_actual_is_not_success(self):
        with self.assertRaises(m.MatcenMismatch):
            m.compare_documents({"game": "d1", "levels": []}, {"game": "d1", "levels": []})
        reference = {
            "game": "d1",
            "mission_filename": "m.msn",
            "levels": [{"level_num": 1, "level_file": "l.rdl", "matcen_regression": fixture()}],
        }
        with self.assertRaises(m.MatcenMismatch):
            m.compare_documents(reference, {"game": "d1", "levels": []})
        wrong_game = copy.deepcopy(reference)
        wrong_game["game"] = "d2"
        with self.assertRaises(m.MatcenMismatch):
            m.compare_documents(reference, wrong_game)
        actual = copy.deepcopy(reference)
        actual["levels"][0].pop("matcen_regression")
        with self.assertRaises(m.MatcenMismatch):
            m.compare_documents(reference, actual)


if __name__ == "__main__":
    unittest.main()
