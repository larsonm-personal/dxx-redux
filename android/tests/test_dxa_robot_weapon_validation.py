import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SOURCE = (REPO_ROOT / "d2/main/dxa_metadata_patch.cpp").read_text(encoding="utf-8")


def function_body(marker: str) -> str:
    start = SOURCE.index(marker)
    opening = SOURCE.index("{", start)
    depth = 1
    cursor = opening + 1
    while cursor < len(SOURCE) and depth:
        if SOURCE[cursor] == "{":
            depth += 1
        elif SOURCE[cursor] == "}":
            depth -= 1
        cursor += 1
    if depth:
        raise AssertionError(f"unterminated block after {marker}")
    return SOURCE[opening + 1 : cursor - 1]


class DxaRobotWeaponValidationTest(unittest.TestCase):
    def test_weapon_fields_use_loaded_table_domains(self) -> None:
        body = function_body("void apply_robot_field")
        self.assertIn(
            'field == "WeaponType")\n\t\trobot.weapon_type = static_cast<sbyte>('
            'required_int_value(value, field.c_str(), 0, N_weapon_types - 1))',
            body,
        )
        self.assertIn(
            'field == "WeaponType2")\n\t\trobot.weapon_type2 = static_cast<sbyte>('
            'required_int_value(value, field.c_str(), -1, N_weapon_types - 1))',
            body,
        )
        self.assertNotIn(
            'robot.weapon_type = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255))',
            body,
        )
        self.assertNotIn(
            'robot.weapon_type2 = static_cast<sbyte>(required_int_value(value, field.c_str(), 0, 255))',
            body,
        )


if __name__ == "__main__":
    unittest.main()
