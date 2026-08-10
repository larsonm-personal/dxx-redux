import re
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
CUSTOM_SOURCE = (REPO_ROOT / "d2/main/d1_custom.c").read_text(encoding="utf-8")
PIGGY_SOURCE = (REPO_ROOT / "d2/main/piggy.c").read_text(encoding="utf-8")
GAMESEQ_SOURCE = (REPO_ROOT / "d2/main/gameseq.c").read_text(encoding="utf-8")


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
        body = function_body(CUSTOM_SOURCE, "d1_custom_load_pog_data")
        resolution = body[body.index("if (has_repl)") : body.index("bitmap_info[i].offset")]

        self.assertIn("repl_idx = d2_index_for_d1_index(indices[i])", resolution)
        self.assertRegex(
            resolution,
            re.compile(r"if \(has_repl\).*?else \{.*?hashtable_search", re.DOTALL),
        )
        self.assertNotIn("repl_idx = indices[i]", resolution)

    def test_resolver_rejects_every_unmapped_namespace_edge(self) -> None:
        body = function_body(PIGGY_SOURCE, "d2_index_for_d1_index")
        d1_read = body.index("d1_tmap_nums[d1_index]")

        self.assertLess(body.index("d1_index < 0"), d1_read)
        self.assertLess(body.index("d1_index >= D1_MAX_TMAP_NUM"), d1_read)
        self.assertLess(body.index("!d1_tmap_nums"), d1_read)
        self.assertIn("d1_tmap_nums[d1_index] < 0", body)
        self.assertIn("d2_tmap_num >= NumTextures", body)
        self.assertIn("d2_bitmap_index >= MAX_BITMAP_FILES", body)

    def test_mapping_is_ready_before_custom_load_and_teardown_uses_resolved_slot(self) -> None:
        level_load = function_body(GAMESEQ_SOURCE, "LoadLevel")
        self.assertLess(level_load.index("load_d1_bitmap_replacements()"), level_load.index("d1_custom_load_data(level_name)"))

        commit = function_body(CUSTOM_SOURCE, "d1_custom_commit_bitmap")
        remove = function_body(CUSTOM_SOURCE, "d1_custom_remove")
        self.assertIn("d1_custom_save_original(info->repl_idx)", commit)
        self.assertIn("*bmp = Bitmap_original[i]", remove)
        self.assertIn("piggy_bitmap_set_file_state(i, Bitmap_original_offset[i]", remove)


if __name__ == "__main__":
    unittest.main()
