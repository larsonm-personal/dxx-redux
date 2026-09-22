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
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/* New file, largely derived from the original Parallax/Interplay Descent code */

/*
 *
 * Small D1-in-D2 gameplay policy helpers.
 *
 */

#include "d1_in_d2_semantics.h"

#include <stdio.h>
#include <string.h>
#include "d1_in_d2.h"
#include "d1_in_d2_ai.h"
#include "bm.h"
#include "cntrlcen.h"
#include "ai.h"
#include "fireball.h"
#include "fvi.h"
#include "game.h"
#include "gameseq.h"
#include "laser.h"
#include "gameseg.h"
#include "object.h"
#include "player.h"
#include "powerup.h"
#include "robot.h"
#include "piggy.h"
#include "rle.h"
#include "textures.h"
#include "texmerge.h"
#include "wall.h"
#include "weapon.h"
#include "vclip.h"
#include "multi.h"
#include "input_demo_hooks.h"
#include "input_demo_debug_logging.h"
#include "input_demo_replay.h"

fix d1_in_d2_pickup_boost(fix base_boost)
{
	return !d1_in_d2_use_d1_gameplay() && Difficulty_level == 0 ? base_boost + base_boost / 2 : base_boost;
}

fix d1_in_d2_boss_health_maximum(fix strength, fix engine_maximum)
{
	return d1_in_d2_use_d1_gameplay() ? strength : engine_maximum;
}

int d1_in_d2_prepare_vulcan_pickup(int weapon_index, int new_weapon, int duplicate_reward, int *ammo)
{
	if (!d1_in_d2_use_d1_gameplay() || weapon_index != VULCAN_INDEX)
		return 0;
	if (!new_weapon && duplicate_reward) {
		*ammo = VULCAN_AMMO_AMOUNT;
		return 1;
	}
	if (new_weapon) {
		if (*ammo < VULCAN_WEAPON_AMMO_AMOUNT)
			*ammo = VULCAN_WEAPON_AMMO_AMOUNT;
		if ((Game_mode & GM_MULTI) && !(Game_mode & GM_MULTI_COOP) &&
			Netgame.LowVulcan && *ammo > VULCAN_WEAPON_AMMO_AMOUNT / 2)
			*ammo = VULCAN_WEAPON_AMMO_AMOUNT / 2;
	}
	return 0;
}

void d1_in_d2_initialize_robot_drop_count(const object *container, object *created)
{
	if (container->type != OBJ_ROBOT || container->contains_type != OBJ_POWERUP)
		return;
	/* D1 enemy drops keep obj_create's count; pickup supplies the ammo minimum
	 * The optional D2 Guide-Bot still uses D2 drop rules in a D1 game */
	if (d1_in_d2_use_d1_gameplay() && !Robot_info[container->id].companion)
		return;
	/* Ordinary D2 robots and the optional Guide-Bot use D2 ammo/charge counts */
	if (container->contains_id == POW_VULCAN_WEAPON || container->contains_id == POW_GAUSS_WEAPON)
		created->ctype.powerup_info.count = VULCAN_WEAPON_AMMO_AMOUNT;
	else if (container->contains_id == POW_OMEGA_WEAPON)
		created->ctype.powerup_info.count = MAX_OMEGA_CHARGE;
}

fix d1_in_d2_released_flare_lifetime(void)
{
	return d1_in_d2_use_d1_gameplay() ? F1_0 / 4 : F1_0 / 8;
}

int d1_in_d2_remove_obsolete_stuck_objects(void)
{
	stuckobj *entry;
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	entry = &Stuck_objects[(unsigned)d_tick_count % MAX_STUCK_OBJECTS];
	/* Native D1 retires the registry entry without shortening the object's life */
	if (entry->wallnum != -1 &&
		(entry->wallnum == 0 || Objects[entry->objnum].signature != entry->signature)) {
		--Num_stuck_objects;
		entry->wallnum = -1;
	}
	return 1;
}

fix d1_in_d2_contact_damage(fix damage)
{
	return !d1_in_d2_use_d1_gameplay() && Difficulty_level == 0 ? damage / 2 : damage;
}

fix d1_in_d2_blast_damage(fix damage)
{
	return !d1_in_d2_use_d1_gameplay() && Difficulty_level == 0 ? damage / 4 : damage;
}

void d1_in_d2_initialize_object_orientation(object *obj, const vms_matrix *orientation)
{
	if (orientation)
		obj->orient = *orientation;
	else if (d1_in_d2_use_d1_gameplay() &&
		(obj->type != OBJ_ROBOT || d1_in_d2_ai_actor_role(obj) == D1_AI_NATIVE_ENEMY))
		/* D1 leaves the newly cleared matrix alone when no orientation is supplied */
		memset(&obj->orient, 0, sizeof(obj->orient));
	else
		obj->orient = vmd_identity_matrix;
}

int d1_in_d2_animate_powerup(object *obj)
{
	vclip_info *animation;
	vclip *clip;
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	animation = &obj->rtype.vclip_info;
	clip = &Vclip[animation->vclip_num];
	animation->frametime -= FrameTime;
	while (animation->frametime < 0) {
		animation->frametime += clip->frame_time;
		if (++animation->framenum >= clip->num_frames)
			animation->framenum = 0;
	}
	return 1;
}

fix d1_in_d2_small_fireball_size(const object *obj, fix scale, int random_value)
{
	const int native = d1_in_d2_use_d1_gameplay() &&
		(obj->type != OBJ_ROBOT || d1_in_d2_ai_actor_role(obj) == D1_AI_NATIVE_ENEMY);
	return fixmul(scale, native ? F1_0 + random_value * 4 : F1_0 / 2 + random_value * 2);
}

int d1_in_d2_volatile_weapon_impact(object *weapon, short segment, vms_vector *hit_point)
{
	weapon_info *wi;
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	wi = &Weapon_info[weapon->id];
	object_create_badass_explosion(weapon, segment, hit_point,
		wi->impact_size + i2f(3), VCLIP_VOLATILE_WALL_HIT,
		wi->strength[Difficulty_level] / 4 + i2f(10),
		wi->damage_radius + i2f(30),
		wi->strength[Difficulty_level] / 2 + i2f(5),
		weapon->ctype.laser_info.parent_num);
	return 1;
}

int d1_in_d2_dead_reactor_effects(void)
{
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	if (Dead_controlcen_object_num != -1 && Countdown_seconds_left > 0)
		if (d_rand_fx() < FrameTime * 4)
			create_small_fireball_on_object(&Objects[Dead_controlcen_object_num], F1_0 * 3, 1);
	return 1;
}

int d1_in_d2_robot_contact_allowed(const object *robot)
{
	return d1_in_d2_ai_actor_role(robot) == D1_AI_NATIVE_ENEMY || !(robot->flags & OF_EXPLODING);
}

int d1_in_d2_robot_pair_collides(const object *first, const object *second)
{
	/* Native D1 collides two melee robots. D2 disabled this to avoid clumping */
	return d1_in_d2_ai_actor_role(first) == D1_AI_NATIVE_ENEMY &&
	       d1_in_d2_ai_actor_role(second) == D1_AI_NATIVE_ENEMY &&
	       Robot_info[first->id].attack_type && Robot_info[second->id].attack_type;
}

static int uses_native_physics(const object *obj)
{
	return d1_in_d2_use_d1_gameplay() &&
	       (obj->type != OBJ_ROBOT || d1_in_d2_ai_actor_role(obj) == D1_AI_NATIVE_ENEMY);
}

int d1_in_d2_bounce_preserves_velocity(const object *obj)
{
	return !uses_native_physics(obj);
}

int d1_in_d2_door_wait_elapsed(const active_door *door)
{
	return d1_in_d2_use_d1_gameplay() ? door->time > DOOR_WAIT_TIME : -1;
}

int d1_in_d2_door_close_blocked(const active_door *door)
{
	int part;
	const wall *front;
	if (!d1_in_d2_use_d1_gameplay())
		return -1;
	front = &Walls[door->front_wallnum[0]];
	if (!(front->flags & WALL_DOOR_AUTO))
		return 0;
	/* Native D1 checks the first doorway for linked parts, including weapons
	 * and fireballs. Preserve that source behavior rather than D2's reopen rule */
	for (part = 0; part < door->n_parts; ++part) {
		segment *seg = &Segments[front->segnum];
		segment *connected = &Segments[seg->children[front->sidenum]];
		int back_side = find_connect_side(seg, connected);
		int objnum;
		for (objnum = seg->objects; objnum != -1; objnum = Objects[objnum].next)
			if (check_poke(objnum, seg - Segments, front->sidenum))
				return 1;
		for (objnum = connected->objects; objnum != -1; objnum = Objects[objnum].next)
			if (check_poke(objnum, connected - Segments, back_side))
				return 1;
	}
	return 0;
}

/* Native d1/main/fireball.c::maybe_replace_powerup_with_energy, using the
 * shared nearby-object query and existing player inventory */
int d1_in_d2_replace_powerup(object *del_obj)
{
	int weapon_index = -1;
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	if (del_obj->contains_type != OBJ_POWERUP)
		return 1;
	if (del_obj->contains_id == POW_CLOAK) {
		if (weapon_nearby(del_obj, del_obj->contains_id))
			del_obj->contains_count = 0;
		return 1;
	}
	switch (del_obj->contains_id) {
		case POW_VULCAN_WEAPON: weapon_index = VULCAN_INDEX; break;
		case POW_SPREADFIRE_WEAPON: weapon_index = SPREADFIRE_INDEX; break;
		case POW_PLASMA_WEAPON: weapon_index = PLASMA_INDEX; break;
		case POW_FUSION_WEAPON: weapon_index = FUSION_INDEX; break;
	}
	// Don't drop vulcan ammo if player maxed out
	if (((weapon_index == VULCAN_INDEX) || del_obj->contains_id == POW_VULCAN_AMMO) &&
	    Players[Player_num].primary_ammo[VULCAN_INDEX] >= VULCAN_AMMO_MAX)
		del_obj->contains_count = 0;
	else if (weapon_index != -1) {
		if ((player_has_weapon(Player_num, weapon_index, 0) & HAS_WEAPON_FLAG) || weapon_nearby(del_obj, del_obj->contains_id)) {
			// SIM RNG: decides whether any duplicate-weapon replacement drops
			if (d_rand() > 16384) {
				del_obj->contains_count = 1;
				del_obj->contains_type = OBJ_POWERUP;
				del_obj->contains_id = weapon_index == VULCAN_INDEX ? POW_VULCAN_AMMO : POW_ENERGY;
			} else
				del_obj->contains_count = 0;
		}
	} else if (del_obj->contains_id == POW_QUAD_FIRE) {
		if ((Players[Player_num].flags & PLAYER_FLAGS_QUAD_LASERS) || weapon_nearby(del_obj, del_obj->contains_id)) {
			// SIM RNG: decides whether the duplicate-quad replacement drops
			if (d_rand() > 16384) {
				del_obj->contains_count = 1;
				del_obj->contains_type = OBJ_POWERUP;
				del_obj->contains_id = POW_ENERGY;
			} else
				del_obj->contains_count = 0;
		}
	}
	// Gated robots must not fill the boss room with energy
	if (del_obj->matcen_creator == BOSS_GATE_MATCEN_NUM &&
	    del_obj->contains_id == POW_ENERGY && del_obj->contains_type == OBJ_POWERUP)
		del_obj->contains_count = 0;
	// Change multiplayer extra-lives into invulnerability
	if ((Game_mode & GM_MULTI) && del_obj->contains_id == POW_EXTRA_LIFE)
		del_obj->contains_id = POW_INVULNERABILITY;
	return 1;
}

/* Native acquisition, firing and death-silence order from d1/main/cntrlcen.c */
int d1_in_d2_reactor_frame(object *obj)
{
	int			best_gun_num;
	if (!d1_in_d2_use_d1_gameplay())
		return 0;

	//	If a boss level, then Control_center_present will be 0.
	if (!Control_center_present)
		return 1;

#ifndef NDEBUG
	if (cheats.robotfiringsuspended || (Game_suspended & SUSP_ROBOTS))
		return 1;
#else
	if (cheats.robotfiringsuspended)
		return 1;
#endif

	if (!(Control_center_been_hit || Control_center_player_been_seen)) {
		if (!(d_tick_count % 8)) {		//	Do every so often...
			vms_vector	vec_to_player;
			fix			dist_to_player;
			int			i;
			segment		*segp = &Segments[obj->segnum];

			// This is a hack.  Since the control center is not processed by
			// ai_do_frame, it doesn't know to deal with cloaked dudes.  It
			// seems to work in single-player mode because it is actually using
			// the value of Believed_player_position that was set by the last
			// person to go through ai_do_frame.  But since a no-robots game
			// never goes through ai_do_frame, I'm making it so the control
			// center can spot cloaked dudes.
#ifdef NETWORK
			if (Game_mode & GM_MULTI)
				Believed_player_pos = Objects[Players[Player_num].objnum].pos;
#endif
			//	Hack for special control centers which are isolated and not reachable because the
			//	real control center is inside the boss.
			for (i=0; i<MAX_SIDES_PER_SEGMENT; i++)
				if (segp->children[i] != -1)
					break;
			if (i == MAX_SIDES_PER_SEGMENT)
				return 1;

			vm_vec_sub(&vec_to_player, &ConsoleObject->pos, &obj->pos);
			dist_to_player = vm_vec_normalize_quick(&vec_to_player);
			if (dist_to_player < F1_0*200) {
				Control_center_player_been_seen = player_is_visible_from_object(obj, &obj->pos, 0, &vec_to_player);
				Control_center_next_fire_time = 0;
			}
		}

		return 1;
	}

	if(is_observer()) {
		Control_center_player_been_seen = 0;
		return 1;
	}

	if (Player_is_dead)
		controlcen_death_silence += FrameTime;
	else
		controlcen_death_silence = 0;

	if ((Control_center_next_fire_time < 0) && !(controlcen_death_silence > F1_0*2)) {
		reactor *reactor = get_reactor_definition(obj->id);
		if (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED)
			best_gun_num = calc_best_gun(reactor->n_guns, obj, &Believed_player_pos);
		else
			best_gun_num = calc_best_gun(reactor->n_guns, obj, &ConsoleObject->pos);

		if (best_gun_num != -1) {
			vms_vector	vec_to_goal;
			fix			dist_to_player;
			fix			delta_fire_time;

			if (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED) {
				vm_vec_sub(&vec_to_goal, &Believed_player_pos, &obj->ctype.reactor_info.gun_pos[best_gun_num]);
				dist_to_player = vm_vec_normalize_quick(&vec_to_goal);
			} else {
				vm_vec_sub(&vec_to_goal, &ConsoleObject->pos, &obj->ctype.reactor_info.gun_pos[best_gun_num]);
				dist_to_player = vm_vec_normalize_quick(&vec_to_goal);
			}

			if (dist_to_player > F1_0*300)
			{
				Control_center_been_hit = 0;
				Control_center_player_been_seen = 0;
				return 1;
			}

			#ifdef NETWORK
			if (Game_mode & GM_MULTI)
				multi_send_controlcen_fire(&vec_to_goal, best_gun_num, obj-Objects);
			#endif
			Laser_create_new_easy( &vec_to_goal, &obj->ctype.reactor_info.gun_pos[best_gun_num], obj-Objects, CONTROLCEN_WEAPON_NUM, 1);

			//	1/4 of time, fire another thing, not directly at player, so it might hit him if he's constantly moving.
			// SIM RNG: this decides whether the reactor fires an extra live shot
			if (d_rand() < 32767/4) {
				vms_vector	randvec;

				make_random_vector(&randvec);
				vm_vec_scale_add2(&vec_to_goal, &randvec, F1_0/4);
				vm_vec_normalize_quick(&vec_to_goal);
				#ifdef NETWORK
				if (Game_mode & GM_MULTI)
					multi_send_controlcen_fire(&vec_to_goal, best_gun_num, obj-Objects);
				#endif
				Laser_create_new_easy( &vec_to_goal, &obj->ctype.reactor_info.gun_pos[best_gun_num], obj-Objects, CONTROLCEN_WEAPON_NUM, 1);
			}

			delta_fire_time = (NDL - Difficulty_level) * F1_0/4;
#ifdef NETWORK
			if (Game_mode & GM_MULTI) // slow down rate of fire in multi player
				delta_fire_time *= 2;
#endif
			Control_center_next_fire_time = delta_fire_time;

		}
	} else
		Control_center_next_fire_time -= FrameTime;

	return 1;
}

int d1_in_d2_reactor_countdown(int engine_seconds)
{
	return d1_in_d2_use_d1_gameplay() ? 50 - 5*Difficulty_level : engine_seconds;
}

fix d1_in_d2_reactor_strength(fix engine_strength)
{
	if (!d1_in_d2_use_d1_gameplay())
		return engine_strength;
	return F1_0 * 200 + Current_level_num * F1_0 * (Current_level_num >= 0 ? 50 : -100);
}

int d1_in_d2_use_d2_resource_drop_suppression(const object *objp, int game_mode)
{
	return objp && !d1_in_d2_use_d1_gameplay() &&
	       !(game_mode & GM_MULTI) && objp->type != OBJ_PLAYER;
}

vms_vector *d1_in_d2_badass_explosion_pos(object *weapon, vms_vector *collision_point)
{
	return d1_in_d2_use_d1_gameplay() ? &weapon->pos : collision_point;
}

vms_vector *d1_in_d2_prepare_player_explosion_pos(object *weapon, vms_vector *collision_point)
{
	if (d1_in_d2_use_d1_gameplay()) {
		weapon->pos.x = collision_point->x;
		weapon->pos.y = collision_point->y;
		weapon->pos.z = collision_point->z;
		return &weapon->pos;
	}

	return collision_point;
}

int d1_in_d2_integrate_rotation(object *obj, int count, fix k, fix drag)
{
	vms_vector accel;
	if (!uses_native_physics(obj))
		return 0;

	if (obj->mtype.phys_info.flags & PF_USES_THRUST)
		vm_vec_copy_scale(&accel,&obj->mtype.phys_info.rotthrust,fixdiv(f1_0,obj->mtype.phys_info.mass));

	while (count--) {
		if (obj->mtype.phys_info.flags & PF_USES_THRUST)
			vm_vec_add2(&obj->mtype.phys_info.rotvel,&accel);

		vm_vec_scale(&obj->mtype.phys_info.rotvel,f1_0-drag);
	}

	if (obj->mtype.phys_info.flags & PF_USES_THRUST)
		vm_vec_scale_add2(&obj->mtype.phys_info.rotvel,&accel,k);
	vm_vec_scale(&obj->mtype.phys_info.rotvel,f1_0-fixmul(k,drag));
	return 1;
}

int d1_in_d2_robot_rotational_hit(object *obj, vms_vector *force_vec, fix rate, fix vecmag)
{
	int skip_before;
	if (!uses_native_physics(obj))
		return 0;
	skip_before = obj->ctype.ai_info.SKIP_AI_COUNT;

	obj->ctype.ai_info.SKIP_AI_COUNT = 2;
	input_demo_record_phys_apply_rot_event(obj, force_vec,
		skip_before, 2 - skip_before,
		obj->ctype.ai_info.SKIP_AI_COUNT, rate, vecmag, 0);
	input_demo_note_ai_schedule_phys_skip(obj, skip_before,
		obj->ctype.ai_info.SKIP_AI_COUNT);
	return 1;
}

void d1_in_d2_note_physics_result(const object *obj, const vms_vector *start,
	int fate, int stopped, int bounced)
{
	if (d1_in_d2_use_d1_gameplay() && input_demo_debug_activity_probe_active() &&
		obj->type == OBJ_ROBOT && ConsoleObject &&
		vm_vec_dist_quick(&obj->pos, &ConsoleObject->pos) < F1_0 * 100 &&
		((fate == HIT_WALL) || (fate == HIT_OBJECT) || (fate == HIT_BAD_P0))) {
		vms_vector moved_vec;
		vms_vector movement_velocity;
		vm_vec_sub(&moved_vec, &obj->pos, start);
		vm_vec_copy_scale(&movement_velocity, &moved_vec, fixdiv(f1_0, FrameTime));
		input_demo_debug_printf(
			"Input demo d1-in-d2 physics final: mode=%s frame=%u gt=%lld obj=%d/%d sig=%d fate=%d stopped=%d bounced=%d pos=(%d,%d,%d) start=(%d,%d,%d) vel=(%d,%d,%d) movement_vel=(%d,%d,%d)\n",
			input_demo_debug_activity_mode_name(), input_demo_debug_frame_index(),
			(long long)GameTime64, (int)(obj - Objects), obj->id, obj->signature,
			fate, stopped, bounced, obj->pos.x, obj->pos.y, obj->pos.z,
			start->x, start->y, start->z, obj->mtype.phys_info.velocity.x,
			obj->mtype.phys_info.velocity.y, obj->mtype.phys_info.velocity.z,
			movement_velocity.x, movement_velocity.y, movement_velocity.z);
	}
}

static int input_demo_fvi_boundary_probe_active(short objnum)
{
	object *objp;

	if (!input_demo_replay_is_loaded() || !d1_in_d2_use_d1_gameplay() ||
		!input_demo_debug_activity_probe_active() ||
		objnum < 0 || objnum > Highest_object_index)
		return 0;
	objp = &Objects[objnum];
	return objp->type == OBJ_WEAPON &&
		objp->ctype.laser_info.parent_type == OBJ_PLAYER &&
		!(objp->flags & (OF_SHOULD_BE_DEAD | OF_HARMLESS));
}

void d1_in_d2_note_wall_boundary(short objnum, int startseg, int side,
	int face, int face_hit_type, int wid_flag, int startmask, int endmask,
	int centermask, int flags, const vms_vector *p0, const vms_vector *p1,
	const vms_vector *hit_point, fix rad)
{
	char probe[900];
	segment *seg = &Segments[startseg];
	int wall_num = seg->sides[side].wall_num;
	int wall_type = -1;
	int wall_state = -1;
	int wall_flags = 0;

	if (!input_demo_fvi_boundary_probe_active(objnum))
		return;
	if (wall_num >= 0 && wall_num < Num_walls) {
		wall_type = Walls[wall_num].type;
		wall_state = Walls[wall_num].state;
		wall_flags = Walls[wall_num].flags;
	}
	snprintf(probe, sizeof(probe),
		"start_seg=%d side=%d face=%d face_hit_type=%d wid=0x%x child=%d wall=%d wall_type=%d wall_state=%d wall_flags=0x%x startmask=0x%x endmask=0x%x centermask=0x%x flags=0x%x rad=%d p0=(%d,%d,%d) p1=(%d,%d,%d) hit=(%d,%d,%d)",
		startseg, side, face, face_hit_type, wid_flag,
		seg->children[side], wall_num, wall_type, wall_state, wall_flags,
		startmask, endmask, centermask, flags, rad,
		p0->x, p0->y, p0->z,
		p1->x, p1->y, p1->z,
		hit_point->x, hit_point->y, hit_point->z);
	input_demo_append_replay_probe_message("fvi_boundary", &Objects[objnum], probe);
}

static int d1_in_d2_check_trans_wall(vms_vector *pnt, segment *seg, int sidenum,
	int facenum, short objnum)
{
	grs_bitmap *bm;
	side *side = &seg->sides[sidenum];
	int bmx, bmy;
	int direct_pixel, gpixel;
	fix u, v;

	find_hitpoint_uv(&u, &v, NULL, pnt, seg, sidenum, facenum);

	if (side->tmap_num2 != 0) {
		bm = texmerge_get_cached_bitmap(side->tmap_num, side->tmap_num2);
	} else {
		bm = &GameBitmaps[Textures[side->tmap_num].index];
		PIGGY_PAGE_IN(Textures[side->tmap_num]);
	}

	if (bm->bm_flags & BM_FLAG_RLE)
		bm = rle_expand_texture(bm);

	bmx = ((unsigned)f2i(u * bm->bm_w)) % bm->bm_w;
	bmy = ((unsigned)f2i(v * bm->bm_h)) % bm->bm_h;
	direct_pixel = bm->bm_data[bmy * bm->bm_w + bmx];
	gpixel = gr_gpixel(bm, bmx, bmy);
	if (input_demo_fvi_boundary_probe_active(objnum)) {
		char probe[512];
		int wall_num = side->wall_num;
		int clip_num = -1;
		int clip_flags = 0;
		int clip_frame0 = -1;

		if (wall_num >= 0 && wall_num < Num_walls) {
			clip_num = Walls[wall_num].clip_num;
			if (clip_num >= 0 && clip_num < Num_wall_anims) {
				clip_flags = WallAnims[clip_num].flags;
				clip_frame0 = WallAnims[clip_num].frames[0];
			}
		}

		snprintf(probe, sizeof(probe),
			"seg=%d side=%d face=%d tmap=%d tmap2=%d wall=%d clip=%d clip_flags=0x%x clip_frame0=%d num_wall_anims=%d u=%d v=%d bmx=%d bmy=%d bm_w=%d bm_h=%d rowsize=%d flags=0x%x type=%d direct=%d gpixel=%d transparent=%d p=(%d,%d,%d)",
			(int)(seg - Segments), sidenum, facenum,
			side->tmap_num, side->tmap_num2, wall_num, clip_num, clip_flags,
			clip_frame0, Num_wall_anims, u, v, bmx, bmy,
			bm->bm_w, bm->bm_h, bm->bm_rowsize, bm->bm_flags, bm->bm_type,
			direct_pixel, gpixel, gpixel == TRANSPARENCY_COLOR,
			pnt->x, pnt->y, pnt->z);
		input_demo_append_replay_probe_message("trans_wall_pixel",
			&Objects[objnum], probe);
	}

	return gpixel == TRANSPARENCY_COLOR;
}

int d1_in_d2_transparent_wall_crossable(int wid_flag, segment *seg,
	int side, int flags, vms_vector *hit_point, int face, short objnum)
{
	if (wid_flag != WID_TRANSPARENT_WALL)
		return 0;
	if (flags & FQ_TRANSWALL)
		return 1;
	if (!(flags & FQ_TRANSPOINT))
		return 0;
	/* Native D1 samples the visible pixels, including on a closed door */
	return d1_in_d2_check_trans_wall(hit_point, seg, side, face, objnum);
}
