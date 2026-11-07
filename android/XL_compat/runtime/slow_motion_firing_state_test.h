#ifndef XL_SLOW_MOTION_FIRING_STATE_TEST_H
#define XL_SLOW_MOTION_FIRING_STATE_TEST_H
extern "C" {
int allowed_to_fire_missile(void);
void do_missile_firing(int);
}
static void slow_motion_firing_state_controls(std::ostream &samples, int profile)
{
	const int mode = Game_mode, dead = Player_is_dead, difficulty = Difficulty_level;
	const auto player = Players[Player_num];
	laser_runtime_state laser_state;
	laser_get_runtime_state(&laser_state);
	std::vector<int> signatures;
	for (int i = 0; i <= Highest_object_index; ++i) signatures.push_back(Objects[i].type == OBJ_NONE ? -1 : Objects[i].signature);
	const auto saved = xl_runtime::weapon_clock_snapshot();
	const fix64 game = GameTime64, missile = Next_missile_fire_time, laser = Next_laser_fire_time, omega = Last_omega_fire_time;
	const fix frame = FrameTime, charge = Omega_charge;
	Player_is_dead = 0;
	const fix lasts[] = { -2 * F1_0, -1, 0, -F1_0 };
	const fix nexts[] = { -1, 0, 1, F1_0 / 4, 5 * F1_0 - 1, 5 * F1_0, 5 * F1_0 + 1, 15 * F1_0 };
	for (int multiplayer : { 0, 1 }) {
		Game_mode = multiplayer ? GM_MULTI : mode;
		for (int li = 0; li < 4; ++li)
			for (int ni = 0; ni < 8; ++ni)
				for (int epoch : { 0, 1 }) {
					GameTime64 = epoch ? INT64_C(1) << 40 : 0;
					require(xl_runtime::restore_weapon_clock(nlohmann::json{ lasts[li] }, true), "Cannot seed missile deadline");
					Next_missile_fire_time = GameTime64 + nexts[ni];
					samples << "E " << profile << ' ' << multiplayer << ' ' << li << ' ' << ni << ' ' << epoch << ' ' << allowed_to_fire_missile() << '\n';
				}

	}
	Game_mode = mode;
	GameTime64 = 10 * F1_0;
	const nlohmann::json baseline{ -123 };
	require(xl_runtime::restore_weapon_clock(baseline, true), "Cannot seed save validation");
	for (const auto &invalid : { nlohmann::json(nullptr), nlohmann::json::array(), nlohmann::json{ 1 }, nlohmann::json{ 0.0 }, nlohmann::json{ true }, nlohmann::json{ UINT64_MAX }, nlohmann::json{ INT64_MIN }, nlohmann::json{ 0, 1 } }) {
		require(!xl_runtime::restore_weapon_clock(invalid, false) && !xl_runtime::restore_weapon_clock(invalid, true), "Malformed weapon state accepted");
		require(xl_runtime::weapon_clock_snapshot() == baseline, "Rejected weapon state partially published");
	}
	require(xl_runtime::restore_weapon_clock(nlohmann::json{ -456 }, false) && xl_runtime::weapon_clock_snapshot() == baseline, "Validation-only weapon restore mutated state");
	Players[Player_num].secondary_weapon = CONCUSSION_INDEX;
	Players[Player_num].secondary_ammo[CONCUSSION_INDEX] = 1;
	Next_missile_fire_time = GameTime64;
	require(xl_runtime::restore_weapon_clock(nlohmann::json{ -987 }, true), "Cannot seed prior missile shot");
	do_missile_firing(0);
	require(Players[Player_num].secondary_ammo[CONCUSSION_INDEX] == 0 && xl_runtime::weapon_clock_snapshot()[0] == 0, "Native missile did not record its last shot");
	GameTime64 += 1001;
	do_missile_firing(0);
	require(xl_runtime::weapon_clock_snapshot()[0] == -1001, "Empty missile attempt changed last shot");
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type != OBJ_NONE && (i >= int(signatures.size()) || Objects[i].signature != signatures[i])) obj_delete(i);
	Players[Player_num] = player;
	Game_mode = mode;
	Player_is_dead = dead;
	Difficulty_level = difficulty;
	GameTime64 = game;
	FrameTime = frame;
	Omega_charge = charge;
	Next_missile_fire_time = missile;
	Next_laser_fire_time = laser;
	Last_omega_fire_time = omega;
	laser_set_runtime_state(&laser_state);
	require(xl_runtime::restore_weapon_clock(saved, true), "Cannot restore firing controls state");
}
#endif
