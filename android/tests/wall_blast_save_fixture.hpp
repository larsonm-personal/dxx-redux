// A wall blast changes passability and emits damaging SIM-randomized explosions
// Exercise its real caller and save/restore path in a loaded campaign level
static nlohmann::json exercise_wall_blast_save(const char *native_directory = nullptr)
{
	using nlohmann::json;
	json report = json::array();
	for (int elapsed : { 0, 8, 24, 25, 31 }) {
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		int wall = -1;
		for (int i = 0; i < Num_walls; ++i)
			if (Walls[i].type == WALL_BLASTABLE && (WallAnims[Walls[i].clip_num].flags & WCF_EXPLODES)) {
				wall = i;
				break;
			}
		require(wall >= 0, "actual campaign contains an exploding blastable wall");
		const int segment = Walls[wall].segnum, side = Walls[wall].sidenum;
		GameTime64 = 10 * F1_0;
		FrameTime = F1_0 / 32;
		d_srand(1234);
		d_srand_fx(5678);
		d_rand_reset_call_count();
		d_rand_reset_stream_call_count(D_RNG_FX);
		wall_destroy(&Segments[segment], side);
		for (int frame = 0; frame < elapsed; ++frame) {
			GameTime64 += FrameTime;
			do_exploding_wall_frame();
		}
		std::string filename = "wall-blast-" + std::to_string(elapsed) + ".sav";
		char description[21] = "Active wall blast";
		require(state_save_all_sub(&filename[0], description), "save during actual wall blast");
		const auto saved = read_save_fixture(filename);
		const auto opposite = save_endian_fixture(saved).convert();
		const std::string opposite_path = "opposite-" + filename;
		write_fixture(opposite_path.c_str(), opposite);
		const auto finish = [&]() {
			json frames = json::array();
			const auto initial_draws = d_rand_get_call_count();
			for (int frame = elapsed; frame < 33; ++frame) {
				GameTime64 += FrameTime;
				do_exploding_wall_frame();
				unsigned seed = 0;
				require(d_rand_get_state(&seed), "observe wall blast SIM seed");
				frames.push_back({ { "frame", frame }, { "flags", Walls[wall].flags }, { "hps", Walls[wall].hps },
				                   { "doorway", wall_is_doorway(&Segments[segment], side) },
				                   { "objects", Highest_object_index + 1 }, { "rng", seed },
				                   { "draws", d_rand_get_call_count() - initial_draws } });
			}
			return frames;
		};
		const auto uninterrupted = finish();
#ifdef DXX_BUILD_DESCENT_II
		require(state_restore_all_sub(&filename[0], 0), "restore wall blast through ordinary D2 save reader");
#else
		require(state_restore_all_sub(&filename[0]), "restore wall blast through ordinary D1 save reader");
#endif
		FrameTime = F1_0 / 32;
		const auto restored = finish();
		report.push_back({ { "elapsed", elapsed }, { "uninterrupted", uninterrupted }, { "restored", restored },
		                   { "differences", json::diff(uninterrupted, restored) } });
		std::string endian_file = opposite_path;
#ifdef DXX_BUILD_DESCENT_II
		require(state_restore_all_sub(&endian_file[0], 0), "restore opposite-endian wall blast");
#else
		require(state_restore_all_sub(&endian_file[0]), "restore opposite-endian wall blast");
#endif
		FrameTime = F1_0 / 32;
		const auto endian_frames = finish();
		report.back()["endian_differences"] = json::diff(uninterrupted, endian_frames);
#ifdef DXX_BUILD_DESCENT_II
		if (native_directory) {
			for (const auto &source : { filename, opposite_path }) {
				const auto data = read_save_fixture("native-checkpoints/" + source);
				d1_save_translate_checkpoint_start start = {};
				require(d1_save_translate_read_checkpoint_start(data.data(), data.size(), &start), "decode native wall-blast checkpoint");
				input_demo_set_skip_level_intro(1);
				StartNewGame(start.current_level);
				GameTime64 = start.game_time;
				Difficulty_level = start.difficulty;
				const std::vector<object> before_objects(Objects, Objects + Highest_object_index + 1);
				const std::vector<expl_wall> before_blasts(expl_wall_list, expl_wall_list + MAX_EXPLODING_WALLS);
				for (int malformed = 0; malformed < 6; ++malformed) {
					auto damaged = data;
					if (!malformed) damaged.pop_back();
					else {
						const int field = malformed <= 2 ? malformed - 1 : malformed == 5 ? 0 : 2;
						const int value = malformed == 1 ? -2 : malformed == 2 ? 6 : malformed == 3 ? -1 : malformed == 4 ? F1_0 + 1 : Highest_segment_index + 1;
						set_int(damaged, damaged.size() - EXPLODING_WALL_RUNTIME_DISK_BYTES + field * 4,
						        start.checkpoint_swap ? SWAPINT(value) : value);
					}
					require(!d1_save_translate_apply_checkpoint_objects(damaged.data(), damaged.size(), &start),
					        "reject truncated or invalid wall-blast checkpoint before applying saved state");
					require(std::memcmp(Objects, before_objects.data(), before_objects.size() * sizeof(object)) == 0 &&
					            std::memcmp(expl_wall_list, before_blasts.data(), before_blasts.size() * sizeof(expl_wall)) == 0,
					        "rejected wall-blast checkpoint leaves objects and active blasts unchanged");
				}
				require(d1_save_translate_apply_checkpoint_objects(data.data(), data.size(), &start), "import native wall blast through normal checkpoint adapter");
				d1_save_translate_apply_checkpoint_player(&start, "walltest");
				FrameTime = F1_0 / 32;
				const auto translated = finish();
				report.back()["import_differences"][source] = json::diff(uninterrupted, translated);
				report.back()["rejected_mutations"][source] = 6;
				if (source == filename) {
					auto legacy = data;
					legacy.resize(legacy.size() - EXPLODING_WALL_RUNTIME_DISK_BYTES);
					set_int(legacy, 4, 18);
					d1_save_translate_checkpoint_start legacy_start = {};
					require(d1_save_translate_read_checkpoint_start(legacy.data(), legacy.size(), &legacy_start), "decode prior native save format");
					expl_wall_list[9] = { segment, side, F1_0 / 2 };
					require(d1_save_translate_apply_checkpoint_objects(legacy.data(), legacy.size(), &legacy_start), "admit native version 18 without a wall-blast record");
					for (const auto &blast : expl_wall_list)
						require(blast.segnum == -1, "legacy checkpoint clears unavailable wall-blast state instead of inheriting another world");
				}
			}
		}
#else
		(void) native_directory;
#endif
	}
	return report;
}

static void write_wall_blast_save_trace(const char *d2_directory, const char *native_directory)
{
	key_init();
	mouse_init();
	texmerge_init(10);
	init_game();
	Game_mode = 0;
	GameArg.SysInputDemoNoRender = 1;
	Player_num = 0;
	N_players = 1;
	std::strcpy(Players[0].callsign, "walltest");
	Difficulty_level = 2;
	if (native_directory) require(PHYSFS_mount(native_directory, "native-checkpoints", 0), "mount native wall-blast checkpoints");
	const auto native = exercise_wall_blast_save(native_directory);
	const auto output = native.dump(2) + "\n";
	write_fixture("wall-blast.json", bytes(output.begin(), output.end()));
#ifdef DXX_BUILD_DESCENT_II
	if (d2_directory) {
		const std::string hog = std::string(d2_directory) + "/descent2.hog";
		require(PHYSFS_mount(d2_directory, nullptr, 0) && PHYSFS_mount(hog.c_str(), nullptr, 0), "mount ordinary D2 wall resources");
		char mission[] = "d2";
		require(load_mission_by_name(mission), "select ordinary D2 wall persistence control");
		const auto control = exercise_wall_blast_save().dump(2) + "\n";
		write_fixture("wall-blast-d2.json", bytes(control.begin(), control.end()));
	}
#else
	(void) d2_directory;
#endif
}
