import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SOURCE = (REPO_ROOT / "d2/main/d1_in_d2/d1_save_translate.c").read_text(encoding="utf-8")


class D1SaveTranslateWeaponValidationTest(unittest.TestCase):
    def test_translated_epoch_is_validated_before_player_decode(self) -> None:
        start = SOURCE.index("int d1_save_translate_read_checkpoint_start(")
        body = SOURCE[start : SOURCE.index("static int d1_save_translate_read_object_movement", start)]
        legacy_clock = body.index("d1_save_translate_read_fix(&reader, &start->game_time)")
        epoch_version = body.index("if (version >= SAVE_EPOCH_D1_VERSION)")
        epoch_read = body.index("d1_save_translate_read_fix64(&reader, &start->saved_epoch)")
        epoch_validation = body.index("save_epoch_valid(start->saved_epoch)")
        player = body.index("d1_save_translate_read_player(&reader, start)")
        objects = body.index("d1_save_translate_read_player_object_pose(&reader, start)")

        self.assertLess(legacy_clock, epoch_version)
        self.assertLess(epoch_version, epoch_read)
        self.assertLess(epoch_read, epoch_validation)
        self.assertLess(epoch_validation, player)
        self.assertLess(player, objects)
        self.assertIn("start->has_saved_epoch = 1", body)
        self.assertNotIn("GameTime64 =", SOURCE)

    def test_translator_rejects_android_layouts(self) -> None:
        self.assertIn("#define D1_SAVE_VERSION SAVE_EPOCH_D1_VERSION", SOURCE)
        self.assertIn("version < D1_SAVE_COMPATIBLE_VERSION || version > D1_SAVE_VERSION", SOURCE)
        self.assertIn("version == 20 || version == 22", SOURCE)
        header = (REPO_ROOT / "android/app/src/main/cpp/shared/save_epoch_format.h").read_text(encoding="utf-8")
        self.assertIn("#define SAVE_EPOCH_D1_VERSION 23", header)

    def test_replay_retains_legacy_clock_and_external_checkpoint_epoch(self) -> None:
        header = (REPO_ROOT / "d2/main/d1_in_d2/d1_save_translate.h").read_text(encoding="utf-8")
        replay = (REPO_ROOT / "android/app/src/main/cpp/shared/input_demo_start_shared.c").read_text(encoding="utf-8")
        self.assertIn("fix game_time;", header)
        self.assertIn("fix64 saved_epoch;", header)
        self.assertIn("GameTime64 = (fix64) d1_checkpoint.game_time +", replay)
        self.assertNotIn("d1_checkpoint.saved_epoch", replay)

    def test_translated_active_boss_timestamp_cannot_become_inactive_zero(self) -> None:
        start = SOURCE.index("static int d1_save_translate_read_d1_ai_state(")
        body = SOURCE[start : SOURCE.index("static int d1_save_translate_validate_d1_ai_state", start)]
        rebase = body.index("state->boss_dying_start_time = time_delta ? GameTime64 + (fix64) time_delta : 0;")
        active_zero = body.index("if (time_delta && !state->boss_dying_start_time)")
        sentinel = body.index("state->boss_dying_start_time = -1;")
        dying_flag = body.index("d1_save_translate_read_s32(reader, &state->boss_dying)")

        self.assertLess(rebase, active_zero)
        self.assertLess(active_zero, sentinel)
        self.assertLess(sentinel, dying_flag)

    def test_translated_weapon_selectors_use_d1_domains(self) -> None:
        marker = "!d1_save_translate_read_s8(&reader, &start->primary_weapon)"
        start = SOURCE.index(marker)
        validation = SOURCE.index("if (start->primary_weapon < 0", start)
        pose_read = SOURCE.index("d1_save_translate_read_player_object_pose", start)
        block = SOURCE[validation:pose_read]

        self.assertIn("start->primary_weapon >= D1_SAVE_TRANSLATE_PRIMARY_WEAPONS", block)
        self.assertIn("start->secondary_weapon < 0", block)
        self.assertIn("start->secondary_weapon >= D1_SAVE_TRANSLATE_SECONDARY_WEAPONS", block)

    def test_translated_difficulty_uses_engine_domain(self) -> None:
        marker = "!d1_save_translate_read_s32(&reader, &start->difficulty)"
        start = SOURCE.index(marker)
        validation = SOURCE.index("if (start->primary_weapon < 0", start)
        history_read = SOURCE.index("if (version >= 13)", validation)
        block = SOURCE[validation:history_read]

        self.assertIn("start->difficulty < 0", block)
        self.assertIn("start->difficulty >= NDL", block)


if __name__ == "__main__":
    unittest.main()
