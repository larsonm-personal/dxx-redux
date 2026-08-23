import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class StatePersistenceContractsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.state = (ROOT / "android/app/src/main/cpp/shared/state_android_shared.c").read_text(encoding="utf-8")
        cls.coop = (ROOT / "android/app/src/main/cpp/shared/coop/coop_save.c").read_text(encoding="utf-8")
        cls.meta_actions = (ROOT / "android/app/src/main/cpp/shared/android_meta_actions.c").read_text(encoding="utf-8")

    def test_quick_save_and_load_use_synchronized_coop_protocol(self) -> None:
        save_start = self.meta_actions.index("static void android_coop_quick_save(void)")
        load_start = self.meta_actions.index("static void android_coop_quick_load(void)", save_start)
        handler_start = self.meta_actions.index("int android_handle_ingame_saveload_request(void)", load_start)
        save_body = self.meta_actions[save_start:load_start]
        load_body = self.meta_actions[load_start:handler_start]
        handler_body = self.meta_actions[handler_start:self.meta_actions.index("if (g_android_autosave_request_kind)", handler_start)]

        self.assertLess(save_body.index("memset(desc"), save_body.index("multi_send_save_game"))
        self.assertLess(save_body.index("multi_send_save_game"), save_body.index("multi_save_game"))
        self.assertIn("ANDROID_SAVE_META_SLOT_QUICK, 1, 0", load_body)
        self.assertLess(load_body.index("multi_send_restore_game"), load_body.index("multi_restore_game"))
        self.assertIn("multi_coop_restore_transfer_pending()", load_body)
        self.assertIn("if (Game_mode & GM_MULTI_COOP)", handler_body)
        self.assertNotIn("Player_is_dead || (Game_mode & GM_MULTI)", handler_body)

    def test_save_stage_is_validated_and_transactionally_published(self) -> None:
        start = self.state.index("int state_android_save_to_path")
        body = self.state[start : self.state.index("static int state_android_save_to_slot_internal", start)]
        save = body.index("state_save_all_sub(staged_filename")
        validate = body.index("state_android_validate_save_path(staged_filename")
        publish = body.index("state_android_publish_single_file(staged_filename")
        self.assertLess(save, validate)
        self.assertLess(validate, publish)
        self.assertIn("PHYSFS_delete(staged_filename)", body)
        self.assertLess(publish, body.index("state_android_write_last_save_set"))

        start = self.state.index("static int state_android_publish_single_file")
        publish_body = self.state[start : self.state.index("static int state_android_publish_save_slot", start)]
        for token in (
            "PHYSFSX_rename(filename, backup)",
            "PHYSFSX_rename(temporary, filename)",
            "PHYSFSX_rename(backup, filename)",
        ):
            self.assertIn(token, publish_body)

    def test_paired_and_coop_saves_publish_in_commit_order(self) -> None:
        for game in ("d1", "d2"):
            source = (ROOT / game / "main/state.c").read_text(encoding="utf-8")
            start = source.index("int state_save_all(")
            body = source[start : source.index("int state_save_all_sub(", start)]
            self.assertIn("state_android_save_to_path(filename, desc", body)
            self.assertIn("ANDROID_SAVE_META_KIND_MANUAL", body)

        start = self.coop.index("int coop_autosave(void)")
        body = self.coop[start : self.coop.index("static void coop_write_autosave_history", start)]
        save = body.index("state_android_save_to_path")
        notify = body.index("multi_send_save_game")
        self.assertLess(save, notify)
        self.assertLess(notify, body.index("coop_write_autosave_history"))
        self.assertIn("return 0;", body[save:notify])

    def test_coop_inventory_validates_before_mutation(self) -> None:
        writer = self.coop.index("static void coop_write_progress_inventory_file")
        loader = self.coop.index("int coop_load_progress_inventory", writer)
        writer_body = self.coop[writer:loader]
        for token in ('"%s.tmp"', "PHYSFS_flush(fp)", "PHYSFS_close(fp) && write_ok", "coop_progress_inventory_publish"):
            self.assertIn(token, writer_body)
        body = self.coop[loader : self.coop.index("void coop_write_progress_json", loader)]
        apply = body.index("coop_apply_record_to_player")
        self.assertLess(body.index("PHYSFS_fileLength(fp) != expected_length"), apply)
        self.assertLess(body.index("coop_progress_inventory_checksum(records"), apply)
        self.assertIn("if (host_record >= 0)", body)
        self.assertIn("if (host_record < 0", body)
        self.assertIn("strncasecmp(records[i].callsign, records[j].callsign", body)

    def test_coop_restore_reapplies_player_spew_no_expire_policy(self) -> None:
        start = self.coop.index("int coop_restore_player_spew_lifetimes(void)")
        body = self.coop[start : self.coop.index("\n}\n", start) + 3]
        for token in (
            "Game_mode & GM_MULTI_COOP",
            "Netgame.PlayerSpewNoExpire",
            "obj->type != OBJ_POWERUP",
            "obj->flags & OF_PLAYER_DROPPED",
            "obj->lifeleft == IMMORTAL_TIME",
            "obj->lifeleft = IMMORTAL_TIME",
        ):
            self.assertIn(token, body)

        for game in ("d1", "d2"):
            source = (ROOT / game / "main/state.c").read_text(encoding="utf-8")
            metadata = source.index("if (have_coop_meta)")
            restore = source.index("coop_restore_player_spew_lifetimes()", metadata)
            missing_metadata = source.index("else if (Game_mode & GM_MULTI_COOP)", metadata)
            self.assertLess(metadata, restore)
            self.assertLess(restore, missing_metadata)

    def test_d1_translation_accepts_only_its_decoded_layout(self) -> None:
        source = (ROOT / "d2/main/d1_save_translate.c").read_text(encoding="utf-8")
        self.assertEqual("D1_SAVE_VERSION", re.search(r"#define D1_SAVE_COMPATIBLE_VERSION\s+(\S+)", source).group(1))
        self.assertIn("version < D1_SAVE_COMPATIBLE_VERSION || version > D1_SAVE_VERSION", source)
        for token in (
            "obj->ctype.ai_info.danger_laser_num = -1",
            "d1_save_translate_validate_object(obj, object_count)",
            "d1_save_translate_rebuild_checkpoint_object_links",
        ):
            self.assertIn(token, source)
        self.assertGreaterEqual(source.count("d1_save_translate_validate_checkpoint_object_links("), 2)

    def test_gamepad_config_decodes_before_publication(self) -> None:
        source = (ROOT / "android/app/src/main/cpp/android_gamepad_config.cpp").read_text(encoding="utf-8")
        start = source.index("static bool load_config_into_playercfg")
        body = source[start : source.index("static void apply_android_virtual_axis_defaults", start)]
        parse = body.index("cfg = json::parse")
        validate = body.index('cfg["thresholds"][axis_map[i].name]')
        publish = body.index("PlayerCfg = staged")
        self.assertLess(parse, validate)
        self.assertLess(validate, publish)
        self.assertLess(publish, body.index("android_axis_mailbox_set_button_deadzone", publish))
        self.assertIn("layout->joystick_size, layout->joystick_size", body)
        self.assertIn("layout->settings_size, KCONFIG_ANDROID_MAX_SETTINGS", body)

    def test_rewind_session_owns_full_mission_identity(self) -> None:
        source = (ROOT / "android/app/src/main/cpp/shared/android_rewind.c").read_text(encoding="utf-8")
        for token in (
            "char *mission;",
            "strlen(Current_mission_filename) + 1",
            "memcpy(mission, Current_mission_filename, mission_bytes)",
            "free(g_android_rewind_session.mission)",
            "g_android_rewind_session.mission = NULL",
        ):
            self.assertIn(token, source)
        self.assertNotIn("ANDROID_REWIND_MISSION_LEN", source)

    def test_resume_metadata_identity_precedes_publication(self) -> None:
        source = (ROOT / "android/app/src/main/cpp/jni_resume_save.cpp").read_text(encoding="utf-8")
        start = source.index("static bool read_resume_candidate")
        self.assertLess(source.index("save_metadata_path_identity_error(path, meta)", start), source.index("out->meta = meta", start))
        for reason in (
            "metadata_game_path_mismatch",
            "metadata_scope_extension_mismatch",
            "metadata_save_kind_slot_mismatch",
            "metadata_pilot_path_mismatch",
            "metadata_mission_path_mismatch",
        ):
            self.assertIn(reason, source)
        explorer = source.index("static json save_explorer_slot_json")
        self.assertLess(source.index("const char *identity_error", explorer), source.index("identity_error ? identity_error", explorer))


if __name__ == "__main__":
    unittest.main()
