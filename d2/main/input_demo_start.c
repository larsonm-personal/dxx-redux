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
#define INPUT_DEMO_RESTORED_PLAYER_DIAG_EXTRA_FMT " headlight_default=%d"
#define INPUT_DEMO_RESTORED_PLAYER_DIAG_EXTRA_ARGS , PlayerCfg.HeadlightActiveDefault
#include "input_demo_start_shared.h"
#undef INPUT_DEMO_PRIMARY_ORDER_COPY_COUNT

int input_demo_load_replay_from_path(const char *demo_path, char *error, size_t error_size)
{
	return input_demo_load_replay_from_path_common(demo_path, INPUT_DEMO_GAME_D2,
		"D2", error, error_size);
}

int input_demo_start_loaded_replay(void)
{
	return input_demo_start_loaded_replay_common();
}

int input_demo_maybe_start_replay_from_cmdline(void)
{
	input_demo_replay_cmdline_options cmdline;
	int cmdline_result;
	const char *demo_path;
	char replay_error[256] = "";

	cmdline_result = input_demo_parse_replay_cmdline(&cmdline);
	if (cmdline_result)
		return cmdline_result;
	demo_path = cmdline.demo_path;
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
	if (!input_demo_start_replay_state_trace_and_log_paths(&cmdline,
		replay_error, sizeof(replay_error)))
		return 1;
	return input_demo_start_loaded_replay();
}