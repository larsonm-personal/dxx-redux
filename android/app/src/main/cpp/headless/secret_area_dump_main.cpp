#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>

#include <nlohmann/json.hpp>

#include <SDL.h>

extern "C" {
#include "args.h"
#include "bm.h"
#include "config.h"
#include "console.h"
#include "digi.h"
#include "dxxerror.h"
#include "game.h"
#include "gameseq.h"
#include "gamesave.h"
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
#include "wall.h"
}

#ifdef DXX_BUILD_DESCENT_II
extern "C" void piggy_init_pigfile(char *filename);
#endif

static unsigned char *headless_screen_pixels = NULL;
static int secret_area_dump_failed = 0;

static void trace_dump_init(const char *stage)
{
	if (getenv("DXX_SECRET_AREA_DUMP_TRACE")) {
		fprintf(stderr, "SECRET-AREA-DUMP TRACE %s\n", stage);
		fflush(stderr);
	}
}

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
	trace_dump_init("mem_init");
	mem_init();
	trace_dump_init("error_init");
	error_init(msgbox_error);
	set_warn_func(msgbox_warning);
	trace_dump_init("physfs_init");
	PHYSFSX_init(argc, argv);
	trace_dump_init("con_init");
	con_init();
	if (GameArg.SysShowCmdHelp) {
		snprintf(error, error_size, "%s", "help requested");
		return 0;
	}
	trace_dump_init("archive_type_check");
	if (!PHYSFSX_checkSupportedArchiveTypes()) {
		snprintf(error, error_size, "%s", "archive type check failed");
		return 0;
	}
#ifdef DXX_BUILD_DESCENT_II
	trace_dump_init("contfile_d2");
	if (!PHYSFSX_contfile_init("descent2.hog", 1) &&
	    !PHYSFSX_contfile_init("d2demo.hog", 1)) {
		snprintf(error, error_size, "%s",
		         "could not find descent2.hog or d2demo.hog; pass -hogdir <dir> with Descent 2 data files");
		return 0;
	}
#else
	trace_dump_init("contfile_d1");
	if (!PHYSFSX_contfile_init("descent.hog", 1)) {
		snprintf(error, error_size, "%s",
		         "could not find descent.hog; pass -hogdir <dir> with Descent data files");
		return 0;
	}
#endif
	trace_dump_init("load_text");
	load_text();
	trace_dump_init("read_config");
	ReadConfigFile();
	trace_dump_init("audio");
	if (!init_headless_audio()) {
		snprintf(error, error_size, "%s", "audio init failed");
		return 0;
	}
	trace_dump_init("archive_content");
	PHYSFSX_addArchiveContent();
	trace_dump_init("gamedata");
	gamedata_init();
	trace_dump_init("texmerge");
	texmerge_init(10);
#ifdef DXX_BUILD_DESCENT_II
	{
		char groupa_pig[] = "groupa.pig";
		trace_dump_init("piggy_init_pigfile");
		piggy_init_pigfile(groupa_pig);
	}
#endif
	trace_dump_init("screen");
	if (!init_headless_screen(error, error_size))
		return 0;
	Screen_mode = SCREEN_GAME;
	trace_dump_init("init_game");
	init_game();
	Players[Player_num].callsign[0] = '\0';
	GameArg.SysUseNiceFPS = 0;
#ifdef DXX_BUILD_DESCENT_II
	GameArg.SysInputDemoNoRender = 1;
#endif
	trace_dump_init("runtime_done");
	return 1;
}

static int load_base_mission(char *error, size_t error_size)
{
#ifdef DXX_BUILD_DESCENT_II
	char d2_mission[] = "d2";
	char d2_demo_mission[] = "d2demo";

	trace_dump_init("load_mission_d2");
	if (load_mission_by_name(d2_mission))
		return 1;
	if (load_mission_by_name(d2_demo_mission))
		return 1;
	snprintf(error, error_size, "%s", "could not load d2 or d2demo mission");
	return 0;
#else
	char d1_mission[] = "";

	trace_dump_init("load_mission_d1");
	if (load_mission_by_name(d1_mission))
		return 1;
	snprintf(error, error_size, "%s", "could not load built-in d1 mission");
	return 0;
#endif
}

static int dump_wall_clip_flags(int wall_num)
{
	int clip_num;

	if (wall_num < 0 || wall_num >= Num_walls)
		return 0;
	clip_num = Walls[wall_num].clip_num;
	if (clip_num < 0 || clip_num >= Num_wall_anims)
		return 0;
	return WallAnims[clip_num].flags;
}

static void trace_wall_inventory(int level_num, const char *level_file)
{
	int wall_type_counts[8] = { 0 };
	int hidden_clip_by_type[8] = { 0 };
	int type;
	int wall_num;

	if (!getenv("DXX_SECRET_AREA_DUMP_TRACE"))
		return;
	for (wall_num = 0; wall_num < Num_walls; ++wall_num) {
		type = Walls[wall_num].type;
		if (type < 0 || type >= (int) (sizeof(wall_type_counts) / sizeof(wall_type_counts[0])))
			type = 0;
		wall_type_counts[type]++;
		if (dump_wall_clip_flags(wall_num) & WCF_HIDDEN)
			hidden_clip_by_type[type]++;
	}
	fprintf(stderr,
	        "SECRET-AREA-DUMP WALLS level=%d file=%s walls=%d normal=%d blastable=%d door=%d illusion=%d open=%d hidden_normal=%d hidden_blastable=%d hidden_door=%d hidden_illusion=%d hidden_open=%d\n",
	        level_num,
	        level_file ? level_file : "",
	        Num_walls,
	        wall_type_counts[WALL_NORMAL],
	        wall_type_counts[WALL_BLASTABLE],
	        wall_type_counts[WALL_DOOR],
	        wall_type_counts[WALL_ILLUSION],
	        wall_type_counts[WALL_OPEN],
	        hidden_clip_by_type[WALL_NORMAL],
	        hidden_clip_by_type[WALL_BLASTABLE],
	        hidden_clip_by_type[WALL_DOOR],
	        hidden_clip_by_type[WALL_ILLUSION],
	        hidden_clip_by_type[WALL_OPEN]);
	fflush(stderr);
}

static nlohmann::ordered_json serialize_int_array(const int *values, int count)
{
	nlohmann::ordered_json result = nlohmann::ordered_json::array();

	for (int index = 0; index < count; ++index)
		result.push_back(values[index]);
	return result;
}

static nlohmann::ordered_json serialize_entrance(const secret_area_entrance &entrance)
{
	nlohmann::ordered_json result;

	result["seg"] = entrance.seg;
	result["side"] = entrance.side;
	result["secret_seg"] = entrance.secret_seg;
	result["wall_num"] = entrance.wall_num;
	result["wall_type"] = entrance.wall_num >= 0 && entrance.wall_num < Num_walls ? (int) Walls[entrance.wall_num].type : -1;
	result["wall_flags"] = entrance.wall_num >= 0 && entrance.wall_num < Num_walls ? (int) Walls[entrance.wall_num].flags : 0;
	result["wall_clip_flags"] = dump_wall_clip_flags(entrance.wall_num);
	return result;
}

static nlohmann::ordered_json serialize_item(const secret_area_item &item)
{
	char fallback[32];
	nlohmann::ordered_json result;

	snprintf(fallback, sizeof(fallback), "powerup %d", item.id);
	result["id"] = item.id;
	result["name"] = item.name[0] ? item.name : fallback;
	result["count"] = item.count;
	result["direct_count"] = item.direct_count;
	result["contained_count"] = item.contained_count;
	return result;
}

static nlohmann::ordered_json serialize_secret(const secret_area_entry &secret)
{
	char label[16];
	nlohmann::ordered_json result;
	nlohmann::ordered_json items = nlohmann::ordered_json::array();
	nlohmann::ordered_json entrances = nlohmann::ordered_json::array();

	snprintf(label, sizeof(label), "S%d", secret.display_index);
	result["id"] = label;
	result["display_index"] = secret.display_index;
	result["entry_distance"] = secret.entry_distance;
	result["entry_seg"] = secret.entry_seg;
	result["entry_side"] = secret.entry_side;
	result["lowest_segment"] = secret.lowest_segment;
	result["label_pos"] = serialize_int_array(secret.label_pos, 3);
	result["robot_count"] = secret.robot_count;
	result["robotmaker_count"] = secret.robotmaker_count;
	result["item_count"] = secret.item_count;
	for (int index = 0; index < secret.item_count; ++index)
		items.push_back(serialize_item(secret.items[index]));
	result["items"] = items;
	for (int index = 0; index < secret.entrance_count; ++index)
		entrances.push_back(serialize_entrance(secret.entrances[index]));
	result["entrances"] = entrances;
	result["segments"] = serialize_int_array(secret.segments, secret.segment_count);
	return result;
}

static nlohmann::ordered_json serialize_current_level(int level_num, const char *level_file)
{
	const secret_area_state *state = secret_area_get_state();
	int total = secret_area_total(state);
	nlohmann::ordered_json result;
	nlohmann::ordered_json secrets = nlohmann::ordered_json::array();

	result["level_num"] = level_num;
	result["level_name"] = Current_level_name;
	result["level_file"] = level_file ? level_file : "";
	result["scanner_enabled"] = state->enabled ? true : false;
	result["disabled_reason"] = secret_area_disabled_reason_name(state->disabled_reason);
	result["raw_candidate_count"] = state->raw_candidate_count;
	result["final_candidate_count"] = state->final_candidate_count;
	result["secret_count"] = total;
	for (int index = 0; index < total; ++index)
		secrets.push_back(serialize_secret(state->secrets[index]));
	result["secrets"] = secrets;
	return result;
}

static int dump_level(nlohmann::ordered_json &levels, int level_num, const char *level_file)
{
	const secret_area_state *state;
	int total;

	trace_dump_init("load_level");
	if (load_level(level_file)) {
		fprintf(stderr, "SECRET-AREA-DUMP FAIL level could not load %s\n", level_file ? level_file : "<null>");
		secret_area_dump_failed = 1;
		return 0;
	}
	trace_wall_inventory(level_num, level_file);
	Current_level_num = level_num;
	trace_dump_init("rescan_level");
	secret_area_rescan_current_level();
	state = secret_area_get_state();
	total = secret_area_total(state);
	if (getenv("DXX_SECRET_AREA_DUMP_TRACE")) {
		fprintf(stderr, "SECRET-AREA-DUMP TRACE level=%d file=%s enabled=%d reason=%s raw=%d final=%d total=%d\n",
		        level_num,
		        level_file ? level_file : "",
		        state->enabled,
		        secret_area_disabled_reason_name(state->disabled_reason),
		        state->raw_candidate_count,
		        state->final_candidate_count,
		        total);
		fflush(stderr);
	}
	levels.push_back(serialize_current_level(level_num, level_file));
	return total;
}

static nlohmann::ordered_json build_dump(int *total_secrets)
{
	nlohmann::ordered_json root;
	nlohmann::ordered_json levels = nlohmann::ordered_json::array();
	int secret_total = 0;

	root["schema"] = "dxx-secret-area-baseline-v1";
	root["algorithm_version"] = 2;
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
	root["levels"] = levels;
	trace_dump_init("assemble_total");
	root["total_secret_count"] = secret_total;
	trace_dump_init("write_dump_done");
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

	trace_dump_init("open_output");
	std::ofstream stream(json_out);
	if (!stream) {
		fprintf(stderr, "SECRET-AREA-DUMP FAIL output could not open %s\n", json_out);
		return 1;
	}
	stream << build_dump(&total_secrets).dump(2) << "\n";
	if (secret_area_dump_failed)
		return 1;
	trace_dump_init("write_done");
	if (!stream) {
		fprintf(stderr, "SECRET-AREA-DUMP FAIL output could not write %s\n", json_out);
		return 1;
	}
	printf("SECRET-AREA-DUMP OK secrets=%d out=%s\n", total_secrets, json_out);
	return 0;
}
