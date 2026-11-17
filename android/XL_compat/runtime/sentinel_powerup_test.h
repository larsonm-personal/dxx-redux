static int sentinel_powerup_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		const char *extra = find_arg_value(argc, argv, "-extra-dir");
		require(extra && mount_extra_dir(extra, error, sizeof(error)), error);
		require(load_base_mission("boilpnt", error, sizeof(error)), error);
		new_player_config();
		input_demo_set_skip_level_intro(1);
		Difficulty_level = 2;
		// A previous level may leave unrelated bytes in these native union slots
		for (int index : { 4, 6 }) {
			std::memset(&Objects[index].ctype, 0x5a, sizeof(Objects[index].ctype));
			std::memset(&Objects[index].mtype, 0x5a, sizeof(Objects[index].mtype));
		}
		StartNewGame(4);
		const auto check = [&] {
			require(Current_level_num == 4 && xl_level_extensions_active() && !std::strcmp(Current_mission_filename, "boilpnt"), "Original sentinel mission was not loaded");
			for (int index : { 4, 6 }) {
				const auto &object = Objects[index];
				require(object.type == OBJ_POWERUP && object.id == POW_FUSION_WEAPON && object.control_type == CT_POWERUP &&
				            object.movement_type == 255 && object.render_type == RT_POWERUP && !object.ctype.powerup_info.count &&
				            object.size == Powerup_info[POW_FUSION_WEAPON].size,
				        "Original sentinel initialization or restoration differs from source");
			}
		};
		check();
		require(Objects[5].id == POW_FUSION_WEAPON && Objects[5].rtype.vclip_info.vclip_num == Powerup_info[POW_FUSION_WEAPON].vclip_num &&
		            Objects[5].rtype.vclip_info.frametime == Vclip[Powerup_info[POW_FUSION_WEAPON].vclip_num].frame_time && !Objects[5].rtype.vclip_info.framenum,
		        "Original invalid Fusion clip did not use its retained definition");
		require(Objects[7].id == POW_VULCAN_AMMO && Objects[7].rtype.vclip_info.vclip_num == Powerup_info[POW_VULCAN_AMMO].vclip_num &&
		            Objects[7].rtype.vclip_info.frametime == Vclip[Powerup_info[POW_VULCAN_AMMO].vclip_num].frame_time &&
		            !Objects[7].rtype.vclip_info.framenum && Objects[7].ctype.powerup_info.count == VULCAN_AMMO_AMOUNT,
		        "Original invalid ammo clip did not use its retained definition and source count");
		const auto snapshot = [] {
			nlohmann::json objects = nlohmann::json::array();
			for (int index : { 4, 5, 6, 7 }) objects.push_back(nlohmann::json(input_demo_object_trace_snapshot(Objects[index], index)));
			uint32_t sim = 0;
			require(d_rand_get_state(&sim), "Cannot observe sentinel SIM state");
			return nlohmann::json{ { "objects", objects }, { "world_before", input_demo_world_trace_snapshot() }, { "native_before", input_demo_native_xl_trace_snapshot() }, { "sim", sim }, { "sim_calls", d_rand_get_call_count() } };
		};
		const auto resources = xl_resource_mod_sources();
		const auto before = snapshot();
		std::map<int, int> scoped_frames;
		require(xl_resource_sources_for_level(4, &scoped_frames) == resources && snapshot() == before,
		        "Saved animation preflight changed the live world or source identity");
		int extended_effect = -1;
		for (const auto &entry : scoped_frames) {
			require(entry.second == xl_pog_effect_frames(&Effects[entry.first]), "Retained and installed animation frame counts differ");
			if (entry.second > Effects[entry.first].vc.num_frames) extended_effect = entry.first;
		}
		require(extended_effect >= 0, "Original Subzero lacks its longer replacement animation control");
		char save[] = "sentinel-original.sav", description[] = "Original sentinel lifecycle";
		stop_time();
		require(state_save_all_sub(save, description), "Cannot save original sentinel world");
		require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2"), "Cannot retire sentinel world into stock");
		require(!xl_level_extensions_active(), "Sentinel profile leaked into stock");
		require(state_restore_all_sub(save, 0), "Cannot restore original sentinel world");
		check();
		const auto restored = snapshot();
		native_continuation_receipt(std::filesystem::path(extra) / "sentinel-restore-comparison.json",
		                            { { "before", before }, { "restored", restored }, { "raw_equal", restored == before }, { "comparison", "Existing native_continuation_semantic observer equivalences" }, { "resources_equal", xl_resource_mod_sources() == resources } });
		require(native_continuation_semantic(restored) == native_continuation_semantic(before) && xl_resource_mod_sources() == resources, "Sentinel disk restore changed objects, world, XL state or SIM");
		const auto position4 = Objects[4].pos, position6 = Objects[6].pos;
		const auto advance = [&] {
			for (int frame = 0, remainder = 0; frame < 120; ++frame) {
				remainder += F1_0;
				FrameTime = remainder / 60;
				remainder %= 60;
				std::memset(&Controls, 0, sizeof(Controls));
				calc_game_time();
				GameProcessFrame();
				require(!Player_is_dead && !Game_suspended && !cheats.enabled, "Sentinel continuation lost ordinary gameplay");
			}
		};
		advance();
		check();
		require(!std::memcmp(&position4, &Objects[4].pos, sizeof(position4)) && !std::memcmp(&position6, &Objects[6].pos, sizeof(position6)), "Sentinel powerup moved during gameplay");
		const auto continued = snapshot();
		require(state_restore_all_sub(save, 0), "Cannot restore original sentinel continuation checkpoint");
		advance();
		check();
		const auto repeated = snapshot();
		native_continuation_receipt(std::filesystem::path(extra) / "sentinel-future-comparison.json", { { "continued", continued }, { "repeated", repeated } });
		require(native_continuation_semantic(continued) == native_continuation_semantic(repeated), "Sentinel checkpoint continuation changed future objects, world, XL state or SIM");
		// Controlled pickup, separately labeled from the ordinary continuation
		Players[Player_num].primary_weapon_flags &= ~(1 << FUSION_INDEX);
		collide_two_objects(ConsoleObject, &Objects[4], &Objects[4].pos);
		require((Players[Player_num].primary_weapon_flags & (1 << FUSION_INDEX)) && (Objects[4].flags & OF_SHOULD_BE_DEAD) &&
		            !(Objects[6].flags & OF_SHOULD_BE_DEAD),
		        "Native Fusion collision did not consume exactly one sentinel pickup");
		obj_delete_all_that_should_be_dead();
		require(Objects[4].type == OBJ_NONE && Objects[6].type == OBJ_POWERUP, "Consumed sentinel pickup did not retire");
		const auto earned = snapshot();
		char earned_save[] = "sentinel-earned.sav";
		require(state_save_all_sub(earned_save, description), "Cannot save consumed sentinel pickup");
		require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && state_restore_all_sub(earned_save, 0), "Cannot cold restore consumed sentinel pickup");
		native_continuation_receipt(std::filesystem::path(extra) / "sentinel-earned-comparison.json", { { "earned", earned }, { "restored", snapshot() } });
		require(Objects[4].type == OBJ_NONE && Objects[6].type == OBJ_POWERUP && (Players[Player_num].primary_weapon_flags & (1 << FUSION_INDEX)) &&
		            native_continuation_semantic(snapshot()) == native_continuation_semantic(earned),
		        "Earned restore resurrected a pickup or lost inventory, world or SIM state");
		extern void mod_identity_scoped_effect_validation(int effect);
		mod_identity_scoped_effect_validation(extended_effect);
		require(native_continuation_semantic(snapshot()) == native_continuation_semantic(earned), "Android effect preflight mutated the live world");
		// Corrupt a separate saved candidate at the real replacement's upper bound
		Effects[extended_effect].frame_count = scoped_frames.at(extended_effect);
		char invalid_save[] = "sentinel-invalid-effect.sav";
		require(state_save_all_sub(invalid_save, description), "Cannot save invalid animation control");
		require(!state_restore_all_sub(invalid_save, 0), "Restore accepted an out-of-range replacement animation frame");
		require(state_restore_all_sub(earned_save, 0) && native_continuation_semantic(snapshot()) == native_continuation_semantic(earned),
		        "Original earned save did not recover after malformed candidate rejection");
		native_continuation_receipt(std::filesystem::path(extra) / "sentinel-powerup.json",
		                            { { "mission", "boilpnt" }, { "level", 4 }, { "poisoned_prior_unions", true }, { "disk_restore_semantic_equal", true }, { "ordinary_frames", 120 }, { "stationary", true }, { "controlled_pickup", true }, { "future_equal", true }, { "consumed_restore_equal", true }, { "before", before }, { "continued", continued }, { "limitation", "Original lifecycle plus controlled collision; separate from sampled readiness or campaign completion" } });
		return 0;
	} catch (const std::exception &error) {
		std::fprintf(stderr, "Sentinel powerup test failed: %s\n", error.what());
		return 1;
	}
}
