#include "../admission.h"
static int trigger_chain_test(int argc, char **argv)
{
	try {
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
		std::ifstream original(root / "neptunxl.hog", std::ios::binary);
		xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(original), std::istreambuf_iterator<char>() };
		xl_compat::Bytes derived{ 'D', 'H', 'F' };
		for (auto member : xl_compat::read_hog(bytes)) {
			if (member.name == "neptunxl.rl2") {
				const auto inventory = xl_compat::inspect_level(member.bytes);
				const auto &section = *std::find_if(inventory.sections.begin(), inventory.sections.end(), [](const auto &s) { return s.name == "triggers"; });
				require(section.count == 1 && inventory.walls.size() >= 8, "unexpected trigger chain source geometry");
				const auto patch = [&](std::size_t offset, std::uint32_t value, int width = 4) {
					for (int i = 0; i < width; ++i) member.bytes.at(offset + i) = static_cast<unsigned char>(value >> (i * 8));
				};
				member.bytes.insert(member.bytes.begin() + section.offset, 7 * 52, 0);
				for (std::size_t i = 0; i < inventory.sections.size(); ++i)
					if (inventory.sections[i].offset > section.offset) patch(inventory.game_offset + 35 + i * 12, inventory.sections[i].offset + 7 * 52);
				patch(inventory.game_offset + 75, 8);
				const auto &walls = *std::find_if(inventory.sections.begin(), inventory.sections.end(), [](const auto &s) { return s.name == "walls"; });
				for (std::size_t i = 0; i < inventory.walls.size(); ++i) patch(walls.offset + i * 25 + 20, i < 8 ? static_cast<unsigned>(i) : 255, 1);
				std::fill(member.bytes.begin() + section.offset, member.bytes.begin() + section.offset + 8 * 52, 0);
				const auto action = [&](int index, int type, int flags, int value, std::initializer_list<int> links) {
					const auto offset = section.offset + index * 52;
					patch(offset, type, 1);
					patch(offset + 1, flags, 1);
					patch(offset + 2, static_cast<unsigned>(links.size()), 1);
					patch(offset + 4, value);
					patch(offset + 8, 0xffffffff);
					int i = 0;
					for (int target : links) {
						const auto &wall = inventory.walls.at(target);
						patch(offset + 12 + i * 2, wall.segment, 2);
						patch(offset + 32 + i * 2, wall.side, 2);
						++i;
					}
				};
				action(0, 31, TF_ONE_SHOT, 0, { 2, 1, 3, 1 });
				action(1, 14, TF_DISABLED, 0, {});
				patch(section.offset + 52 + 2, 1, 1);
				patch(section.offset + 52 + 12, 1, 2);
				action(2, 32, 0, 0, { 1 });
				action(3, 33, 0, 0, { 1 });
				action(4, 31, 0, 2 * F1_0, { 1 });
				action(5, 31, 0, 0, { 0 });
				action(6, 32, 0, 0, { 4 });
				action(7, 33, 0, 0, { 4 });
				require(xl_compat::runtime_blockers(xl_compat::inspect_level(member.bytes)).empty(), "derived chain source failed admission");
			}
			char name[13] = {};
			require(member.name.size() < sizeof(name), "chain HOG member exceeds bound");
			std::strcpy(name, member.name.c_str());
			derived.insert(derived.end(), name, name + sizeof(name));
			for (unsigned shift = 0; shift < 32; shift += 8) derived.push_back(static_cast<unsigned char>(member.bytes.size() >> shift));
			derived.insert(derived.end(), member.bytes.begin(), member.bytes.end());
		}
		write_bytes((root / "chain.hog").string().c_str(), derived);
		std::ofstream descriptor(root / "chain.mn2");
		descriptor << "d2x-name = Derived chain controls\nnum_levels = 1\nneptunxl.rl2\n";
		descriptor.close();
		require(load_base_mission("chain", error, sizeof(error)), error);
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_level_extensions_active() && Num_triggers == 8, "chain source did not load");
		require(Triggers[4].value == 2 && (Triggers[4].flags & TF_DISABLED), "source master value did not normalize from fixed-point count");
		const int player = ConsoleObject - Objects;
		const auto fire = [&](int index) { xl_wall_trigger_reset(); return check_trigger_sub(index, Player_num, 1); };
		// Ordered enable -> teleport -> disable -> ignored teleport through real wall dispatch
		check_trigger(&Segments[Walls[0].segnum], Walls[0].sidenum, player, 1);
		require(ConsoleObject->segnum == 1 && (Triggers[0].flags & TF_DISABLED) && (Triggers[1].flags & TF_DISABLED), "ordered chain or post-action one-shot differs");
		require((xl_preview_trigger_mask() & 15) == 15, "chain children did not activate");
		// A nested chain must bypass top-level cooldown on its children
		Triggers[0].flags = 0;
		require(fire(5) == 0 && (Triggers[1].flags & TF_DISABLED), "nested chain did not execute in order");
		const auto saved_leaf = Triggers[1];
		const auto saved_nested = Triggers[5];
		const auto saved_wall = Walls[8];
		Triggers[5].num_links = 1;
		Triggers[5].seg[0] = Walls[1].segnum;
		Triggers[5].side[0] = Walls[1].sidenum;
		Triggers[1].type = TT_ILLUSION_OFF;
		Triggers[1].flags = 0;
		Triggers[1].seg[0] = Walls[8].segnum;
		Triggers[1].side[0] = Walls[8].sidenum;
		Walls[8].type = WALL_ILLUSION;
		Walls[8].flags = 0;
		require(fire(5) == 0 && (Walls[8].flags & WALL_ILLUSION_OFF), "master did not dispatch stock illusion-off child");
		Triggers[1].type = TT_ILLUSION_ON;
		require(fire(5) == 0 && !(Walls[8].flags & WALL_ILLUSION_OFF), "master did not dispatch stock illusion-on child");
		Triggers[1] = saved_leaf;
		Triggers[5] = saved_nested;
		Walls[8] = saved_wall;
		std::ofstream samples(std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "trigger-counters.txt");
		for (int value : { -1, 0, 1, 2, 65536 })
			for (int disabled : { 0, 1 })
				for (int operation : { 6, 7, 6, 6 }) {
					Triggers[4].value = value;
					Triggers[4].flags = disabled ? TF_DISABLED : 0;
					require(fire(operation) == 0, "counter operation failed");
					samples << value << ' ' << disabled << ' ' << (operation == 6 ? 32 : 33) << ' ' << Triggers[4].value << ' ' << int(Triggers[4].flags) << '\n';
				}
		samples.close();
		require(bool(samples), "counter reference samples not written");
		Triggers[4].value = 2;
		Triggers[4].flags = TF_DISABLED;
		require(fire(6) == 0 && Triggers[4].value == 1 && (Triggers[4].flags & TF_DISABLED), "master enabled before count reached zero");
		require(fire(6) == 0 && Triggers[4].value == 0 && !(Triggers[4].flags & TF_DISABLED), "master did not enable at zero");
		require(fire(7) == 0 && Triggers[4].value == 1 && (Triggers[4].flags & TF_DISABLED), "master disable did not increment raw count");
		// Top-level debounce does not apply to recursive child calls
		timer_update();
		xl_wall_trigger_reset();
		require(check_trigger_sub(7, Player_num, 0) == 0 && check_trigger_sub(7, Player_num, 0) == 1 && Triggers[4].value == 2, "top-level chain debounce differs");
		timer_delay(F1_0 * 4 / 5);
		timer_update();
		require(check_trigger_sub(7, Player_num, 0) == 0 && Triggers[4].value == 3, "chain debounce did not expire");
		// Self-loop bounded independently of post-action one-shot disabling
		const auto saved_zero = Triggers[0];
		Triggers[0].flags = TF_ONE_SHOT;
		Triggers[0].num_links = 2;
		Triggers[0].seg[0] = Walls[7].segnum;
		Triggers[0].side[0] = Walls[7].sidenum;
		Triggers[0].seg[1] = Walls[0].segnum;
		Triggers[0].side[1] = Walls[0].sidenum;
		Triggers[4].value = 0;
		require(fire(0) == 0 && Triggers[4].value == 16 && (Triggers[0].flags & TF_DISABLED), "recursive chain depth or one-shot order differs");
		Triggers[0] = saved_zero;
		const int old_type = ConsoleObject->type;
		for (int actor : { OBJ_ROBOT, OBJ_DEBRIS, OBJ_CNTRLCEN, OBJ_HOSTAGE, OBJ_POWERUP, OBJ_WEAPON, OBJ_CLUTTER }) {
			ConsoleObject->type = actor;
			Triggers[0].flags = 0;
			xl_wall_trigger_reset();
			const bool eligible = actor != OBJ_WEAPON && actor != OBJ_CLUTTER;
			require(xl_wall_trigger(0, player, 0) == (eligible ? 0 : 1), "master actor filter differs");
			require(xl_wall_trigger(2, player, 0) == 1 && xl_wall_trigger(3, player, 0) == 1, "nonplayer enabled or disabled a trigger");
		}
		ConsoleObject->type = old_type;
		// Nonplayer masters can teleport their actor, but cannot run player-only children
		Triggers[1].flags = 0;
		Triggers[0].num_links = 1;
		Triggers[0].seg[0] = Walls[1].segnum;
		Triggers[0].side[0] = Walls[1].sidenum;
		ConsoleObject->type = OBJ_POWERUP;
		xl_wall_trigger_reset();
		require(xl_wall_trigger(0, player, 0) == 0 && ConsoleObject->segnum == 1, "nonplayer master lost actor on teleport child");
		ConsoleObject->type = old_type;
		int from = -1, side = -1, to = -1;
		for (int segment = 0; segment < Num_segments && from < 0; ++segment)
			for (int face = 0; face < 6; ++face)
				if (Segments[segment].children[face] >= 0 && Segments[segment].sides[face].wall_num == -1) {
					from = segment;
					side = face;
					to = Segments[segment].children[face];
					break;
				}
		require(from >= 0 && Highest_segment_index != from && Highest_segment_index != to, "missing master physics crossing fixture");
		const auto before_crossing = *ConsoleObject;
		const auto frame_before_crossing = FrameTime;
		const auto crossing_wall = Walls[0];
		Segments[from].sides[side].wall_num = 0;
		Walls[0] = {};
		Walls[0].segnum = from;
		Walls[0].sidenum = side;
		Walls[0].type = WALL_OPEN;
		Walls[0].trigger = 0;
		Triggers[1].seg[0] = Highest_segment_index;
		vms_vector portal, inside, outside;
		compute_center_point_on_side(&portal, &Segments[from], side);
		compute_segment_center(&inside, &Segments[from]);
		compute_segment_center(&outside, &Segments[to]);
		for (auto *point : { &inside, &outside }) {
			vm_vec_sub(point, point, &portal);
			vm_vec_scale(point, F1_0 / 4);
			vm_vec_add2(point, &portal);
		}
		obj_relink(player, from);
		ConsoleObject->pos = inside;
		ConsoleObject->type = OBJ_POWERUP;
		ConsoleObject->control_type = CT_NONE;
		ConsoleObject->movement_type = MT_PHYSICS;
		ConsoleObject->mtype.phys_info = {};
		ConsoleObject->mtype.phys_info.mass = F1_0;
		vm_vec_sub(&ConsoleObject->mtype.phys_info.velocity, &outside, &inside);
		vm_vec_scale(&ConsoleObject->mtype.phys_info.velocity, i2f(4));
		FrameTime = F1_0 / 4;
		xl_wall_trigger_reset();
		object_move_one(ConsoleObject);
		require(ConsoleObject->segnum == Highest_segment_index, "nonplayer physical crossing missed master and teleport child");
		Segments[from].sides[side].wall_num = -1;
		Walls[0] = crossing_wall;
		obj_relink(player, before_crossing.segnum);
		const int linked_next = ConsoleObject->next, linked_prev = ConsoleObject->prev;
		*ConsoleObject = before_crossing;
		ConsoleObject->next = linked_next;
		ConsoleObject->prev = linked_prev;
		FrameTime = frame_before_crossing;
		Triggers[1] = saved_leaf;
		Triggers[0] = saved_zero;
		Triggers[4].flags = TF_DISABLED;
		Triggers[4].value = 2;
		const auto frame = FrameTime;
		FrameTime = F1_0;
		triggers_frame_process();
		FrameTime = frame;
		require(Triggers[0].time == -1 && Triggers[6].time == -1, "XL scheduling descriptors changed during frame");
		char save[] = "xl-chain.sav", description[] = "Trigger chain source restoration";
		stop_time();
		require(state_save_all_sub(save, description) != 0, "chain save failed");
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "chain stock transition failed");
		require(xl_wall_trigger(0, ConsoleObject - Objects, 0) == -1, "chain hook contaminated stock dispatch");
		require(state_restore_all_sub(save, 0) != 0 && !std::strcmp(Current_mission_filename, "chain"), "chain source restore failed");
		require(Triggers[4].value == 2 && (Triggers[4].flags & TF_DISABLED), "save lost master counter or disabled state");
		require(fire(6) == 0 && Triggers[4].value == 1 && (Triggers[4].flags & TF_DISABLED), "restored counter did not remain authoritative");
		require(fire(6) == 0 && Triggers[4].value == 0 && !(Triggers[4].flags & TF_DISABLED), "restored counter did not enable master");
		// A real exit child stops later actions and skips the master's one-shot update
		require(xl_test_stop_after_level(), "chain exit test requires bounded completion");
		Triggers[0].flags = TF_ONE_SHOT;
		Triggers[0].num_links = 3;
		int link = 0;
		for (int target : { 7, 1, 6 }) {
			Triggers[0].seg[link] = Walls[target].segnum;
			Triggers[0].side[link++] = Walls[target].sidenum;
		}
		Triggers[1].type = TT_EXIT;
		Triggers[1].flags = 0;
		Triggers[1].num_links = 0;
		require(fire(0) == 1 && xl_preview_pilot_completed() && Triggers[4].value == 1 && !(Triggers[0].flags & TF_DISABLED), "exit child did not abort chain before later actions");
		require(fire(6) == 0 && Triggers[4].value == 0, "exit left recursion context poisoned");
		std::cout << "XL wall trigger chain controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
