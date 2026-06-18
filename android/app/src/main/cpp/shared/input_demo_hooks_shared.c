#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "args.h"
#include "ai.h"
#include "cntrlcen.h"
#include "console.h"
#include "endlevel.h"
#include "game.h"
#include "gameseq.h"
#include "input_demo_control_info.h"
#include "input_demo_debug_logging.h"
#include "input_demo_fp_env.h"
#include "input_demo_hooks.h"
#include "input_demo_recorder.h"
#include "input_demo_replay.h"
#include "input_demo_result.h"
#include "input_demo_rng_trace.h"
#include "input_demo_state_trace.h"
#include "laser.h"
#include "maths.h"
#include "mission.h"
#include "newdemo.h"
#include "object.h"
#include "player.h"
#include "robot.h"
#include "timer.h"

#ifdef DXX_BUILD_DESCENT_II
#include "d1_in_d2.h"
#endif

#include "input_demo_hooks_shared.h"

extern int ReadControlsReplayFrame(void);
extern void ReadControlsReplayPostFrame(void);
extern void GameProcessFrame(void);
extern int game_is_time_paused(void);

static int previous_robot_hashes_valid = 0;
static unsigned int previous_robot_hashes[MAX_OBJECTS];
static int previous_robot_ai_static_hashes_valid = 0;
static unsigned int previous_robot_ai_static_hashes[MAX_OBJECTS];
static ai_static previous_robot_ai_static_values[MAX_OBJECTS];
static int previous_robot_ai_static_present[MAX_OBJECTS];
static int previous_robot_anim_pose_hashes_valid = 0;
static unsigned int previous_robot_anim_pose_hashes[MAX_OBJECTS];
static int previous_robot_anim_pose_present[MAX_OBJECTS];
static int previous_fireball_hashes_valid = 0;
static unsigned int previous_fireball_hashes[MAX_OBJECTS];

#ifdef DXX_BUILD_DESCENT_II
extern void input_demo_capture_current_result_prep_d2(player *current_player);
extern void input_demo_capture_current_result_after_game_time_d2(
    input_demo_result *result);
extern int input_demo_capture_current_result_robots_killed_d2(
    player *current_player);
extern void input_demo_capture_current_result_after_powerups_d2(
    input_demo_result *result);
#endif

static int input_demo_collision_trace_enabled(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return input_demo_debug_is_enabled();
#else
	return 1;
#endif
}

static const char *input_demo_result_game_name(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return "d2";
#else
	return "d1";
#endif
}

static const char *input_demo_result_mission_id(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return (Current_mission && Current_mission_filename) ? Current_mission_filename : "";
#else
	return input_demo_current_mission_id();
#endif
}

static void input_demo_prepare_current_result(player *current_player)
{
#ifdef DXX_BUILD_DESCENT_II
	input_demo_capture_current_result_prep_d2(current_player);
#else
	(void) current_player;
#endif
}

static void input_demo_finish_current_result_game_time(input_demo_result *result)
{
#ifdef DXX_BUILD_DESCENT_II
	input_demo_capture_current_result_after_game_time_d2(result);
#else
	(void) result;
#endif
}

static int input_demo_current_result_robots_killed(player *current_player)
{
#ifdef DXX_BUILD_DESCENT_II
	return input_demo_capture_current_result_robots_killed_d2(current_player);
#else
	return current_player->num_robots_level;
#endif
}

static void input_demo_finish_current_result_powerups(input_demo_result *result)
{
#ifdef DXX_BUILD_DESCENT_II
	input_demo_capture_current_result_after_powerups_d2(result);
#else
	(void) result;
#endif
}

static int input_demo_robot_is_camera_awake(int objnum, const object *obj)
{
#ifdef DXX_BUILD_DESCENT_II
	if (d1_in_d2_use_d1_gameplay())
		return (Ai_local_info[objnum].player_awareness_type > 0);
	return ((obj->ctype.ai_info.SUB_FLAGS & SUB_FLAGS_CAMERA_AWAKE) != 0);
#else
	return (Ai_local_info[objnum].player_awareness_type > 0);
#endif
}

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

void input_demo_reset_object_state_diag_history(void)
{
	previous_robot_hashes_valid = 0;
	previous_robot_ai_static_hashes_valid = 0;
	previous_robot_anim_pose_hashes_valid = 0;
	previous_fireball_hashes_valid = 0;
	memset(previous_robot_ai_static_present, 0,
	       sizeof(previous_robot_ai_static_present));
	memset(previous_robot_anim_pose_present, 0,
	       sizeof(previous_robot_anim_pose_present));
}

unsigned int input_demo_state_trace_hash_update(unsigned int hash,
                                                unsigned int value)
{
	hash ^= value + 0x9e3779b9u + (hash << 6) + (hash >> 2);
	return hash;
}

unsigned int input_demo_state_trace_hash_i64(unsigned int hash,
                                             int64_t value)
{
	uint64_t bits = (uint64_t) value;

	hash = input_demo_state_trace_hash_update(hash, (unsigned int) bits);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) (bits >> 32));
	return hash;
}

static unsigned int input_demo_state_trace_hash_ai_static_fields(
    unsigned int hash, const ai_static *aip)
{
	int i;

	if (!aip)
		return hash;

	hash = input_demo_state_trace_hash_update(hash,
	                                          (unsigned int) aip->behavior);
	for (i = 0; i < MAX_AI_FLAGS; ++i)
		hash = input_demo_state_trace_hash_update(
		    hash, (unsigned int) aip->flags[i]);
	hash = input_demo_state_trace_hash_update(
	    hash, (unsigned int) aip->hide_segment);
	hash = input_demo_state_trace_hash_update(
	    hash, (unsigned int) aip->hide_index);
	hash = input_demo_state_trace_hash_update(hash,
	                                          (unsigned int) aip->path_length);
	hash = input_demo_state_trace_hash_update(hash,
	                                          (unsigned int) aip->cur_path_index);
#ifdef DXX_BUILD_DESCENT_II
	hash = input_demo_state_trace_hash_update(
	    hash, (unsigned int) aip->dying_sound_playing);
	hash = input_demo_state_trace_hash_update(
	    hash, (unsigned int) aip->danger_laser_num);
	hash = input_demo_state_trace_hash_update(
	    hash, (unsigned int) aip->danger_laser_signature);
	hash = input_demo_state_trace_hash_i64(hash, aip->dying_start_time);
#else
	hash = input_demo_state_trace_hash_update(
	    hash, (unsigned int) aip->follow_path_start_seg);
	hash = input_demo_state_trace_hash_update(
	    hash, (unsigned int) aip->follow_path_end_seg);
	hash = input_demo_state_trace_hash_update(
	    hash, (unsigned int) aip->danger_laser_signature);
	hash = input_demo_state_trace_hash_update(hash,
	                                          (unsigned int) aip->danger_laser_num);
#endif
	return hash;
}

static unsigned int input_demo_state_trace_hash_ai_static(
    unsigned int hash, const object *obj)
{
	if (!obj || obj->control_type != CT_AI)
		return hash;
	return input_demo_state_trace_hash_ai_static_fields(hash,
	                                                    &obj->ctype.ai_info);
}

static unsigned int input_demo_hash_ai_local_angles(unsigned int hash,
                                                    const vms_angvec *angles)
{
	int i;

	for (i = 0; i < MAX_SUBMODELS; ++i) {
		hash = input_demo_state_trace_hash_update(hash, (unsigned int) angles[i].p);
		hash = input_demo_state_trace_hash_update(hash, (unsigned int) angles[i].b);
		hash = input_demo_state_trace_hash_update(hash, (unsigned int) angles[i].h);
	}
	return hash;
}

static unsigned int input_demo_hash_angvec_range(const vms_angvec *angles,
                                                 int count)
{
	unsigned int hash = 0;
	int i;

	for (i = 0; i < count; ++i) {
		hash = input_demo_state_trace_hash_update(hash, (unsigned int) angles[i].p);
		hash = input_demo_state_trace_hash_update(hash, (unsigned int) angles[i].b);
		hash = input_demo_state_trace_hash_update(hash, (unsigned int) angles[i].h);
	}
	return hash;
}

static unsigned int input_demo_hash_sbyte_range(const sbyte *values, int count)
{
	unsigned int hash = 0;
	int i;

	for (i = 0; i < count; ++i)
		hash = input_demo_state_trace_hash_update(hash, (unsigned int) values[i]);
	return hash;
}

static int input_demo_robot_anim_state_from_ai_state(int ai_state)
{
	switch (ai_state) {
		case AIS_SRCH:
		case AIS_LOCK:
			return AS_ALERT;
		case AIS_FLIN:
			return AS_FLINCH;
		case AIS_FIRE:
			return AS_FIRE;
		case AIS_RECO:
			return AS_RECOIL;
		default:
			return AS_REST;
	}
}

static int input_demo_robot_sample_base_anim_at_goal(const object *obj)
{
	const jointpos *jp_list;
	int joint;
	int num_joint_positions;
	int robot_state;

	if (!obj || obj->id < 0 || obj->id >= N_robot_types)
		return -1;
	if (obj->render_type != RT_POLYOBJ)
		return -1;
	if (Robot_info[obj->id].n_guns == 0)
		return -1;
	robot_state = input_demo_robot_anim_state_from_ai_state(
	    obj->ctype.ai_info.GOAL_STATE);
	num_joint_positions = robot_get_anim_state(&jp_list, obj->id, 0, robot_state);
	for (joint = 0; joint < num_joint_positions; ++joint) {
		const int jointnum = jp_list[joint].jointnum;
		const vms_angvec *goal = &jp_list[joint].angles;
		const vms_angvec *pose;

		if (jointnum < 0 || jointnum >= MAX_SUBMODELS)
			continue;
		pose = &obj->rtype.pobj_info.anim_angles[jointnum];
		if (goal->p != pose->p || goal->b != pose->b || goal->h != pose->h)
			return 0;
	}
	return 1;
}

static unsigned int input_demo_hash_robot_ai_local(unsigned int hash,
                                                   int objnum,
                                                   const object *obj,
                                                   const ai_local *ailp)
{
	int i;

	hash = input_demo_state_trace_hash_update(hash, (unsigned int) objnum);
	hash = input_demo_state_trace_hash_update(hash,
	                                          (unsigned int) obj->signature);
	hash = input_demo_state_trace_hash_update(
	    hash, (unsigned int) ailp->player_awareness_type);
	hash = input_demo_state_trace_hash_update(hash,
	                                          (unsigned int) ailp->retry_count);
	hash = input_demo_state_trace_hash_update(
	    hash, (unsigned int) ailp->consecutive_retries);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) ailp->mode);
	hash = input_demo_state_trace_hash_update(
	    hash, (unsigned int) ailp->previous_visibility);
	hash = input_demo_state_trace_hash_update(
	    hash, (unsigned int) ailp->rapidfire_count);
	hash = input_demo_state_trace_hash_update(hash,
	                                          (unsigned int) ailp->goal_segment);
#ifdef DXX_BUILD_DESCENT_II
	hash = input_demo_state_trace_hash_update(
	    hash, (unsigned int) ailp->next_action_time);
	hash = input_demo_state_trace_hash_update(hash,
	                                          (unsigned int) ailp->next_fire);
	hash = input_demo_state_trace_hash_update(hash,
	                                          (unsigned int) ailp->next_fire2);
#else
	hash = input_demo_state_trace_hash_update(hash,
	                                          (unsigned int) ailp->last_see_time);
	hash = input_demo_state_trace_hash_update(
	    hash, (unsigned int) ailp->last_attack_time);
	hash = input_demo_state_trace_hash_update(hash,
	                                          (unsigned int) ailp->wait_time);
	hash = input_demo_state_trace_hash_update(hash,
	                                          (unsigned int) ailp->next_fire);
#endif
	hash = input_demo_state_trace_hash_update(
	    hash, (unsigned int) ailp->player_awareness_time);
	hash = input_demo_state_trace_hash_i64(hash, ailp->time_player_seen);
	hash = input_demo_state_trace_hash_i64(hash,
	                                       ailp->time_player_sound_attacked);
	hash = input_demo_state_trace_hash_i64(hash, ailp->next_misc_sound_time);
	hash = input_demo_state_trace_hash_update(
	    hash, (unsigned int) ailp->time_since_processed);
	hash = input_demo_hash_ai_local_angles(hash, ailp->goal_angles);
	hash = input_demo_hash_ai_local_angles(hash, ailp->delta_angles);
	for (i = 0; i < MAX_SUBMODELS; ++i) {
		hash = input_demo_state_trace_hash_update(
		    hash, (unsigned int) ailp->goal_state[i]);
		hash = input_demo_state_trace_hash_update(
		    hash, (unsigned int) ailp->achieved_state[i]);
	}
	return hash;
}

static unsigned int input_demo_hash_robot_anim_pose(unsigned int hash,
                                                    const object *obj)
{
	if (!obj || obj->render_type != RT_POLYOBJ)
		return hash;
	hash = input_demo_state_trace_hash_update(
	    hash, (unsigned int) obj->signature);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->id);
	hash = input_demo_state_trace_hash_update(
	    hash, (unsigned int) obj->rtype.pobj_info.model_num);
	hash = input_demo_state_trace_hash_update(
	    hash, (unsigned int) obj->rtype.pobj_info.subobj_flags);
	hash = input_demo_state_trace_hash_update(
	    hash, (unsigned int) obj->rtype.pobj_info.tmap_override);
	return input_demo_hash_ai_local_angles(hash,
	                                       obj->rtype.pobj_info.anim_angles);
}

void input_demo_capture_robot_ai_local_diag(input_demo_state_trace_diag *diag)
{
	int objnum;

	if (!diag)
		return;
	for (objnum = 0; objnum <= Highest_object_index; ++objnum) {
		object *obj = &Objects[objnum];
		ai_local *ailp;
		int i;

		if (obj->type != OBJ_ROBOT)
			continue;
		if (obj->flags & OF_SHOULD_BE_DEAD)
			continue;
		ailp = &Ai_local_info[objnum];
		diag->robot_ai_local_state_hash = input_demo_hash_robot_ai_local(
		    diag->robot_ai_local_state_hash, objnum, obj, ailp);
		i = objnum >> INPUT_DEMO_OBJECT_SLOT_BUCKET_BITS;
		if (i >= 0 && i < INPUT_DEMO_OBJECT_SLOT_BUCKET_COUNT) {
			diag->robot_ai_local_bucket_hashes[i] =
			    input_demo_hash_robot_ai_local(
			        diag->robot_ai_local_bucket_hashes[i], objnum, obj, ailp);
		}
	}
}

void input_demo_record_game_frame(void)
{
	input_demo_control_state state;
	input_demo_control_pulse pulse;
	input_demo_result frame_state;
	input_demo_state_trace_diag diag;
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
	if (input_demo_recorder_frame_count() == 0)
		input_demo_reset_object_state_diag_history();
	input_demo_capture_state_trace_diag(&diag);
	if (!input_demo_recorder_capture_frame((int32_t) FrameTime, &state, &pulse,
	                                       rng_state, 1, rng_call_count,
	                                       &frame_state, &diag,
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
		input_demo_rng_trace_set_context((uint32_t) input_demo_recorder_frame_count(), GameTime64);
		return;
	}
	if (input_demo_replay_is_loaded() &&
	    input_demo_replay_next_frame_index() < input_demo_replay_frame_count())
		input_demo_rng_trace_set_context(input_demo_replay_next_frame_index(), GameTime64);
}

void input_demo_delay_replay_frame_shared(fix frame_time,
                                          fix64 *replay_last_timer_value)
{
	fix64 timer_value;
	fix64 elapsed;

	if (!GameArg.SysUseNiceFPS)
		return;
	if (frame_time <= 0)
		return;
	timer_update();
	timer_value = timer_query();
	if (!replay_last_timer_value || !*replay_last_timer_value) {
		if (replay_last_timer_value)
			*replay_last_timer_value = timer_value;
		return;
	}
	elapsed = timer_value - *replay_last_timer_value;
	while (elapsed < frame_time) {
		timer_delay(frame_time - elapsed);
		timer_update();
		timer_value = timer_query();
		elapsed = timer_value - *replay_last_timer_value;
	}
	*replay_last_timer_value = timer_value;
}

int input_demo_prepare_replay_frame_shared(
    int *logged_state_mismatch,
    int *logged_state_trace_error,
    fix64 *replay_last_timer_value,
    void (*before_prepare_replay_frame)(void),
    void (*stop_replay)(int))
{
	input_demo_replay_frame replay_frame;
	char error[256] = "";

	if (!input_demo_replay_is_loaded())
		return 0;
	if (before_prepare_replay_frame)
		before_prepare_replay_frame();
	if (input_demo_replay_is_finished()) {
		if (stop_replay)
			stop_replay(1);
		return 0;
	}
	if (!input_demo_replay_get_current_frame(&replay_frame, error, sizeof(error))) {
		con_printf(CON_NORMAL, "Input demo replay stopped: %s\n", error);
		if (stop_replay)
			stop_replay(0);
		return 0;
	}
	if (!input_demo_replay_next_frame_index()) {
		if (logged_state_mismatch)
			*logged_state_mismatch = 0;
		if (logged_state_trace_error)
			*logged_state_trace_error = 0;
	}
	input_demo_delay_replay_frame_shared((fix) replay_frame.frame_time,
	                                     replay_last_timer_value);
	input_demo_control_info_from_state(&Controls, &replay_frame.state,
	                                   &replay_frame.pulse);
	FrameTime = (fix) replay_frame.frame_time;
	return 1;
}

void input_demo_stop_replay_shared(int write_result,
                                   fix64 *replay_last_timer_value,
                                   void (*write_replay_result)(void),
                                   void (*before_stop_replay)(void))
{
	if (replay_last_timer_value)
		*replay_last_timer_value = 0;
	if (before_stop_replay && input_demo_replay_is_loaded())
		before_stop_replay();
	if (write_result && write_replay_result)
		write_replay_result();
	input_demo_replay_unload();
	if (Game_wind)
		window_close(Game_wind);
}

int input_demo_finish_replay_shared(int close_window,
                                    fix64 *replay_last_timer_value,
                                    void (*before_write_replay_result)(void),
                                    void (*write_replay_result)(void))
{
	if (replay_last_timer_value)
		*replay_last_timer_value = 0;
	if (!input_demo_replay_is_loaded())
		return 0;
	if (before_write_replay_result)
		before_write_replay_result();
	if (write_replay_result)
		write_replay_result();
	input_demo_replay_unload();
	if (close_window && Game_wind)
		window_close(Game_wind);
	return 1;
}

int input_demo_sync_replay_rng_to_current_frame_shared(
    void (*before_sync_replay_rng)(const input_demo_replay_frame *replay_frame),
    void (*stop_replay)(int))
{
	input_demo_replay_frame replay_frame;
	char error[256] = "";

	if (!input_demo_replay_is_loaded())
		return 0;
	if (!input_demo_replay_get_current_frame(&replay_frame, error, sizeof(error))) {
		con_printf(CON_NORMAL, "Input demo replay stopped: %s\n", error);
		if (stop_replay)
			stop_replay(0);
		return 0;
	}
	if (before_sync_replay_rng)
		before_sync_replay_rng(&replay_frame);
	if (!d_rand_set_state(replay_frame.rng_state)) {
		con_printf(CON_NORMAL,
		           "Input demo replay stopped: active RNG backend cannot restore state\n");
		if (stop_replay)
			stop_replay(0);
		return 0;
	}
	if (input_demo_rng_trace_is_active() && replay_frame.has_rng_call_count)
		d_rand_set_call_count(replay_frame.rng_call_count);
	else
		d_rand_reset_call_count();
	return 1;
}

int input_demo_step_replay_frame_shared(
    int (*prepare_replay_frame)(void),
    int (*sync_replay_rng_to_current_frame)(void))
{
	int read_controls_result;

	input_demo_restore_replay_fp_environment();
	if (!prepare_replay_frame || !prepare_replay_frame())
		return 0;
	read_controls_result = ReadControlsReplayFrame();
	if (!sync_replay_rng_to_current_frame || !sync_replay_rng_to_current_frame())
		return 0;
	if (read_controls_result)
		return 1;
	if (!game_is_time_paused()) {
		calc_game_time();
		GameProcessFrame();
		ReadControlsReplayPostFrame();
		input_demo_advance_replay_frame();
	}
	return 1;
}

void input_demo_advance_replay_frame_shared(
    int (*before_advance_replay_frame)(void),
    void (*stop_replay)(int))
{
	char error[256] = "";

	if (!input_demo_replay_is_loaded())
		return;
	if (before_advance_replay_frame && !before_advance_replay_frame())
		return;
	if (!input_demo_replay_advance_frame(error, sizeof(error))) {
		con_printf(CON_NORMAL, "Input demo replay stopped: %s\n", error);
		if (stop_replay)
			stop_replay(0);
		return;
	}
	if (input_demo_replay_is_finished() && stop_replay)
		stop_replay(1);
}

void input_demo_write_replay_frame_state_trace_shared(
    const input_demo_replay_frame *replay_frame,
    int *logged_state_trace_error)
{
	input_demo_result actual_state;
	input_demo_state_trace_diag diag;
	char error[256] = "";

	if (!replay_frame || !input_demo_state_trace_is_active())
		return;
	input_demo_capture_current_result(&actual_state);
	if (replay_frame->frame == 0)
		input_demo_reset_object_state_diag_history();
	input_demo_capture_state_trace_diag(&diag);
	if (input_demo_state_trace_write_frame(replay_frame->frame,
	                                       replay_frame->frame_time,
	                                       replay_frame->rng_state,
	                                       replay_frame->has_rng_call_count,
	                                       replay_frame->rng_call_count,
	                                       &diag,
	                                       &actual_state,
	                                       error,
	                                       sizeof(error)))
		return;
	if (!logged_state_trace_error || !*logged_state_trace_error)
		con_printf(CON_NORMAL, "Input demo replay state trace write failed: %s\n", error);
	if (logged_state_trace_error)
		*logged_state_trace_error = 1;
	input_demo_state_trace_stop();
}

void input_demo_log_replay_frame_state_mismatch_shared(
    const input_demo_replay_frame *replay_frame,
    int *logged_state_mismatch)
{
	input_demo_result actual_state;
	char error[256] = "";
	char expected_json[4096] = "";
	char actual_json[4096] = "";

	if (!replay_frame || !replay_frame->has_state)
		return;
	if (logged_state_mismatch && *logged_state_mismatch)
		return;
	input_demo_capture_current_result(&actual_state);
	if (input_demo_result_compare_snapshot(&replay_frame->state_result,
	                                       &actual_state,
	                                       error,
	                                       sizeof(error)))
		return;
	if (logged_state_mismatch)
		*logged_state_mismatch = 1;
	if (!input_demo_result_snapshot_to_json_buffer(&replay_frame->state_result,
	                                               expected_json,
	                                               sizeof(expected_json)))
		snprintf(expected_json, sizeof(expected_json), "<snapshot encode failed>");
	if (!input_demo_result_snapshot_to_json_buffer(&actual_state,
	                                               actual_json,
	                                               sizeof(actual_json)))
		snprintf(actual_json, sizeof(actual_json), "<snapshot encode failed>");
	con_printf(CON_NORMAL,
	           "Input demo replay frame state mismatch: frame=%u gt=%lld %s\n",
	           (unsigned int) replay_frame->frame,
	           (long long) GameTime64,
	           error);
	input_demo_debug_printf("Input demo replay expected state: %s\n", expected_json);
	input_demo_debug_printf("Input demo replay actual state: %s\n", actual_json);
}

void input_demo_capture_runtime_state_diag(input_demo_state_trace_diag *diag)
{
	object_runtime_state object_state;
	laser_runtime_state laser_state;
	unsigned int runtime_hash = 0;
	int free_start;

	if (!diag)
		return;

	object_get_runtime_state(&object_state);
	laser_get_runtime_state(&laser_state);
	free_start = object_state.num_objects;
	diag->object_allocator_num_objects = object_state.num_objects;
	diag->object_signature_seed = object_state.signature_seed;
	diag->object_free_head0 = -1;
	diag->object_free_head1 = -1;
	diag->object_free_head2 = -1;
	diag->object_free_head3 = -1;
	diag->object_homer_frame_count = object_state.homer_frame_count;
	diag->object_current_homer_frame_time = object_state.current_homer_frame_time;
	diag->object_do_homer_frame = object_state.do_homer_frame;

	if (free_start >= 0 && free_start <= MAX_OBJECTS) {
		int free_index;

		diag->object_free_list_count = MAX_OBJECTS - free_start;
		for (free_index = free_start; free_index < MAX_OBJECTS; ++free_index) {
			const int head_index = free_index - free_start;
			const short free_slot = object_state.free_obj_list[free_index];

			diag->object_free_list_hash = input_demo_state_trace_hash_update(
			    diag->object_free_list_hash, (unsigned int) (unsigned short) free_slot);
			if (head_index == 0)
				diag->object_free_head0 = free_slot;
			else if (head_index == 1)
				diag->object_free_head1 = free_slot;
			else if (head_index == 2)
				diag->object_free_head2 = free_slot;
			else if (head_index == 3)
				diag->object_free_head3 = free_slot;
		}
	} else
		diag->object_free_list_count = -1;

	diag->weapon_next_laser_delta = (int64_t) (Next_laser_fire_time - GameTime64);
	diag->weapon_next_missile_delta = (int64_t) (Next_missile_fire_time - GameTime64);
	diag->weapon_last_laser_delta = (int64_t) (Last_laser_fired_time - GameTime64);
	diag->weapon_next_flare_delta = (int64_t) (Next_flare_fire_time - GameTime64);
	diag->weapon_auto_fusion_delta = (int64_t) (Auto_fire_fusion_cannon_time - GameTime64);
	diag->weapon_last_omega_delta = (int64_t) laser_state.last_omega_fire_time - GameTime64;
	diag->weapon_global_laser_firing_count = Global_laser_firing_count;
	diag->weapon_global_missile_firing_count = Global_missile_firing_count;
	diag->weapon_fusion_charge = laser_state.fusion_charge;
	diag->weapon_spreadfire_toggle = laser_state.spreadfire_toggle;
	diag->weapon_missile_gun = laser_state.missile_gun;
	diag->weapon_proximity_dropped = laser_state.proximity_dropped;
	diag->weapon_helix_orientation = laser_state.helix_orientation;
	diag->weapon_smartmines_dropped = laser_state.smartmines_dropped;

	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->object_allocator_num_objects);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) object_state.highest_object_index);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->object_signature_seed);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->object_free_list_count);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  diag->object_free_list_hash);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  diag->object_homer_frame_count);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->object_current_homer_frame_time);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->object_do_homer_frame);
	runtime_hash = input_demo_state_trace_hash_i64(runtime_hash,
	                                               diag->weapon_next_laser_delta);
	runtime_hash = input_demo_state_trace_hash_i64(runtime_hash,
	                                               diag->weapon_next_missile_delta);
	runtime_hash = input_demo_state_trace_hash_i64(runtime_hash,
	                                               diag->weapon_last_laser_delta);
	runtime_hash = input_demo_state_trace_hash_i64(runtime_hash,
	                                               diag->weapon_next_flare_delta);
	runtime_hash = input_demo_state_trace_hash_i64(runtime_hash,
	                                               diag->weapon_auto_fusion_delta);
	runtime_hash = input_demo_state_trace_hash_i64(runtime_hash,
	                                               diag->weapon_last_omega_delta);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->weapon_global_laser_firing_count);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->weapon_global_missile_firing_count);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->weapon_fusion_charge);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->weapon_spreadfire_toggle);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->weapon_missile_gun);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->weapon_proximity_dropped);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->weapon_helix_orientation);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->weapon_smartmines_dropped);
	diag->runtime_state_hash = runtime_hash;
}

static void input_demo_set_player_weapon_diag_slot(input_demo_state_trace_diag *diag,
                                                   int slot, int objnum, const object *obj)
{
	if (!diag || !obj)
		return;

	switch (slot) {
		case 0:
			diag->player_weapon_obj0 = objnum;
			diag->player_weapon_sig0 = obj->signature;
			diag->player_weapon_id0 = obj->id;
			break;
		case 1:
			diag->player_weapon_obj1 = objnum;
			diag->player_weapon_sig1 = obj->signature;
			diag->player_weapon_id1 = obj->id;
			break;
		case 2:
			diag->player_weapon_obj2 = objnum;
			diag->player_weapon_sig2 = obj->signature;
			diag->player_weapon_id2 = obj->id;
			break;
		case 3:
			diag->player_weapon_obj3 = objnum;
			diag->player_weapon_sig3 = obj->signature;
			diag->player_weapon_id3 = obj->id;
			break;
	}
}

void input_demo_capture_player_weapon_diag(input_demo_state_trace_diag *diag)
{
	int i;
	int player_objnum;

	if (!diag)
		return;

	diag->player_weapon_obj0 = -1;
	diag->player_weapon_sig0 = -1;
	diag->player_weapon_id0 = -1;
	diag->player_weapon_obj1 = -1;
	diag->player_weapon_sig1 = -1;
	diag->player_weapon_id1 = -1;
	diag->player_weapon_obj2 = -1;
	diag->player_weapon_sig2 = -1;
	diag->player_weapon_id2 = -1;
	diag->player_weapon_obj3 = -1;
	diag->player_weapon_sig3 = -1;
	diag->player_weapon_id3 = -1;
	player_objnum = Players[Player_num].objnum;

	for (i = 0; i <= Highest_object_index; ++i) {
		object *obj = &Objects[i];

		if (obj->type != OBJ_WEAPON)
			continue;
		if (obj->flags & OF_SHOULD_BE_DEAD)
			continue;
		if (obj->ctype.laser_info.parent_type != OBJ_PLAYER)
			continue;
		if (obj->ctype.laser_info.parent_num != player_objnum)
			continue;

		diag->player_weapon_hash = input_demo_state_trace_hash_update(
		    diag->player_weapon_hash, (unsigned int) i);
		diag->player_weapon_hash = input_demo_state_trace_hash_update(
		    diag->player_weapon_hash, (unsigned int) obj->signature);
		diag->player_weapon_hash = input_demo_state_trace_hash_update(
		    diag->player_weapon_hash, (unsigned int) obj->id);
		diag->player_weapon_hash = input_demo_state_trace_hash_update(
		    diag->player_weapon_hash, (unsigned int) obj->segnum);
		diag->player_weapon_hash = input_demo_state_trace_hash_update(
		    diag->player_weapon_hash,
		    (unsigned int) obj->ctype.laser_info.parent_signature);
		if (diag->player_weapon_count < 4)
			input_demo_set_player_weapon_diag_slot(diag,
			                                       diag->player_weapon_count, i, obj);
		diag->player_weapon_count++;
	}
}

static unsigned int input_demo_state_trace_hash_object(unsigned int hash,
                                                       const object *obj)
{
	if (!obj)
		return hash;

	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->signature);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->type);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->id);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->segnum);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->control_type);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->movement_type);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->render_type);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->flags);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->size);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->shields);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->lifeleft);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->attached_obj);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->pos.x);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->pos.y);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->pos.z);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->last_pos.x);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->last_pos.y);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->last_pos.z);

	if (obj->movement_type == MT_PHYSICS) {
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.velocity.x);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.velocity.y);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.velocity.z);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.rotvel.x);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.rotvel.y);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.rotvel.z);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.mass);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.drag);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.brakes);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.turnroll);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.flags);
	}

	if (obj->type == OBJ_WEAPON) {
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->ctype.laser_info.parent_type);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->ctype.laser_info.parent_num);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->ctype.laser_info.parent_signature);
	}

	if (obj->render_type == RT_POLYOBJ) {
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->rtype.pobj_info.model_num);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->rtype.pobj_info.subobj_flags);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->rtype.pobj_info.tmap_override);
	}

	return hash;
}

void input_demo_capture_object_state_diag(input_demo_state_trace_diag *diag)
{
	const int robot_sample_obj = 15;
	const int fireball_sample_obj = 174;
	int i;
	int segnum;
	int fireball_trace_count;

	if (!diag)
		return;
	diag->highest_object_index = Highest_object_index;
	diag->object_slot_bucket_size = INPUT_DEMO_OBJECT_SLOT_BUCKET_SIZE;
	diag->object_focus_slot_base = 0;
	diag->robot_changed_obj = -1;
	diag->robot_changed_sig = -1;
	diag->robot_changed_id = -1;
	diag->robot_changed_bucket = -1;
	diag->robot_changed_model = -1;
	diag->robot_ai_static_changed_obj = -1;
	diag->robot_ai_static_changed_sig = -1;
	diag->robot_ai_static_changed_id = -1;
	diag->robot_anim_pose_changed_obj = -1;
	diag->robot_anim_pose_changed_sig = -1;
	diag->robot_anim_pose_changed_id = -1;
	diag->robot_anim_pose_changed_model = -1;
	diag->robot_anim_pose_changed_subobj_flags = -1;
	diag->fireball_changed_obj = -1;
	diag->fireball_changed_sig = -1;
	diag->fireball_changed_id = -1;
	diag->fireball_changed_bucket = -1;
	diag->fireball_sample_obj = -1;
	diag->fireball_sample_sig = -1;
	diag->fireball_sample_id = -1;
	diag->fireball_sample_seg = -1;
	diag->fireball_sample_delete_objnum = -1;
	diag->fireball_sample_attach_parent = -1;
	diag->fireball_sample_prev_attach = -1;
	diag->fireball_sample_next_attach = -1;
	for (i = 0; i < INPUT_DEMO_FIREBALL_TRACE_COUNT; ++i) {
		diag->fireball_trace_slots[i] = -1;
		diag->fireball_trace_sigs[i] = -1;
		diag->fireball_trace_ids[i] = -1;
		diag->fireball_trace_segs[i] = -1;
		diag->fireball_trace_lifeleft[i] = -1;
		diag->fireball_trace_delete_objnums[i] = -1;
		diag->fireball_trace_attached_objs[i] = -1;
	}
	diag->robot_sample_obj = -1;
	diag->robot_sample_sig = -1;
	diag->robot_sample_id = -1;
	diag->weapon_sample_obj = -1;
	diag->weapon_sample_sig = -1;
	diag->weapon_sample_id = -1;
	diag->weapon_sample_seg = -1;
	diag->weapon_sample_parent_type = -1;
	diag->weapon_sample_parent_num = -1;
	diag->weapon_sample_parent_sig = -1;
	fireball_trace_count = 0;

	for (i = 0; i <= Highest_object_index; ++i) {
		object *obj = &Objects[i];
		int bucket;

		if (obj->type == OBJ_NONE)
			continue;
		if (obj->flags & OF_SHOULD_BE_DEAD)
			continue;

		bucket = i >> INPUT_DEMO_OBJECT_SLOT_BUCKET_BITS;
		if (bucket >= 0 && bucket < INPUT_DEMO_OBJECT_SLOT_BUCKET_COUNT) {
			unsigned int object_hash = input_demo_state_trace_hash_object(0, obj);

			diag->object_slot_counts[bucket]++;
			diag->object_slot_hashes[bucket] = input_demo_state_trace_hash_update(
			    diag->object_slot_hashes[bucket], (unsigned int) i);
			diag->object_slot_hashes[bucket] = input_demo_state_trace_hash_object(
			    diag->object_slot_hashes[bucket], obj);
			if (i >= diag->object_focus_slot_base &&
			    i < diag->object_focus_slot_base + INPUT_DEMO_OBJECT_SLOT_BUCKET_SIZE)
				diag->object_focus_slot_hashes[i - diag->object_focus_slot_base] =
				    object_hash;
		}

		diag->live_object_count++;
		diag->live_object_hash = input_demo_state_trace_hash_object(
		    diag->live_object_hash, obj);

		switch (obj->type) {
			case OBJ_ROBOT: {
				unsigned int object_hash =
				    input_demo_state_trace_hash_object(0, obj);
				unsigned int ai_static_hash = input_demo_state_trace_hash_update(
				    0, (unsigned int) i);
				unsigned int anim_pose_hash =
				    input_demo_state_trace_hash_update(0, (unsigned int) i);

				if (i == robot_sample_obj) {
					ai_local *ailp = &Ai_local_info[i];

					diag->robot_sample_obj = i;
					diag->robot_sample_sig = obj->signature;
					diag->robot_sample_id = obj->id;
					diag->robot_sample_seg = obj->segnum;
					diag->robot_sample_model = obj->rtype.pobj_info.model_num;
					diag->robot_sample_subobj_flags = obj->rtype.pobj_info.subobj_flags;
					diag->robot_sample_behavior = obj->ctype.ai_info.behavior;
					diag->robot_sample_mode = ailp->mode;
					diag->robot_sample_cur_state = obj->ctype.ai_info.CURRENT_STATE;
					diag->robot_sample_goal_state = obj->ctype.ai_info.GOAL_STATE;
					diag->robot_sample_anim_at_goal =
					    input_demo_robot_sample_base_anim_at_goal(obj);
					diag->robot_sample_anim_angles_hash =
					    input_demo_hash_angvec_range(
					        obj->rtype.pobj_info.anim_angles, MAX_SUBMODELS);
					diag->robot_sample_goal_angles_hash =
					    input_demo_hash_angvec_range(ailp->goal_angles, MAX_SUBMODELS);
					diag->robot_sample_delta_angles_hash =
					    input_demo_hash_angvec_range(ailp->delta_angles, MAX_SUBMODELS);
					diag->robot_sample_goal_state_hash =
					    input_demo_hash_sbyte_range(ailp->goal_state, MAX_SUBMODELS);
					diag->robot_sample_achieved_state_hash =
					    input_demo_hash_sbyte_range(ailp->achieved_state, MAX_SUBMODELS);
					diag->robot_sample_goal_seg = ailp->goal_segment;
					diag->robot_sample_hide_index = obj->ctype.ai_info.hide_index;
					diag->robot_sample_path_dir = obj->ctype.ai_info.PATH_DIR;
					diag->robot_sample_prev_vis = ailp->previous_visibility;
					diag->robot_sample_aware = ailp->player_awareness_type;
					diag->robot_sample_aware_time = ailp->player_awareness_time;
					diag->robot_sample_since = ailp->time_since_processed;
					diag->robot_sample_retry = ailp->retry_count;
					diag->robot_sample_retry_chain = ailp->consecutive_retries;
#ifdef DXX_BUILD_DESCENT_II
					diag->robot_sample_next_action = ailp->next_action_time;
#else
					diag->robot_sample_next_action = ailp->wait_time;
#endif
					diag->robot_sample_path_index = obj->ctype.ai_info.cur_path_index;
					diag->robot_sample_path_length = obj->ctype.ai_info.path_length;
					diag->robot_sample_phys_flags = obj->mtype.phys_info.flags;
					diag->robot_sample_vel_x = obj->mtype.phys_info.velocity.x;
					diag->robot_sample_vel_y = obj->mtype.phys_info.velocity.y;
					diag->robot_sample_vel_z = obj->mtype.phys_info.velocity.z;
					diag->robot_sample_pos_x = obj->pos.x;
					diag->robot_sample_pos_y = obj->pos.y;
					diag->robot_sample_pos_z = obj->pos.z;
					if (obj->ctype.ai_info.hide_index >= 0 &&
					    obj->ctype.ai_info.cur_path_index >= 0 &&
					    obj->ctype.ai_info.path_length > 0) {
						const int point_index =
						    obj->ctype.ai_info.hide_index +
						    obj->ctype.ai_info.cur_path_index;
						if (point_index >= 0 &&
						    point_index < Point_segs_free_ptr - Point_segs) {
							const vms_vector *goal_point =
							    &Point_segs[point_index].point;
							diag->robot_sample_goal_x = goal_point->x;
							diag->robot_sample_goal_y = goal_point->y;
							diag->robot_sample_goal_z = goal_point->z;
						}
						if (obj->ctype.ai_info.cur_path_index + 1 <
						    obj->ctype.ai_info.path_length) {
							const int next_point_index = point_index + 1;
							if (next_point_index >= 0 &&
							    next_point_index <
							        Point_segs_free_ptr - Point_segs) {
								const vms_vector *next_goal_point =
								    &Point_segs[next_point_index].point;
								diag->robot_sample_next_goal_x =
								    next_goal_point->x;
								diag->robot_sample_next_goal_y =
								    next_goal_point->y;
								diag->robot_sample_next_goal_z =
								    next_goal_point->z;
							}
						}
					}
					diag->robot_sample_mass = obj->mtype.phys_info.mass;
					diag->robot_sample_drag = obj->mtype.phys_info.drag;
					diag->robot_sample_brakes = obj->mtype.phys_info.brakes;
					diag->robot_sample_fvec_x = obj->orient.fvec.x;
					diag->robot_sample_fvec_y = obj->orient.fvec.y;
					diag->robot_sample_fvec_z = obj->orient.fvec.z;
					diag->robot_sample_rvec_x = obj->orient.rvec.x;
					diag->robot_sample_rvec_y = obj->orient.rvec.y;
					diag->robot_sample_rvec_z = obj->orient.rvec.z;
					diag->robot_sample_uvec_x = obj->orient.uvec.x;
					diag->robot_sample_uvec_y = obj->orient.uvec.y;
					diag->robot_sample_uvec_z = obj->orient.uvec.z;
					diag->robot_sample_orient_hash =
					    input_demo_state_trace_hash_update(
					        diag->robot_sample_orient_hash,
					        (unsigned int) obj->orient.rvec.x);
					diag->robot_sample_orient_hash =
					    input_demo_state_trace_hash_update(
					        diag->robot_sample_orient_hash,
					        (unsigned int) obj->orient.rvec.y);
					diag->robot_sample_orient_hash =
					    input_demo_state_trace_hash_update(
					        diag->robot_sample_orient_hash,
					        (unsigned int) obj->orient.rvec.z);
					diag->robot_sample_orient_hash =
					    input_demo_state_trace_hash_update(
					        diag->robot_sample_orient_hash,
					        (unsigned int) obj->orient.uvec.x);
					diag->robot_sample_orient_hash =
					    input_demo_state_trace_hash_update(
					        diag->robot_sample_orient_hash,
					        (unsigned int) obj->orient.uvec.y);
					diag->robot_sample_orient_hash =
					    input_demo_state_trace_hash_update(
					        diag->robot_sample_orient_hash,
					        (unsigned int) obj->orient.uvec.z);
					diag->robot_sample_orient_hash =
					    input_demo_state_trace_hash_update(
					        diag->robot_sample_orient_hash,
					        (unsigned int) obj->orient.fvec.x);
					diag->robot_sample_orient_hash =
					    input_demo_state_trace_hash_update(
					        diag->robot_sample_orient_hash,
					        (unsigned int) obj->orient.fvec.y);
					diag->robot_sample_orient_hash =
					    input_demo_state_trace_hash_update(
					        diag->robot_sample_orient_hash,
					        (unsigned int) obj->orient.fvec.z);
					diag->robot_sample_rotthrust_x = obj->mtype.phys_info.rotthrust.x;
					diag->robot_sample_rotthrust_y = obj->mtype.phys_info.rotthrust.y;
					diag->robot_sample_rotthrust_z = obj->mtype.phys_info.rotthrust.z;
					diag->robot_sample_rotvel_x = obj->mtype.phys_info.rotvel.x;
					diag->robot_sample_rotvel_y = obj->mtype.phys_info.rotvel.y;
					diag->robot_sample_rotvel_z = obj->mtype.phys_info.rotvel.z;
				}
				diag->robot_object_count++;
				diag->robot_state_hash =
				    input_demo_state_trace_hash_object(diag->robot_state_hash, obj);
				if (bucket >= 0 && bucket < INPUT_DEMO_OBJECT_SLOT_BUCKET_COUNT) {
					diag->robot_object_bucket_hashes[bucket] =
					    input_demo_state_trace_hash_update(
					        diag->robot_object_bucket_hashes[bucket],
					        (unsigned int) i);
					diag->robot_object_bucket_hashes[bucket] =
					    input_demo_state_trace_hash_object(
					        diag->robot_object_bucket_hashes[bucket], obj);
				}
				if (previous_robot_hashes_valid &&
				    diag->robot_changed_obj < 0 &&
				    previous_robot_hashes[i] != object_hash) {
					diag->robot_changed_obj = i;
					diag->robot_changed_sig = obj->signature;
					diag->robot_changed_id = obj->id;
					diag->robot_changed_bucket =
					    i >> INPUT_DEMO_OBJECT_SLOT_BUCKET_BITS;
					diag->robot_changed_prev_hash = previous_robot_hashes[i];
					diag->robot_changed_hash = object_hash;
					diag->robot_changed_type = obj->type;
					diag->robot_changed_seg = obj->segnum;
					diag->robot_changed_control = obj->control_type;
					diag->robot_changed_movement = obj->movement_type;
					diag->robot_changed_render = obj->render_type;
					diag->robot_changed_flags = obj->flags;
					diag->robot_changed_x = obj->pos.x;
					diag->robot_changed_y = obj->pos.y;
					diag->robot_changed_z = obj->pos.z;
					diag->robot_changed_last_x = obj->last_pos.x;
					diag->robot_changed_last_y = obj->last_pos.y;
					diag->robot_changed_last_z = obj->last_pos.z;
					if (obj->movement_type == MT_PHYSICS) {
						diag->robot_changed_vel_x = obj->mtype.phys_info.velocity.x;
						diag->robot_changed_vel_y = obj->mtype.phys_info.velocity.y;
						diag->robot_changed_vel_z = obj->mtype.phys_info.velocity.z;
						diag->robot_changed_rotvel_x = obj->mtype.phys_info.rotvel.x;
						diag->robot_changed_rotvel_y = obj->mtype.phys_info.rotvel.y;
						diag->robot_changed_rotvel_z = obj->mtype.phys_info.rotvel.z;
					}
					if (obj->render_type == RT_POLYOBJ) {
						diag->robot_changed_model = obj->rtype.pobj_info.model_num;
						diag->robot_changed_subobj_flags =
						    obj->rtype.pobj_info.subobj_flags;
					}
				}
				previous_robot_hashes[i] = object_hash;
				ai_static_hash =
				    input_demo_state_trace_hash_ai_static(ai_static_hash, obj);
				if (bucket >= 0 && bucket < INPUT_DEMO_OBJECT_SLOT_BUCKET_COUNT) {
					diag->robot_ai_static_bucket_hashes[bucket] =
					    input_demo_state_trace_hash_update(
					        diag->robot_ai_static_bucket_hashes[bucket],
					        (unsigned int) i);
					diag->robot_ai_static_bucket_hashes[bucket] =
					    input_demo_state_trace_hash_ai_static(
					        diag->robot_ai_static_bucket_hashes[bucket], obj);
				}
				if (previous_robot_ai_static_hashes_valid &&
				    previous_robot_ai_static_present[i] &&
				    diag->robot_ai_static_changed_obj < 0 &&
				    previous_robot_ai_static_hashes[i] != ai_static_hash) {
					ai_static *aip = &obj->ctype.ai_info;

					diag->robot_ai_static_changed_obj = i;
					diag->robot_ai_static_changed_sig = obj->signature;
					diag->robot_ai_static_changed_id = obj->id;
					diag->robot_ai_static_changed_prev_hash =
					    previous_robot_ai_static_hashes[i];
					diag->robot_ai_static_changed_hash = ai_static_hash;
					diag->robot_ai_static_changed_behavior = aip->behavior;
					diag->robot_ai_static_changed_flags_hash =
					    input_demo_hash_sbyte_range(aip->flags, MAX_AI_FLAGS);
					diag->robot_ai_static_changed_current_gun =
					    aip->CURRENT_GUN;
					diag->robot_ai_static_changed_current_state =
					    aip->CURRENT_STATE;
					diag->robot_ai_static_changed_goal_state =
					    aip->GOAL_STATE;
					diag->robot_ai_static_changed_path_dir = aip->PATH_DIR;
#ifdef DXX_BUILD_DESCENT_II
					diag->robot_ai_static_changed_submode = aip->SUB_FLAGS;
#else
					diag->robot_ai_static_changed_submode = aip->SUBMODE;
#endif
					diag->robot_ai_static_changed_goalside = aip->GOALSIDE;
					diag->robot_ai_static_changed_skip_ai_count =
					    aip->SKIP_AI_COUNT;
					diag->robot_ai_static_changed_hide_segment =
					    aip->hide_segment;
					diag->robot_ai_static_changed_hide_index = aip->hide_index;
					diag->robot_ai_static_changed_path_length =
					    aip->path_length;
					diag->robot_ai_static_changed_cur_path_index =
					    aip->cur_path_index;
#ifdef DXX_BUILD_DESCENT_II
					diag->robot_ai_static_changed_follow_start = -1;
					diag->robot_ai_static_changed_follow_end = -1;
#else
					diag->robot_ai_static_changed_follow_start =
					    aip->follow_path_start_seg;
					diag->robot_ai_static_changed_follow_end =
					    aip->follow_path_end_seg;
#endif
					diag->robot_ai_static_changed_danger_laser_num =
					    aip->danger_laser_num;
					diag->robot_ai_static_changed_danger_laser_sig =
					    aip->danger_laser_signature;
				}
				diag->robot_ai_static_state_hash =
				    input_demo_state_trace_hash_update(
				        diag->robot_ai_static_state_hash, (unsigned int) i);
				diag->robot_ai_static_state_hash =
				    input_demo_state_trace_hash_ai_static(
				        diag->robot_ai_static_state_hash, obj);
				anim_pose_hash =
				    input_demo_hash_robot_anim_pose(anim_pose_hash, obj);
				diag->robot_anim_pose_state_hash =
				    input_demo_state_trace_hash_update(
				        diag->robot_anim_pose_state_hash, (unsigned int) i);
				diag->robot_anim_pose_state_hash =
				    input_demo_hash_robot_anim_pose(
				        diag->robot_anim_pose_state_hash, obj);
				if (bucket >= 0 && bucket < INPUT_DEMO_OBJECT_SLOT_BUCKET_COUNT) {
					diag->robot_anim_pose_bucket_hashes[bucket] =
					    input_demo_state_trace_hash_update(
					        diag->robot_anim_pose_bucket_hashes[bucket],
					        (unsigned int) i);
					diag->robot_anim_pose_bucket_hashes[bucket] =
					    input_demo_hash_robot_anim_pose(
					        diag->robot_anim_pose_bucket_hashes[bucket], obj);
				}
				if (previous_robot_anim_pose_hashes_valid &&
				    previous_robot_anim_pose_present[i] &&
				    diag->robot_anim_pose_changed_obj < 0 &&
				    previous_robot_anim_pose_hashes[i] != anim_pose_hash) {
					ai_local *ailp = &Ai_local_info[i];

					diag->robot_anim_pose_changed_obj = i;
					diag->robot_anim_pose_changed_sig = obj->signature;
					diag->robot_anim_pose_changed_id = obj->id;
					diag->robot_anim_pose_changed_prev_hash =
					    previous_robot_anim_pose_hashes[i];
					diag->robot_anim_pose_changed_hash = anim_pose_hash;
					if (obj->render_type == RT_POLYOBJ) {
						diag->robot_anim_pose_changed_model =
						    obj->rtype.pobj_info.model_num;
						diag->robot_anim_pose_changed_subobj_flags =
						    obj->rtype.pobj_info.subobj_flags;
						diag->robot_anim_pose_changed_anim_angles_hash =
						    input_demo_hash_angvec_range(
						        obj->rtype.pobj_info.anim_angles,
						        MAX_SUBMODELS);
					}
					diag->robot_anim_pose_changed_goal_angles_hash =
					    input_demo_hash_angvec_range(ailp->goal_angles,
					                                 MAX_SUBMODELS);
					diag->robot_anim_pose_changed_delta_angles_hash =
					    input_demo_hash_angvec_range(ailp->delta_angles,
					                                 MAX_SUBMODELS);
					diag->robot_anim_pose_changed_goal_state_hash =
					    input_demo_hash_sbyte_range(ailp->goal_state,
					                                MAX_SUBMODELS);
					diag->robot_anim_pose_changed_achieved_state_hash =
					    input_demo_hash_sbyte_range(ailp->achieved_state,
					                                MAX_SUBMODELS);
					diag->robot_anim_pose_changed_current_gun =
					    obj->ctype.ai_info.CURRENT_GUN;
					diag->robot_anim_pose_changed_current_state =
					    obj->ctype.ai_info.CURRENT_STATE;
					diag->robot_anim_pose_changed_goal_state =
					    obj->ctype.ai_info.GOAL_STATE;
				}
				if (input_demo_robot_is_camera_awake(i, obj))
					diag->camera_awake_robots++;
				if (obj->ctype.ai_info.danger_laser_num != -1)
					diag->danger_laser_robots++;
				break;
			}
			case OBJ_WEAPON:
				if (diag->weapon_object_count == 0) {
					diag->weapon_sample_obj = i;
					diag->weapon_sample_sig = obj->signature;
					diag->weapon_sample_id = obj->id;
					diag->weapon_sample_seg = obj->segnum;
					diag->weapon_sample_control = obj->control_type;
					diag->weapon_sample_movement = obj->movement_type;
					diag->weapon_sample_render = obj->render_type;
					diag->weapon_sample_flags = obj->flags;
					diag->weapon_sample_x = obj->pos.x;
					diag->weapon_sample_y = obj->pos.y;
					diag->weapon_sample_z = obj->pos.z;
					diag->weapon_sample_last_x = obj->last_pos.x;
					diag->weapon_sample_last_y = obj->last_pos.y;
					diag->weapon_sample_last_z = obj->last_pos.z;
					diag->weapon_sample_size = obj->size;
					diag->weapon_sample_shields = obj->shields;
					diag->weapon_sample_lifeleft = obj->lifeleft;
					if (obj->movement_type == MT_PHYSICS) {
						diag->weapon_sample_phys_flags =
						    obj->mtype.phys_info.flags;
						diag->weapon_sample_vel_x =
						    obj->mtype.phys_info.velocity.x;
						diag->weapon_sample_vel_y =
						    obj->mtype.phys_info.velocity.y;
						diag->weapon_sample_vel_z =
						    obj->mtype.phys_info.velocity.z;
					}
					if (obj->control_type == CT_WEAPON) {
						diag->weapon_sample_parent_type =
						    obj->ctype.laser_info.parent_type;
						diag->weapon_sample_parent_num =
						    obj->ctype.laser_info.parent_num;
						diag->weapon_sample_parent_sig =
						    obj->ctype.laser_info.parent_signature;
					}
				}
				diag->weapon_object_count++;
				diag->weapon_state_hash = input_demo_state_trace_hash_object(
				    diag->weapon_state_hash, obj);
				break;
			case OBJ_FIREBALL: {
				unsigned int object_hash =
				    input_demo_state_trace_hash_object(0, obj);

				diag->fireball_object_count++;
				diag->fireball_state_hash = input_demo_state_trace_hash_object(
				    diag->fireball_state_hash, obj);
				if (fireball_trace_count < INPUT_DEMO_FIREBALL_TRACE_COUNT) {
					diag->fireball_trace_slots[fireball_trace_count] = i;
					diag->fireball_trace_sigs[fireball_trace_count] = obj->signature;
					diag->fireball_trace_ids[fireball_trace_count] = obj->id;
					diag->fireball_trace_hashes[fireball_trace_count] = object_hash;
					diag->fireball_trace_segs[fireball_trace_count] = obj->segnum;
					diag->fireball_trace_lifeleft[fireball_trace_count] =
					    obj->lifeleft;
					diag->fireball_trace_delete_objnums[fireball_trace_count] =
					    obj->ctype.expl_info.delete_objnum;
					diag->fireball_trace_attached_objs[fireball_trace_count] =
					    obj->attached_obj;
					fireball_trace_count++;
				}
				if (i == fireball_sample_obj) {
					diag->fireball_sample_obj = i;
					diag->fireball_sample_sig = obj->signature;
					diag->fireball_sample_id = obj->id;
					diag->fireball_sample_hash = object_hash;
					diag->fireball_sample_seg = obj->segnum;
					diag->fireball_sample_control = obj->control_type;
					diag->fireball_sample_movement = obj->movement_type;
					diag->fireball_sample_render = obj->render_type;
					diag->fireball_sample_flags = obj->flags;
					diag->fireball_sample_x = obj->pos.x;
					diag->fireball_sample_y = obj->pos.y;
					diag->fireball_sample_z = obj->pos.z;
					diag->fireball_sample_last_x = obj->last_pos.x;
					diag->fireball_sample_last_y = obj->last_pos.y;
					diag->fireball_sample_last_z = obj->last_pos.z;
					diag->fireball_sample_size = obj->size;
					diag->fireball_sample_shields = obj->shields;
					diag->fireball_sample_lifeleft = obj->lifeleft;
					diag->fireball_sample_attached_obj = obj->attached_obj;
					diag->fireball_sample_spawn_time = obj->ctype.expl_info.spawn_time;
					diag->fireball_sample_delete_time = obj->ctype.expl_info.delete_time;
					diag->fireball_sample_delete_objnum =
					    obj->ctype.expl_info.delete_objnum;
					diag->fireball_sample_attach_parent =
					    obj->ctype.expl_info.attach_parent;
					diag->fireball_sample_prev_attach =
					    obj->ctype.expl_info.prev_attach;
					diag->fireball_sample_next_attach =
					    obj->ctype.expl_info.next_attach;
				}
				if (previous_fireball_hashes_valid &&
				    diag->fireball_changed_obj < 0 &&
				    previous_fireball_hashes[i] != object_hash) {
					diag->fireball_changed_obj = i;
					diag->fireball_changed_sig = obj->signature;
					diag->fireball_changed_id = obj->id;
					diag->fireball_changed_bucket =
					    i >> INPUT_DEMO_OBJECT_SLOT_BUCKET_BITS;
					diag->fireball_changed_prev_hash = previous_fireball_hashes[i];
					diag->fireball_changed_hash = object_hash;
					diag->fireball_changed_seg = obj->segnum;
					diag->fireball_changed_control = obj->control_type;
					diag->fireball_changed_movement = obj->movement_type;
					diag->fireball_changed_render = obj->render_type;
					diag->fireball_changed_flags = obj->flags;
					diag->fireball_changed_x = obj->pos.x;
					diag->fireball_changed_y = obj->pos.y;
					diag->fireball_changed_z = obj->pos.z;
					diag->fireball_changed_last_x = obj->last_pos.x;
					diag->fireball_changed_last_y = obj->last_pos.y;
					diag->fireball_changed_last_z = obj->last_pos.z;
					diag->fireball_changed_size = obj->size;
					diag->fireball_changed_shields = obj->shields;
					diag->fireball_changed_lifeleft = obj->lifeleft;
				}
				previous_fireball_hashes[i] = object_hash;
				break;
			}
			case OBJ_DEBRIS:
				diag->debris_object_count++;
				diag->debris_state_hash = input_demo_state_trace_hash_object(
				    diag->debris_state_hash, obj);
				break;
		}
	}
	if (previous_robot_ai_static_hashes_valid &&
	    diag->robot_ai_static_changed_obj >= 0) {
		for (i = 0; i <= Highest_object_index; ++i) {
			object *obj = &Objects[i];

			if ((obj->type != OBJ_ROBOT) || (obj->control_type != CT_AI))
				continue;
			diag->robot_ai_static_without_changed_hash =
			    input_demo_state_trace_hash_update(
			        diag->robot_ai_static_without_changed_hash, (unsigned int) i);
			if (i == diag->robot_ai_static_changed_obj)
				diag->robot_ai_static_without_changed_hash =
				    input_demo_state_trace_hash_ai_static_fields(
				        diag->robot_ai_static_without_changed_hash,
				        &previous_robot_ai_static_values[i]);
			else
				diag->robot_ai_static_without_changed_hash =
				    input_demo_state_trace_hash_ai_static(
				        diag->robot_ai_static_without_changed_hash, obj);
		}
	}
	memset(previous_robot_ai_static_present, 0,
	       sizeof(previous_robot_ai_static_present));
	memset(previous_robot_anim_pose_present, 0,
	       sizeof(previous_robot_anim_pose_present));
	for (i = 0; i <= Highest_object_index; ++i) {
		object *obj = &Objects[i];

		if ((obj->type != OBJ_ROBOT) || (obj->control_type != CT_AI))
			continue;
		previous_robot_ai_static_hashes[i] =
		    input_demo_state_trace_hash_update(0, (unsigned int) i);
		previous_robot_ai_static_hashes[i] =
		    input_demo_state_trace_hash_ai_static(previous_robot_ai_static_hashes[i],
		                                          obj);
		previous_robot_ai_static_values[i] = obj->ctype.ai_info;
		previous_robot_ai_static_present[i] = 1;
		previous_robot_anim_pose_hashes[i] =
		    input_demo_state_trace_hash_update(0, (unsigned int) i);
		previous_robot_anim_pose_hashes[i] =
		    input_demo_hash_robot_anim_pose(previous_robot_anim_pose_hashes[i],
		                                    obj);
		previous_robot_anim_pose_present[i] = 1;
	}
	previous_robot_hashes_valid = 1;
	previous_robot_ai_static_hashes_valid = 1;
	previous_robot_anim_pose_hashes_valid = 1;
	previous_fireball_hashes_valid = 1;

	if (Highest_segment_index < 0)
		return;
	diag->segment_object_list_hash = input_demo_state_trace_hash_update(
	    diag->segment_object_list_hash, (unsigned int) Highest_segment_index);
	for (segnum = 0; segnum <= Highest_segment_index; ++segnum) {
		int objnum = Segments[segnum].objects;
		int guard = 0;
		int segment_count = 0;
		unsigned int segment_hash = 0;

		while (objnum != -1) {
			object *obj;

			if (objnum < 0 || objnum > Highest_object_index) {
				diag->segment_object_link_error_count++;
				break;
			}
			if (++guard > MAX_OBJECTS) {
				diag->segment_object_link_error_count++;
				break;
			}
			obj = &Objects[objnum];
			if (obj->type == OBJ_NONE || obj->segnum != segnum)
				diag->segment_object_link_error_count++;
			segment_count++;
			segment_hash = input_demo_state_trace_hash_update(
			    segment_hash, (unsigned int) objnum);
			segment_hash = input_demo_state_trace_hash_update(
			    segment_hash, (unsigned int) obj->signature);
			segment_hash = input_demo_state_trace_hash_update(
			    segment_hash, (unsigned int) obj->type);
			segment_hash = input_demo_state_trace_hash_update(
			    segment_hash, (unsigned int) obj->id);
			segment_hash = input_demo_state_trace_hash_update(
			    segment_hash, (unsigned int) obj->flags);
			segment_hash = input_demo_state_trace_hash_update(
			    segment_hash, (unsigned int) obj->prev);
			segment_hash = input_demo_state_trace_hash_update(
			    segment_hash, (unsigned int) obj->next);
			objnum = obj->next;
		}
		if (!segment_count)
			continue;
		diag->segment_object_list_count += segment_count;
		diag->segment_object_list_hash = input_demo_state_trace_hash_update(
		    diag->segment_object_list_hash, (unsigned int) segnum);
		diag->segment_object_list_hash = input_demo_state_trace_hash_update(
		    diag->segment_object_list_hash, (unsigned int) segment_count);
		diag->segment_object_list_hash = input_demo_state_trace_hash_update(
		    diag->segment_object_list_hash, segment_hash);
	}
}

void input_demo_capture_current_result(input_demo_result *result)
{
	player *current_player = &Players[Player_num];
	int robots_killed;
	int i;

	input_demo_prepare_current_result(current_player);
	input_demo_result_clear(result);
	snprintf(result->game, sizeof(result->game), "%s",
	         input_demo_result_game_name());
	snprintf(result->mission, sizeof(result->mission), "%s",
	         input_demo_result_mission_id());
	result->level = Current_level_num;
	result->difficulty = Difficulty_level;
	if (input_demo_replay_is_loaded())
		result->frame_count = (uint32_t) input_demo_replay_next_frame_index();
	else if (input_demo_recorder_is_active())
		result->frame_count = (uint32_t) input_demo_recorder_frame_count();
	else
		result->frame_count = 0;
	result->has_game_time64 = 1;
	result->game_time64 = GameTime64;
	input_demo_finish_current_result_game_time(result);

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
		result->player0.primary_ammo[i] =
		    i < MAX_PRIMARY_WEAPONS ? current_player->primary_ammo[i] : 0;
	for (i = 0; i < INPUT_DEMO_RESULT_MAX_SECONDARY_AMMO; ++i)
		result->player0.secondary_ammo[i] =
		    i < MAX_SECONDARY_WEAPONS ? current_player->secondary_ammo[i] : 0;

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
	result->level_summary.robots_alive =
	    input_demo_count_live_objects_of_type(OBJ_ROBOT);
	robots_killed = input_demo_current_result_robots_killed(current_player);
	if (robots_killed < 0)
		robots_killed = 0;
	result->level_summary.robots_killed = robots_killed;
	result->level_summary.hostages_remaining =
	    input_demo_count_live_objects_of_type(OBJ_HOSTAGE);
	result->level_summary.powerups_remaining =
	    input_demo_count_live_objects_of_type(OBJ_POWERUP);
	input_demo_finish_current_result_powerups(result);
	result->level_summary.control_center_destroyed =
	    Control_center_destroyed ? 1 : 0;
	result->level_summary.endlevel_completed = Endlevel_sequence ? 1 : 0;
}

int input_demo_trace_collision_pose_active(void)
{
	return input_demo_collision_trace_enabled() &&
	       (input_demo_recorder_is_active() || input_demo_replay_is_loaded());
}

unsigned int input_demo_trace_collision_frame_index(void)
{
	if (input_demo_replay_is_loaded())
		return (unsigned int) input_demo_replay_next_frame_index();
	if (input_demo_recorder_is_active()) {
		const uint32_t frame_count = input_demo_recorder_frame_count();

		return frame_count ? (unsigned int) (frame_count - 1) : 0;
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

void input_demo_log_player_bump_probe(const char *step, object *obj0,
                                      object *obj1, const vms_vector *relative_velocity,
                                      const vms_vector *float_force, fix scale_num, fix scale_den,
                                      int damage_flag)
{
	const object *player = NULL;
	const object *other = NULL;
	const char *mode_name;
	unsigned int frame_index;
	vms_vector fix_force = { 0, 0, 0 };
	vms_vector force_delta = { 0, 0, 0 };

	if (!input_demo_trace_collision_pose_active())
		return;
	mode_name = input_demo_trace_collision_mode_name();
	frame_index = input_demo_trace_collision_frame_index();

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
	           (long long) GameTime64,
	           step,
	           obj0 ? (int) (obj0 - Objects) : -1,
	           obj0 ? obj0->type : -1,
	           obj0 ? obj0->id : -1,
	           obj0 ? obj0->segnum : -1,
	           obj0 ? obj0->pos.x : 0,
	           obj0 ? obj0->pos.y : 0,
	           obj0 ? obj0->pos.z : 0,
	           obj1 ? (int) (obj1 - Objects) : -1,
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
