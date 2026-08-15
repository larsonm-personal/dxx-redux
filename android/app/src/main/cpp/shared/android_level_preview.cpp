#include "android_level_preview.h"

#include <atomic>
#include <algorithm>
#include <cstdint>
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
#include "ai.h"
#include "aistruct.h"
#include "segment.h"
#include "automap.h"
#include "bm.h"
#include "digi.h"
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
#include "laser.h"
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
#include "sounds.h"
#include "startup_resume_shared.h"
#include "texmerge.h"
#include "timer.h"
#include "window.h"
#include "weapon.h"
#include "vclip.h"

#include "android_log.h"
#include "android_loading_progress.h"
#include "level_metadata_scan.h"
}

#ifdef DXX_BUILD_DESCENT_II
extern "C" void piggy_init_pigfile(char *filename);
#endif

using json = nlohmann::ordered_json;

extern "C" int android_surface_get_display_width(void);
extern "C" int android_surface_get_display_height(void);

static std::string Level_preview_error;
static json Level_preview_request;
static std::string Level_preview_introspection;
static std::string Robot_preview_attack_summary;
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
static std::atomic<int> Robot_preview_pending_number(-1);
static std::atomic<int> Robot_preview_count(0);
static std::vector<int> Robot_preview_navigation_numbers;
static unsigned long long Robot_preview_frames;
static vms_angvec Robot_preview_anim_angles[MAX_SUBMODELS];
static vms_angvec Robot_preview_anim_source[MAX_SUBMODELS];
static vms_angvec Robot_preview_anim_target[MAX_SUBMODELS];
static std::atomic<int> Robot_preview_anim_state(AS_REST);
static std::atomic<int> Robot_preview_anim_frame(0);
static int Robot_preview_animated_joint_count;
static unsigned long long Robot_preview_motion_updates;
static unsigned int Robot_preview_anim_cycle_index;
static std::atomic<int> Robot_preview_sounds_enabled(0);
static unsigned int Robot_preview_sound_seed;
static Uint32 Robot_preview_next_sound_at;
static int Robot_preview_last_sound = -1;
static int Robot_preview_last_sound_sample = -1;
static const char *Robot_preview_last_sound_kind = "none";
static unsigned long long Robot_preview_sounds_played;
static std::atomic<int> Robot_preview_attack_enabled(0);
static int Robot_preview_attack_was_enabled;
static int Robot_preview_has_level;
static int Robot_preview_behavior;
static int Robot_preview_behavior_from_level;
static int Robot_preview_sub_flags;
static int Robot_preview_difficulty;
static fix Robot_preview_target_distance;
static fix Robot_preview_robot_offset_x;
static fix Robot_preview_robot_offset_y;
static fix Robot_preview_robot_distance_offset;
static fix Robot_preview_position_x;
static fix Robot_preview_position_y;
static fix Robot_preview_velocity_x;
static fix Robot_preview_velocity_y;
static Uint32 Robot_preview_next_fire_at;
static int Robot_preview_rapidfire_count;
static int Robot_preview_gun;
static int Robot_preview_last_weapon = -1;
static unsigned long long Robot_preview_shots_fired;
static unsigned long long Robot_preview_projectile_hits;
static unsigned long long Robot_preview_projectile_misses;
static unsigned long long Robot_preview_projectile_updates;
static unsigned long long Robot_preview_attack_frames;
static unsigned long long Robot_preview_actual_weapon_renders;
static unsigned long long Robot_preview_fallback_weapon_renders;
static unsigned long long Robot_preview_mines_dropped;
static unsigned long long Robot_preview_projectiles_retired_offscreen;
static unsigned long long Robot_preview_homing_reversals;
static unsigned long long Robot_preview_projectile_sequence;
static fix Robot_preview_normal_view_radius;
static fix Robot_preview_large_view_radius;
static fix Robot_preview_view_radius;
static int Robot_preview_scale_reference_robot = -1;
static fix Robot_preview_camera_scale = F1_0;
static const char *Robot_preview_camera_tier = "normal";
static fix Robot_preview_last_projectile_render_size;
static fix Robot_preview_last_projectile_render_scale = F1_0;
static vms_vector Robot_preview_last_projectile_origin;
static vms_vector Robot_preview_last_projectile_direction;
static fix Robot_preview_last_projectile_initial_speed;

struct robot_preview_projectile {
	int active;
	int weapon;
	vms_vector position;
	vms_vector velocity;
	vms_vector thrust;
	fix lifeleft;
	int mine;
	int used_actual_render;
	unsigned long long sequence;
};

static robot_preview_projectile Robot_preview_projectiles[32];

static const int Robot_preview_anim_cycle[] = {
	AS_ALERT, AS_FIRE, AS_RECOIL, AS_ALERT, AS_FLINCH, AS_REST
};
static const int ROBOT_PREVIEW_TRANSITION_FRAMES = 45;
static const Uint32 ROBOT_PREVIEW_SOUND_INTERVAL_MS = 3000;
static const fix ROBOT_PREVIEW_FRAME_TIME = F1_0 / 60;
static const fix ROBOT_PREVIEW_PHYSICS_TIME = F1_0 / 64;
static const fix ROBOT_PREVIEW_PROJECTILE_DISPLAY_DISTANCE = i2f(40);
static const int ROBOT_PREVIEW_MAX_ACTIVE_MINES = 2;
static const int ROBOT_PREVIEW_SUPER_HULK = 16;
#ifdef DXX_BUILD_DESCENT_II
static const int ROBOT_PREVIEW_MINI_REACTOR = 65;
#endif
// RUN_FROM is stored on level objects, not in the original D1/D2 robot records.
static const int ROBOT_PREVIEW_BASE_MINE_DROPPER = 10;

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
	debug_log_force(DLOG_GAME, "level preview failed: %s", message.c_str());
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

static const char *robot_preview_anim_state_name(int state)
{
	switch (state) {
		case AS_REST:
			return "rest";
		case AS_ALERT:
			return "alert";
		case AS_FIRE:
			return "fire";
		case AS_RECOIL:
			return "recoil";
		case AS_FLINCH:
			return "flinch";
		default:
			return "unknown";
	}
}

static int robot_preview_set_anim_target(int state);

static const char *robot_preview_behavior_name(int behavior)
{
	switch (behavior) {
		case AIB_STILL:
			return "still";
		case AIB_NORMAL:
			return "normal";
#ifdef DXX_BUILD_DESCENT_II
		case AIB_BEHIND:
			return "behind";
		case AIB_SNIPE:
			return "snipe";
		case AIB_FOLLOW:
			return "follow";
#else
		case AIB_HIDE:
			return "hide";
		case AIB_FOLLOW_PATH:
			return "follow path";
#endif
		case AIB_RUN_FROM:
			return "run from";
		case AIB_STATION:
			return "station";
		default:
			return "normal";
	}
}

static int robot_preview_find_behavior(int number, int *from_level)
{
	Robot_preview_sub_flags = 0;
	if (Robot_preview_has_level) {
		for (int objnum = 0; objnum <= Highest_object_index; ++objnum) {
			const object *obj = &Objects[objnum];
			if (obj->type == OBJ_ROBOT && obj->id == number) {
				*from_level = 1;
#ifdef DXX_BUILD_DESCENT_II
				Robot_preview_sub_flags = obj->ctype.ai_info.SUB_FLAGS;
#endif
				return obj->ctype.ai_info.behavior;
			}
		}
	}
	*from_level = 0;
#ifdef DXX_BUILD_DESCENT_II
	return Robot_info[number].behavior ? Robot_info[number].behavior : AIB_NORMAL;
#else
	return AIB_NORMAL;
#endif
}

enum robot_preview_attack_role {
	ROBOT_PREVIEW_ATTACK_NONE,
	ROBOT_PREVIEW_ATTACK_MELEE,
	ROBOT_PREVIEW_ATTACK_RANGED,
	ROBOT_PREVIEW_ATTACK_MINE_DROPPER
};

static robot_preview_attack_role robot_preview_attack_role_for(const robot_info *robot)
{
	if (robot->attack_type)
		return ROBOT_PREVIEW_ATTACK_MELEE;
#ifdef DXX_BUILD_DESCENT_II
	if (robot->companion || robot->thief)
		return ROBOT_PREVIEW_ATTACK_NONE;
#endif
	if (Robot_preview_behavior == AIB_RUN_FROM ||
	    (!Robot_preview_has_level && Robot_preview_number == ROBOT_PREVIEW_BASE_MINE_DROPPER))
		return ROBOT_PREVIEW_ATTACK_MINE_DROPPER;
	if (Robot_preview_number == ROBOT_BRAIN || robot->n_guns <= 0 ||
	    robot->weapon_type < 0 || robot->weapon_type >= N_weapon_types)
		return ROBOT_PREVIEW_ATTACK_NONE;
	return ROBOT_PREVIEW_ATTACK_RANGED;
}

static const char *robot_preview_attack_role_name(robot_preview_attack_role role)
{
	switch (role) {
		case ROBOT_PREVIEW_ATTACK_MELEE:
			return "melee";
		case ROBOT_PREVIEW_ATTACK_RANGED:
			return "ranged";
		case ROBOT_PREVIEW_ATTACK_MINE_DROPPER:
			return "mine dropper";
		default:
			return "non-firing";
	}
}

static int robot_preview_mine_weapon(void)
{
#ifdef DXX_BUILD_DESCENT_II
	if (Robot_preview_sub_flags & SUB_FLAGS_SPROX)
		return ROBOT_SUPERPROX_ID;
#endif
	return PROXIMITY_ID;
}

static int robot_preview_weapon_for_gun(const robot_info *robot, int gun)
{
#ifdef DXX_BUILD_DESCENT_II
	if (gun == 0 && robot->weapon_type2 >= 0)
		return robot->weapon_type2;
#else
	(void) gun;
#endif
	return robot->weapon_type;
}

struct robot_preview_dps_channel {
	const char *kind;
	int weapon;
	double damage_per_hit;
	double cycle_attacks;
	double cycle_seconds;
	double attacks_per_second;
	double dps;
};

struct robot_preview_dps_result {
	robot_preview_dps_channel primary;
	robot_preview_dps_channel secondary;
	int channel_count;
	int randomized_timing_average;
	double total;
};

static robot_preview_dps_channel robot_preview_dps_channel_for(
    const char *kind, int weapon, double damage_per_hit, double cycle_attacks,
    double cycle_seconds)
{
	robot_preview_dps_channel channel = {};
	channel.kind = kind;
	channel.weapon = weapon;
	channel.damage_per_hit = damage_per_hit;
	channel.cycle_attacks = cycle_attacks;
	channel.cycle_seconds = cycle_seconds;
	if (cycle_attacks > 0 && cycle_seconds > 0) {
		channel.attacks_per_second = cycle_attacks / cycle_seconds;
		channel.dps = damage_per_hit * channel.attacks_per_second;
	}
	return channel;
}

static robot_preview_dps_channel robot_preview_rapidfire_dps_channel(
    const char *kind, int weapon, double damage_per_hit, fix firing_wait,
    int rapidfire_count, int sniper_timing)
{
	const double long_wait = f2fl(firing_wait);
	if (long_wait <= 0)
		return robot_preview_dps_channel_for(kind, weapon, damage_per_hit, 0, 0);
	const int burst_attacks = (std::max) (1, rapidfire_count);
	const double cycle_attacks = sniper_timing && rapidfire_count > 0
	                                 ? burst_attacks * 2.0
	                                 : burst_attacks;
	const double short_wait = (std::min) (0.125, long_wait / 2.0);
	return robot_preview_dps_channel_for(
	    kind, weapon, damage_per_hit, cycle_attacks,
	    long_wait + (cycle_attacks - 1.0) * short_wait);
}

static void robot_preview_add_dps_channel(
    robot_preview_dps_result *result, const robot_preview_dps_channel &channel)
{
	if (result->channel_count == 0)
		result->primary = channel;
	else if (result->channel_count == 1)
		result->secondary = channel;
	else
		return;
	++result->channel_count;
	result->total += channel.dps;
}

static robot_preview_dps_result robot_preview_calculate_dps(const robot_info *robot)
{
	robot_preview_dps_result result = {};
	const robot_preview_attack_role role = robot_preview_attack_role_for(robot);
	const int difficulty = Robot_preview_difficulty;
	if (role == ROBOT_PREVIEW_ATTACK_NONE)
		return result;
	if (role == ROBOT_PREVIEW_ATTACK_MELEE) {
#ifdef DXX_BUILD_DESCENT_II
		const int sniper_timing = Robot_preview_behavior == AIB_SNIPE;
#else
		const int sniper_timing = 0;
#endif
		robot_preview_add_dps_channel(
		    &result,
		    robot_preview_rapidfire_dps_channel(
		        "melee", -1, difficulty + 1.0, robot->firing_wait[difficulty],
		        robot->rapidfire_count[difficulty], sniper_timing));
		result.randomized_timing_average = sniper_timing &&
		                                   robot->rapidfire_count[difficulty] > 0;
		return result;
	}
	if (role == ROBOT_PREVIEW_ATTACK_MINE_DROPPER) {
		const int weapon = robot_preview_mine_weapon();
		if (weapon >= 0 && weapon < N_weapon_types) {
#ifdef DXX_BUILD_DESCENT_II
			const fix wait = (F1_0 / 2) * (NDL + 5 - difficulty);
#else
			const fix wait = F1_0 * 5;
#endif
			robot_preview_add_dps_channel(
			    &result, robot_preview_dps_channel_for(
			                 "mine", weapon, f2fl(Weapon_info[weapon].strength[difficulty]),
			                 1.0, f2fl(wait)));
		}
		return result;
	}

#ifdef DXX_BUILD_DESCENT_II
	const int has_secondary = robot->n_guns > 0 && robot->weapon_type2 >= 0 &&
	                          robot->weapon_type2 < N_weapon_types;
	const int has_primary = robot->weapon_type >= 0 && robot->weapon_type < N_weapon_types &&
	                        (!has_secondary || robot->n_guns > 1);
	const int sniper_timing = Robot_preview_behavior == AIB_SNIPE;
#else
	const int has_secondary = 0;
	const int has_primary = robot->weapon_type >= 0 && robot->weapon_type < N_weapon_types;
	const int sniper_timing = 0;
#endif
	if (has_primary) {
		robot_preview_add_dps_channel(
		    &result,
		    robot_preview_rapidfire_dps_channel(
		        "primary", robot->weapon_type,
		        f2fl(Weapon_info[robot->weapon_type].strength[difficulty]),
		        robot->firing_wait[difficulty], robot->rapidfire_count[difficulty],
		        sniper_timing));
		result.randomized_timing_average = sniper_timing &&
		                                   robot->rapidfire_count[difficulty] > 0;
	}
#ifdef DXX_BUILD_DESCENT_II
	if (has_secondary) {
		robot_preview_add_dps_channel(
		    &result,
		    robot_preview_dps_channel_for(
		        "secondary", robot->weapon_type2,
		        f2fl(Weapon_info[robot->weapon_type2].strength[difficulty]), 1.0,
		        f2fl(robot->firing_wait2[difficulty])));
	}
#else
	(void) has_secondary;
#endif
	return result;
}

static json robot_preview_dps_channel_json(const robot_preview_dps_channel &channel)
{
	return {
		{ "kind", channel.kind ? channel.kind : "" },
		{ "weapon", channel.weapon },
		{ "damage_per_hit", channel.damage_per_hit },
		{ "cycle_attacks", channel.cycle_attacks },
		{ "cycle_seconds", channel.cycle_seconds },
		{ "attacks_per_second", channel.attacks_per_second },
		{ "dps", channel.dps }
	};
}

static json robot_preview_dps_json(const robot_preview_dps_result &result)
{
	json channels = json::array();
	if (result.channel_count > 0)
		channels.push_back(robot_preview_dps_channel_json(result.primary));
	if (result.channel_count > 1)
		channels.push_back(robot_preview_dps_channel_json(result.secondary));
	return {
		{ "total", result.total },
		{ "assumption", "all direct damaging attacks connect" },
		{ "randomized_timing_average", result.randomized_timing_average != 0 },
		{ "channels", channels }
	};
}

static void robot_preview_reset_attack_state(void)
{
	memset(Robot_preview_projectiles, 0, sizeof(Robot_preview_projectiles));
	Robot_preview_next_fire_at = SDL_GetTicks() + 350;
	Robot_preview_rapidfire_count = 0;
	Robot_preview_gun = 0;
	Robot_preview_last_weapon = -1;
	Robot_preview_robot_offset_x = 0;
	Robot_preview_robot_offset_y = 0;
	Robot_preview_robot_distance_offset = 0;
	Robot_preview_position_x = 0;
	Robot_preview_position_y = 0;
	Robot_preview_velocity_x = 0;
	Robot_preview_velocity_y = 0;
}

static void robot_preview_limit_velocity(fix max_speed)
{
	const fix speed = fix_sqrt(
	    fixmul(Robot_preview_velocity_x, Robot_preview_velocity_x) +
	    fixmul(Robot_preview_velocity_y, Robot_preview_velocity_y));
	if (speed > max_speed) {
		Robot_preview_velocity_x = Robot_preview_velocity_x * 3 / 4;
		Robot_preview_velocity_y = Robot_preview_velocity_y * 3 / 4;
	}
}

static void robot_preview_update_movement(void)
{
	const robot_info *robot = &Robot_info[Robot_preview_number];
	const fix distance = Robot_preview_target_distance - Robot_preview_position_x;
	const fix circle = (std::max) (i2f(10), robot->circle_distance[Robot_preview_difficulty]);
	fix max_speed = robot->max_speed[Robot_preview_difficulty];
	int move = 0;
	if (Robot_preview_behavior == AIB_RUN_FROM)
		move = -1;
	else if (Robot_preview_behavior == AIB_STILL || Robot_preview_behavior == AIB_STATION)
		move = 0;
#ifdef DXX_BUILD_DESCENT_II
	else if (Robot_preview_behavior == AIB_SNIPE)
		move = (Robot_preview_frames / 180) & 1 ? -1 : 1;
	else if (Robot_preview_behavior == AIB_BEHIND)
		move = 2;
#else
	else if (Robot_preview_behavior == AIB_HIDE)
		move = (Robot_preview_frames / 240) & 1 ? -1 : 2;
#endif
	else if (robot->attack_type)
		move = distance < i2f(30) && Robot_preview_next_fire_at > SDL_GetTicks() ? -1 : 1;
	else if (distance < circle)
		move = -1;
	else if (distance < circle * 2)
		move = 2;
	else
		move = 1;

	if (move > 0 && move != 2) {
		Robot_preview_velocity_x +=
		    fixmul(F1_0, ROBOT_PREVIEW_FRAME_TIME * 64) * (Robot_preview_difficulty + 5) / 4;
		if (robot->attack_type)
			max_speed *= 2;
	} else if (move < 0) {
		Robot_preview_velocity_x -= fixmul(F1_0, ROBOT_PREVIEW_FRAME_TIME * 16);
	} else if (move == 2) {
		const int direction = ((Robot_preview_number ^ (Robot_preview_frames >> 5)) & 1) ? 1 : -1;
		Robot_preview_velocity_y += direction * fixmul(F1_0, ROBOT_PREVIEW_FRAME_TIME * 32);
	}
	const int evade = robot->evade_speed[Robot_preview_difficulty];
	if (evade > 0 && ((Robot_preview_frames + Robot_preview_number * 3) & 63) < 24) {
		const int direction = ((Robot_preview_number ^ (Robot_preview_frames >> 5)) & 1) ? 1 : -1;
		Robot_preview_velocity_y +=
		    direction * fixmul(F1_0, ROBOT_PREVIEW_FRAME_TIME * 8) * evade;
	}
	robot_preview_limit_velocity((std::max) (F1_0, max_speed));
	if (robot->drag) {
		int count = ROBOT_PREVIEW_FRAME_TIME / ROBOT_PREVIEW_PHYSICS_TIME;
		const fix remainder = ROBOT_PREVIEW_FRAME_TIME % ROBOT_PREVIEW_PHYSICS_TIME;
		const fix fraction = fixdiv(remainder, ROBOT_PREVIEW_PHYSICS_TIME);
		fix total_drag = F1_0;
		while (count--)
			total_drag = fixmul(total_drag, F1_0 - robot->drag);
		total_drag = fixmul(total_drag, F1_0 - fixmul(fraction, robot->drag));
		Robot_preview_velocity_x = fixmul(Robot_preview_velocity_x, total_drag);
		Robot_preview_velocity_y = fixmul(Robot_preview_velocity_y, total_drag);
	}
	Robot_preview_position_x += fixmul(Robot_preview_velocity_x, ROBOT_PREVIEW_FRAME_TIME);
	Robot_preview_position_y += fixmul(Robot_preview_velocity_y, ROBOT_PREVIEW_FRAME_TIME);
	if (Robot_preview_position_x > Robot_preview_target_distance - i2f(5)) {
		Robot_preview_position_x = Robot_preview_target_distance - i2f(5);
		Robot_preview_velocity_x = -abs(Robot_preview_velocity_x);
	} else if (Robot_preview_position_x < -Robot_preview_target_distance) {
		Robot_preview_position_x = -Robot_preview_target_distance;
		Robot_preview_velocity_x = abs(Robot_preview_velocity_x);
	}
	if (abs(Robot_preview_position_y) > Robot_preview_target_distance / 2)
		Robot_preview_velocity_y = -Robot_preview_velocity_y;
	const fix radius = Polygon_models[Robot_preview_model].rad;
	Robot_preview_robot_offset_x = fixmuldiv(
	    radius, Robot_preview_position_x, Robot_preview_target_distance * 2);
	Robot_preview_robot_offset_y = fixmuldiv(
	    radius, Robot_preview_position_y, Robot_preview_target_distance * 2);
	Robot_preview_robot_distance_offset = Robot_preview_robot_offset_x / 8;
}

static void robot_preview_spawn_projectile(int weapon_number, int mine)
{
	if (weapon_number < 0 || weapon_number >= N_weapon_types)
		return;
	if (mine) {
		int active_mines = 0;
		robot_preview_projectile *oldest = NULL;
		for (robot_preview_projectile &candidate : Robot_preview_projectiles) {
			if (!candidate.active || !candidate.mine)
				continue;
			++active_mines;
			if (!oldest || candidate.sequence < oldest->sequence)
				oldest = &candidate;
		}
		if (active_mines >= ROBOT_PREVIEW_MAX_ACTIVE_MINES && oldest)
			oldest->active = 0;
	}
	robot_preview_projectile *projectile = NULL;
	for (robot_preview_projectile &candidate : Robot_preview_projectiles)
		if (!candidate.active) {
			projectile = &candidate;
			break;
		}
	if (!projectile)
		projectile = &Robot_preview_projectiles[Robot_preview_shots_fired %
		                                        (sizeof(Robot_preview_projectiles) /
		                                         sizeof(Robot_preview_projectiles[0]))];
	const weapon_info *weapon = &Weapon_info[weapon_number];
	fix speed = weapon->speed[Robot_preview_difficulty];
#ifdef DXX_BUILD_DESCENT_II
	if (weapon->speedvar != 128) {
		Robot_preview_sound_seed = Robot_preview_sound_seed * 1664525u + 1013904223u;
		const fix random = static_cast<fix>((Robot_preview_sound_seed >> 16) & 0x7fff);
		const fix scale = F1_0 - static_cast<fix>((random * weapon->speedvar) >> 6);
		speed = fixmul(speed, scale);
	}
#endif
	if (weapon->thrust)
		speed /= 2;
	const robot_info *robot = &Robot_info[Robot_preview_number];
	vms_angvec angles = {
		static_cast<fixang>(Robot_preview_pitch.load(std::memory_order_relaxed)),
		0,
		static_cast<fixang>(Robot_preview_heading.load(std::memory_order_relaxed))
	};
	vms_matrix orientation;
	vm_angles_2_matrix(&orientation, &angles);
	vms_vector direction = orientation.fvec;
	if (mine)
		vm_vec_negate(&direction);
	projectile->active = 1;
	projectile->weapon = weapon_number;
	projectile->position = {
		Robot_preview_robot_offset_x, Robot_preview_robot_offset_y,
		Robot_preview_robot_distance_offset
	};
	if (mine) {
		vm_vec_scale_add2(&projectile->position, &direction, F1_0);
	} else {
		const int gun = (std::max) (0, (std::min) (Robot_preview_gun, static_cast<int>(robot->n_guns) - 1));
		object preview_robot = {};
		preview_robot.type = OBJ_ROBOT;
		preview_robot.id = Robot_preview_number;
		preview_robot.render_type = RT_POLYOBJ;
		preview_robot.pos = projectile->position;
		preview_robot.orient = orientation;
		memcpy(preview_robot.rtype.pobj_info.anim_angles, Robot_preview_anim_angles,
		       sizeof(Robot_preview_anim_angles));
		calc_gun_point(&projectile->position, &preview_robot, gun);
	}
	Robot_preview_sound_seed = Robot_preview_sound_seed * 1664525u + 1013904223u;
	fix aim_spread = i2f((NDL - Robot_preview_difficulty - 1) * 2);
#ifdef DXX_BUILD_DESCENT_II
	aim_spread = fixmuldiv(aim_spread, 287 - robot->aim, 287);
#endif
	const fix aim_error = static_cast<fix>(
	    static_cast<long long>(static_cast<int>((Robot_preview_sound_seed >> 16) & 0xffff) - 32768) *
	    aim_spread / 32768);
	if (!mine && aim_error) {
		direction.x += fixmul(orientation.rvec.x, fixdiv(aim_error, ROBOT_PREVIEW_PROJECTILE_DISPLAY_DISTANCE));
		direction.y += fixmul(orientation.rvec.y, fixdiv(aim_error, ROBOT_PREVIEW_PROJECTILE_DISPLAY_DISTANCE));
		direction.z += fixmul(orientation.rvec.z, fixdiv(aim_error, ROBOT_PREVIEW_PROJECTILE_DISPLAY_DISTANCE));
		vm_vec_normalize_quick(&direction);
	}
	vm_vec_copy_scale(&projectile->velocity, &direction, speed);
	vm_vec_copy_scale(&projectile->thrust, &direction, weapon->thrust);
	projectile->lifeleft = weapon->lifetime > 0 ? weapon->lifetime : WEAPON_DEFAULT_LIFETIME;
	projectile->mine = mine;
	projectile->used_actual_render = 0;
	projectile->sequence = ++Robot_preview_projectile_sequence;
	Robot_preview_last_projectile_origin = projectile->position;
	Robot_preview_last_projectile_direction = direction;
	Robot_preview_last_projectile_initial_speed = speed;
	Robot_preview_last_weapon = weapon_number;
	++Robot_preview_shots_fired;
	Robot_preview_mines_dropped += mine != 0;
	if (Robot_preview_sounds_enabled.load(std::memory_order_relaxed) && weapon->flash_sound >= 0)
		digi_play_sample(weapon->flash_sound, F1_0);
}

static void robot_preview_schedule_fire(Uint32 now)
{
	if (now < Robot_preview_next_fire_at)
		return;
	const robot_info *robot = &Robot_info[Robot_preview_number];
	const robot_preview_attack_role role = robot_preview_attack_role_for(robot);
	if (role == ROBOT_PREVIEW_ATTACK_NONE)
		return;
	if (role == ROBOT_PREVIEW_ATTACK_MELEE) {
		robot_preview_set_anim_target(AS_FIRE);
		const int wait_ms = static_cast<int>(
		    static_cast<long long>(robot->firing_wait[Robot_preview_difficulty]) * 1000 / F1_0);
		Robot_preview_next_fire_at = now + (std::max) (125, wait_ms);
		return;
	}
	if (role == ROBOT_PREVIEW_ATTACK_MINE_DROPPER) {
		robot_preview_spawn_projectile(robot_preview_mine_weapon(), 1);
		robot_preview_set_anim_target(AS_FIRE);
#ifdef DXX_BUILD_DESCENT_II
		const fix wait = (F1_0 / 2) * (NDL + 5 - Robot_preview_difficulty);
#else
		const fix wait = F1_0 * 5;
#endif
		Robot_preview_next_fire_at =
		    now + static_cast<Uint32>(static_cast<long long>(wait) * 1000 / F1_0);
		return;
	}
	const int gun_count = (std::max) (1, static_cast<int>(robot->n_guns));
	Robot_preview_gun = (Robot_preview_gun + 1) % gun_count;
	robot_preview_spawn_projectile(robot_preview_weapon_for_gun(robot, Robot_preview_gun), 0);
	robot_preview_set_anim_target(AS_FIRE);
	++Robot_preview_rapidfire_count;
	fix wait = robot->firing_wait[Robot_preview_difficulty];
#ifdef DXX_BUILD_DESCENT_II
	if (Robot_preview_gun == 0 && robot->weapon_type2 >= 0)
		wait = robot->firing_wait2[Robot_preview_difficulty];
	else
#endif
	    if (Robot_preview_rapidfire_count < robot->rapidfire_count[Robot_preview_difficulty])
		wait = (std::min) (F1_0 / 8, wait / 2);
	else
		Robot_preview_rapidfire_count = 0;
	const int wait_ms = static_cast<int>(static_cast<long long>(wait) * 1000 / F1_0);
	Robot_preview_next_fire_at = now + (std::max) (60, wait_ms);
}

static void robot_preview_update_projectiles(void)
{
	const fix radius = (std::max) (F1_0, Robot_preview_view_radius);
	const fix display_scale = fixdiv(radius * 3, ROBOT_PREVIEW_PROJECTILE_DISPLAY_DISTANCE);
	vms_vector target = { radius * 3, 0, 0 };
	for (robot_preview_projectile &projectile : Robot_preview_projectiles) {
		if (!projectile.active)
			continue;
		const weapon_info *weapon = &Weapon_info[projectile.weapon];
		if (weapon->homing_flag && projectile.lifeleft < weapon->lifetime - F1_0 / 2) {
			const fix previous_velocity_x = projectile.velocity.x;
			vms_vector desired;
			vm_vec_sub(&desired, &target, &projectile.position);
			vms_vector velocity = projectile.velocity;
			vm_vec_normalize_quick(&desired);
			fix speed = vm_vec_normalize_quick(&velocity);
			const fix max_speed = weapon->speed[Robot_preview_difficulty];
			if (speed + F1_0 < max_speed) {
				speed += fixmul(max_speed, ROBOT_PREVIEW_FRAME_TIME / 2);
				if (speed > max_speed)
					speed = max_speed;
			}
			vm_vec_add2(&velocity, &desired);
			if (weapon->render_type != WEAPON_RENDER_POLYMODEL)
				vm_vec_add2(&velocity, &desired);
			vm_vec_normalize_quick(&velocity);
			vm_vec_copy_scale(&projectile.velocity, &velocity, speed);
			Robot_preview_homing_reversals +=
			    previous_velocity_x > 0 && projectile.velocity.x < 0;
		}
		if (weapon->drag) {
			int count = ROBOT_PREVIEW_FRAME_TIME / ROBOT_PREVIEW_PHYSICS_TIME;
			const fix remainder = ROBOT_PREVIEW_FRAME_TIME % ROBOT_PREVIEW_PHYSICS_TIME;
			const fix fraction = fixdiv(remainder, ROBOT_PREVIEW_PHYSICS_TIME);
			while (count--) {
				if (weapon->thrust && weapon->mass) {
					vm_vec_scale_add2(
					    &projectile.velocity, &projectile.thrust, fixdiv(F1_0, weapon->mass));
				}
				vm_vec_scale(&projectile.velocity, F1_0 - weapon->drag);
			}
			if (weapon->thrust && weapon->mass) {
				vm_vec_scale_add2(
				    &projectile.velocity, &projectile.thrust,
				    fixmul(fixdiv(F1_0, weapon->mass), fraction));
			}
			const fix fractional_drag = F1_0 - fixmul(fraction, weapon->drag);
			vm_vec_scale(&projectile.velocity, fractional_drag);
		}
		vm_vec_scale_add2(
		    &projectile.position, &projectile.velocity,
		    fixmul(ROBOT_PREVIEW_FRAME_TIME, display_scale));
		projectile.lifeleft -= ROBOT_PREVIEW_FRAME_TIME;
		++Robot_preview_projectile_updates;
		const int reached_homing_target = weapon->homing_flag && projectile.position.x >= target.x;
		if (!projectile.mine &&
		    (reached_homing_target || vm_vec_dist_quick(&projectile.position, &target) <= radius / 2)) {
			if (projectile.lifeleft > 0)
				++Robot_preview_projectile_hits;
			projectile.active = 0;
		} else if (projectile.lifeleft <= 0) {
			++Robot_preview_projectile_misses;
			projectile.active = 0;
		}
	}
}

static int robot_preview_active_projectiles(void)
{
	int count = 0;
	for (const robot_preview_projectile &projectile : Robot_preview_projectiles)
		count += projectile.active != 0;
	return count;
}

static int robot_preview_active_mines(void)
{
	int count = 0;
	for (const robot_preview_projectile &projectile : Robot_preview_projectiles)
		count += projectile.active && projectile.mine;
	return count;
}

static int robot_preview_projectile_is_offscreen(
    const robot_preview_projectile *projectile, const vms_vector *robot_position)
{
	if (projectile->mine)
		return 0;
	const weapon_info *weapon = &Weapon_info[projectile->weapon];
	const fix visual_radius = (std::max) (F1_0 / 2, weapon->blob_size);
	const vms_vector position = {
		robot_position->x - Robot_preview_robot_offset_x + projectile->position.x,
		robot_position->y - Robot_preview_robot_offset_y + projectile->position.y,
		robot_position->z - Robot_preview_robot_distance_offset + projectile->position.z
	};
	if (position.z <= 0)
		return 1;
	return abs(position.x) > position.z * 2 + visual_radius ||
	       abs(position.y) > position.z + visual_radius;
}

static fix robot_preview_projectile_render_scale(const weapon_info *weapon, int mine)
{
#ifndef DXX_BUILD_DESCENT_II
	const fix minimum_size = Robot_preview_view_radius / 5;
	const fix minimum_scale = F1_0 * 3 / 2;
	if (mine && weapon->model_num >= 0 && weapon->model_num < N_polygon_models) {
		const fix radius = Polygon_models[weapon->model_num].rad;
		return radius > 0 ? (std::max) (minimum_scale, fixdiv(minimum_size, radius))
		                  : minimum_scale;
	}
	if (weapon->render_type == WEAPON_RENDER_VCLIP) {
		const int vclip = weapon->weapon_vclip;
		if (vclip >= 0 && vclip < VCLIP_MAXNUM && Vclip[vclip].num_frames > 0) {
			const int bitmap_index = Vclip[vclip].frames[0].index;
			if (bitmap_index >= 0 && bitmap_index < MAX_BITMAP_FILES) {
				const grs_bitmap *bitmap = &GameBitmaps[bitmap_index];
				const int largest = (std::max) (bitmap->bm_w, bitmap->bm_h);
				const int smallest = (std::min) (bitmap->bm_w, bitmap->bm_h);
				if (smallest > 0 && largest > smallest * 2)
					return F1_0;
			}
		}
		return weapon->blob_size > 0
		           ? (std::max) (minimum_scale, fixdiv(minimum_size, weapon->blob_size))
		           : minimum_scale;
	}
	if (weapon->render_type == WEAPON_RENDER_BLOB && weapon->bitmap.index >= 0 &&
	    weapon->bitmap.index < MAX_BITMAP_FILES) {
		const grs_bitmap *bitmap = &GameBitmaps[weapon->bitmap.index];
		const int largest = (std::max) (bitmap->bm_w, bitmap->bm_h);
		const int smallest = (std::min) (bitmap->bm_w, bitmap->bm_h);
		if (smallest > 0 && largest <= smallest * 2 && weapon->blob_size > 0 &&
		    weapon->blob_size < minimum_size)
			return (std::max) (minimum_scale, fixdiv(minimum_size, weapon->blob_size));
		if (smallest > 0 && largest <= smallest * 2)
			return minimum_scale;
	}
#else
	(void) weapon;
	(void) mine;
#endif
	return F1_0;
}

static int robot_preview_draw_weapon(
    const robot_preview_projectile *projectile, const vms_vector *robot_position)
{
	const weapon_info *weapon = &Weapon_info[projectile->weapon];
	object preview_object = {};
	preview_object.type = OBJ_WEAPON;
	preview_object.id = projectile->weapon;
	preview_object.pos = {
		robot_position->x - Robot_preview_robot_offset_x + projectile->position.x,
		robot_position->y - Robot_preview_robot_offset_y + projectile->position.y,
		robot_position->z - Robot_preview_robot_distance_offset + projectile->position.z
	};
	preview_object.lifeleft = projectile->lifeleft;
	preview_object.size = weapon->blob_size > 0 ? weapon->blob_size : F1_0 / 2;
	const fix render_scale = robot_preview_projectile_render_scale(weapon, projectile->mine);
	Robot_preview_last_projectile_render_scale = render_scale;
	vms_vector direction = projectile->velocity;
	if (!direction.x && !direction.y && !direction.z)
		direction.x = F1_0;
	vm_vector_2_matrix(&preview_object.orient, &direction, NULL, NULL);

	switch (weapon->render_type) {
		case WEAPON_RENDER_LASER:
		case WEAPON_RENDER_BLOB:
			preview_object.size = fixmul(preview_object.size, render_scale);
			Robot_preview_last_projectile_render_size = preview_object.size;
			draw_object_blob(&preview_object, weapon->bitmap);
			return 1;
		case WEAPON_RENDER_POLYMODEL: {
			const int model = weapon->model_num;
			if (model < 0 || model >= N_polygon_models)
				return 0;
			Robot_preview_last_projectile_render_size =
			    fixmul(Polygon_models[model].rad, render_scale);
			if (render_scale != F1_0)
				vm_vec_scale(&preview_object.pos, fixdiv(F1_0, render_scale));
			g3s_lrgb light = { F1_0, F1_0, F1_0 };
			draw_polygon_model(
			    &preview_object.pos, &preview_object.orient, NULL, model, 0, light, NULL, NULL);
			return 1;
		}
		case WEAPON_RENDER_VCLIP: {
			const int vclip = weapon->weapon_vclip;
			if (vclip < 0 || vclip >= VCLIP_MAXNUM || Vclip[vclip].num_frames <= 0 ||
			    Vclip[vclip].play_time <= 0)
				return 0;
			preview_object.size = fixmul(preview_object.size, render_scale);
			Robot_preview_last_projectile_render_size = preview_object.size;
			fix frame_time = projectile->lifeleft % Vclip[vclip].play_time;
			if (frame_time < 0)
				frame_time += Vclip[vclip].play_time;
			draw_vclip_object(&preview_object, frame_time, 0, vclip);
			return 1;
		}
		default:
			return 0;
	}
}

static void robot_preview_draw_projectiles(const vms_vector *robot_position, void *data)
{
	(void) data;
	for (robot_preview_projectile &projectile : Robot_preview_projectiles) {
		if (!projectile.active)
			continue;
		if (robot_preview_projectile_is_offscreen(&projectile, robot_position)) {
			projectile.active = 0;
			++Robot_preview_projectiles_retired_offscreen;
			continue;
		}
		const int actual = robot_preview_draw_weapon(&projectile, robot_position);
		if (!projectile.used_actual_render) {
			if (actual)
				++Robot_preview_actual_weapon_renders;
			else
				++Robot_preview_fallback_weapon_renders;
			projectile.used_actual_render = actual ? 1 : -1;
		}
	}
}

static void robot_preview_draw_attack_overlay(void)
{
	const int right = SWIDTH * 84 / 100;
	const int center_y = SHEIGHT / 2;
	gr_setcolor(BM_XRGB(18, 42, 18));
	gr_line(i2f(right - 8), i2f(center_y), i2f(right + 8), i2f(center_y));
	gr_line(i2f(right), i2f(center_y - 8), i2f(right), i2f(center_y + 8));
	for (const robot_preview_projectile &projectile : Robot_preview_projectiles) {
		if (!projectile.active || projectile.used_actual_render >= 0)
			continue;
		const fix radius = (std::max) (F1_0, Polygon_models[Robot_preview_model].rad);
		const int x = SWIDTH / 2 + fixmuldiv(projectile.position.x, SWIDTH / 3, radius * 3);
		const int y = center_y - fixmuldiv(projectile.position.y, SHEIGHT / 3, radius * 3);
		gr_setcolor(BM_XRGB(48, 20, 48));
		gr_disk(i2f(x), i2f(y), i2f(3));
	}
}

static int robot_preview_set_anim_target(int state)
{
	int animated_joints[MAX_SUBMODELS] = {};
	int animated_joint_count = 0;
	const robot_info *robot = &Robot_info[Robot_preview_number];
	const int gun_count = robot->n_guns >= 0 && robot->n_guns <= MAX_GUNS
	                          ? robot->n_guns
	                          : 0;
	const int model_count = Polygon_models[Robot_preview_model].n_models;

	memcpy(Robot_preview_anim_source, Robot_preview_anim_angles,
	       sizeof(Robot_preview_anim_source));
	memcpy(Robot_preview_anim_target, Robot_preview_anim_angles,
	       sizeof(Robot_preview_anim_target));
	for (int gun = 0; gun <= gun_count; ++gun) {
		const jointlist *list = &robot->anim_states[gun][state];
		if (list->offset < 0 || list->n_joints < 0 ||
		    list->offset + list->n_joints > N_robot_joints)
			continue;
		for (int joint = 0; joint < list->n_joints; ++joint) {
			const jointpos *position = &Robot_joints[list->offset + joint];
			if (position->jointnum < 0 || position->jointnum >= model_count ||
			    position->jointnum >= MAX_SUBMODELS)
				continue;
			Robot_preview_anim_target[position->jointnum] = position->angles;
			if (!animated_joints[position->jointnum]) {
				animated_joints[position->jointnum] = 1;
				++animated_joint_count;
			}
		}
	}
	Robot_preview_anim_state.store(state, std::memory_order_relaxed);
	Robot_preview_anim_frame.store(0, std::memory_order_relaxed);
	return animated_joint_count;
}

static fixang robot_preview_interpolate_angle(fixang source, fixang target, int frame)
{
	const int delta = static_cast<int16_t>(target - source);
	return static_cast<fixang>(source + delta * frame / ROBOT_PREVIEW_TRANSITION_FRAMES);
}

static void robot_preview_animate(void)
{
	int frame = Robot_preview_anim_frame.load(std::memory_order_relaxed) + 1;
	if (frame > ROBOT_PREVIEW_TRANSITION_FRAMES)
		frame = ROBOT_PREVIEW_TRANSITION_FRAMES;
	int moved = 0;
	for (int joint = 0; joint < MAX_SUBMODELS; ++joint) {
		vms_angvec next;
		next.p = robot_preview_interpolate_angle(
		    Robot_preview_anim_source[joint].p, Robot_preview_anim_target[joint].p, frame);
		next.b = robot_preview_interpolate_angle(
		    Robot_preview_anim_source[joint].b, Robot_preview_anim_target[joint].b, frame);
		next.h = robot_preview_interpolate_angle(
		    Robot_preview_anim_source[joint].h, Robot_preview_anim_target[joint].h, frame);
		moved |= next.p != Robot_preview_anim_angles[joint].p ||
		         next.b != Robot_preview_anim_angles[joint].b ||
		         next.h != Robot_preview_anim_angles[joint].h;
		Robot_preview_anim_angles[joint] = next;
	}
	if (moved)
		++Robot_preview_motion_updates;
	Robot_preview_anim_frame.store(frame, std::memory_order_relaxed);
	if (frame == ROBOT_PREVIEW_TRANSITION_FRAMES) {
		Robot_preview_anim_cycle_index =
		    (Robot_preview_anim_cycle_index + 1) %
		    (sizeof(Robot_preview_anim_cycle) / sizeof(Robot_preview_anim_cycle[0]));
		const int count =
		    robot_preview_set_anim_target(Robot_preview_anim_cycle[Robot_preview_anim_cycle_index]);
		if (count > Robot_preview_animated_joint_count)
			Robot_preview_animated_joint_count = count;
	}
}

static fix robot_preview_model_radius(int number)
{
	if (number < 0 || number >= N_robot_types)
		return 0;
	const int model = Robot_info[number].model_num;
	return model >= 0 && model < N_polygon_models ? Polygon_models[model].rad : 0;
}

static int robot_preview_uses_large_camera(int number)
{
	if (number < 0 || number >= N_robot_types)
		return 0;
#ifdef DXX_BUILD_DESCENT_II
	if (number == ROBOT_PREVIEW_MINI_REACTOR)
		return 1;
#endif
	return Robot_info[number].boss_flag != 0;
}

static int robot_preview_scale_reference(int number)
{
#ifdef DXX_BUILD_DESCENT_II
	switch (number) {
		case 5:
		case 19:
			return 19;
		case 6:
			return 0;
		case 18:
			return 1;
		case 54:
			return 40;
		case 55:
			return 25;
	}
#else
	(void) number;
#endif
	return -1;
}

static void robot_preview_configure_camera_tiers(void)
{
	Robot_preview_normal_view_radius = robot_preview_model_radius(ROBOT_PREVIEW_SUPER_HULK);
	if (Robot_preview_normal_view_radius <= 0)
		Robot_preview_normal_view_radius = i2f(3);
	Robot_preview_large_view_radius = Robot_preview_normal_view_radius;
	for (int number = 0; number < N_robot_types; ++number) {
		const fix radius = robot_preview_model_radius(number);
		if (robot_preview_uses_large_camera(number) &&
		    radius > Robot_preview_large_view_radius)
			Robot_preview_large_view_radius = radius;
	}
}

static int robot_preview_configure_robot(int number)
{
	if (number < 0 || number >= N_robot_types)
		return 0;
	const int model = Robot_info[number].model_num;
	if (model < 0 || model >= N_polygon_models)
		return 0;
	Robot_preview_number = number;
	Robot_preview_model = model;
	if (robot_preview_uses_large_camera(number)) {
		Robot_preview_camera_tier = "large";
		Robot_preview_view_radius = Robot_preview_large_view_radius;
	} else {
		Robot_preview_camera_tier = "normal";
		Robot_preview_view_radius = Robot_preview_normal_view_radius;
	}
	Robot_preview_scale_reference_robot = robot_preview_scale_reference(number);
	Robot_preview_camera_scale = F1_0;
	if (Robot_preview_scale_reference_robot >= 0) {
		const fix reference_radius = robot_preview_model_radius(Robot_preview_scale_reference_robot);
		const fix model_radius = robot_preview_model_radius(number);
		if (reference_radius > 0 && model_radius > 0)
			Robot_preview_view_radius =
			    fixmuldiv(Robot_preview_view_radius, model_radius, reference_radius);
		if (Robot_preview_scale_reference_robot == 19)
			Robot_preview_camera_scale = F1_0 * 4 / 3;
		Robot_preview_view_radius = fixmul(Robot_preview_view_radius, Robot_preview_camera_scale);
	}
	Robot_preview_behavior = robot_preview_find_behavior(number, &Robot_preview_behavior_from_level);
	Robot_preview_target_distance = (std::max) (i2f(40), Robot_info[number].circle_distance[Robot_preview_difficulty]);
	memset(Robot_preview_anim_angles, 0, sizeof(Robot_preview_anim_angles));
	memset(Robot_preview_anim_source, 0, sizeof(Robot_preview_anim_source));
	memset(Robot_preview_anim_target, 0, sizeof(Robot_preview_anim_target));
	Robot_preview_anim_state.store(AS_REST, std::memory_order_relaxed);
	Robot_preview_anim_frame.store(0, std::memory_order_relaxed);
	Robot_preview_animated_joint_count = robot_preview_set_anim_target(AS_REST);
	memcpy(Robot_preview_anim_angles, Robot_preview_anim_target,
	       sizeof(Robot_preview_anim_angles));
	const int alert_joint_count = robot_preview_set_anim_target(AS_ALERT);
	if (alert_joint_count > Robot_preview_animated_joint_count)
		Robot_preview_animated_joint_count = alert_joint_count;
	Robot_preview_motion_updates = 0;
	Robot_preview_anim_cycle_index = 0;
	Robot_preview_heading.store(F0_5 - 1, std::memory_order_release);
	Robot_preview_pitch.store(0, std::memory_order_release);
	Robot_preview_next_sound_at = SDL_GetTicks() + ROBOT_PREVIEW_SOUND_INTERVAL_MS;
	robot_preview_reset_attack_state();
	Robot_preview_last_projectile_origin = ZERO_VECTOR;
	Robot_preview_last_projectile_direction = ZERO_VECTOR;
	Robot_preview_last_projectile_initial_speed = 0;
	return 1;
}

static void robot_preview_configure_navigation(const json &request)
{
	Robot_preview_navigation_numbers.clear();
	const auto requested = request.find("robot_numbers");
	if (requested != request.end() && requested->is_array()) {
		for (const json &value : *requested) {
			if (!value.is_number_integer())
				continue;
			const int number = value.get<int>();
			if (number < 0 || number >= N_robot_types || robot_preview_model_radius(number) <= 0 ||
			    std::find(
			        Robot_preview_navigation_numbers.begin(), Robot_preview_navigation_numbers.end(),
			        number) != Robot_preview_navigation_numbers.end())
				continue;
			Robot_preview_navigation_numbers.push_back(number);
		}
	} else {
		for (int number = 0; number < N_robot_types; ++number)
			if (robot_preview_model_radius(number) > 0)
				Robot_preview_navigation_numbers.push_back(number);
	}
	if (std::find(
	        Robot_preview_navigation_numbers.begin(), Robot_preview_navigation_numbers.end(),
	        Robot_preview_number) == Robot_preview_navigation_numbers.end())
		Robot_preview_navigation_numbers.insert(
		    Robot_preview_navigation_numbers.begin(), Robot_preview_number);
	Robot_preview_count.store(
	    static_cast<int>(Robot_preview_navigation_numbers.size()), std::memory_order_release);
}

static void robot_preview_play_sound(void)
{
	const robot_info *robot = &Robot_info[Robot_preview_number];
	int sounds[3];
	const char *kinds[3];
	int sound_count = 0;
	const int candidates[] = {
		robot->see_sound, robot->attack_sound, robot->claw_sound
	};
	const char *candidate_kinds[] = { "see", "attack", "claw" };
	for (unsigned int candidate = 0;
	     candidate < sizeof(candidates) / sizeof(candidates[0]); ++candidate) {
		if (candidate == 2 && !robot->attack_type)
			continue;
		if (candidates[candidate] == 255)
			continue;
		int duplicate = 0;
		for (int existing = 0; existing < sound_count; ++existing)
			duplicate |= sounds[existing] == candidates[candidate];
		if (!duplicate) {
			kinds[sound_count] = candidate_kinds[candidate];
			sounds[sound_count++] = candidates[candidate];
		}
	}
	if (!sound_count)
		return;
	Robot_preview_sound_seed = Robot_preview_sound_seed * 1664525u + 1013904223u;
	const unsigned int selected = Robot_preview_sound_seed % sound_count;
	Robot_preview_last_sound = sounds[selected];
	Robot_preview_last_sound_sample =
	    Robot_preview_last_sound >= 0 && Robot_preview_last_sound < MAX_SOUNDS
	        ? digi_xlat_sound(Robot_preview_last_sound)
	        : -1;
	Robot_preview_last_sound_kind = kinds[selected];
	digi_play_sample(Robot_preview_last_sound, F1_0);
	++Robot_preview_sounds_played;
}

static int robot_preview_window_handler(window *wind, d_event *event, void *data)
{
	(void) data;
	switch (event->type) {
		case EVENT_QUIT:
			window_close(wind);
			return 1;
		case EVENT_WINDOW_DRAW: {
			const int pending = Robot_preview_pending_number.load(std::memory_order_acquire);
			if (pending != Robot_preview_number && !robot_preview_configure_robot(pending))
				Robot_preview_pending_number.store(Robot_preview_number, std::memory_order_release);
			const Uint32 now = SDL_GetTicks();
			const int attack_enabled = Robot_preview_attack_enabled.load(std::memory_order_relaxed);
			if (attack_enabled != Robot_preview_attack_was_enabled) {
				robot_preview_reset_attack_state();
				Robot_preview_attack_was_enabled = attack_enabled;
			}
			if (Robot_preview_sounds_enabled.load(std::memory_order_relaxed) &&
			    now >= Robot_preview_next_sound_at) {
				robot_preview_play_sound();
				Robot_preview_next_sound_at = now + ROBOT_PREVIEW_SOUND_INTERVAL_MS;
			}
			robot_preview_animate();
			if (attack_enabled) {
				++Robot_preview_attack_frames;
				robot_preview_update_movement();
				robot_preview_schedule_fire(now);
				robot_preview_update_projectiles();
			} else {
				Robot_preview_robot_offset_x = 0;
				Robot_preview_robot_offset_y = 0;
				Robot_preview_robot_distance_offset = 0;
			}
			vms_angvec angles;
			angles.h = static_cast<fixang>(
			    attack_enabled
			        ? Robot_preview_heading.load(std::memory_order_relaxed)
			        : Robot_preview_heading.fetch_add(40, std::memory_order_relaxed));
			angles.p = static_cast<fixang>(Robot_preview_pitch.load(std::memory_order_relaxed));
			angles.b = 0;
			draw_model_picture_animated_scene(
			    Robot_preview_model, &angles, Robot_preview_anim_angles,
			    Robot_preview_robot_offset_x, Robot_preview_robot_offset_y,
			    Robot_preview_robot_distance_offset, Robot_preview_view_radius,
			    attack_enabled ? robot_preview_draw_projectiles : NULL, NULL);
			if (attack_enabled)
				robot_preview_draw_attack_overlay();
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
	const bool base_game = request.value("base_game", false);
	Robot_preview_has_level = !base_game;
	Robot_preview_behavior = AIB_NORMAL;
	Robot_preview_behavior_from_level = 0;
	Robot_preview_difficulty = (std::max) (0, (std::min) (NDL - 1, Difficulty_level));
	Robot_preview_close_requested.store(0, std::memory_order_release);
	Robot_preview_heading.store(F0_5 - 1, std::memory_order_release);
	Robot_preview_pitch.store(0, std::memory_order_release);
	Robot_preview_active.store(0, std::memory_order_release);
	Robot_preview_model = -1;
	Robot_preview_number = request.value("robot_number", -1);
	Robot_preview_pending_number.store(Robot_preview_number, std::memory_order_release);
	Robot_preview_count.store(0, std::memory_order_release);
	Robot_preview_frames = 0;
	memset(Robot_preview_anim_angles, 0, sizeof(Robot_preview_anim_angles));
	memset(Robot_preview_anim_source, 0, sizeof(Robot_preview_anim_source));
	memset(Robot_preview_anim_target, 0, sizeof(Robot_preview_anim_target));
	Robot_preview_anim_state.store(AS_REST, std::memory_order_relaxed);
	Robot_preview_anim_frame.store(0, std::memory_order_relaxed);
	Robot_preview_animated_joint_count = 0;
	Robot_preview_motion_updates = 0;
	Robot_preview_anim_cycle_index = 0;
	Robot_preview_sound_seed = started_at ^ static_cast<unsigned int>(Robot_preview_number * 2654435761u);
	Robot_preview_next_sound_at = started_at + ROBOT_PREVIEW_SOUND_INTERVAL_MS;
	Robot_preview_last_sound = -1;
	Robot_preview_last_sound_sample = -1;
	Robot_preview_last_sound_kind = "none";
	Robot_preview_sounds_played = 0;
	Robot_preview_shots_fired = 0;
	Robot_preview_projectile_hits = 0;
	Robot_preview_projectile_misses = 0;
	Robot_preview_projectile_updates = 0;
	Robot_preview_attack_frames = 0;
	Robot_preview_actual_weapon_renders = 0;
	Robot_preview_fallback_weapon_renders = 0;
	Robot_preview_mines_dropped = 0;
	Robot_preview_projectiles_retired_offscreen = 0;
	Robot_preview_homing_reversals = 0;
	Robot_preview_projectile_sequence = 0;
	Robot_preview_last_projectile_render_size = 0;
	Robot_preview_last_projectile_render_scale = F1_0;
	Robot_preview_normal_view_radius = 0;
	Robot_preview_large_view_radius = 0;
	Robot_preview_view_radius = 0;
	Robot_preview_scale_reference_robot = -1;
	Robot_preview_camera_scale = F1_0;
	Robot_preview_camera_tier = "normal";
	Robot_preview_navigation_numbers.clear();
	Robot_preview_attack_was_enabled = Robot_preview_attack_enabled.load(std::memory_order_relaxed);
	robot_preview_reset_attack_state();
	loading.update(base_game ? "Preparing base game" : "Mounting mission files", 5);

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
	const std::string level_file = request.value("level_file", "");
	if (!base_game) {
		if (load_preview_mission(request))
			return 1;
		if (level_file.empty() || !PHYSFSX_exists(level_file.c_str(), 1))
			return preview_fail("Robot preview source level is missing");
		loading.update(level_file.c_str(), 55);
		if (load_level(level_file.c_str()))
			return preview_fail(std::string("Could not load robot preview level ") + level_file);
		Current_level_num = request.value("level_num", 1);
#ifdef DXX_BUILD_DESCENT_II
		load_level_robots_file(level_file.c_str());
#endif
	} else {
		loading.update("Loading base robot", 55);
		Current_level_num = 0;
#ifdef DXX_BUILD_DESCENT_II
		std::strcpy(Current_level_palette, DEFAULT_LEVEL_PALETTE);
#endif
	}
	load_preview_palette();
	Robot_preview_difficulty = (std::max) (0, (std::min) (NDL - 1, Difficulty_level));
	if (Robot_preview_number < 0 || Robot_preview_number >= N_robot_types)
		return preview_fail("Robot number is not available in the selected game data");
	robot_preview_configure_camera_tiers();
	robot_preview_configure_navigation(request);
	if (!robot_preview_configure_robot(Robot_preview_number))
		return preview_fail("Robot model is not available in the selected game data");

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
			debug_log(DLOG_GAME, "robot preview first frame ready in %u ms robot=%d model=%d joints=%d level=%s",
			          (unsigned int) (SDL_GetTicks() - started_at), Robot_preview_number,
			          Robot_preview_model, Robot_preview_animated_joint_count, level_file.c_str());
		}
	}
	Robot_preview_active.store(0, std::memory_order_release);
	Robot_preview_count.store(0, std::memory_order_release);
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
		const int source_width = SWIDTH;
		const int source_height = SHEIGHT;
		const int draw_width = android_surface_get_display_width();
		const int draw_height = android_surface_get_display_height();
		const long long aspect_error =
		    static_cast<long long>(draw_width) * source_height -
		    static_cast<long long>(draw_height) * source_width;
		const bool preserve_aspect =
		    draw_width > 0 && draw_height > 0 &&
		    (aspect_error < 0 ? -aspect_error : aspect_error) <=
		        (source_width > source_height ? source_width : source_height);
		const robot_info *robot = &Robot_info[Robot_preview_number];
		auto preview_sound_info = [](int logical) {
			const int sample = logical >= 0 && logical < MAX_SOUNDS
			                       ? digi_xlat_sound(logical)
			                       : -1;
			return json{
				{ "logical", logical }, { "sample", sample }
			};
		};
		json robot_sounds = {
			{ "see", preview_sound_info(robot->see_sound) },
			{ "attack", preview_sound_info(robot->attack_sound) },
			{ "claw", preview_sound_info(robot->claw_sound) },
			{ "claw_in_random_pool", robot->attack_type != 0 }
		};
#ifdef DXX_BUILD_DESCENT_II
		robot_sounds["taunt"] = preview_sound_info(robot->taunt_sound);
		robot_sounds["taunt_in_random_pool"] = false;
#endif
		const robot_preview_attack_role attack_role = robot_preview_attack_role_for(robot);
		const robot_preview_dps_result dps = robot_preview_calculate_dps(robot);
		const int configured_preview_weapon =
		    attack_role == ROBOT_PREVIEW_ATTACK_RANGED
		        ? robot_preview_weapon_for_gun(robot, 0)
		    : attack_role == ROBOT_PREVIEW_ATTACK_MINE_DROPPER ? robot_preview_mine_weapon()
		                                                       : -1;
		const int preview_weapon = Robot_preview_last_weapon >= 0 ? Robot_preview_last_weapon
		                                                          : configured_preview_weapon;
		json weapon_stats = NULL;
		if (preview_weapon >= 0 && preview_weapon < N_weapon_types) {
			const weapon_info *weapon = &Weapon_info[preview_weapon];
			weapon_stats = {
				{ "number", preview_weapon },
				{ "render_type", weapon->render_type },
				{ "model_number", weapon->model_num },
				{ "vclip_number", weapon->weapon_vclip },
				{ "speed", f2fl(weapon->speed[Robot_preview_difficulty]) },
				{ "thrust", f2fl(weapon->thrust) },
				{ "drag", f2fl(weapon->drag) },
				{ "mass", f2fl(weapon->mass) },
				{ "lifetime", f2fl(weapon->lifetime) },
				{ "homing", weapon->homing_flag != 0 },
				{ "matter", weapon->matter != 0 },
				{ "damage", f2fl(weapon->strength[Robot_preview_difficulty]) },
				{ "damage_radius", f2fl(weapon->damage_radius) }
			};
#ifdef DXX_BUILD_DESCENT_II
			weapon_stats["speed_variance"] = weapon->speedvar;
#endif
		}
		json preview = {
			{ "schema", "dxx-robot-preview-introspection-v1" },
			{ "active", true },
			{ "request_id", Level_preview_request.value("request_id", "") },
			{ "game", Level_preview_request.value("game", "") },
			{ "mission_name", Level_preview_request.value("mission_name", "") },
			{ "level_file", Level_preview_request.value("level_file", "") },
			{ "level_num", Level_preview_request.value("level_num", 0) },
			{ "robot_number", Robot_preview_number },
			{ "robot_count", Robot_preview_count.load(std::memory_order_relaxed) },
			{ "navigation_numbers", Robot_preview_navigation_numbers },
			{ "robot_label", Level_preview_request.value("robot_label", "") },
			{ "model_number", Robot_preview_model },
			{ "model_radius", f2fl(Polygon_models[Robot_preview_model].rad) },
			{ "boss", robot->boss_flag != 0 },
			{ "camera_tier", Robot_preview_camera_tier },
			{ "camera_view_radius", f2fl(Robot_preview_view_radius) },
			{ "scale_reference_robot", Robot_preview_scale_reference_robot },
			{ "scale_reference_radius", f2fl(robot_preview_model_radius(
			                                Robot_preview_scale_reference_robot)) },
			{ "camera_scale", f2fl(Robot_preview_camera_scale) },
			{ "display_radius_ratio", f2fl(fixdiv(
			                              Polygon_models[Robot_preview_model].rad,
			                              Robot_preview_view_radius)) },
			{ "normal_camera_view_radius", f2fl(Robot_preview_normal_view_radius) },
			{ "large_camera_view_radius", f2fl(Robot_preview_large_view_radius) },
			{ "palette_ready", Level_preview_palette_ready != 0 },
			{ "palette_name", Level_preview_palette_name },
			{ "frame_count", Robot_preview_frames },
			{ "animation_state", robot_preview_anim_state_name(
			                         Robot_preview_anim_state.load(std::memory_order_relaxed)) },
			{ "animation_transition_frame", Robot_preview_anim_frame.load(std::memory_order_relaxed) },
			{ "animated_joint_count", Robot_preview_animated_joint_count },
			{ "motion_updates", Robot_preview_motion_updates },
			{ "heading", Robot_preview_heading.load(std::memory_order_relaxed) },
			{ "pitch", Robot_preview_pitch.load(std::memory_order_relaxed) },
			{ "sounds_enabled", Robot_preview_sounds_enabled.load(std::memory_order_relaxed) != 0 },
			{ "sounds_played", Robot_preview_sounds_played },
			{ "last_sound", Robot_preview_last_sound },
			{ "last_sound_sample", Robot_preview_last_sound_sample },
			{ "last_sound_kind", Robot_preview_last_sound_kind },
			{ "robot_sounds", robot_sounds },
			{ "attack_enabled", Robot_preview_attack_enabled.load(std::memory_order_relaxed) != 0 },
			{ "difficulty", Robot_preview_difficulty },
			{ "behavior", robot_preview_behavior_name(Robot_preview_behavior) },
			{ "behavior_number", Robot_preview_behavior },
			{ "behavior_source", Robot_preview_behavior_from_level ? "level object" : "robot type fallback" },
			{ "attack_role", robot_preview_attack_role_name(attack_role) },
			{ "can_fire", attack_role != ROBOT_PREVIEW_ATTACK_NONE },
			{ "attack_type", robot->attack_type },
			{ "gun_count", robot->n_guns },
			{ "configured_weapon", robot->weapon_type },
			{ "max_speed", f2fl(robot->max_speed[Robot_preview_difficulty]) },
			{ "circle_distance", f2fl(robot->circle_distance[Robot_preview_difficulty]) },
			{ "evade_speed", robot->evade_speed[Robot_preview_difficulty] },
			{ "firing_wait", f2fl(robot->firing_wait[Robot_preview_difficulty]) },
			{ "rapidfire_count", robot->rapidfire_count[Robot_preview_difficulty] },
			{ "shots_fired", Robot_preview_shots_fired },
			{ "mines_dropped", Robot_preview_mines_dropped },
			{ "active_mines", robot_preview_active_mines() },
			{ "max_active_mines", ROBOT_PREVIEW_MAX_ACTIVE_MINES },
			{ "projectiles_retired_offscreen", Robot_preview_projectiles_retired_offscreen },
			{ "homing_direction_reversals", Robot_preview_homing_reversals },
			{ "last_projectile_render_size", f2fl(Robot_preview_last_projectile_render_size) },
			{ "last_projectile_render_scale", f2fl(Robot_preview_last_projectile_render_scale) },
			{ "last_projectile_initial_speed", f2fl(Robot_preview_last_projectile_initial_speed) },
			{ "projectile_display_distance", f2fl(ROBOT_PREVIEW_PROJECTILE_DISPLAY_DISTANCE) },
			{ "last_projectile_origin", json::array({ f2fl(Robot_preview_last_projectile_origin.x),
			                                          f2fl(Robot_preview_last_projectile_origin.y),
			                                          f2fl(Robot_preview_last_projectile_origin.z) }) },
			{ "last_projectile_direction", json::array({ f2fl(Robot_preview_last_projectile_direction.x),
			                                             f2fl(Robot_preview_last_projectile_direction.y),
			                                             f2fl(Robot_preview_last_projectile_direction.z) }) },
			{ "projectile_hits", Robot_preview_projectile_hits },
			{ "projectile_misses", Robot_preview_projectile_misses },
			{ "projectile_updates", Robot_preview_projectile_updates },
			{ "attack_frames", Robot_preview_attack_frames },
			{ "actual_weapon_renders", Robot_preview_actual_weapon_renders },
			{ "fallback_weapon_renders", Robot_preview_fallback_weapon_renders },
			{ "active_projectiles", robot_preview_active_projectiles() },
			{ "weapon", weapon_stats },
			{ "dps", robot_preview_dps_json(dps) },
			{ "preserve_aspect", preserve_aspect },
			{ "source_width", source_width },
			{ "source_height", source_height },
			{ "draw_width", draw_width },
			{ "draw_height", draw_height },
			{ "close_requested", Robot_preview_close_requested.load(std::memory_order_relaxed) != 0 },
			{ "uptime_ms", now - Level_preview_started_at }
		};
#ifdef DXX_BUILD_DESCENT_II
		preview["configured_weapon2"] = robot->weapon_type2;
		preview["firing_wait2"] = f2fl(robot->firing_wait2[Robot_preview_difficulty]);
		preview["companion"] = robot->companion != 0;
		preview["thief"] = robot->thief != 0;
		preview["sub_flags"] = Robot_preview_sub_flags;
#endif
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

extern "C" int android_robot_preview_select(int direction)
{
	const int count = Robot_preview_count.load(std::memory_order_acquire);
	if (!Robot_preview_active.load(std::memory_order_acquire) || count <= 0 || direction == 0)
		return -1;
	const int current = Robot_preview_pending_number.load(std::memory_order_relaxed);
	const auto current_position = std::find(
	    Robot_preview_navigation_numbers.begin(), Robot_preview_navigation_numbers.end(), current);
	const int current_index = current_position != Robot_preview_navigation_numbers.end()
	                              ? static_cast<int>(
	                                    current_position - Robot_preview_navigation_numbers.begin())
	                              : 0;
	const int selected_index = (current_index + (direction > 0 ? 1 : count - 1)) % count;
	const int selected = Robot_preview_navigation_numbers[selected_index];
	Robot_preview_pending_number.store(selected, std::memory_order_release);
	return selected;
}

extern "C" void android_robot_preview_set_sounds(int enabled)
{
	Robot_preview_sounds_enabled.store(enabled != 0, std::memory_order_release);
}

extern "C" void android_robot_preview_set_attack(int enabled)
{
	Robot_preview_attack_enabled.store(enabled != 0, std::memory_order_release);
}

extern "C" const char *android_robot_preview_attack_summary(void)
{
	if (!Robot_preview_active.load(std::memory_order_acquire) || Robot_preview_number < 0 ||
	    Robot_preview_number >= N_robot_types)
		return "";
	const robot_info *robot = &Robot_info[Robot_preview_number];
	const robot_preview_attack_role role = robot_preview_attack_role_for(robot);
	const robot_preview_dps_result dps = robot_preview_calculate_dps(robot);
	const int weapon_number = role == ROBOT_PREVIEW_ATTACK_RANGED
	                              ? robot_preview_weapon_for_gun(robot, 0)
	                          : role == ROBOT_PREVIEW_ATTACK_MINE_DROPPER
	                              ? robot_preview_mine_weapon()
	                              : -1;
	char dps_summary[160];
	if (dps.channel_count > 1) {
		snprintf(
		    dps_summary, sizeof(dps_summary), "DPS: %.2f direct (%s %.2f + %s %.2f)",
		    dps.total, dps.primary.kind, dps.primary.dps, dps.secondary.kind,
		    dps.secondary.dps);
	} else if (dps.channel_count == 1) {
		snprintf(
		    dps_summary, sizeof(dps_summary),
		    "DPS: %.2f direct (%.1f damage x %.3f/s)%s", dps.total,
		    dps.primary.damage_per_hit, dps.primary.attacks_per_second,
		    dps.randomized_timing_average ? ", averaged timing" : "");
	} else {
		snprintf(dps_summary, sizeof(dps_summary), "DPS: 0.00 direct");
	}
	char summary[560];
	if (weapon_number >= 0 && weapon_number < N_weapon_types) {
		const weapon_info *weapon = &Weapon_info[weapon_number];
		snprintf(
		    summary, sizeof(summary),
		    "%s\nAI: %s (%s), %s\nMove: %.1f speed, %.1f circle, evade %d\nWeapon %d: %.1f speed, %.2f thrust, %.3f drag, %.1fs%s",
		    dps_summary,
		    robot_preview_behavior_name(Robot_preview_behavior),
		    Robot_preview_behavior_from_level ? "level object" : "type fallback",
		    robot_preview_attack_role_name(role),
		    f2fl(robot->max_speed[Robot_preview_difficulty]),
		    f2fl(robot->circle_distance[Robot_preview_difficulty]),
		    robot->evade_speed[Robot_preview_difficulty], weapon_number,
		    f2fl(weapon->speed[Robot_preview_difficulty]), f2fl(weapon->thrust),
		    f2fl(weapon->drag), f2fl(weapon->lifetime),
		    weapon->homing_flag ? ", homing" : "");
	} else {
		snprintf(
		    summary, sizeof(summary),
		    "%s\nAI: %s (%s), %s\nMove: %.1f speed, %.1f circle, evade %d\nNo ranged weapon",
		    dps_summary,
		    robot_preview_behavior_name(Robot_preview_behavior),
		    Robot_preview_behavior_from_level ? "level object" : "type fallback",
		    robot_preview_attack_role_name(role),
		    f2fl(robot->max_speed[Robot_preview_difficulty]),
		    f2fl(robot->circle_distance[Robot_preview_difficulty]),
		    robot->evade_speed[Robot_preview_difficulty]);
	}
	Robot_preview_attack_summary = summary;
	return Robot_preview_attack_summary.c_str();
}
