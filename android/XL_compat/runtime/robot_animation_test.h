extern "C" int do_silly_animation(object *);

static void unused_robot_joint_controls(const std::filesystem::path &root)
{
	using Json = nlohmann::ordered_json;
	std::ifstream original_file(root / "missions/hxmtest.hog", std::ios::binary);
	const xl_compat::Bytes original{ std::istreambuf_iterator<char>(original_file), std::istreambuf_iterator<char>() };
	xl_compat::Bytes output{ 'D', 'H', 'F' };
	int changed = 0, joint_index = -1;
	for (auto member : xl_compat::read_hog(original)) {
		if (!d_stricmp(member.name.c_str(), "alarlof.hxm")) {
			require(member.bytes.size() == 11262, "Unused-joint HXM source changed");
			joint_index = int(member.bytes.at(500)) | (int(member.bytes.at(501)) << 8) |
			              (int(member.bytes.at(502)) << 16) | (int(member.bytes.at(503)) << 24);
			member.bytes.at(504) = MAX_SUBMODELS - 1;
			member.bytes.at(505) = 0;
			++changed;
		}
		char name[13]{};
		require(member.name.size() < sizeof(name), "Unused-joint member name too long");
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(uint8_t(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	require(changed == 1 && joint_index >= 0 && joint_index < N_robot_joints, "Unused-joint fixture row missing");
	write_bytes((root / "missions/hxmjoint.hog").string().c_str(), output);
	{
		std::ofstream descriptor(root / "missions/hxmjoint.mn2");
		descriptor << "d2x-name = Bounded unused joints\nnum_levels = 1\nalarlof.rl2\n";
	}
	char error[256]{};
	require(load_base_mission("hxmjoint", error, sizeof(error)), error);
	StartNewGame(1);
	require(Robot_joints[joint_index].jointnum == MAX_SUBMODELS - 1, "Admitted unused joint was dropped or rewritten");
	const int owner = 100;
	auto &obj = Objects[owner];
	const auto saved_object = obj;
	const auto saved_robot = Robot_info[obj.id];
	const auto saved_local = Ai_local_info[owner];
	const int model = obj.rtype.pobj_info.model_num;
	const int saved_count = Polygon_models[model].n_models;
	std::array<jointpos, 5> saved_joints;
	std::copy_n(Robot_joints, 5, saved_joints.begin());
	Json samples = Json::array();
	const auto angles = [](const vms_angvec *values) {
		Json result = Json::array();
		for (int i = 0; i < MAX_SUBMODELS; ++i) result.push_back({ int(values[i].p), int(values[i].b), int(values[i].h) });
		return result;
	};
	for (int count : { 1, 3, MAX_SUBMODELS })
		for (int guns : { 1, 2, MAX_GUNS })
			for (int goal = 1; goal < 8; ++goal)
				for (int attack : { 0, 1 })
					for (int unused_only : { 0, 1 }) {
						obj = saved_object;
						Robot_info[obj.id] = saved_robot;
						Ai_local_info[owner] = saved_local;
						Polygon_models[model].n_models = count;
						auto &robot = Robot_info[obj.id];
						auto &local = Ai_local_info[owner];
						robot.n_guns = guns;
						robot.attack_type = attack;
						obj.ctype.ai_info.GOAL_STATE = goal;
						obj.ctype.ai_info.CURRENT_STATE = AIS_REST;
						const int targets[] = { 0, 1, 2, 3, MAX_SUBMODELS - 1 };
						Json rows = Json::array();
						for (int i = 0; i < 5; ++i) {
							Robot_joints[i] = { static_cast<short>(unused_only ? MAX_SUBMODELS - 1 : targets[i]),
								                { static_cast<fixang>(100 + i), static_cast<fixang>(-200 - i), static_cast<fixang>(32700 - i) } };
							rows.push_back({ int(Robot_joints[i].jointnum), int(Robot_joints[i].angles.p), int(Robot_joints[i].angles.b), int(Robot_joints[i].angles.h) });
						}
						for (int g = 0; g <= MAX_GUNS; ++g) {
							for (int state = 0; state < N_ANIM_STATES; ++state) robot.anim_states[g][state] = { 5, 0 };
							local.goal_state[g] = g % 2 ? AIS_RECO : AIS_FLIN;
							local.achieved_state[g] = AIS_REST;
						}
						for (int i = 0; i < MAX_SUBMODELS; ++i) {
							obj.rtype.pobj_info.anim_angles[i] = { static_cast<fixang>(i), 0, -30000 };
							local.goal_angles[i] = { 123, 456, 789 };
							local.delta_angles[i] = { 12, 34, 56 };
						}
						const int result = do_silly_animation(&obj);
						Json achieved = Json::array(), goals = Json::array();
						for (int g = 0; g <= MAX_GUNS; ++g) {
							achieved.push_back(local.achieved_state[g]);
							goals.push_back(local.goal_state[g]);
						}
						samples.push_back({ { "count", count }, { "guns", guns }, { "goal", goal }, { "attack", attack }, { "rows", rows }, { "result", result }, { "current", int(obj.ctype.ai_info.CURRENT_STATE) }, { "angles", angles(obj.rtype.pobj_info.anim_angles) }, { "goals", angles(local.goal_angles) }, { "deltas", angles(local.delta_angles) }, { "achieved", achieved }, { "states", goals } });
						for (int i = count; i < MAX_SUBMODELS; ++i)
							require(local.goal_angles[i].p == 123 && local.delta_angles[i].p == 12,
							        "Unused joint wrote animation state outside the active model");
					}
	obj = saved_object;
	Robot_info[obj.id] = saved_robot;
	Ai_local_info[owner] = saved_local;
	Polygon_models[model].n_models = saved_count;
	std::copy(saved_joints.begin(), saved_joints.end(), Robot_joints);
	std::ofstream file(root / "robot-animation-samples.txt");
	file << samples.size() << '\n';
	for (const auto &sample : samples) {
		for (const char *key : { "count", "guns", "goal", "attack" }) file << sample.at(key).get<int>() << ' ';
		for (const auto &row : sample.at("rows"))
			for (const auto &value : row) file << value.get<int>() << ' ';
		for (const char *key : { "result", "current" }) file << sample.at(key).get<int>() << ' ';
		for (const char *key : { "angles", "goals", "deltas" })
			for (const auto &row : sample.at(key))
				for (const auto &value : row) file << value.get<int>() << ' ';
		for (const char *key : { "achieved", "states" })
			for (const auto &value : sample.at(key)) file << value.get<int>() << ' ';
		file << '\n';
	}
	require(bool(file), "Cannot write robot animation source samples");
	const auto definitions = gameplay_definitions();
	char save[] = "unused-joints.sav", description[] = "Unused joint identity";
	stop_time();
	require(state_save_all_sub(save, description), "Unused-joint save failed");
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(),
	        "Unused-joint classic transition failed");
	require(state_restore_all_sub(save, 0) && gameplay_definitions() == definitions && Robot_joints[joint_index].jointnum == MAX_SUBMODELS - 1,
	        "Unused-joint rows or source identity changed through disk/stock restore");
	require(load_base_mission("hxmtest", error, sizeof(error)), error);
	StartNewGame(1);
	std::cout << "Unused-joint native controls: " << samples.size() << " mixed/overlapping/boundary/all-unused animation cases\n";
}
