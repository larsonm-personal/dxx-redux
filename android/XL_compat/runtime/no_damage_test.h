#include "no_damage_test_binding.h"
#include "no_damage_firing_test.h"
#include "no_damage_reference_gates_test.h"
#include "no_damage_guided_test.h"
#include "no_damage_fusion_test.h"
#include "no_damage_wall_test.h"
#include "no_damage_splash_test.h"
extern "C" {
#include "collide.h"
#include "endlevel.h"
}
#include "no_damage_bump_test.h"
#include "no_damage_boundary_test.h"

// Controlled native calls, not an earned route or original Observatory admission
static void no_damage_collision_test(const xl_compat::Inventory &source)
{
	int protected_segment = -1, ordinary_segment = -1;
	for (int i = 0; i < Num_segments; ++i) {
		if (source.segments[i].function == 13) protected_segment = i;
		if (source.segments[i].function == 0) ordinary_segment = i;
	}
	require(protected_segment >= 0 && ordinary_segment >= 0, "no-damage control requires both segment types");
	xl_test_bind_no_damage(source);
	require(xl_preview_no_damage_segment(protected_segment) && !xl_preview_no_damage_segment(ordinary_segment) &&
	            !xl_preview_no_damage_segment(-1) && !xl_preview_no_damage_segment(Num_segments),
	        "no-damage property map differs from source");
	auto bad = source;
	++bad.vertices[0][0];
	bool rejected = false;
	try {
		xl_test_bind_no_damage(bad);
	} catch (const std::runtime_error &) {
		rejected = true;
	}
	require(rejected && xl_preview_no_damage_segment(protected_segment), "mismatched binding changed active properties");
	const int previous_mode = Game_mode, previous_dead = Player_is_dead, previous_end = Endlevel_sequence;
	const auto previous_player = Players[Player_num];
	Game_mode = GM_MULTI;
	require(!xl_preview_no_damage_segment(protected_segment), "no-damage qualification leaked into multiplayer");
	Game_mode = Player_is_dead = Endlevel_sequence = 0;
	Players[Player_num].flags &= ~PLAYER_FLAGS_INVULNERABLE;
	Players[Player_num].shields = i2f(100);
	require(ConsoleObject && ConsoleObject->type == OBJ_PLAYER, "no-damage control needs loaded player");
	const auto move_player = [](int segment) {
		obj_relink(ConsoleObject - Objects, segment);
		compute_segment_center(&ConsoleObject->pos, &Segments[segment]);
	};
	move_player(protected_segment);
	PaletteRedAdd = PaletteGreenAdd = PaletteBlueAdd = 0;
	const auto player_before = *ConsoleObject;
	apply_damage_to_player(ConsoleObject, nullptr, i2f(10), 0);
	require(Players[Player_num].shields == i2f(100) && !std::memcmp(ConsoleObject, &player_before, sizeof(player_before)) &&
	            PaletteRedAdd == 0 && PaletteGreenAdd == 0 && PaletteBlueAdd == 0,
	        "protected player damage changed shields, object or palette");
	move_player(ordinary_segment);
	apply_damage_to_player(ConsoleObject, nullptr, i2f(10), 0);
	require(Players[Player_num].shields == i2f(90) && PaletteRedAdd != 0, "ordinary damage control did not damage/flash");
	vms_vector incoming_position;
	compute_segment_center(&incoming_position, &Segments[protected_segment]);
	const int weapon_index = Laser_create_new(&ConsoleObject->orient.fvec, &incoming_position, protected_segment,
	                                          ConsoleObject - Objects, LASER_ID_L1, 0);
	require(weapon_index >= 0, "cannot create incoming weapon control");
	move_player(protected_segment);
	auto &weapon = Objects[weapon_index];
	weapon.ctype.laser_info.parent_type = OBJ_CNTRLCEN;
	weapon.ctype.laser_info.parent_num = -1;
	weapon.shields = i2f(10);
	weapon.ctype.laser_info.multiplier = F1_0;
	weapon.mtype.phys_info.flags &= ~PF_PERSISTENT;
	const auto weapon_before = weapon, protected_player = *ConsoleObject;
	const auto shields = Players[Player_num].shields;
	const int objects_before = num_objects;
	PaletteRedAdd = PaletteGreenAdd = PaletteBlueAdd = 0;
	vms_vector hit = ConsoleObject->pos;
	collide_player_and_weapon(ConsoleObject, &weapon, &hit);
	require(!std::memcmp(&weapon, &weapon_before, sizeof(weapon)) &&
	            !std::memcmp(ConsoleObject, &protected_player, sizeof(protected_player)) &&
	            Players[Player_num].shields == shields && num_objects == objects_before &&
	            PaletteRedAdd == 0 && PaletteGreenAdd == 0 && PaletteBlueAdd == 0,
	        "protected direct impact changed projectile, player, effects or shields");
	// Same projectile now hits an ordinary player segment; its own segment stays protected
	move_player(ordinary_segment);
	hit = ConsoleObject->pos;
	collide_player_and_weapon(ConsoleObject, &weapon, &hit);
	require(Players[Player_num].shields == shields - i2f(10) && (weapon.flags & OF_SHOULD_BE_DEAD) && num_objects > objects_before,
	        "ordinary direct impact did not damage, dispose projectile and create effect");
	no_damage_firing_test(protected_segment, ordinary_segment);
	no_damage_reference_gates_test(protected_segment, ordinary_segment);
	no_damage_guided_test(protected_segment, ordinary_segment);
	no_damage_wall_test(protected_segment, ordinary_segment);
	no_damage_splash_test(source, protected_segment);
	no_damage_bump_test(source, protected_segment);
	no_damage_fusion_test(source, protected_segment, ordinary_segment);
	no_damage_boundary_test(source);
	Game_mode = previous_mode;
	Player_is_dead = previous_dead;
	Endlevel_sequence = previous_end;
	Players[Player_num] = previous_player;
	std::cout << "No-damage native collision controls: protected damage and impacts preserve state; ordinary controls damage, flash and dispose projectile; original admission tested separately\n";
}
