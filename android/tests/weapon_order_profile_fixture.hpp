static std::vector<ubyte> fixture_native_order(int secondary)
{
	const int count = secondary ? 6 : 7;
#ifdef DXX_BUILD_DESCENT_II
	const ubyte *order = d1_in_d2_weapon_order(secondary);
#else
	const ubyte *order = secondary ? PlayerCfg.SecondaryOrder : PlayerCfg.PrimaryOrder;
#endif
	return { order, order + count };
}

static int Weapon_order_menu_visits;
static void fixture_close_order_game(void)
{
	// The executable normally supplies this landing frame for game-window close
	if (Game_wind && !setjmp(LeaveEvents)) window_close(Game_wind);
}

static int weapon_order_menu_event(d_event *event)
{
	if (event->type != EVENT_IDLE) return 0;
	++Weapon_order_menu_visits;
	window *wind = window_get_front();
	require(wind != nullptr, "reorder menu has its actual window");
	struct {
		event_type type;
		int keycode;
	} key = { EVENT_KEY_COMMAND, KEY_SHIFTED + KEY_DOWN };
	for (int i = 0; i < 12; ++i)
		window_send_event(wind, reinterpret_cast<d_event *>(&key));
	d_event draw = { EVENT_WINDOW_DRAW };
	window_send_event(wind, &draw);
	key.keycode = KEY_ESC;
	window_send_event(wind, reinterpret_cast<d_event *>(&key));
	return 1;
}

// Real menu edits, pilot files, recorder output and replay startup share one order
static nlohmann::json exercise_weapon_order_profile(const char *native_demo, const char *d2_directory)
{
	new_player_config();
	Game_mode = 0;
	Player_num = Player_is_dead = 0;
	Newdemo_state = ND_STATE_NORMAL;
	key_init();
	mouse_init();
	std::strcpy(Players[0].callsign, "d1order");
	GameArg.SysUsePlayersDir = 0;
	const ubyte primary[] = { 16, 4, 0, 3, 2, 1, 255 };
	const ubyte secondary[] = { 2, 4, 3, 1, 0, 255 };
	fixture_native_primary_order(primary);
#ifdef DXX_BUILD_DESCENT_II
	require(d1_in_d2_set_weapon_order(1, secondary, 6), "set native secondary preference");
	const auto d2_cfg = PlayerCfg;
#else
	std::memcpy(PlayerCfg.SecondaryOrder, secondary, 6);
#endif
	std::vector<ubyte> expected_primary(primary, primary + 7), expected_secondary(secondary, secondary + 6);
	std::rotate(expected_primary.begin(), expected_primary.begin() + 1, expected_primary.end());
	std::rotate(expected_secondary.begin(), expected_secondary.begin() + 1, expected_secondary.end());
	Weapon_order_menu_visits = 0;
	set_default_handler(weapon_order_menu_event);
	ReorderPrimary();
	ReorderSecondary();
	set_default_handler(nullptr);
	require(Weapon_order_menu_visits == 2 && !window_get_front(), "both real reorder menus complete");
	std::fprintf(stderr, "Native weapon reorder menus passed\n");
	require(fixture_native_order(0) == expected_primary && fixture_native_order(1) == expected_secondary,
	        "menu moves the first item to the end of each native domain");
#ifdef DXX_BUILD_DESCENT_II
	require(!std::memcmp(d2_cfg.PrimaryOrder, PlayerCfg.PrimaryOrder, sizeof(PlayerCfg.PrimaryOrder)) &&
	            !std::memcmp(d2_cfg.SecondaryOrder, PlayerCfg.SecondaryOrder, sizeof(PlayerCfg.SecondaryOrder)),
	        "native menus leave D2 orders intact");
#endif
	require(write_player_file() == 0, "save both actual pilot files");
	new_player_config();
	require(read_player_file() == 0, "read actual pilot preferences");
	require(fixture_native_order(0) == expected_primary && fixture_native_order(1) == expected_secondary,
	        "native primary and secondary preferences survive pilot reload");
	std::fprintf(stderr, "Native weapon pilot reload passed\n");
#ifdef DXX_BUILD_DESCENT_II
	require(!std::memcmp(d2_cfg.PrimaryOrder, PlayerCfg.PrimaryOrder, sizeof(PlayerCfg.PrimaryOrder)) &&
	            !std::memcmp(d2_cfg.SecondaryOrder, PlayerCfg.SecondaryOrder, sizeof(PlayerCfg.SecondaryOrder)),
	        "binary D2 pilot orders survive native preference persistence");
	const auto valid = PlayerCfg.D1WeaponOrder;
	ubyte duplicate[] = { 16, 4, 0, 3, 2, 1, 1 };
	require(!d1_in_d2_set_weapon_order(0, duplicate, 7) && !std::memcmp(&valid, &PlayerCfg.D1WeaponOrder, sizeof(valid)),
	        "invalid native order cannot partially replace pilot preferences");
	const auto profile = read_save_fixture("d1order.plx");
	for (const char *section : {
	         "[d1 weapon order]\nprimary=16,4,0,3,2,1,1\nsecondary=2,4,3,1,0,16\n[end]\n",
	         "[d1 weapon order]\nprimary=16,4,0,3,2,1\nsecondary=2,4,3,1,0,255junk\n[end]\n",
	         "[D2X OPTIONS]\n[end]\n" }) {
		const std::string text(section);
		write_fixture("d1order.plx", bytes(text.begin(), text.end()));
		require(read_player_file() == 0, "read actual pilot with missing or malformed optional orders");
		const ubyte defaults_primary[] = { 4, 3, 2, 1, 0, 255, 16 };
		const ubyte defaults_secondary[] = { 4, 3, 1, 0, 255, 2 };
		require(!std::memcmp(d1_in_d2_weapon_order(0), defaults_primary, 7) &&
		            !std::memcmp(d1_in_d2_weapon_order(1), defaults_secondary, 6),
		        "missing or malformed native pilot orders use validated defaults");
		PlayerCfg.D1WeaponOrder = valid;
	}
	require(PHYSFS_delete("d1order.plx"), "remove optional text profile in isolated fixture");
	require(read_player_file() == 0 && fixture_native_order(0) != expected_primary,
	        "absent optional profile cannot inherit another pilot's native order");
	write_fixture("d1order.plx", profile);
	require(read_player_file() == 0, "restore the actual menu-edited pilot before recording");
#else
	(void) d2_directory;
#endif
	texmerge_init(10);
	init_game();
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	require(Game_wind != nullptr, "start actual mission for recorder preference capture");
	// Startup may read the pilot; require the saved menu edits at the live boundary
	require(fixture_native_order(0) == expected_primary && fixture_native_order(1) == expected_secondary, "mission entry keeps native pilot orders");
	ThisLevelTime = 0;
	Player_init[0].pos = ConsoleObject->pos;
	Player_init[0].orient = ConsoleObject->orient;
	Player_init[0].segnum = ConsoleObject->segnum;
	require(d_rand_get_replay_mode() == D_RAND_REPLAY_MODE_LCG_STATE, "recording uses the engine SIM stream");
	require(maybe_start_input_demo_recording(0), "start actual recorder with native pilot settings");
	input_demo_control_state controls = {};
	input_demo_control_pulse pulse = {};
	input_demo_result result;
	input_demo_capture_current_result(&result);
	result.frame_count = 1;
	unsigned seed = 0;
	d_rand_get_stream_state(D_RNG_SIM, &seed);
	char error[256] = {};
	require(input_demo_recorder_capture_frame(F1_0 / 25, &controls, &pulse, seed, 0, 0, nullptr, nullptr, error, sizeof(error)), "record one idle frame");
	require(input_demo_recorder_flush_with_result("weapon-order.dximdemo", &result, error, sizeof(error)), "write actual recorder metadata");
	input_demo_recorder_cancel();
	input_demo_file recorded;
	std::string parse_error;
	require(input_demo_file_read("weapon-order.dximdemo", &recorded, &parse_error), "parse generated input demo");
	const auto &cfg = recorded.metadata.player_cfg;
#ifdef DXX_BUILD_DESCENT_II
	require(cfg.primary_order_count == 11 && cfg.secondary_order_count == 11 &&
	            cfg.d1_primary_order_count == 7 && cfg.d1_secondary_order_count == 6 &&
	            !std::memcmp(cfg.d1_primary_order, expected_primary.data(), 7) && !std::memcmp(cfg.d1_secondary_order, expected_secondary.data(), 6),
	        "D2 recording carries both domains with exact native menu order");
#else
	require(cfg.primary_order_count == 7 && cfg.secondary_order_count == 6 &&
	            !std::memcmp(cfg.primary_order, expected_primary.data(), 7) && !std::memcmp(cfg.secondary_order, expected_secondary.data(), 6),
	        "native recording carries exact native menu order");
#endif
	std::fprintf(stderr, "Native weapon recorder capture passed\n");
	fixture_close_order_game();
	for (const char *path : { "weapon-order.dximdemo", native_demo }) {
		if (!path) continue;
		std::fprintf(stderr, "Testing recorded orders at replay startup: %s\n", path);
		new_player_config();
		require(write_player_file() == 0, "persist conflicting default preferences before replay");
#ifdef DXX_BUILD_DESCENT_II
		const auto before_replay = PlayerCfg;
#endif
		require(input_demo_replay_load(path, error, sizeof(error)), "load recorder output");
		input_demo_replay_cmdline_options options = {};
		options.allow_d1_in_d2 = 1;
		require(input_demo_apply_replay_common_setup(&options, error, sizeof(error)) && input_demo_start_loaded_replay_common() == 0,
		        "start actual native or imported replay");
		require(fixture_native_order(0) == expected_primary && fixture_native_order(1) == expected_secondary,
		        "replay startup restores recorded native orders over conflicting pilot preferences");
#ifdef DXX_BUILD_DESCENT_II
		require(!std::memcmp(before_replay.PrimaryOrder, PlayerCfg.PrimaryOrder, sizeof(PlayerCfg.PrimaryOrder)) &&
		            !std::memcmp(before_replay.SecondaryOrder, PlayerCfg.SecondaryOrder, sizeof(PlayerCfg.SecondaryOrder)),
		        "native replay settings leave D2 pilot arrays in their own domain");
#endif
		input_demo_replay_unload();
		fixture_close_order_game();
	}
#ifdef DXX_BUILD_DESCENT_II
	if (d2_directory) {
		require(write_player_file() == 0, "save restored native preferences before switching missions");
		const std::string hog = std::string(d2_directory) + "/descent2.hog";
		require(PHYSFS_mount(d2_directory, nullptr, 0) && PHYSFS_mount(hog.c_str(), nullptr, 0), "mount ordinary D2 mission assets");
		char d2_mission[] = "d2", d1_mission[] = "descent";
		require(load_mission_by_name(d2_mission) && !d1_in_d2_use_d1_gameplay(), "select actual Counterstrike mission");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(!std::memcmp(d2_cfg.PrimaryOrder, PlayerCfg.PrimaryOrder, sizeof(PlayerCfg.PrimaryOrder)) &&
		            !std::memcmp(d2_cfg.SecondaryOrder, PlayerCfg.SecondaryOrder, sizeof(PlayerCfg.SecondaryOrder)),
		        "ordinary D2 entry restores its own pilot ordering");
		require(fixture_native_order(0) == expected_primary && fixture_native_order(1) == expected_secondary,
		        "ordinary D2 entry preserves separate native preferences");
		fixture_close_order_game();
		require(load_mission_by_name(d1_mission), "return to actual First Strike mission");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(d1_in_d2_use_d1_gameplay() && fixture_native_order(0) == expected_primary && fixture_native_order(1) == expected_secondary,
		        "D1 mission entry restores native ordering after ordinary D2 gameplay");
		fixture_close_order_game();
		std::fprintf(stderr, "Native weapon orders survive D1 -> D2 -> D1 mission entry\n");
	}
#endif
	return { { "primary", expected_primary }, { "secondary", expected_secondary }, { "menus", Weapon_order_menu_visits } };
}
