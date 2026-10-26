// Compare measured runtime colors with the independently captured pinned XL table
static void no_damage_ambient_reference_test()
{
	const auto expected_path = find_arg_value(Num_args, Args, "-xl-ambient-reference");
	if (!expected_path) return;
	const auto actual_path = find_arg_value(Num_args, Args, "-xl-ambient-report");
	require(actual_path != nullptr, "ambient reference requires measured report");
	std::ifstream expected_file(expected_path), actual_file(actual_path);
	nlohmann::json expected, actual;
	expected_file >> expected;
	actual_file >> actual;
	require(expected.at("vertices") == actual.at("vertices") && expected.at("colors").size() == actual.at("colors").size(), "ambient reference vertex count differs");
	require(expected.at("sources").size() == actual.at("sources").size(), "ambient reference source count differs");
	double maximum = 0;
	for (std::size_t vertex = 0; vertex < expected.at("colors").size(); ++vertex)
		for (unsigned channel = 0; channel < 3; ++channel) {
			const double delta = std::abs(expected["colors"][vertex][channel].get<double>() - actual["colors"][vertex][channel].get<double>());
			maximum = (std::max) (maximum, delta);
			require(delta < 1e-5, "original ambient vertex differs from pinned reference");
		}
	for (const auto &light : expected.at("sources")) {
		const auto &sources = actual.at("sources");
		const auto found = std::find_if(sources.begin(), sources.end(), [&](const auto &entry) { return entry["segment"] == light["segment"] && entry["side"] == light["side"]; });
		require(found != sources.end(), "reference geometry light missing");
		require(std::abs((*found)["brightness"].template get<double>() - light["brightness"].get<double>()) < 1e-5, "reference geometry light brightness differs");
		for (unsigned channel = 0; channel < 3; ++channel)
			require(std::abs((*found)["rgb"][channel].template get<double>() - light["rgb"][channel].get<double>()) < 1e-5, "reference geometry light color differs");
	}
	std::cout << "Original ambient reference: 224 sources and 328 vertex colors match; max RGB delta " << maximum << '\n';
}

// Original-source loader and native save path
static void no_damage_persistence_test(const xl_compat::Bytes &bytes)
{
	char error[256] = {};
	const auto original = xl_compat::inspect_level(bytes);
	require(load_base_mission("obsrv-xl", error, sizeof(error)), error);
	require(xl_level_supported("obsrv-xl.rl2", 0), xl_level_rejection());
	require(!xl_level_supported("obsrv-xl.rl2", 1), "no-damage qualification admitted multiplayer");
	std::strcpy(Players[Player_num].callsign, "XLTest");
	Difficulty_level = 2;
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	require(xl_preview_rebuilt_ambient_vertices() == original.vertices.size(), "original ambient reconstruction missing");
	unsigned lit_vertices = 0;
	for (int i = 0; i < Num_vertices; ++i) {
		int32_t r = 0, g = 0, b = 0;
		xl_preview_vertex(i, F1_0, &r, &g, &b);
		if (r > 0 || g > 0 || b > 0) ++lit_vertices;
	}
	require(lit_vertices > 0, "rebuilt original ambient is entirely black");
	no_damage_ambient_reference_test();
	int protected_segment = -1, ordinary_segment = -1, protected_count = 0;
	for (int i = 0; i < Num_segments; ++i) {
		const bool expected = original.segments[i].function == 13;
		require(bool(xl_preview_no_damage_segment(i)) == expected, "loader property map differs from original source");
		if (expected) {
			protected_segment = i;
			++protected_count;
		} else if (original.segments[i].function == 0) ordinary_segment = i;
		require(Segment2s[i].special != 13, "no-damage property leaked into stock segment enum");
	}
	require(protected_count == 26 && ordinary_segment >= 0, "original protected/ordinary regions missing");
	const auto move_player = [](int segment) {
		obj_relink(ConsoleObject - Objects, segment);
		compute_segment_center(&ConsoleObject->pos, &Segments[segment]);
		ConsoleObject->mtype.phys_info.velocity = ConsoleObject->mtype.phys_info.rotvel = ConsoleObject->mtype.phys_info.thrust = {};
	};
	move_player(ordinary_segment);
	GameTime64 = i2f(100);
	FrameTime = F1_0 / 60;
	Players[Player_num].secondary_weapon = GUIDED_INDEX;
	Players[Player_num].secondary_weapon_flags |= HAS_FLAG(GUIDED_INDEX);
	Players[Player_num].secondary_ammo[GUIDED_INDEX] = 5;
	Controls = {};
	Controls.fire_secondary_count = 1;
	Next_missile_fire_time = GameTime64;
	do_weapon_n_item_stuff();
	require(Guided_missile[Player_num] && Players[Player_num].secondary_ammo[GUIDED_INDEX] == 4, "persistence control could not launch guided missile");
	const int guided_signature = Guided_missile[Player_num]->signature;
	move_player(protected_segment);
	Controls = {};
	Players[Player_num].primary_weapon = FUSION_INDEX;
	Players[Player_num].primary_weapon_flags |= HAS_FLAG(FUSION_INDEX);
	Players[Player_num].energy = i2f(77);
	Players[Player_num].shields = ConsoleObject->shields = i2f(83);
	Fusion_charge = F1_0;
	Global_laser_firing_count = 1;
	Global_missile_firing_count = 0;
	Auto_fire_fusion_cannon_time = GameTime64 + F1_0 / 2;
	const auto position = ConsoleObject->pos;
	char save[] = "xl-nodamage.sav", description[] = "No-damage state";
	stop_time();
	require(state_save_all_sub(save, description), "protected save failed");
	// Load an actual stock mission, clearing both the owned properties and live weapons
	require(load_base_mission("d2", error, sizeof(error)), error);
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	require(xl_preview_rebuilt_ambient_vertices() == 0, "stock mission retained rebuilt ambient");
	for (int i = 0; i < Num_segments; ++i) require(!xl_preview_no_damage_segment(i), "stock mission retained protection");
	Fusion_charge = 0;
	Global_laser_firing_count = 0;
	Auto_fire_fusion_cannon_time = 0;
	require(state_restore_all_sub(save, 0), "protected restore failed");
	require(xl_preview_rebuilt_ambient_vertices() == original.vertices.size(), "restore did not rebuild original ambient");
	no_damage_ambient_reference_test();
	require(!std::strcmp(Current_mission_filename, "obsrv-xl") && Current_level_num == 1 &&
	            ConsoleObject->segnum == protected_segment && !std::memcmp(&position, &ConsoleObject->pos, sizeof(position)),
	        "restore lost protected mission/player identity");
	for (int i = 0; i < Num_segments; ++i)
		require(bool(xl_preview_no_damage_segment(i)) == (original.segments[i].function == 13), "restore did not reconstruct original properties");
	require(!xl_level_supported("d2leva-1.rl2", 0) && load_level("d2leva-1.rl2") != 0 &&
	            xl_preview_no_damage_segment(protected_segment) && ConsoleObject->segnum == protected_segment &&
	            !std::memcmp(&position, &ConsoleObject->pos, sizeof(position)),
	        "rejected level replaced restored properties or player position");
	require(Fusion_charge == F1_0 && Global_laser_firing_count == 1 && Auto_fire_fusion_cannon_time - GameTime64 == F1_0 / 2 &&
	            Players[Player_num].energy == i2f(77) && Players[Player_num].shields == i2f(83),
	        "restore lost pending fusion charge, queue, timer or player resources");
	auto *guided = Guided_missile[Player_num];
	require(guided && guided->signature == guided_signature && Guided_missile_sig[Player_num] == guided_signature &&
	            Players[Player_num].secondary_ammo[GUIDED_INDEX] == 4,
	        "restore lost guided ownership, signature or ammo");
	GameTime64 += i2f(2);
	Controls = {};
	Controls.fire_secondary_count = 1;
	do_weapon_n_item_stuff();
	require(Guided_missile[Player_num] == guided && Players[Player_num].secondary_ammo[GUIDED_INDEX] == 4,
	        "fresh protected input released restored guidance or spent ammo");
	apply_damage_to_player(ConsoleObject, nullptr, i2f(10), 0);
	require(Players[Player_num].shields == i2f(83), "restored protected region did not suppress damage");
	move_player(ordinary_segment);
	Controls.fire_secondary_count = 1;
	do_weapon_n_item_stuff();
	require(!Guided_missile[Player_num] && Missile_viewer == guided && Players[Player_num].secondary_ammo[GUIDED_INDEX] == 4,
	        "ordinary exit did not release restored guidance without another shot");
	apply_damage_to_player(ConsoleObject, nullptr, i2f(10), 0);
	require(Players[Player_num].shields == i2f(73), "ordinary region retained restored protection");
	const auto fusion_shots = [] {
		int count = 0;
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_WEAPON && Objects[i].id == FUSION_ID) ++count;
		return count;
	};
	const int before = fusion_shots(), saved_suspend = Game_suspended;
	Game_suspended = SUSP_ROBOTS;
	FrameTime = F1_0 / 60;
	Controls = {};
	move_player(protected_segment);
	GameTime64 += FrameTime;
	GameProcessFrame();
	require(Fusion_charge == F1_0 && Global_laser_firing_count == 1 && !Auto_fire_fusion_cannon_time &&
	            Players[Player_num].energy == i2f(77) && fusion_shots() == before,
	        "restored fusion did not retain its pending shot inside protection");
	move_player(ordinary_segment);
	GameTime64 += FrameTime;
	GameProcessFrame();
	require(Fusion_charge == F1_0 + FrameTime && Players[Player_num].energy == i2f(77) - FrameTime &&
	            Auto_fire_fusion_cannon_time > GameTime64 && fusion_shots() == before,
	        "restored fusion exit lost its one-frame recharge");
	GameTime64 += FrameTime;
	GameProcessFrame();
	require(!Fusion_charge && !Global_laser_firing_count && !Auto_fire_fusion_cannon_time &&
	            Players[Player_num].energy == i2f(77) - FrameTime - Weapon_info[FUSION_ID].energy_usage / Weapon_info[FUSION_ID].fire_count &&
	            fusion_shots() == before + 2,
	        "restored fusion did not emit with retained charge and correct costs");
	Game_suspended = saved_suspend;
	require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "persistence stock cleanup failed");
	require(load_base_mission("obsrv-xl", error, sizeof(error)) && xl_level_supported("obsrv-xl.rl2", 0), "supported properties failed normal admission");
	require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "final stock recovery failed");
	std::cout << "No-damage original-source persistence: stock round trip reconstructs 26 properties, fusion queue/timer/resources and guided ownership; rejected candidate retains state; restored fusion emits after exit and guidance releases normally\n";
}
