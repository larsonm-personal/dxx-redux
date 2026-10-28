#include "geometry.h"
// Dispatch controls on original geometry; source admission is tested separately
static int teleport_test(int argc, char **argv)
{
	try {
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		require(load_base_mission("neptunxl", error, sizeof(error)), error);
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_preview_pilot_active() && Num_triggers > 0 && Num_walls > 0, "teleport controls need loaded XL geometry");
		const int player = ConsoleObject - Objects;
		const auto original = *ConsoleObject;
		const auto old_trigger = Triggers[0];
		Triggers[0] = {};
		auto &trigger = Triggers[0];
		trigger.type = 14;
		trigger.time = -1;
		trigger.num_links = 1;
		trigger.seg[0] = 1;
		trigger.side[0] = 0;
		vms_vector center;
		float x = 0, y = 0, z = 0;
		for (int i = 0; i < 8; ++i) {
			const auto &vertex = Vertices[Segments[1].verts[i]];
			x += vertex.x / 65536.0f;
			y += vertex.y / 65536.0f;
			z += vertex.z / 65536.0f;
		}
		center = { static_cast<fix>(x / 8 * 65536), static_cast<fix>(y / 8 * 65536), static_cast<fix>(z / 8 * 65536) };
		ConsoleObject->mtype.phys_info.velocity = {};
		require(xl_wall_trigger(0, player, 0) == 0 && ConsoleObject->segnum == 1 && !std::memcmp(&ConsoleObject->pos, &center, sizeof(center)), "teleport did not relink at destination center");
		require(vm_vec_mag(&ConsoleObject->mtype.phys_info.velocity) == 0, "stationary teleport acquired velocity");
		trigger.flags = TF_ONE_SHOT;
		require(xl_wall_trigger(0, player, 0) == 0 && (trigger.flags & TF_DISABLED), "one-shot teleport did not disable");
		trigger.seg[0] = 2;
		require(xl_wall_trigger(0, player, 0) == 1 && ConsoleObject->segnum == 1, "disabled teleport moved player");
		trigger.flags = 0;
		xl_wall_trigger_reset();
		trigger.time = 100;
		require(xl_wall_trigger(0, player, 0) == 1 && ConsoleObject->segnum == 1, "delayed action ran immediately");
		trigger.time = -1;
		trigger.side[0] = 6;
		require(xl_wall_trigger(0, player, 0) == 1, "malformed destination accepted at dispatch");
		trigger.side[0] = 0;
		for (const int time : { 0, 99, 900001 }) {
			trigger.time = time;
			const auto frame = FrameTime;
			FrameTime = F1_0;
			triggers_frame_process();
			FrameTime = frame;
			require(trigger.time == time && xl_wall_trigger(0, player, 0) == 0, "stock countdown changed immediate XL teleport scheduling");
		}
		trigger.time = -1;
		const int old_dead = Player_is_dead;
		const int dead_segment = ConsoleObject->segnum;
		Player_is_dead = 1;
		require(xl_wall_trigger(0, player, 0) == 0 && ConsoleObject->segnum == dead_segment, "dead-player teleport dispatch or motion differs");
		Player_is_dead = old_dead;
		// Hit the actual wall entry point, rather than only the new dispatcher
		const auto wall = Walls[0];
		Walls[0].trigger = 0;
		const int segment = wall.segnum, side = wall.sidenum;
		require(Segments[segment].sides[side].wall_num == 0, "fixture wall link differs");
		check_trigger(&Segments[segment], side, player, 0);
		require(ConsoleObject->segnum == 2, "wall activation missed teleport dispatch");
		unsigned saved_rng;
		d_rand_get_state(&saved_rng);
		trigger.num_links = 2;
		trigger.seg[0] = 1;
		trigger.seg[1] = 2;
		trigger.side[1] = 0;
		int destinations = 0;
		for (int i = 0; i < 64; ++i) {
			require(xl_wall_trigger(0, player, 0) == 0, "repeat teleport was debounced");
			require(ConsoleObject->segnum == 1 || ConsoleObject->segnum == 2, "random destination escaped links");
			destinations |= 1 << ConsoleObject->segnum;
		}
		require(destinations == 6, "multiple teleport destinations were not exercised");
		d_rand_set_state(saved_rng);
		trigger.num_links = 1;
		// Test actor restrictions without conflating them with AI navigation
		const int old_type = ConsoleObject->type;
		for (const int type : { OBJ_ROBOT, OBJ_DEBRIS, OBJ_CNTRLCEN, OBJ_HOSTAGE, OBJ_POWERUP, OBJ_WEAPON, OBJ_CLUTTER }) {
			ConsoleObject->type = type;
			const int expected = type == OBJ_WEAPON || type == OBJ_CLUTTER ? 1 : 0;
			require(xl_wall_trigger(0, player, 0) == expected, "teleport actor dispatch differs from reference");
		}
		ConsoleObject->type = old_type;
		std::ofstream samples(std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "teleport-transforms.txt");
		for (int target_segment : { 0, 1, 2, 30, 60, 92 })
			for (int target_side = 0; target_side < 6; ++target_side)
				for (const vms_vector velocity : { vms_vector{ 0, 0, 0 }, vms_vector{ F1_0, 0, 0 }, vms_vector{ 0, F1_0, 0 }, vms_vector{ 0, -F1_0, 0 }, vms_vector{ 0, 0, -F1_0 }, vms_vector{ 12345, -67890, 123456 }, vms_vector{ -765432, 12345, -67890 } }) {
					trigger.seg[0] = target_segment;
					trigger.side[0] = target_side;
					const auto geometry = xl_compat::compile_classic_lighting_side(target_segment, target_side);
					for (int axis = 0; axis < 3; ++axis) samples << (std::int64_t(geometry.fixed_normals[0][axis]) + geometry.fixed_normals[1][axis]) / 2 << ' ';
					samples << velocity.x << ' ' << velocity.y << ' ' << velocity.z << ' ';
					ConsoleObject->mtype.phys_info.velocity = velocity;
					require(xl_wall_trigger(0, player, 0) == 0, "reference sample teleport refused");
					for (const auto *vector : { &ConsoleObject->orient.rvec, &ConsoleObject->orient.uvec, &ConsoleObject->orient.fvec, &ConsoleObject->mtype.phys_info.velocity }) samples << vector->x << ' ' << vector->y << ' ' << vector->z << ' ';
					samples << '\n';
				}
		samples.close();
		require(bool(samples), "teleport transform samples not written");
		int crossing_from = -1, crossing_side = -1, crossing_to = -1;
		for (int s = 0; s <= Highest_segment_index && crossing_from < 0; ++s)
			for (int side_index = 0; side_index < 6; ++side_index) {
				const int child = Segments[s].children[side_index];
				if (child >= 0 && Segments[s].sides[side_index].wall_num == -1) {
					crossing_from = s;
					crossing_side = side_index;
					crossing_to = child;
					break;
				}
			}
		require(crossing_from >= 0, "missing physical crossing fixture");
		const auto actor_before = *ConsoleObject;
		const auto frame_before = FrameTime;
		Segments[crossing_from].sides[crossing_side].wall_num = 0;
		Walls[0] = {};
		Walls[0].segnum = crossing_from;
		Walls[0].sidenum = crossing_side;
		Walls[0].type = WALL_OPEN;
		Walls[0].trigger = 0;
		trigger.seg[0] = Highest_segment_index;
		require(trigger.seg[0] != crossing_from && trigger.seg[0] != crossing_to, "crossing target overlaps portal");
		vms_vector portal, inside, outside;
		compute_center_point_on_side(&portal, &Segments[crossing_from], crossing_side);
		compute_segment_center(&inside, &Segments[crossing_from]);
		compute_segment_center(&outside, &Segments[crossing_to]);
		for (auto *point : { &inside, &outside }) {
			vm_vec_sub(point, point, &portal);
			vm_vec_scale(point, F1_0 / 4);
			vm_vec_add2(point, &portal);
		}
		obj_relink(player, crossing_from);
		ConsoleObject->pos = inside;
		ConsoleObject->type = OBJ_POWERUP;
		ConsoleObject->control_type = CT_NONE;
		ConsoleObject->movement_type = MT_PHYSICS;
		ConsoleObject->mtype.phys_info = {};
		ConsoleObject->mtype.phys_info.mass = F1_0;
		vm_vec_sub(&ConsoleObject->mtype.phys_info.velocity, &outside, &inside);
		vm_vec_scale(&ConsoleObject->mtype.phys_info.velocity, i2f(4));
		FrameTime = F1_0 / 4;
		object_move_one(ConsoleObject);
		require(ConsoleObject->segnum == Highest_segment_index, "nonplayer physical crossing did not activate teleport");
		std::cout << "XL nonplayer physics crossing activated derived destination\n";
		Segments[crossing_from].sides[crossing_side].wall_num = -1;
		obj_relink(player, actor_before.segnum);
		const int linked_next = ConsoleObject->next, linked_prev = ConsoleObject->prev;
		*ConsoleObject = actor_before;
		ConsoleObject->next = linked_next;
		ConsoleObject->prev = linked_prev;
		FrameTime = frame_before;
		Walls[0] = wall;
		Triggers[0] = old_trigger;
		obj_relink(player, original.segnum);
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "teleport stock recovery failed");
		const auto stock_trigger = Triggers[0];
		const auto stock_frame = FrameTime;
		Triggers[0].time = F1_0;
		FrameTime = F1_0 / 4;
		triggers_frame_process();
		require(Triggers[0].time == F1_0 * 3 / 4, "XL scheduling changed stock trigger countdown");
		FrameTime = stock_frame;
		Triggers[0].type = 14;
		require(xl_wall_trigger(0, ConsoleObject - Objects, 0) == -1, "XL dispatch contaminated stock gameplay");
		Triggers[0] = stock_trigger;
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
		std::ifstream hog_file(root / "neptunxl.hog", std::ios::binary);
		xl_compat::Bytes hog_bytes{ std::istreambuf_iterator<char>(hog_file), std::istreambuf_iterator<char>() };
		xl_compat::Bytes derived{ 'D', 'H', 'F' };
		bool replaced = false;
		for (auto member : xl_compat::read_hog(hog_bytes)) {
			if (member.name == "neptunxl.rl2") {
				const auto inventory = xl_compat::inspect_level(member.bytes);
				const auto section = std::find_if(inventory.sections.begin(), inventory.sections.end(), [](const xl_compat::Section &s) { return s.name == "triggers"; });
				require(section != inventory.sections.end() && section->count == 1, "unexpected source trigger layout");
				const auto offset = section->offset;
				std::fill(member.bytes.begin() + offset, member.bytes.begin() + offset + 52, 0);
				member.bytes[offset] = 14;
				member.bytes[offset + 1] = TF_ONE_SHOT;
				member.bytes[offset + 2] = 2;
				for (int i = 8; i < 12; ++i) member.bytes[offset + i] = 255;
				member.bytes[offset + 12] = 1;
				member.bytes[offset + 14] = 2;
				replaced = true;
			}
			char name[13] = {};
			require(member.name.size() < sizeof(name), "derived teleport HOG name exceeds bound");
			std::strcpy(name, member.name.c_str());
			derived.insert(derived.end(), name, name + sizeof(name));
			for (unsigned shift = 0; shift < 32; shift += 8) derived.push_back(static_cast<unsigned char>(member.bytes.size() >> shift));
			derived.insert(derived.end(), member.bytes.begin(), member.bytes.end());
		}
		require(replaced, "missing source level for serialized teleport");
		write_bytes((root / "teleport.hog").string().c_str(), derived);
		std::ofstream descriptor(root / "teleport.mn2");
		descriptor << "d2x-name = Derived teleport controls\nnum_levels = 1\nneptunxl.rl2\n";
		descriptor.close();
		require(load_base_mission("teleport", error, sizeof(error)), error);
		require(xl_level_supported("neptunxl.rl2", 0), xl_level_rejection());
		require(!xl_level_supported("neptunxl.rl2", 1), "teleport admitted multiplayer");
		StartNewGame(1);
		require(Triggers[0].type == 14 && Triggers[0].num_links == 2 && Triggers[0].flags == TF_ONE_SHOT, "serialized teleport lost at source admission");
		int source_wall = -1;
		for (int i = 0; i < Num_walls; ++i)
			if (wall_trigger_index(Walls[i].trigger) == 0) {
				source_wall = i;
				break;
			}
		require(source_wall >= 0, "source teleport lacks an activating wall");
		check_trigger(&Segments[Walls[source_wall].segnum], Walls[source_wall].sidenum, ConsoleObject - Objects, 0);
		require(Triggers[0].flags & TF_DISABLED, "source wall did not activate one-shot teleport");
		const int destination_segment = ConsoleObject->segnum;
		const auto destination_position = ConsoleObject->pos;
		const auto destination_orientation = ConsoleObject->orient;
		require(destination_segment == 1 || destination_segment == 2, "source destination differs");
		int solid_side = -1;
		for (int i = 0; i < 6; ++i)
			if (Segments[destination_segment].children[i] == -1 && Segments[destination_segment].sides[i].wall_num == -1) {
				solid_side = i;
				break;
			}
		require(solid_side >= 0, "arrival fixture lacks a solid collision face");
		const auto arrival_physics = ConsoleObject->mtype.phys_info;
		const auto arrival_time = FrameTime;
		vms_vector wall_center, unimpeded;
		compute_center_point_on_side(&wall_center, &Segments[destination_segment], solid_side);
		vm_vec_sub(&ConsoleObject->mtype.phys_info.velocity, &wall_center, &ConsoleObject->pos);
		vm_vec_scale(&ConsoleObject->mtype.phys_info.velocity, i2f(8));
		ConsoleObject->mtype.phys_info.thrust = {};
		ConsoleObject->mtype.phys_info.drag = 0;
		ConsoleObject->mtype.phys_info.flags = 0;
		FrameTime = F1_0 / 4;
		vm_vec_scale_add(&unimpeded, &ConsoleObject->pos, &ConsoleObject->mtype.phys_info.velocity, FrameTime);
		do_physics_sim(ConsoleObject);
		require(ConsoleObject->segnum == destination_segment && get_seg_masks(&ConsoleObject->pos, ConsoleObject->segnum, ConsoleObject->size, __FILE__, __LINE__).centermask == 0, "teleported actor escaped solid arrival geometry");
		require(std::memcmp(&ConsoleObject->pos, &destination_position, sizeof(destination_position)) && std::memcmp(&ConsoleObject->pos, &unimpeded, sizeof(unimpeded)), "arrival movement did not reach and collide with wall");
		ConsoleObject->pos = destination_position;
		ConsoleObject->mtype.phys_info = arrival_physics;
		FrameTime = arrival_time;
		std::cout << "XL post-arrival movement and solid-wall collision passed\n";
		char save[] = "xl-teleport.sav", description[] = "Teleport source restoration";
		stop_time();
		require(state_save_all_sub(save, description) != 0, "teleport save failed");
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "teleport source stock recovery failed");
		require(state_restore_all_sub(save, 0) != 0 && !std::strcmp(Current_mission_filename, "teleport"), "teleport source restore failed");
		require(Triggers[0].type == 14 && (Triggers[0].flags & TF_DISABLED), "restore lost one-shot teleport state");
		require(ConsoleObject->segnum == destination_segment && !std::memcmp(&ConsoleObject->pos, &destination_position, sizeof(destination_position)) && !std::memcmp(&ConsoleObject->orient, &destination_orientation, sizeof(destination_orientation)), "restore lost teleported pose");
		require(xl_wall_trigger(0, ConsoleObject - Objects, 0) == 1, "restored one-shot teleport fired again");
		std::cout << "XL serialized teleport admission and save restoration passed\n";
		std::cout << "XL wall teleport dispatch controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
