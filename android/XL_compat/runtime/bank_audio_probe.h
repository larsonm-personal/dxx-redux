// Included by the Android owner after the shared capture helpers
json bank_earned;
fix bank_energy = 0;
int bank_level = 0;
unsigned bank_checks = 0;
void bank_fixture()
{
	fixture();
	require(game_is_time_paused() && Current_mission && (!std::strcmp(Current_mission_filename, "xbank") || !std::strcmp(Current_mission_filename, "xbkhog")) && *android_mission_assets_key(),
	        "Bank control requires the paused published mission");
	const char *path = !std::strcmp(Current_mission_filename, "xbkhog") ? "xbkhog.s22" : "mods/xbank/xbank.s22";
	require(xl_compat::named_audio_diagnostics().at("classic_replacements") == json{ { "laser01", path } },
	        "Android selected WAVs or the wrong custom bank");
}
void bank_verify()
{
	bank_fixture();
	require(!bank_earned.is_null() && Current_level_num == bank_level && Players[Player_num].energy == bank_energy && snapshot() == bank_earned,
	        "Bank restore lost its level, earned player state, owners or sample phase");
}
void bank_output(unsigned frames = 4096)
{
	bank_fixture();
	capture_begin();
	const auto before = snapshot();
	require(!before.at("voices").empty(), "Bank output requires an active voice");
	xl_named_audio_pause(0);
	capture_run(frames);
	xl_bank_output::compare(before, captured.data(), captured_music.data(), captured_samples, 20, classic_laser_sample() + 4, 200, 22050, 11025);
	checked_samples += captured_samples;
	++output_checks;
	++bank_checks;
	xl_named_audio_pause(1);
	SDL_PauseAudio(0);
}
void bank_control(const char *operation)
{
	if (!std::strcmp(operation, "wave_next")) {
		require(!game_is_time_paused() && Current_mission && !std::strcmp(Current_mission_filename, "xhwav"), "Archive WAV transition requires its running mission");
		StartNewLevelSub(2, 1, 0);
		return;
	}
	if (!std::strcmp(operation, "wave")) {
		fixture();
		require(game_is_time_paused() && Current_mission && !std::strcmp(Current_mission_filename, "xhwav") && *android_mission_assets_key(), "Archive WAV control requires its paused published mission");
		const char *path = Current_level_num == 2 ? "mods/xhwav/sounds/level02/laser01.wav" : "laser01.wav";
		require(xl_compat::named_audio_diagnostics().at("classic_replacements") == json{ { "laser01", path } }, "Android archive/level WAV precedence differs");
		int logical = -1;
		for (int id = 0; id < MAX_SOUNDS; ++id)
			if (xl_gameplay_sound_sample(id) == classic_laser_sample()) {
				logical = id;
				break;
			}
		require(logical >= 0, "Archive WAV lacks its ordinary sample route");
		reset_events();
		capture_begin();
		digi_play_sample(logical, F1_0);
		const auto before = snapshot();
		require(before.at("voices").size() == 1, "Archive WAV did not start its voice");
		xl_named_audio_pause(0);
		capture_run(4096);
		xl_classic_output::compare(before, Current_level_num == 2 ? 2 : 3, captured.data(), captured_music.data(), captured_samples, 20, classic_laser_sample() + 4);
		checked_samples += captured_samples;
		++bank_checks;
		++output_checks;
		xl_named_audio_pause(1);
		SDL_PauseAudio(0);
		return;
	}
	if (!std::strcmp(operation, "disk_load")) {
		require(!bank_earned.is_null() && state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore Android mission bank");
		require(android_open_overlay_pause_if_safe(), "Cannot pause restored bank mission");
		bank_verify();
		return;
	}
	if (!std::strcmp(operation, "next2") || !std::strcmp(operation, "next3") || !std::strcmp(operation, "secret")) {
		require(!game_is_time_paused() && Current_mission && (!std::strcmp(Current_mission_filename, "xbank") || !std::strcmp(Current_mission_filename, "xbkhog")), "Bank transition requires running fixture gameplay");
		const int ordinal = !std::strcmp(operation, "secret") ? -1 : !std::strcmp(operation, "next3") ? 3
		                                                                                              : 2;
		StartNewLevelSub(ordinal, 1, 0);
		require(Current_level_num == ordinal && xl_level_extensions_active(), "Bank transition selected the wrong level");
		return;
	}
	bank_fixture();
	if (!std::strcmp(operation, "prepare")) {
		int logical = -1;
		for (int id = 0; id < MAX_SOUNDS; ++id)
			if (xl_gameplay_sound_sample(id) == classic_laser_sample()) {
				logical = id;
				break;
			}
		require(logical >= 0, "Android bank lacks a logical sample route");
		reset_events();
		SDL_PauseAudio(1);
		digi_play_sample(logical, F1_0);
		require(snapshot().at("voices").size() == 1, "Bank ordinary sound did not start");
		bank_output(26000);
		require(snapshot().at("voices").empty(), "Android bank sound did not finish at the authored length");
		reset_events();
		SDL_PauseAudio(1);
		Viewer = ConsoleObject;
		require(xl_gameplay_sound_object(logical, XL_SOUND_AMBIENT, Players[Player_num].objnum, 1, F1_0, 256 * F1_0, -1, -1, 0) >= 0 &&
		            xl_gameplay_sound_position(logical, ConsoleObject->segnum, 0, &ConsoleObject->pos, 1, F1_0 / 2, 256 * F1_0) >= 0,
		        "Android bank linked routes failed");
		xl_audio_sync_objects();
		require(snapshot().at("voices").size() == 2, "Android bank did not start both linked voices");
		bank_output();
		Players[Player_num].energy = 73 * F1_0 + 123;
		bank_energy = Players[Player_num].energy;
		bank_level = Current_level_num;
		bank_earned = snapshot();
		bank_verify();
	} else if (!std::strcmp(operation, "verify")) {
		bank_verify();
		++bank_checks;
	} else if (!std::strcmp(operation, "output")) {
		bank_verify();
		bank_output();
		require(xl_compat::restore_named_audio(bank_earned, true), "Cannot recover bank output phase");
		bank_verify();
	} else if (!std::strcmp(operation, "disturb")) {
		bank_verify();
		Players[Player_num].energy -= F1_0;
		xl_named_audio_stop();
	} else if (!std::strcmp(operation, "disk_save")) {
		bank_verify();
		require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "XL mission bank", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save Android mission bank state");
	} else throw std::runtime_error("Unknown Android bank operation");
}
