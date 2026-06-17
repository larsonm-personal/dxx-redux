#include <stdio.h>
#include <string.h>

#include "args.h"
#include "ai.h"
#include "cntrlcen.h"
#include "console.h"
#include "controls.h"
#include "endlevel.h"
#include "game.h"
#include "gameseq.h"
#include "input_demo_control_info.h"
#include "input_demo_debug_logging.h"
#include "input_demo_fp_env.h"
#include "input_demo_result.h"
#include "input_demo_state_trace.h"
#include "input_demo_hooks.h"
#include "input_demo_recorder.h"
#include "input_demo_replay.h"
#include "input_demo_rng_trace.h"
#include "laser.h"
#include "timer.h"
#include "maths.h"
#include "mission.h"
#include "newdemo.h"
#include "object.h"
#include "player.h"

extern int Num_awareness_events;
#include "input_demo_hooks_shared.h"

void input_demo_capture_state_trace_diag(input_demo_state_trace_diag *diag)
{
	if (!diag)
		return;
	memset(diag, 0, sizeof(*diag));
	diag->awareness_events = Num_awareness_events;
	diag->d_tick_count = d_tick_count;
	diag->ai_probe_skip_obj = -1;
	diag->ai_probe_skip_sig = -1;
	diag->ai_probe_skip_id = -1;
	diag->ai_probe_timeslice_obj = -1;
	diag->ai_probe_timeslice_sig = -1;
	diag->ai_probe_timeslice_id = -1;
	diag->ai_probe_process_obj = -1;
	diag->ai_probe_process_sig = -1;
	diag->ai_probe_process_id = -1;
	diag->ai_probe_phys_skip_obj = -1;
	diag->ai_probe_phys_skip_sig = -1;
	diag->ai_probe_phys_skip_id = -1;
	diag->ai_probe_phys_skip_before = -1;
	diag->ai_probe_phys_skip_after = -1;
	input_demo_capture_runtime_state_diag(diag);
	input_demo_capture_object_state_diag(diag);
	input_demo_capture_robot_ai_local_diag(diag);
	input_demo_capture_player_weapon_diag(diag);
}
void input_demo_log_player_robot_contact_probe(const char *step,
	object *player,
	object *robot,
	const vms_vector *collision_point,
	int32_t damage)
{
	if (!input_demo_trace_collision_pose_active() ||
		!player ||
		player != ConsoleObject ||
		player->type != OBJ_PLAYER ||
		!robot ||
		robot->type != OBJ_ROBOT)
		return;

	con_printf(CON_NORMAL,
		"Input demo player robot contact: mode=%s frame=%u gt=%lld step=%s player=%d/%d/%d seg=%d pos=(%d,%d,%d) vel=(%d,%d,%d) shields=%d robot=%d/%d/%d seg=%d pos=(%d,%d,%d) vel=(%d,%d,%d) flags=0x%x companion=%d thief=%d kamikaze=%d drain=%d cp=(%d,%d,%d) damage=%d\n",
		input_demo_trace_collision_mode_name(),
		input_demo_trace_collision_frame_index(),
		(long long)GameTime64,
		step,
		(int)(player - Objects),
		player->type,
		player->id,
		player->segnum,
		player->pos.x,
		player->pos.y,
		player->pos.z,
		player->mtype.phys_info.velocity.x,
		player->mtype.phys_info.velocity.y,
		player->mtype.phys_info.velocity.z,
		Players[player->id].shields,
		(int)(robot - Objects),
		robot->type,
		robot->id,
		robot->segnum,
		robot->pos.x,
		robot->pos.y,
		robot->pos.z,
		robot->mtype.phys_info.velocity.x,
		robot->mtype.phys_info.velocity.y,
		robot->mtype.phys_info.velocity.z,
		robot->flags,
		0,
		0,
		0,
		0,
		collision_point ? collision_point->x : 0,
		collision_point ? collision_point->y : 0,
		collision_point ? collision_point->z : 0,
		damage);
}

static int input_demo_trace_local_player_weapon_robot_active(object *weapon, object *robot)
{
	if (!input_demo_trace_collision_pose_active() || !weapon || !robot)
		return 0;
	if (weapon->type != OBJ_WEAPON || robot->type != OBJ_ROBOT)
		return 0;
	if (weapon->ctype.laser_info.parent_type != OBJ_PLAYER)
		return 0;
	return weapon->ctype.laser_info.parent_num == Players[Player_num].objnum;
}

void input_demo_log_weapon_robot_accept_seq(object *weapon, object *robot)
{
	static unsigned int last_frame = (unsigned int)-1;
	static int accept_seq = 0;
	unsigned int frame;

	if (!input_demo_trace_local_player_weapon_robot_active(weapon, robot))
		return;

	frame = input_demo_trace_collision_frame_index();
	if (frame != last_frame) {
		last_frame = frame;
		accept_seq = 0;
	}
	con_printf(CON_NORMAL,
		"Input demo weapon robot accept seq: mode=%s frame=%u accept_seq=%d weapon_obj=%d weapon_sig=%d robot_obj=%d robot_sig=%d\n",
		input_demo_trace_collision_mode_name(),
		frame,
		accept_seq,
		(int)(weapon - Objects),
		weapon->signature,
		(int)(robot - Objects),
		robot->signature);
	accept_seq++;
}

const char *input_demo_current_mission_id(void)
{
	if (Current_mission && Current_mission_filename && Current_mission_filename[0])
		return Current_mission_filename;
	return "d1";
}

static int input_demo_replay_logged_state_mismatch = 0;
static int input_demo_replay_logged_state_trace_error = 0;
static fix64 input_demo_replay_last_timer_value = 0;

void input_demo_log_current_replay_frame_state_mismatch(void)
{
	input_demo_replay_frame replay_frame;
	char error[256] = "";

	if (!input_demo_replay_is_loaded())
		return;
	if (!input_demo_replay_get_current_frame(&replay_frame, error, sizeof(error)))
		return;
	input_demo_write_replay_frame_state_trace_shared(
		&replay_frame,
		&input_demo_replay_logged_state_trace_error);
	input_demo_log_replay_frame_state_mismatch_shared(
		&replay_frame,
		&input_demo_replay_logged_state_mismatch);
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
	if (!input_demo_result_write_json_file(result_path, &result, error, sizeof(error)))
		con_printf(CON_NORMAL, "Input demo replay result write failed: %s\n", error);
	else {
		input_demo_debug_printf("Input demo replay result written: %s\n", result_path);
		if (!input_demo_replay_compare_result(&result, error, sizeof(error)))
			con_printf(CON_NORMAL, "Input demo replay result mismatch: %s\n", error);
		else
			con_printf(CON_NORMAL, "Input demo replay result matched embedded trailer\n");
	}
}

void input_demo_finish_replay_without_close(void)
{
	(void)input_demo_finish_replay_shared(0,
		&input_demo_replay_last_timer_value,
		NULL,
		input_demo_write_replay_result);
}

static void input_demo_stop_replay(int write_result)
{
	input_demo_stop_replay_shared(write_result,
		&input_demo_replay_last_timer_value,
		input_demo_write_replay_result,
		NULL);
}

static int input_demo_sync_replay_rng_to_current_frame(void)
{
	return input_demo_sync_replay_rng_to_current_frame_shared(
		NULL,
		input_demo_stop_replay);
}

int input_demo_prepare_replay_frame(void)
{
	return input_demo_prepare_replay_frame_shared(
		&input_demo_replay_logged_state_mismatch,
		&input_demo_replay_logged_state_trace_error,
		&input_demo_replay_last_timer_value,
		NULL,
		input_demo_stop_replay);
}

int input_demo_step_replay_frame(void)
{
	return input_demo_step_replay_frame_shared(
		input_demo_prepare_replay_frame,
		input_demo_sync_replay_rng_to_current_frame);
}

void input_demo_advance_replay_frame(void)
{
	input_demo_advance_replay_frame_shared(NULL, input_demo_stop_replay);
}

int input_demo_finish_replay_from_mine_exit(void)
{
	return input_demo_finish_replay_shared(1,
		&input_demo_replay_last_timer_value,
		NULL,
		input_demo_write_replay_result);
}
