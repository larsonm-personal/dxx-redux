#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <cmath>
#include <string>

#include <nlohmann/json.hpp>
#include <physfs.h>
#include <SDL.h>

/* Include this before the legacy engine headers, some of which intentionally
 * change structure packing for on-disk formats. */
#include "route_confirmation.h"

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
#include "maths.h"
#include "messagebox.h"
#include "mission.h"
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

double fixed_seconds(int64_t ticks)
{
	const double seconds =
	    static_cast<double>(ticks) / static_cast<double>(F1_0);
	return std::round(seconds * 1000000.0) / 1000000.0;
}

nlohmann::ordered_json serialize_result(const route_confirmation_summary &summary,
                                        const char *mission, int level)
{
	nlohmann::ordered_json result;
	nlohmann::ordered_json objectives = nlohmann::ordered_json::array();
	nlohmann::ordered_json objective_seconds = nlohmann::ordered_json::array();
	result["schema"] = "dxx-guidebot-route-confirmation-v1";
	result["mission"] = mission && *mission ? mission : "d2";
	result["level"] = level;
	result["status"] = route_confirmation_status_name(summary.status);
	result["seed"] = summary.seed;
	result["fixed_hz"] = summary.fixed_hz;
	result["frames"] = summary.frame_count;
	result["simulation_seconds"] = fixed_seconds(summary.elapsed_ticks);
	result["rng_state"] = summary.rng_state;
	result["rng_call_count"] = summary.rng_call_count;
	result["radius"] = {
		{ "player", summary.player_radius },
		{ "guidebot", summary.guidebot_radius },
		{ "effective", summary.effective_radius }
	};
	if (summary.problem[0])
		result["problem"] = summary.problem;
	for (int index = 0; index < summary.objective_count; ++index) {
		const route_confirmation_objective_result &objective =
		    summary.objectives[index];
		nlohmann::ordered_json item;
		item["route_step_index"] = objective.route_step_index;
		item["kind"] = objective.kind;
		item["activation_kind"] = objective.activation_kind;
		item["label"] = objective.label;
		item["frame"] = objective.completed_frame;
		item["seconds"] = fixed_seconds(objective.completed_ticks);
		objectives.push_back(item);
		objective_seconds.push_back(fixed_seconds(objective.completed_ticks));
	}
	result["objectives"] = objectives;
	if (summary.status == ROUTE_CONFIRMATION_CONFIRMED) {
		result["route_confirmation"] = {
			{ "status", "confirmed" },
			{ "generation", 1 },
			{ "seed", ROUTE_CONFIRMATION_CANONICAL_SEED },
			{ "fixed_hz", ROUTE_CONFIRMATION_FIXED_HZ },
			{ "objective_seconds", objective_seconds },
			{ "total_seconds", fixed_seconds(summary.elapsed_ticks) }
		};
	}
	return result;
}
} // namespace

int main(int argc, char *argv[])
{
	char error[256] = "";
	const char *output = find_arg_value(argc, argv, "-route-confirm-json-out");
	const char *mission = find_arg_value(argc, argv, "-mission");
	const char *level_text = find_arg_value(argc, argv, "-level");
	int level = 0;
	if (!output || !parse_level(level_text, &level)) {
		fprintf(stderr,
		        "usage: %s -route-confirm-json-out <path> -level <number> "
		        "[-mission <name>] [-hogdir <dir>]\n",
		        argc > 0 ? argv[0] : "dxx-redux-d2-headless-route");
		return 1;
	}
	if (!init_headless_runtime(argc, argv, error, sizeof(error)) ||
	    !load_requested_mission(mission, error, sizeof(error))) {
		fprintf(stderr, "ROUTE-CONFIRM FAIL init %s\n",
		        error[0] ? error : "runtime initialization failed");
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
	StartNewGame(level);
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
		std::ofstream stream(output);
		if (!stream) {
			fprintf(stderr, "ROUTE-CONFIRM FAIL could not open output %s\n",
			        output);
			return 1;
		}
		stream << serialize_result(*summary, mission, level).dump(2) << "\n";
		if (!stream) {
			fprintf(stderr, "ROUTE-CONFIRM FAIL could not write output %s\n",
			        output);
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
