// Exercise cache ownership through real public-level input, simulation and save paths
extern "C" {
void do_weapon_n_item_stuff(void);
void calc_game_time(void);
void GameProcessFrame(void);
void InitPlayerPosition(int random_flag);
void reset_player_object(void);
void move_player_2_segment(segment *seg, int side);
#ifdef DXX_BUILD_DESCENT_II
void set_pos_from_return_segment(void);
#endif
}

static void test_gameplay_view_lifecycle()
{
	using nlohmann::json;
	require(Game_wind && ConsoleObject && ConsoleObject->type == OBJ_PLAYER,
	        "lifecycle fixture starts through the public game's ordinary level setup");
	require(Newdemo_state == ND_STATE_NORMAL && !input_demo_replay_is_loaded() && !input_demo_recorder_is_active(),
	        "lifecycle fixture exercises live gameplay without replay or recording compensation");
	GameArg.SysInputDemoNoRender = 1;
	FrameTime = F1_0 / 30;
	Controls = {};
	kc_set_controls();
	event_flush();
	const auto collect = []() {
		short candidates[MAX_RENDERED_OBJECTS];
		const int count = render_collect_gameplay_objects(candidates);
		require(count > 0, "public mine has nonempty forward gameplay candidates");
		return std::vector<short>(candidates, candidates + count);
	};
	// Inspect the cache directly: the getter's lazy fallback would conceal missing hooks
	const auto expect_cached = [](const std::vector<short> &expected, const char *message) {
		require(Gameplay_num_rendered_objects == static_cast<int>(expected.size()) &&
		            std::equal(expected.begin(), expected.end(), Gameplay_rendered_object_list),
		        message);
	};
	const auto dispatch_secondary = [](bool pressed) {
		SDL_Event event = {};
		event.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
		event.key.state = pressed ? SDL_PRESSED : SDL_RELEASED;
		event.key.keysym.sym = SDLK_SPACE;
		require(SDL_PushEvent(&event) == 0, "queue ordinary secondary input for lifecycle regression");
		event_poll();
	};
	json checks = json::array();
	require(Gameplay_num_rendered_objects == -1, "ordinary new-level setup invalidates preceding-world candidates");
	// Select real neighboring rooms, rather than a spawn where every view may
	// contain only the player. No mine geometry or target robot is synthesized
	int view_segment = -1, view_robot = -1;
	vms_vector view_position = {};
	vms_matrix forward_orientation = {}, reverse_orientation = {};
	std::vector<short> reverse_candidates;
	for (int robot = 0; robot <= Highest_object_index && view_robot < 0; ++robot) {
		if (Objects[robot].type != OBJ_ROBOT) continue;
		segment &room = Segments[Objects[robot].segnum];
		for (int side = 0; side < 6 && view_robot < 0; ++side) {
			const int neighbor = room.children[side];
			if (neighbor < 0 || !(WALL_IS_DOORWAY(&room, side) & WID_RENDPAST_FLAG)) continue;
			compute_segment_center(&ConsoleObject->pos, &Segments[neighbor]);
			obj_relink(static_cast<int>(ConsoleObject - Objects), neighbor);
			vms_vector direction;
			vm_vec_normalized_dir(&direction, &Objects[robot].pos, &ConsoleObject->pos);
			vm_vector_2_matrix(&forward_orientation, &direction, nullptr, nullptr);
			ConsoleObject->orient = forward_orientation;
			const auto front = collect();
			reverse_orientation = forward_orientation;
			vm_vec_negate(&reverse_orientation.rvec);
			vm_vec_negate(&reverse_orientation.fvec);
			ConsoleObject->orient = reverse_orientation;
			const auto back = collect();
			if (std::find(front.begin(), front.end(), robot) == front.end() ||
			    std::find(back.begin(), back.end(), robot) != back.end()) continue;
			view_robot = robot;
			view_segment = neighbor;
			view_position = ConsoleObject->pos;
			reverse_candidates = back;
		}
	}
	require(view_robot >= 0, "public mine contains a real robot visible forward but absent from reverse candidates");
	const auto set_test_pose = [&](bool reverse) {
		ConsoleObject->pos = ConsoleObject->last_pos = view_position;
		ConsoleObject->orient = reverse ? reverse_orientation : forward_orientation;
		obj_relink(static_cast<int>(ConsoleObject - Objects), view_segment);
	};
	set_test_pose(false);
	const auto initial = collect();
	require(initial != reverse_candidates, "lifecycle fixture proves forward and reverse candidate sets differ");
	Game_suspended |= SUSP_ROBOTS;
	require(Gameplay_num_rendered_objects == -1, "an independent candidate query does not initialize the cache");
	require(PlayerCfg.KeySettings[0][26] == KEY_SPACEBAR && Players[0].secondary_weapon == CONCUSSION_INDEX,
	        "fresh public player uses the ordinary secondary binding and weapon");
	const int initial_ammo = Players[0].secondary_ammo[CONCUSSION_INDEX];
	const auto initial_time = GameTime64;
	std::vector<int> prior_signatures(MAX_OBJECTS, -1);
	for (int i = 0; i < MAX_OBJECTS; ++i)
		if (Objects[i].type == OBJ_WEAPON) prior_signatures[i] = Objects[i].signature;
	Next_missile_fire_time = GameTime64;
	dispatch_secondary(true);
	require(GameTime64 == initial_time && Players[0].secondary_ammo[CONCUSSION_INDEX] == initial_ammo - 1,
	        "real SDL secondary input fires before any simulation step");
	expect_cached(initial, "first input initializes the cache before creating the secondary projectile");
	dispatch_secondary(false);
	checks.push_back("SDL secondary input initializes before simulation");
	// The missile proves the dispatch boundary, but must not kill our real
	// discriminator robot while the following test measures camera rotation
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type == OBJ_WEAPON && Objects[i].signature != prior_signatures[i]) obj_delete(i);

	// Keep a valid prior cache, then turn using actual simulation until its
	// candidates differ. A collection moved to before simulation must fail here
	bool simulation_changed_view = false;
	int rotation_frames = 0;
	for (; rotation_frames < 180 && !simulation_changed_view; ++rotation_frames) {
		const auto before_step = collect();
		Controls = {};
		Controls.heading_time = FrameTime;
		calc_game_time();
		GameProcessFrame();
		const auto after_step = collect();
		expect_cached(after_step, "real GameProcessFrame publishes post-simulation candidates without drawing");
		simulation_changed_view = before_step != after_step;
	}
	require(simulation_changed_view, "real yaw simulation crosses a gameplay candidate boundary");
	checks.push_back("GameProcessFrame refreshes without rendering");

	set_test_pose(true);
	render_update_gameplay_view_objects();
	const auto before_teleport = collect();
	int teleport_side = -1;
	for (int side = 0; side < 6; ++side)
		if (Segments[view_segment].children[side] == Objects[view_robot].segnum) teleport_side = side;
	require(teleport_side >= 0, "native teleport discriminator uses the selected real portal");
	move_player_2_segment(&Segments[view_segment], teleport_side);
	require(Gameplay_num_rendered_objects == -1, "explicit native segment teleport invalidates its warm previous-pose cache");
	const auto teleport_view = collect();
	require(teleport_view != before_teleport && std::find(teleport_view.begin(), teleport_view.end(), view_robot) != teleport_view.end(),
	        "native teleport really changes the eligible target set before first input");
	Controls = {};
	do_weapon_n_item_stuff();
	expect_cached(teleport_view, "first input after an explicit native teleport uses its new pose");
	checks.push_back("native segment teleport and first input");

	Gameplay_num_rendered_objects = 0;
	reset_player_object();
	require(Gameplay_num_rendered_objects == -1, "real player-object reset invalidates the cache");
	const auto reset_view = collect();
	Controls = {};
	do_weapon_n_item_stuff();
	expect_cached(reset_view, "first ordinary input after player reset initializes current candidates");
	checks.push_back("player reset and first input");

	Gameplay_num_rendered_objects = 0;
	InitPlayerPosition(0);
	require(Gameplay_num_rendered_objects == -1, "ordinary respawn positioning invalidates prior-pose candidates");
	const auto respawn_view = collect();
	do_weapon_n_item_stuff();
	expect_cached(respawn_view, "first input after respawn uses the respawn pose");
	checks.push_back("respawn positioning and first input");

#ifdef DXX_BUILD_DESCENT_II
	const int prior_return_segment = Secret_return_segment;
	const auto prior_return_orientation = Secret_return_orient;
	Secret_return_segment = ConsoleObject->segnum;
	Secret_return_orient = ConsoleObject->orient;
	Gameplay_num_rendered_objects = 0;
	set_pos_from_return_segment();
	require(Gameplay_num_rendered_objects == -1, "native secret-return positioning invalidates prior-pose candidates");
	const auto secret_return_view = collect();
	do_weapon_n_item_stuff();
	expect_cached(secret_return_view, "first input after secret-return positioning uses the returned pose");
	Secret_return_segment = prior_return_segment;
	Secret_return_orient = prior_return_orientation;
	checks.push_back("secret-return positioning and first input");
#endif

	Gameplay_num_rendered_objects = 0;
	special_reset_objects();
	require(Gameplay_num_rendered_objects == -1, "object free-list reconstruction invalidates candidate indices");
	do_weapon_n_item_stuff();
	expect_cached(collect(), "input after object reconstruction uses the reconstructed world");
	checks.push_back("object free-list reconstruction");

	char filename[] = "gameplay-view-lifecycle.sav";
	char description[21] = "Gameplay view cache";
	set_test_pose(false);
	const auto saved_candidates = collect();
	require(saved_candidates == initial, "checkpoint pose retains the original real-robot discriminator");
	render_update_gameplay_view_objects();
	const auto saved_pose = ConsoleObject->pos;
	const auto saved_orientation = ConsoleObject->orient;
	stop_time();
	require(state_save_all_sub(filename, description), "write a real public-level lifecycle checkpoint");
	// Give restore a valid stale cache and a different pose, so keeping the old
	// cache or rebuilding before the saved player pose is installed is detectable
	set_test_pose(true);
	render_update_gameplay_view_objects();
	const auto before_restore = collect();
	require(before_restore != saved_candidates, "native restore starts with a demonstrably different warm candidate cache");
	expect_cached(before_restore, "opposite-pose cache is populated before native restore");
#ifdef DXX_BUILD_DESCENT_II
	require(state_restore_all_sub(filename, 0), "restore the real D2-engine lifecycle checkpoint");
#else
	require(state_restore_all_sub(filename), "restore the real native D1 lifecycle checkpoint");
#endif
	require(!std::memcmp(&ConsoleObject->pos, &saved_pose, sizeof(saved_pose)) &&
	            !std::memcmp(&ConsoleObject->orient, &saved_orientation, sizeof(saved_orientation)),
	        "native restore installs the saved ship pose before first input");
	require(Gameplay_num_rendered_objects == -1, "native save restore invalidates the preceding-world cache");
	Controls = {};
	const auto restored_view = collect();
	require(restored_view == saved_candidates, "restored world recovers the saved discriminating candidate list");
	do_weapon_n_item_stuff();
	expect_cached(restored_view, "first input after native restore initializes the restored world without drawing");
	checks.push_back("native save restore and first input");

	// Directly exercise the two destructive reset hooks only after the save is
	// available, then reload through the native loader instead of repairing a mock
	Gameplay_num_rendered_objects = 0;
	reset_objects(1);
	require(Gameplay_num_rendered_objects == -1, "real reset_objects invalidates old object-slot candidates");
	Gameplay_num_rendered_objects = 0;
	init_objects();
	require(Gameplay_num_rendered_objects == -1, "real init_objects invalidates old-world candidates");
#ifdef DXX_BUILD_DESCENT_II
	require(state_restore_all_sub(filename, 0), "reload the public world after destructive reset-hook checks");
#else
	require(state_restore_all_sub(filename), "reload the public world after destructive reset-hook checks");
#endif
	Controls = {};
	do_weapon_n_item_stuff();
	expect_cached(collect(), "native reload recovers candidates after destructive object reset");
	checks.push_back("destructive object resets and native reload");
	const json report = { { "checks", checks }, { "view_segment", view_segment }, { "view_robot", view_robot }, { "initial_candidates", initial }, { "reverse_candidates", reverse_candidates }, { "rotation_frames", rotation_frames }, { "before_restore_candidates", before_restore }, { "restored_candidates", restored_view },
#ifdef DXX_BUILD_DESCENT_II
		                  { "profile", d1_in_d2_use_d1_gameplay() ? "imported-d1" : "native-d2" }
#else
		                  { "profile", "native-d1" }
#endif
	};
	const std::string output = report.dump(2) + "\n";
	write_fixture("gameplay-view-lifecycle.json", bytes(output.begin(), output.end()));
	std::fprintf(stderr, "PASS: %zu public gameplay-view lifecycle checks\n", checks.size());
}
