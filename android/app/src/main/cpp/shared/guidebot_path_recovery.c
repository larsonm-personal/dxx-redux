#include "guidebot_path_recovery.h"

#if defined(__ANDROID__) || defined(DXX_GUIDEBOT_ROUTE_PLANNER)
#include <stdio.h>

#include "inferno.h"
#include "object.h"
#include "ai.h"
#include "game.h"
#include "gameseg.h"
#include "guidebot_route_internal.h"

/* Finish a skipped clear approach after a sustained stall at a portal rim
 * Keep the cursor and full collision radius; do not switch back mid-approach */
void guidebot_route_recover_approach(object *objp, vms_vector *goal_point)
{
	ai_static *aip = &objp->ctype.ai_info;
	static int recovery_signature, recovery_path, recovery_index;
	static fix64 recovery_time;
	static vms_vector recovery_last_pos, recovery_point;
	static fix still_time;
	static int recovering;
	const fix64 elapsed = GameTime64 - recovery_time;
	if (recovery_signature != objp->signature || recovery_path != aip->hide_index ||
	    recovery_index != aip->cur_path_index || elapsed <= 0 || elapsed > F1_0 / 4) {
		still_time = 0;
		recovering = 0;
	} else if (vm_vec_dist(&objp->pos, &recovery_last_pos) <= F1_0 / 64)
		still_time = min(F1_0, still_time + (fix) elapsed);
	else
		still_time = 0;
	recovery_signature = objp->signature;
	recovery_path = aip->hide_index;
	recovery_index = aip->cur_path_index;
	recovery_time = GameTime64;
	recovery_last_pos = objp->pos;
	if (!recovering && still_time >= F1_0 / 4) {
		const int previous = aip->cur_path_index - aip->PATH_DIR;
		if (previous >= 0 && previous < aip->path_length) {
			const point_seg *approach = &Point_segs[aip->hide_index + previous];
			if (approach->segnum >= 0 && approach->segnum <= Highest_segment_index &&
			    (approach->segnum == objp->segnum ||
			     find_connect_side(&Segments[approach->segnum], &Segments[objp->segnum]) >= 0) &&
			    vm_vec_dist(&objp->pos, &approach->point) > F1_0 &&
			    guidebot_route_waypoint_leg_clear(objp, &objp->pos, objp->segnum, &approach->point)) {
				recovery_point = approach->point;
				recovering = 1;
				vm_vec_zero(&objp->mtype.phys_info.velocity);
#if defined(DXX_GUIDEBOT_ROUTE_PLANNER)
				fprintf(stderr, "ROUTE-CONFIRM recover approach actor_seg=%d approach_seg=%d path_index=%d\n",
				        objp->segnum, approach->segnum, aip->cur_path_index);
#endif
			}
		}
	}
	if (recovering) {
		if (vm_vec_dist(&objp->pos, &recovery_point) <= F1_0 / 2 ||
		    !guidebot_route_waypoint_leg_clear(objp, &objp->pos, objp->segnum, &recovery_point))
			recovering = 0;
		else
			*goal_point = recovery_point;
	}
}
#endif
