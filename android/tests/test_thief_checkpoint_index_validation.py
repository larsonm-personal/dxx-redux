import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SOURCE = (REPO_ROOT / "d2/main/escort.c").read_text(encoding="utf-8")


class ThiefCheckpointIndexValidationTest(unittest.TestCase):
    def test_slot_lookup_rejects_invalid_cursor_before_array_access(self) -> None:
        function = SOURCE.index("static int thief_find_stolen_item_slot")
        validation = SOURCE.index("thief_stolen_item_index_is_valid", function)
        direct_return = SOURCE.index("return Stolen_item_index", function)
        first_array_access = SOURCE.index("Stolen_items[slot]", function)

        self.assertLess(validation, direct_return)
        self.assertLess(validation, first_array_access)

    def test_checkpoint_restore_validates_before_publication(self) -> None:
        restore = SOURCE.index("if (have_checkpoint_thief_state)")
        validation = SOURCE.index("thief_stolen_item_index_is_valid", restore)
        assignment = SOURCE.index(
            "Stolen_item_index = checkpoint_thief_state.stolen_item_index", restore
        )

        self.assertLess(validation, assignment)

    def test_schema_and_engine_capacities_must_match(self) -> None:
        self.assertIn(
            "#if MAX_STOLEN_ITEMS != INPUT_DEMO_CHECKPOINT_STOLEN_ITEM_COUNT",
            SOURCE,
        )


if __name__ == "__main__":
    unittest.main()
