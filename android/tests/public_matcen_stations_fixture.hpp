extern "C" fix Fuelcen_give_amount;
// Controlled station-loop fixture using real public levels and native production
// Trigger invocation and retiring each produced robot are fixture operations;
// this deliberately does not claim a normal player route or combat recording
static void test_public_matcen_stations(const char *directory, const char *output, bool activate = true)
{
	using nlohmann::json;
#ifdef DXX_BUILD_DESCENT_II
	const char *game_name = "d2";
	const char *archive = "/d2demo.hog";
	char mission[] = "d2demo";
	const int level = 2, expected_centers = 3;
#else
	const char *game_name = "d1";
	const char *archive = "/descent.hog";
	char mission[] = "";
	const int level = 3, expected_centers = 2;
#endif
	const std::string hog = std::string(directory) + archive;
	require(PHYSFS_mount(directory, nullptr, 1) && PHYSFS_mount(hog.c_str(), nullptr, 1), "mount public station resources");
	char error[256] = "";
	require(input_demo_configure_startup_fp_environment(error, sizeof(error)), error);
	Game_mode = GM_NORMAL;
	GameArg.SndNoSound = GameArg.SndNoMusic = 1;
	GameArg.SysInputDemoNoRender = 1;
	GameCfg.AspectX = 4;
	GameCfg.AspectY = 3;
	Game_screen_mode = SM(640, 480);
	load_text();
	require(init_headless_audio(), "initialize station fixture without video");
	char palette[] = "palette.256";
	gr_use_palette_table(palette);
	gamedata_init();
	require(std::atexit([] { digi_close(); gamedata_close(); }) == 0, "register native game-data cleanup");
	texmerge_init(10);
#ifdef DXX_BUILD_DESCENT_II
	d1_in_d2_init_startup_bitmaps();
#endif
	static grs_screen screen;
	static unsigned char pixels[640 * 480];
	grd_curscreen = &screen;
	screen.sc_mode = Game_screen_mode;
	screen.sc_w = 640;
	screen.sc_h = 480;
	screen.sc_aspect = F1_0;
	gr_init_canvas(&screen.sc_canvas, pixels, BM_LINEAR, 640, 480);
	gr_set_current_canvas(nullptr);
	Screen_mode = SCREEN_GAME;
	init_game();
	new_player_config();
	require(load_mission_by_name(mission), "load authored public station mission");
	Player_num = 0;
	N_players = 1;
	std::strcpy(Players[0].callsign, "stations");
	json cases = json::array(), ordinary_rounds = json::object();
	for (int difficulty : { 2, 4 })
		for (int center = 0; center < expected_centers; ++center)
			for (int policy = 0; policy < 4; ++policy) {
				Difficulty_level = difficulty;
				input_demo_set_skip_level_intro(1);
				StartNewGame(level);
				require(Num_robot_centers == expected_centers, "public level contains its exact nonzero authored matcen set");
				const std::vector<FuelCenter> authored_stations(Station, Station + Num_fuelcenters);
				const fix authored_fuel_rate = Fuelcen_give_amount;
				require(authored_fuel_rate > 0, "loaded energy centers have a positive native refueling rate");
				auto energy_unchanged = [&]() {
					require(Fuelcen_give_amount == authored_fuel_rate, "matcen policy preserves the positive native refueling rate");
					for (size_t i = 0; i != authored_stations.size(); ++i)
						if (authored_stations[i].Type == SEGMENT_IS_FUELCEN)
							require(std::memcmp(&authored_stations[i], &Station[i], sizeof(FuelCenter)) == 0, "matcen policy preserves loaded energy-center station state");
				};
				Game_suspended = 0;
				FrameTime = F1_0 / 60;
				d_srand(1234);
				d_rand_reset_call_count();
				const int station = RobotCenters[center].fuelcen_num;
				FuelCenter &matcen = Station[station];
				require(matcen.Type == SEGMENT_IS_ROBOTMAKER && !matcen.Enabled && matcen.Lives == 3, "real matcen starts disabled with authored lives");
				require(ConsoleObject->segnum != matcen.segnum, "player does not block production segment");
				json cleared = json::array();
				for (int obj = 0; obj <= Highest_object_index; ++obj)
					if (Objects[obj].segnum == matcen.segnum && Objects[obj].type == OBJ_ROBOT) {
						cleared.push_back({ obj, +Objects[obj].id, +Objects[obj].signature });
						obj_delete(obj);
					}
				const int mode = policy == 3 ? MATCEN_MODE_DEFAULT : policy;
				require(matcen_set_mode(mode), "set requested native matcen policy");
				energy_unchanged();
				const int original_robots = Players[0].num_robots_level;
				const int original_kills = Players[0].num_kills_level;
				json births = json::array();
				auto counts = [&]() {
					uint8_t values[MATCEN_MODE_MAX_CENTERS] = {};
					matcen_mode_get_activation_counts(values, sizeof(values));
					return int(values[center]);
				};
				auto advance = [&](int frames, int round) {
					for (int frame = 0; frame != frames; ++frame) {
						GameTime64 += FrameTime;
						fuelcen_update_all();
						for (int obj = 0; obj <= Highest_object_index; ++obj) {
							const object &robot = Objects[obj];
							if (robot.type != OBJ_ROBOT || static_cast<ubyte>(robot.matcen_creator) != (station | 0x80)) continue;
							require(robot.segnum == matcen.segnum && robot.shields == Robot_info[robot.id].strength, "native robot has real station origin and loaded shields");
#ifdef DXX_BUILD_DESCENT_II
							require(RobotCenters[center].robot_flags[robot.id / 32] & (1u << (robot.id % 32)), "native robot belongs to authored robot mask");
#else
							require(RobotCenters[center].robot_flags[0] & (1u << robot.id), "native robot belongs to authored robot mask");
#endif
							births.push_back({ { "round", round }, { "frame", frame }, { "object", obj }, { "id", +robot.id }, { "signature", robot.signature }, { "shields", robot.shields } });
							// Isolate station production from robot congestion and combat
							obj_delete(obj);
						}
					}
				};
				if (activate) trigger_matcen(matcen.segnum);
				if (policy == 3) {
					advance(1, 0);
					require(matcen.Enabled && matcen.Flag == 1 && births.empty(), "active station has begun its genuine morph delay");
					require(matcen_set_mode(MATCEN_MODE_PAUSED), "pause pending native production");
					energy_unchanged();
				}
				advance(60 * 40, 1);
				energy_unchanged();
				const int first_births = int(births.size());
				std::fprintf(stderr, "MATCEN center=%d difficulty=%d policy=%d births=%d enabled=%d flag=%d capacity=%d timer=%d remaining=%d counts=%d robot_total=%d original=%d\n", center, difficulty, policy, first_births, matcen.Enabled, matcen.Flag, matcen.Capacity, matcen.Timer, matcen.Disable_time, counts(), Players[0].num_robots_level, Gamesave_num_org_robots);
				const bool suppressed = mode == MATCEN_MODE_PAUSED || policy == 3;
				require((suppressed ? first_births == 0 : first_births > 1 && first_births <= difficulty + 3) && !matcen.Enabled, "first activation produces multiple real robots or is genuinely suppressed");
				const std::string identity = std::to_string(difficulty) + ":" + std::to_string(center);
				json production = births;
				for (auto &birth : production) {
					birth.erase("object");
					birth.erase("signature");
				}
				if (policy == MATCEN_MODE_DEFAULT) ordinary_rounds[identity] = production;
				if (policy == MATCEN_MODE_ONE_ROUND) require(production == ordinary_rounds[identity], "one-round preserves the complete native first production round");
				const int first_count = counts(), first_lives = matcen.Lives;
				require(first_count == (mode == MATCEN_MODE_PAUSED ? 0 : 1), "production uses one actual activation, not one count per robot");
#ifdef DXX_BUILD_DESCENT_II
				const bool consumes_life = difficulty != 4 || mode == MATCEN_MODE_ONE_ROUND;
#else
				const bool consumes_life = true;
#endif
				require(first_lives == 3 - (first_count && consumes_life), "preserve intentional D1/D2 highest-difficulty life rules");
				if (activate) trigger_matcen(matcen.segnum);
				advance(60 * 40, 2);
				energy_unchanged();
				const int second_births = int(births.size()) - first_births;
				require((policy == MATCEN_MODE_DEFAULT ? second_births > 1 && second_births <= difficulty + 3 : second_births == 0) && !matcen.Enabled, "default reactivates while one-round and paused reject a second production round");
				require(counts() == first_count + (second_births != 0), "suppressed retriggers cannot consume activation counts");
				require(matcen.Lives == first_lives - (second_births != 0 && consumes_life), "suppressed retriggers cannot consume lives");
				require(Players[0].num_robots_level == original_robots + int(births.size()) && Players[0].num_kills_level == original_kills, "real production updates native robot total without fabricated kills");
				int energy_centers = 0;
				for (int i = 0; i != Num_fuelcenters; ++i)
					if (authored_stations[i].Type == SEGMENT_IS_FUELCEN) {
						++energy_centers;
						const FuelCenter before = Station[i];
						require(fuelcen_give_fuel(&Segments[Station[i].segnum], F1_0) == fixmul(FrameTime, authored_fuel_rate), "matcen policy leaves actual energy-center refueling unchanged");
						require(std::memcmp(&before, &Station[i], sizeof(before)) == 0, "refueling preserves authored energy-center station state");
					}
				energy_unchanged();
				require(energy_centers > 0, "public level contains actual energy centers");
				cases.push_back({ { "difficulty", difficulty }, { "center", center }, { "station", station }, { "segment", +matcen.segnum }, { "policy", policy }, { "cleared_authored_robots", cleared }, { "first_births", first_births }, { "second_births", second_births }, { "first_lives", first_lives }, { "second_lives", +matcen.Lives }, { "energy_state_preserved", true }, { "activation_count", counts() }, { "energy_centers", energy_centers }, { "births", births } });
			}
	require(!SDL_WasInit(SDL_INIT_VIDEO), "public station fixture never initializes video");
	const json report = { { "schema", 1 }, { "scope", "controlled_public_station_loop" }, { "game", game_name }, { "level", level }, { "matcens", expected_centers }, { "cases", cases }, { "passed", true } };
	const auto text = report.dump(2) + "\n";
	write_fixture(output, bytes(text.begin(), text.end()));
	std::puts("PASS: controlled public matcen production, suppression and energy centers");
}
