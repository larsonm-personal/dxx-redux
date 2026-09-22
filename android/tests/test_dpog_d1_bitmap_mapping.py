import re
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
CUSTOM_SOURCE = (REPO_ROOT / "d2/main/d1_in_d2/d1_custom.c").read_text(encoding="utf-8")
BITMAP_SOURCE = (REPO_ROOT / "d2/main/d1_in_d2/d1_in_d2_bitmaps.c").read_text(encoding="utf-8")
LIFECYCLE_SOURCE = (REPO_ROOT / "d2/main/d1_in_d2/d1_in_d2.c").read_text(encoding="utf-8")


def function_body(source: str, name: str) -> str:
    match = re.search(rf"\b{name}\s*\([^;]*?\)\s*\{{", source, re.DOTALL)
    if not match:
        raise AssertionError(f"function not found: {name}")
    depth = 1
    cursor = match.end()
    while cursor < len(source) and depth:
        if source[cursor] == "{":
            depth += 1
        elif source[cursor] == "}":
            depth -= 1
        cursor += 1
    if depth:
        raise AssertionError(f"unterminated function: {name}")
    return source[match.end() : cursor - 1]


class DpogD1BitmapMappingTest(unittest.TestCase):
    def test_explicit_dpog_index_is_authoritative(self) -> None:
        # Behavioral layout/precedence coverage lives in test_upstream_compat
        # Keep this guard against reintroducing the legacy/live namespace
        body = function_body(CUSTOM_SOURCE, "d1_custom_read_file")
        self.assertNotIn("d2_index_for_d1_index", body)
        self.assertNotIn("hashtable_search", CUSTOM_SOURCE)
        self.assertNotIn("GameBitmaps", CUSTOM_SOURCE)

    def test_resolver_rejects_every_unmapped_namespace_edge(self) -> None:
        body = function_body(BITMAP_SOURCE, "d2_index_for_d1_index")
        d1_read = body.index("d1_tmap_nums[d1_index]")

        self.assertLess(body.index("d1_index < 0"), d1_read)
        self.assertLess(body.index("d1_index >= D1_MAX_TMAP_NUM"), d1_read)
        self.assertLess(body.index("!d1_tmap_nums"), d1_read)
        self.assertIn("d1_tmap_nums[d1_index] < 0", body)
        self.assertIn("d2_tmap_num >= NumTextures", body)
        self.assertIn("d2_bitmap_index >= MAX_BITMAP_FILES", body)

    def test_custom_preparation_precedes_generation_publication(self) -> None:
        preparation = function_body(LIFECYCLE_SOURCE, "prepare_assets")
        self.assertLess(preparation.index("d1_custom_read_assets("), preparation.index("d1_in_d2_publish_assets("))
        self.assertNotIn("Bitmap_original", CUSTOM_SOURCE)
        self.assertNotIn("Sound_original", CUSTOM_SOURCE)


if __name__ == "__main__":
    unittest.main()
