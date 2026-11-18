#include "input_demo_recorder.h"
#include "input_demo_replay.h"

static void wall_strip_recorded_future(const std::filesystem::path &root, const char *kind)
{
	const auto snapshot = [] {
		nlohmann::json objects = nlohmann::json::array();
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type != OBJ_NONE) objects.push_back(nlohmann::json(input_demo_object_trace_snapshot(Objects[i], i)));
		uint32_t sim = 0;
		require(d_rand_get_state(&sim), "Cannot observe wall strip SIM state");
		return nlohmann::json{ { "objects", objects }, { "world_before", input_demo_world_trace_snapshot() }, { "native_before", input_demo_native_xl_trace_snapshot() }, { "sim", sim }, { "sim_calls", d_rand_get_call_count() } };
	};
	const auto directory = std::filesystem::path(PHYSFS_getWriteDir()) / "input_demo_recordings/new";
	std::set<std::filesystem::path> prior;
	if (std::filesystem::exists(directory))
		for (const auto &entry : std::filesystem::directory_iterator(directory)) prior.insert(entry.path());
	require(input_demo_start_quick_recording_common() && input_demo_recorder_uses_environment(), "Cannot capture wall strip future");
	for (int frame = 0, remainder = 0; frame < 120; ++frame) {
		remainder += F1_0;
		FrameTime = remainder / 60;
		remainder %= 60;
		Controls = {};
		do_weapon_n_item_stuff();
		calc_game_time();
		GameProcessFrame();
		require(render_update_game_view_automap() && !Player_is_dead, "Wall strip capture lost its running native world");
	}
	require(input_demo_stop_quick_recording_common(), "Cannot finalize wall strip capture");
	const auto continued = snapshot();
	std::filesystem::path recording;
	for (const auto &entry : std::filesystem::directory_iterator(directory))
		if (entry.path().extension() == ".dximdemo" && !prior.count(entry.path())) {
			require(recording.empty(), "Multiple wall strip captures");
			recording = entry.path();
		}
	char error[256]{};
	require(!recording.empty() && input_demo_load_replay_from_path(recording.string().c_str(), error, sizeof(error)), error);
	require(input_demo_replay_frame_count() == 120 && input_demo_start_loaded_replay() == 0, "Cannot start wall strip clean replay");
	unsigned frames = 0;
	while (input_demo_replay_is_loaded() && frames < 120) {
		require(input_demo_step_replay_frame(), "Wall strip replay stopped early");
		++frames;
	}
	require(frames == 120 && !input_demo_replay_is_loaded() && input_demo_replay_actual_result_written(), "Wall strip replay lacks terminal evidence");
	const auto repeated = snapshot();
	native_continuation_receipt(root / (std::string("strip-") + kind + "-future.json"), { { "continued", continued }, { "repeated", repeated }, { "raw_equal", continued == repeated } });
	require(native_continuation_semantic(continued) == native_continuation_semantic(repeated), "Wall strip replay changed future objects, world, XL state or SIM");
	std::cout << "Derived " << kind << " strip: 120 captured and clean-replayed frames preserve detailed world/objects/XL and SIM under existing observer equivalences\n";
}
