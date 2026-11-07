#ifndef XL_SLOW_MOTION_ROTATION_TEST_H
#define XL_SLOW_MOTION_ROTATION_TEST_H
extern "C" void do_physics_sim_rot(object *);
extern "C" void check_and_fix_matrix(vms_matrix *);
static void slow_motion_rotation_controls(std::ostream &samples, int profile)
{
	const int saved_mode = Game_mode;
	object *saved_console = ConsoleObject;
	const fix saved_time = FrameTime;
	object actor{};
	actor.type = OBJ_ROBOT;
	actor.orient = vmd_identity_matrix;
	actor.mtype.phys_info.mass = F1_0;
	const auto calls = d_rand_get_stream_call_count(D_RNG_SIM);
	for (int console : { 0, 1 })
		for (int multiplayer : { 0, 1 }) {
			ConsoleObject = console ? &actor : saved_console;
			Game_mode = multiplayer ? GM_MULTI : saved_mode;
			for (int value : { -32768, -32767, -1001, -1, 0, 1, 1001, 32766, 32767 }) {
				vms_angvec angles{ fixang(value), fixang(value / 3), fixang(-value - 1) };
				xl_slow_motion_rotation(&actor, &angles);
				samples << "R " << profile << ' ' << console << ' ' << multiplayer << ' ' << value << ' '
				        << angles.p << ' ' << angles.h << ' ' << angles.b << '\n';
				// Verify the hook in actual physics uses the scaled angle matrix
				// Native matrix/trig fidelity itself is unchanged by this stage
				for (int turns : { 0, 1 }) {
					actor.orient = vmd_identity_matrix;
					actor.mtype.phys_info = {};
					actor.mtype.phys_info.mass = F1_0;
					const vms_vector velocity{ value * 4 + turns * 262144, (-value - 1) * 4 + turns * 262144, (value / 3) * 4 + turns * 262144 };
					actor.mtype.phys_info.rotvel = velocity;
					FrameTime = F1_0 / 4;
					vms_matrix rotation, expected;
					vm_angles_2_matrix(&rotation, &angles);
					vm_matrix_x_matrix(&expected, &actor.orient, &rotation);
					check_and_fix_matrix(&expected);
					do_physics_sim_rot(&actor);
					require(!std::memcmp(&actor.orient, &expected, sizeof(expected)), "Slow rotation hook/matrix ordering differs");
					require(!std::memcmp(&actor.mtype.phys_info.rotvel, &velocity, sizeof(velocity)), "Slow rotation changed angular velocity");
				}
			}
			// Thrust, drag and turn-roll still consume real frame time. Compare
			// identical native updates with slowdown bypassed by network isolation
			for (int flags : { 0, PF_USES_THRUST, PF_FREE_SPINNING }) {
				actor.orient = vmd_identity_matrix;
				actor.mtype.phys_info = {};
				actor.mtype.phys_info.mass = F1_0;
				actor.mtype.phys_info.drag = F1_0 / 32;
				actor.mtype.phys_info.flags = flags | PF_TURNROLL;
				actor.mtype.phys_info.rotvel = { -1001, 2003, -3007 };
				actor.mtype.phys_info.rotthrust = { 127, -257, 509 };
				actor.mtype.phys_info.turnroll = 123;
				FrameTime = F1_0 / 60;
				const auto initial = actor;
				do_physics_sim_rot(&actor);
				const auto slowed = actor.mtype.phys_info;
				actor = initial;
				Game_mode = GM_MULTI;
				do_physics_sim_rot(&actor);
				require(!std::memcmp(&slowed.rotvel, &actor.mtype.phys_info.rotvel, sizeof(slowed.rotvel)) && slowed.turnroll == actor.mtype.phys_info.turnroll, "Slow rotation scaled thrust/drag/turnroll time");
				Game_mode = multiplayer ? GM_MULTI : saved_mode;
			}
		}
	require(d_rand_get_stream_call_count(D_RNG_SIM) == calls, "Slow rotation consumed gameplay RNG");
	ConsoleObject = saved_console;
	Game_mode = saved_mode;
	FrameTime = saved_time;
}
#endif
