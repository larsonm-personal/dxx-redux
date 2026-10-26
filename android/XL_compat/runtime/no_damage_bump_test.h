// Controlled singleplayer dispatch; this does not qualify multiplayer synchronization
static void no_damage_bump_test(const xl_compat::Inventory &source, int segment)
{
	const auto saved_time = GameTime64;
	const auto saved_delay = collide_get_collision_delay_last_play_time();
	unsigned saved_rng = 0;
	d_rand_get_state(&saved_rng);
	GameTime64 = i2f(100);
	int ordinary_segment = -1;
	for (int i = 0; i < Num_segments; ++i)
		if (source.segments[i].function == 0) ordinary_segment = i;
	require(ordinary_segment >= 0, "object collision boundary control needs an ordinary segment");
	obj_relink(ConsoleObject - Objects, segment);
	compute_segment_center(&ConsoleObject->pos, &Segments[segment]);
	auto neutral = source;
	neutral.segments[segment].function = 0;
	for (auto type : { OBJ_CLUTTER, OBJ_PLAYER }) {
		const int id = type == OBJ_PLAYER ? (Player_num + 1) % MAX_PLAYERS : 0;
		const int other_index = obj_create(type, id, segment, &ConsoleObject->pos, &ConsoleObject->orient,
		                                   F1_0, CT_NONE, MT_PHYSICS, RT_NONE);
		require(other_index >= 0, "cannot create no-damage bump control");
		auto &other = Objects[other_index];
		other.mtype.phys_info.mass = ConsoleObject->mtype.phys_info.mass;
		const auto reset = [&] {
			Players[Player_num].shields = ConsoleObject->shields = other.shields = i2f(100);
			ConsoleObject->mtype.phys_info.velocity = { i2f(10), 0, 0 };
			other.mtype.phys_info.velocity = { -i2f(10), 0, 0 };
			ConsoleObject->mtype.phys_info.rotvel = other.mtype.phys_info.rotvel = {};
			collide_set_collision_delay_last_play_time(GameTime64 - i2f(2));
			d_srand(9012);
		};
		xl_test_bind_no_damage(source);
		reset();
		const auto player_before = *ConsoleObject, other_before = other;
		unsigned before_rng = 0, after_rng = 0;
		d_rand_get_state(&before_rng);
		vms_vector hit = ConsoleObject->pos;
		collide_two_objects(ConsoleObject, &other, &hit);
		d_rand_get_state(&after_rng);
		require(!std::memcmp(ConsoleObject, &player_before, sizeof(player_before)) &&
		            !std::memcmp(&other, &other_before, sizeof(other_before)) && Players[Player_num].shields == i2f(100) &&
		            collide_get_collision_delay_last_play_time() == GameTime64 - i2f(2) && before_rng == after_rng,
		        "protected object collision changed response, damage or sound-gate state");
		xl_test_bind_no_damage(neutral);
		reset();
		collide_two_objects(ConsoleObject, &other, &hit);
		require(ConsoleObject->mtype.phys_info.velocity.x != player_before.mtype.phys_info.velocity.x &&
		            other.mtype.phys_info.velocity.x != other_before.mtype.phys_info.velocity.x &&
		            collide_get_collision_delay_last_play_time() != GameTime64 - i2f(2),
		        "ordinary object collision did not bump both actors or enter sound gate");
		// Direct dispatch boundary control: the two actors occupy different segment types
		xl_test_bind_no_damage(source);
		obj_relink(other_index, ordinary_segment);
		reset();
		collide_two_objects(&other, ConsoleObject, &hit);
		if (type == OBJ_PLAYER)
			require(ConsoleObject->mtype.phys_info.velocity.x != player_before.mtype.phys_info.velocity.x &&
			            collide_get_collision_delay_last_play_time() != GameTime64 - i2f(2),
			        "player-pair gate incorrectly used the second player's protected segment");
		else
			require(ConsoleObject->mtype.phys_info.velocity.x == player_before.mtype.phys_info.velocity.x &&
			            collide_get_collision_delay_last_play_time() == GameTime64 - i2f(2),
			        "clutter dispatch used clutter segment instead of protected player segment");
		obj_delete(other_index);
	}
	xl_test_bind_no_damage(source);
	GameTime64 = saved_time;
	collide_set_collision_delay_last_play_time(saved_delay);
	d_rand_set_state(saved_rng);
	std::cout << "No-damage native bump controls: player/clutter and player/player dispatch suppresses response and sound-gate state; ordinary controls bump both objects\n";
}
