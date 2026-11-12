#include "boss_spawn.h"
#include "boss_routes.h"
extern "C" {
#include "boss_health_shared.h"
#include "morph.h"
int boss_spew_robot(object *object, vms_vector *position);
void do_boss_stuff(object *object, int visible);
void teleport_boss(object *object);
int create_gated_robot(int segment, int type, vms_vector *position);
int do_boss_weapon_collision(object *robot, object *weapon, vms_vector *position);
extern fix Boss_invulnerable_dot;
}

static void boss_vulnerability_controls(int owner)
{
	const int difficulty = Difficulty_level;
	auto &boss = Objects[owner];
	const auto orientation = boss.orient;
	const auto role = Robot_info[boss.id].boss_flag;
	boss.orient = vmd_identity_matrix;
	Robot_info[boss.id].boss_flag = BOSS_D2 + 7;
	const int shot = obj_create(OBJ_WEAPON, VULCAN_ID, boss.segnum, &boss.pos, &boss.orient, F1_0, CT_WEAPON, MT_NONE, RT_NONE);
	require(shot >= 0, "Cannot create boss vulnerability collision control");
	Objects[shot].ctype.laser_info.parent_type = OBJ_PLAYER;
	for (int level = 0; level < NDL; ++level) {
		Difficulty_level = level;
		init_robots_for_level();
		for (int side : { -1, 1 }) {
			auto hit = boss.pos;
			hit.x += F1_0;
			hit.z += side * (F1_0 / 16);
			const bool allowed = side > 0 ? level < 2 : level < 3;
			require(bool(do_boss_weapon_collision(&boss, &Objects[shot], &hit)) == allowed,
			        "XL boss side-hit vulnerability does not follow difficulty");
		}
	}
	obj_delete(shot);
	Robot_info[boss.id].boss_flag = role;
	boss.orient = orientation;
	char save[] = "boss-vulnerability.sav", description[] = "Boss vulnerability";
	stop_time();
	require(state_save_all_sub(save, description), "Boss vulnerability save failed");
	Difficulty_level = 0;
	init_robots_for_level();
	require(state_restore_all_sub(save, 0) && Difficulty_level == 4 && Boss_invulnerable_dot == -F1_0 / 4,
	        "Boss vulnerability difficulty was not restored from disk");
	Difficulty_level = difficulty;
	init_robots_for_level();
	std::cout << "XL boss vulnerability controls passed: ten side-hit decisions across five difficulties and disk restore\n";
}

static void boss_lifecycle_controls(const std::vector<int> &owners)
{
	// This derived control isolates the two bosses; mixed objectives run below
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type == OBJ_CNTRLCEN) obj_delete(i);
	const auto state = [](int index) { xl_boss_spawn_state result; require(xl_boss_spawn_state_get(index, &result), "missing lifecycle boss"); return result; };
	const int first = owners[0], second = owners[1];
	const auto stock_start = Boss_cloak_start_time, stock_end = Boss_cloak_end_time, stock_hit = Boss_hit_time, stock_teleport = Last_teleport_time;
	const auto second_position = Objects[second].pos;
	GameTime64 += i2f(40);
	const fix saved_health = Objects[first].shields;
	Objects[first].shields = boss_health_maximum_for_difficulty(Robot_info[Objects[first].id].strength, Difficulty_level) / 100;
	const auto calls = d_rand_get_call_count();
	do_boss_stuff(&Objects[first], 1);
	require(!Objects[first].ctype.ai_info.CLOAKED && d_rand_get_call_count() == calls, "boss one-percent float/double boundary consumed RNG or cloaked");
	Objects[first].shields = saved_health;
	do_boss_stuff(&Objects[first], 1);
	require(Objects[first].ctype.ai_info.CLOAKED && !Objects[second].ctype.ai_info.CLOAKED && state(0).cloak_start == 0 && state(0).cloak_end == BOSS_CLOAK_DURATION, "cloak activation coupled boss owners");
	fix64 start, end;
	require(xl_boss_cloak_times(first, &start, &end) && start == GameTime64 && end == GameTime64 + BOSS_CLOAK_DURATION, "renderer cloak times are not owner-specific");
	GameTime64 += i2f(3);
	do_boss_stuff(&Objects[first], 1);
	require(state(0).teleport == 0 && !std::memcmp(&Objects[second].pos, &second_position, sizeof(second_position)), "boss teleport failed or moved a different owner");
	require(stock_start == Boss_cloak_start_time && stock_end == Boss_cloak_end_time && stock_hit == Boss_hit_time && stock_teleport == Last_teleport_time, "XL cloak/teleport mutated stock globals");
	char live[] = "boss-life.sav", dying[] = "boss-dying.sav", description[] = "Boss lifecycle";
	stop_time();
	require(state_save_all_sub(live, description), "boss cloak/teleport save failed");
	require(!game_is_time_paused(), "Native save/restore left the simulation paused");
	const auto saved = state(0);
	GameTime64 += i2f(20);
	do_boss_stuff(&Objects[first], 1);
	require(!Objects[first].ctype.ai_info.CLOAKED, "boss cloak did not expire");
	require(state_restore_all_sub(live, 0) && state(0).cloak_start == saved.cloak_start && state(0).cloak_end == saved.cloak_end && state(0).teleport == saved.teleport && Objects[first].ctype.ai_info.CLOAKED, "boss cloak/teleport state lost across disk restore");
	require(apply_damage_to_robot(&Objects[first], Objects[first].shields + F1_0, ConsoleObject - Objects), "lethal combat did not enter boss death");
	require(state(0).dying == 1 && xl_boss_is_dying(first) && !xl_boss_is_dying(second) && !Boss_dying && !Boss_dying_start_time, "boss death entered shared stock state");
	GameTime64 += i2f(2);
	do_ai_frame_all();
	stop_time();
	require(state_save_all_sub(dying, description), "boss rolling-death save failed");
	require(!game_is_time_paused(), "Native save/restore left the simulation paused");
	const auto rolling = state(0);
	GameTime64 += i2f(10);
	do_ai_frame_all();
	require(state(0).owner == -1 && state(0).dying == 2 && state(1).owner == second && !state(1).dying && !Control_center_destroyed, "first boss completion retired another owner or started the final countdown");
	require(state_restore_all_sub(dying, 0) && state(0).owner == first && state(0).dying == 1 && state(0).death_elapsed == rolling.death_elapsed && !state(1).dying, "rolling boss death did not restore independently");
	GameTime64 += BOSS_DEATH_DURATION;
	do_ai_frame_all();
	require(state(0).owner == -1 && state(1).owner == second && xl_object_trigger_has_spawn(second), "restored death consumed another boss's actions");
	require(!Control_center_destroyed && xl_level_objectives_remaining() == 1, "restored first boss death lost the remaining objective");
	require(apply_damage_to_robot(&Objects[second], Objects[second].shields + F1_0, ConsoleObject - Objects), "second boss did not enter death roll");
	require(!Control_center_destroyed && xl_level_objectives_remaining() == 1, "countdown started before the final boss death roll completed");
	GameTime64 += BOSS_DEATH_DURATION + 1;
	do_ai_frame_all();
	require(Control_center_destroyed && !xl_level_objectives_remaining(), "final boss death did not start the countdown");
	std::cout << "XL boss lifecycle passed: independent cloak/render times, teleport, lethal combat, rolling-death save/restore and owner retirement\n";
}

static void boss_reactor_controls(const std::vector<int> &owners, char *baseline, const std::filesystem::path &output)
{
	std::ofstream samples(output / "boss-reactor-samples.txt");
	const auto sample = [&](int reactor) { samples << reactor << ' ' << xl_level_objectives_remaining() << ' ' << Control_center_destroyed << '\n'; };
	for (int reactor_first = 0; reactor_first < 2; ++reactor_first) {
		require(state_restore_all_sub(baseline, 0), "reactor/boss baseline restore failed");
		int reactor = -1;
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_CNTRLCEN) reactor = i;
		require(reactor >= 0, "authored reactor missing from mixed objective fixture");
		require(Objects[reactor].type == OBJ_CNTRLCEN && Objects[reactor].render_type == RT_POLYOBJ && Control_center_present &&
		            Objects[reactor].shields > 0 && xl_level_objectives_remaining() == 3,
		        "XL reactor hidden or missing beside bosses");
		const auto destroy_reactor = [&] {
			apply_damage_to_controlcen(&Objects[reactor], Objects[reactor].shields + F1_0, ConsoleObject - Objects);
			sample(1);
			require(Objects[reactor].shields < 0 && Dead_controlcen_object_num == reactor, "reactor combat did not record destruction");
		};
		if (reactor_first) destroy_reactor();
		for (int owner : owners) {
			require(apply_damage_to_robot(&Objects[owner], Objects[owner].shields + F1_0, ConsoleObject - Objects), "mixed objective boss combat failed");
			require(!Control_center_destroyed, "mixed objectives started countdown before completion");
			GameTime64 += BOSS_DEATH_DURATION + 1;
			do_ai_frame_all();
			sample(0);
			if (owner == owners.front()) {
				require(!Control_center_destroyed && xl_level_objectives_remaining() == (reactor_first ? 1 : 2), "mixed objective accounting failed");
				char save[] = "boss-reactor.sav", description[] = "Mixed objectives";
				stop_time();
				require(state_save_all_sub(save, description) && state_restore_all_sub(save, 0), "mixed objective disk roundtrip failed");
				require(!Control_center_destroyed && xl_level_objectives_remaining() == (reactor_first ? 1 : 2), "mixed objective state lost across disk restore");
			}
		}
		if (!reactor_first) {
			require(!Control_center_destroyed && xl_level_objectives_remaining() == 1, "boss deaths skipped the live reactor");
			destroy_reactor();
		}
		require(Control_center_destroyed && !xl_level_objectives_remaining(), "mixed objectives did not start final countdown");
	}
	std::cout << "XL mixed objective controls passed: reactor retained, both destruction orders, no early countdown and disk restore\n";
}

static void boss_reference_samples(const std::filesystem::path &output, char *baseline, int owner)
{
	std::ofstream file(output / "boss-spawn-samples.txt");
	file << N_robot_types << '\n';
	for (int id = 0; id < N_robot_types; ++id) {
		const auto &robot = Robot_info[id];
		file << robot.model_num << ' ' << robot.strength << ' ' << robot.mass << ' ' << robot.drag << ' ' << Polygon_models[robot.model_num].rad << ' ' << int(robot.behavior) << ' ' << int(robot.boss_flag) << ' ' << int(robot.companion) << ' ' << robot.score_value << '\n';
	}
	const auto vector = [&](const vms_vector &v) { file << ' ' << v.x << ' ' << v.y << ' ' << v.z; };
	for (int difficulty = 0; difficulty < NDL; ++difficulty) {
		require(state_restore_all_sub(baseline, 0), "reference difficulty reset failed");
		const int baseline_level_robots = Players[Player_num].num_robots_level;
		const int baseline_total_robots = Players[Player_num].num_robots_total;
		for (int scenario = 0; scenario < 10; ++scenario) {
			for (int i = 0; i <= Highest_object_index; ++i) {
				if (Objects[i].type == OBJ_ROBOT && !Robot_info[Objects[i].id].boss_flag) obj_delete(i);
				else if (Objects[i].type == OBJ_ROBOT) Objects[i].matcen_creator = 0;
			}
			Players[Player_num].num_robots_level = baseline_level_robots;
			Players[Player_num].num_robots_total = baseline_total_robots;
			Difficulty_level = difficulty;
			xl_boss_spawn_load();
			GameTime64 = i2f(20);
			FrameTime = scenario == 8 ? 0 : F1_0 / 60;
			Game_suspended = scenario == 7 ? SUSP_ROBOTS : 0;
			const int segment = Objects[owner].segnum;
			const auto add = [&](fix size, int where, int creator) {
				vms_vector position;
				compute_segment_center(&position, &Segments[where]);
				const int index = obj_create(OBJ_ROBOT, 0, where, &position, &vmd_identity_matrix, size, CT_NONE, MT_NONE, RT_NONE);
				if (index >= 0) Objects[index].matcen_creator = static_cast<sbyte>(creator);
				return index;
			};
			if (scenario == 3 || scenario == 4)
				for (int count = 0; count < 2 * difficulty + 6 + (scenario == 4); ++count) require(add(F1_0, 24, BOSS_GATE_MATCEN_NUM) >= 0, "cannot prepare population boundary");
			if (scenario == 5) require(add(i2f(2000), segment, 0) >= 0, "cannot prepare blocked placement");
			if (scenario == 6)
				while (add(F1_0, 24, 0) >= 0) {}
			const int requested = scenario == 2 ? 255 : scenario == 9 ? -1
			                                                          : 0;
			const int attached = scenario == 9 ? 0 : 1;
			if (!attached) xl_object_trigger_clear();
			xl_boss_spawn_state states[2];
			require(xl_boss_spawn_state_get(0, &states[0]) && xl_boss_spawn_state_get(1, &states[1]), "cannot sample boss state");
			states[0].elapsed = scenario == 0 ? states[0].interval - 1 : states[0].interval;
			require(xl_boss_spawn_restore(states, 2, 1), "cannot set reference cooldown boundary");
			int population = 0;
			std::vector<int> obstacles;
			for (int i = 0; i <= Highest_object_index; ++i) {
				const auto &object = Objects[i];
				if (object.type == OBJ_ROBOT && object.matcen_creator == BOSS_GATE_MATCEN_NUM) ++population;
				if (i != owner && object.segnum == segment && (object.type == OBJ_ROBOT || object.type == OBJ_PLAYER || object.type == OBJ_CNTRLCEN)) obstacles.push_back(i);
			}
			file << difficulty << ' ' << FrameTime << ' ' << states[0].elapsed << ' ' << states[0].interval << ' ' << requested << ' ' << attached << ' ' << population << ' ' << (num_objects < object_capacity()) << ' ' << Game_suspended << ' ' << int(Objects[owner].id) << ' ' << Objects[owner].size;
			vector(Objects[owner].pos);
			vms_vector center;
			compute_segment_center(&center, &Segments[segment]);
			vector(center);
			for (int vertex : Segments[segment].verts) vector(Vertices[vertex]);
			file << ' ' << obstacles.size();
			for (int i : obstacles) {
				file << ' ' << Objects[i].size;
				vector(Objects[i].pos);
			}
			const unsigned seed = 17u + difficulty * 107u + scenario * 31u;
			d_rand_reset_call_count();
			d_srand(seed);
			const int before = Players[Player_num].num_robots_level;
			const int created = xl_boss_spawn_robot(owner, requested, nullptr, attached);
			xl_boss_spawn_state after;
			require(xl_boss_spawn_state_get(0, &after), "missing sampled cooldown");
			const auto draws = d_rand_get_call_count();
			file << ' ' << (created >= 0) << ' ' << after.elapsed << ' ' << Players[Player_num].num_robots_level - before;
			if (created >= 0) {
				const auto &object = Objects[created];
				file << ' ' << int(object.id) << ' ' << object.lifeleft << ' ' << object.shields << ' ' << object.mtype.phys_info.mass << ' ' << object.mtype.phys_info.drag << ' ' << int(object.ctype.ai_info.SKIP_AI_COUNT);
				vector(object.pos);
				vector(object.mtype.phys_info.velocity);
				vector(object.mtype.phys_info.rotthrust);
			}
			file << ' ' << draws;
			unsigned final_state, replayed;
			d_rand_get_state(&final_state);
			d_srand(seed);
			for (std::uint64_t i = 0; i < draws; ++i) file << ' ' << d_rand();
			d_rand_get_state(&replayed);
			require(final_state == replayed, "boss sample RNG transcript differs");
			file << '\n';
		}
	}
	file.close();
	require(bool(file), "cannot write boss reference samples");
	require(state_restore_all_sub(baseline, 0), "cannot restore after boss reference samples");
}

static void boss_restore_guards(const std::filesystem::path &root)
{
	auto *file = PHYSFS_openWrite("boss-state.bin");
	require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "cannot capture boss extension guard");
	const auto user = root / "preview-user";
	std::ifstream input(user / "boss-state.bin", std::ios::binary);
	const xl_compat::Bytes encoded{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	require(encoded.size() > 16, "boss extension payload missing");
	xl_boss_spawn_state original;
	require(xl_boss_spawn_state_get(0, &original), "boss guard owner absent");
	xl_trigger_state wall;
	xl_object_trigger_state object;
	require(xl_wall_trigger_state(0, &wall) && !wall.operated && xl_object_trigger_state_get(0, &object) && object.operated, "unexpected live event state for guards");
	for (int variant = 0; variant < 26; ++variant) {
		auto bad = encoded;
		if (variant == 0) bad.clear();
		else if (variant == 1) bad[12] ^= 1;
		else {
			auto block = nlohmann::json::parse(encoded.begin() + 16, encoded.end());
			block["states"][0] = { 0, -1, ConsoleObject - Objects, 0, 1, 0 };
			block["objects"][0][5] = 0;
			auto &row = block["bosses"][0];
			if (variant == 2) block["bosses"] = nlohmann::json::array();
			if (variant == 3) row[6] = original.owner + 1;
			if (variant == 4) row[7] = original.signature + 1;
			if (variant == 5) row[8] = original.id + 1;
			if (variant == 6) row[9] = original.interval + 1;
			if (variant == 7) row[0] = -1;
			if (variant == 8) row[0] = UINT64_MAX;
			if (variant == 9) row[1] = INT64_MAX;
			if (variant == 10) row[2] = INT64_MIN;
			if (variant == 11) row[5] = -1;
			if (variant == 12) row[10] = 3;
			if (variant == 13) row[11] = 2;
			if (variant == 14) row[11] = 1;
			if (variant == 15) row[10] = 2;
			if (variant == 16) block["producer_implicit"] = 1;
			if (variant == 17) block["producer_implicit"] = UINT64_C(1) << 33;
			if (variant == 18) row[3] = 0.5;
			if (variant == 19) row.push_back(0);
			if (variant == 20) block.erase("bosses");
			if (variant == 21) block["schema"] = 2;
			if (variant == 22) block["source"] = block["source"].get<std::uint64_t>() ^ 1;
			if (variant == 23) row[2] = row[1].get<std::int64_t>() + 1;
			if (variant == 24) row[10] = 1;
			if (variant == 25) row[6] = -1;
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
		write_bytes((user / "boss-corrupt.bin").string().c_str(), bad);
		file = PHYSFS_openRead("boss-corrupt.bin");
		require(file && !xl_wall_trigger_read(file) && PHYSFS_close(file), "malformed boss extension accepted");
		xl_boss_spawn_state after;
		require(xl_boss_spawn_state_get(0, &after) && after.owner == original.owner && after.elapsed == original.elapsed && xl_wall_trigger_state(0, &wall) && !wall.operated && xl_object_trigger_state_get(0, &object) && object.operated, "rejected boss state partially published event state");
	}
	file = PHYSFS_openRead("boss-state.bin");
	require(file && xl_wall_trigger_read(file) && PHYSFS_close(file), "valid boss extension rejected after corrupt controls");
	PHYSFS_delete("boss-state.bin");
	PHYSFS_delete("boss-corrupt.bin");
	std::cout << "XL boss restore guards passed: 26 malformed extensions reject before publishing wall/object/boss state\n";
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
	xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(original), std::istreambuf_iterator<char>() }, output{ 'D', 'H', 'F' }, maker_output{ 'D', 'H', 'F' };
	std::vector<xl_compat::Member> invalid;
	for (auto member : xl_compat::read_hog(bytes)) {
		xl_compat::Bytes maker_level;
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
				patch(action + 3, 1, 1);
				patch(action + 5, 5 * F1_0);
				patch(action + 9, -1);
				patch(action + 13, i == 0 ? 22 : -1, 2);
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
			require(boss_type < 64 && final.sections[6].count > 0, "robotmaker boss fixture needs a low-bit boss and authored maker");
			maker_level = member.bytes;
			for (int byte = 0; byte < 8; ++byte) maker_level.at(final.sections[6].offset + byte) = 0;
			maker_level.at(final.sections[6].offset + boss_type / 8) = static_cast<unsigned char>(1u << (boss_type % 8));
			invalid.push_back({ "badowner.rl2", member.bytes });
			invalid.back().bytes.at(final.objects[owner].source_offset + 1) = 0;
			invalid.push_back({ "badmaker.rl2", member.bytes });
			const auto target = final.sections[3].offset + final.triggers.size() * 52 + 4 + 13;
			invalid.back().bytes.at(target) = invalid.back().bytes.at(target + 1) = 0;
			write_object_fixture_json(root / "boss-fixture.json", { { "owners", { owner, owner + 1 } }, { "boss_type", boss_type }, { "hotshot_health", double(boss_health_maximum_for_difficulty(Robot_info[boss_type].strength, 2)) / F1_0 } });
		}
		char name[13] = {};
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (int i = 0; i < 4; ++i) output.push_back(static_cast<unsigned char>(member.bytes.size() >> (i * 8)));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
		maker_output.insert(maker_output.end(), name, name + sizeof(name));
		const auto &maker_bytes = maker_level.empty() ? member.bytes : maker_level;
		for (int i = 0; i < 4; ++i) maker_output.push_back(static_cast<unsigned char>(maker_bytes.size() >> (i * 8)));
		maker_output.insert(maker_output.end(), maker_bytes.begin(), maker_bytes.end());
	}
	for (const auto &member : invalid) {
		char name[13] = {};
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (int i = 0; i < 4; ++i) output.push_back(static_cast<unsigned char>(member.bytes.size() >> (i * 8)));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "bosses.hog").string().c_str(), output);
	write_bytes((root / "bossmk.hog").string().c_str(), maker_output);
	std::ofstream descriptor(root / "bosses.mn2");
	descriptor << "d2x-name = Derived XL boss spawning\nnum_levels = 1\nneptunxl.rl2\n";
	std::ofstream maker_descriptor(root / "bossmk.mn2");
	maker_descriptor << "d2x-name = XL boss maker\nnum_levels = 1\nneptunxl.rl2\n";
}

static void robotmaker_boss_controls(const std::filesystem::path &root)
{
	write_boss_fixture(root);
	std::ifstream file(root / "boss-fixture.json");
	const auto fixture = nlohmann::json::parse(file);
	char error[256] = {};
	require(load_base_mission("bosses", error, sizeof(error)), error);
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	init_robots_for_level();
	GameTime64 += i2f(2);
	for (const int owner : fixture.at("owners").get<std::vector<int>>())
		Objects[owner].shields = Robot_info[Objects[owner].id].strength / 4;
	char baseline[] = "boss-reference-base.sav", description[] = "Boss reference";
	stop_time();
	require(state_save_all_sub(baseline, description), "boss reference baseline save failed");
	boss_reactor_controls(fixture.at("owners").get<std::vector<int>>(), baseline, root.parent_path());
	boss_reference_samples(root.parent_path(), baseline, fixture.at("owners")[0].get<int>());
	require(load_base_mission("bossmk", error, sizeof(error)), error);
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	{
		const int type = fixture.at("boss_type").get<int>();
		const int center = 0, station = matcen_get_station_num(center);
		require(type < 64 && station >= 0 && xl_boss_authored_count() == 2 &&
		            std::uint32_t(RobotCenters[center].robot_flags[type / 32]) == std::uint32_t(1) << (type % 32),
		        "robotmaker boss control needs an authored low-bit boss and valid maker");
		auto &maker = Station[station];
		maker.Enabled = 1;
		maker.Flag = 1;
		maker.Capacity = i2f(10);
		maker.Timer = Vclip[VCLIP_MORPHING_ROBOT].play_time;
		const auto identity = xl_level_trigger_identity();
		robotmaker_proc(&maker);
		int generated = -1;
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_ROBOT && Objects[i].id == type &&
			    static_cast<unsigned char>(Objects[i].matcen_creator) == (station | 0x80)) generated = i;
		require(generated >= 0 && xl_boss_spawn_count() == 3 && xl_boss_authored_count() == 2 &&
		            xl_level_trigger_identity() == identity && xl_compat::boss_routes().size() == 3,
		        "live robotmaker boss did not acquire independent source-stable lifecycle and routes");
		auto *trailer = PHYSFS_openWrite("boss-generated.bin");
		require(trailer && xl_wall_trigger_write(trailer) && PHYSFS_close(trailer), "robotmaker boss trailer capture failed");
		std::ifstream trailer_input(root.parent_path() / "preview-user" / "boss-generated.bin", std::ios::binary);
		const xl_compat::Bytes encoded{ std::istreambuf_iterator<char>(trailer_input), std::istreambuf_iterator<char>() };
		require(encoded.size() > 16, "robotmaker boss trailer missing");
		auto bad = nlohmann::json::parse(encoded.begin() + 16, encoded.end());
		bad["schema"] = 40;
		const auto payload = bad.dump();
		xl_compat::Bytes corrupt(encoded.begin(), encoded.begin() + 16);
		corrupt.insert(corrupt.end(), payload.begin(), payload.end());
		std::uint32_t checksum = 2166136261u;
		for (unsigned char byte : payload) checksum = (checksum ^ byte) * 16777619u;
		for (int byte = 0; byte < 4; ++byte) {
			corrupt[8 + byte] = static_cast<unsigned char>(payload.size() >> (byte * 8));
			corrupt[12 + byte] = static_cast<unsigned char>(checksum >> (byte * 8));
		}
		write_bytes((root.parent_path() / "preview-user" / "boss-generated-bad.bin").string().c_str(), corrupt);
		xl_boss_spawn_load();
		require(xl_boss_spawn_count() == 2, "robotmaker boss reset did not retain authored owners");
		trailer = PHYSFS_openRead("boss-generated-bad.bin");
		require(trailer && !xl_wall_trigger_read(trailer) && PHYSFS_close(trailer) && xl_boss_spawn_count() == 2,
		        "invalid robotmaker boss trailer published generated state");
		trailer = PHYSFS_openRead("boss-generated.bin");
		require(trailer && xl_wall_trigger_read(trailer) && PHYSFS_close(trailer) && xl_boss_spawn_count() == 3,
		        "valid robotmaker boss trailer did not rebind after rejection");
		PHYSFS_delete("boss-generated.bin");
		PHYSFS_delete("boss-generated-bad.bin");
		char generated_save[] = "boss-generated.sav", description[] = "Robotmaker boss";
		stop_time();
		require(state_save_all_sub(generated_save, description), "robotmaker boss disk save failed");
		require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_boss_spawn_count(),
		        "robotmaker boss leaked into stock mission");
		require(state_restore_all_sub(generated_save, 0) && xl_boss_spawn_count() == 3 &&
		            xl_boss_authored_count() == 2 && xl_level_trigger_identity() == identity &&
		            Objects[generated].type == OBJ_ROBOT && Objects[generated].id == type,
		        "cold robotmaker boss restore lost the generated owner or source identity");
		bool rebound = false;
		for (int i = 0; i < xl_boss_spawn_count(); ++i) {
			xl_boss_spawn_state boss{};
			require(xl_boss_spawn_state_get(i, &boss), "generated boss state missing");
			rebound |= boss.owner == generated && boss.signature == Objects[generated].signature && boss.id == type;
		}
		require(rebound, "cold robotmaker boss route/state did not bind its saved signature");
		obj_delete(generated);
		require(xl_boss_spawn_count() == 2 && xl_boss_authored_count() == 2 && xl_compat::boss_routes().size() == 2,
		        "deleted robotmaker boss retained its owner or routes");
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0 && !xl_boss_spawn_count(),
		        "robotmaker boss did not retire on stock transition");
		std::cout << "XL robotmaker boss lifecycle passed: native creation, bounded owner/routes, source identity and cold disk restore\n";
	}
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
		boss_vulnerability_controls(owners[0]);
		const auto identity = xl_level_trigger_identity();
		require(!xl_level_supported("badowner.rl2", 0) && std::strstr(xl_level_rejection(), "robot boss owner"), "nonboss spawn owner admitted or missing useful rejection");
		require(!xl_level_supported("badmaker.rl2", 0) && std::strstr(xl_level_rejection(), "robotmaker segment"), "invalid spawn selection target admitted or missing useful rejection");
		require(xl_boss_spawn_count() == 2 && xl_level_trigger_identity() == identity && xl_level_supported("neptunxl.rl2", 0), "failed admission replaced live boss state or prevented recovery");
		const auto state = [](int index) { xl_boss_spawn_state result; require(xl_boss_spawn_state_get(index, &result), "missing boss state"); return result; };
		const auto spawned = [] {
			std::vector<int> result;
			for (int i = 0; i <= Highest_object_index; ++i)
				if (Objects[i].type == OBJ_ROBOT && !Robot_info[Objects[i].id].boss_flag && Objects[i].matcen_creator == BOSS_GATE_MATCEN_NUM) result.push_back(i);
			return result;
		};
		GameTime64 = i2f(20);
		FrameTime = F1_0 / 60;
		xl_object_trigger_state events[2];
		require(xl_object_trigger_state_get(0, &events[0]) && xl_object_trigger_state_get(1, &events[1]), "missing source spawn events");
		auto disabled = events[0];
		events[0].flags |= TF_DISABLED;
		require(xl_object_trigger_restore(events, 2, 1), "cannot prepare disabled spawn control");
		const auto disabled_calls = d_rand_get_call_count();
		require(boss_spew_robot(&Objects[owners[0]], &Objects[owners[0]].pos) == -1 && xl_object_trigger_has_spawn(owners[0]) && d_rand_get_call_count() == disabled_calls, "disabled spawn action failed to suppress ordinary boss spew");
		events[0] = disabled;
		require(xl_object_trigger_restore(events, 2, 1), "disabled spawn control did not reset");
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
		require(!game_is_time_paused(), "Native save/restore left the simulation paused");
		const auto original = state(0);
		GameTime64 += original.interval;
		d_srand(123);
		apply_damage_to_robot(&Objects[owners[0]], 1, ConsoleObject - Objects);
		require(spawned().size() == 3 && state(0).elapsed == 0 && state(1).elapsed == original.interval, "permanent damage event did not spawn again after its own cooldown");
		int combat_robot = -1;
		for (int index : spawned())
			if (Objects[index].id == 0) {
				combat_robot = index;
				break;
			}
		require(combat_robot >= 0, "linked boss robot missing for combat");
		GameTime64 += F1_0; // Morph frame calls alone do not advance the game clock
		FrameTime = F1_0 / 30;
		for (int frame = 0; frame < 300 && Objects[combat_robot].control_type == CT_MORPH; ++frame) do_morph_frame(&Objects[combat_robot]);
		require(Objects[combat_robot].control_type == CT_AI && apply_damage_to_robot(&Objects[combat_robot], Objects[combat_robot].shields + F1_0, ConsoleObject - Objects), "boss-created robot did not complete morphing and enter normal combat");
		require(state_restore_all_sub(snapshot, 0), "boss repeat/combat control reset failed");
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
		boss_lifecycle_controls(owners);
		require(state_restore_all_sub(snapshot, 0), "boss source reset after lifecycle controls failed");
		boss_restore_guards(root.parent_path());
		Difficulty_level = (Difficulty_level + 1) % NDL;
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0 && !xl_boss_spawn_count(), "boss state leaked into stock mission");
		init_robots_for_level();
		require(Boss_invulnerable_dot == 0, "XL boss vulnerability leaked into stock mission");
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_ROBOT) Objects[i].matcen_creator = 0;
		int empty = 0;
		while (empty < Num_segments && Segments[empty].objects >= 0) ++empty;
		require(empty < Num_segments, "no empty stock gate-control segment");
		vms_vector center;
		compute_segment_center(&center, &Segments[empty]);
		Gate_interval = i2f(6);
		Last_gate_time = GameTime64 - Gate_interval;
		const int stock_robot = create_gated_robot(empty, 0, &center);
		require(stock_robot >= 0 && Objects[stock_robot].lifeleft == i2f(30) && Last_gate_time == GameTime64, "XL spawning changed stock boss lifetime or shared timer behavior");
		require(state_restore_all_sub(snapshot, 0) && xl_boss_spawn_count() == 2 && state(0).elapsed == original.elapsed, "stock-to-boss save restore failed");
		std::cout << "XL boss spawn controls passed: damage dispatch, two owners, linked/arbitrary selection, morph/lifetime, cooldown, disk restore and stock isolation\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "XL boss spawn controls failed: " << error.what() << '\n';
		return 1;
	}
}
