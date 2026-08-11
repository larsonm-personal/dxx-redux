import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SOURCE = (
    REPO_ROOT / "android/app/src/main/cpp/shared/input_demo_start_shared.c"
).read_text(encoding="utf-8")


class InputDemoStartDifficultyValidationTest(unittest.TestCase):
    def test_new_level_revalidates_before_global_assignment(self) -> None:
        function = SOURCE.index("static int input_demo_start_replay_new_level")
        validation = SOURCE.index("input_demo_difficulty_is_valid", function)
        assignment = SOURCE.index("Difficulty_level = input_demo_replay_difficulty()", function)

        self.assertLess(validation, assignment)

    def test_translated_checkpoint_revalidates_before_global_assignment(self) -> None:
        parse = SOURCE.index("d1_save_translate_read_checkpoint_start")
        validation = SOURCE.index(
            "input_demo_difficulty_is_valid(d1_checkpoint.difficulty)", parse
        )
        assignment = SOURCE.index("Difficulty_level = d1_checkpoint.difficulty", parse)

        self.assertLess(validation, assignment)


if __name__ == "__main__":
    unittest.main()
