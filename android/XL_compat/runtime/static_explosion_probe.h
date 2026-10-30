// Included by the Android audio diagnostic owner; reuse its real driver capture
json static_earned;
std::array<vclip_info, 2> static_frames{};
fix static_energy = 0;
int static_level = 0;
unsigned static_checks = 0, static_gpu_checks = 0;
void static_fixture()
{
	require(game_is_time_paused() && xl_level_extensions_active() && Current_mission && !std::strcmp(Current_mission_filename, "xstatic") &&
	            Highest_object_index >= 50 && Objects[49].type == 19 && Objects[50].type == 19 && *android_mission_assets_key(),
	        "Static explosion control requires the paused published fixture");
	require(!gamesave_level_data_warning_count(), "Android repaired the authored static explosion objects");
}
void static_verify()
{
	static_fixture();
	require(!static_earned.is_null() && Current_level_num == static_level && Players[Player_num].energy == static_energy && snapshot() == static_earned,
	        "Android static explosion restore lost level, player state or audio phase");
	for (unsigned i = 0; i < 2; ++i) {
		const auto &state = Objects[49 + i].rtype.vclip_info;
		require(state.vclip_num == static_frames[i].vclip_num && state.frametime == static_frames[i].frametime && state.framenum == static_frames[i].framenum,
		        "Android static explosion restore lost its earned animation phase");
	}
}
void static_output()
{
	static_fixture();
	capture_begin();
	const auto starting = snapshot();
	require(starting.at("voices").size() >= 2, "Android static explosion lacks mixed looping voices");
	xl_named_audio_pause(0);
	capture_run(4096);
	xl_shared_audio_pcm::output(starting, captured.data(), captured_music.data(), captured_samples, 20, 0);
	checked_samples += captured_samples;
	++output_checks;
	++static_checks;
	xl_named_audio_pause(1);
	SDL_PauseAudio(0);
}
void static_control(const char *operation)
{
	if (!std::strcmp(operation, "disk_load")) {
		require(!static_earned.is_null() && state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore Android static explosion save");
		require(android_open_overlay_pause_if_safe(), "Cannot pause restored static explosion mission");
		static_verify();
		return;
	}
	if (!std::strcmp(operation, "next2") || !std::strcmp(operation, "next3") || !std::strcmp(operation, "secret")) {
		require(!game_is_time_paused() && xl_level_extensions_active(), "Static transitions require running XL gameplay");
		const int ordinal = !std::strcmp(operation, "secret") ? -1 : !std::strcmp(operation, "next3") ? 3
		                                                                                              : 2;
		StartNewLevelSub(ordinal, 1, 0);
		require(Current_level_num == ordinal && Objects[49].type == 19 && Objects[50].type == 19, "Static transition lost its authored objects");
		return;
	}
	static_fixture();
	if (!std::strcmp(operation, "prepare")) {
		SDL_PauseAudio(1);
		set_sound_sources();
		xl_audio_sync_objects();
		const auto time = FrameTime;
		for (int index : { 49, 50 }) {
			auto &object = Objects[index];
			const auto &clip = Vclip[object.id];
			require(object.control_type == CT_POWERUP && object.movement_type == MT_NONE && object.render_type == RT_POWERUP &&
			            object.rtype.vclip_info.vclip_num == object.id && object.lifeleft == IMMORTAL_TIME && clip.num_frames > 3,
			        "Android static initializer differs from source identity/clip/lifetime");
			object.rtype.vclip_info.frametime = 0;
			object.rtype.vclip_info.framenum = 0;
			FrameTime = clip.frame_time;
			do_powerup_frame(&object);
			require(object.rtype.vclip_info.frametime == 0 && object.rtype.vclip_info.framenum == 1, "Android static animation reversed or introduced slot fudge");
			FrameTime = clip.frame_time + 1;
			do_powerup_frame(&object);
			require(object.rtype.vclip_info.frametime == clip.frame_time - 1 && object.rtype.vclip_info.framenum == 3, "Android static animation boundary differs");
		}
		FrameTime = time;
		static_output();
		static_earned = snapshot();
		static_level = Current_level_num;
		static_energy = Players[Player_num].energy;
		for (unsigned i = 0; i < 2; ++i) static_frames[i] = Objects[49 + i].rtype.vclip_info;
		static_verify();
	} else if (!std::strcmp(operation, "gpu")) {
		static_gpu_checks += xl_static_explosion_gpu_check();
	} else if (!std::strcmp(operation, "verify")) {
		static_verify();
		++static_checks;
	} else if (!std::strcmp(operation, "output")) {
		static_verify();
		static_output();
		require(xl_compat::restore_named_audio(static_earned, true), "Cannot recover static audio after capture");
		static_verify();
	} else if (!std::strcmp(operation, "disturb")) {
		static_verify();
		Players[Player_num].energy -= F1_0;
		Objects[49].rtype.vclip_info.frametime = 0;
		Objects[50].rtype.vclip_info.framenum = 0;
		xl_named_audio_stop();
	} else if (!std::strcmp(operation, "disk_save")) {
		static_verify();
		require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "XL static explosions", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save Android static explosion state");
	} else if (!std::strcmp(operation, "reject")) {
		const auto before = snapshot();
		for (const char *name : { "stbadid.rl2", "stbadfr.rl2", "stbadct.rl2", "stbadmv.rl2", "stbadrn.rl2", "stbadtm.rl2" })
			require(!xl_level_supported_at(name, 0, 1) && *xl_level_rejection() && snapshot() == before && Objects[49].type == 19,
			        "Malformed static object was admitted or changed current Android state");
		static_checks += 6;
	} else throw std::runtime_error("Unknown Android static explosion operation");
}
