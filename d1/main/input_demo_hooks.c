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
extern int ReadControlsReplayFrame(void);
extern void ReadControlsReplayPostFrame(void);
extern void GameProcessFrame(void);
extern int game_is_time_paused(void);

#define INPUT_DEMO_RESULT_GAME_NAME "d1"
#define INPUT_DEMO_RESULT_MISSION_ID input_demo_current_mission_id()
#define INPUT_DEMO_ROBOT_IS_CAMERA_AWAKE(objnum, obj) \
	(Ai_local_info[(objnum)].player_awareness_type > 0)
#include "input_demo_hooks_shared.h"
#undef INPUT_DEMO_ROBOT_IS_CAMERA_AWAKE

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
	input_demo_capture_state_trace_diag(&diag);
	if (!input_demo_recorder_capture_frame((int32_t)FrameTime, &state, &pulse, rng_state, 1, rng_call_count,
		&frame_state,
		&diag,
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

static void input_demo_write_replay_frame_state_trace(const input_demo_replay_frame *replay_frame)
{
	input_demo_result actual_state;
	input_demo_state_trace_diag diag;
	char error[256] = "";

	if (!replay_frame || !input_demo_state_trace_is_active())
		return;
	input_demo_capture_current_result(&actual_state);
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
	if (!input_demo_replay_logged_state_trace_error)
		con_printf(CON_NORMAL, "Input demo replay state trace write failed: %s\n", error);
	input_demo_replay_logged_state_trace_error = 1;
	input_demo_state_trace_stop();
}

static void input_demo_log_replay_frame_state_mismatch(const input_demo_replay_frame *replay_frame)
{
	input_demo_result actual_state;
	char error[256] = "";
	char expected_json[4096] = "";
	char actual_json[4096] = "";

	if (!replay_frame || !replay_frame->has_state || input_demo_replay_logged_state_mismatch)
		return;
	input_demo_capture_current_result(&actual_state);
	if (input_demo_result_compare_snapshot(&replay_frame->state_result, &actual_state, error, sizeof(error)))
		return;
	input_demo_replay_logged_state_mismatch = 1;
	if (!input_demo_result_snapshot_to_json_buffer(&replay_frame->state_result, expected_json, sizeof(expected_json)))
		snprintf(expected_json, sizeof(expected_json), "<snapshot encode failed>");
	if (!input_demo_result_snapshot_to_json_buffer(&actual_state, actual_json, sizeof(actual_json)))
		snprintf(actual_json, sizeof(actual_json), "<snapshot encode failed>");
	con_printf(CON_NORMAL,
		"Input demo replay frame state mismatch: frame=%u gt=%lld %s\n",
		(unsigned int)replay_frame->frame,
		(long long)GameTime64,
		error);
	input_demo_debug_printf("Input demo replay expected state: %s\n", expected_json);
	input_demo_debug_printf("Input demo replay actual state: %s\n", actual_json);
}

void input_demo_log_current_replay_frame_state_mismatch(void)
{
	input_demo_replay_frame replay_frame;
	char error[256] = "";

	if (!input_demo_replay_is_loaded())
		return;
	if (!input_demo_replay_get_current_frame(&replay_frame, error, sizeof(error)))
		return;
	input_demo_write_replay_frame_state_trace(&replay_frame);
	input_demo_log_replay_frame_state_mismatch(&replay_frame);
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
	input_demo_replay_last_timer_value = 0;
	if (!input_demo_replay_is_loaded())
		return;
	input_demo_write_replay_result();
	input_demo_replay_unload();
}

static void input_demo_stop_replay(int write_result)
{
	input_demo_replay_last_timer_value = 0;
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

static int input_demo_sync_replay_rng_to_current_frame(void)
{
	input_demo_replay_frame replay_frame;
	char error[256] = "";

	if (!input_demo_replay_is_loaded())
		return 0;
	if (!input_demo_replay_get_current_frame(&replay_frame, error, sizeof(error))) {
		con_printf(CON_NORMAL, "Input demo replay stopped: %s\n", error);
		input_demo_stop_replay(0);
		return 0;
	}
	if (!d_rand_set_state(replay_frame.rng_state)) {
		con_printf(CON_NORMAL, "Input demo replay stopped: active RNG backend cannot restore state\n");
		input_demo_stop_replay(0);
		return 0;
	}
	if (input_demo_rng_trace_is_active() && replay_frame.has_rng_call_count)
		d_rand_set_call_count(replay_frame.rng_call_count);
	else
		d_rand_reset_call_count();
	return 1;
}

int input_demo_prepare_replay_frame(void)
{
	input_demo_replay_frame replay_frame;
	char error[256] = "";

	if (!input_demo_replay_is_loaded())
		return 0;
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
	input_demo_delay_replay_frame((fix)replay_frame.frame_time);
	input_demo_control_info_from_state(&Controls, &replay_frame.state, &replay_frame.pulse);
	FrameTime = (fix)replay_frame.frame_time;
	return 1;
}

int input_demo_step_replay_frame(void)
{
	int read_controls_result;

	input_demo_restore_replay_fp_environment();
	if (!input_demo_prepare_replay_frame())
		return 0;
	read_controls_result = ReadControlsReplayFrame();
	if (!input_demo_sync_replay_rng_to_current_frame())
		return 0;
	if (read_controls_result)
		return 1;
	if (!game_is_time_paused())
	{
		calc_game_time();
		GameProcessFrame();
		ReadControlsReplayPostFrame();
		input_demo_advance_replay_frame();
	}
	return 1;
}

void input_demo_advance_replay_frame(void)
{
	char error[256] = "";

	if (!input_demo_replay_is_loaded())
		return;
	if (!input_demo_replay_advance_frame(error, sizeof(error))) {
		con_printf(CON_NORMAL, "Input demo replay stopped: %s\n", error);
		input_demo_stop_replay(0);
		return;
	}
	if (input_demo_replay_is_finished())
		input_demo_stop_replay(1);
}

int input_demo_finish_replay_from_mine_exit(void)
{
	input_demo_replay_last_timer_value = 0;
	if (!input_demo_replay_is_loaded())
		return 0;
	input_demo_write_replay_result();
	input_demo_replay_unload();
	if (Game_wind)
		window_close(Game_wind);
	return 1;
}