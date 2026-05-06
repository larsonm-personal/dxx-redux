#include <stdio.h>
#include <string.h>

#include "args.h"
#include "game.h"
#include "gameseq.h"
#include "input_demo_debug_logging.h"
#include "input_demo_replay.h"
#include "input_demo_rng_mode.h"
#include "input_demo_rng_trace.h"
#include "mission.h"
#include "newmenu.h"
#include "object.h"
#include "physfsx.h"
#include "playsave.h"
#include "player.h"
#include "replay_debug_overlay.h"
#include "state.h"
#include "text.h"
#include "input_demo_start.h"
#include "input_demo_state_trace.h"

#ifdef __ANDROID__
#include "android_crash_handler.h"
#define INPUT_DEMO_CRUMB(msg) crash_breadcrumb(msg)
#define INPUT_DEMO_CRUMB_V crash_breadcrumb_v
#else
#define INPUT_DEMO_CRUMB(msg) ((void)0)
#define INPUT_DEMO_CRUMB_V(...) ((void)0)
#endif

static int input_demo_skip_level_intro = 0;

static int input_demo_find_cmd_arg(const char *name)
{
	int i;

	for (i = 1; i < Num_args; ++i)
		if (!d_stricmp(Args[i], name))
			return i;

	return 0;
}

static const char *input_demo_cmd_arg_value(int arg_index, const char *name)
{
	if (arg_index + 1 >= Num_args || !Args[arg_index + 1] || Args[arg_index + 1][0] == '-')
	{
		printf("Missing value for %s\n", name);
		return NULL;
	}

	return Args[arg_index + 1];
}

void input_demo_set_skip_level_intro(int skip)
{
	input_demo_skip_level_intro = skip ? 1 : 0;
}

int input_demo_consume_skip_level_intro(void)
{
	int skip = input_demo_skip_level_intro;

	input_demo_skip_level_intro = 0;
	return skip;
}

int input_demo_maybe_validate_metadata_from_cmdline(void)
{
	int arg_index = input_demo_find_cmd_arg("-inputdemo-validate");
	int engine_mode;
	int demo_mode;
	const char *demo_path;
	const char *error;

	if (!arg_index)
		return -1;
	demo_path = input_demo_cmd_arg_value(arg_index, "-inputdemo-validate");
	if (!demo_path)
		return 1;
	engine_mode = d_rand_get_replay_mode();
	error = input_demo_rng_mode_validate_metadata_file(demo_path, engine_mode,
		&demo_mode);
	if (error)
	{
		printf("Input demo file invalid: %s\n", demo_path);
		printf("%s\n", error);
		printf("Active RNG backend expects: %s\n",
			input_demo_rng_mode_name(engine_mode));
		return 1;
	}
	printf("Input demo file OK: %s\n", demo_path);
	printf("rng_mode: %s\n", input_demo_rng_mode_name(demo_mode));
	return 0;
}

static unsigned int input_demo_replay_hash_u8_sequence(const ubyte *values, int count)
{
	unsigned int hash = 2166136261u;
	int i;

	for (i = 0; i < count; i++) {
		hash ^= values[i];
		hash *= 16777619u;
	}
	return hash;
}

static void input_demo_apply_replay_player_cfg(const input_demo_player_cfg *player_cfg)
{
	if (!player_cfg)
		return;
	PlayerCfg.AutoLeveling = player_cfg->auto_leveling;
	PlayerCfg.PersistentDebris = player_cfg->persistent_debris;
	if (player_cfg->has_headlight_active_default)
		PlayerCfg.HeadlightActiveDefault = player_cfg->headlight_active_default;
	PlayerCfg.NoFireAutoselect = player_cfg->no_fire_autoselect;
	PlayerCfg.CycleAutoselectOnly = player_cfg->cycle_autoselect_only;
	PlayerCfg.SelectAfterFire = player_cfg->select_after_fire;
	PlayerCfg.ClassicAutoselectWeapon = player_cfg->classic_autoselect_weapon;
	memcpy(PlayerCfg.PrimaryOrder, player_cfg->primary_order, MAX_PRIMARY_WEAPONS + 1);
	memcpy(PlayerCfg.SecondaryOrder, player_cfg->secondary_order, MAX_SECONDARY_WEAPONS + 1);
}

int input_demo_load_replay_from_path(const char *demo_path, char *error, size_t error_size)
{
	int engine_mode;
	int demo_mode;
	const char *validation_error;

	if (!demo_path || !demo_path[0]) {
		snprintf(error, error_size, "%s", "missing demo path");
		return 0;
	}
	engine_mode = d_rand_get_replay_mode();
	validation_error = input_demo_rng_mode_validate_metadata_file(demo_path, engine_mode,
		&demo_mode);
	if (validation_error) {
		snprintf(error, error_size, "%s", validation_error);
		return 0;
	}
	if (!input_demo_replay_load(demo_path, error, error_size))
		return 0;
	if (input_demo_replay_game() != INPUT_DEMO_GAME_D2)
	{
		input_demo_replay_unload();
		snprintf(error, error_size, "%s", "Input demo replay currently supports D2 demos only");
		return 0;
	}
	return 1;
}

int input_demo_start_loaded_replay(void)
{
	char replay_error[256] = "";
	char mission_name[PATH_MAX] = "";
	char local_checkpoint_name[PATH_MAX] = "";
	const char *start_mode;
	const char *checkpoint_name;
	const uint8_t *checkpoint_data;
	size_t checkpoint_size;
	PHYSFS_file *checkpoint_file = NULL;
	const char *checkpoint_base_name;
	input_demo_player_cfg replay_player_cfg;
	char local_player_callsign[CALLSIGN_LEN + 1] = "";
	int have_replay_player_cfg = 0;

	if (!input_demo_replay_is_loaded())
	{
		printf("Input demo replay is not loaded\n");
		return 1;
	}
	INPUT_DEMO_CRUMB("input_demo: start_loaded_replay enter");
	if (Player_num >= 0 && Player_num < MAX_PLAYERS)
	{
		strncpy(local_player_callsign, Players[Player_num].callsign, CALLSIGN_LEN);
		local_player_callsign[CALLSIGN_LEN] = '\0';
	}
	have_replay_player_cfg = input_demo_replay_get_player_cfg(&replay_player_cfg);
	start_mode = input_demo_replay_start_mode();
	if (!start_mode)
	{
		printf("Input demo replay metadata is missing start_mode\n");
		input_demo_replay_unload();
		return 1;
	}
	if (!input_demo_replay_mission())
	{
		printf("Input demo replay metadata is missing mission\n");
		input_demo_replay_unload();
		return 1;
	}
	snprintf(mission_name, sizeof(mission_name), "%s", input_demo_replay_mission());
	if (!strcmp(start_mode, "new_level")) {
		INPUT_DEMO_CRUMB_V("input_demo: new_level mission=%s level=%d frames=%u",
			mission_name, input_demo_replay_level(), input_demo_replay_frame_count());
		if (!load_mission_by_name(mission_name))
		{
			printf("Input demo replay could not load mission: %s\n", mission_name);
			input_demo_replay_unload();
			return 1;
		}
		Difficulty_level = input_demo_replay_difficulty();
		if (have_replay_player_cfg)
			input_demo_apply_replay_player_cfg(&replay_player_cfg);
		printf("Input demo replay starting: %s level %d, %u frames\n",
			mission_name, input_demo_replay_level(), input_demo_replay_frame_count());
		INPUT_DEMO_CRUMB("input_demo: new_level StartNewGame");
		input_demo_set_skip_level_intro(1);
		StartNewGame(input_demo_replay_level());
		return 0;
	}
	if (strcmp(start_mode, "save_checkpoint") != 0)
	{
		printf("Input demo replay start_mode not supported: %s\n", start_mode);
		input_demo_replay_unload();
		return 1;
	}
	checkpoint_name = input_demo_replay_checkpoint_save_name();
	checkpoint_data = input_demo_replay_checkpoint_data();
	checkpoint_size = input_demo_replay_checkpoint_size();
	if (!input_demo_replay_has_checkpoint() || !checkpoint_name || !checkpoint_name[0] || !checkpoint_data || !checkpoint_size)
	{
		printf("Input demo replay is missing checkpoint data\n");
		input_demo_replay_unload();
		return 1;
	}
	checkpoint_base_name = strrchr(checkpoint_name, '/');
	if (checkpoint_base_name)
		checkpoint_base_name++;
	else
		checkpoint_base_name = checkpoint_name;
	if (GameArg.SysUsePlayersDir)
		snprintf(local_checkpoint_name, sizeof(local_checkpoint_name), "Players/%s", checkpoint_base_name);
	else
		snprintf(local_checkpoint_name, sizeof(local_checkpoint_name), "%s", checkpoint_base_name);
	input_demo_debug_printf("Input demo replay checkpoint temp path: recorded=%s local=%s\n",
		checkpoint_name, local_checkpoint_name);
	INPUT_DEMO_CRUMB_V("input_demo: checkpoint temp=%s size=%u",
		local_checkpoint_name, (unsigned int)checkpoint_size);
	if (!strncmp(local_checkpoint_name, "Players/", 8))
		PHYSFS_mkdir("Players");
	PHYSFS_delete(local_checkpoint_name);
	checkpoint_file = PHYSFS_openWrite(local_checkpoint_name);
	if (!checkpoint_file)
	{
		printf("Input demo replay could not write checkpoint file: %s\n", local_checkpoint_name);
		input_demo_replay_unload();
		return 1;
	}
	if (PHYSFS_writeBytes(checkpoint_file, checkpoint_data, checkpoint_size) != (PHYSFS_sint64) checkpoint_size)
	{
		PHYSFS_close(checkpoint_file);
		PHYSFS_delete(local_checkpoint_name);
		printf("Input demo replay could not write checkpoint bytes: %s\n", local_checkpoint_name);
		input_demo_replay_unload();
		return 1;
	}
	PHYSFS_close(checkpoint_file);
	checkpoint_file = NULL;
	INPUT_DEMO_CRUMB("input_demo: checkpoint bytes written");
	if (!state_restore_all_sub(local_checkpoint_name, 0))
	{
		PHYSFS_delete(local_checkpoint_name);
		printf("Input demo replay could not restore checkpoint: %s\n", local_checkpoint_name);
		input_demo_replay_unload();
		return 1;
	}
	PHYSFS_delete(local_checkpoint_name);
	INPUT_DEMO_CRUMB_V("input_demo: checkpoint restored mission=%s level=%d difficulty=%d gt=%lld",
		Current_mission_filename, Current_level_num, Difficulty_level, (long long)GameTime64);
	{
		int player_cfg_result;
		int replay_auto_level = -1;
		const char *replay_callsign;
		unsigned int primary_order_hash;
		unsigned int secondary_order_hash;
		fix player_mass = 0, player_drag = 0, player_brakes = 0;
		unsigned int player_phys_flags = 0;
		fix ship_mass = 0, ship_drag = 0, ship_brakes = 0;
		fix ship_max_thrust = 0, ship_max_rotthrust = 0, ship_wiggle = 0;

		if (ConsoleObject)
			replay_auto_level = (ConsoleObject->mtype.phys_info.flags & PF_LEVELLING) ? 1 : 0;
		if (!Players[Player_num].callsign[0] && local_player_callsign[0])
		{
			strncpy(Players[Player_num].callsign, local_player_callsign, CALLSIGN_LEN);
			Players[Player_num].callsign[CALLSIGN_LEN] = '\0';
		}
		if (Players[Player_num].callsign[0])
		{
			new_player_config();
			player_cfg_result = read_player_file();
		}
		else
			player_cfg_result = -1;
		if (have_replay_player_cfg)
			input_demo_apply_replay_player_cfg(&replay_player_cfg);
		else if (replay_auto_level >= 0)
			PlayerCfg.AutoLeveling = replay_auto_level;
		primary_order_hash = input_demo_replay_hash_u8_sequence(PlayerCfg.PrimaryOrder, MAX_PRIMARY_WEAPONS + 1);
		secondary_order_hash = input_demo_replay_hash_u8_sequence(PlayerCfg.SecondaryOrder, MAX_SECONDARY_WEAPONS + 1);
		replay_callsign = Players[Player_num].callsign[0] ? Players[Player_num].callsign : "<empty>";
		if (ConsoleObject) {
			player_mass = ConsoleObject->mtype.phys_info.mass;
			player_drag = ConsoleObject->mtype.phys_info.drag;
			player_brakes = ConsoleObject->mtype.phys_info.brakes;
			player_phys_flags = ConsoleObject->mtype.phys_info.flags;
		}
		if (Player_ship) {
			ship_mass = Player_ship->mass;
			ship_drag = Player_ship->drag;
			ship_brakes = Player_ship->brakes;
			ship_max_thrust = Player_ship->max_thrust;
			ship_max_rotthrust = Player_ship->max_rotthrust;
			ship_wiggle = Player_ship->wiggle;
		}
		input_demo_debug_printf("Input demo replay player config: callsign=%s result=%d auto_level=%d debris=%d headlight_default=%d autoselect=(nofire=%d,after=%d,cycle=%d,classic=%d) order_hash=(0x%x,0x%x) player_flags=0x%x phys=(%d,%d,%d,0x%x) ship=(%d,%d,%d,%d,%d,%d)\n",
			replay_callsign, player_cfg_result, PlayerCfg.AutoLeveling,
			PlayerCfg.PersistentDebris, PlayerCfg.HeadlightActiveDefault,
			PlayerCfg.NoFireAutoselect, PlayerCfg.SelectAfterFire,
			PlayerCfg.CycleAutoselectOnly, PlayerCfg.ClassicAutoselectWeapon,
			primary_order_hash, secondary_order_hash,
			Players[Player_num].flags,
			player_mass, player_drag, player_brakes, player_phys_flags,
			ship_mass, ship_drag, ship_brakes, ship_max_thrust, ship_max_rotthrust, ship_wiggle);
	}
	if (d_stricmp(Current_mission_filename, mission_name) || Current_level_num != input_demo_replay_level() ||
		Difficulty_level != input_demo_replay_difficulty())
	{
		printf("Input demo replay checkpoint restore mismatch: mission=%s level=%d difficulty=%d\n",
			Current_mission_filename, Current_level_num, Difficulty_level);
		input_demo_replay_unload();
		return 1;
	}
	printf("Input demo replay starting: %s level %d, %u frames\n",
		mission_name, input_demo_replay_level(), input_demo_replay_frame_count());
	INPUT_DEMO_CRUMB_V("input_demo: replay armed mission=%s level=%d frames=%u",
		mission_name, input_demo_replay_level(), input_demo_replay_frame_count());
	return 0;
}

int input_demo_maybe_start_replay_from_cmdline(void)
{
	int arg_index = input_demo_find_cmd_arg("-inputdemo-replay");
	int actual_result_arg_index = input_demo_find_cmd_arg("-inputdemo-actual-result");
	int replay_labels_arg_index = input_demo_find_cmd_arg("-inputdemo-replay-labels");
	int debug_log_arg_index = 0;
	#if INPUT_DEMO_DEBUG_LOGGING_AVAILABLE
	debug_log_arg_index = input_demo_find_cmd_arg("-inputdemo-debug-log");
	#endif
	int state_log_arg_index = input_demo_find_cmd_arg("-inputdemo-state-log");
	int rng_trace_arg_index = input_demo_find_cmd_arg("-inputdemo-rng-trace");
	const char *demo_path;
	const char *actual_result_path = NULL;
	const char *state_log_path = NULL;
	const char *rng_trace_path = NULL;
	char replay_error[256] = "";

	input_demo_debug_set_enabled(debug_log_arg_index ? 1 : 0);
	g_replay_robot_labels_enabled = replay_labels_arg_index ? 1 : 0;

	if (!arg_index)
		return -1;
	demo_path = input_demo_cmd_arg_value(arg_index, "-inputdemo-replay");
	if (!demo_path)
		return 1;
	INPUT_DEMO_CRUMB_V("input_demo: cmdline path=%s", demo_path);
	if (actual_result_arg_index) {
		actual_result_path = input_demo_cmd_arg_value(actual_result_arg_index,
			"-inputdemo-actual-result");
		if (!actual_result_path)
			return 1;
	}
	if (state_log_arg_index) {
		state_log_path = input_demo_cmd_arg_value(state_log_arg_index,
			"-inputdemo-state-log");
		if (!state_log_path)
			return 1;
	}
	if (rng_trace_arg_index) {
		rng_trace_path = input_demo_cmd_arg_value(rng_trace_arg_index,
			"-inputdemo-rng-trace");
		if (!rng_trace_path)
			return 1;
	}
	if (!input_demo_load_replay_from_path(demo_path, replay_error, sizeof(replay_error)))
	{
		printf("Input demo replay load failed: %s\n", replay_error);
		return 1;
	}
	INPUT_DEMO_CRUMB_V("input_demo: replay load ok start_mode=%s mission=%s level=%d",
		input_demo_replay_start_mode() ? input_demo_replay_start_mode() : "(null)",
		input_demo_replay_mission() ? input_demo_replay_mission() : "(null)",
		input_demo_replay_level());
	if (actual_result_path)
		input_demo_replay_set_actual_result_path(actual_result_path);
	if (rng_trace_path && !input_demo_rng_trace_start_replay(rng_trace_path, replay_error, sizeof(replay_error)))
	{
		printf("Input demo replay rng trace start failed: %s\n", replay_error);
		input_demo_replay_unload();
		return 1;
	}
	if (state_log_path && !input_demo_state_trace_start_replay(state_log_path, replay_error, sizeof(replay_error)))
	{
		printf("Input demo replay state trace start failed: %s\n", replay_error);
		input_demo_replay_unload();
		return 1;
	}
	if (rng_trace_path)
		input_demo_debug_printf("Input demo replay rng trace: %s\n", rng_trace_path);
	if (state_log_path)
		input_demo_debug_printf("Input demo replay state trace: %s\n", state_log_path);
	if (actual_result_path)
		input_demo_debug_printf("Input demo replay actual result: %s\n", actual_result_path);
	return input_demo_start_loaded_replay();
}