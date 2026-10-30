// Native object/segment consumers and real PCM; source math is qualified separately
extern "C" int digi_looping_channel;
namespace xl_linked_audio_control
{
using namespace named_audio_control;
xl_compat::AudioOwner owner(std::int64_t serial)
{
	using namespace xl_compat::audio_runtime;
	AudioLock lock;
	for (unsigned i = 0; i < owners.count; ++i)
		if (owners.entries[i].serial == std::uint64_t(serial)) return owners.entries[i];
	throw std::runtime_error("Missing native linked sound identity");
}
void output()
{
	const auto before = snapshot();
	begin_capture();
	xl_named_audio_pause(0);
	run_output(2048);
	xl_shared_audio_control::compare_output(before);
}
void malformed(const nlohmann::json &earned)
{
	for (unsigned variant = 0; variant < 30; ++variant) {
		auto bad = earned;
		auto &linked = bad["linked"];
		auto &row = linked["owners"][0];
		if (variant == 0) bad.erase("linked");
		if (variant == 1) linked["next"] = 0;
		if (variant == 2) linked["next"] = UINT64_MAX;
		if (variant == 3) linked["deferred"] = 1;
		if (variant == 4) row["serial"] = linked["next"];
		if (variant == 5) row["channel_serial"] = bad["next"];
		if (variant == 6) row["flags"] = 63;
		if (variant == 7) row["sample"] = 0;
		if (variant == 8) row["sound"] = 183;
		if (variant == 9) row["class"] = 7;
		if (variant == 10) row["channel"] = 128;
		if (variant == 11) row["object"] = MAX_OBJECTS;
		if (variant == 12) row["signature"] = row["signature"].get<int>() + 1;
		if (variant == 13) row["segment"] = 0;
		if (variant == 14) row["position"][0] = 1;
		if (variant == 15) row["volume"] = -1;
		if (variant == 16) row["pan"] = 65537;
		if (variant == 17) row["max_distance"] = INT32_MAX;
		if (variant == 18) row["decay"] = 0;
		if (variant == 19) row["audio_volume"] = -2;
		if (variant == 20) row["ambient"] = bool(1);
		if (variant == 21) row["custom"] = 1;
		if (variant == 22) row["external"] = bool(1);
		if (variant == 23) linked["owners"][1]["serial"] = row["serial"];
		if (variant == 24) linked["owners"][1]["channel"] = row["channel"];
		if (variant == 25) linked["owners"][2]["channel_serial"] = row["channel_serial"];
		if (variant == 26) row.erase("volume");
		if (variant == 27) linked["owners"].erase(0);
		if (variant == 28) bad["voices"][0][4] = 499;
		if (variant == 29) bad["voices"][0][5] = 1;
		require(!xl_compat::restore_named_audio(bad, false) && !xl_compat::restore_named_audio(bad, true) && snapshot() == earned,
		        ("Malformed linked state was accepted or changed playback: " + std::to_string(variant)).c_str());
	}
}
void persistence(int source, const nlohmann::json &initial)
{
	using namespace xl_compat;
	using namespace xl_compat::audio_runtime;
	xl_named_audio_stop();
	xl_audio_defer_objects(0);
	Viewer = ConsoleObject;
	auto position = Viewer->pos;
	position.x += 10 * F1_0;
	position.z += 10 * F1_0;
	Objects[source].pos = position;
	obj_relink(source, Viewer->segnum);
	const auto object_token = xl_audio_link_object(0, 3, source, 1, F1_0, 64 * F1_0, -1, -1, 0, 0);
	require(object_token > 0 && xl_audio_link_position(1, Viewer->segnum, 0, &position, 1, F1_0, 64 * F1_0) > object_token, "Cannot create saved active linked owners");
	auto distant = position;
	distant.x += 500 * F1_0;
	const auto quiet = xl_audio_link_position(0, Viewer->segnum, 1, &distant, 1, F1_0, 64 * F1_0);
	xl_audio_defer_objects(1);
	const auto pending = xl_audio_link_position(1, Viewer->segnum, 2, &position, 1, F1_0, 64 * F1_0);
	require(quiet > 0 && pending > 0 && owner(quiet).channel < 0 && owner(pending).channel < 0, "Cannot create saved stopped and deferred owners");
	++GameTime64;
	require(xl_named_audio_action(&Triggers[2], 2), "Cannot create mixed linked save action");
	digi_play_sample_looping(SOUND_SEISMIC_DISTURBANCE_START, F1_0 / 2, -1, -1);
	require(looping.matches(mixer), "Cannot create mixed seismic looping save channel");
	output();
	const auto earned = snapshot();
	require(earned.at("linked").at("owners").size() == 4 && earned.at("voices").size() == 4 && earned.at("linked").at("deferred").get<bool>(), ("Linked save fixture has wrong occupancy: " + earned.dump()).c_str());
	for (const auto &row : earned.at("voices")) require(row[3] > 0, "Linked save did not earn a live playhead");
	xl_named_audio_pause(1);
	begin_capture();
	run_output(2048);
	require(snapshot() == earned && std::all_of(captured.begin(), captured.begin() + captured_samples, [](auto value) { return value == 0; }), "Paused linked sound advanced or emitted PCM");
	malformed(earned);
	auto *file = PHYSFS_openWrite("linked-audio-state.bin");
	require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "Cannot write linked runtime trailer");
	xl_named_audio_stop();
	xl_audio_defer_objects(0);
	file = PHYSFS_openRead("linked-audio-state.bin");
	require(file && xl_wall_trigger_read(file) && PHYSFS_close(file) && snapshot() == earned && owners.valid(mixer), "Trailer restore changed linked owners or bindings");
	output();
	require(restore_named_audio(earned, true), "Cannot reset earned linked state for disk save");
	char save[] = "linked-audio.sav", description[32] = "XL linked audio";
	require(state_save_all_sub(save, description), "Cannot save mixed linked playback");
	// Force stale XL control state immediately before the stock loader, without a frame
	Seismic_sound_playing = 1;
	Seismic_tremor_volume = 16;
	Next_seismic_sound_time = GameTime64 + F1_0;
	char error[256]{};
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(), "Linked audio stock transition failed");
	require(!Seismic_sound_playing && !Seismic_tremor_volume && !Next_seismic_sound_time, "XL seismic control state leaked into the stock loader");
	require(!owners.count && !deferred && snapshot().at("voices").empty() && snapshot().at("loop").empty() && xl_audio_link_object(0, 3, 0, 1, F1_0, 64 * F1_0, -1, -1, 0, 0) < 0, "Linked ownership leaked into stock gameplay");
	digi_play_sample_looping(SOUND_SEISMIC_DISTURBANCE_START, F1_0, -1, -1);
	const int stock_channel = digi_looping_channel;
	require(stock_channel >= 0 && Mix_Playing(stock_channel) && snapshot().at("loop").empty() && snapshot().at("voices").empty(), "Stock looping API entered XL playback or failed to start Redux");
	digi_change_looping_volume(F1_0 / 4);
	digi_stop_looping_sound();
	require(!Mix_Playing(stock_channel), "Stock looping stop failed");
	require(state_restore_all_sub(save, 0) && snapshot() == earned && owners.valid(mixer), "Disk-through-stock restore changed linked playheads, stopped owners or identities");
	require(looping.matches(mixer), "Disk restore lost the seismic loop controller");
	digi_change_looping_volume(F1_0 / 4);
	require(mixer.voices[unsigned(looping.channel)].priority.volume == F1_0 / 8, "Restored engine loop control addressed the wrong voice");
	require(restore_named_audio(earned, true), "Cannot recover earned loop volume");
	output();
	xl_audio_sync_objects();
	const auto before_reopen = snapshot();
	xl_named_audio_backend_close();
	Mix_CloseAudio();
	require(Mix_OpenAudio(48000, AUDIO_S16SYS, 2, 1024) == 0, "Cannot reopen linked output at 48 kHz");
	SDL_PauseAudio(1);
	require(xl_named_audio_backend_open(48000, AUDIO_S16SYS, 2), "Cannot reopen linked playback at another rate");
	const auto reopened = snapshot();
	require(reopened.at("linked") == before_reopen.at("linked") && reopened.at("events") == before_reopen.at("events") && reopened.at("voices").size() == before_reopen.at("voices").size(), "Backend reopen lost linked state before save restoration");
	for (std::size_t i = 0; i < before_reopen.at("voices").size(); ++i) {
		auto expected = before_reopen.at("voices")[i];
		expected[3] = expected[3].get<std::uint64_t>() * 48000 / 44100;
		require(reopened.at("voices")[i] == expected, "Backend reopen changed linked controls or playback phase before restoration");
	}
	output();
	require(restore_named_audio(earned, true), "Cannot restore linked playback at another rate");
	const auto resampled = snapshot();
	require(resampled.at("linked") == earned.at("linked") && owners.valid(mixer), "Device reopen changed saved linked owners");
	for (std::size_t i = 0; i < earned.at("voices").size(); ++i)
		require(resampled.at("voices")[i][3] == earned.at("voices")[i][3].get<std::uint64_t>() * 48000 / 44100, "Linked output-rate conversion lost its phase");
	output();
	xl_named_audio_backend_close();
	Mix_CloseAudio();
	require(Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 1024) == 0, "Cannot recover original linked output rate");
	SDL_PauseAudio(1);
	require(xl_named_audio_backend_open(44100, AUDIO_S16SYS, 2) && restore_named_audio(earned, true), "Cannot recover linked owners at the original rate");
	xl_audio_defer_objects(0);
	xl_audio_sync_objects();
	require(owner(pending).channel >= 0 && owner(quiet).channel < 0 && owner(object_token).signature == Objects[source].signature && owners.valid(mixer), "Restored deferred/quiet owners did not continue correctly");
	output();
	require(restore_named_audio(initial, true), "Cannot reset completed linked save controls");
	std::cout << "Linked persistence passed: mixed PCM, 30 atomic rejections, paused/stopped/deferred owners, trailer and disk-through-stock restore, 48 kHz reopen and resumed lifetime updates\n";
}
void run(const std::filesystem::path &root, const nlohmann::json &initial)
{
	using namespace xl_compat;
	using namespace xl_compat::audio_runtime;
	require(restore_named_audio(initial, true), "Cannot reset native linked audio");
	digi_set_digi_volume(32768);
	object listener = *Viewer;
	compute_segment_center(&listener.pos, &Segments[listener.segnum]);
	listener.orient = vmd_identity_matrix;
	const auto original_viewer = Viewer;
	Viewer = &listener;
	struct ViewerGuard {
		object *previous;
		~ViewerGuard()
		{
			Viewer = previous;
		}
	} viewer_guard{ original_viewer };
	auto position = listener.pos;
	position.x += 10 * F1_0;
	// Avoid the pinned ear-angle function's special zero angle at both unit-dot
	// boundaries; those axial cases already have dedicated source comparisons
	position.z += 10 * F1_0;
	const int source = obj_create(OBJ_MARKER, 0, listener.segnum, &position, &listener.orient, F1_0, CT_NONE, MT_NONE, RT_NONE);
	require(source >= 0, "Cannot create native sound source");
	struct ObjectGuard {
		int index;
		~ObjectGuard()
		{
			xl_named_audio_stop();
			obj_delete(index);
		}
	} object_guard{ source };
	xl_audio_creation_control::run(root, source, initial);
	xl_gameplay_audio_control::run(source, initial);
	const auto create = [&](int forever = 1, int custom = 0) {
		return xl_audio_link_object(0, 3, source, forever, F1_0, 64 * F1_0, -1, -1, 0, custom);
	};
	const auto reset = [&] {
		xl_named_audio_stop();
		xl_audio_defer_objects(0);
		Objects[source].pos = position;
		listener.orient = vmd_identity_matrix;
		digi_set_digi_volume(32768);
	};
	const auto token = create();
	require(token > 0 && owners.count == 1 && owner(token).channel >= 0 && owner(token).decay == 3 && owner(token).signature == Objects[source].signature, "Native object creation lost its source class, identity or channel");
	require(create() == token && owners.count == 1, "Live object duplicate allocated another owner");
	const auto first = owner(token);
	const auto first_voice = snapshot().at("voices")[0];
	output();
	Objects[source].pos.x -= 20 * F1_0;
	xl_audio_sync_objects();
	const auto moved = owner(token);
	require(moved.channel == first.channel && moved.serial == first.serial && moved.volume == first.volume && moved.pan != first.pan && snapshot().at("voices")[0][11] != first_voice[11],
	        ("Moving source did not update in place: channel " + std::to_string(first.channel) + "/" + std::to_string(moved.channel) + ", volume " + std::to_string(first.volume) + "/" + std::to_string(moved.volume) + ", pan " + std::to_string(first.pan) + "/" + std::to_string(moved.pan)).c_str());
	output();
	listener.orient.rvec.x = -F1_0;
	listener.orient.fvec.z = -F1_0;
	xl_audio_sync_objects();
	require(owner(token).pan == first.pan, "Rotating the listener did not reverse spatial pan");
	Objects[source].pos.x = listener.pos.x + 500 * F1_0;
	xl_audio_sync_objects();
	require(owners.count == 1 && owner(token).channel < 0 && snapshot().at("voices").empty(), "Inaudible forever owner was deleted or kept playing");
	Objects[source].pos = position;
	xl_audio_sync_objects();
	require(owner(token).channel >= 0 && owner(token).serial == std::uint64_t(token), "Audible owner did not restart with the same identity");
	output();
	digi_set_digi_volume(0);
	require(owners.count == 1 && owner(token).channel < 0, "Effects slider did not stop linked output");
	digi_set_digi_volume(32768);
	require(owner(token).channel >= 0, "Effects slider did not resume linked output");
	++GameTime64;
	require(xl_named_audio_action(&Triggers[2], 2) == 1 && xl_audio_start_sample(1, 2, F1_0, 32767, 1, -1, -1) >= 0, "Cannot prepare unrelated named and ordinary voices");
	const auto unrelated = snapshot();
	++Objects[source].signature;
	xl_audio_sync_objects();
	require(!owners.count && snapshot().at("voices").size() == 2, "Reused native object retained its old sound or stopped unrelated voices");
	for (const auto &row : unrelated.at("voices"))
		if (row[7] != 1) {
			const auto after = snapshot().at("voices");
			require(std::find(after.begin(), after.end(), row) != after.end(), "Object reuse disturbed an unrelated voice");
		}
	output();
	reset();
	require(create(0) == -1 && !owners.count && snapshot().at("voices").size() == 1 && snapshot().at("voices")[0][7] == 2, "One-shot object sound became a persistent owner");
	reset();
	xl_audio_defer_objects(1);
	const auto pending = create();
	require(pending > 0 && (owner(pending).flags & sound_permanent) && owner(pending).channel < 0, "Deferred object started before activation");
	xl_audio_sync_objects();
	require(owner(pending).channel < 0, "Frame update ignored deferred startup");
	xl_audio_defer_objects(0);
	xl_audio_sync_objects();
	require(owner(pending).channel >= 0, "Deferred owner did not activate");
	reset();
	const auto custom = create(1, 1);
	const auto custom_voice = snapshot().at("voices")[0];
	Objects[source].pos.x -= 20 * F1_0;
	xl_audio_sync_objects();
	require(owner(custom).pan != first.pan && snapshot().at("voices")[0] == custom_voice, "Custom owner lost spatial updates or changed automatic channel controls");
	reset();
	const auto fixed = xl_audio_link_position(0, listener.segnum, 0, &position, 1, F1_0, 64 * F1_0);
	require(fixed > 0 && owner(fixed).channel >= 0 && owner(fixed).decay == 0 && owner(fixed).sound_class == 0, "Fixed segment sound did not start with generic linear decay");
	auto nearby = position;
	nearby.x += F1_0 / 2;
	require(xl_audio_link_position(0, listener.segnum, 0, &nearby, 1, F1_0, 64 * F1_0) == fixed, "Nearby segment duplicate was not reused");
	++nearby.x;
	require(xl_audio_link_position(0, listener.segnum, 0, &nearby, 1, F1_0, 64 * F1_0) > fixed && owners.count == 2, "Segment duplicate radius swallowed a different source");
	output();
	require(!xl_audio_destroy_position(listener.segnum, 1, 0) && owners.count == 2 && xl_audio_destroy_position(listener.segnum, 0, 0) && !owners.count, "Segment/side/sample destruction differs");
	reset();
	const auto retained = create();
	++Objects[source].signature;
	xl_audio_destroy_object(-1, -1);
	require(owners.count == 1 && owner(retained).channel < 0, "Missing-object cleanup did not retain the stopped forever owner");
	const auto stopped = snapshot();
	xl_named_audio_stop();
	require(restore_named_audio(stopped, true) && owner(retained).channel < 0, "Stopped missing lifetime did not survive save restoration");
	xl_audio_sync_objects();
	require(!owners.count, "Frame update did not delete the missing lifetime");
	persistence(source, initial);
	require(restore_named_audio(initial, true), "Cannot leave linked controls at initial audio state");
	Mix_SetPostMix(nullptr, nullptr);
	std::cout << "Linked native audio passed: moving source/listener PCM, duplicate identities, range restart, slider sync, object reuse, mixed owners, one-shot, deferred/custom and segment destruction\n";
}
} // namespace xl_linked_audio_control
