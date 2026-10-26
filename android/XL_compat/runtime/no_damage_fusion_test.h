// Full gameplay frames with controlled placement and an already charged fusion timer
extern "C" void FireLaser(void);
static void no_damage_fusion_test(const xl_compat::Inventory &source, int protected_segment, int ordinary_segment)
{
	const auto saved_controls = Controls;
	const auto saved_time = GameTime64, saved_next = Next_laser_fire_time, saved_auto = Auto_fire_fusion_cannon_time;
	const auto saved_charge = Fusion_charge, saved_frame = FrameTime;
	const int saved_queue = Global_laser_firing_count, saved_suspend = Game_suspended;
	const int saved_difficulty = Difficulty_level;
	const auto move_player = [](int segment) {
		obj_relink(ConsoleObject - Objects, segment);
		compute_segment_center(&ConsoleObject->pos, &Segments[segment]);
		ConsoleObject->mtype.phys_info.velocity = ConsoleObject->mtype.phys_info.rotvel = ConsoleObject->mtype.phys_info.thrust = {};
	};
	const auto shots = [] {
		int count = 0;
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_WEAPON && Objects[i].id == FUSION_ID) ++count;
		return count;
	};
	Controls = {};
	// load_level leaves multiplayer start markers in editor control mode
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type == OBJ_PLAYER) {
			Objects[i].control_type = &Objects[i] == ConsoleObject ? CT_FLYING : CT_NONE;
			if (&Objects[i] != ConsoleObject) Objects[i].movement_type = MT_NONE;
		}
	Game_suspended = SUSP_ROBOTS;
	Difficulty_level = 2;
	GameTime64 = i2f(100);
	FrameTime = F1_0 / 60;
	Players[Player_num].flags = 0;
	Players[Player_num].energy = i2f(100);
	Players[Player_num].shields = ConsoleObject->shields = i2f(100);
	Players[Player_num].primary_weapon = FUSION_INDEX;
	Players[Player_num].primary_weapon_flags |= HAS_FLAG(FUSION_INDEX);
	Fusion_charge = 0;
	Global_laser_firing_count = 0;
	Next_laser_fire_time = GameTime64;
	Auto_fire_fusion_cannon_time = 0;
	move_player(protected_segment);
	Controls.fire_primary_state = 1;
	const int initial_shots = shots();
	for (int frame = 0; frame < 3; ++frame) {
		GameTime64 += FrameTime;
		GameProcessFrame();
		require(ConsoleObject->segnum == protected_segment && !Fusion_charge && !Global_laser_firing_count &&
		            !Auto_fire_fusion_cannon_time && Players[Player_num].energy == i2f(100) && shots() == initial_shots,
		        "fresh protected fusion input charged, spent energy or created a shot");
	}
	Controls = {};
	Fusion_charge = F1_0;
	Auto_fire_fusion_cannon_time = GameTime64 + FrameTime / 4;
	const int before = shots();
	GameTime64 += FrameTime;
	GameProcessFrame();
	require(ConsoleObject->segnum == protected_segment && Fusion_charge == F1_0 &&
	            Global_laser_firing_count == 1 && !Auto_fire_fusion_cannon_time &&
	            Players[Player_num].energy == i2f(100) && shots() == before,
	        "protected fusion timer did not retain the charged pending shot");
	move_player(ordinary_segment);
	GameTime64 += FrameTime;
	GameProcessFrame();
	std::cout << "Fusion exit control: charge=" << Fusion_charge << " queue=" << Global_laser_firing_count
	          << " auto=" << Auto_fire_fusion_cannon_time << " energy=" << Players[Player_num].energy << " shots=" << shots() << '\n';
	require(Fusion_charge == F1_0 + FrameTime && Players[Player_num].energy == i2f(100) - FrameTime &&
	            Auto_fire_fusion_cannon_time > GameTime64 && shots() == before,
	        "fusion exit dropped its queue instead of reference one-frame recharging");
	GameTime64 += FrameTime;
	GameProcessFrame();
	std::cout << "Fusion completion control: charge=" << Fusion_charge << " queue=" << Global_laser_firing_count
	          << " auto=" << Auto_fire_fusion_cannon_time << " energy=" << Players[Player_num].energy << " shots=" << shots() << '\n';
	require(!Fusion_charge && !Auto_fire_fusion_cannon_time && !Global_laser_firing_count &&
	            Players[Player_num].energy == i2f(100) - FrameTime - Weapon_info[FUSION_ID].energy_usage / Weapon_info[FUSION_ID].fire_count &&
	            shots() == before + 2,
	        "fusion pending shot did not emit after exit with retained charge and energy");
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type == OBJ_WEAPON && Objects[i].id == FUSION_ID)
			require(Objects[i].ctype.laser_info.multiplier == F1_0 + (F1_0 + FrameTime) / 2,
			        "fusion exit bolts lost retained charge multiplier");
	// Same queued state without the detected property retains classic FireLaser behavior
	auto neutral = source;
	for (auto &segment : neutral.segments) {
		if (neutral.level_version >= 21) segment.properties &= ~xl_compat::segment_no_damage;
		else if (segment.function == 13) segment.function = 0;
	}
	xl_test_bind_no_damage(neutral);
	Fusion_charge = F1_0;
	Global_laser_firing_count = 1;
	Auto_fire_fusion_cannon_time = 0;
	const auto energy = Players[Player_num].energy;
	FireLaser();
	require(!Global_laser_firing_count && Fusion_charge == F1_0 && Players[Player_num].energy == energy,
	        "fusion queue adapter changed a profile without no-damage segments");
	xl_test_bind_no_damage(source);
	// Switching is a native-selection regression control, not XL's charge-refund policy
	move_player(protected_segment);
	Fusion_charge = F1_0;
	Global_laser_firing_count = 1;
	Auto_fire_fusion_cannon_time = GameTime64 + i2f(1);
	Players[Player_num].primary_weapon_flags |= HAS_FLAG(LASER_INDEX);
	const auto selection_energy = Players[Player_num].energy;
	std::set<int> prior_fusion_signatures;
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type == OBJ_WEAPON && Objects[i].id == FUSION_ID) prior_fusion_signatures.insert(Objects[i].signature);
	do_weapon_select(LASER_INDEX, 0);
	require(Players[Player_num].primary_weapon == LASER_INDEX && !Fusion_charge && !Global_laser_firing_count &&
	            Players[Player_num].energy == selection_energy && !xl_preview_preserve_fusion_queue(),
	        "native weapon switch retained a charged queue or changed energy accounting");
	GameTime64 += FrameTime;
	GameProcessFrame();
	require(!Auto_fire_fusion_cannon_time, "weapon switch did not clear fusion timer on protected frame");
	move_player(ordinary_segment);
	GameTime64 += FrameTime;
	GameProcessFrame();
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type == OBJ_WEAPON && Objects[i].id == FUSION_ID)
			require(prior_fusion_signatures.count(Objects[i].signature), "cancelled fusion shot resurrected after exit");
	require(!Fusion_charge && !Global_laser_firing_count && Players[Player_num].energy == selection_energy,
	        "cancelled fusion queue consumed resources after exit");
	Controls = saved_controls;
	GameTime64 = saved_time;
	Next_laser_fire_time = saved_next;
	Auto_fire_fusion_cannon_time = saved_auto;
	Fusion_charge = saved_charge;
	FrameTime = saved_frame;
	Global_laser_firing_count = saved_queue;
	Game_suspended = saved_suspend;
	Difficulty_level = saved_difficulty;
	std::cout << "No-damage native fusion frames: protected timer queues charge, exit retains reference recharge, following frame emits two bolts\n";
	std::cout << "No-damage fusion input controls: protected fresh input does not charge; native weapon switch cancels timer and cannot resurrect shot; reference refund policy remains different\n";
}
