#ifndef COOP_FLYOUT_ENGINE_H
#define COOP_FLYOUT_ENGINE_H

/* Shared D1/D2 endlevel hooks, included after the engine declarations */
#include "coop_briefing.h"

static void coop_present_flyout(int level)
{
	(void) level;
	start_endlevel_sequence();
	coop_flyout_render();
}

static void finish_endlevel(void)
{
	if (!coop_flyout_active()) PlayerFinishedLevel(0);
}

static unsigned coop_flyout_tunnel_ms(void)
{
	vms_vector position = ConsoleObject->pos, target;
	int segment = ConsoleObject->segnum;
	int side = find_exit_side(ConsoleObject);
	uint64_t distance = 0;
	for (int count = 0; count <= Highest_segment_index; ++count) {
		if (segment < 0 || segment > Highest_segment_index || side < 0 || side >= 6) return 0;
		compute_center_point_on_side(&target, &Segments[segment], side);
		distance += vm_vec_dist(&position, &target);
		int next = Segments[segment].children[side];
		if (next == -2) {
			/* Tunnel distance / 50 units per second, plus the two outside shots */
			uint64_t milliseconds = distance * 1000 / FLY_SPEED + 5000;
			return milliseconds > 60000 ? 60000 : (unsigned) milliseconds;
		}
		if (next < 0 || next > Highest_segment_index) return 0;
		int entry = matt_find_connect_side(next, segment);
		if (entry < 0 || entry >= 6) return 0;
		side = Side_opposite[entry];
		segment = next;
		position = target;
	}
	return 0;
}

#endif
