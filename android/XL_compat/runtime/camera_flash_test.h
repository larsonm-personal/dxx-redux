#include "camera_flash.h"
#include "../camera_flash.h"
extern "C" {
extern fix64 Time_flash_last_played;
}
static void camera_flash_native_controls(object &camera)
{
	const auto saved_player = *ConsoleObject;
	const auto saved_pilot = Players[Player_num];
	const auto saved_frame = FrameTime;
	FrameTime = 1092;
	const int target = projectile_robot();
	auto &robot = Objects[target];
	robot.pos = camera.pos;
	robot.pos.x += F1_0;
	obj_relink(target, camera.segnum);
	const auto original_robot = robot;
	std::vector<std::pair<int, int>> hidden;
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type != OBJ_NONE && i != target && &Objects[i] != ConsoleObject && &Objects[i] != &camera) {
			hidden.emplace_back(i, Objects[i].type);
			Objects[i].type = OBJ_NONE;
		}
	unsigned cases = 0;
	const auto &definition = Weapon_info[FLASH_ID];
	require(definition.flash > 0 && definition.children == -1, "Original flash missile definition is not childless");
	std::cout << "Original flash missile: flash=" << int(definition.flash) << " radius=" << definition.damage_radius << "\n";
	for (bool self : { false, true })
		for (bool reverse : { false, true }) {
			robot = original_robot;
			robot.shields = 1000 * F1_0;
			robot.mtype.phys_info.rotthrust = {};
			robot.mtype.phys_info.rotvel = {};
			robot.mtype.phys_info.flags &= ~PF_USES_THRUST;
			robot.ctype.ai_info.SKIP_AI_COUNT = 0;
			*ConsoleObject = saved_player;
			Players[Player_num] = saved_pilot;
			ConsoleObject->shields = Players[Player_num].shields = 1000 * F1_0;
			Players[Player_num].flags &= ~PLAYER_FLAGS_INVULNERABLE;
			camera.flags = 0;
			xl_camera_flash_clear();
			PALETTE_FLASH_SET(0, 0, 0);
			Flash_effect = 0;
			Time_flash_last_played = 0;
			const int shot = projectile_test_shot(int(ConsoleObject - Objects), FLASH_ID);
			auto &weapon = Objects[shot];
			if (!self) ++weapon.ctype.laser_info.parent_signature;
			const auto point = [](const vms_vector &v) { return xl_compat::FixedLightPoint{ v.x, v.y, v.z }; };
			auto impact = camera.pos;
			const auto adjusted = xl_compat::camera_splash_impact(point(impact), point(camera.pos), camera.size, weapon.size);
			auto sample = xl_compat::camera_splash_sample(point(ConsoleObject->pos), adjusted, definition.strength[Difficulty_level], definition.damage_radius, definition.strength[Difficulty_level]);
			const auto pulse = xl_compat::camera_flash_pulse(sample.force, definition.flash, self);
			require(pulse.intensity > 0, "Camera flash fixture did not reach player");
			auto radial = xl_compat::camera_splash_sample(point(robot.pos), adjusted, definition.strength[Difficulty_level], definition.damage_radius, definition.strength[Difficulty_level]);
			int skip = 0;
			xl_compat::FixedLightPoint thrust{};
			d_srand(5432); // Source splash precedes camera awareness
			const bool uses = xl_compat::camera_flash_robot(skip, thrust, radial.impulse, definition.flash, Robot_info[robot.id].boss_flag != 0, -1, FrameTime, [] { return d_rand(); });
			d_srand(5432);
			std::array<int, MAX_OBJECTS> signatures{};
			const int highest = Highest_object_index;
			for (int i = 0; i <= highest; ++i) signatures[i] = Objects[i].signature;
			collide_two_objects(reverse ? &weapon : &camera, reverse ? &camera : &weapon, &impact);
			require(weapon.flags & OF_SHOULD_BE_DEAD, "Camera flash missile survived impact");
			require(point(robot.mtype.phys_info.rotthrust) == thrust && bool(robot.mtype.phys_info.flags & PF_USES_THRUST) == uses,
			        "Camera flash collision lost source robot rotational stun");
			require(Flash_effect == pulse.delay && !xl_camera_flash_snapshot().is_null() && PaletteRedAdd > 0,
			        "Camera flash collision lost player blindness");
			const auto before = xl_camera_flash_snapshot();
			stop_time();
			require(xl_camera_flash_frame() && xl_camera_flash_snapshot() == before, "Paused camera flash advanced");
			start_time();
			PALETTE_FLASH_ADD(4, 0, 0);
			require(Flash_effect == pulse.delay, "Damage palette composition erased flash delay");
			const auto mixed = xl_camera_flash_snapshot();
			require(xl_camera_flash_restore(mixed, false), "Live camera flash snapshot did not validate");
			xl_camera_flash_clear();
			require(xl_camera_flash_restore(mixed, true) && xl_camera_flash_snapshot() == mixed, "Camera flash snapshot did not restore");
			const auto sim_calls = d_rand_get_call_count();
			GameTime64 += F1_0 / 4;
			require(xl_camera_flash_frame() && Flash_effect == std::max(0, pulse.delay - FrameTime) &&
			            Time_flash_last_played == GameTime64 && d_rand_get_call_count() == sim_calls,
			        "Active flash frame lost delay/sound timing or consumed simulation randomness");
			for (int i = 0; i <= Highest_object_index; ++i)
				if (Objects[i].type == OBJ_FIREBALL && (i > highest || Objects[i].signature != signatures[i])) obj_delete(i);
			obj_delete(shot);
			++cases;
		}
	obj_delete(target);
	for (const auto &entry : hidden) Objects[entry.first].type = entry.second;
	*ConsoleObject = saved_player;
	Players[Player_num] = saved_pilot;
	FrameTime = saved_frame;
	std::cout << "Native camera flash: " << cases << " ordered/self impacts, robot stun, blindness, pause, composition and snapshot passed\n";
	// Keep an impact-earned active pulse for the caller's full save through stock
}
