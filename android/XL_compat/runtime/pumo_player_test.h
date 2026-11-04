// Developer-only ordinary-player simulation; initial level and local poses are setup
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif
namespace pumo_player_probe
{
using Json = nlohmann::json;
static std::string file_hash(const std::filesystem::path &path)
{
	std::ifstream stream(path, std::ios::binary);
	const std::vector<unsigned char> bytes{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
	return picosha2::hash256_hex_string(bytes);
}
static Json snapshot()
{
	Json state = { { "current_level_num", Current_level_num }, { "player_dead", Player_is_dead != 0 }, { "cheats_enabled", cheats.enabled != 0 } };
	state["game_time_raw"] = GameTime64;
	const auto &player = Players[Player_num];
	if (Player_is_dead && player.killer_objnum >= 0 && player.killer_objnum <= Highest_object_index) {
		const auto &killer = Objects[player.killer_objnum];
		state["player_death_source"] = { { "object", player.killer_objnum }, { "type", killer.type }, { "id", killer.id }, { "segment", killer.segnum } };
	}
	state["player"] = { { "has_blue_key", bool(player.flags & PLAYER_FLAGS_BLUE_KEY) }, { "has_red_key", bool(player.flags & PLAYER_FLAGS_RED_KEY) }, { "has_gold_key", bool(player.flags & PLAYER_FLAGS_GOLD_KEY) }, { "invulnerable", bool(player.flags & PLAYER_FLAGS_INVULNERABLE) }, { "shields", double(player.shields) / F1_0 }, { "energy", double(player.energy) / F1_0 }, { "score", player.score }, { "lives", player.lives }, { "robots_killed_level", player.num_kills_level } };
	state["player"]["primary_weapon"] = player.primary_weapon;
	state["player"]["primary_weapon_flags"] = player.primary_weapon_flags;
	if (ConsoleObject) state["position"] = { { "segment", ConsoleObject->segnum }, { "x", double(ConsoleObject->pos.x) / F1_0 }, { "y", double(ConsoleObject->pos.y) / F1_0 }, { "z", double(ConsoleObject->pos.z) / F1_0 } };
	state["reactor_countdown"] = { { "active", Control_center_destroyed != 0 }, { "total_seconds", Total_countdown_time }, { "timer", Countdown_timer } };
	auto &pilot = state["xl_pilot"];
	pilot["remaining_objectives"] = xl_level_objectives_remaining();
	pilot["boost_player_locked"] = xl_speed_boost_player_locked() != 0;
	pilot["runtime_active"] = xl_level_extensions_active() != 0;
	for (int index = 0; index < Num_triggers; ++index) {
		xl_trigger_state value{};
		if (xl_wall_trigger_state(index, &value)) pilot["wall_triggers"][std::to_string(index)] = {
			{ "operated", value.operated != 0 }, { "actor", value.object }, { "shot", value.shot }, { "value", Triggers[index].value }
		};
	}
	for (int index = 0; index < Num_walls; ++index) pilot["doorways"][std::to_string(index)] = { { "state", Walls[index].state }, { "type", Walls[index].type } };
	for (int index = 0; index < xl_speed_boost_state_count(); ++index) {
		xl_speed_boost_state value{};
		if (xl_speed_boost_state_get(index, &value)) pilot["boosts"][std::to_string(index)] = { { "owner", value.owner } };
	}
	state["level_scene"]["reactors"] = Json::array();
	state["level_scene"]["robots"] = Json::array();
	for (int index = 0; index <= Highest_object_index; ++index)
		if (Objects[index].type == OBJ_CNTRLCEN) {
			Json guns = Json::array();
			const auto &object = Objects[index];
			for (int gun = 0; gun < get_reactor_definition(object.id)->n_guns; ++gun) {
				const auto &point = object.ctype.reactor_info.gun_pos[gun], &direction = object.ctype.reactor_info.gun_dir[gun];
				guns.push_back({ { "position_fixed", { point.x, point.y, point.z } }, { "direction_fixed", { direction.x, direction.y, direction.z } } });
			}
			state["level_scene"]["reactors"].push_back({ { "object", index }, { "shields", double(object.shields) / F1_0 }, { "guns", guns } });
		} else if (Objects[index].type == OBJ_ROBOT && ConsoleObject && vm_vec_dist(&Objects[index].pos, &ConsoleObject->pos) < i2f(200))
			state["level_scene"]["robots"].push_back({ { "object", index }, { "id", Objects[index].id }, { "segment", Objects[index].segnum }, { "shields", double(Objects[index].shields) / F1_0 }, { "boss", Robot_info[Objects[index].id].boss_flag }, { "position_fixed", { Objects[index].pos.x, Objects[index].pos.y, Objects[index].pos.z } } });
	for (int weapon : { 0, CONTROLCEN_WEAPON_NUM })
		if (weapon < N_weapon_types) state["weapon_definitions"][std::to_string(weapon)] = { { "damage_radius", Weapon_info[weapon].damage_radius }, { "strength", Weapon_info[weapon].strength[Difficulty_level] }, { "speed", Weapon_info[weapon].speed[Difficulty_level] } };
	return state;
}
static bool matches(const Json &actual, const Json &expected)
{
	if (!expected.is_object()) return actual == expected;
	for (const auto &[key, value] : expected.items()) {
		if (key == "contains_object") {
			if (!actual.is_array() || std::none_of(actual.begin(), actual.end(), [&](const auto &item) { return matches(item, value); })) return false;
		} else if (key == "lt" || key == "gt" || key == "lte" || key == "gte") {
			if (!actual.is_number() || !value.is_number()) return false;
			const double a = actual.get<double>(), b = value.get<double>();
			if ((key == "lt" && !(a < b)) || (key == "gt" && !(a > b)) || (key == "lte" && !(a <= b)) || (key == "gte" && !(a >= b))) return false;
		} else if (!actual.is_object() || !actual.contains(key) || !matches(actual.at(key), value)) return false;
	}
	return true;
}
static bool expectations(const Json &state, const Json &expected, std::string &failed)
{
	for (const auto &[path, value] : expected.items()) {
		const Json *actual = &state;
		std::istringstream names(path);
		std::string key;
		while (std::getline(names, key, '.')) {
			if (!actual->is_object() || !actual->contains(key)) {
				failed = path + " missing";
				return false;
			}
			actual = &actual->at(key);
		}
		if (!matches(*actual, value)) {
			failed = path + " actual=" + actual->dump() + " expected=" + value.dump();
			return false;
		}
	}
	return true;
}
static void pose(const Json &step)
{
	const int segment = step.at("segment");
	require(segment >= 0 && segment <= Highest_segment_index, "native player pose segment out of range");
	vms_vector position, forward;
	if (step.at("action") == "face_view") {
		const int side = step.at("side");
		require(side >= 0 && side < 6 && Segments[segment].sides[side].type == SIDE_IS_QUAD, "native player probe requires a quad local approach");
		vms_vector center, inward;
		compute_center_point_on_side(&position, &Segments[segment], side);
		compute_segment_center(&center, &Segments[segment]);
		vm_vec_sub(&forward, &center, &position);
		const fix length = vm_vec_normalize_quick(&forward);
		inward = Segments[segment].sides[side].normals[0];
		if (vm_vec_dot(&inward, &forward) < 0) vm_vec_negate(&inward);
		vm_vec_normalize_quick(&inward);
		fix distance = fl2f(step.value("distance", 8.0));
		if (distance >= length) distance = length / 2;
		require(distance > 0, "native player approach has no clearance");
		vm_vec_scale_add2(&position, &inward, distance);
		forward = inward;
		vm_vec_negate(&forward);
		vm_vector_2_matrix(&ConsoleObject->orient, &forward, nullptr, nullptr);
	} else {
		const auto &point = step.at("position_fixed");
		position = { point[0].get<int>(), point[1].get<int>(), point[2].get<int>() };
		const vms_angvec angles{ static_cast<fixang>(step.value("pitch", 0)), static_cast<fixang>(step.value("bank", 0)), static_cast<fixang>(step.value("heading", 0)) };
		vm_angles_2_matrix(&ConsoleObject->orient, &angles);
	}
	const int found = find_point_seg(&position, segment);
	require(found >= 0, "native player approach is outside the original mine");
	obj_relink(ConsoleObject - Objects, found);
	ConsoleObject->pos = ConsoleObject->last_pos = position;
	ConsoleObject->mtype.phys_info.velocity = ConsoleObject->mtype.phys_info.rotvel = {};
	ConsoleObject->mtype.phys_info.thrust = ConsoleObject->mtype.phys_info.rotthrust = {};
}
static int run(int argc, char **argv)
{
	const auto *extra = find_arg_value(argc, argv, "-extra-dir");
	if (!extra || !*extra) {
		std::cerr << "Native Pumo player probe requires an isolated extra directory\n";
		return 1;
	}
	const auto root = std::filesystem::path(extra);
	const auto *resource_argument = find_arg_value(argc, argv, "-pumo-resource-root");
	const auto resources = resource_argument ? std::filesystem::path(resource_argument) : root;
	std::ofstream trace(root / "player-trace.jsonl");
	int completed = 0;
	try {
		for (const auto &[file, expected] : std::map<std::string, std::string>{
		         { "missions/pmines.hog", "6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043" },
		         { "missions/pmines.mn2", "45b4138e0a0d49ab8ee09334b7cfa97c0ec75ac6c6b6406b21eba28ddaa19cfd" },
		         { "mods/pmines/pmines.vham", "a623c7a909e6a7b546816f5082e57d33a19fc4b59f84bb3f8493df3ac86b6b05" } }) {
			require(file_hash(resources / file) == expected, ("Native Pumo player requires original " + file).c_str());
		}
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error), true), error);
		require(mount_extra_dir(root.string().c_str(), error, sizeof(error)), error);
		if (resources != root)
			require(PHYSFS_addToSearchPath(resources.string().c_str(), 1), "cannot mount read-only original Pumo resource input");
		require(load_base_mission("d2", error, sizeof(error)), error);
		require(load_base_mission("pmines", error, sizeof(error)), error);
		require(N_robot_types == 78 && N_weapon_types == 65 && Robot_info[68].model_num == 17 && xl_owned_oof_model(171), "native player requires full original Pumo definitions/models, not Vertigo");
		const auto &sources = xl_resource_mod_sources();
		for (const auto *file : { "mods/pmines/pmines.ham", "mods/pmines/pmines.hxm", "mods/pmines/pmines.vham", "mods/pmines/models/model171.oof" })
			require(sources.count(file), "native player is missing original mission-owned resources");
		std::ifstream input(find_arg_value(argc, argv, "-pumo-player-test"));
		Json plan;
		input >> plan;
		std::strcpy(Players[Player_num].callsign, "PumoNat");
		Difficulty_level = 0;
		input_demo_set_skip_level_intro(1);
		const int initial_level = plan.at("initial_level");
		require(initial_level >= 1 && initial_level <= Last_level, "native player initial level out of range");
		StartNewGame(initial_level);
		if (plan.contains("resume_checkpoint")) {
			const std::string file = plan.at("resume_checkpoint");
			require(!file.empty() && std::filesystem::path(file).filename() == file, "native checkpoint must name an isolated save file");
			require(file_hash(root / "preview-user" / file) == plan.at("resume_sha256"), "native earned checkpoint hash changed");
			std::vector<char> name(file.begin(), file.end());
			name.push_back(0);
			require(state_restore_all_sub(name.data(), 0), "native earned checkpoint failed to restore");
			std::string failure;
			const bool restored = expectations(snapshot(), plan.at("checkpoint_expect"), failure);
			require(restored, ("native earned checkpoint: " + failure).c_str());
		}
		double thrust = 0;
		bool firing = false;
		int remainder = 0, fps = plan.value("simulation_fps", 60);
		require(fps >= 1 && fps <= 120, "native player simulation rate out of range");
		Json transitions = Json::array(), checkpoints = Json::object(), last_activation = snapshot();
		auto observed_mask = xl_preview_trigger_mask();
		const auto tick = [&] {
			require(!Player_is_dead && !cheats.enabled && !cheats.ghostphysics && !cheats.monsterdamage && !cheats.robotfiringsuspended && !(Players[Player_num].flags & PLAYER_FLAGS_INVULNERABLE), "ordinary player probe died or acquired assisted combat");
			remainder += F1_0;
			FrameTime = remainder / fps;
			remainder %= fps;
			Controls = {};
			Controls.forward_thrust_time = static_cast<fix>(thrust * FrameTime);
			Controls.fire_primary_state = firing;
			do_weapon_n_item_stuff();
			calc_game_time();
			// Presentation is outside this simulation probe, including later levels
			input_demo_set_skip_level_intro(1);
			const int before_level = Current_level_num;
			GameProcessFrame();
			const auto mask = xl_preview_trigger_mask();
			if (before_level != Current_level_num) {
				transitions.push_back({ { "from", before_level }, { "to", Current_level_num }, { "last_activation", last_activation } });
				trace << Json{ { "event", "normal_level_transition" }, { "transition", transitions.back() }, { "state", snapshot() } }.dump() << '\n';
				last_activation = snapshot();
			} else if (mask != observed_mask) {
				last_activation = snapshot();
				trace << Json{ { "event", "trigger_change" }, { "state", last_activation } }.dump() << '\n';
			}
			observed_mask = mask;
		};
		for (const auto &step : plan.at("steps")) {
			const std::string action = step.at("action");
			trace << Json{ { "step", completed }, { "action", action }, { "simulation_fps", fps }, { "state", snapshot() } }.dump() << '\n';
			trace.flush();
			std::string failed;
			if (action == "assert") {
				const bool passed = expectations(snapshot(), step.at("expect"), failed);
				require(passed, failed.c_str());
			} else if (action == "face_view" || action == "pose_view") pose(step);
			else if (action == "send_axis") {
				require(step.at("axis") == 1, "native player probe only supports forward thrust");
				thrust = -step.at("axis_value").get<double>();
			} else if (action == "send_button") {
				require(step.at("button") == 100, "native player probe only supports primary fire");
				firing = step.value("held", step.value("pressed", 0)) != 0;
			} else if (action == "select_primary") {
				const int slot = step.at("slot");
				require(slot >= 0 && slot < 5 && !firing, "native weapon selection requires a normal released-fire weapon slot");
				// Use the keyboard selection path, including ownership and upgrade checks
				do_weapon_select(slot, 0);
			} else if (action == "wait_ms") {
				for (int frame = 0, count = (step.at("ms").get<int>() * fps + 999) / 1000; frame < count; ++frame) tick();
			} else if (action == "wait_for") {
				const int limit = (step.value("timeout_ms", 10000) * fps + 999) / 1000;
				int frame = 0;
				while (!expectations(snapshot(), step.at("expect"), failed) && frame++ < limit) tick();
				const bool passed = expectations(snapshot(), step.at("expect"), failed);
				require(passed, ("native wait: " + failed).c_str());
			} else if (action == "set_debug") {
				require(step.at("field") == "android_game_request", "unsupported native player request");
				char file[] = "pumo-player-quick.sav", description[] = "Ordinary Pumo checkpoint";
				if (step.at("value") == "quick_save") {
					stop_time();
					require(state_save_all_sub(file, description), "native player checkpoint save failed");
					// Preserve the exact earned file before a later level reuses quick-save
					const auto retained = "pumo-player-level" + std::to_string(Current_level_num) + ".sav";
					std::filesystem::copy_file(root / "preview-user" / file, root / "preview-user" / retained, std::filesystem::copy_options::overwrite_existing);
					checkpoints[std::to_string(Current_level_num)] = { { "file", retained }, { "sha256", file_hash(root / "preview-user" / retained) }, { "state", snapshot() } };
				} else {
					require(step.at("value") == "quick_load", "unsupported native player save action");
					require(state_restore_all_sub(file, 0), "native player checkpoint load failed");
				}
			} else if (action == "frame_rate") {
				fps = step.at("fps");
				require(fps >= 1 && fps <= 120, "native player simulation rate out of range");
				remainder = 0;
			} else require(action == "introspect", "unsupported native player action");
			++completed;
		}
		std::ofstream(root / "player-state.json") << snapshot().dump(2) << '\n';
#ifdef _WIN32
		const int process = _getpid();
#else
		const int process = getpid();
#endif
		std::ofstream(root / "player-result.json") << Json{ { "result", "PASS" }, { "steps", completed }, { "process_id", process }, { "initial_level_setup", initial_level }, { "resumed_earned_checkpoint", plan.contains("resume_checkpoint") }, { "final_level", Current_level_num }, { "transitions", transitions }, { "checkpoints", checkpoints }, { "rendered", bool{ false } } }.dump(2) << '\n';
		std::cout << "Original Pumo ordinary player controls passed: " << completed << " steps, initial level setup " << initial_level << " -> " << Current_level_num << "; unrendered\n";
		return 0;
	} catch (const std::exception &error) {
		std::ofstream(root / "player-state.json") << snapshot().dump(2) << '\n';
		std::ofstream(root / "player-result.json") << Json{ { "result", "FAIL" }, { "steps", completed }, { "error", error.what() } }.dump(2) << '\n';
		std::cerr << "Original Pumo player probe failed: " << error.what() << '\n';
		return 1;
	}
}
} // namespace pumo_player_probe
