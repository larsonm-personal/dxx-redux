#include "boss_destinations.h"
static void write_boss_groups_fixture(const std::filesystem::path &root)
{
	write_boss_transport_fixture(root);
	std::ifstream input(root / "bosstran.hog", std::ios::binary);
	xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() }, output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(bytes)) {
		if (member.name == "neptunxl.rl2") {
			const auto source = xl_compat::inspect_level(member.bytes);
			// Known 23/40 geometry walk; only group bytes and wall types change
			std::size_t offset = source.mine_offset + 5 + source.vertices.size() * 12;
			const auto bits = [](unsigned value) { int count = 0; for (; value; value >>= 1) count += value & 1; return count; };
			for (std::size_t i = 0; i < source.segments.size(); ++i) {
				member.bytes.at(offset + 1) = i == 70 ? 1 : i == 84 ? 2
				                                                    : 255;
				offset += 2;
				const auto children = member.bytes.at(offset++);
				offset += bits(children & 63) * 2 + 16;
				const auto walls = member.bytes.at(offset++);
				offset += bits(walls & 63) * 2;
				for (int side = 0; side < 6; ++side) {
					const auto &face = source.segments[i].sides[side];
					if (source.segments[i].children[side] != -1 && face.wall == -1) continue;
					const bool overlay = (member.bytes.at(offset + 1) & 128) != 0;
					offset += 26 + (overlay ? 2 : 0);
				}
			}
			for (std::size_t i = 0; i < source.walls.size(); ++i) member.bytes.at(source.sections[1].offset + i * 25 + 16) = WALL_OPEN;
			const auto changed = xl_compat::inspect_level(member.bytes);
			require(xl_compat::runtime_blockers(changed).empty(), "boss groups fixture gained blockers");
		}
		char name[13] = {};
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (int i = 0; i < 4; ++i) output.push_back(static_cast<unsigned char>(member.bytes.size() >> (8 * i)));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "bossgrp.hog").string().c_str(), output);
	std::ofstream descriptor(root / "bossgrp.mn2");
	descriptor << "d2x-name = Derived XL boss groups\nnum_levels = 1\nneptunxl.rl2\n";
}

static int boss_groups_test(int argc, char **argv)
{
	try {
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
		write_boss_groups_fixture(root);
		require(load_base_mission("bossgrp", error, sizeof(error)), error);
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_boss_spawn_count() == 2 && xl_level_segment_group(70) == 1 && xl_level_segment_group(84) == 2, "authored boss group source not retained");
		// Physical passability alone would reach other-group rooms from both homes
		for (int home : { 70, 84 }) {
			const int owner = home == 70 ? 49 : 50;
			const auto position = Objects[owner].pos;
			const int segment = Objects[owner].segnum;
			const auto ignored = xl_compat::boss_destinations(home, Num_segments, [](int i, int side) { return Segments[i].children[side]; }, [](int i, int side) { return (WALL_IS_DOORWAY(&Segments[i], side) & WID_FLY_FLAG) != 0; }, [](int) { return 0; }, [&](int i) { return boss_fits_in_seg(&Objects[owner], i) != 0; });
			Objects[owner].pos = position;
			obj_relink(owner, segment);
			require(ignored.size() > 1, "group fixture has no open boundary negative control");
		}
		const auto initial = xl_compat::boss_routes();
		require(initial.size() == 2 && initial[0].home == 20 && initial[1].home == 24, "initial boss homes differ from authored source");
		for (const auto &route : initial)
			for (int segment : route.segments) require(xl_level_segment_group(segment) == -1, "initial setup crossed a default-group boundary");
		Objects[49].shields /= 4;
		xl_object_trigger_event(49, 1);
		xl_object_trigger_frame();
		auto routes = xl_compat::boss_routes();
		require(routes[0].home == 70 && routes[0].segments == std::vector<int>{ 70 } && routes[1].segments == std::vector<int>{ 84 }, "authored damage/master transport ignored destination groups");
		for (int seed = 1; seed < 30; ++seed) {
			d_srand(seed);
			xl_boss_teleport(49);
			xl_boss_teleport(50);
			require(Objects[49].segnum == 70 && Objects[50].segnum == 84, "actual AI escaped an open group boundary");
		}
		char save[] = "xl-groups.sav", description[] = "XL boss groups";
		stop_time();
		require(state_save_all_sub(save, description), "boss group save failed");
		start_time();
		// Cross-group route corruption must reject before publishing event state
		auto *file = PHYSFS_openWrite("groups-state.bin");
		require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "group trailer capture failed");
		const auto user = root.parent_path() / "preview-user";
		std::ifstream encoded_file(user / "groups-state.bin", std::ios::binary);
		xl_compat::Bytes encoded{ std::istreambuf_iterator<char>(encoded_file), std::istreambuf_iterator<char>() };
		auto block = nlohmann::json::parse(encoded.begin() + 16, encoded.end());
		block["states"][0][4] = 0;
		block["boss_routes"][0][1].push_back(84);
		const auto text = block.dump();
		xl_compat::Bytes corrupt(encoded.begin(), encoded.begin() + 16);
		corrupt.insert(corrupt.end(), text.begin(), text.end());
		std::uint32_t checksum = 2166136261u;
		for (unsigned char byte : text) checksum = (checksum ^ byte) * 16777619u;
		for (int i = 0; i < 4; ++i) {
			corrupt[8 + i] = static_cast<unsigned char>(text.size() >> (8 * i));
			corrupt[12 + i] = static_cast<unsigned char>(checksum >> (8 * i));
		}
		write_bytes((user / "groups-bad.bin").string().c_str(), corrupt);
		file = PHYSFS_openRead("groups-bad.bin");
		require(file && !xl_wall_trigger_read(file) && PHYSFS_close(file), "cross-group route was accepted");
		file = PHYSFS_openWrite("groups-after.bin");
		require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "group trailer recapture failed");
		std::ifstream after_file(user / "groups-after.bin", std::ios::binary);
		require(xl_compat::Bytes(std::istreambuf_iterator<char>(after_file), std::istreambuf_iterator<char>()) == encoded, "cross-group corruption partially published state");
		// Switching home groups rebuilds only the moved owner; default -1 is real identity
		trigger action{};
		action.type = 14;
		action.num_links = 1;
		action.seg[0] = 84;
		int destination = 0;
		require(xl_object_teleport_action(&action, 49, &destination) == 0 && xl_compat::boss_routes()[0].segments == std::vector<int>{ 84 }, "boss did not join its new destination group");
		action.seg[0] = 20;
		require(xl_object_teleport_action(&action, 49, &destination) == 0, "default group transport failed");
		const auto unowned = xl_compat::boss_routes();
		require(unowned[0].segments.size() > 1 && unowned[1].segments == routes[1].segments, "default group rebuild changed another owner");
		for (int segment : unowned[0].segments) require(xl_level_segment_group(segment) == -1, "default group acted as a wildcard");
		for (int seed = 1; seed < 30; ++seed) {
			d_srand(seed);
			xl_boss_teleport(49);
			require(xl_level_segment_group(Objects[49].segnum) == -1, "default group AI crossed a label");
		}
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0 && xl_compat::boss_routes().empty(), "group source contaminated stock");
		require(state_restore_all_sub(save, 0) && same_boss_routes(routes, xl_compat::boss_routes()) && xl_level_segment_group(70) == 1, "disk restore lost source group or earned destinations");
		for (int seed = 1; seed < 30; ++seed) {
			d_srand(seed);
			xl_boss_teleport(49);
			require(Objects[49].segnum == 70, "restored AI crossed group boundary");
		}
		std::cout << "XL boss group controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
