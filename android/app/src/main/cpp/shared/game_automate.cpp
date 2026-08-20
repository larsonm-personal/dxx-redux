/*
 * game_automate.cpp -- Automated input scripting for AI-assisted testing.
 *
 * Parses a JSON script of steps (key presses, waits, condition checks)
 * and executes them one per frame, injecting SDL key events to drive
 * the game through menus, briefings, and gameplay automatically.
 *
 * Uses nlohmann/json for parsing.
 * Guarded by INTROSPECT_ON -- only compiled into debug Android builds.
 */

#ifdef INTROSPECT_ON

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

/* Engine headers are pure C -- wrap them for C++ linkage. */
extern "C" {
#include <SDL.h>
}

#ifdef ANDROID
#include <android/log.h>
#include <jni.h>
#define LOG_TAG   "DXX-Automate"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(fmt, ...) printf("[Automate] " fmt "\n", ##__VA_ARGS__)
#define LOGE(fmt, ...) fprintf(stderr, "[Automate] ERROR: " fmt "\n", ##__VA_ARGS__)
#endif

extern "C" {
#include "game_automate.h"
#include "game_introspect.h"
#include "overlay_ringbuf.h"
#include "android_save_meta.h"
#include "android_axis_mailbox.h"
#include "android_screen_advance.h"
#include "android_graphics_options.h"
#include "android_log.h"
#include "android_music_control.h"
#include "android_texture_debug.h"
#include "android_meta_actions.h"
#include "debug_tex_overlay.h"
#include "merged_wall_debug.h"
#include "game.h"
#include "player.h"
#include "screens.h"
#include "inferno.h"
#include "key.h"
#include "playsave.h"
#include "rbaudio.h"
#include "songs.h"
#include "songs_android_shared.h"
#include "state.h"
#include "window.h"
#include "newmenu.h"
#include "gameseg.h"
#include "endlevel.h"
#include "gameseq.h"
#include "kmatrix.h"
#include "multi.h"
#include "object.h"
#include "collide.h"
#include "hudmsg.h"
#include "robot.h"
#include "coop/coop_save.h"
#include "secretarea.h"
#include "switch.h"
#include "wall.h"
#include "cntrlcen.h"
#ifdef DXX_BUILD_DESCENT_II
#include "escort.h"
#endif
#ifdef ANDROID
void android_automation_start_endlevel_sequence(void);
#endif
}

#ifdef ANDROID
extern "C" void android_test_inject_touch_tap(void);
extern "C" void android_test_inject_touch_action(int action);
extern "C" void android_automation_joystick_button(int button, int pressed);
extern "C" int gles3_shim_probe_vbo_arrays(void);
extern "C" int do_game_pause(void);
extern "C" int RBAInvalidateCurrentSourceForTest(void);
extern "C" android_axis_generation android_joystick_axis_publish(
    int axis, int raw_value, int touch_source);
extern "C" JavaVM *g_jvm;
extern "C" jobject g_activity;

static bool automation_select_radial(const char *menu_id, const char *text)
{
	JNIEnv *env;
	int attached = 0;

	if (!g_jvm || !g_activity || !menu_id || !text)
		return false;
	if (g_jvm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
		if (g_jvm->AttachCurrentThread(&env, NULL) != JNI_OK)
			return false;
		attached = 1;
	}
	jclass cls = env->GetObjectClass(g_activity);
	jmethodID method = cls ? env->GetMethodID(
	                             cls, "automateRadialSelection",
	                             "(Ljava/lang/String;Ljava/lang/String;)Z")
	                       : NULL;
	jstring jmenu = method ? env->NewStringUTF(menu_id) : NULL;
	jstring jtext = method ? env->NewStringUTF(text) : NULL;
	bool selected = jmenu && jtext && env->CallBooleanMethod(g_activity, method, jmenu, jtext);
	if (env->ExceptionCheck()) {
		env->ExceptionClear();
		selected = false;
	}
	if (jtext) env->DeleteLocalRef(jtext);
	if (jmenu) env->DeleteLocalRef(jmenu);
	if (cls) env->DeleteLocalRef(cls);
	if (attached) g_jvm->DetachCurrentThread();
	return selected;
}

static void automation_enter_launcher(void)
{
	const int slotnum = ANDROID_SAVE_META_SLOT_AUTO_EXIT;
	int saved = 0;
	SDL_Event ev;

	if (Player_is_dead)
		debug_log(DLOG_GAME, "autosave skipped: enter_launcher player is dead");
	else if (Screen_mode != SCREEN_GAME || Game_wind == NULL || Current_level_num <= 0)
		debug_log(DLOG_GAME, "autosave skipped: enter_launcher not in active level");
	else
		saved = state_android_save_to_slot(slotnum, "AUTO EXIT",
		                                   ANDROID_SAVE_META_KIND_AUTO_EXIT);
	if (saved)
		debug_log(DLOG_GAME, "autosave saved: enter_launcher slot %d", slotnum);

	android_force_quit = 1;
	memset(&ev, 0, sizeof(ev));
	ev.type = SDL_QUIT;
	SDL_PushEvent(&ev);
	debug_log(DLOG_GAME, "autosave exit queued: enter_launcher slot %d", slotnum);
}
#endif

/* D1 does not have SCREEN_MOVIE */
#ifndef SCREEN_MOVIE
#define SCREEN_MOVIE 99
#endif

/* Automap_active is defined in automap.c; we just need the extern. */
extern "C" int Automap_active;
extern "C" unsigned char Automap_visited[];
extern "C" int Current_level_num;
extern "C" volatile int g_intro_active;

/* -- Key name -> SDLKey mapping ---------------------------------------- */

struct key_entry {
	const char *name;
	SDLKey sym;
};

struct key_command_entry {
	const char *name;
	int keycode;
};

static const key_entry key_map[] = {
	/* Navigation */
	{ "enter", SDLK_RETURN },
	{ "return", SDLK_RETURN },
	{ "escape", SDLK_ESCAPE },
	{ "esc", SDLK_ESCAPE },
	{ "tab", SDLK_TAB },
	{ "space", SDLK_SPACE },
	{ "backspace", SDLK_BACKSPACE },
	{ "delete", SDLK_DELETE },

	/* Arrows */
	{ "up", SDLK_UP },
	{ "down", SDLK_DOWN },
	{ "left", SDLK_LEFT },
	{ "right", SDLK_RIGHT },

	/* Letters */
	{ "a", SDLK_a },
	{ "b", SDLK_b },
	{ "c", SDLK_c },
	{ "d", SDLK_d },
	{ "e", SDLK_e },
	{ "f", SDLK_f },
	{ "g", SDLK_g },
	{ "h", SDLK_h },
	{ "i", SDLK_i },
	{ "j", SDLK_j },
	{ "k", SDLK_k },
	{ "l", SDLK_l },
	{ "m", SDLK_m },
	{ "n", SDLK_n },
	{ "o", SDLK_o },
	{ "p", SDLK_p },
	{ "q", SDLK_q },
	{ "r", SDLK_r },
	{ "s", SDLK_s },
	{ "t", SDLK_t },
	{ "u", SDLK_u },
	{ "v", SDLK_v },
	{ "w", SDLK_w },
	{ "x", SDLK_x },
	{ "y", SDLK_y },
	{ "z", SDLK_z },

	/* Digits */
	{ "0", SDLK_0 },
	{ "1", SDLK_1 },
	{ "2", SDLK_2 },
	{ "3", SDLK_3 },
	{ "4", SDLK_4 },
	{ "5", SDLK_5 },
	{ "6", SDLK_6 },
	{ "7", SDLK_7 },
	{ "8", SDLK_8 },
	{ "9", SDLK_9 },

	/* Function keys */
	{ "f1", SDLK_F1 },
	{ "f2", SDLK_F2 },
	{ "f3", SDLK_F3 },
	{ "f4", SDLK_F4 },
	{ "f5", SDLK_F5 },
	{ "f6", SDLK_F6 },
	{ "f7", SDLK_F7 },
	{ "f8", SDLK_F8 },
	{ "f9", SDLK_F9 },
	{ "f10", SDLK_F10 },
	{ "f11", SDLK_F11 },
	{ "f12", SDLK_F12 },

	/* Page / Home */
	{ "home", SDLK_HOME },
	{ "end", SDLK_END },
	{ "pageup", SDLK_PAGEUP },
	{ "pagedown", SDLK_PAGEDOWN },

	/* Modifiers */
	{ "lshift", SDLK_LSHIFT },
	{ "rshift", SDLK_RSHIFT },
	{ "lctrl", SDLK_LCTRL },
	{ "rctrl", SDLK_RCTRL },
	{ "lalt", SDLK_LALT },
	{ "ralt", SDLK_RALT },

	/* Punctuation */
	{ "minus", SDLK_MINUS },
	{ "equals", SDLK_EQUALS },
	{ "comma", SDLK_COMMA },
	{ "period", SDLK_PERIOD },
	{ "slash", SDLK_SLASH },
	{ "semicolon", SDLK_SEMICOLON },

	{ NULL, SDLK_UNKNOWN }
};

static SDLKey lookup_key(const char *name)
{
	for (const key_entry *k = key_map; k->name; k++) {
		if (strcasecmp(k->name, name) == 0)
			return k->sym;
	}
	return SDLK_UNKNOWN;
}

static const key_command_entry key_command_map[] = {
	{ "enter", KEY_ENTER },
	{ "return", KEY_ENTER },
	{ "escape", KEY_ESC },
	{ "esc", KEY_ESC },
	{ "tab", KEY_TAB },
	{ "space", KEY_SPACEBAR },
	{ "backspace", KEY_BACKSP },
	{ "delete", KEY_DELETE },
	{ "up", KEY_UP },
	{ "down", KEY_DOWN },
	{ "left", KEY_LEFT },
	{ "right", KEY_RIGHT },
	{ "f1", KEY_F1 },
	{ "f2", KEY_F2 },
	{ "f3", KEY_F3 },
	{ "f4", KEY_F4 },
	{ "f5", KEY_F5 },
	{ "f6", KEY_F6 },
	{ "f7", KEY_F7 },
	{ "f8", KEY_F8 },
	{ "f9", KEY_F9 },
	{ "f10", KEY_F10 },
	{ "f11", KEY_F11 },
	{ "f12", KEY_F12 },
	{ NULL, -1 }
};

static int lookup_key_command(const char *name)
{
	for (const key_command_entry *k = key_command_map; k->name; k++) {
		if (strcasecmp(k->name, name) == 0)
			return k->keycode;
	}
	return -1;
}

static int objective_mode_from_name(const char *name)
{
	if (strcasecmp(name, "off") == 0)
		return LEVEL_METADATA_OBJECTIVES_OFF;
	if (strcasecmp(name, "all") == 0)
		return LEVEL_METADATA_OBJECTIVES_ALL;
	if (strcasecmp(name, "remaining") == 0)
		return LEVEL_METADATA_OBJECTIVES_REMAINING;
	if (strcasecmp(name, "next") == 0)
		return LEVEL_METADATA_OBJECTIVES_NEXT;
	return -1;
}

/* -- Step types ------------------------------------------------------- */

enum step_type {
	STEP_KEY,                        /* inject key down+up, then delay */
	STEP_WAIT_MS,                    /* wait N milliseconds */
	STEP_WAIT_FOR,                   /* wait for a game state condition */
	STEP_INTROSPECT,                 /* trigger introspection dump */
	STEP_LOG,                        /* emit a logcat message */
	STEP_ASSERT,                     /* check introspection values, fail if mismatch */
	STEP_SELECT,                     /* find menu item by text and select it */
	STEP_SEND_AXIS,                  /* inject joystick axis event */
	STEP_SEND_BUTTON,                /* inject joystick button press+release */
	STEP_SEND_TOUCH_TAP,             /* inject a touch tap through android_input.c */
	STEP_SEND_TOUCH_DOWN,            /* inject a touch down through android_input.c */
	STEP_SEND_TOUCH_UP,              /* inject a touch up through android_input.c */
	STEP_REQUEST_SCREEN_ADVANCE,     /* submit the current or previous native generation */
	STEP_META_ACTION,                /* dispatch a native Android meta action */
	STEP_SELECT_RADIAL,              /* select a Kotlin radial-menu slice by text */
	STEP_SKIP_INTRO,                 /* repeatedly dismiss launch intro with touch or button */
	STEP_SKIP_BRIEFING,              /* escape only if a non-game window covers Game_wind */
	STEP_ASSERT_OVERLAY,             /* check overlay ring buffer for matching entry */
	STEP_FACE_VIEW,                  /* move player inside a segment and face a wall */
	STEP_FACE_FIRST_MERGED,          /* move player to the first merged face on the level */
	STEP_POSE_VIEW,                  /* move player to an exact position and orientation */
	STEP_POSE_ROUTE_GUIDANCE,        /* move player to the active route pose and aim point */
	STEP_PROBE_CROSSHAIR,            /* request merged-wall crosshair probe and wait */
	STEP_ASSERT_PROBE_MATCH,         /* compare two stored probe results */
	STEP_PROBE_GLES_VBO,             /* exercise zero and nonzero VBO array offsets */
	STEP_ENTER_LAUNCHER,             /* yield back to launcher, write LAUNCHER_CONTINUE */
	STEP_ENTER_GAME,                 /* launcher-only: no-op in game engine (skip) */
	STEP_SETUP_COMMAND,              /* launcher-only: no-op in game engine (skip) */
	STEP_RESET_STATE,                /* launcher-only: no-op in game engine (skip) */
	STEP_CLEAR_MODS,                 /* launcher-only: no-op in game engine (skip) */
	STEP_IMPORT_MISSION_ZIP,         /* launcher-only: no-op in game engine (skip) */
	STEP_ANALYZE_LEVEL_METADATA,     /* launcher-only: no-op in game engine (skip) */
	STEP_ANALYZE_LEVEL_METADATA_ALL, /* launcher-only: no-op in game engine (skip) */
	STEP_TRIGGER_ENDLEVEL,           /* call start_endlevel_sequence() */
	STEP_TRIGGER_LEVELCOMPLETE,      /* call PlayerFinishedLevel(0) directly */
	STEP_TRIGGER_POSTLEVEL,          /* open the multiplayer post-level summary */
	STEP_MUSIC_CONTROL,              /* invoke shared Android track controls */
	STEP_REDBOOK_INVALIDATE_SOURCE,  /* automation-only source I/O failure injection */
	STEP_WRITE_CONFIG,               /* launcher-only: no-op in game engine (skip) */
	STEP_TAP_BUTTON,                 /* launcher-only: no-op in game engine (skip) */
	STEP_ASSERT_BUTTON,              /* launcher-only: no-op in game engine (skip) */
	STEP_ASSERT_CONTROLLER_MATCH,    /* launcher-only: no-op in game engine (skip) */
	STEP_ASSERT_MISSION_LIST_HAS_NON_BASE,
	STEP_SELECT_MISSION,       /* select mission if mission picker is present */
	STEP_SET_DEBUG,            /* set a debug flag (e.g. tex_overlay) */
	STEP_SET_SECRET_REVEAL,    /* automation-only: set automap secret reveal */
	STEP_SET_OBJECTIVE_OVERLAY /* automation-only: set objective display mode */
};

/* Key-value pair for STEP_ASSERT expectations.
 * Simple equality: {"key": "3"} or {"key": 3}
 * Comparison ops:  {"key": {"ne": 0}}, {"key": {"gt": 100}},
 *                  {"key": {"range": [10, 1000]}},
 *                  {"key": {"contains": "substring"}},
 *                  {"array_key": {"contains_object": {"field": "value"}}} */
struct assert_expect {
	std::string key;
	std::string value;     /* for simple equality (op=="eq") */
	std::string op = "eq"; /* eq, ne, gt, lt, gte, lte, range, contains, contains_object */
	double num_value = 0;  /* for gt/lt/gte/lte/ne */
	double range_min = 0;
	double range_max = 0;
	json object_value;
};

struct auto_step {
	step_type type = STEP_KEY;
	std::string key_name;               /* STEP_KEY: key name */
	std::string modifier_name;          /* STEP_KEY: optional modifier (e.g. "lshift") */
	int post_delay_ms = 300;            /* STEP_KEY / STEP_SELECT: post-action delay */
	std::string field;                  /* STEP_WAIT_FOR: field name */
	std::string value;                  /* STEP_WAIT_FOR: expected value */
	int timeout_ms = 0;                 /* STEP_WAIT_FOR: timeout (0 = infinite) */
	std::string message;                /* STEP_LOG: message text */
	std::string label;                  /* STEP_PROBE_CROSSHAIR: log label */
	std::vector<assert_expect> expects; /* STEP_ASSERT: expected values */
	std::string select_text;            /* STEP_SELECT: partial text to match */
	std::string radial_menu;            /* STEP_SELECT_RADIAL: radial-menu ID */
	bool select_non_base_mission = false;
	bool select_mission = false;
	bool optional = false;                 /* STEP_SELECT: skip instead of fail on timeout */
	int axis_id = -1;                      /* STEP_SEND_AXIS: axis number (0-5) */
	float axis_value = 0.0f;               /* STEP_SEND_AXIS: value (-1.0 to 1.0) */
	bool axis_touch_source = false;        /* STEP_SEND_AXIS: mark as touch/virtual source */
	bool axis_saturate_sdl_queue = false;  /* STEP_SEND_AXIS: deterministic queue pressure */
	bool axis_use_production_path = false; /* STEP_SEND_AXIS: use production mailbox */
	bool axis_pulse_before_drain = false;  /* STEP_SEND_AXIS: publish value then zero */
	int button_id = -1;                    /* STEP_SEND_BUTTON: button index */
	int button_held = 0;                   /* STEP_SEND_BUTTON: 1 = hold (no release) */
	int button_pressed = 1;                /* STEP_SEND_BUTTON: 0 = release only */
	int meta_action_id = -1;               /* STEP_META_ACTION: action ID */
	std::string music_operation;           /* STEP_MUSIC_CONTROL operation */
	std::string music_source;              /* STEP_MUSIC_CONTROL: source name */
	int music_track = -1;                  /* STEP_MUSIC_CONTROL: index for play */
	int music_last_track = -1;             /* STEP_MUSIC_CONTROL: range end */
	bool music_expect_success = true;      /* STEP_MUSIC_CONTROL: expected result */
	int segment = -1;                      /* STEP_FACE_VIEW: target segment */
	int side = -1;                         /* STEP_FACE_VIEW: target side */
	int face = 0;                          /* STEP_FACE_VIEW: target face on side */
	float distance = 4.0f;                 /* STEP_FACE_VIEW: distance inside segment */
	float pos_x = 0.0f;                    /* STEP_POSE_VIEW: target X position */
	float pos_y = 0.0f;                    /* STEP_POSE_VIEW: target Y position */
	float pos_z = 0.0f;                    /* STEP_POSE_VIEW: target Z position */
	int pitch = 0;                         /* STEP_POSE_VIEW: exact pitch */
	int bank = 0;                          /* STEP_POSE_VIEW: exact bank */
	int heading = 0;                       /* STEP_POSE_VIEW: exact heading */
	int request_frame = -1;                /* STEP_PROBE_CROSSHAIR: request frame */
	bool enabled = false;                  /* STEP_SET_SECRET_REVEAL */
	std::string match_label_a;             /* STEP_ASSERT_PROBE_MATCH: first label */
	std::string match_label_b;             /* STEP_ASSERT_PROBE_MATCH: second label */
	float hot_xy_tolerance = 0.02f;        /* STEP_ASSERT_PROBE_MATCH: max L-inf distance */
	int require_hash_match = 0;            /* STEP_ASSERT_PROBE_MATCH: also require render_hash equal */
	float max_mean_luma_diff = 0.0f;       /* STEP_ASSERT_PROBE_MATCH: 0 disables SAD check */
};

/* -- Script state ----------------------------------------------------- */

static std::vector<auto_step> g_steps;
static int g_current_step = 0;
static int g_active = 0;
static int g_failed = 0;        /* set to 1 on assert/timeout failure */
static Uint32 g_step_start = 0; /* SDL_GetTicks() when step began */
static int g_key_phase = 0;     /* 0=not sent, 1=sent */
static Uint32 g_repeat_start = 0;
static window *g_skip_briefing_last_front = NULL;
static int g_skip_briefing_last_game_wind_seen = -1;

/* -- STEP_SELECT state (multi-frame navigation) ----------------------- */
static int g_select_phase = 0; /* 0=init, 1=navigating, 2=enter sent */
static int g_select_delta = 0; /* remaining navigation steps (+down, -up) */
static int g_held_axis_active[8] = { 0 };
static float g_held_axis_value[8] = { 0.0f };
static int g_held_axis_touch_source[8] = { 0 };
static game_automate_axis_probe g_axis_probe = {};
static int g_axis_dispatch_active = 0;

struct automation_load_request {
	char script_path[512] = "";
	char run_id[80] = "";
	int start_step = 0;
};

static char g_automate_dir[512] = "";
static char g_active_script[512] = "";
static char g_active_run_id[80] = "";
static automation_load_request g_pending_load;
static std::mutex g_pending_load_mutex;
static std::atomic<bool> g_load_requested{ false };
static Uint32 g_script_start = 0; /* SDL_GetTicks() when script began */
static FILE *g_log_fp = NULL;     /* automation_log.jsonl file handle */
static int g_log_seq = 0;         /* monotonic sequence for log lines */

/* -- Probe result store for assert_probe_match ------------------------ */
struct stored_probe {
	std::string label;
	int valid = 0;
	int render_sample_valid = 0;
	float hot_x = 0.0f;
	float hot_y = 0.0f;
	unsigned int render_hash = 0;
	int orient = -1;
	int seg = -1;
	int side = -1;
	int face = -1;
	unsigned char luma[MERGED_WALL_PROBE_RENDER_SAMPLE_COUNT] = {};
	unsigned char mask[MERGED_WALL_PROBE_RENDER_SAMPLE_COUNT] = {};
};
static std::vector<stored_probe> g_stored_probes;

static const stored_probe *find_stored_probe(const std::string &label)
{
	for (const auto &sp : g_stored_probes) {
		if (sp.label == label)
			return &sp;
	}
	return NULL;
}

static void store_probe_result(const std::string &label)
{
	if (label.empty())
		return;
	stored_probe sp;
	sp.label = label;
	sp.valid = 1;
	sp.render_sample_valid = g_merged_wall_probe_result.render_sample_valid;
	sp.hot_x = g_merged_wall_probe_result.render_hot_x;
	sp.hot_y = g_merged_wall_probe_result.render_hot_y;
	sp.render_hash = g_merged_wall_probe_result.render_hash;
	sp.orient = g_merged_wall_probe_result.orient;
	sp.seg = g_merged_wall_probe_result.seg;
	sp.side = g_merged_wall_probe_result.side;
	sp.face = g_merged_wall_probe_result.face;
	memcpy(sp.luma, g_merged_wall_probe_result.render_sample_luma, sizeof(sp.luma));
	memcpy(sp.mask, g_merged_wall_probe_result.render_sample_mask, sizeof(sp.mask));
	for (auto &existing : g_stored_probes) {
		if (existing.label == label) {
			existing = sp;
			return;
		}
	}
	g_stored_probes.push_back(sp);
}

static const char *step_type_name(step_type t)
{
	switch (t) {
		case STEP_KEY: return "key";
		case STEP_WAIT_MS: return "wait_ms";
		case STEP_WAIT_FOR: return "wait_for";
		case STEP_INTROSPECT: return "introspect";
		case STEP_LOG: return "log";
		case STEP_ASSERT: return "assert";
		case STEP_SELECT: return "select";
		case STEP_SEND_AXIS: return "send_axis";
		case STEP_SEND_BUTTON: return "send_button";
		case STEP_SEND_TOUCH_TAP: return "send_touch_tap";
		case STEP_SEND_TOUCH_DOWN: return "send_touch_down";
		case STEP_SEND_TOUCH_UP: return "send_touch_up";
		case STEP_REQUEST_SCREEN_ADVANCE: return "request_screen_advance";
		case STEP_META_ACTION: return "meta_action";
		case STEP_SELECT_RADIAL: return "select_radial";
		case STEP_SKIP_INTRO: return "skip_intro";
		case STEP_SKIP_BRIEFING: return "skip_briefing";
		case STEP_ASSERT_OVERLAY: return "assert_overlay";
		case STEP_FACE_VIEW: return "face_view";
		case STEP_FACE_FIRST_MERGED: return "face_first_merged";
		case STEP_POSE_VIEW: return "pose_view";
		case STEP_POSE_ROUTE_GUIDANCE: return "pose_route_guidance";
		case STEP_PROBE_CROSSHAIR: return "probe_crosshair";
		case STEP_ASSERT_PROBE_MATCH: return "assert_probe_match";
		case STEP_PROBE_GLES_VBO: return "probe_gles_vbo";
		case STEP_ENTER_LAUNCHER: return "enter_launcher";
		case STEP_ENTER_GAME: return "enter_game";
		case STEP_SETUP_COMMAND: return "setup_command";
		case STEP_RESET_STATE: return "reset_state";
		case STEP_CLEAR_MODS: return "clear_mods";
		case STEP_IMPORT_MISSION_ZIP: return "import_mission_zip";
		case STEP_ANALYZE_LEVEL_METADATA: return "analyze_level_metadata";
		case STEP_ANALYZE_LEVEL_METADATA_ALL: return "analyze_level_metadata_all";
		case STEP_TRIGGER_ENDLEVEL: return "trigger_endlevel";
		case STEP_TRIGGER_LEVELCOMPLETE: return "trigger_levelcomplete";
		case STEP_TRIGGER_POSTLEVEL: return "trigger_postlevel";
		case STEP_MUSIC_CONTROL: return "music_control";
		case STEP_REDBOOK_INVALIDATE_SOURCE: return "redbook_invalidate_source";
		case STEP_WRITE_CONFIG: return "write_config";
		case STEP_TAP_BUTTON: return "tap_button";
		case STEP_ASSERT_BUTTON: return "assert_button";
		case STEP_ASSERT_CONTROLLER_MATCH: return "assert_controller_match";
		case STEP_ASSERT_MISSION_LIST_HAS_NON_BASE: return "assert_mission_list_has_non_base";
		case STEP_SELECT_MISSION: return "select_mission";
		case STEP_SET_DEBUG: return "set_debug";
		case STEP_SET_SECRET_REVEAL: return "set_secret_reveal";
		case STEP_SET_OBJECTIVE_OVERLAY: return "set_objective_overlay";
		default: return "unknown";
	}
}

/* -- File-based result/log writing ------------------------------------ */

static FILE *begin_result_file(char *path, size_t path_size, char *temp_path, size_t temp_path_size)
{
	snprintf(path, path_size, "%s/automation_result.json", g_automate_dir);
	snprintf(temp_path, temp_path_size, "%s/automation_result.json.tmp", g_automate_dir);
	return fopen(temp_path, "w");
}

static void publish_result_file(FILE *f, const char *temp_path, const char *path)
{
	fflush(f);
	fclose(f);
	if (rename(temp_path, path) != 0)
		remove(temp_path);
}

static void write_result_file(const char *result, const char *reason)
{
	if (!g_automate_dir[0])
		return;

	char path[1024], temp_path[1024];

	Uint32 elapsed = SDL_GetTicks() - g_script_start;
	const int total_steps = (int) g_steps.size();
	int steps_completed = strcmp(result, "PASS") == 0 ? g_current_step : g_current_step + 1;
	if (steps_completed < 0)
		steps_completed = 0;
	if (steps_completed > total_steps)
		steps_completed = total_steps;

	FILE *f = begin_result_file(path, sizeof(path), temp_path, sizeof(temp_path));
	if (!f)
		return;

	if (reason && reason[0]) {
		/* Minimal JSON escape for the reason string */
		char escaped[512];
		int ei = 0;
		for (const char *p = reason; *p && ei < (int) sizeof(escaped) - 6; p++) {
			if (*p == '"' || *p == '\\') {
				escaped[ei++] = '\\';
				escaped[ei++] = *p;
			} else if (*p == '\n') {
				escaped[ei++] = '\\';
				escaped[ei++] = 'n';
			} else if ((unsigned char) *p >= 0x20) {
				escaped[ei++] = *p;
			}
		}
		escaped[ei] = '\0';

		fprintf(f,
		        "{\"result\":\"%s\",\"run_id\":\"%s\",\"steps_completed\":%d,"
		        "\"total_steps\":%d,\"reason\":\"%s\","
		        "\"elapsed_ms\":%u}\n",
		        result, g_active_run_id, steps_completed, total_steps,
		        escaped, (unsigned) elapsed);
	} else {
		fprintf(f,
		        "{\"result\":\"%s\",\"run_id\":\"%s\",\"steps_completed\":%d,"
		        "\"total_steps\":%d,\"elapsed_ms\":%u}\n",
		        result, g_active_run_id, steps_completed, total_steps,
		        (unsigned) elapsed);
	}

	publish_result_file(f, temp_path, path);
}

static void write_result_file_continue(int next_step)
{
	if (!g_automate_dir[0])
		return;

	char path[1024], temp_path[1024];

	Uint32 elapsed = SDL_GetTicks() - g_script_start;

	FILE *f = begin_result_file(path, sizeof(path), temp_path, sizeof(temp_path));
	if (!f)
		return;

	fprintf(f,
	        "{\"result\":\"LAUNCHER_CONTINUE\",\"run_id\":\"%s\",\"next_step\":%d,"
	        "\"steps_completed\":%d,\"total_steps\":%d,"
	        "\"elapsed_ms\":%u,\"script_path\":\"%s\"}\n",
	        g_active_run_id, next_step, g_current_step + 1, (int) g_steps.size(),
	        (unsigned) elapsed, g_active_script);
	publish_result_file(f, temp_path, path);
}

static void log_append(const char *action, const char *status, const char *detail)
{
	if (!g_log_fp)
		return;

	Uint32 elapsed = SDL_GetTicks() - g_script_start;

	fprintf(g_log_fp,
	        "{\"seq\":%d,\"step\":%d,\"total\":%d,"
	        "\"action\":\"%s\",\"status\":\"%s\","
	        "\"elapsed_ms\":%u,\"detail\":\"%s\"}\n",
	        g_log_seq++, g_current_step + 1, (int) g_steps.size(),
	        action ? action : "", status ? status : "",
	        (unsigned) elapsed, detail ? detail : "");
	fflush(g_log_fp);
}

static void open_log_file(void)
{
	if (g_log_fp) {
		fclose(g_log_fp);
		g_log_fp = NULL;
	}
	g_log_seq = 0;

	if (!g_automate_dir[0])
		return;

	char path[1024];
	snprintf(path, sizeof(path), "%s/automation_log.jsonl", g_automate_dir);
	g_log_fp = fopen(path, "w"); /* truncate */
}

static void remove_stale_result(void)
{
	if (!g_automate_dir[0])
		return;

	char path[1024];
	snprintf(path, sizeof(path), "%s/automation_result.json", g_automate_dir);
	remove(path);
	snprintf(path, sizeof(path), "%s/automation_result.json.tmp", g_automate_dir);
	remove(path);
}

/* -- Key injection ---------------------------------------------------- */

static void inject_key(SDLKey sym, int down)
{
	SDL_Event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	ev.key.state = down ? SDL_PRESSED : SDL_RELEASED;
	ev.key.keysym.sym = sym;
	ev.key.keysym.mod = KMOD_NONE;
	/* Set unicode for printable chars so key_ascii() works (needed for text input fields) */
	ev.key.keysym.unicode = (down && sym >= 32 && sym < 127) ? (Uint16) sym : 0;
	SDL_PushEvent(&ev);
}

static void inject_key_tap(const std::string &name)
{
	SDLKey sym = lookup_key(name.c_str());
	if (sym == SDLK_UNKNOWN) {
		LOGE("Unknown key name: '%s'", name.c_str());
		return;
	}
	LOGI("Injecting key: %s (SDLK %d)", name.c_str(), (int) sym);
	inject_key(sym, 1); /* down */
	inject_key(sym, 0); /* up */
}

static void inject_key_combo(const std::string &modifier, const std::string &key)
{
	SDLKey mod_sym = lookup_key(modifier.c_str());
	SDLKey key_sym = lookup_key(key.c_str());
	if (mod_sym == SDLK_UNKNOWN) {
		LOGE("Unknown modifier name: '%s'", modifier.c_str());
		return;
	}
	if (key_sym == SDLK_UNKNOWN) {
		LOGE("Unknown key name: '%s'", key.c_str());
		return;
	}
	LOGI("Injecting combo: %s + %s", modifier.c_str(), key.c_str());
	inject_key(mod_sym, 1); /* modifier down */
	inject_key(key_sym, 1); /* key down */
	inject_key(key_sym, 0); /* key up */
	inject_key(mod_sym, 0); /* modifier up */
}

/* -- Axis injection --------------------------------------------------- */

static int g_inject_axis_logged; /* suppress repeated logs for re-injection */

static void inject_axis(int axis, float value, bool touch_source,
                        bool saturate_sdl_queue, bool use_production_path,
                        bool pulse_before_drain)
{
	/* Clamp to SDL range: -32768..32767 */
	int ival = (int) (value * 32767.0f);
	if (ival > 32767) ival = 32767;
	if (ival < -32768) ival = -32768;
	int queue_fill_count = 0;
	int queue_saturated = 0;
	if (saturate_sdl_queue) {
		SDL_Event queue_event;
		memset(&queue_event, 0, sizeof(queue_event));
		queue_event.type = SDL_USEREVENT;
		while (queue_fill_count < 4096) {
			if (SDL_PushEvent(&queue_event) < 0) {
				queue_saturated = 1;
				break;
			}
			++queue_fill_count;
		}
	}

#ifdef ANDROID
	int raw_values[ANDROID_AXIS_MAILBOX_AXIS_COUNT] = {};
	unsigned char touch_sources[ANDROID_AXIS_MAILBOX_AXIS_COUNT] = {};
	for (int held_axis = 0; held_axis < 8; ++held_axis) {
		if (!g_held_axis_active[held_axis])
			continue;
		int held_raw = (int) (g_held_axis_value[held_axis] * 32767.0f);
		if (held_raw > 32767) held_raw = 32767;
		if (held_raw < -32768) held_raw = -32768;
		raw_values[held_axis] = held_raw;
		touch_sources[held_axis] = g_held_axis_touch_source[held_axis] != 0;
	}

	if (use_production_path) {
		android_axis_mailbox_release_automation();
		g_axis_probe.first_generation = android_joystick_axis_publish(
		    axis, ival, touch_source ? 1 : 0);
		g_axis_probe.generation = g_axis_probe.first_generation;
		if (pulse_before_drain)
			g_axis_probe.generation = android_joystick_axis_publish(
			    axis, 0, touch_source ? 1 : 0);
	} else {
		g_axis_probe.generation = android_axis_mailbox_publish_automation(
		    raw_values, touch_sources, axis, ival, touch_source ? 1 : 0);
		g_axis_probe.first_generation = g_axis_probe.generation;
	}
	g_axis_probe.valid = 0;
	g_axis_probe.axis = axis;
	g_axis_probe.raw_value = pulse_before_drain ? 0 : ival;
	g_axis_probe.touch_source = touch_source ? 1 : 0;
	g_axis_probe.processed = 0;
	g_axis_probe.queue_fill_count = queue_fill_count;
	g_axis_probe.queue_saturated = queue_saturated;
	g_axis_probe.sample_count = 0;
	g_axis_probe.axis_button_down_edges = 0;
	g_axis_probe.axis_button_up_edges = 0;
	g_axis_probe.pitch_time = 0;
	g_axis_probe.heading_time = 0;
	g_axis_probe.slide_lr_time = 0;
	g_axis_probe.slide_ud_time = 0;
	g_axis_probe.bank_time = 0;
	g_axis_probe.throttle_time = 0;
#else
	SDL_Event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = SDL_JOYAXISMOTION;
	ev.jaxis.which = 0;
	ev.jaxis.axis = (Uint8) axis;
	ev.jaxis.value = (Sint16) ival;
	SDL_PushEvent(&ev);
#endif
	if (!g_inject_axis_logged) {
		LOGI("Injecting axis %d = %.3f (raw %d, touch=%d)", axis, value, ival, touch_source ? 1 : 0);
		g_inject_axis_logged = 1;
	}
}

static void clear_held_axes(void)
{
	int axis;

#ifdef ANDROID
	android_axis_mailbox_release_automation();
#endif
	for (axis = 0; axis < 8; ++axis) {
		g_held_axis_active[axis] = 0;
		g_held_axis_value[axis] = 0.0f;
		g_held_axis_touch_source[axis] = 0;
	}
}

static void set_held_axis(int axis, float value, bool touch_source)
{
	if (axis < 0 || axis >= 8)
		return;

	if (value > -0.0001f && value < 0.0001f) {
		g_held_axis_active[axis] = 0;
		g_held_axis_value[axis] = 0.0f;
		g_held_axis_touch_source[axis] = 0;
	} else {
		g_held_axis_active[axis] = 1;
		g_held_axis_value[axis] = value;
		g_held_axis_touch_source[axis] = touch_source ? 1 : 0;
	}
}

/* -- Button injection ------------------------------------------------- */

static void inject_button(int button, int pressed)
{
#ifdef ANDROID
	android_automation_joystick_button(button, pressed);
#else
	SDL_Event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = pressed ? SDL_JOYBUTTONDOWN : SDL_JOYBUTTONUP;
	ev.jbutton.which = 0;
	ev.jbutton.button = (Uint8) button;
	ev.jbutton.state = pressed ? SDL_PRESSED : SDL_RELEASED;
	SDL_PushEvent(&ev);
#endif
	LOGI("Injecting button %d %s", button, pressed ? "DOWN" : "UP");
}

static void inject_mouse_tap(void)
{
	SDL_Event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = SDL_MOUSEBUTTONDOWN;
	ev.button.button = SDL_BUTTON_LEFT;
	ev.button.state = SDL_PRESSED;
	ev.button.x = 1;
	ev.button.y = 1;
	SDL_PushEvent(&ev);

	memset(&ev, 0, sizeof(ev));
	ev.type = SDL_MOUSEBUTTONUP;
	ev.button.button = SDL_BUTTON_LEFT;
	ev.button.state = SDL_RELEASED;
	ev.button.x = 1;
	ev.button.y = 1;
	SDL_PushEvent(&ev);
	LOGI("Injecting mouse tap");
}

/* -- Menu item search for STEP_SELECT --------------------------------- */

/*
 * Case-insensitive substring search.
 * Returns true if needle is found within haystack.
 */
static bool icontains(const char *haystack, const char *needle)
{
	if (!haystack || !needle) return false;
	size_t hlen = strlen(haystack);
	size_t nlen = strlen(needle);
	if (nlen > hlen) return false;
	for (size_t i = 0; i <= hlen - nlen; i++) {
		bool match = true;
		for (size_t j = 0; j < nlen; j++) {
			if (tolower((unsigned char) haystack[i + j]) != tolower((unsigned char) needle[j])) {
				match = false;
				break;
			}
		}
		if (match) return true;
	}
	return false;
}

extern "C" int newmenu_handler(window *wind, d_event *event, void *data);
extern "C" int listbox_handler(window *wind, d_event *event, void *data);
extern "C" int newmenu_key_command(window *wind, d_event *event, newmenu *menu);
extern "C" int listbox_key_command(window *wind, d_event *event, listbox *lb);
typedef struct kc_menu kc_menu;
extern "C" int kconfig_handler(window *wind, d_event *event, kc_menu *menu);
extern "C" int game_handler(window *wind, d_event *event, void *data);
typedef struct automap automap;
extern "C" int automap_handler(window *wind, d_event *event, automap *am);
extern "C" int automap_key_command(window *wind, d_event *event, automap *am);
#ifdef DXX_BUILD_DESCENT_II
extern "C" int title_handler(window *wind, d_event *event, void *data);
extern "C" int briefing_handler(window *wind, d_event *event, void *data);
extern "C" int MovieHandler(window *wind, d_event *event, void *data);
#endif

struct automate_key_event {
	event_type type;
	int keycode;
};

/*
 * Find a menu item whose text contains `text` (case-insensitive).
 * Searches the front window (newmenu or listbox).
 * On success sets *out_target to the item index and *out_current to
 * the currently selected item. If out_current_type is non-NULL and the
 * front window is a newmenu, also stores the current item's type there.
 * Returns true.
 * On failure logs the error and returns false.
 */
static bool select_find_item(const char *text, int *out_target, int *out_current,
                             int *out_current_type)
{
	window *front = window_get_front();
	if (!front) {
		LOGE("SELECT: no front window");
		return false;
	}

	int (*cb)(window *, d_event *, void *) = window_get_callback(front);
	void *data = window_get_data(front);
	if (!data) {
		LOGE("SELECT: front window has no data");
		return false;
	}

	if (cb == (int (*)(window *, d_event *, void *)) newmenu_handler) {
		newmenu *menu = (newmenu *) data;
		newmenu_item *items = newmenu_get_items(menu);
		int nitems = newmenu_get_nitems(menu);
		int citem = newmenu_get_citem(menu);
		int current_type = (citem >= 0 && citem < nitems) ? items[citem].type : -1;
		const char *current_text =
		    (citem >= 0 && citem < nitems && items[citem].text) ? items[citem].text : "(null)";

		for (int i = 0; i < nitems; i++) {
			if (items[i].text && icontains(items[i].text, text)) {
				/* Skip NM_TYPE_TEXT items -- they are not selectable */
				if (items[i].type == NM_TYPE_TEXT) continue;
				*out_target = i;
				*out_current = citem;
				if (out_current_type) {
					*out_current_type = current_type;
				}
				LOGI("SELECT: found \"%s\" at index %d (current=%d type=%d text=\"%s\") in newmenu",
				     items[i].text, i, citem, current_type, current_text);
				return true;
			}
		}
		LOGE("SELECT: no item matching \"%s\" in newmenu (%d items)", text, nitems);
		for (int i = 0; i < nitems; i++)
			LOGI("  item[%d]: \"%s\"", i, items[i].text ? items[i].text : "(null)");
		return false;
	} else if (cb == (int (*)(window *, d_event *, void *)) listbox_handler) {
		listbox *lb = (listbox *) data;
		char **items = listbox_get_items(lb);
		int nitems = listbox_get_nitems(lb);
		int citem = listbox_get_citem(lb);
		if (out_current_type)
			*out_current_type = -1;

		for (int i = 0; i < nitems; i++) {
			if (items[i] && icontains(items[i], text)) {
				*out_target = i;
				*out_current = citem;
				LOGI("SELECT: found \"%s\" at index %d (current=%d) in listbox",
				     items[i], i, citem);
				return true;
			}
		}
		LOGE("SELECT: no item matching \"%s\" in listbox (%d items)", text, nitems);
		for (int i = 0; i < nitems; i++)
			LOGI("  item[%d]: \"%s\"", i, items[i] ? items[i] : "(null)");
		return false;
	} else {
		LOGE("SELECT: front window is not a newmenu or listbox");
		return false;
	}
}

static bool mission_list_item_is_command(const char *text)
{
	if (!text || !text[0])
		return true;
	if (strcasecmp(text, "cancel") == 0)
		return true;
	if (strcasecmp(text, "ok") == 0)
		return true;
	return false;
}

static bool mission_list_item_is_base_mission(const char *text)
{
	if (!text || !text[0])
		return false;
#ifdef DXX_BUILD_DESCENT_II
	if (strncasecmp(text, "d1:", 3) == 0 ||
	    strncasecmp(text, "d2:", 3) == 0)
		return true;
	if (icontains(text, "counterstrike"))
		return true;
#else
	if (icontains(text, "counterstrike"))
		return true;
	if (icontains(text, "first strike") ||
	    icontains(text, "destination saturn"))
		return true;
#endif
	return false;
}

static bool mission_list_item_is_base_or_command(const char *text)
{
	return mission_list_item_is_command(text) || mission_list_item_is_base_mission(text);
}

static bool select_find_first_non_base_mission(int *out_target, int *out_current,
                                               int *out_current_type)
{
	window *front = window_get_front();
	if (!front) {
		LOGE("SELECT_NON_BASE_MISSION: no front window");
		return false;
	}

	int (*cb)(window *, d_event *, void *) = window_get_callback(front);
	void *data = window_get_data(front);
	if (!data) {
		LOGE("SELECT_NON_BASE_MISSION: front window has no data");
		return false;
	}

	if (cb == (int (*)(window *, d_event *, void *)) newmenu_handler) {
		newmenu *menu = (newmenu *) data;
		newmenu_item *items = newmenu_get_items(menu);
		int nitems = newmenu_get_nitems(menu);
		int citem = newmenu_get_citem(menu);
		int current_type = (citem >= 0 && citem < nitems) ? items[citem].type : -1;

		for (int i = 0; i < nitems; i++) {
			const char *text = items[i].text;
			if (!text || items[i].type == NM_TYPE_TEXT)
				continue;
			if (!mission_list_item_is_base_or_command(text)) {
				*out_target = i;
				*out_current = citem;
				if (out_current_type)
					*out_current_type = current_type;
				LOGI("SELECT_NON_BASE_MISSION: found \"%s\" at index %d (current=%d) in newmenu",
				     text, i, citem);
				return true;
			}
		}
		int base_candidate = -1;
		for (int i = 0; i < nitems; i++) {
			const char *text = items[i].text;
			if (!text || items[i].type == NM_TYPE_TEXT || mission_list_item_is_command(text))
				continue;
			if (mission_list_item_is_base_mission(text)) {
				if (base_candidate >= 0) {
					base_candidate = -1;
					break;
				}
				base_candidate = i;
			}
		}
		if (base_candidate >= 0) {
			*out_target = base_candidate;
			*out_current = citem;
			if (out_current_type)
				*out_current_type = current_type;
			LOGI("SELECT_NON_BASE_MISSION: using sole base mission fallback \"%s\" at index %d (current=%d) in newmenu",
			     items[base_candidate].text, base_candidate, citem);
			return true;
		}
		LOGE("SELECT_NON_BASE_MISSION: no non-base mission in newmenu (%d items)", nitems);
		for (int i = 0; i < nitems; i++)
			LOGI("  item[%d]: \"%s\"", i, items[i].text ? items[i].text : "(null)");
		return false;
	} else if (cb == (int (*)(window *, d_event *, void *)) listbox_handler) {
		listbox *lb = (listbox *) data;
		char **items = listbox_get_items(lb);
		int nitems = listbox_get_nitems(lb);
		int citem = listbox_get_citem(lb);
		if (out_current_type)
			*out_current_type = -1;

		for (int i = 0; i < nitems; i++) {
			if (items[i] && !mission_list_item_is_base_or_command(items[i])) {
				*out_target = i;
				*out_current = citem;
				LOGI("SELECT_NON_BASE_MISSION: found \"%s\" at index %d (current=%d) in listbox",
				     items[i], i, citem);
				return true;
			}
		}
		int base_candidate = -1;
		for (int i = 0; i < nitems; i++) {
			if (!items[i] || mission_list_item_is_command(items[i]))
				continue;
			if (mission_list_item_is_base_mission(items[i])) {
				if (base_candidate >= 0) {
					base_candidate = -1;
					break;
				}
				base_candidate = i;
			}
		}
		if (base_candidate >= 0) {
			*out_target = base_candidate;
			*out_current = citem;
			LOGI("SELECT_NON_BASE_MISSION: using sole base mission fallback \"%s\" at index %d (current=%d) in listbox",
			     items[base_candidate], base_candidate, citem);
			return true;
		}
		LOGE("SELECT_NON_BASE_MISSION: no non-base mission in listbox (%d items)", nitems);
		for (int i = 0; i < nitems; i++)
			LOGI("  item[%d]: \"%s\"", i, items[i] ? items[i] : "(null)");
		return false;
	} else {
		LOGE("SELECT_NON_BASE_MISSION: front window is not a newmenu or listbox");
		return false;
	}
}

static bool front_is_start_level_menu(void)
{
	window *front = window_get_front();
	if (!front)
		return false;

	int (*cb)(window *, d_event *, void *) = window_get_callback(front);
	void *data = window_get_data(front);
	if (cb != (int (*)(window *, d_event *, void *)) newmenu_handler || !data)
		return false;

	newmenu *menu = (newmenu *) data;
	newmenu_item *items = newmenu_get_items(menu);
	int nitems = newmenu_get_nitems(menu);
	int citem = newmenu_get_citem(menu);
	const char *subtitle = newmenu_get_subtitle(menu);
	bool has_start_prompt = subtitle && icontains(subtitle, "select starting level");
	bool has_level_range = false;
	bool has_input = false;

	for (int i = 0; i < nitems; i++) {
		const char *text = items[i].text;
		if (text && icontains(text, "you may start on") &&
		    icontains(text, "level up to"))
			has_level_range = true;
		if (items[i].type == NM_TYPE_INPUT)
			has_input = true;
	}

	if (has_start_prompt && has_level_range && has_input) {
		const char *current_text =
		    (citem >= 0 && citem < nitems && items[citem].text) ? items[citem].text : "";
		LOGI("SELECT_MISSION: mission picker skipped, already at start-level menu (citem=%d text=\"%s\")",
		     citem, current_text);
		return true;
	}

	return false;
}

static bool select_find_target(const auto_step &s, int *target, int *current,
                               int *current_type)
{
	if (s.select_non_base_mission)
		return select_find_first_non_base_mission(target, current, current_type);
	return select_find_item(s.select_text.c_str(), target, current, current_type);
}

static bool select_can_confirm_current_input_as_ok(const auto_step &s,
                                                   int delta, int current_type)
{
	return delta == 1 &&
	       current_type == NM_TYPE_INPUT &&
	       strcasecmp(s.select_text.c_str(), "ok") == 0;
}

static bool select_dispatch_front_menu_key(int keycode, const char *key_name)
{
	window *front = window_get_front();
	if (!front)
		return false;

	int (*cb)(window *, d_event *, void *) = window_get_callback(front);
	if (!cb)
		return false;

	void *data = window_get_data(front);

	automate_key_event key_event;
	key_event.type = EVENT_KEY_COMMAND;
	key_event.keycode = keycode;

	if (cb == (int (*)(window *, d_event *, void *)) newmenu_handler) {
		if (!data)
			return false;
		LOGI("SELECT: dispatching newmenu key %s (key=%d)", key_name, keycode);
		newmenu_key_command(front, (d_event *) &key_event, (newmenu *) data);
		return true;
	}
	if (cb == (int (*)(window *, d_event *, void *)) listbox_handler) {
		if (!data)
			return false;
		LOGI("SELECT: dispatching listbox key %s (key=%d)", key_name, keycode);
		listbox_key_command(front, (d_event *) &key_event, (listbox *) data);
		return true;
	}
	if (cb == (int (*)(window *, d_event *, void *)) kconfig_handler) {
		if (!data)
			return false;
		LOGI("KEY: dispatching kconfig key %s (key=%d)", key_name, keycode);
		kconfig_handler(front, (d_event *) &key_event, (kc_menu *) data);
		return true;
	}
	if (cb == (int (*)(window *, d_event *, void *)) game_handler) {
		LOGI("KEY: dispatching game key %s (key=%d)", key_name, keycode);
		game_handler(front, (d_event *) &key_event, data);
		return true;
	}
	if (cb == (int (*)(window *, d_event *, void *)) automap_handler) {
		if (!data)
			return false;
		LOGI("KEY: dispatching automap key %s (key=%d)", key_name, keycode);
		automap_key_command(front, (d_event *) &key_event, (automap *) data);
		return true;
	}
#ifdef DXX_BUILD_DESCENT_II
	if (cb == (int (*)(window *, d_event *, void *)) title_handler) {
		if (!data)
			return false;
		LOGI("SELECT: dispatching title key %s (key=%d)", key_name, keycode);
		title_handler(front, (d_event *) &key_event, data);
		return true;
	}
	if (cb == (int (*)(window *, d_event *, void *)) briefing_handler) {
		if (!data)
			return false;
		LOGI("SELECT: dispatching briefing key %s (key=%d)", key_name, keycode);
		briefing_handler(front, (d_event *) &key_event, data);
		return true;
	}
	if (cb == (int (*)(window *, d_event *, void *)) MovieHandler) {
		if (!data)
			return false;
		LOGI("SELECT: dispatching movie key %s (key=%d)", key_name, keycode);
		MovieHandler(front, (d_event *) &key_event, data);
		return true;
	}
#endif

	return false;
}

static bool can_direct_dispatch_front_key_command(void)
{
	window *front = window_get_front();
	if (!front)
		return false;

	int (*cb)(window *, d_event *, void *) = window_get_callback(front);
	if (!cb)
		return false;

	void *data = window_get_data(front);

	if (cb == (int (*)(window *, d_event *, void *)) newmenu_handler)
		return data != NULL;
	if (cb == (int (*)(window *, d_event *, void *)) listbox_handler)
		return data != NULL;
	if (cb == (int (*)(window *, d_event *, void *)) kconfig_handler)
		return data != NULL;
	if (cb == (int (*)(window *, d_event *, void *)) game_handler)
		return true;
	if (cb == (int (*)(window *, d_event *, void *)) automap_handler)
		return data != NULL;
#ifdef DXX_BUILD_DESCENT_II
	if (cb == (int (*)(window *, d_event *, void *)) title_handler)
		return data != NULL;
	if (cb == (int (*)(window *, d_event *, void *)) briefing_handler)
		return data != NULL;
	if (cb == (int (*)(window *, d_event *, void *)) MovieHandler)
		return data != NULL;
#endif
	return false;
}

static const char *describe_window_handler(window *wind)
{
	if (!wind)
		return "none";

	int (*cb)(window *, d_event *, void *) = window_get_callback(wind);
	if (cb == (int (*)(window *, d_event *, void *)) newmenu_handler)
		return "newmenu";
	if (cb == (int (*)(window *, d_event *, void *)) listbox_handler)
		return "listbox";
	if (cb == (int (*)(window *, d_event *, void *)) kconfig_handler)
		return "kconfig";
#ifdef DXX_BUILD_DESCENT_II
	if (cb == (int (*)(window *, d_event *, void *)) title_handler)
		return "title";
	if (cb == (int (*)(window *, d_event *, void *)) briefing_handler)
		return "briefing";
	if (cb == (int (*)(window *, d_event *, void *)) MovieHandler)
		return "movie";
#endif
	return "unknown";
}

static void average_three_vectors(vms_vector *dest, const vms_vector *a, const vms_vector *b, const vms_vector *c)
{
	dest->x = (a->x + b->x + c->x) / 3;
	dest->y = (a->y + b->y + c->y) / 3;
	dest->z = (a->z + b->z + c->z) / 3;
}

static int compute_face_view_center(int segnum, int sidenum, int face, vms_vector *center, char *reason, size_t reason_size)
{
	segment *segp;
	side *sidep;
	int side_verts[4];
	const vms_vector *verts[4];

	if (segnum < 0 || segnum > Highest_segment_index) {
		snprintf(reason, reason_size, "face_view: invalid segment %d", segnum);
		return 0;
	}
	if (sidenum < 0 || sidenum >= MAX_SIDES_PER_SEGMENT) {
		snprintf(reason, reason_size, "face_view: invalid side %d", sidenum);
		return 0;
	}

	segp = &Segments[segnum];
	sidep = &segp->sides[sidenum];
	get_side_verts(side_verts, segnum, sidenum);
	for (int i = 0; i < 4; ++i)
		verts[i] = &Vertices[side_verts[i]];

	switch (sidep->type) {
		case SIDE_IS_QUAD:
			if (face != 0) {
				snprintf(reason, reason_size, "face_view: quad side %d face must be 0, got %d", sidenum, face);
				return 0;
			}
			vm_vec_avg4(center, verts[0], verts[1], verts[2], verts[3]);
			return 1;

		case SIDE_IS_TRI_02:
			if (face == 0)
				average_three_vectors(center, verts[0], verts[1], verts[2]);
			else if (face == 1)
				average_three_vectors(center, verts[0], verts[2], verts[3]);
			else {
				snprintf(reason, reason_size, "face_view: tri_02 side %d face must be 0 or 1, got %d", sidenum, face);
				return 0;
			}
			return 1;

		case SIDE_IS_TRI_13:
			if (face == 0)
				average_three_vectors(center, verts[0], verts[1], verts[3]);
			else if (face == 1)
				average_three_vectors(center, verts[1], verts[2], verts[3]);
			else {
				snprintf(reason, reason_size, "face_view: tri_13 side %d face must be 0 or 1, got %d", sidenum, face);
				return 0;
			}
			return 1;

		default:
			snprintf(reason, reason_size, "face_view: unsupported side type %d", sidep->type);
			return 0;
	}
}

static int compute_face_view_normal(int segnum, int sidenum, int face, vms_vector *normal, char *reason, size_t reason_size)
{
	segment *segp;
	side *sidep;
	int side_verts[4];

	if (segnum < 0 || segnum > Highest_segment_index) {
		snprintf(reason, reason_size, "face_view: invalid segment %d", segnum);
		return 0;
	}
	if (sidenum < 0 || sidenum >= MAX_SIDES_PER_SEGMENT) {
		snprintf(reason, reason_size, "face_view: invalid side %d", sidenum);
		return 0;
	}

	segp = &Segments[segnum];
	sidep = &segp->sides[sidenum];
	get_side_verts(side_verts, segnum, sidenum);

	switch (sidep->type) {
		case SIDE_IS_QUAD:
			if (face != 0) {
				snprintf(reason, reason_size, "face_view: quad side %d face must be 0, got %d", sidenum, face);
				return 0;
			}
			vm_vec_normal(normal, &Vertices[side_verts[0]], &Vertices[side_verts[1]], &Vertices[side_verts[2]]);
			return 1;

		case SIDE_IS_TRI_02:
			if (face == 0)
				vm_vec_normal(normal, &Vertices[side_verts[0]], &Vertices[side_verts[1]], &Vertices[side_verts[2]]);
			else if (face == 1)
				vm_vec_normal(normal, &Vertices[side_verts[0]], &Vertices[side_verts[2]], &Vertices[side_verts[3]]);
			else {
				snprintf(reason, reason_size, "face_view: tri_02 side %d face must be 0 or 1, got %d", sidenum, face);
				return 0;
			}
			return 1;

		case SIDE_IS_TRI_13:
			if (face == 0)
				vm_vec_normal(normal, &Vertices[side_verts[0]], &Vertices[side_verts[1]], &Vertices[side_verts[3]]);
			else if (face == 1)
				vm_vec_normal(normal, &Vertices[side_verts[1]], &Vertices[side_verts[2]], &Vertices[side_verts[3]]);
			else {
				snprintf(reason, reason_size, "face_view: tri_13 side %d face must be 0 or 1, got %d", sidenum, face);
				return 0;
			}
			return 1;

		default:
			snprintf(reason, reason_size, "face_view: unsupported side type %d", sidep->type);
			return 0;
	}
}

static int move_player_to_face_view(const auto_step &s, char *reason, size_t reason_size)
{
	vms_vector face_center, segment_center, to_segment, inward, forward, new_pos;
	fix inward_len, travel;
	int new_seg;

	if (Screen_mode != SCREEN_GAME || Game_wind == NULL) {
		snprintf(reason, reason_size, "face_view: game window is not active");
		return 0;
	}
	if (ConsoleObject == NULL) {
		snprintf(reason, reason_size, "face_view: no player object");
		return 0;
	}
	if (!compute_face_view_center(s.segment, s.side, s.face, &face_center, reason, reason_size))
		return 0;

	compute_segment_center(&segment_center, &Segments[s.segment]);
	vm_vec_sub(&to_segment, &segment_center, &face_center);
	inward_len = vm_vec_normalize_quick(&to_segment);
	if (inward_len <= 0) {
		snprintf(reason, reason_size, "face_view: zero inward vector for seg=%d side=%d face=%d",
		         s.segment, s.side, s.face);
		return 0;
	}

	if (!compute_face_view_normal(s.segment, s.side, s.face, &inward, reason, reason_size))
		return 0;
	if (vm_vec_dot(&inward, &to_segment) < 0)
		vm_vec_negate(&inward);
	if (vm_vec_normalize_quick(&inward) <= 0) {
		snprintf(reason, reason_size, "face_view: zero side normal for seg=%d side=%d face=%d",
		         s.segment, s.side, s.face);
		return 0;
	}

	travel = fl2f(s.distance > 0.0f ? s.distance : 4.0f);
	if (travel <= 0)
		travel = inward_len / 2;
	if (travel >= inward_len)
		travel = inward_len / 2;
	if (travel <= 0) {
		snprintf(reason, reason_size, "face_view: invalid travel distance for seg=%d side=%d face=%d",
		         s.segment, s.side, s.face);
		return 0;
	}

	vm_vec_scale(&inward, travel);
	vm_vec_add(&new_pos, &face_center, &inward);
	new_seg = find_point_seg(&new_pos, s.segment);
	if (new_seg < 0) {
		vms_vector inside_pos = segment_center;
		vms_vector outside_pos = new_pos;
		int inside_seg = find_point_seg(&inside_pos, s.segment);

		if (inside_seg < 0)
			inside_seg = s.segment;
		for (int i = 0; i < 8; ++i) {
			vms_vector mid_pos;
			int mid_seg;

			mid_pos.x = (inside_pos.x + outside_pos.x) / 2;
			mid_pos.y = (inside_pos.y + outside_pos.y) / 2;
			mid_pos.z = (inside_pos.z + outside_pos.z) / 2;
			mid_seg = find_point_seg(&mid_pos, s.segment);
			if (mid_seg >= 0) {
				inside_pos = mid_pos;
				inside_seg = mid_seg;
			} else {
				outside_pos = mid_pos;
			}
		}
		new_pos = inside_pos;
		new_seg = inside_seg;
	}

	ConsoleObject->pos = new_pos;
	ConsoleObject->last_pos = new_pos;
	if (ConsoleObject->segnum != new_seg)
		obj_relink((int) (ConsoleObject - Objects), new_seg);
	else
		ConsoleObject->segnum = new_seg;

	vm_vec_sub(&forward, &face_center, &ConsoleObject->pos);
	if (vm_vec_normalize_quick(&forward) <= 0) {
		snprintf(reason, reason_size, "face_view: zero forward vector for seg=%d side=%d face=%d",
		         s.segment, s.side, s.face);
		return 0;
	}
	vm_vector_2_matrix(&ConsoleObject->orient, &forward, NULL, NULL);
	vm_vec_zero(&ConsoleObject->mtype.phys_info.velocity);
	vm_vec_zero(&ConsoleObject->mtype.phys_info.rotvel);
	vm_vec_zero(&ConsoleObject->mtype.phys_info.thrust);
	vm_vec_zero(&ConsoleObject->mtype.phys_info.rotthrust);

	LOGI("face_view: seg=%d side=%d face=%d pos=(%.2f, %.2f, %.2f)",
	     s.segment, s.side, s.face,
	     f2fl(ConsoleObject->pos.x), f2fl(ConsoleObject->pos.y), f2fl(ConsoleObject->pos.z));
	return 1;
}

static int move_player_to_first_merged_face(auto_step &s, char *reason, size_t reason_size)
{
	auto_step attempt = s;

	for (int segnum = 0; segnum <= Highest_segment_index; ++segnum) {
		segment *segp = &Segments[segnum];

		for (int sidenum = 0; sidenum < MAX_SIDES_PER_SEGMENT; ++sidenum) {
			side *sidep = &segp->sides[sidenum];
			int face_count;

			if (sidep->tmap_num2 == 0)
				continue;
			face_count = sidep->type == SIDE_IS_QUAD ? 1 : 2;
			for (int face = 0; face < face_count; ++face) {
				attempt.segment = segnum;
				attempt.side = sidenum;
				attempt.face = face;
				if (!move_player_to_face_view(attempt, reason, reason_size))
					continue;
				s.segment = segnum;
				s.side = sidenum;
				s.face = face;
				LOGI("face_first_merged: seg=%d side=%d face=%d tmap1=%d tmap2=0x%x",
				     segnum, sidenum, face, sidep->tmap_num, sidep->tmap_num2);
				return 1;
			}
		}
	}

	snprintf(reason, reason_size, "face_first_merged: no merged faces on level %d", Current_level_num);
	return 0;
}

static int move_player_to_pose(const auto_step &s, char *reason, size_t reason_size)
{
	vms_vector new_pos;
	vms_angvec angles;
	int search_seg, new_seg;

	if (Screen_mode != SCREEN_GAME || Game_wind == NULL || ConsoleObject == NULL) {
		snprintf(reason, reason_size, "pose_view: game window is not active");
		return 0;
	}

	new_pos.x = fl2f(s.pos_x);
	new_pos.y = fl2f(s.pos_y);
	new_pos.z = fl2f(s.pos_z);
	search_seg = s.segment >= 0 ? s.segment : ConsoleObject->segnum;
	if (search_seg < 0 || search_seg > Highest_segment_index)
		search_seg = ConsoleObject->segnum;
	new_seg = find_point_seg(&new_pos, search_seg);
	if (new_seg < 0) {
		snprintf(reason, reason_size,
		         "pose_view: point not in mine for seg=%d pos=(%.6f, %.6f, %.6f)",
		         search_seg,
		         s.pos_x,
		         s.pos_y,
		         s.pos_z);
		return 0;
	}

	ConsoleObject->pos = new_pos;
	ConsoleObject->last_pos = new_pos;
	if (ConsoleObject->segnum != new_seg)
		obj_relink((int) (ConsoleObject - Objects), new_seg);
	else
		ConsoleObject->segnum = new_seg;

	angles.p = (fixang) s.pitch;
	angles.b = (fixang) s.bank;
	angles.h = (fixang) s.heading;
	vm_angles_2_matrix(&ConsoleObject->orient, &angles);
	vm_vec_zero(&ConsoleObject->mtype.phys_info.velocity);
	vm_vec_zero(&ConsoleObject->mtype.phys_info.rotvel);
	vm_vec_zero(&ConsoleObject->mtype.phys_info.thrust);
	vm_vec_zero(&ConsoleObject->mtype.phys_info.rotthrust);

	LOGI("pose_view: seg=%d pos=(%.6f, %.6f, %.6f) pitch=%d bank=%d heading=%d",
	     new_seg,
	     s.pos_x,
	     s.pos_y,
	     s.pos_z,
	     s.pitch,
	     s.bank,
	     s.heading);
	return 1;
}

static int move_player_to_route_guidance(char *reason, size_t reason_size)
{
#ifdef DXX_BUILD_DESCENT_II
	const level_metadata_state *metadata = level_metadata_get_live_route_state();
	route_planner_plan_summary plan;
	const level_metadata_route_step *step;
	vms_vector activation;
	vms_vector direction;
	int new_seg;

	if (Screen_mode != SCREEN_GAME || Game_wind == NULL || ConsoleObject == NULL) {
		snprintf(reason, reason_size, "pose_route_guidance: game window is not active");
		return 0;
	}
	if (!metadata || !level_metadata_get_live_route_plan_summary(&plan) ||
	    plan.first_pending_step < 0 ||
	    plan.first_pending_step >= metadata->route_step_count) {
		snprintf(reason, reason_size, "pose_route_guidance: no pending route step");
		return 0;
	}
	step = &metadata->route_steps[plan.first_pending_step];
	if (!step->activation_pos_valid || !step->aim_pos_valid) {
		snprintf(reason, reason_size,
		         "pose_route_guidance: pending step has no firing pose");
		return 0;
	}
	activation.x = step->activation_pos[0];
	activation.y = step->activation_pos[1];
	activation.z = step->activation_pos[2];
	new_seg = find_point_seg(&activation, step->seg);
	if (new_seg < 0) {
		snprintf(reason, reason_size,
		         "pose_route_guidance: activation point is outside the mine");
		return 0;
	}
	if (step->activation_kind == LEVEL_METADATA_ROUTE_ACTIVATION_ENTER_EXIT)
		new_seg = step->seg;
	ConsoleObject->pos = activation;
	ConsoleObject->last_pos = ConsoleObject->pos;
	if (ConsoleObject->segnum != new_seg)
		obj_relink(ConsoleObject - Objects, new_seg);
	direction.x = step->aim_pos[0] - step->activation_pos[0];
	direction.y = step->aim_pos[1] - step->activation_pos[1];
	direction.z = step->aim_pos[2] - step->activation_pos[2];
	if (vm_vec_normalize(&direction) != 0)
		vm_vector_2_matrix(&ConsoleObject->orient, &direction, NULL, NULL);
	vm_vec_zero(&ConsoleObject->mtype.phys_info.velocity);
	vm_vec_zero(&ConsoleObject->mtype.phys_info.rotvel);
	vm_vec_zero(&ConsoleObject->mtype.phys_info.thrust);
	vm_vec_zero(&ConsoleObject->mtype.phys_info.rotthrust);
	LOGI("pose_route_guidance: step=%d seg=%d activation=(%d,%d,%d) aim=(%d,%d,%d)",
	     plan.first_pending_step, new_seg,
	     step->activation_pos[0], step->activation_pos[1], step->activation_pos[2],
	     step->aim_pos[0], step->aim_pos[1], step->aim_pos[2]);
	return 1;
#else
	snprintf(reason, reason_size, "pose_route_guidance: requires Descent 2");
	return 0;
#endif
}

static int clear_level_robots(char *reason, size_t reason_size)
{
	int removed = 0;

	if (Screen_mode != SCREEN_GAME || Game_wind == NULL) {
		snprintf(reason, reason_size, "clear_robots: game window is not active");
		return 0;
	}

	for (int objnum = Highest_object_index; objnum >= 0; --objnum) {
		if (Objects[objnum].type != OBJ_ROBOT)
			continue;
#ifdef DXX_BUILD_DESCENT_II
		if (Robot_info[Objects[objnum].id].companion)
			continue;
#endif
		obj_delete(objnum);
		removed++;
	}

	LOGI("clear_robots: removed=%d", removed);
	return 1;
}

static int release_guidebot_cage(char *reason, size_t reason_size)
{
#ifdef DXX_BUILD_DESCENT_II
	unsigned char processed[MAX_WALLS] = {};
	int segments[MAX_SIDES_PER_SEGMENT + 1];
	int segment_count = 1;
	int destroyed = 0;

	if (Screen_mode != SCREEN_GAME || Game_wind == NULL || ConsoleObject == NULL) {
		snprintf(reason, reason_size, "release_guidebot_cage: game is not running");
		return 0;
	}
	if (Buddy_objnum < 0 || Buddy_objnum > Highest_object_index ||
	    Objects[Buddy_objnum].type != OBJ_ROBOT ||
	    !Robot_info[Objects[Buddy_objnum].id].companion) {
		snprintf(reason, reason_size, "release_guidebot_cage: companion not found");
		return 0;
	}
	if (Objects[Buddy_objnum].segnum < 0 ||
	    Objects[Buddy_objnum].segnum > Highest_segment_index) {
		snprintf(reason, reason_size, "release_guidebot_cage: companion segment is invalid");
		return 0;
	}

	segments[0] = Objects[Buddy_objnum].segnum;
	for (int side = 0; side < MAX_SIDES_PER_SEGMENT; ++side) {
		const int child = Segments[segments[0]].children[side];
		if (IS_CHILD(child))
			segments[segment_count++] = child;
	}
	auto destroy_wall = [&](int segnum, int side) {
		const int wall_num = Segments[segnum].sides[side].wall_num;
		if (wall_num < 0 || wall_num >= MAX_WALLS || processed[wall_num] ||
		    Walls[wall_num].type != WALL_BLASTABLE ||
		    (Walls[wall_num].flags & WALL_BLASTED))
			return;
		processed[wall_num] = 1;
		const int child = Segments[segnum].children[side];
		if (IS_CHILD(child)) {
			const int connected_side = find_connect_side(&Segments[segnum], &Segments[child]);
			if (connected_side >= 0) {
				const int connected_wall = Segments[child].sides[connected_side].wall_num;
				if (connected_wall >= 0 && connected_wall < MAX_WALLS)
					processed[connected_wall] = 1;
			}
		}
		wall_destroy(&Segments[segnum], side);
		destroyed++;
	};
	for (int i = 0; i < segment_count; ++i) {
		const int segnum = segments[i];
		for (int side = 0; side < MAX_SIDES_PER_SEGMENT; ++side)
			destroy_wall(segnum, side);
	}
	for (int segnum = 0; !destroyed && segnum <= Highest_segment_index; ++segnum) {
		for (int side = 0; side < MAX_SIDES_PER_SEGMENT; ++side)
			destroy_wall(segnum, side);
	}
	if (!destroyed) {
		escort_notify_blastable_wall_destroyed();
		if (!Buddy_allowed_to_talk) {
			snprintf(reason, reason_size,
			         "release_guidebot_cage: cage remains intact");
			return 0;
		}
	}
	LOGI("release_guidebot_cage: obj=%d seg=%d destroyed=%d",
	     Buddy_objnum, Objects[Buddy_objnum].segnum, destroyed);
	return 1;
#else
	snprintf(reason, reason_size, "release_guidebot_cage: requires Descent 2");
	return 0;
#endif
}

static int automation_key_flags_from_value(const char *value)
{
	char *end = NULL;
	long numeric;
	int flags = 0;

	if (!value)
		return 0;
	numeric = strtol(value, &end, 0);
	if (end && end != value && *end == '\0')
		return (int) numeric & (PLAYER_FLAGS_BLUE_KEY | PLAYER_FLAGS_RED_KEY | PLAYER_FLAGS_GOLD_KEY);
	if (icontains(value, "all")) {
		flags |= PLAYER_FLAGS_BLUE_KEY | PLAYER_FLAGS_RED_KEY | PLAYER_FLAGS_GOLD_KEY;
	} else {
		if (icontains(value, "blue"))
			flags |= PLAYER_FLAGS_BLUE_KEY;
		if (icontains(value, "red"))
			flags |= PLAYER_FLAGS_RED_KEY;
		if (icontains(value, "gold") || icontains(value, "yellow"))
			flags |= PLAYER_FLAGS_GOLD_KEY;
	}
	return flags;
}

static int set_player_key_flags(const std::string &value, char *reason, size_t reason_size)
{
	const int key_mask = PLAYER_FLAGS_BLUE_KEY | PLAYER_FLAGS_RED_KEY | PLAYER_FLAGS_GOLD_KEY;
	const int old_flags = Players[Player_num].flags;
	int new_flags;

	if (Screen_mode != SCREEN_GAME || Game_wind == NULL || ConsoleObject == NULL) {
		snprintf(reason, reason_size, "player_keys: game is not running");
		return 0;
	}
	new_flags = (old_flags & ~key_mask) | automation_key_flags_from_value(value.c_str());
	Players[Player_num].flags = new_flags;
#ifdef DXX_BUILD_DESCENT_II
	escort_note_player_key_flags(old_flags, new_flags);
#endif
	return 1;
}

static int fire_level_trigger(const std::string &value, char *reason, size_t reason_size)
{
	char *end = NULL;
	long trigger_num;

	if (Screen_mode != SCREEN_GAME || Game_wind == NULL || ConsoleObject == NULL) {
		snprintf(reason, reason_size, "fire_trigger: game is not running");
		return 0;
	}
	trigger_num = strtol(value.c_str(), &end, 10);
	if (end == value.c_str() || (end && *end != '\0')) {
		snprintf(reason, reason_size, "fire_trigger: invalid trigger '%s'", value.c_str());
		return 0;
	}
	if (trigger_num < 0 || trigger_num >= Num_triggers) {
		snprintf(reason, reason_size, "fire_trigger: trigger %ld outside 0..%d", trigger_num, Num_triggers - 1);
		return 0;
	}
	check_trigger_sub((int) trigger_num, Player_num, 1);
	LOGI("fire_trigger: trigger=%ld", trigger_num);
	return 1;
}

static int complete_route_objective(char *reason, size_t reason_size)
{
#ifdef DXX_BUILD_DESCENT_II
	const int activation = escort_get_route_goal_activation_kind();
	const int target_seg = escort_get_route_goal_objective_seg();
	const int target_side = escort_get_route_goal_objective_side();
	const int target_wall = escort_get_route_goal_objective_wall();
	int fallback = -1;

	if (Screen_mode != SCREEN_GAME || Game_wind == NULL || ConsoleObject == NULL ||
	    !escort_get_route_goal_active()) {
		snprintf(reason, reason_size,
		         "complete_route_objective: no active route objective");
		return 0;
	}
	if (activation ==
	    LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_BLASTABLE_WALL) {
		object impact = {};

		if (target_seg < 0 || target_seg > Highest_segment_index ||
		    target_side < 0 || target_side >= MAX_SIDES_PER_SEGMENT ||
		    target_wall < 0 || target_wall >= Num_walls ||
		    Segments[target_seg].sides[target_side].wall_num != target_wall ||
		    Walls[target_wall].type != WALL_BLASTABLE) {
			snprintf(reason, reason_size,
			         "complete_route_objective: invalid blastable wall");
			return 0;
		}
		impact.type = OBJ_WEAPON;
		impact.ctype.laser_info.parent_type = OBJ_PLAYER;
		impact.ctype.laser_info.parent_num = ConsoleObject - Objects;
		wall_hit_process(
		    &Segments[target_seg], target_side,
		    Walls[target_wall].hps + F1_0, Player_num, &impact);
		LOGI("complete_route_objective: blastable wall=%d seg=%d side=%d flags=%d",
		     target_wall, target_seg, target_side, Walls[target_wall].flags);
		return 1;
	}
	if (activation == LEVEL_METADATA_ROUTE_ACTIVATION_PICKUP_KEY) {
		for (int objnum = 0; objnum <= Highest_object_index; ++objnum) {
			object *objp = &Objects[objnum];
			if (objp->type != OBJ_POWERUP ||
			    (objp->flags & OF_SHOULD_BE_DEAD) ||
			    objp->segnum != target_seg ||
			    (objp->id != POW_KEY_BLUE &&
			     objp->id != POW_KEY_RED &&
			     objp->id != POW_KEY_GOLD))
				continue;
			collide_player_and_powerup(ConsoleObject, objp, &objp->pos);
			if (!(objp->flags & OF_SHOULD_BE_DEAD)) {
				snprintf(reason, reason_size,
				         "complete_route_objective: key was not collected");
				return 0;
			}
			LOGI("complete_route_objective: key object=%d target_seg=%d",
			     objnum, target_seg);
			return 1;
		}
		snprintf(reason, reason_size,
		         "complete_route_objective: key object not found");
		return 0;
	}
	if (activation == LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_KEY_CARRIER) {
		const int objnum = escort_get_route_goal_objective_object();
		const int key_index = escort_get_route_goal_objective_key_index();
		const int key_flag = key_index == 0   ? PLAYER_FLAGS_BLUE_KEY
		                     : key_index == 1 ? PLAYER_FLAGS_RED_KEY
		                     : key_index == 2 ? PLAYER_FLAGS_GOLD_KEY
		                                      : 0;
		if (objnum < 0 || objnum > Highest_object_index ||
		    Objects[objnum].type != OBJ_ROBOT ||
		    (Objects[objnum].flags & OF_SHOULD_BE_DEAD) || !key_flag) {
			snprintf(reason, reason_size,
			         "complete_route_objective: key carrier not found");
			return 0;
		}
		apply_damage_to_robot(
		    &Objects[objnum], Objects[objnum].shields + F1_0,
		    ConsoleObject - Objects);
		const int old_flags = Players[Player_num].flags;
		Players[Player_num].flags |= key_flag;
		escort_note_player_key_flags(old_flags, Players[Player_num].flags);
		LOGI("complete_route_objective: key carrier=%d key_index=%d target_seg=%d",
		     objnum, key_index, target_seg);
		return 1;
	}
	for (int objnum = 0; objnum <= Highest_object_index; ++objnum) {
		object *objp = &Objects[objnum];
		int matches = 0;
		if (activation == LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_REACTOR)
			matches = objp->type == OBJ_CNTRLCEN;
		else if (activation == LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_BOSS)
			matches = objp->type == OBJ_ROBOT &&
			          Robot_info[objp->id].boss_flag;
		if (!matches || (objp->flags & OF_SHOULD_BE_DEAD))
			continue;
		if (fallback < 0)
			fallback = objnum;
		if (objp->segnum == target_seg) {
			fallback = objnum;
			break;
		}
	}
	if (fallback < 0) {
		snprintf(reason, reason_size,
		         "complete_route_objective: target object not found");
		return 0;
	}
	if (activation == LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_REACTOR) {
		LOGI("complete_route_objective: reactor before shields=%d flags=%d destroyed=%d player_obj=%d type=%d id=%d",
		     Objects[fallback].shields, Objects[fallback].flags,
		     Control_center_destroyed, (int) (ConsoleObject - Objects),
		     ConsoleObject->type, ConsoleObject->id);
		apply_damage_to_controlcen(
		    &Objects[fallback], Objects[fallback].shields + F1_0,
		    (short) (ConsoleObject - Objects));
		LOGI("complete_route_objective: reactor after shields=%d flags=%d destroyed=%d",
		     Objects[fallback].shields, Objects[fallback].flags,
		     Control_center_destroyed);
	} else if (activation == LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_BOSS)
		apply_damage_to_robot(
		    &Objects[fallback], Objects[fallback].shields + F1_0,
		    ConsoleObject - Objects);
	else {
		snprintf(reason, reason_size,
		         "complete_route_objective: unsupported activation %d",
		         activation);
		return 0;
	}
	LOGI("complete_route_objective: activation=%d object=%d target_seg=%d",
	     activation, fallback, target_seg);
	return 1;
#else
	snprintf(reason, reason_size,
	         "complete_route_objective: requires Descent 2");
	return 0;
#endif
}

static int damage_first_boss(const std::string &amount, char *reason, size_t reason_size)
{
	object *boss = NULL;

	if (Screen_mode != SCREEN_GAME || Game_wind == NULL || ConsoleObject == NULL) {
		snprintf(reason, reason_size, "damage_boss: game is not running");
		return 0;
	}
	for (int objnum = 0; objnum <= Highest_object_index; ++objnum) {
		object *candidate = &Objects[objnum];
		if (candidate->type == OBJ_ROBOT && Robot_info[candidate->id].boss_flag &&
		    candidate->shields > 0 && !(candidate->flags & OF_SHOULD_BE_DEAD)) {
			boss = candidate;
			break;
		}
	}
	if (!boss) {
		snprintf(reason, reason_size, "damage_boss: no live boss found");
		return 0;
	}
	fix damage;
	if (amount == "half")
		damage = boss->shields > 1 ? boss->shields / 2 : 1;
	else if (amount == "lethal")
		damage = boss->shields + F1_0;
	else
		damage = (fix) (std::stod(amount) * F1_0);
	if (damage <= 0) {
		snprintf(reason, reason_size, "damage_boss: damage must be positive");
		return 0;
	}
	apply_damage_to_robot(boss, damage, ConsoleObject - Objects);
	LOGI("damage_boss: object=%d damage=%d shields=%d",
	     (int) (boss - Objects), damage, boss->shields);
	return 1;
}

/* -- Condition checking ----------------------------------------------- */

extern "C" window *Game_wind;

static int check_condition(const std::string &field, const std::string &value)
{
	if (strcasecmp(field.c_str(), "in_game") == 0) {
		int in_game = (Game_wind != NULL && Screen_mode == SCREEN_GAME);
		if (strcasecmp(value.c_str(), "true") == 0) return in_game;
		if (strcasecmp(value.c_str(), "false") == 0) return !in_game;
	} else if (strcasecmp(field.c_str(), "screen_mode") == 0) {
		const char *cur = NULL;
		switch (Screen_mode) {
			case SCREEN_MENU: cur = "menu"; break;
			case SCREEN_GAME: cur = "game"; break;
			case SCREEN_EDITOR: cur = "editor"; break;
			case SCREEN_MOVIE: cur = "movie"; break;
		}
		return cur && strcasecmp(cur, value.c_str()) == 0;
	} else if (strcasecmp(field.c_str(), "automap_active") == 0) {
		if (strcasecmp(value.c_str(), "true") == 0) return Automap_active;
		if (strcasecmp(value.c_str(), "false") == 0) return !Automap_active;
	} else if (strcasecmp(field.c_str(), "game_window_is_front") == 0) {
		window *front = window_get_front();
		int is_front = (front && front == Game_wind);
		if (strcasecmp(value.c_str(), "true") == 0) return is_front;
		if (strcasecmp(value.c_str(), "false") == 0) return !is_front;
	} else if (strcasecmp(field.c_str(), "player_dead") == 0) {
		if (strcasecmp(value.c_str(), "true") == 0) return Player_is_dead;
		if (strcasecmp(value.c_str(), "false") == 0) return !Player_is_dead;
	} else if (strcasecmp(field.c_str(), "player_exploded") == 0) {
		if (strcasecmp(value.c_str(), "true") == 0) return Player_exploded;
		if (strcasecmp(value.c_str(), "false") == 0) return !Player_exploded;
	} else {
		/* Fallback: parse introspection JSON with nlohmann and look up the field */
		char *json_str = game_introspect_get_state();
		if (json_str) {
			try {
				json j = json::parse(json_str);
				free(json_str);
				if (j.contains(field)) {
					std::string actual;
					if (j[field].is_string()) {
						actual = j[field].get<std::string>();
					} else {
						actual = j[field].dump();
					}
					return strcasecmp(actual.c_str(), value.c_str()) == 0;
				}
			} catch (...) {
				free(json_str);
			}
			return 0;
		}
	}
	return 0;
}

/* -- Parse script JSON/JSONC with nlohmann ---------------------------- */

static int parse_script(const char *json_text)
{
	g_steps.clear();

	try {
		json script = json::parse(json_text, nullptr, true, true);
		if (!script.is_array()) {
			LOGE("Script must be a JSON array");
			return 0;
		}

		for (const auto &step_json : script) {
			/* Skip metadata elements (e.g. _info used by the test runner) */
			if (step_json.contains("_info"))
				continue;

			auto_step s;

			std::string action = step_json.value("action", "");

			if (action == "key") s.type = STEP_KEY;
			else if (action == "wait_ms") s.type = STEP_WAIT_MS;
			else if (action == "wait_for") s.type = STEP_WAIT_FOR;
			else if (action == "introspect") s.type = STEP_INTROSPECT;
			else if (action == "log") s.type = STEP_LOG;
			else if (action == "assert") s.type = STEP_ASSERT;
			else if (action == "select") s.type = STEP_SELECT;
			else if (action == "select_non_base_mission") {
				s.type = STEP_SELECT;
				s.select_non_base_mission = true;
			} else if (action == "select_mission") {
				s.type = STEP_SELECT_MISSION;
				s.select_mission = true;
			} else if (action == "send_axis") s.type = STEP_SEND_AXIS;
			else if (action == "send_button") s.type = STEP_SEND_BUTTON;
			else if (action == "send_touch_tap") s.type = STEP_SEND_TOUCH_TAP;
			else if (action == "send_touch_down") s.type = STEP_SEND_TOUCH_DOWN;
			else if (action == "send_touch_up") s.type = STEP_SEND_TOUCH_UP;
			else if (action == "request_screen_advance") s.type = STEP_REQUEST_SCREEN_ADVANCE;
			else if (action == "meta_action") s.type = STEP_META_ACTION;
			else if (action == "select_radial") s.type = STEP_SELECT_RADIAL;
			else if (action == "skip_intro") s.type = STEP_SKIP_INTRO;
			else if (action == "skip_briefing") s.type = STEP_SKIP_BRIEFING;
			else if (action == "assert_overlay") s.type = STEP_ASSERT_OVERLAY;
			else if (action == "face_view") s.type = STEP_FACE_VIEW;
			else if (action == "face_first_merged") s.type = STEP_FACE_FIRST_MERGED;
			else if (action == "pose_view") s.type = STEP_POSE_VIEW;
			else if (action == "pose_route_guidance") s.type = STEP_POSE_ROUTE_GUIDANCE;
			else if (action == "probe_crosshair") s.type = STEP_PROBE_CROSSHAIR;
			else if (action == "assert_probe_match") s.type = STEP_ASSERT_PROBE_MATCH;
			else if (action == "probe_gles_vbo") s.type = STEP_PROBE_GLES_VBO;
			else if (action == "enter_launcher") s.type = STEP_ENTER_LAUNCHER;
			else if (action == "enter_game") s.type = STEP_ENTER_GAME;
			else if (action == "setup_command") s.type = STEP_SETUP_COMMAND;
			else if (action == "reset_state") s.type = STEP_RESET_STATE;
			else if (action == "clear_mods") s.type = STEP_CLEAR_MODS;
			else if (action == "import_mission_zip") s.type = STEP_IMPORT_MISSION_ZIP;
			else if (action == "analyze_level_metadata") s.type = STEP_ANALYZE_LEVEL_METADATA;
			else if (action == "analyze_level_metadata_all") s.type = STEP_ANALYZE_LEVEL_METADATA_ALL;
			else if (action == "trigger_endlevel") s.type = STEP_TRIGGER_ENDLEVEL;
			else if (action == "trigger_levelcomplete") s.type = STEP_TRIGGER_LEVELCOMPLETE;
			else if (action == "trigger_postlevel") s.type = STEP_TRIGGER_POSTLEVEL;
			else if (action == "music_control") s.type = STEP_MUSIC_CONTROL;
			else if (action == "redbook_invalidate_source") s.type = STEP_REDBOOK_INVALIDATE_SOURCE;
			else if (action == "write_config") s.type = STEP_WRITE_CONFIG;
			else if (action == "tap_button") s.type = STEP_TAP_BUTTON;
			else if (action == "assert_button") s.type = STEP_ASSERT_BUTTON;
			else if (action == "assert_controller_match") s.type = STEP_ASSERT_CONTROLLER_MATCH;
			else if (action == "assert_mission_list_has_non_base") s.type = STEP_ASSERT_MISSION_LIST_HAS_NON_BASE;
			else if (action == "set_debug") s.type = STEP_SET_DEBUG;
			else if (action == "set_secret_reveal") s.type = STEP_SET_SECRET_REVEAL;
			else if (action == "set_objective_overlay") s.type = STEP_SET_OBJECTIVE_OVERLAY;
			else {
				LOGE("Unknown action: %s", action.c_str());
				continue;
			}

			s.key_name = step_json.value("key", "");
			s.modifier_name = step_json.value("modifier", "");
			s.post_delay_ms = step_json.value("post_delay_ms", step_json.value("ms", 300));
			s.field = step_json.value("field", "");
			s.value = step_json.value("value", "");
			if (s.type == STEP_REQUEST_SCREEN_ADVANCE)
				s.value = step_json.value("generation", "current");
			s.timeout_ms = step_json.value("timeout_ms", 0);
			s.message = step_json.value("message", "");
			s.label = step_json.value("label", "");
			s.select_text = step_json.value("text", "");
			s.radial_menu = step_json.value("menu", "");
			s.optional = step_json.value("optional", false);
			s.axis_id = step_json.value("axis", -1);
			s.axis_value = step_json.value("axis_value", 0.0f);
			s.axis_touch_source = step_json.value("touch_source", false);
			s.axis_saturate_sdl_queue = step_json.value("saturate_sdl_queue", false);
			s.axis_use_production_path = step_json.value("use_production_path", false);
			s.axis_pulse_before_drain = step_json.value("pulse_before_drain", false);
			s.button_id = step_json.value("button", -1);
			s.button_held = step_json.value("held", 0);
			s.button_pressed = step_json.value("pressed", 1);
			s.meta_action_id = step_json.value("id", -1);
			s.music_operation = step_json.value("operation", "");
			s.music_source = step_json.value("source", "");
			s.music_track = step_json.value("track", -1);
			s.music_last_track = step_json.value("last_track", -1);
			s.music_expect_success = step_json.value("expect_success", true);
			s.segment = step_json.value("segment", -1);
			s.side = step_json.value("side", -1);
			s.face = step_json.value("face", 0);
			s.distance = step_json.value("distance", 4.0f);
			s.pos_x = step_json.value("x", 0.0f);
			s.pos_y = step_json.value("y", 0.0f);
			s.pos_z = step_json.value("z", 0.0f);
			s.pitch = step_json.value("pitch", 0);
			s.bank = step_json.value("bank", 0);
			s.heading = step_json.value("heading", 0);
			s.enabled = step_json.value("enabled", false);
			if (s.type == STEP_SET_OBJECTIVE_OVERLAY)
				s.value = step_json.value("mode", "off");
			s.hot_xy_tolerance = step_json.value("hot_xy_tolerance", 0.02f);
			s.require_hash_match = step_json.value("require_hash_match", 0);
			s.max_mean_luma_diff = step_json.value("max_mean_luma_diff", 0.0f);

			/* STEP_ASSERT_PROBE_MATCH: two labels to compare */
			if (step_json.contains("labels") && step_json["labels"].is_array() &&
			    step_json["labels"].size() >= 2) {
				s.match_label_a = step_json["labels"][0].get<std::string>();
				s.match_label_b = step_json["labels"][1].get<std::string>();
			}

			/* Parse "expect" object for STEP_ASSERT */
			if (step_json.contains("expect") && step_json["expect"].is_object()) {
				for (auto &[key, val] : step_json["expect"].items()) {
					assert_expect ae;
					ae.key = key;
					if (val.is_object()) {
						/* Comparison operator: {"gt": 0}, {"ne": 0}, {"range": [1,10]}, {"eq": "3"} */
						for (auto &[op, opval] : val.items()) {
							ae.op = op;
							if (op == "contains_object" && opval.is_object()) {
								ae.object_value = opval;
							} else if (op == "range" && opval.is_array() && opval.size() == 2) {
								ae.range_min = opval[0].get<double>();
								ae.range_max = opval[1].get<double>();
							} else if (opval.is_number()) {
								ae.num_value = opval.get<double>();
							} else if (opval.is_string()) {
								ae.value = opval.get<std::string>();
								/* Also parse as number if possible */
								try {
									ae.num_value = std::stod(ae.value);
								} catch (...) {
								}
							} else {
								ae.value = opval.dump();
							}
							break; /* only first operator */
						}
					} else {
						ae.op = "eq";
						if (val.is_string()) {
							ae.value = val.get<std::string>();
						} else if (val.is_boolean()) {
							ae.value = val.get<bool>() ? "true" : "false";
						} else if (val.is_number_integer()) {
							ae.value = std::to_string(val.get<int>());
						} else {
							ae.value = val.dump();
						}
					}
					s.expects.push_back(std::move(ae));
				}
			}

			g_steps.push_back(std::move(s));
		}
	} catch (const json::parse_error &e) {
		LOGE("JSON parse error: %s", e.what());
		return 0;
	} catch (const std::exception &e) {
		LOGE("Script parse error: %s", e.what());
		return 0;
	}

	LOGI("Parsed %d automation steps", (int) g_steps.size());
	return (int) g_steps.size();
}

/* -- Load script from file -------------------------------------------- */

static int load_script_file(const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f) {
		LOGE("Cannot open script file: %s", path);
		return 0;
	}

	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (len <= 0 || len > 1024 * 1024) {
		LOGE("Script file size invalid: %ld", len);
		fclose(f);
		return 0;
	}

	char *buf = (char *) malloc(len + 1);
	if (!buf) {
		fclose(f);
		return 0;
	}

	size_t nread = fread(buf, 1, len, f);
	fclose(f);
	buf[nread] = '\0';

	int result = parse_script(buf);
	free(buf);
	return result;
}

/* -- Assert: check introspection JSON values ------------------------- */

static std::string json_assert_value_string(const json &val)
{
	if (val.is_number_integer())
		return std::to_string(val.get<int64_t>());
	if (val.is_number()) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "%.1f", val.get<double>());
		return tmp;
	}
	if (val.is_boolean())
		return val.get<bool>() ? "true" : "false";
	if (val.is_string())
		return val.get<std::string>();
	if (val.is_null())
		return "null";
	return val.dump();
}

static bool json_assert_scalar_matches(const json &actual, const json &expected)
{
	if (expected.is_object() && expected.size() == 1) {
		auto it = expected.begin();
		const std::string op = it.key();
		const json &opval = it.value();
		if (opval.is_number() && (op == "gt" || op == "lt" || op == "gte" || op == "lte")) {
			if (!actual.is_number())
				return false;
			double a = actual.get<double>();
			double e = opval.get<double>();
			if (op == "gt")
				return a > e;
			if (op == "lt")
				return a < e;
			if (op == "gte")
				return a >= e;
			return a <= e;
		}
		if (op == "contains" && opval.is_string())
			return icontains(json_assert_value_string(actual).c_str(), opval.get<std::string>().c_str());
	}
	return strcasecmp(json_assert_value_string(actual).c_str(),
	                  json_assert_value_string(expected).c_str()) == 0;
}

static bool json_array_contains_object(const json &array, const json &expected)
{
	if (!array.is_array() || !expected.is_object())
		return false;
	for (const auto &elem : array) {
		if (!elem.is_object())
			continue;
		bool matches = true;
		for (auto &[key, val] : expected.items()) {
			if (!elem.contains(key) || !json_assert_scalar_matches(elem[key], val)) {
				matches = false;
				break;
			}
		}
		if (matches)
			return true;
	}
	return false;
}

/*
 * Run all assertions for a STEP_ASSERT or expectation-backed STEP_WAIT_FOR.
 * Parses introspection JSON with nlohmann and checks expected values.
 * Returns empty string on success, or a failure description on first failure.
 */
static std::string run_assertions(auto_step &s, bool log_success, bool log_failure)
{
	char *json_str = game_introspect_get_state();
	if (!json_str) {
		if (log_failure)
			LOGE("ASSERT_FAIL: Could not get introspection state");
		return "could not get introspection state";
	}

	json state;
	try {
		state = json::parse(json_str);
	} catch (const std::exception &e) {
		if (log_failure)
			LOGE("ASSERT_FAIL: Failed to parse introspection JSON: %s", e.what());
		free(json_str);
		std::string msg = "parse error: ";
		msg += e.what();
		return msg;
	}
	free(json_str);

	for (const auto &ae : s.expects) {
		/* Navigate dot-path keys like "joystick_controls.items[27].value".
		 * Splits on '.' segments; each segment may have a [N] array index. */
		const json *cur = &state;
		std::string path_remaining = ae.key;
		bool path_ok = true;
		while (!path_remaining.empty() && path_ok) {
			std::string seg;
			auto dot = path_remaining.find('.');
			if (dot != std::string::npos) {
				seg = path_remaining.substr(0, dot);
				path_remaining = path_remaining.substr(dot + 1);
			} else {
				seg = path_remaining;
				path_remaining.clear();
			}
			/* Check for array index: "items[27]" -> key="items", idx=27 */
			auto bracket = seg.find('[');
			int arr_idx = -1;
			if (bracket != std::string::npos) {
				arr_idx = atoi(seg.c_str() + bracket + 1);
				seg = seg.substr(0, bracket);
			}
			if (!seg.empty()) {
				if (!cur->is_object() || !cur->contains(seg)) {
					path_ok = false;
					break;
				}
				cur = &(*cur)[seg];
			}
			if (arr_idx >= 0) {
				if (!cur->is_array() || arr_idx >= (int) cur->size()) {
					path_ok = false;
					break;
				}
				cur = &(*cur)[arr_idx];
			}
		}
		if (!path_ok) {
			if (log_failure)
				LOGE("ASSERT_FAIL: key \"%s\" not found in introspection JSON", ae.key.c_str());
			std::string msg = "key \"";
			msg += ae.key;
			msg += "\" not found";
			return msg;
		}

		const auto &val = *cur;
		double actual_num = 0;
		bool is_num = false;
		std::string actual_str;

		if (val.is_number()) {
			actual_num = val.get<double>();
			is_num = true;
			if (val.is_number_integer())
				actual_str = std::to_string(val.get<int64_t>());
			else {
				char tmp[64];
				snprintf(tmp, sizeof(tmp), "%.1f", actual_num);
				actual_str = tmp;
			}
		} else if (val.is_boolean()) {
			actual_str = val.get<bool>() ? "true" : "false";
			actual_num = val.get<bool>() ? 1.0 : 0.0;
			is_num = true;
		} else if (val.is_string()) {
			actual_str = val.get<std::string>();
		} else if (val.is_null()) {
			actual_str = "null";
		} else {
			actual_str = val.dump();
		}

		bool pass = false;
		char desc[256];

		if (ae.op == "eq") {
			pass = (strcasecmp(actual_str.c_str(), ae.value.c_str()) == 0);
			snprintf(desc, sizeof(desc), "\"%s\" == \"%s\" (got \"%s\")",
			         ae.key.c_str(), ae.value.c_str(), actual_str.c_str());
		} else if (ae.op == "ne") {
			pass = is_num ? (actual_num != ae.num_value) : (actual_str != ae.value);
			snprintf(desc, sizeof(desc), "\"%s\" != %.4g (got %s)",
			         ae.key.c_str(), ae.num_value, actual_str.c_str());
		} else if (ae.op == "gt") {
			pass = is_num && actual_num > ae.num_value;
			snprintf(desc, sizeof(desc), "\"%s\" > %.4g (got %s)",
			         ae.key.c_str(), ae.num_value, actual_str.c_str());
		} else if (ae.op == "lt") {
			pass = is_num && actual_num < ae.num_value;
			snprintf(desc, sizeof(desc), "\"%s\" < %.4g (got %s)",
			         ae.key.c_str(), ae.num_value, actual_str.c_str());
		} else if (ae.op == "gte") {
			pass = is_num && actual_num >= ae.num_value;
			snprintf(desc, sizeof(desc), "\"%s\" >= %.4g (got %s)",
			         ae.key.c_str(), ae.num_value, actual_str.c_str());
		} else if (ae.op == "lte") {
			pass = is_num && actual_num <= ae.num_value;
			snprintf(desc, sizeof(desc), "\"%s\" <= %.4g (got %s)",
			         ae.key.c_str(), ae.num_value, actual_str.c_str());
		} else if (ae.op == "range") {
			pass = is_num && actual_num >= ae.range_min && actual_num <= ae.range_max;
			snprintf(desc, sizeof(desc), "\"%s\" in [%.4g, %.4g] (got %s)",
			         ae.key.c_str(), ae.range_min, ae.range_max, actual_str.c_str());
		} else if (ae.op == "contains") {
			/* For arrays: any element substring-matches ae.value.
			 * For strings: substring match. */
			if (val.is_array()) {
				for (const auto &elem : val) {
					std::string es = elem.is_string() ? elem.get<std::string>() : elem.dump();
					if (icontains(es.c_str(), ae.value.c_str())) {
						pass = true;
						break;
					}
				}
			} else {
				pass = icontains(actual_str.c_str(), ae.value.c_str());
			}
			snprintf(desc, sizeof(desc), "\"%s\" contains \"%s\" (got %s)",
			         ae.key.c_str(), ae.value.c_str(), actual_str.c_str());
		} else if (ae.op == "contains_object") {
			pass = json_array_contains_object(val, ae.object_value);
			snprintf(desc, sizeof(desc), "\"%s\" contains object %s (got %s)",
			         ae.key.c_str(), ae.object_value.dump().c_str(), actual_str.c_str());
		} else {
			snprintf(desc, sizeof(desc), "\"%s\": unknown op \"%s\"",
			         ae.key.c_str(), ae.op.c_str());
		}

		if (pass && log_success) {
			LOGI("ASSERT_PASS: %s", desc);
		} else if (!pass) {
			if (log_failure) {
				LOGE("ASSERT_FAIL: %s", desc);
				/* Dump key diagnostic fields for post-mortem analysis */
				auto dump_field = [&](const char *name) {
					if (state.contains(name)) {
						auto &v = state[name];
						if (v.is_number_integer())
							LOGE("  DIAG %s = %d", name, v.get<int>());
						else if (v.is_string())
							LOGE("  DIAG %s = \"%s\"", name, v.get<std::string>().c_str());
					}
				};
				dump_field("control_type");
				dump_field("heading_time");
				dump_field("pitch_time");
				dump_field("bank_time");
				dump_field("slide_on_state");
				dump_field("bank_on_state");
				dump_field("screen_mode");
				dump_field("in_game");
				if (state.contains("raw_joy_axis") && state["raw_joy_axis"].is_array()) {
					std::string axes;
					for (auto &v : state["raw_joy_axis"])
						axes += std::to_string(v.get<int>()) + " ";
					LOGE("  DIAG raw_joy_axis = [%s]", axes.c_str());
				}
				if (state.contains("joy_buttons") && state["joy_buttons"].is_array()) {
					std::string buttons;
					for (auto &v : state["joy_buttons"])
						buttons += std::to_string(v.get<int>()) + " ";
					LOGE("  DIAG joy_buttons = [%s]", buttons.c_str());
				}
			}
			return std::string(desc);
		}
	}

	return std::string();
}

/* -- Advance to next step --------------------------------------------- */

static void stop_script_fail(const char *reason)
{
	LOGE("SCRIPT_RESULT: FAIL at step %d/%d -- %s",
	     g_current_step + 1, (int) g_steps.size(), reason);
	clear_held_axes();
	write_result_file("FAIL", reason);
	log_append("script", "FAIL", reason ? reason : "");
	if (g_log_fp) {
		fclose(g_log_fp);
		g_log_fp = NULL;
	}
	g_active = 0;
	g_failed = 1;
}

static void advance_step(void)
{
	g_current_step++;
	g_step_start = SDL_GetTicks();
	g_key_phase = 0;
	g_repeat_start = 0;
	g_skip_briefing_last_front = NULL;
	g_skip_briefing_last_game_wind_seen = -1;
	g_select_phase = 0;
	g_select_delta = 0;
	g_inject_axis_logged = 0;

	if (g_current_step >= (int) g_steps.size()) {
		LOGI("SCRIPT_RESULT: PASS (%d steps)", (int) g_steps.size());
		clear_held_axes();
		write_result_file("PASS", NULL);
		log_append("script", "PASS", "");
		if (g_log_fp) {
			fclose(g_log_fp);
			g_log_fp = NULL;
		}
		g_active = 0;
		g_failed = 0;
	} else {
		auto &s = g_steps[g_current_step];
		LOGI("Step %d/%d: type=%d", g_current_step + 1, (int) g_steps.size(), (int) s.type);
		log_append(step_type_name(s.type), "start", "");
	}
}

/* -- Public API ------------------------------------------------------- */

extern "C" void game_automate_set_path(const char *dir_path)
{
	if (dir_path) {
		strncpy(g_automate_dir, dir_path, sizeof(g_automate_dir) - 1);
		g_automate_dir[sizeof(g_automate_dir) - 1] = '\0';
		LOGI("Automation dir set: %s", g_automate_dir);
	}
}

extern "C" void game_automate_load_script(const char *script_path, int start_step,
                                          const char *run_id)
{
	if (!script_path)
		return;

	{
		const std::lock_guard<std::mutex> lock(g_pending_load_mutex);
		strncpy(g_pending_load.script_path, script_path, sizeof(g_pending_load.script_path) - 1);
		g_pending_load.script_path[sizeof(g_pending_load.script_path) - 1] = '\0';
		strncpy(g_pending_load.run_id, run_id ? run_id : "", sizeof(g_pending_load.run_id) - 1);
		g_pending_load.run_id[sizeof(g_pending_load.run_id) - 1] = '\0';
		g_pending_load.start_step = start_step;
		g_load_requested.store(true, std::memory_order_release);
	}
	LOGI("Script load requested: %s start_step=%d run_id=%s", script_path, start_step,
	     run_id ? run_id : "");
}

extern "C" int game_automate_get_axis_probe(game_automate_axis_probe *out_probe)
{
	if (!out_probe)
		return 0;
	*out_probe = g_axis_probe;
	return g_axis_probe.valid;
}

extern "C" void game_automate_axis_dispatch_begin(unsigned long long generation,
                                                  int event_axis,
                                                  int event_raw_value,
                                                  int event_touch_source)
{
	g_axis_dispatch_active = generation >= g_axis_probe.first_generation &&
	                         generation <= g_axis_probe.generation &&
	                         event_axis == g_axis_probe.axis &&
	                         (g_axis_probe.first_generation != g_axis_probe.generation ||
	                          (event_raw_value == g_axis_probe.raw_value &&
	                           (event_touch_source != 0) ==
	                               (g_axis_probe.touch_source != 0)));
}

extern "C" void game_automate_axis_dispatch_end(void)
{
	g_axis_dispatch_active = 0;
}

extern "C" void game_automate_axis_button_edge(int pressed)
{
	if (!g_axis_dispatch_active)
		return;
	if (pressed)
		++g_axis_probe.axis_button_down_edges;
	else
		++g_axis_probe.axis_button_up_edges;
}

extern "C" void game_automate_observe_axis_controls(
    int pitch_time, int heading_time, int slide_lr_time, int slide_ud_time,
    int bank_time, int throttle_time)
{
	if (!g_axis_dispatch_active)
		return;
	g_axis_probe.valid = 1;
	g_axis_probe.processed = 1;
	++g_axis_probe.sample_count;
	g_axis_probe.pitch_time = pitch_time;
	g_axis_probe.heading_time = heading_time;
	g_axis_probe.slide_lr_time = slide_lr_time;
	g_axis_probe.slide_ud_time = slide_ud_time;
	g_axis_probe.bank_time = bank_time;
	g_axis_probe.throttle_time = throttle_time;
}

extern "C" void game_automate_tick(void)
{
	/* Handle pending load request (from JNI thread) */
	if (g_load_requested.load(std::memory_order_acquire)) {
		automation_load_request request;
		{
			const std::lock_guard<std::mutex> lock(g_pending_load_mutex);
			request = g_pending_load;
			g_load_requested.store(false, std::memory_order_relaxed);
		}

		strncpy(g_active_script, request.script_path, sizeof(g_active_script) - 1);
		g_active_script[sizeof(g_active_script) - 1] = '\0';
		strncpy(g_active_run_id, request.run_id, sizeof(g_active_run_id) - 1);
		g_active_run_id[sizeof(g_active_run_id) - 1] = '\0';
		LOGI("Loading automation script: %s start_step=%d run_id=%s", g_active_script,
		     request.start_step, g_active_run_id);

		remove_stale_result();
		open_log_file();
		clear_held_axes();

		if (load_script_file(g_active_script)) {
			g_current_step = request.start_step;
			g_step_start = SDL_GetTicks();
			g_script_start = g_step_start;
			memset(&g_axis_probe, 0, sizeof(g_axis_probe));
			g_key_phase = 0;
			g_select_phase = 0;
			g_select_delta = 0;
			g_active = 1;
			g_failed = 0;
			LOGI("Script started: %d steps (from step %d)", (int) g_steps.size(), g_current_step);

			if (g_current_step < (int) g_steps.size()) {
				auto &s = g_steps[g_current_step];
				LOGI("Step %d/%d: type=%d", g_current_step + 1, (int) g_steps.size(), (int) s.type);
				log_append(step_type_name(s.type), "start", "");
			}
		}
	}

	if (!g_active || g_current_step >= (int) g_steps.size())
		return;

	auto &s = g_steps[g_current_step];
	Uint32 now = SDL_GetTicks();
	Uint32 elapsed = now - g_step_start;

	switch (s.type) {
		case STEP_KEY:
			if (g_key_phase == 0) {
				if (!s.modifier_name.empty())
					inject_key_combo(s.modifier_name, s.key_name);
				else {
					int keycode = lookup_key_command(s.key_name.c_str());
					if (keycode >= 0 && can_direct_dispatch_front_key_command()) {
						g_key_phase = 1;
						g_step_start = now;
						if (select_dispatch_front_menu_key(keycode, s.key_name.c_str()))
							break;
						g_key_phase = 0;
					}
					inject_key_tap(s.key_name);
				}
				g_key_phase = 1;
				g_step_start = now;
			} else if (g_key_phase == 1) {
				if (elapsed >= (Uint32) s.post_delay_ms) {
					advance_step();
				}
			}
			break;

		case STEP_WAIT_MS:
			if (elapsed >= (Uint32) s.post_delay_ms) {
				LOGI("Wait completed: %d ms", s.post_delay_ms);
				log_append("wait_ms", "done", "");
				advance_step();
			}
			break;

		case STEP_WAIT_FOR:
			if (!s.expects.empty() ? run_assertions(s, true, false).empty() : check_condition(s.field, s.value)) {
				if (!s.expects.empty())
					LOGI("Condition met: expectations satisfied (after %u ms)", elapsed);
				else
					LOGI("Condition met: %s = %s (after %u ms)", s.field.c_str(), s.value.c_str(), elapsed);
				log_append("wait_for", "done", !s.expects.empty() ? "expect" : s.field.c_str());
				advance_step();
			} else if (s.timeout_ms > 0 && elapsed >= (Uint32) s.timeout_ms) {
				char reason[256];
				if (!s.expects.empty()) {
					std::string fail_desc = run_assertions(s, false, true);
					snprintf(reason, sizeof(reason), "TIMEOUT waiting for expectations: %s (after %d ms)",
					         fail_desc.c_str(), s.timeout_ms);
				} else {
					snprintf(reason, sizeof(reason), "TIMEOUT waiting for %s = %s (after %d ms)",
					         s.field.c_str(), s.value.c_str(), s.timeout_ms);
				}
				log_append("wait_for", "timeout", !s.expects.empty() ? "expect" : s.field.c_str());
				stop_script_fail(reason);
			}
			break;

		case STEP_INTROSPECT:
			LOGI("Triggering introspection dump");
			game_introspect_request();
			advance_step();
			break;

		case STEP_LOG:
			LOGI("SCRIPT: %s", s.message.c_str());
			advance_step();
			break;

		case STEP_ASSERT: {
			std::string fail_desc = run_assertions(s, true, true);
			if (fail_desc.empty()) {
				log_append("assert", "pass", "");
				advance_step();
			} else {
				log_append("assert", "fail", fail_desc.c_str());
				std::string reason = "assert: " + fail_desc;
				stop_script_fail(reason.c_str());
			}
			break;
		}

		case STEP_ASSERT_MISSION_LIST_HAS_NON_BASE: {
			int target, current;
			if (!select_find_first_non_base_mission(&target, &current, NULL)) {
				if (s.timeout_ms > 0 && elapsed < (Uint32) s.timeout_ms)
					break;
				stop_script_fail("assert_mission_list_has_non_base: no non-base mission found");
				break;
			}
			LOGI("ASSERT_PASS: non-base mission found at index %d (current=%d)", target, current);
			log_append("assert_mission_list_has_non_base", "pass", "");
			advance_step();
			break;
		}

		case STEP_SELECT:
		case STEP_SELECT_MISSION:
			if (g_select_phase == 0) {
				/* Phase 0: find the target item and compute navigation delta.
				 * If timeout_ms is set, poll each frame until the item appears
				 * instead of failing immediately. */
				int target, current;
				if (s.select_mission && front_is_start_level_menu()) {
					log_append("select_mission", "skip", "start_level_menu");
					advance_step();
					break;
				}
				bool found = select_find_target(s, &target, &current, NULL);
				if (!found) {
					if (s.timeout_ms > 0 && elapsed < (Uint32) s.timeout_ms) {
						break; /* retry next frame */
					}
					if (s.optional) {
						con_printf(CON_DEBUG, "SELECT: optional item \"%s\" not found, skipping",
						           s.select_text.c_str());
						advance_step();
						break;
					}
					char reason[256];
					snprintf(reason, sizeof(reason),
					         "SELECT: item \"%s\" not found in menu%s",
					         s.select_text.c_str(),
					         s.timeout_ms > 0 ? " (timed out)" : "");
					stop_script_fail(reason);
					break;
				}
				g_select_delta = target - current;
				g_select_phase = 1;
			}
			if (g_select_phase == 1) {
				/*
				 * Phase 1: re-read the live citem each frame and inject
				 * one navigation key.  This handles menus where
				 * newmenu_scroll() skips NM_TYPE_TEXT items, so a single
				 * DOWN press may advance by more than one index.
				 */
				int target, current, current_type;
				bool found = select_find_target(s, &target, &current, &current_type);
				if (!found) {
					stop_script_fail("SELECT: menu disappeared during navigation");
					break;
				}
				int delta = target - current;
				if (select_can_confirm_current_input_as_ok(s, delta, current_type)) {
					LOGI("SELECT: confirming current input for adjacent \"Ok\"");
					g_select_phase = 3;
					g_step_start = now;
					if (!select_dispatch_front_menu_key(KEY_ENTER, "enter"))
						inject_key_tap("enter");
					break;
				}
				if (delta > 0) {
					if (!select_dispatch_front_menu_key(KEY_DOWN, "down"))
						inject_key_tap("down");
				} else if (delta < 0) {
					if (!select_dispatch_front_menu_key(KEY_UP, "up"))
						inject_key_tap("up");
				} else {
					g_select_phase = 3;
					g_step_start = now;
					if (!select_dispatch_front_menu_key(KEY_ENTER, "enter"))
						inject_key_tap("enter");
					LOGI("SELECT: confirmed \"%s\"", s.select_text.c_str());
					break;
				}
				/* else: keep navigating next frame */
			} else if (g_select_phase == 2) {
				/* Phase 2: re-verify position, then press enter.
				 * The cursor can drift between Phase 1 and 2 if the
				 * game loop stalls (e.g. emulator lag). */
				int target, current;
				bool found = select_find_target(s, &target, &current, NULL);
				if (!found) {
					stop_script_fail("SELECT: menu disappeared before confirm");
					break;
				}
				if (target != current) {
					LOGI("SELECT: cursor drifted to %d (target %d), re-navigating", current, target);
					g_select_phase = 1;
					break;
				}
				g_select_phase = 3;
				g_step_start = now;
				if (!select_dispatch_front_menu_key(KEY_ENTER, "enter"))
					inject_key_tap("enter");
				LOGI("SELECT: confirmed \"%s\"", s.select_text.c_str());
			} else if (g_select_phase == 3) {
				/* Phase 3: wait for the key to be processed before advancing */
				if (elapsed >= (Uint32) s.post_delay_ms) {
					advance_step();
				}
			}
			break;

		case STEP_SEND_AXIS:
			if (g_key_phase == 0 && s.axis_id >= 0 && s.axis_id < 8) {
				if (!s.axis_use_production_path)
					set_held_axis(s.axis_id, s.axis_value, s.axis_touch_source);
				inject_axis(s.axis_id, s.axis_value, s.axis_touch_source,
				            s.axis_saturate_sdl_queue,
				            s.axis_use_production_path,
				            s.axis_pulse_before_drain);
				if (s.post_delay_ms <= 0) {
					advance_step();
				} else {
					g_key_phase = 1;
					g_step_start = now;
				}
			} else if (g_key_phase == 1) {
				if (elapsed >= (Uint32) s.post_delay_ms) {
					advance_step();
				}
			}
			break;

		case STEP_SEND_BUTTON:
			if (!s.button_pressed) {
				/* Release-only mode */
				inject_button(s.button_id, 0);
				advance_step();
			} else if (!s.button_held && s.post_delay_ms <= 100 && s.button_id >= 0) {
				/* Short taps need to land within one frame; otherwise slow emulator
				 * frame cadence stretches a 50 ms pulse into a multi-second hold. */
				inject_button(s.button_id, 1);
				inject_button(s.button_id, 0);
				advance_step();
			} else if (g_key_phase == 0 && s.button_id >= 0) {
				inject_button(s.button_id, 1); /* press */
				g_key_phase = 1;
				g_step_start = now;
			} else if (g_key_phase == 1) {
				if (s.button_held || elapsed >= (Uint32) s.post_delay_ms) {
					if (!s.button_held)
						inject_button(s.button_id, 0); /* release */
					advance_step();
				}
			}
			break;

		case STEP_SEND_TOUCH_TAP:
#ifdef ANDROID
			android_test_inject_touch_tap();
			advance_step();
#else
			stop_script_fail("send_touch_tap: Android-only action");
#endif
			break;

		case STEP_SEND_TOUCH_DOWN:
#ifdef ANDROID
			android_test_inject_touch_action(0);
			advance_step();
#else
			stop_script_fail("send_touch_down: Android-only action");
#endif
			break;

		case STEP_SEND_TOUCH_UP:
#ifdef ANDROID
			android_test_inject_touch_action(2);
			advance_step();
#else
			stop_script_fail("send_touch_up: Android-only action");
#endif
			break;

		case STEP_REQUEST_SCREEN_ADVANCE:
#ifdef ANDROID
		{
			unsigned int generation = android_screen_advance_get_generation();
			if (s.value == "previous")
				generation--;
			else if (s.value != "current") {
				stop_script_fail("request_screen_advance: generation must be current or previous");
				break;
			}
			android_screen_advance_request(generation);
			advance_step();
			break;
		}
#else
			stop_script_fail("request_screen_advance: Android-only action");
#endif
		break;

		case STEP_META_ACTION:
			if (!s.button_pressed) {
				meta_action_dispatch(s.meta_action_id, 0);
				advance_step();
			} else if (g_key_phase == 0 && s.meta_action_id >= 0) {
				meta_action_dispatch(s.meta_action_id, 1);
				g_key_phase = 1;
				g_step_start = now;
			} else if (g_key_phase == 1) {
				if (s.button_held || elapsed >= (Uint32) s.post_delay_ms) {
					if (!s.button_held)
						meta_action_dispatch(s.meta_action_id, 0);
					advance_step();
				}
			}
			break;

		case STEP_SELECT_RADIAL:
#ifdef ANDROID
			if (s.radial_menu.empty() || s.select_text.empty()) {
				stop_script_fail("select_radial: menu and text are required");
			} else if (!automation_select_radial(s.radial_menu.c_str(), s.select_text.c_str())) {
				char reason[256];
				snprintf(reason, sizeof(reason),
				         "select_radial: no visible item matching menu=\"%s\" text=\"%s\"",
				         s.radial_menu.c_str(), s.select_text.c_str());
				stop_script_fail(reason);
			} else {
				LOGI("SELECT_RADIAL: selected menu=\"%s\" text=\"%s\"",
				     s.radial_menu.c_str(), s.select_text.c_str());
				advance_step();
			}
#else
			stop_script_fail("select_radial: Android-only action");
#endif
			break;

		case STEP_SKIP_INTRO:
			if (!g_intro_active) {
				LOGI("skip_intro: intro inactive, done");
				advance_step();
				break;
			}
			if (s.timeout_ms > 0 && elapsed >= (Uint32) s.timeout_ms) {
				log_append("skip_intro", "timeout", "intro_active");
				stop_script_fail("skip_intro: timed out waiting for intro to clear");
				break;
			}
			if (g_key_phase == 0) {
				if (s.button_id >= 0) {
					inject_button(s.button_id, 1);
					inject_button(s.button_id, 0);
				} else
					inject_mouse_tap();
				g_key_phase = 1;
				g_repeat_start = now;
			} else if (now - g_repeat_start >= (Uint32) s.post_delay_ms) {
				g_key_phase = 0;
			}
			break;

		case STEP_SKIP_BRIEFING:
			/* Poll each frame until the game window is front, dismissing
			 * any intervening windows (briefing, movie) with escape.
			 * ShowLevelIntro blocks until the briefing closes, so Game_wind
			 * won't exist until we dismiss the briefing first. */
			{
				window *front = window_get_front();
				int game_wind_seen = Game_wind != NULL ? 1 : 0;
				if (front != g_skip_briefing_last_front ||
				    game_wind_seen != g_skip_briefing_last_game_wind_seen) {
					g_skip_briefing_last_front = front;
					g_skip_briefing_last_game_wind_seen = game_wind_seen;
					g_step_start = now;
					elapsed = 0;
				}
			}
			if (s.timeout_ms > 0 && elapsed >= (Uint32) s.timeout_ms) {
				stop_script_fail("skip_briefing: timed out waiting for game window");
				break;
			}
			if (g_key_phase == 0) {
				window *front = window_get_front();
				if (Game_wind != NULL && (front == NULL || front == Game_wind)) {
					LOGI("skip_briefing: game window is front, done");
					advance_step();
				} else if (front != NULL && front != Game_wind) {
					/* Briefing or other non-game window on top -- dismiss it. */
					LOGI("skip_briefing: dismissing non-game window (Game_wind=%s front=%s)",
					     Game_wind ? "exists" : "NULL",
					     describe_window_handler(front));
					if (!select_dispatch_front_menu_key(KEY_ESC, "escape")) {
						if (Game_wind == NULL)
							inject_mouse_tap();
						else
							inject_key_tap("escape");
					}
					g_key_phase = 1;
					g_repeat_start = now;
				} else if (Game_wind == NULL) {
					LOGI("skip_briefing: no front window yet, tapping to advance");
					inject_mouse_tap();
					g_key_phase = 1;
					g_repeat_start = now;
				}
			} else if (g_key_phase == 1) {
				/* Escape was sent; wait post_delay then re-check (phase 0). */
				if (now - g_repeat_start >= (Uint32) s.post_delay_ms) {
					window *front = window_get_front();
					if (Game_wind != NULL && (front == NULL || front == Game_wind)) {
						LOGI("skip_briefing: game window reached after dismiss");
						advance_step();
					} else {
						g_key_phase = 0;
					}
				}
			}
			break;

		case STEP_ASSERT_OVERLAY: {
			/* Search overlay ring buffer for entry matching type (field) and
			 * text substring (value).  field="" matches any type. */
			char *ov_json = overlay_ringbuf_get_json(0, 32);
			if (!ov_json) {
				log_append("assert_overlay", "fail", "could not read overlay buffer");
				stop_script_fail("assert_overlay: could not read overlay buffer");
				break;
			}
			bool found = false;
			std::string detail;
			try {
				json ov = json::parse(ov_json);
				const auto &lines = ov["lines"];
				for (const auto &entry : lines) {
					std::string etype = entry.value("type", "");
					std::string etext = entry.value("text", "");
					if (!s.field.empty() && etype != s.field)
						continue;
					if (etext.find(s.value) != std::string::npos) {
						found = true;
						detail = "matched: type=\"" + etype + "\" text=\"" + etext + "\"";
						break;
					}
				}
				if (!found) {
					int n = (int) lines.size();
					char tmp[128];
					snprintf(tmp, sizeof(tmp), "no overlay entry matching type=\"%s\" contains=\"%s\" (%d entries)",
					         s.field.c_str(), s.value.c_str(), n);
					detail = tmp;
				}
			} catch (const std::exception &e) {
				detail = std::string("parse error: ") + e.what();
			}
			free(ov_json);
			if (found) {
				LOGI("ASSERT_OVERLAY_PASS: %s", detail.c_str());
				log_append("assert_overlay", "pass", detail.c_str());
				advance_step();
			} else {
				LOGE("ASSERT_OVERLAY_FAIL: %s", detail.c_str());
				log_append("assert_overlay", "fail", detail.c_str());
				stop_script_fail(detail.c_str());
			}
			break;
		}

		case STEP_FACE_VIEW:
			if (g_key_phase == 0) {
				char reason[256];

				if (!move_player_to_face_view(s, reason, sizeof(reason))) {
					log_append("face_view", "fail", reason);
					stop_script_fail(reason);
					break;
				}
				log_append("face_view", "done", "");
				g_key_phase = 1;
				g_step_start = now;
			} else if (elapsed >= (Uint32) s.post_delay_ms) {
				advance_step();
			}
			break;

		case STEP_FACE_FIRST_MERGED:
			if (g_key_phase == 0) {
				char reason[256];

				if (!move_player_to_first_merged_face(s, reason, sizeof(reason))) {
					log_append("face_first_merged", "fail", reason);
					stop_script_fail(reason);
					break;
				}
				log_append("face_first_merged", "done", "");
				g_key_phase = 1;
				g_step_start = now;
			} else if (elapsed >= (Uint32) s.post_delay_ms) {
				advance_step();
			}
			break;

		case STEP_POSE_VIEW:
			if (g_key_phase == 0) {
				char reason[256];

				if (!move_player_to_pose(s, reason, sizeof(reason))) {
					log_append("pose_view", "fail", reason);
					stop_script_fail(reason);
					break;
				}
				log_append("pose_view", "done", "");
				g_key_phase = 1;
				g_step_start = now;
			} else if (elapsed >= (Uint32) s.post_delay_ms) {
				advance_step();
			}
			break;

		case STEP_POSE_ROUTE_GUIDANCE:
			if (g_key_phase == 0) {
				char reason[256];

				if (!move_player_to_route_guidance(reason, sizeof(reason))) {
					log_append("pose_route_guidance", "fail", reason);
					stop_script_fail(reason);
					break;
				}
				log_append("pose_route_guidance", "done", "");
				g_key_phase = 1;
				g_step_start = now;
			} else if (elapsed >= (Uint32) s.post_delay_ms) {
				advance_step();
			}
			break;

		case STEP_PROBE_CROSSHAIR: {
			const Uint32 timeout = (Uint32) (s.timeout_ms > 0 ? s.timeout_ms : 2000);

			if (g_key_phase == 0) {
				char detail[192];

				android_merged_wall_request_snapshot(MERGED_WALL_REQUEST_PROBE);
				s.request_frame = g_merged_wall_frame_id;
				g_key_phase = 1;
				g_step_start = now;
				if (!s.label.empty())
					snprintf(detail, sizeof(detail), "label=%s request_frame=%d",
					         s.label.c_str(), s.request_frame);
				else
					snprintf(detail, sizeof(detail), "request_frame=%d", s.request_frame);
				log_append("probe_crosshair", "start", detail);
			} else if (g_merged_wall_probe_result.valid &&
			           g_merged_wall_probe_result.request_frame == s.request_frame &&
			           strcmp(g_merged_wall_probe_result.status, "pending") != 0) {
				char detail[640];

				if (!strcmp(g_merged_wall_probe_result.status, "ok")) {
					snprintf(detail, sizeof(detail),
					         "status=ok seg=%d side=%d face=%d orient=%d route=%s merge_impl=%s flip=%s screen_axis=%s u_shift=%.6f v_shift=%.6f u_span=%.6f v_span=%.6f cached_anchor=%.6f/%.6f legacy_anchor=%.6f/%.6f route_agree=%d render_valid=%d render_hot=%d render_hash=0x%08x render_luma=%d..%d render_hot_xy=%.3f/%.3f",
					         g_merged_wall_probe_result.seg,
					         g_merged_wall_probe_result.side,
					         g_merged_wall_probe_result.face,
					         g_merged_wall_probe_result.orient,
					         g_merged_wall_probe_result.route,
					         g_merged_wall_probe_result.merge_impl,
					         g_merged_wall_probe_result.ovl_flip_axis,
					         g_merged_wall_probe_result.flip_screen_axis,
					         g_merged_wall_probe_result.u_shift_hint,
					         g_merged_wall_probe_result.v_shift_hint,
					         g_merged_wall_probe_result.u_span,
					         g_merged_wall_probe_result.v_span,
					         g_merged_wall_probe_result.cached_anchor_u,
					         g_merged_wall_probe_result.cached_anchor_v,
					         g_merged_wall_probe_result.legacy_anchor_u,
					         g_merged_wall_probe_result.legacy_anchor_v,
					         g_merged_wall_probe_result.route_agree,
					         g_merged_wall_probe_result.render_valid_cells,
					         g_merged_wall_probe_result.render_hot_cells,
					         g_merged_wall_probe_result.render_hash,
					         g_merged_wall_probe_result.render_luma_min,
					         g_merged_wall_probe_result.render_luma_max,
					         g_merged_wall_probe_result.render_hot_x,
					         g_merged_wall_probe_result.render_hot_y);
					log_append("probe_crosshair", "done", detail);
					store_probe_result(s.label);
					advance_step();
				} else {
					char reason[256];

					if (!s.label.empty())
						snprintf(reason, sizeof(reason), "probe_crosshair[%s]: %s",
						         s.label.c_str(), g_merged_wall_probe_result.status);
					else
						snprintf(reason, sizeof(reason), "probe_crosshair: %s",
						         g_merged_wall_probe_result.status);
					log_append("probe_crosshair", "fail", reason);
					stop_script_fail(reason);
				}
			} else if (elapsed >= timeout) {
				char reason[256];

				if (!s.label.empty())
					snprintf(reason, sizeof(reason), "probe_crosshair[%s]: timed out after %u ms",
					         s.label.c_str(), (unsigned) timeout);
				else
					snprintf(reason, sizeof(reason), "probe_crosshair: timed out after %u ms",
					         (unsigned) timeout);
				log_append("probe_crosshair", "fail", reason);
				stop_script_fail(reason);
			}
			break;
		}

		case STEP_ASSERT_PROBE_MATCH: {
			char detail[512];
			const stored_probe *a = find_stored_probe(s.match_label_a);
			const stored_probe *b = find_stored_probe(s.match_label_b);

			if (!a || !b) {
				snprintf(detail, sizeof(detail),
				         "assert_probe_match: missing probe label (%s:%s %s:%s)",
				         s.match_label_a.c_str(), a ? "ok" : "missing",
				         s.match_label_b.c_str(), b ? "ok" : "missing");
				log_append("assert_probe_match", "fail", detail);
				stop_script_fail(detail);
				break;
			}
			if (!a->render_sample_valid || !b->render_sample_valid) {
				snprintf(detail, sizeof(detail),
				         "assert_probe_match[%s vs %s]: render_sample not valid (a=%d b=%d)",
				         s.match_label_a.c_str(), s.match_label_b.c_str(),
				         a->render_sample_valid, b->render_sample_valid);
				log_append("assert_probe_match", "fail", detail);
				stop_script_fail(detail);
				break;
			}
			float dx = a->hot_x - b->hot_x;
			float dy = a->hot_y - b->hot_y;
			float adx = dx < 0 ? -dx : dx;
			float ady = dy < 0 ? -dy : dy;
			int hot_ok = (adx <= s.hot_xy_tolerance) && (ady <= s.hot_xy_tolerance);
			int hash_eq = (a->render_hash == b->render_hash);
			/* Per-cell SAD across cells valid in BOTH probes. Catches
			 * pixel-level mirror/flip even when the centroid of hot cells
			 * happens to land in the same place on symmetric overlays. */
			long sad_sum = 0;
			int sad_count = 0;
			for (int i = 0; i < MERGED_WALL_PROBE_RENDER_SAMPLE_COUNT; i++) {
				if (!a->mask[i] || !b->mask[i])
					continue;
				int d = (int) a->luma[i] - (int) b->luma[i];
				sad_sum += (d < 0 ? -d : d);
				sad_count++;
			}
			float mean_luma_diff = sad_count > 0 ? (float) sad_sum / (float) sad_count : 0.0f;
			int sad_ok = 1;
			if (s.max_mean_luma_diff > 0.0f && sad_count > 0)
				sad_ok = (mean_luma_diff <= s.max_mean_luma_diff);
			snprintf(detail, sizeof(detail),
			         "a=%s(seg=%d/%d/%d hash=0x%08x hot=%.3f/%.3f) b=%s(seg=%d/%d/%d hash=0x%08x hot=%.3f/%.3f) dxy=%.3f/%.3f tol=%.3f hash_eq=%d require_hash=%d sad_mean=%.1f sad_cells=%d sad_max=%.1f",
			         s.match_label_a.c_str(), a->seg, a->side, a->face, a->render_hash,
			         a->hot_x, a->hot_y,
			         s.match_label_b.c_str(), b->seg, b->side, b->face, b->render_hash,
			         b->hot_x, b->hot_y,
			         adx, ady, s.hot_xy_tolerance, hash_eq, s.require_hash_match,
			         mean_luma_diff, sad_count, s.max_mean_luma_diff);
			if (!hot_ok || (s.require_hash_match && !hash_eq) || !sad_ok) {
				log_append("assert_probe_match", "fail", detail);
				stop_script_fail(detail);
				break;
			}
			log_append("assert_probe_match", "done", detail);
			advance_step();
			break;
		}

		case STEP_PROBE_GLES_VBO:
			if (!gles3_shim_probe_vbo_arrays()) {
				log_append("probe_gles_vbo", "fail", "VBO array probe reported a GL error");
				stop_script_fail("probe_gles_vbo: VBO array probe reported a GL error");
				break;
			}
			log_append("probe_gles_vbo", "done", "zero and nonzero offsets passed");
			advance_step();
			break;

		case STEP_ENTER_LAUNCHER: {
			/* Yield control back to the launcher (Kotlin).
			 * Write LAUNCHER_CONTINUE with the next step index so the
			 * launcher can resume the script from there. */
			int next = g_current_step + 1;
			LOGI("ENTER_LAUNCHER: yielding at step %d, next_step=%d",
			     g_current_step + 1, next);
			write_result_file_continue(next);
			log_append("enter_launcher", "yield", "");
			if (g_log_fp) {
				fclose(g_log_fp);
				g_log_fp = NULL;
			}
			g_active = 0;

			/* Use the same Android force-exit path as the Exit to Launcher control so
			 * the engine does not stall in a quit confirmation/menu unwind state.
			 * This already runs on the game thread, so save synchronously instead of
			 * relying on a later idle tick that can be preempted by the launcher. */
#ifdef ANDROID
			automation_enter_launcher();
#else
			extern int Quitting;
			Quitting = 1;
#endif
			break;
		}

		case STEP_ENTER_GAME:
		case STEP_SETUP_COMMAND:
		case STEP_RESET_STATE:
		case STEP_CLEAR_MODS:
		case STEP_IMPORT_MISSION_ZIP:
		case STEP_ANALYZE_LEVEL_METADATA:
		case STEP_ANALYZE_LEVEL_METADATA_ALL:
		case STEP_WRITE_CONFIG:
		case STEP_TAP_BUTTON:
		case STEP_ASSERT_BUTTON:
		case STEP_ASSERT_CONTROLLER_MATCH:
			/* Launcher-only steps -- skip with a log when encountered
			 * in the game engine (should not normally happen) */
			LOGI("Skipping launcher-only step: %s", step_type_name(s.type));
			advance_step();
			break;

		case STEP_TRIGGER_ENDLEVEL:
			if (Screen_mode != SCREEN_GAME || Game_wind == NULL || ConsoleObject == NULL) {
				stop_script_fail("trigger_endlevel: game is not running");
				break;
			}
			advance_step();
#ifdef ANDROID
			android_automation_start_endlevel_sequence();
#else
			start_endlevel_sequence();
#endif
			break;

		case STEP_TRIGGER_LEVELCOMPLETE:
			if (Screen_mode != SCREEN_GAME || Game_wind == NULL || ConsoleObject == NULL) {
				stop_script_fail("trigger_levelcomplete: game is not running");
				break;
			}
			advance_step();
			PlayerFinishedLevel(0);
			break;

		case STEP_TRIGGER_POSTLEVEL: {
			int old_connected;
			int old_game_mode;
			int old_screen_mode;
			if (Screen_mode != SCREEN_GAME || Game_wind == NULL || ConsoleObject == NULL) {
				stop_script_fail("trigger_postlevel: game is not running");
				break;
			}
			old_connected = Players[Player_num].connected;
			old_game_mode = Game_mode;
			old_screen_mode = Screen_mode;
			Players[Player_num].connected = CONNECT_END_MENU;
			Game_mode |= GM_MULTI_COOP;
			advance_step();
			kmatrix_view(0);
			Game_mode = old_game_mode;
			Players[Player_num].connected = old_connected;
			set_screen_mode(old_screen_mode);
			break;
		}

		case STEP_MUSIC_CONTROL: {
			int result;

			if (s.music_operation == "next")
				result = songs_next_track();
			else if (s.music_operation == "previous")
				result = songs_prev_track();
			else if (s.music_operation == "source")
				result = android_music_set_source(s.music_source.c_str());
			else if (s.music_operation == "play")
				result = songs_play_specific_track(s.music_track);
			else if (s.music_operation == "play_range")
				result = RBAPlayTracks(s.music_track, s.music_last_track, NULL);
			else if (s.music_operation == "pause") {
				int before = RBAPeekPlayStatus();
				songs_pause();
				result = before == 1 && RBAPeekPlayStatus() == -1;
			} else if (s.music_operation == "resume") {
				int before = RBAPeekPlayStatus();
				songs_resume();
				result = before == -1 && RBAPeekPlayStatus() == 1;
			} else if (s.music_operation == "stop") {
				songs_stop_all();
				result = RBAPeekPlayStatus() == 0;
			} else {
				stop_script_fail("music_control: unknown operation");
				break;
			}
			if (!!result != s.music_expect_success) {
				stop_script_fail("music_control: unexpected operation result");
				break;
			}
			LOGI("music_control: operation=%s track=%d result=%d expected=%d",
			     s.music_operation.c_str(), s.music_track, result,
			     s.music_expect_success ? 1 : 0);
			advance_step();
			break;
		}

		case STEP_REDBOOK_INVALIDATE_SOURCE:
			if (!RBAInvalidateCurrentSourceForTest()) {
				stop_script_fail("redbook_invalidate_source: no active source");
				break;
			}
			LOGI("redbook_invalidate_source: queued source handle failure");
			advance_step();
			break;

		case STEP_SET_DEBUG:
			if (s.field == "tex_overlay")
				g_debug_tex_overlay_active = (int) std::stod(s.value);
			else if (s.field == "texture_target")
				android_texture_debug_set_target(s.value.c_str());
			else if (s.field == "texture_log")
				debug_log_set_enabled(DLOG_TEXTURE,
				                      (strcasecmp(s.value.c_str(), "true") == 0 || strtol(s.value.c_str(), NULL, 10) != 0) ? 1 : 0);
			else if (s.field == "merged_wall_mode")
				g_merged_wall_debug_mode = (int) std::stod(s.value);
			else if (s.field == "merged_wall_force_two_pass")
				g_merged_wall_force_two_pass =
				    (strcasecmp(s.value.c_str(), "true") == 0 || strtol(s.value.c_str(), NULL, 10) != 0) ? 1 : 0;
			else if (s.field == "graphics_option") {
				size_t separator = s.value.find(':');
				if (separator == std::string::npos || separator == 0 ||
				    separator + 1 >= s.value.size()) {
					stop_script_fail("graphics_option: expected name:value");
					break;
				}
				std::string name = s.value.substr(0, separator);
				int value = (int) strtol(s.value.c_str() + separator + 1, NULL, 10);
				if (!android_graphics_set_option(name.c_str(), value, 0)) {
					stop_script_fail("graphics_option: unknown option");
					break;
				}
			} else if (s.field == "show_robot_hostage_counts") {
				PlayerCfg.ShowRobotHostageCounts =
				    (strcasecmp(s.value.c_str(), "true") == 0 ||
				     strtol(s.value.c_str(), NULL, 10) != 0)
				        ? 1
				        : 0;
			} else if (s.field == "show_boss_health_bar") {
				PlayerCfg.ShowBossHealthBar =
				    (strcasecmp(s.value.c_str(), "true") == 0 ||
				     strtol(s.value.c_str(), NULL, 10) != 0)
				        ? 1
				        : 0;
			} else if (s.field == "reactor_countdown_paused") {
				int paused = (strcasecmp(s.value.c_str(), "true") == 0 ||
				              strtol(s.value.c_str(), NULL, 10) != 0)
				                 ? 1
				                 : 0;
				if (!reactor_countdown_set_paused(paused, Countdown_timer)) {
					stop_script_fail("reactor_countdown_paused: countdown inactive");
					break;
				}
			} else if (s.field == "matcen_mode_cycle_pending") {
				if (strcasecmp(s.value.c_str(), "true") == 0 ||
				    strtol(s.value.c_str(), NULL, 10) != 0)
					android_matcen_mode_cycle_pending = 1;
			} else if (s.field == "damage_boss") {
				char reason[128];
				if (!damage_first_boss(s.value, reason, sizeof(reason))) {
					log_append("set_debug", "fail", reason);
					stop_script_fail(reason);
					break;
				}
			} else if (s.field == "hud_test_message") {
				HUD_init_message(HM_DEFAULT, "%s", s.value.c_str());
			} else if (s.field == "hud_test_message_burst") {
				const long requested = strtol(s.value.c_str(), NULL, 10);
				const int count = requested < 1 ? 1 : requested > HUD_MAX_NUM_STOR ? HUD_MAX_NUM_STOR
				                                                                   : (int) requested;

				for (int message_index = 0; message_index < count; ++message_index)
					HUD_init_message(HM_DEFAULT, "hud test message %d", message_index + 1);
			} else if (s.field == "clear_robots") {
				if (strcasecmp(s.value.c_str(), "true") == 0 || strtol(s.value.c_str(), NULL, 10) != 0) {
					char reason[128];
					if (!clear_level_robots(reason, sizeof(reason))) {
						log_append("set_debug", "fail", reason);
						stop_script_fail(reason);
						break;
					}
				}
			} else if (s.field == "damage_player") {
				if (Screen_mode != SCREEN_GAME || Game_wind == NULL || ConsoleObject == NULL) {
					stop_script_fail("damage_player: game is not running");
					break;
				}
				const double shields = std::stod(s.value);
				const fix damage = (fix) (shields * F1_0);
				apply_damage_to_player(ConsoleObject, ConsoleObject, damage, 0);
			} else if (s.field == "player_keys") {
				char reason[128];
				if (!set_player_key_flags(s.value, reason, sizeof(reason))) {
					log_append("set_debug", "fail", reason);
					stop_script_fail(reason);
					break;
				}
			} else if (s.field == "release_guidebot_cage") {
				char reason[128];
				if ((strcasecmp(s.value.c_str(), "true") == 0 || strtol(s.value.c_str(), NULL, 10) != 0) &&
				    !release_guidebot_cage(reason, sizeof(reason))) {
					log_append("set_debug", "fail", reason);
					stop_script_fail(reason);
					break;
				}
			} else if (s.field == "guidebot_nav_trace") {
				debug_log_set_enabled(
				    DLOG_GUIDEBOT,
				    strcasecmp(s.value.c_str(), "true") == 0 ||
				        strtol(s.value.c_str(), NULL, 10) != 0);
			} else if (s.field == "guidebot_path_parity") {
#ifdef DXX_BUILD_DESCENT_II
				if ((strcasecmp(s.value.c_str(), "true") == 0 || strtol(s.value.c_str(), NULL, 10) != 0) &&
				    !escort_debug_compare_route_path()) {
					stop_script_fail("guidebot_path_parity: comparison failed");
					break;
				}
#else
				stop_script_fail("guidebot_path_parity: D2-only action");
				break;
#endif
			} else if (s.field == "automap_visit_all") {
				if (strcasecmp(s.value.c_str(), "true") == 0 || strtol(s.value.c_str(), NULL, 10) != 0) {
					bool changed = false;
					for (int segnum = 0; segnum <= Highest_segment_index; segnum++) {
						changed = changed || Automap_visited[segnum] == 0;
						Automap_visited[segnum] = 1;
					}
#ifdef DXX_BUILD_DESCENT_II
					if (changed)
						escort_route_notify_automap_changed(-1);
#endif
				}
			} else if (s.field == "automap_visit_segment") {
				const int segnum = (int) strtol(s.value.c_str(), NULL, 10);
				if (segnum < 0 || segnum > Highest_segment_index) {
					stop_script_fail("automap_visit_segment: invalid segment");
					break;
				}
				if (!Automap_visited[segnum]) {
					Automap_visited[segnum] = 1;
#ifdef DXX_BUILD_DESCENT_II
					escort_route_notify_automap_changed(segnum);
#endif
				}
			} else if (s.field == "automap_unvisit_segment") {
				const int segnum = (int) strtol(s.value.c_str(), NULL, 10);
				if (segnum < 0 || segnum > Highest_segment_index) {
					stop_script_fail("automap_unvisit_segment: invalid segment");
					break;
				}
				if (Automap_visited[segnum]) {
					Automap_visited[segnum] = 0;
#ifdef DXX_BUILD_DESCENT_II
					escort_route_notify_automap_changed(segnum);
#endif
				}
			} else if (s.field == "automap_revisit_segment") {
				const int segnum = (int) strtol(s.value.c_str(), NULL, 10);
				if (segnum < 0 || segnum > Highest_segment_index) {
					stop_script_fail("automap_revisit_segment: invalid segment");
					break;
				}
				if (Automap_visited[segnum]) {
					Automap_visited[segnum] = 0;
#ifdef DXX_BUILD_DESCENT_II
					escort_route_notify_automap_changed(segnum);
#endif
					Automap_visited[segnum] = 1;
#ifdef DXX_BUILD_DESCENT_II
					escort_route_notify_automap_changed(segnum);
#endif
				}
			} else if (s.field == "coop_autosave") {
				if ((strcasecmp(s.value.c_str(), "true") == 0 || strtol(s.value.c_str(), NULL, 10) != 0) &&
				    !coop_autosave()) {
					stop_script_fail("coop_autosave: active coop game required");
					break;
				}
			} else if (s.field == "android_game_request") {
#ifdef ANDROID
				if (s.value == "save") {
					g_android_open_save_menu = 1;
					g_android_open_load_menu = 0;
				} else if (s.value == "load") {
					g_android_open_save_menu = 0;
					g_android_open_load_menu = 1;
				} else if (s.value == "game_menu") {
					g_android_open_game_menu = 1;
				} else if (s.value == "pause") {
					do_game_pause();
				} else if (s.value == "auto_minimize") {
					g_android_autosave_request_kind = ANDROID_SAVE_META_KIND_AUTO_MINIMIZE;
				} else if (s.value.rfind("difficulty:", 0) == 0) {
					g_android_difficulty_request = (int) strtol(s.value.c_str() + 11, NULL, 10);
				} else {
					stop_script_fail("android_game_request: unknown request");
					break;
				}
#else
				stop_script_fail("android_game_request: Android-only action");
				break;
#endif
			} else if (s.field == "fire_trigger") {
				char reason[128];
				if (!fire_level_trigger(s.value, reason, sizeof(reason))) {
					log_append("set_debug", "fail", reason);
					stop_script_fail(reason);
					break;
				}
			} else if (s.field == "complete_route_objective") {
				char reason[128];
				if (!complete_route_objective(reason, sizeof(reason))) {
					log_append("set_debug", "fail", reason);
					stop_script_fail(reason);
					break;
				}
			} else if (s.field == "merged_wall_snapshot") {
				int request_mode = (int) strtol(s.value.c_str(), NULL, 10);

				if (strcasecmp(s.value.c_str(), "true") == 0)
					request_mode = MERGED_WALL_REQUEST_SNAPSHOT;
				if (request_mode != 0)
					android_merged_wall_request_snapshot(
					    request_mode == MERGED_WALL_REQUEST_PROBE ? MERGED_WALL_REQUEST_PROBE
					                                              : MERGED_WALL_REQUEST_SNAPSHOT);
			} else
				LOGE("set_debug: unknown field '%s'", s.field.c_str());
			LOGI("set_debug: %s = %s", s.field.c_str(), s.value.c_str());
			advance_step();
			break;

		case STEP_SET_SECRET_REVEAL:
			secret_area_set_reveal_unfound(s.enabled ? 1 : 0);
			LOGI("set_secret_reveal: enabled=%s", s.enabled ? "true" : "false");
			advance_step();
			break;

		case STEP_SET_OBJECTIVE_OVERLAY: {
			int mode;
			if (strcasecmp(s.value.c_str(), "cycle") == 0) {
				level_metadata_cycle_objective_mode();
				mode = level_metadata_get_objective_mode();
			} else {
				mode = objective_mode_from_name(s.value.c_str());
			}
			if (mode < LEVEL_METADATA_OBJECTIVES_OFF) {
				stop_script_fail("set_objective_overlay: invalid mode");
				break;
			}
			if (strcasecmp(s.value.c_str(), "cycle") != 0)
				level_metadata_set_objective_mode(mode);
			LOGI("set_objective_overlay: mode=%s",
			     level_metadata_objective_mode_name(mode));
			advance_step();
			break;
		}
	}
}

#endif /* INTROSPECT_ON */
