// Route to a real authored carrier, then chase only its newly created pickup
static void capture_public_loot_frames(const char *game_name, const char *mission, int level)
{
	using nlohmann::json;
#ifdef DXX_BUILD_DESCENT_II
	require(false, "D2 loot route is not yet qualified");
#endif
	const int target = 39, target_signature = 40, pickup_id = POW_LASER;
	require(Objects[target].type == OBJ_ROBOT && Objects[target].id == 20 && Objects[target].signature == target_signature && Objects[target].segnum == 231 && Objects[target].contains_type == OBJ_POWERUP && Objects[target].contains_id == pickup_id && Objects[target].contains_count == 1, "original public laser-upgrade carrier");
	const int target_shields = Objects[target].shields, target_score = Robot_info[Objects[target].id].score_value;
	int maximum_signature = 0;
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].signature > maximum_signature) maximum_signature = Objects[i].signature;
	auto center_route = public_capture_route_to(Objects[target].segnum);
	std::vector<vms_vector> waypoints;
	// Fly portal-to-portal through the broad early rooms, retaining center
	// guidance from the segment-100 hub into the narrow secret branch
	bool keep_centers = false;
	for (size_t i = 0; i < center_route.size(); i += 2) {
		waypoints.push_back(center_route[i]);
		if (find_point_seg(&center_route[i + 1], ConsoleObject->segnum) == 100) keep_centers = true;
		if (keep_centers) waypoints.push_back(center_route[i + 1]);
	}
	waypoints.push_back(Objects[target].pos);
	size_t waypoint = 0;
	char error[256] = {};
	const int frame_limit = 1000;
	require(input_demo_state_trace_start("capture-observation.jsonl", "record", game_name, mission, level, Difficulty_level, "new_level", frame_limit, error, sizeof(error)), error);
	int damage_frame = -1, death_frame = -1, retirement_frame = -1, loot_frame = -1, pickup_frame = -1, pickup_gain_frame = -1, pickup = -1, pickup_signature = -1;
	json observations = json::array(), screenshot_frames = json::array();
	const auto position = [](const vms_vector &point) { return json::array({ point.x, point.y, point.z }); };
	const auto start = ConsoleObject->pos;
	const bool screenshots = std::getenv("DXX_PUBLIC_CAPTURE_SCREENSHOTS") != nullptr;
	for (int frame = 0; frame < frame_limit; ++frame) {
		FrameTime = F1_0 / 30;
		Controls = {};
		if (pickup < 0 && death_frame < 0) {
			while (waypoint < waypoints.size() && vm_vec_dist(&ConsoleObject->pos, &waypoints[waypoint]) < 8 * F1_0) ++waypoint;
			if (waypoint < waypoints.size()) public_capture_hold(waypoints[waypoint], waypoint + 1 == waypoints.size() ? 2 : 8);
			auto &robot = Objects[target];
			if (vm_vec_dist(&robot.pos, &ConsoleObject->pos) < 120 * F1_0 && object_to_object_visibility(ConsoleObject, &robot, 0)) {
				vms_vector direction, point = robot.pos;
				vm_vec_normalized_dir(&direction, &robot.pos, &ConsoleObject->pos);
				vm_vec_scale_add2(&point, &direction, -50 * F1_0);
				public_capture_hold(point);
				public_capture_aim(robot.pos);
				if (vm_vec_dot(&direction, &ConsoleObject->orient.fvec) > F1_0 * 98 / 100) Controls.fire_primary_state = 1;
			}
		} else if (pickup >= 0 && pickup_frame < 0) {
			public_capture_hold(Objects[pickup].pos, 4);
		}
		do_weapon_n_item_stuff();
		calc_game_time();
		GameProcessFrame();
		require(render_update_main_view_objects(), "loot capture gameplay view state");
		if (damage_frame < 0 && Objects[target].shields < target_shields) damage_frame = frame;
		if (death_frame < 0 && Objects[target].shields < 0 && Players[0].score >= target_score) death_frame = frame;
		if (retirement_frame < 0 && (Objects[target].type != OBJ_ROBOT || Objects[target].signature != target_signature)) retirement_frame = frame;
		if (death_frame >= 0 && pickup < 0)
			for (int i = 0; i <= Highest_object_index; ++i)
				if (Objects[i].type == OBJ_POWERUP && Objects[i].id == pickup_id && Objects[i].signature > maximum_signature) {
					pickup = i;
					pickup_signature = Objects[i].signature;
					loot_frame = frame;
					break;
				}
		if (pickup_gain_frame < 0 && Players[0].laser_level == 1) pickup_gain_frame = frame;
		if (pickup >= 0 && pickup_frame < 0 && (Objects[pickup].type != OBJ_POWERUP || Objects[pickup].signature != pickup_signature)) pickup_frame = frame;
		if (frame % 30 == 0) std::printf("LOOT frame=%d seg=%d waypoints=%zu/%zu robot_seg=%d robot_shields=%d player_shields=%d score=%d pickup=%d laser=%d\n", frame, ConsoleObject->segnum, waypoint, waypoints.size(), Objects[target].segnum, Objects[target].shields, Players[0].shields, Players[0].score, pickup, Players[0].laser_level);
		observations.push_back({ { "f", frame }, { "position", position(ConsoleObject->pos) }, { "segment", int(ConsoleObject->segnum) }, { "energy", int(Players[0].energy) }, { "shields", int(Players[0].shields) }, { "laser_level", +Players[0].laser_level }, { "primary_weapon", +Players[0].primary_weapon } });
		if (screenshots && (frame % 120 == 0 || frame == damage_frame || frame == death_frame || frame == pickup_frame)) {
			GameArg.SysInputDemoNoRender = 0;
			game_render_frame_mono(1);
			screenshot_frames.push_back(frame);
			save_screen_shot(1);
			GameArg.SysInputDemoNoRender = 1;
		}
		if (Player_is_dead || pickup_frame >= 0) break;
	}
	input_demo_result result;
	input_demo_capture_current_result(&result);
	require(input_demo_record_finish_input_tail(error, sizeof(error)), "finalize ordered input tail");
	require(input_demo_recorder_flush_with_result("capture.dximdemo", &result, error, sizeof(error)), error);
	input_demo_state_trace_stop();
	newdemo_write_end();
	require(PHYSFS_close(outfile), "close classic callback sink");
	outfile = nullptr;
	Newdemo_state = ND_STATE_NORMAL;
	const json report = { { "game", game_name }, { "scenario", "loot" }, { "start", position(start) }, { "target", target }, { "target_id", 20 }, { "target_signature", target_signature }, { "target_shields", target_shields }, { "target_score", target_score }, { "damage_frame", damage_frame }, { "action_frame", death_frame }, { "retirement_frame", retirement_frame }, { "loot_frame", loot_frame }, { "pickup", pickup }, { "pickup_signature", pickup_signature }, { "pickup_frame", pickup_frame }, { "pickup_gain_frame", pickup_gain_frame }, { "observations", observations }, { "screenshot_frames", screenshot_frames }, { "wall_contact_frames", 0 } };
	const auto text = report.dump(2) + "\n";
	write_fixture("capture.json", bytes(text.begin(), text.end()));
	require(pickup_frame >= 0 && Players[0].laser_level == 1 && !Player_is_dead, "ordinary controls collected the real authored laser drop");
	std::puts("PASS: public robot loot capture");
}
