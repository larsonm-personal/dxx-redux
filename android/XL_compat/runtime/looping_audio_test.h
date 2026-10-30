namespace xl_looping_audio_control
{
void run(const nlohmann::json &initial)
{
	using namespace named_audio_control;
	using namespace xl_compat;
	using namespace xl_compat::audio_runtime;
	require(restore_named_audio(initial, true), "Cannot reset looping sound fixture");
	digi_play_sample_looping(7, F1_0, -1, -1);
	auto earned = snapshot();
	require(earned.at("loop").size() == 2 && earned.at("voices").size() == 1 && looping.matches(mixer), "Engine looping API did not select XL playback");
	const int slot = looping.channel;
	require(mixer.voices[unsigned(slot)].priority.volume == F1_0 / 2, "Full-scale looping volume was truncated");
	const auto output = [&] {
		const auto before = snapshot();
		begin_capture();
		xl_named_audio_pause(0);
		run_output(2048);
		xl_shared_audio_control::compare_output(before);
	};
	output();
	earned = snapshot();
	digi_play_sample_looping(9, F1_0, -1, -1);
	require(snapshot() == earned, "Missing logical loop replaced the live sound");
	digi_change_looping_volume(F1_0 / 4);
	require(looping.channel == slot && mixer.voices[unsigned(slot)].priority.volume == F1_0 / 8, "Engine loop volume missed its owned voice");
	output();
	earned = snapshot();
	for (unsigned variant = 0; variant < 10; ++variant) {
		auto bad = earned;
		if (variant == 0) bad.erase("loop");
		if (variant == 1) bad["loop"] = nullptr;
		if (variant == 2) bad["loop"] = { slot };
		if (variant == 3) bad["loop"][0] = -1;
		if (variant == 4) bad["loop"][0] = 128;
		if (variant == 5) bad["loop"][1] = 0;
		if (variant == 6) bad["loop"][1] = bad["next"];
		if (variant == 7) bad["loop"][1] = UINT64_MAX;
		if (variant == 8) bad["voices"][0][5] = 1;
		if (variant == 9) bad["voices"][0][8] = 2;
		require(!restore_named_audio(bad, false) && !restore_named_audio(bad, true) && snapshot() == earned, "Malformed loop binding changed playback");
	}
	digi_pause_digi_sounds();
	begin_capture();
	run_output(2048);
	require(snapshot() == earned && std::all_of(captured.begin(), captured.begin() + captured_samples, [](auto value) { return value == 0; }), "Engine pause lost looping phase or emitted PCM");
	digi_resume_digi_sounds();
	require(snapshot() == earned, "Engine resume restarted the XL loop");
	output();
	// The slot can be reused while the looping controller still holds its serial
	for (bool volume_first : { false, true }) {
		require(restore_named_audio(earned, true), "Cannot restore loop control state");
		xl_audio_stop_channel(slot);
		mixer.cursor = unsigned(slot);
		const int replacement = xl_audio_start_sample(1, XL_SOUND_GENERIC, F1_0, 32767, 1, -1, -1);
		require(replacement == slot && mixer.voices[unsigned(slot)].serial != looping.serial, "Loop replacement control did not reuse the slot");
		const auto replacement_voice = mixer.voices[unsigned(slot)];
		if (volume_first) digi_change_looping_volume(0);
		digi_stop_looping_sound();
		require(mixer.voices[unsigned(slot)].remaining == -1 && mixer.voices[unsigned(slot)].priority.volume == replacement_voice.priority.volume && snapshot().at("loop").empty(), "Stale loop volume or stop changed its replacement");
	}
	require(restore_named_audio(earned, true), "Cannot restore live loop before stopping");
	digi_stop_looping_sound();
	require(snapshot().at("loop").empty() && snapshot().at("voices").empty(), "Engine stop left the owned loop alive");
	require(restore_named_audio(initial, true), "Cannot leave looping sound controls at initial state");
	std::cout << "Native looping audio passed: engine dispatch, full-width volume, actual PCM, pause continuity, ten atomic malformed bindings and stale-slot isolation\n";
}
} // namespace xl_looping_audio_control
