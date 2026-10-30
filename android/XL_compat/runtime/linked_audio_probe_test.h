// Android-only diagnostic fixture, using the same captured PCM oracle as native
void compare_linked_output(const json &state)
{
	xl_shared_audio_pcm::output(state, captured.data(), captured_music.data(), captured_samples, 20);
	for (std::size_t i = 0; i < captured_samples; ++i)
		if (captured_music[i]) ++music_samples;
	checked_samples += captured_samples;
	++output_checks;
	++linked_checks;
}
void prepare_linked()
{
	reset_events();
	capture_begin();
	auto position = Viewer->pos;
	vm_vec_scale_add2(&position, &Viewer->orient.rvec, 10 * F1_0);
	vm_vec_scale_add2(&position, &Viewer->orient.fvec, 10 * F1_0);
	const int source = obj_create(OBJ_MARKER, 0, Viewer->segnum, &position, &Viewer->orient, F1_0, CT_NONE, MT_NONE, RT_NONE);
	require(source >= 0, "Cannot create Android linked source");
	require(xl_audio_link_object(0, 3, source, 1, F1_0, 64 * F1_0, -1, -1, 0, 0) > 0, "Android linked object did not start");
	xl_audio_sync_objects();
	auto starting = snapshot();
	require(starting.at("voices").size() == 1 && starting.at("linked").at("owners").size() == 1, "Android linked sound did not enter the shared pool");
	xl_named_audio_pause(0);
	capture_run(4096);
	compare_linked_output(starting);
	vm_vec_scale_add2(&Objects[source].pos, &Viewer->orient.rvec, -20 * F1_0);
	capture_begin();
	const auto moved = snapshot();
	require(moved.at("linked").at("owners")[0].at("serial") == starting.at("linked").at("owners")[0].at("serial") &&
	            moved.at("linked").at("owners")[0].at("pan") != starting.at("linked").at("owners")[0].at("pan"),
	        "Android moving source lost its identity or failed to update pan");
	capture_run(4096);
	compare_linked_output(moved);
	reset_events();
	Objects[source].pos = position;
	capture_begin();
	require(xl_audio_link_object(0, 3, source, 1, F1_0, 64 * F1_0, -1, -1, 0, 0) > 0 &&
	            xl_audio_link_position(1, Viewer->segnum, 0, &position, 1, F1_0, 64 * F1_0) > 0,
	        "Cannot prepare Android saved linked sources");
	auto distant = position;
	distant.x += 500 * F1_0;
	require(xl_audio_link_position(0, Viewer->segnum, 1, &distant, 1, F1_0, 64 * F1_0) > 0, "Cannot prepare Android inaudible owner");
	xl_audio_defer_objects(1);
	require(xl_audio_link_position(1, Viewer->segnum, 2, &position, 1, F1_0, 64 * F1_0) > 0, "Cannot prepare Android deferred owner");
	++GameTime64;
	require(xl_named_audio_action(&Triggers[2], 2) && xl_audio_start_sample(1, 2, F1_0 / 2, 32767, 1, -1, -1) >= 0, "Cannot prepare Android named/unlinked companions");
	xl_audio_sync_objects();
	starting = snapshot();
	require(starting.at("linked").at("owners").size() == 4 && starting.at("voices").size() == 4 &&
	            starting.at("linked").at("owners")[2].at("channel") == -1 && starting.at("linked").at("owners")[3].at("channel") == -1,
	        "Android linked save fixture lost stopped owners or mixed voices");
	xl_named_audio_pause(0);
	capture_run(4096);
	compare_linked_output(starting);
	xl_named_audio_pause(1);
	earned = snapshot();
	for (unsigned variant = 0; variant < 3; ++variant) {
		auto bad = earned;
		auto &entry = bad["linked"]["owners"][0];
		if (variant == 0) entry["signature"] = entry["signature"].get<int>() + 1;
		if (variant == 1) entry["channel_serial"] = bad["next"];
		if (variant == 2) entry["sample"] = 0;
		require(!xl_compat::restore_named_audio(bad, false) && !xl_compat::restore_named_audio(bad, true) && snapshot() == earned, "Android malformed linked state changed playback");
	}
	energy = Players[Player_num].energy;
	version = xl_source_level_version();
	pressure_earned = shared_earned = false;
	linked_earned = true;
	SDL_PauseAudio(0);
	verify();
}
