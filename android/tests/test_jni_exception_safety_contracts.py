import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CPP = ROOT / "android/app/src/main/cpp"


def function(source: str, name: str, next_name: str) -> str:
    start = source.index(name)
    end = source.index(next_name, start)
    return source[start:end]


class JniExceptionSafetyContracts(unittest.TestCase):
    def test_midi_array_acquisition_and_publication_stop_on_failure(self) -> None:
        source = (CPP / "jni_midi_preview.c").read_text(encoding="utf-8")
        start = function(source, "MidiPreviewBridge_nativeStart", "MidiPreviewBridge_nativeStop")
        self.assertLess(start.index("GetByteArrayElements"), start.index("midi_preview_start"))
        self.assertIn("if (!data || (*env)->ExceptionCheck(env)) return JNI_FALSE;", start)
        read = function(source, "nativeReadHogEntry", "return result;")
        self.assertLess(read.index("NewByteArray"), read.index("SetByteArrayRegion"))
        self.assertIn("if (!result || (*env)->ExceptionCheck(env))", read)
        self.assertIn("if ((*env)->ExceptionCheck(env)) return NULL;", read)

    def test_preview_and_fingerprint_strings_use_owned_strict_conversion(self) -> None:
        for name in ("jni_cd_preview.c", "jni_fingerprint.c"):
            source = (CPP / name).read_text(encoding="utf-8")
            self.assertNotIn("GetStringUTFChars", source)
            self.assertNotIn("ReleaseStringUTFChars", source)
            self.assertIn("dxx_jni_string_to_utf8", source)
            self.assertIn("dxx_jni_string_from_utf8", source)

    def test_import_arrays_and_callbacks_are_initialized_transactionally(self) -> None:
        gog = (CPP / "extract/jni_gog_import.c").read_text(encoding="utf-8")
        self.assertIn("if (!result || (*env)->ExceptionCheck(env)) return NULL;", gog)
        self.assertIn("if (!init_gog_extract_ctx(env, progress, &ctx))", gog)
        log = function(gog, "static void launcher_log", "static void launcher_logf")
        self.assertLess(log.index("ExceptionCheck"), log.index("FindClass"))

        disc = (CPP / "extract/jni_disc_import.c").read_text(encoding="utf-8")
        init = function(disc, "static int init_extract_ctx", "static int extract_progress_cb")
        self.assertIn("if (!cls || (*env)->ExceptionCheck(env)) return 0;", init)
        self.assertIn("if (!ctx->on_progress || (*env)->ExceptionCheck(env)) return 0;", init)
        self.assertGreaterEqual(disc.count("if (!init_extract_ctx("), 5)

    def test_reconnect_preserves_pending_exception_and_checks_region_copy(self) -> None:
        source = (CPP / "shared/net/net_udp_reconnect_jni.c").read_text(encoding="utf-8")
        check = function(source, "static int android_jni_check", "static jbyteArray")
        self.assertNotIn("ExceptionClear", check)
        create = function(source, "static jbyteArray", "static int android_jni_copy_byte_array")
        self.assertIn("if (!result || (*env)->ExceptionCheck(env)) return NULL;", create)
        self.assertGreaterEqual(create.count("ExceptionCheck"), 2)

    def test_engine_publishes_activity_only_after_required_setup(self) -> None:
        source = (CPP / "jni_main.c").read_text(encoding="utf-8")
        start = function(source, "MainActivity_startGame", "LevelPreviewActivity_startLevelPreview")
        self.assertLess(start.index("android_cache_asset_manager"), start.index("NewGlobalRef"))
        self.assertLess(start.index("consumeResumeCallsign"), start.index("NewGlobalRef"))
        self.assertIn("if (!g_activity || (*env)->ExceptionCheck(env))", start)
        fatal = function(source, "void android_finish_and_exit", "nativeGetGameWidth")
        self.assertNotIn("ExceptionClear", fatal)
        self.assertIn("if ((*env)->ExceptionCheck(env)) goto fatal_exit;", fatal)

    def test_saf_checks_class_before_method_lookup(self) -> None:
        source = (CPP / "jni_saf.c").read_text(encoding="utf-8")
        class_lookup = source.index("GetObjectClass")
        class_check = source.index("if (!cls || (*env)->ExceptionCheck(env))", class_lookup)
        method_lookup = source.index("GetMethodID", class_check)
        self.assertLess(class_check, method_lookup)


if __name__ == "__main__":
    unittest.main()
