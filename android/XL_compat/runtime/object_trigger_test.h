#include "object_trigger.h"
#include "trigger_state.h"
extern "C" {
#include "collide.h"
#include "cntrlcen.h"
#include "fireball.h"
void obj_delete_all_that_should_be_dead(void);
}

static void write_object_event_fixture(const std::filesystem::path &root)
{
	std::ifstream original(root / "neptunxl.hog", std::ios::binary);
	xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(original), std::istreambuf_iterator<char>() }, output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(bytes)) {
		if (member.name == "neptunxl.rl2") {
			const auto append = [](xl_compat::Bytes &target, std::uint32_t value, int width) {
				for (int i = 0; i < width; ++i) target.push_back(static_cast<unsigned char>(value >> (i * 8)));
			};
			const auto patch = [&](std::size_t offset, std::uint32_t value, int width = 4) {
				for (int i = 0; i < width; ++i) member.bytes.at(offset + i) = static_cast<unsigned char>(value >> (i * 8));
			};
			auto inventory = xl_compat::inspect_level(member.bytes);
			const auto insert = [&](const xl_compat::Inventory &source, std::size_t offset, const xl_compat::Bytes &data) {
				member.bytes.insert(member.bytes.begin() + offset, data.begin(), data.end());
				for (std::size_t i = 0; i < source.sections.size(); ++i)
					if (source.sections[i].offset >= static_cast<int>(offset)) patch(source.game_offset + 35 + i * 12, source.sections[i].offset + static_cast<unsigned>(data.size()));
			};
			require(inventory.level_version == 23 && inventory.game_version == 40 && inventory.walls.size() >= 16 && inventory.triggers.size() == 1, "unexpected object-event geometry source");
			// Minimal ordinary robot, encoded in the inspected 23/40 object layout
			xl_compat::Bytes robot;
			for (int value : std::array<int, 7>{ OBJ_ROBOT, 0, CT_AI, MT_PHYSICS, RT_POLYOBJ, 0, 0 }) append(robot, value, 1);
			append(robot, 0, 2);
			std::array<int, 3> center{};
			for (int component = 0; component < 3; ++component) {
				std::int64_t total = 0;
				for (int vertex : inventory.segments[0].vertices) total += inventory.vertices[vertex][component];
				center[component] = static_cast<int>(total / 8);
				append(robot, center[component], 4);
			}
			for (int i = 0; i < 9; ++i) append(robot, i % 4 == 0 ? F1_0 : 0, 4);
			append(robot, Polygon_models[Robot_info[0].model_num].rad, 4);
			append(robot, Robot_info[0].strength, 4);
			for (int component : center) append(robot, component, 4);
			for (int value : { -1, -1, 0 }) append(robot, value, 1);
			for (int i = 0; i < 15; ++i) append(robot, i == 6 ? F1_0 : 0, 4);
			append(robot, 0, 2);
			append(robot, 0, 2);
			append(robot, AIB_NORMAL, 1);
			for (int i = 1; i < 12; ++i) append(robot, 0, 1);
			for (int i = 0; i < 4; ++i) append(robot, i == 0 ? -1 : 0, 2);
			append(robot, Robot_info[0].model_num, 4);
			for (int i = 0; i < 30; ++i) append(robot, 0, 2);
			append(robot, 0, 4);
			append(robot, -1, 4);
			const int robot_index = static_cast<int>(inventory.objects.size());
			insert(inventory, inventory.objects.back().source_end, robot);
			patch(inventory.game_offset + 39, robot_index + 1);
			inventory = xl_compat::inspect_level(member.bytes);
			std::vector<int> powerups;
			int reactor = -1;
			for (std::size_t i = 0; i < inventory.objects.size(); ++i) {
				if (inventory.objects[i].type == OBJ_POWERUP) powerups.push_back(static_cast<int>(i));
				if (inventory.objects[i].type == OBJ_CNTRLCEN) reactor = static_cast<int>(i);
			}
			require(powerups.size() >= 4 && reactor >= 0, "object-event owners absent");
			const auto &section = *std::find_if(inventory.sections.begin(), inventory.sections.end(), [](const auto &s) { return s.name == "triggers"; });
			const auto &walls = *std::find_if(inventory.sections.begin(), inventory.sections.end(), [](const auto &s) { return s.name == "walls"; });
			patch(section.offset, 31, 1);
			patch(section.offset + 1, 0, 1);
			patch(section.offset + 2, 0, 1);
			patch(section.offset + 4, 0);
			patch(section.offset + 8, -1);
			patch(walls.offset + 20, 0, 1);
			for (int wall = 8; wall < 16; ++wall) {
				patch(walls.offset + wall * 25 + 16, WALL_ILLUSION, 1);
				patch(walls.offset + wall * 25 + 17, 0, 2);
			}
			xl_compat::Bytes payload;
			std::vector<int> owners;
			const auto action = [&](int owner, int type, int flags, int value, std::initializer_list<std::array<int, 2>> targets) {
				append(payload, type, 1);
				append(payload, flags, 2);
				append(payload, static_cast<unsigned>(targets.size()), 1);
				append(payload, 0, 1);
				append(payload, value, 4);
				append(payload, 1000, 4);
				for (int field = 0; field < 2; ++field) {
					for (const auto &target : targets) append(payload, target[field], 2);
					for (std::size_t i = targets.size(); i < 10; ++i) append(payload, 0, 2);
				}
				owners.push_back(owner);
			};
			const auto wall_target = [&](int wall) { return std::array<int, 2>{ inventory.walls[wall].segment, inventory.walls[wall].side }; };
			action(robot_index, 14, 0, 5 * F1_0, { { 1, -1 } });
			action(robot_index, 14, 8, 5 * F1_0, { { 0, -1 }, { 1, -1 }, { 2, -1 } });
			action(robot_index, TT_ILLUSION_OFF, 16, 0, { wall_target(8) });
			action(powerups[0], TT_ILLUSION_OFF, TF_ONE_SHOT, 0, { wall_target(10) });
			action(powerups[1], TT_ILLUSION_OFF, 128 | 8, 0, { wall_target(12) });
			action(powerups[2], 33, 0, 0, { wall_target(0) });
			action(powerups[2], 32, 0, 0, { wall_target(0) });
			action(reactor, 32, 0, 0, { wall_target(0) });
			action(powerups[3], TT_ILLUSION_OFF, TF_DISABLED, 0, { wall_target(14) });
			for (int owner : owners) append(payload, owner, 2);
			const auto offset = static_cast<std::size_t>(section.offset) + 52;
			patch(offset, static_cast<unsigned>(owners.size()));
			insert(inventory, offset + 4, payload);
			const auto final = xl_compat::inspect_level(member.bytes);
			const auto blockers = xl_compat::runtime_blockers(final);
			if (!blockers.empty()) throw std::runtime_error(xl_compat::blocker_message(blockers));
			std::ofstream manifest(root / "events-fixture.json");
			manifest << nlohmann::json({ { "robot", robot_index }, { "reactor", reactor }, { "powerups", powerups }, { "object_actions", owners.size() } }).dump(2) << '\n';
		}
		char name[13] = {};
		require(member.name.size() < sizeof(name), "object fixture member exceeds bound");
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(static_cast<unsigned char>(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "events.hog").string().c_str(), output);
	std::ofstream descriptor(root / "events.mn2");
	descriptor << "d2x-name = Derived object events\nnum_levels = 1\nneptunxl.rl2\n";
}

static int object_trigger_test(int argc, char **argv)
{
	try {
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
		write_object_event_fixture(root);
		if (!std::strcmp(find_arg_value(argc, argv, "-object-trigger-test"), "export")) return 0;
		std::ifstream manifest_file(root / "events-fixture.json");
		const auto manifest = nlohmann::json::parse(manifest_file);
		const int robot = manifest.at("robot"), reactor = manifest.at("reactor");
		const auto powerups = manifest.at("powerups").get<std::vector<int>>();
		require(load_base_mission("events", error, sizeof(error)), error);
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_level_extensions_active() && xl_object_trigger_count() == 9 && Objects[robot].type == OBJ_ROBOT, "object source did not start");
		const auto state = [](int index) { xl_object_trigger_state result; require(xl_object_trigger_state_get(index, &result), "missing object state"); return result; };
		const auto find = [&](int owner, int type, int flags) {
			for (int i = 0; i < xl_object_trigger_count(); ++i) {
				const auto value = state(i);
				if (value.owner == owner && value.type == type && value.flags == flags) return i;
			}
			throw std::runtime_error("object action not found");
		};
		const int once = find(robot, 14, 0), repeat = find(robot, 14, 8), death = find(robot, TT_ILLUSION_OFF, 16);
		const int pickup = find(powerups[0], TT_ILLUSION_OFF, TF_ONE_SHOT), autoplay = find(powerups[1], TT_ILLUSION_OFF, 136), disabled = find(powerups[3], TT_ILLUSION_OFF, TF_DISABLED);
		char initial[] = "xl-object-initial.sav", snapshot[] = "xl-object-live.sav", description[] = "Object events";
		stop_time();
		require(state_save_all_sub(initial, description) != 0, "initial object save failed");
		start_time();
		const auto velocity = vms_vector{ F1_0, 2 * F1_0, -3 * F1_0 };
		Objects[robot].mtype.phys_info.velocity = velocity;
		const auto orientation = Objects[robot].orient;
		const auto maximum = Robot_info[Objects[robot].id].strength;
		Objects[robot].shields = maximum * 3 / 4;
		require(!apply_damage_to_robot(&Objects[robot], maximum / 4, -1) && !state(once).operated, "nonlocal damage activated an object action");
		require(!apply_damage_to_robot(&Objects[robot], 1, ConsoleObject - Objects), "nonlethal robot damage was lethal");
		require(state(once).operated && state(once).value == 0 && state(repeat).operated && state(repeat).value == 5 * F1_0, "threshold event or permanent consumption differs");
		require(!std::memcmp(&Objects[robot].orient, &orientation, sizeof(orientation)) && !std::memcmp(&Objects[robot].mtype.phys_info.velocity, &velocity, sizeof(velocity)), "object teleport changed orientation or velocity");
		const int old_destination = state(repeat).destination;
		apply_damage_to_robot(&Objects[robot], 1, ConsoleObject - Objects);
		require(state(repeat).destination != old_destination && Objects[robot].segnum == state(repeat).destination, "permanent object teleport repeated its previous destination");
		triggers_frame_process();
		require(state(autoplay).operated && (state(autoplay).flags & TF_DISABLED) && (Walls[12].flags & WALL_ILLUSION_OFF), "permanent object autoplay did not fire and disable");
		Objects[powerups[0]].flags |= OF_SHOULD_BE_DEAD;
		obj_delete_all_that_should_be_dead();
		require(state(pickup).owner == -1 && (state(pickup).flags & TF_DISABLED) && (Walls[10].flags & WALL_ILLUSION_OFF), "production removal did not consume one-shot object event");
		Triggers[0].value = 0;
		Triggers[0].flags = 0;
		Objects[powerups[2]].flags |= OF_SHOULD_BE_DEAD;
		obj_delete_all_that_should_be_dead();
		require(Triggers[0].value == 1 && (Triggers[0].flags & TF_DISABLED), "owner/type sorting did not preserve enable-before-disable order");
		Objects[powerups[3]].flags |= OF_SHOULD_BE_DEAD;
		obj_delete_all_that_should_be_dead();
		require(state(disabled).owner == -1 && !state(disabled).operated && !(Walls[14].flags & WALL_ILLUSION_OFF), "disabled removal event was fired or left attached");
		Triggers[0].value = 1;
		do_controlcen_destroyed_stuff(&Objects[reactor]);
		require(Triggers[0].value == 0 && !(Triggers[0].flags & TF_DISABLED), "reactor destruction event failed");
		do_controlcen_destroyed_stuff(&Objects[reactor]);
		require(Triggers[0].value == 0, "reactor event fired twice");
		Control_center_destroyed = 0;
		stop_time();
		require(state_save_all_sub(snapshot, description) != 0, "live object save failed");
		start_time();
		const int saved_destination = state(repeat).destination;
		xl_object_trigger_event(robot, 0);
		require(state(death).owner == -1 && state(death).type == TT_ILLUSION_ON && (Walls[8].flags & WALL_ILLUSION_OFF), "death action did not alternate and consume owner");
		require(state_restore_all_sub(snapshot, 0) != 0 && state(repeat).destination == saved_destination && state(death).owner == robot && state(pickup).owner == -1, "disk restore lost live or consumed object state");
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "object stock transition failed");
		require(xl_object_trigger_count() == 0, "object bindings leaked into stock");
		require(Num_triggers > 0 && Num_walls > 0, "stock leaf control lacks authored geometry");
		Triggers[0].type = TT_ILLUSION_OFF;
		Triggers[0].flags = 0;
		Triggers[0].num_links = 1;
		Triggers[0].seg[0] = Walls[0].segnum;
		Triggers[0].side[0] = Walls[0].sidenum;
		Walls[0].type = WALL_ILLUSION;
		Walls[0].flags &= ~WALL_ILLUSION_OFF;
		require(check_trigger_sub(0, Player_num, 1) == 0 && (Walls[0].flags & WALL_ILLUSION_OFF), "stock illusion-off dispatch regressed");
		Triggers[0].type = TT_ILLUSION_ON;
		require(check_trigger_sub(0, Player_num, 1) == 0 && !(Walls[0].flags & WALL_ILLUSION_OFF), "stock illusion-on dispatch regressed");
		require(state_restore_all_sub(snapshot, 0) != 0 && xl_object_trigger_count() == 9 && state(repeat).destination == saved_destination, "stock-to-object source restoration failed");
		require(state_restore_all_sub(initial, 0) != 0, "initial source restore failed");
		const int original_signature = Objects[powerups[0]].signature;
		const int segment = Objects[powerups[0]].segnum;
		const auto position = Objects[powerups[0]].pos;
		obj_delete(powerups[0]);
		const int replacement = obj_create(OBJ_POWERUP, POW_ENERGY, segment, &position, &vmd_identity_matrix, F1_0, CT_POWERUP, MT_NONE, RT_NONE);
		require(replacement == powerups[0] && Objects[replacement].signature != original_signature, "slot reuse control did not replace the owner");
		xl_object_trigger_event(replacement, 0);
		require(state(pickup).owner == -1 && !state(pickup).operated && !(Walls[10].flags & WALL_ILLUSION_OFF), "replacement object inherited an authored event");
		require(state_restore_all_sub(initial, 0) != 0, "lethal source restore failed");
		require(apply_damage_to_robot(&Objects[robot], Objects[robot].shields + F1_0, ConsoleObject - Objects) != 0, "lethal combat control did not kill robot");
		for (int frame = 0; frame < 180 && state(death).owner >= 0; ++frame) {
			for (int object = 0; object <= Highest_object_index; ++object)
				if (Objects[object].type != OBJ_NONE && Objects[object].control_type == CT_EXPLOSION) {
					Objects[object].lifeleft -= F1_0 / 30;
					do_explosion_sequence(&Objects[object]);
				}
			obj_delete_all_that_should_be_dead();
		}
		require(state(death).owner == -1 && state(death).type == TT_ILLUSION_ON && (Walls[8].flags & WALL_ILLUSION_OFF) && !state(once).operated, "lethal combat/removal did not select the death event alone");
		require(state_restore_all_sub(initial, 0) != 0, "event sample source restore failed");
		std::ofstream samples(root.parent_path() / "object-event-samples.txt");
		for (int type : { 14, int(TT_ILLUSION_OFF) })
			for (int value : { -65536, -1, 0, 1, 5, 9, 10, 11, 65535, 65536, 5 * 65536, 10 * 65536, INT32_MAX })
				for (int flags : { 0, 4, 8, 12 })
					for (int damage : { 0, 1 })
						for (int percent : { 0, 49, 50, 51, 89, 90, 100 }) {
							xl_compat::Inventory sample;
							sample.object_triggers = 1;
							sample.object_trigger_records.push_back({ { type, flags, value, -1, {} }, robot });
							xl_object_trigger_load(sample);
							Objects[robot].shields = static_cast<fix>(std::int64_t(maximum) * percent / 100);
							xl_object_trigger_event(robot, damage);
							const auto result = state(0);
							samples << type << ' ' << value << ' ' << flags << ' ' << damage << ' ' << Objects[robot].shields << ' ' << maximum << ' ' << result.operated << ' ' << result.value << '\n';
						}
		samples.close();
		require(bool(samples), "object event samples not written");
		require(state_restore_all_sub(initial, 0) != 0, "sample overrides did not restore source state");
		auto *file = PHYSFS_openWrite("object-state.bin");
		require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "object extension probe write failed");
		const auto user = root.parent_path() / "preview-user";
		std::ifstream encoded_file(user / "object-state.bin", std::ios::binary);
		const xl_compat::Bytes encoded{ std::istreambuf_iterator<char>(encoded_file), std::istreambuf_iterator<char>() };
		require(encoded.size() > 16, "object extension missing payload");
		for (int variant = 0; variant < 16; ++variant) {
			auto bad = encoded;
			if (variant == 0) bad.clear();
			else if (variant == 1) bad.resize(15);
			else if (variant == 2) bad[12] ^= 1;
			else {
				auto block = nlohmann::json::parse(encoded.begin() + 16, encoded.end());
				block["states"][0] = { 0, -1, ConsoleObject - Objects, 0, 1 };
				if (variant == 3) block["objects"].erase(block["objects"].begin());
				if (variant == 4) block["source"] = block["source"].get<std::uint64_t>() ^ 1;
				if (variant == 5) block["objects"][once][0] = MAX_OBJECTS;
				if (variant == 6) block["objects"][once][1] = state(once).signature + 1;
				if (variant == 7) block["objects"][once][2] = 0;
				if (variant == 8) block["objects"][once][3] = 128;
				if (variant == 9) block["objects"][once][4] = F1_0;
				if (variant == 10) block["objects"][once][5] = 2;
				if (variant == 11) block["objects"][repeat][6] = Num_segments;
				if (variant == 12) block["objects"][once][0] = UINT64_MAX;
				if (variant == 13) block["objects"][disabled][3] = 0;
				if (variant == 14) block["schema"] = 1;
				if (variant == 15) {
					block["states"][0][0] = -1;
					block["objects"][once][5] = 1;
				}
				const auto text = block.dump();
				bad.resize(16);
				bad.insert(bad.end(), text.begin(), text.end());
				std::uint32_t checksum = 2166136261u;
				for (unsigned char byte : text) checksum = (checksum ^ byte) * 16777619u;
				for (int byte = 0; byte < 4; ++byte) {
					bad[8 + byte] = static_cast<unsigned char>(text.size() >> (byte * 8));
					bad[12 + byte] = static_cast<unsigned char>(checksum >> (byte * 8));
				}
			}
			write_bytes((user / "object-corrupt.bin").string().c_str(), bad);
			file = PHYSFS_openRead("object-corrupt.bin");
			require(file && !xl_wall_trigger_read(file) && PHYSFS_close(file), "malformed object extension accepted");
			xl_trigger_state wall;
			require(xl_wall_trigger_state(0, &wall) && !wall.operated && !state(once).operated && state(once).owner == robot, "malformed extension published partial wall or object state");
		}
		std::cout << "XL object event controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
