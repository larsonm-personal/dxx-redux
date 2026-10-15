// Real public checkpoint continuation plus phase arithmetic and disk validation controls
extern "C" {
#include "controls.h"
}
#include "wiggle_phase_runtime.h"

static void test_wiggle_phase_runtime(void)
{
	const fix64 saved_time = GameTime64;
	const unsigned int saved_phase = controls_get_wiggle_phase();
	const fix64 epochs[] = { 0, 255, 256, 65535, 65536, ((fix64)1 << 40) + 12345 };
	for (const fix64 epoch : epochs) {
		GameTime64 = epoch;
		controls_reset_wiggle_phase();
		fix old_sine, new_sine;
		fix_fastsincos((fix)GameTime64, &old_sine, nullptr);
		fix_fastsincos((fix)controls_get_wiggle_phase(), &new_sine, nullptr);
		require(old_sine == new_sine, "fresh play retains original wiggle");
		for (const unsigned int phase : { 0u, 255u, 256u, 65535u }) {
			controls_restore_wiggle_phase(phase);
			require(GameTime64 == epoch && controls_get_wiggle_phase() == phase, "restore phase without rebasing game timers");
			GameTime64 += 2184;
			require(controls_get_wiggle_phase() == ((phase + 2184) & 0xffffu), "phase lower bits and wrap survive next frame");
			GameTime64 = epoch;
		}
	}
	// A replay or rewind absolute epoch already supplies its original phase
	GameTime64 = 930384;
	controls_restore_wiggle_phase(930384 & 0xffffu);
	GameTime64 += 2184;
	require(controls_get_wiggle_phase() == ((930384 + 2184) & 0xffffu), "absolute replay/rewind epoch is not added twice");
	GameTime64 = 0;
	controls_reset_wiggle_phase();
	require(controls_get_wiggle_phase() == 0, "fresh level and secret-return legacy phase reset");

	const auto disk = [](const char *path, PHYSFS_uint32 value, int bytes) {
		PHYSFS_file *fp = PHYSFS_openWrite(path);
		require(fp && PHYSFS_write(fp, &value, bytes, 1) == 1 && PHYSFS_close(fp), "write phase record control");
	};
	for (int swap : { 0, 1 }) {
		const PHYSFS_uint32 phase = swap ? SWAPINT(65535u) : 65535u;
		disk("wiggle-phase.bin", phase, 4);
		controls_restore_wiggle_phase(123);
		PHYSFS_file *fp = PHYSFS_openRead("wiggle-phase.bin");
		require(fp && wiggle_phase_runtime_read(fp, swap, 0), "phase preflight validates both byte orders");
		PHYSFS_close(fp);
		require(controls_get_wiggle_phase() == 123, "preflight does not mutate live phase");
		fp = PHYSFS_openRead("wiggle-phase.bin");
		require(fp && wiggle_phase_runtime_read(fp, swap, 1), "read phase in both byte orders");
		PHYSFS_close(fp);
		require(controls_get_wiggle_phase() == 65535, "apply phase record");
	}
	for (int bytes : { 3, 4 }) {
		disk("wiggle-phase.bin", 65536, bytes);
		controls_restore_wiggle_phase(123);
		PHYSFS_file *fp = PHYSFS_openRead("wiggle-phase.bin");
		require(fp && !wiggle_phase_runtime_read(fp, 0, 1), "reject truncated or out-of-range phase");
		PHYSFS_close(fp);
		require(controls_get_wiggle_phase() == 123, "invalid phase leaves live phase unchanged");
	}
	GameTime64 = saved_time;
	controls_restore_wiggle_phase(saved_phase);
}

static void exercise_wiggle_save_runtime(const char *scenario)
{
	using nlohmann::json;
	test_wiggle_phase_runtime();
	const auto snapshot = []() {
		const object &o = *ConsoleObject;
		unsigned int sim = 0, fx = 0;
		d_rand_get_state(&sim);
		d_rand_get_stream_state(D_RNG_FX, &fx);
		return json { { "pos", { o.pos.x, o.pos.y, o.pos.z } },
			{ "orient", { o.orient.rvec.x, o.orient.rvec.y, o.orient.rvec.z,
			                o.orient.uvec.x, o.orient.uvec.y, o.orient.uvec.z,
			                o.orient.fvec.x, o.orient.fvec.y, o.orient.fvec.z } },
			{ "velocity", { o.mtype.phys_info.velocity.x, o.mtype.phys_info.velocity.y, o.mtype.phys_info.velocity.z } },
			{ "shields", int(Players[Player_num].shields) }, { "sim", sim }, { "fx", fx },
			{ "phase", controls_get_wiggle_phase() } };
	};
	const auto step = []() {
		FrameTime = F1_0 / 30;
		Controls = {};
		do_weapon_n_item_stuff();
		calc_game_time();
		GameProcessFrame();
		require(render_update_main_view_objects(), "update continued gameplay view");
	};
	const auto restore = [](char *path) {
#ifdef DXX_BUILD_DESCENT_II
		return state_restore_all_sub(path, 0);
#else
		return state_restore_all_sub(path);
#endif
	};
	char filename[] = "wiggle-runtime.sav", description[21] = "Wiggle continuation";
	if (std::strcmp(scenario, "load") == 0) {
		PHYSFS_file *fp = PHYSFS_openRead("wiggle-reference.json");
		require(fp != nullptr, "open previous-process phase reference");
		bytes data(static_cast<size_t>(PHYSFS_fileLength(fp)));
		require(PHYSFS_read(fp, data.data(), 1, data.size()) == data.size() && PHYSFS_close(fp), "read previous-process reference");
		const json reference = json::parse(data.begin(), data.end());
		require(restore(filename) && GameTime64 == 0, "cold ordinary phase restore");
		require(snapshot() == reference["initial"], "cold restore phase and player state");
		for (const auto &expected : reference["frames"]) {
			step();
			require(snapshot() == expected, "cold restored real gameplay matches previous process");
		}
		std::puts("PASS wiggle save separate-process restore and 30 continuation frames");
		return;
	}
	const bool no_wiggle = std::strcmp(scenario, "no-wiggle") == 0;
	if (no_wiggle) ConsoleObject->mtype.phys_info.flags &= ~PF_WIGGLE;
	// The second generation saves again after a zero-based restore and seven real frames
	for (int generation = 0; generation < 2; ++generation) {
		if (generation) for (int f = 0; f < 7; ++f) step();
		const auto initial = snapshot();
		stop_time();
		require(state_save_all_sub(filename, description), "write ordinary phase save");
		std::vector<json> expected;
		for (int f = 0; f < (generation ? 30 : 180); ++f) {
			step();
			expected.push_back(snapshot());
		}
		const std::string reference = json { { "initial", initial }, { "frames", expected } }.dump();
		write_fixture("wiggle-reference.json", bytes(reference.begin(), reference.end()));
		require(restore(filename), "restore ordinary phase save");
		require(GameTime64 == 0, "ordinary restore retains zero-based timer semantics");
		require(snapshot() == initial, "restore exact player phase, state and RNG");
		for (size_t f = 0; f < expected.size(); ++f) {
			step();
			if (snapshot() != expected[f]) std::fprintf(stderr, "wiggle generation=%d frame=%zu expected=%s actual=%s\n", generation, f, expected[f].dump().c_str(), snapshot().dump().c_str());
			require(snapshot() == expected[f], "ordinary phase continuation matches real gameplay");
		}
		require(restore(filename), "warm restore before second save generation");
	}
	std::printf("PASS wiggle save scenario=%s first 180 and repeated 30 frames, phases, timers and RNG\n", scenario);
}
