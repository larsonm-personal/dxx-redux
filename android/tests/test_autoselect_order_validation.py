import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


class AutoselectOrderValidationTest(unittest.TestCase):
    def test_jni_validates_before_scanning_pilots(self) -> None:
        source = (
            REPO_ROOT / "android/app/src/main/cpp/android_autoselect.cpp"
        ).read_text(encoding="utf-8")
        writer = source.index("JNI_FUNC(nativeWriteAutoselect)")
        validation = source.index("read_valid_order", writer)
        scan = source.index("for_each_pilot", validation)

        self.assertLess(validation, scan)
        self.assertIn("raw[i] < 0 || raw[i] > 255", source)
        self.assertIn("weapon_order_is_valid", source)

    def test_paired_pilot_loaders_validate_both_orders(self) -> None:
        for game in ("d1", "d2"):
            source = (REPO_ROOT / game / "main/playsave.c").read_text(
                encoding="utf-8"
            )
            self.assertIn("weapon_order_is_valid(PlayerCfg.PrimaryOrder", source)
            self.assertIn("weapon_order_is_valid(PlayerCfg.SecondaryOrder", source)

    def test_d1_helper_requires_complete_lines(self) -> None:
        source = (REPO_ROOT / "d1/main/playsave.c").read_text(encoding="utf-8")
        helper = source[source.index("int plx_read_weapon_order") :]

        self.assertIn("if (n == 7", helper)
        self.assertIn("if (n == 6", helper)


if __name__ == "__main__":
    unittest.main()
