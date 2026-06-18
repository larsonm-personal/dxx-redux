/*
 *
 * Small D1-in-D2 gameplay policy helpers.
 *
 */

#include "d1_in_d2_semantics.h"

#include "d1_in_d2.h"
#include "game.h"
#include "object.h"

int d1_in_d2_use_d2_resource_drop_suppression(const object *objp, int game_mode)
{
	return objp && !d1_in_d2_use_d1_gameplay() &&
	       !(game_mode & GM_MULTI) && objp->type != OBJ_PLAYER;
}

int d1_in_d2_use_d2_badass_robot_explosion(void)
{
	return !d1_in_d2_use_d1_gameplay();
}

int d1_in_d2_ai_visibility_turns_robot(int player_visibility)
{
	if (d1_in_d2_use_d1_gameplay())
		return player_visibility != 0;
	return player_visibility == 2;
}

int d1_in_d2_ai_may_turn_randomly_without_visibility(int game_mode)
{
	return d1_in_d2_use_d1_gameplay() && !(game_mode & GM_MULTI);
}

int d1_in_d2_ai_nearby_fire_shortcut_active(int dist_to_last_fired_upon_player_pos, int threshold)
{
	return !d1_in_d2_use_d1_gameplay() &&
	       dist_to_last_fired_upon_player_pos < threshold;
}
