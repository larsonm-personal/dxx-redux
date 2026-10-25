// Ordinary weapon dispatch with explicit boundary placement; not flight traversal
static void no_damage_guided_test(int protected_segment, int ordinary_segment)
{
	const auto saved_controls = Controls;
	const auto saved_time = GameTime64, saved_next = Next_missile_fire_time;
	const auto saved_frame = FrameTime;
	const int saved_queue = Global_missile_firing_count;
	const auto saved_guided = Guided_missile[Player_num], saved_viewer = Missile_viewer;
	const int saved_guided_sig = Guided_missile_sig[Player_num], saved_viewer_sig = Missile_viewer_sig;
	const auto move_player = [](int segment) {
		obj_relink(ConsoleObject - Objects, segment);
		compute_segment_center(&ConsoleObject->pos, &Segments[segment]);
	};
	Controls = {};
	GameTime64 = i2f(100);
	FrameTime = F1_0 / 60;
	Next_missile_fire_time = GameTime64;
	Global_missile_firing_count = 0;
	Guided_missile[Player_num] = Missile_viewer = nullptr;
	Players[Player_num].secondary_weapon = GUIDED_INDEX;
	Players[Player_num].secondary_weapon_flags |= HAS_FLAG(GUIDED_INDEX);
	Players[Player_num].secondary_ammo[GUIDED_INDEX] = 5;
	move_player(ordinary_segment);
	Controls.fire_secondary_count = 1;
	do_weapon_n_item_stuff();
	auto *guided = Guided_missile[Player_num];
	require(guided && guided->type == OBJ_WEAPON && guided->id == GUIDEDMISS_ID &&
	            Guided_missile_sig[Player_num] == guided->signature && Players[Player_num].secondary_ammo[GUIDED_INDEX] == 4,
	        "ordinary guided launch did not acquire control and spend ammo");
	const int first = guided - Objects;
	move_player(protected_segment);
	GameTime64 += i2f(2);
	const auto before = *guided;
	const auto next_before = Next_missile_fire_time;
	const int count_before = num_objects;
	Controls.fire_secondary_count = 1;
	do_weapon_n_item_stuff();
	require(Guided_missile[Player_num] == guided && !std::memcmp(guided, &before, sizeof(before)) &&
	            Players[Player_num].secondary_ammo[GUIDED_INDEX] == 4 && num_objects == count_before &&
	            Next_missile_fire_time == next_before && !Global_missile_firing_count && !Missile_viewer,
	        "protected fresh fire released/changed the guided missile or consumed resources");
	move_player(ordinary_segment);
	Controls.fire_secondary_count = 1;
	do_weapon_n_item_stuff();
	require(!Guided_missile[Player_num] && Missile_viewer == guided && Missile_viewer_sig == guided->signature &&
	            Players[Player_num].secondary_ammo[GUIDED_INDEX] == 4 && num_objects == count_before &&
	            Next_missile_fire_time > GameTime64,
	        "leaving protection did not permit normal guided release without another shot");
	GameTime64 += i2f(2);
	Controls.fire_secondary_count = 1;
	do_weapon_n_item_stuff();
	guided = Guided_missile[Player_num];
	std::cout << "Guided relaunch control: guided=" << (guided ? int(guided - Objects) : -1) << " first=" << first
	          << " ammo=" << Players[Player_num].secondary_ammo[GUIDED_INDEX] << " selected=" << int(Players[Player_num].secondary_weapon)
	          << " objects=" << num_objects << " next=" << Next_missile_fire_time << " time=" << GameTime64 << '\n';
	require(guided && guided != &Objects[first] && Players[Player_num].secondary_ammo[GUIDED_INDEX] == 3,
	        "ordinary guided relaunch failed");
	const int second = guided - Objects;
	move_player(protected_segment);
	const auto queued_before = *guided;
	const int queued_count = num_objects;
	GameTime64 += i2f(2);
	Global_missile_firing_count = 1;
	do_weapon_n_item_stuff();
	require(!Guided_missile[Player_num] && Missile_viewer == guided && Missile_viewer_sig == guided->signature &&
	            !std::memcmp(guided, &queued_before, sizeof(queued_before)) && num_objects == queued_count &&
	            Players[Player_num].secondary_ammo[GUIDED_INDEX] == 3 && !Global_missile_firing_count &&
	            Next_missile_fire_time > GameTime64,
	        "protected queued guided release did not precede ammo/creation processing");
	Guided_missile[Player_num] = saved_guided;
	Guided_missile_sig[Player_num] = saved_guided_sig;
	Missile_viewer = saved_viewer;
	Missile_viewer_sig = saved_viewer_sig;
	obj_delete(first);
	obj_delete(second);
	Controls = saved_controls;
	GameTime64 = saved_time;
	Next_missile_fire_time = saved_next;
	FrameTime = saved_frame;
	Global_missile_firing_count = saved_queue;
	std::cout << "No-damage native guided controls: fresh protected fire retains control, ordinary exit releases, queued protected fire releases without ammo or projectile changes\n";
}
