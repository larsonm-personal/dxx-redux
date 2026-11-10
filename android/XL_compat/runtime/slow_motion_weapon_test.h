#ifndef XL_SLOW_MOTION_WEAPON_TEST_H
#define XL_SLOW_MOTION_WEAPON_TEST_H
extern "C" {
void omega_charge_frame(void);
void homing_missile_turn_towards_velocity(object *, vms_vector *, fix);
extern fix64 Last_omega_fire_time;
}
static void slow_motion_weapon_controls(std::ostream &samples, int profile)
{
	const int saved_mode = Game_mode, saved_difficulty = Difficulty_level, saved_dead = Player_is_dead;
	const fix saved_time = FrameTime, saved_charge = Omega_charge;
	const fix64 saved_game = GameTime64, saved_last = Last_omega_fire_time;
	const auto player = Players[Player_num];
	object *saved_console = ConsoleObject;
	const fix times[] = { 1, 3, 4, 7, 15, 65, 1092, 16384 };
	GameTime64 = 10 * F1_0;
	Player_is_dead = 0;
	Players[Player_num].primary_weapon_flags |= 1 << OMEGA_INDEX;
	Players[Player_num].primary_weapon = LASER_INDEX;
	const auto calls = d_rand_get_stream_call_count(D_RNG_SIM);
	for (int multiplayer : { 0, 1 }) {
		Game_mode = multiplayer ? GM_MULTI : saved_mode;
		for (int difficulty = 0; difficulty < 5; ++difficulty)
			for (fix time : times)
				for (fix charge : { 0, 1, MAX_OMEGA_CHARGE - 1, MAX_OMEGA_CHARGE })
					for (fix energy : { 0, 1, 100 * F1_0 }) {
						Difficulty_level = difficulty;
						FrameTime = time;
						Omega_charge = charge;
						Players[Player_num].energy = energy;
						Last_omega_fire_time = 0;
						omega_charge_frame();
						samples << "O " << profile << ' ' << multiplayer << ' ' << difficulty << ' ' << time << ' ' << charge << ' ' << energy << ' ' << Omega_charge << ' ' << Players[Player_num].energy << '\n';
					}
		// Keep existing ownership/death/cooldown gates; these are not claimed
		// as reference frame-index cooldown parity
		for (int gate = 0; gate < 3; ++gate) {
			Players[Player_num].primary_weapon_flags = player.primary_weapon_flags | (1 << OMEGA_INDEX);
			Player_is_dead = gate == 1;
			Last_omega_fire_time = gate == 2 ? GameTime64 : 0;
			if (gate == 0) Players[Player_num].primary_weapon_flags &= ~(1 << OMEGA_INDEX);
			Players[Player_num].energy = 100 * F1_0;
			Omega_charge = 0;
			FrameTime = F1_0 / 4;
			omega_charge_frame();
			require(Omega_charge == 0 && Players[Player_num].energy == 100 * F1_0, "Slow Omega changed an existing charge gate");
		}
		Players[Player_num].primary_weapon_flags = player.primary_weapon_flags | (1 << OMEGA_INDEX);
		Player_is_dead = 0;
		object actor{};
		const vms_vector goals[] = { { F1_0, 0, 0 }, { 0, 0, F1_0 }, { 0, 0, -F1_0 }, { 46340, 46340, 0 } };
		for (int console : { 0, 1 })
			for (int goal_index = 0; goal_index < 4; ++goal_index)
				for (fix time : times) {
					ConsoleObject = console ? &actor : saved_console;
					actor.orient = vmd_identity_matrix;
					auto goal = goals[goal_index], forward = goal;
					const fix scale = xl_slow_motion_world_time(time * 16);
					vm_vec_scale(&forward, scale);
					vm_vec_add2(&forward, &actor.orient.fvec);
					vm_vec_normalize_quick(&forward);
					vms_matrix expected;
					vm_vector_2_matrix(&expected, &forward, nullptr, nullptr);
					homing_missile_turn_towards_velocity(&actor, &goal, time);
					require(!std::memcmp(&expected, &actor.orient, sizeof(expected)), "Slow homing orientation hook differs");
					samples << "H " << profile << ' ' << multiplayer << ' ' << console << ' ' << goal_index << ' ' << time << ' ' << scale << '\n';
				}
		ConsoleObject = saved_console;
	}
	require(d_rand_get_stream_call_count(D_RNG_SIM) == calls, "Weapon clock consumers changed gameplay RNG");
	Players[Player_num] = player;
	ConsoleObject = saved_console;
	Game_mode = saved_mode;
	Difficulty_level = saved_difficulty;
	Player_is_dead = saved_dead;
	FrameTime = saved_time;
	GameTime64 = saved_game;
	Last_omega_fire_time = saved_last;
	Omega_charge = saved_charge;
}
#endif
