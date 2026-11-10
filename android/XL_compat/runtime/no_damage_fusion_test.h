extern "C" void CyclePrimary(void);
// Explicit selection refunds XL charge before availability/toggle checks; lower-level callers do not
static void fusion_selection_refund_controls(bool xl)
{
	const auto saved_player = Players[Player_num];
	const auto saved_cfg = PlayerCfg;
	const auto saved_charge = Fusion_charge;
	const auto saved_next = Next_laser_fire_time, saved_missile = Next_missile_fire_time, saved_auto = Auto_fire_fusion_cannon_time;
	const int saved_mode = Game_mode, saved_queue = Global_laser_firing_count, saved_missile_queue = Global_missile_firing_count;
	std::array<ubyte, MAX_PRIMARY_WEAPONS> saved_primary;
	std::array<ubyte, MAX_SECONDARY_WEAPONS> saved_secondary;
	std::memcpy(saved_primary.data(), Primary_last_was_super, sizeof(Primary_last_was_super));
	std::memcpy(saved_secondary.data(), Secondary_last_was_super, sizeof(Secondary_last_was_super));
	unsigned saved_rng = 0;
	d_rand_get_state(&saved_rng);
	const auto saved_calls = d_rand_get_call_count();
	for (const bool multiplayer : { false, true })
		for (const fix charge : { 0, 1, F1_0 / 2, -F1_0 / 2, F1_0 })
			for (const int action : { 0, 1, 2, 3, 4, 5, 6, 7 }) {
				Game_mode = multiplayer ? GM_NETWORK : 0;
				auto &player = Players[Player_num];
				player.primary_weapon = FUSION_INDEX;
				player.secondary_weapon = CONCUSSION_INDEX;
				player.primary_weapon_flags = HAS_FLAG(LASER_INDEX) | HAS_FLAG(FUSION_INDEX);
				player.secondary_weapon_flags = HAS_FLAG(CONCUSSION_INDEX) | HAS_FLAG(HOMING_INDEX);
				player.secondary_ammo[HOMING_INDEX] = 10;
				player.laser_level = 0;
				player.energy = i2f(200); // Refund is not capped
				std::memset(Primary_last_was_super, 0, sizeof(Primary_last_was_super));
				std::memset(Secondary_last_was_super, 0, sizeof(Secondary_last_was_super));
				if (action == 3) {
					player.primary_weapon = VULCAN_INDEX;
					player.primary_weapon_flags |= HAS_FLAG(VULCAN_INDEX) | HAS_FLAG(GAUSS_INDEX);
					player.primary_ammo[VULCAN_INDEX] = 100;
				}
				if (action == 7) {
					for (auto &slot : PlayerCfg.PrimaryOrder) slot = 255;
					PlayerCfg.PrimaryOrder[0] = FUSION_INDEX;
					PlayerCfg.PrimaryOrder[1] = LASER_INDEX;
					PlayerCfg.CycleAutoselectOnly = 0;
				}
				Fusion_charge = charge;
				Global_laser_firing_count = 7;
				Auto_fire_fusion_cannon_time = GameTime64 + F1_0;
				Next_laser_fire_time = Next_missile_fire_time = GameTime64;
				unsigned before_rng = 0, after_rng = 0;
				d_rand_get_state(&before_rng);
				const auto before_calls = d_rand_get_call_count();
				const bool explicit_primary = action <= 3;
				const bool refunded = explicit_primary && xl && !multiplayer;
				const bool changed_primary = action == 0 || action == 3 || action == 5 || action == 7;
				switch (action) {
					case 0: do_weapon_select(LASER_INDEX, 0); break;
					case 1: do_weapon_select(PLASMA_INDEX, 0); break; // Both requested variants unavailable
					case 2: do_weapon_select(FUSION_INDEX, 0); break; // Same-slot toggle unavailable
					case 3: do_weapon_select(VULCAN_INDEX, 0); break; // Owned normal/super toggle
					case 4: do_weapon_select(HOMING_INDEX, 1); break;
					case 5: select_weapon(LASER_INDEX, 0, 0, 0); break;
					case 6: auto_select_weapon(0); break; // Current Fusion is usable
					case 7: CyclePrimary(); break;
				}
				d_rand_get_state(&after_rng);
				require(player.energy == i2f(200) + (refunded ? charge : 0) &&
				            Fusion_charge == ((refunded || changed_primary) ? 0 : charge),
				        "Fusion explicit selection refund, profile scope or lower-level charge behavior differs");
				require(Global_laser_firing_count == (changed_primary ? 0 : 7) &&
				            Auto_fire_fusion_cannon_time == GameTime64 + F1_0,
				        "Fusion failed/same-slot/secondary selection changed queue or timer semantics");
				const int expected_primary = action == 0 || action == 5 || action == 7 ? LASER_INDEX : action == 3 ? GAUSS_INDEX
				                                                                                                   : FUSION_INDEX;
				require(player.primary_weapon == expected_primary && player.secondary_weapon == (action == 4 ? HOMING_INDEX : CONCUSSION_INDEX),
				        "Fusion refund changed selection availability or toggle disposition");
				require(before_rng == after_rng && before_calls == d_rand_get_call_count(),
				        "Fusion selection changed simulation RNG");
				if (explicit_primary) {
					const auto energy = player.energy, remaining_charge = Fusion_charge;
					const int queue = Global_laser_firing_count;
					do_weapon_select(action == 1 ? PLASMA_INDEX : action == 2 ? FUSION_INDEX
					                                          : action == 3   ? VULCAN_INDEX
					                                                          : LASER_INDEX,
					                 0);
					require(player.energy == energy && Fusion_charge == remaining_charge && Global_laser_firing_count == queue,
					        "Repeated explicit selection refunded charge twice or lost a pending failed-selection queue");
				}
			}
	Players[Player_num] = saved_player;
	PlayerCfg = saved_cfg;
	Fusion_charge = saved_charge;
	Next_laser_fire_time = saved_next;
	Next_missile_fire_time = saved_missile;
	Auto_fire_fusion_cannon_time = saved_auto;
	Game_mode = saved_mode;
	Global_laser_firing_count = saved_queue;
	Global_missile_firing_count = saved_missile_queue;
	std::memcpy(Primary_last_was_super, saved_primary.data(), sizeof(Primary_last_was_super));
	std::memcpy(Secondary_last_was_super, saved_secondary.data(), sizeof(Secondary_last_was_super));
	d_rand_set_state(saved_rng);
	d_rand_set_call_count(saved_calls);
	std::cout << "Fusion explicit selection refund and lower-level/profile controls passed; XL=" << xl << "\n";
}

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
	const bool selection_xl = xl_level_extensions_active() && !d1_in_d2_use_d1_gameplay();
	move_player(protected_segment);
	fusion_selection_refund_controls(selection_xl);
	move_player(ordinary_segment);
	fusion_selection_refund_controls(selection_xl);
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
	// Explicit selection refunds XL charge and cancels the pending successful-switch queue
	move_player(protected_segment);
	Fusion_charge = F1_0;
	Global_laser_firing_count = 1;
	Auto_fire_fusion_cannon_time = GameTime64 + i2f(1);
	Players[Player_num].primary_weapon_flags |= HAS_FLAG(LASER_INDEX);
	const auto selection_energy = Players[Player_num].energy + (selection_xl ? Fusion_charge : 0);
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
	std::cout << "No-damage fusion input controls: protected fresh input does not charge; native weapon switch cancels timer and cannot resurrect shot; explicit primary selection refunds XL held charge\n";
}
