// Real mixed callbacks and persisted channel controls through the production bank
namespace xl_shared_audio_control
{
using namespace named_audio_control;
void compare_output(const nlohmann::json &state)
{
	xl_shared_audio_pcm::output(state, captured.data(), nullptr, captured_samples, 128);
}
void malformed(const nlohmann::json &earned)
{
	for (unsigned variant = 0; variant < 11; ++variant) {
		auto bad = earned;
		auto &voice = bad["voices"][0];
		require(voice[7] == 2, "Shared rejection controls need an unlinked first voice");
		if (variant == 0) voice[7] = 1;
		if (variant == 1) voice[7] = 3;
		if (variant == 2) voice[8] = 7;
		if (variant == 3) voice[9] = 1;
		if (variant == 4) voice[10] = 129;
		if (variant == 5) voice[11] = 256;
		if (variant == 6) {
			voice[11] = 0;
			voice[12] = 0;
		}
		if (variant == 7) voice[6] = INT32_MAX;
		if (variant == 8) voice[2] = 0;
		if (variant == 9) voice[4] = 0;
		if (variant == 10) voice[10] = voice[10].get<int>() ? 0 : 1;
		require(!xl_compat::restore_named_audio(bad, false) && !xl_compat::restore_named_audio(bad, true) && snapshot() == earned, "Malformed shared channel state was accepted or changed playback");
	}
}
void run(const std::filesystem::path &root, const nlohmann::json &initial)
{
	require(xl_compat::named_audio_diagnostics().at("classic_samples") == 183 && xl_compat::named_audio_diagnostics().at("resources") == 187, "Shared bank omitted classic or named samples");
	SDL_PauseAudio(1);
	require(xl_compat::restore_named_audio(initial, true), "Cannot reset shared audio controls");
	digi_set_digi_volume(32768);
	const int ordinary = xl_audio_start_sample(0, 3, F1_0 / 2, 0, 1, -1, -1);
	require(ordinary >= 0 && xl_audio_channel_playing(ordinary), "Ordinary sound did not enter the live shared pool");
	++GameTime64;
	require(xl_named_audio_action(&Triggers[0], 0) == 1, "Cannot mix a named action with ordinary playback");
	auto earned = snapshot();
	require(earned.at("voices").size() == 2, "Ordinary and named sounds did not share the same registry");
	begin_capture();
	xl_named_audio_pause(0);
	run_output(4096);
	compare_output(earned);
	require(xl_audio_set_channel_volume(ordinary, 2 * F1_0) && xl_audio_set_channel_pan(ordinary, 65536), "Live ordinary channel controls failed");
	earned = snapshot();
	// Source SetFxVolume updates linked owners, not existing unlinked voices
	digi_set_digi_volume(0);
	begin_capture();
	run_output(4096);
	compare_output(earned);
	digi_set_digi_volume(32768);
	xl_audio_stop_channel(ordinary);
	require(!xl_audio_channel_playing(ordinary) && snapshot().at("voices").size() == 1, "Stopping an ordinary channel stopped an unrelated named voice");

	require(xl_compat::restore_named_audio(initial, true), "Cannot reset mixed pressure controls");
	const unsigned origin = snapshot().at("cursor").get<unsigned>();
	for (unsigned i = 0; i < 128; ++i) {
		if (i < 64) require(xl_audio_start_sample(int(i % 2), 3, i == 17 ? 8192 : F1_0, int(i * 1024), 1, -1, -1) >= 0, "Cannot fill the ordinary half of the shared pool");
		else {
			++GameTime64;
			require(xl_named_audio_action(&Triggers[1], 1) == 1, "Cannot fill the named half of the shared pool");
		}
	}
	require(snapshot().at("voices").size() == 128, "Mixed sound banks did not use exactly 128 shared channels");
	const int replaced = xl_audio_start_sample(1, 3, F1_0, 16384, 1, -1, -1);
	require(replaced == int((origin + 17) % 128), "Ordinary full-pool replacement ignored its class/volume priority");
	++GameTime64;
	require(xl_named_audio_action(&Triggers[1], 1) == 1, "Named replacement failed in the mixed pool");
	earned = snapshot();
	require(earned.at("voices")[(origin + 64) % 128][1] == earned.at("next").get<std::uint64_t>() - 1, "Named full-pool replacement selected another class");
	begin_capture();
	xl_named_audio_pause(0);
	run_output(4096);
	compare_output(earned);
	earned = snapshot();
	require(std::all_of(earned.at("voices").begin(), earned.at("voices").end(), [](const auto &row) { return row[3] > 0; }), "Mixed persistence did not earn nonzero playheads");
	// A separate single ordinary voice exercises every malformed row independently of slot order
	const auto full = earned;
	require(xl_compat::restore_named_audio(initial, true), "Cannot reset shared rejection controls");
	require(xl_audio_start_sample(0, 3, F1_0, 32767, 1, -1, -1) >= 0, "Cannot start shared rejection voice");
	malformed(snapshot());
	require(xl_compat::restore_named_audio(full, true), "Cannot restore full mixed playback");
	char save[] = "shared-audio.sav", description[32] = "XL shared audio";
	require(state_save_all_sub(save, description), "Cannot save mixed ordinary/named playback");
	char error[256]{};
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(), "Shared audio stock transition failed");
	require(snapshot().at("voices").empty() && snapshot().at("source") == "" && xl_audio_start_sample(0, 3, F1_0, 32767, 1, -1, -1) < 0, "Shared voices or entry points leaked into Redux");
	require(state_restore_all_sub(save, 0) && snapshot() == full, "Disk-through-stock restore changed shared gain, pan, class, playheads or allocation");
	begin_capture();
	xl_named_audio_pause(0);
	run_output(4096);
	compare_output(full);
	xl_named_audio_backend_close();
	Mix_CloseAudio();
	require(Mix_OpenAudio(48000, AUDIO_S16SYS, 2, 1024) == 0, "Cannot reopen shared audio at 48 kHz");
	SDL_PauseAudio(1);
	require(xl_named_audio_backend_open(48000, AUDIO_S16SYS, 2) && xl_compat::restore_named_audio(full, true), "Cannot restore mixed channels at the new device rate");
	const auto resampled = snapshot();
	for (std::size_t i = 0; i < 128; ++i) {
		require(resampled.at("voices")[i][3] == full.at("voices")[i][3].get<std::uint64_t>() * 48000 / 44100, "Shared device reopen lost its playback phase");
		for (unsigned field = 7; field < 13; ++field) require(resampled.at("voices")[i][field] == full.at("voices")[i][field], "Shared device reopen changed channel controls");
	}
	begin_capture();
	xl_named_audio_pause(0);
	run_output(4096);
	compare_output(resampled);
	xl_named_audio_backend_close();
	Mix_CloseAudio();
	require(Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 1024) == 0, "Cannot restore shared audio's original device rate");
	SDL_PauseAudio(1);
	require(xl_named_audio_backend_open(44100, AUDIO_S16SYS, 2), "Cannot recover original shared output");
	require(xl_compat::restore_named_audio(initial, true), "Cannot leave shared controls at their initial state");
	Mix_SetPostMix(nullptr, nullptr);
	std::cout << "Shared native audio passed: 183 classic plus four named samples, mixed 128-channel output/replacement, live controls, 11 atomic rejections, disk-through-stock and 48 kHz reopen playback\n";
	(void) root;
}
} // namespace xl_shared_audio_control
