#include <stdio.h>
#include <string.h>

#include <fstream>

#include <SDL.h>
#include <nlohmann/json.hpp>

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
#include "messagebox.h"
#include "mission.h"
#include "physfsx.h"
#include "player.h"
#include "screens.h"
#include "secret_area_scan.h"
#include "secretarea.h"
#include "songs.h"
#include "texmerge.h"
#include "text.h"
#include "u_mem.h"
}

#ifdef DXX_BUILD_DESCENT_II
extern "C" void piggy_init_pigfile(char *filename);
#endif

static unsigned char *headless_screen_pixels = NULL;

static const char *find_arg_value(int argc, char *argv[], const char *name)
{
	for (int index = 1; index + 1 < argc; ++index)
		if (!strcmp(argv[index], name))
			return argv[index + 1];
	return NULL;
}

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

static int init_headless_screen(char *error, size_t error_size)
{
	const int screen_w = (int) SM_W(Game_screen_mode);
	const int screen_h = (int) SM_H(Game_screen_mode);

	if (grd_curscreen)
		return 1;
	grd_curscreen = (grs_screen *) d_calloc(1, sizeof(grs_screen));
	MALLOC(headless_screen_pixels, unsigned char, screen_w *screen_h);
	if (!grd_curscreen || !headless_screen_pixels) {
		snprintf(error, error_size, "%s", "screen allocation failed");
		return 0;
	}
	memset(headless_screen_pixels, 0, (size_t) (screen_w * screen_h));
	grd_curscreen->sc_mode = Game_screen_mode;
	grd_curscreen->sc_w = (short) screen_w;
	grd_curscreen->sc_h = (short) screen_h;
	grd_curscreen->sc_aspect = fixdiv(grd_curscreen->sc_w * GameCfg.AspectX,
	                                  grd_curscreen->sc_h * GameCfg.AspectY);
	gr_init_canvas(&grd_curscreen->sc_canvas, headless_screen_pixels, BM_LINEAR, screen_w, screen_h);
	gr_set_current_canvas(NULL);
	return 1;
}

static int init_secret_area_runtime(int argc, char *argv[], char *error, size_t error_size)
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
#ifdef DXX_BUILD_DESCENT_II
	if (!PHYSFSX_contfile_init("descent2.hog", 1) &&
	    !PHYSFSX_contfile_init("d2demo.hog", 1)) {
		snprintf(error, error_size, "%s",
		         "could not find descent2.hog or d2demo.hog; pass -hogdir <dir> with Descent 2 data files");
		return 0;
	}
#else
	if (!PHYSFSX_contfile_init("descent.hog", 1)) {
		snprintf(error, error_size, "%s",
		         "could not find descent.hog; pass -hogdir <dir> with Descent data files");
		return 0;
	}
#endif
	load_text();
	ReadConfigFile();
	if (!init_headless_audio()) {
		snprintf(error, error_size, "%s", "audio init failed");
		return 0;
	}
	PHYSFSX_addArchiveContent();
	gamedata_init();
	texmerge_init(10);
#ifdef DXX_BUILD_DESCENT_II
	{
		char groupa_pig[] = "groupa.pig";
		piggy_init_pigfile(groupa_pig);
	}
#endif
	if (!init_headless_screen(error, error_size))
		return 0;
	Screen_mode = SCREEN_GAME;
	init_game();
	Players[Player_num].callsign[0] = '\0';
	GameArg.SysUseNiceFPS = 0;
#ifdef DXX_BUILD_DESCENT_II
	GameArg.SysInputDemoNoRender = 1;
#endif
	return 1;
}

static int load_base_mission(char *error, size_t error_size)
{
#ifdef DXX_BUILD_DESCENT_II
	char d2_mission[] = "d2";
	char d2_demo_mission[] = "d2demo";

	if (load_mission_by_name(d2_mission))
		return 1;
	if (load_mission_by_name(d2_demo_mission))
		return 1;
	snprintf(error, error_size, "%s", "could not load d2 or d2demo mission");
	return 0;
#else
	char d1_mission[] = "";

	if (load_mission_by_name(d1_mission))
		return 1;
	snprintf(error, error_size, "%s", "could not load built-in d1 mission");
	return 0;
#endif
}

static nlohmann::ordered_json serialize_secret(const secret_area_entry &secret)
{
	nlohmann::ordered_json item;
	nlohmann::ordered_json entrances = nlohmann::ordered_json::array();
	nlohmann::ordered_json segments = nlohmann::ordered_json::array();
	char label[16];

	snprintf(label, sizeof(label), "S%d", secret.display_index);
	item["id"] = label;
	item["display_index"] = secret.display_index;
	item["entry_distance"] = secret.entry_distance;
	item["entry_seg"] = secret.entry_seg;
	item["entry_side"] = secret.entry_side;
	item["lowest_segment"] = secret.lowest_segment;
	item["label_pos"] = { secret.label_pos[0], secret.label_pos[1], secret.label_pos[2] };
	item["robot_count"] = secret.robot_count;
	item["robotmaker_count"] = secret.robotmaker_count;
	for (int index = 0; index < secret.entrance_count; ++index) {
		const secret_area_entrance &entrance = secret.entrances[index];
		entrances.push_back({
		    { "seg", entrance.seg },
		    { "side", entrance.side },
		    { "secret_seg", entrance.secret_seg },
		    { "wall_num", entrance.wall_num },
		});
	}
	for (int index = 0; index < secret.segment_count; ++index)
		segments.push_back(secret.segments[index]);
	item["entrances"] = entrances;
	item["segments"] = segments;
	return item;
}

static nlohmann::ordered_json serialize_current_level(int level_num, const char *level_file)
{
	const secret_area_state *state = secret_area_get_state();
	nlohmann::ordered_json level;
	nlohmann::ordered_json secrets = nlohmann::ordered_json::array();

	level["level_num"] = level_num;
	level["level_name"] = Current_level_name;
	level["level_file"] = level_file ? level_file : "";
	level["scanner_enabled"] = state->enabled != 0;
	level["disabled_reason"] = secret_area_disabled_reason_name(state->disabled_reason);
	level["raw_candidate_count"] = state->raw_candidate_count;
	level["final_candidate_count"] = state->final_candidate_count;
	level["secret_count"] = secret_area_total(state);
	for (int index = 0; index < secret_area_total(state); ++index)
		secrets.push_back(serialize_secret(state->secrets[index]));
	level["secrets"] = secrets;
	return level;
}

static int dump_level(nlohmann::ordered_json &levels, int level_num, const char *level_file)
{
	LoadLevel(level_num, 0);
	levels.push_back(serialize_current_level(level_num, level_file));
	return secret_area_total(secret_area_get_state());
}

static nlohmann::ordered_json build_dump(int *total_secrets)
{
	nlohmann::ordered_json root;
	nlohmann::ordered_json levels = nlohmann::ordered_json::array();
	int secret_total = 0;

	root["schema"] = "dxx-secret-area-baseline-v1";
	root["algorithm_version"] = 1;
	root["max_generated_secrets"] = SECRET_AREA_MAX_GENERATED;
#ifdef DXX_BUILD_DESCENT_II
	root["game"] = "d2";
#else
	root["game"] = "d1";
#endif
	root["mission_name"] = Current_mission_longname;
	root["mission_filename"] = Current_mission_filename;
	for (int level = 1; level <= Last_level; ++level)
		secret_total += dump_level(levels, level, Level_names[level - 1]);
	for (int level = -1; level >= Last_secret_level; --level)
		secret_total += dump_level(levels, level, Secret_level_names[-level - 1]);
	root["total_secret_count"] = secret_total;
	root["levels"] = levels;
	if (total_secrets)
		*total_secrets = secret_total;
	return root;
}

int main(int argc, char *argv[])
{
	char error[256] = "";
	const char *json_out = find_arg_value(argc, argv, "-secretarea-json-out");
	int total_secrets = 0;

	if (!json_out) {
		fprintf(stderr, "usage: %s -secretarea-json-out <path> [-hogdir <game-data-dir>]\n",
		        argc > 0 ? argv[0] : "dxx-redux-secretareas");
		return 1;
	}
	if (!init_secret_area_runtime(argc, argv, error, sizeof(error))) {
		fprintf(stderr, "SECRET-AREA-DUMP FAIL init %s\n", error[0] ? error : "runtime init failed");
		return 1;
	}
	if (!load_base_mission(error, sizeof(error))) {
		fprintf(stderr, "SECRET-AREA-DUMP FAIL mission %s\n", error[0] ? error : "mission load failed");
		return 1;
	}

	const nlohmann::ordered_json dump = build_dump(&total_secrets);
	std::ofstream stream(json_out);
	if (!stream) {
		fprintf(stderr, "SECRET-AREA-DUMP FAIL output could not open %s\n", json_out);
		return 1;
	}
	stream << dump.dump(2) << "\n";
	if (!stream) {
		fprintf(stderr, "SECRET-AREA-DUMP FAIL output could not write %s\n", json_out);
		return 1;
	}
	printf("SECRET-AREA-DUMP OK levels=%zu secrets=%d out=%s\n", dump["levels"].size(), total_secrets, json_out);
	return 0;
}
