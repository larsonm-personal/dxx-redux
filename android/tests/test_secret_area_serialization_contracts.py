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
    def test_shared_adapter_owns_versioned_runtime_layout(self):
        source = ADAPTER.read_text(encoding="utf-8")
        writer = function_body(source, "secret_area_write_runtime_state")
        reader = function_body(source, "secret_area_read_runtime_state")
        validator = function_body(source, "secret_area_validate_runtime_state")
        self.assertIn("secret_area_encode_saved_state", writer)
        self.assertIn("secret_area_runtime_write(fp, data, 1, sizeof(data))", writer)
        self.assertIn("secret_area_decode_saved_state", validator)
        self.assertIn("secret_area_decode_saved_state", reader)
        self.assertIn("saved.level_identity != Secret_area_level_identity", reader)
        self.assertIn("saved.count = 0", reader)
        self.assertIn("secret_area_restore_identities", reader)
        self.assertIn("secret_area_runtime_read_sxe32(fp, swap)", reader)
        self.assertRegex(reader, r"secret_area_runtime_read\(fp, found, sizeof\(found\[0\]\),\s*SECRET_AREA_MAX_GENERATED\)")
        self.assertIn("secret_area_restore_saved_found", reader)
        legacy = function_body(source, "secret_area_restore_saved_found")
        self.assertIn("secret_area_restore_found_from_visited", legacy)
        self.assertNotIn("secret_area_restore_found(", legacy)

    def test_identity_codec_is_bounded_and_explicitly_ordered(self):
        source = (ADAPTER.parent / "secret_area_scan.c").read_text(encoding="utf-8")
        writer = function_body(source, "secret_area_encode_saved_state")
        self.assertIn("save_unsigned(data, (unsigned int) count, 4)", writer)
        self.assertIn("save_unsigned(data + 4, level_identity, 8)", writer)
        self.assertIn("data + 12 + 9 * i", writer)
        reader = function_body(source, "secret_area_decode_saved_state")
        for token in ("size != SECRET_AREA_IDENTITY_SAVE_SIZE", "count > SECRET_AREA_MAX_GENERATED",
                      "result.identities[i] == result.identities[j]", "result.found[i] > 1"):
            self.assertIn(token, reader)
        self.assertLess(reader.index("result.found[i] > 1"), reader.index("*saved = result"))

    def test_shared_header_and_paired_state_call_sites_match(self):
        header_text = HEADER.read_text(encoding="utf-8")
        self.assertIn(
            "void secret_area_write_runtime_state(rewind_file *fp);", header_text
        )
        self.assertIn(
            "void secret_area_read_runtime_state(rewind_file *fp, int swap, int has_identities);",
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
                validator = function_body(source, "state_validate_runtime_state")
                self.assertIn("version >= STATE_SECRET_AREA_IDENTITY_VERSION", validator)
                self.assertIn("secret_area_validate_runtime_state(fp)", validator)
                self.assertIn("sizeof(int) + SECRET_AREA_MAX_GENERATED", validator)
                self.assertRegex(
                    source,
                    r"if \(version >= STATE_SECRET_AREA_RUNTIME_VERSION\)\s*"
                    r"secret_area_read_runtime_state\(fp, swap, version >= STATE_SECRET_AREA_IDENTITY_VERSION\);",
                )


if __name__ == "__main__":
    unittest.main()
