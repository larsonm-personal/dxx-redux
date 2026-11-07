#ifndef XL_SLOW_MOTION_PHYSICS_TEST_H
#define XL_SLOW_MOTION_PHYSICS_TEST_H
#include "slow_motion_runtime.h"
#include <iomanip>
static void slow_motion_physics_controls(const std::filesystem::path &root)
{
	std::ofstream samples(root.parent_path() / "slow-motion-physics.txt");
	samples << "slow-motion-physics-v1\n"
	        << std::setprecision(9);
	unsigned count = 0;
	const int saved_mode = Game_mode;
	for (int profile = 0; profile < 4; ++profile) {
		xl_slow_motion_reset();
		xl_compat::SlowMotionInput input;
		input.equipment = xl_compat::slow_motion_equipment;
		input.energy = 100 * F1_0;
		if (profile) {
			input.slow_pressed = true;
			xl_runtime::slow_motion_step(input);
			input.slow_pressed = false;
			for (int step = 0; step < (profile == 1 ? 1 : 100); ++step) {
				input.now_ms += 26;
				xl_runtime::slow_motion_step(input);
			}
		}
		if (profile == 3) {
			input.bullet_pressed = true;
			xl_runtime::slow_motion_step(input);
			input.bullet_pressed = false;
			for (int step = 0; step < 100; ++step) {
				input.now_ms += 26;
				xl_runtime::slow_motion_step(input);
			}
		}
		const auto clocks = xl_runtime::slow_motion_state().clocks;
		if (profile == 3) require(clocks[0].speed == 3 && clocks[1].speed == 1, "Native bullet time did not separate clocks");
		for (const auto type : { OBJ_PLAYER, OBJ_ROBOT, OBJ_WEAPON, OBJ_POWERUP }) {
			vms_vector center;
			compute_segment_center(&center, &Segments[60]);
			const int index = obj_create(type, 0, 60, &center, &vmd_identity_matrix, F1_0 / 8, CT_NONE, MT_PHYSICS, RT_NONE);
			require(index >= 0, "Slow-motion actor allocation failed");
			auto &actor = Objects[index];
			actor.shields = i2f(100);
			actor.mtype.phys_info = {};
			actor.mtype.phys_info.mass = F1_0;
			for (int console : { 0, 1 })
				for (int multiplayer : { 0, 1 })
					for (int segment : { 60, 61 })
						for (fix time : { F1_0 / 1000, F1_0 / 60, F1_0 / 4 })
							for (const vms_vector velocity : { vms_vector{ F1_0, 0, 0 }, vms_vector{ -1001, 2003, -3007 } }) {
								Game_mode = multiplayer ? GM_MULTI : saved_mode;
								obj_relink(index, segment);
								compute_segment_center(&actor.pos, &Segments[segment]);
								const auto start = actor.pos;
								actor.mtype.phys_info.velocity = velocity;
								actor.movement_type = MT_PHYSICS;
								FrameTime = time;
								object *saved_console = ConsoleObject;
								if (console) ConsoleObject = &actor;
								const auto random_calls = d_rand_get_stream_call_count(D_RNG_SIM);
								do_physics_sim(&actor);
								ConsoleObject = saved_console;
								require(actor.segnum == segment && !std::memcmp(&actor.mtype.phys_info.velocity, &velocity, sizeof(velocity)), "Slow displacement altered velocity or crossed fixture boundary");
								require(d_rand_get_stream_call_count(D_RNG_SIM) == random_calls, "Slow displacement consumed gameplay RNG");
								actor.movement_type = MT_NONE;
								actor.lifeleft = i2f(100);
								object_move_one(&actor);
								samples << "M " << profile << ' ' << segment << ' ' << clocks[0].speed << ' ' << clocks[1].speed << ' ' << console << ' ' << multiplayer << ' ' << type << ' ' << bool(xl_preview_water_segment(segment)) << ' ' << time << ' '
								        << velocity.x << ' ' << velocity.y << ' ' << velocity.z << ' ' << actor.pos.x - start.x << ' ' << actor.pos.y - start.y << ' ' << actor.pos.z - start.z << ' ' << actor.lifeleft << '\n';
								++count;
							}
			Game_mode = saved_mode;
			actor.movement_type = MT_NONE;
			for (fix life : { IMMORTAL_TIME, 0x3ffffffe, i2f(100) })
				for (fix time : { 0, -1, F1_0 / 60 }) {
					actor.lifeleft = life;
					FrameTime = time;
					object_move_one(&actor);
					const fix expected = (life == IMMORTAL_TIME || life == 0x3ffffffe || time <= 0) ? life : life - fix(time / clocks[0].speed);
					require(actor.lifeleft == expected, "Slow lifetime sentinel/nonpositive-time handling differs");
					samples << "L " << profile << ' ' << type << ' ' << clocks[0].speed << ' ' << time << ' ' << life << ' ' << actor.lifeleft << '\n';
				}
			obj_delete(index);
		}
	}
	Game_mode = saved_mode;
	vms_vector marker_position;
	compute_segment_center(&marker_position, &Segments[60]);
	auto marker_orientation = vmd_identity_matrix;
	const int marker = drop_marker_object(&marker_position, 60, &marker_orientation, 0);
	require(marker >= 0 && Objects[marker].lifeleft == IMMORTAL_TIME - 1, "Native marker sentinel changed");
	FrameTime = F1_0 / 60;
	for (int frame = 0; frame < 3; ++frame) {
		const auto before = Objects[marker].lifeleft;
		object_move_one(&Objects[marker]);
		require(Objects[marker].lifeleft == before - FrameTime, "Slow motion froze/scaled Redux marker flashing");
	}
	obj_delete(marker);
	require(count == 768, "Slow-motion native coverage count changed");
	std::cout << "XL slow-motion displacement/lifetime: " << count << " native samples passed\n";
}
#endif
