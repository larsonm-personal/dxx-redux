#include <jni.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <time.h>
#include <vector>

#include <nlohmann/json.hpp>

#include <physfs.h>
#include <SDL.h>

extern "C" {
#include "android_crash_handler.h"
#include "jni_string.h"
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
#include "makesig.h"
#include "messagebox.h"
#include "mission.h"
#include "object.h"
#include "physfsx.h"
#include "player.h"
#include "screens.h"
#include "secret_area_scan.h"
#include "secretarea.h"
#include "songs.h"
#include "strutil.h"
#include "texmerge.h"
#include "text.h"
#include "u_mem.h"
}

#include "midi_metadata_json.hpp"

#ifdef DXX_BUILD_DESCENT_II
extern "C" void piggy_init_pigfile(char *filename);
#include "level_metadata_replacements.hpp"
static level_metadata_base_definitions Level_metadata_base_definitions = {};
#endif

using json = nlohmann::ordered_json;

static unsigned char *levelmeta_screen_pixels = NULL;
static int levelmeta_runtime_ready = 0;
static int levelmeta_runtime_poisoned = 0;
static std::string levelmeta_data_dir;
static char levelmeta_alloc_file[] = __FILE__;
static char levelmeta_screen_name[] = "levelmeta_screen";
static char levelmeta_pixels_name[] = "levelmeta_screen_pixels";

static json failed_result(const json &request, const char *problem);

static std::string dump_metadata_json(const json &value)
{
	return value.dump(2, ' ', false, json::error_handler_t::replace);
}

static const char *physfs_last_error(void)
{
	const char *error = PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());

	return error ? error : "unknown error";
}

static std::string request_mission_display_name(const json &request)
{
	const std::string display_name = request.value("mission_display_name", "");

	return display_name.empty() ? request.value("mission_name", "") : display_name;
}

static std::string leaf_name(const std::string &path)
{
	const size_t slash = path.find_last_of("/\\");

	return slash == std::string::npos ? path : path.substr(slash + 1);
}

static std::string request_metadata_source_file(const json &request)
{
	static const char *fields[] = {
		"source_path",
		"hog_path",
		"archive_path",
		"mission_filename",
		"level_file",
		"source_name",
	};

	for (const char *field : fields) {
		const std::string value = request.value(field, "");
		if (!value.empty())
			return leaf_name(value);
	}
	const json::const_iterator hog_paths = request.find("hog_paths");
	if (hog_paths != request.end() && hog_paths->is_array() && !hog_paths->empty() && (*hog_paths)[0].is_string())
		return leaf_name((*hog_paths)[0].get<std::string>());
	return "";
}

static void breadcrumb_metadata_request(const json &request, const char *stage)
{
	const std::string game = request.value("game", "");
	const std::string source_type = request.value("source_type", "");
	const std::string file = request_metadata_source_file(request);
	const std::string mission = request_mission_display_name(request);
	const std::string level_file = leaf_name(request.value("level_file", ""));

	crash_breadcrumb_v("levelmeta %s game=%s type=%s file=%s",
	                   stage ? stage : "", game.c_str(), source_type.c_str(), file.c_str());
	if (!mission.empty() || !level_file.empty())
		crash_breadcrumb_v("levelmeta detail mission=%s level=%s", mission.c_str(), level_file.c_str());
}

static void breadcrumb_metadata_level(const json &request, int level_num, const char *level_file)
{
	const std::string source = request_metadata_source_file(request);
	const std::string level = leaf_name(level_file ? level_file : "");

	crash_breadcrumb_v("levelmeta level n=%d file=%s source=%s", level_num, level.c_str(), source.c_str());
}

static void write_checkpoint_progress(const json &request, const char *stage,
                                      const char *detail, int completed,
                                      int total)
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
	if (total > 0) {
		out["completed"] = completed;
		out["total"] = total;
	}
	std::ofstream stream(path, std::ios::trunc);
	if (stream)
		stream << out.dump(2) << "\n";
}

static void write_checkpoint(const json &request, const char *stage,
                             const char *detail)
{
	write_checkpoint_progress(request, stage, detail, -1, -1);
}

struct levelmeta_progress_context {
	const json *request;
	const char *level_file;
	int level_completed;
	int level_total;
	int task_id;
	int last_completed;
	int last_total;
	int last_percent;
	Uint32 last_write_ticks;
	std::string last_stage;
};

struct levelmeta_budget_context {
	std::string cancellation_path;
	unsigned int checks = 0;
	bool background = false;
	unsigned int cpu_duty_percent = 100;
	unsigned long long wall_start_us = 0;
	unsigned long long cpu_start_us = 0;
};

static unsigned long long levelmeta_clock_us(clockid_t clock_id)
{
	struct timespec value;

	if (clock_gettime(clock_id, &value))
		return 0;
	return (unsigned long long) value.tv_sec * 1000000ULL +
	       (unsigned long long) value.tv_nsec / 1000ULL;
}

static int levelmeta_budget_cancelled(void *user)
{
	levelmeta_budget_context *context =
	    static_cast<levelmeta_budget_context *>(user);
	if (!context)
		return 0;
	if (!context->cancellation_path.empty()) {
		std::ifstream cancellation(context->cancellation_path);
		if (cancellation.good())
			return 1;
	}
	if (context->background && (++context->checks & 7u) == 0) {
		const unsigned long long wall_now = levelmeta_clock_us(CLOCK_MONOTONIC);
		const unsigned long long cpu_now =
		    levelmeta_clock_us(CLOCK_THREAD_CPUTIME_ID);
		if (!context->wall_start_us || !context->cpu_start_us) {
			context->wall_start_us = wall_now;
			context->cpu_start_us = cpu_now;
		} else if (context->cpu_duty_percent &&
		           context->cpu_duty_percent < 100) {
			const unsigned long long wall_used =
			    wall_now - context->wall_start_us;
			const unsigned long long cpu_used = cpu_now - context->cpu_start_us;
			const unsigned long long target_wall =
			    cpu_used * 100ULL / context->cpu_duty_percent;
			if (target_wall > wall_used) {
				const unsigned long long delay_us =
				    std::min<unsigned long long>(target_wall - wall_used, 50000ULL);
				SDL_Delay((Uint32) ((delay_us + 999ULL) / 1000ULL));
			}
			if (wall_used >= 1000000ULL) {
				context->wall_start_us = levelmeta_clock_us(CLOCK_MONOTONIC);
				context->cpu_start_us =
				    levelmeta_clock_us(CLOCK_THREAD_CPUTIME_ID);
			}
		}
	}
	return 0;
}

static void write_level_task_progress(
    const levelmeta_progress_context &context, const char *phase,
    int completed, int total)
{
	const std::string path = context.request->value("checkpoint_path", "");
	if (path.empty())
		return;
	json out;
	out["schema"] = "dxx-level-metadata-checkpoint-v1";
	out["request_id"] = context.request->value("request_id", "");
	out["stage"] = "level_progress";
	out["phase"] = phase ? phase : "";
	out["detail"] = context.level_file ? context.level_file : "";
	out["source"] = context.request->value("source_name", "");
	out["game"] = context.request->value("game", "");
	out["task_id"] = context.task_id;
	out["completed"] = completed;
	out["total"] = total;
	out["level_completed"] = context.level_completed;
	out["level_total"] = context.level_total;
	std::ofstream stream(path, std::ios::trunc);
	if (stream)
		stream << out.dump(2) << "\n";
}

static void levelmeta_progress(void *user, const char *stage,
                               int completed, int total)
{
	levelmeta_progress_context *context =
	    static_cast<levelmeta_progress_context *>(user);
	if (!context || !context->request || total <= 0)
		return;
	if (completed < 0)
		completed = 0;
	if (completed > total)
		completed = total;
	const std::string current_stage = stage ? stage : "";
	const int new_task =
	    current_stage != context->last_stage || total != context->last_total ||
	    completed < context->last_completed;
	if (new_task) {
		context->task_id++;
		context->last_stage = current_stage;
		context->last_total = total;
		context->last_completed = -1;
		context->last_percent = -1;
	}
	const int percent = completed * 100 / total;
	const Uint32 now = SDL_GetTicks();
	if (!new_task && percent <= context->last_percent)
		return;
	if (!new_task && percent < 100 && now - context->last_write_ticks < 100)
		return;
	write_level_task_progress(*context, stage, completed, total);
	context->last_completed = completed;
	context->last_percent = percent;
	context->last_write_ticks = now;
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

static int init_levelmeta_runtime(JNIEnv *env, jobject context, const json &request, char *error, size_t error_size)
{
	const std::string requested_data_dir = request.value("data_dir", "");
	std::vector<std::string> arg_storage = build_runtime_args(requested_data_dir);
	std::vector<char *> argv;
#ifdef __ANDROID__
	PHYSFS_AndroidInit android_init;
#endif

	if (levelmeta_runtime_ready) {
		if (levelmeta_runtime_poisoned) {
			snprintf(error, error_size, "%s", "metadata worker requires restart");
			return 0;
		}
		if (requested_data_dir != levelmeta_data_dir) {
			snprintf(error, error_size, "%s", "metadata worker data directory changed");
			return 0;
		}
		return 1;
	}
#ifdef __ANDROID__
	android_init.jnienv = (void *) env;
	android_init.context = (void *) context;
	argv.push_back((char *) &android_init);
	for (size_t i = 1; i < arg_storage.size(); i++)
		argv.push_back(arg_storage[i].data());
#else
	for (std::string &arg : arg_storage)
		argv.push_back(arg.data());
#endif

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
	write_checkpoint(request, "init", "game data");
	load_text();
	ReadConfigFile();
	if (!init_levelmeta_audio()) {
		snprintf(error, error_size, "%s", "audio init failed");
		return 0;
	}
	PHYSFSX_addArchiveContent();
	gamedata_init();
#ifdef DXX_BUILD_DESCENT_II
	level_metadata_capture_base_definitions(Level_metadata_base_definitions);
#endif
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
	GameArg.SysInputDemoNoRender = 1;
	levelmeta_data_dir = requested_data_dir;
	levelmeta_runtime_ready = 1;
	return 1;
}

class LevelMetadataRequestMounts
{
	std::vector<std::string> mounted_paths;
	int mission_loaded = 0;
	int finished = 0;

	static int search_path_contains(const std::string &wanted)
	{
		char **paths = PHYSFS_getSearchPath();
		int found = 0;

		if (!paths)
			return -1;
		for (char **path = paths; *path; ++path)
			if (wanted == *path) {
				found = 1;
				break;
			}
		PHYSFS_freeList(paths);
		return found;
	}

  public:
	~LevelMetadataRequestMounts()
	{
		finish(NULL, 0);
	}

	int finish(char *error, size_t error_size)
	{
		int cleanup_ok = 1;

		if (finished)
			return 1;
#ifdef DXX_BUILD_DESCENT_II
		reset_level_robots_file();
#endif
		if (mission_loaded && Current_mission)
			free_mission();
		Current_mission = NULL;
		for (std::vector<std::string>::reverse_iterator path = mounted_paths.rbegin();
		     path != mounted_paths.rend(); ++path)
			if (!PHYSFS_unmount(path->c_str()) && search_path_contains(*path) != 0)
				cleanup_ok = 0;
		mounted_paths.clear();
		finished = 1;
		if (!cleanup_ok && error && error_size)
			snprintf(error, error_size, "%s", "could not release metadata request mounts");
		return cleanup_ok;
	}

	int mount(const std::string &path)
	{
		if (search_path_contains(path) == 1)
			return 1;
		if (!PHYSFS_mount(path.c_str(), NULL, 0))
			return 0;
		mounted_paths.push_back(path);
		return 1;
	}

	void set_mission_loaded(int loaded)
	{
		mission_loaded = loaded;
		if (!loaded)
			Current_mission = NULL;
	}
};

static json finish_levelmeta_request(LevelMetadataRequestMounts &mounts, const json &request,
                                     json result, char *error, size_t error_size)
{
	if (mounts.finish(error, error_size))
		return result;
	levelmeta_runtime_poisoned = 1;
	return failed_result(request, error);
}

static int mount_request_extra_dir(const json &request, LevelMetadataRequestMounts &mounts,
                                   char *error, size_t error_size)
{
	const std::string extra_data_dir = request.value("extra_data_dir", "");

	if (extra_data_dir.empty())
		return 1;
	write_checkpoint(request, "mount", "staged mission files");
	if (mounts.mount(extra_data_dir))
		return 1;
	snprintf(error, error_size, "could not mount staged files: %s", physfs_last_error());
	return 0;
}

static int load_requested_mission(const json &request, LevelMetadataRequestMounts &mounts,
                                  char *error, size_t error_size)
{
	std::string mission = request.value("mission_name", "");
#ifdef DXX_BUILD_DESCENT_II
	if (mission.empty())
		mission = "d2";
#endif
	write_checkpoint(request, "mission", mission.c_str());
	std::vector<char> mission_name(mission.begin(), mission.end());
	mission_name.push_back('\0');
	if (load_mission_by_name(mission_name.data()) ||
	    load_mission_by_name_from_current_dir(mission_name.data())) {
		mounts.set_mission_loaded(1);
		return 1;
	}
	mounts.set_mission_loaded(0);
	snprintf(error, error_size, "could not load mission %s", mission.empty() ? "<built-in>" : mission.c_str());
	return 0;
}

static int mission_descriptor_available(const json &request)
{
	const std::string mission = request.value("mission_name", "");
	std::string filename;

	if (mission.empty())
		return 0;
#ifdef DXX_BUILD_DESCENT_II
	filename = mission + ".mn2";
#else
	filename = mission + ".msn";
#endif
	return PHYSFSX_exists((MISSION_DIR + filename).c_str(), 1) ||
	       PHYSFSX_exists(filename.c_str(), 1);
}

static int load_mission_if_descriptor_available(const json &request,
                                                LevelMetadataRequestMounts &mounts,
                                                char *error, size_t error_size)
{
	if (!mission_descriptor_available(request))
		return 1;
	return load_requested_mission(request, mounts, error, error_size);
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

static int mount_requested_hogs(const json &request, LevelMetadataRequestMounts &mounts,
                                int require_hog, char *error, size_t error_size)
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
		if (!mounts.mount(hog_path)) {
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

static std::string format_levelmeta_multiplier(double value)
{
	char buffer[32];
	double display_value;
	int decimals;

	if (value <= 0.0)
		return "";
	if (value >= 10.0) {
		double scale = pow(10.0, floor(log10(value)) - 1.0);
		display_value = floor(value / scale + 0.5) * scale;
		decimals = 0;
	} else {
		display_value = value;
		decimals = value >= 1.0 ? 1 : 2;
	}
	snprintf(buffer, sizeof(buffer), "%.*fx", decimals, display_value);
	return buffer;
}

static std::string format_levelmeta_time(int seconds)
{
	char buffer[32];
	int minutes;

	if (seconds < 0)
		seconds = 0;
	minutes = seconds / 60;
	seconds %= 60;
	snprintf(buffer, sizeof(buffer), "%dM:%02dS", minutes, seconds);
	return buffer;
}

static const char *levelmeta_key_name(int key_index)
{
	switch (key_index) {
		case 0:
			return "blue";
		case 1:
			return "red";
		case 2:
			return "gold";
		default:
			return "";
	}
}

static json serialize_route_position(const int pos[3])
{
	return {
		{ "x", pos[0] / LEVEL_METADATA_FIX_SCALE },
		{ "y", pos[1] / LEVEL_METADATA_FIX_SCALE },
		{ "z", pos[2] / LEVEL_METADATA_FIX_SCALE }
	};
}

static json serialize_route_steps(const level_metadata_state *metadata)
{
	json steps = json::array();
	int count;

	if (!metadata)
		return steps;
	count = metadata->route_step_count;
	if (count < 0)
		count = 0;
	if (count > LEVEL_METADATA_MAX_ROUTE_STEPS)
		count = LEVEL_METADATA_MAX_ROUTE_STEPS;
	for (int index = 0; index < count; ++index) {
		const level_metadata_route_step &step = metadata->route_steps[index];
		json item;
		item["index"] = index;
		item["kind"] = level_metadata_route_step_kind_name(step.kind);
		item["activation_kind"] = level_metadata_route_activation_kind_name(step.activation_kind);
		if (step.activation_kind ==
		    LEVEL_METADATA_ROUTE_ACTIVATION_UNRESOLVED_TRIGGER)
			item["calculated"] = false;
		if (step.label[0])
			item["label"] = step.label;
		if (step.seg >= 0)
			item["seg"] = step.seg;
		if (step.side >= 0)
			item["side"] = step.side;
		if (step.wall_num >= 0)
			item["wall"] = step.wall_num;
		if (step.label_pos_valid)
			item["label_pos"] = serialize_route_position(step.label_pos);
		if (step.distance_from_previous > 0.0)
			item["distance"] = step.distance_from_previous;
		if (step.kind == LEVEL_METADATA_ROUTE_KEY && step.key_index >= 0)
			item["key"] = levelmeta_key_name(step.key_index);
		if (step.can_be_bypassed)
			item["can_be_bypassed"] = true;
		if (step.key_carrier_objnum >= 0)
			item["key_carrier_objnum"] = step.key_carrier_objnum;
		if (step.trigger_num >= 0)
			item["trigger"] = step.trigger_num;
		if (step.trigger_type >= 0)
			item["trigger_type_id"] = step.trigger_type;
		if (step.trigger_type_name[0])
			item["trigger_type"] = step.trigger_type_name;
		if (step.opened_link_count > 0) {
			json opened = json::array();
			for (int link = 0; link < step.opened_link_count; ++link) {
				json open;
				if (step.opened_link_seg[link] >= 0)
					open["seg"] = step.opened_link_seg[link];
				if (step.opened_link_side[link] >= 0)
					open["side"] = step.opened_link_side[link];
				if (step.opened_link_wall[link] >= 0)
					open["wall"] = step.opened_link_wall[link];
				opened.push_back(open);
			}
			item["opens"] = opened;
		}
		steps.push_back(item);
	}
	return steps;
}

static json serialize_metadata_notes(const level_metadata_state *metadata)
{
	json notes = json::array();

	if (!metadata)
		return notes;
	if (metadata->route_note[0])
		notes.push_back(metadata->route_note);
	static const char *const key_names[] = { "blue", "red", "gold" };
	for (int key = 0; key < 3; ++key)
		if (metadata->unnecessary_key_mask & (1 << key))
			notes.push_back(std::string(key_names[key]) + " key not necessary");
	if (metadata->guidebot_placement_note[0] &&
	    strcmp(metadata->guidebot_placement_note, metadata->route_note))
		notes.push_back(metadata->guidebot_placement_note);
	if (metadata->guidebot_note[0] && strcmp(metadata->guidebot_note, metadata->route_note))
		notes.push_back(metadata->guidebot_note);
	return notes;
}

static json serialize_current_level_row(int level_num, const char *level_file,
                                        int base_player_ship_radius)
{
	const secret_area_state *secret_state = secret_area_get_state();
	const level_metadata_state *metadata = level_metadata_get_canonical_state();
	char display_level_name[256];
	int robots = 0;
	int hostages = 0;
	json row;
	route_analysis_cache_summary cache_summary;
	level_metadata_visibility_cache_summary visibility_summary;

	count_level_objects(&robots, &hostages);
	row["level_num"] = level_num;
	row["secret"] = level_num < 0;
	level_metadata_choose_level_display_name(
	    level_file, Current_level_name, display_level_name,
	    sizeof(display_level_name));
	row["level_name"] = display_level_name;
	row["level_file"] = level_file ? level_file : "";
	row["robots"] = robots;
	row["hostages"] = hostages;
	row["secrets"] = secret_area_total(secret_state);
	row["matcens"] = metadata ? metadata->matcen_count : 0;
	row["energy_centers"] = metadata ? metadata->energy_center_count : 0;
	row["mine_volume"] = metadata ? metadata->mine_volume : 0.0;
	row["mine_volume_normalized"] = metadata ? metadata->mine_volume_normalized : 0.0;
	row["mine_volume_text"] = metadata ? format_levelmeta_multiplier(metadata->mine_volume_normalized) : "";
	row["travel_distance"] = metadata ? metadata->travel_distance : 0.0;
	row["travel_time_seconds"] = metadata ? metadata->travel_time_seconds : 0;
	row["travel_time_text"] = metadata ? format_levelmeta_time(metadata->travel_time_seconds) : "";
	row["guidebot_count"] = metadata ? metadata->guidebot_count : 0;
	row["guidebot_placed"] = metadata && metadata->guidebot_placed != 0;
	row["guidebot_accessible"] = metadata && metadata->guidebot_accessible != 0;
	row["guidebot_placement_note"] = metadata && metadata->guidebot_placement_note[0] ? metadata->guidebot_placement_note : "";
	row["guidebot_note"] = metadata && metadata->guidebot_note[0] ? metadata->guidebot_note : "";
	row["route_status"] = metadata ? level_metadata_route_status_name(metadata->route_status) : "failed";
	row["route_readiness"] =
	    level_metadata_route_readiness_name(level_metadata_get_route_readiness());
	row["failure_kind"] = "";
	if (metadata && metadata->route_status == LEVEL_METADATA_ROUTE_FAILED) {
		if (strstr(metadata->route_problem, "cancelled"))
			row["failure_kind"] = "preempted";
		else if (strstr(metadata->route_problem, "collision-work budget"))
			row["failure_kind"] = "budget_exhausted";
		else
			row["failure_kind"] = "unroutable";
	}
	row["route_problem"] = metadata && metadata->route_problem[0] ? metadata->route_problem : "";
	row["route_note"] = metadata && metadata->route_note[0] ? metadata->route_note : "";
	row["route_steps"] = serialize_route_steps(metadata);
#ifdef DXX_BUILD_DESCENT_II
	row["replacements"] = json::array();
	if (base_player_ship_radius > 0 && Player_ship &&
	    Player_ship->model_num >= 0 && Player_ship->model_num < N_polygon_models &&
	    Polygon_models[Player_ship->model_num].rad != base_player_ship_radius) {
		row["replacements"].push_back({
		    { "kind", "player_ship_size" },
		    { "label", "Player ship size" },
		    { "base_game", base_player_ship_radius },
		    { "mod", Polygon_models[Player_ship->model_num].rad },
		});
	}
	row["replacement_groups"] =
	    level_metadata_serialize_replacement_groups<json>(Level_metadata_base_definitions, level_file);
#endif
	row["route_cache_file"] =
	    level_metadata_get_route_analysis_cache_summary(&cache_summary) ? cache_summary.filename : "";
	if (level_metadata_get_visibility_cache_summary(&visibility_summary))
		row["visibility_entries"] = visibility_summary.entries;
	row["visibility_checkpoint_sequence"] =
	    level_metadata_get_visibility_checkpoint_sequence();
	row["status"] = "ok";
	row["problems"] = json::array();
	row["notes"] = serialize_metadata_notes(metadata);
	return row;
}

static int count_current_level_coop_starts(void)
{
	int count = 0;
	int playerlike_seen = 0;

	for (int objnum = 0; objnum <= Highest_object_index; ++objnum) {
		const object *obj = &Objects[objnum];
		if (obj->flags & OF_SHOULD_BE_DEAD)
			continue;
		if (obj->type != OBJ_PLAYER && obj->type != OBJ_GHOST && obj->type != OBJ_COOP)
			continue;
		if (obj->type == OBJ_COOP || playerlike_seen == 0)
			++count;
		++playerlike_seen;
	}
	return count;
}

struct CoopStartRange {
	int min = 0;
	int max = 0;

	void add(int value)
	{
		if (value <= 0)
			return;
		if (!min || value < min)
			min = value;
		if (value > max)
			max = value;
	}

	std::string text() const
	{
		char buffer[32];

		if (!min)
			return "";
		if (min == max) {
			snprintf(buffer, sizeof(buffer), "%d", min);
		} else {
			snprintf(buffer, sizeof(buffer), "%d-%d", min, max);
		}
		return buffer;
	}
};

static int request_wants_coop_start_header(const json &request)
{
	const std::string mission_type = request.value("mission_type", "");

	return mission_type.empty() || mission_type == "normal" || mission_type == "Normal" || mission_type == "NORMAL";
}

static void set_coop_start_header(json &root, const json &request, const CoopStartRange &range)
{
	if (request_wants_coop_start_header(request))
		root["coop_starts"] = range.text();
}

enum LevelScanStatus {
	LEVEL_SCAN_FAILED = 0,
	LEVEL_SCAN_OK = 1,
	LEVEL_SCAN_MISSING = 2,
};

static json failed_level_row(int level_num, const char *level_file,
                             const char *problem, const char *failure_kind)
{
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
	row["mine_volume"] = 0.0;
	row["mine_volume_normalized"] = 0.0;
	row["mine_volume_text"] = "";
	row["travel_distance"] = 0.0;
	row["travel_time_seconds"] = 0;
	row["travel_time_text"] = format_levelmeta_time(0);
	row["guidebot_count"] = 0;
	row["guidebot_placed"] = false;
	row["guidebot_accessible"] = false;
	row["guidebot_placement_note"] = "";
	row["guidebot_note"] = "";
	row["route_status"] = "failed";
	row["route_readiness"] = "failed";
	row["failure_kind"] = failure_kind ? failure_kind : "analysis_failed";
	row["route_problem"] = problem;
	row["route_note"] = "";
	row["route_steps"] = json::array();
	row["status"] = "failed";
	row["problems"] = json::array({ problem });
	row["notes"] = json::array();
	return row;
}

static LevelScanStatus scan_level(const json &request, json &levels,
                                  int level_num, const char *level_file,
                                  int completed, int total,
                                  int *coop_starts)
{
	int base_player_ship_radius = 0;

	write_checkpoint_progress(request, "level", level_file ? level_file : "",
	                          completed, total);
	breadcrumb_metadata_level(request, level_num, level_file);
	if (coop_starts)
		*coop_starts = 0;
	if (!level_file || !level_file[0] || !PHYSFSX_exists(level_file, 1)) {
		levels.push_back(failed_level_row(
		    level_num, level_file, "level file is missing", "missing"));
		return LEVEL_SCAN_MISSING;
	}
#ifdef DXX_BUILD_DESCENT_II
	level_metadata_set_switch_projectile_radius_override(0);
	reset_level_robots_file();
#endif
	if (load_level(level_file)) {
		levels.push_back(failed_level_row(
		    level_num, level_file, "could not load level", "invalid_input"));
		return LEVEL_SCAN_FAILED;
	}
	Current_level_num = level_num;
#ifdef DXX_BUILD_DESCENT_II
	base_player_ship_radius = load_level_robots_file(level_file);
#endif
	if (coop_starts)
		*coop_starts = count_current_level_coop_starts();
	levelmeta_progress_context progress_context = {
		&request,
		level_file,
		completed,
		total,
		0,
		-1,
		-1,
		-1,
		0,
		"",
	};
	levelmeta_budget_context budget_context;
	budget_context.cancellation_path = request.value("cancellation_path", "");
	budget_context.background = request.value("background", false);
	budget_context.cpu_duty_percent =
	    request.value("cpu_duty_percent", budget_context.background ? 5u : 100u);
	level_metadata_set_analysis_fvi_limit(
	    request.value("fvi_limit", 1000000u));
	level_metadata_set_defer_guidebot_accessibility(
	    request.value("defer_guidebot_accessibility", false));
	level_metadata_set_progress_callback(levelmeta_progress, &progress_context);
	level_metadata_set_cancel_callback(levelmeta_budget_cancelled, &budget_context);
	secret_area_rescan_current_level();
	level_metadata_set_defer_guidebot_accessibility(0);
	level_metadata_set_analysis_fvi_limit(0);
	level_metadata_set_cancel_callback(NULL, NULL);
	level_metadata_set_progress_callback(NULL, NULL);
	levels.push_back(serialize_current_level_row(
	    level_num, level_file, base_player_ship_radius));
	write_checkpoint_progress(request, "level_done",
	                          level_file ? level_file : "",
	                          completed + 1, total);
	return LEVEL_SCAN_OK;
}

static json analyze_hog_entries(const json &request)
{
	const std::vector<std::string> normal_levels = json_string_array(request, "normal_level_files");
	const std::vector<std::string> secret_levels = json_string_array(request, "secret_level_files");
	json levels = json::array();
	json root;
	int successful = 0;
	int failed = 0;
	int missing_secret = 0;
	int level_num = normal_levels.size() == 1 && secret_levels.empty() ? request.value("level_num", 1) : 1;
	int completed = 0;
	const int total =
	    static_cast<int>(normal_levels.size() + secret_levels.size());
	CoopStartRange coop_start_range;

	if (normal_levels.empty() && secret_levels.empty())
		return failed_result(request, "HOG contains no level entries");
	for (const std::string &level_file : normal_levels) {
		int coop_starts = 0;
		const LevelScanStatus status = scan_level(
		    request, levels, level_num, level_file.c_str(), completed,
		    total, &coop_starts);
		if (status == LEVEL_SCAN_OK) {
			++successful;
			coop_start_range.add(coop_starts);
		} else {
			++failed;
		}
		++completed;
		++level_num;
	}
	level_num = secret_levels.size() == 1 && normal_levels.empty() ? request.value("level_num", -1) : -1;
	for (const std::string &level_file : secret_levels) {
		int coop_starts = 0;
		const LevelScanStatus status = scan_level(
		    request, levels, level_num, level_file.c_str(), completed,
		    total, &coop_starts);
		if (status == LEVEL_SCAN_OK) {
			++successful;
			coop_start_range.add(coop_starts);
		} else if (status == LEVEL_SCAN_MISSING) {
			++missing_secret;
		} else {
			++failed;
		}
		++completed;
		--level_num;
	}

	root["schema"] = "dxx-level-metadata-v1";
	root["status"] = failed == 0 ? "ok" : successful == 0 ? "failed"
	                                                      : "partial";
	root["request_id"] = request.value("request_id", "");
	root["game"] = request.value("game", "");
	root["source"] = request.value("source_name", "");
	root["mission_name"] = request_mission_display_name(request);
	root["mission_filename"] = request.value("mission_filename", request.value("hog_path", ""));
	set_coop_start_header(root, request, coop_start_range);
	root["music_tracks"] = request.value("mission_filename", "").empty()
	                           ? json::array()
	                           : serialize_active_midi_metadata<json>();
	root["levels"] = levels;
	root["problems"] = json::array();
	if (failed)
		root["problems"].push_back("one or more levels could not be loaded");
	if (missing_secret)
		root["problems"].push_back("one or more secret levels are referenced but missing");
	return root;
}

static json analyze_loaded_mission(const json &request)
{
	json levels = json::array();
	json root;
	CoopStartRange coop_start_range;
	int completed = 0;
	const int total = Last_level - Last_secret_level;

	for (int level = 1; level <= Last_level; ++level) {
		int coop_starts = 0;
		if (scan_level(request, levels, level, Level_names[level - 1],
		               completed, total, &coop_starts))
			coop_start_range.add(coop_starts);
		++completed;
	}
	for (int level = -1; level >= Last_secret_level; --level) {
		int coop_starts = 0;
		if (scan_level(request, levels, level,
		               Secret_level_names[-level - 1], completed, total,
		               &coop_starts))
			coop_start_range.add(coop_starts);
		++completed;
	}

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
	set_coop_start_header(root, request, coop_start_range);
	root["music_tracks"] = serialize_active_midi_metadata<json>();
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
	root["mission_name"] = request_mission_display_name(request);
	root["mission_filename"] = request.value("mission_filename", "");
	root["levels"] = json::array();
	root["failure_kind"] = "analysis_failed";
	root["problems"] = json::array({ problem ? problem : "analysis failed" });
	return root;
}

static json analyze_request(JNIEnv *env, jobject context, const json &request)
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
	if (!init_levelmeta_runtime(env, context, request, error, sizeof(error)))
		return failed_result(request, error);
	LevelMetadataRequestMounts mounts;
	if (!mount_request_extra_dir(request, mounts, error, sizeof(error)))
		return finish_levelmeta_request(mounts, request, failed_result(request, error), error, sizeof(error));
	if (source_type == "hog") {
		if (!mount_requested_hogs(request, mounts, 1, error, sizeof(error)))
			return finish_levelmeta_request(mounts, request, failed_result(request, error), error, sizeof(error));
		if (!load_mission_if_descriptor_available(request, mounts, error, sizeof(error)))
			return finish_levelmeta_request(mounts, request, failed_result(request, error), error, sizeof(error));
		return finish_levelmeta_request(mounts, request, analyze_hog_entries(request), error, sizeof(error));
	}
	if (source_type == "mission_files") {
		if (!mount_requested_hogs(request, mounts, 0, error, sizeof(error)))
			return finish_levelmeta_request(mounts, request, failed_result(request, error), error, sizeof(error));
		if (!load_mission_if_descriptor_available(request, mounts, error, sizeof(error)))
			return finish_levelmeta_request(mounts, request, failed_result(request, error), error, sizeof(error));
		return finish_levelmeta_request(mounts, request, analyze_hog_entries(request), error, sizeof(error));
	}
	if (source_type == "level") {
		json root;
		json levels = json::array();
		CoopStartRange coop_start_range;
		std::string level_file = request.value("level_file", "");
		int level_num = request.value("level_num", 1);
		if (level_file.empty())
			return finish_levelmeta_request(
			    mounts, request, failed_result(request, "missing level file"), error, sizeof(error));
		{
			int coop_starts = 0;
			if (scan_level(request, levels, level_num, level_file.c_str(),
			               0, 1, &coop_starts))
				coop_start_range.add(coop_starts);
		}
		root["schema"] = "dxx-level-metadata-v1";
		root["status"] = levels.empty() || levels[0].value("status", "") != "ok" ? "failed" : "ok";
		root["request_id"] = request.value("request_id", "");
		root["game"] = request.value("game", "");
		root["source"] = request.value("source_name", "");
		root["mission_name"] = "";
		root["mission_filename"] = "";
		set_coop_start_header(root, request, coop_start_range);
		root["levels"] = levels;
		root["problems"] = json::array();
		return finish_levelmeta_request(mounts, request, root, error, sizeof(error));
	}
	if (source_type == "active_level") {
		json root;
		json levels = json::array();
		CoopStartRange coop_start_range;
		const std::string level_file = request.value("level_file", "");
		const int level_num = request.value("level_num", 1);
		if (!mount_requested_hogs(request, mounts, 0, error, sizeof(error)))
			return finish_levelmeta_request(
			    mounts, request, failed_result(request, error), error, sizeof(error));
		if (!load_requested_mission(request, mounts, error, sizeof(error)))
			return finish_levelmeta_request(
			    mounts, request, failed_result(request, error), error, sizeof(error));
		if (level_file.empty())
			return finish_levelmeta_request(
			    mounts, request, failed_result(request, "missing active level file"),
			    error, sizeof(error));
		{
			int coop_starts = 0;
			if (scan_level(request, levels, level_num, level_file.c_str(),
			               0, 1, &coop_starts))
				coop_start_range.add(coop_starts);
		}
		root["schema"] = "dxx-level-metadata-v1";
		root["status"] = levels.empty() || levels[0].value("status", "") != "ok" ? "failed" : "ok";
		root["request_id"] = request.value("request_id", "");
		root["game"] = request.value("game", "");
		root["source"] = request.value("source_name", "");
		root["mission_name"] = Current_mission ? Current_mission_longname : "";
		root["mission_filename"] = Current_mission ? Current_mission_filename : "";
		set_coop_start_header(root, request, coop_start_range);
		root["music_tracks"] = serialize_active_midi_metadata<json>();
		root["levels"] = levels;
		root["problems"] = json::array();
		return finish_levelmeta_request(mounts, request, root, error, sizeof(error));
	}
	if (!load_requested_mission(request, mounts, error, sizeof(error)))
		return finish_levelmeta_request(mounts, request, failed_result(request, error), error, sizeof(error));
	return finish_levelmeta_request(mounts, request, analyze_loaded_mission(request), error, sizeof(error));
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_LevelMetadataNativeBridge_nativeAnalyzeLevelMetadata(JNIEnv *env,
                                                                           jobject thiz,
                                                                           jobject jcontext,
                                                                           jstring jrequest)
{
	char *request_chars;
	json request;
	json result;
	std::string dumped;

	if (!jrequest)
		return NULL;
	if (!dxx_jni_string_to_utf8(env, jrequest, &request_chars))
		return NULL;
	try {
		request = json::parse(request_chars);
	} catch (const std::exception &e) {
		request = json::object();
		result = failed_result(request, e.what());
		free(request_chars);
		dumped = dump_metadata_json(result);
		return dxx_jni_string_from_utf8_n(env, dumped.data(), dumped.size());
	}
	free(request_chars);
	try {
		breadcrumb_metadata_request(request, "begin");
		write_checkpoint(request, "begin", request.value("source_name", "").c_str());
		result = analyze_request(env, jcontext ? jcontext : thiz, request);
		write_checkpoint(request, "done", result.value("status", "").c_str());
		breadcrumb_metadata_request(request, result.value("status", "").c_str());
	} catch (const std::exception &e) {
		result = failed_result(request, e.what());
	}
	dumped = dump_metadata_json(result);
	return dxx_jni_string_from_utf8_n(env, dumped.data(), dumped.size());
}
