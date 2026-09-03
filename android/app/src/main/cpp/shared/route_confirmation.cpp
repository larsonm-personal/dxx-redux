#include "route_confirmation.h"

#include <stdio.h>
#include <string.h>

extern "C" {
#include "ai.h"
#include "automap.h"
#include "collide.h"
#include "cntrlcen.h"
#include "controls.h"
#include "escort.h"
#include "fireball.h"
#include "fvi.h"
#include "game.h"
#include "gameseg.h"
#include "guidebot_extensions.h"
#include "guidebot_route_internal.h"
#include "inferno.h"
#include "laser.h"
#include "maths.h"
#include "object.h"
#include "player.h"
#include "powerup.h"
#include "robot.h"
#include "secretarea.h"
#include "segment.h"
#include "switch.h"
#include "wall.h"
}

#if defined(__ANDROID__) || defined(DXX_GUIDEBOT_ROUTE_PLANNER)

extern "C" int Max_escort_length;

namespace
{
enum controller_phase {
	PHASE_IDLE = 0,
	PHASE_NAVIGATE = 1,
	PHASE_WAIT_FOR_REPLAN = 2
};

struct controller_state {
	route_confirmation_summary summary;
	controller_phase phase;
	escort_route_goal goal;
	level_metadata_route_step step;
	int actor_objnum;
	int target_objnum;
	int target_seg;
	int semantic_target_seg;
	int frontier_wall_num;
	vms_vector target_pos;
	int target_pos_valid;
	int previous_actor_seg;
	vms_vector previous_actor_pos;
	int action_applied;
	unsigned int no_progress_frames;
	unsigned int wait_frames;
	unsigned int frontier_extension_count;
	unsigned int duplicate_objective_count;
	unsigned int frame_time_remainder;
	int64_t carrier_armed_ticks;
	int64_t next_flare_ticks;
	int flare_fallback_wall_num;
	int64_t flare_fallback_ticks;
	fix best_distance;
	int player_objnum;
	int player_control_type;
	int player_movement_type;
	int player_was_invulnerable;
	fix64 player_invulnerable_time;
	physics_info player_physics;
	int player_sandbox_active;
};

controller_state State = {};

int set_visible_flare_target(const object *actor, int segnum, int sidenum,
                             vms_vector *direction);

void capture_rng_boundary(route_confirmation_rng_boundary *boundary)
{
	if (!boundary)
		return;
	d_rand_get_stream_state(D_RNG_SIM, &boundary->simulation.state);
	boundary->simulation.call_count =
	    d_rand_get_stream_call_count(D_RNG_SIM);
	d_rand_get_stream_state(D_RNG_FX, &boundary->effects.state);
	boundary->effects.call_count = d_rand_get_stream_call_count(D_RNG_FX);
}

int valid_object(int objnum)
{
	return objnum >= 0 && objnum <= Highest_object_index &&
	       Objects[objnum].type != OBJ_NONE &&
	       !(Objects[objnum].flags & OF_SHOULD_BE_DEAD);
}

int key_powerup_id(int key_index)
{
	return key_index == 0 ? POW_KEY_BLUE : key_index == 2 ? POW_KEY_GOLD
	                                                      : POW_KEY_RED;
}

int key_player_flag(int key_index)
{
	return key_index == 0 ? PLAYER_FLAGS_BLUE_KEY : key_index == 2 ? PLAYER_FLAGS_GOLD_KEY
	                                                               : PLAYER_FLAGS_RED_KEY;
}

int find_key_object(int key_index)
{
	const int powerup_id = key_powerup_id(key_index);
	for (int objnum = 0; objnum <= Highest_object_index; ++objnum)
		if (valid_object(objnum) && Objects[objnum].type == OBJ_POWERUP &&
		    Objects[objnum].id == powerup_id)
			return objnum;
	return -1;
}

int find_object_in_segment(int type, int segnum)
{
	for (int objnum = 0; objnum <= Highest_object_index; ++objnum)
		if (valid_object(objnum) && Objects[objnum].type == type &&
		    (segnum < 0 || Objects[objnum].segnum == segnum))
			return objnum;
	return -1;
}

int robot_carries_key(const object *objp)
{
	return objp && objp->type == OBJ_ROBOT && objp->contains_count > 0 &&
	       objp->contains_type == OBJ_POWERUP &&
	       objp->contains_id >= POW_KEY_BLUE &&
	       objp->contains_id <= POW_KEY_GOLD;
}

void remove_ordinary_robots(void)
{
	for (int objnum = 0; objnum <= Highest_object_index; ++objnum) {
		object *objp = &Objects[objnum];
		if (objp->type != OBJ_ROBOT || objnum == State.actor_objnum ||
		    Robot_info[objp->id].boss_flag || robot_carries_key(objp)) {
			if (objp->type == OBJ_ROBOT && objnum != State.actor_objnum &&
			    objp->shields >= 0 && !(objp->flags & OF_EXPLODING)) {
				objp->control_type = CT_NONE;
				objp->movement_type = MT_NONE;
				vm_vec_zero(&objp->mtype.phys_info.velocity);
				vm_vec_zero(&objp->mtype.phys_info.thrust);
				vm_vec_zero(&objp->mtype.phys_info.rotvel);
				vm_vec_zero(&objp->mtype.phys_info.rotthrust);
			}
			continue;
		}
		obj_delete(objnum);
	}
}

void restore_player_sandbox(void)
{
	if (!State.player_sandbox_active)
		return;
	if (valid_object(State.player_objnum) &&
	    Objects[State.player_objnum].type == OBJ_PLAYER) {
		object *player = &Objects[State.player_objnum];
		player->control_type = State.player_control_type;
		player->movement_type = State.player_movement_type;
		player->mtype.phys_info = State.player_physics;
	}
	if (State.player_was_invulnerable)
		Players[Player_num].flags |= PLAYER_FLAGS_INVULNERABLE;
	else
		Players[Player_num].flags &= ~PLAYER_FLAGS_INVULNERABLE;
	Players[Player_num].invulnerable_time = State.player_invulnerable_time;
	State.player_sandbox_active = 0;
}

void sandbox_player(object *player)
{
	if (!player)
		return;
	State.player_objnum = (int) (player - Objects);
	State.player_control_type = player->control_type;
	State.player_movement_type = player->movement_type;
	State.player_physics = player->mtype.phys_info;
	State.player_was_invulnerable =
	    (Players[Player_num].flags & PLAYER_FLAGS_INVULNERABLE) != 0;
	State.player_invulnerable_time = Players[Player_num].invulnerable_time;
	State.player_sandbox_active = 1;
	player->control_type = CT_NONE;
	player->movement_type = MT_NONE;
	vm_vec_zero(&player->mtype.phys_info.velocity);
	vm_vec_zero(&player->mtype.phys_info.thrust);
	vm_vec_zero(&player->mtype.phys_info.rotvel);
	vm_vec_zero(&player->mtype.phys_info.rotthrust);
	Players[Player_num].flags |= PLAYER_FLAGS_INVULNERABLE;
	Players[Player_num].invulnerable_time = GameTime64;
}

void fail(int status, const char *problem)
{
	capture_rng_boundary(&State.summary.rng_end);
	State.summary.status = status;
	snprintf(State.summary.problem, sizeof(State.summary.problem), "%s",
	         problem ? problem : "route confirmation failed");
	State.phase = PHASE_IDLE;
	restore_player_sandbox();
}

void set_target_position(const level_metadata_route_step &step)
{
	State.target_pos_valid = 0;
	State.target_objnum = -1;
	State.target_seg = State.goal.target_seg;
	State.frontier_wall_num = -1;
	State.flare_fallback_wall_num = -1;

	if (step.activation_kind ==
	    LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_KEY_CARRIER) {
		State.target_objnum = step.key_carrier_objnum;
		if (!valid_object(State.target_objnum) ||
		    !robot_carries_key(&Objects[State.target_objnum]))
			State.target_objnum = -1;
		if (valid_object(State.target_objnum)) {
			State.target_seg = Objects[State.target_objnum].segnum;
			State.target_pos = Objects[State.target_objnum].pos;
			State.target_pos_valid = 1;
		}
	} else if (step.kind == LEVEL_METADATA_ROUTE_KEY) {
		State.target_objnum = find_key_object(step.key_index);
		if (valid_object(State.target_objnum)) {
			vms_vector segment_center;
			vms_vector contact_offset;
			const object *actor = valid_object(State.actor_objnum)
			                          ? &Objects[State.actor_objnum]
			                          : NULL;
			const object *key = &Objects[State.target_objnum];
			State.target_seg = Objects[State.target_objnum].segnum;
			State.target_pos = key->pos;
			/* Aim for a deterministic point on the pickup contact shell on
			 * the segment-center side of the key.  Steering a large actor at
			 * the key center can pin it against the wall before the two collision
			 * spheres overlap. */
			if (actor) {
				compute_segment_center(&segment_center,
				                       &Segments[State.target_seg]);
				vm_vec_sub(&contact_offset, &segment_center, &key->pos);
				if (vm_vec_normalize_quick(&contact_offset)) {
					vm_vec_scale(&contact_offset,
					             actor->size + key->size - F1_0 / 8);
					vm_vec_add(&State.target_pos, &key->pos,
					           &contact_offset);
				}
			}
			State.target_pos_valid = 1;
		}
	} else if (step.kind == LEVEL_METADATA_ROUTE_REACTOR) {
		State.target_objnum = find_object_in_segment(OBJ_CNTRLCEN, step.seg);
		if (valid_object(State.target_objnum)) {
			if (step.activation_pos_valid && State.goal.target_seg >= 0 &&
			    State.goal.target_seg < Num_segments) {
				State.target_seg = State.goal.target_seg;
				State.target_pos.x = step.activation_pos[0];
				State.target_pos.y = step.activation_pos[1];
				State.target_pos.z = step.activation_pos[2];
			} else {
				State.target_seg = Objects[State.target_objnum].segnum;
				State.target_pos = Objects[State.target_objnum].pos;
			}
			State.target_pos_valid = 1;
		}
	} else if (step.kind == LEVEL_METADATA_ROUTE_BOSS) {
		for (int objnum = 0; objnum <= Highest_object_index; ++objnum)
			if (valid_object(objnum) && Objects[objnum].type == OBJ_ROBOT &&
			    Robot_info[Objects[objnum].id].boss_flag &&
			    (step.seg < 0 || Objects[objnum].segnum == step.seg)) {
				State.target_objnum = objnum;
				if (step.activation_pos_valid && State.goal.target_seg >= 0 &&
				    State.goal.target_seg < Num_segments) {
					State.target_seg = State.goal.target_seg;
					State.target_pos.x = step.activation_pos[0];
					State.target_pos.y = step.activation_pos[1];
					State.target_pos.z = step.activation_pos[2];
				} else {
					State.target_seg = Objects[objnum].segnum;
					State.target_pos = Objects[objnum].pos;
				}
				State.target_pos_valid = 1;
				break;
			}
	} else if ((step.activation_kind ==
	                LEVEL_METADATA_ROUTE_ACTIVATION_FLY_THROUGH_TRIGGER ||
	            step.activation_kind ==
	                LEVEL_METADATA_ROUTE_ACTIVATION_PASS_THROUGH_TRIGGER) &&
	           step.seg >= 0 && step.seg < Num_segments && step.side >= 0 &&
	           step.side < MAX_SIDES_PER_SEGMENT) {
		State.target_seg = step.seg;
		compute_center_point_on_side(&State.target_pos, &Segments[step.seg],
		                             step.side);
		State.target_pos_valid = 1;
	} else if (step.activation_kind ==
	               LEVEL_METADATA_ROUTE_ACTIVATION_ENTER_EXIT &&
	           step.seg >= 0 && step.seg < Num_segments && step.side >= 0 &&
	           step.side < MAX_SIDES_PER_SEGMENT) {
		const int child = Segments[step.seg].children[step.side];
		if (child >= 0 && child < Num_segments) {
			State.target_seg = child;
			compute_segment_center(&State.target_pos, &Segments[child]);
			State.target_pos_valid = 1;
		} else {
			State.target_seg = step.seg;
			compute_center_point_on_side(&State.target_pos,
			                             &Segments[step.seg], step.side);
			State.target_pos_valid = 1;
		}
	} else if (step.activation_pos_valid) {
		State.target_pos.x = step.activation_pos[0];
		State.target_pos.y = step.activation_pos[1];
		State.target_pos.z = step.activation_pos[2];
		State.target_pos_valid = 1;
	} else if (step.switch_guidance_candidate_count > 0) {
		State.target_seg = step.switch_guidance_candidate_seg[0];
		State.target_pos.x = step.switch_guidance_candidate_pos[0][0];
		State.target_pos.y = step.switch_guidance_candidate_pos[0][1];
		State.target_pos.z = step.switch_guidance_candidate_pos[0][2];
		State.target_pos_valid = 1;
	} else if (step.seg >= 0 && step.seg < Num_segments) {
		State.target_seg = step.seg;
		compute_segment_center(&State.target_pos, &Segments[step.seg]);
		State.target_pos_valid = 1;
	}

	if (!State.target_pos_valid && step.wall_num >= 0 &&
	    step.wall_num < Num_walls) {
		const wall *wallp = &Walls[step.wall_num];
		State.target_seg = wallp->segnum;
		compute_center_point_on_side(&State.target_pos,
		                             &Segments[wallp->segnum],
		                             wallp->sidenum);
		State.target_pos_valid = 1;
	}
}

void refine_last_path_point(object *actor)
{
	ai_static *aip;
	point_seg *last;
	if (!actor || !State.target_pos_valid)
		return;
	aip = &actor->ctype.ai_info;
	if (aip->hide_index < 0 || aip->path_length <= 0)
		return;
	last = &Point_segs[aip->hide_index + aip->path_length - 1];
	if (last->segnum == State.target_seg)
		last->point = State.target_pos;
}

int prepare_next_goal(void)
{
	const level_metadata_state *metadata;
	int selected_index = -1;
	int route_goal;
	object *actor;

	if (!valid_object(State.actor_objnum)) {
		fail(ROUTE_CONFIRMATION_FAILED, "Guide-Bot object is no longer valid");
		return 0;
	}
	actor = &Objects[State.actor_objnum];
	level_metadata_rescan_route_from_object(State.actor_objnum);
	if (!level_metadata_prepare_guidebot_path_view(State.actor_objnum)) {
		fail(ROUTE_CONFIRMATION_FAILED, "could not prepare live Guide-Bot route view");
		return 0;
	}
	Escort_route_target_mode = ESCORT_ROUTE_TARGET_END_OF_LEVEL;
	route_goal = escort_route_select_next_goal(&State.goal, &selected_index);
	metadata = level_metadata_get_live_route_state();
	if (route_goal == ESCORT_GOAL_UNSPECIFIED || !State.goal.active ||
	    !metadata || selected_index < 0 ||
	    selected_index >= metadata->route_step_count) {
#if defined(DXX_GUIDEBOT_ROUTE_PLANNER)
		route_planner_plan_summary diagnostic_summary = {};
		const int have_summary =
		    level_metadata_get_live_route_plan_summary(&diagnostic_summary);
		fprintf(stderr,
		        "ROUTE-CONFIRM no_goal route_goal=%d active=%d selected=%d "
		        "metadata=%d steps=%d route_status=%d problem=%s "
		        "summary=%d pending=%d terminal=%d frontier=%d readiness=%s\n",
		        route_goal, State.goal.active, selected_index, metadata != NULL,
		        metadata ? metadata->route_step_count : -1,
		        metadata ? metadata->route_status : -1,
		        metadata ? metadata->route_problem : "",
		        have_summary, diagnostic_summary.first_pending_step,
		        diagnostic_summary.first_pending_path_terminal_segment,
		        diagnostic_summary.partial_frontier_segment,
		        level_metadata_route_readiness_name(
		            level_metadata_get_route_readiness()));
#endif
		fail(ROUTE_CONFIRMATION_FAILED, "live Guide-Bot route has no actionable goal");
		return 0;
	}
	State.step = metadata->route_steps[selected_index];
	/* Publish exactly the selected live goal before asking the same physical
	 * frontier logic used by the in-game Guide-Bot to choose this leg's
	 * reachable navigation endpoint. */
	Escort_route_goal = State.goal;
#if defined(DXX_GUIDEBOT_ROUTE_PLANNER)
	fprintf(stderr,
	        "ROUTE-CONFIRM goal step=%d kind=%d activation=%d actor_seg=%d "
	        "goal_seg=%d step_seg=%d path_terminal=%d activation_pos=%d "
	        "wall=%d trigger=%d label=%s\n",
	        selected_index, State.step.kind, State.step.activation_kind,
	        actor->segnum, State.goal.target_seg, State.step.seg,
	        State.step.path_terminal_segment, State.step.activation_pos_valid,
	        State.step.wall_num, State.step.trigger_num, State.step.label);
#endif
	State.summary.current_route_step_index = selected_index;
	State.summary.current_kind = State.step.kind;
	State.summary.current_activation_kind = State.step.activation_kind;
	set_target_position(State.step);
	{
		const int semantic_target_seg = State.target_seg;
		State.semantic_target_seg = semantic_target_seg;
		const int crossing_exit =
		    State.step.activation_kind ==
		        LEVEL_METADATA_ROUTE_ACTIVATION_ENTER_EXIT &&
		    State.step.seg >= 0 && State.step.seg < Num_segments &&
		    State.step.side >= 0 &&
		    State.step.side < MAX_SIDES_PER_SEGMENT &&
		    Segments[State.step.seg].children[State.step.side] ==
		        semantic_target_seg;
		/* Route to the authored exit side first.  Asking ordinary AI path
		 * generation for the segment beyond an exit can produce a very long
		 * alternate path because the exit wall is not ordinarily traversable.
		 * The final adjacent crossing is installed only after reaching the
		 * source segment. */
		const int physical_target_seg = crossing_exit
		                                    ? escort_route_physical_target(
		                                          actor, State.step.seg,
		                                          Max_escort_length)
		                                    : escort_route_physical_target(
		                                          actor, semantic_target_seg,
		                                          Max_escort_length);
		if (physical_target_seg >= 0 && physical_target_seg < Num_segments &&
		    physical_target_seg != semantic_target_seg) {
			State.target_seg = physical_target_seg;
			compute_segment_center(&State.target_pos,
			                       &Segments[physical_target_seg]);
			State.target_pos_valid = 1;
		}
#if defined(DXX_GUIDEBOT_ROUTE_PLANNER)
		fprintf(stderr,
		        "ROUTE-CONFIRM physical_target semantic=%d navigation=%d "
		        "player_key_flags=%d frontier_keyed=%d\n",
		        semantic_target_seg, State.target_seg,
		        Players[Player_num].flags &
		            (PLAYER_FLAGS_BLUE_KEY | PLAYER_FLAGS_RED_KEY |
		             PLAYER_FLAGS_GOLD_KEY),
		        Escort_route_goal.frontier_player_keyed_door);
		if (State.target_seg == actor->segnum &&
		    State.target_seg != semantic_target_seg) {
			for (int side = 0; side < MAX_SIDES_PER_SEGMENT; ++side) {
				const int child = Segments[actor->segnum].children[side];
				const int wall_num = Segments[actor->segnum].sides[side].wall_num;
				fprintf(stderr,
				        "ROUTE-CONFIRM frontier_side seg=%d side=%d child=%d "
				        "passable=%d wall=%d type=%d state=%d flags=%d keys=%d\n",
				        actor->segnum, side, child,
				        level_metadata_guidebot_side_passable_current(
				            actor->segnum, side),
				        wall_num, wall_num >= 0 ? Walls[wall_num].type : -1,
				        wall_num >= 0 ? Walls[wall_num].state : -1,
				        wall_num >= 0 ? Walls[wall_num].flags : 0,
				        wall_num >= 0 ? Walls[wall_num].keys : 0);
			}
		}
#endif
	}
	if (State.target_seg < 0 || State.target_seg >= Num_segments) {
		fail(ROUTE_CONFIRMATION_FAILED, "route goal has no valid physical target segment");
		return 0;
	}
	if (State.step.activation_kind ==
	        LEVEL_METADATA_ROUTE_ACTIVATION_ENTER_EXIT &&
	    State.target_seg == State.semantic_target_seg)
		create_path_to_segment(actor, State.target_seg, Max_escort_length, 1);
	else
		create_guidebot_route_path_to_segment(actor, State.target_seg,
		                                      Max_escort_length, 1);
	actor->ctype.ai_info.SKIP_AI_COUNT = 0;
	Ai_local_info[State.actor_objnum].mode = AIM_GOTO_OBJECT;
	refine_last_path_point(actor);
	if (actor->segnum != State.target_seg &&
	    actor->ctype.ai_info.path_length <= 0) {
		fail(ROUTE_CONFIRMATION_FAILED, "Guide-Bot path generation returned no path");
		return 0;
	}
	State.previous_actor_seg = actor->segnum;
	State.previous_actor_pos = actor->pos;
	State.action_applied = 0;
	State.no_progress_frames = 0;
	State.wait_frames = 0;
	State.frontier_extension_count = 0;
	State.carrier_armed_ticks =
	    State.step.activation_kind ==
	            LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_KEY_CARRIER
	        ? State.summary.elapsed_ticks
	        : -1;
	State.best_distance = State.target_pos_valid
	                          ? vm_vec_dist_quick(&actor->pos, &State.target_pos)
	                          : 0x7fffffff;
	State.phase = PHASE_NAVIGATE;
	return 1;
}

int extend_current_goal_from_frontier(void)
{
	object *actor;
	int physical_target_seg;
	if (!valid_object(State.actor_objnum)) {
		fail(ROUTE_CONFIRMATION_FAILED, "Guide-Bot object is no longer valid");
		return 0;
	}
	if (++State.frontier_extension_count >= 8) {
		fail(ROUTE_CONFIRMATION_TIMEOUT,
		     "physical route frontier did not open after 8 interaction attempts");
		return 0;
	}
	actor = &Objects[State.actor_objnum];
	if (State.step.activation_kind ==
	        LEVEL_METADATA_ROUTE_ACTIVATION_ENTER_EXIT &&
	    State.step.seg >= 0 && State.step.seg < Num_segments &&
	    State.step.side >= 0 && State.step.side < MAX_SIDES_PER_SEGMENT &&
	    actor->segnum == State.step.seg &&
	    Segments[State.step.seg].children[State.step.side] ==
	        State.semantic_target_seg) {
		State.target_seg = State.semantic_target_seg;
		compute_segment_center(&State.target_pos,
		                       &Segments[State.target_seg]);
		State.target_pos_valid = 1;
		create_path_to_segment(actor, State.target_seg, 4, 1);
		actor->ctype.ai_info.SKIP_AI_COUNT = 0;
		Ai_local_info[State.actor_objnum].mode = AIM_GOTO_OBJECT;
		refine_last_path_point(actor);
		State.previous_actor_seg = actor->segnum;
		State.previous_actor_pos = actor->pos;
		State.no_progress_frames = 0;
		State.wait_frames = 0;
		State.best_distance =
		    vm_vec_dist_quick(&actor->pos, &State.target_pos);
		return 1;
	}
	/* Door and trigger animation gets a chance to settle every frame.  A full
	 * live metadata refresh is only needed periodically while the same physical
	 * frontier remains closed; rebuilding it every simulated second can exhaust
	 * the legacy Guide-Bot planner's shared path workspace. */
	if (State.frontier_extension_count > 1 &&
	    State.frontier_extension_count != 4 &&
	    State.frontier_extension_count != 8) {
		vm_vec_zero(&actor->mtype.phys_info.velocity);
		vm_vec_zero(&actor->mtype.phys_info.thrust);
		State.no_progress_frames = 0;
		State.wait_frames = 0;
		return 1;
	}
	if (!level_metadata_prepare_guidebot_path_view(State.actor_objnum)) {
		fail(ROUTE_CONFIRMATION_FAILED,
		     "could not refresh the Guide-Bot frontier path view");
		return 0;
	}
	Escort_route_goal = State.goal;
	set_target_position(State.step);
	State.semantic_target_seg = State.target_seg;
	physical_target_seg = escort_route_physical_target(
	    actor,
	    State.step.activation_kind ==
	            LEVEL_METADATA_ROUTE_ACTIVATION_ENTER_EXIT
	        ? State.step.seg
	        : State.semantic_target_seg,
	    Max_escort_length);
	if (physical_target_seg >= 0 && physical_target_seg < Num_segments &&
	    physical_target_seg != State.semantic_target_seg) {
		State.target_seg = physical_target_seg;
		compute_segment_center(&State.target_pos,
		                       &Segments[physical_target_seg]);
		State.target_pos_valid = 1;
	}
#if defined(DXX_GUIDEBOT_ROUTE_PLANNER)
	fprintf(stderr,
	        "ROUTE-CONFIRM extend_frontier attempt=%u actor_seg=%d "
	        "semantic=%d navigation=%d keyed=%d\n",
	        State.frontier_extension_count, actor->segnum,
	        State.semantic_target_seg, State.target_seg,
	        Escort_route_goal.frontier_player_keyed_door);
#endif
	if (State.target_seg < 0 || State.target_seg >= Num_segments) {
		fail(ROUTE_CONFIRMATION_FAILED,
		     "frontier extension has no valid physical target segment");
		return 0;
	}
	/* A closed one-sided frontier can legitimately leave the actor in the same
	 * physical target segment for several interaction retries.  Rebuilding a
	 * zero-length Guide-Bot path each second needlessly consumes the shared path
	 * pool and eventually corrupts it. */
	if (actor->segnum != State.target_seg) {
		create_guidebot_route_path_to_segment(actor, State.target_seg,
		                                      Max_escort_length, 1);
		refine_last_path_point(actor);
	}
	actor->ctype.ai_info.SKIP_AI_COUNT = 0;
	Ai_local_info[State.actor_objnum].mode = AIM_GOTO_OBJECT;
	if (actor->segnum != State.target_seg &&
	    actor->ctype.ai_info.path_length <= 0) {
		fail(ROUTE_CONFIRMATION_FAILED,
		     "Guide-Bot frontier extension returned no path");
		return 0;
	}
	State.previous_actor_seg = actor->segnum;
	State.previous_actor_pos = actor->pos;
	State.no_progress_frames = 0;
	State.wait_frames = 0;
	State.best_distance = State.target_pos_valid
	                          ? vm_vec_dist_quick(&actor->pos,
	                                              &State.target_pos)
	                          : 0x7fffffff;
	return 1;
}

int actor_reached_target(const object *actor)
{
	fix distance;
	fix tolerance;
	if (!actor || actor->segnum != State.target_seg)
		return 0;
	if (State.step.activation_kind ==
	        LEVEL_METADATA_ROUTE_ACTIVATION_PICKUP_KEY &&
	    State.target_seg == State.semantic_target_seg &&
	    valid_object(State.target_objnum))
		return vm_vec_dist_quick(
		           &actor->pos, &Objects[State.target_objnum].pos) <=
		       actor->size + Objects[State.target_objnum].size;
	if (!State.target_pos_valid)
		return 1;
	distance = vm_vec_dist_quick(&actor->pos, &State.target_pos);
	tolerance = actor->size + i2f(4);
	return distance <= tolerance;
}

int objective_source(int *segnum, int *sidenum)
{
	if (State.step.wall_num >= 0 && State.step.wall_num < Num_walls) {
		*segnum = Walls[State.step.wall_num].segnum;
		*sidenum = Walls[State.step.wall_num].sidenum;
		return 1;
	}
	if (State.step.seg >= 0 && State.step.seg < Num_segments &&
	    State.step.side >= 0 && State.step.side < MAX_SIDES_PER_SEGMENT) {
		*segnum = State.step.seg;
		*sidenum = State.step.side;
		return 1;
	}
	return 0;
}

int target_is_visible(const object *actor, const object *target)
{
	fvi_query query = {};
	fvi_info hit = {};
	vms_vector start;
	vms_vector end;
	int fate;
	if (!actor || !target)
		return 0;
	start = actor->pos;
	end = target->pos;
	query.p0 = &start;
	query.p1 = &end;
	query.startseg = actor->segnum;
	query.rad = 0;
	query.thisobjnum = (short) (actor - Objects);
	query.ignore_obj_list = NULL;
	query.flags = FQ_CHECK_OBJS | FQ_IGNORE_POWERUPS;
	fate = find_vector_intersection(&query, &hit);
	return fate == HIT_NONE ||
	       (fate == HIT_OBJECT && hit.hit_object == target - Objects);
}

int actor_swept_contacted_object(const object *actor, const object *target)
{
	fvi_query query = {};
	fvi_info hit = {};
	vms_vector start;
	vms_vector end;
	if (!actor || !target)
		return 0;
	if (vm_vec_dist_quick(&actor->pos, &target->pos) <=
	    actor->size + target->size)
		return 1;
	start = State.previous_actor_pos;
	end = actor->pos;
	query.p0 = &start;
	query.p1 = &end;
	query.startseg = State.previous_actor_seg;
	query.rad = actor->size;
	query.thisobjnum = (short) (actor - Objects);
	query.ignore_obj_list = NULL;
	query.flags = FQ_CHECK_OBJS;
	return find_vector_intersection(&query, &hit) == HIT_OBJECT &&
	       hit.hit_object == target - Objects;
}

void record_objective_and_replan(void)
{
	route_confirmation_objective_result *result;
	for (int index = 0; index < State.summary.objective_count; ++index) {
		const route_confirmation_objective_result *completed =
		    &State.summary.objectives[index];
		if (completed->route_step_index ==
		        State.summary.current_route_step_index &&
		    completed->kind == State.step.kind &&
		    completed->activation_kind == State.step.activation_kind) {
			/* Restorer triggers can reactivate an already completed semantic
			 * step while later prerequisites are being resolved.  Execute and
			 * replan it, but keep one timing entry per authored objective. */
			if (++State.duplicate_objective_count > 32)
				fail(ROUTE_CONFIRMATION_TIMEOUT,
				     "semantic objective repeated without route advancement");
			else
				prepare_next_goal();
			return;
		}
	}
	State.duplicate_objective_count = 0;
	if (State.summary.objective_count >= ROUTE_CONFIRMATION_MAX_OBJECTIVES) {
		fail(ROUTE_CONFIRMATION_FAILED, "route objective result capacity exceeded");
		return;
	}
	result = &State.summary.objectives[State.summary.objective_count++];
	memset(result, 0, sizeof(*result));
	result->route_step_index = State.summary.current_route_step_index;
	result->kind = State.step.kind;
	result->activation_kind = State.step.activation_kind;
	result->completed_ticks = State.summary.elapsed_ticks;
	result->completed_frame = State.summary.frame_count;
	snprintf(result->label, sizeof(result->label), "%s", State.step.label);
#if defined(DXX_GUIDEBOT_ROUTE_PLANNER)
	fprintf(stderr,
	        "ROUTE-CONFIRM complete step=%d frame=%u ticks=%lld label=%s\n",
	        result->route_step_index, result->completed_frame,
	        (long long) result->completed_ticks, result->label);
#endif
	if (State.step.kind == LEVEL_METADATA_ROUTE_EXIT) {
		capture_rng_boundary(&State.summary.rng_end);
		State.summary.status = ROUTE_CONFIRMATION_CONFIRMED;
		State.phase = PHASE_IDLE;
		restore_player_sandbox();
		return;
	}
	prepare_next_goal();
}

int actor_is_close_to_side(const object *actor, int segnum, int sidenum)
{
	vms_vector center;
	fix radius = 0;
	int vertices[4];
	if (!actor || segnum < 0 || segnum >= Num_segments || sidenum < 0 ||
	    sidenum >= MAX_SIDES_PER_SEGMENT)
		return 0;
	compute_center_point_on_side(&center, &Segments[segnum], sidenum);
	get_side_verts(vertices, segnum, sidenum);
	for (int vertex = 0; vertex < 4; ++vertex) {
		const fix distance =
		    vm_vec_dist_quick(&center, &Vertices[vertices[vertex]]);
		if (distance > radius)
			radius = distance;
	}
	return vm_vec_dist_quick(&actor->pos, &center) <=
	       radius + actor->size + i2f(2);
}

int apply_flare_fallback(object *actor, int segnum, int sidenum, int wall_num)
{
	vms_vector direction;
	if (!actor || wall_num < 0 || wall_num >= Num_walls ||
	    wall_num != State.flare_fallback_wall_num ||
	    State.summary.elapsed_ticks < State.flare_fallback_ticks ||
	    Walls[wall_num].type != WALL_DOOR ||
	    Walls[wall_num].state != WALL_DOOR_CLOSED ||
	    !actor_is_close_to_side(actor, segnum, sidenum) ||
	    !set_visible_flare_target(actor, segnum, sidenum, &direction))
		return 0;
	wall_hit_process(&Segments[segnum], sidenum, i2f(1000), Player_num,
	                 ConsoleObject);
	State.flare_fallback_wall_num = -1;
#if defined(DXX_GUIDEBOT_ROUTE_PLANNER)
	fprintf(stderr,
	        "ROUTE-CONFIRM close flare fallback wall=%d seg=%d side=%d "
	        "actor_seg=%d\n",
	        wall_num, segnum, sidenum, actor->segnum);
#endif
	return Walls[wall_num].state != WALL_DOOR_CLOSED;
}

void recover_path_door(object *actor)
{
	ai_static *aip;
	int next_index;
	int next_seg;
	int side;
	int wall_num;
	if (!actor)
		return;
	aip = &actor->ctype.ai_info;
	if (aip->hide_index < 0 || aip->path_length <= 0)
		return;
	next_index = aip->cur_path_index + aip->PATH_DIR;
	if (next_index < 0 || next_index >= aip->path_length)
		next_index = aip->cur_path_index;
	if (next_index < 0 || next_index >= aip->path_length)
		return;
	next_seg = Point_segs[aip->hide_index + next_index].segnum;
	if (next_seg == actor->segnum)
		return;
	side = find_connect_side(&Segments[next_seg], &Segments[actor->segnum]);
	if (side < 0)
		return;
	wall_num = Segments[actor->segnum].sides[side].wall_num;
	if (wall_num < 0 || wall_num >= Num_walls)
		return;
	if (Walls[wall_num].type == WALL_DOOR)
		apply_flare_fallback(actor, actor->segnum, side, wall_num);
}

int wall_accepts_route_flare(int wall_num)
{
	const wall *wallp;
	int child;
	if (wall_num < 0 || wall_num >= Num_walls)
		return 0;
	wallp = &Walls[wall_num];
	if (wallp->type == WALL_BLASTABLE)
		return !(wallp->flags & WALL_BLASTED);
	if (wallp->type != WALL_DOOR || wallp->state != WALL_DOOR_CLOSED ||
	    (wallp->flags & WALL_DOOR_LOCKED) ||
	    wallp->clip_num < 0 || wallp->clip_num >= Num_wall_anims)
		return 0;
	if (WallAnims[wallp->clip_num].flags & WCF_HIDDEN) {
		child = Segments[wallp->segnum].children[wallp->sidenum];
		if (child < 0 || child >= Num_segments ||
		    !Automap_visited[wallp->segnum] || !Automap_visited[child])
			return 0;
	}
	return wallp->keys == KEY_NONE ||
	       (wallp->keys & Players[Player_num].flags);
}

int set_visible_flare_target(const object *actor, int segnum, int sidenum,
                             vms_vector *direction)
{
	fvi_query query = {};
	fvi_info hit = {};
	vms_vector start;
	vms_vector target;
	vms_vector end;
	int fate;
	if (!actor || !direction || segnum < 0 || segnum >= Num_segments ||
	    sidenum < 0 || sidenum >= MAX_SIDES_PER_SEGMENT)
		return 0;
	compute_center_point_on_side(&target, &Segments[segnum], sidenum);
	vm_vec_sub(direction, &target, &actor->pos);
	if (!vm_vec_normalize_quick(direction))
		return 0;
	start = actor->pos;
	end = target;
	vm_vec_scale_add2(&end, direction, F1_0);
	query.p0 = &start;
	query.p1 = &end;
	query.startseg = actor->segnum;
	query.rad = 0;
	query.thisobjnum = (short) (actor - Objects);
	query.flags = FQ_IGNORE_POWERUPS;
	fate = find_vector_intersection(&query, &hit);
	return fate == HIT_WALL && hit.hit_side_seg == segnum &&
	       hit.hit_side == sidenum;
}

int find_route_flare_target(object *actor, int *segnum, int *sidenum,
                            int *wall_num, vms_vector *direction)
{
	ai_static *aip;
	int previous_seg;
	if (!actor || !segnum || !sidenum || !wall_num || !direction)
		return 0;
	aip = &actor->ctype.ai_info;
	if (!State.action_applied &&
	    State.step.activation_kind ==
	        LEVEL_METADATA_ROUTE_ACTIVATION_OPEN_HIDDEN_DOOR &&
	    objective_source(segnum, sidenum)) {
		const int objective_wall =
		    Segments[*segnum].sides[*sidenum].wall_num;
		if (objective_wall >= 0 && objective_wall < Num_walls &&
		    Walls[objective_wall].type == WALL_DOOR &&
		    Walls[objective_wall].state == WALL_DOOR_CLOSED &&
		    !(Walls[objective_wall].flags & WALL_DOOR_LOCKED) &&
		    Walls[objective_wall].clip_num >= 0 &&
		    Walls[objective_wall].clip_num < Num_wall_anims &&
		    (WallAnims[Walls[objective_wall].clip_num].flags & WCF_HIDDEN) &&
		    set_visible_flare_target(actor, *segnum, *sidenum, direction)) {
			*wall_num = objective_wall;
			return 1;
		}
	}
	previous_seg = actor->segnum;
	for (int lookahead = 0; aip->hide_index >= 0 && lookahead < 3;
	     ++lookahead) {
		const int path_offset =
		    aip->cur_path_index + (lookahead + 1) * aip->PATH_DIR;
		const int path_index = aip->hide_index + path_offset;
		int side;
		if (path_offset < 0 || path_offset >= aip->path_length)
			break;
		const int next_seg = Point_segs[path_index].segnum;
		if (next_seg == previous_seg)
			continue;
		side = find_connect_side(&Segments[next_seg], &Segments[previous_seg]);
		if (side >= 0) {
			const int candidate_wall =
			    Segments[previous_seg].sides[side].wall_num;
			if (wall_accepts_route_flare(candidate_wall) &&
			    set_visible_flare_target(actor, previous_seg, side,
			                             direction)) {
				*segnum = previous_seg;
				*sidenum = side;
				*wall_num = candidate_wall;
				return 1;
			}
		}
		previous_seg = next_seg;
	}
	if (State.target_seg != State.semantic_target_seg &&
	    actor->segnum == State.target_seg) {
		for (int side = 0; side < MAX_SIDES_PER_SEGMENT; ++side) {
			const int candidate_wall =
			    Segments[actor->segnum].sides[side].wall_num;
			if (Escort_route_goal.frontier_player_keyed_door &&
			    candidate_wall >= 0 && candidate_wall < Num_walls &&
			    Walls[candidate_wall].keys == KEY_NONE)
				continue;
			if (wall_accepts_route_flare(candidate_wall) &&
			    set_visible_flare_target(actor, actor->segnum, side,
			                             direction)) {
				*segnum = actor->segnum;
				*sidenum = side;
				*wall_num = candidate_wall;
				State.frontier_wall_num = candidate_wall;
				return 1;
			}
		}
	}
	return 0;
}

void fire_path_flare(object *actor)
{
	int segnum;
	int sidenum;
	int wall_num;
	int weapon_objnum;
	vms_vector direction;
	vms_vector target;
	if (State.flare_fallback_wall_num >= 0 &&
	    State.flare_fallback_wall_num < Num_walls &&
	    Walls[State.flare_fallback_wall_num].state != WALL_DOOR_CLOSED)
		State.flare_fallback_wall_num = -1;
	if (!actor || State.summary.elapsed_ticks < State.next_flare_ticks ||
	    !find_route_flare_target(actor, &segnum, &sidenum, &wall_num,
	                             &direction))
		return;
	weapon_objnum = Laser_create_new_easy(
	    &direction, &actor->pos, (int) (actor - Objects), FLARE_ID, 1);
	if (weapon_objnum < 0)
		return;
	if (State.flare_fallback_wall_num != wall_num) {
		const fix speed =
		    vm_vec_mag_quick(&Objects[weapon_objnum].mtype.phys_info.velocity);
		compute_center_point_on_side(&target, &Segments[segnum], sidenum);
		State.flare_fallback_wall_num = wall_num;
		State.flare_fallback_ticks =
		    State.summary.elapsed_ticks +
		    (speed > 0 ? fixdiv(vm_vec_dist_quick(&actor->pos, &target), speed)
		               : i2f(2)) +
		    F1_0 / 4;
	}
	State.next_flare_ticks = State.summary.elapsed_ticks + F1_0 / 2;
#if defined(DXX_GUIDEBOT_ROUTE_PLANNER)
	fprintf(stderr,
	        "ROUTE-CONFIRM flare wall=%d seg=%d side=%d actor_seg=%d\n",
	        wall_num, segnum, sidenum, actor->segnum);
#endif
}

void speed_up_actor(object *actor)
{
	if (actor)
		vm_vec_scale(&actor->mtype.phys_info.velocity, 8 * F1_0 / 5);
}

void shoot_frontier_door(object *actor)
{
	if (!actor || State.target_seg == State.semantic_target_seg ||
	    (State.step.activation_kind ==
	         LEVEL_METADATA_ROUTE_ACTIVATION_ENTER_EXIT &&
	     actor->segnum == State.step.seg))
		return;
	for (int side = 0; side < MAX_SIDES_PER_SEGMENT; ++side) {
		const int wall_num = Segments[actor->segnum].sides[side].wall_num;
		if (wall_num < 0 || wall_num >= Num_walls ||
		    (Walls[wall_num].type != WALL_DOOR &&
		     Walls[wall_num].type != WALL_BLASTABLE))
			continue;
		/* Recover only after a real flare aimed at this door had enough time to
		 * arrive.  wall_hit_process remains authoritative for keys and locks. */
		apply_flare_fallback(actor, actor->segnum, side, wall_num);
	}
}

int frontier_door_is_opening(void)
{
	const wall *wallp;
	if (State.frontier_wall_num < 0 ||
	    State.frontier_wall_num >= Num_walls)
		return 0;
	wallp = &Walls[State.frontier_wall_num];
	return wallp->type == WALL_OPEN ||
	       (wallp->flags & (WALL_BLASTED | WALL_DOOR_OPENED)) ||
	       wallp->state == WALL_DOOR_OPENING ||
	       wallp->state == WALL_DOOR_CLOAKING;
}

void apply_objective_action(object *actor)
{
	int segnum;
	int sidenum;
	if (!actor ||
	    (State.action_applied &&
	     State.step.activation_kind !=
	         LEVEL_METADATA_ROUTE_ACTIVATION_OPEN_HIDDEN_DOOR &&
	     State.step.activation_kind !=
	         LEVEL_METADATA_ROUTE_ACTIVATION_FLY_THROUGH_TRIGGER &&
	     State.step.activation_kind !=
	         LEVEL_METADATA_ROUTE_ACTIVATION_PASS_THROUGH_TRIGGER &&
	     State.step.activation_kind !=
	         LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_BOSS))
		return;
	switch (State.step.activation_kind) {
		case LEVEL_METADATA_ROUTE_ACTIVATION_PICKUP_KEY:
			if (valid_object(State.target_objnum) &&
			    actor_swept_contacted_object(
			        actor, &Objects[State.target_objnum])) {
				vms_vector contact = Objects[State.target_objnum].pos;
				collide_player_and_powerup(ConsoleObject,
				                           &Objects[State.target_objnum], &contact);
				if (Players[Player_num].flags &
				    key_player_flag(State.step.key_index))
					record_objective_and_replan();
			}
			break;

		case LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH:
			if (actor_reached_target(actor) &&
			    State.target_seg == State.semantic_target_seg &&
			    objective_source(&segnum, &sidenum)) {
				State.action_applied = 1;
				check_trigger(&Segments[segnum], (short) sidenum,
				              (short) State.actor_objnum, 1);
				record_objective_and_replan();
			}
			break;

		case LEVEL_METADATA_ROUTE_ACTIVATION_OPEN_HIDDEN_DOOR:
			if (objective_source(&segnum, &sidenum)) {
				const int child = Segments[segnum].children[sidenum];
				const int wall_num =
				    Segments[segnum].sides[sidenum].wall_num;
				if (!State.action_applied &&
				    ((wall_num >= 0 && wall_num < Num_walls &&
				      (Walls[wall_num].state == WALL_DOOR_OPENING ||
				       Walls[wall_num].state == WALL_DOOR_WAITING ||
				       Walls[wall_num].state == WALL_DOOR_OPEN)) ||
				     actor_reached_target(actor))) {
					if (wall_num >= 0 && wall_num < Num_walls &&
					    Walls[wall_num].state == WALL_DOOR_CLOSED &&
					    !apply_flare_fallback(actor, segnum, sidenum,
					                          wall_num))
						break;
					if (child < 0 || child >= Num_segments) {
						fail(ROUTE_CONFIRMATION_FAILED,
						     "hidden door has no traversable child segment");
						break;
					}
					State.action_applied = 1;
					State.target_seg = child;
					compute_segment_center(&State.target_pos,
					                       &Segments[child]);
					State.target_pos_valid = 1;
					create_guidebot_route_path_to_segment(
					    actor, child, Max_escort_length, 1);
					actor->ctype.ai_info.SKIP_AI_COUNT = 0;
					Ai_local_info[State.actor_objnum].mode =
					    AIM_GOTO_OBJECT;
					refine_last_path_point(actor);
					State.best_distance = vm_vec_dist_quick(
					    &actor->pos, &State.target_pos);
					State.no_progress_frames = 0;
				}
				if (State.action_applied &&
				    ((State.previous_actor_seg == segnum &&
				      actor->segnum == child) ||
				     (State.previous_actor_seg == child &&
				      actor->segnum == segnum)))
					record_objective_and_replan();
			}
			break;

		case LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_BLASTABLE_WALL:
			if (actor_reached_target(actor) &&
			    State.target_seg == State.semantic_target_seg &&
			    objective_source(&segnum, &sidenum)) {
				State.action_applied = 1;
				wall_hit_process(&Segments[segnum], sidenum, i2f(1000),
				                 Player_num, ConsoleObject);
				record_objective_and_replan();
			}
			break;

		case LEVEL_METADATA_ROUTE_ACTIVATION_FLY_THROUGH_TRIGGER:
		case LEVEL_METADATA_ROUTE_ACTIVATION_PASS_THROUGH_TRIGGER:
			if (objective_source(&segnum, &sidenum)) {
				const int child = Segments[segnum].children[sidenum];
				const int crossed =
				    child >= 0 && child < Num_segments &&
				    ((State.previous_actor_seg == segnum &&
				      actor->segnum == child) ||
				     (State.previous_actor_seg == child &&
				      actor->segnum == segnum));
				if (crossed) {
					check_trigger(&Segments[segnum], (short) sidenum,
					              (short) State.actor_objnum, 0);
					record_objective_and_replan();
					break;
				}
				if (!State.action_applied && actor->segnum == segnum &&
				    actor_reached_target(actor)) {
					if (child < 0 || child >= Num_segments) {
						fail(ROUTE_CONFIRMATION_FAILED,
						     "fly-through trigger has no traversable child segment");
						break;
					}
					State.action_applied = 1;
					State.target_seg = child;
					compute_segment_center(&State.target_pos,
					                       &Segments[child]);
					State.target_pos_valid = 1;
					create_path_to_segment(actor, child, 4, 1);
					actor->ctype.ai_info.SKIP_AI_COUNT = 0;
					Ai_local_info[State.actor_objnum].mode =
					    AIM_GOTO_OBJECT;
					refine_last_path_point(actor);
					State.best_distance = vm_vec_dist_quick(
					    &actor->pos, &State.target_pos);
					State.no_progress_frames = 0;
				}
			}
			break;

		case LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_REACTOR:
			if (actor_reached_target(actor) && valid_object(State.target_objnum) &&
			    target_is_visible(actor, &Objects[State.target_objnum])) {
				State.action_applied = 1;
				apply_damage_to_controlcen(&Objects[State.target_objnum],
				                           Objects[State.target_objnum].shields + F1_0,
				                           (short) (ConsoleObject - Objects));
				record_objective_and_replan();
			}
			break;

		case LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_BOSS:
			if (!State.action_applied && actor_reached_target(actor) &&
			    valid_object(State.target_objnum) &&
			    target_is_visible(actor, &Objects[State.target_objnum])) {
				State.action_applied = 1;
				apply_damage_to_robot(&Objects[State.target_objnum],
				                      Objects[State.target_objnum].shields + F1_0,
				                      ConsoleObject - Objects);
			}
			/* A boss with negative shields remains the live primary objective
			 * throughout its death roll.  Wait for the normal engine transition
			 * that opens the exit before rescanning the route. */
			if (State.action_applied && Control_center_destroyed)
				record_objective_and_replan();
			break;

		case LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_KEY_CARRIER:
			if (!State.action_applied && State.carrier_armed_ticks >= 0 &&
			    State.summary.elapsed_ticks - State.carrier_armed_ticks >=
			        i2f(3) &&
			    valid_object(State.target_objnum)) {
				State.action_applied = 1;
				apply_damage_to_robot(
				    &Objects[State.target_objnum],
				    Objects[State.target_objnum].shields + F1_0,
				    ConsoleObject - Objects);
			}
			if (State.action_applied &&
			    find_key_object(State.step.key_index) >= 0)
				record_objective_and_replan();
			break;

		case LEVEL_METADATA_ROUTE_ACTIVATION_ENTER_EXIT:
			if (objective_source(&segnum, &sidenum)) {
				const int child = Segments[segnum].children[sidenum];
				const int crossed = child >= 0 && child < Num_segments &&
				                    ((State.previous_actor_seg == segnum &&
				                      actor->segnum == child) ||
				                     (State.previous_actor_seg == child &&
				                      actor->segnum == segnum));
				const int touched_one_sided =
				    (child < 0 || child >= Num_segments) &&
				    State.target_seg == State.semantic_target_seg &&
				    actor_reached_target(actor);
				if (!crossed && !touched_one_sided)
					break;
				State.action_applied = 1;
				check_trigger(&Segments[segnum], (short) sidenum,
				              (short) State.actor_objnum, 0);
				if (State.summary.status == ROUTE_CONFIRMATION_RUNNING)
					record_objective_and_replan();
			}
			break;

		default:
			if (actor_reached_target(actor))
				fail(ROUTE_CONFIRMATION_UNSUPPORTED,
				     "route objective activation is not supported");
			break;
	}
}
} // namespace

extern "C" int route_confirmation_start(void)
{
	const game_d_tick_state tick_state = { 0, 0, 0 };
	object *actor;
	memset(&State, 0, sizeof(State));
	State.summary.status = ROUTE_CONFIRMATION_RUNNING;
	State.summary.seed = ROUTE_CONFIRMATION_CANONICAL_SEED;
	State.summary.fixed_hz = ROUTE_CONFIRMATION_FIXED_HZ;
	State.summary.current_route_step_index = -1;
	State.actor_objnum = -1;
	State.target_objnum = -1;
	State.flare_fallback_wall_num = -1;
	State.best_distance = 0x7fffffff;
	d_srand_stream(D_RNG_SIM, ROUTE_CONFIRMATION_CANONICAL_SEED);
	d_srand_stream(D_RNG_FX, ROUTE_CONFIRMATION_CANONICAL_SEED);
	d_rand_reset_stream_call_count(D_RNG_SIM);
	d_rand_reset_stream_call_count(D_RNG_FX);
	capture_rng_boundary(&State.summary.rng_start);
	State.summary.rng_end = State.summary.rng_start;
	game_set_d_tick_state(&tick_state);
	Difficulty_level = 2;
	if (Game_mode != GM_NORMAL) {
		fail(ROUTE_CONFIRMATION_UNSUPPORTED,
		     "canonical route confirmation requires a local single-player game");
		return 0;
	}
	escort_spawn_at_player();
	if (!escort_buddy_is_active() || Buddy_objnum < 0) {
		fail(ROUTE_CONFIRMATION_UNSUPPORTED,
		     "level has no active Guide-Bot verification actor");
		return 0;
	}
	State.actor_objnum = Buddy_objnum;
	actor = &Objects[State.actor_objnum];
#if defined(__ANDROID__) || defined(DXX_GUIDEBOT_ROUTE_DESKTOP)
	/* Manual headed confirmation watches from the simulated actor so the
	 * visible game follows the route instead of remaining at player start. */
	Viewer = actor;
#endif
	/* Headed automation can start several presentation frames after level load.
	 * Always begin from the authored player start, not the player's incidental
	 * gravity-adjusted position at the instant the command was dispatched. */
	actor->pos = Player_init[Player_num].pos;
	actor->last_pos = actor->pos;
	actor->orient = Player_init[Player_num].orient;
	obj_relink(State.actor_objnum, Player_init[Player_num].segnum);
	if (ConsoleObject) {
		ConsoleObject->pos = Player_init[Player_num].pos;
		ConsoleObject->last_pos = ConsoleObject->pos;
		ConsoleObject->orient = Player_init[Player_num].orient;
		obj_relink((int) (ConsoleObject - Objects),
		           Player_init[Player_num].segnum);
		sandbox_player(ConsoleObject);
	}
	vm_vec_zero(&actor->mtype.phys_info.velocity);
	vm_vec_zero(&actor->mtype.phys_info.thrust);
	vm_vec_zero(&actor->mtype.phys_info.rotvel);
	vm_vec_zero(&actor->mtype.phys_info.rotthrust);
	State.summary.player_radius = level_metadata_player_radius_current();
	State.summary.guidebot_radius = actor->size;
	if (State.summary.player_radius > actor->size)
		actor->size = State.summary.player_radius;
	State.summary.effective_radius = actor->size;
	/* Build the canonical level plan once.  Subsequent objective selection must
	 * refresh only the live view from the verification actor; rebuilding the
	 * canonical scan clears the live plan that the Guide-Bot consumes. */
	secret_area_rescan_current_level();
	/* Sandbox normalization is deliberately after the authored canonical scan.
	 * The live view below represents the world the actor actually traverses. */
	remove_ordinary_robots();
	memset(&Controls, 0, sizeof(Controls));
	return prepare_next_goal();
}

extern "C" void route_confirmation_prepare_frame_time(void)
{
	if (State.summary.status != ROUTE_CONFIRMATION_RUNNING)
		return;
	State.frame_time_remainder += F1_0;
	FrameTime =
	    (fix) (State.frame_time_remainder / ROUTE_CONFIRMATION_FIXED_HZ);
	State.frame_time_remainder %= ROUTE_CONFIRMATION_FIXED_HZ;
}

extern "C" void route_confirmation_before_frame(void)
{
	if (State.summary.status != ROUTE_CONFIRMATION_RUNNING)
		return;
	memset(&Controls, 0, sizeof(Controls));
	if (State.player_sandbox_active) {
		Players[Player_num].flags |= PLAYER_FLAGS_INVULNERABLE;
		Players[Player_num].invulnerable_time = GameTime64;
	}
	remove_ordinary_robots();
	/* The route proof must be allowed to finish after demonstrating that the
	 * reactor can be destroyed.  Pause the normal escape countdown through its
	 * engine API so long verification paths do not involuntarily end the level. */
	if (reactor_countdown_is_active() && !Reactor_countdown_paused)
		reactor_countdown_set_paused(1, Countdown_timer);
	if (valid_object(State.actor_objnum))
		fire_path_flare(&Objects[State.actor_objnum]);
	if (valid_object(State.actor_objnum))
		recover_path_door(&Objects[State.actor_objnum]);
	if (valid_object(State.actor_objnum) &&
	    State.wait_frames == 0 &&
	    actor_reached_target(&Objects[State.actor_objnum]))
		shoot_frontier_door(&Objects[State.actor_objnum]);
}

extern "C" void route_confirmation_after_frame(void)
{
	object *actor;
	fix distance;
	if (State.summary.status != ROUTE_CONFIRMATION_RUNNING)
		return;
	State.summary.frame_count++;
	State.summary.elapsed_ticks += FrameTime;
	if (State.summary.frame_count > ROUTE_CONFIRMATION_FIXED_HZ * 600) {
		fail(ROUTE_CONFIRMATION_TIMEOUT,
		     "route confirmation exceeded 10 simulation minutes");
		return;
	}
	if (!valid_object(State.actor_objnum)) {
		fail(ROUTE_CONFIRMATION_FAILED, "Guide-Bot verification actor was lost");
		return;
	}
	actor = &Objects[State.actor_objnum];
	if (actor->segnum >= 0 && actor->segnum < Num_segments)
		Automap_visited[actor->segnum] = 1;
	if (State.target_pos_valid) {
		distance = vm_vec_dist_quick(&actor->pos, &State.target_pos);
		if (distance + F1_0 / 4 < State.best_distance) {
			State.best_distance = distance;
			State.no_progress_frames = 0;
		} else if (++State.no_progress_frames > 3600) {
#if defined(DXX_GUIDEBOT_ROUTE_PLANNER)
			for (int path_index = 0;
			     path_index < actor->ctype.ai_info.path_length; ++path_index) {
				const int point_index = actor->ctype.ai_info.hide_index + path_index;
				const int path_seg = Point_segs[point_index].segnum;
				int path_side = -1;
				int path_wall = -1;
				if (path_index + 1 < actor->ctype.ai_info.path_length) {
					const int next_seg = Point_segs[point_index + 1].segnum;
					path_side = find_connect_side(&Segments[next_seg],
					                              &Segments[path_seg]);
					if (path_side >= 0)
						path_wall = Segments[path_seg].sides[path_side].wall_num;
				}
				fprintf(stderr,
				        "ROUTE-CONFIRM path i=%d seg=%d side=%d wall=%d type=%d "
				        "state=%d flags=%d keys=%d%s\n",
				        path_index, path_seg, path_side, path_wall,
				        path_wall >= 0 ? Walls[path_wall].type : -1,
				        path_wall >= 0 ? Walls[path_wall].state : -1,
				        path_wall >= 0 ? Walls[path_wall].flags : 0,
				        path_wall >= 0 ? Walls[path_wall].keys : 0,
				        path_index == actor->ctype.ai_info.cur_path_index ? " current" : "");
			}
			fprintf(stderr,
			        "ROUTE-CONFIRM stalled actor_seg=%d target_seg=%d dist=%d "
			        "best=%d actor_size=%d target_obj=%d target_size=%d "
			        "path_index=%d path_length=%d hide_index=%d "
			        "pos=(%d,%d,%d) target=(%d,%d,%d)\n",
			        actor->segnum, State.target_seg, distance,
			        State.best_distance, actor->size, State.target_objnum,
			        valid_object(State.target_objnum)
			            ? Objects[State.target_objnum].size
			            : -1,
			        actor->ctype.ai_info.cur_path_index,
			        actor->ctype.ai_info.path_length,
			        actor->ctype.ai_info.hide_index, actor->pos.x, actor->pos.y,
			        actor->pos.z, State.target_pos.x, State.target_pos.y,
			        State.target_pos.z);
#endif
			fail(ROUTE_CONFIRMATION_TIMEOUT,
			     "Guide-Bot made no route progress for 60 simulation seconds");
			return;
		}
	}
	apply_objective_action(actor);
	/* A physical frontier is deliberately not an objective completion.  Hold
	 * there briefly so door animations and trigger effects can settle, then ask
	 * the live planner to extend the same semantic objective from the actor's
	 * actual new position. */
	if (State.summary.status == ROUTE_CONFIRMATION_RUNNING &&
	    actor_reached_target(actor) &&
	    State.target_seg != State.semantic_target_seg) {
		if ((State.step.activation_kind ==
		         LEVEL_METADATA_ROUTE_ACTIVATION_ENTER_EXIT &&
		     actor->segnum == State.step.seg) ||
		    frontier_door_is_opening())
			extend_current_goal_from_frontier();
		else {
			vm_vec_zero(&actor->mtype.phys_info.velocity);
			vm_vec_zero(&actor->mtype.phys_info.thrust);
			if (++State.wait_frames >= ROUTE_CONFIRMATION_FIXED_HZ)
				extend_current_goal_from_frontier();
		}
	}
	if (State.summary.status == ROUTE_CONFIRMATION_RUNNING)
		State.previous_actor_seg = actor->segnum;
	if (State.summary.status == ROUTE_CONFIRMATION_RUNNING)
		State.previous_actor_pos = actor->pos;
}

extern "C" void route_confirmation_stop(void)
{
	if (State.summary.status == ROUTE_CONFIRMATION_RUNNING)
		fail(ROUTE_CONFIRMATION_FAILED, "route confirmation was stopped");
}

extern "C" int route_confirmation_is_terminal(void)
{
	return State.summary.status != ROUTE_CONFIRMATION_IDLE &&
	       State.summary.status != ROUTE_CONFIRMATION_RUNNING;
}

extern "C" int route_confirmation_drive_companion(object *objp)
{
	if (State.summary.status != ROUTE_CONFIRMATION_RUNNING || !objp ||
	    objp - Objects != State.actor_objnum)
		return 0;
	if (actor_reached_target(objp)) {
		vm_vec_zero(&objp->mtype.phys_info.velocity);
		vm_vec_zero(&objp->mtype.phys_info.thrust);
		return 1;
	}
	if (State.target_pos_valid && objp->ctype.ai_info.PATH_DIR > 0 &&
	    objp->ctype.ai_info.path_length > 0 &&
	    objp->ctype.ai_info.cur_path_index >=
	        objp->ctype.ai_info.path_length - 1) {
		ai_path_set_orient_and_vel(objp, &State.target_pos, 2, NULL);
		speed_up_actor(objp);
		return 1;
	}
	objp->ctype.ai_info.SKIP_AI_COUNT = 0;
	Ai_local_info[State.actor_objnum].mode = AIM_GOTO_OBJECT;
	ai_follow_path(objp, 2, 2, NULL);
	speed_up_actor(objp);
	return 1;
}

extern "C" int route_confirmation_handle_exit_trigger(int objnum)
{
	if (State.summary.status != ROUTE_CONFIRMATION_RUNNING ||
	    objnum != State.actor_objnum ||
	    State.step.activation_kind !=
	        LEVEL_METADATA_ROUTE_ACTIVATION_ENTER_EXIT)
		return 0;
	/* The exact authored side was physically crossed and dispatch reached the
	 * normal exit-trigger handler.  Record the proof, but suppress loading the
	 * next level so the harness can serialize this level's result. */
	record_objective_and_replan();
	return 1;
}

extern "C" const route_confirmation_summary *route_confirmation_get_summary(void)
{
	return &State.summary;
}

#else

extern "C" int route_confirmation_start(void)
{
	return 0;
}
extern "C" void route_confirmation_prepare_frame_time(void) {}
extern "C" void route_confirmation_before_frame(void) {}
extern "C" void route_confirmation_after_frame(void) {}
extern "C" void route_confirmation_stop(void) {}
extern "C" int route_confirmation_is_terminal(void)
{
	return 1;
}
extern "C" int route_confirmation_drive_companion(object *)
{
	return 0;
}
extern "C" int route_confirmation_handle_exit_trigger(int)
{
	return 0;
}
extern "C" const route_confirmation_summary *route_confirmation_get_summary(void)
{
	static route_confirmation_summary summary = {};
	summary.status = ROUTE_CONFIRMATION_UNSUPPORTED;
	return &summary;
}

#endif

extern "C" const char *route_confirmation_status_name(int status)
{
	switch (status) {
		case ROUTE_CONFIRMATION_IDLE: return "idle";
		case ROUTE_CONFIRMATION_RUNNING: return "running";
		case ROUTE_CONFIRMATION_CONFIRMED: return "confirmed";
		case ROUTE_CONFIRMATION_PARTIAL: return "partial";
		case ROUTE_CONFIRMATION_FAILED: return "failed";
		case ROUTE_CONFIRMATION_TIMEOUT: return "timeout";
		case ROUTE_CONFIRMATION_UNSUPPORTED: return "unsupported";
		default: return "unknown";
	}
}
