/*
 * D1 weapon behavior adapted from d1/main/laser.c
 * Shared projectile creation, visibility, physics and replay recording stay in the engine
 */

#include <stdlib.h>
#include <stdio.h>
#include "inferno.h"
#include "object.h"
#include "weapon.h"
#include "laser.h"
#include "multi.h"
#include "robot.h"
#include "ai.h"
#include "segpoint.h"
#include "fvi.h"
#include "timer.h"
#include "dxxerror.h"
#include "homing_compat.h"
#include "input_demo_hooks.h"
#include "d1_in_d2.h"
#include "d1_in_d2_weapons.h"

int d1_in_d2_laser_are_related(int first, int second)
{
	const object *a, *b;
	if (!d1_in_d2_use_d1_gameplay())
		return -1;
	if (first < 0 || second < 0)
		return 0;
	a = &Objects[first];
	b = &Objects[second];
	/* Preserve native D1's directional parent check and two-second mine grace */
	if (a->type == OBJ_WEAPON && a->ctype.laser_info.parent_num == second &&
		a->ctype.laser_info.parent_signature == b->signature)
		return a->id != PROXIMITY_ID || a->ctype.laser_info.creation_time + F1_0 * 2 >= GameTime64;
	if (b->type == OBJ_WEAPON && b->ctype.laser_info.parent_num == first &&
		b->ctype.laser_info.parent_signature == a->signature)
		return 1;
	if (a->type != OBJ_WEAPON || b->type != OBJ_WEAPON)
		return 0;
	/* Unlike D2, foreign projectiles participate in collision traversal even
	 * when the collision leaves both alive. Skipping it changes fixed-point travel */
	if (a->ctype.laser_info.parent_signature != b->ctype.laser_info.parent_signature)
		return 0;
	return !is_proximity_bomb_or_smart_mine(a->id) && !is_proximity_bomb_or_smart_mine(b->id);
}

int d1_in_d2_configure_homing(fix turn_time)
{
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	/* Native D1 uses this cone with both original and Redux steering */
	Min_acquirable_dot = HOMING_COMPAT_D1_ACQUISITION_DOT;
	Min_trackable_dot = homing_compat_d1_retention_dot(turn_time, Min_acquirable_dot);
	return 1;
}

static int target_hidden_from(const object *target, const object *tracker)
{
	if (target->type == OBJ_PLAYER)
		return (Players[target->id].flags & PLAYER_FLAGS_CLOAKED) != 0;
	if (target->type == OBJ_ROBOT)
		return target->ctype.ai_info.CLOAKED ||
			(Robot_info[target->id].companion && tracker->ctype.laser_info.parent_type == OBJ_PLAYER);
	return 0;
}

static int target_is_trackable(int target, object *tracker, fix *dot)
{
	vms_vector direction;
	if (target == -1 || (Game_mode & GM_MULTI_COOP))
		return 0;
	if ((target == Players[Player_num].objnum && (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED)) ||
		(Objects[target].type == OBJ_ROBOT && target_hidden_from(&Objects[target], tracker)))
		return 0;
	vm_vec_sub(&direction, &Objects[target].pos, &tracker->pos);
	vm_vec_normalize_quick(&direction);
	*dot = vm_vec_dot(&direction, &tracker->orient.fvec);
	if (*dot < Min_trackable_dot && *dot > F1_0 * 9 / 10) {
		vm_vec_normalize(&direction);
		*dot = vm_vec_dot(&direction, &tracker->orient.fvec);
	}
	return *dot >= Min_trackable_dot && object_to_object_visibility(tracker, &Objects[target], FQ_TRANSWALL);
}

static void trace_target_choice(const char *phase, object *tracker, int target, fix dot)
{
	char probe[192];
	if (!input_demo_replay_homing_desync_probe_active())
		return;
	snprintf(probe, sizeof(probe), "frame=%u phase=%s result=%d dot=%d parent=%d",
		input_demo_trace_frame_index(), phase, target, dot, tracker->ctype.laser_info.parent_num);
	input_demo_append_replay_probe_message("d1_homing_target", tracker, probe);
}

int d1_in_d2_scan_homing_targets(vms_vector *position, object *tracker, int type1, int type2)
{
	int i, best = -1;
	fix best_dot = -2 * F1_0;
	if (!d1_in_d2_use_d1_gameplay())
		return D1_HOMING_NOT_HANDLED;
	if (!Weapon_info[tracker->id].homing_flag) {
		Int3();
		return 0;
	}
	for (i = 0; i <= Highest_object_index; ++i) {
		object *candidate = &Objects[i];
		vms_vector direction;
		fix distance, dot;
		/* Native D1 does not add proximity bombs to the requested object types */
		if ((candidate->type != type1 && candidate->type != type2) ||
			i == tracker->ctype.laser_info.parent_num ||
			target_hidden_from(candidate, tracker))
			continue;
#ifdef NETWORK
		if (candidate->type == OBJ_PLAYER && (Game_mode & GM_TEAM) &&
			Objects[tracker->ctype.laser_info.parent_num].type == OBJ_PLAYER &&
			get_team(candidate->id) == get_team(Objects[tracker->ctype.laser_info.parent_num].id))
			continue;
#endif
		vm_vec_sub(&direction, &candidate->pos, position);
		distance = vm_vec_mag(&direction);
		if (distance >= MAX_TRACKABLE_DIST)
			continue;
		vm_vec_normalize(&direction);
		dot = vm_vec_dot(&direction, &tracker->orient.fvec);
		if (dot > HOMING_COMPAT_D1_ACQUISITION_DOT && dot > best_dot &&
			object_to_object_visibility(tracker, candidate, FQ_TRANSWALL)) {
			best_dot = dot;
			best = i;
			trace_target_choice("scan_candidate", tracker, best, best_dot);
		}
	}
	trace_target_choice("scan_result", tracker, best, best_dot);
	return best;
}

int d1_in_d2_acquire_homing_target(vms_vector *position, object *tracker)
{
	int i, view = -1, best = -1;
	fix best_dot = -2 * F1_0;
	if (!d1_in_d2_use_d1_gameplay())
		return D1_HOMING_NOT_HANDLED;
	if (Game_mode & GM_MULTI) {
		if (tracker->ctype.laser_info.parent_type == OBJ_PLAYER)
			return d1_in_d2_scan_homing_targets(position, tracker,
				(Game_mode & GM_MULTI_COOP) ? OBJ_ROBOT : OBJ_PLAYER,
				(Game_mode & GM_MULTI_COOP) ? -1 : OBJ_ROBOT);
		return d1_in_d2_scan_homing_targets(position, tracker, OBJ_PLAYER, -1);
	}
	if (tracker->ctype.laser_info.parent_num != Players[Player_num].objnum)
		return (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED) ? -1 : (int)(ConsoleObject - Objects);
	if (!input_demo_replay_is_loaded()) {
		/* D2 exposes the main view separately from optional HUD cameras */
		for (i = 0; i < MAX_RENDERED_WINDOWS; ++i)
			if (Window_rendered_data[i].time >= timer_query() - 1 &&
				Window_rendered_data[i].viewer == ConsoleObject && !Window_rendered_data[i].rear_view) {
				view = i;
				break;
			}
	}
	if (view == -1)
		return d1_in_d2_scan_homing_targets(position, tracker, OBJ_ROBOT, -1);
	for (i = Window_rendered_data[view].num_objects - 1; i >= 0; --i) {
		const int target = Window_rendered_data[view].rendered_objects[i];
		vms_vector direction;
		fix dot;
		if (target == Players[Player_num].objnum ||
			(Objects[target].type == OBJ_ROBOT && target_hidden_from(&Objects[target], tracker)))
			continue;
		vm_vec_sub(&direction, &Objects[target].pos, position);
		vm_vec_normalize_quick(&direction);
		dot = vm_vec_dot(&direction, &tracker->orient.fvec);
		/* The original rendered-list acquisition has no complete-scan distance cap */
		if (dot > HOMING_COMPAT_D1_ACQUISITION_DOT && dot > best_dot &&
			object_to_object_visibility(tracker, &Objects[target], FQ_TRANSWALL)) {
			best = target;
			best_dot = dot;
		}
	}
	trace_target_choice("rendered_result", tracker, best, best_dot);
	return best;
}

int d1_in_d2_track_homing_target(int target, object *tracker, fix *dot, unsigned frame, int original_homing)
{
	unsigned phase;
	int type1, type2 = -1;
	if (!d1_in_d2_use_d1_gameplay())
		return D1_HOMING_NOT_HANDLED;
	if (target_is_trackable(target, tracker, dot))
		return target;
	phase = original_homing ? (unsigned)((tracker - Objects) ^ frame) :
		frame - tracker->ctype.laser_info.creation_framecount;
	if ((phase % 4) != 0)
		return -1;
	if (Objects[tracker->ctype.laser_info.parent_num].type == OBJ_PLAYER) {
		if (target == -1) {
			if (Game_mode & GM_MULTI_COOP)
				type1 = OBJ_ROBOT;
			else {
				type1 = OBJ_PLAYER;
				if (!(Game_mode & GM_MULTI) || (Game_mode & GM_MULTI_ROBOTS))
					type2 = OBJ_ROBOT;
			}
		} else {
			type1 = Objects[tracker->ctype.laser_info.track_goal].type;
			if (type1 != OBJ_PLAYER && type1 != OBJ_ROBOT)
				return -1;
		}
	} else
		type1 = target == -1 ? OBJ_PLAYER : Objects[tracker->ctype.laser_info.track_goal].type;
	return d1_in_d2_scan_homing_targets(&tracker->pos, tracker, type1, type2);
}

int d1_in_d2_primary_projectile(int default_projectile)
{
	/* D1 record 12 supplies firing costs/timing; record 20 is the visible shot */
	if (d1_in_d2_has_native_assets() && default_projectile == SPREADFIRE_ID)
		return 20;
	return default_projectile;
}

int d1_in_d2_initialize_player_weapon(object *weapon, fix fusion_charge, int game_mode)
{
	int fusion_scale;

	if (!d1_in_d2_use_d1_gameplay())
		return 0;

	/* D1 quad lasers keep full per-bolt damage */
	weapon->ctype.laser_info.multiplier = F1_0;
	if (weapon->id != FUSION_ID)
		return 1;

	fusion_scale = (game_mode & GM_MULTI) ? 2 : 4;
	if (fusion_charge > 0) {
		if (fusion_charge <= F1_0 * fusion_scale)
			weapon->ctype.laser_info.multiplier = F1_0 + fusion_charge / 2;
		else
			weapon->ctype.laser_info.multiplier = F1_0 * fusion_scale;
	}
	if (game_mode & GM_MULTI)
		weapon->ctype.laser_info.multiplier /= 2;
	return 1;
}

int d1_in_d2_position_weapon(object *weapon, const object *parent, const vms_vector *direction, fix length)
{
	vms_vector end;
	int segment;
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	if (parent->type == OBJ_WEAPON || Weapon_info[weapon->id].render_type == WEAPON_RENDER_NONE || weapon->id == FLARE_ID)
		return 1;
	/* D1 advances robot/reactor shots as well as player shots to the gun tip */
	vm_vec_scale_add(&end, &weapon->pos, direction, length / 2);
	segment = find_point_seg(&end, weapon->segnum);
	if (segment == weapon->segnum)
		weapon->pos = end;
	else if (segment != -1) {
		weapon->pos = end;
		obj_relink(weapon - Objects, segment);
	}
	return 1;
}

void d1_in_d2_notify_player_fire(object *player, int weapon_type)
{
	/* D1 generates awareness at impacts, not at this D2 firing phase */
	if (d1_in_d2_use_d1_gameplay())
		return;
	input_demo_set_awareness_source("laser_player_fire", player - Objects, weapon_type);
	create_awareness_event(player, PA_WEAPON_WALL_COLLISION);
}

int d1_in_d2_limit_weapon_speed(object *weapon)
{
	fix speed, maximum;
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	if (!Weapon_info[weapon->id].thrust)
		return 1;
	maximum = Weapon_info[weapon->id].speed[Difficulty_level];
	if ((Game_mode & GM_MULTI) && Netgame.OriginalD1Weapons && weapon->id == SPREADFIRE_ID)
		maximum = 200 * F1_0;
	speed = vm_vec_mag_quick(&weapon->mtype.phys_info.velocity);
	if (speed > maximum && maximum)
		vm_vec_scale(&weapon->mtype.phys_info.velocity, fixdiv(maximum, speed));
	return 1;
}

int d1_in_d2_weapon_wall_impact_allowed(const object *weapon)
{
	return !d1_in_d2_use_d1_gameplay() || !(weapon->mtype.phys_info.flags & PF_BOUNCE);
}

int d1_in_d2_smart_child(const object *parent, int default_child)
{
	if (d1_in_d2_use_d1_gameplay() && parent->type == OBJ_WEAPON && parent->id == SMART_ID) {
		/* Earlier D1 editions have no separate, easier-to-dodge robot child */
		if (parent->ctype.laser_info.parent_type == OBJ_ROBOT && N_weapon_types > ROBOT_SMART_HOMING_ID)
			return ROBOT_SMART_HOMING_ID;
		return PLAYER_SMART_HOMING_ID;
	}
	return default_child;
}

int d1_in_d2_turn_homing_weapon(object *weapon, int target, fix *dot, fix turn_time, int original_homing)
{
	vms_vector to_target, velocity;
	fix speed, max_speed, absdot;

	if (!d1_in_d2_use_d1_gameplay())
		return 0;

	vm_vec_sub(&to_target, &Objects[target].pos, &weapon->pos);
	vm_vec_normalize_quick(&to_target);
	velocity = weapon->mtype.phys_info.velocity;
	speed = vm_vec_normalize_quick(&velocity);
	if (original_homing)
		*dot = vm_vec_dot(&velocity, &to_target);
	max_speed = Weapon_info[weapon->id].speed[Difficulty_level];
	if ((Game_mode & GM_MULTI) && Netgame.OriginalD1Weapons && weapon->id == SPREADFIRE_ID)
		max_speed = 200 * F1_0;
	if (speed + F1_0 < max_speed) {
		speed += fixmul(max_speed, turn_time / 2);
		if (speed > max_speed)
			speed = max_speed;
	}

	vm_vec_add2(&velocity, &to_target);
	/* The boss' smart children track better */
	if (Weapon_info[weapon->id].render_type != WEAPON_RENDER_POLYMODEL)
		vm_vec_add2(&velocity, &to_target);
	vm_vec_normalize_quick(&velocity);
	vm_vec_scale(&velocity, speed);
	weapon->mtype.phys_info.velocity = velocity;

	absdot = abs(F1_0 - *dot);
	if (original_homing) {
		if (absdot > F1_0 / 8) {
			if (absdot > F1_0 / 4)
				absdot = F1_0 / 4;
			weapon->lifeleft -= fixmul(absdot * 16, turn_time);
		}
	} else
		weapon->lifeleft -= fixmul(absdot * 32, turn_time);

	if (Weapon_info[weapon->id].render_type == WEAPON_RENDER_POLYMODEL) {
		vms_vector forward = velocity;
		/* Preserve the existing D1 steering phase's fixed-point evaluation order */
		vm_vec_scale(&forward, (original_homing ? turn_time : FrameTime) * 8);
		vm_vec_add2(&forward, &weapon->orient.fvec);
		vm_vec_normalize_quick(&forward);
		vm_vector_2_matrix(&weapon->orient, &forward, NULL, NULL);
	}
	return 1;
}
