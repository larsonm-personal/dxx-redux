// Synthetic Fusion parameters on public data; actual no-input gameplay and ordinary save paths
static void exercise_fusion_save_runtime(const char *scenario)
{
	const bool due = std::strcmp(scenario, "due") == 0;
	const bool past = std::strcmp(scenario, "past") == 0;
	const bool soon = std::strcmp(scenario, "soon") == 0;
	const bool future = std::strcmp(scenario, "future") == 0;
	const bool inactive_charged = std::strcmp(scenario, "inactive-charged") == 0;
	const bool nonfusion = std::strcmp(scenario, "nonfusion") == 0;
	require(due || past || soon || future || inactive_charged || nonfusion || std::strcmp(scenario, "idle") == 0, "known optional Fusion timer scenario");
	// Shareware excludes Fusion assets. Reuse its valid laser definition, identically on both legs
	const auto synthetic_fusion = Weapon_info[LASER_ID_L1];
	require(synthetic_fusion.render_type == WEAPON_RENDER_POLYMODEL ||
	            ((synthetic_fusion.render_type == WEAPON_RENDER_BLOB || synthetic_fusion.render_type == WEAPON_RENDER_VCLIP) && synthetic_fusion.blob_size > 0),
	        "public laser supplies valid synthetic Fusion projectile geometry");
	Weapon_info[FUSION_ID] = synthetic_fusion;
	Players[Player_num].primary_weapon_flags |= 1 << FUSION_INDEX;
	Players[Player_num].primary_weapon = nonfusion ? LASER_INDEX : FUSION_INDEX;
	Players[Player_num].energy = 100 * F1_0;
	Fusion_charge = due || past || soon || future || inactive_charged ? F1_0 / 2 : 0;
	const fix saved_charge = Fusion_charge;
	FrameTime = F1_0 / 30;
	Auto_fire_fusion_cannon_time = due ? GameTime64 : past ? GameTime64 - 1 : soon ? GameTime64 + 1 : future ? GameTime64 + 10 * FrameTime : 0;
	Next_laser_fire_time = GameTime64;
	const bool active = Auto_fire_fusion_cannon_time != 0;
	const auto count_fusion = []() {
		int count = 0;
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_WEAPON && Objects[i].id == FUSION_ID && Objects[i].ctype.laser_info.parent_type == OBJ_PLAYER)
				++count;
		return count;
	};
	const auto restore = [&](char *path) {
#ifdef DXX_BUILD_DESCENT_II
		const int result = state_restore_all_sub(path, 0);
#else
		const int result = state_restore_all_sub(path);
#endif
		// The loader reinstalls stock definitions; reapply the same explicitly synthetic asset fixture
		if (result) Weapon_info[FUSION_ID] = synthetic_fusion;
		return result;
	};
	const auto frame = []() {
		FrameTime = F1_0 / 30;
		Controls = {};
		do_weapon_n_item_stuff();
		calc_game_time();
		GameProcessFrame();
	};
	char filename[] = "fusion-runtime.sav", description[21] = "Fusion runtime";
	stop_time();
	require(state_save_all_sub(filename, description), "save optional Fusion timer through ordinary serializer");
	const int before = count_fusion();
	const int continuation_frames = future ? 12 : 1;
	for (int i = 0; i < continuation_frames; ++i) frame();
	const int expected_shots = count_fusion() - before;
	const int expected_energy = Players[Player_num].energy;
	require(expected_shots == ((due || past || soon || future) ? 2 : 0), "uninterrupted input-free Fusion outcome is nonvacuous");
	require(restore(filename), "restore optional Fusion timer");
	require((Auto_fire_fusion_cannon_time != 0) == active, "restore the optional timer's explicit active state");
	require(Fusion_charge == saved_charge, "restore Fusion charge independently of optional timer activity");
	if (due) require(Auto_fire_fusion_cannon_time == -1, "active due-now deadline uses established nonzero convention");
	// A second save at the rebased epoch must preserve the same active/inactive state
	char repeated[] = "fusion-runtime-repeated.sav";
	stop_time();
	require(state_save_all_sub(repeated, description) && restore(repeated), "repeat ordinary save at rebased epoch");
	require((Auto_fire_fusion_cannon_time != 0) == active, "repeated saves preserve optional timer activity");
	const bytes opposite_endian = save_endian_fixture(read_save_fixture(repeated)).convert();
	char swapped[] = "fusion-runtime-swapped.sav";
	write_fixture(swapped, opposite_endian);
	stop_time();
	require(restore(swapped) && (Auto_fire_fusion_cannon_time != 0) == active,
	        "opposite-endian complete save preserves optional timer activity");
	require(Fusion_charge == saved_charge, "repeated and opposite-endian saves preserve Fusion charge");
	const int restored_before = count_fusion();
	for (int i = 0; i < continuation_frames; ++i) frame();
	require(count_fusion() - restored_before == expected_shots && Players[Player_num].energy == expected_energy,
	        "resumed no-input projectile creation and energy match uninterrupted continuation");
	// Native fixtures have no Android launcher metadata, so the active flag is the last four bytes
	const bytes valid = read_save_fixture(repeated);
	require(valid.size() >= 4, "complete Fusion flag bytes");
	for (int corruption = 0; corruption < 6; ++corruption) {
		bytes damaged = valid;
		if (corruption < 2) set_int(damaged, damaged.size() - 4, corruption == 0 ? 2 : -1);
		else damaged.resize(damaged.size() - (corruption - 1));
		char bad[] = "fusion-runtime-invalid.sav";
		write_fixture(bad, damaged);
		stop_time();
		require(!restore(bad), "reject invalid or truncated optional timer flag");
		// Native runtime validation occurs after level preparation; require exact valid retry, not transactional rollback
		stop_time();
		require(restore(repeated) && (Auto_fire_fusion_cannon_time != 0) == active,
		        "valid optional timer save remains loadable after malformed-tail refusal");
	}
	std::printf("PASS: optional Fusion timer %s, repeated saves, gameplay outcome and invalid-tail controls\n", scenario);
}
