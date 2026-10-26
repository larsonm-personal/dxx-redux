// Matched inputs for the separately compiled, pinned reference gate functions
static void no_damage_reference_gates_test(int protected_segment, int ordinary_segment)
{
	const auto saved_time = GameTime64, saved_laser = Next_laser_fire_time, saved_missile = Next_missile_fire_time;
	const int saved_dead = Player_is_dead, saved_queue = Global_missile_firing_count;
	const int saved_segment = ConsoleObject->segnum;
	GameTime64 = i2f(100);
	for (int protected_region : { 0, 1 })
		for (int dead : { 0, 1 })
			for (int offset : { -1, 0, 1 }) {
				obj_relink(ConsoleObject - Objects, protected_region ? protected_segment : ordinary_segment);
				Player_is_dead = dead;
				Next_laser_fire_time = Next_missile_fire_time = GameTime64 + i2f(offset);
				Global_missile_firing_count = 3;
				const int gun = allowed_to_fire_laser(), missile = allowed_to_fire_missile();
				std::cout << "ND_GATE " << protected_region << ' ' << dead << ' ' << offset << ' '
				          << gun << ' ' << missile << ' ' << Global_missile_firing_count << '\n';
			}
	obj_relink(ConsoleObject - Objects, saved_segment);
	GameTime64 = saved_time;
	Next_laser_fire_time = saved_laser;
	Next_missile_fire_time = saved_missile;
	Player_is_dead = saved_dead;
	Global_missile_firing_count = saved_queue;
}
