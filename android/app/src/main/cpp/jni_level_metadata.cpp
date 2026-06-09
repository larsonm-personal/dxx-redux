#include <jni.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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
#include "object.h"
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

using json = nlohmann::ordered_json;

static unsigned char *levelmeta_screen_pixels = NULL;
static int levelmeta_runtime_ready = 0;
static char levelmeta_alloc_file[] = __FILE__;
static char levelmeta_screen_name[] = "levelmeta_screen";
static char levelmeta_pixels_name[] = "levelmeta_screen_pixels";

static json failed_result(const json &request, const char *problem);

static const char *physfs_last_error(void)
{
	const char *error = PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());

	return error ? error : "unknown error";
}

static void write_checkpoint(const json &request, const char *stage, const char *detail)
{
	const std::string path = request.value("checkpoint_path", "");
	if (path.empty())
		return;
	json out;
	out["schema"] = "dxx-level-metadata-checkpoint-v1";
	out["request_id"] = request.value("request_id", "");
	out["stage"] = stage ? stage : "";
	out["detail"] = detail ? detail : "";
	out["source"] = request.value("source_name", "");
	out["game"] = request.value("game", "");
	std::ofstream stream(path, std::ios::trunc);
	if (stream)
		stream << out.dump(2) << "\n";
}

static int init_levelmeta_audio(void)
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

static int init_levelmeta_screen(char *error, size_t error_size)
{
	const int screen_w = (int) SM_W(Game_screen_mode);
	const int screen_h = (int) SM_H(Game_screen_mode);

	if (grd_curscreen)
		return 1;
#ifdef DXX_BUILD_DESCENT_II
	grd_curscreen = (grs_screen *) mem_calloc(1, sizeof(grs_screen), levelmeta_screen_name,
	                                          levelmeta_alloc_file, __LINE__);
	levelmeta_screen_pixels = (unsigned char *) mem_malloc((size_t) screen_w * (size_t) screen_h,
	                                                       levelmeta_pixels_name, levelmeta_alloc_file,
	                                                       __LINE__);
#else
	grd_curscreen = (grs_screen *) mem_malloc(sizeof(grs_screen), levelmeta_screen_name,
	                                          levelmeta_alloc_file, __LINE__, 1);
	levelmeta_screen_pixels = (unsigned char *) mem_malloc((size_t) screen_w * (size_t) screen_h,
	                                                       levelmeta_pixels_name, levelmeta_alloc_file,
	                                                       __LINE__, 0);
#endif
	if (!grd_curscreen || !levelmeta_screen_pixels) {
		snprintf(error, error_size, "%s", "screen allocation failed");
		return 0;
	}
	memset(grd_curscreen, 0, sizeof(grs_screen));
	memset(levelmeta_screen_pixels, 0, (size_t) (screen_w * screen_h));
	grd_curscreen->sc_mode = Game_screen_mode;
	grd_curscreen->sc_w = (short) screen_w;
	grd_curscreen->sc_h = (short) screen_h;
	grd_curscreen->sc_aspect =
	    fixdiv(grd_curscreen->sc_w * GameCfg.AspectX, grd_curscreen->sc_h * GameCfg.AspectY);
	gr_init_canvas(&grd_curscreen->sc_canvas, levelmeta_screen_pixels, BM_LINEAR, screen_w, screen_h);
	gr_set_current_canvas(NULL);
	return 1;
}

static std::vector<std::string> build_runtime_args(const std::string &data_dir)
{
	std::vector<std::string> args;
	args.emplace_back("dxx-levelmeta");
	if (!data_dir.empty()) {
		args.emplace_back("-hogdir");
		args.emplace_back(data_dir);
	}
	return args;
}

static int init_levelmeta_runtime(const json &request, char *error, size_t error_size)
{
	std::vector<std::string> arg_storage = build_runtime_args(request.value("data_dir", ""));
	std::vector<char *> argv;

	if (levelmeta_runtime_ready)
		return 1;
	for (std::string &arg : arg_storage)
		argv.push_back(arg.data());

	write_checkpoint(request, "init", "memory");
	mem_init();
	error_init(msgbox_error);
	set_warn_func(msgbox_warning);
	PHYSFSX_init((int) argv.size(), argv.data());
	if (GameArg.SysShowCmdHelp) {
		snprintf(error, error_size, "%s", "help requested");
		return 0;
	}
	if (!PHYSFSX_checkSupportedArchiveTypes()) {
		snprintf(error, error_size, "%s", "archive type check failed");
		return 0;
	}
#ifdef DXX_BUILD_DESCENT_II
	write_checkpoint(request, "mount", "descent2.hog");
	if (!PHYSFSX_contfile_init("descent2.hog", 1) && !PHYSFSX_contfile_init("d2demo.hog", 1)) {
		snprintf(error, error_size, "%s", "could not find descent2.hog or d2demo.hog");
		return 0;
	}
#else
	write_checkpoint(request, "mount", "descent.hog");
	if (!PHYSFSX_contfile_init("descent.hog", 1)) {
		snprintf(error, error_size, "%s", "could not find descent.hog");
		return 0;
	}
#endif
	{
		const std::string extra_data_dir = request.value("extra_data_dir", "");
		if (!extra_data_dir.empty()) {
			write_checkpoint(request, "mount", "staged mission files");
			PHYSFS_addToSearchPath(extra_data_dir.c_str(), 0);
		}
	}
	write_checkpoint(request, "init", "game data");
	load_text();
	ReadConfigFile();
	if (!init_levelmeta_audio()) {
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
	if (!init_levelmeta_screen(error, error_size))
		return 0;
	Screen_mode = SCREEN_GAME;
	init_game();
	Players[Player_num].callsign[0] = '\0';
	GameArg.SysUseNiceFPS = 0;
#ifdef DXX_BUILD_DESCENT_II
	GameArg.SysInputDemoNoRender = 1;
#endif
	levelmeta_runtime_ready = 1;
	return 1;
}

static int load_requested_mission(const json &request, char *error, size_t error_size)
{
	std::string mission = request.value("mission_name", "");
#ifdef DXX_BUILD_DESCENT_II
	if (mission.empty())
		mission = "d2";
#endif
	write_checkpoint(request, "mission", mission.c_str());
	std::vector<char> mission_name(mission.begin(), mission.end());
	mission_name.push_back('\0');
	if (load_mission_by_name(mission_name.data()))
		return 1;
	snprintf(error, error_size, "could not load mission %s", mission.empty() ? "<built-in>" : mission.c_str());
	return 0;
}

static std::vector<std::string> json_string_array(const json &request, const char *name)
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

static int mount_requested_hogs(const json &request, int require_hog, char *error, size_t error_size)
{
	std::vector<std::string> hog_paths = json_string_array(request, "hog_paths");

	if (hog_paths.empty()) {
		const std::string hog_path = request.value("hog_path", "");
		if (!hog_path.empty())
			hog_paths.push_back(hog_path);
	}
	if (hog_paths.empty() && require_hog) {
		snprintf(error, error_size, "%s", "missing HOG path");
		return 0;
	}
	for (const std::string &hog_path : hog_paths) {
		write_checkpoint(request, "mount", hog_path.c_str());
		if (!PHYSFS_mount(hog_path.c_str(), NULL, 0)) {
			snprintf(error, error_size, "could not mount HOG: %s", physfs_last_error());
			return 0;
		}
	}
	return 1;
}

static void count_level_objects(int *robots, int *hostages)
{
	*robots = 0;
	*hostages = 0;
	for (int objnum = 0; objnum <= Highest_object_index; ++objnum) {
		const object *obj = &Objects[objnum];
		if (obj->flags & OF_SHOULD_BE_DEAD)
			continue;
		if (obj->type == OBJ_ROBOT)
			++*robots;
		else if (obj->type == OBJ_HOSTAGE)
			++*hostages;
	}
}

static json serialize_current_level_row(int level_num, const char *level_file)
{
	const secret_area_state *secret_state = secret_area_get_state();
	const level_metadata_state *metadata = level_metadata_get_state();
	int robots = 0;
	int hostages = 0;
	json row;

	count_level_objects(&robots, &hostages);
	row["level_num"] = level_num;
	row["secret"] = level_num < 0;
	row["level_name"] = Current_level_name;
	row["level_file"] = level_file ? level_file : "";
	row["robots"] = robots;
	row["hostages"] = hostages;
	row["secrets"] = secret_area_total(secret_state);
	row["matcens"] = metadata ? metadata->matcen_count : 0;
	row["energy_centers"] = metadata ? metadata->energy_center_count : 0;
	row["status"] = "ok";
	row["problems"] = json::array();
	return row;
}

static int scan_level(const json &request, json &levels, int level_num, const char *level_file)
{
	write_checkpoint(request, "level", level_file ? level_file : "");
	if (load_level(level_file)) {
		json row;
		row["level_num"] = level_num;
		row["secret"] = level_num < 0;
		row["level_name"] = "";
		row["level_file"] = level_file ? level_file : "";
		row["robots"] = 0;
		row["hostages"] = 0;
		row["secrets"] = 0;
		row["matcens"] = 0;
		row["energy_centers"] = 0;
		row["status"] = "failed";
		row["problems"] = json::array({ "could not load level" });
		levels.push_back(row);
		return 0;
	}
	Current_level_num = level_num;
	secret_area_rescan_current_level();
	levels.push_back(serialize_current_level_row(level_num, level_file));
	return 1;
}

static json analyze_hog_entries(const json &request)
{
	const std::vector<std::string> normal_levels = json_string_array(request, "normal_level_files");
	const std::vector<std::string> secret_levels = json_string_array(request, "secret_level_files");
	json levels = json::array();
	json root;
	int successful = 0;
	int failed = 0;
	int level_num = 1;

	if (normal_levels.empty() && secret_levels.empty())
		return failed_result(request, "HOG contains no level entries");
	for (const std::string &level_file : normal_levels) {
		if (scan_level(request, levels, level_num, level_file.c_str()))
			++successful;
		else
			++failed;
		++level_num;
	}
	level_num = -1;
	for (const std::string &level_file : secret_levels) {
		if (scan_level(request, levels, level_num, level_file.c_str()))
			++successful;
		else
			++failed;
		--level_num;
	}

	root["schema"] = "dxx-level-metadata-v1";
	root["status"] = failed == 0 ? "ok" : successful == 0 ? "failed"
	                                                      : "partial";
	root["request_id"] = request.value("request_id", "");
	root["game"] = request.value("game", "");
	root["source"] = request.value("source_name", "");
	root["mission_name"] = request.value("mission_name", "");
	root["mission_filename"] = request.value("hog_path", "");
	root["levels"] = levels;
	root["problems"] = failed == 0 ? json::array() : json::array({ "one or more levels could not be loaded" });
	return root;
}

static json analyze_loaded_mission(const json &request)
{
	json levels = json::array();
	json root;

	for (int level = 1; level <= Last_level; ++level)
		scan_level(request, levels, level, Level_names[level - 1]);
	for (int level = -1; level >= Last_secret_level; --level)
		scan_level(request, levels, level, Secret_level_names[-level - 1]);

	root["schema"] = "dxx-level-metadata-v1";
	root["status"] = "ok";
	root["request_id"] = request.value("request_id", "");
#ifdef DXX_BUILD_DESCENT_II
	root["game"] = "d2";
#else
	root["game"] = "d1";
#endif
	root["source"] = request.value("source_name", "");
	root["mission_name"] = Current_mission ? Current_mission_longname : "";
	root["mission_filename"] = Current_mission ? Current_mission_filename : "";
	root["levels"] = levels;
	root["problems"] = json::array();
	return root;
}

static json failed_result(const json &request, const char *problem)
{
	json root;
	root["schema"] = "dxx-level-metadata-v1";
	root["status"] = "failed";
	root["request_id"] = request.value("request_id", "");
	root["game"] = request.value("game", "");
	root["source"] = request.value("source_name", "");
	root["levels"] = json::array();
	root["problems"] = json::array({ problem ? problem : "analysis failed" });
	return root;
}

static json analyze_request(const json &request)
{
	char error[256] = "";
	const std::string source_type = request.value("source_type", "");

	if (source_type == "debug_crash") {
		volatile int *ptr = NULL;
		*ptr = 1;
	}
	if (source_type == "debug_hang") {
		for (;;)
			SDL_Delay(1000);
	}
	if (!init_levelmeta_runtime(request, error, sizeof(error)))
		return failed_result(request, error);
	if (source_type == "hog") {
		if (!mount_requested_hogs(request, 1, error, sizeof(error)))
			return failed_result(request, error);
		return analyze_hog_entries(request);
	}
	if (source_type == "mission_files") {
		if (!mount_requested_hogs(request, 0, error, sizeof(error)))
			return failed_result(request, error);
		return analyze_hog_entries(request);
	}
	if (source_type == "level") {
		json root;
		json levels = json::array();
		std::string level_file = request.value("level_file", "");
		int level_num = request.value("level_num", 1);
		if (level_file.empty())
			return failed_result(request, "missing level file");
		scan_level(request, levels, level_num, level_file.c_str());
		root["schema"] = "dxx-level-metadata-v1";
		root["status"] = levels.empty() || levels[0].value("status", "") != "ok" ? "failed" : "ok";
		root["request_id"] = request.value("request_id", "");
		root["game"] = request.value("game", "");
		root["source"] = request.value("source_name", "");
		root["mission_name"] = "";
		root["mission_filename"] = "";
		root["levels"] = levels;
		root["problems"] = json::array();
		return root;
	}
	if (!load_requested_mission(request, error, sizeof(error)))
		return failed_result(request, error);
	return analyze_loaded_mission(request);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_LevelMetadataNativeBridge_nativeAnalyzeLevelMetadata(JNIEnv *env,
                                                                           jobject,
                                                                           jstring jrequest)
{
	const char *request_chars;
	json request;
	json result;
	std::string dumped;

	if (!jrequest)
		return NULL;
	request_chars = env->GetStringUTFChars(jrequest, NULL);
	if (!request_chars)
		return NULL;
	try {
		request = json::parse(request_chars);
	} catch (const std::exception &e) {
		request = json::object();
		result = failed_result(request, e.what());
		env->ReleaseStringUTFChars(jrequest, request_chars);
		dumped = result.dump();
		return env->NewStringUTF(dumped.c_str());
	}
	env->ReleaseStringUTFChars(jrequest, request_chars);
	try {
		write_checkpoint(request, "begin", request.value("source_name", "").c_str());
		result = analyze_request(request);
		write_checkpoint(request, "done", result.value("status", "").c_str());
	} catch (const std::exception &e) {
		result = failed_result(request, e.what());
	}
	dumped = result.dump();
	return env->NewStringUTF(dumped.c_str());
}
