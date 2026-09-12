#include "guidebot_path_recovery.h"

#if defined(__ANDROID__) || defined(DXX_GUIDEBOT_ROUTE_PLANNER)
#include <stdio.h>

#include "inferno.h"
#include "object.h"
#include "ai.h"
#include "game.h"
#include "gameseg.h"
#include "fvi.h"
#include "wall.h"
#include "maths.h"
#include "guidebot_route_internal.h"

static int precise_recovery;
static vms_vector precise_recovery_point;

/* Match the short collision sweeps used by movement when validating a recovery leg */
static int recovery_leg_clear(const object *objp, const vms_vector *from,
                              int segnum, const vms_vector *to)
{
	vms_vector current = *from;
	const int chunks = vm_vec_dist(from, to) / F1_0 + 1;
	if (!guidebot_route_waypoint_leg_clear(objp, from, segnum, to))
		return 0;
	for (int chunk = 1; chunk <= chunks; ++chunk) {
		vms_vector next;
		next.x = from->x + (fix) (((long long) to->x - from->x) * chunk / chunks);
		next.y = from->y + (fix) (((long long) to->y - from->y) * chunk / chunks);
		next.z = from->z + (fix) (((long long) to->z - from->z) * chunk / chunks);
		if (!guidebot_route_waypoint_leg_clear(objp, &current, segnum, &next))
			return 0;
		segnum = find_point_seg(&next, segnum);
		if (segnum < 0)
			return 0;
		current = next;
	}
	return 1;
}

/* Find an occupiable interior waypoint with a verified incoming and outgoing leg */
static vms_vector recovery_sample(int segnum, const vms_vector *center, int sample)
{
	vms_vector point = *center;
	if (sample) {
		vms_vector offset, surface;
		if (sample <= 18)
			compute_center_point_on_side(&surface, &Segments[segnum], (sample - 1) / 3);
		else
			surface = Vertices[Segments[segnum].verts[(sample - 19) / 3]];
		vm_vec_sub(&offset, &surface, center);
		vm_vec_scale_add(&point, center, &offset, ((sample - 1) % 3 + 1) * (F1_0 / 4));
	}
	return point;
}

static int recovery_point_fits(const object *objp, int segnum, const vms_vector *point)
{
	object probe = *objp;
	probe.pos = *point;
	probe.segnum = (short) segnum;
	if (ConsoleObject && ConsoleObject->size > probe.size)
		probe.size = ConsoleObject->size;
	return find_point_seg(point, segnum) == segnum && !object_intersects_wall(&probe);
}

static int find_recovery_waypoint(const object *objp, int segnum,
                                  const vms_vector *to, vms_vector *result)
{
	vms_vector center;
	if (segnum < 0 || segnum > Highest_segment_index)
		return 0;
	compute_segment_center(&center, &Segments[segnum]);
	for (int sample = 0; sample < 43; ++sample) {
		vms_vector point = recovery_sample(segnum, &center, sample);
		if (!recovery_point_fits(objp, segnum, &point) ||
		    vm_vec_dist(&objp->pos, &point) <= F1_0 ||
		    !recovery_leg_clear(objp, &objp->pos, objp->segnum, &point) ||
		    !recovery_leg_clear(objp, &point, segnum, to))
			continue;
		*result = point;
		return 1;
	}
	return 0;
}

/* A thin tapered connector can have an unoccupiable center. Replace only an
 * invalid intermediate point, proving the approach and unchanged continuation */
static int repair_recovery_waypoint(object *objp, vms_vector *goal_point,
                                    vms_vector *approach)
{
	ai_static *aip = &objp->ctype.ai_info;
	const int next = aip->cur_path_index + aip->PATH_DIR;
	if (next < 0 || next >= aip->path_length)
		return 0;
	point_seg *waypoint = &Point_segs[aip->hide_index + aip->cur_path_index];
	const int segnum = waypoint->segnum;
	if (segnum < 0 || segnum > Highest_segment_index ||
	    recovery_point_fits(objp, segnum, &waypoint->point))
		return 0;
	vms_vector center;
	compute_segment_center(&center, &Segments[segnum]);
	for (int sample = 0; sample < 43; ++sample) {
		vms_vector point = recovery_sample(segnum, &center, sample);
		if (!recovery_point_fits(objp, segnum, &point) ||
		    !recovery_leg_clear(objp, &point, segnum, &Point_segs[aip->hide_index + next].point) ||
		    !find_recovery_waypoint(objp, objp->segnum, &point, approach))
			continue;
		waypoint->point = *goal_point = point;
#if defined(DXX_GUIDEBOT_ROUTE_PLANNER)
		fprintf(stderr, "ROUTE-CONFIRM repair tapered waypoint actor_seg=%d waypoint_seg=%d path_index=%d\n",
		        objp->segnum, segnum, aip->cur_path_index);
#endif
		return 1;
	}
	return 0;
}

/* A failed approach is not proof that the objective is unreachable. Try a
 * different topological route, preserving the goal and trigger exclusions */
static int recover_alternate_path(object *objp, vms_vector *goal_point)
{
	ai_static *aip = &objp->ctype.ai_info;
	ai_local *ailp = &Ai_local_info[objp - Objects];
	const int used = (int) (Point_segs_free_ptr - Point_segs);
	const int depth = Max_escort_length;
	int next_seg = -1;
	unsigned int rng, rng_calls;
	if (aip->PATH_DIR != 1 || aip->hide_index < 0 || aip->path_length <= 1 ||
	    aip->cur_path_index < 0 || aip->cur_path_index >= aip->path_length ||
	    aip->hide_index + aip->path_length > used || depth <= 0 ||
	    depth > MAX_SEGMENTS || used + 2 * (depth + 2) + 2 * MAX_PATH_LENGTH > MAX_POINT_SEGS / 2)
		return 0;
	for (int i = aip->cur_path_index; i < aip->path_length; ++i) {
		next_seg = Point_segs[aip->hide_index + i].segnum;
		if (next_seg != objp->segnum)
			break;
	}
	if (next_seg < 0 || next_seg > Highest_segment_index || next_seg == objp->segnum)
		return 0;
	const int side = find_connect_side(&Segments[next_seg], &Segments[objp->segnum]);
	/* Only reconsider a presently open portal, never bypass a closed door */
	if (side < 0 || !(WALL_IS_DOORWAY(&Segments[objp->segnum], side) & WID_FLY_FLAG))
		return 0;
	const int goal = Point_segs[aip->hide_index + aip->path_length - 1].segnum;
	if (goal < 0 || goal > Highest_segment_index || goal == objp->segnum || !d_rand_get_state(&rng))
		return 0;
	const ai_static saved_ai = *aip;
	const ai_local saved_local = *ailp;
	rng_calls = d_rand_get_call_count();
	/* Leave enough room for all safety points below the GC threshold. This
	 * keeps the original path intact until the replacement is accepted */
	int accepted = create_path_to_segment_avoiding_edges(objp, goal, depth, 1,
	                                                     objp->segnum, next_seg, -1, -1);
	for (int i = 1; accepted && i < aip->path_length; ++i) {
		const int from = Point_segs[aip->hide_index + i - 1].segnum;
		const int to = Point_segs[aip->hide_index + i].segnum;
		const int avoid_from[2] = { Escort_route_avoid_from_seg, Escort_route_avoid_from_seg2 };
		const int avoid_to[2] = { Escort_route_avoid_seg, Escort_route_avoid_seg2 };
		for (int edge = 0; edge < 2; ++edge)
			if ((from == avoid_from[edge] && to == avoid_to[edge]) ||
			    (to == avoid_from[edge] && from == avoid_to[edge]))
				accepted = 0;
	}
	if (!accepted) {
		*aip = saved_ai;
		*ailp = saved_local;
		Point_segs_free_ptr = Point_segs + used;
		d_rand_set_state(rng);
		d_rand_set_call_count(rng_calls);
		return 0;
	}
	*goal_point = Point_segs[aip->hide_index].point;
#if defined(DXX_GUIDEBOT_ROUTE_PLANNER)
	fprintf(stderr, "ROUTE-CONFIRM recover alternate actor_seg=%d avoided_seg=%d goal=%d points=%d\n",
	        objp->segnum, next_seg, goal, aip->path_length);
#endif
	return 1;
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
	 * are occupiable. A long sweep can miss a rim hit by short movement steps;
	 * use the same stepped check that validates the replacement legs */
	if (!recovering && progress_time >= F1_0 / 4 &&
	    !recovery_leg_clear(objp, &objp->pos, objp->segnum, goal_point)) {
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
	if (!recovering && progress_time >= F1_0 &&
	    !recovery_leg_clear(objp, &objp->pos, objp->segnum, goal_point)) {
		progress_time = 0;
		if (recover_alternate_path(objp, goal_point)) {
			precise_recovery = 0;
			return;
		}
		if (repair_recovery_waypoint(objp, goal_point, &recovery_point)) {
			precise_recovery_point = recovery_point;
			precise_recovery = recovering = 1;
			vm_vec_zero(&objp->mtype.phys_info.velocity);
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
