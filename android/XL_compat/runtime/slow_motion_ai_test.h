#ifndef XL_SLOW_MOTION_AI_TEST_H
#define XL_SLOW_MOTION_AI_TEST_H
static void slow_motion_ai_controls(std::ostream &samples, int profile)
{
	const int saved_mode = Game_mode;
	const fix saved_time = FrameTime;
	const auto player_position = ConsoleObject->pos;
	const int player_room = ConsoleObject->segnum;
	const auto definition = Robot_info[0];
	vms_vector center;
	compute_segment_center(&center, &Segments[60]);
	int far_room = -1;
	fix max_distance = 0;
	for (int room = 0; room < Num_segments; ++room) {
		vms_vector point;
		compute_segment_center(&point, &Segments[room]);
		const auto distance = vm_vec_dist_quick(&center, &point);
		if (distance > max_distance) {
			max_distance = distance;
			far_room = room;
		}
	}
	require(far_room >= 0 && max_distance > i2f(300), "AI clock fixture lacks a distant room");
	obj_relink(ConsoleObject - Objects, far_room);
	compute_segment_center(&ConsoleObject->pos, &Segments[far_room]);
	const int index = obj_create(OBJ_ROBOT, 0, 60, &center, &vmd_identity_matrix, F1_0 / 8, CT_AI, MT_NONE, RT_NONE);
	require(index >= 0, "AI clock actor allocation failed");
	auto &actor = Objects[index];
	init_ai_object(index, AIB_STATION, -1);
	require(!definition.companion && !definition.thief, "AI clock fixture needs an ordinary robot");
	const auto initial_actor = actor;
	const fix floor = fix(8 * F1_0 * xl_runtime::slow_motion_state().clocks[0].speed);
	const fix times[] = { -24 * F1_0 - 1, -24 * F1_0, -24 * F1_0 + 1, -8 * F1_0 - 1, -8 * F1_0, -8 * F1_0 + 1, -floor - 1, -floor, -floor + 1, 0, F1_0 };
	for (int multiplayer : { 0, 1 })
		for (int secondary : { -1, 0 })
			for (int sample = 0; sample < 11; ++sample)
				for (fix time : { F1_0 / 1000, F1_0 / 60, F1_0 / 4 })
					for (fix elapsed : { 0, 256 * F1_0 - 1, 256 * F1_0 }) {
						Game_mode = multiplayer ? GM_MULTI : saved_mode;
						Robot_info[0].weapon_type2 = secondary;
						actor = initial_actor;
						actor.ctype.ai_info.behavior = AIB_STATION;
						actor.ctype.ai_info.hide_segment = -1;
						actor.ctype.ai_info.SKIP_AI_COUNT = 0;
						auto &local = Ai_local_info[index];
						local = {};
						local.mode = AIM_FOLLOW_PATH;
						local.next_fire = times[sample];
						local.next_fire2 = times[sample] - 1;
						local.time_since_processed = elapsed;
						FrameTime = time;
						do_ai_frame(&actor);
						samples << "A " << profile << ' ' << multiplayer << ' ' << secondary << ' ' << sample << ' ' << time << ' ' << elapsed << ' '
						        << local.next_fire << ' ' << local.next_fire2 << ' ' << local.time_since_processed << '\n';
					}
	Robot_info[0] = definition;
	obj_delete(index);
	obj_relink(ConsoleObject - Objects, player_room);
	ConsoleObject->pos = player_position;
	Game_mode = saved_mode;
	FrameTime = saved_time;
}
static void slow_motion_ai_turn_controls(std::ostream &samples, int profile)
{
	const int saved_mode = Game_mode;
	const fix saved_time = FrameTime, saved_seismic = Seismic_tremor_magnitude;
	object *saved_console = ConsoleObject;
	object actor{};
	actor.type = OBJ_ROBOT;
	Seismic_tremor_magnitude = 0;
	const vms_vector goals[] = { { F1_0, 0, 0 }, { 0, 0, F1_0 }, { 0, 0, -F1_0 }, { 1001, 0, 65528 } };
	const auto calls = d_rand_get_stream_call_count(D_RNG_SIM);
	for (int multiplayer : { 0, 1 })
		for (int console : { 0, 1 })
			for (int goal_index = 0; goal_index < 4; ++goal_index)
				for (fix rate : { F1_0 / 4, F1_0, F1_0 * 4 })
					for (fix time : { F1_0 / 1000, F1_0 / 60, F1_0 / 4 }) {
						Game_mode = multiplayer ? GM_MULTI : saved_mode;
						ConsoleObject = console ? &actor : saved_console;
						FrameTime = time;
						actor.id = 0;
						actor.orient = vmd_identity_matrix;
						auto goal = goals[goal_index], forward = goal;
						const fix dot = xl_slow_motion_world_time(vm_vec_dot(&goal, &actor.orient.fvec));
						const fix scale = xl_slow_motion_world_time(fixdiv(time, rate));
						if (dot < F1_0 - time / 2) {
							vm_vec_scale(&forward, scale);
							vm_vec_add2(&forward, &actor.orient.fvec);
							if (vm_vec_normalize_quick(&forward) < F1_0 / 256) forward = goal;
						}
						vms_matrix expected;
						vm_vector_2_matrix(&expected, &forward, nullptr, &actor.orient.rvec);
						ai_turn_towards_vector(&goal, &actor, rate);
						require(!std::memcmp(&expected, &actor.orient, sizeof(expected)), "AI world-clock turn hook differs");
						samples << "B " << profile << ' ' << multiplayer << ' ' << console << ' ' << goal_index << ' ' << rate << ' ' << time << ' ' << dot << ' ' << scale << '\n';
						// The baby-spider branch keeps the separate impulse consumer
						actor.id = 14;
						actor.orient = vmd_identity_matrix;
						actor.mtype.phys_info.rotvel = { -8191, 8192, 123 };
						const auto before = actor;
						ai_turn_towards_vector(&goal, &actor, rate);
						const auto actual = actor.mtype.phys_info.rotvel;
						actor = before;
						physics_turn_towards_vector(&goal, &actor, rate);
						require(!std::memcmp(&actual, &actor.mtype.phys_info.rotvel, sizeof(actual)), "Baby spider lost impulse clock dispatch");
						const auto prior = actor;
						ai_turn_towards_vector(&goal, &actor, 0);
						require(!std::memcmp(&prior.orient, &actor.orient, sizeof(actor.orient)) && !std::memcmp(&prior.mtype.phys_info.rotvel, &actor.mtype.phys_info.rotvel, sizeof(actual)), "Zero-rate AI turn changed state");
					}
	require(d_rand_get_stream_call_count(D_RNG_SIM) == calls, "AI turn consumed RNG without seismic activity");
	Seismic_tremor_magnitude = saved_seismic;
	Game_mode = saved_mode;
	FrameTime = saved_time;
	ConsoleObject = saved_console;
}
#endif
