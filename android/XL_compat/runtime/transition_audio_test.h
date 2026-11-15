#ifndef DXX_XL_TRANSITION_AUDIO_TEST_H
#define DXX_XL_TRANSITION_AUDIO_TEST_H
#include "../slow_motion.h"
namespace xl_transition_audio_control
{
inline void run(const std::filesystem::path &root, int ordinal)
{
	using namespace xl_compat;
	using namespace named_audio_control;
	SDL_PauseAudio(1);
	const auto original = xl_ambient_objects_control::source_level();
	auto original_bank = prepare_named_audio(Gamesave_current_filename, original, ordinal);
	const auto initial = snapshot();
	const auto folder = root / "mods/xsound/sounds";
	const auto slow = folder / "slowdown.wav", fast = folder / "speedup.wav";
	require(!std::filesystem::exists(slow) && !std::filesystem::exists(fast), "Transition control refuses to replace existing cues");
	struct Guard {
		std::shared_ptr<const AudioResources> bank;
		std::filesystem::path slow, fast;
		~Guard()
		{
			SDL_PauseAudio(1);
			xl_named_audio_slow_motion_reset();
			install_named_audio(std::move(bank));
			std::error_code error;
			std::filesystem::remove(slow, error);
			std::filesystem::remove(fast, error);
		}
	} guard{ original_bank, slow, fast };
	const auto world = [] {
		auto objects = nlohmann::json::object();
		for (int slot = 0; slot < object_capacity(); ++slot)
			if (Objects[slot].type != OBJ_NONE) objects[std::to_string(slot)] = input_demo_object_trace_snapshot(Objects[slot], slot);
		uint32_t sim;
		require(d_rand_get_state(&sim), "Cannot observe transition SIM state");
		return nlohmann::json{ { "world", input_demo_world_trace_snapshot() }, { "pool", input_demo_object_pool_trace_snapshot() }, { "objects", objects }, { "sim_state", sim }, { "sim_calls", d_rand_get_call_count() } };
	};
	const auto before = world();
	auto source = original;
	// Derived inventory only: retain the original HOG and native world unchanged
	source.objects.at(0).contains_type = 7;
	source.objects.at(0).contains_id = 48;
	source.objects.at(0).contains_count = 1;
	const auto absent = prepare_named_audio(Gamesave_current_filename, source, ordinal);
	require(snapshot() == initial && world() == before, "Preparing absent transitions mutated the live owner or world");
	// Reuse the fixture's labeled synthetic WAV through the native resource owner
	const auto wave = load_audio_resource("sound.wav", Gamesave_current_filename, ordinal);
	std::filesystem::create_directories(folder);
	const auto write = [](const std::filesystem::path &path, const std::string &bytes) {
		std::ofstream file(path, std::ios::binary);
		file.write(bytes.data(), std::streamsize(bytes.size()));
		require(bool(file), "Cannot write derived transition cue");
	};
	write(slow, wave.bytes);
	write(fast, wave.bytes);
	const auto present = prepare_named_audio(Gamesave_current_filename, source, ordinal);
	write(slow, "not a RIFF WAVE");
	bool rejected = false;
	try {
		(void) prepare_named_audio(Gamesave_current_filename, source, ordinal);
	} catch (const std::runtime_error &error) {
		rejected = std::string(error.what()).find("Invalid XL object sound") != std::string::npos;
	}
	require(rejected && snapshot() == initial && world() == before, "Malformed transition cue passed or mutated live state");
	std::filesystem::remove(slow);
	std::filesystem::remove(fast);
	auto receipts = nlohmann::json::array();
	for (const bool available : { false, true }) {
		install_named_audio(available ? present : absent);
		xl_named_audio_slow_motion_reset();
		const auto diagnostic = named_audio_diagnostics();
		require(diagnostic.at("unavailable_transition_output").size() == (available ? 0u : 2u), "Prepared transition availability changed after files were removed");
		require(diagnostic.at("unavailable_ambient_output").empty(), "Transition absence was mislabeled as ambient absence");
		SlowMotionState state;
		SlowMotionInput input;
		input.xl = input.mixer = true;
		input.equipment = slow_motion_equipment | bullet_time_equipment;
		input.energy = 100 * F1_0;
		input.frame = 1092;
		auto trajectory = nlohmann::json::array();
		for (int step = 0; step < 420; ++step) {
			input.now_ms += step % 3 ? 26 : 25;
			input.slow_pressed = step == 0 || step == 20 || step == 40 || step == 250;
			input.bullet_pressed = step == 150 || step == 180;
			input.force_off = step == 350;
			const auto output = slow_motion_frame(state, input, slow_motion_audio_event);
			input.energy += output.energy_delta;
			auto clocks = nlohmann::json::array();
			for (const auto &clock : state.clocks) clocks.push_back({ clock.speed, clock.direction, clock.updated_ms, clock.active });
			auto events = nlohmann::json::array();
			for (const auto event : output.events) events.push_back(int(event));
			trajectory.push_back({ { "clocks", clocks }, { "energy", input.energy }, { "delta", output.energy_delta }, { "consumed", output.consumed_controls }, { "events", events }, { "transition_sound", state.transition_sound } });
		}
		receipts.push_back(trajectory);
		// Actual callback output distinguishes retained PCM from absent channels
		begin_capture();
		xl_named_audio_slow_motion_reset();
		const auto empty = snapshot();
		const bool played = slow_motion_audio_event(SlowMotionEvent::play_slowdown);
		require(played == available, "Transition callback reported fabricated or missing playback");
		if (!available) require(snapshot().at("voices").empty() && snapshot().at("next") == empty.at("next") && snapshot().at("cursor") == empty.at("cursor"), "Absent transition acquired a channel or serial");
		xl_named_audio_pause(0);
		run_output(2048);
		const bool audible = std::any_of(captured.begin(), captured.begin() + captured_samples, [](auto sample) { return sample != 0; });
		require(audible == available, "Transition actual PCM availability differs");
		const auto saved = snapshot();
		require(restore_named_audio(saved, true), "Transition owner failed save-state restoration");
	}
	unsigned stops = 0;
	for (unsigned step = 0; step < receipts[0].size(); ++step) {
		const auto &absent_row = receipts[0][step], &present_row = receipts[1][step];
		for (const char *key : { "clocks", "energy", "delta", "consumed" })
			require(absent_row.at(key) == present_row.at(key), "Cue absence changed time-equipment gameplay state");
		require(!absent_row.at("transition_sound").get<bool>(), "Missing cue fabricated a transition handle");
		auto expected = nlohmann::json::array();
		for (const auto &event : present_row.at("events")) {
			if (event == int(SlowMotionEvent::stop_transition_sound)) ++stops;
			else expected.push_back(event);
		}
		require(absent_row.at("events") == expected, "Cue absence changed events beyond stopping its unavailable handle");
	}
	require(stops > 0, "Transition comparison did not exercise stop handles");
	xl_named_audio_slow_motion_reset();
	install_named_audio(original_bank);
	require(restore_named_audio(initial, true) && world() == before, "Transition control changed native world, objects or SIM RNG");
	std::ofstream receipt(root / ("transition-audio-" + std::to_string(ordinal) + ".json"));
	receipt << receipts.dump(2) << '\n';
	require(bool(receipt), "Cannot preserve transition trajectory receipt");
	std::cout << "Transition audio passed: retained present/absent owners, malformed rejection, actual PCM, clocks/energy/controls, save state and unchanged world/object/SIM state\n";
}
} // namespace xl_transition_audio_control
#endif
