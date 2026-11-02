#include "../static_contact.h"

static void static_robot_native_controls(const std::filesystem::path &root, bool vertigo)
{
	const int id = vertigo ? 69 : 0;
	const auto original_definition = Robot_info[id];
	struct DefinitionGuard {
		int id;
		robot_info definition;
		~DefinitionGuard()
		{
			Robot_info[id] = definition;
		}
	} definition_guard{ id, original_definition };
	if (!vertigo) Robot_info[id].energy_blobs = 2;
	require(!Robot_info[id].attack_type && Robot_info[id].energy_blobs, "Static control needs a non-melee energy-blob robot");
	int room = -1;
	vms_vector center{};
	for (int segment = 0; segment < Num_segments; ++segment) {
		compute_segment_center(&center, &Segments[segment]);
		if (Segments[segment].objects < 0 && !get_seg_masks(&center, segment, 10 * F1_0, __FILE__, __LINE__).facemask) {
			room = segment;
			break;
		}
	}
	require(room >= 0, "Static contact needs an empty original room");
	obj_relink(int(ConsoleObject - Objects), room);
	ConsoleObject->pos = center;
	const int original_difficulty = Difficulty_level;
	Difficulty_level = NDL - 1;
	FrameTime = F1_0 / 8;
	for (bool stationary : { false, true }) {
		std::map<int, int> original;
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type != OBJ_NONE) original.emplace(i, Objects[i].signature);
		const int owner = projectile_robot();
		auto &robot = Objects[owner];
		robot.id = id;
		robot.rtype.pobj_info.model_num = Robot_info[id].model_num;
		robot.mtype.phys_info.mass = Robot_info[id].mass;
		robot.shields = 1000 * F1_0;
		init_ai_object(owner, stationary ? 135 : AIB_NORMAL, -1);
		const int shot = projectile_test_shot(int(ConsoleObject - Objects), LASER_ID_L1);
		Objects[shot].shields = 100 * F1_0;
		unsigned children_before = 0;
		for (int i = 0; i <= Highest_object_index; ++i) children_before += Objects[i].type == OBJ_WEAPON && Objects[i].id == ROBOT_SMART_HOMING_ID;
		collide_two_objects(&robot, &Objects[shot], &robot.pos);
		unsigned children_after = 0;
		for (int i = 0; i <= Highest_object_index; ++i) children_after += Objects[i].type == OBJ_WEAPON && Objects[i].id == ROBOT_SMART_HOMING_ID;
		require(stationary ? children_after == children_before : children_after > children_before,
		        "Static energy-blob suppression changed the normal robot control");
		require(robot.shields == 900 * F1_0 && robot.movement_type == (stationary ? MT_NONE : MT_PHYSICS),
		        "Static energy contact lost ordinary damage or owner immobility");
		if (stationary) {
			const int beam = projectile_test_shot(int(ConsoleObject - Objects), OMEGA_ID);
			auto &omega = Objects[beam];
			require(omega.movement_type == MT_NONE && !omega.mtype.phys_info.mass, "Production Omega fixture is physical");
			omega.mtype.phys_info.velocity = { 100001, -65537, 17 };
			collide_two_objects(&robot, &omega, &robot.pos);
			require(omega.mtype.phys_info.velocity.x == 100001 && omega.mtype.phys_info.velocity.y == -65537 &&
			            omega.mtype.phys_info.velocity.z == 17 && robot.movement_type == MT_NONE,
			        "Nonphysical Omega changed static contact motion");
		}
		for (int i = Highest_object_index; i >= 0; --i)
			if (Objects[i].type != OBJ_NONE && (!original.count(i) || original.at(i) != Objects[i].signature)) obj_delete(i);
	}
	Difficulty_level = original_difficulty;
	// Derived fixture supplies an authored static slot for the source omitted-payload restore
	write_object_event_fixture(root / "missions");
	std::ifstream input(root / "missions/events.hog", std::ios::binary);
	xl_compat::Bytes encoded{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() }, packed{ 'D', 'H', 'F' };
	int owner = -1;
	for (auto member : xl_compat::read_hog(encoded)) {
		if (member.name == "neptunxl.rl2") {
			const auto level = xl_compat::inspect_level(member.bytes);
			for (unsigned i = 0; i < level.objects.size(); ++i) {
				const auto &authored = level.objects[i];
				if (authored.type != OBJ_ROBOT || authored.control != CT_AI || authored.movement != MT_PHYSICS || authored.render != RT_POLYOBJ) continue;
				owner = int(i);
				const auto patch = [&](std::size_t offset, int value, int width) {
					for (int byte = 0; byte < width; ++byte) member.bytes.at(offset + byte) = static_cast<unsigned char>(std::uint32_t(value) >> (byte * 8));
				};
				require(level.game_version == 40 && level.level_version >= 21, "Static fixture requires audited modern layout");
				patch(authored.source_offset + 1, id, 1);
				patch(authored.source_offset + 7, room, 2);
				patch(authored.source_offset + 104, Robot_info[id].mass, 4);
				patch(authored.source_end - 92, 135, 1);
				for (int axis = 0; axis < 3; ++axis) {
					const int value = axis == 0 ? center.x : axis == 1 ? center.y
					                                                   : center.z;
					patch(authored.source_offset + 9 + axis * 4, value, 4);
					patch(authored.source_offset + 65 + axis * 4, value, 4);
				}
				const auto checked = xl_compat::inspect_level(member.bytes);
				require(checked.objects[i].control_data[0] == 135 && checked.objects[i].id == id, "Static fixture patch lost its authored control");
				break;
			}
		}
		xl_test::hog_member(packed, member.name, member.bytes, false);
	}
	require(owner >= 0, "Static fixture lacks an authored robot slot");
	write_bytes((root / "missions/stcont.hog").string().c_str(), packed);
	{
		std::ofstream descriptor(root / "missions/stcont.mn2");
		descriptor << "d2x-name = Derived static contacts\nnum_levels = 1\nneptunxl.rl2\n";
	}
	char fixture_error[256]{};
	require(load_base_mission("stcont", fixture_error, sizeof(fixture_error)), fixture_error);
	StartNewGame(1);
	Game_suspended = SUSP_ROBOTS;
	obj_relink(int(ConsoleObject - Objects), room);

	auto &robot = Objects[owner];

	require(robot.ctype.ai_info.behavior == 135 && robot.control_type == CT_NONE && robot.movement_type == MT_NONE, "Authored static initialization failed");
	ConsoleObject->pos = center;
	ConsoleObject->pos.x -= 7 * F1_0;
	ConsoleObject->last_pos = ConsoleObject->pos;
	ConsoleObject->mtype.phys_info.velocity = { 32 * F1_0, 0, 0 };
	ConsoleObject->mtype.phys_info.rotvel = {};
	ConsoleObject->mtype.phys_info.drag = 0;
	FrameTime = F1_0 / 4;
	const auto before = *ConsoleObject;
	const int count = num_objects, awareness = Num_awareness_events;
	const int shield = Players[Player_num].shields;
	const auto expected = xl_compat::static_contact({ robot.pos.x, robot.pos.y, robot.pos.z }, robot.mtype.phys_info.mass,
	                                                { before.pos.x, before.pos.y, before.pos.z }, { 32 * F1_0, 0, 0 }, before.mtype.phys_info.mass,
	                                                true, false, false, xl_compat::FixedLightPoint{});
	collide_two_objects(&robot, ConsoleObject, &robot.pos);
	require(num_objects == count && Num_awareness_events == awareness && Players[Player_num].shields == shield,
	        "Static player contact created impact effects, awareness or damage");
	require(ConsoleObject->mtype.phys_info.velocity.x == expected.velocity[0] && ConsoleObject->mtype.phys_info.velocity.y == expected.velocity[1] &&
	            ConsoleObject->mtype.phys_info.velocity.z == expected.velocity[2] && robot.movement_type == MT_NONE,
	        "Native static player bump differs from qualified force composition");
	*ConsoleObject = before;
	const auto ticks = SDL_GetTicks();
	if (ticks < 500) SDL_Delay(500 - ticks);
	xl_camera_controller_begin_frame();
	vms_vector end = center, hit{}, normal{};
	fvi_query query{};
	query.p0 = &ConsoleObject->pos;
	query.p1 = &end;
	query.startseg = room;
	query.rad = ConsoleObject->size;
	query.thisobjnum = int(ConsoleObject - Objects);
	query.flags = FQ_CHECK_OBJS;
	fvi_info info{};
	require(find_vector_intersection(&query, &info) == HIT_OBJECT && info.hit_object == owner,
	        "Native FVI failed to hit a static robot");
	const int distance = xl_static_robot_query(&robot, ConsoleObject, query.p0, query.p1, query.rad, &hit, &normal);
	require(distance && hit.x == info.hit_pnt.x && hit.y == info.hit_pnt.y && hit.z == info.hit_pnt.z && !normal.x && !normal.y && !normal.z,
	        "Static FVI lost source sphere contact coordinates");
	xl_compat::CameraHitState robot_hit, player_hit;
	require(xl_camera_hit_state_get(owner, robot_hit) && robot_hit.repaired == int32_t(xl_camera_controller_ticks()),
	        "Static sphere query did not register its target hit");
	do_physics_sim(ConsoleObject);
	require(xl_camera_hit_state_get(int(ConsoleObject - Objects), player_hit) && player_hit.repaired == int32_t(xl_camera_controller_ticks()) &&
	            robot.control_type == CT_NONE && robot.movement_type == MT_NONE,
	        "Static physical contact lost reverse-query hit state or moved the display");
	char save[] = "static-contact.sav", description[] = "Static contacts", error[256]{};
	const int signature = robot.signature;
	stop_time();
	require(state_save_all_sub(save, description), "Cannot save earned static contact state");
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_static_robot(&Objects[owner]),
	        "Static profile leaked into stock");
	require(state_restore_all_sub(save, 0) && Objects[owner].signature == signature && xl_static_robot(&Objects[owner]) &&
	            Objects[owner].control_type == CT_NONE && Objects[owner].movement_type == MT_NONE && Objects[owner].mtype.phys_info.mass == Robot_info[id].mass && xl_camera_hit_state_get(owner, robot_hit),
	        "Full disk restore lost static identity, motion or earned hit state");
	std::cout << "Native static contacts: original=" << vertigo << "; energy-blob control, player effects, sphere FVI, reverse repair and full disk restore passed\n";
}
