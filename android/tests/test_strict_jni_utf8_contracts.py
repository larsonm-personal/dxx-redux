import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SHARED = ROOT / "android/app/src/main/cpp/shared"


class StrictJniUtf8Contracts(unittest.TestCase):
    def test_shared_converter_uses_utf16_jni_apis_and_strict_codec(self) -> None:
        source = (SHARED / "jni_string.c").read_text(encoding="utf-8")
        self.assertIn("GetStringChars", source)
        self.assertIn("ReleaseStringChars", source)
        self.assertIn("dxx_utf16_to_utf8", source)
        self.assertIn("dxx_utf8_to_utf16", source)
        self.assertIn("(*env)->NewString(env", source)
        self.assertNotIn("GetStringUTFChars", source)
        self.assertNotIn("NewStringUTF", source)

    def test_saf_uri_conversion_is_strict_and_precedes_callback(self) -> None:
        source = (ROOT / "android/app/src/main/cpp/jni_saf.c").read_text(
            encoding="utf-8"
        )
        conversion = source.index("dxx_jni_string_from_utf8(env, content_uri)")
        callback = source.index("CallIntMethod", conversion)
        self.assertLess(conversion, callback)
        self.assertIn("if (!juri)", source[conversion:callback])
        self.assertIn("return -1;", source[conversion:callback])
        self.assertNotIn("NewStringUTF", source)

    def test_midi_paths_and_json_use_strict_shared_conversion(self) -> None:
        source = (ROOT / "android/app/src/main/cpp/jni_midi_preview.c").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("GetStringUTFChars", source)
        self.assertNotIn("ReleaseStringUTFChars", source)
        self.assertNotIn("NewStringUTF", source)
        self.assertEqual(source.count("dxx_jni_string_to_utf8"), 3)
        self.assertGreaterEqual(source.count("dxx_jni_string_from_utf8"), 2)
        self.assertLess(
            source.index("dxx_jni_string_to_utf8(env, jfilesDir"),
            source.index("midi_enumerate_tracks(files_dir)"),
        )
        self.assertLess(
            source.index("dxx_jni_string_to_utf8(env, jhogPath"),
            source.index("hog_read_entry(hog_path"),
        )


if __name__ == "__main__":
    unittest.main()
