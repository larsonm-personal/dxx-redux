#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "3d.h"
#include "args.h"
#include "ai.h"
#include "cntrlcen.h"
#include "console.h"
#include "controls.h"
#include "endlevel.h"
#include "fvi.h"
#include "game.h"
#include "gameseq.h"
#include "input_demo_control_info.h"
#include "input_demo_debug_logging.h"
#include "input_demo_fp_env.h"
#include "input_demo_hooks.h"
#include "input_demo_recorder.h"
#include "input_demo_replay.h"
#include "input_demo_result.h"
#include "input_demo_state_trace.h"
#include "input_demo_rng_trace.h"
#include "timer.h"
#include "maths.h"
#include "mission.h"
#include "newdemo.h"
#include "object.h"
#include "player.h"
#include "playsave.h"
#include "laser.h"
#include "wall.h"

extern int Num_awareness_events;
extern int ReadControlsReplayFrame(void);
extern void GameProcessFrame(void);
extern int game_is_time_paused(void);

#define INPUT_DEMO_RESULT_KILLS_MODE_NONE   0
#define INPUT_DEMO_RESULT_KILLS_MODE_REPLAY 1
#define INPUT_DEMO_RESULT_KILLS_MODE_RECORD 2

static int input_demo_result_kills_mode = 0;
static int input_demo_result_kills_baseline = 0;
static int input_demo_result_kills_baseline_valid = 0;

static int input_demo_count_live_objects_of_type(int object_type)
{
	int count = 0;
	int i;

	for (i = 0; i <= Highest_object_index; ++i) {
		if (Objects[i].type != object_type)
			continue;
		if (Objects[i].flags & OF_SHOULD_BE_DEAD)
			continue;
		count++;
	}
	return count;
}

void input_demo_record_game_frame(void)
{
	input_demo_control_state state;
	input_demo_control_pulse pulse;
	input_demo_result frame_state;
	unsigned int rng_call_count;
	unsigned int rng_state;
	char error[256] = "";

	if (Newdemo_state != ND_STATE_RECORDING || !input_demo_recorder_is_active())
		return;
	if (!d_rand_get_state(&rng_state)) {
		con_printf(CON_NORMAL, "Input demo recording stopped: live recording requires an lcg_state RNG backend\n");
		input_demo_recorder_cancel();
		return;
	}
	rng_call_count = d_rand_get_call_count();
	input_demo_control_state_from_control_info(&state, &pulse, &Controls);
	input_demo_capture_current_result(&frame_state);
	if (!input_demo_recorder_capture_frame((int32_t)FrameTime, &state, &pulse, rng_state, 1, rng_call_count,
		&frame_state,
		error, sizeof(error))) {
		con_printf(CON_NORMAL, "Input demo recording stopped: %s\n", error);
		input_demo_recorder_cancel();
	}
}

void input_demo_update_rng_trace_context(void)
{
	if (!input_demo_rng_trace_is_active())
		return;
	if (Newdemo_state == ND_STATE_RECORDING && input_demo_recorder_is_active()) {
		input_demo_rng_trace_set_context((uint32_t)input_demo_recorder_frame_count(), GameTime64);
		return;
	}
	if (input_demo_replay_is_loaded() &&
		input_demo_replay_next_frame_index() < input_demo_replay_frame_count())
		input_demo_rng_trace_set_context(input_demo_replay_next_frame_index(), GameTime64);
}

void input_demo_capture_state_trace_diag(input_demo_state_trace_diag *diag)
{
	int i;

	if (!diag)
		return;
	memset(diag, 0, sizeof(*diag));
	diag->awareness_events = Num_awareness_events;
	diag->d_tick_count = d_tick_count;
	if (ConsoleObject) {
		diag->player_vel_x = ConsoleObject->mtype.phys_info.velocity.x;
		diag->player_vel_y = ConsoleObject->mtype.phys_info.velocity.y;
		diag->player_vel_z = ConsoleObject->mtype.phys_info.velocity.z;
		diag->player_last_x = ConsoleObject->last_pos.x;
		diag->player_last_y = ConsoleObject->last_pos.y;
		diag->player_last_z = ConsoleObject->last_pos.z;
	}
	for (i = 0; i <= Highest_object_index; ++i) {
		object *obj = &Objects[i];

		if (obj->type != OBJ_ROBOT)
			continue;
		if (obj->flags & OF_SHOULD_BE_DEAD)
			continue;
		if (obj->ctype.ai_info.SUB_FLAGS & SUB_FLAGS_CAMERA_AWAKE)
			diag->camera_awake_robots++;
		if (obj->ctype.ai_info.danger_laser_num != -1)
			diag->danger_laser_robots++;
	}
}

void input_demo_capture_current_result(input_demo_result *result)
{
	player *current_player = &Players[Player_num];
	int robots_killed = 0;
	int i;

	input_demo_result_clear(result);
	snprintf(result->game, sizeof(result->game), "%s", "d2");
	if (Current_mission && Current_mission_filename)
		snprintf(result->mission, sizeof(result->mission), "%s", Current_mission_filename);
	result->level = Current_level_num;
	result->difficulty = Difficulty_level;
	if (input_demo_replay_is_loaded())
		result->frame_count = (uint32_t)input_demo_replay_next_frame_index();
	else if (input_demo_recorder_is_active())
		result->frame_count = (uint32_t)input_demo_recorder_frame_count();
	else
		result->frame_count = 0;
	result->has_game_time64 = 1;
	result->game_time64 = GameTime64;

	result->player0.present = 1;
	result->player0.energy = f2i(current_player->energy);
	result->player0.shields = f2i(current_player->shields);
	result->player0.score = current_player->score;
	result->player0.lives = current_player->lives;
	result->player0.laser_level = current_player->laser_level;
	result->player0.primary_weapon = current_player->primary_weapon;
	result->player0.secondary_weapon = current_player->secondary_weapon;
	result->player0.flags = current_player->flags;
	result->player0.hostages = current_player->hostages_on_board;
	for (i = 0; i < INPUT_DEMO_RESULT_MAX_PRIMARY_AMMO; ++i)
		result->player0.primary_ammo[i] = i < MAX_PRIMARY_WEAPONS ? current_player->primary_ammo[i] : 0;
	for (i = 0; i < INPUT_DEMO_RESULT_MAX_SECONDARY_AMMO; ++i)
		result->player0.secondary_ammo[i] = i < MAX_SECONDARY_WEAPONS ? current_player->secondary_ammo[i] : 0;

	if (ConsoleObject) {
		result->position.present = 1;
		result->position.segment = ConsoleObject->segnum;
		result->position.x = ConsoleObject->pos.x;
		result->position.y = ConsoleObject->pos.y;
		result->position.z = ConsoleObject->pos.z;
		result->position.has_forward = 1;
		result->position.fx = ConsoleObject->orient.fvec.x;
		result->position.fy = ConsoleObject->orient.fvec.y;
		result->position.fz = ConsoleObject->orient.fvec.z;
	}

	result->level_summary.present = 1;
	result->level_summary.robots_alive = input_demo_count_live_objects_of_type(OBJ_ROBOT);
	if (input_demo_result_kills_baseline_valid)
		robots_killed = current_player->num_kills_level - input_demo_result_kills_baseline;
	if (robots_killed < 0)
		robots_killed = 0;
	result->level_summary.robots_killed = robots_killed;
	result->level_summary.hostages_remaining = input_demo_count_live_objects_of_type(OBJ_HOSTAGE);
	result->level_summary.powerups_remaining = input_demo_count_live_objects_of_type(OBJ_POWERUP);
	result->level_summary.control_center_destroyed = Control_center_destroyed ? 1 : 0;
	result->level_summary.endlevel_completed = Endlevel_sequence ? 1 : 0;
}

void input_demo_update_result_kills_baseline(void)
{
	int mode = INPUT_DEMO_RESULT_KILLS_MODE_NONE;

	if (input_demo_replay_is_loaded())
		mode = INPUT_DEMO_RESULT_KILLS_MODE_REPLAY;
	else if (Newdemo_state == ND_STATE_RECORDING && input_demo_recorder_is_active())
		mode = INPUT_DEMO_RESULT_KILLS_MODE_RECORD;

	if (mode != input_demo_result_kills_mode) {
		input_demo_result_kills_mode = mode;
		input_demo_result_kills_baseline_valid = 0;
	}

	if (mode == INPUT_DEMO_RESULT_KILLS_MODE_NONE)
		return;

	if (!input_demo_result_kills_baseline_valid) {
		input_demo_result_kills_baseline = Players[Player_num].num_kills_level;
		input_demo_result_kills_baseline_valid = 1;
	}
}

typedef struct input_demo_path_probe_state {
	int valid;
	int signature;
	unsigned int hash;
} input_demo_path_probe_state;

static input_demo_path_probe_state g_input_demo_last_path_detail[MAX_OBJECTS];
static input_demo_path_probe_state g_input_demo_last_path_points[MAX_OBJECTS];
static const char *input_demo_awareness_source_tag = "unset";
static int input_demo_awareness_source_objnum = -1;
static int input_demo_awareness_aux_objnum = -1;

void input_demo_set_awareness_source(const char *source_tag, int source_objnum, int aux_objnum)
{
	input_demo_awareness_source_tag = source_tag ? source_tag : "unset";
	input_demo_awareness_source_objnum = source_objnum;
	input_demo_awareness_aux_objnum = aux_objnum;
}

void input_demo_consume_awareness_source(const char **source_tag, int *source_objnum, int *aux_objnum)
{
	if (source_tag)
		*source_tag = input_demo_awareness_source_tag;
	if (source_objnum)
		*source_objnum = input_demo_awareness_source_objnum;
	if (aux_objnum)
		*aux_objnum = input_demo_awareness_aux_objnum;
	input_demo_awareness_source_tag = "unset";
	input_demo_awareness_source_objnum = -1;
	input_demo_awareness_aux_objnum = -1;
}

int input_demo_should_match_android_companion_velocity(void)
{
#ifdef __ANDROID__
	return 1;
#else
	return input_demo_recorder_is_active() || input_demo_replay_is_loaded();
#endif
}

unsigned int input_demo_trace_frame_index(void)
{
	return input_demo_debug_frame_index();
}

static int input_demo_trace_path_active(void)
{
	return input_demo_debug_activity_probe_active();
}

int input_demo_replay_path_probe_active(object *objp)
{
	return input_demo_trace_path_active() && (objp != NULL);
}

int input_demo_replay_follow_probe_active(object *objp)
{
	return input_demo_trace_path_active() &&
		objp &&
		(objp->type == OBJ_ROBOT) &&
		Robot_info[objp->id].companion;
}

int input_demo_replay_path_request_probe_active(object *objp)
{
	return input_demo_replay_path_probe_active(objp);
}

static unsigned int input_demo_path_probe_hash_add(unsigned int hash, int value)
{
	hash ^= (unsigned int)value;
	hash *= 16777619u;
	return hash;
}

static unsigned int input_demo_path_probe_hash_text(unsigned int hash, const char *text)
{
	int i;

	if (!text)
		return input_demo_path_probe_hash_add(hash, -1);

	for (i = 0; text[i]; i++) {
		hash ^= (unsigned char)text[i];
		hash *= 16777619u;
	}
	return input_demo_path_probe_hash_add(hash, 0);
}

void input_demo_log_path_robot_state(const char *label, object *objp)
{
	const int objnum = objp ? (int)(objp - Objects) : -1;
	ai_static *aip;
	ai_local *ailp;
	robot_info *robptr;

	if (!input_demo_replay_path_probe_active(objp) || (objnum < 0) || (objp->type != OBJ_ROBOT))
		return;

	aip = &objp->ctype.ai_info;
	ailp = &Ai_local_info[objnum];
	robptr = &Robot_info[objp->id];

	con_printf(CON_NORMAL,
		"Input demo replay path state: frame=%u step=%s obj=%d sig=%d id=%d companion=%d behavior=%d mode=%d cur_state=%d goal_state=%d seg=%d player_seg=%d believed_seg=%d goal_seg=%d prev_vis=%d aware=%d aware_time=%d retry=%d retry_chain=%d rapid=%d seen=%lld since=%d next_action=%d next_fire=%d next_fire2=%d path=%d/%d hide=%d dir=%d pos=(%d,%d,%d) vel=(%d,%d,%d)\n",
		input_demo_trace_frame_index(),
		label,
		objnum,
		objp->signature,
		objp->id,
		robptr->companion,
		aip->behavior,
		ailp->mode,
		aip->CURRENT_STATE,
		aip->GOAL_STATE,
		objp->segnum,
		ConsoleObject ? ConsoleObject->segnum : -1,
		Believed_player_seg,
		ailp->goal_segment,
		ailp->previous_visibility,
		ailp->player_awareness_type,
		ailp->player_awareness_time,
		ailp->retry_count,
		ailp->consecutive_retries,
		ailp->rapidfire_count,
		(long long)ailp->time_player_seen,
		ailp->time_since_processed,
		ailp->next_action_time,
		ailp->next_fire,
		ailp->next_fire2,
		aip->cur_path_index,
		aip->path_length,
		aip->hide_index,
		aip->PATH_DIR,
		objp->pos.x,
		objp->pos.y,
		objp->pos.z,
		objp->mtype.phys_info.velocity.x,
		objp->mtype.phys_info.velocity.y,
		objp->mtype.phys_info.velocity.z);
}

void input_demo_log_path_request(const char *label,
	object *objp,
	int start_seg,
	int end_seg,
	int max_length,
	int safety_flag,
	int avoid_seg)
{
	const int objnum = objp ? (int)(objp - Objects) : -1;
	ai_static *aip = (objp && objnum >= 0) ? &objp->ctype.ai_info : NULL;
	ai_local *ailp = (objp && objnum >= 0) ? &Ai_local_info[objnum] : NULL;

	con_printf(CON_NORMAL,
		"Input demo replay path request: frame=%u step=%s obj=%d type=%d control=%d id=%d companion=%d behavior=%d mode=%d seg=%d player_seg=%d believed_seg=%d start=%d end=%d same=%d goal_seg=%d max=%d safety=%d avoid=%d cur_path=%d/%d hide=%d dir=%d\n",
		input_demo_trace_frame_index(),
		label,
		objnum,
		objp ? objp->type : -1,
		objp ? objp->control_type : -1,
		objp ? objp->id : -1,
		(objp && (objp->type == OBJ_ROBOT)) ? Robot_info[objp->id].companion : 0,
		aip ? aip->behavior : -1,
		ailp ? ailp->mode : -1,
		objp ? objp->segnum : -1,
		ConsoleObject ? ConsoleObject->segnum : -1,
		Believed_player_seg,
		start_seg,
		end_seg,
		start_seg == end_seg,
		ailp ? ailp->goal_segment : -1,
		max_length,
		safety_flag,
		avoid_seg,
		aip ? aip->cur_path_index : -1,
		aip ? aip->path_length : -1,
		aip ? aip->hide_index : -1,
		aip ? aip->PATH_DIR : 0);
	input_demo_log_path_robot_state(label, objp);
}

void input_demo_log_path_probe(object *objp,
	int start_seg,
	int end_seg,
	int max_depth,
	int random_flag,
	int safety_flag,
	int avoid_seg,
	int result,
	unsigned int rng_before,
	unsigned int rng_call_count_before)
{
	unsigned int rng_after = 0;
	unsigned int rng_call_count_after = d_rand_get_call_count();
	const int objnum = objp ? (int)(objp - Objects) : -1;
	ai_local *ailp = (objp && objnum >= 0) ? &Ai_local_info[objnum] : NULL;

	d_rand_get_state(&rng_after);
	con_printf(CON_NORMAL,
		"Input demo replay path probe: frame=%u obj=%d type=%d control=%d id=%d companion=%d behavior=%d mode=%d seg=%d player_seg=%d believed_seg=%d start=%d end=%d same=%d max=%d random=%d safety=%d avoid=%d result=%d calls=%u->%u before=%u after=%u\n",
		input_demo_trace_frame_index(),
		objnum,
		objp ? objp->type : -1,
		objp ? objp->control_type : -1,
		objp ? objp->id : -1,
		objp ? Robot_info[objp->id].companion : 0,
		objp ? objp->ctype.ai_info.behavior : -1,
		ailp ? ailp->mode : -1,
		objp ? objp->segnum : -1,
		ConsoleObject ? ConsoleObject->segnum : -1,
		Believed_player_seg,
		start_seg,
		end_seg,
		start_seg == end_seg,
		max_depth,
		random_flag,
		safety_flag,
		avoid_seg,
		result,
		rng_call_count_before,
		rng_call_count_after,
		rng_before,
		rng_after);
	input_demo_log_path_robot_state("create_path_points result", objp);
}

void input_demo_log_path_detail(object *objp,
	int start_seg,
	int end_seg,
	int random_flag,
	int random_xlate_seed_count,
	int random_xlate_refresh_roll_count,
	int random_xlate_refresh_count,
	int queue_push_count,
	int raw_num_points,
	int final_num_points)
{
	const int objnum = objp ? (int)(objp - Objects) : -1;
	ai_static *aip = (objp && objnum >= 0) ? &objp->ctype.ai_info : NULL;
	ai_local *ailp = (objp && objnum >= 0) ? &Ai_local_info[objnum] : NULL;
	input_demo_path_probe_state *last_detail;
	unsigned int detail_hash = 2166136261u;
	int companion;

	if (!input_demo_replay_path_probe_active(objp) || (objnum < 0))
		return;

	companion = (objp->type == OBJ_ROBOT) ? Robot_info[objp->id].companion : 0;
	detail_hash = input_demo_path_probe_hash_add(detail_hash, objp->type);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, objp->control_type);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, objp->id);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, companion);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, aip ? aip->behavior : -1);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, ailp ? ailp->mode : -1);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, objp->segnum);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, ConsoleObject ? ConsoleObject->segnum : -1);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, Believed_player_seg);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, start_seg);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, end_seg);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, random_flag);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, random_xlate_seed_count);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, random_xlate_refresh_roll_count);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, random_xlate_refresh_count);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, queue_push_count);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, raw_num_points);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, final_num_points);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, ailp ? ailp->goal_segment : -1);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, aip ? aip->cur_path_index : -1);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, aip ? aip->path_length : -1);
	detail_hash = input_demo_path_probe_hash_add(detail_hash, aip ? aip->hide_index : -1);
	last_detail = &g_input_demo_last_path_detail[objnum];
	if (last_detail->valid && (last_detail->signature == objp->signature) && (last_detail->hash == detail_hash))
		return;
	last_detail->valid = 1;
	last_detail->signature = objp->signature;
	last_detail->hash = detail_hash;

	con_printf(CON_NORMAL,
		"Input demo replay path detail: frame=%u obj=%d type=%d control=%d id=%d companion=%d behavior=%d mode=%d seg=%d player_seg=%d believed_seg=%d start=%d end=%d same=%d random=%d seed_xlate=%d refresh_rolls=%d refresh_xlate=%d queue_pushes=%d raw_points=%d final_points=%d goal_seg=%d cur_path=%d/%d hide=%d\n",
		input_demo_trace_frame_index(),
		objnum,
		objp ? objp->type : -1,
		objp ? objp->control_type : -1,
		objp ? objp->id : -1,
		(objp && (objp->type == OBJ_ROBOT)) ? Robot_info[objp->id].companion : 0,
		aip ? aip->behavior : -1,
		ailp ? ailp->mode : -1,
		objp ? objp->segnum : -1,
		ConsoleObject ? ConsoleObject->segnum : -1,
		Believed_player_seg,
		start_seg,
		end_seg,
		start_seg == end_seg,
		random_flag,
		random_xlate_seed_count,
		random_xlate_refresh_roll_count,
		random_xlate_refresh_count,
		queue_push_count,
		raw_num_points,
		final_num_points,
		ailp ? ailp->goal_segment : -1,
		aip ? aip->cur_path_index : -1,
		aip ? aip->path_length : -1,
		aip ? aip->hide_index : -1);
	input_demo_log_path_robot_state("create_path_points detail", objp);
}

void input_demo_log_path_points(const char *label, object *objp, point_seg *psegs, int num_points)
{
	const int objnum = objp ? (int)(objp - Objects) : -1;
	char segs[512];
	int i;
	int limit;
	int offset;
	int written;
	input_demo_path_probe_state *last_points;
	unsigned int points_hash = 2166136261u;

	if (!input_demo_replay_path_probe_active(objp) || (objnum < 0) || !psegs || num_points <= 0)
		return;

	points_hash = input_demo_path_probe_hash_text(points_hash, label);
	points_hash = input_demo_path_probe_hash_add(points_hash, num_points);
	for (i = 0; i < num_points; i++)
		points_hash = input_demo_path_probe_hash_add(points_hash, psegs[i].segnum);
	last_points = &g_input_demo_last_path_points[objnum];
	if (last_points->valid && (last_points->signature == objp->signature) && (last_points->hash == points_hash))
		return;
	last_points->valid = 1;
	last_points->signature = objp->signature;
	last_points->hash = points_hash;

	limit = num_points < 24 ? num_points : 24;
	offset = 0;
	segs[0] = 0;
	for (i=0; i<limit; i++) {
		written = snprintf(segs + offset, sizeof(segs) - offset, "%s%d", i ? "," : "", psegs[i].segnum);
		if (written < 0)
			break;
		if (written >= (int)(sizeof(segs) - offset)) {
			offset = sizeof(segs) - 1;
			break;
		}
		offset += written;
	}
	if ((limit < num_points) && (offset < (int)sizeof(segs)))
		snprintf(segs + offset, sizeof(segs) - offset, ",...");

	con_printf(CON_NORMAL,
		"Input demo replay path points: frame=%u step=%s obj=%d points=%d listed=%d segs=%s\n",
		input_demo_trace_frame_index(),
		label,
		objnum,
		num_points,
		limit,
		segs);
}

int input_demo_trace_ai_rng_active(object *obj)
{
	ai_static *aip;
	ai_local *ailp;

	if (!obj || !input_demo_debug_replay_probe_active() || (obj->type != OBJ_ROBOT))
		return 0;

	aip = &obj->ctype.ai_info;
	ailp = &Ai_local_info[obj-Objects];

	return Robot_info[obj->id].companion ||
		(obj->segnum == ConsoleObject->segnum) ||
		(obj->segnum == Believed_player_seg) ||
		(ailp->goal_segment == ConsoleObject->segnum) ||
		(ailp->goal_segment == Believed_player_seg) ||
		(ailp->mode >= AIM_GOTO_PLAYER) ||
		(aip->behavior == AIB_SNIPE) ||
		(ailp->player_awareness_type > 0);
}

int input_demo_robot_lifecycle_probe_active(void)
{
	return input_demo_debug_activity_frame_in_range(500u, 2200u);
}

int input_demo_robot_visual_probe_active(void)
{
	return input_demo_debug_replay_frame_in_range(1360u, 1450u) ||
		input_demo_debug_replay_frame_in_range(1880u, 1950u);
}

int input_demo_robot_lifecycle_is_target(int objnum, object *obj)
{
	if (!input_demo_robot_lifecycle_probe_active() || !obj || (obj->type != OBJ_ROBOT))
		return 0;

	if ((objnum >= 73) && (objnum <= 75))
		return 1;
	if ((obj->signature >= 3784) && (obj->signature <= 3786))
		return 1;
	if (obj->flags & (OF_EXPLODING | OF_SHOULD_BE_DEAD))
		return 1;

	return 0;
}

static void input_demo_log_robot_visual_state(object *obj, const g3s_lrgb *light, int tmap_override, int override_bm_index, int override_bm_flags, ubyte probe_codes, int probe_behind, int probe_projected, const g3s_point *probe_point)
{
	const int objnum = obj ? (int)(obj - Objects) : -1;
	const int probe_p3_codes = probe_point ? probe_point->p3_codes : 0;
	const int probe_p3_flags = probe_point ? probe_point->p3_flags : 0;
	const int probe_sx = (probe_projected && probe_point) ? f2i(probe_point->p3_sx) : -1;
	const int probe_sy = (probe_projected && probe_point) ? f2i(probe_point->p3_sy) : -1;
	const int probe_z = probe_point ? probe_point->p3_z : 0;

	if (!obj || !light || (objnum < 0))
		return;

	if (tmap_override != -1) {
		con_printf(CON_NORMAL,
			"Input demo robot visual state: frame=%u obj=%d sig=%d id=%d seg=%d rtype=%d cloak=%d cloak_type=%d flags=0x%x light=(%d,%d,%d) alpha=%d path=tmap_override tmap_override=%d override_bm_index=%d override_bm_flags=0x%x probe_codes=0x%x probe_behind=%d probe_projected=%d probe_p3_codes=0x%x probe_p3_flags=0x%x probe_sxy=(%d,%d) probe_z=%d\n",
			(unsigned int)input_demo_replay_next_frame_index(),
			objnum,
			obj->signature,
			obj->id,
			obj->segnum,
			obj->render_type,
			obj->ctype.ai_info.CLOAKED,
			Robot_info[obj->id].cloak_type,
			obj->flags,
			light->r,
			light->g,
			light->b,
			PlayerCfg.AlphaEffects,
			tmap_override,
			override_bm_index,
			override_bm_flags,
			probe_codes,
			probe_behind,
			probe_projected,
			probe_p3_codes,
			probe_p3_flags,
			probe_sx,
			probe_sy,
			probe_z);
		return;
	}

	con_printf(CON_NORMAL,
		"Input demo robot visual state: frame=%u obj=%d sig=%d id=%d seg=%d rtype=%d cloak=%d cloak_type=%d flags=0x%x light=(%d,%d,%d) alpha=%d path=default probe_codes=0x%x probe_behind=%d probe_projected=%d probe_p3_codes=0x%x probe_p3_flags=0x%x probe_sxy=(%d,%d) probe_z=%d\n",
		(unsigned int)input_demo_replay_next_frame_index(),
		objnum,
		obj->signature,
		obj->id,
		obj->segnum,
		obj->render_type,
		obj->ctype.ai_info.CLOAKED,
		Robot_info[obj->id].cloak_type,
		obj->flags,
		light->r,
		light->g,
		light->b,
		PlayerCfg.AlphaEffects,
		probe_codes,
		probe_behind,
		probe_projected,
		probe_p3_codes,
		probe_p3_flags,
		probe_sx,
		probe_sy,
		probe_z);
}

void input_demo_log_robot_visual_id38(object *obj, ubyte probe_codes, int probe_behind, int probe_projected, const g3s_point *probe_point)
{
	const int objnum = obj ? (int)(obj - Objects) : -1;
	const int probe_p3_codes = probe_point ? probe_point->p3_codes : 0;
	const int probe_p3_flags = probe_point ? probe_point->p3_flags : 0;
	const int probe_sx = (probe_projected && probe_point) ? f2i(probe_point->p3_sx) : -1;
	const int probe_sy = (probe_projected && probe_point) ? f2i(probe_point->p3_sy) : -1;
	const int probe_z = probe_point ? probe_point->p3_z : 0;

	if (!obj || (objnum < 0))
		return;

	con_printf(CON_NORMAL,
		"Input demo robot visual id38: frame=%u obj=%d sig=%d seg=%d flags=0x%x codes=0x%x behind=%d projected=%d p3_codes=0x%x p3_flags=0x%x sxy=(%d,%d) z=%d\n",
		(unsigned int)input_demo_replay_next_frame_index(),
		objnum,
		obj->signature,
		obj->segnum,
		obj->flags,
		probe_codes,
		probe_behind,
		probe_projected,
		probe_p3_codes,
		probe_p3_flags,
		probe_sx,
		probe_sy,
		probe_z);
}

void input_demo_log_robot_visual_state_default(object *obj, const g3s_lrgb *light, ubyte probe_codes, int probe_behind, int probe_projected, const g3s_point *probe_point)
{
	input_demo_log_robot_visual_state(obj, light, -1, -1, 0, probe_codes, probe_behind, probe_projected, probe_point);
}

void input_demo_log_robot_visual_state_tmap_override(object *obj, const g3s_lrgb *light, int override_bm_index, int override_bm_flags, ubyte probe_codes, int probe_behind, int probe_projected, const g3s_point *probe_point)
{
	input_demo_log_robot_visual_state(obj, light, obj ? obj->rtype.pobj_info.tmap_override : -1, override_bm_index, override_bm_flags, probe_codes, probe_behind, probe_projected, probe_point);
}

void input_demo_log_robot_visual_player_cloak(object *obj)
{
	const int objnum = obj ? (int)(obj - Objects) : -1;

	if (!obj || (objnum < 0))
		return;

	con_printf(CON_NORMAL,
		"Input demo robot visual state: frame=%u obj=%d sig=%d path=player_cloak\n",
		(unsigned int)input_demo_replay_next_frame_index(),
		objnum,
		obj->signature);
}

void input_demo_log_robot_visual_robot_cloak(object *obj)
{
	const int objnum = obj ? (int)(obj - Objects) : -1;

	if (!obj || (objnum < 0))
		return;

	con_printf(CON_NORMAL,
		"Input demo robot visual state: frame=%u obj=%d sig=%d path=robot_cloak boss=%d\n",
		(unsigned int)input_demo_replay_next_frame_index(),
		objnum,
		obj->signature,
		Robot_info[obj->id].boss_flag != 0);
}

void input_demo_log_robot_poly_probe(object *obj, int faces_considered, int faces_drawn, int tmap_override)
{
	const int objnum = obj ? (int)(obj - Objects) : -1;

	if (!obj || (objnum < 0))
		return;

	if (tmap_override != -1) {
		con_printf(CON_NORMAL,
			"Input demo robot poly probe: frame=%u obj=%d sig=%d model_num=%d faces_considered=%d faces_drawn=%d path=tmap_override tmap_override=%d\n",
			(unsigned int)input_demo_replay_next_frame_index(),
			objnum,
			obj->signature,
			obj->rtype.pobj_info.model_num,
			faces_considered,
			faces_drawn,
			tmap_override);
		return;
	}

	con_printf(CON_NORMAL,
		"Input demo robot poly probe: frame=%u obj=%d sig=%d model_num=%d faces_considered=%d faces_drawn=%d\n",
		(unsigned int)input_demo_replay_next_frame_index(),
		objnum,
		obj->signature,
		obj->rtype.pobj_info.model_num,
		faces_considered,
		faces_drawn);
}

static int input_demo_trace_robot_pose_active(void)
{
	return input_demo_debug_activity_probe_active();
}

static const char *input_demo_trace_robot_pose_mode_name(void)
{
	return input_demo_debug_activity_mode_name();
}

typedef struct input_demo_trace_view_probe {
	int hit_type;
	int hit_seg;
	int hit_side;
	int hit_side_seg;
	int hit_object;
	int doorway_flags;
	int wall_num;
	int wall_type;
	int wall_state;
	int wall_flags;
	int wall_keys;
} input_demo_trace_view_probe;

static void input_demo_trace_robot_view_probe(object *robot, input_demo_trace_view_probe *probe)
{
	fvi_query fq;
	fvi_info hit;

	if (!probe)
		return;

	probe->hit_type = -1;
	probe->hit_seg = -1;
	probe->hit_side = -1;
	probe->hit_side_seg = -1;
	probe->hit_object = -1;
	probe->doorway_flags = -1;
	probe->wall_num = -1;
	probe->wall_type = -1;
	probe->wall_state = -1;
	probe->wall_flags = -1;
	probe->wall_keys = -1;

	if (!robot || !ConsoleObject || robot->type != OBJ_ROBOT)
		return;

	fq.p0 = &ConsoleObject->pos;
	fq.startseg = ConsoleObject->segnum;
	fq.p1 = &robot->pos;
	fq.rad = 0;
	fq.thisobjnum = (short)(ConsoleObject - Objects);
	fq.ignore_obj_list = NULL;
	fq.flags = FQ_CHECK_OBJS | FQ_TRANSWALL;

	find_vector_intersection(&fq, &hit);
	probe->hit_type = hit.hit_type;
	probe->hit_seg = hit.hit_seg;
	probe->hit_side = hit.hit_side;
	probe->hit_side_seg = hit.hit_side_seg;
	probe->hit_object = hit.hit_object;

	if ((hit.hit_type == HIT_WALL) && (hit.hit_side_seg >= 0) && (hit.hit_side >= 0)) {
		segment *hit_seg = &Segments[hit.hit_side_seg];
		int wall_num = hit_seg->sides[hit.hit_side].wall_num;

		probe->doorway_flags = WALL_IS_DOORWAY(hit_seg, hit.hit_side);
		probe->wall_num = wall_num;

		if ((wall_num >= 0) && (wall_num < Num_walls)) {
			wall *wallp = &Walls[wall_num];

			probe->wall_type = wallp->type;
			probe->wall_state = wallp->state;
			probe->wall_flags = wallp->flags;
			probe->wall_keys = wallp->keys;
		}
	}
}

static int input_demo_trace_robot_is_in_view(object *robot, int *los_out, fix *front_dot_out)
{
	vms_vector to_robot;
	int los;
	fix front_dot;

	if (!robot || !ConsoleObject || robot->type != OBJ_ROBOT) {
		if (los_out)
			*los_out = 0;
		if (front_dot_out)
			*front_dot_out = 0;
		return 0;
	}

	vm_vec_sub(&to_robot, &robot->pos, &ConsoleObject->pos);
	los = object_to_object_visibility(ConsoleObject, robot, FQ_TRANSWALL) != 0;
	front_dot = vm_vec_dot(&ConsoleObject->orient.fvec, &to_robot);

	if (los_out)
		*los_out = los;
	if (front_dot_out)
		*front_dot_out = front_dot;

	return los && (front_dot > 0);
}

void input_demo_trace_tracked_robot_poses(void)
{
	static ubyte tracked[MAX_OBJECTS];
	static int tracked_sig[MAX_OBJECTS];
	static sbyte tracked_last_in_view[MAX_OBJECTS];
	static short tracked_last_seg[MAX_OBJECTS];
	static unsigned int last_frame = UINT_MAX;
	static int tracked_total = 0;
	static int last_summary_tracked_total = -1;
	static int last_summary_tracked_visible = -1;
	static int last_summary_tracked_list_count = -1;
	static char last_summary_list[512];
	static int last_score = -1;
	static int last_kills = -1;
	char tracked_list[512];
	int tracked_list_len = 0;
	int tracked_list_count = 0;
	int tracked_visible = 0;
	unsigned int frame;
	int score_now;
	int kills_now;
	int score_delta = 0;
	int kills_delta = 0;
	int i;

	if (!input_demo_trace_robot_pose_active()) {
		memset(tracked, 0, sizeof(tracked));
		memset(tracked_sig, 0, sizeof(tracked_sig));
		memset(tracked_last_in_view, 0xff, sizeof(tracked_last_in_view));
		memset(tracked_last_seg, 0xff, sizeof(tracked_last_seg));
		last_frame = UINT_MAX;
		tracked_total = 0;
		last_summary_tracked_total = -1;
		last_summary_tracked_visible = -1;
		last_summary_tracked_list_count = -1;
		last_summary_list[0] = '\0';
		last_score = -1;
		last_kills = -1;
		return;
	}

	frame = input_demo_trace_frame_index();
	if (frame < last_frame) {
		memset(tracked, 0, sizeof(tracked));
		memset(tracked_sig, 0, sizeof(tracked_sig));
		memset(tracked_last_in_view, 0xff, sizeof(tracked_last_in_view));
		memset(tracked_last_seg, 0xff, sizeof(tracked_last_seg));
		tracked_total = 0;
		last_summary_tracked_total = -1;
		last_summary_tracked_visible = -1;
		last_summary_tracked_list_count = -1;
		last_summary_list[0] = '\0';
		last_score = -1;
		last_kills = -1;
	}
	last_frame = frame;
	score_now = Players[Player_num].score;
	kills_now = Players[Player_num].num_kills_level;
	if (last_score >= 0)
		score_delta = score_now - last_score;
	if (last_kills >= 0)
		kills_delta = kills_now - last_kills;

	for (i = 0; i <= Highest_object_index; ++i) {
		object *objp = &Objects[i];
		int los;
		fix front_dot;

		if (objp->type != OBJ_ROBOT)
			continue;
		if (tracked[i])
			continue;
		if (!input_demo_trace_robot_is_in_view(objp, &los, &front_dot))
			continue;

		tracked[i] = 1;
		tracked_sig[i] = objp->signature;
		tracked_total++;
		con_printf(CON_NORMAL,
			"Input demo robot pose track: mode=%s frame=%u gt=%lld step=discover tracked_total=%d robot_obj=%d robot_sig=%d robot_id=%d robot_seg=%d los=%d front_dot=%d pos=(%d,%d,%d) vel=(%d,%d,%d) shields=%d life=%d flags=0x%x exploding=%d companion=%d boss=%d\n",
			input_demo_trace_robot_pose_mode_name(),
			frame,
			(long long)GameTime64,
			tracked_total,
			i,
			objp->signature,
			objp->id,
			objp->segnum,
			los,
			front_dot,
			objp->pos.x,
			objp->pos.y,
			objp->pos.z,
			objp->mtype.phys_info.velocity.x,
			objp->mtype.phys_info.velocity.y,
			objp->mtype.phys_info.velocity.z,
			objp->shields,
			objp->lifeleft,
			objp->flags,
			(objp->flags & OF_EXPLODING) != 0,
			Robot_info[objp->id].companion,
			Robot_info[objp->id].boss_flag);
	}

	for (i = 0; i <= Highest_object_index; ++i) {
		object *objp = &Objects[i];
		int los = 0;
		fix front_dot = 0;
		int in_view = 0;
		int sig_owner_obj = -1;
		int sig_owner_type = -1;
		int sig_owner_id = -1;
		int sig_owner_seg = -1;
		const char *missing_reason = "slot_cleared";
		const char *view_transition = "steady";
		int view_dropped;
		int view_restored;
		int seg_changed_hidden;
		int prev_in_view = tracked_last_in_view[i];
		int prev_seg = tracked_last_seg[i];
		int written;
		int j;

		if (!tracked[i])
			continue;

		if (tracked_list_len < (int)sizeof(tracked_list) - 1) {
			written = snprintf(tracked_list + tracked_list_len,
				sizeof(tracked_list) - tracked_list_len,
				"%s%d:%d",
				tracked_list_count ? "," : "",
				i,
				tracked_sig[i]);
			if (written > 0 && written < (int)(sizeof(tracked_list) - tracked_list_len))
				tracked_list_len += written;
		}
		tracked_list_count++;

		if ((objp->type == OBJ_ROBOT) && (objp->signature == tracked_sig[i]))
			in_view = input_demo_trace_robot_is_in_view(objp, &los, &front_dot);
		if (in_view)
			tracked_visible++;

		if ((objp->type == OBJ_ROBOT) && (objp->signature == tracked_sig[i])) {
			input_demo_trace_view_probe view_probe;

			view_dropped = (prev_in_view == 1) && !in_view;
			view_restored = (prev_in_view == 0) && in_view;
			seg_changed_hidden = (prev_seg != -1) && (prev_seg != objp->segnum) && !in_view;
			if (view_dropped)
				view_transition = "drop";
			else if (view_restored)
				view_transition = "restore";
			else if (seg_changed_hidden)
				view_transition = "seg_change_hidden";

			if (view_dropped || view_restored || seg_changed_hidden) {
				input_demo_trace_robot_view_probe(objp, &view_probe);
				con_printf(CON_NORMAL,
					"Input demo robot pose track: mode=%s frame=%u gt=%lld step=view_gate tracked_total=%d robot_obj=%d robot_sig=%d robot_id=%d transition=%s prev_in_view=%d in_view=%d prev_seg=%d robot_seg=%d los=%d front_dot=%d hit_type=%d hit_seg=%d hit_side=%d hit_side_seg=%d hit_obj=%d doorway=0x%x wall_num=%d wall_type=%d wall_state=%d wall_flags=0x%x wall_keys=0x%x\n",
					input_demo_trace_robot_pose_mode_name(),
					frame,
					(long long)GameTime64,
					tracked_total,
					i,
					objp->signature,
					objp->id,
					view_transition,
					prev_in_view,
					in_view,
					prev_seg,
					objp->segnum,
					los,
					front_dot,
					view_probe.hit_type,
					view_probe.hit_seg,
					view_probe.hit_side,
					view_probe.hit_side_seg,
					view_probe.hit_object,
					view_probe.doorway_flags,
					view_probe.wall_num,
					view_probe.wall_type,
					view_probe.wall_state,
					view_probe.wall_flags,
					view_probe.wall_keys);
			}

			tracked_last_in_view[i] = in_view ? 1 : 0;
			tracked_last_seg[i] = (short)objp->segnum;
		} else {
			tracked_last_in_view[i] = -1;
			tracked_last_seg[i] = -1;

			for (j = 0; j <= Highest_object_index; ++j) {
				object *owner = &Objects[j];

				if ((owner->type == OBJ_NONE) || (owner->signature != tracked_sig[i]))
					continue;

				sig_owner_obj = j;
				sig_owner_type = owner->type;
				sig_owner_id = owner->id;
				sig_owner_seg = owner->segnum;
				missing_reason = "signature_elsewhere";
				break;
			}
			con_printf(CON_NORMAL,
				"Input demo robot pose track: mode=%s frame=%u gt=%lld step=missing tracked_total=%d robot_obj=%d tracked_sig=%d slot_type=%d slot_sig=%d slot_seg=%d reason=%s sig_owner_obj=%d sig_owner_type=%d sig_owner_id=%d sig_owner_seg=%d score=%d score_delta=%d kills=%d kills_delta=%d\n",
				input_demo_trace_robot_pose_mode_name(),
				frame,
				(long long)GameTime64,
				tracked_total,
				i,
				tracked_sig[i],
				objp->type,
				objp->signature,
				objp->segnum,
				missing_reason,
				sig_owner_obj,
				sig_owner_type,
				sig_owner_id,
				sig_owner_seg,
				score_now,
				score_delta,
				kills_now,
				kills_delta);
		}
	}

	tracked_list[tracked_list_len] = '\0';
	if ((tracked_total != last_summary_tracked_total) ||
	    (tracked_visible != last_summary_tracked_visible) ||
	    (tracked_list_count != last_summary_tracked_list_count) ||
	    strcmp(tracked_list_len ? tracked_list : "none", last_summary_list) != 0) {
		con_printf(CON_NORMAL,
			"Input demo robot pose track: mode=%s frame=%u gt=%lld step=summary tracked_total=%d tracked_visible=%d listed=%d list=%s\n",
			input_demo_trace_robot_pose_mode_name(),
			frame,
			(long long)GameTime64,
			tracked_total,
			tracked_visible,
			tracked_list_count,
			tracked_list_len ? tracked_list : "none");
		last_summary_tracked_total = tracked_total;
		last_summary_tracked_visible = tracked_visible;
		last_summary_tracked_list_count = tracked_list_count;
		snprintf(last_summary_list, sizeof(last_summary_list), "%s", tracked_list_len ? tracked_list : "none");
	}
	last_score = score_now;
	last_kills = kills_now;
}

unsigned int input_demo_trace_robot_fire_frame_index(void)
{
	if (input_demo_replay_is_loaded())
		return (unsigned int)input_demo_replay_next_frame_index();
	if (input_demo_recorder_is_active()) {
		const uint32_t frame_count = input_demo_recorder_frame_count();

		return frame_count ? (unsigned int)(frame_count - 1) : 0;
	}
	return 0;
}

int input_demo_trace_robot_fire_active(object *objp)
{
	return input_demo_debug_is_enabled() &&
		(input_demo_recorder_is_active() || input_demo_replay_is_loaded()) &&
		objp &&
		(objp->type == OBJ_ROBOT);
}

void input_demo_log_robot_fire_state(const char *label, object *objp)
{
	const int objnum = objp ? (int)(objp - Objects) : -1;
	ai_static *aip;
	ai_local *ailp;
	robot_info *robptr;

	if (!input_demo_trace_robot_fire_active(objp) || (objnum < 0))
		return;

	aip = &objp->ctype.ai_info;
	ailp = &Ai_local_info[objnum];
	robptr = &Robot_info[objp->id];

	con_printf(CON_NORMAL,
		"Input demo replay fire state: frame=%u step=%s obj=%d sig=%d id=%d companion=%d behavior=%d mode=%d cur_state=%d goal_state=%d gun=%d seg=%d player_seg=%d believed_seg=%d goal_seg=%d prev_vis=%d aware=%d aware_time=%d retry=%d retry_chain=%d rapid=%d seen=%lld since=%d next_action=%d next_fire=%d next_fire2=%d path=%d/%d hide=%d dir=%d pos=(%d,%d,%d) vel=(%d,%d,%d)\n",
		input_demo_trace_robot_fire_frame_index(),
		label,
		objnum,
		objp->signature,
		objp->id,
		robptr->companion,
		aip->behavior,
		ailp->mode,
		aip->CURRENT_STATE,
		aip->GOAL_STATE,
		aip->CURRENT_GUN,
		objp->segnum,
		ConsoleObject ? ConsoleObject->segnum : -1,
		Believed_player_seg,
		ailp->goal_segment,
		ailp->previous_visibility,
		ailp->player_awareness_type,
		ailp->player_awareness_time,
		ailp->retry_count,
		ailp->consecutive_retries,
		ailp->rapidfire_count,
		(long long)ailp->time_player_seen,
		ailp->time_since_processed,
		ailp->next_action_time,
		ailp->next_fire,
		ailp->next_fire2,
		aip->cur_path_index,
		aip->path_length,
		aip->hide_index,
		aip->PATH_DIR,
		objp->pos.x,
		objp->pos.y,
		objp->pos.z,
		objp->mtype.phys_info.velocity.x,
		objp->mtype.phys_info.velocity.y,
		objp->mtype.phys_info.velocity.z);
}

int input_demo_trace_ai_robot_active(object *objp, ai_static *aip, ai_local *ailp)
{
	if (!input_demo_debug_record_probe_active() || !objp || (objp->type != OBJ_ROBOT))
		return 0;

	return Robot_info[objp->id].companion ||
		(objp->segnum == ConsoleObject->segnum) ||
		(objp->segnum == Believed_player_seg) ||
		(ailp->goal_segment == ConsoleObject->segnum) ||
		(ailp->goal_segment == Believed_player_seg) ||
		(ailp->mode >= AIM_GOTO_PLAYER) ||
		(aip->behavior == AIB_SNIPE) ||
		(ailp->player_awareness_type > 0);
}

void input_demo_log_ai_robot_state(const char *label, object *objp)
{
	const int objnum = objp ? (int)(objp - Objects) : -1;
	ai_static *aip;
	ai_local *ailp;

	if (!objp || (objnum < 0))
		return;

	aip = &objp->ctype.ai_info;
	ailp = &Ai_local_info[objnum];

	con_printf(CON_NORMAL,
		"Input demo replay AI robot: frame=%u step=%s obj=%d sig=%d id=%d size=%d shields=%d life=%d companion=%d behavior=%d mode=%d cur_state=%d goal_state=%d gun=%d path_dir=%d goal_side=%d danger_obj=%d danger_sig=%d retry=%d retry_chain=%d rapid=%d seg=%d player_seg=%d believed_seg=%d goal_seg=%d prev_vis=%d aware=%d aware_time=%d seen=%lld since=%d next_action=%d next_fire=%d next_fire2=%d path=%d/%d hide=%d skip=%d pos=(%d,%d,%d) vel=(%d,%d,%d)\n",
		input_demo_trace_frame_index(),
		label,
		objnum,
		objp->signature,
		objp->id,
		objp->size,
		objp->shields,
		objp->lifeleft,
		Robot_info[objp->id].companion,
		aip->behavior,
		ailp->mode,
		aip->CURRENT_STATE,
		aip->GOAL_STATE,
		aip->CURRENT_GUN,
		aip->PATH_DIR,
		aip->GOALSIDE,
		aip->danger_laser_num,
		aip->danger_laser_signature,
		ailp->retry_count,
		ailp->consecutive_retries,
		ailp->rapidfire_count,
		objp->segnum,
		ConsoleObject ? ConsoleObject->segnum : -1,
		Believed_player_seg,
		ailp->goal_segment,
		ailp->previous_visibility,
		ailp->player_awareness_type,
		ailp->player_awareness_time,
		(long long)ailp->time_player_seen,
		ailp->time_since_processed,
		ailp->next_action_time,
		ailp->next_fire,
		ailp->next_fire2,
		aip->cur_path_index,
		aip->path_length,
		aip->hide_index,
		aip->SKIP_AI_COUNT,
		objp->pos.x,
		objp->pos.y,
		objp->pos.z,
		objp->mtype.phys_info.velocity.x,
		objp->mtype.phys_info.velocity.y,
		objp->mtype.phys_info.velocity.z);
}

static int input_demo_count_live_player_weapons(int weapon_id)
{
	int count = 0;
	int i;

	for (i = 0; i <= Highest_object_index; ++i) {
		object *obj = &Objects[i];

		if (obj->type != OBJ_WEAPON)
			continue;
		if (obj->flags & OF_SHOULD_BE_DEAD)
			continue;
		if (obj->ctype.laser_info.parent_type != OBJ_PLAYER)
			continue;
		if (obj->ctype.laser_info.parent_num != Players[Player_num].objnum)
			continue;
		if ((weapon_id != -1) && (obj->id != weapon_id))
			continue;
		count++;
	}
	return count;
}

static int input_demo_replay_tail_probe_frame_active(uint32_t frame)
{
	return frame >= 816 && frame <= 825;
}

static int input_demo_replay_fire_probe_active(void)
{
	if (!input_demo_replay_is_loaded())
		return 0;

	return Controls.fire_primary_state ||
		Controls.fire_primary_count ||
		Global_laser_firing_count ||
		(Player_fired_laser_this_frame != -1) ||
		(Fusion_charge > 0) ||
		(Auto_fire_fusion_cannon_time > 0) ||
		(input_demo_count_live_player_weapons(-1) > 0);
}

void input_demo_log_replay_energy_stage(const char *label)
{
	player *current_player = &Players[Player_num];
	uint32_t replay_frame_index;

	if (!input_demo_replay_is_loaded())
		return;
	replay_frame_index = (uint32_t)input_demo_replay_next_frame_index();
	if (!input_demo_replay_tail_probe_frame_active(replay_frame_index))
		return;

	con_printf(CON_NORMAL,
		"Input demo replay energy stage: frame=%u gt=%lld step=%s energy=%d energy_raw=%d shields=%d shields_raw=%d primary=%d flags=0x%x f1s=%u f1c=%u flare=%u transfer=%u headlight=%u ab=%d glfc=%d seg=%d pos=(%d,%d,%d)\n",
		(unsigned int)replay_frame_index,
		(long long)GameTime64,
		label,
		f2i(current_player->energy),
		current_player->energy,
		f2i(current_player->shields),
		current_player->shields,
		current_player->primary_weapon,
		current_player->flags,
		(unsigned int)Controls.fire_primary_state,
		(unsigned int)Controls.fire_primary_count,
		(unsigned int)Controls.fire_flare_count,
		(unsigned int)Controls.energy_to_shield_state,
		(unsigned int)Controls.headlight_count,
		current_player->afterburner_charge,
		Global_laser_firing_count,
		ConsoleObject ? ConsoleObject->segnum : -1,
		ConsoleObject ? ConsoleObject->pos.x : 0,
		ConsoleObject ? ConsoleObject->pos.y : 0,
		ConsoleObject ? ConsoleObject->pos.z : 0);
}

static int input_demo_replay_logged_state_mismatch = 0;
static int input_demo_replay_logged_state_trace_error = 0;
static fix64 input_demo_replay_last_timer_value = 0;
static uint32_t input_demo_replay_result_frame_count_override = 0;
static int input_demo_replay_result_has_game_time64_override = 0;
static int64_t input_demo_replay_result_game_time64_override = 0;
static int input_demo_replay_result_endlevel_completed_override = -1;
static int input_demo_replay_compare_terminal_exit_only = 0;

void input_demo_log_current_replay_frame_state_mismatch(void)
{
	input_demo_replay_frame replay_frame;
	char error[256] = "";

	if (!input_demo_replay_is_loaded())
		return;
	if (!input_demo_replay_get_current_frame(&replay_frame, error, sizeof(error)))
		return;
	input_demo_debug_write_replay_frame_state_trace((void *)&replay_frame);
	input_demo_debug_log_replay_frame_state_mismatch((void *)&replay_frame);
}

static void input_demo_write_replay_result(void)
{
	input_demo_result result;
	char error[256] = "";
	const char *result_path;

	if (!input_demo_replay_is_loaded())
		return;
	result_path = input_demo_replay_actual_result_path();
	if (!(result_path && result_path[0]))
		return;
	input_demo_capture_current_result(&result);
	if (input_demo_replay_result_frame_count_override)
		result.frame_count = input_demo_replay_result_frame_count_override;
	if (input_demo_replay_result_has_game_time64_override) {
		result.has_game_time64 = 1;
		result.game_time64 = input_demo_replay_result_game_time64_override;
	}
	if (input_demo_replay_result_endlevel_completed_override >= 0) {
		result.level_summary.present = 1;
		result.level_summary.endlevel_completed = input_demo_replay_result_endlevel_completed_override;
	}
	input_demo_replay_result_frame_count_override = 0;
	input_demo_replay_result_has_game_time64_override = 0;
	input_demo_replay_result_game_time64_override = 0;
	input_demo_replay_result_endlevel_completed_override = -1;
	if (!input_demo_result_write_json_file(result_path, &result, error, sizeof(error)))
		con_printf(CON_NORMAL, "Input demo replay result write failed: %s\n", error);
	else {
		con_printf(CON_NORMAL, "Input demo replay result written: %s\n", result_path);
		if (input_demo_replay_compare_terminal_exit_only) {
			input_demo_result expected;

			if (!input_demo_replay_get_expected_result(&expected, error, sizeof(error)))
				con_printf(CON_NORMAL, "Input demo replay result compare setup failed: %s\n", error);
			else {
				expected.player0.present = 0;
				expected.position.present = 0;
				expected.level_summary.endlevel_completed = result.level_summary.endlevel_completed;
				if (!input_demo_result_compare(&expected, &result, error, sizeof(error)))
					con_printf(CON_NORMAL, "Input demo replay result mismatch: %s\n", error);
				else
					con_printf(CON_NORMAL, "Input demo replay result matched embedded trailer terminal-exit subset\n");
			}
		} else if (!input_demo_replay_compare_result(&result, error, sizeof(error)))
			con_printf(CON_NORMAL, "Input demo replay result mismatch: %s\n", error);
		else
			con_printf(CON_NORMAL, "Input demo replay result matched embedded trailer\n");
	}
	input_demo_replay_compare_terminal_exit_only = 0;
}

int input_demo_finish_replay_from_level_exit(void)
{
	input_demo_replay_last_timer_value = 0;
	if (!input_demo_replay_is_loaded())
		return 0;
	input_demo_replay_result_frame_count_override = input_demo_replay_frame_count();
	input_demo_replay_result_has_game_time64_override = 1;
	input_demo_replay_result_game_time64_override = input_demo_replay_final_game_time64();
	input_demo_replay_result_endlevel_completed_override = 1;
	input_demo_replay_compare_terminal_exit_only = 1;
	input_demo_debug_log_result_state("level-exit");
	input_demo_write_replay_result();
	input_demo_replay_unload();
	if (Game_wind)
		window_close(Game_wind);
	return 1;
}

int input_demo_finish_replay_from_mine_exit(void)
{
	input_demo_replay_last_timer_value = 0;
	if (!input_demo_replay_is_loaded())
		return 0;
	input_demo_replay_result_frame_count_override = input_demo_replay_frame_count();
	input_demo_replay_result_has_game_time64_override = 1;
	input_demo_replay_result_game_time64_override = input_demo_replay_final_game_time64();
	input_demo_replay_compare_terminal_exit_only = 1;
	input_demo_debug_log_result_state("mine-exit");
	input_demo_write_replay_result();
	input_demo_replay_unload();
	if (Game_wind)
		window_close(Game_wind);
	return 1;
}

static void input_demo_stop_replay(int write_result)
{
	input_demo_replay_last_timer_value = 0;
	if (input_demo_replay_is_loaded())
		input_demo_debug_log_result_state("stop");
	if (write_result)
		input_demo_write_replay_result();
	input_demo_replay_unload();
	if (Game_wind)
		window_close(Game_wind);
}

static void input_demo_delay_replay_frame(fix frame_time)
{
	fix64 timer_value;
	fix64 elapsed;

	if (!GameArg.SysUseNiceFPS)
		return;
	if (frame_time <= 0)
		return;
	timer_update();
	timer_value = timer_query();
	if (!input_demo_replay_last_timer_value) {
		input_demo_replay_last_timer_value = timer_value;
		return;
	}
	elapsed = timer_value - input_demo_replay_last_timer_value;
	while (elapsed < frame_time)
	{
		timer_delay(frame_time - elapsed);
		timer_update();
		timer_value = timer_query();
		elapsed = timer_value - input_demo_replay_last_timer_value;
	}
	input_demo_replay_last_timer_value = timer_value;
}

int input_demo_prepare_replay_frame(void)
{
	input_demo_replay_frame replay_frame;
	unsigned int actual_rng_state;
	char error[256] = "";

	if (!input_demo_replay_is_loaded())
		return 0;
	input_demo_update_result_kills_baseline();
	if (input_demo_replay_is_finished()) {
		input_demo_stop_replay(1);
		return 0;
	}
	if (!input_demo_replay_get_current_frame(&replay_frame, error, sizeof(error))) {
		con_printf(CON_NORMAL, "Input demo replay stopped: %s\n", error);
		input_demo_stop_replay(0);
		return 0;
	}
	if (!input_demo_replay_next_frame_index()) {
		input_demo_replay_logged_state_mismatch = 0;
		input_demo_replay_logged_state_trace_error = 0;
	}
	if (input_demo_replay_next_frame_index() > 0 && d_rand_get_state(&actual_rng_state) &&
		actual_rng_state != replay_frame.rng_state)
		con_printf(CON_NORMAL,
			"Input demo replay rng state mismatch: frame=%u gt=%lld expected=%u actual=%u\n",
			(unsigned int)input_demo_replay_next_frame_index(),
			(long long)GameTime64,
			replay_frame.rng_state,
			actual_rng_state);
	input_demo_delay_replay_frame((fix)replay_frame.frame_time);
	input_demo_control_info_from_state(&Controls, &replay_frame.state, &replay_frame.pulse);
	if (!d_rand_set_state(replay_frame.rng_state)) {
		con_printf(CON_NORMAL, "Input demo replay stopped: active RNG backend cannot restore state\n");
		input_demo_stop_replay(0);
		return 0;
	}
	if (input_demo_rng_trace_is_active() && replay_frame.has_rng_call_count)
		d_rand_set_call_count(replay_frame.rng_call_count);
	else
		d_rand_reset_call_count();
	FrameTime = (fix)replay_frame.frame_time;
	return 1;
}

int input_demo_step_replay_frame(void)
{
	input_demo_restore_replay_fp_environment();
	if (!input_demo_prepare_replay_frame())
		return 0;
	if (ReadControlsReplayFrame())
		return 0;
	if (!game_is_time_paused())
	{
		calc_game_time();
		GameProcessFrame();
		input_demo_advance_replay_frame();
	}
	return 1;
}

void input_demo_advance_replay_frame(void)
{
	input_demo_replay_frame replay_frame;
	char error[256] = "";

	if (!input_demo_replay_is_loaded())
		return;
	if (!input_demo_replay_get_current_frame(&replay_frame, error, sizeof(error))) {
		con_printf(CON_NORMAL, "Input demo replay stopped: %s\n", error);
		input_demo_stop_replay(0);
		return;
	}
	if (input_demo_replay_next_frame_index() + 1 >= input_demo_replay_frame_count())
		input_demo_debug_log_result_state("post-final-frame");
	if (!input_demo_replay_advance_frame(error, sizeof(error))) {
		con_printf(CON_NORMAL, "Input demo replay stopped: %s\n", error);
		input_demo_stop_replay(0);
		return;
	}
	if (input_demo_replay_is_finished())
		input_demo_stop_replay(1);
}

void input_demo_finish_replay_without_close(void)
{
	input_demo_replay_last_timer_value = 0;
	if (!input_demo_replay_is_loaded())
		return;
	input_demo_write_replay_result();
	input_demo_replay_unload();
}