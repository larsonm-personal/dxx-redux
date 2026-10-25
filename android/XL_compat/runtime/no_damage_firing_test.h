extern "C" {
int allowed_to_fire_laser(void);
int allowed_to_fire_missile(void);
}

static void no_damage_firing_test(int protected_segment, int ordinary_segment)
{
	const auto saved_controls = Controls;
	const auto saved_time = GameTime64;
	const auto saved_frame = FrameTime;
	const auto saved_laser = Next_laser_fire_time, saved_missile = Next_missile_fire_time, saved_flare = Next_flare_fire_time;
	const auto saved_last_laser = Last_laser_fired_time;
	const int saved_primary_queue = Global_laser_firing_count, saved_secondary_queue = Global_missile_firing_count;
	const int saved_difficulty = Difficulty_level;
	const auto move_player = [](int segment) {
		obj_relink(ConsoleObject - Objects, segment);
		compute_segment_center(&ConsoleObject->pos, &Segments[segment]);
	};
	Controls = {};
	GameTime64 = i2f(100);
	FrameTime = F1_0 / 60;
	Difficulty_level = 2;
	Players[Player_num].energy = i2f(100);
	Players[Player_num].primary_weapon = LASER_INDEX;
	Players[Player_num].secondary_weapon = CONCUSSION_INDEX;
	Players[Player_num].secondary_ammo[CONCUSSION_INDEX] = 10;
	Players[Player_num].secondary_ammo[PROXIMITY_INDEX] = 4;
	Players[Player_num].secondary_ammo[SMART_MINE_INDEX] = 4;
	Next_laser_fire_time = Next_missile_fire_time = Next_flare_fire_time = GameTime64;
	Global_laser_firing_count = 1;
	Global_missile_firing_count = 0;
	move_player(protected_segment);
	const int before = num_objects;
	require(!allowed_to_fire_laser() && !allowed_to_fire_missile(), "protected input firing gate open");
	do_laser_firing_player();
	require(Players[Player_num].energy == i2f(100) && num_objects == before && Next_laser_fire_time == GameTime64,
	        "protected queued primary consumed energy, created objects or advanced clock");
	Controls.fire_secondary_count = 1;
	Controls.drop_bomb_count = 2;
	do_weapon_n_item_stuff();
	require(num_objects == before && Players[Player_num].secondary_ammo[CONCUSSION_INDEX] == 10 &&
	            Players[Player_num].secondary_ammo[PROXIMITY_INDEX] == 4 && Players[Player_num].secondary_ammo[SMART_MINE_INDEX] == 4 &&
	            !Controls.drop_bomb_count && !Global_missile_firing_count,
	        "protected fresh missile/bomb input spent ammo or created objects");
	Controls.fire_flare_count = 1;
	do_weapon_n_item_stuff();
	require(num_objects == before && Players[Player_num].energy == i2f(100) - Weapon_info[FLARE_ID].energy_usage &&
	            Next_flare_fire_time == GameTime64 + F1_0 / 4,
	        "protected flare must consume energy/cooldown without a projectile");
	Global_missile_firing_count = 1;
	do_weapon_n_item_stuff();
	require(num_objects == before && Players[Player_num].secondary_ammo[CONCUSSION_INDEX] == 9 &&
	            !Global_missile_firing_count && Next_missile_fire_time > GameTime64,
	        "protected queued missile must consume ammo/cooldown without a projectile");
	// Creation is based on the parent segment, not the muzzle segment
	vms_vector muzzle;
	compute_segment_center(&muzzle, &Segments[ordinary_segment]);
	require(Laser_create_new(&ConsoleObject->orient.fvec, &muzzle, ordinary_segment, ConsoleObject - Objects, LASER_ID_L1, 0) < 0,
	        "protected parent fired through an ordinary muzzle segment");
	move_player(ordinary_segment);
	compute_segment_center(&muzzle, &Segments[protected_segment]);
	const int outside = Laser_create_new(&ConsoleObject->orient.fvec, &muzzle, protected_segment, ConsoleObject - Objects, LASER_ID_L1, 0);
	require(outside >= 0, "ordinary parent could not fire into protected segment");
	const int robot = obj_create(OBJ_ROBOT, 0, protected_segment, &muzzle, &ConsoleObject->orient, F1_0, CT_AI, MT_PHYSICS, RT_POLYOBJ);
	require(robot >= 0, "cannot create robot firing control");
	require(Laser_create_new(&ConsoleObject->orient.fvec, &muzzle, protected_segment, robot, LASER_ID_L1, 0) >= 0,
	        "no-damage property incorrectly prevented robot firing");
	require(allowed_to_fire_laser(), "ordinary primary gate closed");
	const auto energy = Players[Player_num].energy;
	const int ordinary_before = num_objects;
	do_laser_firing_player();
	require(num_objects > ordinary_before && Players[Player_num].energy < energy, "ordinary primary did not fire/spend energy");
	GameTime64 += i2f(1);
	Controls.fire_secondary_count = 1;
	const int missile_before = num_objects;
	do_weapon_n_item_stuff();
	require(num_objects > missile_before && Players[Player_num].secondary_ammo[CONCUSSION_INDEX] == 8,
	        "ordinary missile did not fire/spend ammo after leaving protected region");
	Controls = saved_controls;
	GameTime64 = saved_time;
	FrameTime = saved_frame;
	Next_laser_fire_time = saved_laser;
	Next_missile_fire_time = saved_missile;
	Next_flare_fire_time = saved_flare;
	Last_laser_fired_time = saved_last_laser;
	Difficulty_level = saved_difficulty;
	Global_laser_firing_count = saved_primary_queue;
	Global_missile_firing_count = saved_secondary_queue;
	std::cout << "No-damage native firing controls: primary and fresh missiles/bombs blocked, flare and queued missile costs retained, parent-segment boundary and ordinary exit verified\n";
}
