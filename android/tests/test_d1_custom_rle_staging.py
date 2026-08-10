import re
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SOURCE = (REPO_ROOT / "d2/main/d1_custom.c").read_text(encoding="utf-8")


def function_body(name: str) -> str:
    match = re.search(rf"\b{name}\s*\([^;]*?\)\s*\{{", SOURCE, re.DOTALL)
    if not match:
        raise AssertionError(f"function not found: {name}")
    start = match.end()
    depth = 1
    cursor = start
    while cursor < len(SOURCE) and depth:
        if SOURCE[cursor] == "{":
            depth += 1
        elif SOURCE[cursor] == "}":
            depth -= 1
        cursor += 1
    if depth:
        raise AssertionError(f"unterminated function: {name}")
    return SOURCE[start : cursor - 1]


class D1CustomRleStagingTest(unittest.TestCase):
    def test_rle_validation_precedes_staged_ownership(self) -> None:
        body = function_body("d1_custom_stage_bitmap")
        self.assertIn("if (info->rle_big)", body)
        validate = body.index("bounded_rle_validate_bitmap")
        publish = body.index("info->data = data")
        self.assertLess(validate, publish)

    def test_complete_file_is_staged_before_any_bitmap_commit(self) -> None:
        for name in ("d1_custom_load_pog_data", "d1_custom_load_file"):
            body = function_body(name)
            stage = body.index("d1_custom_stage_bitmap")
            commit = body.index("d1_custom_commit_bitmap")
            self.assertLess(stage, commit)
            between = body[stage:commit]
            self.assertIn("d1_custom_free_staged_bitmaps", between)
            self.assertNotIn("GameBitmaps", between)

    def test_bitmap_offsets_use_checked_width(self) -> None:
        self.assertIn("PHYSFS_sint64 offset", SOURCE)
        stage = function_body("d1_custom_stage_bitmap")
        self.assertIn("data_size > file_len - info->offset", stage)
        self.assertNotIn("info->offset + data_size", stage)


if __name__ == "__main__":
    unittest.main()
