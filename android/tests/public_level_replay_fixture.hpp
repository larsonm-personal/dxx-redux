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
	const bool screenshots = std::getenv("DXX_PUBLIC_CAPTURE_SCREENSHOTS") != nullptr;
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
	const int sdl_status = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	if (sdl_status) std::fprintf(stderr, "SDL initialization: %s\n", SDL_GetError());
	require(sdl_status == 0 && gr_init(Game_screen_mode) == 0, "initialize capture window");
	if (screenshots) GameArg.DbgGlReadPixelsOk = 1;
	key_init();
	mouse_init();
	char palette[] = "palette.256";
	gr_use_palette_table(palette);
	gamefont_init();
	gamedata_init();
	texmerge_init(10);
#ifdef DXX_BUILD_DESCENT_II
	// Match desktop startup: level ambience depends on bitmap transparency flags
	d1_in_d2_init_startup_bitmaps();
#endif
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
	const bool combat = std::strcmp(scenario, "combat") == 0;
	const bool shoot_switch = std::strcmp(scenario, "shoot-switch") == 0;
#ifdef DXX_BUILD_DESCENT_II
	const int level = shoot_switch ? 2 : 1;
#else
	require(!shoot_switch, "shoot-switch capture requires D2");
	const int level = 1;
#endif
	StartNewGame(level);
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
	settings.level = level;
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
	require(flight || pickup || shoot_switch || combat || std::strcmp(scenario, "door") == 0, "known public scenario");
	const int frame_count = flight ? 360 : 600;
	require(input_demo_state_trace_start("capture-observation.jsonl", "record", game_name, settings.mission, level, Difficulty_level, "new_level", frame_count, error, sizeof(error)), error);
	const auto position = [](const vms_vector &p) { return json::array({ p.x, p.y, p.z }); };
	const auto start = ConsoleObject->pos;
	json observations = json::array();
	json screenshot_frames = json::array();
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
		if (shoot_switch) {
#ifdef DXX_BUILD_DESCENT_II
			require(Num_walls > 154 && Num_triggers > 17 && ConsoleObject->segnum == 217, "expected normal public level-two spawn and authored switch");
			require(Walls[154].segnum == 84 && Walls[154].sidenum == 2 && Walls[154].trigger == 17, "expected authored shoot-switch wall");
			require((Walls[116].flags & WALL_DOOR_LOCKED) && !(Walls[117].flags & WALL_DOOR_LOCKED) && Walls[116].state == WALL_DOOR_CLOSED && Walls[117].state == WALL_DOOR_CLOSED, "authored forward passage locked and both sides closed");
			target = 154;
			goal = 84;
			require(Segments[85].children[4] == 235 && Segments[235].children[5] == 85, "authored doors are the two sides of one passage");
			require(depth[goal] >= 0, "shoot switch reachable without locked doors");
			require(Segment2s[217].s2_flags & S2F_AMBIENT_LAVA, "ordinary level startup propagated authored lava ambience");
			compute_segment_center(&destination, &Segments[goal]);
#endif
		} else if (combat) {
			for (int i = 0; i <= Highest_object_index; ++i)
				if (Objects[i].type == OBJ_ROBOT && depth[Objects[i].segnum] >= 0 && depth[Objects[i].segnum] < best) {
					target = i;
					goal = Objects[i].segnum;
					best = depth[goal];
					destination = Objects[i].pos;
				}
			require(target >= 0, "reachable authentic public combat robot");
			std::printf("COMBAT target=%d id=%d segment=%d depth=%d shields=%d score=%d contains=%d/%d/%d definition=%d/%d/%d/%d\n", target, Objects[target].id, goal, best, Objects[target].shields, Robot_info[Objects[target].id].score_value, Objects[target].contains_type, Objects[target].contains_id, Objects[target].contains_count, Robot_info[Objects[target].id].contains_type, Robot_info[Objects[target].id].contains_id, Robot_info[Objects[target].id].contains_count, Robot_info[Objects[target].id].contains_prob);
		} else if (pickup) {
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
		if (!pickup && !shoot_switch && !combat) {
			const int child = Segments[goal].children[Walls[target].sidenum];
			require(child >= 0, "door has a real neighboring segment");
			compute_segment_center(&destination, &Segments[child]);
			waypoints.push_back(destination);
		}
	}
	size_t waypoint = 0;
	const int target_signature = (pickup || combat) ? Objects[target].signature : -1;
	const int target_id = (pickup || combat) ? Objects[target].id : -1;
	const int target_shields = combat ? Objects[target].shields : 0;
	const int target_score = combat ? Robot_info[target_id].score_value : 0;
	int damage_frame = -1;
	int retirement_frame = -1;
	int action_frame = -1;
	int contact_frames = 0;
	int passage_frame = -1;
	vms_vector switch_center = {};
	if (shoot_switch) compute_center_point_on_side(&switch_center, &Segments[84], 2);
	const auto append_route = [&](int goal) {
		std::vector<int> parent(Highest_segment_index + 1, -1), queue{ ConsoleObject->segnum };
		const int origin = ConsoleObject->segnum;
		parent[origin] = origin;
		for (size_t q = 0; q < queue.size() && parent[goal] < 0; ++q)
			for (int side = 0; side < 6; ++side) {
				const int seg = queue[q], child = Segments[seg].children[side], wall = Segments[seg].sides[side].wall_num;
				if (child < 0 || parent[child] >= 0) continue;
				if (wall >= 0 && !(Walls[wall].type == WALL_OPEN || Walls[wall].type == WALL_ILLUSION || (Walls[wall].type == WALL_DOOR && !(Walls[wall].flags & WALL_DOOR_LOCKED) && Walls[wall].keys == KEY_NONE))) continue;
				parent[child] = seg;
				queue.push_back(child);
			}
		require(parent[goal] >= 0, "unlocked passage reachable with ordinary controls");
		std::vector<int> path;
		for (int seg = goal; seg != origin; seg = parent[seg]) path.push_back(seg);
		std::reverse(path.begin(), path.end());
		waypoints.clear();
		waypoint = 0;
		int previous = origin;
		for (int seg : path) {
			vms_vector point;
			for (int side = 0; side < 6; ++side)
				if (Segments[previous].children[side] == seg) {
					compute_center_point_on_side(&point, &Segments[previous], side);
					waypoints.push_back(point);
				}
			compute_segment_center(&point, &Segments[seg]);
			waypoints.push_back(point);
			previous = seg;
		}
	};
	for (int frame = 0; frame < frame_count; ++frame) {
		FrameTime = F1_0 / (flight ? 60 : 30);
		Controls = {};
		if (flight) {
			if (frame < 300) Controls.forward_thrust_time = -FrameTime;
		} else {
			while (waypoint < waypoints.size() && vm_vec_dist(&ConsoleObject->pos, &waypoints[waypoint]) < (shoot_switch && action_frame >= 0 && waypoint + 1 == waypoints.size() ? F1_0 : 8 * F1_0)) ++waypoint;
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
		if (!pickup && !shoot_switch && !combat && frame < 180) Controls.fire_primary_state = 1;
		if (shoot_switch && action_frame < 0 && ConsoleObject->segnum == 84) {
			vms_vector direction;
			vm_vec_normalized_dir(&direction, &switch_center, &ConsoleObject->pos);
			const fix horizontal = vm_vec_dot(&direction, &ConsoleObject->orient.rvec);
			const fix vertical = vm_vec_dot(&direction, &ConsoleObject->orient.uvec);
			const auto turn = [&](fix error) { return error > F1_0 / 64 ? FrameTime : error < -F1_0 / 64 ? -FrameTime
				                                                                                         : 0; };
			Controls.heading_time = turn(horizontal - ConsoleObject->mtype.phys_info.rotvel.y * 2);
			Controls.pitch_time = turn(-vertical - ConsoleObject->mtype.phys_info.rotvel.x * 2);
			if (vm_vec_dot(&direction, &ConsoleObject->orient.fvec) > F1_0 * 99 / 100) Controls.fire_primary_state = 1;
		}

		if (combat && action_frame < 0) {
			auto &robot = Objects[target];
			if (vm_vec_dist(&robot.pos, &ConsoleObject->pos) < 120 * F1_0 && object_to_object_visibility(ConsoleObject, &robot, 0)) {
				vms_vector direction;
				vm_vec_normalized_dir(&direction, &robot.pos, &ConsoleObject->pos);
				vms_vector delta;
				vm_vec_sub(&delta, &robot.pos, &ConsoleObject->pos);
				vm_vec_scale_add2(&delta, &direction, -50 * F1_0);
				const auto hold = [&](const vms_vector &axis) {
					const fix error = vm_vec_dot(&delta, &axis) - vm_vec_dot(&ConsoleObject->mtype.phys_info.velocity, &axis) / 2;
					return error > F1_0 ? FrameTime : error < -F1_0 ? -FrameTime : 0;
				};
				Controls.forward_thrust_time = hold(ConsoleObject->orient.fvec);
				Controls.sideways_thrust_time = hold(ConsoleObject->orient.rvec);
				Controls.vertical_thrust_time = hold(ConsoleObject->orient.uvec);
				const fix horizontal = vm_vec_dot(&direction, &ConsoleObject->orient.rvec);
				const fix vertical = vm_vec_dot(&direction, &ConsoleObject->orient.uvec);
				const auto turn = [&](fix error) { return error > F1_0 / 64 ? FrameTime : error < -F1_0 / 64 ? -FrameTime : 0; };
				Controls.heading_time = turn(horizontal - ConsoleObject->mtype.phys_info.rotvel.y * 2);
				Controls.pitch_time = turn(-vertical - ConsoleObject->mtype.phys_info.rotvel.x * 2);
				if (vm_vec_dot(&direction, &ConsoleObject->orient.fvec) > F1_0 * 98 / 100) Controls.fire_primary_state = 1;
			}
		}
		if (combat && action_frame >= 0) Controls = {};
		const auto previous_position = ConsoleObject->pos;
		do_weapon_n_item_stuff();
		calc_game_time();
		GameProcessFrame();
		require(render_update_main_view_objects(), "capture gameplay view state");
		if (flight && frame > 30 && frame < 300 && vm_vec_dist(&previous_position, &ConsoleObject->pos) < F1_0 / 100) ++contact_frames;
		if (!flight && !shoot_switch && !combat && action_frame < 0 && (pickup ? Objects[target].type != OBJ_POWERUP || Objects[target].signature != target_signature : Walls[target].state == WALL_DOOR_OPENING || Walls[target].state == WALL_DOOR_WAITING)) action_frame = frame;
		if (shoot_switch && action_frame < 0 && !(Walls[116].flags & WALL_DOOR_LOCKED) && !(Walls[117].flags & WALL_DOOR_LOCKED)) {
			action_frame = frame;
			append_route(Walls[116].segnum);
			vms_vector point;
			compute_center_point_on_side(&point, &Segments[Walls[116].segnum], Walls[116].sidenum);
			waypoints.push_back(point);
			compute_segment_center(&point, &Segments[Walls[117].segnum]);
			waypoints.push_back(point);
		}
		if (shoot_switch && action_frame >= 0 && ConsoleObject->segnum == Walls[117].segnum && passage_frame < 0) passage_frame = frame;

		if (combat) {
			if (frame % 30 == 0) std::printf("COMBAT frame=%d segment=%d target_segment=%d waypoints=%zu/%zu shields=%d score=%d fire=%d\n", frame, ConsoleObject->segnum, Objects[target].segnum, waypoint, waypoints.size(), Objects[target].shields, Players[0].score, Controls.fire_primary_state);
			if (damage_frame < 0 && Objects[target].shields < target_shields) damage_frame = frame;
			if (action_frame < 0 && Objects[target].shields < 0 && Players[0].score >= target_score) action_frame = frame;
			if (retirement_frame < 0 && (Objects[target].type != OBJ_ROBOT || Objects[target].signature != target_signature)) retirement_frame = frame;
			if (screenshots) {
				GameArg.SysInputDemoNoRender = 0;
				game_render_frame_mono(1);
				if (frame % 30 == 0 || frame == damage_frame || frame == action_frame) {
					screenshot_frames.push_back(frame);
					save_screen_shot(1);
				}
				GameArg.SysInputDemoNoRender = 1;
			}
		}
		json doors = json::array();
		for (int i = 0; i < Num_walls; ++i)
			if (Walls[i].type == WALL_DOOR) doors.push_back({ i, Walls[i].state });
		observations.push_back({ { "f", frame }, { "position", position(ConsoleObject->pos) }, { "segment", int(ConsoleObject->segnum) }, { "energy", int(Players[0].energy) }, { "shields", int(Players[0].shields) }, { "doors", doors }, { "target_shields", combat ? int(Objects[target].shields) : 0 }, { "score", int(Players[0].score) } });
		if (shoot_switch ? passage_frame >= 0 : combat ? retirement_frame >= 0 : action_frame >= 0 && frame >= action_frame + (pickup ? 30 : 60)) break;
	}
	require(!shoot_switch || ((Walls[116].flags & WALL_DOOR_OPENED) && (Walls[117].flags & WALL_DOOR_OPENED)), "both passage doors are open at the traversal boundary");
	require(!shoot_switch || passage_frame >= 0, "ordinary controls traversed unlocked target passage");
	require(flight || action_frame >= 0, "ordinary controls reached a real pickup or opened the selected door");
	input_demo_result result;
	input_demo_capture_current_result(&result);
	require(input_demo_recorder_flush_with_result("capture.dximdemo", &result, error, sizeof(error)), error);
	input_demo_state_trace_stop();
	newdemo_write_end();
	require(PHYSFS_close(outfile), "close classic callback sink");
	outfile = nullptr;
	Newdemo_state = ND_STATE_NORMAL;
	json report = { { "game", game_name }, { "scenario", scenario }, { "start", position(start) }, { "target", target }, { "target_id", target_id }, { "target_signature", target_signature }, { "action_frame", action_frame }, { "damage_frame", damage_frame }, { "retirement_frame", retirement_frame }, { "target_shields", target_shields }, { "target_score", target_score }, { "passage_frame", passage_frame }, { "wall_contact_frames", contact_frames }, { "waypoints_reached", waypoint }, { "waypoints_total", waypoints.size() }, { "observations", observations }, { "screenshot_frames", screenshot_frames } };
	const auto text = report.dump(2) + "\n";
	write_fixture("capture.json", bytes(text.begin(), text.end()));
	std::puts("PASS: public level-start capture");
}
