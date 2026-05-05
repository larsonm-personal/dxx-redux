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
#include "input_demo_hooks.h"
#include "input_demo_recorder.h"
#include "input_demo_replay.h"
#include "input_demo_result.h"
#include "input_demo_rng_trace.h"
#include "timer.h"
#include "maths.h"
#include "mission.h"
#include "newdemo.h"
#include "object.h"
#include "player.h"

extern int Num_awareness_events;

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