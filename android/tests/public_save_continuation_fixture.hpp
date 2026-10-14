// Observe ordinary runtime-generated saves without changing their clock semantics
static void exercise_public_save_continuation(const char *scenario)
{
	using nlohmann::json;
#ifdef DXX_BUILD_DESCENT_II
	const char *game_name = "d2", *mission = "d2demo";
#else
	const char *game_name = "d1", *mission = "d1";
#endif
	const bool door = std::strcmp(scenario, "door") == 0;
	require(door || std::strcmp(scenario, "flight") == 0, "known save continuation scenario");
	const int frames = door ? 180 : 60;
	char error[256] = {};
	const auto boundary = [&](int frame, const char *phase) {
		require(input_demo_object_trace_boundary(frame, phase, error, sizeof(error)), error);
		require(input_demo_world_trace_write(frame, phase, error, sizeof(error)), error);
	};
	const auto start = [&](const char *path, int count) {
		require(input_demo_state_trace_start(path, "ordinary_save", game_name, mission, 1, Difficulty_level, "save_checkpoint", count, error, sizeof(error)), error);
		boundary(0, "restored");
	};
	const auto continuation = [&](const char *path) {
		start(path, frames);
		for (int frame = 0; frame < frames; ++frame) {
			FrameTime = F1_0 / 30;
			Controls = {};
			if (!door) {
				Controls.forward_thrust_time = FrameTime;
				if (frame < 30) Controls.fire_primary_state = 1;
			}
			const json input = { { "type", "continuation_input" }, { "version", 1 }, { "f", frame }, { "frame_time", FrameTime }, { "forward", Controls.forward_thrust_time }, { "fire_primary", Controls.fire_primary_state } };
			require(input_demo_state_trace_write_json(input.dump().c_str(), error, sizeof(error)), error);
			do_weapon_n_item_stuff();
			calc_game_time();
			GameProcessFrame();
			require(render_update_main_view_objects(), "update continuation gameplay view state");
			require(input_demo_object_trace_write(frame, error, sizeof(error)), error);
			require(input_demo_world_trace_write(frame, nullptr, error, sizeof(error)), error);
		}
		boundary(frames, "terminal");
		input_demo_state_trace_stop();
	};
	start("before-save.jsonl.gz", 0);
	boundary(0, "terminal");
	input_demo_state_trace_stop();
	char filename[] = "public-continuation.sav";
	char description[21] = "Public continuation";
	stop_time();
	require(state_save_all_sub(filename, description), "generate temporary native gameplay save");
	continuation("uninterrupted.jsonl.gz");
#ifdef DXX_BUILD_DESCENT_II
	require(state_restore_all_sub(filename, 0), "reload runtime-generated ordinary D2 save");
#else
	require(state_restore_all_sub(filename), "reload runtime-generated ordinary D1 save");
#endif
	continuation("resumed.jsonl.gz");
	std::puts("CAPTURED: ordinary save persistence and raw continuation diagnostics");
}
