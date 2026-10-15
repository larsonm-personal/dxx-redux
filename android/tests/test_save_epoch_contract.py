"""Source-contract checks for epoch precedence outside the host ordinary-load fixture.

These inspect production wiring. They do not claim runtime coverage of Android
rewind, input-demo replay, or the D2 secret-return transition.
"""
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
SHARED = ROOT / "android/app/src/main/cpp/shared"


class SaveEpochContractTest(unittest.TestCase):
    def test_new_versions_are_explicit(self):
        source = (SHARED / "save_epoch_format.h").read_text(encoding="utf-8")
        self.assertRegex(source, r"#define\s+SAVE_EPOCH_D1_VERSION\s+23\b")
        self.assertRegex(source, r"#define\s+SAVE_EPOCH_D2_VERSION\s+47\b")
        self.assertRegex(source, r"#define\s+SAVE_EPOCH_DISK_BYTES\s+8\b")
        self.assertIn("INT64_MAX / 128", source)
        self.assertIn("epoch >= 0 && epoch <= SAVE_EPOCH_MAX", source)

    def test_reader_checks_both_words_before_publishing(self):
        source = (SHARED / "save_epoch_runtime.h").read_text(encoding="utf-8")
        reader = source[source.index("static inline int save_epoch_read"):]
        self.assertIn("PHYSFS_read(fp, words, sizeof(words), 1) != 1", reader)
        self.assertIn("words[0] = SWAPINT(words[0])", reader)
        self.assertIn("words[1] = SWAPINT(words[1])", reader)
        self.assertLess(reader.index("value > SAVE_EPOCH_MAX"), reader.index("*epoch ="))
        self.assertNotIn("GameTime64 =", reader)

    def test_rewind_replay_then_saved_epoch_precedence(self):
        for game in ("d1", "d2"):
            with self.subTest(game=game):
                source = (ROOT / game / "main/state.c").read_text(encoding="utf-8")
                start = source.index("//Restore GameTime")
                end = source.index("// Start new game", start)
                selection = source[start:end]
                self.assertLess(selection.index("!save_epoch_read"), selection.index("GameTime64 ="))
                self.assertLess(selection.index("android_rewind_restore_game_time64_active()"),
                                selection.index("input_demo_replay_has_checkpoint()"))
                self.assertRegex(selection, r"android_rewind_restore_game_time64\(\);\s*else\s*\{")
                self.assertRegex(selection, r"GameTime64 = \(fix64\)tmptime32;\s*"
                                           r"if \(input_demo_replay_has_checkpoint\(\)\)\s*"
                                           r"GameTime64 \+= input_demo_replay_checkpoint_start_gt\(\);\s*else if")
                self.assertIn("GameTime64 = saved_epoch", selection)
                expected_guard = "version >= SAVE_EPOCH_D2_VERSION && !secret_restore" if game == "d2" else "version >= SAVE_EPOCH_D1_VERSION"
                self.assertIn("else if (" + expected_guard + ")", selection)
                # The header must reject before the restore marks the world replaced
                self.assertLess(selection.index("!save_epoch_read"), selection.index("*world_changed = 1"))

    def test_native_and_translated_boss_active_zero_collision_is_preserved(self):
        for game in ("d1", "d2"):
            with self.subTest(game=game):
                source = (ROOT / game / "main/ai.c").read_text(encoding="utf-8")
                self.assertRegex(source, r"Boss_dying_start_time = tmptime32 \? GameTime64 \+ \(fix64\)tmptime32 : 0;\s*"
                                         r"if \(tmptime32 && !Boss_dying_start_time\)\s*Boss_dying_start_time = -1;")
        translated = (ROOT / "d2/main/d1_in_d2/d1_save_translate.c").read_text(encoding="utf-8")
        self.assertRegex(translated, r"state->boss_dying_start_time = time_delta \? GameTime64 \+ \(fix64\) time_delta : 0;\s*"
                                    r"if \(time_delta && !state->boss_dying_start_time\)\s*state->boss_dying_start_time = -1;")

    def test_writers_keep_zero_legacy_clock_before_authoritative_epoch(self):
        for game in ("d1", "d2"):
            with self.subTest(game=game):
                source = (ROOT / game / "main/state.c").read_text(encoding="utf-8")
                start = source.rfind("//Save GameTime", 0, source.index("!save_epoch_write(fp, GameTime64)"))
                end = source.index("//Save player info", start)
                header = source[start:end]
                self.assertRegex(header, r"tmptime32 = 0;\s*"
                                         r"PHYSFS_write\(fp, &tmptime32, sizeof\(fix\), 1\);\s*"
                                         r"if \(!save_epoch_write\(fp, GameTime64\)\)")


if __name__ == "__main__":
    unittest.main()
