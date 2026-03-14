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
#include "game.h"
#include "screens.h"
#include "inferno.h"
#include "window.h"
#include "newmenu.h"
#include "object.h"
}

/* D1 does not have SCREEN_MOVIE */
#ifndef SCREEN_MOVIE
#define SCREEN_MOVIE 99
#endif

/* Automap_active is defined in automap.c; we just need the extern. */
extern "C" int Automap_active;

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
	STEP_KEY,          /* inject key down+up, then delay */
	STEP_WAIT_MS,      /* wait N milliseconds */
	STEP_WAIT_FOR,     /* wait for a game state condition */
	STEP_INTROSPECT,   /* trigger introspection dump */
	STEP_LOG,          /* emit a logcat message */
	STEP_ASSERT,       /* check introspection values, fail if mismatch */
	STEP_SELECT,       /* find menu item by text and select it */
	STEP_SEND_AXIS,    /* inject joystick axis event */
	STEP_SKIP_BRIEFING /* escape only if a non-game window covers Game_wind */
};

/* Key-value pair for STEP_ASSERT expectations.
 * Simple equality: {"key": "3"} or {"key": 3}
 * Comparison ops:  {"key": {"ne": 0}}, {"key": {"gt": 100}},
 *                  {"key": {"range": [10, 1000]}} */
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
	int axis_id = -1;                   /* STEP_SEND_AXIS: axis number (0-5) */
	float axis_value = 0.0f;            /* STEP_SEND_AXIS: value (-1.0 to 1.0) */
};

/* -- Script state ----------------------------------------------------- */

static std::vector<auto_step> g_steps;
static int g_current_step = 0;
static int g_active = 0;
static int g_failed = 0;        /* set to 1 on assert/timeout failure */
static Uint32 g_step_start = 0; /* SDL_GetTicks() when step began */
static int g_key_phase = 0;     /* 0=not sent, 1=down sent, 2=done */

/* -- STEP_SELECT state (multi-frame navigation) ----------------------- */
static int g_select_phase = 0; /* 0=init, 1=navigating, 2=enter sent */
static int g_select_delta = 0; /* remaining navigation steps (+down, -up) */

static char g_automate_dir[512] = "";
static char g_pending_script[512] = "";
static volatile int g_load_requested = 0;

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
	LOGI("Injecting axis %d = %.3f (raw %d)", axis, value, ival);
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
			else if (action == "skip_briefing") s.type = STEP_SKIP_BRIEFING;
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
			s.axis_id = step_json.value("axis", -1);
			s.axis_value = step_json.value("axis_value", 0.0f);

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
 * Returns 1 if all pass, 0 on first failure (logs the failure).
 */
static int run_assertions(auto_step &s)
{
	char *json_str = game_introspect_get_state();
	if (!json_str) {
		LOGE("ASSERT_FAIL: Could not get introspection state");
		return 0;
	}

	json state;
	try {
		state = json::parse(json_str);
	} catch (const std::exception &e) {
		LOGE("ASSERT_FAIL: Failed to parse introspection JSON: %s", e.what());
		free(json_str);
		return 0;
	}
	free(json_str);

	for (const auto &ae : s.expects) {
		if (!state.contains(ae.key)) {
			LOGE("ASSERT_FAIL: key \"%s\" not found in introspection JSON", ae.key.c_str());
			return 0;
		}

		const auto &val = state[ae.key];
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
		} else {
			snprintf(desc, sizeof(desc), "\"%s\": unknown op \"%s\"",
			         ae.key.c_str(), ae.op.c_str());
		}

		if (pass) {
			LOGI("ASSERT_PASS: %s", desc);
		} else {
			LOGE("ASSERT_FAIL: %s", desc);
			return 0;
		}
	}

	return 1;
}

/* -- Advance to next step --------------------------------------------- */

static void stop_script_fail(const char *reason)
{
	LOGE("SCRIPT_RESULT: FAIL at step %d/%d -- %s",
	     g_current_step + 1, (int) g_steps.size(), reason);
	g_active = 0;
	g_failed = 1;
}

static void advance_step(void)
{
	g_current_step++;
	g_step_start = SDL_GetTicks();
	g_key_phase = 0;
	g_select_phase = 0;
	g_select_delta = 0;

	if (g_current_step >= (int) g_steps.size()) {
		LOGI("SCRIPT_RESULT: PASS (%d steps)", (int) g_steps.size());
		g_active = 0;
		g_failed = 0;
	} else {
		auto &s = g_steps[g_current_step];
		LOGI("Step %d/%d: type=%d", g_current_step + 1, (int) g_steps.size(), (int) s.type);
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

		if (load_script_file(g_pending_script)) {
			g_current_step = 0;
			g_step_start = SDL_GetTicks();
			g_key_phase = 0;
			g_select_phase = 0;
			g_select_delta = 0;
			g_active = 1;
			g_failed = 0;
			LOGI("Script started: %d steps", (int) g_steps.size());

			if (!g_steps.empty()) {
				auto &s = g_steps[0];
				LOGI("Step 1/%d: type=%d", (int) g_steps.size(), (int) s.type);
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
				advance_step();
			}
			break;

		case STEP_WAIT_FOR:
			if (check_condition(s.field, s.value)) {
				LOGI("Condition met: %s = %s (after %u ms)", s.field.c_str(), s.value.c_str(), elapsed);
				advance_step();
			} else if (s.timeout_ms > 0 && elapsed >= (Uint32) s.timeout_ms) {
				char reason[256];
				snprintf(reason, sizeof(reason), "TIMEOUT waiting for %s = %s (after %d ms)",
				         s.field.c_str(), s.value.c_str(), s.timeout_ms);
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

		case STEP_ASSERT:
			if (run_assertions(s)) {
				advance_step();
			} else {
				stop_script_fail("assertion failed");
			}
			break;

		case STEP_SELECT:
			if (g_select_phase == 0) {
				/* Phase 0: find the target item and compute navigation delta */
				int target, current;
				if (!select_find_item(s.select_text.c_str(), &target, &current)) {
					char reason[256];
					snprintf(reason, sizeof(reason),
					         "SELECT: item \"%s\" not found in menu",
					         s.select_text.c_str());
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
				/* Phase 2: press enter to confirm selection */
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
			if (s.axis_id >= 0 && s.axis_id < 8) {
				inject_axis(s.axis_id, s.axis_value);
			}
			/* Hold for post_delay_ms so the axis has time to affect the game */
			if (elapsed >= (Uint32) s.post_delay_ms) {
				advance_step();
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
	}
}

#endif /* INTROSPECT_ON */
