// Derived property controls on unchanged Neptune geometry, without navigation
static void modern_water_boundary_test(int wet)
{
	int side = -1, dry = -1;
	for (int i = 0; i < 6; ++i) {
		const int child = Segments[wet].children[i];
		if (child >= 0 && Segments[wet].sides[i].wall_num == -1 && !xl_preview_water_segment(child) && !xl_preview_no_damage_segment(child)) {
			side = i;
			dry = child;
			break;
		}
	}
	require(side >= 0, "modern water fixture lacks dry shared boundary");
	const auto saved_object = *ConsoleObject;
	const auto saved_player = Players[Player_num];
	const auto saved_frame = FrameTime;
	const auto saved_laser = Next_laser_fire_time, saved_missile = Next_missile_fire_time;
	unsigned saved_rng;
	d_rand_get_state(&saved_rng);
	std::vector<ubyte> flags;
	for (int i = 0; i <= Highest_object_index; ++i) {
		flags.push_back(Objects[i].flags);
		if (&Objects[i] != ConsoleObject) Objects[i].flags |= OF_SHOULD_BE_DEAD;
	}
	vms_vector portal, inside, outside;
	compute_center_point_on_side(&portal, &Segments[wet], side);
	compute_segment_center(&inside, &Segments[wet]);
	compute_segment_center(&outside, &Segments[dry]);
	for (auto *point : { &inside, &outside }) {
		vm_vec_sub(point, point, &portal);
		vm_vec_scale(point, F1_0 / 4);
		vm_vec_add2(point, &portal);
	}
	FrameTime = F1_0 / 4;
	Next_laser_fire_time = Next_missile_fire_time = GameTime64;
	for (bool entering : { true, false }) {
		obj_relink(ConsoleObject - Objects, entering ? dry : wet);
		ConsoleObject->pos = entering ? outside : inside;
		const auto finish = entering ? inside : outside;
		ConsoleObject->orient = vmd_identity_matrix;
		ConsoleObject->mtype.phys_info = {};
		ConsoleObject->mtype.phys_info.mass = F1_0;
		vm_vec_sub(&ConsoleObject->mtype.phys_info.velocity, &finish, &ConsoleObject->pos);
		vm_vec_scale(&ConsoleObject->mtype.phys_info.velocity, i2f(4));
		Players[Player_num].flags &= ~PLAYER_FLAGS_INVULNERABLE;
		Players[Player_num].shields = ConsoleObject->shields = i2f(100);
		do_physics_sim(ConsoleObject);
		require(ConsoleObject->segnum == (entering ? wet : dry), "modern water native boundary crossing failed");
		require(bool(xl_preview_water_segment(ConsoleObject->segnum)) == entering && bool(xl_preview_no_damage_segment(ConsoleObject->segnum)) == entering, "combined source properties did not follow native boundary crossing");
		require(bool(allowed_to_fire_laser()) == !entering && bool(allowed_to_fire_missile()) == !entering, "combined boundary firing gate differs");
		apply_damage_to_player(ConsoleObject, nullptr, i2f(10), 0);
		require(Players[Player_num].shields == i2f(entering ? 100 : 90), "combined boundary damage gate differs");
	}
	obj_relink(ConsoleObject - Objects, saved_object.segnum);
	const int next = ConsoleObject->next, prev = ConsoleObject->prev;
	*ConsoleObject = saved_object;
	ConsoleObject->next = next;
	ConsoleObject->prev = prev;
	Players[Player_num] = saved_player;
	for (std::size_t i = 0; i < flags.size(); ++i) Objects[i].flags = flags[i];
	FrameTime = saved_frame;
	Next_laser_fire_time = saved_laser;
	Next_missile_fire_time = saved_missile;
	d_srand(saved_rng);
	std::cout << "Modern water source boundary: both directions, combined properties, firing and damage; no private property binding\n";
}

static int modern_properties_test(int argc, char **argv)
{
	try {
		const auto mode = find_arg_value(argc, argv, "-modern-properties-test");
		const bool water_mode = !std::strcmp(mode, "water");
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		require(load_base_mission("neptunxl", error, sizeof(error)), error);
		std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> file(PHYSFS_openRead("neptunxl.rl2"), PHYSFS_close);
		require(file && PHYSFS_fileLength(file.get()) > 0 && PHYSFS_fileLength(file.get()) < 1024 * 1024, "missing bounded Neptune fixture");
		xl_compat::Bytes bytes(static_cast<std::size_t>(PHYSFS_fileLength(file.get())));
		require(PHYSFS_readBytes(file.get(), bytes.data(), bytes.size()) == PHYSFS_sint64(bytes.size()), "incomplete property fixture");
		file.reset();
		auto source = xl_compat::inspect_level(bytes);
		require(source.level_version == 23 && source.game_version == 40, "wrong modern property fixture");
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		int protected_segment = -1, fuel_segment = -1;
		for (int i = 0; i < Num_segments; ++i) {
			if (source.segments[i].function == SEGMENT_IS_FUELCEN) fuel_segment = i;
			if (source.segments[i].function != 0 || protected_segment >= 0) continue;
			for (int side = 0; side < 6; ++side) {
				const int child = Segments[i].children[side];
				if (child >= 0 && source.segments[child].function == 0 && Segments[i].sides[side].wall_num == -1) protected_segment = i;
			}
		}
		require(protected_segment >= 0 && fuel_segment >= 0, "fixture lacks required independent functions and shared boundary");
		source.segments[protected_segment].properties = xl_compat::segment_no_damage;
		no_damage_collision_test(source);
		source.segments[fuel_segment].properties = xl_compat::segment_no_damage;
		xl_test_bind_no_damage(source);
		require(Segment2s[fuel_segment].special == SEGMENT_IS_FUELCEN && xl_preview_no_damage_segment(fuel_segment), "modern no-damage replaced fuel function");
		const auto frame = FrameTime;
		FrameTime = F1_0 / 60;
		require(fuelcen_give_fuel(&Segments[fuel_segment], F1_0) > 0, "protected fuel function no longer supplies energy");
		FrameTime = frame;
		require(load_level("neptunxl.rl2") == 0 && !xl_preview_no_damage_segment(protected_segment) && !xl_preview_no_damage_segment(fuel_segment), "derived binding survived original source reload");
		// Serialize the same derived properties and use ordinary mission admission
		const auto extras = source.game_offset - (source.vertices.size() + source.segments.size() * 6 + 910) * 13 - source.segments.size() * 13;
		int water_only = -1;
		for (int i = 0; i < Num_segments; ++i)
			if (i != protected_segment && i != fuel_segment && source.segments[i].function == 0) water_only = i;
		require(water_only >= 0, "missing water-only control region");
		for (int segment : { protected_segment, fuel_segment }) {
			require(bytes.at(extras + segment * 13) == source.segments[segment].function && bytes.at(extras + segment * 13 + 4) == 0, "derived property offset mismatch");
			bytes.at(extras + segment * 13 + 4) = xl_compat::segment_no_damage | (water_mode ? xl_compat::segment_water : 0);
		}
		if (water_mode) bytes.at(extras + water_only * 13 + 4) = xl_compat::segment_water;
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
		std::ifstream hog_file(root / "neptunxl.hog", std::ios::binary);
		xl_compat::Bytes hog_bytes{ std::istreambuf_iterator<char>(hog_file), std::istreambuf_iterator<char>() };
		xl_compat::Bytes derived{ 'D', 'H', 'F' };
		for (auto member : xl_compat::read_hog(hog_bytes)) {
			if (member.name == "neptunxl.rl2") member.bytes = bytes;
			char name[13] = {};
			require(member.name.size() < sizeof(name), "derived HOG member name exceeds bound");
			std::strcpy(name, member.name.c_str());
			derived.insert(derived.end(), name, name + sizeof(name));
			for (unsigned shift = 0; shift < 32; shift += 8) derived.push_back(static_cast<unsigned char>(member.bytes.size() >> shift));
			derived.insert(derived.end(), member.bytes.begin(), member.bytes.end());
		}
		write_bytes((root / "modprop.hog").string().c_str(), derived);
		std::ofstream descriptor(root / "modprop.mn2");
		descriptor << "d2x-name = Derived modern properties\nnum_levels = 1\nneptunxl.rl2\n";
		descriptor.close();
		require(load_base_mission("modprop", error, sizeof(error)), error);
		require(xl_level_supported("neptunxl.rl2", 0), xl_level_rejection());
		require(!xl_level_supported("neptunxl.rl2", 1), "modern properties admitted multiplayer");
		StartNewGame(1);
		require(xl_preview_no_damage_segment(protected_segment) && xl_preview_no_damage_segment(fuel_segment) && Segment2s[fuel_segment].special == SEGMENT_IS_FUELCEN, "serialized modern property/function map lost");
		depth_policy(true);
		if (water_mode) {
			require(xl_preview_pilot_active(), "modern water failed to select scene-depth rendering profile");
			modern_water_boundary_test(protected_segment);
			require(xl_preview_water_segment(protected_segment) && xl_preview_water_segment(fuel_segment) && xl_preview_water_segment(water_only) && !xl_preview_no_damage_segment(water_only), "modern water/no-damage combination lost independent bits");
			unsigned tinted_vertices = 0;
			for (int s : { protected_segment, water_only, fuel_segment }) {
				for (int side = 0; side < 6; ++side) {
					if (Segments[s].children[side] >= 0 || Segments[s].sides[side].wall_num >= 0) continue;
					int vertices[4];
					get_side_verts(vertices, s, side);
					for (int vertex : vertices) {
						std::array<int32_t, 3> plain{}, water{}, after{};
						xl_preview_end_face();
						xl_preview_vertex(vertex, F1_0, &plain[0], &plain[1], &plain[2]);
						xl_preview_begin_face(s, side);
						xl_preview_vertex(vertex, F1_0, &water[0], &water[1], &water[2]);
						xl_preview_end_face();
						xl_preview_vertex(vertex, F1_0, &after[0], &after[1], &after[2]);
						require(after == plain, "water material leaked beyond face draw");
						if (water != plain) ++tinted_vertices;
					}
				}
			}
			require(tinted_vertices > 0, "modern water material never reached runtime vertex shading");
			unsigned colored_portals = 0;
			for (int s = 0; s < Num_segments; ++s) {
				for (int side = 0; side < 6; ++side) {
					int32_t points[4][3];
					float colors[4][4];
					const int child = Segments[s].children[side];
					const bool colored = xl_preview_water_portal(s, side, points, colors) != 0;
					if (!xl_preview_water_segment(s) || (child >= 0 && xl_preview_water_segment(child)) || Segments[s].sides[side].wall_num >= 0)
						require(!colored, "dry, internal-water or wall-backed portal colored");
					if (!colored) continue;
					++colored_portals;
					require(child == -2 || (child >= 0 && source.segments[s].function == source.segments[child].function), "portal ignored independent function boundary");
					int vertices[4];
					get_side_verts(vertices, s, side);
					for (int i = 0; i < 4; ++i) {
						const int corner = (i + (Segments[s].sides[side].type == SIDE_IS_TRI_13 ? 1 : 0)) % 4;
						const auto &v = Vertices[vertices[corner]];
						require(points[i][0] == v.x && points[i][1] == v.y && points[i][2] == v.z && colors[i][3] == 0.333f, "portal geometry, split or alpha changed");
					}
				}
			}
			require(colored_portals > 0, "derived modern fixture has no colored portal coverage");
			bool external_checked = false;
			for (int side = 0; side < 6 && !external_checked; ++side) {
				auto &segment = Segments[protected_segment];
				if (segment.children[side] != -1 || segment.sides[side].wall_num >= 0) continue;
				int32_t points[4][3];
				float colors[4][4];
				require(!xl_preview_water_portal(protected_segment, side, points, colors), "solid water wall treated as portal");
				// Derived external-exit control: topology restored before gameplay resumes
				segment.children[side] = -2;
				const bool external = xl_preview_water_portal(protected_segment, side, points, colors) != 0;
				segment.children[side] = -1;
				require(external, "external water boundary omitted");
				external_checked = true;
			}
			require(external_checked, "no solid face available for external boundary control");
			const auto prior_object = *ConsoleObject;
			const auto prior_time = FrameTime;
			obj_relink(ConsoleObject - Objects, water_only);
			compute_segment_center(&ConsoleObject->pos, &Segments[water_only]);
			ConsoleObject->mtype.phys_info.velocity = { F1_0, 0, 0 };
			ConsoleObject->mtype.phys_info.thrust = ConsoleObject->mtype.phys_info.rotvel = ConsoleObject->mtype.phys_info.rotthrust = {};
			ConsoleObject->mtype.phys_info.drag = ConsoleObject->mtype.phys_info.flags = 0;
			FrameTime = F1_0 / 16;
			const auto start = ConsoleObject->pos;
			do_physics_sim(ConsoleObject);
			require(ConsoleObject->pos.x - start.x == 3072 && ConsoleObject->pos.y == start.y && ConsoleObject->pos.z == start.z && ConsoleObject->mtype.phys_info.velocity.x == F1_0, "modern water native displacement or stored velocity differs");
			const auto calls = d_rand_get_stream_call_count(D_RNG_SIM);
			require(!xl_preview_water_frame(ConsoleObject - Objects) && d_rand_get_stream_call_count(D_RNG_SIM) == calls, "idle modern water consumed simulation RNG");
			ConsoleObject->mtype.phys_info.thrust.x = F1_0;
			require(xl_preview_water_frame(ConsoleObject - Objects) && d_rand_get_stream_call_count(D_RNG_SIM) == calls + 2, "modern water turbulence RNG differs");
			Players[Player_num].shields = i2f(100);
			apply_damage_to_player(ConsoleObject, nullptr, i2f(10), 0);
			require(Players[Player_num].shields == i2f(90), "water alone incorrectly grants no-damage protection");
			obj_relink(ConsoleObject - Objects, prior_object.segnum);
			ConsoleObject->pos = prior_object.pos;
			ConsoleObject->mtype = prior_object.mtype;
			FrameTime = prior_time;
		}
		obj_relink(ConsoleObject - Objects, protected_segment);
		compute_segment_center(&ConsoleObject->pos, &Segments[protected_segment]);
		Players[Player_num].shields = i2f(83);
		char save[] = "xl-modern.sav", description[] = "Modern property source restore";
		stop_time();
		require(state_save_all_sub(save, description) != 0, "modern property save failed");
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "modern property stock recovery failed");
		depth_policy(false);
		for (int i = 0; i < Num_segments; ++i) require(!xl_preview_no_damage_segment(i) && !xl_preview_water_segment(i), "property state leaked into stock");
		int32_t stock_points[4][3];
		float stock_colors[4][4];
		for (int i = 0; i < Num_segments; ++i)
			for (int side = 0; side < 6; ++side)
				require(!xl_preview_water_portal(i, side, stock_points, stock_colors), "water portal state leaked into stock");
		require(state_restore_all_sub(save, 0) != 0 && !std::strcmp(Current_mission_filename, "modprop"), "modern source restore from stock failed");
		depth_policy(true);
		require(xl_preview_no_damage_segment(protected_segment) && xl_preview_no_damage_segment(fuel_segment) && Segment2s[fuel_segment].special == SEGMENT_IS_FUELCEN, "restore lost independent modern properties");
		require(bool(xl_preview_water_segment(protected_segment)) == water_mode && bool(xl_preview_water_segment(water_only)) == water_mode, "source restore lost water property state");
		apply_damage_to_player(ConsoleObject, nullptr, i2f(10), 0);
		require(Players[Player_num].shields == i2f(83), "restored modern property failed to suppress damage");
		int ordinary = -1;
		for (int i = 0; i < Num_segments; ++i)
			if (!xl_preview_no_damage_segment(i) && Segment2s[i].special == 0) ordinary = i;
		require(ordinary >= 0, "missing ordinary firing control");
		no_damage_firing_test(protected_segment, ordinary);
		std::cout << "Modern no-damage derived controls passed: damage, firing, collisions, fusion, boundary, independent fuel function, source admission and save restoration\n";
		if (water_mode) std::cout << "Modern water derived controls passed: normal admission, native motion, turbulence, material boundaries, independent protection and source restore\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
