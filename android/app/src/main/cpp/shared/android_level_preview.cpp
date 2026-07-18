#include "android_level_preview.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <physfs.h>
#include <SDL.h>

extern "C" {
#include "args.h"
#include "segment.h"
#include "automap.h"
#include "bm.h"
#include "event.h"
#include "game.h"
#include "gameseq.h"
#include "gamesave.h"
#include "gr.h"
#include "inferno.h"
#include "mission.h"
#include "object.h"
#include "physfsx.h"
#include "player.h"
#include "screens.h"
#include "secretarea.h"
#include "startup_resume_shared.h"
#include "texmerge.h"
#include "window.h"

#include "android_log.h"
}

#ifdef DXX_BUILD_DESCENT_II
extern "C" void piggy_init_pigfile(char *filename);
#endif

using json = nlohmann::ordered_json;

static std::string Level_preview_error;

static int preview_fail(const std::string &message)
{
	Level_preview_error = message;
	debug_log(DLOG_GAME, "level preview failed: %s", message.c_str());
	return 1;
}

static const char *physfs_error(void)
{
	const char *error = PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());
	return error ? error : "unknown error";
}

static std::vector<std::string> json_strings(const json &request, const char *name)
{
	std::vector<std::string> values;
	const json::const_iterator found = request.find(name);
	if (found == request.end() || !found->is_array())
		return values;
	for (const json &value : *found)
		if (value.is_string())
			values.push_back(value.get<std::string>());
	return values;
}

static int mount_preview_content(const json &request)
{
	const std::string extra_data_dir = request.value("extra_data_dir", "");
	const std::string mission_filename = request.value("mission_filename", "");
	std::vector<std::string> hog_paths = json_strings(request, "hog_paths");
	if (hog_paths.empty()) {
		const std::string hog_path = request.value("hog_path", "");
		if (!hog_path.empty())
			hog_paths.push_back(hog_path);
	}
	if (!extra_data_dir.empty()) {
		const bool flattened_mission_dir =
		    !mission_filename.empty() && mission_filename.find('/') == std::string::npos &&
		    mission_filename.find('\\') == std::string::npos;
		const int mounted = flattened_mission_dir
		                        ? PHYSFS_mount(extra_data_dir.c_str(), MISSION_DIR, 0)
		                        : PHYSFS_addToSearchPath(extra_data_dir.c_str(), 0);
		if (!mounted)
			return preview_fail(std::string("Could not mount preview mission files: ") + physfs_error());
	}
	for (const std::string &hog_path : hog_paths)
		if (!PHYSFS_mount(hog_path.c_str(), NULL, 0))
			return preview_fail(std::string("Could not mount preview HOG: ") + physfs_error());
	return 0;
}

static int load_preview_mission(const json &request)
{
	std::string mission = request.value("mission_name", "");
	if (mission.empty())
		return 0;
	std::vector<char> mission_name(mission.begin(), mission.end());
	mission_name.push_back('\0');
	if (!load_mission_by_name(mission_name.data()))
		return preview_fail(std::string("Could not load preview mission ") + mission);
	return 0;
}

static int select_preview_player(void)
{
	int start_objnum = -1;
	for (int objnum = 0; objnum <= Highest_object_index; ++objnum) {
		if (Objects[objnum].type == OBJ_PLAYER || Objects[objnum].type == OBJ_GHOST) {
			start_objnum = objnum;
			break;
		}
	}
	if (start_objnum < 0)
		return preview_fail("Level has no normal player start");
	Player_num = 0;
	N_players = 1;
	Players[0].objnum = start_objnum;
	Players[0].flags |= PLAYER_FLAGS_MAP_ALL;
	Objects[start_objnum].type = OBJ_PLAYER;
	Objects[start_objnum].id = 0;
	ConsoleObject = &Objects[start_objnum];
	Viewer = ConsoleObject;
	return 0;
}

static int preview_base_window_handler(window *wind, d_event *event, void *data)
{
	(void) data;
	if (event->type == EVENT_QUIT) {
		window_close(wind);
		return 1;
	}
	if (event->type == EVENT_WINDOW_CLOSE) {
		if (Game_wind == wind)
			Game_wind = NULL;
		return 1;
	}
	return 0;
}

extern "C" const char *android_level_preview_request_path(void)
{
	const int index = startup_find_cmd_arg("-level-preview-request");
	return index > 0 && index + 1 < Num_args ? Args[index + 1] : NULL;
}

extern "C" int android_level_preview_run(const char *request_path)
{
	const Uint32 started_at = SDL_GetTicks();
	Level_preview_error.clear();
	if (!request_path || !request_path[0])
		return preview_fail("Preview request path is missing");

	json request;
	try {
		std::ifstream stream(request_path);
		if (!stream)
			return preview_fail("Preview request file is missing");
		stream >> request;
	} catch (const std::exception &error) {
		return preview_fail(std::string("Could not parse preview request: ") + error.what());
	}
	if (request.value("schema", "") != "dxx-level-preview-request-v1")
		return preview_fail("Unsupported preview request schema");

	const std::string preview_write_dir = request.value("preview_write_dir", "");
	if (preview_write_dir.empty() || !PHYSFS_setWriteDir(preview_write_dir.c_str()) ||
	    !PHYSFS_addToSearchPath(preview_write_dir.c_str(), 0))
		return preview_fail(std::string("Could not isolate preview writes: ") + physfs_error());
	if (mount_preview_content(request))
		return 1;

	gamedata_init();
	texmerge_init(10);
#ifdef DXX_BUILD_DESCENT_II
	{
		char groupa_pig[] = "groupa.pig";
		piggy_init_pigfile(groupa_pig);
	}
#endif
	init_game();
	Players[Player_num].callsign[0] = '\0';
	if (load_preview_mission(request))
		return 1;

	const std::string level_file = request.value("level_file", "");
	if (level_file.empty() || !PHYSFSX_exists(level_file.c_str(), 1))
		return preview_fail("Preview level file is missing");
	if (load_level(level_file.c_str()))
		return preview_fail(std::string("Could not load preview level ") + level_file);
	Current_level_num = request.value("level_num", 1);
	Game_mode = GM_NORMAL;
	if (select_preview_player())
		return 1;
	secret_area_rescan_current_level();
	secret_area_set_reveal_unfound(0);
	level_metadata_set_objective_mode(LEVEL_METADATA_OBJECTIVES_OFF);
	level_metadata_rescan_route_from_object(Players[0].objnum);
	set_screen_mode(SCREEN_GAME);

	Game_wind = window_create(
	    &grd_curscreen->sc_canvas, 0, 0, SWIDTH, SHEIGHT,
	    preview_base_window_handler, NULL);
	if (!Game_wind)
		return preview_fail("Could not create preview window");
	do_automap();
	if (!Automap_active) {
		window_close(Game_wind);
		return preview_fail("Could not open automap preview");
	}
	debug_log(DLOG_GAME, "level preview first frame ready in %u ms level=%s",
	          (unsigned int) (SDL_GetTicks() - started_at), level_file.c_str());
	while (Automap_active && window_get_front())
		event_process();
	if (Game_wind)
		window_close(Game_wind);
	debug_log(DLOG_GAME, "level preview closed after %u ms",
	          (unsigned int) (SDL_GetTicks() - started_at));
	return 0;
}

extern "C" const char *android_level_preview_last_error(void)
{
	return Level_preview_error.c_str();
}
