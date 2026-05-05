#include <stdio.h>
#include <string.h>

#include <SDL.h>

#include "input_demo_rng_trace.h"
#include "input_demo_start.h"
#include "input_demo_replay.h"
#include "input_demo_state_trace.h"

extern "C" {
#include "args.h"
#include "bm.h"
#include "config.h"
#include "console.h"
#include "digi.h"
#include "dxxerror.h"
#include "game.h"
#include "gr.h"
#include "inferno.h"
#include "messagebox.h"
#include "physfsx.h"
#include "piggy.h"
#include "player.h"
#include "screens.h"
#include "songs.h"
#include "timer.h"
#include "texmerge.h"
#include "text.h"
#include "u_mem.h"
}

#ifdef inline
#undef inline
#endif

extern "C" void piggy_init_pigfile(char *filename);

static unsigned char *headless_screen_pixels = NULL;

static int init_headless_audio(void)
{
	static char sdl_audio_driver[] = "SDL_AUDIODRIVER=dummy";

	GameArg.SndNoMusic = 1;
	SDL_putenv(sdl_audio_driver);
	digi_select_system(GameArg.SndDisableSdlMixer ? SDLAUDIO_SYSTEM : SDLMIXER_SYSTEM);
	if (digi_init())
		return 0;
	digi_set_digi_volume(0);
	songs_set_volume(0);
	return 1;
}

static const char *find_arg_value(int argc, char *argv[], const char *name)
{
	for (int index = 1; index + 1 < argc; ++index)
		if (!strcmp(argv[index], name))
			return argv[index + 1];
	return NULL;
}

static int find_arg_index(int argc, char *argv[], const char *name)
{
	for (int index = 1; index < argc; ++index)
		if (!strcmp(argv[index], name))
			return index;
	return -1;
}

static int parse_headless_console_output_mode(const char *value)
{
	int mode;

	if (!value)
		return 2;
	mode = atoi(value);
	if (mode != 1 && mode != 2)
		return 0;
	return mode;
}

static int init_headless_runtime(int argc, char *argv[], int console_output_mode, char *error, size_t error_size)
{
	int screen_w;
	int screen_h;

	mem_init();
	error_init(msgbox_error);
	set_warn_func(msgbox_warning);
	PHYSFSX_init(argc, argv);
	if (console_output_mode == 1)
		GameArg.DbgVerbose = -1;
	con_init();
	if (GameArg.SysShowCmdHelp) {
		snprintf(error, error_size, "%s", "help requested");
		return 0;
	}
	if (!PHYSFSX_checkSupportedArchiveTypes()) {
		snprintf(error, error_size, "%s", "archive type check failed");
		return 0;
	}
	if (!PHYSFSX_contfile_init("descent2.hog", 1) &&
	    !PHYSFSX_contfile_init("d2demo.hog", 1)) {
		snprintf(error, error_size, "%s",
		         "could not find descent2.hog or d2demo.hog; pass -hogdir <dir> with Descent 2 data files");
		return 0;
	}
	load_text();
	ReadConfigFile();
	if (!init_headless_audio()) {
		snprintf(error, error_size, "%s", "audio init failed");
		return 0;
	}
	PHYSFSX_addArchiveContent();
	gamedata_init();
	texmerge_init(10);
	piggy_init_pigfile("groupa.pig");
	screen_w = (int) SM_W(Game_screen_mode);
	screen_h = (int) SM_H(Game_screen_mode);
	if (!grd_curscreen) {
		CALLOC(grd_curscreen, grs_screen, 1);
		MALLOC(headless_screen_pixels, unsigned char, screen_w *screen_h);
		memset(headless_screen_pixels, 0, (size_t) (screen_w * screen_h));
		grd_curscreen->sc_mode = Game_screen_mode;
		grd_curscreen->sc_w = (short) screen_w;
		grd_curscreen->sc_h = (short) screen_h;
		grd_curscreen->sc_aspect = fixdiv(grd_curscreen->sc_w * GameCfg.AspectX,
		                                  grd_curscreen->sc_h * GameCfg.AspectY);
		gr_init_canvas(&grd_curscreen->sc_canvas, headless_screen_pixels, BM_LINEAR, screen_w, screen_h);
		gr_set_current_canvas(NULL);
	}
	Screen_mode = SCREEN_GAME;
	init_game();
	Players[Player_num].callsign[0] = '\0';
	GameArg.SysUseNiceFPS = 0;
	GameArg.SysInputDemoNoRender = 1;
	return 1;
}

int main(int argc, char *argv[])
{
	char error[256] = "";
	const char *demo_path = find_arg_value(argc, argv, "-inputdemo-replay");
	int actual_result_arg_index = find_arg_index(argc, argv, "-inputdemo-actual-result");
	int state_log_arg_index = find_arg_index(argc, argv, "-inputdemo-state-log");
	int rng_trace_arg_index = find_arg_index(argc, argv, "-inputdemo-rng-trace");
	int console_output_arg_index = find_arg_index(argc, argv, "-headless-console-output");
	const char *actual_result_path = actual_result_arg_index >= 0 ? find_arg_value(argc, argv, "-inputdemo-actual-result") : NULL;
	const char *state_log_path = state_log_arg_index >= 0 ? find_arg_value(argc, argv, "-inputdemo-state-log") : NULL;
	const char *rng_trace_path = rng_trace_arg_index >= 0 ? find_arg_value(argc, argv, "-inputdemo-rng-trace") : NULL;
	const char *console_output_value = console_output_arg_index >= 0 ? find_arg_value(argc, argv, "-headless-console-output") : NULL;
	int console_output_mode = parse_headless_console_output_mode(console_output_value);
	char mission_name[64] = "";
	char start_mode[32] = "";
	char result_path[260] = "";
	int level_num = 0;
	int difficulty = 0;
	unsigned int frame_count = 0;

	if (!demo_path) {
		fprintf(stderr, "usage: %s -inputdemo-replay <demo.dximdemo> [-inputdemo-actual-result <actual_result.json>] [-inputdemo-state-log <actual_state.jsonl>] [-inputdemo-rng-trace <actual_rngtrace.jsonl>] [-headless-console-output <1|2>]\n", argc > 0 ? argv[0] : "dxx-redux-d2-headless");
		return 1;
	}
	if (actual_result_arg_index >= 0 && !actual_result_path) {
		fprintf(stderr, "HEADLESS-RUN FAIL args missing value for -inputdemo-actual-result\n");
		return 1;
	}
	if (state_log_arg_index >= 0 && !state_log_path) {
		fprintf(stderr, "HEADLESS-RUN FAIL args missing value for -inputdemo-state-log\n");
		return 1;
	}
	if (rng_trace_arg_index >= 0 && !rng_trace_path) {
		fprintf(stderr, "HEADLESS-RUN FAIL args missing value for -inputdemo-rng-trace\n");
		return 1;
	}
	if (console_output_arg_index >= 0 && !console_output_value) {
		fprintf(stderr, "HEADLESS-RUN FAIL args missing value for -headless-console-output\n");
		return 1;
	}
	if (console_output_mode == 0) {
		fprintf(stderr, "HEADLESS-RUN FAIL args invalid -headless-console-output (use 1 or 2)\n");
		return 1;
	}
	if (!init_headless_runtime(argc, argv, console_output_mode, error, sizeof(error))) {
		fprintf(stderr, "HEADLESS-RUN FAIL init %s\n", error[0] ? error : "runtime init failed");
		return 1;
	}

	if (!input_demo_load_replay_from_path(demo_path, error, sizeof(error))) {
		fprintf(stderr, "HEADLESS-RUN FAIL load %s\n", error[0] ? error : "replay load failed");
		return 1;
	}
	if (actual_result_path)
		input_demo_replay_set_actual_result_path(actual_result_path);
	if (rng_trace_path && !input_demo_rng_trace_start_replay(rng_trace_path, error, sizeof(error))) {
		fprintf(stderr, "HEADLESS-RUN FAIL rng trace %s\n", error[0] ? error : "rng trace start failed");
		input_demo_replay_unload();
		return 1;
	}
	if (strcmp(input_demo_replay_start_mode() ? input_demo_replay_start_mode() : "", "save_checkpoint")) {
		fprintf(stderr, "HEADLESS-RUN FAIL unsupported start_mode %s\n",
		        input_demo_replay_start_mode() ? input_demo_replay_start_mode() : "<missing>");
		input_demo_replay_unload();
		return 1;
	}
	snprintf(mission_name, sizeof(mission_name), "%s",
	         input_demo_replay_mission() ? input_demo_replay_mission() : "");
	snprintf(start_mode, sizeof(start_mode), "%s",
	         input_demo_replay_start_mode() ? input_demo_replay_start_mode() : "");
	level_num = input_demo_replay_level();
	difficulty = input_demo_replay_difficulty();
	frame_count = (unsigned int) input_demo_replay_frame_count();
	if (input_demo_replay_actual_result_path())
		snprintf(result_path, sizeof(result_path), "%s", input_demo_replay_actual_result_path());
	if (state_log_path && !input_demo_state_trace_start_replay(state_log_path, error, sizeof(error))) {
		fprintf(stderr, "HEADLESS-RUN FAIL state trace %s\n", error[0] ? error : "state trace start failed");
		input_demo_replay_unload();
		return 1;
	}
	if (input_demo_start_loaded_replay()) {
		fprintf(stderr, "HEADLESS-RUN FAIL start replay\n");
		return 1;
	}
	while (input_demo_replay_is_loaded()) {
		timer_update();
		if (!input_demo_step_replay_frame())
			break;
	}
	if (input_demo_replay_is_loaded())
		input_demo_finish_replay_without_close();

	printf("HEADLESS-RUN OK game=d2 mission=%s level=%d difficulty=%d start_mode=%s frames=%u result=%s\n",
	       mission_name,
	       level_num,
	       difficulty,
	       start_mode,
	       frame_count,
	       result_path[0] ? result_path : "<none>");
	args_exit();
	return 0;
}