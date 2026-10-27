#include "boss_spawn.h"
extern "C" {
#include "boss_health_shared.h"
int boss_spew_robot(object *object, vms_vector *position);
}

static void write_boss_fixture(const std::filesystem::path &root)
{
	write_object_event_fixture(root);
	int boss_type = -1;
	for (int id = 0; id < N_robot_types; ++id)
		if (Robot_info[id].boss_flag == BOSS_D2) {
			boss_type = id;
			break;
		}
	require(boss_type >= 0, "retail boss definition missing");
	std::ifstream original(root / "events.hog", std::ios::binary);
	xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(original), std::istreambuf_iterator<char>() }, output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(bytes)) {
		if (member.name == "neptunxl.rl2") {
			auto level = xl_compat::inspect_level(member.bytes);
			const auto patch = [&](std::size_t offset, std::uint32_t value, int width = 4) {
				for (int i = 0; i < width; ++i) member.bytes.at(offset + i) = static_cast<unsigned char>(value >> (i * 8));
			};
			const auto append = [&](std::uint32_t value, int width) {
				for (int i = 0; i < width; ++i) member.bytes.push_back(static_cast<unsigned char>(value >> (i * 8)));
			};
			const auto relocate = [&](std::size_t offset, int delta) {
				for (std::size_t i = 0; i < level.sections.size(); ++i)
					if (level.sections[i].offset >= static_cast<int>(offset)) patch(level.game_offset + 35 + i * 12, level.sections[i].offset + delta);
			};
			const int owner = static_cast<int>(level.objects.size()) - 1;
			const auto &object = level.objects.back();
			require(object.type == OBJ_ROBOT, "boss fixture template robot missing");
			patch(object.source_offset + 1, boss_type, 1);
			patch(object.source_offset + 57, Polygon_models[Robot_info[boss_type].model_num].rad);
			patch(object.source_offset + 61, Robot_info[boss_type].strength);
			patch(object.source_end - 72, Robot_info[boss_type].model_num);
			xl_compat::Bytes clone(member.bytes.begin() + object.source_offset, member.bytes.begin() + object.source_end);
			member.bytes.insert(member.bytes.begin() + object.source_end, clone.begin(), clone.end());
			relocate(object.source_end, static_cast<int>(clone.size()));
			patch(level.game_offset + 39, owner + 2);
			patch(object.source_end + 7, 24, 2);
			for (int component = 0; component < 3; ++component) {
				std::int64_t center = 0;
				for (int vertex : level.segments[24].vertices) center += level.vertices[vertex][component];
				patch(object.source_end + 9 + component * 4, static_cast<int>(center / 8));
				patch(object.source_end + 65 + component * 4, static_cast<int>(center / 8));
			}
			level = xl_compat::inspect_level(member.bytes);
			const auto &section = level.sections[3];
			const auto offset = section.offset + level.triggers.size() * 52;
			const std::size_t payload = offset + 4;
			member.bytes.erase(member.bytes.begin() + payload, member.bytes.begin() + payload + 10 * 55);
			member.bytes.insert(member.bytes.begin() + payload, 2 * 55, 0);
			relocate(payload + 10 * 55, -8 * 55);
			patch(offset, 2);
			for (int i = 0; i < 2; ++i) {
				const auto action = payload + i * 53;
				patch(action, 26, 1);
				patch(action + 1, 8, 2);
				patch(action + 3, i == 0 ? 1 : 0, 1);
				patch(action + 5, 5 * F1_0);
				patch(action + 9, -1);
				patch(action + 13, 22, 2);
				patch(action + 33, -1, 2);
				patch(payload + 106 + i * 2, owner + i, 2);
			}
			level = xl_compat::inspect_level(member.bytes);
			const auto properties = level.game_offset - (level.vertices.size() + level.segments.size() * 6 + 910 + level.segments.size()) * 13;
			patch(properties + 22 * 13, SEGMENT_IS_ROBOTMAKER, 1);
			patch(properties + 22 * 13 + 1, 0, 1);
			patch(level.game_offset + 35 + 6 * 12, static_cast<unsigned>(member.bytes.size()));
			patch(level.game_offset + 39 + 6 * 12, 1);
			patch(level.game_offset + 43 + 6 * 12, 20);
			append(1, 4);
			append(0, 4);
			append(F1_0, 4);
			append(F1_0, 4);
			append(-1, 2);
			append(-1, 2);
			// Enlarge the derived arena so real boss models leave room for placement
			// retries. This modified geometry never qualifies original Neptune
			const auto arena = xl_compat::inspect_level(member.bytes);
			for (std::size_t i = 0; i < arena.vertices.size(); ++i)
				for (int component = 0; component < 3; ++component)
					patch(arena.mine_offset + 5 + i * 12 + component * 4, arena.vertices[i][component] * 8);
			for (const auto &actor : arena.objects)
				for (int component = 0; component < 3; ++component) {
					patch(actor.source_offset + 9 + component * 4, actor.position[component] * 8);
					patch(actor.source_offset + 65 + component * 4, actor.last_position[component] * 8);
				}
			const auto final = xl_compat::inspect_level(member.bytes);
			const auto blockers = xl_compat::runtime_blockers(final);
			require(blockers.empty(), xl_compat::blocker_message(blockers).c_str());
			write_object_fixture_json(root / "boss-fixture.json", { { "owners", { owner, owner + 1 } }, { "boss_type", boss_type }, { "hotshot_health", double(boss_health_maximum_for_difficulty(Robot_info[boss_type].strength, 2)) / F1_0 } });
		}
		char name[13] = {};
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (int i = 0; i < 4; ++i) output.push_back(static_cast<unsigned char>(member.bytes.size() >> (i * 8)));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "bosses.hog").string().c_str(), output);
	std::ofstream descriptor(root / "bosses.mn2");
	descriptor << "d2x-name = Derived XL boss spawning\nnum_levels = 1\nneptunxl.rl2\n";
}

static int boss_spawn_test(int argc, char **argv)
{
	try {
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
		write_boss_fixture(root);
		std::ifstream file(root / "boss-fixture.json");
		const auto fixture = nlohmann::json::parse(file);
		const auto owners = fixture.at("owners").get<std::vector<int>>();
		require(load_base_mission("bosses", error, sizeof(error)), error);
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_level_extensions_active() && xl_boss_spawn_count() == 2, "two authored bosses did not load");
		const auto state = [](int index) { xl_boss_spawn_state result; require(xl_boss_spawn_state_get(index, &result), "missing boss state"); return result; };
		const auto spawned = [] {
			std::vector<int> result;
			for (int i = 0; i <= Highest_object_index; ++i)
				if (Objects[i].type == OBJ_ROBOT && !Robot_info[Objects[i].id].boss_flag && Objects[i].matcen_creator == BOSS_GATE_MATCEN_NUM) result.push_back(i);
			return result;
		};
		GameTime64 = i2f(20);
		FrameTime = F1_0 / 60;
		const auto stock_time = Last_gate_time;
		for (int owner : owners) {
			require(xl_object_trigger_has_spawn(owner), "spawn action binding missing");
			const auto calls = d_rand_get_call_count();
			require(boss_spew_robot(&Objects[owner], &Objects[owner].pos) == -1 && calls == d_rand_get_call_count(), "ordinary boss spew was not suppressed before selection");
			Objects[owner].shields = Robot_info[Objects[owner].id].strength / 4;
			require(!apply_damage_to_robot(&Objects[owner], 1, ConsoleObject - Objects), "nonlethal boss damage killed owner");
		}
		if (spawned().size() != 2 || state(0).elapsed != 0 || state(1).elapsed != 0 || stock_time != Last_gate_time)
			throw std::runtime_error("owner-specific spawn timers or stock isolation failed: robots=" + std::to_string(spawned().size()) + " elapsed=" + std::to_string(state(0).elapsed) + "/" + std::to_string(state(1).elapsed) + " stock=" + std::to_string(Last_gate_time - stock_time));
		for (int index : spawned()) {
			const auto &object = Objects[index];
			require(object.control_type == CT_MORPH && object.lifeleft == i2f(30) + (F1_0 / 2) * Difficulty_level * 15 && object.shields == Robot_info[object.id].strength, "spawn lifetime, morph or shields differ");
			require(object.segnum == Objects[owners[0]].segnum || object.segnum == Objects[owners[1]].segnum, "robot spawned at selection matcen instead of owner");
		}
		for (int owner : owners) apply_damage_to_robot(&Objects[owner], 1, ConsoleObject - Objects);
		require(spawned().size() == 2, "cooldown permitted immediate repeat spawning");
		char snapshot[] = "xl-bosses.sav", description[] = "XL boss spawning";
		stop_time();
		require(state_save_all_sub(snapshot, description), "boss spawn disk capture failed");
		start_time();
		const auto original = state(0);
		GameTime64 += i2f(2);
		xl_boss_spawn_allow_second(owners[0]);
		require(state(0).elapsed > state(0).interval && state(1).elapsed == i2f(2), "boss force gate changed another owner");
		require(state_restore_all_sub(snapshot, 0) && state(0).elapsed == original.elapsed && state(1).elapsed == original.elapsed && spawned().size() == 2, "boss disk restore lost timers or robots");
		xl_boss_spawn_state restored[2] = { state(0), state(1) };
		for (int field = 0; field < 6; ++field) {
			auto bad = restored[0];
			if (field == 0) bad.owner = owners[1];
			if (field == 1) bad.signature++;
			if (field == 2) bad.id++;
			if (field == 3) bad.interval++;
			if (field == 4) bad.elapsed = -1;
			if (field == 5) bad.elapsed = INT64_MAX;
			xl_boss_spawn_state corrupt[2] = { bad, restored[1] };
			require(!xl_boss_spawn_restore(corrupt, 2, 1) && state(0).elapsed == restored[0].elapsed, "malformed boss state accepted or partially published");
		}
		Difficulty_level = (Difficulty_level + 1) % NDL;
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0 && !xl_boss_spawn_count(), "boss state leaked into stock mission");
		require(state_restore_all_sub(snapshot, 0) && xl_boss_spawn_count() == 2 && state(0).elapsed == original.elapsed, "stock-to-boss save restore failed");
		std::cout << "XL boss spawn controls passed: damage dispatch, two owners, linked/arbitrary selection, morph/lifetime, cooldown, disk restore and stock isolation\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "XL boss spawn controls failed: " << error.what() << '\n';
		return 1;
	}
}
