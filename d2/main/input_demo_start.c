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

#define INPUT_DEMO_PRIMARY_ORDER_COPY_COUNT (MAX_PRIMARY_WEAPONS + 1)
#define INPUT_DEMO_APPLY_EXTRA_PLAYER_CFG(player_cfg) \
	do { \
		if ((player_cfg)->has_headlight_active_default) \
			PlayerCfg.HeadlightActiveDefault = (player_cfg)->headlight_active_default; \
	} while (0)
#define INPUT_DEMO_APPLY_EXTRA_REPLAY_CMDLINE_OPTIONS(options) \
	do { \
		g_replay_robot_labels_enabled = (options)->replay_labels_enabled; \
	} while (0)
#define INPUT_DEMO_RESTORE_CHECKPOINT_SAVE(path) state_restore_all_sub(path, 0)
#include "input_demo_start_shared.h"
#undef INPUT_DEMO_PRIMARY_ORDER_COPY_COUNT

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
	return input_demo_load_replay_checked(demo_path, INPUT_DEMO_GAME_D2, "D2",
		error, error_size);
}

int input_demo_start_loaded_replay(void)
{
	char replay_error[256] = "";
	const char *checkpoint_name;
	const uint8_t *checkpoint_data;
	size_t checkpoint_size;
	input_demo_replay_loaded_context replay_context;
	char *mission_name;
	const char *start_mode;
	const char *local_player_callsign;
	const input_demo_player_cfg *replay_player_cfg;
	int have_replay_player_cfg;

	if (!input_demo_replay_is_loaded())
	{
		printf("Input demo replay is not loaded\n");
		return 1;
	}
	INPUT_DEMO_CRUMB("input_demo: start_loaded_replay enter");
	if (!input_demo_prepare_loaded_replay_context(&replay_context))
		return 1;
	mission_name = replay_context.mission_name;
	start_mode = replay_context.start_mode;
	local_player_callsign = replay_context.local_player_callsign;
	replay_player_cfg = &replay_context.replay_player_cfg;
	have_replay_player_cfg = replay_context.have_replay_player_cfg;
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
			input_demo_apply_replay_player_cfg(replay_player_cfg);
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
	if (!input_demo_restore_replay_checkpoint_data(checkpoint_name,
		checkpoint_data, checkpoint_size))
		return 1;
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
			input_demo_apply_replay_player_cfg(replay_player_cfg);
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
	input_demo_replay_cmdline_options cmdline;
	int cmdline_result;
	const char *demo_path;
	const char *state_log_path;
	char replay_error[256] = "";

	cmdline_result = input_demo_parse_replay_cmdline(&cmdline);
	if (cmdline_result)
		return cmdline_result;
	demo_path = cmdline.demo_path;
	state_log_path = cmdline.state_log_path;
	INPUT_DEMO_CRUMB_V("input_demo: cmdline path=%s", demo_path);
	if (!input_demo_load_replay_from_path(demo_path, replay_error, sizeof(replay_error)))
	{
		printf("Input demo replay load failed: %s\n", replay_error);
		return 1;
	}
	INPUT_DEMO_CRUMB_V("input_demo: replay load ok start_mode=%s mission=%s level=%d",
		input_demo_replay_start_mode() ? input_demo_replay_start_mode() : "(null)",
		input_demo_replay_mission() ? input_demo_replay_mission() : "(null)",
		input_demo_replay_level());
	if (!input_demo_apply_replay_common_setup(&cmdline, replay_error,
		sizeof(replay_error)))
	{
		printf("Input demo replay rng trace start failed: %s\n", replay_error);
		return 1;
	}
	if (state_log_path && !input_demo_state_trace_start_replay(state_log_path, replay_error, sizeof(replay_error)))
	{
		printf("Input demo replay state trace start failed: %s\n", replay_error);
		input_demo_replay_unload();
		return 1;
	}
	if (cmdline.rng_trace_path)
		input_demo_debug_printf("Input demo replay rng trace: %s\n", cmdline.rng_trace_path);
	if (state_log_path)
		input_demo_debug_printf("Input demo replay state trace: %s\n", state_log_path);
	if (cmdline.actual_result_path)
		input_demo_debug_printf("Input demo replay actual result: %s\n", cmdline.actual_result_path);
	return input_demo_start_loaded_replay();
}