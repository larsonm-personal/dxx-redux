#ifndef DXX_BUILD_DESCENT_II
extern "C" int check_trans_wall(vms_vector *, segment *, int, int);
#endif

// Observe the engine's actual native UV and merged-pixel decision at an authored
// target point. Live projectile trajectory and collision identity are separate gates
static nlohmann::json observe_public_fan_pixel(vms_vector point)
{
	using nlohmann::json;
	auto &surface = Segments[135].sides[4];
	fix u, v;
	find_hitpoint_uv(&u, &v, nullptr, &point, &Segments[135], 4, 0);
	grs_bitmap *bitmap = texmerge_get_cached_bitmap(surface.tmap_num, surface.tmap_num2);
	if (bitmap->bm_flags & BM_FLAG_RLE) bitmap = rle_expand_texture(bitmap);
	const int x = unsigned(f2i(u * bitmap->bm_w)) % bitmap->bm_w;
	const int y = unsigned(f2i(v * bitmap->bm_h)) % bitmap->bm_h;
	const int pixel = gr_gpixel(bitmap, x, y);
#ifndef DXX_BUILD_DESCENT_II
	const int native_crossable = check_trans_wall(&point, &Segments[135], 4, 0);
	require(native_crossable == (pixel == 255), "native fan predicate agrees with native merged pixel");
#else
	const int native_crossable = -1;
#endif
	return json{ { "point", { int(point.x), int(point.y), int(point.z) } }, { "u", int(u) }, { "v", int(v) }, { "x", x }, { "y", y }, { "pixel", pixel }, { "native_crossable", native_crossable }, { "effect_frame", int(Effects[3].frame_count) }, { "wall_bitmap", int(Textures[329].index) } };
}

// Observation only: native asset readers and authored geometry, no world edits
static void observe_public_fan_resources(const char *game_name)
{
	using nlohmann::json;
	require(Current_level_num == 1 && Highest_segment_index >= 135 && Num_effects > 3, "authored public level-one fan");
	require(Effects[3].changing_wall_texture == 329 && Effects[3].vc.num_frames == 8, "original fan eclip mapping");
	const auto vector = [](const vms_vector &v) { return json::array({ v.x, v.y, v.z }); };
	json bitmaps = json::array();
	const auto bitmap = [&](bitmap_index index, const std::string &name) {
		PIGGY_PAGE_IN(index);
		grs_bitmap *source = &GameBitmaps[index.index];
		grs_bitmap *decoded = source->bm_flags & BM_FLAG_RLE ? rle_expand_texture(source) : source;
		require(decoded->bm_w > 0 && decoded->bm_h > 0 && decoded->bm_w <= 256 && decoded->bm_h <= 256, "bounded original fan bitmap");
		bytes pixels;
		for (int y = 0; y < decoded->bm_h; ++y)
			pixels.insert(pixels.end(), decoded->bm_data + y * decoded->bm_rowsize, decoded->bm_data + y * decoded->bm_rowsize + decoded->bm_w);
		const std::string file = name + ".indexed";
		write_fixture(file.c_str(), pixels);
		bitmaps.push_back({ { "file", file }, { "index", int(index.index) }, { "name", piggy_game_bitmap_name(source) }, { "width", int(source->bm_w) }, { "height", int(source->bm_h) }, { "flags", int(source->bm_flags) }, { "decoded_rowsize", int(decoded->bm_rowsize) }, { "transparent_255", std::count(pixels.begin(), pixels.end(), 255) }, { "supertransparent_254", std::count(pixels.begin(), pixels.end(), 254) } });
	};
	for (int frame = 0; frame < Effects[3].vc.num_frames; ++frame)
		bitmap(Effects[3].vc.frames[frame], "fan-frame-" + std::to_string(frame));
	bitmap(Textures[83], "fan-base-83");
	write_fixture("fan.palette", bytes(gr_palette, gr_palette + 768));
	json graph = json::array(), surfaces = json::array();
	for (int segment = 0; segment <= Highest_segment_index; ++segment) {
		vms_vector center;
		compute_segment_center(&center, &Segments[segment]);
		json sides = json::array();
		for (int side = 0; side < 6; ++side) {
			const auto &s = Segments[segment].sides[side];
			vms_vector side_center;
			compute_center_point_on_side(&side_center, &Segments[segment], side);
			json entry = { { "side", side }, { "child", int(Segments[segment].children[side]) }, { "wall", int(s.wall_num) }, { "center", vector(side_center) } };
			if (s.wall_num >= 0) {
				const auto &w = Walls[s.wall_num];
				entry["wall_state"] = { { "type", int(w.type) }, { "flags", int(w.flags) }, { "keys", int(w.keys) } };
			}
			sides.push_back(entry);
			if (s.tmap_num == 329 || (s.tmap_num2 & 0x3fff) == 329) {
				json vertices = json::array(), uvls = json::array();
				for (int corner = 0; corner < 4; ++corner) {
					vertices.push_back(vector(Vertices[Segments[segment].verts[Side_to_verts[side][corner]]]));
					uvls.push_back({ int(s.uvls[corner].u), int(s.uvls[corner].v), int(s.uvls[corner].l) });
				}
				entry.update({ { "segment", segment }, { "type", int(s.type) }, { "texture", int(s.tmap_num) }, { "overlay", int(s.tmap_num2) }, { "vertices", vertices }, { "uvls", uvls }, { "normals", { vector(s.normals[0]), vector(s.normals[1]) } } });
				surfaces.push_back(entry);
			}
		}
		graph.push_back({ { "segment", segment }, { "center", vector(center) }, { "sides", sides } });
	}
	json weapons = json::array();
	for (int id : { int(Primary_weapon_to_weapon_info[LASER_INDEX]), int(FLARE_ID) }) {
		const auto &w = Weapon_info[id];
		weapons.push_back({ { "id", id }, { "speed", int(w.speed[Difficulty_level]) }, { "blob_size", int(w.blob_size) }, { "render_type", int(w.render_type) }, { "lifetime", int(w.lifetime) }, { "model", int(w.model_num) }, { "model_radius", int(Polygon_models[w.model_num].rad) }, { "po_len_to_width_ratio", int(w.po_len_to_width_ratio) } });
	}
	const json report = { { "role", "observation-only exploratory native reader; not an input demo" }, { "game", game_name }, { "level", Current_level_num }, { "game_time", GameTime64 }, { "player_segment", int(ConsoleObject->segnum) }, { "player_position", vector(ConsoleObject->pos) }, { "fan_effect", { { "index", 3 }, { "texture", int(Effects[3].changing_wall_texture) }, { "frame_count", int(Effects[3].frame_count) }, { "time_left", int(Effects[3].time_left) }, { "frame_time", int(Effects[3].vc.frame_time) } } }, { "bitmaps", bitmaps }, { "surfaces", surfaces }, { "segments", graph }, { "gun_points", { vector(Player_ship->gun_points[0]), vector(Player_ship->gun_points[1]), vector(Player_ship->gun_points[6]) } }, { "weapons", weapons } };
	const auto text = report.dump(2) + "\n";
	write_fixture("fan-observation.json", bytes(text.begin(), text.end()));
	std::puts("PASS: native-decoded fan assets and authored geometry observed without gameplay edits");
}

// Exploratory route: only ordinary control input advances authored level state
static void capture_public_fan_transparency(const char *game_name, const char *mission)
{
	using nlohmann::json;
	constexpr int frame_limit = 600, fan_segment = 135, fan_side = 4;
	require(Current_level_num == 1 && (Segments[fan_segment].sides[fan_side].tmap_num2 & 0x3fff) == 329, "original public fan surface");
	const auto vector = [](const vms_vector &v) { return json::array({ v.x, v.y, v.z }); };
	vms_vector hold, aim;
	compute_segment_center(&hold, &Segments[fan_segment]);
	// Authored UV pixel (47,34), read from the unmodified original fan surface
	vms_vector hit_center = { 8209978, -3153217, 30070145 };
	const auto &flare_info = Weapon_info[FLARE_ID];
	const fix flare_radius = fixdiv(Polygon_models[flare_info.model_num].rad, flare_info.po_len_to_width_ratio);
	hit_center.z += fixdiv(flare_radius, Segments[fan_segment].sides[fan_side].normals[0].z);
	aim = hit_center;
	const auto route = public_capture_route_to(fan_segment);
	std::vector<vms_vector> waypoints;
	for (size_t i = 0; i < route.size(); i += 2) waypoints.push_back(route[i]);
	waypoints.push_back(hold);
	char error[256] = {};
	require(input_demo_state_trace_start("capture-observation.jsonl.gz", "record", game_name, mission, 1, Difficulty_level, "new_level", frame_limit, error, sizeof(error)), error);
	const bool screenshots = std::getenv("DXX_PUBLIC_CAPTURE_SCREENSHOTS") != nullptr;
	const bool pixel_observation = std::getenv("DXX_PUBLIC_FAN_NO_PIXEL_OBSERVATION") == nullptr;
	const char *no_fire_setting = std::getenv("DXX_PUBLIC_FAN_NO_FIRE");
	const bool no_fire = no_fire_setting && std::strcmp(no_fire_setting, "1") == 0;
	const char *delay_setting = std::getenv("DXX_PUBLIC_FAN_DELAY");
	const int fire_delay = delay_setting ? std::atoi(delay_setting) : 0;
	require(fire_delay == 0 || fire_delay == 1, "bounded one-frame causal firing delay");
	int armed = -1;
	int arrival = -1, request = -1, stable_frames = 0, flare = -1, flare_signature = -1;
	size_t waypoint = 0;
	json observations = json::array(), screenshot_events = json::array();
	for (int frame = 0; frame < frame_limit; ++frame) {
		FrameTime = F1_0 / 16;
		Controls = {};
		if (arrival < 0) {
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
		} else if (armed < 0) {
			// Proportional ordinary analog input avoids coarse 16 Hz bang-bang oscillation
			const auto control = [&](fix error, fix scale) {
				const fix value = fixmul(FrameTime, fixdiv(error, scale));
				return value < -FrameTime ? -FrameTime : value > FrameTime ? FrameTime
				                                                           : value;
			};
			vms_vector delta, offset;
			vm_vec_sub(&delta, &hold, &ConsoleObject->pos);
			const auto thrust = [&](const vms_vector &axis) { return control(vm_vec_dot(&delta, &axis) - vm_vec_dot(&ConsoleObject->mtype.phys_info.velocity, &axis) / 2, 4 * F1_0); };
			Controls.forward_thrust_time = thrust(ConsoleObject->orient.fvec);
			Controls.sideways_thrust_time = thrust(ConsoleObject->orient.rvec);
			Controls.vertical_thrust_time = thrust(ConsoleObject->orient.uvec);
			vms_matrix transform;
			vm_copy_transpose_matrix(&transform, &ConsoleObject->orient);
			vm_vec_rotate(&offset, &Player_ship->gun_points[6], &transform);
			vm_vec_sub(&aim, &hit_center, &offset);
			vms_vector direction;
			vm_vec_normalized_dir(&direction, &aim, &ConsoleObject->pos);
			Controls.heading_time = control(vm_vec_dot(&direction, &ConsoleObject->orient.rvec) - ConsoleObject->mtype.phys_info.rotvel.y * 2, F1_0);
			Controls.pitch_time = control(-vm_vec_dot(&direction, &ConsoleObject->orient.uvec) - ConsoleObject->mtype.phys_info.rotvel.x * 2, F1_0);
			const bool stable = vm_vec_dist(&ConsoleObject->pos, &hold) < 2 * F1_0 && vm_vec_mag(&ConsoleObject->mtype.phys_info.velocity) < 4 * F1_0 && vm_vec_dot(&direction, &ConsoleObject->orient.fvec) > F1_0 * 9999 / 10000 && vm_vec_mag(&ConsoleObject->mtype.phys_info.rotvel) < F1_0 / 16;
			stable_frames = stable ? stable_frames + 1 : 0;
			int next_phase = Effects[3].frame_count;
			for (fix remaining = Effects[3].time_left - 2 * FrameTime; remaining < 0; remaining += Effects[3].vc.frame_time) next_phase = (next_phase + 1) % Effects[3].vc.num_frames;
			if (stable_frames >= 3 && next_phase == 0) {
				armed = frame;
				Controls = {};
			}
		}
		// Release every flight input at the shared arm frame; the controls differ
		// only by omitting the flare or delaying that one request by one frame
		if (armed >= 0 && frame == armed + fire_delay) {
			request = frame;
			if (!no_fire) Controls.fire_flare_count = 1;
		}
		do_weapon_n_item_stuff();
		if (frame == request)
			for (int slot = 0; slot <= Highest_object_index; ++slot)
				if (Objects[slot].type == OBJ_WEAPON && Objects[slot].id == FLARE_ID && Objects[slot].ctype.laser_info.parent_num == Players[0].objnum && Objects[slot].ctype.laser_info.parent_signature == ConsoleObject->signature) {
					flare = slot;
					flare_signature = Objects[slot].signature;
				}
		calc_game_time();
		GameProcessFrame();
		require(render_update_main_view_objects(), "fan capture gameplay view state");
		require(render_update_game_view_automap(), "fan capture native camera map discovery");
		if (arrival < 0 && ConsoleObject->segnum == fan_segment) arrival = frame;
		json row = { { "f", frame }, { "game_time", GameTime64 }, { "segment", int(ConsoleObject->segnum) }, { "position", vector(ConsoleObject->pos) }, { "velocity", vector(ConsoleObject->mtype.phys_info.velocity) }, { "forward", vector(ConsoleObject->orient.fvec) }, { "shields", int(Players[0].shields) }, { "waypoint", waypoint }, { "stable_frames", stable_frames }, { "fan_frame", int(Effects[3].frame_count) }, { "fan_timer", int(Effects[3].time_left) }, { "fan_bitmap", int(Textures[329].index) } };
		if (flare >= 0 && Objects[flare].type == OBJ_WEAPON && Objects[flare].signature == flare_signature) {
			const auto &o = Objects[flare];
			row["flare"] = { { "slot", flare }, { "signature", flare_signature }, { "segment", int(o.segnum) }, { "position", vector(o.pos) }, { "velocity", vector(o.mtype.phys_info.velocity) }, { "size", int(o.size) }, { "flags", int(o.flags) }, { "physics_flags", int(o.mtype.phys_info.flags) }, { "lifeleft", int(o.lifeleft) }, { "parent_num", int(o.ctype.laser_info.parent_num) }, { "parent_signature", int(o.ctype.laser_info.parent_signature) } };
		}
		if (pixel_observation && armed >= 0) {
			row["pixel_probe"] = observe_public_fan_pixel(hit_center);
			// Read-only sample of the exact first imported collision point observed
			// in the paired diagnostic. The final checker separately verifies that
			// the real projectile still hits this point; it is never a pose input
			row["collision_pixel_probe"] = observe_public_fan_pixel({ 8208474, -3156161, 30042682 });
		}
		observations.push_back(row);
		const bool moment = frame == 0 || frame == arrival || frame == request || (request >= 0 && frame == request + 8);
		if (screenshots && moment) {
			GameArg.SysInputDemoNoRender = 0;
			game_render_frame_mono(1);
			save_screen_shot(1);
			GameArg.SysInputDemoNoRender = 1;
			screenshot_events.push_back(frame);
		}
		if (frame % 32 == 0 || moment) std::printf("FAN frame=%d segment=%d waypoint=%zu arrival=%d request=%d stable=%d shields=%d phase=%d/%d\n", frame, ConsoleObject->segnum, waypoint, arrival, request, stable_frames, Players[0].shields, Effects[3].frame_count, Effects[3].time_left);
		if (Player_is_dead || (armed >= 0 && frame >= armed + 24)) break;
	}
	input_demo_result result;
	input_demo_capture_current_result(&result);
	require(input_demo_record_finish_input_tail(error, sizeof(error)), "finalize exploratory fan input tail");
	require(input_demo_recorder_flush_with_result("capture.dximdemo", &result, error, sizeof(error)), error);
	input_demo_state_trace_stop();
	newdemo_write_end();
	require(PHYSFS_close(outfile), "close fan classic callback sink");
	outfile = nullptr;
	Newdemo_state = ND_STATE_NORMAL;
	const json report = { { "role", "ordinary-start fan collision diagnostic; native reference only" }, { "scenario", "fan-transparency" }, { "capture_role", "native_d1_reference" }, { "public_replay_admission", false }, { "pixel_observation", pixel_observation }, { "game", game_name }, { "frame_limit", frame_limit }, { "frames", observations.size() }, { "hold", vector(hold) }, { "aim", vector(aim) }, { "hit_center", vector(hit_center) }, { "flare_radius", int(flare_radius) }, { "arrival_frame", arrival }, { "armed_frame", armed }, { "no_fire", no_fire }, { "fire_delay", fire_delay }, { "request_frame", request }, { "flare_slot", flare }, { "flare_signature", flare_signature }, { "observations", observations }, { "screenshot_frames", screenshot_events } };
	const auto text = report.dump(2) + "\n";
	write_fixture("capture.json", bytes(text.begin(), text.end()));
	require(arrival >= 0 && request >= 0 && (no_fire ? flare < 0 : flare >= 0) && !Player_is_dead, "ordinary controls reached fan and performed the declared flare intervention");
	std::puts("PASS: exploratory ordinary-start fan shot captured");
}
