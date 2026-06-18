/*
 *
 * D1-in-D2 input-demo compatibility helpers.
 *
 */

#include "d1_in_d2_input_demo.h"

#include "d1_in_d2.h"
#include "player.h"

int d1_in_d2_input_demo_result_robots_killed(player *current_player,
	int d2_baseline_valid, int d2_baseline)
{
	if (!current_player)
		return 0;
	if (d1_in_d2_use_d1_gameplay())
		return current_player->num_robots_level;
	if (!d2_baseline_valid)
		return 0;
	return current_player->num_kills_level - d2_baseline;
}
