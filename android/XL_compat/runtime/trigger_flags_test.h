// Derived 23/40 controls; authored dispatch without level navigation
static void write_trigger_flags_fixture(const std::filesystem::path &root)
{
	write_object_event_fixture(root);
	std::ifstream input(root / "events.hog", std::ios::binary);
	xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() }, output{ 'D', 'H', 'F' };
	int forcefield = -1;
	for (int i = 0; i < NumTextures; ++i)
		if (TmapInfo[i].flags & TMI_FORCE_FIELD) {
			forcefield = i;
			break;
		}
	require(forcefield >= 0, "retail forcefield material missing");
	for (auto member : xl_compat::read_hog(bytes)) {
		if (member.name == "neptunxl.rl2") {
			auto source = xl_compat::inspect_level(member.bytes);
			const auto patch = [&](std::size_t offset, std::uint32_t value, int width = 4) {
				for (int i = 0; i < width; ++i) member.bytes.at(offset + i) = static_cast<unsigned char>(value >> (8 * i));
			};
			const auto trigger_offset = source.sections[3].offset;
			member.bytes.insert(member.bytes.begin() + trigger_offset, 4 * 52, 0);
			for (std::size_t i = 0; i < source.sections.size(); ++i)
				if (source.sections[i].offset > trigger_offset) patch(source.game_offset + 35 + i * 12, source.sections[i].offset + 4 * 52);
			patch(source.game_offset + 75, 8);
			std::fill(member.bytes.begin() + trigger_offset, member.bytes.begin() + trigger_offset + 8 * 52, 0);
			const auto action = [&](int index, int type, int flags, int target, int time = -1) {
				const auto base = trigger_offset + index * 52;
				patch(base, type, 1);
				patch(base + 1, flags, 1);
				patch(base + 2, 1, 1);
				patch(base + 8, time);
				patch(base + 12, source.walls[target].segment, 2);
				patch(base + 32, source.walls[target].side, 2);
			};
			action(0, TT_CLOSE_WALL, 32, 0);
			action(1, TT_OPEN_WALL, 64, 0);
			action(2, TT_OPEN_WALL, 32, 0);
			action(3, TT_ILLUSION_OFF, 0, 2);
			action(4, TT_ILLUSION_OFF, 96 | TF_ONE_SHOT, 2);
			action(5, TT_ILLUSION_ON, 32, 2);
			action(6, TT_ILLUSION_OFF, 64 | 16, 2);
			action(7, TT_ILLUSION_OFF, 96, 2, 500);
			const auto wall_offset = source.sections[1].offset;
			for (std::size_t i = 0; i < source.walls.size(); ++i) {
				patch(wall_offset + i * 25 + 20, i < 8 ? static_cast<unsigned>(i) : 255, 1);
				if (i < 4) {
					patch(wall_offset + i * 25 + 16, i < 2 ? WALL_OPEN : WALL_ILLUSION, 1);
					patch(wall_offset + i * 25 + 17, 0, 2);
				}
			}
			// Existing fixture supplies ten owner bindings; only robot removal is enabled
			const auto object_offset = trigger_offset + 8 * 52 + 4;
			for (int i = 0; i < 10; ++i) {
				const auto base = object_offset + i * 53;
				patch(base, TT_ILLUSION_OFF, 1);
				patch(base + 1, i == 0 ? 98 : TF_DISABLED, 2);
				patch(base + 3, 1, 1);
				patch(base + 5, 0);
				patch(base + 9, -1);
				patch(base + 13, source.walls[2].segment, 2);
				patch(base + 33, source.walls[2].side, 2);
			}
			// Walk the known 23/40 mine encoding to patch just the existing textures
			std::size_t offset = source.mine_offset + 5 + source.vertices.size() * 12;
			const auto bits = [](unsigned value) { int n = 0; for (; value; value >>= 1) n += value & 1; return n; };
			for (std::size_t segment = 0; segment < source.segments.size(); ++segment) {
				offset += 2;
				const auto children = member.bytes.at(offset++);
				offset += bits(children & 63) * 2 + 16;
				const auto walls = member.bytes.at(offset++);
				offset += bits(walls & 63) * 2;
				for (int side = 0; side < 6; ++side) {
					const auto &face = source.segments[segment].sides[side];
					if (source.segments[segment].children[side] != -1 && face.wall == -1) continue;
					const bool overlay = (member.bytes.at(offset + 1) & 128) != 0;
					if (face.wall == 0 || face.wall == 1) patch(offset, forcefield | (overlay ? 0x8000 : 0), 2);
					offset += 26 + (overlay ? 2 : 0);
				}
			}
			source = xl_compat::inspect_level(member.bytes);
			require(xl_compat::runtime_blockers(source).empty(), "trigger flag fixture failed admission");
			for (int wall : { 0, 1 }) require(source.segments[source.walls[wall].segment].sides[source.walls[wall].side].texture == forcefield, "forcefield patch missed authored material");
		}
		char name[13] = {};
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (int i = 0; i < 4; ++i) output.push_back(static_cast<unsigned char>(member.bytes.size() >> (8 * i)));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "trigflag.hog").string().c_str(), output);
	std::ofstream descriptor(root / "trigflag.mn2");
	descriptor << "d2x-name = Derived XL trigger flags\nnum_levels = 1\nneptunxl.rl2\n";
}

static int trigger_flags_test(int argc, char **argv)
{
	try {
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
		write_trigger_flags_fixture(root);
		require(load_base_mission("trigflag", error, sizeof(error)), error);
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_level_extensions_active() && Num_triggers == 8 && xl_object_trigger_count() == 10, "flag source did not load");
		const auto fire = [&](int index) { xl_wall_trigger_reset(); return check_trigger_sub(index, Player_num, 1); };
		const auto hum = [] { return xl_segment_sound_count(Walls[0].segnum, Walls[0].sidenum, SOUND_FORCEFIELD_HUM, 0); };
		const auto illusion = [&](bool off) { require(bool(Walls[2].flags & WALL_ILLUSION_OFF) == off && bool(Walls[3].flags & WALL_ILLUSION_OFF) == off, "illusion connected-side pair differs"); };
		for (int flags : { 0, 32, 64, 96 }) {
			Triggers[0].flags = flags;
			require(fire(0) == 0 && Walls[0].type == WALL_CLOSED && Walls[1].type == WALL_CLOSED && hum() == 1, "forcefield close lost hum or paired walls");
			Triggers[1].flags = flags;
			const int enabled = (flags & 64) == 0;
			const auto prior = xl_trigger_cue_queries(SOUND_FORCEFIELD_OFF, enabled);
			require(fire(1) == 0 && Walls[0].type == WALL_OPEN && Walls[1].type == WALL_OPEN && hum() == 0, "forcefield open lost state or retained hum");
			require(xl_trigger_cue_queries(SOUND_FORCEFIELD_OFF, enabled) == prior + 1, "native forcefield cue decision differs");
			require(fire(1) == 0 && xl_trigger_cue_queries(SOUND_FORCEFIELD_OFF, enabled) == prior + 1, "unchanged forcefield emitted a cue");
			require(fire(5) == 0, "illusion reset failed");
			illusion(false);
			Triggers[3].flags = flags;
			const auto cue = xl_trigger_cue_queries(SOUND_WALL_REMOVED, !enabled);
			require(fire(3) == 0, "illusion toggle failed");
			illusion(true);
			require(xl_trigger_cue_queries(SOUND_WALL_REMOVED, !enabled) == cue + 1, "native illusion cue decision differs");
		}
		Triggers[0].flags = 32;
		Triggers[1].flags = 64;
		Triggers[3].flags = 0;
		// These bits do not change either teleport transform or consume extra RNG
		const auto leaf = Triggers[3];
		Triggers[3].type = 14;
		Triggers[3].seg[0] = 1;
		Triggers[3].side[0] = 0;
		vms_matrix expected_orientation{};
		vms_vector expected_velocity{};
		unsigned expected_rng = 0;
		for (int flags : { 0, 32, 64, 96 }) {
			Triggers[3].flags = flags;
			ConsoleObject->orient = vmd_identity_matrix;
			ConsoleObject->mtype.phys_info.velocity = { F1_0, 2 * F1_0, -F1_0 };
			d_srand(71);
			require(fire(3) == 0 && ConsoleObject->segnum == 1, "new flags blocked wall teleport");
			unsigned rng;
			require(d_rand_get_state(&rng), "flag teleport RNG unavailable");
			if (!flags) {
				expected_orientation = ConsoleObject->orient;
				expected_velocity = ConsoleObject->mtype.phys_info.velocity;
				expected_rng = rng;
			} else require(!std::memcmp(&expected_orientation, &ConsoleObject->orient, sizeof(expected_orientation)) && !std::memcmp(&expected_velocity, &ConsoleObject->mtype.phys_info.velocity, sizeof(expected_velocity)) && rng == expected_rng, "inert flags changed teleport transform or RNG");
		}
		Triggers[3] = leaf;
		require(fire(5) == 0 && fire(4) == 0 && Triggers[4].flags == 102, "new bits lost one-shot state");
		require(fire(5) == 0 && fire(4) == 1, "one-shot repeated");
		illusion(false);
		require(fire(6) == 0 && Triggers[6].type == TT_ILLUSION_ON && Triggers[6].flags == 80, "new bits lost alternate state");
		illusion(true);
		// Save a pending action and mutated byte flags, then restore through stock
		xl_wall_trigger_reset();
		require(check_trigger_sub(7, Player_num, 1) == 1, "delayed flag action did not arm");
		GameTime64 += F1_0 / 4;
		xl_trigger_state pending;
		require(xl_wall_trigger_state(7, &pending) && pending.delay_ms == 500, "new bits lost selected delay");
		char save[] = "xl-flags.sav", description[] = "XL cue flags";
		stop_time();
		require(state_save_all_sub(save, description), "flag save failed");
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "flag stock transition failed");
		trigger stock{};
		stock.type = TT_ILLUSION_OFF;
		stock.flags = 64;
		require(!xl_trigger_illusion_action(&stock) && xl_trigger_cue_enabled(&stock, SOUND_FORCEFIELD_OFF) && xl_wall_trigger(0, ConsoleObject - Objects, 0) == -1, "XL flag handling contaminated Redux alternate");
		require(state_restore_all_sub(save, 0), "flag restore failed");
		xl_trigger_state restored;
		require(xl_wall_trigger_state(7, &restored) && restored.elapsed == pending.elapsed && restored.delay_ms == 500 && Triggers[7].flags == 96 && Triggers[4].flags == 102 && Triggers[6].type == TT_ILLUSION_ON && Triggers[6].flags == 80, "disk lost flags, alternate or pending state");
		require(check_trigger_sub(6, Player_num, 1) == 0 && Triggers[6].type == TT_ILLUSION_OFF, "restored alternate did not continue");
		illusion(false);
		const auto cue = xl_trigger_cue_queries(SOUND_WALL_REMOVED, 1);
		GameTime64 += F1_0 / 4 - 1;
		triggers_frame_process();
		illusion(false);
		++GameTime64;
		triggers_frame_process();
		illusion(true);
		require(xl_trigger_cue_queries(SOUND_WALL_REMOVED, 1) == cue + 1, "restored delayed cue absent");
		// Actual robot death/removal dispatch, with the new flags on its owned action
		require(fire(5) == 0, "object cue reset failed");
		const int robot = 49;
		require(Objects[robot].type == OBJ_ROBOT && apply_damage_to_robot(&Objects[robot], Objects[robot].shields + F1_0, ConsoleObject - Objects), "object cue combat did not kill authored owner");
		for (int frame = 0; frame < 180; ++frame) {
			for (int i = 0; i <= Highest_object_index; ++i)
				if (Objects[i].type != OBJ_NONE && Objects[i].control_type == CT_EXPLOSION) {
					Objects[i].lifeleft -= F1_0 / 30;
					do_explosion_sequence(&Objects[i]);
				}
			obj_delete_all_that_should_be_dead();
		}
		illusion(true);
		bool found = false;
		for (int i = 0; i < xl_object_trigger_count(); ++i) {
			xl_object_trigger_state state;
			require(xl_object_trigger_state_get(i, &state), "object flag state missing");
			if (state.operated) {
				require(state.owner == -1 && state.flags == 102, "object removal lost byte flags or one-shot");
				found = true;
			}
		}
		require(found, "authored object removal did not operate");
		require(xl_trigger_cue_queries(SOUND_WALL_REMOVED, 1) == cue + 2, "authored removal did not reach the enabled cue");
		// Missing back wall and exterior child must never touch an invalid wall
		auto &front = Segments[Walls[2].segnum];
		const int side = Walls[2].sidenum, child = front.children[side];
		const int back = find_connect_side(&front, &Segments[child]);
		require(back >= 0, "illusion boundary missing");
		const int back_wall = Segments[child].sides[back].wall_num;
		Segments[child].sides[back].wall_num = -1;
		require(fire(5) == 0 && fire(3) == 0, "missing-back XL toggle failed");
		Segments[child].sides[back].wall_num = back_wall;
		front.children[side] = -1;
		require(fire(5) == 0 && fire(3) == 0, "exterior XL toggle failed");
		front.children[side] = child;
		std::cout << "XL trigger flag controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
