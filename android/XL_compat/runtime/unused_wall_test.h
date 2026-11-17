#include "input_demo_recorder.h"

static int unused_wall_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		const char *extra = find_arg_value(argc, argv, "-extra-dir");
		require(extra && mount_extra_dir(extra, error, sizeof(error)), error);
		require(load_base_mission("wshock20", error, sizeof(error)), error);
		new_player_config();
		input_demo_set_skip_level_intro(1);
		Difficulty_level = 2;
		StartNewGame(4);
		require(Current_level_num == 4 && xl_level_extensions_active(), "Original Nautilus did not load");
		std::vector<int> clips, illusions;
		unsigned closed = 0;
		for (int i = 0; i < Num_walls; ++i) {
			clips.push_back(Walls[i].clip_num);
			if (Walls[i].clip_num != -33) continue;
			if (Walls[i].type == WALL_CLOSED) ++closed;
			else if (Walls[i].type == WALL_ILLUSION) illusions.push_back(i);
			else require(false, "Original negative clip belongs to an unexpected wall type");
		}
		require(closed == 24 && illusions.size() == 4, "Original unused wall field inventory differs");
		const auto check_clips = [&] {
			require(clips.size() == unsigned(Num_walls), "Original wall count changed");
			for (int i = 0; i < Num_walls; ++i) require(Walls[i].clip_num == clips[i], "An unused authored wall clip was normalized");
		};
		const auto snapshot = [] {
			nlohmann::json objects = nlohmann::json::array();
			for (int i = 0; i <= Highest_object_index; ++i)
				if (Objects[i].type != OBJ_NONE) objects.push_back(nlohmann::json(input_demo_object_trace_snapshot(Objects[i], i)));
			uint32_t sim = 0;
			require(d_rand_get_state(&sim), "Cannot observe original wall SIM state");
			return nlohmann::json{ { "objects", objects }, { "world_before", input_demo_world_trace_snapshot() }, { "native_before", input_demo_native_xl_trace_snapshot() }, { "sim", sim }, { "sim_calls", d_rand_get_call_count() } };
		};
		const auto advance = [&](int frames) {
			for (int frame = 0, remainder = 0; frame < frames; ++frame) {
				remainder += F1_0;
				FrameTime = remainder / 60;
				remainder %= 60;
				std::memset(&Controls, 0, sizeof(Controls));
				if (input_demo_recorder_is_active()) do_weapon_n_item_stuff();
				calc_game_time();
				GameProcessFrame();
				require(render_update_game_view_automap(), "Original wall continuation has no gameplay view");
				require(!Player_is_dead && !Game_suspended && !cheats.enabled, "Original wall continuation lost ordinary gameplay");
			}
			check_clips();
		};
		char save[] = "unused-wall-original.sav", description[] = "Original unused wall fields";
		stop_time();
		require(state_save_all_sub(save, description), "Cannot save original unused wall fields");
		require(state_restore_all_sub(save, 0), "Cannot enter original wall continuation checkpoint");
		const auto recording_root = std::filesystem::path(PHYSFS_getWriteDir()) / "input_demo_recordings" / "new";
		std::set<std::filesystem::path> previous;
		if (std::filesystem::exists(recording_root))
			for (const auto &entry : std::filesystem::directory_iterator(recording_root)) previous.insert(entry.path());
		require(input_demo_start_quick_recording_common() && input_demo_recorder_uses_environment(), "Cannot capture original wall environment");
		advance(120);
		require(input_demo_stop_quick_recording_common(), "Cannot finish original wall capture");
		const auto continued = snapshot();
		std::filesystem::path recording;
		for (const auto &entry : std::filesystem::directory_iterator(recording_root))
			if (entry.path().extension() == ".dximdemo" && !previous.count(entry.path())) {
				require(recording.empty(), "Multiple original wall captures");
				recording = entry.path();
			}
		require(!recording.empty() && input_demo_load_replay_from_path(recording.string().c_str(), error, sizeof(error)), error);
		require(input_demo_replay_frame_count() == 120 && input_demo_start_loaded_replay() == 0, "Cannot start original wall replay");
		unsigned completed = 0;
		while (input_demo_replay_is_loaded() && completed < 120) {
			require(input_demo_step_replay_frame(), "Original wall replay stopped early");
			++completed;
		}
		require(completed == 120 && !input_demo_replay_is_loaded() && input_demo_replay_actual_result_written(), "Original wall replay lacks its terminal result");
		const auto repeated = snapshot();
		native_continuation_receipt(std::filesystem::path(extra) / "unused-wall-future.json", { { "continued", continued }, { "repeated", repeated } });
		require(native_continuation_semantic(continued) == native_continuation_semantic(repeated), "Original wall save changed its future world or SIM");
		for (int index : illusions) {
			auto &wall = Walls[index];
			wall_illusion_off(&Segments[wall.segnum], wall.sidenum);
			require((wall.flags & WALL_ILLUSION_OFF) && !(wall_is_doorway(&Segments[wall.segnum], wall.sidenum) & WID_RENDER_FLAG), "Original illusion did not turn off");
			wall_illusion_on(&Segments[wall.segnum], wall.sidenum);
			require(!(wall.flags & WALL_ILLUSION_OFF) && (wall_is_doorway(&Segments[wall.segnum], wall.sidenum) & WID_RENDER_FLAG), "Original illusion did not turn on");
		}
		check_clips();
		require(Walls[0].type == WALL_CLOSED && Walls[1].type == WALL_CLOSED && clips[0] == -33 && clips[1] == -33, "Original closed wall pair differs");
		start_wall_cloak(&Segments[Walls[0].segnum], Walls[0].sidenum);
		advance(15);
		require(Num_cloaking_walls == 1 && Walls[0].state == WALL_DOOR_CLOAKING, "Original closed wall did not enter its cloak transition");
		const auto partial = snapshot();
		char partial_save[] = "unused-wall-cloaking.sav";
		stop_time();
		require(state_save_all_sub(partial_save, description), "Cannot save original partial cloak");
		require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(), "Cannot retire original walls into stock");
		require(state_restore_all_sub(partial_save, 0), "Cannot cold restore original partial cloak");
		const auto restored = snapshot();
		native_continuation_receipt(std::filesystem::path(extra) / "unused-wall-restore.json", { { "partial", partial }, { "restored", restored } });
		require(native_continuation_semantic(partial) == native_continuation_semantic(restored), "Partial cloak restore changed original world or SIM");
		advance(60);
		require(!Num_cloaking_walls && Walls[0].type == WALL_OPEN && Walls[1].type == WALL_OPEN, "Original closed pair did not finish opening");
		start_wall_decloak(&Segments[Walls[0].segnum], Walls[0].sidenum);
		advance(60);
		require(Num_cloaking_walls == 1 && Walls[0].state == WALL_DOOR_DECLOAKING && CloakingWalls[0].time == F1_0,
		        "Original decloak did not retain its exact one-second boundary");
		advance(1);
		require(!Num_cloaking_walls && Walls[0].type == WALL_CLOSED && Walls[1].type == WALL_CLOSED, "Original closed pair did not finish closing");
		native_continuation_receipt(std::filesystem::path(extra) / "unused-wall.json",
		                            { { "mission", "wshock20" }, { "level", 4 }, { "ordinary_frames", 120 }, { "closed_unused_clips", closed }, { "illusion_unused_clips", illusions.size() }, { "future_equal", true }, { "cold_cloak_restore", true }, { "illusion_transitions", true }, { "cloak_transitions", true }, { "raw_clips_preserved", true }, { "limitation", "Controlled original wall lifecycle, not campaign completion" } });
		return 0;
	} catch (const std::exception &error) {
		std::fprintf(stderr, "Original unused-wall test failed: %s\n", error.what());
		return 1;
	}
}
