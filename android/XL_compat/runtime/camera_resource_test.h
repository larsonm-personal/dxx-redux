#include "../camera_contact.h"
#include "../explosion_motion.h"
extern "C" {
#include "collide.h"
#include "automap.h"
}
static void camera_player_contact_controls(object &camera)
{
	const auto saved_player = Objects[1];
	auto *saved_console = ConsoleObject;
	const int saved_map = Automap_active, saved_awareness = Num_awareness_events;
	const auto saved_flags = camera.flags;
	const auto camera_before = camera;
	auto &player = Objects[1];
	player.type = OBJ_PLAYER;
	player.id = Player_num;
	player.movement_type = MT_PHYSICS;
	player.flags = 0;
	player.pos = camera.pos;
	player.segnum = camera.segnum;
	player.shields = 37 * F1_0;
	ConsoleObject = &player;
	unsigned cases = 0;
	for (bool reverse : { false, true })
		for (bool map : { false, true })
			for (bool exploding : { false, true }) {
				camera.flags = exploding ? OF_EXPLODING : 0;
				Automap_active = map;
				Num_awareness_events = 0;
				player.mtype.phys_info.mass = 98317;
				player.mtype.phys_info.velocity = { 100001, -65537, 17 };
				const auto expected = xl_compat::camera_reflect_velocity({ 100001, -65537, 17 }, 98317, map || exploding);
				std::array<int, MAX_OBJECTS> signatures{};
				for (int i = 0; i <= Highest_object_index; ++i) signatures[i] = Objects[i].type == OBJ_NONE ? -1 : Objects[i].signature;
				const int old_highest = Highest_object_index;
				collide_two_objects(reverse ? &player : &camera, reverse ? &camera : &player, &player.pos);
				unsigned flashes = 0;
				for (int i = 0; i <= Highest_object_index; ++i) {
					if (Objects[i].type == OBJ_NONE || (i <= old_highest && signatures[i] == Objects[i].signature)) continue;
					require(Objects[i].type == OBJ_FIREBALL && Objects[i].id == Weapon_info[0].wall_hit_vclip &&
					            Objects[i].size == Weapon_info[0].impact_size,
					        "Camera contact created the wrong impact flash");
					auto &effect = Objects[i];
					const auto origin = effect.pos;
					const auto duration = effect.lifeleft;
					auto *saved_viewer = Viewer;
					auto viewer = player;
					viewer.pos.x += 10 * F1_0;
					Viewer = &viewer;
					effect.lifeleft /= 2;
					const auto moved = xl_level_extensions_active() ? xl_compat::explosion_motion({ origin.x, origin.y, origin.z }, { viewer.pos.x, viewer.pos.y, viewer.pos.z },
					                                                                              2 * effect.size, duration, effect.lifeleft)
					                                                : xl_compat::FixedLightPoint{ origin.x, origin.y, origin.z };
					do_explosion_sequence(&effect);
					Viewer = saved_viewer;
					require(effect.pos.x == moved[0] && effect.pos.y == moved[1] && effect.pos.z == moved[2],
					        "Camera player-contact flash motion or stock isolation differs");
					++flashes;
					obj_delete(i);
				}
				require(flashes == unsigned(!exploding) && Num_awareness_events == int(!exploding), "Camera contact flash/awareness gate differs");
				if (!exploding) require(Awareness_events[0].type == PA_PLAYER_COLLISION && Awareness_events[0].segnum == player.segnum,
					                    "Camera contact awareness source differs");
				require(player.mtype.phys_info.velocity.x == expected[0] && player.mtype.phys_info.velocity.y == expected[1] &&
				            player.mtype.phys_info.velocity.z == expected[2] && player.shields == 37 * F1_0,
				        "Camera contact reflection or player damage differs");
				require(camera.shields == camera_before.shields && camera.type == 16 && camera.control_type == 18 &&
				            !std::memcmp(&camera.pos, &camera_before.pos, sizeof(camera.pos)) &&
				            !std::memcmp(&camera.orient, &camera_before.orient, sizeof(camera.orient)),
				        "Camera contact mutated camera pose or health");
				++cases;
			}
	camera.flags = saved_flags;
	Objects[1] = saved_player;
	ConsoleObject = saved_console;
	Automap_active = saved_map;
	Num_awareness_events = saved_awareness;
	std::cout << "Native camera/player contact: " << cases << " ordered dispatch, flash, awareness, reflection, automap, exploding and health controls passed, xl_motion=" << xl_level_extensions_active() << "\n";
}

static void camera_resource_controls(const std::filesystem::path &root)
{
	if (!std::filesystem::exists(root / "data/d2x-xl.hog")) return;
	char error[256]{};
	require(load_base_mission("hxmtest", error, sizeof(error)), error);
	const int robots = N_robot_types, models = N_polygon_models, joints = N_robot_joints;
	const auto before = gameplay_definitions();
	// Appended death-model entries legitimately differ from unused stock table slots
	const auto stock_model = hxm_model_receipt(0, models);
	std::ifstream archive(root / "data/d2x-xl.hog", std::ios::binary);
	xl_compat::Bytes hog{ std::istreambuf_iterator<char>(archive), std::istreambuf_iterator<char>() }, original;
	for (const auto &member : xl_compat::read_hog(hog))
		if (member.name == "cambot.hxm") original = member.bytes;
	require(original.size() == 3994, "Original camera HXM size differs");
	const auto loose = root / "cambot.hxm";
	for (int variant = 0; variant < 5; ++variant) {
		auto bad = original;
		if (variant == 0) bad.resize(1200);
		if (variant == 1) bad[8] = 2;
		if (variant == 2) bad[16 + 118] = 9;
		if (variant == 3) bad[12] = 255;
		if (variant == 4) bad.back() = 1;
		write_bytes(loose.string().c_str(), bad);
		XlPreparedResources rejected("__camera__.rl2");
		bool failed = false;
		try {
			rejected.prepare_camera_model();
		} catch (const std::runtime_error &) {
			failed = true;
		}
		require(failed && rejected.camera_model_id() == -1 && rejected.camera_robot_id() == -1, "Malformed camera addon changed staged ownership");
		require(N_robot_types == robots && N_polygon_models == models && N_robot_joints == joints && gameplay_definitions() == before && hxm_model_receipt(0, models) == stock_model,
		        "Rejected camera addon mutated live definitions");
		require(!PHYSFS_getMountPoint("__xl_camera_resources__.hog"), "Rejected camera addon leaked private mount");
	}
	std::filesystem::remove(loose);
	const auto shadow = root / "camera-unowned";
	std::filesystem::create_directories(shadow);
	write_bytes((shadow / "cambot.hxm").string().c_str(), original);
	require(PHYSFS_addToSearchPath(shadow.string().c_str(), 0), "Cannot mount private camera ownership control");
	bool shadow_rejected = false;
	try {
		XlPreparedResources unowned("__camera__.rl2");
		unowned.prepare_camera_model();
	} catch (const std::runtime_error &error) {
		shadow_rejected = std::strstr(error.what(), "shadowed") != nullptr;
	}
	require(PHYSFS_removeFromSearchPath(shadow.string().c_str()), "Cannot unmount private camera ownership control");
	require(shadow_rejected && gameplay_definitions() == before, "Unowned camera shadow was accepted or changed live definitions");
	xl_compat::Bytes installed;
	for (int cycle = 0; cycle < 3; ++cycle) {
		auto prepared = std::make_unique<XlPreparedResources>("__camera__.rl2");
		prepared->prepare_camera_model();
		prepared->prepare_camera_model();
		require(prepared->camera_robot_id() == robots && prepared->camera_model_id() == models, "Camera addon replaced serialized slots instead of appending");
		const auto &graph = prepared->dependencies();
		require(graph.robot_count == robots + 1 && graph.model_count == models + 1 && graph.joint_count == joints &&
		            graph.model_data[models].rad == 116018 && graph.models.count(models),
		        "Camera staged dependency counts, radius or model graph differ");
		require(!PHYSFS_getMountPoint("__xl_camera_resources__.hog"), "Camera preparation leaked private archive mount");
		if (!cycle) require(gameplay_definitions() == before && N_polygon_models == models, "Camera preflight published definitions");
		// The normal native level path resets prior definitions after preflight
		reset_level_robots_file();
		require(N_robot_types == robots && N_polygon_models == models && gameplay_definitions() == before, "Native camera reset did not restore base tables");
		auto *active = prepared.get();
		xl_set_prepared_resources("__camera__.rl2", std::move(prepared));
		require(xl_install_prepared_hxm("__camera__.rl2"), "Camera addon installation failed");
		require(N_robot_types == robots + 1 && N_polygon_models == models + 1 && N_robot_joints == joints, "Camera installation counts differ");
		const auto &robot = Robot_info[robots];
		require(robot.model_num == models && robot.n_guns == 2 && robot.strength == -1 && robot.mass == F1_0 / 2 && robot.drag == 0 && robot.behavior == 128 && robot.aim == 0 &&
		            !robot.contains_count && !robot.score_value && !robot.attack_type && !robot.see_sound && !robot.attack_sound && !robot.claw_sound && !robot.taunt_sound,
		        "Camera normalized robot definition differs");
		const auto receipt = hxm_model_receipt(models, models + 1);
		if (!cycle) installed = receipt;
		const auto stock_after = hxm_model_receipt(0, models);
		const auto diagnose = [&](const char *name, const xl_compat::Bytes &expected, const xl_compat::Bytes &actual) {
			if (expected == actual) return;
			std::size_t at = 0;
			while (at < expected.size() && at < actual.size() && expected[at] == actual[at]) ++at;
			std::cerr << "Camera resource receipt " << name << " cycle=" << cycle << " sizes=" << expected.size() << '/' << actual.size() << " first_difference=" << at
			          << " expected=" << (at < expected.size() ? int(expected[at]) : -1) << " actual=" << (at < actual.size() ? int(actual[at]) : -1) << " header=" << sizeof(polymodel) << '\n';
		};
		diagnose("camera", installed, receipt);
		diagnose("stock", stock_model, stock_after);
		require(receipt == installed && stock_after == stock_model, "Camera repeated install changed original model or stock geometry");
		require(xl_install_prepared_hxm("__camera__.rl2") && hxm_model_receipt(models, models + 1) == installed, "Camera repeated native install lost owned model bytes");
		// Exercise the real native object layout without running an unqualified controller
		const auto saved_object = Objects[0];
		const auto saved_ai = Ai_local_info[0];
		auto &camera = Objects[0];
		camera.type = 16;
		camera.id = 0;
		camera.render_type = RT_POLYOBJ;
		camera.control_type = CT_AI;
		camera.movement_type = MT_PHYSICS;
		camera.rtype.pobj_info.model_num = 0;
		camera.size = 7;
		camera.lifeleft = 123;
		camera.ctype.ai_info.behavior = 128;
		Ai_local_info[0].mode = 9;
		require(Highest_object_index >= 1, "Camera controls require two native object slots");
		const auto saved_second = Objects[1];
		Objects[1].type = 16;
		Objects[1].render_type = RT_POLYOBJ;
		Objects[1].rtype.pobj_info.model_num = N_polygon_models;
		const auto before_invalid = camera;
		bool invalid_rejected = false;
		try {
			active->initialize_camera_objects();
		} catch (const std::runtime_error &) {
			invalid_rejected = true;
		}
		const bool unchanged = !std::memcmp(&camera, &before_invalid, sizeof(camera)) && Ai_local_info[0].mode == 9;
		Objects[1] = saved_second;
		if (!invalid_rejected || !unchanged) {
			Objects[0] = saved_object;
			Ai_local_info[0] = saved_ai;
			throw std::runtime_error("Invalid old camera model partially initialized native objects");
		}
		// Exercise the normal native resource reset/load/initialization hook
		load_level_robots_file("__camera__.rl2");
		const bool initialized = camera.type == 16 && camera.id == robots && camera.control_type == 18 && camera.movement_type == MT_NONE &&
		                         camera.render_type == RT_POLYOBJ && camera.rtype.pobj_info.model_num == models && camera.size == Polygon_models[0].rad &&
		                         camera.size != Polygon_models[models].rad && camera.lifeleft == IMMORTAL_TIME && Ai_local_info[0].mode == 0 &&
		                         camera.ctype.ai_info.behavior == 128 && !std::memcmp(&camera.pos, &saved_object.pos, sizeof(camera.pos)) &&
		                         !std::memcmp(&camera.orient, &saved_object.orient, sizeof(camera.orient)) && camera.signature == saved_object.signature;
		if (initialized) camera_player_contact_controls(camera);
		Objects[0] = saved_object;
		Ai_local_info[0] = saved_ai;
		require(initialized, "Native camera object initialization changed source size ordering, identity, pose or behavior");
	}
	require(load_base_mission("d2", error, sizeof(error)), error);
	require(N_robot_types == robots && N_polygon_models == models && N_robot_joints == joints && gameplay_definitions() == before && hxm_model_receipt(0, models) == stock_model,
	        "Camera addon leaked definitions into stock mission");
	std::cout << "Original camera HXM native append, staged dependencies, five malformed controls, unowned shadow, repeated installation/reset and stock isolation passed\n";
	std::cout << "Native camera object initialization: normal resource-load hook, invalid-model transaction, original type, prior model radius, camera control/model, immortal lifetime, retained pose/signature/AI behavior passed\n";
}
