#include "android_level_preview.h"

#include <atomic>
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
#ifdef DXX_BUILD_DESCENT_II
#include "gamepal.h"
#endif
#include "gameseq.h"
#include "gamesave.h"
#include "gr.h"
#include "inferno.h"
#include "kconfig.h"
#include "mission.h"
#include "object.h"
#ifdef OGL
#include "ogl_init.h"
#endif
#include "physfsx.h"
#include "playsave.h"
#include "palette.h"
#include "player.h"
#include "polyobj.h"
#include "robot.h"
#include "screens.h"
#include "secretarea.h"
#include "startup_resume_shared.h"
#include "texmerge.h"
#include "timer.h"
#include "window.h"

#include "android_log.h"
#include "android_loading_progress.h"
#include "level_metadata_scan.h"
}

#ifdef DXX_BUILD_DESCENT_II
extern "C" void piggy_init_pigfile(char *filename);
#endif

using json = nlohmann::ordered_json;

static std::string Level_preview_error;
static json Level_preview_request;
static std::string Level_preview_introspection;
static Uint32 Level_preview_started_at;
static Uint32 Level_preview_first_frame_at;
static unsigned long long Level_preview_event_iterations;
static int Level_preview_player_objnum = -1;
static int Level_preview_player_segment = -1;
static int Level_preview_is_active;
static int Level_preview_palette_ready;
static std::string Level_preview_palette_name;
static std::atomic<int> Level_preview_close_requested(0);
static int Level_preview_loading_progress_completed;
static int Level_preview_loading_progress_max_percent;
static unsigned long long Level_preview_metadata_progress_callbacks;
static std::atomic<int> Robot_preview_close_requested(0);
static std::atomic<int> Robot_preview_heading(F0_5 - 1);
static std::atomic<int> Robot_preview_pitch(0);
static std::atomic<int> Robot_preview_active(0);
static int Robot_preview_model = -1;
static int Robot_preview_number = -1;
static unsigned long long Robot_preview_frames;

class preview_loading_progress_guard
{
  public:
	preview_loading_progress_guard()
	{
		android_loading_progress_begin("Preparing Preview", 100);
		update("Reading preview request", 0);
	}

	~preview_loading_progress_guard()
	{
		finish();
	}

	void update(const char *label, int percent)
	{
		if (!active_)
			return;
		if (percent < Level_preview_loading_progress_max_percent)
			percent = Level_preview_loading_progress_max_percent;
		if (percent > 100)
			percent = 100;
		Level_preview_loading_progress_max_percent = percent;
		android_loading_progress_update(label, percent, 100);
	}

	void finish()
	{
		if (!active_)
			return;
		Level_preview_loading_progress_max_percent = 100;
		android_loading_progress_end();
		active_ = false;
		Level_preview_loading_progress_completed = 1;
	}

  private:
	bool active_ = true;
};

struct preview_metadata_progress_context {
	preview_loading_progress_guard *loading;
	int range_start;
	int range_end;
	int route_only;
	int max_percent;
};

static int preview_metadata_cancelled(void *user)
{
	(void) user;
	return Level_preview_close_requested.load(std::memory_order_acquire) != 0;
}

static void preview_metadata_progress(
    void *user, const char *stage, int completed, int total)
{
	preview_metadata_progress_context *context =
	    static_cast<preview_metadata_progress_context *>(user);
	const char *label = "Analyzing level";
	int range_start = 0;
	int range_end = 1000;
	int fraction = 0;

	if (!context || !context->loading || !stage)
		return;
	++Level_preview_metadata_progress_callbacks;
	if (total > 0) {
		if (completed < 0)
			completed = 0;
		if (completed > total)
			completed = total;
		fraction = static_cast<int>(
		    static_cast<long long>(completed) * 1000 / total);
	}
	if (!strcmp(stage, "level_topology")) {
		label = "Building map topology";
		range_end = context->route_only ? 1000 : 150;
	} else if (!strcmp(stage, "secret_areas")) {
		label = "Scanning secret areas";
		range_start = context->route_only ? 0 : 150;
		range_end = context->route_only ? 1000 : 250;
	} else if (!strcmp(stage, "level_summary")) {
		label = "Analyzing level";
		range_start = context->route_only ? 0 : 250;
		range_end = context->route_only ? 1000 : 350;
	} else if (!strcmp(stage, "route_visibility")) {
		label = "Checking firing paths";
		range_start = context->route_only ? 0 : 350;
		range_end = 950;
	} else if (!strcmp(stage, "route_target_visibility")) {
		label = "Checking objective visibility";
		range_start = context->route_only ? 0 : 350;
		range_end = 950;
	} else if (!strcmp(stage, "route_planning")) {
		label = "Planning objectives";
		if (completed >= total && total > 0) {
			range_start = 1000;
			range_end = 1000;
			fraction = 1000;
		} else {
			range_start = context->route_only ? 0 : 350;
			range_end = range_start;
		}
	}
	const int phase_progress =
	    range_start + (range_end - range_start) * fraction / 1000;
	int percent = context->range_start +
	              (context->range_end - context->range_start) *
	                  phase_progress / 1000;
	if (percent < context->max_percent)
		percent = context->max_percent;
	context->max_percent = percent;
	context->loading->update(label, percent);
}

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
	Objects[start_objnum].type = OBJ_PLAYER;
	Objects[start_objnum].id = 0;
	ConsoleObject = &Objects[start_objnum];
	Viewer = ConsoleObject;
	Level_preview_player_objnum = start_objnum;
	Level_preview_player_segment = Objects[start_objnum].segnum;
	return 0;
}

static void reveal_preview_automap_segments(void)
{
	Players[0].flags &= ~PLAYER_FLAGS_MAP_ALL;
	for (int segnum = 0; segnum <= Highest_segment_index; ++segnum)
		Automap_visited[segnum] = 1;
}

static void configure_preview_touch_axes(void)
{
	/* Use the engine's Android touch-overlay defaults without reading or
	 * writing a pilot profile.  Preview processes are deliberately isolated. */
	PlayerCfg.ControlType = CONTROL_USING_JOYSTICK;
	PlayerCfg.AutomapFreeFlight = 1;
	kconfig_get_default_settings(
	    PlayerCfg.KeySettings[0], PlayerCfg.KeySettings[1], PlayerCfg.KeySettings[2]);
	kc_set_controls();
	kconfig_set_joystick_item(13, 3);
	kconfig_set_joystick_item(15, 2);
	kconfig_set_joystick_item(17, 0);
	kconfig_set_joystick_item(19, 7);
	kconfig_set_joystick_item(21, 6);
	kconfig_set_joystick_item(23, 1);
}

static void load_preview_palette(void)
{
#ifdef DXX_BUILD_DESCENT_II
	load_palette(Current_level_palette, 1, 1);
	Level_preview_palette_name = Current_level_palette;
#else
	char palette_name[] = "palette.256";
	gr_use_palette_table(palette_name);
	Level_preview_palette_name = "palette.256";
#endif
	gr_palette_load(gr_palette);
#if defined(OGL) && defined(__ANDROID__)
	ogl_invalidate_game_palette_textures();
#endif
	Level_preview_palette_ready = 1;
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

static int robot_preview_window_handler(window *wind, d_event *event, void *data)
{
	(void) data;
	switch (event->type) {
		case EVENT_QUIT:
			window_close(wind);
			return 1;
		case EVENT_WINDOW_DRAW: {
			vms_angvec angles;
			angles.h = static_cast<fixang>(
			    Robot_preview_heading.fetch_add(40, std::memory_order_relaxed));
			angles.p = static_cast<fixang>(Robot_preview_pitch.load(std::memory_order_relaxed));
			angles.b = 0;
			draw_model_picture(Robot_preview_model, &angles);
			timer_delay(F1_0 / 60);
			return 1;
		}
		case EVENT_WINDOW_CLOSE:
			if (Game_wind == wind)
				Game_wind = NULL;
			return 0;
		default:
			return 0;
	}
}

static int run_robot_preview(
    const json &request, Uint32 started_at, preview_loading_progress_guard &loading)
{
	Robot_preview_close_requested.store(0, std::memory_order_release);
	Robot_preview_heading.store(F0_5 - 1, std::memory_order_release);
	Robot_preview_pitch.store(0, std::memory_order_release);
	Robot_preview_active.store(0, std::memory_order_release);
	Robot_preview_model = -1;
	Robot_preview_number = request.value("robot_number", -1);
	Robot_preview_frames = 0;
	loading.update("Mounting mission files", 5);

	const std::string preview_write_dir = request.value("preview_write_dir", "");
	if (preview_write_dir.empty() || !PHYSFS_setWriteDir(preview_write_dir.c_str()) ||
	    !PHYSFS_addToSearchPath(preview_write_dir.c_str(), 0))
		return preview_fail(std::string("Could not isolate robot preview writes: ") + physfs_error());
	if (mount_preview_content(request))
		return 1;

	loading.update("Loading game data", 15);
	gamedata_init();
	texmerge_init(10);
#ifdef DXX_BUILD_DESCENT_II
	{
		char groupa_pig[] = "groupa.pig";
		piggy_init_pigfile(groupa_pig);
	}
#endif
	loading.update("Initializing game", 35);
	init_game();
	new_player_config();
	Players[Player_num].callsign[0] = '\0';
	if (load_preview_mission(request))
		return 1;

	const std::string level_file = request.value("level_file", "");
	if (level_file.empty() || !PHYSFSX_exists(level_file.c_str(), 1))
		return preview_fail("Robot preview source level is missing");
	loading.update(level_file.c_str(), 55);
	if (load_level(level_file.c_str()))
		return preview_fail(std::string("Could not load robot preview level ") + level_file);
	Current_level_num = request.value("level_num", 1);
#ifdef DXX_BUILD_DESCENT_II
	load_level_robots_file(level_file.c_str());
#endif
	load_preview_palette();
	if (Robot_preview_number < 0 || Robot_preview_number >= N_robot_types)
		return preview_fail("Robot number is not available in the selected level");
	Robot_preview_model = Robot_info[Robot_preview_number].model_num;
	if (Robot_preview_model < 0 || Robot_preview_model >= N_polygon_models)
		return preview_fail("Robot model is not available in the selected level");

	loading.update("Opening robot viewer", 90);
	set_screen_mode(SCREEN_GAME);
	Game_wind = window_create(
	    &grd_curscreen->sc_canvas, 0, 0, SWIDTH, SHEIGHT,
	    robot_preview_window_handler, NULL);
	if (!Game_wind)
		return preview_fail("Could not create robot preview window");
	Robot_preview_active.store(1, std::memory_order_release);
	int first_frame_pending = 1;
	while (Game_wind) {
		if (Robot_preview_close_requested.exchange(0, std::memory_order_acq_rel)) {
			window_close(Game_wind);
			continue;
		}
		FrameTime = F1_0 / 60;
		event_process();
		++Robot_preview_frames;
		if (first_frame_pending) {
			first_frame_pending = 0;
			loading.finish();
			debug_log(DLOG_GAME, "robot preview first frame ready in %u ms robot=%d model=%d level=%s",
			          (unsigned int) (SDL_GetTicks() - started_at), Robot_preview_number,
			          Robot_preview_model, level_file.c_str());
		}
	}
	Robot_preview_active.store(0, std::memory_order_release);
	if (Game_wind)
		window_close(Game_wind);
	debug_log(DLOG_GAME, "robot preview closed after %u ms robot=%d",
	          (unsigned int) (SDL_GetTicks() - started_at), Robot_preview_number);
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
	Level_preview_request = json::object();
	Level_preview_introspection.clear();
	Level_preview_started_at = started_at;
	Level_preview_first_frame_at = 0;
	Level_preview_event_iterations = 0;
	Level_preview_player_objnum = -1;
	Level_preview_player_segment = -1;
	Level_preview_is_active = 0;
	Level_preview_palette_ready = 0;
	Level_preview_palette_name.clear();
	Level_preview_close_requested.store(0, std::memory_order_release);
	Level_preview_loading_progress_completed = 0;
	Level_preview_loading_progress_max_percent = 0;
	Level_preview_metadata_progress_callbacks = 0;
	preview_loading_progress_guard loading_progress;
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
	if (request.value("schema", "") == "dxx-robot-preview-request-v1") {
		Level_preview_request = request;
		return run_robot_preview(request, started_at, loading_progress);
	}
	if (request.value("schema", "") != "dxx-level-preview-request-v1")
		return preview_fail("Unsupported preview request schema");
	Level_preview_request = request;
	loading_progress.update("Mounting mission files", 5);

	const std::string preview_write_dir = request.value("preview_write_dir", "");
	if (preview_write_dir.empty() || !PHYSFS_setWriteDir(preview_write_dir.c_str()) ||
	    !PHYSFS_addToSearchPath(preview_write_dir.c_str(), 0))
		return preview_fail(std::string("Could not isolate preview writes: ") + physfs_error());
	if (mount_preview_content(request))
		return 1;

	loading_progress.update("Loading game data", 10);
	gamedata_init();
	texmerge_init(10);
#ifdef DXX_BUILD_DESCENT_II
	{
		char groupa_pig[] = "groupa.pig";
		piggy_init_pigfile(groupa_pig);
	}
#endif
	loading_progress.update("Initializing game", 25);
	init_game();
	/* Supply the in-memory sensitivity/deadzone defaults normally established
	 * when a pilot is created, without reading or writing any pilot file. */
	new_player_config();
	Players[Player_num].callsign[0] = '\0';
	loading_progress.update("Loading mission", 30);
	if (load_preview_mission(request))
		return 1;

	const std::string level_file = request.value("level_file", "");
	if (level_file.empty() || !PHYSFSX_exists(level_file.c_str(), 1))
		return preview_fail("Preview level file is missing");
	loading_progress.update(level_file.c_str(), 35);
	if (load_level(level_file.c_str()))
		return preview_fail(std::string("Could not load preview level ") + level_file);
	loading_progress.update("Preparing level palette", 45);
	load_preview_palette();
	Current_level_num = request.value("level_num", 1);
#ifdef DXX_BUILD_DESCENT_II
	load_level_robots_file(level_file.c_str());
#endif
	Game_mode = GM_NORMAL;
	if (select_preview_player())
		return 1;
	preview_metadata_progress_context canonical_progress = {
		&loading_progress, 48, 75, 0, 48
	};
	level_metadata_set_progress_callback(
	    preview_metadata_progress, &canonical_progress);
	level_metadata_set_cancel_callback(preview_metadata_cancelled, NULL);
	secret_area_rescan_current_level();
	if (preview_metadata_cancelled(NULL)) {
		level_metadata_set_cancel_callback(NULL, NULL);
		level_metadata_set_progress_callback(NULL, NULL);
		return preview_fail("Preview preparation cancelled");
	}
	secret_area_set_reveal_unfound(0);
	automap_clear_visited();
	reveal_preview_automap_segments();
	level_metadata_set_objective_mode(LEVEL_METADATA_OBJECTIVES_OFF);
	preview_metadata_progress_context live_progress = {
		&loading_progress, 75, 96, 1, 75
	};
	level_metadata_set_progress_callback(
	    preview_metadata_progress, &live_progress);
	level_metadata_rescan_route_from_object(Players[0].objnum);
	level_metadata_set_cancel_callback(NULL, NULL);
	level_metadata_set_progress_callback(NULL, NULL);
	if (preview_metadata_cancelled(NULL))
		return preview_fail("Preview preparation cancelled");
	loading_progress.update("Opening automap", 97);
	set_screen_mode(SCREEN_GAME);

	Game_wind = window_create(
	    &grd_curscreen->sc_canvas, 0, 0, SWIDTH, SHEIGHT,
	    preview_base_window_handler, NULL);
	if (!Game_wind)
		return preview_fail("Could not create preview window");
	configure_preview_touch_axes();
	loading_progress.update("Opening automap", 99);
	do_automap();
	if (!Automap_active) {
		window_close(Game_wind);
		return preview_fail("Could not open automap preview");
	}
	Level_preview_is_active = 1;
	int first_frame_pending = 1;
	while (Automap_active && window_get_front()) {
		if (Level_preview_close_requested.exchange(0, std::memory_order_acq_rel)) {
			window_close(window_get_front());
			continue;
		}
		/* The lightweight path has no pilot lifecycle to restore these after
		 * engine events that refresh control settings. */
		configure_preview_touch_axes();
		/* Automap pauses the engine timer.  Give its controls a stable render
		 * timestep without advancing GameTime or running game simulation. */
		FrameTime = F1_0 / 60;
		++Level_preview_event_iterations;
		event_process();
		if (first_frame_pending) {
			first_frame_pending = 0;
			Level_preview_first_frame_at = SDL_GetTicks();
			loading_progress.finish();
			debug_log(DLOG_GAME, "level preview first frame ready in %u ms level=%s",
			          (unsigned int) (Level_preview_first_frame_at - started_at),
			          level_file.c_str());
		}
	}
	Level_preview_is_active = 0;
	if (Game_wind)
		window_close(Game_wind);
	debug_log(DLOG_GAME, "level preview closed after %u ms",
	          (unsigned int) (SDL_GetTicks() - started_at));
	return 0;
}

extern "C" int android_level_preview_active(void)
{
	return Level_preview_is_active || Robot_preview_active.load(std::memory_order_acquire);
}

extern "C" void android_level_preview_request_close(void)
{
	Level_preview_close_requested.store(1, std::memory_order_release);
}

extern "C" const char *android_level_preview_introspection_json(void)
{
	if (Robot_preview_active.load(std::memory_order_acquire)) {
		const Uint32 now = SDL_GetTicks();
		json preview = {
			{ "schema", "dxx-robot-preview-introspection-v1" },
			{ "active", true },
			{ "request_id", Level_preview_request.value("request_id", "") },
			{ "game", Level_preview_request.value("game", "") },
			{ "mission_name", Level_preview_request.value("mission_name", "") },
			{ "level_file", Level_preview_request.value("level_file", "") },
			{ "level_num", Level_preview_request.value("level_num", 0) },
			{ "robot_number", Robot_preview_number },
			{ "robot_label", Level_preview_request.value("robot_label", "") },
			{ "model_number", Robot_preview_model },
			{ "palette_ready", Level_preview_palette_ready != 0 },
			{ "palette_name", Level_preview_palette_name },
			{ "frame_count", Robot_preview_frames },
			{ "heading", Robot_preview_heading.load(std::memory_order_relaxed) },
			{ "pitch", Robot_preview_pitch.load(std::memory_order_relaxed) },
			{ "close_requested", Robot_preview_close_requested.load(std::memory_order_relaxed) != 0 },
			{ "uptime_ms", now - Level_preview_started_at }
		};
		Level_preview_introspection = preview.dump();
		return Level_preview_introspection.c_str();
	}
	if (!Level_preview_is_active)
		return NULL;
	configure_preview_touch_axes();
	const Uint32 now = SDL_GetTicks();
	json preview = {
		{ "schema", "dxx-level-preview-introspection-v1" },
		{ "active", true },
		{ "request_id", Level_preview_request.value("request_id", "") },
		{ "game", Level_preview_request.value("game", "") },
		{ "source_name", Level_preview_request.value("source_name", "") },
		{ "mission_name", Level_preview_request.value("mission_name", "") },
		{ "mission_filename", Level_preview_request.value("mission_filename", "") },
		{ "level_file", Level_preview_request.value("level_file", "") },
		{ "level_num", Level_preview_request.value("level_num", 0) },
		{ "secret_level", Level_preview_request.value("secret_level", false) },
		{ "player_start_objnum", Level_preview_player_objnum },
		{ "player_start_segment", Level_preview_player_segment },
		{ "map_all", true },
		{ "map_powerup_active", false },
		{ "all_segments_visited", true },
		{ "palette_ready", Level_preview_palette_ready != 0 },
		{ "palette_name", Level_preview_palette_name },
		{ "event_iterations", Level_preview_event_iterations },
		{ "first_frame_ms", Level_preview_first_frame_at
		                        ? Level_preview_first_frame_at - Level_preview_started_at
		                        : 0 },
		{ "loading_progress_completed", Level_preview_loading_progress_completed != 0 },
		{ "loading_progress_max_percent", Level_preview_loading_progress_max_percent },
		{ "loading_progress_ui_updates", android_loading_progress_get_flush_count() },
		{ "metadata_progress_callbacks", Level_preview_metadata_progress_callbacks },
		{ "uptime_ms", now - Level_preview_started_at }
	};
	Level_preview_introspection = preview.dump();
	return Level_preview_introspection.c_str();
}

extern "C" const char *android_level_preview_last_error(void)
{
	return Level_preview_error.c_str();
}

extern "C" void android_robot_preview_request_close(void)
{
	Robot_preview_close_requested.store(1, std::memory_order_release);
}

extern "C" void android_robot_preview_rotate(int heading_delta, int pitch_delta)
{
	if (!Robot_preview_active.load(std::memory_order_acquire))
		return;
	Robot_preview_heading.fetch_add(heading_delta, std::memory_order_relaxed);
	Robot_preview_pitch.fetch_add(pitch_delta, std::memory_order_relaxed);
}

extern "C" void android_robot_preview_reset(void)
{
	Robot_preview_heading.store(F0_5 - 1, std::memory_order_release);
	Robot_preview_pitch.store(0, std::memory_order_release);
}
