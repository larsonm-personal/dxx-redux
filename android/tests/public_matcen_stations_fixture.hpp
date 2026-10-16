#include "matcen_regression.hpp"
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
	// Synthetic trigger links on unchanged public station mappings.  These are
	// dispatch witnesses, not authored trigger routes or player/combat recordings.
	json dispatch_cases = json::array();
	for (const char *scenario : { "wall_dispatch", "sub", "reverse", "no_dispatch", "empty_mask" }) {
		Difficulty_level = 2;
		input_demo_set_skip_level_intro(1);
		StartNewGame(level);
		require(matcen_set_mode(MATCEN_MODE_DEFAULT), "reset dispatch witness policy");
		Game_suspended = 0;
		FrameTime = F1_0 / 60;
		d_srand(1234);
		d_rand_reset_call_count();
		const bool reverse = !std::strcmp(scenario, "reverse");
		const bool dispatch = std::strcmp(scenario, "no_dispatch") != 0;
		const bool empty_mask = !std::strcmp(scenario, "empty_mask");
		json mapping = json::array(), cleared = json::array();
		for (int center = 0; center < Num_robot_centers; ++center) {
			const int station = RobotCenters[center].fuelcen_num, segment = Station[station].segnum;
#ifdef DXX_BUILD_DESCENT_II
			const int ordinal = Segment2s[segment].matcen_num;
#else
			const int ordinal = Segments[segment].matcen_num;
#endif
			require(ordinal == center && ConsoleObject->segnum != segment, "loaded public dispatch mapping retains its generated ordinal");
			json mask = json::array();
			for (unsigned word = 0; word < sizeof(RobotCenters[center].robot_flags) / sizeof(RobotCenters[center].robot_flags[0]); ++word) {
				mask.push_back(static_cast<uint32_t>(RobotCenters[center].robot_flags[word]));
				if (empty_mask) RobotCenters[center].robot_flags[word] = 0;
			}
			mapping.push_back({ { "ordinal", ordinal }, { "station", station }, { "segment", segment }, { "robot_flags", mask } });
			for (int obj = 0; obj <= Highest_object_index; ++obj)
				if (Objects[obj].type == OBJ_ROBOT && Objects[obj].segnum == segment) {
					cleared.push_back({ obj, +Objects[obj].id, +Objects[obj].signature });
					obj_delete(obj);
				}
		}
		// Preserve and disclose ordered links, including the repeated first target.
		const int first = reverse ? 0 : expected_centers - 1, second = reverse ? expected_centers - 1 : 0;
		const int first_segment = Station[RobotCenters[first].fuelcen_num].segnum;
		const int second_segment = Station[RobotCenters[second].fuelcen_num].segnum;
		json authored_triggers = matcen_regression_serialize<json>()["triggers"];
		for (int index = 0; index < Num_triggers; ++index) {
			authored_triggers[index]["runtime_type"] = +Triggers[index].type;
			authored_triggers[index]["runtime_flags"] = +Triggers[index].flags;
		}
		require(Num_triggers > 0 && Num_walls > 0, "public dispatch fixture has authored triggers and walls");
		Triggers[0] = {};
#ifdef DXX_BUILD_DESCENT_II
		Triggers[0].type = TT_MATCEN;
#else
		Triggers[0].flags = TRIGGER_MATCEN;
#endif
		Triggers[0].num_links = 3;
		Triggers[0].seg[0] = Triggers[0].seg[2] = first_segment;
		Triggers[0].seg[1] = second_segment;
		const int source_segment = ConsoleObject->segnum, source_side = 0;
		Segments[source_segment].sides[source_side].wall_num = 0;
		Walls[0].trigger = 0;
		std::vector<int> existing_signatures;
		for (int obj = 0; obj <= Highest_object_index; ++obj)
			if (Objects[obj].type != OBJ_NONE) existing_signatures.push_back(Objects[obj].signature);
		if (dispatch) {
			if (!std::strcmp(scenario, "sub")) require(check_trigger_sub(0, Player_num, 0) == 0, "native matcen trigger dispatch succeeds");
			else check_trigger(&Segments[source_segment], source_side, Players[Player_num].objnum, 0);
		}
		// Each successful trigger_matcen creates a native light before any frame
		// advance.  Signature order witnesses dispatch order and duplicate gating.
		std::vector<std::pair<int, int>> lights;
		for (int obj = 0; obj <= Highest_object_index; ++obj)
			if (Objects[obj].type == OBJ_LIGHT && std::find(existing_signatures.begin(), existing_signatures.end(), Objects[obj].signature) == existing_signatures.end())
				lights.emplace_back(static_cast<int>(Objects[obj].signature), static_cast<int>(Objects[obj].segnum));
		std::sort(lights.begin(), lights.end());
		json activation_lights = json::array();
		for (const auto &light : lights) activation_lights.push_back({ { "signature", light.first }, { "segment", light.second } });
		require(lights.size() == (dispatch ? 2u : 0u), "duplicate trigger link does not activate twice");
		if (dispatch) require(lights[0].second == first_segment && lights[1].second == second_segment, "native activation lights preserve actual ordered target dispatch");
		uint8_t activation_counts[MATCEN_MODE_MAX_CENTERS] = {};
		matcen_mode_get_activation_counts(activation_counts, sizeof(activation_counts));
		json counts = json::array(), births = json::array();
		for (int center = 0; center < expected_centers; ++center) {
			const bool active = dispatch && (center == first || center == second);
			require(activation_counts[center] == active, "trigger dispatch activates exactly its unique mapped ordinals");
			counts.push_back(activation_counts[center]);
		}
		for (int frame = 0; frame < 2400; ++frame) {
			GameTime64 += FrameTime;
			fuelcen_update_all();
			for (int obj = 0; obj <= Highest_object_index; ++obj) {
				const object &robot = Objects[obj];
				if (robot.type != OBJ_ROBOT || std::find(existing_signatures.begin(), existing_signatures.end(), robot.signature) != existing_signatures.end()) continue;
				require(static_cast<ubyte>(robot.matcen_creator) & 0x80, "new dispatch robot has native station creator");
				const int station = static_cast<ubyte>(robot.matcen_creator) & 0x7f;
				int ordinal = -1;
				for (int center = 0; center < expected_centers; ++center)
					if (RobotCenters[center].fuelcen_num == station) ordinal = center;
				require(ordinal == first || ordinal == second, "dispatch birth originates at a targeted station");
				require(robot.segnum == Station[station].segnum, "dispatch birth retains actual station segment");
				require(robot.shields == Robot_info[robot.id].strength, "dispatch robot retains loaded native shields");
				require(RobotCenters[ordinal].robot_flags[robot.id / 32] & (1u << (robot.id % 32)), "dispatch birth retains mapped robot mask");
				births.push_back({ { "frame", frame }, { "object", obj }, { "signature", robot.signature }, { "id", +robot.id }, { "shields", robot.shields }, { "station", station }, { "segment", +robot.segnum }, { "ordinal", ordinal } });
				obj_delete(obj);
			}
		}
		for (int center : { first, second }) {
			int produced = 0;
			for (const auto &birth : births) if (birth["ordinal"] == center) ++produced;
			require(dispatch && !empty_mask ? produced > 1 && produced <= 5 : produced == 0, "dispatch and mask controls bound actual native production");
		}
		dispatch_cases.push_back({ { "scenario", scenario }, { "authored_triggers", authored_triggers }, { "synthetic_links", true }, { "zeroed_robot_masks", empty_mask }, { "source_segment", source_segment }, { "source_side", source_side }, { "trigger", 0 }, { "links", { first_segment, second_segment, first_segment } }, { "mapping", mapping }, { "cleared_authored_robots", cleared }, { "activation_lights", activation_lights }, { "activation_counts", counts }, { "births", births } });
	}
	require(!SDL_WasInit(SDL_INIT_VIDEO), "public station fixture never initializes video");
	const json report = { { "schema", 1 }, { "scope", "controlled_public_station_loop" }, { "game", game_name }, { "level", level }, { "matcens", expected_centers }, { "cases", cases }, { "dispatch_cases", dispatch_cases }, { "passed", true } };
	const auto text = report.dump(2) + "\n";
	write_fixture(output, bytes(text.begin(), text.end()));
	std::puts("PASS: controlled public matcen production, suppression and energy centers");
}
