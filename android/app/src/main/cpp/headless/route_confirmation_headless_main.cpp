#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include <physfs.h>
#include <SDL.h>

/* Include this before the legacy engine headers, some of which intentionally
 * change structure packing for on-disk formats. */
#include "route_confirmation.h"
#include "route_confirmation_result.h"

extern "C" {
#include "args.h"
#include "bm.h"
#include "config.h"
#include "console.h"
#include "digi.h"
#include "dxxerror.h"
#include "game.h"
#include "gameseq.h"
#include "gr.h"
#include "inferno.h"
#include "input_demo_start.h"
#include "level_metadata_scan.h"
#include "maths.h"
#include "messagebox.h"
#include "mission.h"
#include "newdemo.h"
#include "physfsx.h"
#include "piggy.h"
#include "player.h"
#include "screens.h"
#include "songs.h"
#include "texmerge.h"
#include "text.h"
#include "u_mem.h"
}

#ifdef inline
#undef inline
#endif

extern "C" void piggy_init_pigfile(char *filename);
extern "C" void GameProcessFrame(void);

namespace
{
unsigned char *Headless_screen_pixels = nullptr;

const char *find_arg_value(int argc, char *argv[], const char *name)
{
	for (int index = 1; index + 1 < argc; ++index)
		if (!strcmp(argv[index], name))
			return argv[index + 1];
	return nullptr;
}

int parse_level(const char *text, int *level)
{
	char *end = nullptr;
	long value;
	if (!text || !level)
		return 0;
	value = strtol(text, &end, 10);
	if (!end || *end || value == 0 || value < -127 || value > 127)
		return 0;
	*level = static_cast<int>(value);
	return 1;
}

int init_headless_audio()
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

int init_headless_screen(char *error, size_t error_size)
{
	const int screen_w = static_cast<int>(SM_W(Game_screen_mode));
	const int screen_h = static_cast<int>(SM_H(Game_screen_mode));
	if (grd_curscreen)
		return 1;
	CALLOC(grd_curscreen, grs_screen, 1);
	MALLOC(Headless_screen_pixels, unsigned char, screen_w *screen_h);
	if (!grd_curscreen || !Headless_screen_pixels) {
		snprintf(error, error_size, "%s", "screen allocation failed");
		return 0;
	}
	memset(Headless_screen_pixels, 0,
	       static_cast<size_t>(screen_w * screen_h));
	grd_curscreen->sc_mode = Game_screen_mode;
	grd_curscreen->sc_w = static_cast<short>(screen_w);
	grd_curscreen->sc_h = static_cast<short>(screen_h);
	grd_curscreen->sc_aspect =
	    fixdiv(grd_curscreen->sc_w * GameCfg.AspectX,
	           grd_curscreen->sc_h * GameCfg.AspectY);
	gr_init_canvas(&grd_curscreen->sc_canvas, Headless_screen_pixels,
	               BM_LINEAR, screen_w, screen_h);
	gr_set_current_canvas(nullptr);
	return 1;
}

int init_headless_runtime(int argc, char *argv[], char *error,
                          size_t error_size)
{
	mem_init();
	error_init(msgbox_error);
	set_warn_func(msgbox_warning);
	PHYSFSX_init(argc, argv);
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
		         "could not find descent2.hog or d2demo.hog");
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
	{
		char groupa_pig[] = "groupa.pig";
		piggy_init_pigfile(groupa_pig);
	}
	if (!init_headless_screen(error, error_size))
		return 0;
	Screen_mode = SCREEN_GAME;
	init_game();
	snprintf(Players[Player_num].callsign,
	         sizeof(Players[Player_num].callsign), "%s", "RouteBot");
	GameArg.SysUseNiceFPS = 0;
	GameArg.SysInputDemoNoRender = 1;
	return 1;
}

int load_requested_mission(const char *requested, char *error,
                           size_t error_size)
{
	std::string mission = requested && *requested ? requested : "d2";
	if (load_mission_by_name(&mission[0]))
		return 1;
	snprintf(error, error_size, "could not load mission %s", mission.c_str());
	return 0;
}

} // namespace

int main(int argc, char *argv[])
{
	char error[256] = "";
	const char *output = find_arg_value(argc, argv, "-route-confirm-json-out");
	const char *mission = find_arg_value(argc, argv, "-mission");
	const char *extra_dir = find_arg_value(argc, argv, "-extra-dir");
	const char *level_text = find_arg_value(argc, argv, "-level");
	int level = 0;
	if (!output || !parse_level(level_text, &level)) {
		fprintf(stderr,
		        "usage: %s -route-confirm-json-out <path> -level <number> "
		        "[-mission <name>] [-hogdir <dir>]\n",
		        argc > 0 ? argv[0] : "dxx-redux-d2-headless-route");
		return 1;
	}
	if (!init_headless_runtime(argc, argv, error, sizeof(error))) {
		fprintf(stderr, "ROUTE-CONFIRM FAIL init %s\n",
		        error[0] ? error : "runtime initialization failed");
		return 1;
	}
	if (extra_dir && *extra_dir && !PHYSFS_addToSearchPath(extra_dir, 0)) {
		fprintf(stderr, "ROUTE-CONFIRM FAIL could not mount extra dir %s\n",
		        extra_dir);
		return 1;
	}
	if (!load_requested_mission(mission, error, sizeof(error))) {
		fprintf(stderr, "ROUTE-CONFIRM FAIL mission %s\n",
		        error[0] ? error : "mission load failed");
		return 1;
	}
	if ((level > 0 && level > Last_level) ||
	    (level < 0 && level < Last_secret_level)) {
		fprintf(stderr, "ROUTE-CONFIRM FAIL level %d outside mission range\n",
		        level);
		return 1;
	}
	d_srand_stream(D_RNG_SIM, ROUTE_CONFIRMATION_CANONICAL_SEED);
	d_srand_stream(D_RNG_FX, ROUTE_CONFIRMATION_CANONICAL_SEED);
	d_rand_reset_stream_call_count(D_RNG_SIM);
	d_rand_reset_stream_call_count(D_RNG_FX);
	Difficulty_level = 2;
	input_demo_set_skip_level_intro(1);
	/* Secret-level startup normally opens a modal stars-screen message.  Mark
	 * only the load itself as demo playback so the no-window executable takes
	 * the existing noninteractive branch, then restore normal simulation. */
	if (level < 0)
		Newdemo_state = ND_STATE_PLAYBACK;
	StartNewGame(level);
	if (level < 0)
		Newdemo_state = ND_STATE_NORMAL;
	if (!route_confirmation_start()) {
		fprintf(stderr, "ROUTE-CONFIRM FAIL start %s\n",
		        route_confirmation_get_summary()->problem);
	}
	while (!route_confirmation_is_terminal()) {
		route_confirmation_prepare_frame_time();
		calc_game_time();
		GameProcessFrame();
	}
	{
		const route_confirmation_summary *summary =
		    route_confirmation_get_summary();
		if (!route_confirmation_write_json(output, mission, level, error,
		                                   sizeof(error))) {
			fprintf(stderr, "ROUTE-CONFIRM FAIL %s\n", error);
			return 1;
		}
		printf("ROUTE-CONFIRM %s mission=%s level=%d frames=%u out=%s\n",
		       summary->status == ROUTE_CONFIRMATION_CONFIRMED ? "OK" : "FAIL",
		       mission && *mission ? mission : "d2", level,
		       summary->frame_count, output);
		args_exit();
		return summary->status == ROUTE_CONFIRMATION_CONFIRMED ? 0 : 2;
	}
}
