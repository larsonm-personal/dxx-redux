// Public D2 level routed through a synthetic secret slot to exercise real entry hooks
#include "input_demo_fixture.h"
#include "input_demo_replay.h"
#ifdef DXX_BUILD_DESCENT_II
extern "C" {
void StartNewLevelSecret(int level_num, int page_in_textures);
extern int First_secret_visit;
}
#endif

static void exercise_wiggle_level_start(const char *scenario)
{
#ifdef DXX_BUILD_DESCENT_II
	using nlohmann::json;
	const bool load = std::strcmp(scenario, "level-load") == 0;
	char error[512] = {};
	std::string message;
	// The freely available demo has no authored secret map; use its unchanged first map
	const auto prepare_secret = []() {
		if (!Secret_level_names) Secret_level_names = (d_fname *)d_malloc(sizeof(d_fname));
		std::strcpy(Secret_level_names[0], Level_names[0]);
		Last_secret_level = -1;
		First_secret_visit = 1;
		input_demo_set_skip_level_intro(1);
	};
	const auto sample = []() {
		const object &o = *ConsoleObject;
		return json { { "phase", controls_get_wiggle_phase() }, { "clock", GameTime64 },
			{ "pos", { o.pos.x, o.pos.y, o.pos.z } },
			{ "velocity", { o.mtype.phys_info.velocity.x, o.mtype.phys_info.velocity.y, o.mtype.phys_info.velocity.z } } };
	};
	const auto physics_step = []() {
		FrameTime = F1_0 / 30;
		Controls = {};
		GameTime64 += FrameTime;
		read_flying_controls(ConsoleObject);
		do_physics_sim(ConsoleObject);
	};
	if (load) {
		require(input_demo_replay_load("wiggle-level.dximdemo", error, sizeof(error)), error);
		// Fresh secret start must clear stale offset, then apply the recorded effective phase
		controls_restore_wiggle_phase(111);
		prepare_secret();
		StartNewGame(-1);
		PHYSFS_file *fp = PHYSFS_openRead("wiggle-level-reference.json");
		require(fp != nullptr, "open secret-entry previous-process physics reference");
		bytes data(static_cast<size_t>(PHYSFS_fileLength(fp)));
		require(PHYSFS_read(fp, data.data(), 1, data.size()) == data.size() && PHYSFS_close(fp), "read secret-entry reference");
		const json expected = json::parse(data.begin(), data.end());
		require(sample() == expected["initial"], "cold secret-level replay restores effective phase and clock");
		for (const auto &frame : expected["frames"]) {
			physics_step();
			require(sample() == frame, "cold secret-level player physics matches carried phase");
		}
		input_demo_replay_unload();
		controls_restore_wiggle_phase(111);
		prepare_secret();
		StartNewGame(-1);
		require(controls_get_wiggle_phase() == ((unsigned int)GameTime64 & 0xffffu),
		        "fresh secret game without replay clears the prior session offset");
		std::puts("PASS cold secret-entry metadata and 60 player physics steps");
		return;
	}
	char save[] = "wiggle-before-secret.sav", description[21] = "Before secret";
	stop_time();
	require(state_save_all_sub(save, description) && state_restore_all_sub(save, 0), "ordinary restore before secret entry");
	require(GameTime64 == 0 && controls_get_wiggle_phase() != 0, "ordinary restore creates nonzero phase at zero clock");
	prepare_secret();
	input_demo_set_record_each_level(1);
	GameArg.SysInputDemoNoRender = 0;
	input_demo_begin_new_level(-1);
	GameArg.SysInputDemoNoRender = 1;
	const input_demo_level_start *captured = input_demo_recording_level_start();
	require(captured && captured->wiggle_phase == controls_get_wiggle_phase(), "level-start hook captures effective carried phase");
	input_demo_file demo;
	demo.metadata.version = 4;
	demo.metadata.game = "d2";
	demo.metadata.mission = Current_mission_filename;
	demo.metadata.git_version = "fixture";
	demo.metadata.arch = "fixture";
	demo.metadata.level = -1;
	demo.metadata.difficulty = Difficulty_level;
	demo.metadata.start_mode = "new_level";
	demo.metadata.rng_mode = "lcg_state";
	demo.metadata.frame_count = 1;
	demo.metadata.has_level_start = true;
	demo.metadata.level_start = *captured;
	demo.frames.resize(1);
	demo.frames[0].input.has_frame_time = 1;
	demo.frames[0].input.frame_time = F1_0 / 30;
	demo.frames[0].rng.state = captured->rng_state;
	demo.has_result = true;
	std::strcpy(demo.result.game, "d2");
	std::snprintf(demo.result.mission, sizeof(demo.result.mission), "%s", Current_mission_filename);
	demo.result.level = -1;
	demo.result.difficulty = Difficulty_level;
	demo.result.frame_count = 1;
	if (!input_demo_file_write("wiggle-level.dximdemo", demo, &message)) require(false, message.c_str());
	input_demo_set_record_each_level(0);
	input_demo_set_skip_level_intro(1);
	StartNewLevelSecret(-1, 0);
	json result { { "initial", sample() }, { "frames", json::array() } };
	for (int frame = 0; frame < 60; ++frame) {
		physics_step();
		result["frames"].push_back(sample());
	}
	const std::string text = result.dump();
	write_fixture("wiggle-level-reference.json", bytes(text.begin(), text.end()));
	std::puts("PASS restored-save secret-entry metadata and 60 player physics steps");
#else
	(void)scenario;
	require(false, "secret-entry phase fixture requires D2");
#endif
}
