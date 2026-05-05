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
#include "escort.h"
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

typedef struct input_demo_trace_key_snapshot {
	int valid;
	unsigned int state_key;
} input_demo_trace_key_snapshot;

typedef struct input_demo_escort_visit_snapshot {
	int valid;
	int mode;
	int buddy_seg;
	int player_seg;
	int believed_seg;
	int cur_path_index;
	int path_length;
	fix64 buddy_last_seen_player;
	fix64 buddy_last_player_path_created;
	fix64 escort_last_path_created;
	int away_gate;
	int recent_path_gate;
	int goto_player_gate;
	int same_seg_gate;
	int early_path_gate;
	int visit;
} input_demo_escort_visit_snapshot;

typedef struct input_demo_escort_state_snapshot {
	int valid;
	int player_visibility;
	int mode;
	int buddy_allowed_to_talk;
	int buddy_seg;
	int player_seg;
	int cur_path_index;
	int path_length;
	int hide_index;
	fix64 buddy_last_seen_player;
	fix64 buddy_last_player_path_created;
	fix64 escort_last_path_created;
	fix64 time_player_seen;
	int escort_goal_object;
	int escort_special_goal;
	int should_visit_player;
	int away_gate;
	int recent_path_gate;
	int goto_player_gate;
	int same_seg_gate;
	int early_path_gate;
} input_demo_escort_state_snapshot;

static input_demo_path_probe_state g_input_demo_last_path_detail[MAX_OBJECTS];
static input_demo_path_probe_state g_input_demo_last_path_points[MAX_OBJECTS];
static input_demo_escort_visit_snapshot g_input_demo_escort_visit_snapshot;
static input_demo_escort_state_snapshot g_input_demo_escort_state_snapshot;
static input_demo_trace_key_snapshot g_input_demo_snipe_entry_snapshot;
static input_demo_trace_key_snapshot g_input_demo_snipe_exit_snapshot;
static input_demo_trace_key_snapshot g_input_demo_thief_entry_snapshot;
static input_demo_trace_key_snapshot g_input_demo_thief_exit_snapshot;
static const char *input_demo_awareness_source_tag = "unset";
static int input_demo_awareness_source_objnum = -1;
static int input_demo_awareness_aux_objnum = -1;
static int input_demo_record_event_append_logged_error = 0;
static int g_input_demo_escort_segment_snapshot_valid = 0;
static int g_input_demo_escort_segment_snapshot_player_seg = -1;
static int g_input_demo_escort_segment_snapshot_believed_seg = -1;

extern fix64 Buddy_last_seen_player, Buddy_last_player_path_created;

#define INPUT_DEMO_SUSPECT_SPREADFIRE_FRAME_START 594
#define INPUT_DEMO_SUSPECT_SPREADFIRE_FRAME_END 615
#define INPUT_DEMO_SUSPECT_SPREADFIRE_TRACK_COUNT 3

static int input_demo_suspect_spreadfire_signatures[INPUT_DEMO_SUSPECT_SPREADFIRE_TRACK_COUNT] = { -1, -1, -1 };

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

int input_demo_trace_ai_active(void)
{
	return input_demo_debug_record_probe_active();
}

int input_demo_replay_awareness_probe_active(void)
{
	return input_demo_debug_activity_probe_active();
}

int input_demo_trace_escort_active(void)
{
	return input_demo_debug_activity_probe_active();
}

int input_demo_replay_collision_probe_active(void)
{
	return input_demo_debug_is_enabled() && input_demo_replay_is_loaded();
}

int input_demo_trace_collision_pose_active(void)
{
	return input_demo_debug_is_enabled() &&
		(input_demo_recorder_is_active() || input_demo_replay_is_loaded());
}

unsigned int input_demo_trace_collision_frame_index(void)
{
	if (input_demo_replay_is_loaded())
		return (unsigned int)input_demo_replay_next_frame_index();
	if (input_demo_recorder_is_active()) {
		const uint32_t frame_count = input_demo_recorder_frame_count();

		return frame_count ? (unsigned int)(frame_count - 1) : 0;
	}
	return 0;
}

const char *input_demo_trace_collision_mode_name(void)
{
	if (input_demo_replay_is_loaded())
		return "replay";
	if (input_demo_recorder_is_active())
		return "record";
	return "none";
}

int input_demo_replay_spreadfire_probe_active(void)
{
	return input_demo_debug_is_enabled() &&
		input_demo_replay_is_loaded() &&
		Players[Player_num].primary_weapon == SPREADFIRE_INDEX;
}

static int input_demo_replay_weapon_lifetime_probe_active(void)
{
	return input_demo_debug_is_enabled() && input_demo_replay_is_loaded();
}

int input_demo_replay_weapon_focus_active(void)
{
	unsigned int frame;

	if (!input_demo_replay_is_loaded())
		return 0;
	frame = (unsigned int)input_demo_replay_next_frame_index();
	return frame >= 1265 && frame <= 1267;
}

int input_demo_weapon_trace_active(void)
{
	return input_demo_debug_is_enabled() &&
		(input_demo_recorder_is_active() || input_demo_replay_is_loaded());
}

static unsigned int input_demo_weapon_trace_frame_index(void)
{
	if (input_demo_replay_is_loaded())
		return (unsigned int)input_demo_replay_next_frame_index();
	if (input_demo_recorder_is_active()) {
		const uint32_t frame_count = input_demo_recorder_frame_count();

		return frame_count ? (unsigned int)(frame_count - 1) : 0;
	}
	return 0;
}

static const char *input_demo_weapon_trace_mode_name(void)
{
	if (input_demo_replay_is_loaded())
		return "replay";
	if (input_demo_recorder_is_active())
		return "record";
	return "none";
}

static int input_demo_replay_weapon_creation_probe_active(void)
{
	return input_demo_replay_spreadfire_probe_active() ||
		input_demo_replay_weapon_lifetime_probe_active();
}

int input_demo_replay_is_player_owned_weapon(object *obj)
{
	return obj &&
		obj->type == OBJ_WEAPON &&
		obj->ctype.laser_info.parent_type == OBJ_PLAYER &&
		obj->ctype.laser_info.parent_num == Players[Player_num].objnum;
}

int input_demo_weapon_create_probe_active(object *obj)
{
	if (!obj || !input_demo_weapon_trace_active())
		return 0;
	if (obj->ctype.laser_info.parent_type == OBJ_ROBOT)
		return !(obj->flags & OF_HARMLESS);
	return input_demo_replay_is_player_owned_weapon(obj) && obj->id != FLARE_ID;
}

static void input_demo_reset_suspect_spreadfire_tracking(void)
{
	int i;

	for (i = 0; i < INPUT_DEMO_SUSPECT_SPREADFIRE_TRACK_COUNT; i++)
		input_demo_suspect_spreadfire_signatures[i] = -1;
}

static int input_demo_suspect_spreadfire_frame_active(unsigned int frame)
{
	return frame >= INPUT_DEMO_SUSPECT_SPREADFIRE_FRAME_START &&
		frame <= INPUT_DEMO_SUSPECT_SPREADFIRE_FRAME_END;
}

static void input_demo_refresh_suspect_spreadfire_tracking(void)
{
	if (!input_demo_replay_is_loaded() ||
		input_demo_replay_next_frame_index() < INPUT_DEMO_SUSPECT_SPREADFIRE_FRAME_START)
		input_demo_reset_suspect_spreadfire_tracking();
}

static int input_demo_tracked_suspect_spreadfire_index(object *obj)
{
	unsigned int frame;
	int i;

	if (!obj || !input_demo_replay_is_loaded())
		return -1;

	input_demo_refresh_suspect_spreadfire_tracking();
	frame = (unsigned int)input_demo_replay_next_frame_index();
	if (!input_demo_suspect_spreadfire_frame_active(frame))
		return -1;

	for (i = 0; i < INPUT_DEMO_SUSPECT_SPREADFIRE_TRACK_COUNT; i++)
		if (input_demo_suspect_spreadfire_signatures[i] == obj->signature)
			return i;

	return -1;
}

void input_demo_maybe_track_suspect_spreadfire(object *obj)
{
	unsigned int frame;
	int i;

	if (!obj || !input_demo_replay_is_loaded())
		return;

	input_demo_refresh_suspect_spreadfire_tracking();
	frame = (unsigned int)input_demo_replay_next_frame_index();
	if (frame != INPUT_DEMO_SUSPECT_SPREADFIRE_FRAME_START ||
		!input_demo_replay_is_player_owned_weapon(obj) ||
		obj->id != SPREADFIRE_ID)
		return;

	for (i = 0; i < INPUT_DEMO_SUSPECT_SPREADFIRE_TRACK_COUNT; i++)
		if (input_demo_suspect_spreadfire_signatures[i] == obj->signature)
			return;

	for (i = 0; i < INPUT_DEMO_SUSPECT_SPREADFIRE_TRACK_COUNT; i++)
		if (input_demo_suspect_spreadfire_signatures[i] == -1) {
			input_demo_suspect_spreadfire_signatures[i] = obj->signature;
			return;
		}
}

void input_demo_log_weapon_lifetime(const char *step, object *obj)
{
	const int tracked_index = input_demo_tracked_suspect_spreadfire_index(obj);

	if (!obj || !input_demo_weapon_trace_active())
		return;

	con_printf(CON_NORMAL,
		"Input demo weapon probe: mode=%s frame=%u gt=%lld step=%s obj=%d id=%d sig=%d track=%d seg=%d life=%d shields=%d flags=0x%x parent_type=%d parent=%d parent_sig=%d last_hit=%d ctime=%lld vel=(%d,%d,%d) pos=(%d,%d,%d) last=(%d,%d,%d)\n",
		input_demo_weapon_trace_mode_name(),
		input_demo_weapon_trace_frame_index(),
		(long long)GameTime64,
		step,
		obj - Objects,
		obj->id,
		obj->signature,
		tracked_index,
		obj->segnum,
		obj->lifeleft,
		obj->shields,
		obj->flags,
		obj->ctype.laser_info.parent_type,
		obj->ctype.laser_info.parent_num,
		obj->ctype.laser_info.parent_signature,
		obj->ctype.laser_info.last_hitobj,
		(long long)obj->ctype.laser_info.creation_time,
		obj->mtype.phys_info.velocity.x,
		obj->mtype.phys_info.velocity.y,
		obj->mtype.phys_info.velocity.z,
		obj->pos.x,
		obj->pos.y,
		obj->pos.z,
		obj->last_pos.x,
		obj->last_pos.y,
		obj->last_pos.z);
}

static void input_demo_record_frame_event_json(const char *json_text)
{
	char error[256] = "";

	if (!input_demo_recorder_is_active())
		return;
	if (!input_demo_recorder_append_frame_event_json(json_text, error, sizeof(error)) &&
		!input_demo_record_event_append_logged_error) {
		input_demo_record_event_append_logged_error = 1;
		con_printf(CON_NORMAL, "Input demo recorder event append failed: %s\n", error);
	}
}

void input_demo_record_wall_impact_event(object *weapon, int hitwall, int blew_up, int robot_escort)
{
	char json[256];

	if (!weapon)
		return;
	snprintf(json, sizeof(json),
		"{\"kind\":\"impact\",\"target\":\"wall\",\"gt\":%lld,\"weapon_obj\":%d,\"weapon_id\":%d,\"parent\":%d,\"seg\":%d,\"hitwall\":%d,\"blew_up\":%s,\"escort\":%s}",
		(long long)GameTime64,
		(int)(weapon - Objects),
		weapon->id,
		weapon->ctype.laser_info.parent_num,
		weapon->segnum,
		hitwall,
		blew_up ? "true" : "false",
		robot_escort ? "true" : "false");
	input_demo_record_frame_event_json(json);
}

void input_demo_record_robot_damage_event(object *robot, int32_t damage, int32_t old_shields)
{
	char json[512];

	if (!robot)
		return;
	snprintf(json, sizeof(json),
		"{\"kind\":\"robot_damage\",\"gt\":%lld,\"robot_obj\":%d,\"robot_sig\":%d,\"robot_id\":%d,\"damage\":%d,\"shields_before\":%d,\"shields_after\":%d,\"dead\":%s,\"x\":%d,\"y\":%d,\"z\":%d}",
		(long long)GameTime64,
		(int)(robot - Objects),
		robot->signature,
		robot->id,
		damage,
		old_shields,
		robot->shields,
		robot->shields < 0 ? "true" : "false",
		robot->pos.x,
		robot->pos.y,
		robot->pos.z);
	input_demo_record_frame_event_json(json);
}

void input_demo_record_robot_impact_event(object *weapon, object *robot)
{
	char json[256];

	if (!weapon || !robot)
		return;
	snprintf(json, sizeof(json),
		"{\"kind\":\"impact\",\"target\":\"robot\",\"gt\":%lld,\"weapon_obj\":%d,\"weapon_id\":%d,\"parent\":%d,\"robot_obj\":%d,\"robot_id\":%d,\"seg\":%d}",
		(long long)GameTime64,
		(int)(weapon - Objects),
		weapon->id,
		weapon->ctype.laser_info.parent_num,
		(int)(robot - Objects),
		robot->id,
		weapon->segnum);
	input_demo_record_frame_event_json(json);
}

void input_demo_record_player_damage_event(int32_t damage, int32_t old_shields, object *killer, int possibly_friendly)
{
	char json[384];
	const int killer_obj = killer ? (int)(killer - Objects) : -1;
	const int killer_type = killer ? killer->type : -1;
	const int killer_id = killer ? killer->id : -1;
	const int killer_sig = killer ? killer->signature : -1;
	const int killer_seg = killer ? killer->segnum : -1;

	snprintf(json, sizeof(json),
		"{\"kind\":\"player_damage\",\"gt\":%lld,\"damage\":%d,\"shields_before\":%d,\"shields_after\":%d,\"killer_type\":%d,\"killer_obj\":%d,\"killer_id\":%d,\"killer_sig\":%d,\"killer_seg\":%d,\"friendly\":%s}",
		(long long)GameTime64,
		damage,
		old_shields,
		Players[Player_num].shields,
		killer_type,
		killer_obj,
		killer_id,
		killer_sig,
		killer_seg,
		possibly_friendly ? "true" : "false");
	input_demo_record_frame_event_json(json);
}

int input_demo_replay_powerup_probe_active(void)
{
	const uint32_t frame = (uint32_t)input_demo_replay_next_frame_index();

	return input_demo_replay_is_loaded() && frame >= 816 && frame <= 818;
}

void input_demo_record_weapon_create_event(object *obj)
{
	char json[512];
	const int tracked_index = input_demo_tracked_suspect_spreadfire_index(obj);

	if (!obj)
		return;
	snprintf(json, sizeof(json),
		"{\"kind\":\"weapon_create\",\"gt\":%lld,\"obj\":%d,\"id\":%d,\"sig\":%d,\"track\":%d,\"seg\":%d,\"life\":%d,\"shields\":%d,\"flags\":%d,\"parent_type\":%d,\"parent\":%d,\"parent_sig\":%d,\"ctime\":%lld,\"vx\":%d,\"vy\":%d,\"vz\":%d,\"x\":%d,\"y\":%d,\"z\":%d}",
		(long long)GameTime64,
		(int)(obj - Objects),
		obj->id,
		obj->signature,
		tracked_index,
		obj->segnum,
		obj->lifeleft,
		obj->shields,
		obj->flags,
		obj->ctype.laser_info.parent_type,
		obj->ctype.laser_info.parent_num,
		obj->ctype.laser_info.parent_signature,
		(long long)obj->ctype.laser_info.creation_time,
		obj->mtype.phys_info.velocity.x,
		obj->mtype.phys_info.velocity.y,
		obj->mtype.phys_info.velocity.z,
		obj->pos.x,
		obj->pos.y,
		obj->pos.z);
	input_demo_record_frame_event_json(json);
}

void input_demo_record_player_shot_event(object *obj, int laser_type, int gun_num, int32_t spreadr, int32_t spreadu, int32_t delay_time, int make_sound, int harmless)
{
	char json[256];

	if (!obj)
		return;
	snprintf(json, sizeof(json),
		"{\"kind\":\"player_shot\",\"gt\":%lld,\"shooter_obj\":%d,\"laser_type\":%d,\"gun\":%d,\"spreadr\":%d,\"spreadu\":%d,\"delay\":%d,\"harmless\":%s,\"sound\":%s}",
		(long long)GameTime64,
		(int)(obj - Objects),
		laser_type,
		gun_num,
		spreadr,
		spreadu,
		delay_time,
		harmless ? "true" : "false",
		make_sound ? "true" : "false");
	input_demo_record_frame_event_json(json);
}

void input_demo_record_spreadfire_emit_event(int nfires, int flags, int spreadfire_toggle, int64_t next_laser_delta, int64_t last_laser_delta)
{
	char json[256];

	snprintf(json, sizeof(json),
		"{\"kind\":\"spreadfire_emit\",\"gt\":%lld,\"nfires\":%d,\"toggle_before\":%d,\"flags\":%d,\"next_laser_delta\":%lld,\"last_laser_delta\":%lld}",
		(long long)GameTime64,
		nfires,
		spreadfire_toggle,
		flags,
		(long long)next_laser_delta,
		(long long)last_laser_delta);
	input_demo_record_frame_event_json(json);
}

void input_demo_log_player_bump_probe(const char *step, object *obj0, object *obj1, const vms_vector *relative_velocity, const vms_vector *float_force, fix scale_num, fix scale_den, int damage_flag)
{
	const object *player = NULL;
	const object *other = NULL;
	const char *mode_name = "none";
	unsigned int frame_index = 0;
	vms_vector fix_force = {0, 0, 0};
	vms_vector force_delta = {0, 0, 0};

	if (!input_demo_debug_is_enabled())
		return;
	if (input_demo_replay_is_loaded()) {
		mode_name = "replay";
		frame_index = (unsigned int)input_demo_replay_next_frame_index();
	} else if (input_demo_recorder_is_active()) {
		const uint32_t frame_count = input_demo_recorder_frame_count();

		mode_name = "record";
		frame_index = frame_count ? (unsigned int)(frame_count - 1) : 0;
	} else
		return;

	if (obj0 == ConsoleObject && obj0 && obj0->type == OBJ_PLAYER &&
		obj1 && (obj1->type == OBJ_WEAPON || obj1->type == OBJ_ROBOT)) {
		player = obj0;
		other = obj1;
	} else if (obj1 == ConsoleObject && obj1 && obj1->type == OBJ_PLAYER &&
		obj0 && (obj0->type == OBJ_WEAPON || obj0->type == OBJ_ROBOT)) {
		player = obj1;
		other = obj0;
	}
	if (!player || !other)
		return;
	if (relative_velocity && scale_den) {
		fix_force.x = fixmuldiv(relative_velocity->x, scale_num, scale_den);
		fix_force.y = fixmuldiv(relative_velocity->y, scale_num, scale_den);
		fix_force.z = fixmuldiv(relative_velocity->z, scale_num, scale_den);
	}
	if (float_force) {
		force_delta.x = float_force->x - fix_force.x;
		force_delta.y = float_force->y - fix_force.y;
		force_delta.z = float_force->z - fix_force.z;
	}

	con_printf(CON_NORMAL,
		"Input demo bump probe: mode=%s frame=%u gt=%lld step=%s obj0=%d/%d/%d seg=%d pos=(%d,%d,%d) obj1=%d/%d/%d seg=%d pos=(%d,%d,%d) damage=%d rel_vel=(%d,%d,%d) scale=(%d,%d) float_force=(%d,%d,%d) fix_force=(%d,%d,%d) delta=(%d,%d,%d) obj0_vel=(%d,%d,%d) obj1_vel=(%d,%d,%d) obj0_mass=%d obj1_mass=%d\n",
		mode_name,
		frame_index,
		(long long)GameTime64,
		step,
		obj0 ? (int)(obj0 - Objects) : -1,
		obj0 ? obj0->type : -1,
		obj0 ? obj0->id : -1,
		obj0 ? obj0->segnum : -1,
		obj0 ? obj0->pos.x : 0,
		obj0 ? obj0->pos.y : 0,
		obj0 ? obj0->pos.z : 0,
		obj1 ? (int)(obj1 - Objects) : -1,
		obj1 ? obj1->type : -1,
		obj1 ? obj1->id : -1,
		obj1 ? obj1->segnum : -1,
		obj1 ? obj1->pos.x : 0,
		obj1 ? obj1->pos.y : 0,
		obj1 ? obj1->pos.z : 0,
		damage_flag,
		relative_velocity ? relative_velocity->x : 0,
		relative_velocity ? relative_velocity->y : 0,
		relative_velocity ? relative_velocity->z : 0,
		scale_num,
		scale_den,
		float_force ? float_force->x : 0,
		float_force ? float_force->y : 0,
		float_force ? float_force->z : 0,
		fix_force.x,
		fix_force.y,
		fix_force.z,
		force_delta.x,
		force_delta.y,
		force_delta.z,
		obj0 ? obj0->mtype.phys_info.velocity.x : 0,
		obj0 ? obj0->mtype.phys_info.velocity.y : 0,
		obj0 ? obj0->mtype.phys_info.velocity.z : 0,
		obj1 ? obj1->mtype.phys_info.velocity.x : 0,
		obj1 ? obj1->mtype.phys_info.velocity.y : 0,
		obj1 ? obj1->mtype.phys_info.velocity.z : 0,
		obj0 ? obj0->mtype.phys_info.mass : 0,
		obj1 ? obj1->mtype.phys_info.mass : 0);
}

void input_demo_log_score_probe(const char *score_kind, int points, int score_after)
{
	if (!input_demo_debug_is_enabled() || !input_demo_replay_is_loaded())
		return;

	con_printf(CON_NORMAL,
		"Input demo replay score probe: frame=%u gt=%lld kind=%s delta=%d score=%d\n",
		(unsigned int)input_demo_replay_next_frame_index(),
		(long long)GameTime64,
		score_kind,
		points,
		score_after);
}

void input_demo_log_reactor_hit(object *controlcen, fix damage, fix old_shields)
{
	if (!controlcen || !input_demo_replay_is_loaded())
		return;

	con_printf(CON_NORMAL,
		"Input demo replay reactor hit: gt=%lld frame=%u damage=%d shields=%d->%d seg=%d obj=%d\n",
		(long long)GameTime64,
		(unsigned int)input_demo_replay_next_frame_index(),
		f2i(damage),
		f2i(old_shields),
		f2i(controlcen->shields),
		controlcen->segnum,
		(int)(controlcen - Objects));
}

void input_demo_log_reactor_destroyed(object *controlcen)
{
	if (!controlcen || !input_demo_replay_is_loaded())
		return;

	con_printf(CON_NORMAL,
		"Input demo replay reactor destroyed: gt=%lld frame=%u seg=%d obj=%d\n",
		(long long)GameTime64,
		(unsigned int)input_demo_replay_next_frame_index(),
		controlcen->segnum,
		(int)(controlcen - Objects));
}

void input_demo_log_collision_player_wall_impact(const char *mode_name, unsigned int frame_index, object *playerobj, int hitseg, int hitwall, int hitspeed, int damage, int force_field_hit)
{
	if (!mode_name || !playerobj)
		return;

	con_printf(CON_NORMAL,
		"Input demo impact probe: mode=%s frame=%u kind=player_wall player_obj=%d seg=%d hitwall=%d hitspeed=%d damage=%d force=%d\n",
		mode_name,
		frame_index,
		(int)(playerobj - Objects),
		hitseg,
		hitwall,
		hitspeed,
		damage,
		force_field_hit);
}

void input_demo_log_replay_player_wall_collision(object *playerobj, int hitseg, int hitwall, int hitspeed, const vms_vector *hitpt)
{
	if (!playerobj || !input_demo_replay_is_loaded())
		return;

	con_printf(CON_NORMAL,
		"Input demo replay collision probe: frame=%u gt=%lld step=player_wall seg=%d hitseg=%d wall=%d speed=%d pos=(%d,%d,%d) hit=(%d,%d,%d)\n",
		(unsigned int)input_demo_replay_next_frame_index(),
		(long long)GameTime64,
		playerobj->segnum,
		hitseg,
		hitwall,
		hitspeed,
		playerobj->pos.x,
		playerobj->pos.y,
		playerobj->pos.z,
		hitpt ? hitpt->x : 0,
		hitpt ? hitpt->y : 0,
		hitpt ? hitpt->z : 0);
}

void input_demo_log_collision_weapon_wall_impact(const char *mode_name, unsigned int frame_index, object *weapon, int hitwall, int blew_up, int robot_escort)
{
	if (!mode_name || !weapon)
		return;

	con_printf(CON_NORMAL,
		"Input demo impact probe: mode=%s frame=%u kind=wall weapon_obj=%d weapon_id=%d parent=%d seg=%d hitwall=%d blew_up=%d escort=%d\n",
		mode_name,
		frame_index,
		(int)(weapon - Objects),
		weapon->id,
		weapon->ctype.laser_info.parent_num,
		weapon->segnum,
		hitwall,
		blew_up,
		robot_escort);
}

void input_demo_log_collision_player_robot_impact(const char *mode_name, unsigned int frame_index, object *playerobj, object *robot)
{
	if (!mode_name || !playerobj || !robot)
		return;

	con_printf(CON_NORMAL,
		"Input demo impact probe: mode=%s frame=%u kind=player_robot player_obj=%d robot_obj=%d robot_id=%d seg=%d\n",
		mode_name,
		frame_index,
		(int)(playerobj - Objects),
		(int)(robot - Objects),
		robot->id,
		playerobj->segnum);
}

void input_demo_log_collision_weapon_robot_impact(const char *mode_name, unsigned int frame_index, object *weapon, object *robot)
{
	if (!mode_name || !weapon || !robot)
		return;

	con_printf(CON_NORMAL,
		"Input demo impact probe: mode=%s frame=%u kind=robot weapon_obj=%d weapon_id=%d parent=%d robot_obj=%d robot_id=%d seg=%d\n",
		mode_name,
		frame_index,
		(int)(weapon - Objects),
		weapon->id,
		weapon->ctype.laser_info.parent_num,
		(int)(robot - Objects),
		robot->id,
		weapon->segnum);
}

void input_demo_log_replay_robot_damage(object *robot, fix damage, fix old_shields)
{
	if (!robot || !input_demo_debug_is_enabled() || !input_demo_replay_is_loaded())
		return;

	con_printf(CON_NORMAL,
		"Input demo replay robot damage: gt=%lld frame=%u robot_obj=%d robot_sig=%d robot_id=%d damage=%d shields=%d->%d dead=%d pos=(%d,%d,%d)\n",
		(long long)GameTime64,
		(unsigned int)input_demo_replay_next_frame_index(),
		(int)(robot - Objects),
		robot->signature,
		robot->id,
		damage,
		old_shields,
		robot->shields,
		robot->shields < 0,
		robot->pos.x,
		robot->pos.y,
		robot->pos.z);
}

void input_demo_log_player_damage_probe(const char *mode_name, unsigned int frame_index, int damage, fix old_shields, fix shields_after, int killer_type, int killer_obj, int killer_id, int killer_sig, int killer_seg, int possibly_friendly)
{
	if (!mode_name)
		return;

	con_printf(CON_NORMAL,
		"Input demo player damage: mode=%s gt=%lld frame=%u damage=%d shields=%d->%d killer_type=%d killer_obj=%d killer_id=%d killer_sig=%d killer_seg=%d friendly=%d\n",
		mode_name,
		(long long)GameTime64,
		frame_index,
		damage,
		old_shields,
		shields_after,
		killer_type,
		killer_obj,
		killer_id,
		killer_sig,
		killer_seg,
		possibly_friendly);
}

void input_demo_log_player_weapon_hit(const char *mode_name, unsigned int frame_index, object *weapon, object *playerobj, int damage, const vms_vector *collision_point)
{
	const int parent_num = weapon ? weapon->ctype.laser_info.parent_num : -1;
	const int parent_slot_valid = parent_num > -1 && parent_num <= Highest_object_index;
	const int parent_slot_type = parent_slot_valid ? Objects[parent_num].type : -1;
	const int parent_slot_id = parent_slot_valid ? Objects[parent_num].id : -1;
	const int parent_slot_sig = parent_slot_valid ? Objects[parent_num].signature : -1;
	const int parent_slot_seg = parent_slot_valid ? Objects[parent_num].segnum : -1;

	if (!mode_name || !weapon || !playerobj)
		return;

	con_printf(CON_NORMAL,
		"Input demo player weapon hit: mode=%s gt=%lld frame=%u weapon_obj=%d weapon_id=%d weapon_sig=%d weapon_seg=%d damage=%d parent_type=%d parent_num=%d parent_sig=%d slot_valid=%d slot_type=%d slot_id=%d slot_sig=%d slot_seg=%d sig_match=%d weapon_life=%d weapon_shields=%d weapon_flags=0x%x weapon_ctime=%lld weapon_pos=(%d,%d,%d) weapon_vel=(%d,%d,%d) player_pos=(%d,%d,%d) player_vel=(%d,%d,%d) hit=(%d,%d,%d)\n",
		mode_name,
		(long long)GameTime64,
		frame_index,
		(int)(weapon - Objects),
		weapon->id,
		weapon->signature,
		weapon->segnum,
		damage,
		weapon->ctype.laser_info.parent_type,
		parent_num,
		weapon->ctype.laser_info.parent_signature,
		parent_slot_valid,
		parent_slot_type,
		parent_slot_id,
		parent_slot_sig,
		parent_slot_seg,
		parent_slot_valid && parent_slot_sig == weapon->ctype.laser_info.parent_signature,
		weapon->lifeleft,
		weapon->shields,
		weapon->flags,
		(long long)weapon->ctype.laser_info.creation_time,
		weapon->pos.x,
		weapon->pos.y,
		weapon->pos.z,
		weapon->mtype.phys_info.velocity.x,
		weapon->mtype.phys_info.velocity.y,
		weapon->mtype.phys_info.velocity.z,
		playerobj->pos.x,
		playerobj->pos.y,
		playerobj->pos.z,
		playerobj->mtype.phys_info.velocity.x,
		playerobj->mtype.phys_info.velocity.y,
		playerobj->mtype.phys_info.velocity.z,
		collision_point ? collision_point->x : 0,
		collision_point ? collision_point->y : 0,
		collision_point ? collision_point->z : 0);
}

void input_demo_log_replay_powerup_probe_before(object *powerup, int energy_before, int shields_before)
{
	if (!powerup)
		return;

	con_printf(CON_NORMAL,
		"Input demo replay powerup probe: frame=%u step=before obj=%d id=%d energy=%d shields=%d flags=0x%x seg=%d pos=(%d,%d,%d)\n",
		(unsigned int)input_demo_replay_next_frame_index(),
		(int)(powerup - Objects),
		powerup->id,
		energy_before,
		shields_before,
		powerup->flags,
		powerup->segnum,
		powerup->pos.x,
		powerup->pos.y,
		powerup->pos.z);
}

void input_demo_log_replay_powerup_probe_after(object *powerup, int powerup_used, int energy_before, int energy_after, int shields_before, int shields_after)
{
	if (!powerup)
		return;

	con_printf(CON_NORMAL,
		"Input demo replay powerup probe: frame=%u step=after obj=%d id=%d used=%d energy_before=%d energy_after=%d shields_before=%d shields_after=%d flags=0x%x dead=%d\n",
		(unsigned int)input_demo_replay_next_frame_index(),
		(int)(powerup - Objects),
		powerup->id,
		powerup_used,
		energy_before,
		energy_after,
		shields_before,
		shields_after,
		powerup->flags,
		(powerup->flags & OF_SHOULD_BE_DEAD) != 0);
}

void input_demo_log_replay_object_object_collision(object *a, object *b, const vms_vector *collision_point)
{
	if (!a || !b)
		return;

	con_printf(CON_NORMAL,
		"Input demo replay collision probe: frame=%u gt=%lld step=object_object a=%d/%d/%d seg=%d b=%d/%d/%d seg=%d point=(%d,%d,%d)\n",
		(unsigned int)input_demo_replay_next_frame_index(),
		(long long)GameTime64,
		(int)(a - Objects),
		a->type,
		a->id,
		a->segnum,
		(int)(b - Objects),
		b->type,
		b->id,
		b->segnum,
		collision_point ? collision_point->x : 0,
		collision_point ? collision_point->y : 0,
		collision_point ? collision_point->z : 0);
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

static unsigned int input_demo_trace_hash_label(const char *label)
{
	unsigned int key = 0;

	if (!label)
		return 0;

	while (*label) {
		key = key * 131u + (unsigned int)(unsigned char)(*label);
		label++;
	}

	return key;
}

static int input_demo_trace_fix_bucket(fix value)
{
	if (value < 0)
		return -1;

	return f2i(value);
}

static int input_demo_trace_key_snapshot_should_log(input_demo_trace_key_snapshot *snapshot, unsigned int state_key)
{
	if (!snapshot)
		return 1;

	if (snapshot->valid && snapshot->state_key == state_key)
		return 0;

	snapshot->valid = 1;
	snapshot->state_key = state_key;
	return 1;
}

void input_demo_reset_escort_state_probes(void)
{
	memset(&g_input_demo_escort_visit_snapshot, 0, sizeof(g_input_demo_escort_visit_snapshot));
	memset(&g_input_demo_escort_state_snapshot, 0, sizeof(g_input_demo_escort_state_snapshot));
	memset(&g_input_demo_snipe_entry_snapshot, 0, sizeof(g_input_demo_snipe_entry_snapshot));
	memset(&g_input_demo_snipe_exit_snapshot, 0, sizeof(g_input_demo_snipe_exit_snapshot));
	memset(&g_input_demo_thief_entry_snapshot, 0, sizeof(g_input_demo_thief_entry_snapshot));
	memset(&g_input_demo_thief_exit_snapshot, 0, sizeof(g_input_demo_thief_exit_snapshot));
	g_input_demo_escort_segment_snapshot_valid = 0;
	g_input_demo_escort_segment_snapshot_player_seg = -1;
	g_input_demo_escort_segment_snapshot_believed_seg = -1;
}

void input_demo_log_escort_rng_progress(const char *label, unsigned int *rng_before, unsigned int *rng_call_count_before)
{
	unsigned int rng_after;
	unsigned int rng_call_count_after;

	if (!d_rand_get_state(&rng_after))
		return;
	rng_call_count_after = d_rand_get_call_count();
	if (rng_after == *rng_before && rng_call_count_after == *rng_call_count_before)
		return;
	con_printf(CON_NORMAL,
		"Input demo replay escort rng progress: frame=%u step=%s calls=%u->%u before=%u after=%u\n",
		input_demo_trace_frame_index(),
		label,
		*rng_call_count_before,
		rng_call_count_after,
		*rng_before,
		rng_after);
	*rng_before = rng_after;
	*rng_call_count_before = rng_call_count_after;
}

void input_demo_log_escort_path_state(const char *label, object *objp)
{
	char segs[512];
	int objnum;
	ai_static *aip;
	ai_local *ailp;
	int limit;
	int offset;
	int i;
	int written;

	if (!input_demo_trace_escort_active() || !objp)
		return;

	objnum = objp - Objects;
	aip = &objp->ctype.ai_info;
	ailp = &Ai_local_info[objnum];
	if ((aip->hide_index < 0) || (aip->path_length <= 0)) {
		strncpy(segs, "<none>", sizeof(segs));
		segs[sizeof(segs) - 1] = 0;
	} else {
		limit = aip->path_length < 24 ? aip->path_length : 24;
		offset = 0;
		segs[0] = 0;
		for (i=0; i<limit; i++) {
			written = snprintf(segs + offset, sizeof(segs) - offset, "%s%d", i ? "," : "", Point_segs[aip->hide_index + i].segnum);
			if (written < 0)
				break;
			if (written >= (int)(sizeof(segs) - offset)) {
				offset = sizeof(segs) - 1;
				break;
			}
			offset += written;
		}
		if ((limit < aip->path_length) && (offset < (int)sizeof(segs)))
			snprintf(segs + offset, sizeof(segs) - offset, ",...");
	}

	con_printf(CON_NORMAL,
		"Input demo replay escort path state: frame=%u step=%s obj=%d seg=%d mode=%d behavior=%d goal=%d special=%d goal_seg=%d cur_path=%d/%d hide=%d segs=%s\n",
		input_demo_trace_frame_index(),
		label,
		objnum,
		objp->segnum,
		ailp->mode,
		aip->behavior,
		Escort_goal_object,
		Escort_special_goal,
		ailp->goal_segment,
		aip->cur_path_index,
		aip->path_length,
		aip->hide_index,
		segs);
}

void input_demo_log_escort_restore_normalization(object *objp, ai_local *ailp,
	int64_t raw_time_player_seen, int64_t raw_escort_last_path_created)
{
	ai_static *aip;

	if (!input_demo_trace_escort_active() || !objp || !ailp)
		return;

	aip = &objp->ctype.ai_info;
	con_printf(CON_NORMAL,
		"Input demo replay escort restore normalize: gt=%lld obj=%d seg=%d mode=%d prev_vis=%d raw_seen=%lld raw_escort_last_path=%lld final_seen=%lld final_last_player_path=%lld final_escort_last_path=%lld cur_path=%d/%d hide_index=%d\n",
		(long long)GameTime64,
		(int)(objp - Objects),
		objp->segnum,
		ailp->mode,
		ailp->previous_visibility,
		(long long)raw_time_player_seen,
		(long long)raw_escort_last_path_created,
		(long long)Buddy_last_seen_player,
		(long long)Buddy_last_player_path_created,
		(long long)Escort_last_path_created,
		aip->cur_path_index,
		aip->path_length,
		aip->hide_index);
}

void input_demo_log_escort_segment_change(object *objp, ai_local *ailp, ai_static *aip,
	int player_seg, int believed_seg)
{
	if (!input_demo_trace_escort_active() || !objp || !ailp || !aip)
		return;

	if (!g_input_demo_escort_segment_snapshot_valid) {
		g_input_demo_escort_segment_snapshot_valid = 1;
		g_input_demo_escort_segment_snapshot_player_seg = player_seg;
		g_input_demo_escort_segment_snapshot_believed_seg = believed_seg;
		return;
	}

	if ((player_seg == g_input_demo_escort_segment_snapshot_player_seg) &&
		(believed_seg == g_input_demo_escort_segment_snapshot_believed_seg))
		return;

	con_printf(CON_NORMAL,
		"Input demo replay escort segment change: gt=%lld player_seg=%d->%d believed_seg=%d->%d buddy_seg=%d mode=%d cur_path=%d/%d prev_vis=%d last_seen=%lld last_player_path=%lld\n",
		(long long)GameTime64,
		g_input_demo_escort_segment_snapshot_player_seg,
		player_seg,
		g_input_demo_escort_segment_snapshot_believed_seg,
		believed_seg,
		objp->segnum,
		ailp->mode,
		aip->cur_path_index,
		aip->path_length,
		ailp->previous_visibility,
		(long long)Buddy_last_seen_player,
		(long long)Buddy_last_player_path_created);

	g_input_demo_escort_segment_snapshot_player_seg = player_seg;
	g_input_demo_escort_segment_snapshot_believed_seg = believed_seg;
}

void input_demo_log_escort_visit_change(object *objp, ai_local *ailp, ai_static *aip,
	int player_seg, int believed_seg, int away_gate, int recent_path_gate,
	int goto_player_gate, int same_seg_gate, int early_path_gate, int visit)
{
	input_demo_escort_visit_snapshot current;

	if (!input_demo_trace_escort_active() || !objp || !ailp || !aip)
		return;

	current.valid = 1;
	current.mode = ailp->mode;
	current.buddy_seg = objp->segnum;
	current.player_seg = player_seg;
	current.believed_seg = believed_seg;
	current.cur_path_index = aip->cur_path_index;
	current.path_length = aip->path_length;
	current.buddy_last_seen_player = Buddy_last_seen_player;
	current.buddy_last_player_path_created = Buddy_last_player_path_created;
	current.escort_last_path_created = Escort_last_path_created;
	current.away_gate = away_gate;
	current.recent_path_gate = recent_path_gate;
	current.goto_player_gate = goto_player_gate;
	current.same_seg_gate = same_seg_gate;
	current.early_path_gate = early_path_gate;
	current.visit = visit;

	if (!g_input_demo_escort_visit_snapshot.valid) {
		g_input_demo_escort_visit_snapshot = current;
		return;
	}

	if ((current.mode == g_input_demo_escort_visit_snapshot.mode) &&
		(current.buddy_seg == g_input_demo_escort_visit_snapshot.buddy_seg) &&
		(current.player_seg == g_input_demo_escort_visit_snapshot.player_seg) &&
		(current.believed_seg == g_input_demo_escort_visit_snapshot.believed_seg) &&
		(current.cur_path_index == g_input_demo_escort_visit_snapshot.cur_path_index) &&
		(current.path_length == g_input_demo_escort_visit_snapshot.path_length) &&
		(current.buddy_last_player_path_created == g_input_demo_escort_visit_snapshot.buddy_last_player_path_created) &&
		(current.escort_last_path_created == g_input_demo_escort_visit_snapshot.escort_last_path_created) &&
		(current.away_gate == g_input_demo_escort_visit_snapshot.away_gate) &&
		(current.recent_path_gate == g_input_demo_escort_visit_snapshot.recent_path_gate) &&
		(current.goto_player_gate == g_input_demo_escort_visit_snapshot.goto_player_gate) &&
		(current.same_seg_gate == g_input_demo_escort_visit_snapshot.same_seg_gate) &&
		(current.early_path_gate == g_input_demo_escort_visit_snapshot.early_path_gate) &&
		(current.visit == g_input_demo_escort_visit_snapshot.visit))
		return;

	con_printf(CON_NORMAL,
		"Input demo replay escort visit change: gt=%lld mode=%d->%d buddy_seg=%d->%d player_seg=%d->%d believed_seg=%d->%d cur_path=%d/%d->%d/%d last_seen=%lld->%lld last_player_path=%lld->%lld escort_last_path=%lld->%lld away=%d->%d recent=%d->%d goto=%d->%d same=%d->%d early=%d->%d visit=%d->%d\n",
		(long long)GameTime64,
		g_input_demo_escort_visit_snapshot.mode,
		current.mode,
		g_input_demo_escort_visit_snapshot.buddy_seg,
		current.buddy_seg,
		g_input_demo_escort_visit_snapshot.player_seg,
		current.player_seg,
		g_input_demo_escort_visit_snapshot.believed_seg,
		current.believed_seg,
		g_input_demo_escort_visit_snapshot.cur_path_index,
		g_input_demo_escort_visit_snapshot.path_length,
		current.cur_path_index,
		current.path_length,
		(long long)g_input_demo_escort_visit_snapshot.buddy_last_seen_player,
		(long long)current.buddy_last_seen_player,
		(long long)g_input_demo_escort_visit_snapshot.buddy_last_player_path_created,
		(long long)current.buddy_last_player_path_created,
		(long long)g_input_demo_escort_visit_snapshot.escort_last_path_created,
		(long long)current.escort_last_path_created,
		g_input_demo_escort_visit_snapshot.away_gate,
		current.away_gate,
		g_input_demo_escort_visit_snapshot.recent_path_gate,
		current.recent_path_gate,
		g_input_demo_escort_visit_snapshot.goto_player_gate,
		current.goto_player_gate,
		g_input_demo_escort_visit_snapshot.same_seg_gate,
		current.same_seg_gate,
		g_input_demo_escort_visit_snapshot.early_path_gate,
		current.early_path_gate,
		g_input_demo_escort_visit_snapshot.visit,
		current.visit);

	g_input_demo_escort_visit_snapshot = current;
}

void input_demo_log_escort_state(object *objp, ai_local *ailp, ai_static *aip,
	int32_t dist_to_player, int player_visibility, int should_visit_player,
	int64_t since_seen, int64_t since_player_path, int away_gate,
	int recent_path_gate, int goto_player_gate, int same_seg_gate,
	int early_path_gate)
{
	input_demo_escort_state_snapshot current;

	if (!input_demo_trace_escort_active() || !objp || !ailp || !aip)
		return;

	current.valid = 1;
	current.player_visibility = player_visibility;
	current.mode = ailp->mode;
	current.buddy_allowed_to_talk = Buddy_allowed_to_talk;
	current.buddy_seg = objp->segnum;
	current.player_seg = ConsoleObject ? ConsoleObject->segnum : -1;
	current.cur_path_index = aip->cur_path_index;
	current.path_length = aip->path_length;
	current.hide_index = aip->hide_index;
	current.buddy_last_seen_player = Buddy_last_seen_player;
	current.buddy_last_player_path_created = Buddy_last_player_path_created;
	current.escort_last_path_created = Escort_last_path_created;
	current.time_player_seen = ailp->time_player_seen;
	current.escort_goal_object = Escort_goal_object;
	current.escort_special_goal = Escort_special_goal;
	current.should_visit_player = should_visit_player;
	current.away_gate = away_gate;
	current.recent_path_gate = recent_path_gate;
	current.goto_player_gate = goto_player_gate;
	current.same_seg_gate = same_seg_gate;
	current.early_path_gate = early_path_gate;

	if (g_input_demo_escort_state_snapshot.valid &&
		(current.player_visibility == g_input_demo_escort_state_snapshot.player_visibility) &&
		(current.mode == g_input_demo_escort_state_snapshot.mode) &&
		(current.buddy_allowed_to_talk == g_input_demo_escort_state_snapshot.buddy_allowed_to_talk) &&
		(current.buddy_seg == g_input_demo_escort_state_snapshot.buddy_seg) &&
		(current.player_seg == g_input_demo_escort_state_snapshot.player_seg) &&
		(current.cur_path_index == g_input_demo_escort_state_snapshot.cur_path_index) &&
		(current.path_length == g_input_demo_escort_state_snapshot.path_length) &&
		(current.hide_index == g_input_demo_escort_state_snapshot.hide_index) &&
		(current.buddy_last_seen_player == g_input_demo_escort_state_snapshot.buddy_last_seen_player) &&
		(current.buddy_last_player_path_created == g_input_demo_escort_state_snapshot.buddy_last_player_path_created) &&
		(current.escort_last_path_created == g_input_demo_escort_state_snapshot.escort_last_path_created) &&
		(current.time_player_seen == g_input_demo_escort_state_snapshot.time_player_seen) &&
		(current.escort_goal_object == g_input_demo_escort_state_snapshot.escort_goal_object) &&
		(current.escort_special_goal == g_input_demo_escort_state_snapshot.escort_special_goal) &&
		(current.should_visit_player == g_input_demo_escort_state_snapshot.should_visit_player) &&
		(current.away_gate == g_input_demo_escort_state_snapshot.away_gate) &&
		(current.recent_path_gate == g_input_demo_escort_state_snapshot.recent_path_gate) &&
		(current.goto_player_gate == g_input_demo_escort_state_snapshot.goto_player_gate) &&
		(current.same_seg_gate == g_input_demo_escort_state_snapshot.same_seg_gate) &&
		(current.early_path_gate == g_input_demo_escort_state_snapshot.early_path_gate))
		return;

	con_printf(CON_NORMAL,
		"Input demo replay escort state: frame=%u gt=%lld vis=%d mode=%d talk=%d dist=%d buddy_seg=%d player_seg=%d cur_path=%d/%d hide_index=%d last_seen=%lld last_player_path=%lld escort_last_path=%lld seen=%lld goal=%d special=%d visit=%d since_seen=%lld since_player_path=%lld away_gate=%d recent_path_gate=%d goto_player_gate=%d same_seg_gate=%d early_path_gate=%d\n",
			input_demo_trace_frame_index(),
			(long long)GameTime64,
			player_visibility,
			ailp->mode,
			Buddy_allowed_to_talk,
			dist_to_player,
			objp->segnum,
			ConsoleObject ? ConsoleObject->segnum : -1,
			aip->cur_path_index,
			aip->path_length,
			aip->hide_index,
			(long long)Buddy_last_seen_player,
			(long long)Buddy_last_player_path_created,
			(long long)Escort_last_path_created,
			(long long)ailp->time_player_seen,
			Escort_goal_object,
			Escort_special_goal,
			should_visit_player,
			(long long)since_seen,
			(long long)since_player_path,
			away_gate,
			recent_path_gate,
			goto_player_gate,
			same_seg_gate,
			early_path_gate);

	g_input_demo_escort_state_snapshot = current;
}

void input_demo_log_snipe_detail_probe(int entry_probe, const char *step, object *objp, ai_local *ailp,
	int player_visibility, int32_t dist_to_player)
{
	input_demo_trace_key_snapshot *snapshot;
	ai_static *aip;
	unsigned int state_key;

	if (!input_demo_trace_escort_active() || !objp || !ailp)
		return;

	snapshot = entry_probe ? &g_input_demo_snipe_entry_snapshot : &g_input_demo_snipe_exit_snapshot;
	aip = &objp->ctype.ai_info;
	state_key = input_demo_trace_hash_label(step);
	state_key = state_key * 131u + (unsigned int)ailp->mode;
	state_key = state_key * 131u + (unsigned int)input_demo_trace_fix_bucket(ailp->next_action_time);
	state_key = state_key * 131u + (unsigned int)player_visibility;
	state_key = state_key * 131u + (unsigned int)aip->cur_path_index;
	state_key = state_key * 131u + (unsigned int)aip->path_length;
	state_key = state_key * 131u + (unsigned int)aip->hide_index;
	state_key = state_key * 131u + (unsigned int)objp->segnum;

	if (!input_demo_trace_key_snapshot_should_log(snapshot, state_key))
		return;

	con_printf(CON_NORMAL,
		"Input demo replay snipe detail: frame=%u obj=%d step=%s mode=%d next_action=%d vis=%d dist=%d path=%d/%d hide=%d seg=%d pos=(%d,%d,%d)\n",
		input_demo_trace_frame_index(),
		(int)(objp - Objects),
		step,
		ailp->mode,
		ailp->next_action_time,
		player_visibility,
		dist_to_player,
		aip->cur_path_index,
		aip->path_length,
		aip->hide_index,
		objp->segnum,
		objp->pos.x,
		objp->pos.y,
		objp->pos.z);
}

void input_demo_log_thief_detail_probe(int entry_probe, const char *step, object *objp, ai_local *ailp,
	int player_visibility, int32_t dist_to_player)
{
	input_demo_trace_key_snapshot *snapshot;
	ai_static *aip;
	unsigned int state_key;

	if (!input_demo_trace_escort_active() || !objp || !ailp)
		return;

	snapshot = entry_probe ? &g_input_demo_thief_entry_snapshot : &g_input_demo_thief_exit_snapshot;
	aip = &objp->ctype.ai_info;
	state_key = input_demo_trace_hash_label(step);
	state_key = state_key * 131u + (unsigned int)ailp->mode;
	state_key = state_key * 131u + (unsigned int)input_demo_trace_fix_bucket(ailp->next_action_time);
	state_key = state_key * 131u + (unsigned int)player_visibility;
	state_key = state_key * 131u + (unsigned int)ailp->player_awareness_type;
	state_key = state_key * 131u + (unsigned int)aip->cur_path_index;
	state_key = state_key * 131u + (unsigned int)aip->path_length;
	state_key = state_key * 131u + (unsigned int)aip->hide_index;
	state_key = state_key * 131u + (unsigned int)objp->segnum;

	if (!input_demo_trace_key_snapshot_should_log(snapshot, state_key))
		return;

	con_printf(CON_NORMAL,
		"Input demo replay thief detail: frame=%u obj=%d step=%s mode=%d next_action=%d vis=%d aware=%d dist=%d path=%d/%d hide=%d seg=%d pos=(%d,%d,%d)\n",
		input_demo_trace_frame_index(),
		(int)(objp - Objects),
		step,
		ailp->mode,
		ailp->next_action_time,
		player_visibility,
		ailp->player_awareness_type,
		dist_to_player,
		aip->cur_path_index,
		aip->path_length,
		aip->hide_index,
		objp->segnum,
		objp->pos.x,
		objp->pos.y,
		objp->pos.z);
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

void input_demo_log_path_points(const char *label, object *objp, const void *psegs_data, int num_points)
{
	const point_seg *psegs = (const point_seg *)psegs_data;
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

void input_demo_log_robot_lifecycle_delete(int objnum, object *obj)
{
	if (!input_demo_robot_lifecycle_is_target(objnum, obj))
		return;

	con_printf(CON_NORMAL,
		"Input demo robot lifecycle: mode=%s frame=%u gt=%lld step=obj_delete obj=%d sig=%d id=%d seg=%d flags=0x%x shields=%d life=%d exploding=%d should_die=%d\n",
		input_demo_debug_activity_mode_name(),
		input_demo_debug_frame_index(),
		(long long)GameTime64,
		objnum,
		obj->signature,
		obj->id,
		obj->segnum,
		obj->flags,
		obj->shields,
		obj->lifeleft,
		(obj->flags & OF_EXPLODING) != 0,
		(obj->flags & OF_SHOULD_BE_DEAD) != 0);
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

void input_demo_log_awareness_vulcan_roll(object *objp, int type, int vulcan_roll, unsigned int sim_calls_before, unsigned int sim_state_before)
{
	const int objnum = objp ? (int)(objp - Objects) : -1;

	if (!objp || (objnum < 0))
		return;

	con_printf(CON_NORMAL,
		"Input demo awareness vulcan roll: mode=%s frame=%u gt=%lld type=%d obj=%d roll=%d threshold=3276 calls_before=%u state_before=%u\n",
		input_demo_debug_activity_mode_name(),
		input_demo_trace_frame_index(),
		(long long)GameTime64,
		type,
		objnum,
		vulcan_roll,
		sim_calls_before,
		sim_state_before);
}

void input_demo_log_awareness_add_return(const char *reason, object *objp, int type, int added, unsigned int sim_calls_before, unsigned int sim_calls_after, unsigned int sim_state_before, unsigned int sim_state_after)
{
	const int objnum = objp ? (int)(objp - Objects) : -1;

	if (!objp || (objnum < 0))
		return;

	if (reason && reason[0]) {
		con_printf(CON_NORMAL,
			"Input demo awareness add return: mode=%s frame=%u gt=%lld type=%d obj=%d added=%d reason=%s calls=%u->%u state=%u->%u\n",
			input_demo_debug_activity_mode_name(),
			input_demo_trace_frame_index(),
			(long long)GameTime64,
			type,
			objnum,
			added,
			reason,
			sim_calls_before,
			sim_calls_after,
			sim_state_before,
			sim_state_after);
		return;
	}

	con_printf(CON_NORMAL,
		"Input demo awareness add return: mode=%s frame=%u gt=%lld type=%d obj=%d added=%d calls=%u->%u state=%u->%u\n",
		input_demo_debug_activity_mode_name(),
		input_demo_trace_frame_index(),
		(long long)GameTime64,
		type,
		objnum,
		added,
		sim_calls_before,
		sim_calls_after,
		sim_state_before,
		sim_state_after);
}

void input_demo_log_awareness_entry(object *objp, int type, const char *source_tag, int source_objnum, int aux_objnum, unsigned int sim_calls_entry, unsigned int sim_state_entry, int num_awareness_before, int overall_agitation_before, int multiplayer_awareness_allowed)
{
	const int objnum = objp ? (int)(objp - Objects) : -1;

	if (!objp || (objnum < 0))
		return;

	con_printf(CON_NORMAL,
		"Input demo awareness entry: mode=%s frame=%u gt=%lld type=%d obj=%d source=%s source_obj=%d aux_obj=%d calls=%u state=%u awareness=%d agitation=%d gate=%d\n",
		input_demo_debug_activity_mode_name(),
		input_demo_trace_frame_index(),
		(long long)GameTime64,
		type,
		objnum,
		source_tag,
		source_objnum,
		aux_objnum,
		sim_calls_entry,
		sim_state_entry,
		num_awareness_before,
		overall_agitation_before,
		multiplayer_awareness_allowed);
}

void input_demo_log_awareness_probe(object *objp, int type)
{
	const int objnum = objp ? (int)(objp - Objects) : -1;
	int parent_type = -1;
	int parent_num = -1;
	int parent_sig = -1;

	if (!objp || (objnum < 0))
		return;

	if (objp->type == OBJ_WEAPON) {
		parent_type = objp->ctype.laser_info.parent_type;
		parent_num = objp->ctype.laser_info.parent_num;
		parent_sig = objp->ctype.laser_info.parent_signature;
	}

	con_printf(CON_NORMAL,
		"Input demo awareness probe: mode=%s frame=%u gt=%lld type=%d obj=%d obj_type=%d obj_id=%d sig=%d seg=%d life=%d flags=%d parent_type=%d parent_num=%d parent_sig=%d pos=(%d,%d,%d)\n",
		input_demo_debug_activity_mode_name(),
		input_demo_trace_frame_index(),
		(long long)GameTime64,
		type,
		objnum,
		objp->type,
		objp->id,
		objp->signature,
		objp->segnum,
		objp->lifeleft,
		objp->flags,
		parent_type,
		parent_num,
		parent_sig,
		objp->pos.x,
		objp->pos.y,
		objp->pos.z);
}

void input_demo_log_awareness_post_add(object *objp, int type, int awareness_added, unsigned int sim_calls_entry, unsigned int sim_calls_after_add, unsigned int sim_state_entry, unsigned int sim_state_after_add)
{
	const int objnum = objp ? (int)(objp - Objects) : -1;

	if (!objp || (objnum < 0))
		return;

	con_printf(CON_NORMAL,
		"Input demo awareness post-add: mode=%s frame=%u gt=%lld type=%d obj=%d added=%d calls=%u->%u state=%u->%u\n",
		input_demo_debug_activity_mode_name(),
		input_demo_trace_frame_index(),
		(long long)GameTime64,
		type,
		objnum,
		awareness_added,
		sim_calls_entry,
		sim_calls_after_add,
		sim_state_entry,
		sim_state_after_add);
}

void input_demo_log_awareness_post_gate(object *objp, int type, int rng_gate_value, int rng_gate_pass, unsigned int sim_calls_before, unsigned int sim_calls_after, unsigned int sim_state_before, unsigned int sim_state_after)
{
	const int objnum = objp ? (int)(objp - Objects) : -1;

	if (!objp || (objnum < 0))
		return;

	con_printf(CON_NORMAL,
		"Input demo awareness post-gate: mode=%s frame=%u gt=%lld type=%d obj=%d rng=%d pass=%d calls=%u->%u state=%u->%u\n",
		input_demo_debug_activity_mode_name(),
		input_demo_trace_frame_index(),
		(long long)GameTime64,
		type,
		objnum,
		rng_gate_value,
		rng_gate_pass,
		sim_calls_before,
		sim_calls_after,
		sim_state_before,
		sim_state_after);
}

void input_demo_log_awareness_result(object *objp, int type, const char *source_tag, int source_objnum, int aux_objnum, int awareness_added, int skipped_observer, int awareness_before, int awareness_after, int agitation_before, int agitation_after, int multiplayer_awareness_allowed, int rng_gate_value, int rng_gate_pass)
{
	const int objnum = objp ? (int)(objp - Objects) : -1;

	if (!objp || (objnum < 0))
		return;

	if (skipped_observer) {
		con_printf(CON_NORMAL,
			"Input demo awareness result: mode=%s frame=%u gt=%lld type=%d obj=%d source=%s source_obj=%d aux_obj=%d skipped=observer awareness=%d->%d agitation=%d->%d gate=%d rng=%d rng_pass=%d\n",
			input_demo_debug_activity_mode_name(),
			input_demo_trace_frame_index(),
			(long long)GameTime64,
			type,
			objnum,
			source_tag,
			source_objnum,
			aux_objnum,
			awareness_before,
			awareness_after,
			agitation_before,
			agitation_after,
			multiplayer_awareness_allowed,
			rng_gate_value,
			rng_gate_pass);
		return;
	}

	con_printf(CON_NORMAL,
		"Input demo awareness result: mode=%s frame=%u gt=%lld type=%d obj=%d source=%s source_obj=%d aux_obj=%d added=%d awareness=%d->%d agitation=%d->%d gate=%d rng=%d rng_pass=%d\n",
		input_demo_debug_activity_mode_name(),
		input_demo_trace_frame_index(),
		(long long)GameTime64,
		type,
		objnum,
		source_tag,
		source_objnum,
		aux_objnum,
		awareness_added,
		awareness_before,
		awareness_after,
		agitation_before,
		agitation_after,
		multiplayer_awareness_allowed,
		rng_gate_value,
		rng_gate_pass);
}

void input_demo_log_ai_state(void)
{
	if (!ConsoleObject)
		return;

	con_printf(CON_NORMAL,
		"Input demo replay AI state: frame=%u gt=%lld player_seg=%d believed_seg=%d player=(%d,%d,%d) believed=(%d,%d,%d) last_fired=(%d,%d,%d) dist=%d events=%d agitation=%d\n",
		input_demo_trace_frame_index(),
		(long long)GameTime64,
		ConsoleObject->segnum,
		Believed_player_seg,
		ConsoleObject->pos.x,
		ConsoleObject->pos.y,
		ConsoleObject->pos.z,
		Believed_player_pos.x,
		Believed_player_pos.y,
		Believed_player_pos.z,
		Last_fired_upon_player_pos.x,
		Last_fired_upon_player_pos.y,
		Last_fired_upon_player_pos.z,
		Dist_to_last_fired_upon_player_pos,
		Num_awareness_events,
		Overall_agitation);
}

void input_demo_log_ai_frame(void)
{
	if (!ConsoleObject)
		return;

	con_printf(CON_NORMAL,
		"Input demo replay AI frame: frame=%u gt=%lld player_seg=%d believed_seg=%d events=%d agitation=%d\n",
		input_demo_trace_frame_index(),
		(long long)GameTime64,
		ConsoleObject->segnum,
		Believed_player_seg,
		Num_awareness_events,
		Overall_agitation);
}

void input_demo_log_ai_frame_summary(int traced_robot_count)
{
	con_printf(CON_NORMAL,
		"Input demo replay AI frame summary: frame=%u traced=%d highest_obj=%d\n",
		input_demo_trace_frame_index(),
		traced_robot_count,
		Highest_object_index);
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