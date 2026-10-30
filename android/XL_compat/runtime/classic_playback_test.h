// Loaded replacement consumers and actual output using independent labeled WAVs
#include "classic_output_test.h"
namespace xl_classic_playback_control
{
using namespace named_audio_control;
std::uint64_t checked = 0;
void output(const nlohmann::json &state, int ordinal)
{
	xl_classic_output::compare(state, ordinal, captured.data(), nullptr, captured_samples, 128);
	checked += captured_samples;
}
void capture_output(int ordinal, unsigned frames = 4096)
{
	const auto earned = snapshot();
	require(!earned.at("voices").empty(), "Classic replacement output needs a live voice");
	begin_capture();
	xl_named_audio_pause(0);
	run_output(frames);
	output(earned, ordinal);
}
void run(const std::filesystem::path &root, const std::string &filename)
{
	checked = 0;
	std::map<int, nlohmann::json> saved;
	int logical = -1;
	for (int id = 0; id < MAX_SOUNDS; ++id)
		if (xl_gameplay_sound_sample(id) == 0) {
			logical = id;
			break;
		}
	require(logical >= 0, "Classic replacement fixture lacks a logical route to sample zero");
	for (int ordinal : { 1, 2, -1, 3, 1 }) {
		LoadLevel(ordinal, 0);
		SDL_PauseAudio(1);
		require(Current_level_num == ordinal && xl_level_extensions_active(), "Classic replacement level transition failed");
		digi_stop_digi_sounds();
		digi_set_digi_volume(32768);
		GameTime64 = 10 * F1_0;
		const auto initial = snapshot();
		// Use the ordinary engine entry point, then linked object and position routes
		digi_play_sample(logical, F1_0);
		require(snapshot().at("voices").size() == 1 && snapshot().at("voices")[0][2] == 4,
		        "Ordinary engine sound did not select the replacement sample");
		capture_output(ordinal, 23074);
		require(snapshot().at("voices").empty(), "Finite replacement voice did not complete at the authored WAV length");
		require(xl_compat::restore_named_audio(initial, true), "Cannot reset replacement route controls");
		Viewer = ConsoleObject;
		require(xl_gameplay_sound_object(logical, XL_SOUND_AMBIENT, Players[Player_num].objnum, 1, F1_0, 256 * F1_0, -1, -1, 0) >= 0,
		        "Cannot create replacement object owner");
		require(xl_gameplay_sound_position(logical, ConsoleObject->segnum, 0, &ConsoleObject->pos, 1, F1_0 / 2, 256 * F1_0) >= 0,
		        "Cannot create replacement position owner");
		xl_audio_sync_objects();
		require(snapshot().at("linked").at("owners").size() == 2 && snapshot().at("voices").size() == 2,
		        "Replacement linked routes lost owners or channels");
		require(xl_named_audio_action(&Triggers[0], 0), "Cannot mix replacement owners with named playback");
		capture_output(ordinal, 45124);
		require(snapshot().at("voices").size() == 2, "Replacement loops stopped or finite named playback failed to finish");
		xl_named_audio_pause(1);
		const auto earned = snapshot();
		for (const auto &[previous, state] : saved) {
			if (previous == ordinal) require(state.at("source") == earned.at("source"), "Returning replacement scope changed resource identity");
			else require(state.at("source") != earned.at("source") && !xl_compat::restore_named_audio(state, true) && snapshot() == earned,
				         "Wrong replacement scope restored or changed live state");
		}
		saved[ordinal] = earned;
		char save[] = "classic-playback.sav", description[] = "XL classic replacement";
		require(state_save_all_sub(save, description), "Cannot save replacement playback");
		char error[256]{};
		require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(), "Replacement stock transition failed");
		require(snapshot().at("source") == "" && snapshot().at("voices").empty(), "Replacement bank leaked into Redux");
		require(state_restore_all_sub(save, 0) && Current_level_num == ordinal && snapshot() == earned,
		        "Disk-through-stock replacement restore changed sample bindings or playback");
		capture_output(ordinal);
		// Rate reopening must resample the WAV and keep earned positions and owners
		xl_named_audio_backend_close();
		Mix_CloseAudio();
		require(Mix_OpenAudio(48000, AUDIO_S16SYS, 2, 1024) == 0, "Cannot reopen replacement output at 48 kHz");
		SDL_PauseAudio(1);
		require(xl_named_audio_backend_open(48000, AUDIO_S16SYS, 2) && xl_compat::restore_named_audio(earned, true), "Cannot restore replacement voices at 48 kHz");
		const auto resampled = snapshot();
		for (std::size_t i = 0; i < earned.at("voices").size(); ++i)
			require(resampled.at("voices")[i][3] == earned.at("voices")[i][3].get<std::uint64_t>() * 48000 / 44100, "Replacement reopen lost its playhead");
		capture_output(ordinal);
		xl_named_audio_backend_close();
		Mix_CloseAudio();
		require(Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 1024) == 0, "Cannot recover replacement output at 44.1 kHz");
		SDL_PauseAudio(1);
		require(xl_named_audio_backend_open(44100, AUDIO_S16SYS, 2), "Cannot reopen original replacement backend");
	}
	require(saved.size() == 4, "Replacement playback omitted a normal, secret or fallback scope");
	// A valid edit at the same path must invalidate old saved voices, even when
	// the new WAV keeps the same format and duration
	const auto path = root / "mods" / "xscope" / "sounds" / "level01" / filename;
	std::ifstream file(path, std::ios::binary);
	const xl_compat::Bytes original{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	file.close();
	require(original.size() > 48, "Missing replacement mutation fixture");
	auto changed = original;
	changed[48] ^= 1;
	write_bytes(path.string().c_str(), changed);
	LoadLevel(1, 0);
	SDL_PauseAudio(1);
	const auto changed_state = snapshot();
	const bool rejected = changed_state.at("source") != saved.at(1).at("source") &&
	                      !xl_compat::restore_named_audio(saved.at(1), true) && snapshot() == changed_state;
	write_bytes(path.string().c_str(), original);
	require(rejected, "Changed replacement bytes accepted stale saved playback or partially changed state");
	LoadLevel(1, 0);
	SDL_PauseAudio(1);
	require(xl_compat::restore_named_audio(saved.at(1), true) && snapshot() == saved.at(1), "Restoring replacement bytes did not recover the original bank identity");
	capture_output(1);
	std::cout << "Classic replacement playback passed: four scopes, logical/object/position consumers, mixed named output, disk-through-stock and rate reopen, " << checked << " actual PCM samples\n";
}
} // namespace xl_classic_playback_control
