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

/*
 *
 * Game loop for Inferno
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <SDL.h>
#include <setjmp.h>
#include "pstypes.h"
#include "console.h"
#include "gr.h"
#include "inferno.h"
#include "game.h"
#include "key.h"
#include "object.h"
#include "physics.h"
#include "dxxerror.h"
#include "joy.h"
#include "iff.h"
#include "pcx.h"
#include "timer.h"
#include "render.h"
#include "laser.h"
#include "screens.h"
#include "textures.h"
#include "slew.h"
#include "gauges.h"
#include "texmap.h"
#include "3d.h"
#include "effects.h"
#include "menu.h"
#include "gameseg.h"
#include "wall.h"
#include "ai.h"
#include "fuelcen.h"
#include "digi.h"
#include "u_mem.h"
#include "palette.h"
#include "morph.h"
#include "lighting.h"
#include "newdemo.h"
#include "collide.h"
#include "weapon.h"
#include "sounds.h"
#include "args.h"
#include "gameseq.h"
#include "automap.h"
#include "text.h"
#include "powerup.h"
#include "fireball.h"
#include "newmenu.h"
#include "gamefont.h"
#include "endlevel.h"
#include "kconfig.h"
#include "mouse.h"
#include "switch.h"
#include "controls.h"
#include "songs.h"
#include "rbaudio.h"
#include "gamepal.h"
#include "mission.h"

#include "multi.h"
#include "cntrlcen.h"
#include "pcx.h"
#include "state.h"
#include "piggy.h"
#include "multibot.h"
#ifdef __ANDROID__
#include "android_crash_handler.h"
#include "coop_save.h"
#endif
#include "fvi.h"
#include "ai.h"
#include "robot.h"
#include "playsave.h"
#include "fix.h"
#include "hudmsg.h"
#include "movie.h"
#include "event.h"
#include "window.h"
#include "maths.h"

#include "input_demo_control_info.h"
#include "input_demo_result.h"
#include "input_demo_replay.h"
#include "input_demo_state_trace.h"
#include "input_demo_recorder.h"
#include "input_demo_energy_trace.h"
#include "input_demo_rng_trace.h"
#include "input_demo_fp_env.h"
#include "input_demo_debug_logging.h"

#ifdef OGL
#include "ogl_init.h"
#endif

#ifdef EDITOR
#include "editor/editor.h"
#include "editor/esegment.h"
#endif


#ifndef NDEBUG
int	Mark_count = 0;                 // number of debugging marks set
#endif

static fix64 last_timer_value=0;
fix ThisLevelTime=0;
static int time_paused=0;

grs_canvas	Screen_3d_window;							// The rectangle for rendering the mine to

int	force_cockpit_redraw=0;
int	PaletteRedAdd, PaletteGreenAdd, PaletteBlueAdd;

//	Toggle_var points at a variable which gets !ed on del-T press.
int	Dummy_var;
int	*Toggle_var = &Dummy_var;

#ifdef EDITOR
//flag for whether initial fade-in has been done
char	faded_in;
#endif

int	Game_suspended=0; //if non-zero, nothing moves but player
fix64	Auto_fire_fusion_cannon_time = 0;
fix	Fusion_charge = 0;
int	Game_mode = GM_GAME_OVER;
int	Global_laser_firing_count = 0;
int	Global_missile_firing_count = 0;
fix64	Next_flare_fire_time = 0;
static unsigned int Game_simulation_frame_id = 0;
#define	FLARE_BIG_DELAY	(F1_0*2)

//	Function prototypes for GAME.C exclusively.

void GameProcessFrame(void);
void FireLaser(void);
void slide_textures(void);
void powerup_grab_cheat_all(void);
void game_init_render_sub_buffers(int x, int y, int w, int h);

//	Other functions
extern void multi_check_for_killgoal_winner();

extern int ReadControls(d_event *event);		// located in gamecntl.c
extern int ReadControlsReplayFrame(void);	// located in gamecntl.c
extern void do_final_boss_frame(void);

static void input_demo_record_game_frame(void)
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

static void input_demo_update_rng_trace_context(void)
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

static void input_demo_capture_state_trace_diag(input_demo_state_trace_diag *diag)
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

#define INPUT_DEMO_RESULT_KILLS_MODE_NONE   0
#define INPUT_DEMO_RESULT_KILLS_MODE_REPLAY 1
#define INPUT_DEMO_RESULT_KILLS_MODE_RECORD 2

static int input_demo_result_kills_mode = 0;
static int input_demo_result_kills_baseline = 0;
static int input_demo_result_kills_baseline_valid = 0;

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

static int input_demo_replay_logged_state_mismatch = 0;
static int input_demo_replay_logged_state_trace_error = 0;

static void input_demo_update_result_kills_baseline(void)
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



static int input_demo_replay_tail_probe_frame_active(uint32_t frame)
{
	return frame >= 816 && frame <= 825;
}



static void input_demo_log_current_replay_frame_state_mismatch(void)
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

static fix64 input_demo_replay_last_timer_value = 0;
static uint32_t input_demo_replay_result_frame_count_override = 0;
static int input_demo_replay_result_has_game_time64_override = 0;
static int64_t input_demo_replay_result_game_time64_override = 0;
static int input_demo_replay_result_endlevel_completed_override = -1;
static int input_demo_replay_compare_terminal_exit_only = 0;



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



static void input_demo_log_replay_energy_stage(const char *label)
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

int input_demo_step_replay_frame(void)
{
	input_demo_restore_replay_fp_environment();
	if (!input_demo_prepare_replay_frame())
		return 0;
	if (ReadControlsReplayFrame())
		return 0;
	if (!time_paused)
	{
		calc_game_time();
		GameProcessFrame();
		input_demo_advance_replay_frame();
	}
	return 1;
}


// text functions

extern ubyte DefiningMarkerMessage;
extern char Marker_input[];

// Cheats
game_cheats cheats;

//	==============================================================================================

//this is called once per game
void init_game()
{
	init_objects();

	init_special_effects();

	init_exploding_walls();

	Clear_window = 2;		//	do portal only window clear.
}


void reset_palette_add()
{
	PaletteRedAdd 		= 0;
	PaletteGreenAdd	= 0;
	PaletteBlueAdd		= 0;
}


u_int32_t Game_screen_mode = SM(640,480);

//initialize the various canvases on the game screen
//called every time the screen mode or cockpit changes
void init_cockpit()
{
	//Initialize the on-screen canvases

	if (Screen_mode != SCREEN_GAME)
		return;

	if ( Screen_mode == SCREEN_EDITOR )
		PlayerCfg.CurrentCockpitMode = CM_FULL_SCREEN;

#ifndef OGL
	if ( Game_screen_mode != (GameArg.GfxHiresGFXAvailable? SM(640,480) : SM(320,200)) && PlayerCfg.CurrentCockpitMode != CM_LETTERBOX) {
		PlayerCfg.CurrentCockpitMode = CM_FULL_SCREEN;
	}
#endif

	if (is_observer() && !can_draw_observer_cockpit())
		PlayerCfg.CurrentCockpitMode = CM_FULL_SCREEN;

	gr_set_current_canvas(NULL);

	switch (PlayerCfg.CurrentCockpitMode) {
		case CM_FULL_COCKPIT:
			game_init_render_sub_buffers(0, 0, SWIDTH, (SHEIGHT * 2) / 3);
			break;

		case CM_REAR_VIEW:
		{
			int x1 = 0, y1 = 0, x2 = SWIDTH, y2 = (SHEIGHT * 2) / 3;
			grs_bitmap* bm;

			PIGGY_PAGE_IN(cockpit_bitmap[PlayerCfg.CurrentCockpitMode]);
			bm = &GameBitmaps[cockpit_bitmap[PlayerCfg.CurrentCockpitMode].index];
			gr_bitblt_find_transparent_area(bm, &x1, &y1, &x2, &y2);
			game_init_render_sub_buffers(x1 * ((float)SWIDTH / bm->bm_w), y1 * ((float)SHEIGHT / bm->bm_h), (x2 - x1 + 1) * ((float)SWIDTH / bm->bm_w), (y2 - y1 + 2) * ((float)SHEIGHT / bm->bm_h));
			break;
		}
		case CM_FULL_SCREEN:
			game_init_render_sub_buffers(0, 0, SWIDTH, SHEIGHT);
			break;

		case CM_STATUS_BAR:
			game_init_render_sub_buffers(0, 0, SWIDTH, (HIRESMODE ? (SHEIGHT * 2) / 2.6 : (SHEIGHT * 2) / 2.72));
			break;

		case CM_LETTERBOX:
		{
			int x, y, w, h;

			x = 0; w = SM_W(Game_screen_mode);
			h = (SM_H(Game_screen_mode) * 3) / 4; // true letterbox size (16:9)
			y = (SM_H(Game_screen_mode) - h) / 2;

			gr_rect(x, 0, w, SM_H(Game_screen_mode) - h);
			gr_rect(x, SM_H(Game_screen_mode) - h, w, SM_H(Game_screen_mode));

			game_init_render_sub_buffers(x, y, w, h);
			break;
		}
	}

	gr_set_current_canvas(NULL);
}

//selects a given cockpit (or lack of one).  See types in game.h
void select_cockpit(int mode)
{
	if (mode != PlayerCfg.CurrentCockpitMode) {		//new mode
		PlayerCfg.CurrentCockpitMode = mode;
		init_cockpit();
	}
}

//force cockpit redraw next time. call this if you've trashed the screen
void reset_cockpit()
{
	force_cockpit_redraw=1;
	last_drawn_cockpit = -1;
}

void game_init_render_sub_buffers( int x, int y, int w, int h )
{
	gr_clear_canvas(0);
	gr_init_sub_canvas( &Screen_3d_window, &grd_curscreen->sc_canvas, x, y, w, h );
}


// Sets up the canvases we will be rendering to
void game_init_render_buffers(int render_w, int render_h)
{
	game_init_render_sub_buffers( 0, 0, render_w, render_h );
}

//called to change the screen mode. Parameter sm is the new mode, one of
//SMODE_GAME or SMODE_EDITOR. returns mode acutally set (could be other
//mode if cannot init requested mode)
int set_screen_mode(int sm)
{
	if ( (Screen_mode == sm) && !((sm==SCREEN_GAME) && (grd_curscreen->sc_mode != Game_screen_mode)) && !(sm==SCREEN_MENU) )
	{
		gr_set_current_canvas(NULL);
		return 1;
	}

#ifdef EDITOR
	Canv_editor = NULL;
#endif

	Screen_mode = sm;

	switch( Screen_mode )
	{
		case SCREEN_MENU:
			if  (grd_curscreen->sc_mode != Game_screen_mode)
				if (gr_set_mode(Game_screen_mode))
					Error("Cannot set screen mode.");
			break;

		case SCREEN_GAME:
			if  (grd_curscreen->sc_mode != Game_screen_mode)
				if (gr_set_mode(Game_screen_mode))
					Error("Cannot set screen mode.");
			break;
#ifdef EDITOR
		case SCREEN_EDITOR:
			if (grd_curscreen->sc_mode != SM(800,600))	{
				int gr_error;
				if ((gr_error=gr_set_mode(SM(800,600)))!=0) { //force into game scrren
					Warning("Cannot init editor screen (error=%d)",gr_error);
					return 0;
				}
			}
			break;
#endif
		case SCREEN_MOVIE:
			if (grd_curscreen->sc_mode != SM(MOVIE_WIDTH,MOVIE_HEIGHT))	{
				if (gr_set_mode(SM(MOVIE_WIDTH,MOVIE_HEIGHT))) Error("Cannot set screen mode for game!");
				gr_palette_load( gr_palette );
			}
			break;
		default:
			Error("Invalid screen mode %d",sm);
	}

	gr_set_current_canvas(NULL);

	return 1;
}

void stop_time()
{
	if (time_paused==0) {
		fix64 time;
		timer_update();
		time = timer_query();
	}
	time_paused++;
}

void start_time()
{
	#ifdef __ANDROID__
	if (time_paused <= 0) {
		crash_breadcrumb_v("start_time underflow ignored at %s:%d", __FILE__, __LINE__);
		time_paused = 0;
		return;
	}
	#endif
	time_paused--;
	Assert(time_paused >= 0);
	if (time_paused==0) {
		fix64 time;
		timer_update();
		time = timer_query();
	}
}

void game_flush_inputs()
{
	int dx,dy,dz;
	event_flush();
	key_flush();
	joy_flush();
	mouse_flush();
	mouse_get_delta( &dx, &dy, &dz );	// Read mouse
	memset(&Controls,0,sizeof(control_info));
}

/*
 * timer that every 50ms sets d_tick_step true and increments d_tick_count
 */
static fix d_tick_timer = 0;

void calc_d_tick()
{
	d_tick_step = 0;

	d_tick_timer += FrameTime;
	if (d_tick_timer >= F1_0/20)
	{
		d_tick_step = 1;
		d_tick_count++;
		if (d_tick_count > 1000000)
			d_tick_count = 0;
		d_tick_timer = (d_tick_timer-(F1_0/20));
	}
}

unsigned int game_get_simulation_frame_id(void)
{
	return Game_simulation_frame_id;
}

void game_get_d_tick_state(game_d_tick_state *state)
{
	if (!state)
		return;

	state->count = d_tick_count;
	state->step = d_tick_step;
	state->timer = d_tick_timer;
}

void game_set_d_tick_state(const game_d_tick_state *state)
{
	if (!state)
		return;

	d_tick_count = state->count;
	d_tick_step = state->step;
	d_tick_timer = state->timer;
}

void reset_time()
{
	timer_update();
	last_timer_value = timer_query();
}

#ifdef __ANDROID__
/* android port: always-on FPS counter + frame timing for video info overlay */
int g_current_fps = 0;
int g_frame_time_us = 0;      /* last frame time in microseconds */
int g_frame_time_avg_us = 0;  /* rolling average over last 60 frames */
int g_frame_time_max_us = 0;  /* max over last 60 frames */
#endif

void calc_frame_time()
{
	fix64 timer_value;
	fix last_frametime = FrameTime;

	timer_update();
	timer_value = timer_query();
	FrameTime = timer_value - last_timer_value;

	{
		int max_fps = GameCfg.VSync ? MAXIMUM_FPS : PlayerCfg.maxFps;
		if (max_fps <= 0) max_fps = MAXIMUM_FPS;
		while (FrameTime < f1_0 / max_fps)
		{
			if (GameArg.SysUseNiceFPS && !GameCfg.VSync)
				timer_delay(f1_0 / max_fps - FrameTime);
			timer_update();
			timer_value = timer_query();
			FrameTime = timer_value - last_timer_value;
		}
	}

	if ( cheats.turbo )
		FrameTime *= 2;

	last_timer_value = timer_value;

	if (FrameTime < 0)				//if bogus frametime...
		FrameTime = (last_frametime==0?1:last_frametime);		//...then use time from last frame

#ifdef __ANDROID__
	/* android port: update FPS counter + frame timing for video info overlay */
	{
		static int fps_cnt = 0;
		static fix64 fps_t = 0;
		/* rolling window for avg/max over last 60 frames */
		static int ft_ring[60];
		static int ft_idx = 0;
		int ft_us = (int)((FrameTime * 15625LL) / 1024);
		g_frame_time_us = ft_us;
		ft_ring[ft_idx] = ft_us;
		ft_idx = (ft_idx + 1) % 60;
		fps_cnt++;
		if (timer_value >= fps_t + F1_0) {
			g_current_fps = fps_cnt;
			fps_cnt = 0;
			fps_t = timer_value;
			/* compute avg and max over the ring */
			int sum = 0, mx = 0;
			for (int i = 0; i < 60; i++) {
				sum += ft_ring[i];
				if (ft_ring[i] > mx) mx = ft_ring[i];
			}
			g_frame_time_avg_us = sum / 60;
			g_frame_time_max_us = mx;
		}
	}
#endif
}

void calc_game_time()
{
	GameTime64 += FrameTime;

	// CED -- Something's busted with the D2 code.  Here's D1.
	fix D1_MIN_TRACKABLE = (3 * F1_0) / 4; 
	Min_trackable_dot = 3*(F1_0 - D1_MIN_TRACKABLE)/4 + D1_MIN_TRACKABLE;


	calc_d_tick();
}

void move_player_2_segment(segment *seg,int side)
{
	vms_vector vp;

	compute_segment_center(&ConsoleObject->pos,seg);
	compute_center_point_on_side(&vp,seg,side);
	vm_vec_sub2(&vp,&ConsoleObject->pos);
	vm_vector_2_matrix(&ConsoleObject->orient,&vp,NULL,NULL);

	obj_relink( ConsoleObject-Objects, SEG_PTR_2_NUM(seg) );

}

void do_photos();
void level_with_floor();

#ifndef OGL
void save_screen_shot(int automap_flag)
{
	grs_canvas *screen_canv=&grd_curscreen->sc_canvas;
	static int savenum=0;
	grs_canvas *temp_canv,*save_canv;
        char savename[FILENAME_LEN+sizeof(SCRNS_DIR)];
	ubyte pal[768];

	stop_time();

	if (!PHYSFSX_exists(SCRNS_DIR,0))
		PHYSFS_mkdir(SCRNS_DIR); //try making directory

	save_canv = grd_curcanv;
	temp_canv = gr_create_canvas(screen_canv->cv_bitmap.bm_w,screen_canv->cv_bitmap.bm_h);
	gr_set_current_canvas(temp_canv);
	gr_ubitmap(0,0,&screen_canv->cv_bitmap);

	gr_set_current_canvas(save_canv);

	do
	{
		sprintf(savename, "%sscrn%04d.pcx",SCRNS_DIR, savenum++);
	} while (PHYSFSX_exists(savename,0));

	gr_set_current_canvas(NULL);

	if (!automap_flag)
		HUD_init_message(HM_DEFAULT, "%s 'scrn%04d.pcx'", TXT_DUMPING_SCREEN, savenum-1 );

	gr_palette_read(pal);		//get actual palette from the hardware
	pcx_write_bitmap(savename,&temp_canv->cv_bitmap,pal);
	gr_set_current_canvas(screen_canv);
	gr_ubitmap(0,0,&temp_canv->cv_bitmap);
	gr_free_canvas(temp_canv);
	gr_set_current_canvas(save_canv);

	start_time();
}

#endif

//initialize flying
void fly_init(object *obj)
{
	obj->control_type = CT_FLYING;
	obj->movement_type = MT_PHYSICS;

	vm_vec_zero(&obj->mtype.phys_info.velocity);
	vm_vec_zero(&obj->mtype.phys_info.thrust);
	vm_vec_zero(&obj->mtype.phys_info.rotvel);
	vm_vec_zero(&obj->mtype.phys_info.rotthrust);
}

void test_anim_states();

extern int been_in_editor;

//	------------------------------------------------------------------------------------
void do_cloak_stuff(void)
{
	int i;

	for (i = (Netgame.host_is_obs ? 1 : 0); i < N_players; i++)
		if (Players[i].flags & PLAYER_FLAGS_CLOAKED) {
			if (GameTime64 > Players[i].cloak_time+CLOAK_TIME_MAX)
			{
				Players[i].flags &= ~PLAYER_FLAGS_CLOAKED;
				if (i == Player_num) {
					digi_play_sample( SOUND_CLOAK_OFF, F1_0);
#ifdef NETWORK
					if (Game_mode & GM_MULTI)
					{
						multi_send_play_sound(SOUND_CLOAK_OFF, F1_0);
						multi_send_ship_status();
					}
					maybe_drop_net_powerup(POW_CLOAK);
					if ( Newdemo_state != ND_STATE_PLAYBACK )
						multi_send_decloak(); // For demo recording
#endif
				}
			}
		}
}

int FakingInvul=0;

//	------------------------------------------------------------------------------------
void do_invulnerable_stuff(void)
{
	if (Players[Player_num].flags & PLAYER_FLAGS_INVULNERABLE) {
		if (GameTime64 > Players[Player_num].invulnerable_time+INVULNERABLE_TIME_MAX)
		{
			Players[Player_num].flags ^= PLAYER_FLAGS_INVULNERABLE;
			if (FakingInvul==0 && Newdemo_state != ND_STATE_PLAYBACK)
			{
				digi_play_sample( SOUND_INVULNERABILITY_OFF, F1_0);
				#ifdef NETWORK
				if (Game_mode & GM_MULTI)
				{
					multi_send_play_sound(SOUND_INVULNERABILITY_OFF, F1_0);
					maybe_drop_net_powerup(POW_INVULNERABILITY);
					multi_send_ship_status();
				}
				#endif
			}
#ifdef __ANDROID__
			/* android port: send ship status when spawn invuln expires so
			 * remote machines clear the stale INVULNERABLE flag */
			else if (FakingInvul && (Game_mode & GM_MULTI_COOP))
				multi_send_ship_status();
#endif
			FakingInvul=0;
		}
	}
}

ubyte	Last_afterburner_state = 0;
fix Last_afterburner_charge = 0;

#define AFTERBURNER_LOOP_START	((GameArg.SndDigiSampleRate==SAMPLE_RATE_22K)?32027:(32027/2))		//20098
#define AFTERBURNER_LOOP_END		((GameArg.SndDigiSampleRate==SAMPLE_RATE_22K)?48452:(48452/2))		//25776

int	Ab_scale = 4;

//	------------------------------------------------------------------------------------
#ifdef NETWORK
extern void multi_send_sound_function (char,char);
#endif

void do_afterburner_stuff(void)
{
	static sbyte func_play = 0;

	if (!(Players[Player_num].flags & PLAYER_FLAGS_AFTERBURNER))
		Players[Player_num].afterburner_charge = 0;

	if (Endlevel_sequence || Player_is_dead)
	{
		digi_kill_sound_linked_to_object( Players[Player_num].objnum);
#ifdef NETWORK
		if (Game_mode & GM_MULTI && func_play)
		{
			multi_send_sound_function (0,0);
			func_play = 0;
		}
#endif
	}

	if ((Controls.afterburner_state != Last_afterburner_state && Last_afterburner_charge) || (Last_afterburner_state && Last_afterburner_charge && !Players[Player_num].afterburner_charge)) {

		if (Players[Player_num].afterburner_charge && Controls.afterburner_state && (Players[Player_num].flags & PLAYER_FLAGS_AFTERBURNER)) {
			digi_link_sound_to_object3( SOUND_AFTERBURNER_IGNITE, Players[Player_num].objnum, 1, F1_0, i2f(256), AFTERBURNER_LOOP_START, AFTERBURNER_LOOP_END );
#ifdef NETWORK
			if (Game_mode & GM_MULTI)
			{
				multi_send_sound_function (3,SOUND_AFTERBURNER_IGNITE);
				func_play = 1;
			}
#endif
		} else {
			digi_kill_sound_linked_to_object( Players[Player_num].objnum);
			digi_link_sound_to_object2( SOUND_AFTERBURNER_PLAY, Players[Player_num].objnum, 0, F1_0, i2f(256));
#ifdef NETWORK
			if (Game_mode & GM_MULTI)
			{
			 	multi_send_sound_function (0,0);
				func_play = 0;
			}
#endif
		}
	}

	//@@if (Controls.afterburner_state && Afterburner_charge)
	//@@	afterburner_shake();

	Last_afterburner_state = Controls.afterburner_state;
	Last_afterburner_charge = Players[Player_num].afterburner_charge;
}

// -- //	------------------------------------------------------------------------------------
// -- //	if energy < F1_0/2, recharge up to F1_0/2
// -- void recharge_energy_frame(void)
// -- {
// -- 	if (Players[Player_num].energy < Weapon_info[0].energy_usage) {
// -- 		Players[Player_num].energy += FrameTime/4;
// --
// -- 		if (Players[Player_num].energy > Weapon_info[0].energy_usage)
// -- 			Players[Player_num].energy = Weapon_info[0].energy_usage;
// -- 	}
// -- }

//	Amount to diminish guns towards normal, per second.
#define	DIMINISH_RATE 16 // gots to be a power of 2, else change the code in diminish_palette_towards_normal

extern fix Flash_effect;

 //adds to rgb values for palette flash
void PALETTE_FLASH_ADD(int _dr, int _dg, int _db)
{
	int	maxval;

	PaletteRedAdd += _dr;
	PaletteGreenAdd += _dg;
	PaletteBlueAdd += _db;

	if (Flash_effect)
		maxval = 60;
	else
		maxval = MAX_PALETTE_ADD;

	if (PaletteRedAdd > maxval)
		PaletteRedAdd = maxval;

	if (PaletteGreenAdd > maxval)
		PaletteGreenAdd = maxval;

	if (PaletteBlueAdd > maxval)
		PaletteBlueAdd = maxval;

	if (PaletteRedAdd < -maxval)
		PaletteRedAdd = -maxval;

	if (PaletteGreenAdd < -maxval)
		PaletteGreenAdd = -maxval;

	if (PaletteBlueAdd < -maxval)
		PaletteBlueAdd = -maxval;
}

fix64	Time_flash_last_played;

//	------------------------------------------------------------------------------------
//	Diminish palette effects towards normal.
void diminish_palette_towards_normal(void)
{
	int	dec_amount = 0;
	int	diminish_rate = !(Game_mode & GM_MULTI) || !Netgame.ReducedFlash ? DIMINISH_RATE : (DIMINISH_RATE * 20);

	//	Diminish at DIMINISH_RATE units/second.
	//	For frame rates > DIMINISH_RATE Hz, use randomness to achieve this.
	if (FrameTime < F1_0/diminish_rate) {
		if (d_rand_fx() < FrameTime*diminish_rate/2)	//	Note: d_rand() is in 0..32767, and 8 Hz means decrement every frame
			dec_amount = 1;
	} else {
		dec_amount = f2i(FrameTime*diminish_rate);	// one second = DIMINISH_RATE counts
		if (dec_amount == 0)
			dec_amount++;				// make sure we decrement by something
	}

	if (Flash_effect) {
		int	force_do = 0;

		//	Part of hack system to force update of palette after exiting a menu.
		if (Time_flash_last_played) {
			force_do = 1;
			PaletteRedAdd ^= 1;	//	Very Tricky!  In gr_palette_step_up, if all stepups same as last time, won't do anything!
		}

		if (Time_flash_last_played + F1_0/8 < GameTime64) {
			digi_play_sample( SOUND_CLOAK_OFF, Flash_effect/4);
			Time_flash_last_played = GameTime64;
		}

		Flash_effect -= FrameTime;
		if (Flash_effect < 0)
			Flash_effect = 0;

		if (force_do || (d_rand_fx() > 4096 )) {
      	if ( (Newdemo_state==ND_STATE_RECORDING) && (PaletteRedAdd || PaletteGreenAdd || PaletteBlueAdd) )
	      	newdemo_record_palette_effect(PaletteRedAdd, PaletteGreenAdd, PaletteBlueAdd);

			gr_palette_step_up( PaletteRedAdd, PaletteGreenAdd, PaletteBlueAdd );

			return;
		}

	}

	if (PaletteRedAdd > 0 ) { PaletteRedAdd -= dec_amount; if (PaletteRedAdd < 0 ) PaletteRedAdd = 0; }
	if (PaletteRedAdd < 0 ) { PaletteRedAdd += dec_amount; if (PaletteRedAdd > 0 ) PaletteRedAdd = 0; }

	if (PaletteGreenAdd > 0 ) { PaletteGreenAdd -= dec_amount; if (PaletteGreenAdd < 0 ) PaletteGreenAdd = 0; }
	if (PaletteGreenAdd < 0 ) { PaletteGreenAdd += dec_amount; if (PaletteGreenAdd > 0 ) PaletteGreenAdd = 0; }

	if (PaletteBlueAdd > 0 ) { PaletteBlueAdd -= dec_amount; if (PaletteBlueAdd < 0 ) PaletteBlueAdd = 0; }
	if (PaletteBlueAdd < 0 ) { PaletteBlueAdd += dec_amount; if (PaletteBlueAdd > 0 ) PaletteBlueAdd = 0; }

	if ( (Newdemo_state==ND_STATE_RECORDING) && (PaletteRedAdd || PaletteGreenAdd || PaletteBlueAdd) )
		newdemo_record_palette_effect(PaletteRedAdd, PaletteGreenAdd, PaletteBlueAdd);

	gr_palette_step_up( PaletteRedAdd, PaletteGreenAdd, PaletteBlueAdd );
}

int	Redsave, Bluesave, Greensave;

void palette_save(void)
{
	Redsave = PaletteRedAdd; Bluesave = PaletteBlueAdd; Greensave = PaletteGreenAdd;
}

void palette_restore(void)
{
	PaletteRedAdd = Redsave; PaletteBlueAdd = Bluesave; PaletteGreenAdd = Greensave;
	gr_palette_step_up( PaletteRedAdd, PaletteGreenAdd, PaletteBlueAdd );

	//	Forces flash effect to fixup palette next frame.
	Time_flash_last_played = 0;
}

extern void dead_player_frame(void);

//	--------------------------------------------------------------------------------------------------
int allowed_to_fire_laser(void)
{
	if (Player_is_dead) {
		Global_missile_firing_count = 0;
		return 0;
	}

	if (is_observer()) {
		return 0;
	}

	//	Make sure enough time has elapsed to fire laser
	if (Next_laser_fire_time > GameTime64)
		return 0;

	return 1;
}

int allowed_to_fire_flare(void)
{
	if (Next_flare_fire_time > GameTime64)
		return 0;

	if (is_observer()) {
		return 0;
	}

	if (Players[Player_num].energy >= Weapon_info[FLARE_ID].energy_usage)
		Next_flare_fire_time = GameTime64 + F1_0/4;
	else
		Next_flare_fire_time = GameTime64 + FLARE_BIG_DELAY;

	return 1;
}

int allowed_to_fire_missile(void)
{
	//	Make sure enough time has elapsed to fire missile
	if (Next_missile_fire_time > GameTime64)
		return 0;

	if (is_observer()) {
		return 0;
	}

	return 1;
}

void full_palette_save(void)
{
	palette_save();
	reset_palette_add();
	gr_palette_load( gr_palette );
}

extern int Death_sequence_aborted;

#ifdef USE_SDLMIXER
#define EXT_MUSIC_TEXT "Jukebox/Audio CD"
#else
#define EXT_MUSIC_TEXT "Audio CD"
#endif

static int free_help(newmenu *menu, d_event *event, void *userdata)
{
	userdata = userdata;

	if (event->type == EVENT_WINDOW_CLOSE)
	{
		newmenu_item *items = newmenu_get_items(menu);
		d_free(items);
	}

	return 0;
}

void show_help()
{
	int nitems = 0;
	newmenu_item *m;

	MALLOC(m, newmenu_item, 26);
	if (!m)
		return;

	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = TXT_HELP_ESC;
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "SHIFT-ESC\t  SHOW GAME LOG";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "F1\t  THIS SCREEN";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = TXT_HELP_F2;
#if !(defined(__APPLE__) || defined(macintosh))
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "Alt-F2/F3\t  SAVE/LOAD GAME";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "Alt-F1\t  Fast Save";
#else
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "Alt-F2/F3 (\x85-SHIFT-s/\x85-o)\t  SAVE/LOAD GAME";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "Alt-F1 (\x85-s)\t  Fast Save";
#endif
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "F3\t  SWITCH COCKPIT MODES";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = TXT_HELP_F4;
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = TXT_HELP_F5;
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "ALT-F7\t  SWITCH HUD MODES";
#if !(defined(__APPLE__) || defined(macintosh))
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = TXT_HELP_PAUSE;
#else
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "Pause (\x85-P)\t  Pause";
#endif
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = TXT_HELP_PRTSCN;
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = TXT_HELP_1TO5;
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = TXT_HELP_6TO10;
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "Shift-F1/F2\t  Cycle left/right window";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "Shift-F4\t  GuideBot menu";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "Alt-Shift-F4\t  Rename GuideBot";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "Shift-F5/F6\t  Drop primary/secondary";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "Shift-number\t  GuideBot commands";
#if !(defined(__APPLE__) || defined(macintosh))
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "Alt-Shift-F9\t  Eject Audio CD";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "Alt-Shift-F10\t  Play/Pause " EXT_MUSIC_TEXT;
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "Alt-Shift-F11/F12\t  Previous/Next Song";
#else
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "\x85-E\t  Eject Audio CD";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "\x85-Up/Down\t  Play/Pause " EXT_MUSIC_TEXT;
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "\x85-Left/Right\t  Previous/Next Song";
#endif
#if (defined(__APPLE__) || defined(macintosh))
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "(Use \x85-# for F#. e.g. \x85-1 for F1)";
#endif

	newmenu_dotiny( NULL, TXT_KEYS, nitems, m, 0, free_help, NULL );
}

void show_netgame_help()
{
	int nitems = 0;
	newmenu_item *m;

	MALLOC(m, newmenu_item, 19);
	if (!m)
		return;

	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "F1\t  THIS SCREEN";
	if (!is_observer()) {
		m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "ALT-0\t  DROP FLAG";
#if !(defined(__APPLE__) || defined(macintosh))
		m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "Alt-F2/F3\t  SAVE/LOAD COOP GAME";
#else
		m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "Alt-F2/F3 (\x85-SHIFT-s/\x85-o)\t  SAVE/LOAD COOP GAME";
#endif
	}
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "ALT-F4\t  SHOW PLAYER NAMES ON HUD";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "F6\t  TOGGLE CONNECTION STATS";	
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "F7\t  TOGGLE KILL LIST";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "ALT-F7\t  SWITCH HUD MODES";
	if (!is_observer()) {
		m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "F8\t  SEND MESSAGE";
		m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "(SHIFT-)F9 to F12\t  (DEFINE)SEND MACRO";
	}
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "PAUSE\t  SHOW NETGAME INFORMATION";

#if (defined(__APPLE__) || defined(macintosh))
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "(Use \x85-# for F#. e.g. \x85-1 for F1)";
#endif
	if (is_observer()) {
		m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "";
		m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "OBSERVERS:";
		m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "CTRL+1 to CTRL+7\t  OBSERVE SPECIFIC PLAYER";
		m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "CTRL+8\t  FLY FREELY";
		m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "CTRL+9/0\t  OBSERVE PREVIOUS/NEXT PLAYER";
		m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "CTRL+MINUS\t  OBSERVE PLAYER IN FIRST PERSON";
		m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "CTRL+EQUALS\t  OBSERVE PLAYER IN THIRD PERSON";
	}
	else {
		m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "";
		m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "MULTIPLAYER MESSAGE COMMANDS:";
		m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "(*): TEXT\t  SEND TEXT TO PLAYER/TEAM (*)";
		m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "/Handicap: (*)\t  SET YOUR STARTING SHIELDS TO (*) [10-100]";
		m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "/move: (*)\t  MOVE PLAYER (*) TO OTHER TEAM (Host-only)";
		m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "/kick: (*)\t  KICK PLAYER (*) FROM GAME (Host-only)";
		m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "/KillReactor\t  BLOW UP THE MINE (Host-only)";
		m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "/noobs\t  KICK OBSERVERS (Host-only)";
	}

	newmenu_dotiny( NULL, TXT_KEYS, nitems, m, 0, free_help, NULL );
}

void show_newdemo_help()
{
	newmenu_item *m;
	int nitems = 0;

	MALLOC(m, newmenu_item, 15);
	if (!m)
		return;

	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "ESC\t  QUIT DEMO PLAYBACK";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "F1\t  THIS SCREEN";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = TXT_HELP_F2;
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "F3\t  SWITCH COCKPIT MODES";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "F4\t  TOGGLE PERCENTAGE DISPLAY";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "UP\t  PLAY";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "DOWN\t  PAUSE";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "RIGHT\t  ONE FRAME FORWARD";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "LEFT\t  ONE FRAME BACKWARD";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "SHIFT-RIGHT\t  FAST FORWARD";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "SHIFT-LEFT\t  FAST BACKWARD";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "CTRL-RIGHT\t  JUMP TO END";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "CTRL-LEFT\t  JUMP TO START";
#if (defined(__APPLE__) || defined(macintosh))
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "";
	m[nitems].type = NM_TYPE_TEXT; m[nitems++].text = "(Use \x85-# for F#. e.g. \x85-1 for F1)";
#endif

	newmenu_dotiny( NULL, "DEMO PLAYBACK CONTROLS", nitems, m, 0, free_help, NULL );
}

//temp function until Matt cleans up game sequencing
extern void temp_reset_stuff_on_level();

#define LEAVE_TIME 0x4000		//how long until we decide key is down	(Used to be 0x4000)

//deal with rear view - switch it on, or off, or whatever
void check_rear_view()
{
	static int leave_mode;
	static fix64 entry_time;

	if (Newdemo_state == ND_STATE_PLAYBACK)
		return;

	if ( Controls.rear_view_count > 0) {	//key/button has gone down
		Controls.rear_view_count = 0;

		if (Rear_view) {
			Rear_view = 0;
			if (PlayerCfg.CurrentCockpitMode==CM_REAR_VIEW) {
				select_cockpit(PlayerCfg.PreferredCockpitMode);
			}
			if (Newdemo_state == ND_STATE_RECORDING)
				newdemo_record_restore_rearview();
		}
		else {
			Rear_view = 1;
			leave_mode = 0;		//means wait for another key
			entry_time = timer_query();
			if (PlayerCfg.CurrentCockpitMode == CM_FULL_COCKPIT) {
				select_cockpit(CM_REAR_VIEW);
			}
			if (Newdemo_state == ND_STATE_RECORDING)
				newdemo_record_rearview();
		}
	}
	else
		if (Controls.rear_view_state) {
			if(PlayerCfg.StickyRearview) {
				if (leave_mode == 0 && (timer_query() - entry_time) > LEAVE_TIME)
					leave_mode = 1;
			} else {
				leave_mode = 1; 
			}
		}
		else
		{
			if (leave_mode==1 && Rear_view) {
				Rear_view = 0;
				if (PlayerCfg.CurrentCockpitMode==CM_REAR_VIEW) {
					select_cockpit(PlayerCfg.PreferredCockpitMode);
				}
				if (Newdemo_state == ND_STATE_RECORDING)
					newdemo_record_restore_rearview();
			}
		}
}

void reset_rear_view(void)
{
	if (Rear_view) {
		if (Newdemo_state == ND_STATE_RECORDING)
			newdemo_record_restore_rearview();
	}

	Rear_view = 0;
	select_cockpit(PlayerCfg.PreferredCockpitMode);
}

int Config_menu_flag;

extern char AcidCheatOn,old_IntMethod;

//turns off all cheats & resets cheater flag
void game_disable_cheats()
{
	memset(&cheats, 0, sizeof(cheats));
}

extern int netplayerinfo_on;

//	game_setup()
// ----------------------------------------------------------------------------

int game_handler(window *wind, d_event *event, void *data);

window *game_setup(void)
{
	window *game_wind = NULL;

	PlayerCfg.CurrentCockpitMode = PlayerCfg.PreferredCockpitMode;
	last_drawn_cockpit = -1;	// Force cockpit to redraw next time a frame renders.
	Endlevel_sequence = 0;

	if (!GameArg.SysInputDemoNoRender || input_demo_replay_is_loaded()) {
		game_wind = window_create(&grd_curscreen->sc_canvas, 0, 0, SWIDTH, SHEIGHT, game_handler, NULL);
		if (!game_wind)
			return NULL;
	}

	reset_palette_add();
	init_cockpit();
	init_gauges();
	netplayerinfo_on = 0;

#ifdef EDITOR
	if (!Cursegp)
	{
		Cursegp = &Segments[0];
		Curside = 0;
	}
	
	if (Segments[ConsoleObject->segnum].segnum == -1)      //segment no longer exists
		obj_relink( ConsoleObject-Objects, SEG_PTR_2_NUM(Cursegp) );

	if (!check_obj_seg(ConsoleObject))
		move_player_2_segment(Cursegp,Curside);
#endif

	Viewer = ConsoleObject;
	fly_init(ConsoleObject);
	Game_suspended = 0;
	reset_time();
	FrameTime = 0;			//make first frame zero

#ifdef EDITOR
	if (Current_level_num == 0) {	//not a real level
		init_player_stats_game(Player_num);
		init_ai_objects();
	}
#endif

	fix_object_segs();

	return game_wind;
}

void game_render_frame();

window *Game_wind = NULL;

// Event handler for the game
int game_handler(window *wind, d_event *event, void *data)
{
	data = data;

	switch (event->type)
	{
		case EVENT_WINDOW_ACTIVATED:
			set_screen_mode(SCREEN_GAME);

			event_toggle_focus(1);
			key_toggle_repeat(0);
			game_flush_inputs();

			if (time_paused)
				start_time();

			if (!((Game_mode & GM_MULTI) && (Newdemo_state != ND_STATE_PLAYBACK)))
				digi_resume_digi_sounds();

			if (!((Game_mode & GM_MULTI) && (Newdemo_state != ND_STATE_PLAYBACK)))
				palette_restore();
			
			reset_cockpit();
			break;

		case EVENT_WINDOW_DEACTIVATED:
			if (!(((Game_mode & GM_MULTI) && (Newdemo_state != ND_STATE_PLAYBACK)) && (!Endlevel_sequence)) )
				stop_time();

			if (!((Game_mode & GM_MULTI) && (Newdemo_state != ND_STATE_PLAYBACK)))
				digi_pause_digi_sounds();

			if (!((Game_mode & GM_MULTI) && (Newdemo_state != ND_STATE_PLAYBACK)))
				full_palette_save();

			event_toggle_focus(0);
			key_toggle_repeat(1);
			break;

		case EVENT_JOYSTICK_BUTTON_UP:
		case EVENT_JOYSTICK_BUTTON_DOWN:
		case EVENT_JOYSTICK_MOVED:
		case EVENT_MOUSE_BUTTON_UP:
		case EVENT_MOUSE_BUTTON_DOWN:
		case EVENT_MOUSE_MOVED:
		case EVENT_KEY_COMMAND:
		case EVENT_KEY_RELEASE:
		case EVENT_IDLE:
			if (input_demo_replay_is_loaded())
				return 1;
			return ReadControls(event);

		case EVENT_WINDOW_DRAW:
			if (input_demo_replay_is_loaded()) {
				if (!input_demo_step_replay_frame())
					return 1;
			} else {
				calc_frame_time();
				if (!time_paused)
				{
					calc_game_time();
					GameProcessFrame();
				}
			}

			if (!Automap_active)		// efficiency hack
			{
				if (force_cockpit_redraw) {			//screen need redrawing?
					init_cockpit();
					force_cockpit_redraw=0;
				}
				game_render_frame();
			}
			break;

		case EVENT_WINDOW_CLOSE:
			digi_stop_digi_sounds();
			input_demo_replay_unload();

			if ( (Newdemo_state == ND_STATE_RECORDING) || (Newdemo_state == ND_STATE_PAUSED) )
				newdemo_stop_recording(0);

#ifdef NETWORK
			multi_leave_game();
#endif

			if ( Newdemo_state == ND_STATE_PLAYBACK )
				newdemo_stop_playback();

			songs_play_song( SONG_TITLE, 1 );

			game_disable_cheats();
			Game_mode = GM_GAME_OVER;
#ifndef __ANDROID__
			if (GameArg.GameLogSplit)
				con_switch_log(NULL); // switch back to default log
#endif
#ifdef EDITOR
			if (!EditorWindow)		// have to do it this way because of the necessary longjmp. Yuck.
#endif
				show_menus();
			Game_wind = NULL;
			event_toggle_focus(0);
			key_toggle_repeat(1);
			break;

		case EVENT_WINDOW_CLOSED:
			longjmp(LeaveEvents, 0);
			break;

		default:
			break;
	}

	return 0;
}

// Initialise game, actually runs in main event loop
void game()
{
	hide_menus();
	Game_wind = game_setup();
}

//called at the end of the program
void close_game()
{
	close_gauges();
	restore_effect_bitmap_icons();
}


#ifdef EDITOR
extern void player_follow_path(object *objp);
extern void check_create_player_path(void);
#endif

extern	int Do_appearance_effect;

object *Missile_viewer=NULL;
int Missile_viewer_sig=-1;

int Marker_viewer_num[2]={-1,-1};
int Coop_view_player[2]={-1,-1};

//returns ptr to escort robot, or NULL
object *find_escort()
{
	int i;

	for (i=0; i<=Highest_object_index; i++)
		if (Objects[i].type == OBJ_ROBOT)
			if (Robot_info[Objects[i].id].companion)
				return &Objects[i];

	return NULL;
}

extern void process_super_mines_frame(void);
extern void do_seismic_stuff(void);

//if water or fire level, make occasional sound
void do_ambient_sounds()
{
	int has_water,has_lava;
	int sound;

	has_lava = (Segment2s[ConsoleObject->segnum].s2_flags & S2F_AMBIENT_LAVA);
	has_water = (Segment2s[ConsoleObject->segnum].s2_flags & S2F_AMBIENT_WATER);

	if (has_lava) {							//has lava
		sound = SOUND_AMBIENT_LAVA;
		if (has_water && (d_rand_fx() & 1))	//both, pick one
			sound = SOUND_AMBIENT_WATER;
	}
	else if (has_water)						//just water
		sound = SOUND_AMBIENT_WATER;
	else
		return;

	if (((d_rand_fx() << 3) < FrameTime)) {						//play the sound
		fix volume = d_rand_fx() + f1_0/2;
		digi_play_sample(sound,volume);
	}
}

// -- extern void lightning_frame(void);

extern void omega_charge_frame(void);

void flicker_lights();

void game_leave_menus(void)
{
	window *wind;

	if (!Game_wind)
		return;

	while ((wind = window_get_front()) && (wind != Game_wind)) // go through all windows and actually close them if they want to
		window_close(wind);
}

void GameProcessFrame(void)
{
	fix player_shields = Players[Player_num].shields;
	int player_was_dead = Player_is_dead;
	Game_simulation_frame_id++;

	input_demo_update_rng_trace_context();
	input_demo_debug_log_replay_energy_stage("entry");
	input_demo_debug_log_player_motion_state("entry");
	input_demo_log_current_replay_frame_state_mismatch();
	input_demo_record_game_frame();
	update_player_stats();
	diminish_palette_towards_normal();		//	Should leave palette effect up for as long as possible by putting right before render.
	do_afterburner_stuff();
	do_cloak_stuff();
	do_invulnerable_stuff();
	remove_obsolete_stuck_objects();
	init_ai_frame();
	do_final_boss_frame();

	if ((Players[Player_num].flags & PLAYER_FLAGS_HEADLIGHT) && (Players[Player_num].flags & PLAYER_FLAGS_HEADLIGHT_ON)) {
		fix energy_before = Players[Player_num].energy;
		static int turned_off=0;
		Players[Player_num].energy -= (FrameTime*3/8);
		if (Players[Player_num].energy < i2f(10)) {
			if (!turned_off) {
				Players[Player_num].flags &= ~PLAYER_FLAGS_HEADLIGHT_ON;
				turned_off = 1;
#ifdef NETWORK
				if (Game_mode & GM_MULTI)
					multi_send_flags(Player_num);
#endif
			}
		}
		else
			turned_off = 0;

		if (Players[Player_num].energy <= 0) {
			Players[Player_num].energy = 0;
			Players[Player_num].flags &= ~PLAYER_FLAGS_HEADLIGHT_ON;
#ifdef NETWORK
			if (Game_mode & GM_MULTI)
				multi_send_flags(Player_num);
#endif
		}
		input_demo_trace_energy_change("headlight", energy_before, Players[Player_num].energy, "", "");
	}
	input_demo_log_replay_energy_stage("after_pre_move_systems");


#ifdef EDITOR
	check_create_player_path();
	player_follow_path(ConsoleObject);
#endif

#ifdef NETWORK
	if (Game_mode & GM_MULTI)
	{
		multi_do_frame();
		if (Netgame.PlayTimeAllowed && ThisLevelTime>=i2f((Netgame.PlayTimeAllowed*5*60)))
			multi_check_for_killgoal_winner();
	}
#endif

	dead_player_frame();
	if (Newdemo_state != ND_STATE_PLAYBACK)
		do_controlcen_dead_frame();

	input_demo_update_result_kills_baseline();

	process_super_mines_frame();
	do_seismic_stuff();
	do_ambient_sounds();

#ifdef NETWORK
	if ((Game_mode & GM_MULTI) && Netgame.PlayTimeAllowed)
		ThisLevelTime +=FrameTime;
#endif

	digi_sync_sounds();

	if (Endlevel_sequence) {
		do_endlevel_frame();
		powerup_grab_cheat_all();
		do_special_effects();
		return;					//skip everything else
	}

	if (Newdemo_state != ND_STATE_PLAYBACK)
		do_exploding_wall_frame();
	if ((Newdemo_state != ND_STATE_PLAYBACK) || (Newdemo_vcr_state != ND_STATE_PAUSED)) {
		do_special_effects();
		wall_frame_process();
		triggers_frame_process();
	}

	if (Control_center_destroyed) {
		if (Newdemo_state==ND_STATE_RECORDING )
			newdemo_record_control_center_destroyed();
	}

	flash_frame();

	if ( Newdemo_state == ND_STATE_PLAYBACK ) {
		newdemo_playback_one_frame();
		if ( Newdemo_state != ND_STATE_PLAYBACK ) {
			if (Game_wind)
				window_close(Game_wind);		// Go back to menu
			return;
		}
	}
	else
	{
		Players[Player_num].homing_object_dist = -1;		//	Assume not being tracked.  Laser_do_weapon_sequence modifies this.

		object_move_all();
		input_demo_debug_log_replay_energy_stage("after_move");
		input_demo_debug_log_player_motion_state("after_move");
		powerup_grab_cheat_all();

		if (Endlevel_sequence)	//might have been started during move
			return;

		fuelcen_update_all();
		input_demo_log_replay_energy_stage("after_fuelcen");
		do_ai_frame_all();
		input_demo_log_replay_energy_stage("after_ai");

		{
			const int can_fire_laser = allowed_to_fire_laser();
			input_demo_debug_log_replay_fire_state("before FireLaser", can_fire_laser);
			if (can_fire_laser)
				FireLaser();				// Fire Laser!
			input_demo_debug_log_replay_fire_state("after FireLaser", can_fire_laser);
		}
		input_demo_log_replay_energy_stage("after_FireLaser");

		if (Auto_fire_fusion_cannon_time) {
			if (Players[Player_num].primary_weapon != FUSION_INDEX)
				Auto_fire_fusion_cannon_time = 0;
			else if (GameTime64 + FrameTime/2 >= Auto_fire_fusion_cannon_time) {
				Auto_fire_fusion_cannon_time = 0;
				Global_laser_firing_count = 1;
			} else if (d_tick_step) {
				vms_vector	rand_vec;
				fix			bump_amount;

				Global_laser_firing_count = 0;

				ConsoleObject->mtype.phys_info.rotvel.x += (d_rand() - 16384)/8;
				ConsoleObject->mtype.phys_info.rotvel.z += (d_rand() - 16384)/8;

				make_random_vector(&rand_vec);

				bump_amount = F1_0*4;

				if (Fusion_charge > F1_0*2)
					bump_amount = Fusion_charge*4;

				bump_one_object(ConsoleObject, &rand_vec, bump_amount);
			}
			else
			{
				Global_laser_firing_count = 0;
			}
		}

		if (Global_laser_firing_count) {
			do_laser_firing_player();
			input_demo_debug_log_replay_fire_state("after do_laser_firing_player", allowed_to_fire_laser());
		}
		input_demo_log_replay_energy_stage("after_weapon_block");

		delayed_autoselect(); /* SelectAfterFire */ 
		do_shield_warnings(); 

		check_robot_respawns();
	}
	input_demo_log_replay_energy_stage("exit");

	if (Do_appearance_effect) {
		create_player_appearance_effect(ConsoleObject);
		Do_appearance_effect = 0;
#ifdef NETWORK
		if ((Game_mode & GM_MULTI) && (Netgame.SpawnStyle == SPAWN_STYLE_SHORT_INVUL || Netgame.SpawnStyle == SPAWN_STYLE_LONG_INVUL ))
		{
			Players[Player_num].flags |= PLAYER_FLAGS_INVULNERABLE;
			if(Netgame.SpawnStyle == SPAWN_STYLE_SHORT_INVUL ) {
				Players[Player_num].invulnerable_time = GameTime64 - F1_0*30 + F1_0/2; // 500 ms invuln
			} else {
				Players[Player_num].invulnerable_time = GameTime64-i2f(27);
			}
			FakingInvul=1;
		}
#endif
	}

	omega_charge_frame();
	slide_textures();
	flicker_lights();

	//if the player is taking damage, give up guided missile control
	if (Players[Player_num].shields != player_shields)
		release_guided_missile(Player_num);

	// Check if we have to close in-game menus for multiplayer
	if ((Game_mode & GM_MULTI) && (Players[Player_num].connected == CONNECT_PLAYING))
	{
		if ( Endlevel_sequence || ((Control_center_destroyed) && (Countdown_seconds_left <= 1)) || // close menus when end of level...
			(Automap_active && ((Player_is_dead != player_was_dead) || (Players[Player_num].shields<=0 && player_shields>0))) ) // close autmap when dying ...
			game_leave_menus();
	}

	render_warn_robots_about_player_fire();

	input_demo_debug_log_player_motion_state("exit");

#ifdef __ANDROID__
	/* Periodic coop autosave -- host only, every 30 seconds of level time */
	{
		static fix64 last_coop_autosave_time = 0;
		if ((Game_mode & GM_MULTI_COOP) && multi_i_am_master() &&
		    GameTime64 >= last_coop_autosave_time + F1_0 * 30) {
			last_coop_autosave_time = GameTime64;
			coop_autosave();
		}
	}
#endif
}

//!!extern int Goal_blue_segnum,Goal_red_segnum;
//!!extern int Hoard_goal_eclip;
//!!
//!!//do cool pulsing lights in hoard goals
//!!hoard_light_pulse()
//!!{
//!!	if (Game_mode & GM_HOARD) {
//!!		fix light;
//!!		int frame;
//!!
//!!		frame = Effects[Hoard_goal_eclip].frame_count;
//!!
//!!		frame++;
//!!
//!!		if (frame >= Effects[Hoard_goal_eclip].vc.num_frames)
//!!			frame = 0;
//!!
//!!		light = abs(frame - 5) * f1_0 / 5;
//!!
//!!		Segment2s[Goal_red_segnum].static_light = Segment2s[Goal_blue_segnum].static_light = light;
//!!	}
//!!}


ubyte	Slide_segs[MAX_SEGMENTS];
int	Slide_segs_computed;

void compute_slide_segs(void)
{
	int	segnum, sidenum;

	for (segnum=0;segnum<=Highest_segment_index;segnum++) {
		Slide_segs[segnum] = 0;
		for (sidenum=0;sidenum<6;sidenum++) {
			int tmn = Segments[segnum].sides[sidenum].tmap_num;
			if (TmapInfo[tmn].slide_u != 0 || TmapInfo[tmn].slide_v != 0)
				Slide_segs[segnum] |= 1 << sidenum;
		}
	}

	Slide_segs_computed = 1;
}

//	-----------------------------------------------------------------------------
void slide_textures(void)
{
	int segnum,sidenum,i;

	if (!Slide_segs_computed)
		compute_slide_segs();

	for (segnum=0;segnum<=Highest_segment_index;segnum++) {
		if (Slide_segs[segnum]) {
			for (sidenum=0;sidenum<6;sidenum++) {
				if (Slide_segs[segnum] & (1 << sidenum)) {
					int tmn = Segments[segnum].sides[sidenum].tmap_num;
					if (TmapInfo[tmn].slide_u != 0 || TmapInfo[tmn].slide_v != 0) {
						for (i=0;i<4;i++) {
							Segments[segnum].sides[sidenum].uvls[i].u += fixmul(FrameTime,TmapInfo[tmn].slide_u<<8);
							Segments[segnum].sides[sidenum].uvls[i].v += fixmul(FrameTime,TmapInfo[tmn].slide_v<<8);
							if (Segments[segnum].sides[sidenum].uvls[i].u > f2_0) {
								int j;
								for (j=0;j<4;j++)
									Segments[segnum].sides[sidenum].uvls[j].u -= f1_0;
							}
							if (Segments[segnum].sides[sidenum].uvls[i].v > f2_0) {
								int j;
								for (j=0;j<4;j++)
									Segments[segnum].sides[sidenum].uvls[j].v -= f1_0;
							}
							if (Segments[segnum].sides[sidenum].uvls[i].u < -f2_0) {
								int j;
								for (j=0;j<4;j++)
									Segments[segnum].sides[sidenum].uvls[j].u += f1_0;
							}
							if (Segments[segnum].sides[sidenum].uvls[i].v < -f2_0) {
								int j;
								for (j=0;j<4;j++)
									Segments[segnum].sides[sidenum].uvls[j].v += f1_0;
							}
						}
					}
				}
			}
		}
	}
}

flickering_light Flickering_lights[MAX_FLICKERING_LIGHTS];

int Num_flickering_lights=0;

void flicker_lights()
{
	int l;
	flickering_light *f;

	f = Flickering_lights;

	for (l=0;l<Num_flickering_lights;l++,f++) {
		segment *segp = &Segments[f->segnum];

		//make sure this is actually a light
		if (! (WALL_IS_DOORWAY(segp, f->sidenum) & WID_RENDER_FLAG))
			continue;
		if (! (TmapInfo[segp->sides[f->sidenum].tmap_num].lighting | TmapInfo[segp->sides[f->sidenum].tmap_num2 & 0x3fff].lighting))
			continue;

		if (f->timer == 0x80000000)		//disabled
			continue;

		if ((f->timer -= FrameTime) < 0) {

			while (f->timer < 0)
				f->timer += f->delay;

			f->mask = ((f->mask&0x80000000)?1:0) + (f->mask<<1);

			if (f->mask & 1)
				add_light(f->segnum,f->sidenum);
			else
				subtract_light(f->segnum,f->sidenum);
		}
	}
}

//returns ptr to flickering light structure, or NULL if can't find
flickering_light *find_flicker(int segnum,int sidenum)
{
	int l;
	flickering_light *f;

	//see if there's already an entry for this seg/side

	f = Flickering_lights;

	for (l=0;l<Num_flickering_lights;l++,f++)
		if (f->segnum == segnum && f->sidenum == sidenum)	//found it!
			return f;

	return NULL;
}

//turn flickering off (because light has been turned off)
void disable_flicker(int segnum,int sidenum)
{
	flickering_light *f;

	if ((f=find_flicker(segnum,sidenum)) != NULL)
		f->timer = 0x80000000;
}

//turn flickering off (because light has been turned on)
void enable_flicker(int segnum,int sidenum)
{
	flickering_light *f;

	if ((f=find_flicker(segnum,sidenum)) != NULL)
		f->timer = 0;
}


#ifdef EDITOR

//returns 1 if ok, 0 if error
int add_flicker(int segnum, int sidenum, fix delay, unsigned long mask)
{
	int l;
	flickering_light *f;

	//see if there's already an entry for this seg/side

	f = Flickering_lights;

	for (l=0;l<Num_flickering_lights;l++,f++)
		if (f->segnum == segnum && f->sidenum == sidenum)	//found it!
			break;

	if (mask==0) {		//clearing entry
		if (l == Num_flickering_lights)
			return 0;
		else {
			int i;
			for (i=l;i<Num_flickering_lights-1;i++)
				Flickering_lights[i] = Flickering_lights[i+1];
			Num_flickering_lights--;
			return 1;
		}
	}

	if (l == Num_flickering_lights) {
		if (Num_flickering_lights == MAX_FLICKERING_LIGHTS)
			return 0;
		else
			Num_flickering_lights++;
	}

	f->segnum = segnum;
	f->sidenum = sidenum;
	f->delay = f->timer = delay;
	f->mask = mask;

	return 1;
}

#endif

//	-----------------------------------------------------------------------------
//	Fire Laser:  Registers a laser fire, and performs special stuff for the fusion
//				    cannon.
void FireLaser()
{
	int wants_fire = Controls.fire_primary_state || Controls.fire_primary_count;

	Controls.fire_primary_count = 0;
	Global_laser_firing_count = wants_fire?Weapon_info[Primary_weapon_to_weapon_info[Players[Player_num].primary_weapon]].fire_count:0;

	if ((Players[Player_num].primary_weapon == FUSION_INDEX) && (Global_laser_firing_count)) {
		if ((Players[Player_num].energy < F1_0*2) && (Auto_fire_fusion_cannon_time == 0)) {
			Global_laser_firing_count = 0;
		} else {
			static fix64 Fusion_next_sound_time = 0;
			const fix energy_before = Players[Player_num].energy;
			int flash_val;

			if (Fusion_charge == 0)
				Players[Player_num].energy -= F1_0*2;

			Fusion_charge += FrameTime;
			Players[Player_num].energy -= FrameTime;

			if (Players[Player_num].energy <= 0) {
				Players[Player_num].energy = 0;
				Auto_fire_fusion_cannon_time = GameTime64 -1;	//	Fire now!
			} else
				Auto_fire_fusion_cannon_time = GameTime64 + FrameTime/2 + 1;		//	Fire the fusion cannon at this time in the future.

			{
				char extra_json[160];
				char extra_log[160];

				snprintf(extra_json, sizeof(extra_json), ",\"fusion_charge\":%d,\"fire_count\":%d", Fusion_charge, Global_laser_firing_count);
				snprintf(extra_log, sizeof(extra_log), " fusion_charge=%d fire_count=%d", Fusion_charge, Global_laser_firing_count);
				input_demo_trace_energy_change("fusion_charge", energy_before, Players[Player_num].energy, extra_json, extra_log);
			}

			flash_val = !(Game_mode & GM_MULTI) || !Netgame.ReducedFlash ? Fusion_charge >> 11 :
				PaletteRedAdd < 10 ? 10 - PaletteRedAdd : 0;
			if (Fusion_charge < F1_0*2)
				PALETTE_FLASH_ADD(flash_val, 0, flash_val);
			else
				PALETTE_FLASH_ADD(flash_val, flash_val, 0);

			if (Fusion_next_sound_time > GameTime64 + F1_0/8 + D_RAND_MAX/4) // GameTime64 is smaller than max delay - player in new level?
				Fusion_next_sound_time = GameTime64 - 1;

			if (Fusion_next_sound_time < GameTime64) {
				if (Fusion_charge > F1_0*2) {
					digi_play_sample( 11, F1_0 );

					fix damage = d_rand() * 4; 
					#ifdef NETWORK
					if(Game_mode & GM_MULTI) {
						multi_send_play_sound(11, F1_0);
						con_printf(CON_NORMAL, "You took %.1f damage from overcharging fusion, shields now %.1f\n", f2fl(damage), f2fl(Players[Player_num].shields - damage));

						multi_send_damage(damage, Players[Player_num].shields, OBJ_PLAYER, Player_num, DAMAGE_OVERCHARGE, NULL);
					}
					#endif
					
					apply_damage_to_player(ConsoleObject, ConsoleObject, damage, 0);
				} else {
					if (input_demo_replay_is_loaded())
						con_printf(CON_NORMAL,
							"Input demo replay fire probe: frame=%u kind=fusion_warmup player_obj=%d auto=%d charge=%d next_sound=%lld\n",
							(unsigned int)input_demo_replay_next_frame_index(),
							ConsoleObject - Objects,
							Auto_fire_fusion_cannon_time > 0,
							Fusion_charge,
							(long long)Fusion_next_sound_time);
					input_demo_set_awareness_source("game_fusion_warmup", ConsoleObject - Objects, Fusion_charge);
					create_awareness_event(ConsoleObject, PA_WEAPON_ROBOT_COLLISION);
					digi_play_sample( SOUND_FUSION_WARMUP, F1_0 );
					#ifdef NETWORK
					if (Game_mode & GM_MULTI)
						multi_send_play_sound(SOUND_FUSION_WARMUP, F1_0);
					#endif
				}
				Fusion_next_sound_time = GameTime64 + F1_0/8 + d_rand()/4;
			}

			if (Game_mode & GM_MULTI)
				multi_send_ship_status();
		}
	}
}


//	-------------------------------------------------------------------------------------------------------
//	If player is close enough to objnum, which ought to be a powerup, pick it up!
//	This could easily be made difficulty level dependent.
void powerup_grab_cheat(object *player, int objnum)
{
	fix	powerup_size;
	fix	player_size;
	fix	dist;

	Assert(Objects[objnum].type == OBJ_POWERUP);

	powerup_size = Objects[objnum].size;
	player_size = player->size;

	dist = vm_vec_dist_quick(&Objects[objnum].pos, &player->pos);

	if ((dist < 2*(powerup_size + player_size)) && !(Objects[objnum].flags & OF_SHOULD_BE_DEAD)) {
		vms_vector	collision_point;

		vm_vec_avg(&collision_point, &Objects[objnum].pos, &player->pos);
		collide_player_and_powerup(player, &Objects[objnum], &collision_point);
	}
}

//	-------------------------------------------------------------------------------------------------------
//	Make it easier to pick up powerups.
//	For all powerups in this segment, pick them up at up to twice pickuppable distance based on dot product
//	from player to powerup and player's forward vector.
//	This has the effect of picking them up more easily left/right and up/down, but not making them disappear
//	way before the player gets there.
void powerup_grab_cheat_all(void)
{
	segment	*segp;
	int		objnum;

	segp = &Segments[ConsoleObject->segnum];
	objnum = segp->objects;

	while (objnum != -1) {
		if (Objects[objnum].type == OBJ_POWERUP)
			powerup_grab_cheat(ConsoleObject, objnum);
		objnum = Objects[objnum].next;
	}

}

int	Last_level_path_created = -1;

#ifdef SHOW_EXIT_PATH

//	------------------------------------------------------------------------------------------------------------------
//	Create path for player from current segment to goal segment.
//	Return true if path created, else return false.
int mark_player_path_to_segment(int segnum)
{
	int		i;
	object	*objp = ConsoleObject;
	short		player_path_length=0;
	int		player_hide_index=-1;

	if (Last_level_path_created == Current_level_num) {
		return 0;
	}

	Last_level_path_created = Current_level_num;

	if (create_path_points(objp, objp->segnum, segnum, Point_segs_free_ptr, &player_path_length, 100, 0, 0, -1) == -1) {
		return 0;
	}

	player_hide_index = Point_segs_free_ptr - Point_segs;
	Point_segs_free_ptr += player_path_length;

	if (Point_segs_free_ptr - Point_segs + MAX_PATH_LENGTH*2 > MAX_POINT_SEGS) {
		ai_reset_all_paths();
		return 0;
	}

	for (i=1; i<player_path_length; i++) {
		int			segnum, objnum;
		vms_vector	seg_center;
		object		*obj;

		segnum = Point_segs[player_hide_index+i].segnum;
		seg_center = Point_segs[player_hide_index+i].point;

		objnum = obj_create( OBJ_POWERUP, POW_ENERGY, segnum, &seg_center, &vmd_identity_matrix, Powerup_info[POW_ENERGY].size, CT_POWERUP, MT_NONE, RT_POWERUP);
		if (objnum == -1) {
			Int3();		//	Unable to drop energy powerup for path
			return 1;
		}

		obj = &Objects[objnum];
		obj->rtype.vclip_info.vclip_num = Powerup_info[obj->id].vclip_num;
		obj->rtype.vclip_info.frametime = Vclip[obj->rtype.vclip_info.vclip_num].frame_time;
		obj->rtype.vclip_info.framenum = 0;
		obj->lifeleft = F1_0*100 + d_rand() * 4;
	}

	return 1;
}

//	Return true if it happened, else return false.
int create_special_path(void)
{
	int	i,j;

	//	---------- Find exit doors ----------
	for (i=0; i<=Highest_segment_index; i++)
		for (j=0; j<MAX_SIDES_PER_SEGMENT; j++)
			if (Segments[i].children[j] == -2) {
				return mark_player_path_to_segment(i);
			}

	return 0;
}

#endif


#ifndef RELEASE
int	Max_obj_count_mike = 0;

//	Shows current number of used objects.
void show_free_objects(void)
{
	if (!(d_tick_count & 8)) {
		int	i;
		int	count=0;

		for (i=0; i<=Highest_object_index; i++)
			if (Objects[i].type != OBJ_NONE)
				count++;

		if (count > Max_obj_count_mike) {
			Max_obj_count_mike = count;
		}
	}
}

#endif

/*
 * reads a flickering_light structure from a PHYSFS_file
 */
void flickering_light_read(flickering_light *fl, PHYSFS_file *fp)
{
	fl->segnum = PHYSFSX_readShort(fp);
	fl->sidenum = PHYSFSX_readShort(fp);
	fl->mask = PHYSFSX_readInt(fp);
	fl->timer = PHYSFSX_readFix(fp);
	fl->delay = PHYSFSX_readFix(fp);
}

void flickering_light_write(flickering_light *fl, PHYSFS_file *fp)
{
	PHYSFS_writeSLE16(fp, fl->segnum);
	PHYSFS_writeSLE16(fp, fl->sidenum);
	PHYSFS_writeULE32(fp, fl->mask);
	PHYSFSX_writeFix(fp, fl->timer);
	PHYSFSX_writeFix(fp, fl->delay);
}

bool is_observer() {
	return (Game_mode & GM_OBSERVER) != 0;
}

object* get_player_view_object()
{
	// If we're observing a player, ConsoleObject may not actually be their location if we're in chase cam.
	// But we still want to show extra views from their real location, so correct it
	if (is_observer() && is_observing_player())
		return &Objects[Players[Current_obs_player].objnum];
	else
		return ConsoleObject;
}

bool can_draw_observer_cockpit()
{
	// The result of this function is meaningful only in observer mode.
	Assert(is_observer());

	// We can only draw a cockpit in observer mode if the relevant setting is enabled, and we're
	// observing in first-person mode.
	return PlayerCfg.ObsShowCockpit[get_observer_game_mode()] && is_observing_player() && !Obs_at_distance;
}
