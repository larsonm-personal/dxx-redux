import re
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


class DxaHamPatchTransactionTest(unittest.TestCase):
    def test_snapshot_covers_every_mutated_table(self) -> None:
        constructor = block_after("ham_patch_snapshot()")
        restore = block_after("void restore() const")
        arrays = (
            ("textures", "Textures"),
            ("texture_info", "TmapInfo"),
            ("vclips", "Vclip"),
            ("effects", "Effects"),
            ("wall_anims", "WallAnims"),
            ("sounds", "Sounds"),
            ("alt_sounds", "AltSounds"),
            ("robots", "Robot_info"),
            ("weapons", "Weapon_info"),
            ("object_bitmaps", "ObjBitmaps"),
            ("object_bitmap_pointers", "ObjBitmapPtrs"),
        )
        for saved, live in arrays:
            self.assertIn(f"std::memcpy({saved}, {live}, sizeof({saved}))", constructor)
            self.assertIn(f"std::memcpy({live}, {saved}, sizeof({saved}))", restore)

        scalars = (
            ("texture_count", "NumTextures"),
            ("vclip_count", "Num_vclips"),
            ("effect_count", "Num_effects"),
            ("wall_anim_count", "Num_wall_anims"),
            ("max_virtual_bitmap_index", "g_max_virtual_bitmap_index"),
        )
        for saved, live in scalars:
            self.assertIn(f"{saved} = {live}", constructor)
            self.assertIn(f"{live} = {saved}", restore)

    def test_snapshot_precedes_first_mutation_and_failure_restores_it(self) -> None:
        apply_function = block_after('extern "C" void dxa_metadata_patch_apply_d2_ham(void)')
        snapshot = apply_function.index("snapshot = std::make_unique<ham_patch_snapshot>()")
        mutation_loop = apply_function.index("int applied = 0")
        apply_value = apply_function.index("apply_patch_value(path, *value)")
        restore = apply_function.index("snapshot->restore()")
        failure_log = apply_function.index("DXA metadata: failed to apply")

        self.assertLess(snapshot, mutation_loop)
        self.assertLess(mutation_loop, apply_value)
        self.assertLess(apply_value, restore)
        self.assertLess(restore, failure_log)
        self.assertRegex(
            apply_function[apply_value:restore],
            re.compile(r"catch\s*\(const std::exception &ex\)\s*\{\s*if \(snapshot\)", re.DOTALL),
        )


if __name__ == "__main__":
    unittest.main()
