#ifndef XL_SLOW_MOTION_DEADLINE_TEST_H
#define XL_SLOW_MOTION_DEADLINE_TEST_H
extern "C" {
int allowed_to_fire_laser(void);
int allowed_to_fire_flare(void);
extern fix64 Next_flare_fire_time;
}
static void slow_motion_deadline_controls(std::ostream &samples, int profile)
{
	const int saved_mode = Game_mode, saved_dead = Player_is_dead;
	const auto player = Players[Player_num];
	const fix64 game = GameTime64, last = Last_laser_fired_time, next = Next_laser_fire_time, flare = Next_flare_fire_time;
	const auto calls = d_rand_get_stream_call_count(D_RNG_SIM);
	Player_is_dead = 0;
	const fix lasts[] = { -2 * F1_0, -1, 0, F1_0 };
	const fix nexts[] = { -1, 0, 1, F1_0 / 4, 2 * F1_0 - 1, 2 * F1_0, 2 * F1_0 + 1, 6 * F1_0 };
	for (int multiplayer : { 0, 1 })
		for (int li = 0; li < 4; ++li)
			for (int ni = 0; ni < 8; ++ni)
				for (int epoch : { 0, 1 })
					for (int enough : { 0, 1 }) {
						Game_mode = multiplayer ? GM_MULTI : saved_mode;
						GameTime64 = epoch ? INT64_C(1) << 40 : 0;
						Last_laser_fired_time = GameTime64 + lasts[li];
						Next_laser_fire_time = Next_flare_fire_time = GameTime64 + nexts[ni];
						Players[Player_num].energy = enough ? Weapon_info[FLARE_ID].energy_usage : Weapon_info[FLARE_ID].energy_usage - 1;
						const auto primary = allowed_to_fire_laser();
						const auto allowed_flare = allowed_to_fire_flare();
						samples << "D " << profile << ' ' << multiplayer << ' ' << li << ' ' << ni << ' ' << epoch << ' ' << enough << ' ' << primary << ' ' << allowed_flare << ' ' << Next_flare_fire_time - GameTime64 << '\n';
						require(Last_laser_fired_time == GameTime64 + lasts[li] && Next_laser_fire_time == GameTime64 + nexts[ni], "Primary eligibility mutated saved timestamps");
					}
	Game_mode = saved_mode;
	if (profile) {
		require(xl_slow_motion_primary_blocked(INT64_MIN, INT64_MAX, 0), "Unrepresentable primary interval admitted");
		require(xl_slow_motion_primary_blocked(INT64_MAX - F1_0, INT64_MAX, 0), "Overflowing primary deadline admitted");
		require(xl_slow_motion_primary_blocked(0, INT32_MAX, 0), "Undefined fix conversion admitted");
	}
	Player_is_dead = 1;
	GameTime64 = F1_0;
	Last_laser_fired_time = Next_laser_fire_time = 0;
	require(!allowed_to_fire_laser(), "Slow primary bypassed dead-player gate");
	require(d_rand_get_stream_call_count(D_RNG_SIM) == calls, "Firing eligibility consumed gameplay RNG");
	Players[Player_num] = player;
	Game_mode = saved_mode;
	Player_is_dead = saved_dead;
	GameTime64 = game;
	Last_laser_fired_time = last;
	Next_laser_fire_time = next;
	Next_flare_fire_time = flare;
}
#endif
