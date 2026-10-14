// Isolated native helpers; synthetic state is never a public replay capture
#ifdef DXX_BUILD_DESCENT_II
extern "C" {
extern int weapon_box_user[2];
void show_extra_views(void);
}
#endif

static nlohmann::json test_native_missile_view_helpers_recording_off()
{
#ifndef DXX_BUILD_DESCENT_II
	return { { "supported", false } };
#else
	using nlohmann::json;
	require(!input_demo_recorder_is_active() && Newdemo_state == ND_STATE_NORMAL,
	        "missile view helpers run with recording disabled");
	const auto saved_cfg = PlayerCfg;
	const auto saved_args = GameArg;
	const int saved_game_mode = Game_mode, saved_players = N_players;
	const fix64 saved_graph_until = Show_graph_until;
	object *const saved_missile = Missile_viewer;
	const int saved_signature = Missile_viewer_sig;
	grs_screen *const saved_screen = grd_curscreen;
	grs_canvas *const saved_canvas = grd_curcanv;
	object *const saved_viewer = Viewer;
	const int saved_rear = Rear_view, saved_camera = Ai_last_missile_camera;
	const fix64 saved_time = GameTime64, saved_clock = timer_query();
	const fix saved_frame_time = FrameTime;
	const auto saved_frame = game_get_simulation_frame_id();
	const unsigned sim_calls = d_rand_get_call_count(), fx_calls = d_rand_get_stream_call_count(D_RNG_FX);
	unsigned sim_state = 0, fx_state = 0;
	const int has_sim = d_rand_get_state(&sim_state), has_fx = d_rand_get_stream_state(D_RNG_FX, &fx_state);
	const std::vector<object> saved_objects(Objects, Objects + Highest_object_index + 1);
	const std::vector<ai_local> saved_ai(Ai_local_info, Ai_local_info + Highest_object_index + 1);
	const std::vector<window_rendered_data> saved_candidates(Window_rendered_data, Window_rendered_data + MAX_RENDERED_WINDOWS);
	const int saved_box_users[] = { weapon_box_user[0], weapon_box_user[1] };
	json selectors = json::array(), geometry = json::array();
	object candidate = {};
	candidate.signature = 42;
	for (const int enabled : { 0, 1 })
		for (const int type : { OBJ_NONE, OBJ_WEAPON, OBJ_ROBOT })
			for (const int signature : { -1, 42, 43 }) {
				PlayerCfg.MissileViewEnabled = enabled;
				candidate.type = type;
				Missile_viewer = &candidate;
				Missile_viewer_sig = signature;
				const bool selected = enabled && type != OBJ_NONE && signature != 43;
				require(select_missile_viewer() == (selected ? &candidate : nullptr) &&
				            Missile_viewer == (selected ? &candidate : nullptr) &&
				            Missile_viewer_sig == (selected ? 42 : -1),
				        "non-guided selector preserves exact native enabled/type/signature lifecycle");
				selectors.push_back({ { "enabled", enabled }, { "type", type }, { "signature", signature }, { "selected", selected } });
			}
	Missile_viewer = nullptr;
	Missile_viewer_sig = 43;
	require(!select_missile_viewer() && Missile_viewer_sig == 43, "null native viewer does not rewrite a dormant signature");

	// Native D2 gauge rectangles, including their inclusive right/bottom edges
	const int boxes[2][2][2][4] = {
		{ { { 63, 151, 59, 43 }, { 202, 151, 62, 43 } }, { { 34, 153, 56, 44 }, { 169, 153, 56, 44 } } },
		{ { { 121, 364, 122, 107 }, { 404, 363, 126, 107 } }, { { 68, 381, 112, 93 }, { 338, 381, 112, 93 } } }
	};
	for (const auto dimensions : { std::pair<int, int>{ 320, 200 }, { 640, 480 }, { 1280, 720 }, { 480, 640 } }) {
		grs_screen screen = {};
		std::vector<ubyte> pixels(dimensions.first * dimensions.second);
		screen.sc_w = dimensions.first;
		screen.sc_h = dimensions.second;
		gr_init_canvas(&screen.sc_canvas, pixels.data(), BM_LINEAR, screen.sc_w, screen.sc_h);
		grd_curscreen = &screen;
		for (const int hires_available : { 0, 1 }) {
			GameArg.GfxHiresGFXAvailable = hires_available;
			const int hires = HIRESMODE != 0;
			const auto scale_x = [&](int value) {
#ifdef OGL
				return int(double(value) * screen.sc_w / (hires ? 640 : 320) + 0.5);
#else
				return value;
#endif
			};
			const auto scale_y = [&](int value) {
#ifdef OGL
				return int(double(value) * screen.sc_h / (hires ? 480 : 200) + 0.5);
#else
				return value;
#endif
			};
			for (const int mode : { CM_FULL_SCREEN, CM_FULL_COCKPIT, CM_STATUS_BAR, CM_REAR_VIEW, CM_LETTERBOX })
				for (const int win : { 0, 1 })
					for (const int graph : { 0, 1, 2 }) {
						PlayerCfg.CurrentCockpitMode = mode;
						Game_mode = graph ? GM_OBSERVER | GM_NETWORK : 0;
						N_players = 2;
						PlayerCfg.ObsShowKillGraph[0] = 1;
						Show_graph_until = GameTime64 + (graph == 1 ? F1_0 : 0);
						grs_canvas canvas = {};
						int x = -987, y = -654;
						const bool drawable = mode != CM_REAR_VIEW && mode != CM_LETTERBOX && !(mode == CM_FULL_SCREEN && graph == 1);
						require(cockpit_window_view_canvas(win, &canvas, &x, &y) == drawable,
						        "native cockpit modes and observer kill graph retain exact draw eligibility");
						if (drawable) {
							int expected_x, expected_y, expected_w, expected_h;
							grs_canvas imported = {};
							if (mode == CM_FULL_SCREEN) {
								const int size = hires ? 106 : 44;
								expected_w = scale_x(100) > scale_y(100) ? scale_y(size) : scale_x(size);
								expected_h = scale_y(100) > scale_x(100) ? scale_x(size) : scale_y(size);
								expected_x = screen.sc_w / 2 + (win ? expected_w / 10 : -(expected_w + expected_w / 10));
								expected_y = screen.sc_h - expected_h - screen.sc_h / 15;
								require(x == expected_x && y == expected_y, "full-screen overlap offsets retain native arithmetic");
							} else if (d1_in_d2_cockpit_window_canvas(win, &imported)) {
								expected_x = imported.cv_bitmap.bm_x;
								expected_y = imported.cv_bitmap.bm_y;
								expected_w = imported.cv_bitmap.bm_w;
								expected_h = imported.cv_bitmap.bm_h;
							} else {
								const int *box = boxes[hires][mode == CM_STATUS_BAR][win];
								expected_x = scale_x(box[0]);
								expected_y = scale_y(box[1]);
								expected_w = scale_x(box[2]);
								expected_h = scale_y(box[3]);
							}
							require(canvas.cv_bitmap.bm_x == expected_x && canvas.cv_bitmap.bm_y == expected_y &&
							            canvas.cv_bitmap.bm_w == expected_w && canvas.cv_bitmap.bm_h == expected_h,
							        "extracted cockpit canvas retains native hires, aspect scaling and gauge rectangles");
						} else {
							const grs_canvas untouched = {};
							require(!std::memcmp(&canvas, &untouched, sizeof(canvas)) && x == -987 && y == -654,
							        "rejected geometry does not modify the output canvas or overlap offsets");
						}
						if (mode != CM_FULL_SCREEN)
							require(x == -987 && y == -654, "non-full-screen geometry leaves overlap offsets untouched");
						// Retirement is independent of whether this mode can display a subview
						candidate.type = OBJ_WEAPON;
						PlayerCfg.MissileViewEnabled = 1;
						Missile_viewer = &candidate;
						Missile_viewer_sig = 43;
						require(!select_missile_viewer() && !Missile_viewer && Missile_viewer_sig == -1,
						        "stale missile cleanup still occurs without a drawable window");
						geometry.push_back({ { "width", screen.sc_w }, { "height", screen.sc_h }, { "hires", hires }, { "mode", mode }, { "window", win }, { "graph", graph }, { "drawable", drawable } });
					}
		}
	}
	grd_curscreen = saved_screen;
	PlayerCfg = saved_cfg;
	GameArg = saved_args;
	Game_mode = saved_game_mode;
	N_players = saved_players;
	Show_graph_until = saved_graph_until;
	Missile_viewer = saved_missile;
	Missile_viewer_sig = saved_signature;
	unsigned after_sim = 0, after_fx = 0;
	require(has_sim == d_rand_get_state(&after_sim) && has_fx == d_rand_get_stream_state(D_RNG_FX, &after_fx) &&
	            sim_state == after_sim && fx_state == after_fx && sim_calls == d_rand_get_call_count() &&
	            fx_calls == d_rand_get_stream_call_count(D_RNG_FX),
	        "native selector and geometry consume neither RNG stream");
	require(GameTime64 == saved_time && FrameTime == saved_frame_time && timer_query() == saved_clock &&
	            game_get_simulation_frame_id() == saved_frame,
	        "native selector and geometry never advance any clock");
	require(!std::memcmp(saved_objects.data(), Objects, saved_objects.size() * sizeof(object)) &&
	            !std::memcmp(saved_ai.data(), Ai_local_info, saved_ai.size() * sizeof(ai_local)) && Ai_last_missile_camera == saved_camera,
	        "native selector and geometry do not mutate objects or wake camera robots");
	require(!std::memcmp(saved_candidates.data(), Window_rendered_data, saved_candidates.size() * sizeof(window_rendered_data)) &&
	            Viewer == saved_viewer && Rear_view == saved_rear && grd_curcanv == saved_canvas &&
	            weapon_box_user[0] == saved_box_users[0] && weapon_box_user[1] == saved_box_users[1],
	        "native selector and geometry preserve candidate caches, current view, canvas and gauge users");
	require(!input_demo_recorder_is_active() && input_demo_recorder_pending_input_operation_count() == 0,
	        "native selector and geometry create no recording state");
	return { { "supported", true }, { "recording", false }, { "public_replay_admission", false }, { "selectors", selectors }, { "geometry", geometry } };
#endif
}

// This renderer oracle deliberately allocates and positions a synthetic camera
// It runs only in the isolated recording-off scenario, never a replay capture
static nlohmann::json test_native_missile_view_map_recording_off()
{
#ifndef DXX_BUILD_DESCENT_II
	return { { "supported", false } };
#else
	using nlohmann::json;
	require(!input_demo_recorder_is_active() && Newdemo_state == ND_STATE_NORMAL && !Automap_active && !Endlevel_sequence,
	        "synthetic missile map oracle is outside recording and modal/endlevel rendering");
	const auto saved_cfg = PlayerCfg;
	const auto saved_game_cfg = GameCfg;
	const int saved_mode = Game_mode, saved_rear = Rear_view;
	const grs_canvas saved_main = Screen_3d_window;
	grs_canvas *const saved_canvas = grd_curcanv;
	object *const saved_viewer = Viewer, *const saved_missile = Missile_viewer, *const saved_guided = Guided_missile[Player_num];
	const int saved_signature = Missile_viewer_sig, saved_guided_signature = Guided_missile_sig[Player_num];
	const std::vector<ubyte> saved_map(Automap_visited, Automap_visited + MAX_SEGMENTS);
	const std::vector<window_rendered_data> original_candidates(Window_rendered_data, Window_rendered_data + MAX_RENDERED_WINDOWS);
	PlayerCfg.CurrentCockpitMode = CM_FULL_SCREEN;
	PlayerCfg.Cockpit3DView[0] = PlayerCfg.Cockpit3DView[1] = CV_NONE;
	PlayerCfg.MissileViewEnabled = 1;
	Game_mode = 0;
	Rear_view = 0;
	Viewer = ConsoleObject;
	Guided_missile[Player_num] = nullptr;
	Missile_viewer = nullptr;
	gr_init_sub_canvas(&Screen_3d_window, &grd_curscreen->sc_canvas, 0, 0, SWIDTH, SHEIGHT);
	std::fill_n(Automap_visited, MAX_SEGMENTS, 0);
	gr_set_current_canvas(&Screen_3d_window);
	render_frame(0, 0);
	const std::vector<ubyte> main_drawn(Automap_visited, Automap_visited + MAX_SEGMENTS);

	// Find a portal-connected camera position that the actual main draw misses
	std::vector<int> parent(Highest_segment_index + 1, -1), queue{ ConsoleObject->segnum };
	parent[ConsoleObject->segnum] = ConsoleObject->segnum;
	for (size_t row = 0; row < queue.size(); ++row)
		for (int side = 0; side < 6; ++side) {
			const int segment = queue[row], child = Segments[segment].children[side];
			if (child >= 0 && parent[child] == -1 && (WALL_IS_DOORWAY(&Segments[segment], side) & WID_RENDPAST_FLAG)) {
				parent[child] = segment;
				queue.push_back(child);
			}
		}
	const int camera = obj_create(OBJ_WEAPON, CONCUSSION_ID, ConsoleObject->segnum, &ConsoleObject->pos,
	                              &ConsoleObject->orient, F1_0 / 4, CT_WEAPON, MT_NONE, RT_NONE);
	require(camera >= 0, "allocate isolated native missile map camera");
	Objects[camera].ctype.laser_info.parent_type = OBJ_PLAYER;
	Objects[camera].ctype.laser_info.parent_num = int(ConsoleObject - Objects);
	Objects[camera].ctype.laser_info.parent_signature = ConsoleObject->signature;
	int chosen = -1;
	bool around_corner = false;
	for (const int segment : queue) {
		if (main_drawn[segment]) continue;
		compute_segment_center(&Objects[camera].pos, &Segments[segment]);
		obj_relink(camera, segment);
		const bool blocked = !object_to_object_visibility(ConsoleObject, &Objects[camera], 0);
		if (chosen == -1 || blocked) chosen = segment;
		if (blocked) {
			around_corner = true;
			break;
		}
	}
	require(chosen >= 0, "actual main view misses a portal-connected missile segment");
	compute_segment_center(&Objects[camera].pos, &Segments[chosen]);
	obj_relink(camera, chosen);
	vms_vector previous, forward;
	compute_segment_center(&previous, &Segments[parent[chosen]]);
	vm_vec_normalized_dir(&forward, &Objects[camera].pos, &previous);
	vm_vector_2_matrix(&Objects[camera].orient, &forward, nullptr, nullptr);
	json cases = json::array();
	for (const int classic : { 0, 1 })
		for (const int mode : { CM_FULL_SCREEN, CM_FULL_COCKPIT, CM_STATUS_BAR, CM_REAR_VIEW, CM_LETTERBOX })
			for (const int enabled : { 0, 1 }) {
				GameCfg.ClassicDepth = classic;
				PlayerCfg.CurrentCockpitMode = mode;
				PlayerCfg.MissileViewEnabled = enabled;
				Missile_viewer = &Objects[camera];
				Missile_viewer_sig = -1;
				std::fill_n(Automap_visited, MAX_SEGMENTS, 0);
				gr_set_current_canvas(&Screen_3d_window);
				render_frame(0, 0);
				const std::vector<ubyte> main_only(Automap_visited, Automap_visited + MAX_SEGMENTS);
				show_extra_views();
				const std::vector<ubyte> drawn(Automap_visited, Automap_visited + MAX_SEGMENTS);
				object *const drawn_viewer = Missile_viewer;
				const int drawn_signature = Missile_viewer_sig;
				const bool drawable = enabled && mode != CM_REAR_VIEW && mode != CM_LETTERBOX;
				int extra_segments = 0;
				for (int segment = 0; segment <= Highest_segment_index; ++segment)
					extra_segments += drawn[segment] && !main_only[segment];
				require(drawable ? (!main_only[chosen] && drawn[chosen] && extra_segments > 0) : (drawn == main_only),
				        "native missile window adds unseen segments exactly when drawable and enabled");

				Missile_viewer = &Objects[camera];
				Missile_viewer_sig = -1;
				std::fill_n(Automap_visited, MAX_SEGMENTS, 0);
				gr_set_current_canvas(nullptr);
				grs_canvas *const cpu_canvas = grd_curcanv;
				const std::vector<object> objects(Objects, Objects + Highest_object_index + 1);
				const std::vector<ai_local> locals(Ai_local_info, Ai_local_info + Highest_object_index + 1);
				const std::vector<window_rendered_data> candidates(Window_rendered_data, Window_rendered_data + MAX_RENDERED_WINDOWS);
				const int ai_camera = Ai_last_missile_camera;
				const fix64 clock = timer_query(), game_time = GameTime64;
				const fix frame_time = FrameTime;
				const auto frame = game_get_simulation_frame_id();
				const auto sim_calls = d_rand_get_call_count(), fx_calls = d_rand_get_stream_call_count(D_RNG_FX);
				unsigned sim_state = 0, fx_state = 0;
				const int has_sim = d_rand_get_state(&sim_state), has_fx = d_rand_get_stream_state(D_RNG_FX, &fx_state);
				require(render_update_game_view_automap() && std::equal(drawn.begin(), drawn.end(), Automap_visited),
				        "combined CPU map discovery equals actual native main and missile renderer bytes");
				require(Missile_viewer == drawn_viewer && Missile_viewer_sig == drawn_signature,
				        "CPU map discovery preserves native selected missile retirement and signature adoption");
				require(!std::memcmp(objects.data(), Objects, objects.size() * sizeof(object)) &&
				            !std::memcmp(locals.data(), Ai_local_info, locals.size() * sizeof(ai_local)) && Ai_last_missile_camera == ai_camera,
				        "CPU map discovery cannot draw objects or wake missile camera robots");
				require(!std::memcmp(candidates.data(), Window_rendered_data, candidates.size() * sizeof(window_rendered_data)) &&
				            Viewer == ConsoleObject && Rear_view == 0 && grd_curcanv == cpu_canvas,
				        "CPU map discovery preserves all candidate bytes and restores native view and canvas");
				unsigned after_sim = 0, after_fx = 0;
				require(d_rand_get_state(&after_sim) == has_sim && d_rand_get_stream_state(D_RNG_FX, &after_fx) == has_fx &&
				            after_sim == sim_state && after_fx == fx_state && d_rand_get_call_count() == sim_calls &&
				            d_rand_get_stream_call_count(D_RNG_FX) == fx_calls,
				        "CPU missile map discovery consumes neither RNG stream");
				require(timer_query() == clock && GameTime64 == game_time && FrameTime == frame_time &&
				            game_get_simulation_frame_id() == frame,
				        "CPU missile map discovery advances no clock");
				std::fill_n(Automap_visited, MAX_SEGMENTS, 0);
				require(render_update_main_view_automap() && std::equal(main_only.begin(), main_only.end(), Automap_visited),
				        "main-only CPU oracle remains exact and demonstrably misses auxiliary discoveries");
				cases.push_back({ { "classic_depth", classic }, { "cockpit", mode }, { "enabled", enabled }, { "extra_segments", extra_segments }, { "combined_matches_renderer", true }, { "main_only_matches_renderer", !drawable } });
			}

	// The existing renderer gives the default missile priority over the right selection
	PlayerCfg.CurrentCockpitMode = CM_FULL_SCREEN;
	PlayerCfg.MissileViewEnabled = 1;
	PlayerCfg.Cockpit3DView[0] = CV_NONE;
	PlayerCfg.Cockpit3DView[1] = CV_REAR;
	Guided_missile[Player_num] = nullptr;
	Missile_viewer = &Objects[camera];
	for (const int signature : { -1, int(Objects[camera].signature) }) {
		Missile_viewer_sig = signature;
		require(!render_game_view_automap_unsupported_reason() && Missile_viewer == &Objects[camera] && Missile_viewer_sig == signature,
		        "read-only support guard preserves valid missile priority over a masked right auxiliary view");
	}
	json unsupported = json::array();
	for (const int kind : { 0, 1, 2, 3 }) {
		PlayerCfg.CurrentCockpitMode = CM_FULL_SCREEN;
		PlayerCfg.Cockpit3DView[0] = PlayerCfg.Cockpit3DView[1] = CV_NONE;
		PlayerCfg.GuidedInBigWindow = kind == 1;
		Missile_viewer = nullptr;
		Missile_viewer_sig = -1;
		Guided_missile[Player_num] = kind < 2 ? &Objects[camera] : nullptr;
		Guided_missile_sig[Player_num] = Objects[camera].signature;
		Objects[camera].id = GUIDEDMISS_ID;
		if (kind >= 2) PlayerCfg.Cockpit3DView[kind - 2] = kind == 2 ? CV_REAR : CV_ESCORT;
		const char *reason = render_game_view_automap_unsupported_reason();
		require(reason && std::strstr(reason, kind < 2 ? "guided missile camera" : "auxiliary cockpit camera"),
		        "unqualified guided and selected subviews have explicit support diagnostics");
		const auto cfg = PlayerCfg;
		const std::vector<object> objects(Objects, Objects + Highest_object_index + 1);
		const std::vector<ai_local> locals(Ai_local_info, Ai_local_info + Highest_object_index + 1);
		const std::vector<window_rendered_data> candidates(Window_rendered_data, Window_rendered_data + MAX_RENDERED_WINDOWS);
		const std::vector<ubyte> map(Automap_visited, Automap_visited + MAX_SEGMENTS);
		const auto sim_calls = d_rand_get_call_count(), fx_calls = d_rand_get_stream_call_count(D_RNG_FX);
		unsigned sim_state = 0, fx_state = 0;
		const int has_sim = d_rand_get_state(&sim_state), has_fx = d_rand_get_stream_state(D_RNG_FX, &fx_state);
		object *const missile = Missile_viewer, *const guided = Guided_missile[Player_num];
		const int missile_signature = Missile_viewer_sig, classic_state = Newdemo_state;
		const fix64 time = GameTime64, clock = timer_query();
		input_demo_recorder_settings settings;
		input_demo_recorder_settings_clear(&settings);
		settings.game = INPUT_DEMO_GAME_D2;
		settings.mission = "d2demo";
		settings.level = 1;
		settings.difficulty = Difficulty_level;
		settings.rng_mode = "lcg_state";
		settings.ordered_input = 1;
		char error[256] = "";
		require(input_demo_recorder_start(&settings, error, sizeof(error)), error);
		input_demo_record_input_context();
		require(!input_demo_recorder_is_active() && !render_update_game_view_automap(),
		        "unqualified subviews stop companion input capture instead of silently approximating discovery");
		unsigned after_sim = 0, after_fx = 0;
		require(has_sim == d_rand_get_state(&after_sim) && has_fx == d_rand_get_stream_state(D_RNG_FX, &after_fx) &&
		            sim_state == after_sim && fx_state == after_fx && sim_calls == d_rand_get_call_count() &&
		            fx_calls == d_rand_get_stream_call_count(D_RNG_FX),
		        "unsupported-view diagnostics preserve both RNG streams");
		require(!std::memcmp(objects.data(), Objects, objects.size() * sizeof(object)) &&
		            !std::memcmp(locals.data(), Ai_local_info, locals.size() * sizeof(ai_local)) &&
		            !std::memcmp(candidates.data(), Window_rendered_data, candidates.size() * sizeof(window_rendered_data)) &&
		            !std::memcmp(&cfg, &PlayerCfg, sizeof(cfg)) && std::equal(map.begin(), map.end(), Automap_visited) &&
		            Missile_viewer == missile && Missile_viewer_sig == missile_signature && Guided_missile[Player_num] == guided &&
		            Newdemo_state == classic_state && GameTime64 == time && timer_query() == clock,
		        "unsupported-view capture cancellation preserves the live camera, objects, AI, candidates, map, clocks and classic-demo state");
		unsupported.push_back({ { "kind", kind }, { "reason", reason }, { "capture_canceled", true }, { "live_state_unchanged", true } });
	}
	Objects[camera].id = CONCUSSION_ID;

	PlayerCfg = saved_cfg;
	GameCfg = saved_game_cfg;
	Game_mode = saved_mode;
	Rear_view = saved_rear;
	Screen_3d_window = saved_main;
	grd_curcanv = saved_canvas;
	Viewer = saved_viewer;
	Missile_viewer = saved_missile;
	Missile_viewer_sig = saved_signature;
	Guided_missile[Player_num] = saved_guided;
	Guided_missile_sig[Player_num] = saved_guided_signature;
	std::copy(saved_map.begin(), saved_map.end(), Automap_visited);
	std::copy(original_candidates.begin(), original_candidates.end(), Window_rendered_data);
	obj_delete(camera);
	require(!input_demo_recorder_is_active() && input_demo_recorder_pending_input_operation_count() == 0,
	        "synthetic missile map oracle never creates a public recording");
	return { { "supported", true }, { "recording", false }, { "public_replay_admission", false }, { "missile_segment", chosen }, { "main_segment", int(ConsoleObject->segnum) }, { "around_corner", around_corner }, { "cases", cases }, { "missile_override_supported", true }, { "unsupported", unsupported } };
#endif
}
