// Included inside the Android audio probe namespace to reuse its real PCM capture
json seismic_earned, seismic_audio_earned;
vms_vector seismic_rotation{};
int seismic_version = 0, seismic_energy = 0;
unsigned seismic_verifies = 0, seismic_event_checks = 0;
void verify_seismic()
{
	require(seismic_version && xl_source_level_version() == seismic_version && xl_compat::seismic_gameplay_snapshot() == seismic_earned && snapshot() == seismic_audio_earned && Players[Player_num].energy == seismic_energy && !std::memcmp(&seismic_rotation, &ConsoleObject->mtype.phys_info.rotvel, sizeof(seismic_rotation)), "Android seismic persistence lost exact physics, cadence, distance, pending action or audio");
	++seismic_verifies;
}
void seismic_output()
{
	capture_begin();
	const auto state = snapshot();
	xl_named_audio_pause(0);
	capture_run(4096);
	require(!invalid && captured_samples > 0, "Invalid seismic PCM capture");
	xl_shared_audio_pcm::output(state, captured.data(), captured_music.data(), captured_samples, 20, 0);
	checked_samples += captured_samples;
	++seismic_checks;
	xl_named_audio_pause(1);
}
void seismic_control(const char *operation)
{
	using namespace xl_seismic_controls;
	if (!std::strcmp(operation, "stock")) {
		const auto before = seismic_gameplay_snapshot();
		trigger action{};
		action.type = 36;
		action.value = F1_0;
		action.time = 3;
		require(!xl_level_extensions_active() && !xl_seismic_action(&action) && !xl_seismic_frame() && !xl_seismic_detonation(nullptr) && seismic_gameplay_snapshot() == before && before[0] == 0 && before[1] == 0 && before[2].is_null() && before[5] == 0 && before[6] == 0, "XL seismic gameplay leaked into Android Redux");
	} else if (!std::strcmp(operation, "disk_load")) {
		SDL_PauseAudio(1);
		require(seismic_version && state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore Android seismic disk save");
		verify_seismic();
		require(android_open_overlay_pause_if_safe(), "Cannot pause restored seismic controls");
		xl_named_audio_pause(1);
		SDL_PauseAudio(0);
	} else {
		xl_seismic_controls::fixture();
		if (!std::strcmp(operation, "events")) {
			xl_seismic_controls::events();
			SDL_PauseAudio(1);
			digi_set_digi_volume(32768);
			xl_seismic_audio_control::run(seismic_output);
			++seismic_event_checks;
			SDL_PauseAudio(0);
		} else if (!std::strcmp(operation, "prepare")) {
			SDL_PauseAudio(1);
			xl_seismic_controls::prepare();
			seismic_output();
			require(android_open_overlay_pause_if_safe(), "Cannot freeze earned seismic controls");
			xl_named_audio_pause(1);
			seismic_earned = seismic_gameplay_snapshot();
			seismic_audio_earned = snapshot();
			seismic_rotation = ConsoleObject->mtype.phys_info.rotvel;
			seismic_energy = Players[Player_num].energy;
			seismic_version = xl_source_level_version();
			verify_seismic();
			for (const char *name : { "badquake.rl2", "badqobj.rl2" }) require(!xl_level_supported(name, 0) && std::strstr(xl_level_rejection(), "seismic"), "Android admitted overflowing mine-shake duration");
			SDL_PauseAudio(0);
		} else if (!std::strcmp(operation, "verify")) {
			verify_seismic();
			xl_seismic_controls::paused();
		} else if (!std::strcmp(operation, "disturb")) {
			verify_seismic();
			trigger action{};
			action.type = 36;
			action.value = F1_0;
			action.time = 1;
			require(xl_seismic_action(&action), "Cannot disturb pending mine-shake state");
			++ConsoleObject->mtype.phys_info.rotvel.x;
			Players[Player_num].energy -= F1_0;
		} else if (!std::strcmp(operation, "disk_save")) {
			verify_seismic();
			require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "XL seismic continuation", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save Android seismic state");
		} else if (!std::strcmp(operation, "continue")) xl_seismic_controls::continuation();
		else if (!std::strcmp(operation, "next")) {
			require(!game_is_time_paused() && Current_level_num == 1 && Last_level == 2, "Seismic transition requires the running fixture");
			StartNewLevelSub(2, 1, 0);
			const auto fresh = seismic_gameplay_snapshot();
			require(xl_source_level_version() == 27 && fresh[2].is_null() && std::all_of(fresh[3].begin(), fresh[3].end(), [](const auto &row) { return row[0].is_null(); }), "Seismic level transition retained an old disturbance or detonation");
		} else throw std::runtime_error("Unknown seismic operation");
	}
}
