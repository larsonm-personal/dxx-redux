#include "speed_boost.h"
#include "speed_boost_lifecycle_test.h"
// Controlled engine actions and source comparisons; no autonomous navigation
static int speed_boost_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		require(load_base_mission("neptunxl", error, sizeof(error)), error);
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_level_extensions_active() && Num_walls > 0 && Num_triggers > 0, "missing boost control geometry");
		const int player = ConsoleObject - Objects;
		const auto original = *ConsoleObject;
		std::vector<wall> walls(Walls, Walls + Num_walls);
		for (int index = 0; index < Num_walls; ++index) Walls[index].trigger = 255;
		const auto wall_origin = walls[0];
		const auto old_trigger = Triggers[0];
		std::ofstream samples(std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "boost-samples.txt");
		const auto emit = [&](const vms_vector &value) { samples << value.x << ' ' << value.y << ' ' << value.z << ' '; };
		const auto emit_matrix = [&](const vms_matrix &value) { emit(value.rvec); emit(value.uvec); emit(value.fvec); };
		unsigned count = 0;
		for (int destination : { 1, 30, 60, 92 })
			for (int side = 0; side < 6; ++side)
				for (int mode : { 0, 1, 2 }) {
					int from = wall_origin.segnum, from_side = wall_origin.sidenum;
					if (mode == 2) {
						from = Segments[destination].children[side];
						if (from < 0) continue;
						from_side = find_connect_side(&Segments[destination], &Segments[from]);
						require(from_side >= 0, "boost test connection has no reciprocal side");
					}
					Walls[0].segnum = from;
					Walls[0].sidenum = from_side;
					Walls[0].trigger = 0;
					const auto target = xl_compat::compile_classic_lighting_side(destination, side);
					const auto start = xl_compat::compile_classic_lighting_side(from, from_side);
					for (int value : { 0, 1, 5, 10, 11, -1, INT32_MIN, F1_0 })
						for (int flags : { 0, 32 })
							for (int ticks : { 0, 1, 8, 9 }) {
								xl_speed_boost_clear();
								require(xl_speed_boost_restore(nullptr, 0, 3, 1), "cannot seed reference last-speed control");
								const vms_angvec initial_angles{ 1700, -4500, 6500 };
								vm_angles_2_matrix(&ConsoleObject->orient, &initial_angles);
								ConsoleObject->mtype.phys_info.velocity = { 12345, -67890, 123456 };
								trigger action{};
								action.type = 15;
								action.value = value;
								action.flags = flags;
								action.time = -1;
								action.num_links = 1;
								action.seg[0] = destination;
								action.side[0] = side;
								samples << "A " << value << " 3 " << mode << ' ' << flags << ' ' << ticks << " 1 ";
								for (int axis = 0; axis < 3; ++axis) samples << (std::int64_t(target.fixed_normals[0][axis]) + target.fixed_normals[1][axis]) / 2 << ' ';
								for (int component : start.center) samples << component << ' ';
								for (int component : target.center) samples << component << ' ';
								emit(ConsoleObject->mtype.phys_info.velocity);
								emit_matrix(ConsoleObject->orient);
								require(xl_speed_boost_action(&action, mode ? 0 : -1, player, 0) == 0, "boost action refused valid sample");
								FrameTime = F1_0 / 40 + 1;
								for (int tick = 0; tick < ticks; ++tick) xl_speed_boost_frame();
								xl_speed_boost_state state{};
								xl_speed_boost_state_get(0, &state);
								samples << state.boosted << ' ' << xl_speed_boost_last_speed() << ' ' << state.steps << ' ';
								for (const auto *vector : { state.velocity, state.minimum, state.maximum })
									for (int axis = 0; axis < 3; ++axis) samples << vector[axis] << ' ';
								emit(ConsoleObject->mtype.phys_info.velocity);
								emit_matrix(ConsoleObject->orient);
								samples << '\n';
								++count;
								if (value != 1 || ticks || flags) continue;
								for (fix time : { 1, F1_0 / 120, F1_0 / 64, F1_0 / 30, F1_0 / 2 })
									for (vms_vector thrust : { vms_vector{}, vms_vector{ i2f(800), -i2f(100), i2f(500) }, vms_vector{ -i2f(800), i2f(100), -i2f(500) } }) {
										auto &physics = ConsoleObject->mtype.phys_info;
										physics.flags |= PF_USES_THRUST;
										physics.mass = i2f(4);
										physics.drag = 1000;
										physics.thrust = thrust;
										physics.velocity = { 123456, -234567, 345678 };
										samples << "D " << time << ' ' << physics.mass << ' ' << physics.drag << ' ';
										emit(thrust);
										emit(physics.velocity);
										require(xl_speed_boost_drag(player, time), "boost drag did not handle live state");
										emit(physics.velocity);
										samples << '\n';
										++count;
									}
							}
				}
		samples.close();
		require(bool(samples) && count > 500, "boost samples incomplete");
		std::copy(walls.begin(), walls.end(), Walls);
		Triggers[0] = old_trigger;
		*ConsoleObject = original;
		xl_speed_boost_clear();
		speed_boost_lifecycle_test(std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions");
		std::cout << "XL speed boost source controls passed: " << count << " samples\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "XL speed boost control failure: " << error.what() << '\n';
		return 1;
	}
}
