#include "segment_damage.h"
#include "../segment_damage.h"
#include "preview.h"
extern "C" {
#include "inferno.h"
#include "object.h"
#include "player.h"
#include "game.h"
void start_player_death_sequence(object *player);
}
extern "C" int xl_segment_damage_tick(object *actor)
{
	int16_t shield_rate = 0, energy_rate = 0;
	if (!actor || actor != ConsoleObject || actor->type != OBJ_PLAYER || actor->id != Player_num ||
	    !xl_segment_damage_rates(actor->segnum, &shield_rate, &energy_rate)) return 1;
	auto &player = Players[Player_num];
	const auto shields = xl_compat::segment_damage_amount(FrameTime, shield_rate, player.shields, true);
	const auto energy = xl_compat::segment_damage_amount(FrameTime, energy_rate, player.energy, false);
	player.shields = xl_compat::segment_resource_after(player.shields, shields);
	player.energy = xl_compat::segment_resource_after(player.energy, energy);
	// Leave the death sequence's object-shield reset and camera ownership intact
	if (shields) actor->shields = player.shields;
	if (player.shields < 0) {
		start_player_death_sequence(actor);
		return 0;
	}
	return 1;
}
