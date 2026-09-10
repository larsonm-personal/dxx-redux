#include "guidebot_path_recovery.h"

#if defined(__ANDROID__) || defined(DXX_GUIDEBOT_ROUTE_PLANNER)
#include <stdio.h>

#include "inferno.h"
#include "object.h"
#include "ai.h"
#include "game.h"
#include "gameseg.h"
#include "guidebot_route_internal.h"

static int precise_recovery;
static vms_vector precise_recovery_point;

/* Find an occupiable interior waypoint with a verified incoming and outgoing leg */
static int find_recovery_waypoint(const object *objp, int segnum,
                                  const vms_vector *to, vms_vector *result)
{
	vms_vector center;
	if (segnum < 0 || segnum > Highest_segment_index)
		return 0;
	compute_segment_center(&center, &Segments[segnum]);
	for (int sample = 0; sample < 43; ++sample) {
		vms_vector point = center;
		if (sample) {
			vms_vector offset, surface;
			if (sample <= 18)
				compute_center_point_on_side(&surface, &Segments[segnum], (sample - 1) / 3);
			else
				surface = Vertices[Segments[segnum].verts[(sample - 19) / 3]];
			vm_vec_sub(&offset, &surface, &center);
			vm_vec_scale_add(&point, &center, &offset, ((sample - 1) % 3 + 1) * (F1_0 / 4));
		}
		if (find_point_seg(&point, segnum) != segnum ||
		    vm_vec_dist(&objp->pos, &point) <= F1_0 ||
		    !guidebot_route_waypoint_leg_clear(objp, &objp->pos, objp->segnum, &point) ||
		    !guidebot_route_waypoint_leg_clear(objp, &point, segnum, to))
			continue;
		*result = point;
		return 1;
	}
	return 0;
}

/* Finish a skipped clear approach after a sustained stall at a portal rim
 * Keep the cursor and full collision radius; do not switch back mid-approach */
void guidebot_route_recover_approach(object *objp, vms_vector *goal_point)
{
	ai_static *aip = &objp->ctype.ai_info;
	static int recovery_signature, recovery_path, recovery_index;
	static fix64 recovery_time;
	static vms_vector recovery_last_pos, recovery_point;
	static fix still_time, progress_time, best_distance;
	static vms_vector progress_goal;
	static int recovering;
	const fix64 elapsed = GameTime64 - recovery_time;
	const fix distance = vm_vec_dist(&objp->pos, goal_point);
	if (recovery_signature != objp->signature || recovery_path != aip->hide_index ||
	    recovery_index != aip->cur_path_index || elapsed <= 0 || elapsed > F1_0 / 4) {
		still_time = 0;
		recovering = 0;
		precise_recovery = 0;
		progress_time = 0;
		best_distance = distance;
	} else if (vm_vec_dist(&objp->pos, &recovery_last_pos) <= F1_0 / 64)
		still_time = min(F1_0, still_time + (fix) elapsed);
	else
		still_time = 0;
	if (progress_goal.x != goal_point->x || progress_goal.y != goal_point->y ||
	    progress_goal.z != goal_point->z || distance < best_distance - F1_0 / 64) {
		progress_time = 0;
		best_distance = distance;
	} else if (elapsed > 0 && elapsed <= F1_0 / 4)
		progress_time = min(F1_0, progress_time + (fix) elapsed);
	progress_goal = *goal_point;
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
	/* A portal can require an off-center approach even when both waypoints
	 * are occupiable. Search nearby interior positions with two clear legs */
	if (!recovering && progress_time >= F1_0 / 4 &&
	    !guidebot_route_waypoint_leg_clear(objp, &objp->pos, objp->segnum, goal_point)) {
		const int previous = aip->cur_path_index - aip->PATH_DIR;
		const int segments[2] = { previous >= 0 && previous < aip->path_length ? Point_segs[aip->hide_index + previous].segnum : -1,
			                      objp->segnum };
		for (int candidate = 0; candidate < 2 && !recovering; ++candidate) {
			const int segnum = segments[candidate];
			vms_vector center;
			if (segnum < 0 || segnum > Highest_segment_index ||
			    (candidate && segnum == segments[0]) ||
			    (segnum != objp->segnum && find_connect_side(&Segments[segnum], &Segments[objp->segnum]) < 0))
				continue;
			if (find_recovery_waypoint(objp, segnum, goal_point, &center)) {
				recovery_point = precise_recovery_point = center;
				precise_recovery = recovering = 1;
				vm_vec_zero(&objp->mtype.phys_info.velocity);
#if defined(DXX_GUIDEBOT_ROUTE_PLANNER)
				fprintf(stderr, "ROUTE-CONFIRM recover portal actor_seg=%d approach_seg=%d path_index=%d\n",
				        objp->segnum, segnum, aip->cur_path_index);
#endif
			}
		}
	}
	if (recovering) {
		if (vm_vec_dist(&objp->pos, &recovery_point) <= (precise_recovery ? F1_0 / 32 : F1_0 / 2) ||
		    !guidebot_route_waypoint_leg_clear(objp, &objp->pos, objp->segnum, &recovery_point)) {
			if (precise_recovery)
				vm_vec_zero(&objp->mtype.phys_info.velocity);
			recovering = precise_recovery = 0;
			progress_time = 0;
			best_distance = distance;
		} else
			*goal_point = recovery_point;
	}
}
/* Approach recovery needs accurate arrival instead of blended momentum
 * Bound travel to half the remaining distance, including the 2x sim speed cap */
void guidebot_route_steer_approach(object *objp)
{
	vms_vector direction;
	fix distance, speed;
	if (!precise_recovery || FrameTime <= 0 ||
	    !guidebot_route_waypoint_leg_clear(objp, &objp->pos, objp->segnum, &precise_recovery_point))
		return;
	distance = vm_vec_normalized_dir(&direction, &precise_recovery_point, &objp->pos);
	speed = min(vm_vec_mag(&objp->mtype.phys_info.velocity), fixdiv(distance, FrameTime * 2));
	vm_vec_copy_scale(&objp->mtype.phys_info.velocity, &direction, speed);
}
#endif
