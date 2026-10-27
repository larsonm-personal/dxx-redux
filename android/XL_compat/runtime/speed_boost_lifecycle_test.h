#include "speed_boost_fixture.h"
static std::vector<xl_speed_boost_state> capture_boosts()
{
	std::vector<xl_speed_boost_state> result(xl_speed_boost_state_count());
	for (std::size_t row = 0; row < result.size(); ++row) require(xl_speed_boost_state_get(static_cast<int>(row), &result[row]), "boost capture failed");
	return result;
}
static bool same_boosts(const std::vector<xl_speed_boost_state> &a, const std::vector<xl_speed_boost_state> &b)
{
	return a.size() == b.size() && (a.empty() || !std::memcmp(a.data(), b.data(), a.size() * sizeof(a[0])));
}
static void speed_boost_lifecycle_test(const std::filesystem::path &root)
{
	char error[256]{};
	write_speed_boost_fixture(root);
	require(load_base_mission("boost", error, sizeof(error)), error);
	require(xl_level_supported("neptunxl.rl2", 0), xl_level_rejection());
	require(!xl_level_supported("neptunxl.rl2", 1), "boost fixture admitted multiplayer");
	StartNewGame(1);
	std::ifstream manifest_file(root / "boost-fixture.json");
	const auto manifest = nlohmann::json::parse(manifest_file);
	const int area = manifest.at("area"), side = manifest.at("side"), child = manifest.at("child"), robot = manifest.at("robot");
	const int player = ConsoleObject - Objects;
	require(xl_speed_boost_segment(area) && !xl_speed_boost_segment(child) && Segment2s[area].special == 0, "boost function leaked into stock segment enum");
	require(Triggers[0].type == 15 && xl_level_trigger_state_required(), "authored boost type or persistence requirement missing");
	xl_object_trigger_frame();
	require(xl_speed_boost_active(robot) == 1 && !xl_speed_boost_active(player), "object autoplay boost selected the wrong actor");
	require(check_trigger_sub(0, Player_num, 1) == 0 && xl_speed_boost_active(player) == 1 && xl_speed_boost_player_locked(), "authored player boost dispatch failed");
	require(check_trigger_sub(0, Player_num, 1) == 0, "speed boost incorrectly used ordinary trigger debounce");
	FrameTime = 1000;
	xl_speed_boost_frame();
	const auto earned = capture_boosts();
	require(earned.size() == 2 && earned[0].owner == player && earned[1].owner == robot && earned[0].steps == 9 && earned[0].phase == 40000, "actors lost independent orientation phase");
	require(xl_wall_trigger(5, player, 0) == 1, "delayed boost ran immediately");
	char save[] = "boost-earned.sav", description[] = "Earned XL speed boosts";
	stop_time();
	require(state_save_all_sub(save, description), "boost disk save failed");
	const auto velocity = ConsoleObject->mtype.phys_info.velocity;
	const auto orientation = ConsoleObject->orient;
	// Validate the complete trailer before publishing any event or boost state
	auto *file = PHYSFS_openWrite("boost-state.bin");
	require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "boost trailer write failed");
	const auto user = root.parent_path() / "preview-user";
	std::ifstream encoded_file(user / "boost-state.bin", std::ios::binary);
	const xl_compat::Bytes encoded{ std::istreambuf_iterator<char>(encoded_file), std::istreambuf_iterator<char>() };
	require(encoded.size() > 16, "missing boost trailer");
	for (int variant = 0; variant < 15; ++variant) {
		auto block = nlohmann::json::parse(encoded.begin() + 16, encoded.end());
		block["states"][5][0] = 123;
		auto &rows = block["boosts"]["states"];
		if (variant == 0) block.erase("boosts");
		if (variant == 1) block["boosts"]["speed"] = 11;
		if (variant == 2) rows[0][0] = MAX_OBJECTS;
		if (variant == 3) rows[0][1] = earned[0].signature + 1;
		if (variant == 4) rows[0][2] = 2;
		if (variant == 5) rows[0][3] = 0;
		if (variant == 6) rows[0][4] = 10;
		if (variant == 7) rows[0][5] = F1_0;
		if (variant == 8) rows[0][6] = INT32_MAX;
		if (variant == 9) rows[0][9] = 0;
		if (variant == 10) rows[1] = rows[0];
		if (variant == 11) rows[0][1] = UINT64_MAX;
		if (variant == 12) rows[0][2] = nlohmann::json::parse("true");
		if (variant == 13) block["schema"] = 6;
		if (variant == 14) rows[0][18] = INT32_MAX;
		const auto text = block.dump();
		xl_compat::Bytes bad(encoded.begin(), encoded.begin() + 16);
		bad.insert(bad.end(), text.begin(), text.end());
		std::uint32_t checksum = 2166136261u;
		for (unsigned char byte : text) checksum = (checksum ^ byte) * 16777619u;
		for (int byte = 0; byte < 4; ++byte) {
			bad[8 + byte] = static_cast<unsigned char>(text.size() >> (byte * 8));
			bad[12 + byte] = static_cast<unsigned char>(checksum >> (byte * 8));
		}
		write_bytes((user / "boost-corrupt.bin").string().c_str(), bad);
		file = PHYSFS_openRead("boost-corrupt.bin");
		xl_trigger_state pending;
		require(file && !xl_wall_trigger_read(file) && PHYSFS_close(file) && same_boosts(earned, capture_boosts()) && xl_wall_trigger_state(5, &pending) && pending.elapsed == 0, "malformed boost trailer partially published");
	}
	require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "boost stock recovery failed");
	require(!xl_speed_boost_state_count() && !xl_speed_boost_player_locked() && !xl_speed_boost_drag(ConsoleObject - Objects, F1_0), "XL boost leaked into stock runtime");
	const auto stock_velocity = ConsoleObject->mtype.phys_info.velocity;
	xl_speed_boost_stop(ConsoleObject - Objects);
	require(!std::memcmp(&stock_velocity, &ConsoleObject->mtype.phys_info.velocity, sizeof(stock_velocity)), "XL stop modified Redux velocity");
	require(state_restore_all_sub(save, 0) && same_boosts(earned, capture_boosts()) && xl_speed_boost_segment(area), "disk restore lost boosts, phase or source function");
	require(!std::memcmp(&velocity, &ConsoleObject->mtype.phys_info.velocity, sizeof(velocity)) && !std::memcmp(&orientation, &ConsoleObject->orient, sizeof(orientation)), "disk restore changed earned motion");
	// Round-trip the same trailer after changing all live boost state
	xl_speed_boost_clear();
	file = PHYSFS_openRead("boost-state.bin");
	require(file && xl_wall_trigger_read(file) && PHYSFS_close(file) && same_boosts(earned, capture_boosts()), "boost trailer restore failed");
	FrameTime = 639;
	xl_speed_boost_frame();
	require(capture_boosts()[0].steps == 8, "saved orientation phase missed next tick");
	// Positive boost suppresses forces; explicit release restores ordinary force handling
	vms_vector force{ i2f(5), i2f(3), -i2f(2) };
	const auto before_force = ConsoleObject->mtype.phys_info.velocity;
	phys_apply_force(ConsoleObject, &force);
	require(!std::memcmp(&before_force, &ConsoleObject->mtype.phys_info.velocity, sizeof(before_force)), "positive player boost accepted linear force");
	require(check_trigger_sub(1, Player_num, 0) == 0 && !xl_speed_boost_player_locked(), "authored zero-speed release failed");
	const auto released = ConsoleObject->mtype.phys_info.velocity;
	require(std::abs(vm_vec_mag(&released) - i2f(60)) < 200, "release did not normalize velocity to 60");
	phys_apply_force(ConsoleObject, &force);
	require(std::memcmp(&released, &ConsoleObject->mtype.phys_info.velocity, sizeof(released)), "release retained positive-boost force suppression");
	// Actual no-thrust physics retains each actor's earned velocity rather than damping it
	require(state_restore_all_sub(save, 0), "motion setup restore failed");
	for (int actor : { player, robot }) {
		int empty = -1;
		for (int segment = 0; segment < Num_segments; ++segment)
			if (Segments[segment].objects < 0) {
				empty = segment;
				break;
			}
		require(empty >= 0, "motion control has no empty segment");
		auto &object = Objects[actor];
		obj_relink(actor, empty);
		compute_segment_center(&object.pos, &Segments[empty]);
		object.mtype.phys_info.thrust = {};
		object.mtype.phys_info.rotvel = {};
		object.mtype.phys_info.flags = PF_USES_THRUST;
		object.mtype.phys_info.mass = i2f(4);
		object.mtype.phys_info.drag = 1000;
		const auto state = capture_boosts()[actor == player ? 0 : 1];
		const vms_vector expected_velocity{ state.velocity[0], state.velocity[1], state.velocity[2] };
		FrameTime = F1_0 / 1000;
		for (int frame = 0; frame < 5; ++frame) {
			vms_vector displacement, expected;
			vm_vec_copy_scale(&displacement, &expected_velocity, FrameTime);
			vm_vec_add(&expected, &object.pos, &displacement);
			do_physics_sim(&object);
			require(!std::memcmp(&object.pos, &expected, sizeof(expected)) && !std::memcmp(&object.mtype.phys_info.velocity, &expected_velocity, sizeof(expected_velocity)), "real no-thrust boost physics damped or used another actor's velocity");
		}
	}
	// Drive an actual player/robot collision through FVI and the physics dispatcher
	require(state_restore_all_sub(save, 0), "collision setup restore failed");
	int collision_segment = -1;
	for (int segment = 0; segment < Num_segments; ++segment)
		if (Segments[segment].objects < 0) {
			collision_segment = segment;
			break;
		}
	require(collision_segment >= 0, "collision control has no empty segment");
	obj_relink(player, collision_segment);
	obj_relink(robot, collision_segment);
	compute_segment_center(&ConsoleObject->pos, &Segments[collision_segment]);
	ConsoleObject->size = F1_0 / 4;
	ConsoleObject->mtype.phys_info.thrust = {};
	ConsoleObject->mtype.phys_info.rotvel = {};
	ConsoleObject->mtype.phys_info.flags = PF_USES_THRUST;
	ConsoleObject->mtype.phys_info.mass = i2f(4);
	const auto collision_state = capture_boosts()[0];
	const vms_vector collision_velocity{ collision_state.velocity[0], collision_state.velocity[1], collision_state.velocity[2] };
	ConsoleObject->mtype.phys_info.velocity = collision_velocity;
	auto direction = collision_velocity;
	vm_vec_normalize(&direction);
	auto &obstacle = Objects[robot];
	obstacle.id = 0;
	obstacle.size = F1_0 / 4;
	obstacle.shields = i2f(10000);
	obstacle.control_type = CT_NONE;
	obstacle.movement_type = MT_PHYSICS;
	obstacle.mtype.phys_info = {};
	obstacle.mtype.phys_info.mass = i2f(4);
	vm_vec_scale_add(&obstacle.pos, &ConsoleObject->pos, &direction, F1_0 + F1_0 / 2);
	FrameTime = F1_0 / 100;
	do_physics_sim(ConsoleObject);
	require(!std::memcmp(&collision_velocity, &ConsoleObject->mtype.phys_info.velocity, sizeof(collision_velocity)) && vm_vec_mag(&obstacle.mtype.phys_info.velocity) > 0, "boost collision failed to move robot while preserving player velocity");
	// A live object-master descendant retains its initiating nonplayer through delay
	require(state_restore_all_sub(save, 0), "live master setup restore failed");
	xl_wall_trigger_reset();
	trigger master{};
	master.type = 31;
	master.num_links = 1;
	master.seg[0] = Walls[5].segnum;
	master.side[0] = Walls[5].sidenum;
	require(!xl_object_master_action(&master, robot), "live boost master dispatch failed");
	xl_trigger_state pending_boost;
	require(xl_wall_trigger_state(5, &pending_boost) && pending_boost.attached && pending_boost.object == robot, "boost master dropped pending actor context");
	GameTime64 += F1_0;
	FrameTime = 0;
	triggers_frame_process();
	const auto completed = capture_boosts();
	require(completed.size() == 2 && completed[0].speed == 5 && completed[1].speed == 6, "delayed attached boost changed the player instead of its initiating actor");
	// Real crossings exercise the authored dispatch and the actor-aware area boundary
	require(state_restore_all_sub(save, 0), "boundary setup restore failed");
	vms_vector portal, inside, outside;
	compute_center_point_on_side(&portal, &Segments[area], side);
	compute_segment_center(&inside, &Segments[area]);
	compute_segment_center(&outside, &Segments[child]);
	for (auto *point : { &inside, &outside }) {
		vm_vec_sub(point, point, &portal);
		vm_vec_scale(point, F1_0 / 4);
		vm_vec_add2(point, &portal);
	}
	const auto pose = [&] {
		obj_relink(player, area);
		ConsoleObject->pos = inside;
		ConsoleObject->control_type = CT_NONE;
		ConsoleObject->movement_type = MT_PHYSICS;
		ConsoleObject->mtype.phys_info = {};
		ConsoleObject->mtype.phys_info.mass = F1_0;
		vm_vec_sub(&ConsoleObject->mtype.phys_info.velocity, &outside, &inside);
		vm_vec_scale(&ConsoleObject->mtype.phys_info.velocity, i2f(4));
		FrameTime = F1_0 / 4;
	};
	xl_speed_boost_clear();
	pose();
	object_move_one(ConsoleObject);
	require(ConsoleObject->segnum == child && xl_speed_boost_active(player) == 1, "real authored wall crossing did not activate boost");
	Walls[0].trigger = 255;
	pose();
	do_physics_sim(ConsoleObject);
	require(ConsoleObject->segnum == area && xl_speed_boost_active(player) == 1, "positive boost escaped restricted area");
	Walls[0].trigger = 1;
	pose();
	object_move_one(ConsoleObject);
	require(ConsoleObject->segnum == child && !xl_speed_boost_active(player), "authored release wall failed to permit escape and stop boost");
	Walls[0].trigger = 0;
	// Removed actors cannot lend their earned boost or delayed actions to reused slots
	require(state_restore_all_sub(save, 0), "actor reuse setup restore failed");
	xl_wall_trigger_reset();
	require(xl_wall_trigger(5, robot, 0) == 1, "robot delayed boost did not arm");
	const int segment = Objects[robot].segnum;
	const auto position = Objects[robot].pos;
	const int signature = Objects[robot].signature;
	obj_delete(robot);
	const int replacement = obj_create(OBJ_POWERUP, POW_ENERGY, segment, &position, &vmd_identity_matrix, F1_0, CT_POWERUP, MT_PHYSICS, RT_NONE);
	require(replacement == robot && Objects[replacement].signature != signature && !xl_speed_boost_active(replacement), "recycled actor inherited boost");
	GameTime64 += F1_0;
	FrameTime = 0;
	triggers_frame_process();
	require(!xl_speed_boost_active(replacement), "delayed boost targeted recycled actor");
	require(state_restore_all_sub(save, 0), "death setup restore failed");
	Player_is_dead = 1;
	xl_speed_boost_frame();
	require(!xl_speed_boost_active(player) && xl_speed_boost_active(robot), "player death cleared wrong owner's boost");
	Player_is_dead = 0;
	require(state_restore_all_sub(save, 0), "exit setup restore failed");
	xl_wall_trigger_exit();
	require(!xl_speed_boost_active(player) && xl_speed_boost_active(robot), "level exit failed to stop the player boost independently");
	require(state_restore_all_sub(save, 0), "final boost restore failed");
	// Object teleports preserve the owner's boost; wall teleports supersede it
	trigger teleport{};
	teleport.type = 14;
	teleport.num_links = 1;
	teleport.seg[0] = 70;
	teleport.side[0] = 0;
	int selected = -1;
	const auto before_transport = capture_boosts();
	const auto robot_velocity = Objects[robot].mtype.phys_info.velocity;
	require(!xl_object_teleport_action(&teleport, robot, &selected) && Objects[robot].segnum == 70 && same_boosts(before_transport, capture_boosts()) && !std::memcmp(&robot_velocity, &Objects[robot].mtype.phys_info.velocity, sizeof(robot_velocity)), "object teleport lost earned boost motion");
	const auto old_action = Triggers[0];
	Triggers[0] = teleport;
	require(!xl_wall_teleport_action(0, player, 0) && !xl_speed_boost_active(player) && capture_boosts().size() == 1 && xl_speed_boost_active(robot), "wall teleport retained superseded turn or cleared another actor");
	Triggers[0] = old_action;
	require(state_restore_all_sub(save, 0), "stationary owner setup restore failed");
	Objects[robot].movement_type = MT_NONE;
	Objects[robot].control_type = CT_NONE;
	const auto stationary_position = Objects[robot].pos;
	trigger stationary = Triggers[0];
	stationary.value = 7;
	require(!xl_speed_boost_action(&stationary, -1, robot, 0), "stationary owner boost rejected");
	FrameTime = F1_0 / 40 + 1;
	xl_speed_boost_frame();
	object_move_one(&Objects[robot]);
	require(capture_boosts()[1].steps == 8 && !std::memcmp(&stationary_position, &Objects[robot].pos, sizeof(stationary_position)), "stationary owner failed to turn independently or acquired translation");
	// Negative authored values share the previous speed across owners and level resets
	stationary.value = -1;
	require(!xl_speed_boost_action(&stationary, -1, player, 0) && capture_boosts()[0].speed == 7, "negative boost value lost cross-owner previous speed");
	xl_speed_boost_clear();
	require(!xl_speed_boost_action(&stationary, -1, player, 0) && capture_boosts()[0].speed == 7, "level reset lost the source's shared previous speed");
	require(state_restore_all_sub(save, 0) && xl_speed_boost_last_speed() == 5, "disk restore did not recover the saved previous speed");
	std::cout << "XL boost lifecycle: authored crossing, area restriction/release, object autoplay, actor identity, disk/phase, atomic malformed trailers and Redux isolation passed\n";
}
