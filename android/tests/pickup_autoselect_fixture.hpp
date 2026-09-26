// Actual powerup acquisition, queued selection and release in both engines
void test_quad_autoselect_runtime(bool native);

static void fixture_native_primary_order(const ubyte *order)
{
#ifdef DXX_BUILD_DESCENT_II
	require(d1_in_d2_set_weapon_order(0, order, 7), "apply actual native weapon-order preference");
#else
	std::memcpy(PlayerCfg.PrimaryOrder, order, 7);
#endif
}

static void fixture_native_secondary_order(const ubyte *order)
{
#ifdef DXX_BUILD_DESCENT_II
	require(d1_in_d2_set_weapon_order(1, order, 6), "apply native secondary order preference");
#else
	std::memcpy(PlayerCfg.SecondaryOrder, order, 6);
#endif
}

static nlohmann::json exercise_classic_cutoff(void)
{
	const ubyte order[] = { 16, 4, 0, 3, 2, 1, 255 };
	InitWeaponOrdering();
	fixture_native_primary_order(order);
	Game_mode = 0;
	Player_num = Player_is_dead = 0;
	Newdemo_state = ND_STATE_NORMAL;
	init_test_corridor();
	ConsoleObject = &Objects[0];
	ConsoleObject->type = OBJ_PLAYER;
	Players[0] = {};
	Players[0].objnum = 0;
	Players[0].primary_weapon = VULCAN_INDEX;
	Players[0].primary_weapon_flags = 3;
	PlayerCfg.ClassicAutoselectWeapon = 1;
	std::fprintf(stderr, "Testing empty Vulcan with every primary above the classic cutoff\n");
	std::fflush(stderr);
	auto_select_weapon(0);
	require(Players[0].primary_weapon == LASER_INDEX, "exhausted classic search returns to lasers");
	return { { "selected", Players[0].primary_weapon } };
}

static nlohmann::json exercise_weapon_selection(bool native)
{
	using nlohmann::json;
	const auto saved_cfg = PlayerCfg;
	const ubyte orders[][7] = {
		{ 4, 3, 2, 1, 0, 255, 16 },
		{ 16, 4, 3, 2, 1, 255, 0 },
		{ 1, 16, 0, 3, 255, 4, 2 },
		{ 16, 4, 0, 3, 2, 1, 255 },
		{ 255, 16, 4, 3, 2, 1, 0 },
	};
	json trace = json::array();
	Game_mode = 0;
	Player_num = Player_is_dead = 0;
	Newdemo_state = ND_STATE_NORMAL;
	init_test_corridor();
	ConsoleObject = &Objects[0];
	ConsoleObject->type = OBJ_PLAYER;
	for (int order = 0; order < (native ? 5 : 3); ++order)
		for (int quads = 0; quads < 2; ++quads)
			for (int owned : { 1, 3, 5, 9, 17, 11, 21, 31 })
				for (int selected = 0; selected < 5; ++selected)
					for (fix energy : { 0, F1_0, 100 * F1_0 })
						for (int ammo : { 0, 100 })
							for (int restricted = 0; restricted < 2; ++restricted)
								for (int operation = 0; operation < 3; ++operation) {
									PlayerCfg = saved_cfg;
									InitWeaponOrdering();
									if (native) {
										fixture_native_primary_order(orders[order]);
									}
									PlayerCfg.CycleAutoselectOnly = restricted;
									PlayerCfg.ClassicAutoselectWeapon = operation == 2;
									Controls = {};
									Players[0] = {};
									auto &player = Players[0];
									player.objnum = 0;
									player.primary_weapon = selected;
									player.primary_weapon_flags = owned;
									player.flags = quads ? PLAYER_FLAGS_QUAD_LASERS : 0;
									player.energy = energy;
									player.primary_ammo[VULCAN_INDEX] = ammo;
									GameTime64 = 10 * F1_0;
									Next_laser_fire_time = GameTime64 - F1_0;
									Global_laser_firing_count = 2;
									Fusion_charge = F1_0 / 3;
									const auto draws = d_rand_get_call_count();
									json sequence = json::array();
									for (int step = 0; step < (operation == 0 ? 7 : 1); ++step) {
										if (operation == 0) CyclePrimary();
										else auto_select_weapon(0);
										sequence.push_back({ player.primary_weapon, Next_laser_fire_time - GameTime64, Global_laser_firing_count, Fusion_charge });
									}
									require(draws == d_rand_get_call_count(), "cycling and fallback do not advance SIM RNG");
									trace.push_back({ { "case", { order, quads, owned, selected, energy, ammo, restricted, operation } }, { "sequence", sequence } });
								}
	if (native) {
		const ubyte secondary_orders[][6] = {
			{ 4, 3, 1, 0, 255, 2 },
			{ 2, 4, 3, 1, 0, 255 },
			{ 1, 2, 255, 0, 3, 4 },
			{ 255, 4, 3, 2, 1, 0 },
		};
		for (int order = 0; order < 4; ++order)
			for (int owned : { 0, 1, 2, 4, 8, 16, 11, 31 })
				for (int selected = 0; selected < 5; ++selected)
					for (int ammo : { 0, 1, 2, 3 })
						for (int restricted = 0; restricted < 2; ++restricted)
							for (int operation = 0; operation < 3; ++operation) {
								PlayerCfg = saved_cfg;
								InitWeaponOrdering();
								fixture_native_secondary_order(secondary_orders[order]);
								PlayerCfg.CycleAutoselectOnly = restricted;
								PlayerCfg.ClassicAutoselectWeapon = operation == 2;
								Controls = {};
								Players[0] = {};
								auto &player = Players[0];
								player.objnum = 0;
								player.secondary_weapon = selected;
								player.secondary_weapon_flags = owned;
								player.energy = 100 * F1_0;
								for (int weapon = 0; weapon < 5; ++weapon)
									player.secondary_ammo[weapon] = ammo == 1 || (ammo == 2 && weapon != selected) || (ammo == 3 && weapon == PROXIMITY_INDEX) ? 5 : 0;
								GameTime64 = 10 * F1_0;
								Next_missile_fire_time = GameTime64 - F1_0;
								const auto draws = d_rand_get_call_count();
								json sequence = json::array();
								for (int step = 0; step < (operation == 0 ? 6 : 1); ++step) {
									if (operation == 0) CycleSecondary();
									else auto_select_weapon(1);
									sequence.push_back({ player.secondary_weapon, Next_missile_fire_time - GameTime64 });
								}
								require(draws == d_rand_get_call_count(), "secondary cycling and fallback do not advance SIM RNG");
								trace.push_back({ { "secondary_case", { order, owned, selected, ammo, restricted, operation } }, { "sequence", sequence } });
							}
	}
	PlayerCfg = saved_cfg;
	return trace;
}

static nlohmann::json exercise_pickup_autoselect(bool native)
{
	using nlohmann::json;
	const auto saved_cfg = PlayerCfg;
	const ubyte native_orders[][7] = {
		{ 4, 3, 2, 1, 0, 255, 16 },
		{ 16, 4, 0, 3, 2, 1, 255 },
		{ 4, 3, 16, 2, 1, 255, 0 },
	};
	const int powerups[] = { POW_LASER, POW_LASER, POW_LASER, POW_QUAD_FIRE,
		                     POW_QUAD_FIRE, POW_QUAD_FIRE, POW_SPREADFIRE_WEAPON, POW_SPREADFIRE_WEAPON,
		                     POW_FUSION_WEAPON, POW_LASER, POW_VULCAN_AMMO };
	json trace = json::array();
	Game_mode = 0;
	Player_num = Player_is_dead = 0;
	Newdemo_state = ND_STATE_NORMAL;
	init_test_corridor();
	ConsoleObject = &Objects[0];
	ConsoleObject->type = OBJ_PLAYER;
	for (int order = 0; order < 4; ++order)
		for (int scenario = 0; scenario < 11; ++scenario)
			for (int firing = 0; firing < 4; ++firing)
				for (int once = 0; once < 2; ++once)
					for (int consumed = 0; consumed < 2; ++consumed)
						for (int pending : { -1, SPREADFIRE_INDEX, PLASMA_INDEX }) {
							PlayerCfg = saved_cfg;
							InitWeaponOrdering();
							if (native && order < 3) {
								fixture_native_primary_order(native_orders[order]);
							}
							PlayerCfg.AutoselectOnlyOnce = once;
							PlayerCfg.SelectAfterFire = firing == 1;
							PlayerCfg.NoFireAutoselect = firing == 2;
							std::memset(&Controls, 0, sizeof(Controls));
							Controls.fire_primary_state = firing != 0;
							Players[0] = {};
							auto &player = Players[0];
							player.objnum = 0;
							player.energy = player.shields = 100 * F1_0;
							player.primary_weapon = scenario == 2 || scenario == 4 || scenario == 5 || scenario == 8 ? PLASMA_INDEX : LASER_INDEX;
							player.primary_weapon_flags = 1 | (1 << player.primary_weapon);
							player.flags = scenario == 1 || scenario >= 5 ? PLAYER_FLAGS_QUAD_LASERS : 0;
							if (scenario == 7) player.primary_weapon_flags |= 1 << SPREADFIRE_INDEX;
							if (scenario == 10) player.primary_weapon_flags |= 1 << VULCAN_INDEX;
							player.laser_level = scenario == 9 ? 3 : 0;
							PrimaryWeaponPickedUp = consumed;
							SecondaryWeaponPickedUp = 0;
							delayed_primary_autoselect_weapon_index = pending;
							delayed_secondary_autoselect_weapon_index = -1;
							GameTime64 = 10 * F1_0;
							Next_laser_fire_time = GameTime64 - F1_0;
							Global_laser_firing_count = 2;
							Fusion_charge = F1_0 / 3;
							d_srand(1234);
							const auto draws = d_rand_get_call_count();
							const auto snapshot = [&]() -> json {
								return { { "selected", player.primary_weapon }, { "pending", delayed_primary_autoselect_weapon_index }, { "picked_up", PrimaryWeaponPickedUp }, { "inventory", player.primary_weapon_flags }, { "flags", player.flags }, { "laser_level", player.laser_level }, { "energy", player.energy }, { "ammo", player.primary_ammo[VULCAN_INDEX] }, { "rearm", Next_laser_fire_time - GameTime64 }, { "firing_count", Global_laser_firing_count }, { "fusion_charge", Fusion_charge } };
							};
							object pickup = {};
							pickup.type = OBJ_POWERUP;
							pickup.id = powerups[scenario];
							pickup.lifeleft = F1_0;
							const int used = do_powerup(&pickup);
							const auto acquired = snapshot();
							Controls.fire_primary_state = 0;
							delayed_autoselect();
							const auto released = snapshot();
							pick_up_primary(FUSION_INDEX);
							require(draws == d_rand_get_call_count(), "pickup and selection do not advance SIM RNG");
							trace.push_back({ { "case", { order, scenario, firing, once, consumed, pending } },
							                  { "used", used },
							                  { "acquired", acquired },
							                  { "released", released },
							                  { "followup", snapshot() } });
						}
	test_quad_autoselect_runtime(native);
	PlayerCfg = saved_cfg;
	Controls = {};
	reset_auto_select();
	return trace;
}
