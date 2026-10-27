extern "C" {
void collide_player_and_wall(object *, fix, short, short, vms_vector *);
int check_volatile_wall(object *, int, int, vms_vector *);
}

// Synthetic contact surfaces on the checked layout, not original-level traversal
static void no_damage_wall_test(int protected_segment, int ordinary_segment)
{
	const auto saved_protected = Segments[protected_segment].sides[0];
	const auto saved_ordinary = Segments[ordinary_segment].sides[0];
	const auto protected_child = Segments[protected_segment].children[0];
	const auto ordinary_child = Segments[ordinary_segment].children[0];
	const auto saved_texture = TmapInfo[0];
	const auto saved_frame = FrameTime;
	unsigned saved_rng = 0;
	d_rand_get_state(&saved_rng);
	for (int segment : { protected_segment, ordinary_segment }) {
		Segments[segment].sides[0].tmap_num = 0;
		Segments[segment].sides[0].tmap_num2 = 0;
		Segments[segment].sides[0].wall_num = -1;
		// XL classifies physical sides, not an arbitrary texture on an open portal
		Segments[segment].children[0] = -1;
	}
	TmapInfo[0].damage = 0;
	TmapInfo[0].flags = 0;
	obj_relink(ConsoleObject - Objects, ordinary_segment);
	compute_segment_center(&ConsoleObject->pos, &Segments[ordinary_segment]);
	const auto reset = [] {
		Players[Player_num].shields = i2f(100);
		PaletteRedAdd = PaletteGreenAdd = PaletteBlueAdd = 0;
		ConsoleObject->mtype.phys_info.rotvel = {};
		ConsoleObject->mtype.phys_info.thrust = {};
		d_srand(1234);
	};
	reset();
	const int awareness = Num_awareness_events;
	collide_player_and_wall(ConsoleObject, i2f(128), protected_segment, 0, &ConsoleObject->pos);
	require(Players[Player_num].shields == i2f(100) && PaletteRedAdd == 0 && Num_awareness_events == awareness,
	        "protected wall hit damaged/flashed/alerted an ordinary-segment player");
	collide_player_and_wall(ConsoleObject, i2f(128), ordinary_segment, 0, &ConsoleObject->pos);
	require(Players[Player_num].shields == i2f(99) && PaletteRedAdd > 0,
	        "ordinary wall control did not cause bump damage and flash");
	TmapInfo[0].flags = TMI_FORCE_FIELD;
	reset();
	collide_player_and_wall(ConsoleObject, i2f(128), protected_segment, 0, &ConsoleObject->pos);
	const auto protected_rotation = ConsoleObject->mtype.phys_info.rotvel;
	std::cout << "Protected force-field control: shields=" << Players[Player_num].shields << " palette=" << PaletteRedAdd << ',' << PaletteBlueAdd
	          << " rotation=" << protected_rotation.x << ',' << protected_rotation.y << ',' << protected_rotation.z << '\n';
	require(Players[Player_num].shields == i2f(100) && PaletteBlueAdd == MAX_PALETTE_ADD && PaletteRedAdd == 0 &&
	            (protected_rotation.x || protected_rotation.y || protected_rotation.z),
	        "protected force field lost blue flash/rotation or inflicted damage");
	reset();
	collide_player_and_wall(ConsoleObject, i2f(128), ordinary_segment, 0, &ConsoleObject->pos);
	require(Players[Player_num].shields == i2f(91) &&
	            !std::memcmp(&protected_rotation, &ConsoleObject->mtype.phys_info.rotvel, sizeof(protected_rotation)),
	        "force-field protection changed the retained rotational response");
	TmapInfo[0].flags = TMI_VOLATILE;
	TmapInfo[0].damage = i2f(16);
	FrameTime = F1_0 / 4;
	reset();
	unsigned before_rng = 0, after_rng = 0;
	d_rand_get_state(&before_rng);
	require(check_volatile_wall(ConsoleObject, protected_segment, 0, &ConsoleObject->pos) == 1,
	        "protected volatile contact lost lava classification");
	d_rand_get_state(&after_rng);
	require(Players[Player_num].shields == i2f(100) && PaletteRedAdd == 0 && before_rng == after_rng &&
	            !ConsoleObject->mtype.phys_info.rotvel.x && !ConsoleObject->mtype.phys_info.rotvel.z,
	        "protected idle volatile contact damaged, flashed or consumed rotational RNG");
	ConsoleObject->mtype.phys_info.thrust.x = F1_0;
	require(check_volatile_wall(ConsoleObject, protected_segment, 0, &ConsoleObject->pos) == 1 &&
	            Players[Player_num].shields == i2f(100) && PaletteRedAdd == 0 &&
	            (ConsoleObject->mtype.phys_info.rotvel.x || ConsoleObject->mtype.phys_info.rotvel.z),
	        "protected thrusting volatile contact lost rotation or applied damage");
	reset();
	require(check_volatile_wall(ConsoleObject, ordinary_segment, 0, &ConsoleObject->pos) == 1 &&
	            Players[Player_num].shields < i2f(100) && PaletteRedAdd > 0,
	        "ordinary volatile control did not damage and flash");
	TmapInfo[0].flags = TMI_WATER;
	TmapInfo[0].damage = 0;
	reset();
	require(check_volatile_wall(ConsoleObject, protected_segment, 0, &ConsoleObject->pos) == 2 &&
	            Players[Player_num].shields == i2f(100) && !ConsoleObject->mtype.phys_info.rotvel.x && !ConsoleObject->mtype.phys_info.rotvel.z,
	        "protected idle water contact classification or rotation differs");
	ConsoleObject->mtype.phys_info.thrust.z = F1_0;
	require(check_volatile_wall(ConsoleObject, protected_segment, 0, &ConsoleObject->pos) == 2 &&
	            (ConsoleObject->mtype.phys_info.rotvel.x || ConsoleObject->mtype.phys_info.rotvel.z),
	        "protected thrusting water contact lost rotation");
	TmapInfo[0] = saved_texture;
	Segments[protected_segment].sides[0] = saved_protected;
	Segments[ordinary_segment].sides[0] = saved_ordinary;
	Segments[protected_segment].children[0] = protected_child;
	Segments[ordinary_segment].children[0] = ordinary_child;
	FrameTime = saved_frame;
	d_rand_set_state(saved_rng);
	std::cout << "No-damage native wall controls: hit-segment damage suppression, retained force-field response, lava/water classification and thrust-dependent rotation verified\n";
}
