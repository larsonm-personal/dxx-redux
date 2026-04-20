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
#include "android_log.h"
#include "android_texture_debug.h"
#include "android_meta_actions.h"
#include "debug_tex_overlay.h"
#include "merged_wall_debug.h"
#include "game.h"
#include "screens.h"
#include "inferno.h"
#include "window.h"
#include "newmenu.h"
#include "gameseg.h"
#include "object.h"
}

/* D1 does not have SCREEN_MOVIE */
#ifndef SCREEN_MOVIE
#define SCREEN_MOVIE 99
#endif

/* Automap_active is defined in automap.c; we just need the extern. */
extern "C" int Automap_active;
extern "C" volatile int g_intro_active;

/* -- Key name -> SDLKey mapping ---------------------------------------- */

struct key_entry {
	const char *name;
	SDLKey sym;
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

/* -- Step types ------------------------------------------------------- */

enum step_type {
	STEP_KEY,                     /* inject key down+up, then delay */
	STEP_WAIT_MS,                 /* wait N milliseconds */
	STEP_WAIT_FOR,                /* wait for a game state condition */
	STEP_INTROSPECT,              /* trigger introspection dump */
	STEP_LOG,                     /* emit a logcat message */
	STEP_ASSERT,                  /* check introspection values, fail if mismatch */
	STEP_SELECT,                  /* find menu item by text and select it */
	STEP_SEND_AXIS,               /* inject joystick axis event */
	STEP_SEND_BUTTON,             /* inject joystick button press+release */
	STEP_SKIP_INTRO,              /* repeatedly dismiss launch intro with touch or button */
	STEP_SKIP_BRIEFING,           /* escape only if a non-game window covers Game_wind */
	STEP_ASSERT_OVERLAY,          /* check overlay ring buffer for matching entry */
	STEP_FACE_VIEW,               /* move player inside a segment and face a wall */
	STEP_POSE_VIEW,               /* move player to an exact position and orientation */
	STEP_ENTER_LAUNCHER,          /* yield back to launcher, write LAUNCHER_CONTINUE */
	STEP_ENTER_GAME,              /* launcher-only: no-op in game engine (skip) */
	STEP_SETUP_COMMAND,           /* launcher-only: no-op in game engine (skip) */
	STEP_RESET_STATE,             /* launcher-only: no-op in game engine (skip) */
	STEP_WRITE_CONFIG,            /* launcher-only: no-op in game engine (skip) */
	STEP_TAP_BUTTON,              /* launcher-only: no-op in game engine (skip) */
	STEP_ASSERT_BUTTON,           /* launcher-only: no-op in game engine (skip) */
	STEP_ASSERT_CONTROLLER_MATCH, /* launcher-only: no-op in game engine (skip) */
	STEP_SET_DEBUG                /* set a debug flag (e.g. tex_overlay) */
};

/* Key-value pair for STEP_ASSERT expectations.
 * Simple equality: {"key": "3"} or {"key": 3}
 * Comparison ops:  {"key": {"ne": 0}}, {"key": {"gt": 100}},
 *                  {"key": {"range": [10, 1000]}},
 *                  {"key": {"contains": "substring"}} */
struct assert_expect {
	std::string key;
	std::string value;     /* for simple equality (op=="eq") */
	std::string op = "eq"; /* eq, ne, gt, lt, gte, lte, range */
	double num_value = 0;  /* for gt/lt/gte/lte/ne */
	double range_min = 0;
	double range_max = 0;
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
	std::vector<assert_expect> expects; /* STEP_ASSERT: expected values */
	std::string select_text;            /* STEP_SELECT: partial text to match */
	bool optional = false;              /* STEP_SELECT: skip instead of fail on timeout */
	int axis_id = -1;                   /* STEP_SEND_AXIS: axis number (0-5) */
	float axis_value = 0.0f;            /* STEP_SEND_AXIS: value (-1.0 to 1.0) */
	int button_id = -1;                 /* STEP_SEND_BUTTON: button index */
	int button_held = 0;                /* STEP_SEND_BUTTON: 1 = hold (no release) */
	int button_pressed = 1;             /* STEP_SEND_BUTTON: 0 = release only */
	int segment = -1;                   /* STEP_FACE_VIEW: target segment */
	int side = -1;                      /* STEP_FACE_VIEW: target side */
	int face = 0;                       /* STEP_FACE_VIEW: target face on side */
	float distance = 4.0f;              /* STEP_FACE_VIEW: distance inside segment */
	float pos_x = 0.0f;                 /* STEP_POSE_VIEW: target X position */
	float pos_y = 0.0f;                 /* STEP_POSE_VIEW: target Y position */
	float pos_z = 0.0f;                 /* STEP_POSE_VIEW: target Z position */
	int pitch = 0;                      /* STEP_POSE_VIEW: exact pitch */
	int bank = 0;                       /* STEP_POSE_VIEW: exact bank */
	int heading = 0;                    /* STEP_POSE_VIEW: exact heading */
};

/* -- Script state ----------------------------------------------------- */

static std::vector<auto_step> g_steps;
static int g_current_step = 0;
static int g_active = 0;
static int g_failed = 0;        /* set to 1 on assert/timeout failure */
static Uint32 g_step_start = 0; /* SDL_GetTicks() when step began */
static int g_key_phase = 0;     /* 0=not sent, 1=down sent, 2=done */
static Uint32 g_repeat_start = 0;

/* -- STEP_SELECT state (multi-frame navigation) ----------------------- */
static int g_select_phase = 0; /* 0=init, 1=navigating, 2=enter sent */
static int g_select_delta = 0; /* remaining navigation steps (+down, -up) */

static char g_automate_dir[512] = "";
static char g_pending_script[512] = "";
static volatile int g_load_requested = 0;
static int g_start_step = 0;      /* skip to this step index on load */
static Uint32 g_script_start = 0; /* SDL_GetTicks() when script began */
static FILE *g_log_fp = NULL;     /* automation_log.jsonl file handle */
static int g_log_seq = 0;         /* monotonic sequence for log lines */

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
		case STEP_SKIP_INTRO: return "skip_intro";
		case STEP_SKIP_BRIEFING: return "skip_briefing";
		case STEP_ASSERT_OVERLAY: return "assert_overlay";
		case STEP_FACE_VIEW: return "face_view";
		case STEP_POSE_VIEW: return "pose_view";
		case STEP_ENTER_LAUNCHER: return "enter_launcher";
		case STEP_ENTER_GAME: return "enter_game";
		case STEP_SETUP_COMMAND: return "setup_command";
		case STEP_RESET_STATE: return "reset_state";
		case STEP_WRITE_CONFIG: return "write_config";
		case STEP_TAP_BUTTON: return "tap_button";
		case STEP_ASSERT_BUTTON: return "assert_button";
		case STEP_ASSERT_CONTROLLER_MATCH: return "assert_controller_match";
		case STEP_SET_DEBUG: return "set_debug";
		default: return "unknown";
	}
}

/* -- File-based result/log writing ------------------------------------ */

static void write_result_file(const char *result, const char *reason)
{
	if (!g_automate_dir[0])
		return;

	char path[1024];
	snprintf(path, sizeof(path), "%s/automation_result.json", g_automate_dir);

	Uint32 elapsed = SDL_GetTicks() - g_script_start;

	FILE *f = fopen(path, "w");
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
		        "{\"result\":\"%s\",\"steps_completed\":%d,"
		        "\"total_steps\":%d,\"reason\":\"%s\","
		        "\"elapsed_ms\":%u}\n",
		        result, g_current_step + 1, (int) g_steps.size(),
		        escaped, (unsigned) elapsed);
	} else {
		fprintf(f,
		        "{\"result\":\"%s\",\"steps_completed\":%d,"
		        "\"total_steps\":%d,\"elapsed_ms\":%u}\n",
		        result, g_current_step + 1, (int) g_steps.size(),
		        (unsigned) elapsed);
	}

	fclose(f);
}

static void write_result_file_continue(int next_step)
{
	if (!g_automate_dir[0])
		return;

	char path[1024];
	snprintf(path, sizeof(path), "%s/automation_result.json", g_automate_dir);

	Uint32 elapsed = SDL_GetTicks() - g_script_start;

	FILE *f = fopen(path, "w");
	if (!f)
		return;

	fprintf(f,
	        "{\"result\":\"LAUNCHER_CONTINUE\",\"next_step\":%d,"
	        "\"steps_completed\":%d,\"total_steps\":%d,"
	        "\"elapsed_ms\":%u,\"script_path\":\"%s\"}\n",
	        next_step, g_current_step + 1, (int) g_steps.size(),
	        (unsigned) elapsed, g_pending_script);
	fclose(f);
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

static void inject_axis(int axis, float value)
{
	/* Clamp to SDL range: -32768..32767 */
	int ival = (int) (value * 32767.0f);
	if (ival > 32767) ival = 32767;
	if (ival < -32768) ival = -32768;

	SDL_Event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = SDL_JOYAXISMOTION;
	ev.jaxis.which = 0;
	ev.jaxis.axis = (Uint8) axis;
	ev.jaxis.value = (Sint16) ival;
	SDL_PushEvent(&ev);
	if (!g_inject_axis_logged) {
		LOGI("Injecting axis %d = %.3f (raw %d)", axis, value, ival);
		g_inject_axis_logged = 1;
	}
}

/* -- Button injection ------------------------------------------------- */

static void inject_button(int button, int pressed)
{
	SDL_Event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = pressed ? SDL_JOYBUTTONDOWN : SDL_JOYBUTTONUP;
	ev.jbutton.which = 0;
	ev.jbutton.button = (Uint8) button;
	ev.jbutton.state = pressed ? SDL_PRESSED : SDL_RELEASED;
	SDL_PushEvent(&ev);
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

/*
 * Find a menu item whose text contains `text` (case-insensitive).
 * Searches the front window (newmenu or listbox).
 * On success sets *out_target to the item index and *out_current to
 * the currently selected item. Returns true.
 * On failure logs the error and returns false.
 */
static bool select_find_item(const char *text, int *out_target, int *out_current)
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

		for (int i = 0; i < nitems; i++) {
			if (items[i].text && icontains(items[i].text, text)) {
				/* Skip NM_TYPE_TEXT items -- they are not selectable */
				if (items[i].type == NM_TYPE_TEXT) continue;
				*out_target = i;
				*out_current = citem;
				LOGI("SELECT: found \"%s\" at index %d (current=%d) in newmenu",
				     items[i].text, i, citem);
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
		obj_delete(objnum);
		removed++;
	}

	LOGI("clear_robots: removed=%d", removed);
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

/* -- Parse script JSON/JSON5 with nlohmann ---------------------------- */

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
			else if (action == "send_axis") s.type = STEP_SEND_AXIS;
			else if (action == "send_button") s.type = STEP_SEND_BUTTON;
			else if (action == "skip_intro") s.type = STEP_SKIP_INTRO;
			else if (action == "skip_briefing") s.type = STEP_SKIP_BRIEFING;
			else if (action == "assert_overlay") s.type = STEP_ASSERT_OVERLAY;
			else if (action == "face_view") s.type = STEP_FACE_VIEW;
			else if (action == "pose_view") s.type = STEP_POSE_VIEW;
			else if (action == "enter_launcher") s.type = STEP_ENTER_LAUNCHER;
			else if (action == "enter_game") s.type = STEP_ENTER_GAME;
			else if (action == "setup_command") s.type = STEP_SETUP_COMMAND;
			else if (action == "reset_state") s.type = STEP_RESET_STATE;
			else if (action == "write_config") s.type = STEP_WRITE_CONFIG;
			else if (action == "tap_button") s.type = STEP_TAP_BUTTON;
			else if (action == "assert_button") s.type = STEP_ASSERT_BUTTON;
			else if (action == "assert_controller_match") s.type = STEP_ASSERT_CONTROLLER_MATCH;
			else if (action == "set_debug") s.type = STEP_SET_DEBUG;
			else {
				LOGE("Unknown action: %s", action.c_str());
				continue;
			}

			s.key_name = step_json.value("key", "");
			s.modifier_name = step_json.value("modifier", "");
			s.post_delay_ms = step_json.value("post_delay_ms", step_json.value("ms", 300));
			s.field = step_json.value("field", "");
			s.value = step_json.value("value", "");
			s.timeout_ms = step_json.value("timeout_ms", 0);
			s.message = step_json.value("message", "");
			s.select_text = step_json.value("text", "");
			s.optional = step_json.value("optional", false);
			s.axis_id = step_json.value("axis", -1);
			s.axis_value = step_json.value("axis_value", 0.0f);
			s.button_id = step_json.value("button", -1);
			s.button_held = step_json.value("held", 0);
			s.button_pressed = step_json.value("pressed", 1);
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

			/* Parse "expect" object for STEP_ASSERT */
			if (step_json.contains("expect") && step_json["expect"].is_object()) {
				for (auto &[key, val] : step_json["expect"].items()) {
					assert_expect ae;
					ae.key = key;
					if (val.is_object()) {
						/* Comparison operator: {"gt": 0}, {"ne": 0}, {"range": [1,10]}, {"eq": "3"} */
						for (auto &[op, opval] : val.items()) {
							ae.op = op;
							if (op == "range" && opval.is_array() && opval.size() == 2) {
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

/*
 * Run all assertions for a STEP_ASSERT.
 * Parses introspection JSON with nlohmann and checks expected values.
 * Returns empty string on success, or a failure description on first failure.
 */
static std::string run_assertions(auto_step &s)
{
	char *json_str = game_introspect_get_state();
	if (!json_str) {
		LOGE("ASSERT_FAIL: Could not get introspection state");
		return "could not get introspection state";
	}

	json state;
	try {
		state = json::parse(json_str);
	} catch (const std::exception &e) {
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
		} else {
			snprintf(desc, sizeof(desc), "\"%s\": unknown op \"%s\"",
			         ae.key.c_str(), ae.op.c_str());
		}

		if (pass) {
			LOGI("ASSERT_PASS: %s", desc);
		} else {
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
	g_select_phase = 0;
	g_select_delta = 0;
	g_inject_axis_logged = 0;

	if (g_current_step >= (int) g_steps.size()) {
		LOGI("SCRIPT_RESULT: PASS (%d steps)", (int) g_steps.size());
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

extern "C" void game_automate_load_script(const char *script_path)
{
	if (script_path) {
		strncpy(g_pending_script, script_path, sizeof(g_pending_script) - 1);
		g_pending_script[sizeof(g_pending_script) - 1] = '\0';
		g_load_requested = 1;
		LOGI("Script load requested: %s", script_path);
	}
}

extern "C" void game_automate_tick(void)
{
	/* Handle pending load request (from JNI thread) */
	if (g_load_requested) {
		g_load_requested = 0;
		LOGI("Loading automation script: %s", g_pending_script);

		remove_stale_result();
		open_log_file();

		if (load_script_file(g_pending_script)) {
			g_current_step = g_start_step;
			g_start_step = 0; /* reset for next load */
			g_step_start = SDL_GetTicks();
			g_script_start = g_step_start;
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
				else
					inject_key_tap(s.key_name);
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
			if (check_condition(s.field, s.value)) {
				LOGI("Condition met: %s = %s (after %u ms)", s.field.c_str(), s.value.c_str(), elapsed);
				log_append("wait_for", "done", s.field.c_str());
				advance_step();
			} else if (s.timeout_ms > 0 && elapsed >= (Uint32) s.timeout_ms) {
				char reason[256];
				snprintf(reason, sizeof(reason), "TIMEOUT waiting for %s = %s (after %d ms)",
				         s.field.c_str(), s.value.c_str(), s.timeout_ms);
				log_append("wait_for", "timeout", s.field.c_str());
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
			std::string fail_desc = run_assertions(s);
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

		case STEP_SELECT:
			if (g_select_phase == 0) {
				/* Phase 0: find the target item and compute navigation delta.
				 * If timeout_ms is set, poll each frame until the item appears
				 * instead of failing immediately. */
				int target, current;
				if (!select_find_item(s.select_text.c_str(), &target, &current)) {
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
				if (g_select_delta == 0) {
					/* Already on the right item -- go straight to enter */
					g_select_phase = 2;
				}
			}
			if (g_select_phase == 1) {
				/*
				 * Phase 1: re-read the live citem each frame and inject
				 * one navigation key.  This handles menus where
				 * newmenu_scroll() skips NM_TYPE_TEXT items, so a single
				 * DOWN press may advance by more than one index.
				 */
				int target, current;
				if (!select_find_item(s.select_text.c_str(), &target, &current)) {
					stop_script_fail("SELECT: menu disappeared during navigation");
					break;
				}
				int delta = target - current;
				if (delta > 0) {
					inject_key_tap("down");
				} else if (delta < 0) {
					inject_key_tap("up");
				}
				if (delta == 0) {
					g_select_phase = 2;
				}
				/* else: keep navigating next frame */
			} else if (g_select_phase == 2) {
				/* Phase 2: re-verify position, then press enter.
				 * The cursor can drift between Phase 1 and 2 if the
				 * game loop stalls (e.g. emulator lag). */
				int target, current;
				if (!select_find_item(s.select_text.c_str(), &target, &current)) {
					stop_script_fail("SELECT: menu disappeared before confirm");
					break;
				}
				if (target != current) {
					LOGI("SELECT: cursor drifted to %d (target %d), re-navigating", current, target);
					g_select_phase = 1;
					break;
				}
				inject_key_tap("enter");
				LOGI("SELECT: confirmed \"%s\"", s.select_text.c_str());
				g_select_phase = 3;
				g_step_start = now;
			} else if (g_select_phase == 3) {
				/* Phase 3: wait for the key to be processed before advancing */
				if (elapsed >= (Uint32) s.post_delay_ms) {
					advance_step();
				}
			}
			break;

		case STEP_SEND_AXIS:
			if (g_key_phase == 0 && s.axis_id >= 0 && s.axis_id < 8) {
				inject_axis(s.axis_id, s.axis_value);
				g_key_phase = 1;
				g_step_start = now;
			} else if (g_key_phase == 1) {
				/* Re-inject every tick to fight the touch overlay's zero-flood */
				inject_axis(s.axis_id, s.axis_value);
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
			if (g_key_phase == 0) {
				window *front = window_get_front();
				if (Game_wind != NULL && (front == NULL || front == Game_wind)) {
					LOGI("skip_briefing: game window is front, done");
					advance_step();
				} else if (front != NULL && front != Game_wind) {
					/* Briefing or other non-game window on top -- dismiss it. */
					LOGI("skip_briefing: dismissing non-game window (Game_wind=%s)",
					     Game_wind ? "exists" : "NULL");
					inject_key_tap("escape");
					g_key_phase = 1;
					g_step_start = now;
				} else {
					/* No windows at all yet -- keep polling. */
					if (s.timeout_ms > 0 && elapsed >= (Uint32) s.timeout_ms) {
						LOGI("skip_briefing: timed out with no windows (%u ms)", elapsed);
						advance_step();
					}
				}
			} else if (g_key_phase == 1) {
				/* Escape was sent; wait post_delay then re-check (phase 0). */
				if (elapsed >= (Uint32) s.post_delay_ms) {
					g_key_phase = 0;
					g_step_start = now;
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
			 * the engine does not stall in a quit confirmation/menu unwind state. */
#ifdef ANDROID
			meta_action_dispatch(META_RETURN_TO_LAUNCHER, 1);
#else
			extern int Quitting;
			Quitting = 1;
#endif
			break;
		}

		case STEP_ENTER_GAME:
		case STEP_SETUP_COMMAND:
		case STEP_RESET_STATE:
		case STEP_WRITE_CONFIG:
		case STEP_TAP_BUTTON:
		case STEP_ASSERT_BUTTON:
		case STEP_ASSERT_CONTROLLER_MATCH:
			/* Launcher-only steps -- skip with a log when encountered
			 * in the game engine (should not normally happen) */
			LOGI("Skipping launcher-only step: %s", step_type_name(s.type));
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
			else if (s.field == "clear_robots") {
				if (strcasecmp(s.value.c_str(), "true") == 0 || strtol(s.value.c_str(), NULL, 10) != 0) {
					char reason[128];
					if (!clear_level_robots(reason, sizeof(reason))) {
						log_append("set_debug", "fail", reason);
						stop_script_fail(reason);
						break;
					}
				}
			} else if (s.field == "merged_wall_snapshot") {
				if (strcasecmp(s.value.c_str(), "true") == 0 || strtol(s.value.c_str(), NULL, 10) != 0)
					android_merged_wall_request_snapshot();
			} else if (s.field == "merged_wall_experiment") {
				int experiment = (int) std::stod(s.value);

				switch (experiment) {
					case MERGED_WALL_EXPERIMENT_FORCE_LEGACY_TEXMERGE:
					case MERGED_WALL_EXPERIMENT_CLEAR_SECONDARY_UNITS_SINGLE:
						g_merged_wall_experiment_mode = experiment;
						break;
					default:
						g_merged_wall_experiment_mode = MERGED_WALL_EXPERIMENT_DEFAULT;
						break;
				}
				__sync_synchronize();
				g_merged_wall_experiment_pending_apply = 1;
			} else
				LOGE("set_debug: unknown field '%s'", s.field.c_str());
			LOGI("set_debug: %s = %s", s.field.c_str(), s.value.c_str());
			advance_step();
			break;
	}
}

extern "C" void game_automate_set_start_step(int step)
{
	g_start_step = step;
	LOGI("Start step set to %d", step);
}

#endif /* INTROSPECT_ON */
