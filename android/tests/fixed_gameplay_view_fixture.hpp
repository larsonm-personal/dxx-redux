/* Included by test_upstream_compat: real engine projection, portal and combat paths */
#include "gameplay_view_policy.h"
#include "input_demo_replay.h"
extern "C" {
extern fix Render_zoom, View_zoom;
extern vms_vector Window_scale, Matrix_scale, View_position, Viewer_eye;
extern vms_matrix View_matrix, Unscaled_matrix;
extern g3s_point Segment_points[];
extern int Canvas_width, Canvas_height, free_point_num;
extern fix Canv_w2, Canv_h2;
extern g3s_point *free_points[];
#ifndef DXX_BUILD_DESCENT_II
void update_all_robot_location_info(void);
#endif
}

static void test_fixed_gameplay_view()
{
	const int saved_rear = Rear_view, saved_depth = Render_depth;
	const int saved_classic = GameCfg.ClassicDepth, saved_acid = cheats.acid;
	const auto saved_cockpit = PlayerCfg.CurrentCockpitMode;
	const fix saved_zoom = Render_zoom;
	grs_canvas *saved_canvas = grd_curcanv;
	grs_screen *saved_screen = grd_curscreen;
	grs_screen screen = {};
	grs_canvas canvas = {};
	grd_curscreen = &screen;
	grd_curcanv = &canvas;
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Mission *saved_mission = Current_mission;
	Current_mission = &mission;
	const int profiles[] = { 1, 2 };
#else
	const int profiles[] = { 1 };
#endif
	vms_vector origin = { 0, 0, 0 };
	vms_matrix identity = vmd_identity_matrix;
	// Exact fixed-point boundaries through the production rotate/project path
	static_assert(GAMEPLAY_VIEW_WIDTH == 320 && GAMEPLAY_VIEW_HEIGHT == 133, "chosen original cockpit bounds");
	static_assert(GAMEPLAY_VIEW_SCREEN_ASPECT == 78643 && GAMEPLAY_VIEW_X_SCALE == 32685, "DOS fixed-point rounding");
	g3_start_frame_projection_size(GAMEPLAY_VIEW_WIDTH, GAMEPLAY_VIEW_HEIGHT, GAMEPLAY_VIEW_SCREEN_ASPECT);
	g3_set_view_matrix(&origin, &identity, GAMEPLAY_VIEW_ZOOM);
	require(Matrix_scale.x == 32685 && Matrix_scale.y == F1_0 && Matrix_scale.z == 0x9000,
	        "canonical cockpit projection retains exact DOS pixel aspect, canvas and zoom");
	for (int sign : { -1, 1 }) {
		for (int axis : { 0, 1 }) {
			vms_vector point = { 0, 0, 16 * (axis ? GAMEPLAY_VIEW_Y_SCALE : GAMEPLAY_VIEW_X_SCALE) };
			fix value = sign * 16 * GAMEPLAY_VIEW_ZOOM;
			if (axis) point.y = value;
			else point.x = value;
			g3s_point projected = {};
			require(!g3_rotate_point(&projected, &point), "exact angular gameplay boundary lies on the clip plane");
			g3_project_point(&projected);
			require(projected.p3_flags & PF_PROJECTED, "angular boundary is projectable");
			value += sign * 8;
			if (axis) point.y = value;
			else point.x = value;
			require(g3_rotate_point(&projected, &point) != 0, "fixed-point steps beyond the angular boundary are clipped");
			value -= sign * 16;
			if (axis) point.y = value;
			else point.x = value;
			require(!g3_rotate_point(&projected, &point), "fixed-point steps inside the angular boundary are retained");
		}
	}
	g3s_point rejected = {};
	vms_vector ninety_degree_top = { 0, 3 * F1_0, 3 * F1_0 };
	require(g3_rotate_point(&rejected, &ninety_degree_top) & CC_OFF_TOP,
	        "historical cockpit projection is deliberately narrower than 90 vertical degrees");

	int checks = 0;
	for (const int profile : profiles) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
#endif
		init_test_corridor(5);
		ConsoleObject = Viewer = &Objects[0];
		ConsoleObject->type = OBJ_PLAYER;
		ConsoleObject->orient = identity;
		compute_segment_center(&ConsoleObject->pos, &Segments[2]);
		obj_relink(0, 2);
		Players[0].objnum = Player_num = 0;
		N_players = 1;
		Game_mode = 0;
		Endlevel_sequence = Automap_active = 0;
		vms_vector front_pos, back_pos;
		compute_segment_center(&front_pos, &Segments[4]);
		compute_segment_center(&back_pos, &Segments[0]);
		const int front = obj_create(OBJ_ROBOT, 0, 4, &front_pos, &identity, F1_0, CT_AI, MT_NONE, RT_POLYOBJ);
		const int back = obj_create(OBJ_ROBOT, 0, 0, &back_pos, &identity, F1_0, CT_AI, MT_NONE, RT_POLYOBJ);
		require(front > 0 && back > 0, "create separated front and rear gameplay targets");
		Robot_info[0] = {};
		N_robot_types = 1;
		Weapon_info[HOMING_ID] = {};
		Weapon_info[HOMING_ID].homing_flag = 1;
		N_weapon_types = (std::max) (N_weapon_types, HOMING_ID + 1);
		const int missile = obj_create(OBJ_WEAPON, HOMING_ID, 2, &ConsoleObject->pos, &identity, F1_0 / 8, CT_WEAPON, MT_NONE, RT_NONE);
		require(missile > 0, "create player homing missile for camera-independent acquisition");
		Objects[missile].ctype.laser_info.parent_type = OBJ_PLAYER;
		Objects[missile].ctype.laser_info.parent_num = 0;
		Objects[missile].ctype.laser_info.parent_signature = ConsoleObject->signature;
		short canonical[MAX_RENDERED_OBJECTS];
		const int canonical_count = render_collect_gameplay_objects(canonical);
		const std::vector<short> expected(canonical, canonical + canonical_count);
		auto contains = [](const std::vector<short> &items, int target) { return std::find(items.begin(), items.end(), target) != items.end(); };
		require(canonical_count > 0 && contains(expected, front) && !contains(expected, back),
		        "fixed view includes the forward corridor and excludes the rear corridor");
		// Independent camera-collector oracle at the exact canonical projection
		canvas.cv_bitmap.bm_w = GAMEPLAY_VIEW_WIDTH;
		canvas.cv_bitmap.bm_h = GAMEPLAY_VIEW_HEIGHT;
		screen.sc_aspect = GAMEPLAY_VIEW_SCREEN_ASPECT;
		Rear_view = GameCfg.ClassicDepth = 0;
		Render_depth = MAX_RENDER_SEGS;
		Render_zoom = GAMEPLAY_VIEW_ZOOM;
		short reference[MAX_RENDERED_OBJECTS];
		int reference_count = render_collect_view_objects(0, reference);
		require(std::vector<short>(reference, reference + reference_count) == expected,
		        "canonical gameplay collector preserves original portal and object ordering");
		Rear_view = 1;
		reference_count = render_collect_view_objects(0, reference);
		const std::vector<short> reverse(reference, reference + reference_count);
		require(contains(reverse, back) && !contains(reverse, front), "old rear camera list demonstrably differs from ship-forward candidates");
		for (const int aspect : { 0, 1, 2 }) {
			const int widths[] = { 640, 1920, 480 }, heights[] = { 480, 1080, 800 };
			screen.sc_w = widths[aspect];
			screen.sc_h = heights[aspect];
			// Actual full-screen pixel-aspect calculation, not merely subcanvas resizing
			screen.sc_aspect = fixdiv(screen.sc_w * 3, screen.sc_h * 4);
			for (const int cockpit : { CM_FULL_COCKPIT, CM_STATUS_BAR, CM_FULL_SCREEN, CM_REAR_VIEW, CM_LETTERBOX }) {
				PlayerCfg.CurrentCockpitMode = cockpit;
				canvas.cv_bitmap.bm_w = screen.sc_w;
				canvas.cv_bitmap.bm_h = screen.sc_h * (cockpit == CM_FULL_SCREEN ? 1 : 2) / (cockpit == CM_FULL_SCREEN ? 1 : 3);
				for (const fix zoom : { 0x9000, 43940, 52658, 63858 }) {
					Render_zoom = zoom; // Existing base/100/110/120 visual zoom mapping
					for (const int rear : { 0, 1, 1, 0 }) {
						Rear_view = rear; // Held/toggled/released states share the same policy
						for (const int classic : { 0, 1 }) {
							GameCfg.ClassicDepth = classic;
							cheats.acid = classic;
							Render_depth = classic ? 1 : MAX_RENDER_SEGS;
							Viewer = rear ? &Objects[back] : ConsoleObject;
							g3_start_frame_projection();
							render_setup_view(F1_0 / 4);
							const auto matrix = View_matrix, unscaled = Unscaled_matrix;
							const auto scale = Matrix_scale, eye = Viewer_eye, window_scale = Window_scale, position = View_position;
							const int canvas_width = Canvas_width, canvas_height = Canvas_height;
							const fix canvas_w2 = Canv_w2, canvas_h2 = Canv_h2;
							std::swap(free_points[0], free_points[1]);
							const auto first_free = free_points[0], second_free = free_points[1];
							const int old_free_count = free_point_num;
							free_point_num = 1;
							const fix view_zoom = View_zoom;
							const std::vector<short> segments(Render_list, Render_list + N_render_segs);
							std::memset(Automap_visited, 0, sizeof(Automap_visited));
							Automap_visited[0] = 1;
							const std::vector<ubyte> map(Automap_visited, Automap_visited + MAX_SEGMENTS);
							const std::vector<object> before(Objects, Objects + Highest_object_index + 1);
							const auto sim = d_rand_get_call_count(), fx = d_rand_get_stream_call_count(D_RNG_FX);
							unsigned sim_state = 0, fx_state = 0, after_sim = 0, after_fx = 0;
							d_rand_get_state(&sim_state);
							d_rand_get_stream_state(D_RNG_FX, &fx_state);
							short candidates[MAX_RENDERED_OBJECTS];
							const int count = render_collect_gameplay_objects(candidates);
							require(count == canonical_count && std::vector<short>(candidates, candidates + count) == expected,
							        "gameplay candidates ignore actual aspect, cockpit, FoV, rear/external camera and depth settings");
							require(!std::memcmp(before.data(), Objects, before.size() * sizeof(object)) &&
							            std::equal(map.begin(), map.end(), Automap_visited),
							        "collection preserves world objects and camera-based map discovery");
							d_rand_get_state(&after_sim);
							d_rand_get_stream_state(D_RNG_FX, &after_fx);
							require(sim == d_rand_get_call_count() && fx == d_rand_get_stream_call_count(D_RNG_FX) && sim_state == after_sim && fx_state == after_fx,
							        "collection preserves both RNG states and counts");
							require(grd_curcanv == &canvas && Viewer == (rear ? &Objects[back] : ConsoleObject) &&
							            !std::memcmp(&matrix, &View_matrix, sizeof(matrix)) && !std::memcmp(&unscaled, &Unscaled_matrix, sizeof(unscaled)) &&
							            !std::memcmp(&scale, &Matrix_scale, sizeof(scale)) && !std::memcmp(&eye, &Viewer_eye, sizeof(eye)) && view_zoom == View_zoom &&
							            std::vector<short>(Render_list, Render_list + N_render_segs) == segments && cheats.acid == classic && GameCfg.ClassicDepth == classic && Render_depth == (classic ? 1 : MAX_RENDER_SEGS),
							        "collection restores visual camera projection, viewport, segment list and settings");
							require(Canvas_width == canvas_width && Canvas_height == canvas_height && Canv_w2 == canvas_w2 && Canv_h2 == canvas_h2 &&
							            !std::memcmp(&window_scale, &Window_scale, sizeof(window_scale)) && !std::memcmp(&position, &View_position, sizeof(position)) &&
							            free_point_num == 1 && free_points[0] == first_free && free_points[1] == second_free,
							        "collection preserves all scalar/vector projection globals and the clipping allocator permutation");
							free_point_num = old_free_count;
							std::swap(free_points[0], free_points[1]);
							render_update_gameplay_view_objects();
							require(find_homing_object(&ConsoleObject->pos, &Objects[missile]) == front,
							        "actual local-player homing acquisition remains ship-forward across visual settings");
							++checks;
						}
					}
				}
			}
		}
		// A loaded replay must take precisely the same bounded path as live play.
		const char *replay_game = profile == 1 ? "d1" : "d2";
		const nlohmann::json replay_header = { { "type", "header" }, { "version", 5 }, { "game", replay_game }, { "mission", "fixture" }, { "level", 1 }, { "difficulty", 2 }, { "frame_count", 1 }, { "git_version", "fixture" }, { "arch", "fixture" }, { "build_number", 0 }, { "start_mode", "new_level" }, { "rng_mode", "lcg_state" } };
		std::string replay_text = replay_header.dump() + "\n" +
		                          R"({"type":"frame","f":0,"ft":1092,"input":{"s":{}},"rng":{"s":1234,"c":0},"events":[],"dispatch":{"pre_rng":"=","ops":[{"k":"sample","ft":1092,"s":"="}]}})"
		                          "\n";

		replay_text += R"({"type":"input_tail","f":1,"input":{"s":{}},"rng":{"s":1234,"c":0},"dispatch":{"pre_rng":"=","ops":[]}})"
		               "\n";
		input_demo_result replay_result;
		input_demo_result_clear(&replay_result);
		std::strcpy(replay_result.game, replay_game);
		std::strcpy(replay_result.mission, "fixture");
		replay_result.level = 1;
		replay_result.difficulty = 2;
		replay_result.frame_count = 1;
		std::string result_text, result_error;
		require(input_demo_result_to_json_text(replay_result, &result_text, &result_error), result_error.c_str());
		replay_text += "{\"type\":\"result\",\"result\":" + nlohmann::json::parse(result_text).dump() + "}\n";
		write_fixture("gameplay-view-loaded.dximdemo", bytes(replay_text.begin(), replay_text.end()));
		for (const int loaded : { 0, 1 }) {
			char error[256] = "";
			if (loaded) require(input_demo_replay_load("gameplay-view-loaded.dximdemo", error, sizeof(error)), error);
			render_update_gameplay_view_objects();
			require(find_homing_object(&ConsoleObject->pos, &Objects[missile]) == front,
			        "loaded replay and live play choose the same fixed-forward homing target");
			Gameplay_num_rendered_objects = 0;
			require(find_homing_object(&ConsoleObject->pos, &Objects[missile]) == -1,
			        "neither live nor loaded replay widens an empty bounded player list to a complete scan");
		}
		input_demo_replay_unload();
		render_update_gameplay_view_objects();

		// The wall-clock-driven acid effect remains visual, never gameplay
		cheats.acid = 1;
		timer_update();
		const fix64 first_time = timer_query();
		SDL_Delay(20);
		timer_update();
		require(timer_query() > first_time, "acid regression samples distinct wall-clock times");
		short later[MAX_RENDERED_OBJECTS];
		const int later_count = render_collect_gameplay_objects(later);
		require(later_count == canonical_count && std::equal(expected.begin(), expected.end(), later) && cheats.acid == 1,
		        "gameplay candidates ignore acid wobble and time while retaining the visual setting");
		g3_start_frame_projection_size(GAMEPLAY_VIEW_WIDTH, GAMEPLAY_VIEW_HEIGHT, GAMEPLAY_VIEW_SCREEN_ASPECT);
		g3_set_view_matrix(&origin, &identity, GAMEPLAY_VIEW_ZOOM);
		int vertex = 0;
		render_start_frame();
		rotate_list(1, &vertex);
		const auto wobbled = Segment_points[0].p3_vec;
		cheats.acid = 0;
		render_start_frame();
		rotate_list(1, &vertex);
		require(std::memcmp(&wobbled, &Segment_points[0].p3_vec, sizeof(wobbled)), "ordinary visual vertex rotation still applies the acid effect");

		// Cached candidates are not replaced by rear/HUD drawing or elapsed wall time
		render_reset_gameplay_view_objects();
		require(find_homing_object(&ConsoleObject->pos, &Objects[missile]) == front,
		        "first secondary acquisition after a world reset initializes without any render or simulation step");
		const short *cached;
		require(render_get_gameplay_view_objects(&cached) == canonical_count && std::equal(expected.begin(), expected.end(), cached),
		        "first input after world reset lazily initializes forward candidates");
		ConsoleObject->orient.fvec.z = ConsoleObject->orient.rvec.x = -F1_0;
		require(render_get_gameplay_view_objects(&cached) == canonical_count && std::equal(expected.begin(), expected.end(), cached),
		        "current-step movement retains preceding-step candidates until the simulation boundary");
		render_update_gameplay_view_objects();
		require(contains(std::vector<short>(cached, cached + Gameplay_num_rendered_objects), back), "next simulation boundary updates ship orientation");
		ConsoleObject->orient = identity;
		render_reset_gameplay_view_objects();
		require(render_get_gameplay_view_objects(&cached) == canonical_count && std::equal(expected.begin(), expected.end(), cached), "reset prevents cross-world stale candidates");
		// Warning coordinates use exactly the same pre-scaled matrix as g3.
		// This is a strict forward box, not angular or portal/LOS eligibility.
		const int laser = obj_create(OBJ_WEAPON, 0, 2, &ConsoleObject->pos, &identity, F1_0 / 8, CT_WEAPON, MT_NONE, RT_NONE);
		Player_fired_laser_this_frame = laser;
		auto warn = [] {
#ifdef DXX_BUILD_DESCENT_II
			render_warn_robots_about_player_fire();
#else
			update_all_robot_location_info();
#endif
		};
		for (const int rear : { 0, 1 }) {
			Rear_view = rear;
			Viewer = &Objects[back];
			Objects[front].pos = front_pos;
			Objects[front].ctype.ai_info.danger_laser_num = Objects[back].ctype.ai_info.danger_laser_num = -1;
			warn();
			require(Objects[front].ctype.ai_info.danger_laser_num == laser && Objects[back].ctype.ai_info.danger_laser_num == -1 &&
			            Objects[front].ctype.ai_info.danger_laser_signature == Objects[laser].signature,
			        "forward warning number and signature ignore rear/external camera");
			for (const int axis : { 0, 1 })
				for (const int sign : { -1, 1 })
					for (const int inside : { 0, 1 }) {
						const int64_t scale = axis ? GAMEPLAY_VIEW_Y_SCALE : GAMEPLAY_VIEW_X_SCALE;
						const int64_t threshold = 4 * F1_0;
						// First world coordinate whose signed, floor-rounded projection
						// reaches the strict threshold, independently calculated in integers.
						const fix edge = sign > 0 ? (threshold * F1_0 + scale - 1) / scale : ((threshold - 1) * F1_0 / scale) + 1;
						Objects[front].pos = ConsoleObject->pos;
						Objects[front].pos.z += 40 * F1_0;
						(axis ? Objects[front].pos.y : Objects[front].pos.x) += sign * (edge - inside);
						Objects[front].ctype.ai_info.danger_laser_num = -1;
						warn();
						require((Objects[front].ctype.ai_info.danger_laser_num == laser) == bool(inside),
						        "warning box accepts exactly the inside fixed-point edge on both signs and axes");
					}
			for (const int axis : { 0, 1 }) {
				Objects[front].pos = ConsoleObject->pos;
				Objects[front].pos.z += 40 * F1_0;
				(axis ? Objects[front].pos.y : Objects[front].pos.x) += 6 * F1_0;
				Objects[front].ctype.ai_info.danger_laser_num = -1;
				warn();
				require((Objects[front].ctype.ai_info.danger_laser_num == laser) == !axis,
				        "cockpit scaling restores the horizontal six-unit warning while vertical six units stays outside");
			}
		}
		for (vms_angvec angles : { vms_angvec{ 0, 0, 0 }, vms_angvec{ 1000, 7000, 12000 }, vms_angvec{ -3000, 16000, -6000 } }) {
			vms_matrix orientation, scaled;
			vm_angles_2_matrix(&orientation, &angles);
			g3_start_frame_projection_size(GAMEPLAY_VIEW_WIDTH, GAMEPLAY_VIEW_HEIGHT, GAMEPLAY_VIEW_SCREEN_ASPECT);
			g3_set_view_matrix(&ConsoleObject->pos, &orientation, GAMEPLAY_VIEW_ZOOM);
			render_gameplay_view_orientation(&scaled, &orientation);
			require(!std::memcmp(&scaled, &View_matrix, sizeof(scaled)),
			        "warning projection matches g3 matrix scaling order and fixed-point rounding at arbitrary orientations");
		}
		Player_fired_laser_this_frame = -1;

		// Whole portals just inside/outside each angular boundary, allowing for
		// native integer portal-window clipping rather than point equality alone
		for (int axis : { 0, 1 })
			for (int sign : { -1, 1 })
				for (int outside : { 0, 1 }) {
					init_test_corridor(2);
					Viewer = ConsoleObject = &Objects[0];
					ConsoleObject->type = OBJ_PLAYER;
					ConsoleObject->pos = origin;
					ConsoleObject->orient = identity;
					const fix center = sign * (axis ? (outside ? 14 : 8) : (outside ? 25 : 20)) * F1_0;
					for (int vertex = 0; vertex < 4; ++vertex) {
						Vertices[vertex].x *= 10;
						Vertices[vertex].y *= 10;
					}
					for (int plane = 1; plane <= 2; ++plane)
						for (int vertex = 0; vertex < 4; ++vertex) {
							auto &point = Vertices[plane * 4 + vertex];
							point.x = (point.x < 0 ? -1 : 1) * F1_0 / 2 + (axis ? 0 : center);
							point.y = (point.y < 0 ? -1 : 1) * F1_0 / 2 + (axis ? center : 0);
							point.z = plane * 20 * F1_0;
						}
					validate_segment_all();
					vms_vector position;
					compute_segment_center(&position, &Segments[1]);
					const int target = obj_create(OBJ_ROBOT, 0, 1, &position, &identity, F1_0 / 8, CT_AI, MT_NONE, RT_NONE);
					require(target > 0, "create portal angular boundary target");
					short candidates[MAX_RENDERED_OBJECTS];
					const int count = render_collect_gameplay_objects(candidates);
					require(count >= 0 && (std::find(candidates, candidates + count, target) != candidates + count) == !outside,
					        "canonical traversal accepts inside portals and rejects outside portals on all four frustum boundaries");
				}

		// A target beyond the software budget distinguishes native build policies.
		init_test_corridor(28);
		Viewer = ConsoleObject = &Objects[0];
		ConsoleObject->type = OBJ_PLAYER;
		ConsoleObject->pos = origin;
		ConsoleObject->orient = identity;
		vms_vector deep_position;
		compute_segment_center(&deep_position, &Segments[26]);
		const int deep = obj_create(OBJ_ROBOT, 0, 26, &deep_position, &identity, F1_0 / 8, CT_NONE, MT_NONE, RT_NONE);
		for (const int depth : { 1, 20, MAX_RENDER_SEGS }) {
			Render_depth = depth;
			short candidates[MAX_RENDERED_OBJECTS];
			const int count = render_collect_gameplay_objects(candidates);
			require(count >= 0 && (std::find(candidates, candidates + count, deep) != candidates + count) == (GAMEPLAY_VIEW_RENDER_DEPTH > 26) && Render_depth == depth,
			        "fixed gameplay traversal uses the normal backend depth even after transient endlevel or detail changes");
		}

		// Exactly 101 eligible objects across five untruncated rows. This forces
		// three overflows, with an independently derived far-to-near offer order
		init_test_corridor(5);
		Viewer = ConsoleObject = &Objects[0];
		ConsoleObject->type = OBJ_PLAYER;
		ConsoleObject->pos = origin;
		ConsoleObject->orient = identity;
		for (int i = 0; i < 100; ++i) {
			const int segment = i / 20;
			vms_vector position = { 0, 0, (segment * 20 * F1_0) + (i % 20 + 1) * F1_0 / 4 };
			require(obj_create(OBJ_ROBOT, 0, segment, &position, &identity, F1_0 / 8, CT_NONE, MT_NONE, RT_NONE) == i + 1,
			        "create ordered multi-segment overflow targets");
		}
		std::vector<short> folded;
		for (short target = 100; target >= 0; --target) {
			if (folded.size() == MAX_RENDERED_OBJECTS) folded.resize(MAX_RENDERED_OBJECTS / 2);
			folded.push_back(target);
		}
		require(folded.size() == 26, "101 candidates exercise three upper-half overwrites");
		canvas.cv_bitmap.bm_w = GAMEPLAY_VIEW_WIDTH;
		canvas.cv_bitmap.bm_h = GAMEPLAY_VIEW_HEIGHT;
		screen.sc_aspect = GAMEPLAY_VIEW_SCREEN_ASPECT;
		Render_zoom = GAMEPLAY_VIEW_ZOOM;
		Render_depth = GAMEPLAY_VIEW_RENDER_DEPTH;
		GameCfg.ClassicDepth = Rear_view = cheats.acid = 0;
		short legacy[MAX_RENDERED_OBJECTS];
		const int legacy_count = render_collect_view_objects(0, legacy);
		require(legacy_count == int(folded.size()) && std::equal(folded.begin(), folded.end(), legacy),
		        "independent native visual collector matches the calculated overflow order");
		short canonical_overflow[MAX_RENDERED_OBJECTS];
		const int overflow_count = render_collect_gameplay_objects(canonical_overflow);
		require(overflow_count == legacy_count && std::equal(folded.begin(), folded.end(), canonical_overflow),
		        "gameplay collector retains the exact native overflow candidates and order");

		// Stress native mixed-row sorting, migration, the D1 49-object sort limit
		// and the historical 50-candidate upper-half overwrite rule
		for (const int density : { 8, 48, 64, 96, 160 }) {
			init_test_corridor(3);
			Viewer = ConsoleObject = &Objects[0];
			ConsoleObject->type = OBJ_PLAYER;
			ConsoleObject->orient = identity;
			for (int i = 0; i < density; ++i) {
				const int segment = density >= 64 ? 0 : i % 3;
				vms_vector position;
				compute_segment_center(&position, &Segments[segment]);
				position.x += (i % 5 - 2) * F1_0 / 4;
				position.z += (i % 7 - 3) * F1_0;
				const auto type = i % 4 == 0 ? OBJ_FIREBALL : i % 4 == 1 ? OBJ_WEAPON
				                                                         : OBJ_ROBOT;
				require(obj_create(type, 0, segment, &position, &identity, (i % 3 + 1) * 4 * F1_0,
				                   type == OBJ_WEAPON ? CT_WEAPON : CT_NONE, MT_NONE, RT_NONE) > 0,
				        "create dense gameplay candidate scene");
			}
			std::vector<short> expected_dense;
			for (const int classic : { 0, 1 })
				for (const int depth : { 1, 20, MAX_RENDER_SEGS }) {
					GameCfg.ClassicDepth = classic;
					Render_depth = depth;
					short candidates[MAX_RENDERED_OBJECTS + 1];
					candidates[MAX_RENDERED_OBJECTS] = 12345;
					const int count = render_collect_gameplay_objects(candidates);
					require(count >= 0 && count <= MAX_RENDERED_OBJECTS && candidates[MAX_RENDERED_OBJECTS] == 12345,
					        "dense gameplay collection stays within the native candidate capacity");
					const std::vector<short> actual(candidates, candidates + count);
					if (expected_dense.empty()) expected_dense = actual;
					require(actual == expected_dense, "mixed dense ordering and overflow ignore visual ClassicDepth and transient depth");
				}
		}
		// The newly restored transverse warning actually reaches native/imported
		// avoidance logic. Stale signatures must still suppress that response.
		vms_vector expected_evade = {};
		unsigned expected_rng = 0;
		const fix old_frame = FrameTime;
		const int old_tick = d_tick_count;
		FrameTime = F1_0 / 64;
		d_tick_count = 0;
		for (const int rear : { 0, 1 })
			for (const int stale : { 0, 1 }) {
				object &robot = create_frame_robot(AIB_NORMAL, 40 * F1_0);
				ConsoleObject->pos = origin;
				ConsoleObject->orient = identity;
				robot.pos = { 6 * F1_0, 0, 40 * F1_0 };
				robot.orient.rvec.x = robot.orient.fvec.z = -F1_0;
				robot.render_type = RT_POLYOBJ;
				robot.shields = Robot_info[0].strength = F1_0;
				Robot_info[0].field_of_view[Difficulty_level] = 0;
				Robot_info[0].evade_speed[Difficulty_level] = 2;
				Ai_local_info[&robot - Objects].mode = AIM_STILL;
				Ai_local_info[&robot - Objects].previous_visibility = 2;
				robot.ctype.ai_info.CURRENT_STATE = robot.ctype.ai_info.GOAL_STATE = AIS_LOCK;
				// Keep the approaching projectile off the robot-to-player visibility ray
				vms_vector shot_pos = { 0, 3 * F1_0, 0 };
				const int shot = obj_create(OBJ_WEAPON, 0, 0, &shot_pos, &identity, F1_0 / 8, CT_WEAPON, MT_PHYSICS, RT_NONE);
				Objects[shot].mtype.phys_info.velocity = { 0, 0, F1_0 };
				Player_fired_laser_this_frame = shot;
				Rear_view = rear;
				Viewer = &robot;
				warn();
				require(robot.ctype.ai_info.danger_laser_num == shot && robot.ctype.ai_info.danger_laser_signature == Objects[shot].signature,
				        "six-unit transverse robot receives the restored warning and live signature");
				robot.ctype.ai_info.danger_laser_signature += stale;
				robot.render_type = RT_NONE; // Avoid asset-only animation in this geometry fixture.
				Believed_player_pos = ConsoleObject->pos;
				Ai_cloak_info[(&robot - Objects) & (MAX_AI_CLOAK_INFO - 1)].last_position = ConsoleObject->pos;
				d_srand(19);
				const unsigned before_rng = d_rand_get_call_count();
				do_ai_frame(&robot);
				std::fprintf(stderr, "warning evade profile=%d rear=%d stale=%d velocity=%d,%d,%d visibility=%d mode=%d danger=%d\n", profile, rear, stale, robot.mtype.phys_info.velocity.x, robot.mtype.phys_info.velocity.y, robot.mtype.phys_info.velocity.z, Ai_local_info[&robot - Objects].previous_visibility, Ai_local_info[&robot - Objects].mode, robot.ctype.ai_info.danger_laser_num);
				require((vm_vec_mag_quick(&robot.mtype.phys_info.velocity) != 0) == !stale,
				        "actual native/imported robot frame evades the restored warning but rejects a stale signature");
				if (!stale) {
					if (!rear) {
						expected_evade = robot.mtype.phys_info.velocity;
						expected_rng = d_rand_get_call_count() - before_rng;
					}
					require(!std::memcmp(&expected_evade, &robot.mtype.phys_info.velocity, sizeof(expected_evade)) && d_rand_get_call_count() - before_rng == expected_rng,
					        "rear camera leaves actual avoidance velocity and simulation RNG consumption unchanged");
				}
			}
		FrameTime = old_frame;
		d_tick_count = old_tick;
		Player_fired_laser_this_frame = -1;
	}
	grd_curcanv = saved_canvas;
	grd_curscreen = saved_screen;
	Rear_view = saved_rear;
	Render_depth = saved_depth;
	GameCfg.ClassicDepth = saved_classic;
	cheats.acid = saved_acid;
	PlayerCfg.CurrentCockpitMode = saved_cockpit;
	Render_zoom = saved_zoom;
	Viewer = ConsoleObject;
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = saved_mission;
#endif
	std::fprintf(stderr, "PASS: %d fixed-view aspect/cockpit/FoV/rear/depth combinations plus boundaries and lifecycle\n", checks);
}
