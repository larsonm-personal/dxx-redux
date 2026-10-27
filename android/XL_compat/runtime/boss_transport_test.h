#include "boss_routes.h"
extern "C" int boss_fits_in_seg(object *boss, int segment);

static bool same_boss_routes(const std::vector<xl_compat::BossRoutes> &a, const std::vector<xl_compat::BossRoutes> &b)
{
	if (a.size() != b.size()) return false;
	for (std::size_t i = 0; i < a.size(); ++i)
		if (a[i].home != b[i].home || a[i].segments != b[i].segments) return false;
	return true;
}

static void write_boss_transport_fixture(const std::filesystem::path &root)
{
	write_boss_fixture(root);
	std::ifstream file(root / "bosses.hog", std::ios::binary);
	xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() }, output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(bytes)) {
		if (member.name == "badowner.rl2" || member.name == "badmaker.rl2") continue;
		if (member.name == "neptunxl.rl2") {
			const auto level = xl_compat::inspect_level(member.bytes);
			const auto patch = [&](std::size_t offset, std::uint32_t value, int width = 4) {
				for (int i = 0; i < width; ++i) member.bytes.at(offset + i) = static_cast<unsigned char>(value >> (8 * i));
			};
			const auto triggers = level.sections[3].offset;
			patch(triggers, 14, 1);
			patch(triggers + 1, 0, 1);
			patch(triggers + 2, 1, 1);
			patch(triggers + 4, 0);
			patch(triggers + 8, -1);
			patch(triggers + 12, 84, 2);
			patch(triggers + 32, 0, 2);
			const auto first = triggers + level.triggers.size() * 52 + 4;
			patch(first, 14, 1);
			patch(first + 13, 70, 2);
			const auto second = first + 53;
			patch(second, 31, 1);
			patch(second + 1, 128, 2);
			patch(second + 5, 0);
			patch(second + 13, 24, 2);
			patch(second + 33, 5, 2);
			// These original pairs are illusions. Make the two destination rooms
			// truly isolated so cached routes distinguish object-teleport setup
			for (int wall : { 8, 9, 18, 19 }) patch(level.sections[1].offset + wall * 25 + 16, WALL_CLOSED, 1);
			const auto changed = xl_compat::inspect_level(member.bytes);
			require(xl_compat::runtime_blockers(changed).empty(), "boss transport fixture gained unrelated blockers");
		}
		char name[13] = {};
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (int i = 0; i < 4; ++i) output.push_back(static_cast<unsigned char>(member.bytes.size() >> (i * 8)));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "bosstran.hog").string().c_str(), output);
	std::ofstream descriptor(root / "bosstran.mn2");
	descriptor << "d2x-name = Derived XL boss transport\nnum_levels = 1\nneptunxl.rl2\n";
}

static void boss_transport_samples(const std::filesystem::path &root, char *baseline, const std::vector<int> &owners)
{
	std::ofstream samples(root.parent_path() / "boss-transport-samples.txt");
	samples << 24 << '\n';
	const auto vector = [&](const vms_vector &v) { samples << ' ' << v.x << ' ' << v.y << ' ' << v.z; };
	for (int seed : { 3, 91, 271 })
		for (int scenario = 0; scenario < 8; ++scenario) {
			require(state_restore_all_sub(baseline, 0), "boss transport sample restore failed");
			GameTime64 = i2f(40);
			std::array<xl_boss_spawn_state, 2> seeded;
			for (int i = 0; i < 2; ++i) {
				require(xl_boss_spawn_state_get(i, &seeded[i]), "sample boss clock seed absent");
				seeded[i].elapsed = i2f(i + 1);
				seeded[i].cloak_start = -i2f(i + 2);
				seeded[i].cloak_end = seeded[i].cloak_start + BOSS_CLOAK_DURATION;
				seeded[i].teleport = -i2f(i + 3);
				seeded[i].hit = -i2f(i + 4);
				if (scenario == 5 && i == 1) {
					Objects[owners[i]].shields = -1;
					seeded[i].dying = seeded[i].sound = 1;
					seeded[i].death_elapsed = i2f(1);
				}
			}
			require(xl_boss_spawn_restore(seeded.data(), 2, 1), "sample nonzero boss clocks rejected");
			const int actor = scenario == 5 || scenario == 6 ? 1 : 0, owner = owners[actor];
			trigger action{};
			action.type = 14;
			action.num_links = scenario == 0 ? 0 : scenario == 2 ? 2
			                                                     : 1;
			action.seg[0] = scenario == 3 ? Objects[owner].segnum : scenario == 4 || scenario == 6 ? 84
			                                                    : scenario == 7                    ? 3
			                                                                                       : 70;
			action.seg[1] = 84;
			int destination = scenario == 2 ? 70 : 0;
			if (scenario == 3)
				for (int i = 0; i < Num_walls; ++i) Walls[i].type = WALL_CLOSED;
			if (scenario == 7) Objects[owner].size = i2f(10000);
			const auto prior_routes = xl_compat::boss_routes();
			const auto position = Objects[owner].pos;
			const int segment = Objects[owner].segnum;
			const auto facing = Objects[owner].orient;
			const auto motion = Objects[owner].mtype.phys_info.velocity;
			std::vector<int> fits(Num_segments);
			for (int i = 0; i < Num_segments; ++i) fits[i] = boss_fits_in_seg(&Objects[owner], i);
			Objects[owner].pos = position;
			obj_relink(owner, segment);
			std::array<xl_boss_spawn_state, 2> clocks;
			for (int i = 0; i < 2; ++i) require(xl_boss_spawn_state_get(i, &clocks[i]), "sample boss clocks absent");
			std::ostringstream before;
			for (int i = 0; i < Num_segments; ++i) {
				before << xl_level_segment_group(i);
				for (int child : Segments[i].children) before << ' ' << child;
				for (int side = 0; side < 6; ++side) before << ' ' << (WALL_IS_DOORWAY(&Segments[i], side) & WID_FLY_FLAG ? 1 : 0);
				vms_vector center;
				compute_segment_center(&center, &Segments[i]);
				before << ' ' << center.x << ' ' << center.y << ' ' << center.z << ' ' << fits[i] << '\n';
			}
			for (int i = 0; i < 2; ++i) {
				const auto &o = Objects[owners[i]];
				before << o.segnum << ' ' << o.pos.x << ' ' << o.pos.y << ' ' << o.pos.z << ' ' << prior_routes[i].segments.size();
				for (int s : prior_routes[i].segments) before << ' ' << s;
				before << '\n';
			}
			d_srand(seed);
			unsigned rng;
			require(d_rand_get_state(&rng), "boss transport RNG state unavailable");
			const auto calls = d_rand_get_call_count();
			if (scenario == 6) {
				xl_object_trigger_frame();
				xl_trigger_state child;
				require(xl_wall_trigger_state(0, &child), "master child state absent");
				destination = child.destination;
			} else require(xl_object_teleport_action(&action, owner, &destination) == 0, "boss transport action failed");
			const auto after = xl_compat::boss_routes();
			samples << Num_segments << ' ' << actor << ' ' << int(action.num_links) << ' ' << (scenario == 2 ? 70 : 0) << ' ' << d_rand_get_call_count() - calls << '\n'
			        << before.str();
			for (int i = 0; i < action.num_links; ++i) samples << action.seg[i] << ' ';
			samples << '\n';
			unsigned final_rng;
			require(d_rand_get_state(&final_rng), "boss transport final RNG unavailable");
			const auto final_calls = d_rand_get_call_count();
			d_rand_set_state(rng);
			d_rand_set_call_count(calls);
			while (d_rand_get_call_count() < final_calls) samples << d_rand() << ' ';
			unsigned replayed;
			require(d_rand_get_state(&replayed) && replayed == final_rng, "boss transport RNG transcript differs");
			samples << '\n'
			        << destination << '\n';
			for (int i = 0; i < 2; ++i) {
				const auto &o = Objects[owners[i]];
				samples << o.segnum;
				vector(o.pos);
				samples << ' ' << after[i].segments.size();
				for (int s : after[i].segments) samples << ' ' << s;
				samples << '\n';
				xl_boss_spawn_state now;
				require(xl_boss_spawn_state_get(i, &now), "sample boss state disappeared");
				require(now.elapsed == clocks[i].elapsed && now.cloak_start == clocks[i].cloak_start && now.cloak_end == clocks[i].cloak_end && now.teleport == clocks[i].teleport && now.hit == clocks[i].hit && now.dying == clocks[i].dying && now.death_elapsed == clocks[i].death_elapsed && now.sound == clocks[i].sound, "object teleport reset boss clocks/death state");
			}
			require(!std::memcmp(&facing, &Objects[owner].orient, sizeof(facing)) && !std::memcmp(&motion, &Objects[owner].mtype.phys_info.velocity, sizeof(motion)), "boss object teleport changed facing or velocity");
			if (scenario == 3) require(same_boss_routes(prior_routes, after), "same-segment action rebuilt cached destinations");
		}
	require(bool(samples), "boss transport samples not written");
}

static void boss_route_guards(const std::filesystem::path &user)
{
	const auto capture = [&] {
		auto *file = PHYSFS_openWrite("transport-state.bin");
		require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "route guard capture failed");
		std::ifstream input(user / "transport-state.bin", std::ios::binary);
		return xl_compat::Bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	};
	const auto original = capture();
	for (int variant = 0; variant < 16; ++variant) {
		auto bad = original;
		auto block = nlohmann::json::parse(original.begin() + 16, original.end());
		block["states"][0][4] = 0;
		block["objects"][0][5] = 0;
		auto &row = block["boss_routes"][0];
		if (variant == 0) block.erase("boss_routes");
		if (variant == 1) block["boss_routes"] = nlohmann::json::array();
		if (variant == 2) row.push_back(0);
		if (variant == 3) row[0] = -1;
		if (variant == 4) row[0] = nlohmann::json::parse("true");
		if (variant == 5) row[0] = UINT64_MAX;
		if (variant == 6) row[0] = Num_segments;
		if (variant == 7) row[1] = nlohmann::json::array();
		if (variant == 8) row[1][0] = -1;
		if (variant == 9) row[1][0] = Num_segments;
		if (variant == 10) row[1][0] = nlohmann::json::parse("true");
		if (variant == 11) row[1][0] = 1.5;
		if (variant == 12) row[1][0] = UINT64_MAX;
		if (variant == 13) row[1] = { 70, 70, 70 };
		if (variant == 14) row[1] = { 70, 71, 71 };
		if (variant == 15) row[1] = std::vector<int>(1001, 70);
		const auto text = block.dump();
		bad.resize(16);
		bad.insert(bad.end(), text.begin(), text.end());
		std::uint32_t checksum = 2166136261u;
		for (unsigned char byte : text) checksum = (checksum ^ byte) * 16777619u;
		for (int byte = 0; byte < 4; ++byte) {
			bad[8 + byte] = static_cast<unsigned char>(text.size() >> (byte * 8));
			bad[12 + byte] = static_cast<unsigned char>(checksum >> (byte * 8));
		}
		write_bytes((user / "transport-bad.bin").string().c_str(), bad);
		auto *file = PHYSFS_openRead("transport-bad.bin");
		require(file && !xl_wall_trigger_read(file) && PHYSFS_close(file), "malformed destination state accepted");
		require(capture() == original, "malformed destination state partially published events or routes");
	}
	std::cout << "XL boss destination guards passed: 16 malformed payloads rejected before publication\n";
}

static int boss_transport_test(int argc, char **argv)
{
	try {
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
		write_boss_transport_fixture(root);
		std::ifstream fixture_file(root / "boss-fixture.json");
		const auto fixture = nlohmann::json::parse(fixture_file);
		const auto owners = fixture.at("owners").get<std::vector<int>>();
		require(load_base_mission("bosstran", error, sizeof(error)), error);
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_boss_spawn_count() == 2 && xl_level_supported("neptunxl.rl2", 0), "boss object teleport source was not admitted");
		char baseline[] = "transport-base.sav", earned[] = "transport-earned.sav", description[] = "Boss transport";
		stop_time();
		require(state_save_all_sub(baseline, description), "boss transport baseline save failed");
		start_time();
		boss_transport_samples(root, baseline, owners);
		require(state_restore_all_sub(baseline, 0), "boss transport acceptance reset failed");
		const auto initial = xl_compat::boss_routes();
		Objects[owners[0]].shields /= 4;
		xl_object_trigger_event(owners[0], 1);
		auto routes = xl_compat::boss_routes();
		xl_object_trigger_state damage_action;
		xl_object_trigger_state_get(0, &damage_action);
		std::cout << "Boss damage transport witness: segment=" << Objects[owners[0]].segnum << " home=" << routes[0].home << " destinations=" << routes[0].segments.size() << " other=" << routes[1].segments.size() << " initial-other=" << initial[1].segments.size() << " action=" << int(damage_action.type) << '/' << damage_action.value << '/' << damage_action.operated << '\n';
		require(Objects[owners[0]].segnum == 70 && routes[0].home == 70 && routes[0].segments == std::vector<int>{ 70 } && routes[1].segments == initial[1].segments, "damage teleport did not isolate the moved boss at its authored destination");
		xl_object_trigger_frame();
		routes = xl_compat::boss_routes();
		require(Objects[owners[1]].segnum == 84 && routes[1].home == 84 && routes[1].segments == std::vector<int>{ 84 }, "autoplay object master lost boss context or destination rebuild");
		// Opening the old boundary changes a newly computed list, not the earned cache
		Walls[8].type = Walls[9].type = WALL_OPEN;
		int destination = 0;
		trigger same{};
		same.type = 14;
		same.num_links = 1;
		same.seg[0] = 70;
		xl_object_teleport_action(&same, owners[0], &destination);
		require(same_boss_routes(routes, xl_compat::boss_routes()), "same-segment action refreshed an earned list after wall changes");
		stop_time();
		require(state_save_all_sub(earned, description), "earned boss destination save failed");
		start_time();
		boss_route_guards(root.parent_path() / "preview-user");
		const auto stock_time = Last_teleport_time;
		for (int seed = 1; seed < 30; ++seed) {
			d_srand(seed);
			xl_boss_teleport(owners[0]);
			require(Objects[owners[0]].segnum == 70, "AI boss teleport escaped the cached destination list");
		}
		require(Last_teleport_time == stock_time, "XL object transport touched Redux global timing");
		xl_boss_object_teleported(owners[0]);
		require(xl_compat::boss_routes()[0].segments.size() > 1, "opened-wall negative control did not distinguish recomputation");
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0 && xl_compat::boss_routes().empty(), "boss routes leaked into stock");
		require(state_restore_all_sub(earned, 0) && same_boss_routes(routes, xl_compat::boss_routes()), "stock-to-XL restore recomputed or lost earned destinations");
		for (int seed = 1; seed < 30; ++seed) {
			d_srand(seed);
			xl_boss_teleport(owners[0]);
			require(Objects[owners[0]].segnum == 70, "restored AI teleported outside earned destinations");
		}
		std::cout << "XL boss transport controls passed: authored damage/master actions, owner clocks, earned routes, disk continuation and stock isolation\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "XL boss transport failed: " << error.what() << '\n';
		return 1;
	}
}
