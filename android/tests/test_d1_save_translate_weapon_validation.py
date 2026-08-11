import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SOURCE = (REPO_ROOT / "d2/main/d1_save_translate.c").read_text(encoding="utf-8")


class D1SaveTranslateWeaponValidationTest(unittest.TestCase):
    def test_translated_weapon_selectors_use_d1_domains(self) -> None:
        marker = "!d1_save_translate_read_s8(&reader, &start->primary_weapon)"
        start = SOURCE.index(marker)
        validation = SOURCE.index("if (start->primary_weapon < 0", start)
        pose_read = SOURCE.index("d1_save_translate_read_player_object_pose", start)
        block = SOURCE[validation:pose_read]

        self.assertIn(
            "start->primary_weapon >= D1_SAVE_TRANSLATE_PRIMARY_WEAPONS", block
        )
        self.assertIn("start->secondary_weapon < 0", block)
        self.assertIn(
            "start->secondary_weapon >= D1_SAVE_TRANSLATE_SECONDARY_WEAPONS", block
        )

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
