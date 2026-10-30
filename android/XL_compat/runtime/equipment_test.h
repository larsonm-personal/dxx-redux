// Production-load equipment controls use poses and authored actions, never navigation
#include "equipment.h"
extern "C" {
#include "powerup.h"
#include "fuelcen.h"
void do_matcen(const trigger *action);
}

static xl_compat::Inventory write_equipment_fixture(const std::filesystem::path &root, int version = 23)
{
	std::ifstream original(root / "neptunxl.hog", std::ios::binary);
	xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(original), std::istreambuf_iterator<char>() }, output{ 'D', 'H', 'F' };
	xl_compat::Inventory result;
	for (auto member : xl_compat::read_hog(bytes)) {
		if (member.name == "neptunxl.rl2") {
			auto level = xl_compat::inspect_level(member.bytes);
			require(level.level_version == 23 && level.game_version == 40 && level.equipment.empty() && level.matcens.empty() && level.triggers.size() == 1 && !level.object_triggers, "unexpected equipment source layout");
			const auto patch = [&](std::size_t offset, std::uint32_t value, int width = 4) {
				for (int i = 0; i < width; ++i) member.bytes.at(offset + i) = static_cast<unsigned char>(value >> (i * 8));
			};
			const auto append = [&](std::uint32_t value, int width) {
				for (int i = 0; i < width; ++i) member.bytes.push_back(static_cast<unsigned char>(value >> (i * 8)));
			};
			const auto properties = level.game_offset - (level.vertices.size() + level.segments.size() * 6 + 910 + level.segments.size()) * 13;
			for (int i = 0; i < 3; ++i) {
				const int segment = std::array<int, 3>{ 20, 22, 23 }[i];
				require(level.segments[segment].function == 0 && !level.segments[segment].properties, "equipment fixture segment is not neutral");
				patch(properties + segment * 13, i == 1 ? 4 : 11, 1);
				patch(properties + segment * 13 + 1, i == 0 ? 1 : 0, 1);
			}
			std::array<int, 3> home{};
			for (int c = 0; c < 3; ++c) {
				std::int64_t sum = 0;
				for (int vertex : level.segments[0].vertices) sum += level.vertices[vertex][c];
				home[c] = static_cast<int>(sum / 8);
			}
			int reactor = -1;
			for (std::size_t i = 0; i < level.objects.size(); ++i) {
				const auto &object = level.objects[i];
				if (object.type == OBJ_CNTRLCEN) reactor = static_cast<int>(i);
				if (object.type != OBJ_POWERUP || (object.segment != 20 && object.segment != 23)) continue;
				patch(object.source_offset + 7, 0, 2);
				for (int c = 0; c < 3; ++c) {
					patch(object.source_offset + 9 + c * 4, home[c]);
					patch(object.source_offset + 65 + c * 4, home[c]);
				}
			}
			require(reactor >= 0, "equipment fixture reactor absent");
			const auto offset = level.sections[3].offset;
			patch(offset, TT_MATCEN, 1);
			patch(offset + 1, TF_NO_MESSAGE | 8, 1);
			patch(offset + 2, 3, 1);
			patch(offset + 8, -1);
			for (int i = 0; i < 3; ++i) {
				patch(offset + 12 + i * 2, std::array<int, 3>{ 20, 22, 23 }[i], 2);
				patch(offset + 32 + i * 2, 0, 2);
			}
			level = xl_compat::inspect_level(member.bytes);
			std::vector<int> targets;
			for (std::size_t i = 0; i < level.segments.size() && targets.size() < 2; ++i)
				if (level.segments[i].function == 1 || level.segments[i].function == 2 || level.segments[i].function == 4 || level.segments[i].function == 11) targets.push_back(static_cast<int>(i));
			for (int segment : { 20, 23 })
				if (std::find(targets.begin(), targets.end(), segment) == targets.end()) targets.push_back(segment);
			xl_compat::Bytes payload;
			const auto put = [&](std::uint32_t value, int width) {
				for (int i = 0; i < width; ++i) payload.push_back(static_cast<unsigned char>(value >> (i * 8)));
			};
			put(TT_MATCEN, 1);
			put(8, 2);
			put(static_cast<unsigned>(targets.size()), 1);
			put(0, 1);
			put(0, 4);
			put(-1, 4);
			for (int i = 0; i < 10; ++i) put(i < static_cast<int>(targets.size()) ? targets[i] : 0, 2);
			for (int i = 0; i < 10; ++i) put(0, 2);
			put(reactor, 2);
			const auto insertion = offset + 56;
			member.bytes.insert(member.bytes.begin() + insertion, payload.begin(), payload.end());
			for (std::size_t i = 0; i < level.sections.size(); ++i)
				if (level.sections[i].offset >= insertion) patch(level.game_offset + 35 + i * 12, level.sections[i].offset + static_cast<unsigned>(payload.size()));
			patch(offset + 52, 1);
			patch(level.game_offset + 35 + 6 * 12, static_cast<unsigned>(member.bytes.size()));
			patch(level.game_offset + 39 + 6 * 12, 1);
			patch(level.game_offset + 43 + 6 * 12, 20);
			for (int value : { 1, 0, F1_0, F1_0 }) append(value, 4);
			append(-1, 2);
			append(-1, 2);
			patch(level.game_offset + 35 + 9 * 12, static_cast<unsigned>(member.bytes.size()));
			patch(level.game_offset + 39 + 9 * 12, 2);
			patch(level.game_offset + 43 + 9 * 12, 20);
			for (int row = 0; row < 2; ++row) {
				append(1u << (row ? POW_VULCAN_AMMO : POW_ENERGY), 4);
				append(0, 4);
				append(F1_0 * (row + 1), 4);
				append(i2f(30), 4);
				append(-1, 2);
				append(32767, 2);
			}
			if (version != 23) member.bytes = xl_test::runtime_layout_fixture(member.bytes, version).bytes;
			result = xl_compat::inspect_level(member.bytes);
			const auto blockers = xl_compat::runtime_blockers(result);
			require(blockers.empty(), xl_compat::blocker_message(blockers).c_str());
		}
		char name[13] = {};
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (int shift = 0; shift < 32; shift += 8) output.push_back(static_cast<unsigned char>(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "equip.hog").string().c_str(), output);
	std::ofstream descriptor(root / "equip.mn2");
	descriptor << "d2x-name = Derived XL equipment\nnum_levels = 1\nneptunxl.rl2\n";
	return result;
}

static void equipment_reference_samples(const std::filesystem::path &output, const xl_compat::Inventory &authored, char *baseline)
{
	require(state_restore_all_sub(baseline, 0), "equipment sample baseline failed");
	std::array<int, MAX_OBJECTS> original_signatures;
	for (int i = 0; i < MAX_OBJECTS; ++i) original_signatures[i] = Objects[i].signature;
	const auto cleanup = [&] {
		for (int i = Highest_object_index; i > 0; --i)
			if (Objects[i].type != OBJ_NONE && Objects[i].signature != original_signatures[i]) obj_delete(i);
	};
	std::ofstream file(output / "equipment-samples.txt");
	file << Num_vclips << '\n';
	for (int i = 0; i < Num_vclips; ++i) file << Vclip[i].play_time << ' ' << Vclip[i].frame_time << ' ' << Vclip[i].sound_num << '\n';
	for (int i = 0; i < 50; ++i) file << Powerup_info[i].size << ' ' << Powerup_info[i].vclip_num << '\n';
	for (int difficulty = 0; difficulty < NDL; ++difficulty)
		for (int scenario = 0; scenario < 14; ++scenario)
			for (unsigned seed : { 1u, 17u, 65535u }) {
				cleanup();
				auto sample = authored;
				std::uint32_t first = 1u << POW_ENERGY, second = 0;
				if (scenario == 7 || scenario == 11) first = 1u << POW_VULCAN_AMMO;
				if (scenario == 8) {
					first |= 1u << POW_VULCAN_AMMO;
					second = 1u << (POW_EARTHSHAKER_MISSILE - 32);
				}
				if (scenario == 9) first = 0;
				sample.equipment[0][0] = static_cast<std::int32_t>(first);
				sample.equipment[0][1] = static_cast<std::int32_t>(second);
				xl_equipment_load(sample);
				Difficulty_level = difficulty;
				Game_suspended = scenario == 12 ? SUSP_ROBOTS : scenario == 13 ? 4
				                                                               : 0;
				FrameTime = F1_0 / 60;
				xl_equipment_state states[2];
				require(xl_equipment_state_get(0, &states[0]) && xl_equipment_state_get(1, &states[1]), "equipment sample missing state");
				states[0].enabled = scenario != 0;
				states[0].pending = (scenario >= 6 && scenario != 10);
				states[0].timer = (states[0].pending ? Vclip[VCLIP_MORPHING_ROBOT].play_time / 2 : i2f(3 * (difficulty + 1))) - FrameTime - (scenario == 1 || scenario == 6);
				states[1].enabled = 0;
				require(xl_equipment_restore(states, 2, 1), "equipment sample state rejected");
				if (scenario >= 3 && scenario <= 5) {
					vms_vector center;
					compute_segment_center(&center, &Segments[20]);
					const int type = scenario == 3 ? OBJ_POWERUP : scenario == 4 ? OBJ_LIGHT
					                                                             : OBJ_PLAYER;
					const int id = scenario == 4 ? OBJ_PLAYER : 0;
					require(obj_create(static_cast<object_type_t>(type), id, 20, &center, &vmd_identity_matrix, F1_0, CT_NONE, MT_NONE, RT_NONE) >= 0, "equipment blocker allocation failed");
				}
				if (scenario == 10 || scenario == 11) {
					vms_vector center;
					compute_segment_center(&center, &Segments[0]);
					int filled = 0;
					while (filled < MAX_OBJECTS && obj_create(OBJ_LIGHT, 0, 0, &center, &vmd_identity_matrix, F1_0, CT_NONE, MT_NONE, RT_NONE) >= 0) ++filled;
					require(filled > 0 && filled < MAX_OBJECTS, "equipment allocation exhaustion did not fill the pool");
				}
				std::vector<std::array<int, 2>> occupants;
				for (int i = Segments[20].objects; i >= 0; i = Objects[i].next) occupants.push_back({ Objects[i].type, Objects[i].id });
				file << difficulty << ' ' << scenario << ' ' << first << ' ' << second << ' ' << states[0].timer << ' ' << states[0].enabled << ' ' << states[0].pending << ' ' << FrameTime << ' ' << Game_suspended << ' ' << (scenario != 10) << ' ' << (scenario != 11) << ' ' << occupants.size();
				for (const auto &object : occupants) file << ' ' << object[0] << ' ' << object[1];
				d_srand(seed);
				d_rand_set_call_count(0);
				xl_equipment_frame(0, Num_segments);
				const auto draws = d_rand_get_call_count();
				unsigned final_state, replayed;
				d_rand_get_state(&final_state);
				file << ' ' << draws;
				d_srand(seed);
				for (std::uint64_t i = 0; i < draws; ++i) file << ' ' << d_rand();
				d_rand_get_state(&replayed);
				require(replayed == final_state, "equipment sample RNG transcript differs");
				xl_equipment_state after;
				require(xl_equipment_state_get(0, &after), "equipment sample lost state");
				int effects = 0, item = -1;
				for (int i = Segments[20].objects; i >= 0; i = Objects[i].next) {
					if (Objects[i].type == OBJ_FIREBALL && Objects[i].id == VCLIP_MORPHING_ROBOT) ++effects;
					if (Objects[i].type == OBJ_POWERUP && Objects[i].control_type == CT_POWERUP) item = i;
				}
				if (item >= 0) {
					const auto &object = Objects[item];
					require(object.lifeleft == IMMORTAL_TIME && object.matcen_creator == -1 && object.movement_type == MT_PHYSICS && object.render_type == RT_POWERUP && object.rtype.vclip_info.vclip_num == Powerup_info[object.id].vclip_num && object.rtype.vclip_info.frametime == Vclip[object.rtype.vclip_info.vclip_num].frame_time && object.rtype.vclip_info.framenum == 0 && object.size == Powerup_info[object.id].size && !object.mtype.phys_info.mass && !object.mtype.phys_info.drag && !object.mtype.phys_info.flags, "generated equipment properties differ");
				}
				file << ' ' << after.timer << ' ' << after.enabled << ' ' << after.pending << ' ' << effects << ' ' << (item >= 0 ? Objects[item].id : -1) << ' ' << (item >= 0 ? Objects[item].ctype.powerup_info.count : 0) << '\n';
			}
	cleanup();
	file.close();
	require(bool(file), "cannot write equipment reference samples");
	require(state_restore_all_sub(baseline, 0), "equipment sampler did not restore source");
}

static void equipment_restore_guards(const std::filesystem::path &output)
{
	auto *file = PHYSFS_openWrite("equipment-state.bin");
	require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "cannot capture equipment extension guard");
	std::ifstream input(output / "preview-user" / "equipment-state.bin", std::ios::binary);
	const xl_compat::Bytes encoded{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	require(encoded.size() > 16, "equipment extension missing payload");
	xl_equipment_state live[2];
	require(xl_equipment_state_get(0, &live[0]) && xl_equipment_state_get(1, &live[1]), "equipment guard state missing");
	live[0].enabled = 1;
	live[0].timer = 123;
	require(xl_equipment_restore(live, 2, 1), "cannot prepare live equipment guard");
	xl_object_trigger_state object;
	require(xl_object_trigger_state_get(0, &object), "equipment object control missing");
	object.operated = 1;
	require(xl_object_trigger_restore(&object, 1, 1), "cannot prepare live object guard");
	xl_trigger_state wall;
	require(xl_wall_trigger_state(0, &wall), "equipment wall control missing");
	for (int variant = 0; variant < 22; ++variant) {
		auto bad = encoded;
		if (variant == 0) bad.clear();
		else if (variant == 1) bad[12] ^= 1;
		else {
			auto block = nlohmann::json::parse(encoded.begin() + 16, encoded.end());
			block["states"][0] = { 0, -1, ConsoleObject - Objects, 0, 1, 0 };
			auto &row = block["equipment"][0];
			if (variant == 2) block["schema"] = 3;
			if (variant == 3) block["source"] = block["source"].get<std::uint64_t>() ^ 1;
			if (variant == 4) block.erase("equipment");
			if (variant == 5) block["equipment"] = nlohmann::json::array();
			if (variant == 6) row.erase(row.end() - 1);
			if (variant == 7) row[0] = -2;
			if (variant == 8) row[0] = INT64_MAX;
			if (variant == 9) row[0] = UINT64_MAX;
			if (variant == 10) row[0] = 0.5;
			if (variant == 11) row[1] = 23;
			if (variant == 12) row[2] = 2;
			if (variant == 13) row[3] = 2;
			if (variant == 14) block["unused"] = 0;
			if (variant == 15) row[1] = UINT64_MAX;
			if (variant == 16) row[3] = nlohmann::json::parse("true");
			if (variant == 21) row[3] = 1;
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
		write_bytes((output / "preview-user" / "equipment-corrupt.bin").string().c_str(), bad);
		const int count = Num_fuelcenters, segment = Station[0].segnum, type = Station[0].Type;
		if (variant == 17) Num_fuelcenters = 0;
		if (variant == 18) Station[0].segnum = -1;
		if (variant == 19) Station[0].Type = -1;
		if (variant == 20) std::swap(Station[0], Station[1]);
		file = PHYSFS_openRead("equipment-corrupt.bin");
		require(file && !xl_wall_trigger_read(file) && PHYSFS_close(file), "malformed equipment extension accepted");
		if (variant == 20) std::swap(Station[0], Station[1]);
		Num_fuelcenters = count;
		Station[0].segnum = segment;
		Station[0].Type = type;
		xl_equipment_state after;
		xl_object_trigger_state after_object;
		xl_trigger_state after_wall;
		require(xl_equipment_state_get(0, &after) && after.timer == 123 && after.enabled && xl_object_trigger_state_get(0, &after_object) && after_object.operated && xl_wall_trigger_state(0, &after_wall) && after_wall.operated == wall.operated && after_wall.object == wall.object, "rejected equipment state partially published event state");
	}
	file = PHYSFS_openRead("equipment-state.bin");
	require(file && xl_wall_trigger_read(file) && PHYSFS_close(file), "valid equipment extension rejected after corrupt controls");
	PHYSFS_delete("equipment-state.bin");
	PHYSFS_delete("equipment-corrupt.bin");
	std::cout << "XL equipment restore guards passed: 22 malformed extensions or producer links reject atomically\n";
}

static void equipment_initialization_controls(const std::filesystem::path &output, const xl_compat::Inventory &authored, char *baseline)
{
	require(NumTextures > 1 && Num_effects > 0 && authored.walls.size() >= 2, "equipment initialization assets missing");
	const auto texture = TmapInfo[1];
	const auto effect = Effects[0];
	const auto stations = xl_compat::producer_layout(authored).stations;
	int owner = -1;
	for (std::size_t i = 0; i < authored.objects.size(); ++i)
		if (authored.objects[i].type == OBJ_CNTRLCEN) owner = static_cast<int>(i);
	require(owner >= 0, "equipment initialization object owner missing");
	std::ofstream file(output / "equipment-initialization.txt");
	for (int scenario = 0; scenario < 13; ++scenario) {
		auto sample = authored;
		for (auto &wall : sample.walls) wall.trigger = -1;
		sample.object_trigger_records.clear();
		sample.triggers = { { TT_MATCEN, 0, 0, -1, {} } };
		const int target = scenario == 11 ? stations.at(1) : scenario == 12 ? 23
		                                                                    : stations.at(0);
		sample.triggers[0].targets = { { target, 0 } };
		auto &first = sample.walls[0], &second = sample.walls[1];
		sample.segments[first.segment].sides[first.side].overlay = 1;
		sample.segments[second.segment].sides[second.side].overlay = 1;
		TmapInfo[1].eclip_num = -1;
		TmapInfo[1].destroyed = 2;
		Effects[0].flags = 0;
		Effects[0].dest_bm_num = 2;
		if ((scenario >= 1 && scenario <= 7) || scenario == 12) first.trigger = 0;
		if (scenario == 2) TmapInfo[1].destroyed = -1;
		if (scenario >= 3 && scenario <= 5) TmapInfo[1].eclip_num = 0;
		if (scenario == 4) Effects[0].dest_bm_num = -1;
		if (scenario == 5) Effects[0].flags = EF_ONE_SHOT;
		if (scenario == 6 || scenario == 7) sample.segments[first.segment].sides[first.side].overlay = 0;
		if (scenario == 7) second.trigger = 0;
		if (scenario >= 8 && scenario <= 11) sample.object_trigger_records.push_back({ { TT_MATCEN, 0, 0, -1, { { target, 0 } } }, scenario == 8 ? -1 : owner });
		sample.object_triggers = static_cast<int>(sample.object_trigger_records.size());
		const auto saved_type = Objects[owner].type;
		if (scenario == 10) Objects[owner].type = OBJ_NONE;
		xl_equipment_load(sample);
		file << scenario << ' ' << stations.size() << " 2";
		for (int segment : stations) file << ' ' << segment;
		for (int row = 0; row < 2; ++row) {
			const int segment = row ? 23 : 20;
			file << ' ' << std::distance(stations.begin(), std::find(stations.begin(), stations.end(), segment));
		}
		file << ' ' << target << " 2";
		for (int wall = 0; wall < 2; ++wall) {
			const auto &entry = sample.walls[wall];
			file << ' ' << entry.trigger << ' ' << entry.segment << ' ' << entry.side << ' ' << sample.segments[entry.segment].sides[entry.side].overlay;
		}
		file << ' ' << TmapInfo[1].eclip_num << ' ' << TmapInfo[1].destroyed << ' ' << Effects[0].flags << ' ' << Effects[0].dest_bm_num;
		file << ' ' << sample.object_trigger_records.size() << ' ' << (scenario == 8 ? -1 : owner) << ' ' << int(Objects[owner].type);
		for (int row = 0; row < 2; ++row) {
			xl_equipment_state state;
			require(xl_equipment_state_get(row, &state), "initialization state missing");
			file << ' ' << state.enabled;
		}
		file << '\n';
		Objects[owner].type = saved_type;
	}
	TmapInfo[1] = texture;
	Effects[0] = effect;
	file.close();
	require(bool(file), "cannot write equipment initialization samples");
	require(state_restore_all_sub(baseline, 0), "initialization controls did not restore source");
}

static void equipment_mixed_order_controls(const xl_compat::Inventory &authored, char *baseline)
{
	for (unsigned seed : { 17u, 65535u }) {
		std::array<int, 6> expected;
		std::uint64_t expected_draws = 0;
		unsigned expected_rng = 0;
		for (int production = 0; production < 2; ++production) {
			require(state_restore_all_sub(baseline, 0), "mixed producer baseline failed");
			auto sample = authored;
			for (auto &row : sample.equipment) row[0] = (1 << POW_ENERGY) | (1 << POW_VULCAN_AMMO);
			xl_equipment_load(sample);
			xl_equipment_state states[2];
			for (int i = 0; i < 2; ++i) {
				require(xl_equipment_state_get(i, &states[i]), "mixed equipment state missing");
				states[i].enabled = states[i].pending = 1;
				states[i].timer = Vclip[VCLIP_MORPHING_ROBOT].play_time / 2;
			}
			require(xl_equipment_restore(states, 2, 1), "mixed equipment state rejected");
			RobotCenters[0].robot_flags[0] = 3;
			auto &center = Station[matcen_get_station_num(0)];
			center.Enabled = center.Flag = 1;
			center.Capacity = F1_0;
			center.Timer = Vclip[VCLIP_MORPHING_ROBOT].play_time / 2 + 1;
			center.Disable_time = i2f(26);
			Game_suspended = 0;
			FrameTime = 0;
			d_srand(seed);
			d_rand_set_call_count(0);
			if (production) fuelcen_update_all();
			else {
				// Pinned UpdateAllProducers visits all producer types in geometry order
				xl_equipment_frame(0, 22);
				robotmaker_proc(&center);
				xl_equipment_frame(23, Num_segments);
			}
			std::array<int, 6> result{ -1, -1, -1, -1, -1, -1 };
			for (int i = 0; i <= Highest_object_index; ++i) {
				const auto &object = Objects[i];
				const int row = object.type == OBJ_POWERUP && object.segnum == 20 ? 0 : object.type == OBJ_ROBOT && object.segnum == 22 ? 1
				                                                                    : object.type == OBJ_POWERUP && object.segnum == 23 ? 2
				                                                                                                                        : -1;
				if (row >= 0) {
					result[row * 2] = object.id;
					result[row * 2 + 1] = i;
				}
			}
			unsigned rng;
			d_rand_get_state(&rng);
			require(result[0] >= 0 && result[2] >= 0 && result[4] >= 0, "mixed producer sequence did not emit all actors");
			if (!production) {
				expected = result;
				expected_draws = d_rand_get_call_count();
				expected_rng = rng;
			} else require(result == expected && d_rand_get_call_count() == expected_draws && rng == expected_rng, "mixed equipment/robot update reordered selection RNG or allocation");
		}
	}
	require(state_restore_all_sub(baseline, 0), "mixed producer controls did not restore source");
	std::cout << "XL mixed producers passed: geometry order preserves selected actors, allocation slots and RNG\n";
}

static int equipment_test(int argc, char **argv)
{
	try {
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
		const auto layout_argument = find_arg_value(argc, argv, "-layout-version");
		const int version = layout_argument ? std::atoi(layout_argument) : 23;
		const auto authored = write_equipment_fixture(root, version);
		require(load_base_mission("equip", error, sizeof(error)), error);
		std::strcpy(Players[Player_num].callsign, "XLTest");
		Difficulty_level = 2;
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_source_level_version() == version, "Equipment test loaded the wrong source layout");
		require(xl_level_extensions_active() && xl_equipment_count() == 2 && Num_robot_centers == 1, "equipment fixture did not load");
		const auto state = [](int i) { xl_equipment_state state; require(xl_equipment_state_get(i, &state), "missing equipment state"); return state; };
		const auto items = [](int segment) {
			std::vector<int> found;
			for (int i = Segments[segment].objects; i >= 0; i = Objects[i].next)
				if (Objects[i].type == OBJ_POWERUP) found.push_back(i);
			return found;
		};
		require(state(0).segment == 20 && state(1).segment == 23 && !state(0).enabled && !state(1).enabled && state(0).timer == -1, "authored assignment or initial target discovery differs");
		require(Segment2s[20].special == SEGMENT_IS_NOTHING && Segment2s[23].special == SEGMENT_IS_NOTHING, "equipment function leaked into stock enums");
		obj_relink(ConsoleObject - Objects, 0);
		compute_segment_center(&ConsoleObject->pos, &Segments[0]);
		Game_suspended = SUSP_ROBOTS;
		char initial[] = "equip-initial.sav", pending[] = "equip-pending.sav", description[] = "XL equipment";
		stop_time();
		require(state_save_all_sub(initial, description), "equipment initial save failed");
		require(!game_is_time_paused(), "Native save/restore left the simulation paused");
		require(check_trigger_sub(0, Player_num, 1) == 0 && state(0).enabled && state(1).enabled && Station[matcen_get_station_num(0)].Enabled, "authored action did not activate mixed producers");
		FrameTime = i2f(9);
		fuelcen_update_all();
		require(!state(0).pending && state(0).timer == i2f(9) - 1, "equipment delay ignored initial minus-one tick");
		FrameTime = 1;
		fuelcen_update_all();
		require(state(0).pending && state(1).pending && state(0).timer == 0 && items(20).empty() && items(23).empty(), "equipment effect boundary or robot suspension differs");
		FrameTime = Vclip[VCLIP_MORPHING_ROBOT].play_time / 4;
		fuelcen_update_all();
		const auto captured = state(0);
		stop_time();
		require(state_save_all_sub(pending, description), "pending equipment save failed");
		require(!game_is_time_paused(), "Native save/restore left the simulation paused");
		require(xl_equipment_trigger(20) == 2, "equipment disable failed");
		FrameTime = i2f(20);
		fuelcen_update_all();
		require(state(0).timer == captured.timer && state(0).pending, "disabled equipment advanced or reset pending state");
		require(xl_equipment_trigger(20) == 1, "equipment re-enable failed");
		FrameTime = Vclip[VCLIP_MORPHING_ROBOT].play_time / 2 - captured.timer;
		fuelcen_update_all();
		require(items(20).size() == 1 && Objects[items(20)[0]].id == POW_ENERGY && items(23).size() == 1 && Objects[items(23)[0]].id == POW_VULCAN_AMMO, "source equipment contents swapped or missing");
		require(Objects[items(23)[0]].ctype.powerup_info.count == VULCAN_AMMO_AMOUNT && Objects[items(23)[0]].lifeleft == IMMORTAL_TIME && Objects[items(23)[0]].matcen_creator == -1, "generated item count, lifetime or provenance differs");
		FrameTime = i2f(20);
		fuelcen_update_all();
		require(state(0).timer == 0 && !state(0).pending && items(20).size() == 1, "occupied equipment did not restart its delay");
		Players[Player_num].energy = i2f(50);
		auto &energy = Objects[items(20)[0]];
		collide_player_and_powerup(ConsoleObject, &energy, &energy.pos);
		require(Players[Player_num].energy > i2f(50) && (energy.flags & OF_SHOULD_BE_DEAD), "generated energy did not use ordinary pickup rewards");
		obj_delete_all_that_should_be_dead();
		FrameTime = i2f(9);
		fuelcen_update_all();
		require(state(0).pending && !state(1).pending, "pickup did not release only the emptied equipment segment");
		FrameTime = Vclip[VCLIP_MORPHING_ROBOT].play_time / 2;
		fuelcen_update_all();
		require(items(20).size() == 1, "equipment did not replenish after pickup");
		require(state_restore_all_sub(pending, 0) && state(0).timer == captured.timer && state(0).pending && items(20).empty(), "pending equipment disk restore differs");
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0 && !xl_equipment_count() && !xl_equipment_trigger(20), "equipment state leaked into stock dispatch");
		Difficulty_level = 0;
		require(state_restore_all_sub(pending, 0) && xl_equipment_count() == 2 && state(0).pending && state(0).timer == captured.timer && Difficulty_level == 2, "stock-to-equipment restore lost pending state or difficulty");
		equipment_reference_samples(root.parent_path(), authored, initial);
		equipment_initialization_controls(root.parent_path(), authored, initial);
		equipment_mixed_order_controls(authored, initial);
		equipment_restore_guards(root.parent_path());
		std::cout << "XL equipment controls passed: assignment, authored activation, timing, toggles, production/pickup, pending restore and stock isolation\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "XL equipment controls failed: " << error.what() << '\n';
		return 1;
	}
}
