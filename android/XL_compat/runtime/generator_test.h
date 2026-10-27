// Production-load controls for source/native generator assignment, without navigation
#include "robot_generator.h"
#include "trigger_state.h"
extern "C" {
#include "fuelcen.h"
#include "gamesave.h"
#include "matcen_mode.h"
#include "automap.h"
void robotmaker_proc(FuelCenter *center);
}

static void generator_contact_samples(const std::filesystem::path &output)
{
	const auto original = Segments[20].children;
	std::array<short, 6> children;
	for (int side = 0; side < 6; ++side) children[side] = original[side];
	vms_vector center;
	compute_segment_center(&center, &Segments[20]);
	auto *robot = create_morph_robot(&Segments[20], &center, 0);
	require(robot, "cannot create generator contact actor");
	std::ofstream samples(output / "generator-contact.txt");
	for (bool player : { false, true })
		for (int profile = 0; profile < 6; ++profile)
			for (int offset : { 0, F1_0 / 3 })
				for (unsigned seed : { 1u, 17u, 365u, 0xabcdef01u }) {
					auto &actor = player ? *ConsoleObject : *robot;
					obj_relink(&actor - Objects, 20);
					actor.pos = center;
					actor.pos.x += offset;
					actor.pos.z -= offset / 2;
					actor.shields = Players[Player_num].shields = i2f(100);
					Players[Player_num].flags &= ~PLAYER_FLAGS_INVULNERABLE;
					actor.movement_type = profile == 3 ? MT_NONE : MT_PHYSICS;
					actor.mtype.phys_info.mass = profile == 2 ? 0 : profile == 1 ? F1_0 * 3 / 2
					                                                             : F1_0;
					actor.mtype.phys_info.velocity = { F1_0, -i2f(2), F1_0 / 7 };
					Automap_active = profile == 4;
					int passable = 0;
					for (int side = 0; side < 6; ++side) {
						Segments[20].children[side] = profile == 5 ? -1 : children[side];
						if (WALL_IS_DOORWAY(&Segments[20], side) & WID_FLY_FLAG) passable |= 1 << side;
					}
					samples << player << ' ' << Automap_active << ' ' << int(actor.movement_type) << ' ' << actor.mtype.phys_info.mass << ' ' << actor.size << ' ' << actor.pos.x << ' ' << actor.pos.y << ' ' << actor.pos.z << ' ' << passable;
					for (int side = 0; side < 6; ++side) {
						vms_vector point;
						compute_center_point_on_side(&point, &Segments[20], side);
						samples << ' ' << point.x << ' ' << point.y << ' ' << point.z;
					}
					d_srand(seed);
					d_rand_set_call_count(0);
					if (player) collide_player_and_materialization_center(&actor);
					else collide_robot_and_materialization_center(&actor);
					const auto &velocity = actor.mtype.phys_info.velocity;
					const fix damage = i2f(100) - (player ? Players[Player_num].shields : actor.shields);
					const auto count = d_rand_get_call_count();
					require(count <= 100, "generator contact used unexpected RNG volume");
					samples << ' ' << velocity.x << ' ' << velocity.y << ' ' << velocity.z << ' ' << damage << ' ' << count;
					unsigned after, replayed;
					d_rand_get_state(&after);
					d_srand(seed);
					for (std::uint64_t draw = 0; draw < count; ++draw) samples << ' ' << d_rand();
					d_rand_get_state(&replayed);
					require(after == replayed, "generator contact RNG transcript differs");
					samples << '\n';
				}
	for (int side = 0; side < 6; ++side) Segments[20].children[side] = children[side];
	Automap_active = 0;
	samples.close();
	require(bool(samples), "cannot write contact samples");
}

static void generator_behavior_controls(const std::filesystem::path &output)
{
	char baseline[] = "genbase.sav", description[20] = "XL generator tests";
	stop_time();
	require(state_save_all_sub(baseline, description) != 0, "generator control baseline save failed");
	start_time();
	const int original_mode = matcen_mode_get();
	const auto reset = [&] {
		require(state_restore_all_sub(baseline, 0) != 0, "generator control reset failed");
		// Desktop keeps this user preference separately; Android persists its metadata
		matcen_set_mode(original_mode);
	};
	const auto prepare = []() -> FuelCenter & {
		auto &center = Station[matcen_get_station_num(0)];
		center.Enabled = 1;
		center.Disable_time = i2f(30);
		center.Capacity = i2f(3);
		center.Flag = 0;
		center.Timer = i2f(1000);
		FrameTime = 0;
		return center;
	};
	generator_contact_samples(output);
	reset();
	// Capture production selection and RNG with explicit synthetic high-ID definitions
	// This does not waive the independent mission HAM/resource admission gate
	const int original_types = N_robot_types;
	const auto saved66 = Robot_info[66], saved70 = Robot_info[70], saved74 = Robot_info[74];
	Robot_info[66] = Robot_info[70] = Robot_info[74] = Robot_info[0];
	Robot_info[74].boss_flag = 1;
	N_robot_types = (std::max) (N_robot_types, 75);
	const auto add_robot = [](int id) {
		vms_vector position;
		compute_segment_center(&position, &Segments[24]);
		return obj_create(OBJ_ROBOT, id, 24, &position, &vmd_identity_matrix, F1_0, CT_NONE, MT_NONE, RT_NONE);
	};
	const int high66 = add_robot(66), high70 = add_robot(70), low65 = add_robot(65), boss74 = add_robot(74), nonrobot = add_robot(70);
	require(high66 >= 0 && high70 >= 0 && low65 >= 0 && boss74 >= 0 && nonrobot >= 0, "cannot create high-ID selection controls");
	Objects[nonrobot].type = OBJ_POWERUP;
	xl_robot_generator_load();
	require(xl_robot_generator_implicit_flags() == ((1u << 2) | (1u << 6)), "implicit generator mask differs");
	std::ofstream masks(output / "generator-mask.txt");
	masks << xl_robot_generator_implicit_flags() << '\n';
	for (int i = 0; i <= Highest_object_index; ++i) {
		const auto &object = Objects[i];
		masks << int(object.type) << ' ' << int(object.id) << ' ' << (object.type == OBJ_ROBOT ? int(Robot_info[object.id].boss_flag != 0) : 0) << '\n';
	}
	masks.close();
	require(bool(masks), "cannot write generator mask samples");
	const auto original_center = RobotCenters[0];
	std::ofstream samples(output / "generator-selection.txt");
	for (const auto &flags : std::array<std::array<std::uint32_t, 2>, 7>{ { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 0x80000000u, 0x80000000u }, { 0xffffffffu, 0xffffffffu }, { 3, 0 }, { 0x55555555u, 0xaaaaaaaau } } }) {
		RobotCenters[0].robot_flags[0] = static_cast<int>(flags[0]);
		RobotCenters[0].robot_flags[1] = static_cast<int>(flags[1]);
		for (unsigned seed : { 0u, 1u, 17u, 32767u, 65535u, 0xabcdef01u }) {
			d_srand(seed);
			const int token = d_rand();
			unsigned after_one;
			d_rand_get_state(&after_one);
			d_srand(seed);
			const auto selected = xl_robot_generator_type(0);
			unsigned after;
			d_rand_get_state(&after);
			samples << flags[0] << ' ' << flags[1] << ' ' << xl_robot_generator_implicit_flags() << ' ' << seed << ' ' << token << ' ' << after_one << ' ' << selected << ' ' << after << '\n';
		}
	}
	samples.close();
	require(bool(samples), "cannot write generator selection samples");
	RobotCenters[0] = original_center;
	obj_delete(high66);
	obj_delete(high70);
	obj_delete(low65);
	obj_delete(boss74);
	obj_delete(nonrobot);
	require(xl_robot_generator_implicit_flags() == ((1u << 2) | (1u << 6)), "implicit mask incorrectly follows live removals");
	Robot_info[66] = saved66;
	Robot_info[70] = saved70;
	Robot_info[74] = saved74;
	N_robot_types = original_types;
	reset();
	require(!xl_robot_generator_implicit_flags(), "source reload retained synthetic implicit mask");
	std::ofstream distances(output / "generator-distance.txt");
	for (const auto &point : std::array<vms_vector, 6>{ { { 0, 0, 0 }, { 1, 2, 3 }, { i2f(20), i2f(30), i2f(40) }, { -i2f(500), i2f(201), i2f(47) }, { i2f(2500), -i2f(2500), i2f(50) }, { i2f(320), 3, 0 } } }) {
		const vms_vector origin{};
		distances << point.x << ' ' << point.y << ' ' << point.z << ' ' << xl_robot_generator_distance(&point, &origin) << '\n';
	}
	distances.close();
	require(bool(distances), "cannot write generator distance samples");
	{
		const auto saved = Robot_info[0];
		const int level = Current_level_num;
		std::ofstream shields(output / "generator-shields.txt");
		for (int kind = 0; kind < 3; ++kind)
			for (int test_level : { 1, 9, -13 })
				for (int difficulty = 0; difficulty < NDL; ++difficulty) {
					Robot_info[0].thief = kind == 1;
					Robot_info[0].companion = kind == 2;
					Robot_info[0].strength = i2f(100);
					Current_level_num = test_level;
					Difficulty_level = difficulty;
					vms_vector position;
					compute_segment_center(&position, &Segments[24]);
					auto *robot = create_morph_robot(&Segments[24], &position, 0);
					require(robot, "cannot create generated health control");
					shields << kind << ' ' << test_level << ' ' << difficulty << ' ' << robot->shields << '\n';
					obj_delete(robot - Objects);
				}
		shields.close();
		require(bool(shields), "cannot write generator health samples");
		Robot_info[0] = saved;
		Current_level_num = level;
	}
	reset();
	// Equal initial timer must fire; equal half-morph timer must still wait
	{
		auto &center = prepare();
		const unsigned seed = 17;
		d_srand(seed);
		fix top = xl_robot_generator_distance(&ConsoleObject->pos, &center.Center) / 64 + d_rand() * 2 + i2f(2);
		if (top > i2f(5)) top = i2f(5) + d_rand();
		if (top < i2f(2)) top = F1_0 * 3 / 2 + d_rand() * 2;
		center.Timer = top;
		d_srand(seed);
		robotmaker_proc(&center);
		require(center.Flag == 1 && center.Timer == 0, "XL exact initial timer boundary did not start morph");
		center.Timer = Vclip[VCLIP_MORPHING_ROBOT].play_time / 2;
		const auto capacity = center.Capacity;
		robotmaker_proc(&center);
		require(center.Flag == 1 && center.Capacity == capacity, "XL half-morph equality spawned too early");
	}
	reset();
	{
		auto &center = prepare();
		center.Disable_time = 1;
		FrameTime = 2;
		robotmaker_proc(&center);
		require(!center.Enabled && center.Flag == 1, "XL disable expiry lost the reference's final production frame");
	}
	reset();
	{
		auto &center = prepare();
		const int robots = Players[Player_num].num_robots_level;
		Players[Player_num].num_robots_level = Gamesave_num_org_robots + 15;
		Players[Player_num].num_kills_level = 0;
		robotmaker_proc(&center);
		require(center.Flag == 0 && center.Timer == i2f(1000), "XL global population equality did not suppress generation");
		Players[Player_num].num_robots_level = robots;
		robotmaker_proc(&center);
		require(center.Flag == 1, "XL generator did not recover after population dropped");
	}
	reset();
	{
		auto &center = prepare();
		std::vector<int> occupied;
		vms_vector position;
		compute_segment_center(&position, &Segments[24]);
		for (;;) {
			const int index = obj_create(OBJ_LIGHT, 0, 24, &position, &vmd_identity_matrix, 0, CT_NONE, MT_NONE, RT_NONE);
			if (index < 0) break;
			occupied.push_back(index);
		}
		require(!occupied.empty(), "object pressure control did not allocate objects");
		center.Enabled = 0;
		trigger_matcen(center.segnum);
		require(center.Enabled, "XL activation failed when its light could not be allocated");
		robotmaker_proc(&center);
		require(center.Flag == 0 && center.Timer == i2f(1000), "failed XL morph effect incorrectly started a spawn");
		obj_delete(occupied.back());
		robotmaker_proc(&center);
		require(center.Flag == 1 && center.Timer == 0, "XL morph effect did not retry after a slot became free");
	}
	reset();
	for (bool robot : { false, true }) {
		auto &center = prepare();
		object *actor = ConsoleObject;
		if (robot) actor = create_morph_robot(&Segments[center.segnum], &center.Center, 0);
		require(actor, "cannot create generator occupancy actor");
		obj_relink(actor - Objects, center.segnum);
		actor->pos = center.Center;
		actor->shields = i2f(100);
		Players[Player_num].shields = i2f(100);
		Players[Player_num].flags &= ~PLAYER_FLAGS_INVULNERABLE;
		robotmaker_proc(&center);
		require(center.Flag == 0 && center.Timer < i2f(1000), "occupied XL generator did not defer production");
		require(robot ? actor->shields == i2f(99) : Players[Player_num].shields == i2f(96), "occupied XL generator did not use native collision damage");
		reset();
	}
	std::ofstream activations(output / "generator-activation.txt");
	for (int difficulty = 0; difficulty < NDL; ++difficulty)
		for (int lives : { 0, 1, 3 })
			for (int enabled : { 0, 1 }) {
				Difficulty_level = difficulty;
				auto &center = Station[matcen_get_station_num(0)];
				center.Lives = static_cast<sbyte>(lives);
				center.Enabled = static_cast<sbyte>(enabled);
				center.Capacity = 0;
				center.Timer = 0;
				center.Disable_time = 0;
				trigger_matcen(center.segnum);
				activations << difficulty << ' ' << lives << ' ' << enabled << ' ' << int(center.Lives) << ' ' << int(center.Enabled) << ' ' << center.Capacity << ' ' << center.Timer << ' ' << center.Disable_time << '\n';
			}
	activations.close();
	require(bool(activations), "cannot write generator activation samples");
	reset();
	{
		auto &center = Station[matcen_get_station_num(0)];
		for (int wave = 0; wave < 3; ++wave) {
			trigger_matcen(center.segnum);
			require(center.Enabled && center.Lives == 2 - wave, "XL generation wave did not consume one life");
			center.Capacity = F1_0;
			center.Flag = 1;
			center.Timer = Vclip[VCLIP_MORPHING_ROBOT].play_time / 2 + 1;
			FrameTime = 0;
			const int before = Players[Player_num].num_robots_level;
			robotmaker_proc(&center);
			require(!center.Capacity && Players[Player_num].num_robots_level == before + 1, "XL final-capacity robot was not created");
			robotmaker_proc(&center);
			require(Players[Player_num].num_robots_level == before + 1, "XL exhausted capacity emitted another robot");
			FrameTime = center.Disable_time;
			robotmaker_proc(&center);
			require(!center.Enabled, "XL exhausted wave did not expire");
		}
		trigger_matcen(center.segnum);
		require(!center.Enabled && !center.Lives, "XL exhausted lives allowed an extra activation");
	}
	reset();
	for (int mode : { MATCEN_MODE_ONE_ROUND, MATCEN_MODE_PAUSED }) {
		matcen_set_mode(mode);
		Difficulty_level = NDL - 1;
		auto &center = Station[matcen_get_station_num(0)];
		trigger_matcen(center.segnum);
		require(bool(center.Enabled) == (mode == MATCEN_MODE_ONE_ROUND), "XL activation ignored user matcen mode");
		center.Enabled = 0;
		trigger_matcen(center.segnum);
		require(!center.Enabled, "XL user matcen mode allowed a forbidden repeat");
		reset();
	}
	{
		char name[] = "gen-guard.xl";
		auto *file = PHYSFS_openWrite(name);
		require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "cannot capture generator extension guard");
		xl_wall_trigger_reset();
		require(check_trigger_sub(0, Player_num, 1) == 0, "cannot establish live trigger state for generator guards");
		const int station = matcen_get_station_num(0), segment = RobotCenters[0].segnum;
		for (int variant = 0; variant < 7; ++variant) {
			const auto saved = RobotCenters[0];
			const int count = Num_robot_centers, fuel = Num_fuelcenters, type = Station[station].Type;
			const auto owner = Segment2s[segment].matcen_num;
			if (variant == 0) Num_robot_centers = 0;
			if (variant == 1) Num_fuelcenters = MAX_NUM_FUELCENS + 1;
			if (variant == 2) RobotCenters[0].robot_flags[0] ^= 1;
			if (variant == 3) RobotCenters[0].segnum = -1;
			if (variant == 4) RobotCenters[0].fuelcen_num = -1;
			if (variant == 5) Station[station].Type = SEGMENT_IS_FUELCEN;
			if (variant == 6) Segment2s[segment].matcen_num = -1;
			file = PHYSFS_openRead(name);
			require(file && !xl_wall_trigger_read(file) && PHYSFS_close(file), "corrupt saved generator selection/assignment accepted");
			xl_trigger_state live;
			require(xl_wall_trigger_state(0, &live) && live.operated, "invalid generator restore published wall state");
			RobotCenters[0] = saved;
			Num_robot_centers = count;
			Num_fuelcenters = fuel;
			Station[station].Type = type;
			Segment2s[segment].matcen_num = owner;
		}
		file = PHYSFS_openRead(name);
		require(file && xl_wall_trigger_read(file) && PHYSFS_close(file), "valid generator extension failed after rejected controls");
		PHYSFS_delete(name);
	}
	reset();
	{
		// Exercise stock dispatch on the same geometry, followed by a real source restore
		xl_preview_clear_mission();
		require(!xl_robot_generator_implicit_flags() && !xl_robot_generator_has_types(0), "XL generator selection leaked into stock dispatch");
		auto &center = prepare();
		const unsigned seed = 17;
		d_srand(seed);
		fix top = vm_vec_dist_quick(&ConsoleObject->pos, &center.Center) / 64 + d_rand() * 2 + i2f(2);
		if (top > i2f(5)) top = i2f(5) + d_rand();
		if (top < i2f(2)) top = F1_0 * 3 / 2 + d_rand() * 2;
		center.Timer = top;
		d_srand(seed);
		robotmaker_proc(&center);
		require(center.Flag == 0 && center.Timer == top, "XL inclusive timer boundary polluted stock dispatch");
		const auto saved = Robot_info[0];
		Robot_info[0].thief = 1;
		Robot_info[0].companion = 0;
		Robot_info[0].strength = i2f(100);
		Current_level_num = 9;
		vms_vector position;
		compute_segment_center(&position, &Segments[24]);
		auto *robot = create_morph_robot(&Segments[24], &position, 0);
		require(robot && robot->shields == i2f(100), "XL generated-health scaling polluted stock dispatch");
		Robot_info[0] = saved;
	}
	reset();
	std::cout << "XL generator behavior controls passed: selection/RNG, distance, timing, population, occupancy damage, allocation retry, difficulty and user modes\n";
}

static void write_generator_fixture(const std::filesystem::path &root)
{
	std::ifstream original(root / "neptunxl.hog", std::ios::binary);
	xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(original), std::istreambuf_iterator<char>() }, output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(bytes)) {
		if (member.name == "neptunxl.rl2") {
			const auto source = xl_compat::inspect_level(member.bytes);
			require(source.level_version == 23 && source.game_version == 40 && source.matcens.empty() && source.triggers.size() == 1, "unexpected generator source layout");
			const auto patch = [&](std::size_t offset, std::uint32_t value, int width = 4) {
				for (int i = 0; i < width; ++i) member.bytes.at(offset + i) = static_cast<unsigned char>(value >> (i * 8));
			};
			const auto append = [&](std::uint32_t value, int width) {
				for (int i = 0; i < width; ++i) member.bytes.push_back(static_cast<unsigned char>(value >> (i * 8)));
			};
			const auto properties = source.game_offset - (source.vertices.size() + source.segments.size() * 6 + 910 + source.segments.size()) * 13;
			for (int i = 0; i < 3; ++i) {
				const int segment = std::array<int, 3>{ 20, 22, 23 }[i];
				require(source.segments[segment].function == 0 && source.segments[segment].properties == 0, "generator fixture segment is not neutral");
				patch(properties + segment * 13, SEGMENT_IS_ROBOTMAKER, 1);
				patch(properties + segment * 13 + 1, std::array<int, 3>{ 2, 0, 1 }[i], 1);
			}
			const auto &trigger = source.sections.at(3);
			patch(trigger.offset, TT_MATCEN, 1);
			patch(trigger.offset + 1, TF_NO_MESSAGE, 1);
			patch(trigger.offset + 2, 3, 1);
			patch(trigger.offset + 8, -1);
			for (int i = 0; i < 3; ++i) {
				patch(trigger.offset + 12 + i * 2, std::array<int, 3>{ 20, 22, 23 }[i], 2);
				patch(trigger.offset + 32 + i * 2, 0, 2);
			}
			patch(source.game_offset + 35 + 6 * 12, static_cast<unsigned>(member.bytes.size()));
			patch(source.game_offset + 39 + 6 * 12, 3);
			patch(source.game_offset + 43 + 6 * 12, 20);
			for (int row = 0; row < 3; ++row) {
				append(1u << row, 4);
				append(0, 4);
				append((row + 1) * F1_0, 4);
				append((row + 4) * F1_0, 4);
				append(-1, 2);
				append(32767, 2);
			}
			const auto blockers = xl_compat::runtime_blockers(xl_compat::inspect_level(member.bytes));
			require(blockers.empty(), xl_compat::blocker_message(blockers).c_str());
		}
		char name[13] = {};
		require(member.name.size() < sizeof(name), "generator fixture member exceeds bound");
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(static_cast<unsigned char>(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "genmap.hog").string().c_str(), output);
	std::ofstream descriptor(root / "genmap.mn2");
	descriptor << "d2x-name = Derived generator mapping\nnum_levels = 1\nneptunxl.rl2\n";
}

static int generator_assignment_test(int argc, char **argv)
{
	try {
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
		write_generator_fixture(root);
		require(load_base_mission("genmap", error, sizeof(error)), error);
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_level_extensions_active() && Num_robot_centers == 3, "generator fixture did not load");
		const auto verify = [] {
			for (int native = 0; native < 3; ++native) {
				const int segment = std::array<int, 3>{ 20, 22, 23 }[native];
				const int expected = std::array<int, 3>{ 2, 0, 1 }[native];
				require(Segment2s[segment].matcen_num == native && RobotCenters[native].segnum == segment && RobotCenters[native].robot_flags[0] == (1 << expected), "production generator contents or segment links swapped");
				const int station = matcen_get_station_num(native);
				require(station >= 0 && Station[station].segnum == segment, "production generator station link broken");
			}
		};
		verify();
		generator_behavior_controls(root.parent_path());
		require(check_trigger_sub(0, Player_num, 1) == 0, "production matcen trigger dispatch failed");
		FrameTime = F1_0 / 4;
		for (int i = 0; i < 3; ++i) {
			auto &center = Station[matcen_get_station_num(i)];
			require(center.Enabled && center.Lives == 2, "source-linked matcen did not activate");
			robotmaker_proc(&center);
			require(center.Flag == 1 && center.Timer == 0, "source-linked matcen did not enter morph effect phase");
		}
		char snapshot[] = "genmap.sav", description[20] = "XL generator map";
		stop_time();
		require(state_save_all_sub(snapshot, description) != 0, "pending generator disk save failed");
		start_time();
		const auto spawn = [] {
			FrameTime = F1_0;
			for (int native = 0; native < 3; ++native) {
				const int station = matcen_get_station_num(native);
				auto &center = Station[station];
				const auto capacity = center.Capacity;
				const int count = Players[Player_num].num_robots_level;
				robotmaker_proc(&center);
				require(!center.Flag && center.Capacity == capacity - F1_0 && Players[Player_num].num_robots_level == count + 1, "production generator did not consume capacity and create robot");
				int spawned = -1;
				for (int i = 0; i <= Highest_object_index; ++i)
					if (Objects[i].type == OBJ_ROBOT && static_cast<unsigned char>(Objects[i].matcen_creator) == (station | 0x80)) {
						require(spawned < 0, "multiple robots from one pending generator");
						spawned = i;
					}
				if (spawned < 0 || Objects[spawned].id != std::array<int, 3>{ 2, 0, 1 }[native] || Objects[spawned].segnum != center.segnum)
					throw std::runtime_error("actual spawned robot differs: native=" + std::to_string(native) + " station=" + std::to_string(station) + " object=" + std::to_string(spawned) + " id=" + std::to_string(spawned < 0 ? -1 : Objects[spawned].id) + " segment=" + std::to_string(spawned < 0 ? -1 : Objects[spawned].segnum) + " expected_segment=" + std::to_string(center.segnum));
			}
		};
		spawn();
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0 && !xl_level_extensions_active(), "generator stock transition failed");
		require(state_restore_all_sub(snapshot, 0) != 0 && xl_level_extensions_active(), "stock-to-generator pending restore failed");
		verify();
		for (int i = 0; i < 3; ++i) require(Station[matcen_get_station_num(i)].Flag == 1, "pending generator morph state lost on restore");
		spawn();
		std::cout << "XL generator assignment runtime passed: production trigger, morph spawns, pending disk restore and stock transition\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "XL generator assignment failed: " << error.what() << '\n';
		return 1;
	}
}
