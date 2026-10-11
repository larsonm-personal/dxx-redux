// Fresh public level starts: only ordinary controls may alter the world
extern "C" {
void do_weapon_n_item_stuff(void);
void calc_game_time(void);
void GameProcessFrame(void);
void newdemo_write_end(void);
extern PHYSFS_file *outfile;
int input_demo_configure_startup_fp_environment(char *, size_t);
}

static void capture_public_level_replay(const char *directory, const char *scenario)
{
	using nlohmann::json;
#ifdef DXX_BUILD_DESCENT_II
	const char *game_name = "d2";
	const char *archive = "/d2demo.hog";
	char mission[] = "d2demo";
#else
	const char *game_name = "d1";
	const char *archive = "/descent.hog";
	char mission[] = "";
#endif
	const std::string hog = std::string(directory) + archive;
	require(PHYSFS_mount(directory, nullptr, 1) && PHYSFS_mount(hog.c_str(), nullptr, 1), "mount pinned public resources");
	char error[256] = "";
	require(input_demo_configure_startup_fp_environment(error, sizeof(error)), error);
	Game_mode = 0;
	GameArg.SndNoSound = GameArg.SndNoMusic = 1;
	GameArg.SysWindow = GameCfg.WindowMode = 1;
	GameCfg.AspectX = 4;
	GameCfg.AspectY = 3;
	Game_screen_mode = SM(640, 480);
	digi_select_system(SDLAUDIO_SYSTEM);
	load_text();
	require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0 && gr_init(Game_screen_mode) == 0, "initialize capture window");
	key_init();
	mouse_init();
	char palette[] = "palette.256";
	gr_use_palette_table(palette);
	gamefont_init();
	gamedata_init();
	texmerge_init(10);
	init_game();
	new_player_config();
#ifdef DXX_BUILD_DESCENT_II
	guidebot_routing_set_default(GUIDEBOT_ROUTING_ORIGINAL);
	guidebot_routing_set_mode(GUIDEBOT_ROUTING_ORIGINAL);
#endif
	require(load_mission_by_name(mission), "select real public level");
	Player_num = 0;
	N_players = 1;
	std::strcpy(Players[0].callsign, "public");
	Difficulty_level = 2;
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	if (!Game_wind) ::game();
	GameArg.SysInputDemoNoRender = 1;
	newdemo_start_recording(0);
	require(Newdemo_state == ND_STATE_RECORDING && input_demo_recorder_is_active(), "initialize production record callbacks");
	input_demo_recorder_cancel();
	input_demo_recorder_settings settings;
	input_demo_recorder_settings_clear(&settings);
#ifdef DXX_BUILD_DESCENT_II
	settings.game = INPUT_DEMO_GAME_D2;
	settings.player_cfg.guidebot_routing_mode = GUIDEBOT_ROUTING_ORIGINAL;
	settings.mission = "d2demo";
	settings.player_cfg.has_headlight_active_default = 1;
	settings.player_cfg.headlight_active_default = PlayerCfg.HeadlightActiveDefault;
#else
	settings.game = INPUT_DEMO_GAME_D1;
	settings.mission = "d1";
#endif
	settings.level = 1;
	settings.difficulty = Difficulty_level;
	settings.rng_mode = "lcg_state";
	settings.record_per_frame_state = 1;
	settings.has_player_cfg = 1;
	settings.player_cfg.auto_leveling = PlayerCfg.AutoLeveling;
	settings.player_cfg.persistent_debris = PlayerCfg.PersistentDebris;
	settings.player_cfg.no_fire_autoselect = PlayerCfg.NoFireAutoselect;
	settings.player_cfg.cycle_autoselect_only = PlayerCfg.CycleAutoselectOnly;
	settings.player_cfg.select_after_fire = PlayerCfg.SelectAfterFire;
	settings.player_cfg.classic_autoselect_weapon = PlayerCfg.ClassicAutoselectWeapon;
	settings.player_cfg.autoselect_only_once = PlayerCfg.AutoselectOnlyOnce;
	settings.player_cfg.original_homing = PlayerCfg.OriginalHoming;
	settings.player_cfg.primary_order_count = sizeof(PlayerCfg.PrimaryOrder);
	settings.player_cfg.secondary_order_count = sizeof(PlayerCfg.SecondaryOrder);
	std::memcpy(settings.player_cfg.primary_order, PlayerCfg.PrimaryOrder, sizeof(PlayerCfg.PrimaryOrder));
	std::memcpy(settings.player_cfg.secondary_order, PlayerCfg.SecondaryOrder, sizeof(PlayerCfg.SecondaryOrder));
	d_srand(1234);
	d_rand_reset_call_count();
	require(input_demo_recorder_start(&settings, error, sizeof(error)), error);
	const bool flight = std::strcmp(scenario, "flight") == 0;
	const bool pickup = std::strcmp(scenario, "pickup") == 0;
	require(flight || pickup || std::strcmp(scenario, "door") == 0, "known public scenario");
	const int frame_count = flight ? 360 : 600;
	require(input_demo_state_trace_start("capture-observation.jsonl", "record", game_name, settings.mission, 1, Difficulty_level, "new_level", frame_count, error, sizeof(error)), error);
	const auto position = [](const vms_vector &p) { return json::array({ p.x, p.y, p.z }); };
	const auto start = ConsoleObject->pos;
	json observations = json::array();
	std::vector<vms_vector> waypoints;
	int target = -1;
	if (!flight) {
		std::vector<int> parent(Highest_segment_index + 1, -1), depth(parent.size(), -1), queue;
		const int origin = ConsoleObject->segnum;
		depth[origin] = 0;
		queue.push_back(origin);
		for (size_t q = 0; q < queue.size(); ++q) {
			const int seg = queue[q];
			for (int side = 0; side < 6; ++side) {
				const int child = Segments[seg].children[side], wall = Segments[seg].sides[side].wall_num;
				if (child < 0 || depth[child] >= 0) continue;
				if (wall >= 0 && !(Walls[wall].type == WALL_OPEN || Walls[wall].type == WALL_ILLUSION || (Walls[wall].type == WALL_DOOR && !(Walls[wall].flags & WALL_DOOR_LOCKED) && Walls[wall].keys == KEY_NONE))) continue;
				parent[child] = seg;
				depth[child] = depth[seg] + 1;
				queue.push_back(child);
			}
		}
		int goal = -1, best = INT_MAX;
		vms_vector destination = {};
		if (pickup) {
			for (int i = 0; i <= Highest_object_index; ++i)
				if (Objects[i].type == OBJ_POWERUP && depth[Objects[i].segnum] >= 0 && depth[Objects[i].segnum] < best) {
					target = i;
					goal = Objects[i].segnum;
					best = depth[goal];
					destination = Objects[i].pos;
				}
		} else {
			for (int i = 0; i < Num_walls; ++i)
				if (Walls[i].type == WALL_DOOR && Walls[i].state == WALL_DOOR_CLOSED && !(Walls[i].flags & WALL_DOOR_LOCKED) && Walls[i].keys == KEY_NONE && depth[Walls[i].segnum] >= 0 && depth[Walls[i].segnum] < best) {
					target = i;
					goal = Walls[i].segnum;
					best = depth[goal];
					compute_center_point_on_side(&destination, &Segments[goal], Walls[i].sidenum);
				}
		}
		require(target >= 0, "reachable existing public action target");
		std::vector<int> path;
		for (int seg = goal; seg != origin; seg = parent[seg]) path.push_back(seg);
		std::reverse(path.begin(), path.end());
		int previous = origin;
		for (int seg : path) {
			for (int side = 0; side < 6; ++side)
				if (Segments[previous].children[side] == seg) {
					vms_vector point;
					compute_center_point_on_side(&point, &Segments[previous], side);
					waypoints.push_back(point);
				}
			vms_vector point;
			compute_segment_center(&point, &Segments[seg]);
			waypoints.push_back(point);
			previous = seg;
		}
		waypoints.push_back(destination);
		if (!pickup) {
			const int child = Segments[goal].children[Walls[target].sidenum];
			require(child >= 0, "door has a real neighboring segment");
			compute_segment_center(&destination, &Segments[child]);
			waypoints.push_back(destination);
		}
	}
	size_t waypoint = 0;
	const int target_signature = pickup ? Objects[target].signature : -1;
	const int target_id = pickup ? Objects[target].id : -1;
	int action_frame = -1;
	int contact_frames = 0;
	for (int frame = 0; frame < frame_count; ++frame) {
		FrameTime = F1_0 / (flight ? 60 : 30);
		Controls = {};
		if (flight) {
			if (frame < 300) Controls.forward_thrust_time = -FrameTime;
		} else {
			while (waypoint < waypoints.size() && vm_vec_dist(&ConsoleObject->pos, &waypoints[waypoint]) < 8 * F1_0) ++waypoint;
			if (waypoint < waypoints.size()) {
				vms_vector delta;
				vm_vec_sub(&delta, &waypoints[waypoint], &ConsoleObject->pos);
				const auto thrust = [&](const vms_vector &axis) {
					const fix error = vm_vec_dot(&delta, &axis) - vm_vec_dot(&ConsoleObject->mtype.phys_info.velocity, &axis) / (waypoint + 1 == waypoints.size() ? 2 : 8);
					return error > F1_0 ? FrameTime : error < -F1_0 ? -FrameTime
					                                                : 0;
				};
				Controls.forward_thrust_time = thrust(ConsoleObject->orient.fvec);
				Controls.sideways_thrust_time = thrust(ConsoleObject->orient.rvec);
				Controls.vertical_thrust_time = thrust(ConsoleObject->orient.uvec);
			}
		}
		if (!pickup && frame < 180) Controls.fire_primary_state = 1;
		const auto previous_position = ConsoleObject->pos;
		do_weapon_n_item_stuff();
		calc_game_time();
		GameProcessFrame();
		require(render_update_main_view_objects(), "capture gameplay view state");
		if (flight && frame > 30 && frame < 300 && vm_vec_dist(&previous_position, &ConsoleObject->pos) < F1_0 / 100) ++contact_frames;
		if (!flight && action_frame < 0 && (pickup ? Objects[target].type != OBJ_POWERUP || Objects[target].signature != target_signature : Walls[target].state == WALL_DOOR_OPENING || Walls[target].state == WALL_DOOR_WAITING)) action_frame = frame;
		json doors = json::array();
		for (int i = 0; i < Num_walls; ++i)
			if (Walls[i].type == WALL_DOOR) doors.push_back({ i, Walls[i].state });
		observations.push_back({ { "f", frame }, { "position", position(ConsoleObject->pos) }, { "segment", int(ConsoleObject->segnum) }, { "energy", int(Players[0].energy) }, { "shields", int(Players[0].shields) }, { "doors", doors } });
		if (action_frame >= 0 && frame >= action_frame + (pickup ? 30 : 60)) break;
	}
	require(flight || action_frame >= 0, "ordinary controls reached a real pickup or opened the selected door");
	input_demo_result result;
	input_demo_capture_current_result(&result);
	require(input_demo_recorder_flush_with_result("capture.dximdemo", &result, error, sizeof(error)), error);
	input_demo_state_trace_stop();
	newdemo_write_end();
	require(PHYSFS_close(outfile), "close classic callback sink");
	outfile = nullptr;
	Newdemo_state = ND_STATE_NORMAL;
	json report = { { "game", game_name }, { "scenario", scenario }, { "start", position(start) }, { "target", target }, { "target_id", target_id }, { "target_signature", target_signature }, { "action_frame", action_frame }, { "wall_contact_frames", contact_frames }, { "waypoints_reached", waypoint }, { "waypoints_total", waypoints.size() }, { "observations", observations } };
	const auto text = report.dump(2) + "\n";
	write_fixture("capture.json", bytes(text.begin(), text.end()));
	std::puts("PASS: public level-start capture");
}
