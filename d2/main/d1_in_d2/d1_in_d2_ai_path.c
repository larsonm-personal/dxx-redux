/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/* Native D1 path construction and requests, adapted from d1/main/aipath.c
 * Geometry, point storage and garbage collection remain engine services */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "inferno.h"
#include "object.h"
#include "ai.h"
#include "wall.h"
#include "game.h"
#include "gameseg.h"
#include "dxxerror.h"
#include "d1_in_d2.h"
#include "d1_in_d2_ai.h"
#include "input_demo_hooks.h"
#include "deterministic_math.h"
#include "d1_in_d2_ai_internal.h"

static int native_path_actor(const object *obj)
{
	return obj && d1_in_d2_use_d1_gameplay() &&
		(obj == ConsoleObject || d1_in_d2_ai_actor_role(obj) == D1_AI_NATIVE_ENEMY);
}

int d1_in_d2_ai_door_is_openable(object *obj, segment *seg, int side)
{
	if (!native_path_actor(obj))
		return D1_AI_NOT_APPLICABLE;
	if (!IS_CHILD(seg->children[side]) || seg->sides[side].wall_num < 0)
		return 0;
	const wall *door = &Walls[seg->sides[side].wall_num];
	if (obj == ConsoleObject)
		return door->type == WALL_DOOR;
	return (obj->id == ROBOT_BRAIN || obj->ctype.ai_info.behavior == AIB_RUN_FROM) &&
		door->type == WALL_DOOR && door->keys == KEY_NONE && !(door->flags & WALL_DOOR_LOCKED);
}

/* The legacy path API has no capacity argument for caller-owned arrays
 * Bound shared-pool writes without subtracting unrelated pointers */
static int path_capacity(point_seg *points, int *shared_pool)
{
	const uintptr_t address = (uintptr_t)points;
	const uintptr_t first = (uintptr_t)Point_segs;
	const uintptr_t end = (uintptr_t)(Point_segs + MAX_POINT_SEGS);
	*shared_pool = address >= first && address <= end;
	return *shared_pool ? (int)((end - address) / sizeof(*points)) : MAX_POINT_SEGS;
}

static void insert_center_points(point_seg *psegs, short *num_points)
{
	int	i, last_point;

	last_point = *num_points-1;

	for (i=last_point; i>0; i--) {
		int			connect_side;
		vms_vector	center_point, new_point;

		psegs[2*i] = psegs[i];
		connect_side = find_connect_side(&Segments[psegs[i].segnum], &Segments[psegs[i-1].segnum]);
		Assert(connect_side != -1);	//	Impossible!  These two segments must be connected, they were created by create_path_points (which was created by mk!)
		if (connect_side == -1)			//	Try to blow past the assert, this should at least prevent a hang.
			connect_side = 0;
		compute_center_point_on_side(&center_point, &Segments[psegs[i-1].segnum], connect_side);
		vm_vec_sub(&new_point, &psegs[i-1].point, &center_point);
		new_point.x /= 16;
		new_point.y /= 16;
		new_point.z /= 16;
		vm_vec_sub(&psegs[2*i-1].point, &center_point, &new_point);
		psegs[2*i-1].segnum = psegs[2*i].segnum;
		(*num_points)++;
	}
}

int d1_in_d2_ai_create_path_points(object *objp, int start_seg, int end_seg, point_seg *psegs, short *num_points, int max_depth, int random_flag, int safety_flag, int avoid_seg)
{
	if (!native_path_actor(objp))
		return D1_AI_PATH_NOT_APPLICABLE;
	int shared_pool, capacity = path_capacity(psegs, &shared_pool);
	int result = 0, queue_push_count = 0, raw_num_points = 0;
	unsigned int replay_rng_state = 0, replay_rng_call_count = 0;
	const int probe = input_demo_replay_path_probe_active(objp) && d_rand_get_state(&replay_rng_state);
	if (probe)
		replay_rng_call_count = d_rand_get_call_count();
	int		cur_seg;
	int		sidenum;
	int		qtail = 0, qhead = 0;
	int		i;
	sbyte		visited[MAX_SEGMENTS];
	seg_seg	seg_queue[MAX_SEGMENTS];
	short		depth[MAX_SEGMENTS];
	int		cur_depth;
	sbyte		random_xlate[MAX_SIDES_PER_SEGMENT];
	point_seg	*original_psegs = psegs;


	if ((objp->type == OBJ_ROBOT) && (objp->ctype.ai_info.behavior == AIB_RUN_FROM)) {
		random_flag = 1;
		avoid_seg = ConsoleObject->segnum;
		// Int3();
	}

	if (max_depth == -1)
		max_depth = MAX_PATH_LENGTH;

	*num_points = 0;
	if (start_seg < 0 || start_seg > Highest_segment_index || avoid_seg < -1 || avoid_seg > Highest_segment_index)
		return -1;

//	for (i=0; i<=Highest_segment_index; i++) {
//		visited[i] = 0;
//		depth[i] = 0;
//	}
	memset(visited, 0, sizeof(visited[0])*(Highest_segment_index+1));
	memset(depth, 0, sizeof(depth[0])*(Highest_segment_index+1));

	//	If there is a segment we're not allowed to visit, mark it.
	if (avoid_seg != -1) {
		Assert(avoid_seg <= Highest_segment_index);
		if ((start_seg != avoid_seg) && (end_seg != avoid_seg)) {
			visited[avoid_seg] = 1;
			depth[avoid_seg] = 0;
		}
	}

	if (random_flag)
		create_random_xlate(random_xlate);

	cur_seg = start_seg;
	visited[cur_seg] = 1;
	cur_depth = 0;

	while (cur_seg != end_seg) {
		segment	*segp = &Segments[cur_seg];

		for (sidenum = 0; sidenum < MAX_SIDES_PER_SEGMENT; sidenum++) {

			int	snum = sidenum;

			if (random_flag)
				snum = random_xlate[sidenum];

			if (IS_CHILD(segp->children[snum]) && segp->children[snum] <= Highest_segment_index && ((WALL_IS_DOORWAY(segp, snum) & WID_FLY_FLAG) || (d1_in_d2_ai_door_is_openable(objp, segp, snum)))) {
				int	this_seg = segp->children[snum];

				if (!visited[this_seg]) {
					seg_queue[qtail].start = cur_seg;
					seg_queue[qtail].end = this_seg;
					visited[this_seg] = 1;
					depth[qtail++] = cur_depth+1;
					queue_push_count++;
					if (depth[qtail-1] == max_depth) {
						end_seg = seg_queue[qtail-1].end;
						goto cpp_done1;
					}
				}

			}
		}	//	for (sidenum...

		if (qtail <= 0)
			break;

		if (qhead >= qtail) {
			//	Couldn't get to goal, return a path as far as we got, which probably acceptable to the unparticular caller.
			end_seg = seg_queue[qtail-1].end;
			break;
		}

		cur_seg = seg_queue[qhead].end;
		cur_depth = depth[qhead];
		qhead++;

cpp_done1: ;
	}	//	while (cur_seg ...

	if (qtail > 0)
	{
		//	Set qtail to the segment which ends at the goal.
		while (--qtail >= 0 && seg_queue[qtail].end != end_seg) {}
		if (qtail < 0) {
			result = -1;
			goto finished;
		}
	}
	else
		qtail = -1;


	while (qtail >= 0) {
		int	parent_seg, this_seg;

		this_seg = seg_queue[qtail].end;
		parent_seg = seg_queue[qtail].start;
		if (*num_points >= capacity)
			goto exhausted;
		psegs->segnum = this_seg;
		compute_segment_center(&psegs->point,&Segments[this_seg]);
		psegs++;
		(*num_points)++;

		if (parent_seg == start_seg)
			break;

		while (--qtail >= 0 && seg_queue[qtail].end != parent_seg) {}
		Assert(qtail >= 0);
	}

	if (*num_points >= capacity)
		goto exhausted;
	psegs->segnum = start_seg;
	compute_segment_center(&psegs->point,&Segments[start_seg]);
	psegs++;
	(*num_points)++;


	//	Now, reverse point_segs in place.
	for (i=0; i< (*num_points)/2; i++) {
		point_seg		temp_point_seg = *(original_psegs + i);
		*(original_psegs + i) = *(original_psegs + *num_points - i - 1);
		*(original_psegs + *num_points - i - 1) = temp_point_seg;
	}

	//	Now, if safety_flag set, then insert the point at the center of the side connecting two segments
	//	between the two points.  This is messy because we must insert into the list.  The simplest (and not too slow)
	//	way to do this is to start at the end of the list and go backwards.
	raw_num_points = *num_points;
	if (safety_flag) {
		if (2 * *num_points + (shared_pool ? 2 : -1) > capacity) {
			goto exhausted;
		} else {
			insert_center_points(original_psegs, num_points);
		}
	}

	goto finished;
exhausted:
	if (shared_pool)
		ai_reset_all_paths();
	*num_points = 0;
	result = -1;
finished:
	if (probe) {
		input_demo_log_path_probe(objp, start_seg, end_seg, max_depth, random_flag, safety_flag,
			avoid_seg, result, replay_rng_state, replay_rng_call_count);
		input_demo_log_path_detail(objp, start_seg, end_seg, random_flag, random_flag != 0,
			0, 0, queue_push_count, raw_num_points, *num_points);
		input_demo_log_path_points(result ? "create_path_points partial" : "create_path_points final", objp, original_psegs, *num_points);
	}
	return result;
}

int d1_in_d2_ai_create_path_to_player(object *objp, int max_length, int safety_flag)
{
	if (d1_in_d2_ai_actor_role(objp) != D1_AI_NATIVE_ENEMY)
		return 0;
	ai_static	*aip = &objp->ctype.ai_info;
	ai_local		*ailp = &Ai_local_info[objp-Objects];
	int			start_seg, end_seg;
	int			replay_path_request_probe_active;

	if (max_length == -1)
		max_length = MAX_DEPTH_TO_SEARCH_FOR_PLAYER;

	ailp->time_player_seen = GameTime64;			//	Prevent from resetting path quickly.
	ailp->goal_segment = ConsoleObject->segnum;

	start_seg = objp->segnum;
	end_seg = ailp->goal_segment;
	replay_path_request_probe_active = input_demo_replay_path_request_probe_active(objp);
	if (replay_path_request_probe_active)
		input_demo_log_path_request("create_path_to_player begin", objp, start_seg, end_seg, max_length, safety_flag, -1);

	if (end_seg == -1) {
		;
	} else {
		if (d1_in_d2_ai_create_path_points(objp, start_seg, end_seg, Point_segs_free_ptr, &aip->path_length, max_length, 1, safety_flag, -1) != 0)
			return 1;
		aip->hide_index = Point_segs_free_ptr - Point_segs;
		aip->cur_path_index = 0;
		Point_segs_free_ptr += aip->path_length;
		if (Point_segs_free_ptr - Point_segs + MAX_PATH_LENGTH*2 > MAX_POINT_SEGS) {
			//Int3();	//	Contact Mike: This is stupid.  Should call maybe_ai_garbage_collect before the add.
			//force_dump_ai_objects_all("Error in create_path_to_player");
			ai_reset_all_paths();
			return 1;
		}
//		Assert(Point_segs_free_ptr - Point_segs + MAX_PATH_LENGTH*2 < MAX_POINT_SEGS);
		aip->PATH_DIR = 1;		//	Initialize to moving forward.
		aip->flags[4] = D1_AISM_GOHIDE;		//	This forces immediate movement.
		ailp->mode = AIM_FOLLOW_PATH;
		ailp->player_awareness_type = 0;		//	If robot too aware of player, will set mode to chase
	}

	maybe_ai_path_garbage_collect();
	if (replay_path_request_probe_active && (end_seg != -1) && (aip->path_length > 0))
		input_demo_log_path_points("create_path_to_player final", objp, &Point_segs[aip->hide_index], aip->path_length);
	if (replay_path_request_probe_active)
		input_demo_log_path_request(end_seg == -1 ? "create_path_to_player skipped" : "create_path_to_player done", objp, start_seg, end_seg, max_length, safety_flag, -1);

	return 1;
}

int d1_in_d2_ai_create_path_to_station(object *objp, int max_length)
{
	if (d1_in_d2_ai_actor_role(objp) != D1_AI_NATIVE_ENEMY)
		return 0;
	ai_static	*aip = &objp->ctype.ai_info;
	ai_local		*ailp = &Ai_local_info[objp-Objects];
	int			start_seg, end_seg;

	if (max_length == -1)
		max_length = MAX_DEPTH_TO_SEARCH_FOR_PLAYER;

	ailp->time_player_seen = GameTime64;			//	Prevent from resetting path quickly.

	start_seg = objp->segnum;
	end_seg = aip->hide_segment;

	if (end_seg == -1) {
		;
	} else {
		if (d1_in_d2_ai_create_path_points(objp, start_seg, end_seg, Point_segs_free_ptr, &aip->path_length, max_length, 1, 1, -1) != 0)
			return 1;
		aip->hide_index = Point_segs_free_ptr - Point_segs;
		aip->cur_path_index = 0;

		Point_segs_free_ptr += aip->path_length;
		if (Point_segs_free_ptr - Point_segs + MAX_PATH_LENGTH*2 > MAX_POINT_SEGS) {
			//Int3();	//	Contact Mike: Stupid.
			//force_dump_ai_objects_all("Error in create_path_to_station");
			ai_reset_all_paths();
			return 1;
		}
//		Assert(Point_segs_free_ptr - Point_segs + MAX_PATH_LENGTH*2 < MAX_POINT_SEGS);
		aip->PATH_DIR = 1;		//	Initialize to moving forward.
		// aip->SUBMODE = AISM_GOHIDE;		//	This forces immediate movement.
		ailp->mode = AIM_FOLLOW_PATH;
		ailp->player_awareness_type = 0;
	}


	maybe_ai_path_garbage_collect();

	return 1;
}

int d1_in_d2_ai_create_random_path(object *objp, int path_length, int avoid_seg)
{
	if (d1_in_d2_ai_actor_role(objp) != D1_AI_NATIVE_ENEMY)
		return 0;
	ai_static	*aip=&objp->ctype.ai_info;
	ai_local		*ailp = &Ai_local_info[objp-Objects];
	int			requested_path_length = path_length;
	int			replay_path_request_probe_active = input_demo_replay_path_request_probe_active(objp);

	if (replay_path_request_probe_active)
		input_demo_log_path_request("create_n_segment_path begin", objp, objp->segnum, -2, requested_path_length, 0, avoid_seg);

	if (d1_in_d2_ai_create_path_points(objp, objp->segnum, -2, Point_segs_free_ptr, &aip->path_length, path_length, 1, 0, avoid_seg) == -1) {
		Point_segs_free_ptr += aip->path_length;
		while ((--path_length > 0) && (d1_in_d2_ai_create_path_points(objp, objp->segnum, -2, Point_segs_free_ptr, &aip->path_length, path_length, 1, 0, -1) == -1)) {
		}
		if (path_length <= 0)
			return 1;
	}

	aip->hide_index = Point_segs_free_ptr - Point_segs;
	aip->cur_path_index = 0;
	Point_segs_free_ptr += aip->path_length;
	if (Point_segs_free_ptr - Point_segs + MAX_PATH_LENGTH*2 > MAX_POINT_SEGS) {
		//Int3();	//	Contact Mike: This is curious, though not deadly. /eip++;g
		//force_dump_ai_objects_all("Error in crete_n_segment_path 2");
		ai_reset_all_paths();
	}

	aip->PATH_DIR = 1;		//	Initialize to moving forward.
	aip->flags[4] = -1;		//	Don't know what this means.
	ailp->mode = AIM_FOLLOW_PATH;

	maybe_ai_path_garbage_collect();
	if (replay_path_request_probe_active && (aip->path_length > 0))
		input_demo_log_path_points("create_n_segment_path final", objp, &Point_segs[aip->hide_index], aip->path_length);
	if (replay_path_request_probe_active)
		input_demo_log_path_request("create_n_segment_path done", objp, objp->segnum, -2, requested_path_length, 0, avoid_seg);

	return 1;
}


enum { D1_AVOID_SEG_LENGTH = 7 };

static int valid_path_range(const ai_static *state)
{
	return state->path_length > 0 && state->hide_index >= 0 &&
		state->hide_index + state->path_length <= Point_segs_free_ptr - Point_segs;
}

static void create_hide_path(object *objp)
{
	ai_static	*aip = &objp->ctype.ai_info;
	ai_local		*ailp = &Ai_local_info[objp-Objects];
	int			start_seg, end_seg;

	start_seg = objp->segnum;
	end_seg = ailp->goal_segment;

	if (end_seg == -1)
		d1_in_d2_ai_create_random_path(objp, 3, -1);

	if (end_seg == -1) {
		;
	} else {
		if (d1_in_d2_ai_create_path_points(objp, start_seg, end_seg, Point_segs_free_ptr, &aip->path_length, -1, 0, 0, -1) != 0)
			return;
		aip->hide_index = Point_segs_free_ptr - Point_segs;
		aip->cur_path_index = 0;
		Point_segs_free_ptr += aip->path_length;
		if (Point_segs_free_ptr - Point_segs + MAX_PATH_LENGTH*2 > MAX_POINT_SEGS) {
			//Int3();	//	Contact Mike: This is curious, though not deadly. /eip++;g
			//force_dump_ai_objects_all("Error in create_path");
			ai_reset_all_paths();
		}
		aip->PATH_DIR = 1;		//	Initialize to moving forward.
		aip->flags[4] = D1_AISM_HIDING;		//	Pretend we are hiding, so we sit here until bothered.
	}

	maybe_ai_path_garbage_collect();

}

static void set_path_motion(object *objp, vms_vector *goal_point)
{
	vms_vector	cur_vel = objp->mtype.phys_info.velocity;
	vms_vector	norm_cur_vel;
	vms_vector	norm_vec_to_goal;
	vms_vector	cur_pos = objp->pos;
	vms_vector	norm_fvec;
	fix			speed_scale;
	fix			dot;
	robot_info	*robptr = &Robot_info[objp->id];
	fix			max_speed;

	//	If evading player, use highest difficulty level speed, plus something based on diff level
	max_speed = robptr->max_speed[Difficulty_level];
	if (Ai_local_info[objp-Objects].mode == AIM_RUN_FROM_OBJECT)
		max_speed = max_speed*3/2;

	vm_vec_sub(&norm_vec_to_goal, goal_point, &cur_pos);
	vm_vec_normalize_quick(&norm_vec_to_goal);

	norm_cur_vel = cur_vel;
	vm_vec_normalize_quick(&norm_cur_vel);

	norm_fvec = objp->orient.fvec;
	vm_vec_normalize_quick(&norm_fvec);

	dot = vm_vec_dot(&norm_vec_to_goal, &norm_fvec);

	//	If very close to facing opposite desired vector, perturb vector
	if (dot < -15*F1_0/16) {
		norm_cur_vel = norm_vec_to_goal;
	} else {
		norm_cur_vel.x += dxx_ai_path_smoothing_delta(norm_vec_to_goal.x, FrameTime);
		norm_cur_vel.y += dxx_ai_path_smoothing_delta(norm_vec_to_goal.y, FrameTime);
		norm_cur_vel.z += dxx_ai_path_smoothing_delta(norm_vec_to_goal.z, FrameTime);
	}

	vm_vec_normalize_quick(&norm_cur_vel);

	//	Set speed based on this robot type's maximum allowed speed and how hard it is turning.
	//	How hard it is turning is based on the dot product of (vector to goal) and (current velocity vector)
	//	Note that since 3*F1_0/4 is added to dot product, it is possible for the robot to back up.

	//	Set speed and orientation.
	if (dot < 0)
		dot /= -4;

	speed_scale = fixmul(max_speed, dot);
	vm_vec_scale(&norm_cur_vel, speed_scale);
	objp->mtype.phys_info.velocity = norm_cur_vel;

	if (Ai_local_info[objp-Objects].mode == AIM_RUN_FROM_OBJECT)
		d1_in_d2_ai_turn_towards_vector(&norm_vec_to_goal, objp, robptr->turn_time[NDL-1]/2);
	else
		d1_in_d2_ai_turn_towards_vector(&norm_vec_to_goal, objp, robptr->turn_time[Difficulty_level]);

}

int d1_in_d2_ai_follow_path(object *objp, int player_visibility)
{
	if (d1_in_d2_ai_actor_role(objp) != D1_AI_NATIVE_ENEMY)
		return 0;
	ai_static		*aip = &objp->ctype.ai_info;

	vms_vector	goal_point, new_goal_point;
	fix			dist_to_goal;
	// robot_info	*robptr = &Robot_info[objp->id];
	int			forced_break, original_dir, original_index;
	ai_local		*ailp = &Ai_local_info[objp-Objects];
	fix			threshold_distance;

	if ((aip->hide_index == -1) || (aip->path_length == 0))
         {
                if (ailp->mode == AIM_RUN_FROM_OBJECT) {
			d1_in_d2_ai_create_random_path(objp, 5, -1);
			ailp->mode = AIM_RUN_FROM_OBJECT;
		} else
			create_hide_path(objp);
         }

if (aip->path_length > 0 && !valid_path_range(aip)) {
	//Int3();	//	Contact Mike: Bad.  Path goes into what is believed to be free space.
	//force_dump_ai_objects_all("Error in ai_follow_path");
	ai_reset_all_paths();
}

	if (aip->path_length < 2) {
		if (ailp->mode == AIM_RUN_FROM_OBJECT) {
			if (ConsoleObject->segnum == objp->segnum)
				d1_in_d2_ai_create_random_path(objp, D1_AVOID_SEG_LENGTH, -1);			//	Can't avoid segment player is in, robot is already in it! (That's what the -1 is for)
			else
				d1_in_d2_ai_create_random_path(objp, D1_AVOID_SEG_LENGTH, ConsoleObject->segnum);
			ailp->mode = AIM_RUN_FROM_OBJECT;	//	It gets bashed in create_n_segment_path
		} else {
			ailp->mode = AIM_STILL;
		}
		return 1;
	}

	Assert((aip->PATH_DIR == -1) || (aip->PATH_DIR == 1));

	if ((aip->flags[4] == D1_AISM_HIDING) && (aip->behavior == D1_AIB_HIDE))
		return 1;

	/* Native code reads the cursor before correcting it below. Clamp only
	 * the read here so an endpoint cursor cannot access another robot's path */
	const int read_index = max(0, min((int)aip->cur_path_index, (int)aip->path_length - 1));
	goal_point = Point_segs[aip->hide_index + read_index].point;
	dist_to_goal = vm_vec_dist_quick(&goal_point, &objp->pos);

	//	If running from player, only run until can't be seen.
	if (ailp->mode == AIM_RUN_FROM_OBJECT) {
		if ((player_visibility == 0) && (ailp->player_awareness_type == 0)) {
			fix	vel_scale;

			vel_scale = F1_0 - FrameTime/2;
			if (vel_scale < F1_0/2)
				vel_scale = F1_0/2;

			vm_vec_scale(&objp->mtype.phys_info.velocity, vel_scale);

			return 1;
		} else {
			//	If player on path (beyond point robot is now at), then create a new path.
			point_seg	*curpsp = &Point_segs[aip->hide_index];
			int			player_segnum = ConsoleObject->segnum;
			int			i;

			//	This is probably being done every frame, which is wasteful.
			for (i=max(0, (int)aip->cur_path_index); i<aip->path_length; i++)
				if (curpsp[i].segnum == player_segnum) {
					if (player_segnum != objp->segnum)
						d1_in_d2_ai_create_random_path(objp, D1_AVOID_SEG_LENGTH, player_segnum);
					else
						d1_in_d2_ai_create_random_path(objp, D1_AVOID_SEG_LENGTH, -1);
					ailp->mode = AIM_RUN_FROM_OBJECT;	//	It gets bashed in create_n_segment_path
					break;
				}
			if (player_visibility) {
				ailp->player_awareness_type = 1;
				ailp->player_awareness_time = F1_0;
			}
		}
	}

	if (aip->cur_path_index < 0)
		aip->cur_path_index = 0;
	else if (aip->cur_path_index >= aip->path_length)
              {
                if (ailp->mode == AIM_RUN_FROM_OBJECT) {
			d1_in_d2_ai_create_random_path(objp, D1_AVOID_SEG_LENGTH, ConsoleObject->segnum);
			ailp->mode = AIM_RUN_FROM_OBJECT;	//	It gets bashed in create_n_segment_path
		} else
			aip->cur_path_index = aip->path_length-1;
              }

	if (!valid_path_range(aip))
		return 1;
	goal_point = Point_segs[aip->hide_index + aip->cur_path_index].point;

	//	If near goal, pick another goal point.
	forced_break = 0;		//	Gets set for short paths.
	original_dir = aip->PATH_DIR;
	original_index = aip->cur_path_index;
	const fix velocity_mag = vm_vec_mag_quick(&objp->mtype.phys_info.velocity);
	threshold_distance = fixmul(velocity_mag, FrameTime)*2 + F1_0*2;
	if (input_demo_replay_follow_probe_active(objp) && dist_to_goal < threshold_distance) {
		input_demo_log_follow_advance_trigger(objp, dist_to_goal, threshold_distance, velocity_mag);
		input_demo_log_path_robot_state("follow advance trigger", objp);
	}

	while ((dist_to_goal < threshold_distance) && !forced_break) {

		//	Advance to next point on path.
		aip->cur_path_index += aip->PATH_DIR;

		//	See if next point wraps past end of path (in either direction), and if so, deal with it based on mode.
		if ((aip->cur_path_index >= aip->path_length) || (aip->cur_path_index < 0)) {
			if (input_demo_replay_follow_probe_active(objp)) {
				input_demo_log_follow_wrap(objp, player_visibility);
				input_demo_log_path_robot_state("follow wrap", objp);
			}

			//	If mode = hiding, then stay here until get bonked or hit by player.
			if (ailp->mode == D1_AIM_HIDE) {
				ailp->mode = AIM_STILL;
				return 1;		// Stay here until bonked or hit by player.
			} else if (aip->behavior == AIB_STATION) {
				d1_in_d2_ai_create_path_to_station(objp, 15);
				if (!valid_path_range(aip) || aip->hide_segment != Point_segs[aip->hide_index+aip->path_length-1].segnum) {
					ailp->mode = AIM_STILL;
				}
				return 1;
			} else if ((ailp->mode == AIM_FOLLOW_PATH) && (aip->behavior != D1_AIB_FOLLOW_PATH)) {
				d1_in_d2_ai_create_path_to_player(objp, 10, 1);
			} else if (ailp->mode == AIM_RUN_FROM_OBJECT) {
				d1_in_d2_ai_create_random_path(objp, D1_AVOID_SEG_LENGTH, ConsoleObject->segnum);
				ailp->mode = AIM_RUN_FROM_OBJECT;	//	It gets bashed in create_n_segment_path
			} else {
				//	Reached end of the line.  First see if opposite end point is reachable, and if so, go there.
				//	If not, turn around.
				int			opposite_end_index;
				vms_vector	*opposite_end_point;
				fvi_info		hit_data;
				int			fate;
				fvi_query	fq = { 0 };

				// See which end we're nearer and look at the opposite end point.
				if (abs(aip->cur_path_index - aip->path_length) < aip->cur_path_index) {
					//	Nearer to far end (ie, index not 0), so try to reach 0.
					opposite_end_index = 0;
				} else {
					//	Nearer to 0 end, so try to reach far end.
					opposite_end_index = aip->path_length-1;
				}

				opposite_end_point = &Point_segs[aip->hide_index + opposite_end_index].point;

				fq.p0						= &objp->pos;
				fq.startseg				= objp->segnum;
				fq.p1						= opposite_end_point;
				fq.rad					= objp->size;
				fq.thisobjnum			= objp-Objects;
				fq.ignore_obj_list	= NULL;
				fq.flags					= 0; 				//what about trans walls???

				fate = find_vector_intersection(&fq,&hit_data);

				if (fate != HIT_WALL) {
					//	We can be circular!  Do it!
					//	Path direction is unchanged.
					aip->cur_path_index = opposite_end_index;
				} else {
					aip->PATH_DIR = -aip->PATH_DIR;
				}
			}
			break;
		} else {
			new_goal_point = Point_segs[aip->hide_index + aip->cur_path_index].point;
			goal_point = new_goal_point;
			dist_to_goal = vm_vec_dist_quick(&goal_point, &objp->pos);
		}

		//	If went all the way around to original point, in same direction, then get out of here!
//		Cur_index[debug_count] = aip->cur_path_index;
//		Cur_dir[debug_count] = aip->PATH_DIR;
//		debug_count++;
		if ((aip->cur_path_index == original_index) && (aip->PATH_DIR == original_dir)) {
			d1_in_d2_ai_create_path_to_player(objp, 3, 1);
			forced_break = 1;
		}
	}	//	end while

	//	Set velocity (objp->mtype.phys_info.velocity) and orientation (objp->orient) for this object.
	if (input_demo_replay_follow_probe_active(objp) &&
	    (aip->cur_path_index != original_index || aip->PATH_DIR != original_dir)) {
		input_demo_log_follow_advance_result(objp, original_index, original_dir,
			dist_to_goal, threshold_distance, velocity_mag, forced_break);
		input_demo_log_path_robot_state("follow advance result", objp);
	}
	set_path_motion(objp, &goal_point);

	return 1;
}
