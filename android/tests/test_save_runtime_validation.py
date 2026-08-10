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
D1_INPUT_DEMO = (REPO_ROOT / "d1/main/input_demo_hooks.c").read_text(encoding="utf-8")
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


def topology_valid(objects: list[tuple[int, int, int] | None], segment_count: int) -> bool:
    heads = [-1] * segment_count
    seen: set[int] = set()
    for index, obj in enumerate(objects):
        if obj is None:
            continue
        segment, previous, _ = obj
        if not 0 <= segment < segment_count:
            return False
        if previous == -1:
            if heads[segment] != -1:
                return False
            heads[segment] = index
    for index, obj in enumerate(objects):
        if obj is None:
            continue
        segment, previous, following = obj
        if previous != -1:
            if not 0 <= previous < len(objects) or objects[previous] is None:
                return False
            if objects[previous][0] != segment or objects[previous][2] != index:
                return False
        if following != -1:
            if not 0 <= following < len(objects) or objects[following] is None:
                return False
            if objects[following][0] != segment or objects[following][1] != index:
                return False
    for segment, head in enumerate(heads):
        current = head
        while current != -1:
            if current in seen or objects[current] is None or objects[current][0] != segment:
                return False
            seen.add(current)
            current = objects[current][2]
    return seen == {index for index, obj in enumerate(objects) if obj is not None}


class SaveRuntimeValidationTest(unittest.TestCase):
    def test_topology_model_covers_valid_and_malformed_graphs(self) -> None:
        self.assertTrue(topology_valid([], 2))
        self.assertTrue(topology_valid([(0, -1, 2), None, (0, 0, -1)], 2))
        self.assertTrue(topology_valid([(1, -1, -1), (0, -1, -1)], 2))
        self.assertFalse(topology_valid([(0, -1, -1), (0, -1, -1)], 1))
        self.assertFalse(topology_valid([(0, -1, 1), (0, 0, 0)], 1))
        self.assertFalse(topology_valid([(0, -1, 1), (0, -1, -1)], 1))
        self.assertFalse(topology_valid([(0, -1, -1), (0, 0, -1)], 1))
        self.assertFalse(topology_valid([(2, -1, -1)], 2))

    def test_topology_validation_is_staged_and_fallback_starts_empty(self) -> None:
        for source, name in (
            (D1_INPUT_DEMO, "input_demo_restore_checkpoint_object_links"),
            (D2_STATE, "state_restore_segment_object_links"),
        ):
            body = function_body(source, name)
            self.assertIn("int segment_heads[MAX_SEGMENTS]", body)
            self.assertIn("segment_heads[obj->segnum] = i", body)
            publish = body.index("Segments[segnum].objects = segment_heads[segnum]")
            coverage = body.index("Objects[i].type != OBJ_NONE && !seen[i]")
            self.assertLess(coverage, publish)

        d1_restore = D1_STATE.index("input_demo_restore_checkpoint_object_links()))")
        d1_clear = D1_STATE.index("Segments[i].objects = -1", d1_restore)
        d1_link = D1_STATE.index("obj_link(i,segnum)", d1_clear)
        self.assertLess(d1_clear, d1_link)
        d2_fallback = function_body(D2_STATE, "state_relink_objects_by_index")
        self.assertLess(
            d2_fallback.index("Segments[i].objects = -1"),
            d2_fallback.index("obj_link(i,segnum)"),
        )

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

    def test_ai_restore_preflights_exact_bytes_before_publication(self) -> None:
        for source in (D1_AI, D2_AI):
            preflight = function_body(source, "ai_restore_preflight")
            restore = function_body(source, "ai_restore_state")
            self.assertIn("ai_restore_advanced_exactly", preflight)
            self.assertIn("PHYSFS_seek(fp, start)", preflight)
            self.assertIn("if (!ai_restore_preflight", restore)
            validate = restore.index("if (!ai_restore_preflight")
            publish = restore.index("Ai_initialized =")
            self.assertLess(validate, publish)

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
        reader = function_body(TRANSLATOR, "d1_save_translate_read_runtime_state")
        commit = function_body(TRANSLATOR, "d1_save_translate_commit_runtime_state")
        self.assertIn("d1_save_translate_validate_runtime_allocator", reader)
        self.assertIn("object_set_runtime_state(&state->object_state)", commit)
        apply = function_body(TRANSLATOR, "d1_save_translate_apply_checkpoint_objects")
        validate = apply.index("d1_save_translate_read_runtime_state")
        publish = apply.index("reset_objects(1)")
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
            "wallp->linked_wall >= state->num_walls",
            "wallp->trigger >= state->num_triggers",
            "door->front_wallnum[j] >= state->num_walls",
            "triggerp->num_links > MAX_WALLS_PER_LINK",
            "sidep->wall_num >= state->num_walls",
            "state->robot_centers[i].fuelcen_num >= state->num_fuelcenters",
            "state->control_center_triggers.num_links > MAX_CONTROLCEN_LINKS",
        ):
            self.assertIn(text, body)

    def test_translated_checkpoint_stages_every_section_before_commit(self) -> None:
        body = function_body(TRANSLATOR, "d1_save_translate_apply_checkpoint_objects")
        for text in (
            "d1_save_translate_read_d1_state_to_runtime",
            "d1_save_translate_validate_world_state",
            "d1_save_translate_validate_d1_ai_state",
            "d1_save_translate_read_runtime_state",
        ):
            self.assertIn(text, body)
        first_publish = body.index("reset_objects(1)")
        for text in (
            "d1_save_translate_read_d1_state_to_runtime",
            "d1_save_translate_validate_world_state",
            "d1_save_translate_validate_d1_ai_state",
            "d1_save_translate_read_runtime_state",
        ):
            self.assertLess(body.index(text), first_publish)


if __name__ == "__main__":
    unittest.main()
