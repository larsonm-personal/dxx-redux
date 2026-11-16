extern "C" void set_robot_state(object *obj, int state);
static int original_gun_point_test(const std::filesystem::path &root)
{
	char error[256]{};
	require(load_base_mission("boilpnt", error, sizeof(error)), error);
	new_player_config();
	input_demo_set_skip_level_intro(1);
	Difficulty_level = 2;
	StartNewGame(7);
	require(xl_level_extensions_active() && !Player_is_dead && !Game_suspended && !cheats.enabled,
	        "Gun point audit requires ordinary original Boiling Point startup");
	const auto definitions = gameplay_definitions();
	const auto world = input_demo_world_trace_snapshot();
	uint32_t sim_before = 0, sim_after = 0;
	require(d_rand_get_state(&sim_before), "Cannot observe gun audit SIM state");
	const auto calls = d_rand_get_call_count();
	std::ofstream output(root / "gun-points.txt");
	require(bool(output), "Cannot open gun point receipt");
	output << "xl-gun-points-v1\n";
	const auto vector = [&](const vms_vector &v) { output << ' ' << v.x << ' ' << v.y << ' ' << v.z; };
	std::set<int> robots;
	unsigned cases = 0, beyond_geometry = 0;
	for (int index = 0; index <= Highest_object_index; ++index) {
		const auto &original = Objects[index];
		if (original.type != OBJ_ROBOT || !robots.insert(original.id).second) continue;
		const auto &robot = Robot_info[original.id];
		const int gun_count = robot.n_guns ? robot.n_guns : 1;
		require(robot.model_num == original.rtype.pobj_info.model_num, "Original robot and object model differ");
		const auto &model = Polygon_models[robot.model_num];
		for (int state = 0; state < N_ANIM_STATES; ++state) {
			object candidate = original;
			set_robot_state(&candidate, state);
			for (int gun = 0; gun < gun_count; ++gun) {
				vms_vector local = robot.gun_points[gun], actual{};
				require(!xl_model_gun_point(robot.model_num, gun, &local), "Gun audit requires original HAM/HXM gun points");
				calc_gun_point(&actual, &candidate, gun);
				output << "case " << index << ' ' << int(original.id) << ' ' << robot.model_num << ' ' << state << ' ' << gun
				       << ' ' << model.n_models << ' ' << int(robot.gun_submodels[gun]);
				vector(local);
				vector(candidate.pos);
				vector(candidate.orient.rvec);
				vector(candidate.orient.uvec);
				vector(candidate.orient.fvec);
				for (int part = 0; part < MAX_SUBMODELS; ++part) {
					output << ' ' << int(model.submodel_parents[part]);
					vector(model.submodel_offsets[part]);
					const auto &angle = candidate.rtype.pobj_info.anim_angles[part];
					output << ' ' << angle.p << ' ' << angle.b << ' ' << angle.h;
				}
				vector(actual);
				output << '\n';
				++cases;
				beyond_geometry += robot.gun_submodels[gun] >= model.n_models;
			}
		}
	}
	output << "end " << cases << '\n';
	output.close();
	require(bool(output) && cases && beyond_geometry, "Gun audit did not cover original fixed-header gun links");
	require(d_rand_get_state(&sim_after) && sim_before == sim_after && calls == d_rand_get_call_count() &&
	            definitions == gameplay_definitions() && world == input_demo_world_trace_snapshot(),
	        "Gun point sampling changed live definitions, world or SIM RNG");
	native_continuation_receipt(root / "texture-dependency-audit.json",
	                            { { "mission", "boilpnt" }, { "level", 7 }, { "gun_point_cases", cases }, { "fixed_header_cases", beyond_geometry }, { "sim_rng_unchanged", true }, { "limitation", "Native original-definition samples; pinned-source comparison is separate" } });
	return 0;
}
