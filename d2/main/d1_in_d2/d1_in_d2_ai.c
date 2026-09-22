/* D1 robot operations, kept separate from D2 scheduling and firing */
#include <stdlib.h>
#include "inferno.h"
#include "game.h"
#include "object.h"
#include "player.h"
#include "multi.h"
#include "multibot.h"
#include "laser.h"
#include "ai.h"
#include "robot.h"
#include "dxxerror.h"
#include "d1_in_d2.h"
#include "d1_in_d2_ai.h"
#include "d1_in_d2_ai_internal.h"
#include "console.h"
#include "segpoint.h"
#include "fvi.h"
#include "digi.h"
#include "polyobj.h"
#include "gameseg.h"
#include "wall.h"
#include "fireball.h"
#include "morph.h"
#include "vclip.h"
#include "effects.h"
#include "sounds.h"
#include "cntrlcen.h"
#include "escort.h"
#include "physics.h"
#include "input_demo_hooks.h"

extern int Robot_sound_volume;

static int sight_visibility(object *obj, vms_vector *pos, fix field_of_view, vms_vector *direction);
static void compute_visibility(object *obj, vms_vector *pos, ai_local *local,
	vms_vector *direction, int *visibility, robot_info *info, int *computed);

enum { D1_AI_FRAME_READY = 0, D1_AI_FRAME_DEFER = 1 };

static int Boss_hit_pending;
#ifdef NETWORK
static int Boss_gate_effect_state;
#endif
/* These tagged values occupy the existing 32-bit hit-time field only for D1
 * content. A real boss hit timestamp is never almost nine hours in the future
 * Keep the D2 save layout/values unchanged and accept older untagged D1 saves */
enum { D1_BOSS_HIT_CLEAR = 0x7ffffffe, D1_BOSS_HIT_PENDING = 0x7fffffff };
enum { D1_BOSS_DEATH_SOUND_DURATION = 0x2ae14 }; /* Native fixed 2.68 seconds */
enum { D1_ROBOT_BABY_SPIDER = 14 };

void d1_in_d2_ai_reset_boss_state(void)
{
	Boss_hit_pending = 0;
#ifdef NETWORK
	Boss_gate_effect_state = 0;
#endif
}

void d1_in_d2_ai_restore_boss_hit(int pending)
{
	Boss_hit_pending = pending != 0;
}

fix d1_in_d2_ai_save_boss_hit(fix engine_delta)
{
	return d1_in_d2_use_d1_gameplay() ? (Boss_hit_pending ? D1_BOSS_HIT_PENDING : D1_BOSS_HIT_CLEAR) : engine_delta;
}

fix64 d1_in_d2_ai_restore_boss_hit_time(fix saved)
{
	if (!d1_in_d2_use_d1_gameplay())
		return GameTime64 + (fix64)saved;
	Boss_hit_pending = saved == D1_BOSS_HIT_PENDING;
	return -F1_0*10;
}

enum d1_ai_actor_role d1_in_d2_ai_actor_role(const object *obj)
{
	return d1_in_d2_use_d1_gameplay() && obj->type == OBJ_ROBOT &&
		!Robot_info[obj->id].companion ? D1_AI_NATIVE_ENEMY : D1_AI_ENGINE_ACTOR;
}

int d1_in_d2_ai_camera_can_wake(const object *obj)
{
	return d1_in_d2_ai_actor_role(obj) != D1_AI_NATIVE_ENEMY;
}

int d1_in_d2_ai_morph_robot_mode(const object *obj, int engine_mode)
{
	if (d1_in_d2_ai_actor_role(obj) != D1_AI_NATIVE_ENEMY)
		return engine_mode;
	/* Native matcen robots keep the exit path's mode; only toasters run away */
	return obj->id == 10 ? AIM_RUN_FROM_OBJECT : Ai_local_info[obj - Objects].mode;
}

int d1_in_d2_ai_flash_can_stun(const object *obj)
{
	return d1_in_d2_ai_actor_role(obj) != D1_AI_NATIVE_ENEMY;
}

fix d1_in_d2_ai_robot_blast_damage(const object *obj, fix damage)
{
	const int boss = Robot_info[obj->id].boss_flag - BOSS_D2;
	if (d1_in_d2_ai_actor_role(obj) == D1_AI_NATIVE_ENEMY)
		return damage;
	/* Native boss identities must never index the D2-only resistance table */
	return boss >= 0 && boss < NUM_D2_BOSSES && Boss_invulnerable_matter[boss] ? damage / 4 : damage;
}

/* Native D1 pae_aux walk, with a second result for optional engine actors
 * Remaining depth 4 reaches four edges; engine delivery stops one edge earlier
 * Both results come from one queue traversal and are discarded after delivery */
static void propagate_awareness(int segnum, int type, int depth, sbyte *native, sbyte *engine)
{
	int side;
	if (segnum < 0 || segnum > Highest_segment_index || segnum >= MAX_SEGMENTS)
		return;
	if (native[segnum] < type)
		native[segnum] = type;
	if (depth && engine && engine[segnum] < type)
		engine[segnum] = type;
	if (depth)
		for (side = 0; side < MAX_SIDES_PER_SEGMENT; ++side)
			if (IS_CHILD(Segments[segnum].children[side]))
				propagate_awareness(Segments[segnum].children[side], type == 4 ? 3 : type,
					depth - 1, native, engine);
}

static void deliver_awareness(void)
{
	sbyte native[MAX_SEGMENTS] = {0}, engine[MAX_SEGMENTS] = {0};
	int i;
	const int engine_enabled = !(Game_mode & GM_MULTI) || (Game_mode & GM_MULTI_ROBOTS);
	for (i = 0; i < Num_awareness_events && i < MAX_AWARENESS_EVENTS; ++i)
		propagate_awareness(Awareness_events[i].segnum, Awareness_events[i].type, 4,
			native, engine_enabled ? engine : NULL);
	Num_awareness_events = 0;
	for (i = 0; i <= Highest_object_index; ++i) {
		const object *obj = &Objects[i];
		ai_local *local = &Ai_local_info[i];
		int awareness;
		if (obj->control_type != CT_AI || obj->segnum < 0 ||
			obj->segnum > Highest_segment_index || obj->segnum >= MAX_SEGMENTS)
			continue;
		awareness = d1_in_d2_ai_actor_role(obj) == D1_AI_NATIVE_ENEMY ? native[obj->segnum] : engine[obj->segnum];
		if (awareness > local->player_awareness_type) {
			local->player_awareness_type = awareness;
			local->player_awareness_time = PLAYER_AWARENESS_INITIAL_TIME;
		}
	}
}

int d1_in_d2_ai_deliver_awareness(void)
{
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	deliver_awareness();
	return 1;
}

int d1_in_d2_ai_create_awareness_event(object *obj, int type)
{
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	/* Native D1 always admits event production, even in multiplayer without
	 * robots. Observer rejection and the remaining algorithm match D2 */
	ai_create_awareness_event_common(obj, type, 1);
	return 1;
}

int d1_in_d2_ai_finish_world_frame(void)
{
	int i;
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	if (Ai_last_missile_camera > -1 && ((d_tick_count & 0x0f) == 0 ||
		Ai_last_missile_camera > Highest_object_index || Objects[Ai_last_missile_camera].type != OBJ_WEAPON)) {
		Ai_last_missile_camera = -1;
		for (i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_ROBOT && d1_in_d2_ai_actor_role(&Objects[i]) == D1_AI_ENGINE_ACTOR)
				Objects[i].ctype.ai_info.SUB_FLAGS &= ~SUB_FLAGS_CAMERA_AWAKE;
	}
	/* Native bosses complete death in their object frame, even without sight
	 * Any engine actors retain the engine's separate global completion phase */
	if (Boss_dying)
		for (i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_ROBOT && d1_in_d2_ai_actor_role(&Objects[i]) == D1_AI_ENGINE_ACTOR &&
				Robot_info[Objects[i].id].boss_flag)
				do_boss_dying_frame(&Objects[i]);
	return 1;
}

int d1_in_d2_ai_robot_hit(object *obj, int type)
{
	if (obj->control_type != CT_AI || d1_in_d2_ai_actor_role(obj) != D1_AI_NATIVE_ENEMY)
		return 0;
	/* Native do_ai_robot_hit compares behavior against mode constants
	 * Preserve that source quirk: the six normal AIB_* values do not match */
	if (type == PA_WEAPON_ROBOT_COLLISION || type == PA_PLAYER_COLLISION)
		switch (obj->ctype.ai_info.behavior) {
			case D1_AIM_HIDE:
				obj->ctype.ai_info.SUB_FLAGS = D1_AISM_GOHIDE;
				break;
			case AIM_STILL:
				Ai_local_info[obj-Objects].mode = AIM_CHASE_OBJECT;
				break;
		}
	return 1;
}

/* Native D1 boss preparation and gating, adapted from d1/main/ai.c
 * Geometry, object storage and morph execution remain engine services */
int d1_in_d2_ai_initialize_boss(void)
{
	object *boss = NULL;
	fix original_size = 0;
	int i;
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	d1_in_d2_ai_reset_boss_state();
	for (i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type == OBJ_ROBOT && Robot_info[Objects[i].id].boss_flag)
			boss = &Objects[i];
	if (boss) {
		original_size = boss->size;
		boss->size = fixmul((F1_0/4)*3, original_size);
	}
	init_boss_segments(Boss_teleport_segs, &Num_boss_teleport_segs, 1, 0);
	init_boss_segments(Boss_gate_segs, &Num_boss_gate_segs, 0, 0);
	if (boss)
		boss->size = original_size;
	/* Native D1 has no separate final-level interval policy */
	Boss_teleport_interval = F1_0*8;
	Boss_cloak_interval = F1_0*10;
	return 1;
}

fix d1_in_d2_ai_gate_interval(fix engine_interval)
{
	return d1_in_d2_use_d1_gameplay() ? F1_0*5 - Difficulty_level*F1_0/2 : engine_interval;
}

int d1_in_d2_ai_gate_robot(int type, int segnum, int *result)
{
	int i, count = 0, objnum;
	vms_vector pos;
	robot_info *info;
	object *obj;
	fix size;
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	*result = -1;
	/* A random destination is selected before the native population check */
	if (segnum < 0) {
		if (Num_boss_gate_segs <= 0)
			return 1;
		segnum = Boss_gate_segs[(d_rand() * Num_boss_gate_segs) >> 15];
	}
	if (segnum < 0 || segnum > Highest_segment_index || type < 0 || type >= N_robot_types)
		return 1;
	info = &Robot_info[type];
	size = Polygon_models[info->model_num].rad;
	for (i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type == OBJ_ROBOT && Objects[i].matcen_creator == BOSS_GATE_MATCEN_NUM)
			++count;
	if (count > 2*Difficulty_level + 3)
		goto failed;
	pick_random_point_in_seg(&pos, segnum);
	if (check_object_object_intersection(&pos, size, &Segments[segnum]))
		goto failed;
	objnum = obj_create(OBJ_ROBOT, type, segnum, &pos, &vmd_identity_matrix,
		size, CT_AI, MT_PHYSICS, RT_POLYOBJ);
	if (objnum < 0)
		goto failed;
	con_printf(CON_DEBUG, "Gating in object %hu in segment %hu\n", (unsigned short)objnum, (unsigned short)segnum);
#ifdef NETWORK
	Net_create_objnums[0] = objnum;
#endif
	obj = &Objects[objnum];
	obj->rtype.pobj_info.model_num = info->model_num;
	obj->rtype.pobj_info.subobj_flags = 0;
	obj->mtype.phys_info.mass = info->mass;
	obj->mtype.phys_info.drag = info->drag;
	obj->mtype.phys_info.flags |= PF_LEVELLING;
	obj->shields = info->strength;
	obj->matcen_creator = BOSS_GATE_MATCEN_NUM;
	/* Source robot 10 is the toaster; other gated robots use normal behavior */
	init_ai_object(objnum, type == 10 ? AIB_RUN_FROM : AIB_NORMAL, -1);
	object_create_explosion(segnum, &pos, i2f(10), VCLIP_MORPHING_ROBOT);
	digi_link_sound_to_pos(Vclip[VCLIP_MORPHING_ROBOT].sound_num, segnum, 0, &pos, 0, F1_0);
	morph_start(obj);
	Last_gate_time = GameTime64;
	Players[Player_num].num_robots_level++;
	Players[Player_num].num_robots_total++;
	*result = objnum;
	return 1;

failed:
	Last_gate_time = GameTime64 - 3*Gate_interval/4;
	return 1;
}

int d1_in_d2_ai_boss_weapon_hit(const object *obj)
{
	if (d1_in_d2_ai_actor_role(obj) != D1_AI_NATIVE_ENEMY || !Robot_info[obj->id].boss_flag)
		return 0;
	Boss_hit_pending = 1;
	return 1;
}

int d1_in_d2_ai_finish_boss_damage(object *obj, int killer)
{
	if (d1_in_d2_ai_actor_role(obj) != D1_AI_NATIVE_ENEMY || !Robot_info[obj->id].boss_flag)
		return -1;
	if (obj->shields >= 0)
		return 0;
	Players[Player_num].num_kills_level++;
	Players[Player_num].num_kills_total++;
#ifdef NETWORK
	if (Game_mode & GM_MULTI) {
		if (!multi_explode_robot_sub(obj-Objects, killer, 0, 0))
			return 0;
		multi_send_robot_explode(obj-Objects, killer, 0);
		if (multi_i_am_master() && (Game_mode & GM_MULTI_ROBOTS))
			kill_respawnable_robot(obj);
		return 1;
	}
#else
	(void)killer;
#endif
	start_boss_death_sequence(obj);
	return 1;
}

static void teleport_boss(object *obj)
{
	vms_vector direction;
	int index, segnum;
	if (Num_boss_teleport_segs <= 0)
		return;
	/* SIM RNG: native destination selection and wire list index */
	index = (d_rand() * Num_boss_teleport_segs) >> 15;
	segnum = Boss_teleport_segs[index];
	Assert(segnum >= 0 && segnum <= Highest_segment_index);
#ifdef NETWORK
	if (Game_mode & GM_MULTI)
		multi_send_boss_actions(obj-Objects, 1, index, 0);
#endif
	compute_segment_center(&obj->pos, &Segments[segnum]);
	obj_relink(obj-Objects, segnum);
	escort_note_boss_teleported(obj-Objects);
	Last_teleport_time = GameTime64;
	vm_vec_sub(&direction, &Objects[Players[Player_num].objnum].pos, &obj->pos);
	vm_vector_2_matrix(&obj->orient, &direction, NULL, NULL);
	digi_link_sound_to_pos(Vclip[VCLIP_MORPHING_ROBOT].sound_num, segnum, 0, &obj->pos, 0, F1_0);
	digi_kill_sound_linked_to_object(obj-Objects);
	digi_link_sound_to_object2(SOUND_BOSS_SHARE_SEE, obj-Objects, 1, F1_0, F1_0*512);
	Ai_local_info[obj-Objects].next_fire = 0;
}

static void boss_dying_frame(object *obj)
{
	/* Native D1 overwrites its intermediate sin/cos values with these rolls */
	obj->mtype.phys_info.rotvel.x = (GameTime64 - Boss_dying_start_time)/9;
	obj->mtype.phys_info.rotvel.y = (GameTime64 - Boss_dying_start_time)/5;
	obj->mtype.phys_info.rotvel.z = (GameTime64 - Boss_dying_start_time)/7;
	if (Boss_dying_start_time + BOSS_DEATH_DURATION - D1_BOSS_DEATH_SOUND_DURATION < GameTime64) {
		if (!Boss_dying_sound_playing) {
			Boss_dying_sound_playing = 1;
			digi_link_sound_to_object2(SOUND_BOSS_SHARE_DIE, obj-Objects, 0, F1_0*4, F1_0*1024);
		/* FX RNG: native cosmetic fireball cadence and scale */
		} else if (d_rand_fx() < FrameTime*16)
			create_small_fireball_on_object(obj, (F1_0 + d_rand_fx())*8, 0);
	} else if (d_rand_fx() < FrameTime*8)
		create_small_fireball_on_object(obj, (F1_0/2 + d_rand_fx())*8, 1);
	if (Boss_dying_start_time + BOSS_DEATH_DURATION < GameTime64 || GameTime64 + F1_0*2 < Boss_dying_start_time) {
		Boss_dying_start_time = GameTime64;
		do_controlcen_destroyed_stuff(NULL);
		explode_object(obj, F1_0/4);
		digi_link_sound_to_object2(SOUND_BADASS_EXPLOSION, obj-Objects, 0, F2_0, F1_0*512);
	}
}

static void boss_frame(object *obj)
{
	if (Boss_dying) {
		boss_dying_frame(obj);
		return;
	}
	if (obj->ctype.ai_info.CLOAKED == 1) {
		if (GameTime64 - Boss_cloak_start_time > BOSS_CLOAK_DURATION/3 &&
			Boss_cloak_end_time - GameTime64 > BOSS_CLOAK_DURATION/3 &&
			GameTime64 - Last_teleport_time > Boss_teleport_interval) {
			if (ai_multiplayer_awareness(obj, 98))
				teleport_boss(obj);
		} else if (Boss_hit_pending) {
			Boss_hit_pending = 0;
			Last_teleport_time -= Boss_teleport_interval/4;
		}
		if (GameTime64 > Boss_cloak_end_time)
			obj->ctype.ai_info.CLOAKED = 0;
	} else if (GameTime64 - Boss_cloak_end_time > Boss_cloak_interval || Boss_hit_pending) {
		if (ai_multiplayer_awareness(obj, 95)) {
			Boss_hit_pending = 0;
			Boss_cloak_start_time = GameTime64;
			Boss_cloak_end_time = GameTime64 + Boss_cloak_duration;
			obj->ctype.ai_info.CLOAKED = 1;
#ifdef NETWORK
			if (Game_mode & GM_MULTI)
				multi_send_boss_actions(obj-Objects, 2, 0, 0);
#endif
		}
	}
}

static void super_boss_frame(object *obj, fix distance, int visible)
{
	static const sbyte gate_types[] = {0, 1, 8, 9, 10, 11, 12, 15, 16, 18, 19, 20, 22, 0, 8, 11, 19, 20, 8, 20, 8};
	boss_frame(obj);
#ifdef NETWORK
	/* Only the master can cause gating */
	if ((Game_mode & GM_MULTI) && !multi_i_am_master())
		return;
#endif
	if (distance < F1_0*150 || visible || (Game_mode & GM_MULTI)) {
		const int effect_active = GameTime64 - Last_gate_time > Gate_interval/2;
		if (effect_active)
			restart_effect(ECLIP_NUM_BOSS);
		else
			stop_effect(ECLIP_NUM_BOSS);
#ifdef NETWORK
		if (Boss_gate_effect_state != effect_active) {
			if (Game_mode & GM_MULTI)
				multi_send_boss_actions(obj-Objects, effect_active ? 4 : 5, 0, 0);
			Boss_gate_effect_state = effect_active;
		}
#endif
		if (GameTime64 - Last_gate_time > Gate_interval && ai_multiplayer_awareness(obj, 99)) {
			const int type = gate_types[(d_rand() * (sizeof(gate_types)/sizeof(gate_types[0]))) >> 15];
			int created;
			d1_in_d2_ai_gate_robot(type, -1, &created);
#ifdef NETWORK
			if (created >= 0 && (Game_mode & GM_MULTI)) {
				multi_send_boss_actions(obj-Objects, 3, type, created);
				map_objnum_local_to_local(created);
			}
#endif
		}
	}
}

static void update_boss(object *obj, fix *distance, vms_vector *origin,
	vms_vector *direction, int *visibility, int *computed)
{
	const int flag = Robot_info[obj->id].boss_flag;
	if (!flag)
		return;
	if (obj->ctype.ai_info.GOAL_STATE == AIS_FLIN)
		obj->ctype.ai_info.GOAL_STATE = AIS_FIRE;
	if (obj->ctype.ai_info.CURRENT_STATE == AIS_FLIN)
		obj->ctype.ai_info.CURRENT_STATE = AIS_FIRE;
	if (flag == 1) {
		*distance /= 4;
		boss_frame(obj);
		*distance *= 4;
	} else if (flag == 2) {
		int visible;
		fix gate_distance = *distance/4;
		compute_visibility(obj, origin, &Ai_local_info[obj-Objects], direction,
			visibility, &Robot_info[obj->id], computed);
		visible = *visibility;
		if (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED) {
			visible = 0;
			gate_distance = vm_vec_dist_quick(&ConsoleObject->pos, &obj->pos)/4;
		}
		super_boss_frame(obj, gate_distance, visible);
	} else
		Int3();
	return;
}

static int initial_mode(int behavior)
{
	switch (behavior) {
		case AIB_STILL: return AIM_STILL;
		case AIB_NORMAL: return AIM_CHASE_OBJECT;
		case D1_AIB_HIDE: return D1_AIM_HIDE;
		case AIB_RUN_FROM: return AIM_RUN_FROM_OBJECT;
		case D1_AIB_FOLLOW_PATH: return AIM_FOLLOW_PATH;
		case AIB_STATION: return AIM_STILL;
		default: Int3();
	}
	return AIM_STILL;
}

static int prepare_frame(object *obj, fix *distance, vms_vector *gun_point, vms_vector *visibility_origin)
{
	const int objnum = obj - Objects;
	ai_static *state = &obj->ctype.ai_info;
	ai_local *local = &Ai_local_info[objnum];
	robot_info *info = &Robot_info[obj->id];
	if (state->SKIP_AI_COUNT) {
		input_demo_note_ai_schedule_skip_return(obj);
		input_demo_log_ai_schedule_probe("skip_return_pre", obj, state, local, 0,
			local->previous_visibility, -1);
		state->SKIP_AI_COUNT--;
		input_demo_log_ai_schedule_probe("skip_return_post", obj, state, local, 0,
			local->previous_visibility, -1);
		return D1_AI_FRAME_DEFER;
	}
	if (is_observer())
		return D1_AI_FRAME_DEFER;
	Assert(info->always_0xabcd == 0xabcd);
	/* Keep the engine's shared death-sequence service at its existing phase */
	if (do_any_robot_dying_frame(obj))
		return D1_AI_FRAME_DEFER;
	if (state->GOAL_STATE == AIS_FLIN && local->next_fire < 0)
		state->GOAL_STATE = AIS_FIRE;
#ifndef NDEBUG
	if (state->behavior == AIB_RUN_FROM && local->mode != AIM_RUN_FROM_OBJECT)
		Int3();
	if (!Do_ai_flag)
		return D1_AI_FRAME_DEFER;
	if (Break_on_object == objnum)
		Int3();
#endif
	if (state->behavior < AIB_STILL || state->behavior > AIB_STATION)
		state->behavior = AIB_NORMAL;
	Assert(obj->segnum != -1);
	Assert(obj->id < N_robot_types);
	if (local->next_fire > -F1_0*8)
		local->next_fire -= FrameTime;
	if (local->time_since_processed < F1_0*256)
		local->time_since_processed += FrameTime;
	if (info->cloak_type == RI_CLOAKED_EXCEPT_FIRING)
		state->CLOAKED = local->next_fire < F1_0/2;

	Believed_player_pos = Ai_cloak_info[objnum & (MAX_AI_CLOAK_INFO-1)].last_position;
	if (!(Players[Player_num].flags & PLAYER_FLAGS_CLOAKED))
		Believed_player_pos = ConsoleObject->pos;
	*distance = vm_vec_dist_quick(&Believed_player_pos, &obj->pos);
	input_demo_log_ai_schedule_probe("pre_awareness", obj, state, local,
		*distance, local->previous_visibility, objnum ^ d_tick_count);
	if (local->next_fire <= 0 && *distance < F1_0*200 && info->n_guns && !info->attack_type) {
		calc_gun_point(gun_point, obj, state->CURRENT_GUN);
		*visibility_origin = *gun_point;
	} else {
		*visibility_origin = obj->pos;
		vm_vec_zero(gun_point);
	}
	return D1_AI_FRAME_READY;
}

static int prepare_behavior(object *obj, fix dist_to_player)
{
	const int objnum = obj - Objects;
	ai_static *aip = &obj->ctype.ai_info;
	ai_local *ailp = &Ai_local_info[objnum];
	// Occasionally make non-still robots make a path to the player.  Based on agitation and distance from player.
	if ((aip->behavior != AIB_RUN_FROM) && (aip->behavior != AIB_STILL) && !(Game_mode & GM_MULTI))
		if (Overall_agitation > 70) {
			if (dist_to_player < F1_0*200) {
				// SIM RNG: these rolls decide whether the robot creates a live agitation path to the player
				int agitation_path_trigger_roll = d_rand();
				int agitation_path_trigger_pass = agitation_path_trigger_roll < FrameTime/4;
				int agitation_path_roll = -1;
				int agitation_path_pass = 0;
				int agitation_path_max_length = 4 + Overall_agitation/8 + Difficulty_level;
				int agitation_path_pre_mode = ailp->mode;
				int agitation_path_pre_goal_segment = ailp->goal_segment;
				int agitation_path_pre_path_index = aip->cur_path_index;
				int agitation_path_pre_path_length = aip->path_length;
				int agitation_path_pre_hide_index = aip->hide_index;
				int agitation_path_pre_path_dir = aip->PATH_DIR;
				int64_t agitation_path_pre_time_player_seen = ailp->time_player_seen;

				if (agitation_path_trigger_pass) {
					// SIM RNG: this second roll decides whether the live agitation path is actually created
					agitation_path_roll = d_rand();
					agitation_path_pass = agitation_path_roll * (Overall_agitation - 40) > F1_0*5;
				}

				if (agitation_path_pass) {
					d1_in_d2_ai_create_path_to_player(obj, agitation_path_max_length, 1);
				}

				if (agitation_path_trigger_pass || input_demo_trace_ai_visibility_active(obj))
					input_demo_log_ai_agitation_path_gate(obj,
						dist_to_player,
						Overall_agitation,
						agitation_path_trigger_roll,
						FrameTime/4,
						agitation_path_trigger_pass,
						agitation_path_roll,
						Overall_agitation - 40,
						agitation_path_pass,
						agitation_path_max_length,
						agitation_path_pre_mode,
						agitation_path_pre_goal_segment,
						agitation_path_pre_path_index,
						agitation_path_pre_path_length,
						agitation_path_pre_hide_index,
						agitation_path_pre_path_dir,
						agitation_path_pre_time_player_seen);

				if (agitation_path_pass)
					return D1_AI_FRAME_DEFER;
			}
		}

	//	If retry count not 0, then add it into consecutive_retries.
	//	If it is 0, cut down consecutive_retries.
	//	This is largely a hack to speed up physics and deal with stupid AI.  This is low level
	//	communication between systems of a sort that should not be done.
	if ((ailp->retry_count) && !(Game_mode & GM_MULTI)) {
		ailp->consecutive_retries += ailp->retry_count;
		ailp->retry_count = 0;
		if (ailp->consecutive_retries > 3) {
			switch (ailp->mode) {
				case AIM_CHASE_OBJECT:
					d1_in_d2_ai_create_path_to_player(obj, 4 + Overall_agitation/8 + Difficulty_level, 1);
					break;
				case AIM_STILL:
					if (!((aip->behavior == AIB_STILL) || (aip->behavior == AIB_STATION)))	//	Behavior is still, so don't follow path.
						attempt_to_resume_path(obj);
					break;
				case AIM_FOLLOW_PATH:
					if (Game_mode & GM_MULTI)
						ailp->mode = AIM_STILL;
					else
						attempt_to_resume_path(obj);
					break;
				case AIM_RUN_FROM_OBJECT:
					move_towards_segment_center(obj);
					obj->mtype.phys_info.velocity.x = 0;
					obj->mtype.phys_info.velocity.y = 0;
					obj->mtype.phys_info.velocity.z = 0;
					d1_in_d2_ai_create_random_path(obj, 5, -1);
					ailp->mode = AIM_RUN_FROM_OBJECT;
					break;
				case D1_AIM_HIDE:
					move_towards_segment_center(obj);
					obj->mtype.phys_info.velocity.x = 0;
					obj->mtype.phys_info.velocity.y = 0;
					obj->mtype.phys_info.velocity.z = 0;
					if (Overall_agitation > (50 - Difficulty_level*4))
						d1_in_d2_ai_create_path_to_player(obj, 4 + Overall_agitation/8, 1);
					else {
						d1_in_d2_ai_create_random_path(obj, 5, -1);
					}
					break;
				case AIM_OPEN_DOOR:
					d1_in_d2_ai_create_random_path(obj, 5, -1);
					break;
				#ifndef NDEBUG
				case AIM_FOLLOW_PATH_2:
					Int3(); // Should never happen!
					break;
				#endif
			}
			ailp->consecutive_retries = 0;
		}
	} else
		ailp->consecutive_retries /= 2;

	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	// If in materialization center, exit
	if (!(Game_mode & GM_MULTI) && (Segment2s[obj->segnum].special == SEGMENT_IS_ROBOTMAKER)) {
		d1_in_d2_ai_follow_path(obj, 1);		// 1 = player is visible, which might be a lie, but it works.
		return D1_AI_FRAME_DEFER;
	}

	// Decrease player awareness due to the passage of time.
	if (ailp->player_awareness_type) {
		if (ailp->player_awareness_time > 0) {
			ailp->player_awareness_time -= FrameTime;
			if (ailp->player_awareness_time <= 0) {
				ailp->player_awareness_time = F1_0*2;   //new: 11/05/94
				ailp->player_awareness_type--;          //new: 11/05/94
			}
		} else {
			ailp->player_awareness_type--;
			ailp->player_awareness_time = F1_0*2;
			//aip->GOAL_STATE = AIS_REST;
		}
	} else
		aip->GOAL_STATE = AIS_REST;                     //new: 12/13/94


	if (Player_is_dead && (ailp->player_awareness_type == 0))
		// SIM RNG: this decides whether a live robot re-paths toward the dead player
		if ((dist_to_player < F1_0*200) && (d_rand() < FrameTime/8)) {
			if ((aip->behavior != AIB_STILL) && (aip->behavior != AIB_RUN_FROM)) {
				if (!ai_multiplayer_awareness(obj, 30))
					return D1_AI_FRAME_DEFER;
				ai_multi_send_robot_position(objnum, -1);

				if (!((ailp->mode == AIM_FOLLOW_PATH) && (aip->cur_path_index < aip->path_length-1)))
				{
					if (dist_to_player < F1_0*30)
						d1_in_d2_ai_create_random_path(obj, 5, 1);
					else
						d1_in_d2_ai_create_path_to_player(obj, 20, 1);
				}
			}
		}

	//	Make sure that if this guy got hit or bumped, then he's chasing player.
	if ((ailp->player_awareness_type == PA_WEAPON_ROBOT_COLLISION) || (ailp->player_awareness_type >= PA_PLAYER_COLLISION)) {
		if ((aip->behavior != AIB_STILL) && (aip->behavior != D1_AIB_FOLLOW_PATH) && (aip->behavior != AIB_RUN_FROM) && (obj->id != ROBOT_BRAIN))
			ailp->mode = AIM_CHASE_OBJECT;
	}
	return D1_AI_FRAME_READY;
}

static int defer_frame(object *obj, fix distance, int previous_visibility, int frame_reference, const char *reason)
{
	ai_static *state = &obj->ctype.ai_info;
	ai_local *local = &Ai_local_info[obj - Objects];
	input_demo_note_ai_schedule_timeslice_return(obj);
	input_demo_note_ai_schedule_detail(reason, obj, previous_visibility,
		local->player_awareness_type, local->player_awareness_time,
		state->SKIP_AI_COUNT, local->time_since_processed, distance, frame_reference);
	input_demo_log_ai_schedule_probe(reason, obj, state, local, distance,
		previous_visibility, frame_reference);
	return D1_AI_FRAME_DEFER;
}

static int schedule_frame(object *obj, fix distance, int previous_visibility, int frame_reference)
{
	ai_static *state = &obj->ctype.ai_info;
	ai_local *local = &Ai_local_info[obj - Objects];
	/* Hit robots always process; the debug-selected actor also bypasses time slicing */
	if (local->player_awareness_type >= PA_WEAPON_ROBOT_COLLISION - 1)
		return D1_AI_FRAME_READY;
#ifndef NDEBUG
	if (Break_on_object == obj - Objects)
		return D1_AI_FRAME_READY;
#endif
	if (distance > F1_0*250 && local->time_since_processed <= F1_0*2)
		return defer_frame(obj, distance, previous_visibility, frame_reference, "d1_timeslice_return_250");
	/* Returning station robots bypass the nearer distance tiers */
	if (state->behavior == AIB_STATION && local->mode == AIM_FOLLOW_PATH && state->hide_segment != obj->segnum)
		return D1_AI_FRAME_READY;
	if (distance > F1_0*150 && local->time_since_processed <= F1_0)
		return defer_frame(obj, distance, previous_visibility, frame_reference, "d1_timeslice_return_150");
	if (distance > F1_0*100 && local->time_since_processed <= F1_0/2)
		return defer_frame(obj, distance, previous_visibility, frame_reference, "d1_timeslice_return_100");
	return D1_AI_FRAME_READY;
}

int d1_in_d2_ai_initialize_behavior(object *obj, ai_local *local, int behavior, int hide_segment)
{
	ai_static *state = &obj->ctype.ai_info;
	if (d1_in_d2_ai_actor_role(obj) != D1_AI_NATIVE_ENEMY)
		return 0;
	if (!behavior)
		behavior = AIB_NORMAL;
	local->mode = AIM_STILL;
	if (behavior != -1) {
		state->behavior = behavior;
		local->mode = initial_mode(behavior);
	} else if (state->behavior < AIB_STILL || state->behavior > AIB_STATION)
		state->behavior = AIB_NORMAL;

	if (behavior == D1_AIB_HIDE || behavior == D1_AIB_FOLLOW_PATH ||
		behavior == AIB_STATION || behavior == AIB_RUN_FROM) {
		state->hide_segment = hide_segment;
		local->goal_segment = hide_segment;
		state->hide_index = -1;
		state->cur_path_index = 0;
	}
	return 1;
}

/* Native ai_turn_towards_vector; fixed math defines fixdiv's zero divisor
 * Preserve that source result rather than D2's immobile-reactor shortcut */
void d1_in_d2_ai_turn_towards_vector(vms_vector *goal, object *obj, fix rate)
{
	if (obj->id == D1_ROBOT_BABY_SPIDER && obj->type == OBJ_ROBOT) {
		physics_turn_towards_vector(goal, obj, rate);
		return;
	}
	vms_vector forward = *goal;
	if (vm_vec_dot(goal, &obj->orient.fvec) < F1_0 - FrameTime/2) {
		vm_vec_scale(&forward, fixdiv(FrameTime, rate));
		vm_vec_add2(&forward, &obj->orient.fvec);
		if (vm_vec_normalize_quick(&forward) < F1_0/256)
			forward = *goal;
	}
	vm_vector_2_matrix(&obj->orient, &forward, NULL, &obj->orient.rvec);
}

static void turn_randomly(vms_vector *vec_to_player, object *obj, fix rate,
	int previous_visibility)
{
	vms_vector curvec;

	if (previous_visibility)
		if (d_rand() > 0x7400) {
			d1_in_d2_ai_turn_towards_vector(vec_to_player, obj, rate);
			return;
		}

	curvec = obj->mtype.phys_info.rotvel;
	curvec.y += F1_0/64;
	curvec.x += curvec.y/6;
	curvec.y += curvec.z/4;
	curvec.z += curvec.x/10;

	if (abs(curvec.x) > F1_0/8)
		curvec.x /= 4;
	if (abs(curvec.y) > F1_0/8)
		curvec.y /= 4;
	if (abs(curvec.z) > F1_0/8)
		curvec.z /= 4;

	obj->mtype.phys_info.rotvel = curvec;
}

static int brain_door(const object *obj)
{
	const segment *seg = &Segments[obj->segnum];
	for (int side = 0; side < MAX_SIDES_PER_SEGMENT; ++side) {
		const int wall_num = seg->sides[side].wall_num;
		if (wall_num < 0)
			continue;
		const wall *door = &Walls[wall_num];
		if (door->type == WALL_DOOR && door->keys == KEY_NONE &&
		    door->state == WALL_DOOR_CLOSED && !(door->flags & WALL_DOOR_LOCKED))
			return side;
	}
	return -1;
}

static int update_brain(object *obj, vms_vector *origin, vms_vector *direction,
	int *visibility, int *computed)
{
	if (obj->id != ROBOT_BRAIN)
		return D1_AI_FRAME_READY;
	ai_static *state = &obj->ctype.ai_info;
	ai_local *local = &Ai_local_info[obj-Objects];
	robot_info *info = &Robot_info[obj->id];
	if (ConsoleObject->segnum == obj->segnum) {
		if (!ai_multiplayer_awareness(obj, 97))
			return D1_AI_FRAME_DEFER;
		compute_visibility(obj, origin, local, direction, visibility, info, computed);
		move_away_from_player(obj, direction, 0);
		ai_multi_send_robot_position(obj-Objects, -1);
	} else if (local->mode != AIM_STILL) {
		const int side = brain_door(obj);
		if (side != -1) {
			local->mode = AIM_OPEN_DOOR;
			state->GOALSIDE = side;
		} else if (local->mode != AIM_FOLLOW_PATH) {
			if (!ai_multiplayer_awareness(obj, 50))
				return D1_AI_FRAME_DEFER;
			d1_in_d2_ai_create_random_path(obj, 8 + Difficulty_level, -1);
			ai_multi_send_robot_position(obj-Objects, -1);
		}
	} else {
		compute_visibility(obj, origin, local, direction, visibility, info, computed);
		if (*visibility) {
			if (!ai_multiplayer_awareness(obj, 50))
				return D1_AI_FRAME_DEFER;
			d1_in_d2_ai_create_random_path(obj, 8 + Difficulty_level, -1);
			ai_multi_send_robot_position(obj-Objects, -1);
		}
	}
	return D1_AI_FRAME_READY;
}

/* Native hiding movement after the frame's awareness/firing prelude */
static void hide(object *obj, int visibility)
{
	ai_static *state = &obj->ctype.ai_info;
	d1_in_d2_ai_follow_path(obj, visibility);
	if (state->GOAL_STATE != AIS_FLIN || state->CURRENT_STATE == AIS_FLIN)
		state->GOAL_STATE = AIS_LOCK;
	ai_multi_send_robot_position(obj-Objects, -1);
	return;
}

static int run_from(object *obj, int visibility)
{
	ai_static *state = &obj->ctype.ai_info;
	ai_local *local = &Ai_local_info[obj-Objects];
	if (visibility && !local->player_awareness_type)
		local->player_awareness_type = PA_WEAPON_ROBOT_COLLISION;
	if (!(Game_mode & GM_MULTI) || visibility)
		if (ai_multiplayer_awareness(obj, 75)) {
			d1_in_d2_ai_follow_path(obj, visibility);
			ai_multi_send_robot_position(obj-Objects, -1);
		}
	if (state->GOAL_STATE != AIS_FLIN || state->CURRENT_STATE == AIS_FLIN)
		state->GOAL_STATE = AIS_LOCK;
	/* Only drop while visible, so unseen bombs do not give the robot away
	 * Fleeing robots must not turn around to aim at the player */
	if (local->next_fire <= 0 && visibility) {
		vms_vector fire_vec, fire_pos;
		if (!ai_multiplayer_awareness(obj, 75))
			return D1_AI_FRAME_DEFER;
		fire_vec = obj->orient.fvec;
		vm_vec_negate(&fire_vec);
		vm_vec_add(&fire_pos, &obj->pos, &fire_vec);
		Laser_create_new_easy(&fire_vec, &fire_pos, obj-Objects, PROXIMITY_ID, 1);
		local->next_fire = F1_0*5;
#ifdef NETWORK
#ifndef SHAREWARE
		if (Game_mode & GM_MULTI) {
			ai_multi_send_robot_position(obj-Objects, -1);
			multi_send_robot_fire(obj-Objects, -1, &fire_vec);
		}
#endif
#endif
	}
	return D1_AI_FRAME_READY;
}

int d1_in_d2_ai_next_fire_time(const object *obj, ai_local *ailp, robot_info *robptr)
{
	if (d1_in_d2_ai_actor_role(obj) != D1_AI_NATIVE_ENEMY)
		return 0;
	ailp->rapidfire_count++;

	if (ailp->rapidfire_count < robptr->rapidfire_count[Difficulty_level]) {
		ailp->next_fire = min(F1_0/8, robptr->firing_wait[Difficulty_level]/2);
	} else {
		ailp->rapidfire_count = 0;
		ailp->next_fire = robptr->firing_wait[Difficulty_level];
	}
	return 1;
}

static void circle_player(object *objp, vms_vector *vec_to_player, int fast_flag)
{
	physics_info	*pptr = &objp->mtype.phys_info;
	fix				speed;
	robot_info		*robptr = &Robot_info[objp->id];
	int				dir;
	vms_vector		evade_vector;

	if (fast_flag == 0)
		return;

	dir = ((objp-Objects) ^ ((d_tick_count + 3*(objp-Objects)) >> 5)) & 3;

	Assert((dir >= 0) && (dir <= 3));

	switch (dir) {
		case 0:
			evade_vector.x = fixmul(vec_to_player->z, FrameTime*32);
			evade_vector.y = fixmul(vec_to_player->y, FrameTime*32);
			evade_vector.z = fixmul(-vec_to_player->x, FrameTime*32);
			break;
		case 1:
			evade_vector.x = fixmul(-vec_to_player->z, FrameTime*32);
			evade_vector.y = fixmul(vec_to_player->y, FrameTime*32);
			evade_vector.z = fixmul(vec_to_player->x, FrameTime*32);
			break;
		case 2:
			evade_vector.x = fixmul(-vec_to_player->y, FrameTime*32);
			evade_vector.y = fixmul(vec_to_player->x, FrameTime*32);
			evade_vector.z = fixmul(vec_to_player->z, FrameTime*32);
			break;
		case 3:
			evade_vector.x = fixmul(vec_to_player->y, FrameTime*32);
			evade_vector.y = fixmul(-vec_to_player->x, FrameTime*32);
			evade_vector.z = fixmul(vec_to_player->z, FrameTime*32);
			break;
	}

	//	Note: -1 means normal circling about the player.  > 0 means fast evasion.
	if (fast_flag > 0) {
		fix	dot;

		//	Only take evasive action if looking at player.
		//	Evasion speed is scaled by percentage of shields left so wounded robots evade less effectively.

		dot = vm_vec_dot(vec_to_player, &objp->orient.fvec);
		if ((dot > robptr->field_of_view[Difficulty_level]) && !(ConsoleObject->flags & PLAYER_FLAGS_CLOAKED)) {
			fix	damage_scale;

			/* Retain the engine safeguard for zero-strength custom definitions */
			damage_scale = robptr->strength ? fixdiv(objp->shields, robptr->strength) : F1_0;
			if (damage_scale > F1_0)
				damage_scale = F1_0;		//	Just in case...
			else if (damage_scale < 0)
				damage_scale = 0;			//	Just in case...

			vm_vec_scale(&evade_vector, i2f(fast_flag) + damage_scale);
		}
	}

	pptr->velocity.x += evade_vector.x;
	pptr->velocity.y += evade_vector.y;
	pptr->velocity.z += evade_vector.z;

	speed = vm_vec_mag_quick(&pptr->velocity);
	if (speed > robptr->max_speed[Difficulty_level]) {
		pptr->velocity.x = (pptr->velocity.x*3)/4;
		pptr->velocity.y = (pptr->velocity.y*3)/4;
		pptr->velocity.z = (pptr->velocity.z*3)/4;
	}

}

static void move_relative(object *objp, fix dist_to_player,
	vms_vector *vec_to_player, fix circle_distance, int evade_only)
{
	ai_local *ailp = &Ai_local_info[objp - Objects];
	object		*dobjp;
	robot_info	*robptr = &Robot_info[objp->id];

	//	See if should take avoidance.

// New way, green guys don't evade:	if ((robptr->attack_type == 0) && (objp->ctype.ai_info.danger_laser_num != -1)) {
	if (objp->ctype.ai_info.danger_laser_num != -1) {
		dobjp = &Objects[objp->ctype.ai_info.danger_laser_num];

		if ((dobjp->type == OBJ_WEAPON) && (dobjp->signature == objp->ctype.ai_info.danger_laser_signature)) {
			fix			dot, dist_to_laser, field_of_view;
			vms_vector	vec_to_laser, laser_fvec;

			field_of_view = Robot_info[objp->id].field_of_view[Difficulty_level];

			vm_vec_sub(&vec_to_laser, &dobjp->pos, &objp->pos);
			dist_to_laser = vm_vec_normalize_quick(&vec_to_laser);
			dot = vm_vec_dot(&vec_to_laser, &objp->orient.fvec);

			if (dot > field_of_view) {
				fix			laser_robot_dot;
				vms_vector	laser_vec_to_robot;

				//	The laser is seen by the robot, see if it might hit the robot.
				//	Get the laser's direction.  If it's a polyobj, it can be gotten cheaply from the orientation matrix.
				if (dobjp->render_type == RT_POLYOBJ)
					laser_fvec = dobjp->orient.fvec;
				else {		//	Not a polyobj, get velocity and normalize.
					laser_fvec = dobjp->mtype.phys_info.velocity;	//dobjp->orient.fvec;
					vm_vec_normalize_quick(&laser_fvec);
				}
				vm_vec_sub(&laser_vec_to_robot, &objp->pos, &dobjp->pos);
				vm_vec_normalize_quick(&laser_vec_to_robot);
				laser_robot_dot = vm_vec_dot(&laser_fvec, &laser_vec_to_robot);

				if ((laser_robot_dot > F1_0*7/8) && (dist_to_laser < F1_0*80)) {
					int	evade_speed;

					ai_evaded = 1;
					evade_speed = Robot_info[objp->id].evade_speed[Difficulty_level];

					circle_player(objp, vec_to_player, evade_speed);
				}
			}
			return;
		}
	}

	//	If only allowed to do evade code, then done.
	//	Hmm, perhaps brilliant insight.  If want claw-type guys to keep coming, don't return here after evasion.
	if ((!robptr->attack_type) && evade_only)
		return;

	//	If we fall out of above, then no object to be avoided.
	objp->ctype.ai_info.danger_laser_num = -1;

	//	Green guy selects move around/towards/away based on firing time, not distance.
	if (robptr->attack_type == 1) {
		if (((ailp->next_fire > robptr->firing_wait[Difficulty_level]/4) && (dist_to_player < F1_0*30)) || Player_is_dead) {
			//	1/4 of time, move around player, 3/4 of time, move away from player
			// SIM RNG: this chooses the robot's live evasive movement
			if (d_rand() < 8192) {
				circle_player(objp, vec_to_player, -1);
			} else {
				move_away_from_player(objp, vec_to_player, 1);
			}
		} else {
			move_towards_player(objp, vec_to_player);
		}
	} else {
		if (dist_to_player < circle_distance)
			move_away_from_player(objp, vec_to_player, 0);
		else if (dist_to_player < circle_distance*2)
			circle_player(objp, vec_to_player, -1);
		else
			move_towards_player(objp, vec_to_player);
	}

	return;
}

static void aim_at_player(vms_vector *fire_vec, vms_vector *believed_player_pos, vms_vector *fire_point)
{
	vms_vector bpp_diff;

	bpp_diff.x = believed_player_pos->x + (d_rand()-16384) * (NDL-Difficulty_level-1) * 4;
	bpp_diff.y = believed_player_pos->y + (d_rand()-16384) * (NDL-Difficulty_level-1) * 4;
	bpp_diff.z = believed_player_pos->z + (d_rand()-16384) * (NDL-Difficulty_level-1) * 4;
	(void)d_rand();
	vm_vec_normalized_dir_quick(fire_vec, &bpp_diff, fire_point);
}

/* A shot is a complete native operation, including suppression and completion */
static void fire_laser(object *obj, vms_vector *fire_point)
{
	const int objnum = obj - Objects;
	ai_local *local = &Ai_local_info[objnum];
	robot_info *info = &Robot_info[obj->id];
	vms_vector direction;
	if (cheats.robotfiringsuspended)
		return;
	Assert(info->attack_type != 1);
	if (obj->control_type == CT_MORPH || Player_exploded)
		return;

	// If player is cloaked, maybe don't fire based on how long cloaked and randomness
	if (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED) {
		const fix64 cloak_time = Ai_cloak_info[objnum % MAX_AI_CLOAK_INFO].last_time;
		if (GameTime64 - cloak_time > CLOAK_TIME_MAX/4)
			// SIM RNG: this decides whether cloaking suppresses a live shot
			if (d_rand() > fixdiv(GameTime64 - cloak_time, CLOAK_TIME_MAX)/2) {
				d1_in_d2_ai_next_fire_time(obj, local, info);
				return;
			}
	}

	/* D1 hide submode shares storage with D2 flags, not their interpretation */
	aim_at_player(&direction, &Believed_player_pos, fire_point);
	Laser_create_new_easy(&direction, fire_point, objnum, info->weapon_type, 1);
	input_demo_log_robot_fire_probe(obj, &direction, info->weapon_type);
#ifdef NETWORK
	if (Game_mode & GM_MULTI) {
		ai_multi_send_robot_position(objnum, -1);
		multi_send_robot_fire(objnum, obj->ctype.ai_info.CURRENT_GUN, &direction);
	}
#endif
	if (input_demo_trace_robot_fire_active(obj))
		input_demo_log_robot_fire_state("robot_fire before_awareness", obj, info->weapon_type);
	input_demo_set_awareness_source("ai2_robot_fire", objnum, info->weapon_type);
	d1_in_d2_ai_create_awareness_event(obj, PA_NEARBY_ROBOT_FIRED);
	if (input_demo_trace_robot_fire_active(obj))
		input_demo_log_robot_fire_state("robot_fire after_awareness", obj, info->weapon_type);
	d1_in_d2_ai_next_fire_time(obj, local, info);
	if (input_demo_trace_robot_fire_active(obj))
		input_demo_log_robot_fire_state("robot_fire after_set_next_fire_time", obj, info->weapon_type);

	// If the boss fired, allow him to teleport very soon, pending other factors
	if (info->boss_flag)
		Last_teleport_time -= Boss_teleport_interval/2;
	return;
}

static void prepare_fire(object *obj, int player_visibility, vms_vector *vec_to_player)
{
	if (player_visibility >= 1) {
		//	Now, if in robot's field of view, lock onto player
		fix	dot = vm_vec_dot(&obj->orient.fvec, vec_to_player);
		if ((dot >= 7*F1_0/8) || (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED)) {
			ai_static	*aip = &obj->ctype.ai_info;
			ai_local		*ailp = &Ai_local_info[obj-Objects];

			switch (aip->GOAL_STATE) {
				case AIS_NONE:
				case AIS_REST:
				case AIS_SRCH:
				case AIS_LOCK:
					aip->GOAL_STATE = AIS_FIRE;
					if (ailp->player_awareness_type <= PA_NEARBY_ROBOT_FIRED) {
						ailp->player_awareness_type = PA_NEARBY_ROBOT_FIRED;
						ailp->player_awareness_time = PLAYER_AWARENESS_INITIAL_TIME;
					}
					break;
			}
		} else if (dot >= F1_0/2) {
			ai_static	*aip = &obj->ctype.ai_info;
			switch (aip->GOAL_STATE) {
				case AIS_NONE:
				case AIS_REST:
				case AIS_SRCH:
					aip->GOAL_STATE = AIS_LOCK;
					break;
			}
		}
	}
	return;
}

static void fire_at_player(object *obj, vms_vector *vec_to_player,
	fix dist_to_player, vms_vector *gun_point, int player_visibility, int object_animates)
{
	ai_static *aip = &obj->ctype.ai_info;
	ai_local *ailp = &Ai_local_info[obj - Objects];
	robot_info *robptr = &Robot_info[obj->id];
	fix	dot;
	#define D1_FIRE_GATE(label, dot_value, dot_gate, melee_value, hit_value) \
		input_demo_log_ai_fire_gate_probe(obj, label, aip->CURRENT_GUN, \
			player_visibility, dist_to_player, dot_value, dot_gate, \
			-1, -1, melee_value, hit_value, object_animates)

	if (player_visibility == 2) {
		//	Changed by mk, 01/04/94, onearm would take about 9 seconds until he can fire at you.
		// if (((!object_animates) || (ailp->achieved_state[aip->CURRENT_GUN] == AIS_FIRE)) && (ailp->next_fire <= 0)) {
		if (!object_animates || (ailp->next_fire <= 0)) {
			dot = vm_vec_dot(&obj->orient.fvec, vec_to_player);
			if (dot >= 7*F1_0/8) {

				if (aip->CURRENT_GUN < Robot_info[obj->id].n_guns) {
					if (robptr->attack_type == 1) {
						if (!Player_exploded && (dist_to_player < obj->size + ConsoleObject->size + F1_0*2)) {		// robptr->circle_distance[Difficulty_level] + ConsoleObject->size) {
							if (!ai_multiplayer_awareness(obj, ROBOT_FIRE_AGITATION-2)) {
								D1_FIRE_GATE("gate_direct_awareness_blocked", dot, 7*F1_0/8, obj->size + ConsoleObject->size + F1_0*2, -1);
								return;
							}
							do_ai_robot_hit_attack(obj, ConsoleObject, &obj->pos);
						} else {
							D1_FIRE_GATE("gate_direct_melee_blocked", dot, 7*F1_0/8, obj->size + ConsoleObject->size + F1_0*2, -1);
							return;
						}
					} else {
						if ((gun_point->x == 0) && (gun_point->y == 0) && (gun_point->z == 0)) {
							D1_FIRE_GATE("gate_direct_gun_point_null", dot, 7*F1_0/8, -1, -1);
						} else {
							if (!ai_multiplayer_awareness(obj, ROBOT_FIRE_AGITATION)) {
								D1_FIRE_GATE("gate_direct_awareness_blocked", dot, 7*F1_0/8, -1, -1);
								return;
							}
							fire_laser(obj, gun_point);
							input_demo_log_ai_fire_probe(obj, "actual_fire_direct", aip->CURRENT_GUN, player_visibility, dist_to_player);
						}
					}

					//	Wants to fire, so should go into chase mode, probably.
					if ( (aip->behavior != AIB_RUN_FROM) && (aip->behavior != AIB_STILL) && (aip->behavior != D1_AIB_FOLLOW_PATH) && ((ailp->mode == AIM_FOLLOW_PATH) || (ailp->mode == AIM_STILL)))
						ailp->mode = AIM_CHASE_OBJECT;
				} else {
					D1_FIRE_GATE("gate_direct_gun_oob", dot, 7*F1_0/8, -1, -1);
				}

				aip->GOAL_STATE = AIS_RECO;
				ailp->goal_state[aip->CURRENT_GUN] = AIS_RECO;

				// Switch to next gun for next fire.
				aip->CURRENT_GUN++;
				if (aip->CURRENT_GUN >= Robot_info[obj->id].n_guns)
					aip->CURRENT_GUN = 0;
			} else {
				D1_FIRE_GATE("gate_direct_dot_low", dot, 7*F1_0/8, -1, -1);
			}
		} else {
			D1_FIRE_GATE("gate_direct_not_ready", -1, 7*F1_0/8, -1, -1);
		}
	} else if (Weapon_info[Robot_info[obj->id].weapon_type].homing_flag == 1) {
		//	Robots which fire homing weapons might fire even if they don't have a bead on the player.
		const fix hit_distance = vm_vec_dist_quick(&Hit_pos, &obj->pos);
		if (((!object_animates) || (ailp->achieved_state[aip->CURRENT_GUN] == AIS_FIRE)) && (ailp->next_fire <= 0) && (hit_distance > F1_0*40)) {
			if (!ai_multiplayer_awareness(obj, ROBOT_FIRE_AGITATION)) {
				D1_FIRE_GATE("gate_homing_awareness_blocked", -1, -1, -1, hit_distance);
				return;
			}
			if (IS_VEC_NULL(gun_point)) {
				D1_FIRE_GATE("gate_homing_gun_point_null", -1, -1, -1, hit_distance);
				return;
			}
			fire_laser(obj, gun_point);
			input_demo_log_ai_fire_probe(obj, "actual_fire_homing", aip->CURRENT_GUN, player_visibility, dist_to_player);

			aip->GOAL_STATE = AIS_RECO;
			ailp->goal_state[aip->CURRENT_GUN] = AIS_RECO;

			// Switch to next gun for next fire.
			aip->CURRENT_GUN++;
			if (aip->CURRENT_GUN >= Robot_info[obj->id].n_guns)
				aip->CURRENT_GUN = 0;
		} else {
			D1_FIRE_GATE("gate_homing_preconditions", -1, -1, -1, hit_distance);
			// Switch to next gun for next fire.
			aip->CURRENT_GUN++;
			if (aip->CURRENT_GUN >= Robot_info[obj->id].n_guns)
				aip->CURRENT_GUN = 0;
		}
	}
	#undef D1_FIRE_GATE
	return;
}

/* Native D1 perception uses the shared FVI result and AI cloak cache
 * It never interprets the D1 hide submode as D2 gun-segment flags */
static int sight_visibility(object *objp, vms_vector *pos, fix field_of_view, vms_vector *vec_to_player)
{
	fix			dot;
	fvi_query	fq;
	int			visibility_result;

	fq.p0						= pos;
	if ((pos->x != objp->pos.x) || (pos->y != objp->pos.y) || (pos->z != objp->pos.z)) {
		int	segnum = find_point_seg(pos, objp->segnum);
		if (segnum == -1) {
			fq.startseg = objp->segnum;
			*pos = objp->pos;
			con_printf(CON_DEBUG, "Object %hu, gun is outside mine, moving towards center.\n", (unsigned short)(objp-Objects));
			move_towards_segment_center(objp);
		} else
			fq.startseg = segnum;
	} else
		fq.startseg			= objp->segnum;
	fq.p1						= &Believed_player_pos;
	fq.rad					= F1_0/4;
	fq.thisobjnum			= objp-Objects;
	fq.ignore_obj_list	= NULL;
	fq.flags					= FQ_TRANSWALL | FQ_CHECK_OBJS;		//what about trans walls???

	Hit_type = find_vector_intersection(&fq,&Hit_data);

	Hit_pos = Hit_data.hit_pnt;
	Hit_seg = Hit_data.hit_seg;

	dot = 0;
	visibility_result = 0;
	if ((Hit_type == HIT_NONE) || ((Hit_type == HIT_OBJECT) && (Hit_data.hit_object == Players[Player_num].objnum))) {
		dot = vm_vec_dot(vec_to_player, &objp->orient.fvec);
		if (dot > field_of_view - (Overall_agitation << 9)) {
			visibility_result = 2;
		} else {
			visibility_result = 1;
		}
	}

	input_demo_log_ai_visibility_fvi_probe(objp, "player_is_visible",
		visibility_result, Hit_type, Hit_seg, Hit_data.hit_object,
		fq.startseg, fq.flags, dot, field_of_view, &Hit_pos, pos,
		&Believed_player_pos);
	return visibility_result;
}

static void compute_visibility(object *objp, vms_vector *pos, ai_local *ailp, vms_vector *vec_to_player, int *player_visibility, robot_info *robptr, int *flag)
{
	if (!*flag) {
		int previous_visibility_before = ailp->previous_visibility;
		int raw_player_visibility = -1;
		int sight_sound_gate = 0;
		int attack_sound_gate = 0;
		int misc_sound_gate = 0;

		if (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED) {
			fix			delta_time, dist;
			int			cloak_index = (objp-Objects) % MAX_AI_CLOAK_INFO;

			delta_time = GameTime64 - Ai_cloak_info[cloak_index].last_time;
			if (delta_time > F1_0*2) {
				vms_vector	randvec;

				Ai_cloak_info[cloak_index].last_time = GameTime64;
				make_random_vector(&randvec);
				vm_vec_scale_add2(&Ai_cloak_info[cloak_index].last_position, &randvec, 8*delta_time );
			}

			dist = vm_vec_normalized_dir_quick(vec_to_player, &Ai_cloak_info[cloak_index].last_position, pos);
			*player_visibility = sight_visibility(objp, pos, robptr->field_of_view[Difficulty_level], vec_to_player);
			raw_player_visibility = *player_visibility;
			// *player_visibility = 2;

			// FX RNG: sound only, this just jitters robot chatter cadence
			if ((ailp->next_misc_sound_time < GameTime64) && (ailp->next_fire < F1_0) && (dist < F1_0*20)) {
				misc_sound_gate = 1;
				ailp->next_misc_sound_time = GameTime64 + (d_rand_fx() + F1_0) * (7 - Difficulty_level) / 1;
				digi_link_sound_to_pos( robptr->see_sound, objp->segnum, 0, pos, 0 , Robot_sound_volume);
			}
		} else {
			//	Compute expensive stuff -- vec_to_player and player_visibility
			vm_vec_normalized_dir_quick(vec_to_player, &Believed_player_pos, pos);
			if ((vec_to_player->x == 0) && (vec_to_player->y == 0) && (vec_to_player->z == 0)) {
				con_printf(CON_DEBUG, "Warning: Player and robot at exactly the same location.\n");
				vec_to_player->x = F1_0;
			}
			*player_visibility = sight_visibility(objp, pos, robptr->field_of_view[Difficulty_level], vec_to_player);
			raw_player_visibility = *player_visibility;

			//	This horrible code added by MK in desperation on 12/13/94 to make robots wake up as soon as they
			//	see you without killing frame rate.
			{
				ai_static	*aip = &objp->ctype.ai_info;
			if ((*player_visibility == 2) && (ailp->previous_visibility != 2))
				if ((aip->GOAL_STATE == AIS_REST) || (aip->CURRENT_STATE == AIS_REST)) {
					aip->GOAL_STATE = AIS_FIRE;
					aip->CURRENT_STATE = AIS_FIRE;
				}
			}

			// FX RNG: sound only, these timers only vary robot sight and attack chatter cadence
			if (!Player_exploded && (ailp->previous_visibility != *player_visibility) && (*player_visibility == 2)) {
				if (ailp->previous_visibility == 0) {
					if (ailp->time_player_seen + F1_0/2 < GameTime64) {
						sight_sound_gate = 1;
						digi_link_sound_to_pos( robptr->see_sound, objp->segnum, 0, pos, 0 , Robot_sound_volume);
						ailp->time_player_sound_attacked = GameTime64;
						ailp->next_misc_sound_time = GameTime64 + F1_0 + d_rand_fx()*4;
					}
				} else if (ailp->time_player_sound_attacked + F1_0/4 < GameTime64) {
					attack_sound_gate = 1;
					digi_link_sound_to_pos( robptr->attack_sound, objp->segnum, 0, pos, 0 , Robot_sound_volume);
					ailp->time_player_sound_attacked = GameTime64;
				}
			}

			if ((*player_visibility == 2) && (ailp->next_misc_sound_time < GameTime64)) {
				misc_sound_gate = 1;
				ailp->next_misc_sound_time = GameTime64 + (d_rand_fx() + F1_0) * (7 - Difficulty_level) / 2;
				digi_link_sound_to_pos( robptr->attack_sound, objp->segnum, 0, pos, 0 , Robot_sound_volume);
			}
			ailp->previous_visibility = *player_visibility;
		}

		*flag = 1;

		if (*player_visibility) {
			ailp->time_player_seen = GameTime64;
		}

		if (input_demo_trace_ai_visibility_active(objp) &&
			(raw_player_visibility >= 0) &&
			((previous_visibility_before != raw_player_visibility) ||
			 (raw_player_visibility != *player_visibility) || sight_sound_gate ||
			 attack_sound_gate || misc_sound_gate)) {
			input_demo_log_ai_visibility_probe(objp, "compute_vis",
				previous_visibility_before, raw_player_visibility,
				*player_visibility, sight_sound_gate, attack_sound_gate,
				misc_sound_gate, pos, &Believed_player_pos);
		}
	}

	return;
}

/* Native frame order adapted from d1/main/ai.c::do_ai_frame
 * Engine route/replica guards precede the public dispatch. All native early
 * returns and completion stay here; shared services do not select game policy */
static void native_frame(object *obj)
{
	const int objnum = obj - Objects;
	ai_static *aip = &obj->ctype.ai_info;
	ai_local *ailp = &Ai_local_info[objnum];
	robot_info *robptr = &Robot_info[obj->id];
	fix dist_to_player, schedule_dist_to_player, dot;
	vms_vector vec_to_player, gun_point, vis_vec_pos;
	int player_visibility = -1, visibility_and_vec_computed = 0;
	int object_animates, new_goal_state;
	const int obj_ref = objnum ^ d_tick_count;
	if (prepare_frame(obj, &dist_to_player, &gun_point, &vis_vec_pos) == D1_AI_FRAME_DEFER)
		return;
	const int previous_visibility = ailp->previous_visibility;
	schedule_dist_to_player = dist_to_player;
	if (prepare_behavior(obj, dist_to_player) == D1_AI_FRAME_DEFER)
		return;
	if (aip->GOAL_STATE == AIS_FLIN && aip->CURRENT_STATE == AIS_FLIN)
		aip->GOAL_STATE = AIS_LOCK;
	if (Animation_enabled && dist_to_player < F1_0*100) {
		object_animates = do_silly_animation(obj);
		if (object_animates)
			ai_frame_animation(obj);
	} else {
		aip->CURRENT_STATE = aip->GOAL_STATE;
		object_animates = 0;
	}
	update_boss(obj, &dist_to_player, &vis_vec_pos, &vec_to_player,
		&player_visibility, &visibility_and_vec_computed);
	if (schedule_frame(obj, dist_to_player, previous_visibility, obj_ref) == D1_AI_FRAME_DEFER)
		return;
	input_demo_note_ai_schedule_process(obj);
	input_demo_note_ai_schedule_detail("process_enter", obj,
		previous_visibility, ailp->player_awareness_type,
		ailp->player_awareness_time, aip->SKIP_AI_COUNT,
		ailp->time_since_processed, schedule_dist_to_player, obj_ref);
	input_demo_log_ai_schedule_probe("process_enter", obj, aip, ailp,
		schedule_dist_to_player, previous_visibility, obj_ref);
	ailp->time_since_processed = -((objnum & 3) * FrameTime) / 2;
	if (update_brain(obj, &vis_vec_pos, &vec_to_player,
		&player_visibility, &visibility_and_vec_computed) == D1_AI_FRAME_DEFER)
		return;

	switch (ailp->mode) {
		case AIM_CHASE_OBJECT: {        // chasing player, sort of, chase if far, back off if close, circle in between
			fix circle_distance;
			int chase_path_pre_mode = ailp->mode;
			int chase_path_pre_goal_segment = ailp->goal_segment;
			int chase_path_pre_path_index = aip->cur_path_index;
			int chase_path_pre_path_length = aip->path_length;
			int chase_path_pre_hide_index = aip->hide_index;
			int chase_path_pre_path_dir = aip->PATH_DIR;
			int64_t chase_path_pre_time_player_seen = ailp->time_player_seen;
			int chase_path_gate_pass;
			int chase_path_awareness_allowed = 1;
			int chase_path_created = 0;

			circle_distance = robptr->circle_distance[Difficulty_level] + ConsoleObject->size;
			// Green guy doesn't get his circle distance boosted, else he might never attack.
			if (robptr->attack_type != 1)
				circle_distance += (objnum&0xf) * F1_0/2;

			compute_visibility(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);
			chase_path_gate_pass = (player_visibility < 2) && (previous_visibility == 2);

			// @mk, 12/27/94, structure here was strange.  Would do both clauses of what are now this if/then/else.  Used to be if/then, if/then.
			if (chase_path_gate_pass) { // this is redundant: mk, 01/15/95: && (ailp->mode == AIM_CHASE_OBJECT)) {
				if (!ai_multiplayer_awareness(obj, 53)) {
					chase_path_awareness_allowed = 0;
					input_demo_log_ai_chase_path_gate(obj,
						dist_to_player,
						previous_visibility,
						player_visibility,
						chase_path_gate_pass,
						chase_path_awareness_allowed,
						chase_path_created,
						chase_path_pre_mode,
						chase_path_pre_goal_segment,
						chase_path_pre_path_index,
						chase_path_pre_path_length,
						chase_path_pre_hide_index,
						chase_path_pre_path_dir,
						chase_path_pre_time_player_seen);
					if (maybe_ai_do_actual_firing_stuff(obj, aip))
						fire_at_player(obj, &vec_to_player, dist_to_player, &gun_point, player_visibility, object_animates);
					return;
				}
				d1_in_d2_ai_create_path_to_player(obj, 8, 1);
				chase_path_created = 1;
				ai_multi_send_robot_position(objnum, -1);
			}

			input_demo_log_ai_chase_path_gate(obj,
				dist_to_player,
				previous_visibility,
				player_visibility,
				chase_path_gate_pass,
				chase_path_awareness_allowed,
				chase_path_created,
				chase_path_pre_mode,
				chase_path_pre_goal_segment,
				chase_path_pre_path_index,
				chase_path_pre_path_length,
				chase_path_pre_hide_index,
				chase_path_pre_path_dir,
				chase_path_pre_time_player_seen);

			if (!chase_path_gate_pass &&
				(player_visibility == 0) && (dist_to_player > F1_0*80) && (!(Game_mode & GM_MULTI))) {
				// If pretty far from the player, player cannot be seen
				// (obstructed) and in chase mode, switch to follow path mode.
				// This has one desirable benefit of avoiding physics retries.
				if (aip->behavior == AIB_STATION) {
					ailp->goal_segment = aip->hide_segment;
					d1_in_d2_ai_create_path_to_station(obj, 15);
				} else {
					d1_in_d2_ai_create_random_path(obj, 5, -1);
				}
				break;
			}

			if ((aip->CURRENT_STATE == AIS_REST) && (aip->GOAL_STATE == AIS_REST)) {
				if (player_visibility) {
					// SIM RNG: these rolls decide whether a live resting robot wakes into search state
					if (d_rand() < FrameTime*player_visibility) {
						if (dist_to_player/256 < d_rand()*player_visibility) {
							aip->GOAL_STATE = AIS_SRCH;
							aip->CURRENT_STATE = AIS_SRCH;
						}
					}
				}
			}

			if (GameTime64 - ailp->time_player_seen > CHASE_TIME_LENGTH) {

				if (Game_mode & GM_MULTI)
					if (!player_visibility && (dist_to_player > F1_0*70)) {
						ailp->mode = AIM_STILL;
						return;
					}

				if (!ai_multiplayer_awareness(obj, 64)) {
					if (maybe_ai_do_actual_firing_stuff(obj, aip))
						fire_at_player(obj, &vec_to_player, dist_to_player, &gun_point, player_visibility, object_animates);
					return;
				}
				d1_in_d2_ai_create_path_to_player(obj, 10, 1);
				ai_multi_send_robot_position(objnum, -1);
			} else if ((aip->CURRENT_STATE != AIS_REST) && (aip->GOAL_STATE != AIS_REST)) {
				if (!ai_multiplayer_awareness(obj, 70)) {
					if (maybe_ai_do_actual_firing_stuff(obj, aip))
						fire_at_player(obj, &vec_to_player, dist_to_player, &gun_point, player_visibility, object_animates);
					return;
				}
				move_relative(obj, dist_to_player, &vec_to_player, circle_distance, 0);

				if ((obj_ref & 1) && ((aip->GOAL_STATE == AIS_SRCH) || (aip->GOAL_STATE == AIS_LOCK))) {
					if (player_visibility) // == 2)
						d1_in_d2_ai_turn_towards_vector(&vec_to_player, obj, robptr->turn_time[Difficulty_level]);
					else
						turn_randomly(&vec_to_player, obj,
							robptr->turn_time[Difficulty_level], previous_visibility);
				}

				if (ai_evaded) {
					ai_multi_send_robot_position(objnum, 1);
					ai_evaded = 0;
				} else
					ai_multi_send_robot_position(objnum, -1);

				prepare_fire(obj, player_visibility, &vec_to_player);
			}
			break;
		}

		case AIM_RUN_FROM_OBJECT:
			compute_visibility(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);
			if (run_from(obj, player_visibility) == D1_AI_FRAME_DEFER)
				return;
			break;

		case AIM_FOLLOW_PATH: {
			int anger_level = 65;
			int follow_path_pre_mode = ailp->mode;
			int follow_path_pre_goal_segment = ailp->goal_segment;
			int follow_path_pre_path_index = aip->cur_path_index;
			int follow_path_pre_path_length = aip->path_length;
			int follow_path_pre_hide_index = aip->hide_index;
			int follow_path_pre_path_dir = aip->PATH_DIR;
			int64_t follow_path_pre_time_player_seen = ailp->time_player_seen;
			int follow_path_awareness_allowed = 1;
			int follow_path_called = 0;
			int follow_path_visible_chase_pass = 0;
			int follow_path_still_pass = 0;

			if (aip->behavior == AIB_STATION)
				if (aip->path_length >= 1 && aip->hide_index >= 0 &&
				    aip->hide_index + aip->path_length <= Point_segs_free_ptr - Point_segs &&
				    Point_segs[aip->hide_index + aip->path_length - 1].segnum == aip->hide_segment) {
					anger_level = 64;
				}

			compute_visibility(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);

			if (!ai_multiplayer_awareness(obj, anger_level)) {
				follow_path_awareness_allowed = 0;
				input_demo_log_ai_follow_path_transition(obj,
					dist_to_player,
					anger_level,
					previous_visibility,
					player_visibility,
					follow_path_awareness_allowed,
					follow_path_called,
					follow_path_visible_chase_pass,
					follow_path_still_pass,
					follow_path_pre_mode,
					follow_path_pre_goal_segment,
					follow_path_pre_path_index,
					follow_path_pre_path_length,
					follow_path_pre_hide_index,
					follow_path_pre_path_dir,
					follow_path_pre_time_player_seen);
				if (maybe_ai_do_actual_firing_stuff(obj, aip)) {
					compute_visibility(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);
					fire_at_player(obj, &vec_to_player, dist_to_player, &gun_point, player_visibility, object_animates);
				}
				return;
			}

			d1_in_d2_ai_follow_path(obj, player_visibility);
			follow_path_called = 1;

			if (aip->GOAL_STATE != AIS_FLIN)
				aip->GOAL_STATE = AIS_LOCK;
			else if (aip->CURRENT_STATE == AIS_FLIN)
				aip->GOAL_STATE = AIS_LOCK;

			if ((aip->behavior != D1_AIB_FOLLOW_PATH) && (aip->behavior != AIB_RUN_FROM))
				prepare_fire(obj, player_visibility, &vec_to_player);

			follow_path_visible_chase_pass =
				(player_visibility == 2) &&
				(aip->behavior != D1_AIB_FOLLOW_PATH) &&
				(aip->behavior != AIB_RUN_FROM) &&
				(obj->id != ROBOT_BRAIN);

			if (follow_path_visible_chase_pass) {
				if (robptr->attack_type == 0)
					ailp->mode = AIM_CHASE_OBJECT;
				// This should not just be distance based, but also time-since-player-seen based.
			} else {
				follow_path_still_pass =
					(player_visibility == 0) &&
					(aip->behavior == AIB_NORMAL) &&
					(ailp->mode == AIM_FOLLOW_PATH);

				if (follow_path_still_pass) {
					ailp->mode = AIM_STILL;
					aip->hide_index = -1;
					aip->path_length = 0;
				}
			}

			input_demo_log_ai_follow_path_transition(obj,
				dist_to_player,
				anger_level,
				previous_visibility,
				player_visibility,
				follow_path_awareness_allowed,
				follow_path_called,
				follow_path_visible_chase_pass,
				follow_path_still_pass,
				follow_path_pre_mode,
				follow_path_pre_goal_segment,
				follow_path_pre_path_index,
				follow_path_pre_path_length,
				follow_path_pre_hide_index,
				follow_path_pre_path_dir,
				follow_path_pre_time_player_seen);

			ai_multi_send_robot_position(objnum, -1);

			break;
		}

		case D1_AIM_HIDE:
			if (!ai_multiplayer_awareness(obj, 71)) {
				if (maybe_ai_do_actual_firing_stuff(obj, aip)) {
					compute_visibility(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);
					fire_at_player(obj, &vec_to_player, dist_to_player, &gun_point, player_visibility, object_animates);
				}
				return;
			}

			compute_visibility(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);

			hide(obj, player_visibility);
			break;

		case AIM_STILL:
			if ((dist_to_player < F1_0*120+Difficulty_level*F1_0*20) || (ailp->player_awareness_type >= PA_WEAPON_ROBOT_COLLISION-1)) {
				compute_visibility(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);

				// turn towards vector if visible this time or last time, or rand
				// new!
				// SIM RNG: this decides whether a live idle robot turns to face the player
				if ((player_visibility) || (previous_visibility) || ((d_rand() > 0x4000) && !(Game_mode & GM_MULTI))) {
					if (!ai_multiplayer_awareness(obj, 71)) {
						if (maybe_ai_do_actual_firing_stuff(obj, aip))
							fire_at_player(obj, &vec_to_player, dist_to_player, &gun_point, player_visibility, object_animates);
						return;
					}
					d1_in_d2_ai_turn_towards_vector(&vec_to_player, obj, robptr->turn_time[Difficulty_level]);
					ai_multi_send_robot_position(objnum, -1);
				}

				prepare_fire(obj, player_visibility, &vec_to_player);
				//	This is debugging code!  Remove it!  It's to make the green guy attack without doing other kinds of movement.
				if (player_visibility) {		//	Change, MK, 01/03/94 for Multiplayer reasons.  If robots can't see you (even with eyes on back of head), then don't do evasion.
					if (robptr->attack_type == 1) {
						aip->behavior = AIB_NORMAL;
						if (!ai_multiplayer_awareness(obj, 80)) {
							if (maybe_ai_do_actual_firing_stuff(obj, aip))
								fire_at_player(obj, &vec_to_player, dist_to_player, &gun_point, player_visibility, object_animates);
							return;
						}
						move_relative(obj, dist_to_player, &vec_to_player, 0, 0);
						if (ai_evaded) {
							ai_multi_send_robot_position(objnum, 1);
							ai_evaded = 0;
						}
						else
							ai_multi_send_robot_position(objnum, -1);
					} else {
						// Robots in hover mode are allowed to evade at half normal speed.
						if (!ai_multiplayer_awareness(obj, 81)) {
							if (maybe_ai_do_actual_firing_stuff(obj, aip))
								fire_at_player(obj, &vec_to_player, dist_to_player, &gun_point, player_visibility, object_animates);
							return;
						}
						move_relative(obj, dist_to_player, &vec_to_player, 0, 1);
						if (ai_evaded) {
							ai_multi_send_robot_position(objnum, -1);
							ai_evaded = 0;
						}
						else
							ai_multi_send_robot_position(objnum, -1);
					}
				} else if ((obj->segnum != aip->hide_segment) && (dist_to_player > F1_0*80) && (!(Game_mode & GM_MULTI))) {
					//	If pretty far from the player, player cannot be seen (obstructed) and in chase mode, switch to follow path mode.
					// This has one desirable benefit of avoiding physics retries.
					if (aip->behavior == AIB_STATION) {
						ailp->goal_segment = aip->hide_segment;
						d1_in_d2_ai_create_path_to_station(obj, 15);
						// -- show_path_and_other(obj);
					}
					break;
				}
			}

			break;
		case AIM_OPEN_DOOR: {       // trying to open a door.
			vms_vector center_point, goal_vector;
			Assert(obj->id == ROBOT_BRAIN);     // Make sure this guy is allowed to be in this mode.

			if (!ai_multiplayer_awareness(obj, 62))
				return;
			compute_center_point_on_side(&center_point, &Segments[obj->segnum], aip->GOALSIDE);
			vm_vec_sub(&goal_vector, &center_point, &obj->pos);
			vm_vec_normalize_quick(&goal_vector);
			d1_in_d2_ai_turn_towards_vector(&goal_vector, obj, robptr->turn_time[Difficulty_level]);
			move_towards_vector(obj, &goal_vector, 0);
			ai_multi_send_robot_position(objnum, -1);

			break;
		}

		default:
			ailp->mode = AIM_CHASE_OBJECT;
			break;
	}       // end: switch (ailp->mode) {

	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	// If the robot can see you, increase his awareness of you.
	// This prevents the problem of a robot looking right at you but doing nothing.
	// Assert(player_visibility != -1); // Means it didn't get initialized!
	compute_visibility(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);
	if (player_visibility == 2)
		if (ailp->player_awareness_type == 0)
			ailp->player_awareness_type = PA_PLAYER_COLLISION;

	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	if (!object_animates) {
		aip->CURRENT_STATE = aip->GOAL_STATE;
	}

	Assert(ailp->player_awareness_type <= AIE_MAX);
	Assert(aip->CURRENT_STATE < AIS_MAX);
	Assert(aip->GOAL_STATE < AIS_MAX);

	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	if (ailp->player_awareness_type) {
		new_goal_state = ai_transition_goal(ailp->player_awareness_type, aip->CURRENT_STATE, aip->GOAL_STATE);
		if (ailp->player_awareness_type == PA_WEAPON_ROBOT_COLLISION) {
			// Decrease awareness, else this robot will flinch every frame.
			ailp->player_awareness_type--;
			ailp->player_awareness_time = F1_0*3;
		}

		if (new_goal_state == AIS_ERR_)
			new_goal_state = AIS_REST;

		if (aip->CURRENT_STATE == AIS_NONE)
			aip->CURRENT_STATE = AIS_REST;

		aip->GOAL_STATE = new_goal_state;

	}

	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	// If new state = fire, then set all gun states to fire.
	if (aip->GOAL_STATE == AIS_FIRE) {
		int i,num_guns;
		num_guns = Robot_info[obj->id].n_guns;
		for (i=0; i<num_guns; i++)
			ailp->goal_state[i] = AIS_FIRE;
	}

	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	// Hack by mk on 01/04/94, if a guy hasn't animated to the firing state, but his next_fire says ok to fire, bash him there
	if ((ailp->next_fire < 0) && (aip->GOAL_STATE == AIS_FIRE))
		aip->CURRENT_STATE = AIS_FIRE;

	if ((aip->GOAL_STATE != AIS_FLIN)  && (obj->id != ROBOT_BRAIN)) {
		switch (aip->CURRENT_STATE) {
			case	AIS_NONE:
				compute_visibility(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);

				dot = vm_vec_dot(&obj->orient.fvec, &vec_to_player);
				if (dot >= F1_0/2)
					if (aip->GOAL_STATE == AIS_REST)
						aip->GOAL_STATE = AIS_SRCH;
				break;
			case	AIS_REST:
				if (aip->GOAL_STATE == AIS_REST) {
					compute_visibility(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);
					if ((ailp->next_fire <= 0) && (player_visibility)) {
						aip->GOAL_STATE = AIS_FIRE;
					}
				}
				break;
			case	AIS_SRCH:
				if (!ai_multiplayer_awareness(obj, 60))
					return;

				compute_visibility(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);

				if (player_visibility) {
					d1_in_d2_ai_turn_towards_vector(&vec_to_player, obj, robptr->turn_time[Difficulty_level]);
					ai_multi_send_robot_position(objnum, -1);
				} else if (!(Game_mode & GM_MULTI))
					turn_randomly(&vec_to_player, obj, robptr->turn_time[Difficulty_level], previous_visibility);
				break;
			case	AIS_LOCK:
				compute_visibility(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);

				if (!(Game_mode & GM_MULTI) || (player_visibility)) {
					if (!ai_multiplayer_awareness(obj, 68))
						return;

					if (player_visibility) {
						d1_in_d2_ai_turn_towards_vector(&vec_to_player, obj, robptr->turn_time[Difficulty_level]);
						ai_multi_send_robot_position(objnum, -1);
					} else if (!(Game_mode & GM_MULTI))
						turn_randomly(&vec_to_player, obj, robptr->turn_time[Difficulty_level], previous_visibility);
				}
				break;
			case	AIS_FIRE:
				compute_visibility(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);

				if (player_visibility) {
					if (!ai_multiplayer_awareness(obj, (ROBOT_FIRE_AGITATION-1)))
					{
						if (Game_mode & GM_MULTI) {
							fire_at_player(obj, &vec_to_player, dist_to_player, &gun_point, player_visibility, object_animates);
							return;
						}
					}
					d1_in_d2_ai_turn_towards_vector(&vec_to_player, obj, robptr->turn_time[Difficulty_level]);
					ai_multi_send_robot_position(objnum, -1);
				} else if (!(Game_mode & GM_MULTI)) {
					turn_randomly(&vec_to_player, obj, robptr->turn_time[Difficulty_level], previous_visibility);
				}

				//	Fire at player, if appropriate.
				fire_at_player(obj, &vec_to_player, dist_to_player, &gun_point, player_visibility, object_animates);

				break;
			case	AIS_RECO:
				if (!(obj_ref & 3)) {
					compute_visibility(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);
					if (player_visibility) {
						if (!ai_multiplayer_awareness(obj, 69))
							return;
						d1_in_d2_ai_turn_towards_vector(&vec_to_player, obj, robptr->turn_time[Difficulty_level]);
						ai_multi_send_robot_position(objnum, -1);
					} else if (!(Game_mode & GM_MULTI)) {
						turn_randomly(&vec_to_player, obj, robptr->turn_time[Difficulty_level], previous_visibility);
					}
				}
				break;
			case	AIS_FLIN:
				break;
			default:
				aip->GOAL_STATE = AIS_REST;
				aip->CURRENT_STATE = AIS_REST;
				break;
		}
	} // end of: if (aip->GOAL_STATE != AIS_FLIN) {

	// Switch to next gun for next fire.
	if (player_visibility == 0) {
		aip->CURRENT_GUN++;
		if (aip->CURRENT_GUN >= Robot_info[obj->id].n_guns)
			aip->CURRENT_GUN = 0;
	}

}

int d1_in_d2_ai_run_frame(object *obj)
{
	if (d1_in_d2_ai_actor_role(obj) != D1_AI_NATIVE_ENEMY)
		return 0;
	native_frame(obj);
	return 1;
}
