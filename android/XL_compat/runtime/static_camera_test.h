#include "../static_contact.h"
static void static_camera_native_controls(object &camera)
{
	const auto saved_camera = camera;
	auto authored_camera = saved_camera;
	authored_camera.mtype.phys_info = {};
	const auto saved_player = *ConsoleObject;
	const auto saved_frame = FrameTime;
	const auto saved_map = Automap_active;
	const auto point = [](const vms_vector &v) { return xl_compat::FixedLightPoint{ v.x, v.y, v.z }; };
	unsigned players = 0, weapons = 0;
	for (bool reverse : { false, true })
		for (bool exploding : { false, true })
			for (bool map : { false, true }) {
				camera = authored_camera;
				camera.ctype.ai_info.behavior = 135;
				camera.flags = exploding ? OF_EXPLODING : 0;
				*ConsoleObject = saved_player;
				ConsoleObject->movement_type = MT_PHYSICS;
				ConsoleObject->mtype.phys_info.velocity = { 100001, -65537, 17 };
				ConsoleObject->mtype.phys_info.rotvel = {};
				Automap_active = map;
				Num_awareness_events = 0;
				const int count = num_objects;
				const auto expected = xl_compat::static_contact(point(camera.pos), camera.mtype.phys_info.mass,
				                                                point(ConsoleObject->pos), point(ConsoleObject->mtype.phys_info.velocity),
				                                                ConsoleObject->mtype.phys_info.mass, true, false, false, xl_compat::FixedLightPoint{});
				collide_two_objects(reverse ? ConsoleObject : &camera, reverse ? &camera : ConsoleObject, &camera.pos);
				require(num_objects == count && !Num_awareness_events && point(ConsoleObject->mtype.phys_info.velocity) == expected.velocity &&
				            ConsoleObject->shields == saved_player.shields && point(camera.pos) == point(saved_camera.pos),
				        "Static camera player contact lost source force/effect gates");
				++players;
			}
	Automap_active = 0;
	FrameTime = 1092;
	for (int id : { LASER_ID_L1, LASER_ID_L6, FLARE_ID, VULCAN_ID, FUSION_ID, GAUSS_ID, SPREADFIRE_ID, PLASMA_ID, HELIX_ID, PHOENIX_ID, OMEGA_ID })
		for (bool reverse : { false, true })
			for (bool harmless : { false, true }) {
				camera = authored_camera;
				camera.ctype.ai_info.behavior = 135;
				camera.flags = 0;
				std::map<int, int> original;
				for (int i = 0; i <= Highest_object_index; ++i)
					if (Objects[i].type != OBJ_NONE) original.emplace(i, int{ Objects[i].signature });
				const int shot = projectile_test_shot(int(ConsoleObject - Objects), id);
				auto &weapon = Objects[shot];
				weapon.pos = camera.pos;
				weapon.pos.x -= 5 * F1_0;
				weapon.mtype.phys_info.velocity = { 32 * F1_0, 0, 0 };
				weapon.flags = harmless ? OF_HARMLESS : 0;
				const auto expected = xl_compat::static_contact(point(camera.pos), camera.mtype.phys_info.mass, point(weapon.pos),
				                                                point(weapon.mtype.phys_info.velocity), weapon.mtype.phys_info.mass, false,
				                                                weapon.mtype.phys_info.flags & PF_PERSISTENT, false);
				collide_two_objects(reverse ? &weapon : &camera, reverse ? &camera : &weapon, &camera.pos);
				require(point(weapon.mtype.phys_info.velocity) == (harmless || weapon.movement_type != MT_PHYSICS ? xl_compat::FixedLightPoint{ 32 * F1_0, 0, 0 } : expected.velocity) &&
				            camera.shields == saved_camera.shields && camera.movement_type == MT_NONE && camera.control_type == 18,
				        "Static camera primary contact lost source bump or indestructibility");
				for (int i = Highest_object_index; i >= 0; --i)
					if (Objects[i].type != OBJ_NONE && (!original.count(i) || original.at(i) != Objects[i].signature)) obj_delete(i);
				++weapons;
			}
	camera = authored_camera;
	camera.ctype.ai_info.behavior = 135;
	camera.flags = 0;
	camera_missile_native_controls(camera);
	camera_passive_physics_controls(camera, true);
	*ConsoleObject = saved_player;
	ConsoleObject->movement_type = MT_PHYSICS;
	ConsoleObject->pos = camera.pos;
	ConsoleObject->pos.x -= 5 * F1_0;
	ConsoleObject->last_pos = ConsoleObject->pos;
	ConsoleObject->mtype.phys_info = {};
	ConsoleObject->mtype.phys_info.mass = F1_0;
	ConsoleObject->mtype.phys_info.velocity.x = 32 * F1_0;
	FrameTime = F1_0 / 8;
	Num_awareness_events = 0;
	const int count = num_objects;
	xl_camera_hit_state_created(int(ConsoleObject - Objects));
	const auto ticks = SDL_GetTicks();
	if (ticks < 500) SDL_Delay(500 - ticks);
	xl_camera_controller_begin_frame();
	do_physics_sim(ConsoleObject);
	xl_compat::CameraHitState hit;
	require(num_objects == count && !Num_awareness_events && xl_camera_hit_state_get(int(ConsoleObject - Objects), hit) &&
	            hit.repaired == int32_t(xl_camera_controller_ticks()) && camera.control_type == 18 && camera.movement_type == MT_NONE &&
	            point(camera.pos) == point(saved_camera.pos),
	        "Static camera physics lost reverse hit state or created player impact effects");
	camera = saved_camera;
	*ConsoleObject = saved_player;
	FrameTime = saved_frame;
	Automap_active = saved_map;
	std::cout << "Native static camera: " << players << " player gates, " << weapons << " primary contacts, missile/passive physics and reverse player hits passed\n";
}
