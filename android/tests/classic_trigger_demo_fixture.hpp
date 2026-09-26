#ifdef DXX_BUILD_DESCENT_II
extern "C" {
extern PHYSFS_file *infile, *outfile;
int newdemo_read_frame_information(int rewrite);
void newdemo_write_end(void);
void newdemo_record_start_demo(void);
}

// Exercise the real crossing writer and frame decoder, with no simulation tick
static void test_classic_trigger_demo(void)
{
	using nlohmann::json;
	json results = json::array();
	const short native_flags[] = { TRIGGER_SECRET_EXIT, TRIGGER_CONTROL_DOORS | TRIGGER_SECRET_EXIT, TRIGGER_EXIT | TRIGGER_SECRET_EXIT };
	for (int scenario = 0; scenario < 10; ++scenario) {
		init_test_corridor();
		Player_num = Player_is_dead = Game_mode = 0;
		Players[0] = {};
		Players[0].objnum = 0;
		Players[0].shields = scenario == 4 ? -F1_0 : 100 * F1_0;
		ConsoleObject = Viewer = &Objects[0];
		Objects[0].type = OBJ_PLAYER;
		Objects[1].type = OBJ_ROBOT;
		Objects[1].id = 0;
		Robot_info[0].companion = 1;
		Num_walls = Num_triggers = 1;
		Walls[0] = {};
		Walls[0].trigger = scenario == 9 ? 255 : 0;
		Segments[0].sides[4].wall_num = scenario == 8 ? -1 : 0;
		Triggers[0] = {};
		if (scenario < 3) install_native_trigger(0, native_flags[scenario]);
		else {
			Triggers[0].type = scenario == 7 ? TT_OPEN_DOOR : TT_SECRET_EXIT;
			Triggers[0].flags = scenario == 3 ? TF_DISABLED : TF_NO_MESSAGE;
		}
		const std::string path = "classic-trigger-" + std::to_string(scenario) + ".dem";
		outfile = PHYSFSX_openWriteBuffered(path.c_str());
		require(outfile != nullptr, "open isolated classic event recording");
		Newdemo_state = ND_STATE_RECORDING;
		if (scenario == 5 || scenario == 6) {
			newdemo_record_trigger(0, 4, 0, 1);
			newdemo_record_secret_exit_blown(scenario - 5);
		} else {
			check_trigger(&Segments[0], 4, scenario < 3 ? 1 : 0, 1);
		}
		newdemo_record_player_energy(73 + scenario);
		newdemo_record_start_frame(F1_0 / 10);
		const ubyte eof = 0;
		require(PHYSFS_writeBytes(outfile, &eof, 1) == 1, "terminate the recorded frame with the classic EOF event");
		require(PHYSFS_close(outfile), "close actual trigger and next-frame recording");
		outfile = nullptr;
		const auto recorded = read_save_fixture(path);
		require(recorded.size() == (scenario == 5 || scenario == 6 ? 37 : 32), "actual writer emits only the expected optional exit status");
		std::fprintf(stderr, "Decoding actual trigger scenario %d (%zu bytes)\n", scenario, recorded.size());
		std::fflush(stderr);
		const auto draws = d_rand_get_call_count();
		for (int rewrite = 0; rewrite < 2; ++rewrite) {
			infile = PHYSFSX_openReadBuffered(path.c_str());
			require(infile != nullptr, "open actual trigger recording for decode");
			Newdemo_state = ND_STATE_PLAYBACK;
			Newdemo_vcr_state = ND_STATE_PAUSED;
			const std::string rewritten = path + ".rewrite";
			if (rewrite) outfile = PHYSFSX_openWriteBuffered(rewritten.c_str());
			const int decoded = newdemo_read_frame_information(rewrite);
			std::fprintf(stderr, "  rewrite=%d result=%d consumed=%lld expected=%zu\n", rewrite, decoded, static_cast<long long>(PHYSFS_tell(infile)), recorded.size() - 1);
			require(decoded == 1 && PHYSFS_tell(infile) == static_cast<PHYSFS_sint64>(recorded.size() - 1),
			        "trigger decode reaches the exact next frame boundary");
			require(PHYSFS_close(infile), "close decoded event input");
			infile = nullptr;
			if (rewrite) {
				require(PHYSFS_close(outfile), "close rewritten trigger events");
				outfile = nullptr;
				require(read_save_fixture(rewritten) == bytes(recorded.begin(), recorded.end() - 1), "rewrite preserves all event bytes without inferring framing from trigger type");
			}
		}
		require(draws == d_rand_get_call_count(), "paused decoding and rewriting do not consume SIM RNG");
		results.push_back({ { "scenario", scenario }, { "bytes", recorded.size() }, { "decoded", true }, { "rewritten", true } });
	}
	Newdemo_state = Newdemo_vcr_state = ND_STATE_NORMAL;
	const auto report = results.dump(2) + "\n";
	write_fixture("classic-trigger-report.json", bytes(report.begin(), report.end()));
	std::puts("Classic trigger recording and decoding passed");
}

// Independent byte-order conversion for the fixture's single-player records
// Optional robot layout is captured at recording time, never from the decoder
static bytes classic_trigger_opposite_endian(bytes data, const std::vector<int> &robot_submodels = {})
{
	const auto u32 = [&](size_t at) {
		require(at + 4 <= data.size(), "bounded fixture integer");
		return static_cast<unsigned>(data[at]) | (static_cast<unsigned>(data[at + 1]) << 8) |
		       (static_cast<unsigned>(data[at + 2]) << 16) | (static_cast<unsigned>(data[at + 3]) << 24);
	};
	require(data.size() > 57 && data[0] == 1 && data[1] == 16 && (data[2] == 3 || data[2] == 4) && u32(7) == 0,
	        "convert an actual single-player D2-format recording");
	size_t cursor = 3;
	const auto skip = [&](size_t count) {
		require(cursor + count <= data.size(), "bounded fixture byte field");
		cursor += count;
	};
	const auto flip = [&](size_t count) {
		require(cursor + count <= data.size(), "bounded fixture endian field");
		std::reverse(data.begin() + cursor, data.begin() + cursor + count);
		cursor += count;
	};
	for (int i = 0; i < 3; ++i) flip(4);  // clock, mode, score
	for (int i = 0; i < 20; ++i) flip(2); // primary and secondary ammo
	skip(1);                              // laser level
	skip(1 + data[cursor]);               // mission string
	skip(2);
	flip(4);
	skip(2); // energy, shields, flags and selected weapons
	bool first_level = true;
	for (;;) {
		require(cursor < data.size(), "classic fixture contains its trailer");
		const int event = data[cursor++];
		switch (event) {
			case 28: { // initial level and wall records
				skip(2);
				if (!first_level) break;
				first_level = false;
				const auto walls = u32(cursor);
				flip(4);
				for (unsigned i = 0; i < walls; ++i) {
					skip(3);
					flip(2);
					flip(2);
				}
				break;
			}
			case 52: // frame-local native asset identity contains only bytes
				skip(1);
				skip(1 + data[cursor]);
				break;
			case 3:
			case 4: {
				if (event == 3) skip(1); // viewer window
				require(cursor + 4 <= data.size() && data[cursor] == RT_POLYOBJ, "fixture records polygon objects");
				const int type = data[cursor + 1], robot = data[cursor + 2];
				require(type == OBJ_PLAYER || (type == OBJ_ROBOT && robot < static_cast<int>(robot_submodels.size()) && robot_submodels[robot] > 0),
				        "fixture has a known player or non-boss physics robot layout");
				skip(4);
				flip(2);                             // signature
				skip(9);                             // orientation bytes
				for (int i = 0; i < 7; ++i) flip(2); // short position
				for (int i = 0; i < 3; ++i) flip(4); // last position
				skip(1);                             // lifetime
				for (int i = 0; i < 6; ++i) flip(4); // physics velocity and thrust
				if (type == OBJ_ROBOT)
					for (int i = 0; i < robot_submodels[robot] * 3; ++i) flip(2);
				flip(4); // texture override
				break;
			}
			case 24:
			case 31:
			case 47: break; // view resets
			case 2:
				flip(2);
				flip(4);
				flip(4);
				break; // frame boundary
			case 9:
				for (int i = 0; i < 4; ++i) flip(4);
				break;
			case 48: flip(4); break;
			case 17: skip(2); break;
			case 0:
				flip(2);
				flip(2);
				flip(2);
				flip(4); // last frame and EOF padding
				skip(2);
				flip(4);
				skip(2);
				for (int i = 0; i < 20; ++i) flip(2);
				skip(1);
				flip(4);
				flip(2);
				skip(2); // laser, score, trailer length, level, EOF
				require(cursor == data.size(), "convert every byte of the actual classic trailer");
				return data;
			default: require(false, "unexpected event in the object-free endian control");
		}
	}
}

// Actual mission headers, walls, viewer objects, frame playback and JSON export
static int classic_conversion_message_event(d_event *event)
{
	if (event->type != EVENT_IDLE) return 0;
	struct {
		event_type type;
		int keycode;
	} key = { EVENT_KEY_COMMAND, KEY_ENTER };
	window_send_event(window_get_front(), reinterpret_cast<d_event *>(&key));
	return 1;
}

static nlohmann::json exercise_classic_trigger_missions(const char *d2_directory)
{
	using nlohmann::json;
	json results = json::array();
	new_player_config();
	std::strcpy(Players[0].callsign, "clsdemo");
	GameArg.SysUsePlayersDir = 0;
	texmerge_init(10);
	init_game();
	for (int profile = 1; profile <= 2; ++profile) {
		Game_mode = 0;
		if (profile == 2) {
			const std::string hog = std::string(d2_directory) + "/descent2.hog";
			require(PHYSFS_mount(d2_directory, nullptr, 0) && PHYSFS_mount(hog.c_str(), nullptr, 0), "mount ordinary D2 demo resources");
			char mission[] = "d2";
			require(load_mission_by_name(mission), "select actual ordinary D2 demo mission");
		}
		int level = 1, exit_wall = -1;
		for (; level <= Last_level && exit_wall < 0; ++level) {
			LoadLevel(level, 0);
			for (int wall = 0; wall < Num_walls; ++wall)
				if (Walls[wall].trigger < Num_triggers && (trigger_exit_flags(Walls[wall].trigger) & TRIGGER_SECRET_EXIT)) {
					exit_wall = wall;
					break;
				}
		}
		require(exit_wall >= 0, "find an actual secret exit in the loaded mission");
		--level;
		for (int status = -1; status <= (profile == 1 ? -1 : 1); ++status) {
			Game_mode = 0;
			input_demo_set_skip_level_intro(1);
			StartNewGame(level);
			const int seg = Walls[exit_wall].segnum, side = Walls[exit_wall].sidenum;
			const int trigger = Walls[exit_wall].trigger;
			newdemo_start_recording(0);
			require(Newdemo_state == ND_STATE_RECORDING && outfile, "start actual classic recorder");
			input_demo_recorder_cancel();
			newdemo_record_start_frame(F1_0 / 10);
			newdemo_record_viewer_object(ConsoleObject);
			const auto trigger_offset = PHYSFS_tell(outfile);
			if (profile == 2 && status < 0) {
				// Rejected crossings write a trigger event but no exit status
				Triggers[trigger].flags |= TF_DISABLED;
				check_trigger(&Segments[seg], side, Players[0].objnum, 1);
			} else {
				newdemo_record_trigger(seg, side, Players[0].objnum, 1);
				if (status >= 0) newdemo_record_secret_exit_blown(status);
			}
			newdemo_record_player_energy(73);
			newdemo_record_start_frame(F1_0 / 10);
			newdemo_record_viewer_object(ConsoleObject);
			newdemo_write_end();
			require(PHYSFS_close(outfile), "close complete actual classic demo");
			outfile = nullptr;
			Newdemo_state = ND_STATE_NORMAL;
			const std::string stem = "mission-" + std::to_string(profile) + "-status-" + std::to_string(status);
			const std::string demo = "demos/" + stem + ".dem";
			const auto recording = read_save_fixture("demos/tmpdemo.dem");
			write_fixture(demo.c_str(), recording);
			fixture_close_order_game();
			char error[256] = {};
			require(newdemo_dump_json(demo.c_str(), (stem + ".jsonl").c_str(), error, sizeof(error)), error);
			const std::string mounted_demo = stem + ".dem", mounted_output = stem + "-mounted.jsonl";
			write_fixture(mounted_demo.c_str(), recording);
			require(newdemo_dump_json(mounted_demo.c_str(), mounted_output.c_str(), error, sizeof(error)) && PHYSFS_getMountPoint("."),
			        "successful export reuses and preserves the existing source mount");
			if (profile == 1) {
				const auto before = read_save_fixture(mounted_output);
				require(PHYSFS_mkdir("demo-shadow"), "create isolated source-shadow control");
				write_fixture(("demo-shadow/" + mounted_demo).c_str(), { 'b', 'a', 'd' });
				require(PHYSFS_mount("demo-shadow", nullptr, 0), "mount a different file at the same virtual name");
				require(!newdemo_dump_json(mounted_demo.c_str(), mounted_output.c_str(), error, sizeof(error)) &&
				            std::strstr(error, "shadowed") && PHYSFS_getMountPoint(".") && PHYSFS_getMountPoint("demo-shadow") &&
				            read_save_fixture(mounted_output) == before,
				        "export rejects a shadowed source without retiring caller mounts or replacing output");
				require(PHYSFS_unmount("demo-shadow"), "retire only the fixture-owned shadow mount");
			}
			if (status >= 0) {
				const bytes previous = { 'k', 'e', 'e', 'p' };
				for (int payload = 0; payload < 4; ++payload) {
					const std::string broken = stem + "-short-" + std::to_string(payload) + ".dem";
					const std::string output = broken + ".jsonl";
					write_fixture(broken.c_str(), bytes(recording.begin(), recording.begin() + trigger_offset + 18 + payload));
					write_fixture(output.c_str(), previous);
					require(!newdemo_dump_json(broken.c_str(), output.c_str(), error, sizeof(error)) && read_save_fixture(output) == previous,
					        "truncated optional exit status fails without replacing prior output");
					require(std::strstr(error, "frame decode failed") && PHYSFS_getMountPoint("."),
					        "truncated status reaches the decoder and retains the caller-owned mount");
				}
				// Corrupt playback releases its mission; prepare the next independent operation
				char mission[] = "d2";
				require(load_mission_by_name(mission), "reload the mission after rejected corrupt recordings");
				LoadLevel(level, 0);
			}
			if (status != 0) {
				// Decode successive recorded frames using the supported no-interpolation setting
				Newdemo_do_interpolate = 0;
				FrameTime = F1_0 / 25;
				std::vector<char> filename(stem.begin(), stem.end());
				filename.insert(filename.end(), { '.', 'd', 'e', 'm', '\0' });
				newdemo_start_playback(filename.data());
				std::fprintf(stderr, "Playback boundary: state=%d window=%d level=%d expected_level=%d energy=%d\n", Newdemo_state, Game_wind != nullptr, Current_level_num, level, Players[0].energy);
				require(Newdemo_state == ND_STATE_PLAYBACK && Game_wind && Current_level_num == level && Players[0].energy == 73 * F1_0,
				        "normal playback reaches the following event without taking a rejected/native secret exit");
				fixture_close_order_game();
			}
			// The public endian converter reads the same frame decoder in rewrite mode
			Game_mode = 0;
			outfile = PHYSFSX_openWriteBuffered("demos/endian-native.dem");
			require(outfile != nullptr, "open complete object-free endian control");
			newdemo_record_start_demo();
			newdemo_record_start_frame(F1_0 / 10);
			newdemo_record_trigger(seg, side, 0, 1);
			if (status >= 0) newdemo_record_secret_exit_blown(status);
			newdemo_record_player_energy(61);
			newdemo_record_start_frame(F1_0 / 10);
			newdemo_write_end();
			require(PHYSFS_close(outfile), "close complete endian control");
			outfile = nullptr;
			const auto endian_native = read_save_fixture("demos/endian-native.dem");
			const std::string endian_name = stem + "-opposite.dem";
			write_fixture(("demos/" + endian_name).c_str(), classic_trigger_opposite_endian(endian_native));
			std::vector<char> endian_filename(endian_name.begin(), endian_name.end());
			endian_filename.push_back('\0');
			set_default_handler(classic_conversion_message_event);
			const int converted = newdemo_swap_endian(endian_filename.data());
			set_default_handler(nullptr);
			require(converted && read_save_fixture("demos/" + endian_name) == endian_native,
			        "public endian rewrite restores the complete native byte stream");
			results.push_back({ { "profile", profile }, { "level", level }, { "status", status }, { "bytes", recording.size() }, { "dumped", true }, { "played", status != 0 }, { "endian", true } });
			std::fprintf(stderr, "Complete classic demo passed: %s, level %d\n", stem.c_str(), level);
		}
	}
	return results;
}
#endif
