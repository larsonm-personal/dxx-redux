import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ADAPTER = ROOT / "android/app/src/main/cpp/shared/secretarea.c"
STATE_FILES = [ROOT / "d1/main/state.c", ROOT / "d2/main/state.c"]
HEADER = ROOT / "android/app/src/main/cpp/shared/secretarea.h"


def function_body(source: str, name: str) -> str:
    match = re.search(rf"\b{name}\s*\([^;]*\)\s*\{{", source)
    if not match:
        raise AssertionError(f"missing function {name}")
    start = source.index("{", match.start())
    depth = 0
    for index in range(start, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError(f"unterminated function {name}")


class SecretAreaSerializationContracts(unittest.TestCase):
    def test_shared_adapter_owns_exact_runtime_layout(self):
        source = ADAPTER.read_text(encoding="utf-8")
        writer = function_body(source, "secret_area_write_runtime_state")
        reader = function_body(source, "secret_area_read_runtime_state")

        self.assertIn("int total = secret_area_total(&Secret_area_state);", writer)
        self.assertRegex(
            writer,
            r"secret_area_runtime_write\(fp, &total, sizeof\(total\), 1\);\s*"
            r"secret_area_runtime_write\(fp,\s*"
            r"total > 0 \? Secret_area_state\.found : empty_found,\s*"
            r"sizeof\(empty_found\[0\]\), SECRET_AREA_MAX_GENERATED\);",
        )
        self.assertIn(
            "int saved_total = secret_area_runtime_read_sxe32(fp, swap);", reader
        )
        self.assertRegex(
            reader,
            r"secret_area_runtime_read\(fp, found, sizeof\(found\[0\]\),\s*"
            r"SECRET_AREA_MAX_GENERATED\);",
        )
        self.assertIn("Highest_segment_index + 1", reader)
        self.assertIn("Automap_visited", reader)
        self.assertLess(
            reader.index("secret_area_runtime_read_sxe32"),
            reader.index("secret_area_runtime_read(fp"),
        )
        self.assertLess(
            reader.index("secret_area_runtime_read(fp"),
            reader.index("secret_area_restore_saved_found"),
        )

    def test_shared_header_and_paired_state_call_sites_match(self):
        header_text = HEADER.read_text(encoding="utf-8")
        self.assertIn(
            "void secret_area_write_runtime_state(rewind_file *fp);", header_text
        )
        self.assertIn(
            "void secret_area_read_runtime_state(rewind_file *fp, int swap);",
            header_text,
        )

        for path in STATE_FILES:
            with self.subTest(path=path):
                source = path.read_text(encoding="utf-8")
                writer = function_body(source, "state_write_runtime_state")
                self.assertNotIn("state_write_secret_area_runtime_state", source)
                self.assertNotIn("state_read_secret_area_runtime_state", source)
                self.assertEqual(writer.count("secret_area_write_runtime_state(fp);"), 1)
                self.assertLess(
                    writer.index("state_write_effect_runtime_state"),
                    writer.index("secret_area_write_runtime_state"),
                )
                self.assertRegex(
                    source,
                    r"if \(version >= STATE_SECRET_AREA_RUNTIME_VERSION\)\s*"
                    r"secret_area_read_runtime_state\(fp, swap\);",
                )


if __name__ == "__main__":
    unittest.main()
