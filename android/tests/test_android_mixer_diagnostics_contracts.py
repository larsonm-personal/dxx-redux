import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SHARED_C = ROOT / "android/app/src/main/cpp/shared/android_audio_diagnostics.c"
SHARED_H = ROOT / "android/app/src/main/cpp/shared/android_audio_diagnostics.h"
MIXERS = [
    ROOT / "d1/arch/sdl/digi_mixer.c",
    ROOT / "d2/arch/sdl/digi_mixer.c",
]


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


class AndroidMixerDiagnosticsContracts(unittest.TestCase):
    def test_shared_owner_queries_and_logs_exact_fields(self):
        source = SHARED_C.read_text(encoding="utf-8")
        header = SHARED_H.read_text(encoding="utf-8")
        init_body = function_body(source, "androidaud_log_mixer_init")
        failure_body = function_body(source, "androidaud_log_mixer_open_failed")

        self.assertIn(
            "void androidaud_log_mixer_init(int requested_rate, int mixer_buffer_frames);",
            header,
        )
        self.assertIn(
            "void androidaud_log_mixer_open_failed(const char *error);", header
        )
        self.assertEqual(init_body.count("Mix_QuerySpec("), 1)
        self.assertIn(
            "Mix_OpenAudio ok: requested=%d actual=%d fmt=0x%04X ch=%d "
            "buf=%d (native_rate=%d)",
            init_body,
        )
        self.assertIn(
            "[audio] init: mixer_rate=%d actual_rate=%d fmt=0x%04X ch=%d "
            "buf_frames=%d",
            init_body,
        )
        self.assertIn("ERROR: Couldn't open audio: %s", failure_body)

    def test_paired_mixers_retain_only_compact_diagnostic_calls(self):
        for path in MIXERS:
            with self.subTest(path=path):
                source = path.read_text(encoding="utf-8")
                init_body = function_body(source, "digi_mixer_init")

                self.assertNotIn("<android/log.h>", source)
                self.assertNotIn("MIXLOG", source)
                self.assertNotIn("Mix_QuerySpec", init_body)
                self.assertNotRegex(init_body, r"actual_(freq|fmt|ch)")
                self.assertEqual(
                    init_body.count("androidaud_log_mixer_open_failed(SDL_GetError());"),
                    1,
                )
                self.assertRegex(
                    init_body,
                    r"androidaud_log_mixer_init\("
                    r"(?:digi_sample_rate|DIGI_MIXER_OUTPUT_RATE), SOUND_BUFFER_SIZE\);",
                )


if __name__ == "__main__":
    unittest.main()
