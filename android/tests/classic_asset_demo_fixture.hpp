#ifdef DXX_BUILD_DESCENT_II
static int classic_interpolated_companion(char *demo, int robot)
{
	// EOF closes the game window through the executable's event landing frame
	if (setjmp(LeaveEvents)) return 0;
	Newdemo_do_interpolate = 1;
	newdemo_start_playback(demo);
	for (int frame = 0; frame < 3 && Newdemo_state == ND_STATE_PLAYBACK && Highest_object_index != 1; ++frame) {
		FrameTime = F1_0 / 10;
		newdemo_playback_one_frame();
	}
	std::fprintf(stderr, "Interpolated companion: state=%d window=%d highest=%d id=%d\n", Newdemo_state, Game_wind != nullptr, Highest_object_index, Objects[1].id);
	return Newdemo_state == ND_STATE_PLAYBACK && Game_wind && Highest_object_index == 1 && Objects[1].type == OBJ_ROBOT && Objects[1].id == robot;
}

static nlohmann::json exercise_classic_custom_missions(const char *d2_directory)
{
	using nlohmann::json;
	const std::string hog = std::string(d2_directory) + "/descent2.hog";
	require(PHYSFS_unmount(hog.c_str()) && PHYSFS_unmount(d2_directory), "record native-only custom data without optional definitions");
	char mission[] = "descent";
	require(load_mission_by_name(mission), "select native custom recording mission");
	std::string custom_name = Level_names[0];
	custom_name.replace(custom_name.find_last_of('.'), std::string::npos, ".hx1");
	const bytes custom = d1_custom_definition_fixture();
	write_fixture(custom_name.c_str(), custom);
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	require(N_robot_types == 24 && Robot_info[0].mass == 7 * F1_0, "cold level entry uses actual custom native definitions");
	const auto make_actor = [] {
		const int model = Robot_info[0].model_num;
		const int actor = obj_create(OBJ_ROBOT, 0, ConsoleObject->segnum, &ConsoleObject->pos, &ConsoleObject->orient,
		                             Polygon_models[model].rad, CT_AI, MT_PHYSICS, RT_POLYOBJ);
		require(actor > 0, "create native custom model actor through the engine object factory");
		Objects[actor].rtype.pobj_info.model_num = model;
		Objects[actor].rtype.pobj_info.tmap_override = -1;
		Objects[actor].rtype.pobj_info.anim_angles[0] = { 111, 222, 333 };
		return actor;
	};
	const auto frames = [](int actor) {
		for (int frame = 0; frame < 3; ++frame) {
			newdemo_record_start_frame(F1_0 / 10);
			newdemo_record_viewer_object(ConsoleObject);
			newdemo_record_render_object(&Objects[actor]);
		}
	};
	const auto finish = [](const char *path) {
		newdemo_write_end();
		require(PHYSFS_close(outfile), "finish complete custom rendering demo");
		outfile = nullptr;
		Newdemo_state = ND_STATE_NORMAL;
		const auto data = read_save_fixture("demos/tmpdemo.dem");
		write_fixture(path, data);
		fixture_close_order_game();
		return data;
	};
	const int actor = make_actor();
	std::vector<int> layout(N_robot_types);
	layout[0] = Polygon_models[Robot_info[0].model_num].n_models;
	newdemo_start_recording(0);
	input_demo_recorder_cancel();
	frames(actor);
	const auto recording = finish("demos/custom.dem");
	const auto opposite = classic_trigger_opposite_endian(recording, layout);
	json report = json::array();
	for (const char *scenario : { "same", "changed", "missing", "restored", "optional-added" }) {
		if (!std::strcmp(scenario, "changed")) {
			bytes changed = custom;
			set_int(changed, 16 + 136, 8 * F1_0);
			write_fixture(custom_name.c_str(), changed);
		} else if (!std::strcmp(scenario, "missing")) require(PHYSFS_delete(custom_name.c_str()), "remove only isolated custom source");
		else if (!std::strcmp(scenario, "restored")) write_fixture(custom_name.c_str(), custom);
		else if (!std::strcmp(scenario, "optional-added"))
			require(PHYSFS_mount(d2_directory, nullptr, 1) && PHYSFS_mount(hog.c_str(), nullptr, 1), "native recording permits newly available optional assets");
		const bool accepted = std::strcmp(scenario, "changed") && std::strcmp(scenario, "missing");
		const std::string output = std::string("custom-") + scenario + ".jsonl";
		const bytes prior = { 'k', 'e', 'e', 'p' };
		write_fixture(output.c_str(), prior);
		char error[256] = {};
		unsigned seed = 0, after_seed = 0;
		require(d_rand_get_state(&seed), "observe SIM seed before rendering-demo export");
		const auto calls = d_rand_get_call_count();
		const int dumped = newdemo_dump_json("demos/custom.dem", output.c_str(), error, sizeof(error));
		require(d_rand_get_state(&after_seed) && seed == after_seed && calls == d_rand_get_call_count(), "rendering-demo asset preparation does not affect SIM RNG");
		require(accepted ? dumped : !dumped && read_save_fixture(output) == prior && std::strstr(error, "base/custom"),
		        "custom source admission uses actual level definitions and preserves rejected output");
		const std::string endian_name = std::string("custom-") + scenario + "-opposite.dem";
		write_fixture(("demos/" + endian_name).c_str(), opposite);
		std::vector<char> endian(endian_name.begin(), endian_name.end());
		endian.push_back('\0');
		set_default_handler(classic_conversion_message_event);
		const int converted = newdemo_swap_endian(endian.data());
		set_default_handler(nullptr);
		require(accepted ? converted && read_save_fixture("demos/" + endian_name) == recording
		                 : !converted && read_save_fixture("demos/" + endian_name) == opposite,
		        "custom endian conversion preserves native object and identity framing");
		if (!std::strcmp(scenario, "same")) {
			write_fixture(("demos/" + endian_name).c_str(), opposite);
			set_default_handler(classic_conversion_message_event);
			const int repeated = newdemo_swap_endian(endian.data());
			set_default_handler(nullptr);
			require(!repeated && read_save_fixture("demos/" + endian_name) == opposite && read_save_fixture("demos/custom-same-opposite.ppc") == opposite,
			        "existing endian backup is preserved and failed publication is reported");
		}
		report.push_back({ { "scenario", scenario }, { "dumped", dumped }, { "converted", converted }, { "sim_rng_unchanged", true } });
		std::fprintf(stderr, "Custom demo %s: dumped=%d converted=%d\n", scenario, dumped, converted);
	}
	// Actual campaign level entry writes its boundary before loading new assets
	require(load_mission_by_name(mission), "reload mission before recording a level transition");
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	int current_actor = make_actor();
	newdemo_start_recording(0);
	input_demo_recorder_cancel();
	frames(current_actor);
	FrameTime = F1_0 / 10;
	StartNewLevelSub(2, 0, 0);
	require(Newdemo_state == ND_STATE_RECORDING && Current_level_num == 2 && Robot_info[0].mass != 7 * F1_0,
	        "recording crosses actual level entry from custom to stock definitions");
	current_actor = make_actor();
	frames(current_actor);
	finish("demos/custom-levels.dem");
	char levels[] = "custom-levels.dem", error[256] = {};
	require(newdemo_dump_json("demos/custom-levels.dem", "custom-levels.jsonl", error, sizeof(error)), error);
	Newdemo_do_interpolate = 0;
	FrameTime = F1_0 / 25;
	newdemo_start_playback(levels);
	require(Newdemo_state == ND_STATE_PLAYBACK && Current_level_num == 1 && Robot_info[0].mass == 7 * F1_0,
	        "playback begins with custom model definitions");
	newdemo_goto_end(0);
	require(Newdemo_state == ND_STATE_PLAYBACK && Current_level_num == 2 && Objects[1].id == 0 && Robot_info[0].mass != 7 * F1_0,
	        "direct last-frame seek binds stock definitions independently");
	for (int step = 0; step < 8 && Current_level_num != 1; ++step) {
		Newdemo_vcr_state = ND_STATE_ONEFRAMEBACKWARD;
		newdemo_playback_one_frame();
	}
	require(Newdemo_state == ND_STATE_PLAYBACK && Current_level_num == 1 && Robot_info[0].mass == 7 * F1_0,
	        "backwards playback crosses into the original custom generation");
	newdemo_goto_beginning();
	require(Newdemo_state == ND_STATE_PLAYBACK && Objects[1].id == 0 && Objects[1].rtype.pobj_info.anim_angles[0].p == 111,
	        "beginning seek restores the actual custom object pose");
	fixture_close_order_game();
	require(PHYSFS_delete(custom_name.c_str()), "retire fixture-owned custom source");
	write_fixture("recorded-custom.hx1", custom);
	const auto cli = json({ { "custom_name", custom_name }, { "source", "recorded-custom.hx1" }, { "demo", "demos/custom.dem" } }).dump(2) + "\n";
	write_fixture("classic-custom-cli.json", bytes(cli.begin(), cli.end()));
	const auto text = report.dump(2) + "\n";
	write_fixture("classic-custom-report.json", bytes(text.begin(), text.end()));
	return report;
}

// Use complete recordings and cold level loads, so a retained model cannot hide
// a missing optional package
static nlohmann::json exercise_classic_asset_missions(const char *d2_directory)
{
	using nlohmann::json;
	const std::string hog = std::string(d2_directory) + "/descent2.hog";
	require(PHYSFS_mount(d2_directory, nullptr, 1) && PHYSFS_mount(hog.c_str(), nullptr, 1), "mount optional recording assets");
	new_player_config();
	std::strcpy(Players[0].callsign, "clsasset");
	GameArg.SysUsePlayersDir = 0;
	GameArg.SndDigiSampleRate = SAMPLE_RATE_11K;
	texmerge_init(10);
	init_game();
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	const int buddy = create_buddy_bot_at_player(0);
	require(buddy > 0 && Robot_info[Objects[buddy].id].companion, "record a real optional companion");
	const int robot = Objects[buddy].id;
	const fix mass = Robot_info[robot].mass;
	std::vector<int> robot_layout(N_robot_types);
	robot_layout[robot] = Polygon_models[Robot_info[robot].model_num].n_models;
	size_t identity_offset = 0, robot_offset = 0;
	newdemo_start_recording(0);
	input_demo_recorder_cancel();
	for (int frame = 0; frame < 3; ++frame) {
		newdemo_record_start_frame(F1_0 / 10);
		if (!frame) identity_offset = static_cast<size_t>(PHYSFS_tell(outfile));
		newdemo_record_viewer_object(ConsoleObject);
		if (!frame) robot_offset = static_cast<size_t>(PHYSFS_tell(outfile));
		newdemo_record_render_object(&Objects[buddy]);
	}
	newdemo_write_end();
	require(PHYSFS_close(outfile), "finish companion recording");
	outfile = nullptr;
	Newdemo_state = ND_STATE_NORMAL;
	const auto recording = read_save_fixture("demos/tmpdemo.dem");
	const auto opposite = classic_trigger_opposite_endian(recording, robot_layout);
	write_fixture("companion.dem", recording);
	write_fixture("demos/companion.dem", recording);
	fixture_close_order_game();
	json report = json::array();
	bool correct = true;
	for (const char *scenario : { "same", "changed", "missing" }) {
		if (!std::strcmp(scenario, "changed")) {
			PHYSFS_file *file = PHYSFSX_openReadBuffered("descent2.ham");
			require(file && PHYSFS_fileLength(file) > 8, "open original optional HAM");
			bytes data(static_cast<size_t>(PHYSFS_fileLength(file)));
			require(PHYSFS_readBytes(file, data.data(), data.size()) == static_cast<PHYSFS_sint64>(data.size()) && PHYSFS_seek(file, 8), "copy optional HAM into the isolated fixture");
			for (int width : { 22, 2, 82, 130, 126 }) {
				const int count = PHYSFSX_readInt(file);
				require(count >= 0 && PHYSFS_seek(file, PHYSFS_tell(file) + static_cast<PHYSFS_sint64>(count) * width), "locate companion source table");
			}
			require(PHYSFSX_readInt(file) > 33, "source HAM contains the companion");
			const size_t offset = static_cast<size_t>(PHYSFS_tell(file)) + 33 * 480 + 136;
			require(offset + 4 <= data.size() && PHYSFS_close(file), "locate companion mass");
			set_int(data, offset, mass + F1_0);
			write_fixture("descent2.ham", data);
		} else if (!std::strcmp(scenario, "missing")) {
			require(PHYSFS_delete("descent2.ham") && PHYSFS_unmount(hog.c_str()) && PHYSFS_unmount(d2_directory), "remove optional package from the fixture");
		}
		char mission[] = "descent";
		require(load_mission_by_name(mission), "reselect mission after export cleanup");
		LoadLevel(1, 0);
		const int prepared = N_robot_types;
		const fix prepared_mass = robot < N_robot_types ? Robot_info[robot].mass : -1;
		const std::string output = std::string(scenario) + ".jsonl";
		const bytes previous = { 'k', 'e', 'e', 'p' };
		write_fixture(output.c_str(), previous);
		char error[256] = {};
		const int dumped = newdemo_dump_json("companion.dem", output.c_str(), error, sizeof(error));
		report.push_back({ { "scenario", scenario }, { "prepared_robots", prepared }, { "recorded_mass", mass }, { "prepared_mass", prepared_mass }, { "dumped", dumped }, { "error", error } });
		const auto text = report.dump(2) + "\n";
		write_fixture("classic-asset-report.json", bytes(text.begin(), text.end()));
		std::fprintf(stderr, "Companion demo %s: prepared=%d mass=%d dumped=%d error=%s\n", scenario, prepared, prepared_mass, dumped, error);
		std::fflush(stderr);
		if (!std::strcmp(scenario, "same")) correct = correct && dumped;
		else correct = correct && !dumped && read_save_fixture(output) == previous;
		Newdemo_do_interpolate = 0;
		FrameTime = F1_0 / 25;
		char demo[] = "companion.dem";
		set_default_handler(classic_conversion_message_event);
		newdemo_start_playback(demo);
		set_default_handler(nullptr);
		const bool playing = Newdemo_state == ND_STATE_PLAYBACK && Game_wind;
		const bool recovered = Newdemo_state == ND_STATE_NORMAL && !Game_wind;
		std::fprintf(stderr, "Companion playback %s: playing=%d recovered=%d\n", scenario, playing, recovered);
		if (!std::strcmp(scenario, "same")) {
			require(playing && Highest_object_index == 1 && Objects[1].type == OBJ_ROBOT && Objects[1].id == robot,
			        "normal playback binds the actual companion record");
			newdemo_goto_end(0);
			require(Newdemo_state == ND_STATE_PLAYBACK && Objects[1].id == robot, "direct end seek binds the last frame's companion");
			Newdemo_vcr_state = ND_STATE_ONEFRAMEBACKWARD;
			newdemo_playback_one_frame();
			require(Newdemo_state == ND_STATE_PLAYBACK && Objects[1].id == robot, "backwards playback binds frame-local sources");
		} else correct = correct && recovered;
		fixture_close_order_game();
		if (!std::strcmp(scenario, "same")) {
			require(classic_interpolated_companion(demo, robot),
			        "default interpolated playback reads frame-local companion identities");
			fixture_close_order_game();
		}
		const std::string endian_name = std::string(scenario) + "-opposite.dem";
		write_fixture(("demos/" + endian_name).c_str(), opposite);
		std::vector<char> endian_file(endian_name.begin(), endian_name.end());
		endian_file.push_back('\0');
		set_default_handler(classic_conversion_message_event);
		const int converted = newdemo_swap_endian(endian_file.data());
		set_default_handler(nullptr);
		require(!std::strcmp(scenario, "same") ? converted && read_save_fixture("demos/" + endian_name) == recording
		                                       : !converted && read_save_fixture("demos/" + endian_name) == opposite,
		        "endian conversion validates sources and preserves rejected input");
		report.back()["played"] = playing;
		report.back()["menu_recovered"] = recovered;
		report.back()["converted"] = converted;
		const auto complete = report.dump(2) + "\n";
		write_fixture("classic-asset-report.json", bytes(complete.begin(), complete.end()));
	}
	require(correct, "recordings accept matching sources and reject changed or missing companion definitions");
	require(PHYSFS_mount(d2_directory, nullptr, 1) && PHYSFS_mount(hog.c_str(), nullptr, 1), "restore original optional package after rejection");
	char error[256] = {};
	require(newdemo_dump_json("companion.dem", "restored.jsonl", error, sizeof(error)), "restoring original sources makes the recording usable again");
	require(recording[2] == 4 && recording[identity_offset] == 52 && recording[identity_offset + 2] == 64, "recording explicitly binds the native and optional namespace");
	for (int invalid = 0; invalid < 11; ++invalid) {
		bytes damaged = recording;
		if (invalid == 0) damaged[2] = 3; // pre-identity imported demo namespace
		else if (invalid == 1) damaged[1] = 17;
		else if (invalid < 6) damaged[identity_offset + 2] = invalid == 2 ? 0 : invalid == 3 ? 31
			                                                                : invalid == 4   ? 65
			                                                                                 : 255;
		else if (invalid == 6) damaged[identity_offset + 3] ^= 1;
		else if (invalid == 7) damaged[identity_offset + 35] ^= 1;
		else if (invalid == 8) damaged.resize(identity_offset + 66);
		else if (invalid == 9) damaged.erase(damaged.begin() + identity_offset, damaged.begin() + identity_offset + 67);
		else damaged[robot_offset + 3] = 255;
		const std::string name = "invalid-" + std::to_string(invalid);
		write_fixture((name + ".dem").c_str(), damaged);
		const bytes prior = { 'k', 'e', 'e', 'p' };
		write_fixture((name + ".jsonl").c_str(), prior);
		require(!newdemo_dump_json((name + ".dem").c_str(), (name + ".jsonl").c_str(), error, sizeof(error)) && read_save_fixture(name + ".jsonl") == prior,
		        "unsupported, malformed, missing and truncated identities fail before object decoding");
	}
	require(newdemo_dump_json("companion.dem", "recovered.jsonl", error, sizeof(error)), "valid recording recovers after malformed inputs");
	exercise_classic_custom_missions(d2_directory);
	return report;
}
#endif
