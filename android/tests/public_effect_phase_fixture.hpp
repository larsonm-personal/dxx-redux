// Preserve the live early-monitor phase before attempting any parity repair
// Only ordinary flight controls alter the world; effect state is observed as-is
static void capture_public_effect_phase(const char *game_name, const char *mission, int level)
{
	using nlohmann::json;
	constexpr int monitor_segment = 11, monitor_side = 0;
	constexpr int frame_limit = 240, stable_frames_required = 30;
	require(level == 1 && Highest_segment_index >= 12 && Num_effects > 26, "public D1 level-one monitor resources");
	const auto has_texture = [](int segment, int side, int texture) {
		const auto &wall = Segments[segment].sides[side];
		return wall.tmap_num == texture || (wall.tmap_num2 & 0x3fff) == texture;
	};
	require(has_texture(11, 0, 349) && has_texture(11, 2, 349) && has_texture(12, 3, 352), "original early monitor texture locations");
	require(Effects[24].changing_wall_texture == 349 && Effects[26].changing_wall_texture == 352, "original monitor eclip mappings");
	const auto vector = [](const vms_vector &point) { return json::array({ point.x, point.y, point.z }); };
	const auto bitmap_name = [](int index) {
		if (index < 0 || index >= MAX_BITMAP_FILES) return std::string();
		const char *name = piggy_game_bitmap_name(&GameBitmaps[index]);
		return std::string(name ? name : "");
	};
	const auto effect = [&](int index) {
		const auto &ec = Effects[index];
		const int texture = ec.changing_wall_texture;
		const int selected = texture >= 0 && texture < MAX_TEXTURES ? Textures[texture].index : -1;
		const int frame_bitmap = ec.frame_count >= 0 && ec.frame_count < ec.vc.num_frames && ec.frame_count < VCLIP_MAX_FRAMES ? ec.vc.frames[ec.frame_count].index : -1;
		return json{ { "effect", index }, { "time_left", int(ec.time_left) }, { "frame_count", int(ec.frame_count) }, { "frame_time", int(ec.vc.frame_time) }, { "num_frames", int(ec.vc.num_frames) }, { "flags", int(ec.flags) }, { "changing_wall_texture", texture }, { "selected_bitmap", selected }, { "selected_bitmap_name", bitmap_name(selected) }, { "frame_bitmap", frame_bitmap }, { "frame_bitmap_name", bitmap_name(frame_bitmap) } };
	};
	const auto snapshot = [&]() {
		return json{ { "game_time", GameTime64 }, { "segment", int(ConsoleObject->segnum) }, { "position", vector(ConsoleObject->pos) }, { "velocity", vector(ConsoleObject->mtype.phys_info.velocity) }, { "orientation", { { "right", vector(ConsoleObject->orient.rvec) }, { "up", vector(ConsoleObject->orient.uvec) }, { "forward", vector(ConsoleObject->orient.fvec) } } }, { "rotational_velocity", vector(ConsoleObject->mtype.phys_info.rotvel) }, { "effect24", effect(24) }, { "effect26", effect(26) }, { "shields", int(Players[0].shields) } };
	};
	// This is before the first fixture simulation frame, not a synthesized phase
	const json initial = snapshot();
	vms_vector hold, aim;
	compute_segment_center(&hold, &Segments[monitor_segment]);
	compute_center_point_on_side(&aim, &Segments[monitor_segment], monitor_side);
	const auto center_route = public_capture_route_to(monitor_segment);
	std::vector<vms_vector> waypoints;
	// Use the already qualified loot route's portal-only early-room guidance
	for (size_t i = 0; i < center_route.size(); i += 2) waypoints.push_back(center_route[i]);
	waypoints.push_back(hold);
	size_t waypoint = 0;
	char error[256] = {};
	require(input_demo_state_trace_start("capture-observation.jsonl.gz", "record", game_name, mission, level, Difficulty_level, "new_level", frame_limit, error, sizeof(error)), error);
	const bool screenshots = std::getenv("DXX_PUBLIC_CAPTURE_SCREENSHOTS") != nullptr;
	int arrival_frame = -1, first_stable_frame = -1, stable_start_frame = -1, stable_frames = 0;
	json observations = json::array(), screenshot_frames = json::array(), screenshot_events = json::array();
	for (int frame = 0; frame < frame_limit; ++frame) {
		FrameTime = F1_0 / 30;
		Controls = {};
		if (arrival_frame < 0) {
			while (waypoint < waypoints.size() && vm_vec_dist(&ConsoleObject->pos, &waypoints[waypoint]) < 8 * F1_0) ++waypoint;
			if (waypoint < waypoints.size()) public_capture_hold(waypoints[waypoint], waypoint + 1 == waypoints.size() ? 2 : 8);
		} else {
			public_capture_hold(hold);
			public_capture_aim(aim);
		}
		do_weapon_n_item_stuff();
		calc_game_time();
		GameProcessFrame();
		require(render_update_main_view_objects(), "monitor capture gameplay view state");
		require(render_update_game_view_automap(), "fault capture native camera map discovery");
		if (arrival_frame < 0 && ConsoleObject->segnum == monitor_segment) arrival_frame = frame;
		vms_vector direction;
		vm_vec_normalized_dir(&direction, &aim, &ConsoleObject->pos);
		const fix aim_alignment = vm_vec_dot(&direction, &ConsoleObject->orient.fvec);
		const bool stable = ConsoleObject->segnum == monitor_segment && vm_vec_dist(&ConsoleObject->pos, &hold) < 4 * F1_0 &&
		                    vm_vec_mag(&ConsoleObject->mtype.phys_info.velocity) < 8 * F1_0 && aim_alignment > F1_0 * 99 / 100 &&
		                    vm_vec_mag(&ConsoleObject->mtype.phys_info.rotvel) < F1_0 / 16;
		if (stable) {
			if (first_stable_frame < 0) first_stable_frame = frame;
			if (!stable_frames) stable_start_frame = frame;
			++stable_frames;
		} else {
			stable_frames = 0;
			stable_start_frame = -1;
		}
		json observation = snapshot();
		observation.update({ { "f", frame }, { "frame_time", int(FrameTime) }, { "aim_alignment", int(aim_alignment) }, { "stable_view", stable }, { "stable_frames", stable_frames }, { "waypoint", waypoint } });
		observations.push_back(observation);
		const bool finished = stable_frames >= stable_frames_required;
		const char *moment = frame == arrival_frame ? "arrival" : finished                  ? "stable_end"
		                                                      : frame == first_stable_frame ? "first_stable"
		                                                      : frame == 0                  ? "first_frame"
		                                                      : stable && frame == 170      ? "stable_animation_phase"
		                                                                                    : nullptr;
		if (screenshots && moment) {
			GameArg.SysInputDemoNoRender = 0;
			game_render_frame_mono(1);
			save_screen_shot(1);
			GameArg.SysInputDemoNoRender = 1;
			screenshot_frames.push_back(frame);
			screenshot_events.push_back({ { "f", frame }, { "moment", moment } });
		}
		if (frame % 30 == 0 || moment)
			std::printf("EFFECT-PHASE frame=%d segment=%d arrival=%d stable=%d effect24=%d/%d bitmap=%d effect26=%d/%d bitmap=%d\n", frame, ConsoleObject->segnum, arrival_frame, stable_frames, Effects[24].frame_count, Effects[24].time_left, Textures[349].index, Effects[26].frame_count, Effects[26].time_left, Textures[352].index);
		if (Player_is_dead || finished) break;
	}
	input_demo_result result;
	input_demo_capture_current_result(&result);
	require(input_demo_record_finish_input_tail(error, sizeof(error)), "finalize ordered monitor input tail");
	require(input_demo_recorder_flush_with_result("capture.dximdemo", &result, error, sizeof(error)), error);
	input_demo_state_trace_stop();
	newdemo_write_end();
	require(PHYSFS_close(outfile), "close monitor classic callback sink");
	outfile = nullptr;
	Newdemo_state = ND_STATE_NORMAL;
#ifdef DXX_BUILD_DESCENT_II
	const char *capture_role = "imported_d1_diagnostic";
#else
	const char *capture_role = "native_d1_reference";
#endif
	const json report = { { "game", game_name }, { "mission", mission }, { "level", level }, { "scenario", "effect-phase" }, { "capture_role", capture_role }, { "public_replay_admission", false }, { "frame_limit", frame_limit }, { "frames", observations.size() }, { "monitor", { { "segment", monitor_segment }, { "side", monitor_side }, { "texture", 349 }, { "effect", 24 }, { "asset", "misc066.abm" } } }, { "adjacent_monitor", { { "segment", 12 }, { "side", 3 }, { "texture", 352 }, { "effect", 26 } } }, { "hold", vector(hold) }, { "aim", vector(aim) }, { "initial", initial }, { "arrival_frame", arrival_frame }, { "first_stable_frame", first_stable_frame }, { "stable_start_frame", stable_start_frame }, { "stable_frames", stable_frames }, { "observations", observations }, { "screenshot_frames", screenshot_frames }, { "screenshot_events", screenshot_events }, { "observation_phase", "post_GameProcessFrame_pre_render" }, { "screenshots_uncropped", true } };
	const auto text = report.dump(2) + "\n";
	write_fixture("capture.json", bytes(text.begin(), text.end()));
	require(arrival_frame >= 0 && stable_frames >= stable_frames_required && !Player_is_dead, "ordinary controls reached the monitor and held thirty stable view frames");
	require(!observations.empty() && observations.size() <= unsigned(frame_limit), "compact monitor capture stays inside its hard frame cap");
	std::puts("PASS: diagnostic ordinary-start monitor capture; native D1 remains the expected source");
}
