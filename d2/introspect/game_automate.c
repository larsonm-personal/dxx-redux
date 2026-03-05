/*
 * game_automate.c — Automated input scripting for AI-assisted testing.
 *
 * Parses a JSON script of steps (key presses, waits, condition checks)
 * and executes them one per frame, injecting SDL key events to drive
 * the game through menus, briefings, and gameplay automatically.
 *
 * Guarded by INTROSPECT_ON — only compiled into debug Android builds.
 */

#ifdef INTROSPECT_ON

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <SDL.h>

#ifdef ANDROID
#include <android/log.h>
#define LOG_TAG "DXX-Automate"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(fmt, ...) printf("[Automate] " fmt "\n", ##__VA_ARGS__)
#define LOGE(fmt, ...) fprintf(stderr, "[Automate] ERROR: " fmt "\n", ##__VA_ARGS__)
#endif

#include "game_automate.h"
#include "game_introspect.h"

/* Engine headers for condition checks */
#include "game.h"
#include "screens.h"
#include "inferno.h"
#include "window.h"
#include "newmenu.h"

/* Automap_active is defined in automap.c; we just need the extern. */
extern int Automap_active;

/* ── Key name → SDLKey mapping ──────────────────────────────────────── */

typedef struct {
    const char *name;
    SDLKey      sym;
} key_entry;

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

typedef enum {
    STEP_KEY,           /* inject key down+up, then delay */
    STEP_WAIT_MS,       /* wait N milliseconds */
    STEP_WAIT_FOR,      /* wait for a game state condition */
    STEP_INTROSPECT,    /* trigger introspection dump */
    STEP_LOG,           /* emit a logcat message */
    STEP_ASSERT         /* check introspection values, fail if mismatch */
} step_type;

/* Key-value pair for STEP_ASSERT expectations */
typedef struct {
    char key[64];
    char value[128];
} assert_expect;

#define MAX_EXPECTS 16

typedef struct {
    step_type type;
    char      key_name[32];     /* STEP_KEY: key name */
    int       delay_ms;         /* STEP_KEY / STEP_WAIT_MS: delay in ms */
    char      field[64];        /* STEP_WAIT_FOR: field name */
    char      value[64];        /* STEP_WAIT_FOR: expected value */
    int       timeout_ms;       /* STEP_WAIT_FOR: timeout (0 = infinite) */
    char      message[128];     /* STEP_LOG: message text */
    assert_expect expects[MAX_EXPECTS]; /* STEP_ASSERT: expected values */
    int       num_expects;              /* STEP_ASSERT: number of expectations */
} auto_step;

#define MAX_STEPS 256

/* ── Script state ───────────────────────────────────────────────────── */

static auto_step  g_steps[MAX_STEPS];
static int        g_num_steps    = 0;
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

static void inject_key_tap(const char *name) {
    SDLKey sym = lookup_key(name);
    if (sym == SDLK_UNKNOWN) {
        LOGE("Unknown key name: '%s'", name);
        return;
    }
    LOGI("Injecting key: %s (SDLK %d)", name, (int)sym);
    inject_key(sym, 1);  /* down */
    inject_key(sym, 0);  /* up */
}

/* ── Condition checking ─────────────────────────────────────────────── */

extern window *Game_wind;

static int check_condition(const char *field, const char *value) {
    if (strcasecmp(field, "in_game") == 0) {
        int in_game = (Game_wind != NULL && Screen_mode == SCREEN_GAME);
        if (strcasecmp(value, "true") == 0) return in_game;
        if (strcasecmp(value, "false") == 0) return !in_game;
    }
    else if (strcasecmp(field, "screen_mode") == 0) {
        const char *cur = NULL;
        switch (Screen_mode) {
            case SCREEN_MENU:   cur = "menu";   break;
            case SCREEN_GAME:   cur = "game";   break;
            case SCREEN_EDITOR: cur = "editor"; break;
            case SCREEN_MOVIE:  cur = "movie";  break;
        }
        return cur && strcasecmp(cur, value) == 0;
    }
    else if (strcasecmp(field, "automap_active") == 0) {
        if (strcasecmp(value, "true") == 0) return Automap_active;
        if (strcasecmp(value, "false") == 0) return !Automap_active;
    }
    else if (strcasecmp(field, "game_window_is_front") == 0) {
        window *front = window_get_front();
        int is_front = (front && front == Game_wind);
        if (strcasecmp(value, "true") == 0) return is_front;
        if (strcasecmp(value, "false") == 0) return !is_front;
    }
    else {
        /* Fallback: search introspection JSON for "field": value pattern */
        char *json = game_introspect_get_state();
        if (json) {
            char pattern[256];
            snprintf(pattern, sizeof(pattern), "\"%s\"", field);
            char *pos = strstr(json, pattern);
            int found = 0;
            if (pos) {
                /* Skip to the colon and value */
                pos += strlen(pattern);
                while (*pos && (*pos == ' ' || *pos == ':')) pos++;
                /* Check if value matches (rough string match) */
                if (strncasecmp(pos, value, strlen(value)) == 0)
                    found = 1;
                /* Also check quoted value */
                char qval[128];
                snprintf(qval, sizeof(qval), "\"%s\"", value);
                if (strncasecmp(pos, qval, strlen(qval)) == 0)
                    found = 1;
            }
            free(json);
            return found;
        }
    }
    return 0;
}

/* ── Minimal JSON parser for our script format ──────────────────────── */

/* Skip whitespace */
static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

/* Parse a JSON string value (expects p to point at opening ").
 * Writes into buf (max buflen-1 chars), returns pointer past closing ". */
static const char *parse_string(const char *p, char *buf, size_t buflen) {
    if (*p != '"') { buf[0] = '\0'; return p; }
    p++; /* skip opening quote */
    size_t i = 0;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) {
            p++; /* skip backslash */
            switch (*p) {
                case 'n': if (i < buflen-1) buf[i++] = '\n'; break;
                case 't': if (i < buflen-1) buf[i++] = '\t'; break;
                case '"': if (i < buflen-1) buf[i++] = '"';  break;
                case '\\':if (i < buflen-1) buf[i++] = '\\'; break;
                default:  if (i < buflen-1) buf[i++] = *p;   break;
            }
        } else {
            if (i < buflen-1) buf[i++] = *p;
        }
        p++;
    }
    buf[i] = '\0';
    if (*p == '"') p++; /* skip closing quote */
    return p;
}

/* Parse a number (integer). Returns pointer past the number. */
static const char *parse_int(const char *p, int *val) {
    *val = 0;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    while (*p >= '0' && *p <= '9') {
        *val = *val * 10 + (*p - '0');
        p++;
    }
    if (neg) *val = -*val;
    return p;
}

/* Parse a boolean (true/false). Returns pointer past it. */
static const char *parse_bool(const char *p, char *buf, size_t buflen) {
    if (strncmp(p, "true", 4) == 0) {
        strncpy(buf, "true", buflen); buf[buflen-1] = '\0';
        return p + 4;
    }
    if (strncmp(p, "false", 5) == 0) {
        strncpy(buf, "false", buflen); buf[buflen-1] = '\0';
        return p + 5;
    }
    buf[0] = '\0';
    return p;
}

/* Parse the full script JSON.
 * Format: [ { "action": "key", "key": "enter", "delay_ms": 500 }, ... ] */
static int parse_script(const char *json) {
    const char *p = skip_ws(json);
    if (*p != '[') {
        LOGE("Script must be a JSON array");
        return 0;
    }
    p++;

    g_num_steps = 0;

    while (g_num_steps < MAX_STEPS) {
        p = skip_ws(p);
        if (*p == ']') break;
        if (*p == ',') { p++; continue; }
        if (*p != '{') {
            LOGE("Expected '{' at offset %d", (int)(p - json));
            break;
        }
        p++; /* skip '{' */

        auto_step *s = &g_steps[g_num_steps];
        memset(s, 0, sizeof(*s));
        s->delay_ms = 300; /* default delay for key steps */

        /* Parse key-value pairs within the object */
        while (1) {
            p = skip_ws(p);
            if (*p == '}') { p++; break; }
            if (*p == ',') { p++; continue; }

            /* Parse key name */
            char key[64];
            p = parse_string(p, key, sizeof(key));
            p = skip_ws(p);
            if (*p == ':') p++;
            p = skip_ws(p);

            /* Parse value based on type */
            if (*p == '"') {
                char val[128];
                p = parse_string(p, val, sizeof(val));

                if (strcmp(key, "action") == 0) {
                    if (strcmp(val, "key") == 0)        s->type = STEP_KEY;
                    else if (strcmp(val, "wait_ms") == 0) s->type = STEP_WAIT_MS;
                    else if (strcmp(val, "wait_for") == 0) s->type = STEP_WAIT_FOR;
                    else if (strcmp(val, "introspect") == 0) s->type = STEP_INTROSPECT;
                    else if (strcmp(val, "log") == 0)    s->type = STEP_LOG;
                    else if (strcmp(val, "assert") == 0) s->type = STEP_ASSERT;
                    else LOGE("Unknown action: %s", val);
                }
                else if (strcmp(key, "key") == 0) strncpy(s->key_name, val, sizeof(s->key_name)-1);
                else if (strcmp(key, "field") == 0) strncpy(s->field, val, sizeof(s->field)-1);
                else if (strcmp(key, "value") == 0) strncpy(s->value, val, sizeof(s->value)-1);
                else if (strcmp(key, "message") == 0) strncpy(s->message, val, sizeof(s->message)-1);
            }
            else if (*p >= '0' && *p <= '9' || *p == '-') {
                int val;
                p = parse_int(p, &val);

                if (strcmp(key, "delay_ms") == 0)   s->delay_ms = val;
                else if (strcmp(key, "ms") == 0)    s->delay_ms = val;
                else if (strcmp(key, "timeout_ms") == 0) s->timeout_ms = val;
            }
            else if (*p == 't' || *p == 'f') {
                char val[16];
                p = parse_bool(p, val, sizeof(val));
                if (strcmp(key, "value") == 0) strncpy(s->value, val, sizeof(s->value)-1);
            }
            else if (*p == '{') {
                /* Parse "expect": { "key1": "val1", "key2": val2, ... } */
                if (strcmp(key, "expect") == 0) {
                    p++; /* skip '{' */
                    s->num_expects = 0;
                    while (s->num_expects < MAX_EXPECTS) {
                        p = skip_ws(p);
                        if (*p == '}') { p++; break; }
                        if (*p == ',') { p++; continue; }
                        if (*p != '"') break;
                        assert_expect *ae = &s->expects[s->num_expects];
                        p = parse_string(p, ae->key, sizeof(ae->key));
                        p = skip_ws(p);
                        if (*p == ':') p++;
                        p = skip_ws(p);
                        if (*p == '"') {
                            p = parse_string(p, ae->value, sizeof(ae->value));
                        } else if (*p == 't' || *p == 'f') {
                            p = parse_bool(p, ae->value, sizeof(ae->value));
                        } else if ((*p >= '0' && *p <= '9') || *p == '-') {
                            int iv;
                            p = parse_int(p, &iv);
                            snprintf(ae->value, sizeof(ae->value), "%d", iv);
                        } else {
                            /* skip unknown */
                            while (*p && *p != ',' && *p != '}') p++;
                            continue;
                        }
                        s->num_expects++;
                    }
                } else {
                    /* Skip unknown object */
                    int depth = 1;
                    p++;
                    while (*p && depth > 0) {
                        if (*p == '{') depth++;
                        else if (*p == '}') depth--;
                        p++;
                    }
                }
            }
            else if (*p == '[') {
                /* Skip unknown array */
                int depth = 1;
                p++;
                while (*p && depth > 0) {
                    if (*p == '[') depth++;
                    else if (*p == ']') depth--;
                    p++;
                }
            }
            else {
                /* Skip unknown value */
                while (*p && *p != ',' && *p != '}') p++;
            }
        }

        g_num_steps++;
    }

    LOGI("Parsed %d automation steps", g_num_steps);
    return g_num_steps;
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

    size_t read = fread(buf, 1, len, f);
    fclose(f);
    buf[read] = '\0';

    int result = parse_script(buf);
    free(buf);
    return result;
}

/* ── Assert: check introspection JSON values ───────────────────────── */

/*
 * Find "key": <value> in a JSON string and extract the raw value text.
 * Returns 1 if found, 0 if not.  Writes the value (unquoted) into buf.
 */
static int json_find_value(const char *json, const char *key, char *buf, size_t buflen) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *pos = json;
    while ((pos = strstr(pos, pattern)) != NULL) {
        /* Make sure this is a real key (preceded by { or , or whitespace) */
        pos += strlen(pattern);
        /* skip whitespace and colon */
        while (*pos && (*pos == ' ' || *pos == '\t' || *pos == ':')) pos++;
        if (!*pos) return 0;

        /* Extract the value */
        if (*pos == '"') {
            /* Quoted string value */
            pos++;
            size_t i = 0;
            while (*pos && *pos != '"' && i < buflen - 1) {
                buf[i++] = *pos++;
            }
            buf[i] = '\0';
            return 1;
        } else {
            /* Unquoted: number, bool, null */
            size_t i = 0;
            while (*pos && *pos != ',' && *pos != '}' && *pos != ']' &&
                   !isspace((unsigned char)*pos) && i < buflen - 1) {
                buf[i++] = *pos++;
            }
            buf[i] = '\0';
            return 1;
        }
    }
    return 0;
}

/*
 * Run all assertions for a STEP_ASSERT.
 * Returns 1 if all pass, 0 on first failure (logs the failure).
 */
static int run_assertions(auto_step *s) {
    char *json = game_introspect_get_state();
    if (!json) {
        LOGE("ASSERT_FAIL: Could not get introspection state");
        return 0;
    }

    for (int i = 0; i < s->num_expects; i++) {
        char actual[128];
        if (!json_find_value(json, s->expects[i].key, actual, sizeof(actual))) {
            LOGE("ASSERT_FAIL: key \"%s\" not found in introspection JSON", s->expects[i].key);
            LOGI("ASSERT_EXPECTED: \"%s\" = \"%s\"", s->expects[i].key, s->expects[i].value);
            free(json);
            return 0;
        }
        if (strcasecmp(actual, s->expects[i].value) != 0) {
            LOGE("ASSERT_FAIL: \"%s\" expected \"%s\" but got \"%s\"",
                 s->expects[i].key, s->expects[i].value, actual);
            free(json);
            return 0;
        }
        LOGI("ASSERT_PASS: \"%s\" == \"%s\"", s->expects[i].key, s->expects[i].value);
    }

    free(json);
    return 1;
}

/* ── Advance to next step ───────────────────────────────────────────── */

static void stop_script_fail(const char *reason) {
    LOGE("SCRIPT_RESULT: FAIL at step %d/%d — %s", g_current_step + 1, g_num_steps, reason);
    g_active = 0;
    g_failed = 1;
}

static void advance_step(void) {
    g_current_step++;
    g_step_start = SDL_GetTicks();
    g_key_phase = 0;

    if (g_current_step >= g_num_steps) {
        LOGI("SCRIPT_RESULT: PASS (%d steps)", g_num_steps);
        g_active = 0;
        g_failed = 0;
    } else {
        auto_step *s = &g_steps[g_current_step];
        LOGI("Step %d/%d: type=%d", g_current_step + 1, g_num_steps, (int)s->type);
    }
}

/* ── Public API ─────────────────────────────────────────────────────── */

void game_automate_set_path(const char *dir_path) {
    if (dir_path) {
        strncpy(g_automate_dir, dir_path, sizeof(g_automate_dir) - 1);
        g_automate_dir[sizeof(g_automate_dir) - 1] = '\0';
        LOGI("Automation dir set: %s", g_automate_dir);
    }
}

void game_automate_load_script(const char *script_path) {
    if (script_path) {
        strncpy(g_pending_script, script_path, sizeof(g_pending_script) - 1);
        g_pending_script[sizeof(g_pending_script) - 1] = '\0';
        g_load_requested = 1;
        LOGI("Script load requested: %s", script_path);
    }
}

void game_automate_tick(void) {
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
            LOGI("Script started: %d steps", g_num_steps);

            if (g_num_steps > 0) {
                auto_step *s = &g_steps[0];
                LOGI("Step 1/%d: type=%d", g_num_steps, (int)s->type);
            }
        }
    }

    if (!g_active || g_current_step >= g_num_steps)
        return;

    auto_step *s = &g_steps[g_current_step];
    Uint32 now = SDL_GetTicks();
    Uint32 elapsed = now - g_step_start;

    switch (s->type) {
    case STEP_KEY:
        if (g_key_phase == 0) {
            /* Inject the key tap */
            inject_key_tap(s->key_name);
            g_key_phase = 1;
            g_step_start = now; /* reset timer for delay phase */
        } else if (g_key_phase == 1) {
            /* Wait for delay_ms after key press */
            if (elapsed >= (Uint32)s->delay_ms) {
                advance_step();
            }
        }
        break;

    case STEP_WAIT_MS:
        if (elapsed >= (Uint32)s->delay_ms) {
            LOGI("Wait completed: %d ms", s->delay_ms);
            advance_step();
        }
        break;

    case STEP_WAIT_FOR:
        if (check_condition(s->field, s->value)) {
            LOGI("Condition met: %s = %s (after %u ms)", s->field, s->value, elapsed);
            advance_step();
        } else if (s->timeout_ms > 0 && elapsed >= (Uint32)s->timeout_ms) {
            char reason[256];
            snprintf(reason, sizeof(reason), "TIMEOUT waiting for %s = %s (after %d ms)",
                     s->field, s->value, s->timeout_ms);
            stop_script_fail(reason);
        }
        break;

    case STEP_INTROSPECT:
        LOGI("Triggering introspection dump");
        game_introspect_request();
        advance_step();
        break;

    case STEP_LOG:
        LOGI("SCRIPT: %s", s->message);
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
