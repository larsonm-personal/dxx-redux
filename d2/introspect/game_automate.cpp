/*
 * game_automate.cpp — Automated input scripting for AI-assisted testing.
 *
 * Parses a JSON script of steps (key presses, waits, condition checks)
 * and executes them one per frame, injecting SDL key events to drive
 * the game through menus, briefings, and gameplay automatically.
 *
 * Uses nlohmann/json for parsing.
 * Guarded by INTROSPECT_ON — only compiled into debug Android builds.
 */

#ifdef INTROSPECT_ON

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

/* Engine headers are pure C — wrap them for C++ linkage. */
extern "C" {
#include <SDL.h>
}

#ifdef ANDROID
#include <android/log.h>
#define LOG_TAG "DXX-Automate"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
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
}

/* Automap_active is defined in automap.c; we just need the extern. */
extern "C" int Automap_active;

/* ── Key name → SDLKey mapping ──────────────────────────────────────── */

struct key_entry {
    const char *name;
    SDLKey      sym;
};

static const key_entry key_map[] = {
    /* Navigation */
    {"enter",     SDLK_RETURN},
    {"return",    SDLK_RETURN},
    {"escape",    SDLK_ESCAPE},
    {"esc",       SDLK_ESCAPE},
    {"tab",       SDLK_TAB},
    {"space",     SDLK_SPACE},
    {"backspace", SDLK_BACKSPACE},
    {"delete",    SDLK_DELETE},

    /* Arrows */
    {"up",    SDLK_UP},
    {"down",  SDLK_DOWN},
    {"left",  SDLK_LEFT},
    {"right", SDLK_RIGHT},

    /* Letters */
    {"a", SDLK_a}, {"b", SDLK_b}, {"c", SDLK_c}, {"d", SDLK_d},
    {"e", SDLK_e}, {"f", SDLK_f}, {"g", SDLK_g}, {"h", SDLK_h},
    {"i", SDLK_i}, {"j", SDLK_j}, {"k", SDLK_k}, {"l", SDLK_l},
    {"m", SDLK_m}, {"n", SDLK_n}, {"o", SDLK_o}, {"p", SDLK_p},
    {"q", SDLK_q}, {"r", SDLK_r}, {"s", SDLK_s}, {"t", SDLK_t},
    {"u", SDLK_u}, {"v", SDLK_v}, {"w", SDLK_w}, {"x", SDLK_x},
    {"y", SDLK_y}, {"z", SDLK_z},

    /* Digits */
    {"0", SDLK_0}, {"1", SDLK_1}, {"2", SDLK_2}, {"3", SDLK_3},
    {"4", SDLK_4}, {"5", SDLK_5}, {"6", SDLK_6}, {"7", SDLK_7},
    {"8", SDLK_8}, {"9", SDLK_9},

    /* Function keys */
    {"f1",  SDLK_F1},  {"f2",  SDLK_F2},  {"f3",  SDLK_F3},
    {"f4",  SDLK_F4},  {"f5",  SDLK_F5},  {"f6",  SDLK_F6},
    {"f7",  SDLK_F7},  {"f8",  SDLK_F8},  {"f9",  SDLK_F9},
    {"f10", SDLK_F10}, {"f11", SDLK_F11}, {"f12", SDLK_F12},

    /* Page / Home */
    {"home",     SDLK_HOME},
    {"end",      SDLK_END},
    {"pageup",   SDLK_PAGEUP},
    {"pagedown", SDLK_PAGEDOWN},

    /* Modifiers */
    {"lshift", SDLK_LSHIFT}, {"rshift", SDLK_RSHIFT},
    {"lctrl",  SDLK_LCTRL},  {"rctrl",  SDLK_RCTRL},
    {"lalt",   SDLK_LALT},   {"ralt",   SDLK_RALT},

    /* Punctuation */
    {"minus",        SDLK_MINUS},
    {"equals",       SDLK_EQUALS},
    {"comma",        SDLK_COMMA},
    {"period",       SDLK_PERIOD},
    {"slash",        SDLK_SLASH},
    {"semicolon",    SDLK_SEMICOLON},

    {NULL, SDLK_UNKNOWN}
};

static SDLKey lookup_key(const char *name) {
    for (const key_entry *k = key_map; k->name; k++) {
        if (strcasecmp(k->name, name) == 0)
            return k->sym;
    }
    return SDLK_UNKNOWN;
}

/* ── Step types ─────────────────────────────────────────────────────── */

enum step_type {
    STEP_KEY,           /* inject key down+up, then delay */
    STEP_WAIT_MS,       /* wait N milliseconds */
    STEP_WAIT_FOR,      /* wait for a game state condition */
    STEP_INTROSPECT,    /* trigger introspection dump */
    STEP_LOG,           /* emit a logcat message */
    STEP_ASSERT         /* check introspection values, fail if mismatch */
};

/* Key-value pair for STEP_ASSERT expectations */
struct assert_expect {
    std::string key;
    std::string value;
};

struct auto_step {
    step_type   type = STEP_KEY;
    std::string key_name;               /* STEP_KEY: key name */
    int         delay_ms = 300;         /* STEP_KEY / STEP_WAIT_MS: delay in ms */
    std::string field;                  /* STEP_WAIT_FOR: field name */
    std::string value;                  /* STEP_WAIT_FOR: expected value */
    int         timeout_ms = 0;        /* STEP_WAIT_FOR: timeout (0 = infinite) */
    std::string message;                /* STEP_LOG: message text */
    std::vector<assert_expect> expects; /* STEP_ASSERT: expected values */
};

/* ── Script state ───────────────────────────────────────────────────── */

static std::vector<auto_step> g_steps;
static int        g_current_step = 0;
static int        g_active       = 0;
static int        g_failed       = 0;   /* set to 1 on assert/timeout failure */
static Uint32     g_step_start   = 0;   /* SDL_GetTicks() when step began */
static int        g_key_phase    = 0;   /* 0=not sent, 1=down sent, 2=done */

static char g_automate_dir[512]    = "";
static char g_pending_script[512]  = "";
static volatile int g_load_requested = 0;

/* ── Key injection ──────────────────────────────────────────────────── */

static void inject_key(SDLKey sym, int down) {
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type           = down ? SDL_KEYDOWN : SDL_KEYUP;
    ev.key.state      = down ? SDL_PRESSED : SDL_RELEASED;
    ev.key.keysym.sym = sym;
    ev.key.keysym.mod = KMOD_NONE;
    ev.key.keysym.unicode = 0;
    SDL_PushEvent(&ev);
}

static void inject_key_tap(const std::string &name) {
    SDLKey sym = lookup_key(name.c_str());
    if (sym == SDLK_UNKNOWN) {
        LOGE("Unknown key name: '%s'", name.c_str());
        return;
    }
    LOGI("Injecting key: %s (SDLK %d)", name.c_str(), (int)sym);
    inject_key(sym, 1);  /* down */
    inject_key(sym, 0);  /* up */
}

/* ── Condition checking ─────────────────────────────────────────────── */

extern "C" window *Game_wind;

static int check_condition(const std::string &field, const std::string &value) {
    if (strcasecmp(field.c_str(), "in_game") == 0) {
        int in_game = (Game_wind != NULL && Screen_mode == SCREEN_GAME);
        if (strcasecmp(value.c_str(), "true") == 0) return in_game;
        if (strcasecmp(value.c_str(), "false") == 0) return !in_game;
    }
    else if (strcasecmp(field.c_str(), "screen_mode") == 0) {
        const char *cur = NULL;
        switch (Screen_mode) {
            case SCREEN_MENU:   cur = "menu";   break;
            case SCREEN_GAME:   cur = "game";   break;
            case SCREEN_EDITOR: cur = "editor"; break;
            case SCREEN_MOVIE:  cur = "movie";  break;
        }
        return cur && strcasecmp(cur, value.c_str()) == 0;
    }
    else if (strcasecmp(field.c_str(), "automap_active") == 0) {
        if (strcasecmp(value.c_str(), "true") == 0) return Automap_active;
        if (strcasecmp(value.c_str(), "false") == 0) return !Automap_active;
    }
    else if (strcasecmp(field.c_str(), "game_window_is_front") == 0) {
        window *front = window_get_front();
        int is_front = (front && front == Game_wind);
        if (strcasecmp(value.c_str(), "true") == 0) return is_front;
        if (strcasecmp(value.c_str(), "false") == 0) return !is_front;
    }
    else {
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

/* ── Parse script JSON with nlohmann ────────────────────────────────── */

static int parse_script(const char *json_text) {
    g_steps.clear();

    try {
        json script = json::parse(json_text);
        if (!script.is_array()) {
            LOGE("Script must be a JSON array");
            return 0;
        }

        for (const auto &step_json : script) {
            auto_step s;

            std::string action = step_json.value("action", "");

            if (action == "key")             s.type = STEP_KEY;
            else if (action == "wait_ms")    s.type = STEP_WAIT_MS;
            else if (action == "wait_for")   s.type = STEP_WAIT_FOR;
            else if (action == "introspect") s.type = STEP_INTROSPECT;
            else if (action == "log")        s.type = STEP_LOG;
            else if (action == "assert")     s.type = STEP_ASSERT;
            else {
                LOGE("Unknown action: %s", action.c_str());
                continue;
            }

            s.key_name   = step_json.value("key", "");
            s.delay_ms   = step_json.value("delay_ms", step_json.value("ms", 300));
            s.field      = step_json.value("field", "");
            s.value      = step_json.value("value", "");
            s.timeout_ms = step_json.value("timeout_ms", 0);
            s.message    = step_json.value("message", "");

            /* Parse "expect" object for STEP_ASSERT */
            if (step_json.contains("expect") && step_json["expect"].is_object()) {
                for (auto &[key, val] : step_json["expect"].items()) {
                    assert_expect ae;
                    ae.key = key;
                    if (val.is_string()) {
                        ae.value = val.get<std::string>();
                    } else if (val.is_boolean()) {
                        ae.value = val.get<bool>() ? "true" : "false";
                    } else if (val.is_number_integer()) {
                        ae.value = std::to_string(val.get<int>());
                    } else {
                        ae.value = val.dump();
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

    LOGI("Parsed %d automation steps", (int)g_steps.size());
    return (int)g_steps.size();
}

/* ── Load script from file ──────────────────────────────────────────── */

static int load_script_file(const char *path) {
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

    char *buf = (char *)malloc(len + 1);
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

/* ── Assert: check introspection JSON values ───────────────────────── */

/*
 * Run all assertions for a STEP_ASSERT.
 * Parses introspection JSON with nlohmann and checks expected values.
 * Returns 1 if all pass, 0 on first failure (logs the failure).
 */
static int run_assertions(auto_step &s) {
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
            LOGI("ASSERT_EXPECTED: \"%s\" = \"%s\"", ae.key.c_str(), ae.value.c_str());
            return 0;
        }

        std::string actual;
        const auto &val = state[ae.key];
        if (val.is_string()) {
            actual = val.get<std::string>();
        } else if (val.is_boolean()) {
            actual = val.get<bool>() ? "true" : "false";
        } else if (val.is_number_integer()) {
            actual = std::to_string(val.get<int>());
        } else if (val.is_number_float()) {
            /* Format to match typical expectations (e.g. "100.0") */
            char tmp[64];
            snprintf(tmp, sizeof(tmp), "%.1f", val.get<double>());
            actual = tmp;
        } else if (val.is_null()) {
            actual = "null";
        } else {
            actual = val.dump();
        }

        if (strcasecmp(actual.c_str(), ae.value.c_str()) != 0) {
            LOGE("ASSERT_FAIL: \"%s\" expected \"%s\" but got \"%s\"",
                 ae.key.c_str(), ae.value.c_str(), actual.c_str());
            return 0;
        }
        LOGI("ASSERT_PASS: \"%s\" == \"%s\"", ae.key.c_str(), ae.value.c_str());
    }

    return 1;
}

/* ── Advance to next step ───────────────────────────────────────────── */

static void stop_script_fail(const char *reason) {
    LOGE("SCRIPT_RESULT: FAIL at step %d/%d — %s",
         g_current_step + 1, (int)g_steps.size(), reason);
    g_active = 0;
    g_failed = 1;
}

static void advance_step(void) {
    g_current_step++;
    g_step_start = SDL_GetTicks();
    g_key_phase = 0;

    if (g_current_step >= (int)g_steps.size()) {
        LOGI("SCRIPT_RESULT: PASS (%d steps)", (int)g_steps.size());
        g_active = 0;
        g_failed = 0;
    } else {
        auto &s = g_steps[g_current_step];
        LOGI("Step %d/%d: type=%d", g_current_step + 1, (int)g_steps.size(), (int)s.type);
    }
}

/* ── Public API ─────────────────────────────────────────────────────── */

extern "C" void game_automate_set_path(const char *dir_path) {
    if (dir_path) {
        strncpy(g_automate_dir, dir_path, sizeof(g_automate_dir) - 1);
        g_automate_dir[sizeof(g_automate_dir) - 1] = '\0';
        LOGI("Automation dir set: %s", g_automate_dir);
    }
}

extern "C" void game_automate_load_script(const char *script_path) {
    if (script_path) {
        strncpy(g_pending_script, script_path, sizeof(g_pending_script) - 1);
        g_pending_script[sizeof(g_pending_script) - 1] = '\0';
        g_load_requested = 1;
        LOGI("Script load requested: %s", script_path);
    }
}

extern "C" void game_automate_tick(void) {
    /* Handle pending load request (from JNI thread) */
    if (g_load_requested) {
        g_load_requested = 0;
        LOGI("Loading automation script: %s", g_pending_script);

        if (load_script_file(g_pending_script)) {
            g_current_step = 0;
            g_step_start = SDL_GetTicks();
            g_key_phase = 0;
            g_active = 1;
            g_failed = 0;
            LOGI("Script started: %d steps", (int)g_steps.size());

            if (!g_steps.empty()) {
                auto &s = g_steps[0];
                LOGI("Step 1/%d: type=%d", (int)g_steps.size(), (int)s.type);
            }
        }
    }

    if (!g_active || g_current_step >= (int)g_steps.size())
        return;

    auto &s = g_steps[g_current_step];
    Uint32 now = SDL_GetTicks();
    Uint32 elapsed = now - g_step_start;

    switch (s.type) {
    case STEP_KEY:
        if (g_key_phase == 0) {
            inject_key_tap(s.key_name);
            g_key_phase = 1;
            g_step_start = now;
        } else if (g_key_phase == 1) {
            if (elapsed >= (Uint32)s.delay_ms) {
                advance_step();
            }
        }
        break;

    case STEP_WAIT_MS:
        if (elapsed >= (Uint32)s.delay_ms) {
            LOGI("Wait completed: %d ms", s.delay_ms);
            advance_step();
        }
        break;

    case STEP_WAIT_FOR:
        if (check_condition(s.field, s.value)) {
            LOGI("Condition met: %s = %s (after %u ms)", s.field.c_str(), s.value.c_str(), elapsed);
            advance_step();
        } else if (s.timeout_ms > 0 && elapsed >= (Uint32)s.timeout_ms) {
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
    }
}

#endif /* INTROSPECT_ON */
