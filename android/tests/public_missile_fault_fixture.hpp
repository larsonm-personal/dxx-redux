// Use the engine's native preference writer, never duplicate binary PLR offsets
static void write_public_d1_comparison_profile()
{
#ifdef DXX_BUILD_DESCENT_II
	using nlohmann::json;
	Player_num = 0;
	Game_mode = 0;
	Newdemo_state = ND_STATE_NORMAL;
	GameArg.SysUsePlayersDir = 0;
	std::strcpy(Players[0].callsign, "d1match");
	new_player_config();
	require(PlayerCfg.MissileViewEnabled == 1, "normal D2 default missile camera remains enabled");
	require(write_player_file() == 0, "write ordinary default comparison pilot");
	const auto original_binary = read_save_fixture("d1match.plr");
	const auto original_text = read_save_fixture("d1match.plx");
	write_fixture("default.plr", original_binary);
	write_fixture("default.plx", original_text);
	PlayerCfg.MissileViewEnabled = 0;
	require(write_player_file() == 0, "write explicit missile-camera-disabled comparison pilot");
	const auto matched_binary = read_save_fixture("d1match.plr");
	const auto matched_text = read_save_fixture("d1match.plx");
	require(original_binary.size() == matched_binary.size() && original_text == matched_text, "comparison profile preserves layout and every text preference");
	json changed_bytes = json::array();
	for (size_t i = 0; i < original_binary.size(); ++i)
		if (original_binary[i] != matched_binary[i]) changed_bytes.push_back({ { "offset", i }, { "default", original_binary[i] }, { "comparison", matched_binary[i] } });
	require(changed_bytes.size() == 1, "native writer changes only the declared one-byte camera preference");
	new_player_config();
	require(PlayerCfg.MissileViewEnabled == 1 && read_player_file() == 0 && PlayerCfg.MissileViewEnabled == 0, "native reader restores explicit comparison preference over the normal default");
	const json report = { { "profile", "d1match" }, { "purpose", "matched D1 presentation prerequisite" }, { "default_missile_view", 1 }, { "comparison_missile_view", 0 }, { "binary_changes", changed_bytes }, { "text_profile_unchanged", true }, { "native_readback_verified", true } };
	const auto text = report.dump(2) + "\n";
	write_fixture("comparison-profile.json", bytes(text.begin(), text.end()));
	std::puts("PASS: explicit imported-D1 comparison pilot; production defaults unchanged");
#else
	require(false, "imported-D1 comparison profile requires the native D2 writer");
#endif
}

// Ordinary-start missile probes retain full live evidence even when parity fails
static void capture_public_missile_fault(const char *game_name, const char *mission, const char *scenario)
{
	using nlohmann::json;
	const bool mine_blast = std::strcmp(scenario, "mine-blast") == 0;
	const bool self_splash = mine_blast || std::strcmp(scenario, "missile-self-splash") == 0;
	require(self_splash || std::strcmp(scenario, "missile-robot") == 0, "known missile fault scenario");
#ifdef DXX_BUILD_DESCENT_II
	require(false, "record missile fault inputs with native D1 before imported playback");
#endif
	require(Players[0].secondary_weapon == 0 && Players[0].secondary_ammo[0] == 2 + NDL - Difficulty_level, "ordinary difficulty-dependent starting concussion inventory");
	const char *negative_setting = std::getenv("DXX_PUBLIC_CAPTURE_NO_MISSILE");
	const bool no_missile = negative_setting && std::strcmp(negative_setting, "1") == 0;
	const bool screenshots = std::getenv("DXX_PUBLIC_CAPTURE_SCREENSHOTS") != nullptr;
	const int target = self_splash ? -1 : 8;
	const int target_signature = self_splash ? -1 : Objects[target].signature;
	if (!self_splash) require(Objects[target].type == OBJ_ROBOT && Objects[target].id == 3 && target_signature == 9, "original ordinary-start robot target");
	const int original_segment = ConsoleObject->segnum;
	vms_vector aim = {}, hold = {};
	int wall_side = -1;
	std::vector<vms_vector> waypoints;
	const auto select_solid_side = [&]() {
		fix nearest = INT_MAX;
		for (int side = 0; side < 6; ++side) {
			if (Segments[ConsoleObject->segnum].children[side] >= 0) continue;
			vms_vector point;
			compute_center_point_on_side(&point, &Segments[ConsoleObject->segnum], side);
			const fix distance = vm_vec_dist(&point, &ConsoleObject->pos);
			if (distance < nearest) {
				nearest = distance;
				wall_side = side;
				aim = point;
			}
		}
		require(wall_side >= 0, "ordinary spawn segment has a solid side");
		vms_vector inward;
		vm_vec_normalized_dir(&inward, &ConsoleObject->pos, &aim);
		vm_vec_scale_add(&hold, &aim, &inward, ConsoleObject->size * 2 + 2 * F1_0);
	};
	int mine_pickup = -1;
	int mine_pickup_signature = -1;
	if (mine_blast) {
		std::vector<int> depth(Highest_segment_index + 1, -1), queue{ ConsoleObject->segnum };
		depth[ConsoleObject->segnum] = 0;
		for (size_t q = 0; q < queue.size(); ++q)
			for (int side = 0; side < 6; ++side) {
				const int seg = queue[q], child = Segments[seg].children[side], wall = Segments[seg].sides[side].wall_num;
				if (child < 0 || depth[child] >= 0) continue;
				if (wall >= 0 && !(Walls[wall].type == WALL_OPEN || Walls[wall].type == WALL_ILLUSION || (Walls[wall].type == WALL_DOOR && !(Walls[wall].flags & WALL_DOOR_LOCKED) && Walls[wall].keys == KEY_NONE))) continue;
				depth[child] = depth[seg] + 1;
				queue.push_back(child);
			}
		int nearest = INT_MAX;
		for (int slot = 0; slot <= Highest_object_index; ++slot)
			if (Objects[slot].type == OBJ_POWERUP) {
				const auto &pickup = Objects[slot];
				std::printf("MINE-INVENTORY level=%d slot=%d id=%d signature=%d segment=%d depth=%d\n", Current_level_num, slot, pickup.id, pickup.signature, pickup.segnum, depth[pickup.segnum]);
				if (pickup.id == POW_PROXIMITY_WEAPON && depth[pickup.segnum] >= 0 && depth[pickup.segnum] < nearest) {
					mine_pickup = slot;
					mine_pickup_signature = pickup.signature;
					nearest = depth[pickup.segnum];
				}
			}
		require(mine_pickup >= 0, "authored proximity pickup reachable without keys or world changes");
		const auto center_route = public_capture_route_to(Objects[mine_pickup].segnum);
		for (size_t i = 0; i < center_route.size(); i += 2) waypoints.push_back(center_route[i]);
		waypoints.push_back(Objects[mine_pickup].pos);
	} else if (self_splash) {
		select_solid_side();
	} else {
		waypoints = public_capture_route_to(Objects[target].segnum);
		waypoints.push_back(Objects[target].pos);
	}
	char error[256] = {};
	const int frame_limit = mine_blast ? 900 : 360;
	require(input_demo_state_trace_start("capture-observation.jsonl.gz", "record", game_name, mission, Current_level_num, Difficulty_level, "new_level", frame_limit, error, sizeof(error)), error);
	const auto vector = [](const vms_vector &point) { return json::array({ point.x, point.y, point.z }); };
	const fix initial_shields = Players[0].shields;
	const json initial_player = { { "slot", int(Players[0].objnum) }, { "signature", int(ConsoleObject->signature) }, { "type", int(ConsoleObject->type) } };
	const fix target_initial_shields = self_splash ? 0 : Objects[target].shields;
	int request_frame = -1, shot_frame = -1, damage_frame = -1, death_frame = -1, retirement_frame = -1;
	int missile = -1, missile_signature = -1;
	int mine_pickup_frame = -1, mine_drop_frame = -1, mine = -1, mine_signature = -1;
	size_t waypoint = 0;
	json observations = json::array(), screenshot_frames = json::array();
	for (int frame = 0; frame < frame_limit; ++frame) {
		FrameTime = F1_0 / (mine_blast ? 16 : 30);
		Controls = {};
		bool ready = false;
		if (request_frame < 0) {
			if (mine_blast && mine_drop_frame >= 0) {
				if (frame < mine_drop_frame + 8) Controls.sideways_thrust_time = FrameTime;
				ready = frame == mine_drop_frame + 8;
			} else if (mine_blast && mine_pickup_frame < 0) {
				while (waypoint < waypoints.size() && vm_vec_dist(&ConsoleObject->pos, &waypoints[waypoint]) < 5 * F1_0) ++waypoint;
				if (waypoint < waypoints.size()) public_capture_hold(waypoints[waypoint], waypoint + 1 == waypoints.size() ? 2 : 8);
				int threat = -1;
				fix nearest = 90 * F1_0;
				for (int slot = 0; slot <= Highest_object_index; ++slot)
					if (Objects[slot].type == OBJ_ROBOT && Objects[slot].shields >= 0 && !(Objects[slot].flags & OF_EXPLODING) && vm_vec_dist(&Objects[slot].pos, &ConsoleObject->pos) < nearest && object_to_object_visibility(ConsoleObject, &Objects[slot], 0)) {
						threat = slot;
						nearest = vm_vec_dist(&Objects[slot].pos, &ConsoleObject->pos);
					}
				if (threat >= 0) {
					public_capture_aim(Objects[threat].pos);
					vms_vector direction;
					vm_vec_normalized_dir(&direction, &Objects[threat].pos, &ConsoleObject->pos);
					if (vm_vec_dot(&direction, &ConsoleObject->orient.fvec) > F1_0 * 98 / 100) Controls.fire_primary_state = 1;
				}
			} else if (self_splash) {
				public_capture_hold(hold);
				public_capture_aim(aim);
				vms_vector direction;
				vm_vec_normalized_dir(&direction, &aim, &ConsoleObject->pos);
				ready = vm_vec_dist(&hold, &ConsoleObject->pos) < (mine_blast ? 4 : 2) * F1_0 &&
				        vm_vec_mag(&ConsoleObject->mtype.phys_info.velocity) < (mine_blast ? 10 : 5) * F1_0 &&
				        vm_vec_dot(&direction, &ConsoleObject->orient.fvec) > F1_0 * (mine_blast ? 990 : 999) / 1000;
				if (mine_blast) {
					int threat = -1;
					fix nearest = 40 * F1_0;
					for (int slot = 0; slot <= Highest_object_index; ++slot)
						if (Objects[slot].type == OBJ_ROBOT && Objects[slot].shields >= 0 && !(Objects[slot].flags & OF_EXPLODING) && vm_vec_dist(&Objects[slot].pos, &ConsoleObject->pos) < nearest && object_to_object_visibility(ConsoleObject, &Objects[slot], 0)) {
							threat = slot;
							nearest = vm_vec_dist(&Objects[slot].pos, &ConsoleObject->pos);
						}
					if (threat >= 0) {
						public_capture_aim(Objects[threat].pos);
						vm_vec_normalized_dir(&direction, &Objects[threat].pos, &ConsoleObject->pos);
						if (vm_vec_dot(&direction, &ConsoleObject->orient.fvec) > F1_0 * 98 / 100) Controls.fire_primary_state = 1;
						ready = false;
					}
					for (int slot = 0; slot <= Highest_object_index; ++slot)
						if (Objects[slot].type == OBJ_WEAPON && Objects[slot].ctype.laser_info.parent_type == OBJ_ROBOT && vm_vec_dist(&Objects[slot].pos, &ConsoleObject->pos) < 50 * F1_0) ready = false;
				}
			} else {
				while (waypoint < waypoints.size() && vm_vec_dist(&ConsoleObject->pos, &waypoints[waypoint]) < 8 * F1_0) ++waypoint;
				if (waypoint < waypoints.size()) public_capture_hold(waypoints[waypoint], waypoint + 1 == waypoints.size() ? 2 : 8);
				auto &robot = Objects[target];
				if (vm_vec_dist(&robot.pos, &ConsoleObject->pos) < 120 * F1_0 && object_to_object_visibility(ConsoleObject, &robot, 0)) {
					vms_vector direction, point = robot.pos;
					vm_vec_normalized_dir(&direction, &robot.pos, &ConsoleObject->pos);
					vm_vec_scale_add2(&point, &direction, -50 * F1_0);
					public_capture_hold(point);
					public_capture_aim(robot.pos);
					ready = vm_vec_dot(&direction, &ConsoleObject->orient.fvec) > F1_0 * 995 / 1000;
				}
			}
			if (ready) {
				if (mine_blast && mine_drop_frame < 0) {
					Controls.drop_bomb_count = 1;
					mine_drop_frame = frame;
				} else if (!mine_blast || frame >= mine_drop_frame + 8) {
					request_frame = frame;
					if (!no_missile) Controls.fire_secondary_state = 1;
				}
			}
		}
		const int ammo_before = Players[0].secondary_ammo[0];
		do_weapon_n_item_stuff();
		if (mine_blast && mine_drop_frame == frame)
			for (int slot = 0; slot <= Highest_object_index; ++slot)
				if (Objects[slot].type == OBJ_WEAPON && Objects[slot].id == PROXIMITY_ID && Objects[slot].ctype.laser_info.parent_num == Players[0].objnum && Objects[slot].ctype.laser_info.parent_signature == ConsoleObject->signature) {
					mine = slot;
					mine_signature = Objects[slot].signature;
				}
		if (Players[0].secondary_ammo[0] < ammo_before) {
			shot_frame = frame;
			for (int slot = 0; slot <= Highest_object_index; ++slot)
				if (Objects[slot].type == OBJ_WEAPON && Objects[slot].id == CONCUSSION_ID &&
				    Objects[slot].ctype.laser_info.parent_num == Players[0].objnum &&
				    Objects[slot].ctype.laser_info.parent_signature == ConsoleObject->signature) {
					missile = slot;
					missile_signature = Objects[slot].signature;
				}
		}
		calc_game_time();
		GameProcessFrame();
		require(render_update_main_view_objects(), "missile capture gameplay view state");
		require(render_update_game_view_automap(), "fault capture native camera map discovery");
		if (mine_blast && mine_pickup_frame < 0 && Players[0].secondary_ammo[PROXIMITY_INDEX] > 0 && (Objects[mine_pickup].flags & OF_SHOULD_BE_DEAD || Objects[mine_pickup].type != OBJ_POWERUP || Objects[mine_pickup].signature != mine_pickup_signature)) {
			mine_pickup_frame = frame;
			select_solid_side();
		}
		if (self_splash) {
			if (damage_frame < 0 && Players[0].shields < initial_shields) damage_frame = frame;
		} else {
			if (damage_frame < 0 && Objects[target].shields < target_initial_shields) damage_frame = frame;
			if (death_frame < 0 && Objects[target].shields < 0) death_frame = frame;
			if (retirement_frame < 0 && (Objects[target].type != OBJ_ROBOT || Objects[target].signature != target_signature)) retirement_frame = frame;
		}
		observations.push_back({ { "f", frame }, { "position", vector(ConsoleObject->pos) }, { "velocity", vector(ConsoleObject->mtype.phys_info.velocity) }, { "segment", int(ConsoleObject->segnum) }, { "shields", int(Players[0].shields) }, { "ammo", int(Players[0].secondary_ammo[0]) }, { "score", int(Players[0].score) }, { "target_shields", self_splash ? 0 : int(Objects[target].shields) } });
		if (mine_blast) {
			json identity = nullptr;
			if (mine >= 0) {
				const auto &object = Objects[mine];
				identity = { { "slot", mine }, { "signature", int(object.signature) }, { "type", int(object.type) }, { "id", int(object.id) }, { "flags", int(object.flags) } };
				if (object.type == OBJ_WEAPON) identity["parent"] = { { "type", int(object.ctype.laser_info.parent_type) }, { "slot", int(object.ctype.laser_info.parent_num) }, { "signature", int(object.ctype.laser_info.parent_signature) }, { "creation_time", int64_t(object.ctype.laser_info.creation_time) } };
			}
			observations.back().update({ { "waypoint", waypoint }, { "waypoints", waypoints.size() }, { "mine_ammo", int(Players[0].secondary_ammo[PROXIMITY_INDEX]) }, { "mine_live", mine >= 0 && Objects[mine].type == OBJ_WEAPON && Objects[mine].signature == mine_signature }, { "mine_identity", identity }, { "mine_velocity", mine >= 0 ? vector(Objects[mine].mtype.phys_info.velocity) : json(nullptr) }, { "mine_position", mine >= 0 ? vector(Objects[mine].pos) : json(nullptr) } });
		}
		if (frame % 30 == 0 || frame == request_frame || frame == damage_frame || frame == retirement_frame)
			std::printf("MISSILE scenario=%s frame=%d request=%d shot=%d damage=%d player_shields=%d target_shields=%d ammo=%d score=%d segment=%d\n", scenario, frame, request_frame, shot_frame, damage_frame, Players[0].shields, self_splash ? 0 : Objects[target].shields, Players[0].secondary_ammo[0], Players[0].score, ConsoleObject->segnum);
		if (screenshots && (frame % 30 == 0 || frame == request_frame || frame == damage_frame || frame == retirement_frame)) {
			GameArg.SysInputDemoNoRender = 0;
			game_render_frame_mono(1);
			screenshot_frames.push_back(frame);
			save_screen_shot(1);
			GameArg.SysInputDemoNoRender = 1;
		}
		if (Player_is_dead || (request_frame >= 0 && frame >= request_frame + (mine_blast ? 16 : 90))) break;
	}
	input_demo_result result;
	input_demo_capture_current_result(&result);
	require(input_demo_record_finish_input_tail(error, sizeof(error)), "finalize ordered missile input tail");
	require(input_demo_recorder_flush_with_result("capture.dximdemo", &result, error, sizeof(error)), error);
	input_demo_state_trace_stop();
	newdemo_write_end();
	require(PHYSFS_close(outfile), "close missile classic callback sink");
	outfile = nullptr;
	Newdemo_state = ND_STATE_NORMAL;
	const json report = { { "game", game_name }, { "scenario", scenario }, { "initial_player", initial_player }, { "damage_frame_scope", self_splash ? "first_any_player_damage" : "first_target_robot_damage" }, { "negative_no_missile", no_missile }, { "frames", observations.size() }, { "spawn_segment", original_segment }, { "solid_side", wall_side }, { "aim", vector(aim) }, { "hold", vector(hold) }, { "target", target }, { "target_signature", target_signature }, { "request_frame", request_frame }, { "shot_frame", shot_frame }, { "damage_frame", damage_frame }, { "death_frame", death_frame }, { "retirement_frame", retirement_frame }, { "missile", missile }, { "missile_signature", missile_signature }, { "mine_pickup", mine_pickup }, { "mine_pickup_frame", mine_pickup_frame }, { "mine_drop_frame", mine_drop_frame }, { "mine", mine }, { "mine_signature", mine_signature }, { "initial_shields", int(initial_shields) }, { "initial_target_shields", int(target_initial_shields) }, { "missile_damage_radius", int(Weapon_info[CONCUSSION_ID].damage_radius) }, { "observations", observations }, { "screenshot_frames", screenshot_frames } };
	const auto text = report.dump(2) + "\n";
	write_fixture("capture.json", bytes(text.begin(), text.end()));
	require(request_frame >= 0, "ordinary controls reached missile firing position");
	require(no_missile || (shot_frame >= 0 && missile >= 0), "ordinary controls fired a player-owned concussion");
	std::puts("PASS: diagnostic ordinary-start missile capture; inspect outcomes before admission");
}
