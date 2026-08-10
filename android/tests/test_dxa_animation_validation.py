import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SOURCE = (REPO_ROOT / "d2/main/dxa_metadata_patch.cpp").read_text(encoding="utf-8")


def block_after(marker: str) -> str:
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


class DxaAnimationValidationTest(unittest.TestCase):
    def test_validators_reject_nonprogressing_records(self) -> None:
        vclip = block_after("void validate_vclip_record")
        self.assertIn("clip.num_frames < 1", vclip)
        self.assertIn("clip.num_frames > VCLIP_MAX_FRAMES", vclip)
        self.assertIn("clip.play_time <= 0 || clip.frame_time <= 0", vclip)
        self.assertIn("clip.play_time / clip.num_frames <= 0", vclip)

        effect = block_after("void validate_eclip_record")
        self.assertIn("effect.vc.num_frames < 1", effect)
        self.assertIn("effect.vc.num_frames > VCLIP_MAX_FRAMES", effect)
        self.assertIn("effect.vc.play_time <= 0 || effect.vc.frame_time <= 0", effect)
        self.assertIn("effect.vc.play_time / effect.vc.num_frames <= 0", effect)
        self.assertIn("effect.time_left < 0", effect)
        self.assertIn("effect.frame_count < 0", effect)
        self.assertIn("effect.frame_count >= effect.vc.num_frames", effect)

        wall = block_after("void validate_wclip_record")
        self.assertIn("wall.num_frames < 1", wall)
        self.assertIn("wall.num_frames > MAX_CLIP_FRAMES", wall)
        self.assertIn("wall.play_time <= 0", wall)
        self.assertIn("wall.play_time / wall.num_frames <= 0", wall)

    def test_full_rows_are_staged_and_validate_frame_arrays(self) -> None:
        cases = (
            ("void apply_vclip", "vclip clip = Vclip[index]", "validate_vclip_record", "Vclip[index] = clip"),
            ("void apply_eclip", "eclip effect = Effects[index]", "validate_eclip_record", "Effects[index] = effect"),
            ("void apply_wclip", "wclip wall = WallAnims[index]", "validate_wclip_record", "WallAnims[index] = wall"),
        )
        for marker, staging, validation, commit in cases:
            with self.subTest(marker=marker):
                body = block_after(marker)
                self.assertIn(staging, body)
                self.assertIn('value.at("Frames")', body)
                self.assertLess(body.index(validation), body.index(commit))

        bitmap_frames = block_after("void read_bitmap_frames")
        self.assertIn("value.size() > static_cast<size_t>(count)", bitmap_frames)
        wall_frames = block_after("void read_wclip_frames")
        self.assertIn("value.size() > MAX_CLIP_FRAMES", wall_frames)

    def test_field_updates_validate_the_complete_staged_record(self) -> None:
        cases = (
            ("void apply_eclip_field", "eclip effect = Effects[index]", "validate_eclip_record(effect)", "Effects[index] = effect"),
            ("void apply_wclip_field", "wclip wall = WallAnims[index]", "validate_wclip_record(wall)", "WallAnims[index] = wall"),
        )
        for marker, staging, validation, commit in cases:
            with self.subTest(marker=marker):
                body = block_after(marker)
                self.assertIn(staging, body)
                self.assertLess(body.index(validation), body.index(commit))

        eclip = block_after("void apply_eclip_field")
        self.assertIn('required_int_value(value, field.c_str(), 1, 0x40000000)', eclip)
        wall = block_after("void apply_wclip_field")
        self.assertIn('required_int_value(value, field.c_str(), 1, 0x40000000)', wall)


if __name__ == "__main__":
    unittest.main()
