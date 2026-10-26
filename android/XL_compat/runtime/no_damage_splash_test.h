extern "C" {
#include "fireball.h"
extern fix Flash_effect;
}

// Paired radius explosions at identical coordinates; only the test property differs
static void no_damage_splash_test(const xl_compat::Inventory &source, int segment)
{
	obj_relink(ConsoleObject - Objects, segment);
	compute_segment_center(&ConsoleObject->pos, &Segments[segment]);
	ConsoleObject->mtype.phys_info.velocity = {};
	ConsoleObject->mtype.phys_info.rotvel = {};
	ConsoleObject->shields = i2f(100);
	const auto initial = *ConsoleObject;
	const auto saved_flash = Flash_effect;
	unsigned saved_rng = 0;
	d_rand_get_state(&saved_rng);
	vms_vector origin = ConsoleObject->pos;
	origin.x += F1_0;
	require(find_point_seg(&origin, segment) == segment, "splash control origin is outside its segment");
	const auto explode = [&](object *weapon = nullptr) {
		ConsoleObject->mtype = initial.mtype;
		ConsoleObject->shields = initial.shields;
		Players[Player_num].shields = i2f(100);
		PaletteRedAdd = PaletteGreenAdd = PaletteBlueAdd = 0;
		Flash_effect = 0;
		d_srand(5678);
		const auto *effect = object_create_badass_explosion(weapon, segment, &origin, F1_0, VCLIP_SMALL_EXPLOSION,
		                                                    i2f(20), i2f(8), i2f(40), -1);
		require(effect && effect->type == OBJ_FIREBALL, "splash control did not create explosion");
	};
	explode();
	const auto protected_physics = ConsoleObject->mtype.phys_info;
	require(Players[Player_num].shields == i2f(100) && PaletteRedAdd == 0 &&
	            (protected_physics.velocity.x || protected_physics.velocity.y || protected_physics.velocity.z) &&
	            (protected_physics.rotvel.x || protected_physics.rotvel.y || protected_physics.rotvel.z),
	        "protected splash lost force/rotation or applied damage flash");
	auto neutral = source;
	if (neutral.level_version >= 21) neutral.segments[segment].properties &= ~xl_compat::segment_no_damage;
	else neutral.segments[segment].function = 0;
	xl_test_bind_no_damage(neutral);
	explode();
	require(Players[Player_num].shields < i2f(100) && PaletteRedAdd > 0 &&
	            !std::memcmp(&protected_physics, &ConsoleObject->mtype.phys_info, sizeof(protected_physics)),
	        "ordinary splash failed to damage or differed in retained physical response");
	xl_test_bind_no_damage(source);
	const int flash_weapon = obj_create(OBJ_WEAPON, FLASH_ID, segment, &origin, &ConsoleObject->orient, F1_0, CT_WEAPON, MT_PHYSICS, RT_NONE);
	require(flash_weapon >= 0 && Weapon_info[FLASH_ID].flash > 0 && Weapon_info[FLASH_ID].children == -1,
	        "flash splash control has unexpected weapon definitions");
	Objects[flash_weapon].ctype.laser_info.parent_type = OBJ_PLAYER;
	Objects[flash_weapon].ctype.laser_info.parent_num = ConsoleObject - Objects;
	Objects[flash_weapon].ctype.laser_info.parent_signature = ConsoleObject->signature;
	explode(&Objects[flash_weapon]);
	const auto protected_flash_physics = ConsoleObject->mtype.phys_info;
	const auto protected_flash_delay = Flash_effect;
	require(Players[Player_num].shields == i2f(100) && protected_flash_delay > 0 &&
	            PaletteRedAdd > 0 && PaletteGreenAdd > 0 && PaletteBlueAdd > 0,
	        "protected flash missile lost blinding effect or inflicted damage");
	xl_test_bind_no_damage(neutral);
	explode(&Objects[flash_weapon]);
	require(Players[Player_num].shields < i2f(100) && Flash_effect == protected_flash_delay &&
	            !std::memcmp(&protected_flash_physics, &ConsoleObject->mtype.phys_info, sizeof(protected_flash_physics)),
	        "flash missile protection changed blast physics or blinding duration");
	xl_test_bind_no_damage(source);
	Flash_effect = saved_flash;
	d_rand_set_state(saved_rng);
	std::cout << "No-damage native splash controls: radius explosion retains identical force and rotation while suppressing shield loss; flash missile blinding remains active\n";
}
