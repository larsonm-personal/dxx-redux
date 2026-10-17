#include "matcen_regression.hpp"
extern "C" int Num_extry_robots;
// Controlled native authored-trigger probe. No trigger/link/mask rewrites.
static void test_authored_matcen(const char *directory, const char *extra, const char *mission, int level, const char *output, bool dispatch, int lifecycle_difficulty = -1)
{
	using nlohmann::json;
	const bool lifecycle = lifecycle_difficulty >= 0;
	require(lifecycle_difficulty >= -1 && lifecycle_difficulty <= 4, "authored lifecycle difficulty is zero through four");
	const int difficulty = lifecycle ? lifecycle_difficulty : 2;
#ifdef DXX_BUILD_DESCENT_II
	const char *game_name = "d2", *archive = "/descent2.hog";
#else
	const char *game_name = "d1", *archive = "/descent.hog";
#endif
	std::vector<char> mission_name(mission, mission + std::strlen(mission) + 1);
	const std::string hog = std::string(directory) + archive;
	require(PHYSFS_mount(directory, nullptr, 1) && PHYSFS_mount(hog.c_str(), nullptr, 1), "mount authored mission base resources");
	require(PHYSFS_mount(extra, nullptr, 0), "mount authored extra mission stage");
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
	require(load_mission_by_name(mission_name.data()), "load authored mission");
	Player_num = 0;
	N_players = 1;
	std::strcpy(Players[0].callsign, "stations");

	auto load = [&]() {
		Difficulty_level = difficulty;
		input_demo_set_skip_level_intro(1);
		StartNewGame(level);
		require(Current_level_num == level, "authored fixture loaded requested level");
		require(matcen_set_mode(MATCEN_MODE_DEFAULT), "default authored matcen policy");
		Game_suspended = 0;
		GameTime64 = 0;
		FrameTime = F1_0 / 60;
		d_srand(1234);
		d_rand_reset_call_count();
	};
	load();
	if (lifecycle) require(Matcen_regression_capture.complete && !Matcen_regression_capture.overflow, "lifecycle baseline is a complete admitted fresh-level capture");
	const json baseline = matcen_regression_serialize<json>();
	const int trigger_count = Num_triggers;
	require(trigger_count >= 0 && trigger_count <= MAX_TRIGGERS, "physical trigger count bounds");
	json cases = json::array();
	int lifecycle_executed = 0, lifecycle_births = 0;
	for (int index = 0; index < trigger_count; ++index) {
		load();
		require(Num_triggers == trigger_count, "fresh load preserves trigger identities");
		const auto &t = Triggers[index];
		bool matcen = false, mixed = false;
#ifdef DXX_BUILD_DESCENT_II
		short source_flags = 0;
		if (d1_in_d2_trigger_source_flags(&t, &source_flags)) {
			matcen = source_flags & TRIGGER_MATCEN;
			mixed = source_flags & ~(TRIGGER_MATCEN | TRIGGER_ON | TRIGGER_ONE_SHOT);
		} else matcen = t.type == TT_MATCEN;
#else
		matcen = t.flags & TRIGGER_MATCEN;
		mixed = t.flags & ~(TRIGGER_MATCEN | TRIGGER_ON | TRIGGER_ONE_SHOT);
#endif
		if (!matcen) continue;
		json entry = { { "trigger", index }, { "dispatch_requested", dispatch }, { "load_snapshot", matcen_regression_serialize<json>() }, { "runtime_type", +t.type }, { "runtime_flags", +t.flags } };
		if (lifecycle) require(entry["load_snapshot"] == baseline, "fresh lifecycle loads preserve the same captured authored bindings");
		json links = json::array(), reasons = json::array();
		std::vector<int> centers;
		if (mixed) reasons.push_back("unsupported_mixed_action_trigger");
#ifdef DXX_BUILD_DESCENT_II
		if (!d1_in_d2_trigger_source_flags(&t, &source_flags) && (t.flags & TF_DISABLED)) reasons.push_back("authored_trigger_disabled");
		if (lifecycle) {
			const bool native_d1 = d1_in_d2_trigger_source_flags(&t, &source_flags);
			entry["trigger_format"] = native_d1 ? "native_d1_in_d2" : "native_d2";
			if (native_d1) entry["source_flags"] = source_flags;
			if (native_d1 ? (source_flags & TRIGGER_ONE_SHOT) : (t.flags & TF_ONE_SHOT)) reasons.push_back("one_shot_outside_repeatable_lifecycle_scope");
		}
#else
		if (lifecycle) {
			entry["trigger_format"] = "native_d1";
			entry["source_flags"] = +t.flags;
			if (t.flags & TRIGGER_ONE_SHOT) reasons.push_back("one_shot_outside_repeatable_lifecycle_scope");
		}
#endif
		if (Num_fuelcenters < 0 || Num_fuelcenters > MAX_NUM_FUELCENS) reasons.push_back("invalid_station_count");
		for (int st = 0; st < Num_fuelcenters && st < MAX_NUM_FUELCENS; ++st) {
			if (Station[st].segnum < 0 || Station[st].segnum > Highest_segment_index) reasons.push_back("unsafe_station_loop_segment");
			if (Station[st].Type == SEGMENT_IS_ROBOTMAKER && Station[st].Enabled) reasons.push_back("unexpected_pre_enabled_station");
		}
		if (!Matcen_regression_capture.complete || Matcen_regression_capture.overflow) reasons.push_back("incomplete_or_overflowed_disk_capture");
		if (t.num_links <= 0 || t.num_links > MAX_WALLS_PER_LINK) reasons.push_back("invalid_or_empty_link_count");
		for (int link = 0; link < t.num_links && link < MAX_WALLS_PER_LINK; ++link) {
			const int seg = t.seg[link];
			json target = { { "ordinal", link }, { "segment", seg }, { "side", +t.side[link] } };
			std::string reason;
			int center = -1, station = -1;
			if (seg < 0 || seg > Highest_segment_index || seg >= MAX_SEGMENTS) reason = "invalid_segment";
			else if (t.side[link] < 0 || t.side[link] >= 6) reason = "invalid_side";
			else {
#ifdef DXX_BUILD_DESCENT_II
				const auto &s = Segment2s[seg];
#else
				const auto &s = Segments[seg];
#endif
				center = s.matcen_num;
				if (s.special != SEGMENT_IS_ROBOTMAKER) reason = "not_robotmaker_segment";
				else if (center < 0 || center >= MAX_ROBOT_CENTERS) reason = "physical_out_of_bounds";
				else if (center >= Num_robot_centers) reason = "runtime_count_unbound";
				else if (center >= Matcen_regression_capture.disk_count) reason = "logical_disk_unbound";
				else {
					const auto &r = RobotCenters[center];
					station = r.fuelcen_num;
					if (station < 0 || station >= Num_fuelcenters || station >= MAX_NUM_FUELCENS || station >= 128) reason = "invalid_station";
					else if (Station[station].Type != SEGMENT_IS_ROBOTMAKER) reason = "wrong_station_type";
					else if (Station[station].segnum != seg || s.value != station) reason = "station_segment_binding_mismatch";
					else if (ConsoleObject->segnum == seg) reason = "player_obstructs_target";
					else {
						bool nonempty = false;
						for (unsigned word = 0; word < sizeof(r.robot_flags) / sizeof(r.robot_flags[0]); ++word)
							for (unsigned bit = 0; bit < 32; ++bit)
								if (static_cast<uint32_t>(r.robot_flags[word]) & (1u << bit)) {
									nonempty = true;
									const unsigned type = word * 32 + bit;
									if (type >= static_cast<unsigned>(N_robot_types) || type >= MAX_ROBOT_TYPES) reason = "invalid_robot_mask";
									else if (Robot_info[type].model_num < 0 || Robot_info[type].model_num >= N_polygon_models) reason = "invalid_robot_model";
								}
						target["nonempty_mask"] = nonempty;
					}
				}
			}
			target["center"] = center;
			target["station"] = station;
			target["status"] = reason.empty() ? "eligible" : reason;
			if (!reason.empty()) reasons.push_back({ { "link", link }, { "reason", reason } });
			else if (std::find(centers.begin(), centers.end(), center) == centers.end()) centers.push_back(center);
			links.push_back(target);
		}
		entry["links"] = links;
		if (!reasons.empty()) {
			entry["status"] = "skipped";
			entry["skip_reasons"] = reasons;
			cases.push_back(entry);
			continue;
		}
		auto states = [&]() {
			uint8_t counts[MATCEN_MODE_MAX_CENTERS] = {};
			matcen_mode_get_activation_counts(counts, sizeof(counts));
			json result = json::array();
			for (const int c : centers) {
				const int st = RobotCenters[c].fuelcen_num;
				const auto &f = Station[st];
				result.push_back({ { "ordinal", c }, { "station", st }, { "segment", +f.segnum }, { "enabled", +f.Enabled }, { "lives", +f.Lives }, { "capacity", +f.Capacity }, { "timer", +f.Timer }, { "flag", +f.Flag }, { "disable_time", +f.Disable_time }, { "activation_count", counts[c] } });
			}
			return result;
		};
		json cleared = json::array();
		for (int obj = 0; obj <= Highest_object_index; ++obj)
			if (Objects[obj].type == OBJ_ROBOT)
				for (const int c : centers)
					if (Objects[obj].segnum == Station[RobotCenters[c].fuelcen_num].segnum) {
						cleared.push_back({ { "object", obj }, { "type", +Objects[obj].id }, { "signature", +Objects[obj].signature }, { "segment", +Objects[obj].segnum } });
						obj_delete(obj);
						break;
					}
		std::vector<int> existing;
		for (int obj = 0; obj <= Highest_object_index; ++obj)
			if (Objects[obj].type != OBJ_NONE) existing.push_back(Objects[obj].signature);
		const int initial_total = Players[0].num_robots_level, initial_kills = Players[0].num_kills_level;
		if (lifecycle) {
			// All attempts share one seeded native sequence and its untouched counters
			// One-shot triggers are excluded; native D1 also needs the outer crossing path
			auto vector = [](const vms_vector &v) { return json::array({ v.x, v.y, v.z }); };
			auto trigger_state = [&]() {
				json targets = json::array();
				for (int link = 0; link < t.num_links; ++link)
					targets.push_back({ { "ordinal", link }, { "segment", +t.seg[link] }, { "side", +t.side[link] } });
				return json{ { "type", +t.type }, { "flags", +t.flags }, { "num_links", +t.num_links }, { "links", targets } };
			};
			auto snapshot = [&]() {
				unsigned rng = 0;
				require(d_rand_get_state(&rng), "observe native lifecycle SIM state");
				return json{ { "centers", states() }, { "trigger", trigger_state() }, { "game_time", GameTime64 }, { "sim_state", rng }, { "sim_calls", d_rand_get_call_count() }, { "robot_total", +Players[0].num_robots_level }, { "robot_kills", +Players[0].num_kills_level }, { "robot_limit", Gamesave_num_org_robots + Num_extry_robots }, { "player", { { "number", Player_num }, { "object", +Players[0].objnum }, { "segment", +ConsoleObject->segnum }, { "position", vector(ConsoleObject->pos) }, { "forward", vector(ConsoleObject->orient.fvec) }, { "right", vector(ConsoleObject->orient.rvec) }, { "up", vector(ConsoleObject->orient.uvec) } } } };
			};
			auto lights = [&]() {
				std::vector<std::pair<int, int>> sorted;
				for (int obj = 0; obj <= Highest_object_index; ++obj)
					if (Objects[obj].type == OBJ_LIGHT) sorted.emplace_back(+Objects[obj].signature, obj);
				std::sort(sorted.begin(), sorted.end());
				json result = json::array();
				for (const auto &item : sorted) {
					const auto &light = Objects[item.second];
					result.push_back({ { "signature", item.first }, { "object", item.second }, { "segment", +light.segnum }, { "position", vector(light.pos) } });
				}
				return result;
			};
			bool unlimited_lives = false;
#ifdef DXX_BUILD_DESCENT_II
			unlimited_lives = difficulty + 1 == NDL;
#endif
			entry["initial"] = snapshot();
			for (const auto &state : entry["initial"]["centers"])
				require(state["enabled"] == 0 && state["lives"] == 3 && state["activation_count"] == 0, "fresh lifecycle starts with three native lives and no activations");
			json rounds = json::array();
			int total_births = 0;
			for (int attempt = 1; attempt <= 4; ++attempt) {
				const bool expect_activation = dispatch && (unlimited_lives || attempt <= 3);
				json round = { { "attempt", attempt }, { "frame_offset", (attempt - 1) * 2400 }, { "before_dispatch", snapshot() } };
				const json before_lights = lights();
				for (const auto &state : round["before_dispatch"]["centers"])
					require(state["enabled"] == 0, "previous lifecycle attempt completed before another dispatch");
				round["dispatch_result"] = dispatch ? check_trigger_sub(index, Player_num, 0) : -1;
				round["after_dispatch"] = snapshot();
				for (size_t c = 0; c < centers.size(); ++c) {
					const auto &before = round["before_dispatch"]["centers"][c], &after = round["after_dispatch"]["centers"][c];
					require(after["activation_count"].get<int>() == before["activation_count"].get<int>() + expect_activation, "ordered observed links activate each unique target once while lives permit");
					require(after["lives"].get<int>() == before["lives"].get<int>() - (expect_activation && !unlimited_lives), "native lifecycle preserves D2 highest-difficulty life exception");
					require(after["enabled"].get<int>() == expect_activation, "native lifecycle activation or exhaustion matches available lives");
				}
				const json after_lights = lights();
				json activation_lights = json::array();
				for (const auto &light : after_lights)
					if (std::none_of(before_lights.begin(), before_lights.end(), [&](const json &old) { return old["signature"] == light["signature"]; })) activation_lights.push_back(light);
				require(activation_lights.size() == (expect_activation ? centers.size() : 0), "one native light per activated unique target");
				for (size_t c = 0; c < activation_lights.size(); ++c)
					require(activation_lights[c]["segment"] == Station[RobotCenters[centers[c]].fuelcen_num].segnum, "lifecycle activation lights retain observed link order");
				round["activation_lights"] = activation_lights;
				if (attempt == 1) {
					round["active_retrigger_result"] = dispatch ? check_trigger_sub(index, Player_num, 0) : -1;
					round["after_active_retrigger"] = snapshot();
					require(round["after_active_retrigger"] == round["after_dispatch"] && lights() == after_lights, "immediate active retrigger changes no native station state, counters, RNG, or lights");
				}
				json births = json::array();
				int first_cap_frame = -1, first_all_disabled_frame = -1;
				const char *first_cap_phase = "not_observed";
				auto observe_cap = [&](int frame, const char *phase) {
					if (first_cap_frame < 0 && Players[0].num_robots_level - Players[0].num_kills_level >= Gamesave_num_org_robots + Num_extry_robots) {
						first_cap_frame = frame;
						first_cap_phase = phase;
					}
				};
				for (int frame = 0; frame < 2400; ++frame) {
					GameTime64 += FrameTime;
					observe_cap(frame, "before_station_update");
					fuelcen_update_all();
					observe_cap(frame, "after_station_update");
					for (int obj = 0; obj <= Highest_object_index; ++obj) {
						const auto &robot = Objects[obj];
						if (robot.type != OBJ_ROBOT || std::find(existing.begin(), existing.end(), robot.signature) != existing.end()) continue;
						require(static_cast<ubyte>(robot.matcen_creator) & 0x80, "lifecycle birth has station creator");
						const int station = static_cast<ubyte>(robot.matcen_creator) & 0x7f;
						int center = -1;
						for (const int c : centers)
							if (RobotCenters[c].fuelcen_num == station) center = c;
						require(center >= 0 && robot.id < N_robot_types && robot.id < MAX_ROBOT_TYPES, "lifecycle birth belongs to a valid eligible target");
						require(static_cast<uint32_t>(RobotCenters[center].robot_flags[robot.id / 32]) & (1u << (robot.id % 32)), "lifecycle birth belongs to exact mapped mask");
						require(robot.segnum == Station[station].segnum, "lifecycle birth retains station segment");
						births.push_back({ { "frame", frame }, { "absolute_frame", (attempt - 1) * 2400 + frame }, { "spawn_ordinal", total_births++ }, { "ordinal", center }, { "station", station }, { "segment", +robot.segnum }, { "position", vector(robot.pos) }, { "type", +robot.id }, { "signature", +robot.signature }, { "object", obj }, { "shields", +robot.shields } });
						obj_delete(obj);
					}
					if (expect_activation && first_all_disabled_frame < 0 && std::all_of(centers.begin(), centers.end(), [](int c) { return !Station[RobotCenters[c].fuelcen_num].Enabled; })) first_all_disabled_frame = frame;
				}
				json outcomes = json::array();
				for (const int c : centers) {
					bool nonempty = false;
					for (unsigned word = 0; word < sizeof(RobotCenters[c].robot_flags) / sizeof(RobotCenters[c].robot_flags[0]); ++word) nonempty = nonempty || RobotCenters[c].robot_flags[word];
					int produced = 0;
					for (const auto &birth : births)
						if (birth["ordinal"] == c) ++produced;
					require(expect_activation && nonempty ? (produced > 0 || first_cap_frame >= 0) : produced == 0, "lifecycle production is real, empty, exhausted, controlled, or cap-inconclusive");
					outcomes.push_back({ { "ordinal", c }, { "births", produced }, { "status", !dispatch ? "no_dispatch_control" : (!expect_activation ? "native_lives_exhausted" : (!nonempty ? "empty_mask" : (produced ? "produced" : "inconclusive_zero_births_with_global_cap"))) } });
				}
				round["births"] = births;
				round["production_outcomes"] = outcomes;
				round["global_cap_observed"] = first_cap_frame >= 0;
				round["first_cap_frame"] = first_cap_frame;
				round["first_cap_phase"] = first_cap_phase;
				round["first_all_disabled_frame"] = first_all_disabled_frame;
				round["after_frames"] = snapshot();
				require(!expect_activation || first_all_disabled_frame >= 0, "native activation completes within forty seconds without forced reset");
				require(trigger_state() == entry["initial"]["trigger"], "lifecycle preserves observed trigger flags, target order, and duplicates");
				require(Players[0].num_robots_level == initial_total + total_births && Players[0].num_kills_level == initial_kills, "lifecycle retirement neither resets the cap nor fabricates kills");
				rounds.push_back(round);
			}
			entry["status"] = "executed";
			entry["cleared_authored_robots"] = cleared;
			entry["unlimited_native_lives"] = unlimited_lives;
			entry["rounds"] = rounds;
			entry["birth_count"] = total_births;
			++lifecycle_executed;
			lifecycle_births += total_births;
			cases.push_back(entry);
			continue;
		}
		entry["before_dispatch"] = states();
		entry["dispatch_result"] = dispatch ? check_trigger_sub(index, Player_num, 0) : -1;
		entry["after_dispatch"] = states();
		for (size_t c = 0; c < centers.size(); ++c) {
			const auto &before = entry["before_dispatch"][c], &after = entry["after_dispatch"][c];
			require(after["activation_count"].get<int>() == (dispatch ? 1 : 0), "original ordered links activate each bound ordinal once");
			require(after["lives"].get<int>() == before["lives"].get<int>() - (dispatch ? 1 : 0), "difficulty two activation consumes exactly one native life");
		}
		std::vector<std::pair<int, int>> lights;
		for (int obj = 0; obj <= Highest_object_index; ++obj)
			if (Objects[obj].type == OBJ_LIGHT && std::find(existing.begin(), existing.end(), Objects[obj].signature) == existing.end()) lights.emplace_back(+Objects[obj].signature, +Objects[obj].segnum);
		std::sort(lights.begin(), lights.end());
		require(lights.size() == (dispatch ? centers.size() : 0), "authored trigger creates one activation light per unique target");
		for (size_t c = 0; c < lights.size(); ++c) require(lights[c].second == Station[RobotCenters[centers[c]].fuelcen_num].segnum, "activation lights retain authored target order");
		json activations = json::array();
		for (const auto &l : lights) activations.push_back({ { "signature", l.first }, { "segment", l.second } });
		json births = json::array();
		bool global_cap_observed = false;
		for (int frame = 0; frame < 2400; ++frame) {
			GameTime64 += FrameTime;
			global_cap_observed = global_cap_observed || (Players[0].num_robots_level - Players[0].num_kills_level >= Gamesave_num_org_robots + Num_extry_robots);
			fuelcen_update_all();
			for (int obj = 0; obj <= Highest_object_index; ++obj) {
				const auto &robot = Objects[obj];
				if (robot.type != OBJ_ROBOT || std::find(existing.begin(), existing.end(), robot.signature) != existing.end()) continue;
				require(static_cast<ubyte>(robot.matcen_creator) & 0x80, "authored birth has station creator");
				const int station = static_cast<ubyte>(robot.matcen_creator) & 0x7f;
				int center = -1;
				for (const int c : centers)
					if (RobotCenters[c].fuelcen_num == station) center = c;
				require(center >= 0, "authored birth belongs to eligible target");
				require(robot.id < N_robot_types && robot.id < MAX_ROBOT_TYPES, "authored birth type is valid");
				require(RobotCenters[center].robot_flags[robot.id / 32] & (1u << (robot.id % 32)), "authored birth belongs to exact mapped mask");
				require(robot.segnum == Station[station].segnum, "authored birth retains station segment");
				births.push_back({ { "frame", frame }, { "spawn_ordinal", births.size() }, { "ordinal", center }, { "station", station }, { "segment", +robot.segnum }, { "type", +robot.id }, { "signature", +robot.signature }, { "object", obj }, { "shields", +robot.shields } });
				obj_delete(obj);
			}
		}
		json production_outcomes = json::array();
		for (const int c : centers) {
			bool nonempty = false;
			for (unsigned word = 0; word < sizeof(RobotCenters[c].robot_flags) / sizeof(RobotCenters[c].robot_flags[0]); ++word) nonempty = nonempty || RobotCenters[c].robot_flags[word];
			int produced = 0;
			for (const auto &b : births)
				if (b["ordinal"] == c) ++produced;
			require(dispatch && nonempty ? (produced > 0 || global_cap_observed) : produced == 0, "eligible original trigger produces real births or observed native cap; empty-mask/control produces none");
			production_outcomes.push_back({ { "ordinal", c }, { "births", produced }, { "status", !dispatch ? "no_dispatch_control" : (!nonempty ? "empty_mask" : (produced ? "produced" : "native_global_cap")) } });
		}
		require(Players[0].num_robots_level == initial_total + static_cast<int>(births.size()) && Players[0].num_kills_level == initial_kills, "retirement does not fabricate kills");
		entry["status"] = "executed";
		entry["cleared_authored_robots"] = cleared;
		entry["activation_lights"] = activations;
		entry["births"] = births;
		entry["production_outcomes"] = production_outcomes;
		entry["global_cap_observed"] = global_cap_observed;
		entry["after_frames"] = states();
		cases.push_back(entry);
	}
	require(!SDL_WasInit(SDL_INIT_VIDEO), "authored station fixture never initializes video");
	if (lifecycle) {
		const json report = { { "schema", 2 }, { "scope", "controlled_authored_trigger_station_lifecycle" }, { "game", game_name }, { "mission", mission }, { "level", level }, { "difficulty", difficulty }, { "rng_seed", 1234 }, { "frame_time", F1_0 / 60 }, { "frames_per_attempt", 2400 }, { "attempts", 4 }, { "dispatch_requested", dispatch }, { "observed_runtime_links_preserved", true }, { "disk_original_links_verified", false }, { "fixture_operations", { "clear target-segment authored robots once", "retire each generated robot immediately without combat or kill-count changes", "advance only station loop; retain activation lights and explosions without object-lifetime processing", "seed RNG once per fresh trigger case; preserve native counters across all four attempts" } }, { "baseline", baseline }, { "cases", cases }, { "execution_summary", { { "matcen_triggers", cases.size() }, { "executed", lifecycle_executed }, { "skipped", cases.size() - lifecycle_executed }, { "births", lifecycle_births }, { "status", cases.empty() ? "no_matcen_triggers" : (!lifecycle_executed ? "all_cases_skipped" : "native_invariants_checked") } } }, { "comparison_performed", false }, { "passed", lifecycle_executed > 0 } };
		const auto text = report.dump(2) + "\n";
		write_fixture(output, bytes(text.begin(), text.end()));
		std::puts(lifecycle_executed ? "PASS: controlled native authored matcen lifecycle invariants" : "SKIP: no eligible authored matcen lifecycle cases");
		return;
	}
	const json report = { { "schema", 1 }, { "scope", "controlled_authored_trigger_station_loop" }, { "game", game_name }, { "mission", mission }, { "level", level }, { "difficulty", 2 }, { "rng_seed", 1234 }, { "frame_time", F1_0 / 60 }, { "frames", 2400 }, { "original_links_preserved", true }, { "fixture_operations", { "clear target-segment authored robots", "retire each generated robot immediately without combat", "advance only station loop" } }, { "baseline", baseline }, { "cases", cases }, { "passed", true } };
	const auto text = report.dump(2) + "\n";
	write_fixture(output, bytes(text.begin(), text.end()));
	std::puts("PASS: safe original authored matcen trigger probe");
}
