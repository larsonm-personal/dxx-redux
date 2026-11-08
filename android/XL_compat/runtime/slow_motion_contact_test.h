#ifndef XL_SLOW_MOTION_CONTACT_TEST_H
#define XL_SLOW_MOTION_CONTACT_TEST_H
extern "C" void physics_turn_towards_vector(vms_vector *, object *, fix);
static void slow_motion_turn_controls(std::ostream &samples, int profile)
{
	const int saved_mode = Game_mode;
	object *saved_console = ConsoleObject;
	object actor{};
	actor.type = OBJ_ROBOT;
	const vms_angvec initial{ 1234, -567, 2345 };
	vm_angles_2_matrix(&actor.orient, &initial);
	const vms_vector goals[] = { { F1_0, 0, 0 }, { -F1_0, 0, 0 }, { 0, F1_0, 0 }, { 0, -F1_0, 0 }, { 0, 0, F1_0 }, { 0, 0, -F1_0 }, { 127, -257, F1_0 }, { -1001, 2003, F1_0 } };
	const auto calls = d_rand_get_stream_call_count(D_RNG_SIM);
	for (int console : { 0, 1 })
		for (int multiplayer : { 0, 1 })
			for (int morph : { 0, 1 })
				for (int goal_index = 0; goal_index < 8; ++goal_index)
					for (fix rate : { F1_0 / 4, F1_0, F1_0 * 4 })
						for (int sign : { -1, 1 }) {
							ConsoleObject = console ? &actor : saved_console;
							Game_mode = multiplayer ? GM_MULTI : saved_mode;
							actor.control_type = morph ? CT_MORPH : CT_NONE;
							actor.mtype.phys_info.rotvel = { sign * 8191, -sign * 8192, 123 };
							auto goal = goals[goal_index];
							vms_angvec destination, current;
							vm_extract_angles_vector(&destination, &goal);
							vm_extract_angles_vector(&current, &actor.orient.fvec);
							physics_turn_towards_vector(&goal, &actor, rate);
							const auto result = actor.mtype.phys_info.rotvel;
							samples << "T " << profile << ' ' << console << ' ' << multiplayer << ' ' << morph << ' ' << goal_index << ' ' << rate << ' ' << sign << ' '
							        << destination.p << ' ' << destination.h << ' ' << current.p << ' ' << current.h << ' '
							        << result.x << ' ' << result.y << ' ' << result.z << '\n';
							vms_vector zero{};
							physics_turn_towards_vector(&zero, &actor, rate);
							require(!std::memcmp(&result, &actor.mtype.phys_info.rotvel, sizeof(result)), "Zero turn impulse changed angular velocity");
						}
	require(d_rand_get_stream_call_count(D_RNG_SIM) == calls, "Turn impulse scaling consumed gameplay RNG");
	ConsoleObject = saved_console;
	Game_mode = saved_mode;
}
static void slow_motion_wall_controls(std::ostream &samples, int profile)
{
	const int saved_mode = Game_mode;
	object *saved_console = ConsoleObject;
	const fix saved_time = FrameTime;
	for (int room : { 60, 61 }) {
		int wall = -1;
		for (int side = 0; side < 6; ++side) {
			const auto n = Segments[room].sides[side].normals[0];
			if (Segments[room].children[side] == -1 && Segments[room].sides[side].wall_num == -1 &&
			    abs(n.x) + abs(n.y) + abs(n.z) == F1_0 &&
			    !(TmapInfo[Segments[room].sides[side].tmap_num].flags & (TMI_FORCE_FIELD | TMI_VOLATILE | TMI_WATER))) wall = side;
		}
		require(wall >= 0, "Slow contact fixture needs an axis-aligned ordinary wall");
		vms_vector face;
		compute_center_point_on_side(&face, &Segments[room], wall);
		const auto normal = Segments[room].sides[wall].normals[0];
		for (int console : { 0, 1 })
			for (int multiplayer : { 0, 1 })
				for (int bounce : { 0, 1 }) {
					Game_mode = multiplayer ? GM_MULTI : saved_mode;
					const int index = obj_create(OBJ_WEAPON, LASER_ID_L1, room, &face, &vmd_identity_matrix, F1_0 / 8, CT_NONE, MT_PHYSICS, RT_NONE);
					require(index >= 0, "Slow contact actor allocation failed");
					auto &actor = Objects[index];
					actor.mtype.phys_info = {};
					actor.mtype.phys_info.mass = F1_0;
					actor.mtype.phys_info.flags = bounce ? PF_BOUNCE : 0;
					actor.ctype.laser_info.parent_type = OBJ_PLAYER;
					actor.ctype.laser_info.parent_num = Players[Player_num].objnum;
					actor.ctype.laser_info.parent_signature = saved_console->signature;
					vm_vec_scale_add(&actor.pos, &face, &normal, actor.size + F1_0 / 16);
					vm_vec_copy_scale(&actor.mtype.phys_info.velocity, &normal, -i2f(4));
					auto start = actor.pos, velocity = actor.mtype.phys_info.velocity;
					ConsoleObject = console ? &actor : saved_console;
					FrameTime = F1_0 / 4;
					vms_vector offset, destination;
					vm_vec_copy_scale(&offset, &velocity, xl_preview_motion_time(room, FrameTime));
					xl_slow_motion_offset(&actor, &offset);
					vm_vec_add(&destination, &start, &offset);
					fvi_query query{};
					int ignored[] = { -1 };
					query.p0 = &start;
					query.p1 = &destination;
					query.startseg = room;
					query.rad = actor.size;
					query.thisobjnum = index;
					query.ignore_obj_list = ignored;
					query.flags = FQ_CHECK_OBJS | FQ_TRANSPOINT;
					fvi_info hit{};
					require(find_vector_intersection(&query, &hit) == HIT_WALL && hit.hit_side == wall && hit.hit_seg == room, "Slow contact first hit differs from fixture wall");
					do_physics_sim(&actor);
					require(actor.segnum == room && bool(actor.flags & OF_SHOULD_BE_DEAD) == !bounce, "Slow contact termination/bounce differs");
					if (!bounce)
						require(!std::memcmp(&velocity, &actor.mtype.phys_info.velocity, sizeof(velocity)), "Terminating slow contact reconstructed velocity");
					samples << "C " << profile << ' ' << console << ' ' << multiplayer << ' ' << room << ' ' << bounce;
					for (const auto &v : { start, velocity, hit.hit_pnt, hit.hit_wallnorm, actor.pos, actor.mtype.phys_info.velocity }) samples << ' ' << v.x << ' ' << v.y << ' ' << v.z;
					samples << '\n';
					ConsoleObject = saved_console;
					obj_delete(index);
				}
	}
	Game_mode = saved_mode;
	FrameTime = saved_time;
}
#endif
