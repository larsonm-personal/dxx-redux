import re
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
D1_STATE = (REPO_ROOT / "d1/main/state.c").read_text(encoding="utf-8")
D2_STATE = (REPO_ROOT / "d2/main/state.c").read_text(encoding="utf-8")
D1_OBJECT = (REPO_ROOT / "d1/main/object.c").read_text(encoding="utf-8")
D2_OBJECT = (REPO_ROOT / "d2/main/object.c").read_text(encoding="utf-8")
D1_AI = (REPO_ROOT / "d1/main/ai.c").read_text(encoding="utf-8")
D2_AI = (REPO_ROOT / "d2/main/ai.c").read_text(encoding="utf-8")
TRANSLATOR = (REPO_ROOT / "d2/main/d1_save_translate.c").read_text(encoding="utf-8")


def function_body(source: str, name: str) -> str:
    match = re.search(rf"\b{name}\s*\([^;]*?\)\s*\{{", source, re.DOTALL)
    if not match:
        raise AssertionError(f"function not found: {name}")
    start = match.end()
    depth = 1
    cursor = start
    while cursor < len(source) and depth:
        if source[cursor] == "{":
            depth += 1
        elif source[cursor] == "}":
            depth -= 1
        cursor += 1
    if depth:
        raise AssertionError(f"unterminated function: {name}")
    return source[start : cursor - 1]


def allocator_valid(
    in_use: list[bool], num_objects: int, highest: int, free_tail: list[int]
) -> bool:
    live = [index for index, used in enumerate(in_use) if used]
    if num_objects != len(live) or highest != (live[-1] if live else -1):
        return False
    if len(free_tail) != len(in_use) - num_objects:
        return False
    return len(set(free_tail)) == len(free_tail) and set(free_tail) == {
        index for index, used in enumerate(in_use) if not used
    }


class SaveRuntimeValidationTest(unittest.TestCase):
    def test_allocator_partition_model_rejects_unsafe_states(self) -> None:
        in_use = [True, False, True, False]
        self.assertTrue(allocator_valid(in_use, 2, 2, [3, 1]))
        self.assertFalse(allocator_valid(in_use, -1, 2, [3, 1]))
        self.assertFalse(allocator_valid(in_use, 3, 2, [3]))
        self.assertFalse(allocator_valid(in_use, 2, 4, [3, 1]))
        self.assertFalse(allocator_valid(in_use, 2, 2, [1, 1]))
        self.assertFalse(allocator_valid(in_use, 2, 2, [3, 2]))

    def test_paired_allocator_validators_prove_the_complete_partition(self) -> None:
        required = (
            "state->num_objects != live_count",
            "state->highest_object_index != highest_live",
            "free_seen[objnum]",
            "Objects[objnum].type != OBJ_NONE",
            "(Objects[i].type == OBJ_NONE) != (free_seen[i] != 0)",
        )
        for source in (D1_OBJECT, D2_OBJECT):
            body = function_body(source, "object_validate_runtime_state")
            for text in required:
                self.assertIn(text, body)

    def test_restore_preflights_then_rewinds_before_applying(self) -> None:
        for source in (D1_STATE, D2_STATE):
            validator = function_body(source, "state_validate_runtime_state")
            self.assertIn("PHYSFS_seek(fp, start)", validator)
            restore_call = source.index("!state_validate_runtime_state(fp, swap, version)")
            apply_call = source.index("state_read_runtime_state(fp, swap", restore_call)
            self.assertLess(restore_call, apply_call)

    def test_variable_sections_are_bounded_before_iteration(self) -> None:
        for source in (D1_STATE, D2_STATE):
            morph = function_body(source, "state_validate_morph_runtime_state")
            effects = function_body(source, "state_validate_effect_runtime_state")
            self.assertIn("active_morphs > MAX_MORPH_OBJECTS", morph)
            self.assertIn("count > Num_effects", effects)
            self.assertIn("state_runtime_skip", morph)
            self.assertIn("state_runtime_read_s32", effects)

    def test_ai_restore_bounds_saved_variable_counts(self) -> None:
        d1_restore = function_body(D1_AI, "ai_restore_state")
        d2_restore = function_body(D2_AI, "ai_restore_state")
        for body in (d1_restore, d2_restore):
            guard = body.index("saved_num_awareness_events > MAX_AWARENESS_EVENTS")
            loop = body.index("i < saved_num_awareness_events")
            self.assertLess(guard, loop)
            self.assertIn("event.segnum > Highest_segment_index", body)
            self.assertIn("event.type > PA_WEAPON_ROBOT_COLLISION", body)
        boss_guard = d2_restore.index("Num_boss_teleport_segs > MAX_BOSS_TELEPORT_SEGS")
        boss_loop = d2_restore.index("i < Num_boss_gate_segs")
        self.assertLess(boss_guard, boss_loop)
        self.assertIn("temp > MAX_POINT_SEGS", d2_restore)
        self.assertIn("Believed_player_seg > Highest_segment_index", d2_restore)
        for source in (D1_STATE, D2_STATE):
            restore = source.index("if (!ai_restore_state")
            close = source.index("PHYSFS_close(fp)", restore)
            reject = source.index("return 0", close)
            self.assertLess(restore, close)
            self.assertLess(close, reject)

    def test_runtime_references_are_validated_before_publication(self) -> None:
        required_morph = (
            "objnum > Highest_object_index",
            "Objects[objnum].signature != morph_sig",
            "Objects[objnum].render_type != RT_MORPH",
            "model_num >= N_polygon_models",
            "counts[j] > MAX_VECS - starts[j]",
        )
        required_effect = (
            "effect_num >= Num_effects",
            "seen[effect_num]",
            "segnum > Highest_segment_index",
            "dest_bm_num >= NumTextures",
        )
        required_stuck = (
            "Objects[objnum].signature != signature",
            "wallnum >= Num_walls",
            "return count == active",
        )
        for source in (D1_STATE, D2_STATE):
            morph = function_body(source, "state_validate_morph_runtime_state")
            effects = function_body(source, "state_validate_effect_runtime_state")
            stuck = function_body(source, "state_validate_stuck_runtime_state")
            for text in required_morph:
                self.assertIn(text, morph)
            for text in required_effect:
                self.assertIn(text, effects)
            for text in required_stuck:
                self.assertIn(text, stuck)

    def test_checked_skip_rejects_truncation_without_wraparound(self) -> None:
        for source in (D1_STATE, D2_STATE):
            body = function_body(source, "state_runtime_skip")
            self.assertIn("position > length", body)
            self.assertIn("bytes > (size_t)(length - position)", body)
            self.assertNotIn("position + bytes > length", body)

    def test_translated_checkpoint_validates_allocator_before_global_apply(self) -> None:
        body = function_body(TRANSLATOR, "d1_save_translate_apply_runtime_state")
        validate = body.index("object_validate_runtime_state(&object_state)")
        publish = body.index("object_set_runtime_state(&object_state)")
        self.assertLess(validate, publish)

    def test_translated_objects_are_staged_and_validated_before_publication(self) -> None:
        body = function_body(TRANSLATOR, "d1_save_translate_apply_checkpoint_objects")
        parse = body.index("d1_save_translate_read_object(&reader, &translated_objects[i])")
        validate = body.index("d1_save_translate_validate_object_references")
        publish = body.index("memcpy(Objects, translated_objects")
        self.assertLess(parse, validate)
        self.assertLess(validate, publish)

        validator = function_body(TRANSLATOR, "d1_save_translate_validate_object")
        for text in (
            "obj->type >= MAX_OBJECT_TYPES",
            "obj->segnum > Highest_segment_index",
            "obj->id >= N_robot_types",
            "obj->id >= N_weapon_types",
            "obj->id >= N_powerup_types",
            "obj->rtype.pobj_info.model_num >= model_limit",
            "obj->rtype.vclip_info.vclip_num >= Num_vclips",
        ):
            self.assertIn(text, validator)

    def test_translated_world_references_are_checked(self) -> None:
        body = function_body(TRANSLATOR, "d1_save_translate_validate_world_state")
        for text in (
            "wallp->segnum > Highest_segment_index",
            "wallp->linked_wall >= Num_walls",
            "wallp->trigger >= Num_triggers",
            "door->front_wallnum[j] >= Num_walls",
            "triggerp->num_links > MAX_WALLS_PER_LINK",
            "sidep->wall_num >= Num_walls",
            "RobotCenters[i].fuelcen_num >= Num_fuelcenters",
            "ControlCenterTriggers.num_links > MAX_WALLS_PER_LINK",
        ):
            self.assertIn(text, body)


if __name__ == "__main__":
    unittest.main()
