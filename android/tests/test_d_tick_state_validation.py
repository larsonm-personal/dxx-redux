import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


class DTickStateValidationTest(unittest.TestCase):
    def test_paired_validators_cover_serialized_domains(self) -> None:
        for game in ("d1", "d2"):
            source = (REPO_ROOT / game / "main/game.c").read_text(encoding="utf-8")
            validator = source[source.index("int game_d_tick_state_is_valid") :]

            self.assertIn("state->count >= 0", validator)
            self.assertIn("state->count <= GAME_D_TICK_COUNT_MAX", validator)
            self.assertIn("state->step == 0 || state->step == 1", validator)
            self.assertIn("state->timer >= 0", validator)
            self.assertIn("state->timer <= GAME_D_TICK_TIMER_MAX", validator)

    def test_paired_preflight_validates_before_runtime_publication(self) -> None:
        for game in ("d1", "d2"):
            source = (REPO_ROOT / game / "main/state.c").read_text(encoding="utf-8")
            preflight = source.index("static int state_validate_runtime_state")
            validation = source.index("game_d_tick_state_is_valid", preflight)
            publication = source.index("game_set_d_tick_state", validation)

            self.assertLess(validation, publication)

    def test_translation_validates_before_runtime_publication(self) -> None:
        source = (REPO_ROOT / "d2/main/d1_save_translate.c").read_text(
            encoding="utf-8"
        )
        validation = source.index("game_d_tick_state_is_valid")
        publication = source.index("game_set_d_tick_state", validation)

        self.assertLess(validation, publication)

    def test_paired_cleanup_indices_are_unsigned_and_bounded(self) -> None:
        for game in ("d1", "d2"):
            source = (REPO_ROOT / game / "main/wall.c").read_text(encoding="utf-8")
            self.assertIn(
                "(unsigned int)d_tick_count % MAX_STUCK_OBJECTS", source
            )


if __name__ == "__main__":
    unittest.main()
