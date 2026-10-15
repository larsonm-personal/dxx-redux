// A wall blast changes passability and emits damaging SIM-randomized explosions
// Exercise its real caller and save/restore path in a loaded campaign level
static nlohmann::json exercise_wall_blast_save(const char *native_directory = nullptr, bool shareware = false, bool ordinary_d2 = false)
{
	using nlohmann::json;
	json report = json::array();
	for (int elapsed : { 0, 8, 24, 25, 31 }) {
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		int wall = -1;
		for (int i = 0; i < Num_walls; ++i)
			if (Walls[i].type == WALL_BLASTABLE && Walls[i].clip_num >= 0 && Walls[i].clip_num < Num_wall_anims && (WallAnims[Walls[i].clip_num].flags & WCF_EXPLODES)) {
				wall = i;
				break;
			}
		require(wall >= 0, "actual campaign contains an exploding blastable wall");
		const int segment = Walls[wall].segnum, side = Walls[wall].sidenum;
		if (shareware) {
			require(std::strcmp(Level_names[0], ordinary_d2 ? "d2leva-1.sl2" : "level01.sdl") == 0,
			        "public save fixture selects the pinned demo level");
			require(wall == (ordinary_d2 ? 37 : 10) && Walls[wall].clip_num == 6,
			        "public save fixture selects the authentic exploding wall, without substituting geometry");
		}
		const json selection = { { "edition", shareware ? "shareware" : "retail" }, { "content_game", ordinary_d2 ? "d2" : "d1" }, { "mission", Current_mission_filename }, { "level", Level_names[0] }, { "level_num", Current_level_num }, { "wall", wall }, { "segment", segment }, { "side", side }, { "clip", +Walls[wall].clip_num }, { "wall_type", +Walls[wall].type }, { "clip_flags", +WallAnims[Walls[wall].clip_num].flags } };
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
		std::string filename = std::string(ordinary_d2 ? "d2-wall-blast-" : "wall-blast-") + std::to_string(elapsed) + ".sav";
		char description[21] = "Active wall blast";
		// Both native save subroutines consume the pause owned by their normal caller
		stop_time();
		require(state_save_all_sub(&filename[0], description), "save during actual wall blast");
		const auto saved = read_save_fixture(filename);
		const auto opposite = save_endian_fixture(saved).convert();
		require(save_endian_fixture(opposite).convert() == saved, "independent endian conversion preserves every save byte reversibly");
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
				frames.push_back({ { "frame", frame }, { "flags", +Walls[wall].flags }, { "hps", +Walls[wall].hps }, { "doorway", wall_is_doorway(&Segments[segment], side) }, { "objects", Highest_object_index + 1 }, { "rng", seed }, { "draws", d_rand_get_call_count() - initial_draws } });
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
		report.push_back({ { "elapsed", elapsed }, { "selection", selection }, { "uninterrupted", uninterrupted }, { "restored", restored }, { "differences", json::diff(uninterrupted, restored) } });
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
				// Native v21 appends phase and Fusion-active uint32 words after the pool.
				const int trailing_runtime_bytes = start.version >= 21 ? 8 : 0;
				input_demo_set_skip_level_intro(1);
				StartNewGame(start.current_level);
				GameTime64 = start.game_time;
				Difficulty_level = start.difficulty;
				const std::vector<object> before_objects(Objects, Objects + Highest_object_index + 1);
				const std::vector<expl_wall> before_blasts(expl_wall_list, expl_wall_list + MAX_EXPLODING_WALLS);
				const std::vector<::wall> before_walls(Walls, Walls + Num_walls);
				const std::vector<::segment> before_segments(Segments, Segments + Highest_segment_index + 1);
				const auto before_player = Players[Player_num];
				const int before_highest_object = Highest_object_index, before_num_walls = Num_walls;
				const auto before_time = GameTime64;
				unsigned before_rng = 0, before_fx_rng = 0;
				require(d_rand_get_state(&before_rng) && d_rand_get_stream_state(D_RNG_FX, &before_fx_rng), "snapshot checkpoint rejection RNG");
				const auto before_draws = d_rand_get_call_count(), before_fx_draws = d_rand_get_stream_call_count(D_RNG_FX);
				const auto reject_unchanged = [&](const bytes &damaged) {
					require(!d1_save_translate_apply_checkpoint_objects(damaged.data(), damaged.size(), &start),
					        "reject truncated or invalid wall-blast checkpoint before applying saved state");
					unsigned rng = 0, fx_rng = 0;
					require(d_rand_get_state(&rng) && d_rand_get_stream_state(D_RNG_FX, &fx_rng) &&
					            rng == before_rng && fx_rng == before_fx_rng && d_rand_get_call_count() == before_draws &&
					            d_rand_get_stream_call_count(D_RNG_FX) == before_fx_draws && GameTime64 == before_time &&
					            Highest_object_index == before_highest_object && Num_walls == before_num_walls &&
					            std::memcmp(Objects, before_objects.data(), before_objects.size() * sizeof(object)) == 0 &&
					            std::memcmp(Walls, before_walls.data(), before_walls.size() * sizeof(::wall)) == 0 &&
					            std::memcmp(Segments, before_segments.data(), before_segments.size() * sizeof(::segment)) == 0 &&
					            std::memcmp(&Players[Player_num], &before_player, sizeof(before_player)) == 0 &&
					            std::memcmp(expl_wall_list, before_blasts.data(), before_blasts.size() * sizeof(expl_wall)) == 0,
					        "rejected checkpoint leaves objects, walls, segments, player, time, RNG and active blasts unchanged");
				};
				for (int malformed = 0; malformed < 6; ++malformed) {
					auto damaged = data;
					if (!malformed) damaged.pop_back();
					else {
						const int field = malformed <= 2 ? malformed - 1 : malformed == 5 ? 0
						                                                                  : 2;
						const int value = malformed == 1 ? -2 : malformed == 2 ? 6
						                                    : malformed == 3   ? -1
						                                    : malformed == 4   ? F1_0 + 1
						                                                       : Highest_segment_index + 1;
						set_int(damaged, damaged.size() - trailing_runtime_bytes - EXPLODING_WALL_RUNTIME_DISK_BYTES + field * 4,
						        start.checkpoint_swap ? SWAPINT(value) : value);
					}
					reject_unchanged(damaged);
				}
				// Every truncation boundary in the appended pool must fail atomically
				for (int missing = 2; missing <= EXPLODING_WALL_RUNTIME_DISK_BYTES + trailing_runtime_bytes; ++missing) {
					auto damaged = data;
					damaged.resize(damaged.size() - missing);
					reject_unchanged(damaged);
				}
				auto last_slot = data;
				set_int(last_slot, last_slot.size() - trailing_runtime_bytes - 12, start.checkpoint_swap ? SWAPINT(-2) : -2);
				reject_unchanged(last_slot);
				require(d1_save_translate_apply_checkpoint_objects(data.data(), data.size(), &start), "import native wall blast through normal checkpoint adapter");
				d1_save_translate_apply_checkpoint_player(&start, "walltest");
				FrameTime = F1_0 / 32;
				const auto translated = finish();
				report.back()["import_differences"][source] = json::diff(uninterrupted, translated);
				report.back()["rejected_mutations"][source] = EXPLODING_WALL_RUNTIME_DISK_BYTES + trailing_runtime_bytes + 6;
				if (source == filename) {
					auto legacy = data;
					legacy.resize(legacy.size() - EXPLODING_WALL_RUNTIME_DISK_BYTES - trailing_runtime_bytes);
					if (start.version >= 23) {
						constexpr size_t epoch_offset = 8 + 20 + 100 * 50 * 3 + 4 + 9 + 12;
						legacy.erase(legacy.begin() + epoch_offset, legacy.begin() + epoch_offset + 8);
					}
					set_int(legacy, 4, 18);
					d1_save_translate_checkpoint_start legacy_start = {};
					require(d1_save_translate_read_checkpoint_start(legacy.data(), legacy.size(), &legacy_start), "decode prior native save format");
					expl_wall_list[9] = { segment, side, F1_0 / 2 };
					require(d1_save_translate_apply_checkpoint_objects(legacy.data(), legacy.size(), &legacy_start), "admit native version 18 without a wall-blast record");
					report.back()["legacy_cleared"] = true;
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

static void write_wall_blast_save_trace(const char *d2_directory, const char *native_directory, bool shareware)
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
	const auto native = exercise_wall_blast_save(native_directory, shareware);
	const auto output = native.dump(2) + "\n";
	write_fixture("wall-blast.json", bytes(output.begin(), output.end()));
#ifdef DXX_BUILD_DESCENT_II
	if (d2_directory) {
		const std::string hog = std::string(d2_directory) + (shareware ? "/d2demo.hog" : "/descent2.hog");
		require(PHYSFS_mount(d2_directory, nullptr, 0) && PHYSFS_mount(hog.c_str(), nullptr, 0), "mount ordinary D2 wall resources");
		char retail_mission[] = "d2", public_mission[] = "d2demo";
		char *mission = shareware ? public_mission : retail_mission;
		require(load_mission_by_name(mission), "select ordinary D2 wall persistence control");
		const auto control = exercise_wall_blast_save(nullptr, shareware, true).dump(2) + "\n";
		write_fixture("wall-blast-d2.json", bytes(control.begin(), control.end()));
	}
#else
	(void) d2_directory;
#endif
}
