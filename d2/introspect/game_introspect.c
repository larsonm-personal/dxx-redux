/*
 * game_introspect.c — Debug introspection API for AI-assisted testing.
 *
 * Serializes the current game state into a JSON string so that
 * automated tools can query menus, player stats, position, etc.
 * without resorting to screenshot / image analysis.
 *
 * Guarded by INTROSPECT_ON — only compiled into debug Android builds.
 */

#ifdef INTROSPECT_ON

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "game_introspect.h"
#include "window.h"
#include "newmenu.h"
#include "object.h"
#include "player.h"
#include "game.h"
#include "gameseq.h"
#include "inferno.h"
#include "screens.h"
#include "maths.h"
#include "vecmat.h"
#include "weapon.h"

/* ── Helpers to identify front-window types ─────────────────────────── */

/*
 * newmenu_handler / listbox_handler are non-static in newmenu.c, but
 * have no public declaration.  Declare them here so we can compare
 * a window's callback pointer to identify window type.
 */
extern int newmenu_handler(window *wind, d_event *event, void *data);
extern int listbox_handler(window *wind, d_event *event, void *data);

/* window_get_callback / window_get_data — declared in window.h,
 * implemented in window.c (guarded by INTROSPECT_ON). */

/* ── Tiny JSON-builder helpers ──────────────────────────────────────── */

/* Dynamic buffer for building JSON without a library. */
typedef struct {
    char  *buf;
    size_t len;       /* current string length (excluding NUL) */
    size_t cap;       /* allocated capacity */
} jbuf_t;

static void jb_init(jbuf_t *j) {
    j->cap = 4096;
    j->buf = (char *)malloc(j->cap);
    j->buf[0] = '\0';
    j->len = 0;
}

static void jb_ensure(jbuf_t *j, size_t extra) {
    while (j->len + extra + 1 > j->cap) {
        j->cap *= 2;
        j->buf = (char *)realloc(j->buf, j->cap);
    }
}

static void jb_cat(jbuf_t *j, const char *s) {
    size_t n = strlen(s);
    jb_ensure(j, n);
    memcpy(j->buf + j->len, s, n + 1);
    j->len += n;
}

/* Append a printf-formatted string. */
static void jb_printf(jbuf_t *j, const char *fmt, ...) {
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    jb_cat(j, tmp);
}

/* Append a JSON-escaped string value (with surrounding quotes). */
static void jb_string(jbuf_t *j, const char *s) {
    if (!s) { jb_cat(j, "null"); return; }
    jb_ensure(j, strlen(s) * 2 + 2);
    j->buf[j->len++] = '"';
    for (; *s; s++) {
        switch (*s) {
            case '"':  j->buf[j->len++] = '\\'; j->buf[j->len++] = '"';  break;
            case '\\': j->buf[j->len++] = '\\'; j->buf[j->len++] = '\\'; break;
            case '\n': j->buf[j->len++] = '\\'; j->buf[j->len++] = 'n';  break;
            case '\r': j->buf[j->len++] = '\\'; j->buf[j->len++] = 'r';  break;
            case '\t': j->buf[j->len++] = '\\'; j->buf[j->len++] = 't';  break;
            default:
                if ((unsigned char)*s < 0x20) {
                    /* Skip non-printable control characters */
                } else {
                    j->buf[j->len++] = *s;
                }
                break;
        }
    }
    j->buf[j->len++] = '"';
    j->buf[j->len] = '\0';
}

/* ── NM_TYPE → string ───────────────────────────────────────────────── */
static const char *nm_type_name(int type) {
    switch (type) {
        case NM_TYPE_MENU:       return "menu";
        case NM_TYPE_INPUT:      return "input";
        case NM_TYPE_CHECK:      return "check";
        case NM_TYPE_RADIO:      return "radio";
        case NM_TYPE_TEXT:       return "text";
        case NM_TYPE_NUMBER:     return "number";
        case NM_TYPE_INPUT_MENU: return "input_menu";
        case NM_TYPE_SLIDER:     return "slider";
        default:                 return "unknown";
    }
}

/* ── Screen mode → string ───────────────────────────────────────────── */
static const char *screen_mode_name(int mode) {
    switch (mode) {
        case SCREEN_MENU:   return "menu";
        case SCREEN_GAME:   return "game";
        case SCREEN_EDITOR: return "editor";
        case SCREEN_MOVIE:  return "movie";
        default:            return "unknown";
    }
}

/* ── Serialize a newmenu ────────────────────────────────────────────── */
static void serialize_newmenu(jbuf_t *j, void *data) {
    newmenu *menu = (newmenu *)data;
    newmenu_item *items = newmenu_get_items(menu);
    int nitems = newmenu_get_nitems(menu);
    int citem  = newmenu_get_citem(menu);
    const char *title    = newmenu_get_title(menu);
    const char *subtitle = newmenu_get_subtitle(menu);

    jb_cat(j, "\"menu\": {");
    jb_cat(j, "\"type\": \"newmenu\",");
    jb_cat(j, "\"title\": "); jb_string(j, title); jb_cat(j, ",");
    jb_cat(j, "\"subtitle\": "); jb_string(j, subtitle); jb_cat(j, ",");
    jb_printf(j, "\"selected_index\": %d,", citem);
    jb_printf(j, "\"num_items\": %d,", nitems);
    jb_printf(j, "\"scroll_offset\": %d,", newmenu_get_scroll_offset(menu));
    jb_printf(j, "\"is_scroll_box\": %s,", newmenu_get_is_scroll_box(menu) ? "true" : "false");
    jb_cat(j, "\"items\": [");

    for (int i = 0; i < nitems; i++) {
        if (i > 0) jb_cat(j, ",");
        jb_cat(j, "{");
        jb_printf(j, "\"index\": %d,", i);
        jb_cat(j, "\"type\": "); jb_string(j, nm_type_name(items[i].type)); jb_cat(j, ",");
        jb_cat(j, "\"text\": "); jb_string(j, items[i].text); jb_cat(j, ",");
        jb_printf(j, "\"value\": %d,", items[i].value);
        jb_printf(j, "\"selected\": %s", (i == citem) ? "true" : "false");
        /* For sliders / numbers, include min/max */
        if (items[i].type == NM_TYPE_SLIDER || items[i].type == NM_TYPE_NUMBER) {
            jb_printf(j, ",\"min\": %d,\"max\": %d", items[i].min_value, items[i].max_value);
        }
        jb_cat(j, "}");
    }
    jb_cat(j, "]}");
}

/* ── Serialize a listbox ────────────────────────────────────────────── */
static void serialize_listbox(jbuf_t *j, void *data) {
    listbox *lb = (listbox *)data;
    char **items = listbox_get_items(lb);
    int nitems   = listbox_get_nitems(lb);
    int citem    = listbox_get_citem(lb);
    const char *title = listbox_get_title(lb);

    jb_cat(j, "\"menu\": {");
    jb_cat(j, "\"type\": \"listbox\",");
    jb_cat(j, "\"title\": "); jb_string(j, title); jb_cat(j, ",");
    jb_printf(j, "\"selected_index\": %d,", citem);
    jb_printf(j, "\"num_items\": %d,", nitems);
    jb_cat(j, "\"items\": [");

    for (int i = 0; i < nitems; i++) {
        if (i > 0) jb_cat(j, ",");
        jb_cat(j, "{");
        jb_printf(j, "\"index\": %d,", i);
        jb_cat(j, "\"text\": "); jb_string(j, items[i]); jb_cat(j, ",");
        jb_printf(j, "\"selected\": %s", (i == citem) ? "true" : "false");
        jb_cat(j, "}");
    }
    jb_cat(j, "]}");
}

/* ── Serialize player + position ────────────────────────────────────── */
static void serialize_player(jbuf_t *j) {
    player *p = &Players[Player_num];
    jb_cat(j, "\"player\": {");
    jb_cat(j, "\"callsign\": "); jb_string(j, p->callsign); jb_cat(j, ",");
    jb_printf(j, "\"energy\": %.1f,",   f2fl(p->energy));
    jb_printf(j, "\"shields\": %.1f,",  f2fl(p->shields));
    jb_printf(j, "\"score\": %d,",      p->score);
    jb_printf(j, "\"lives\": %d,",      (int)p->lives);
    jb_printf(j, "\"level\": %d,",      (int)p->level);
    jb_printf(j, "\"laser_level\": %d,", (int)p->laser_level);
    jb_printf(j, "\"flags\": %u,",      p->flags);
    jb_printf(j, "\"primary_weapon\": %d,",   (int)p->primary_weapon);
    jb_printf(j, "\"secondary_weapon\": %d,", (int)p->secondary_weapon);
    jb_printf(j, "\"primary_weapon_flags\": %u,",   (unsigned)p->primary_weapon_flags);
    jb_printf(j, "\"secondary_weapon_flags\": %u,", (unsigned)p->secondary_weapon_flags);
    jb_printf(j, "\"hostages_on_board\": %d,", (int)p->hostages_on_board);
    jb_printf(j, "\"hostages_level\": %d,",    (int)p->hostages_level);
    jb_printf(j, "\"afterburner_charge\": %.2f,", f2fl(p->afterburner_charge));

    /* Ammo arrays */
    jb_cat(j, "\"primary_ammo\": [");
    for (int i = 0; i < MAX_PRIMARY_WEAPONS; i++) {
        if (i > 0) jb_cat(j, ",");
        jb_printf(j, "%u", (unsigned)p->primary_ammo[i]);
    }
    jb_cat(j, "],\"secondary_ammo\": [");
    for (int i = 0; i < MAX_SECONDARY_WEAPONS; i++) {
        if (i > 0) jb_cat(j, ",");
        jb_printf(j, "%u", (unsigned)p->secondary_ammo[i]);
    }
    jb_cat(j, "],");

    /* Key flags decoded */
    jb_printf(j, "\"has_blue_key\": %s,",  (p->flags & PLAYER_FLAGS_BLUE_KEY)  ? "true" : "false");
    jb_printf(j, "\"has_red_key\": %s,",   (p->flags & PLAYER_FLAGS_RED_KEY)   ? "true" : "false");
    jb_printf(j, "\"has_gold_key\": %s,",  (p->flags & PLAYER_FLAGS_GOLD_KEY)  ? "true" : "false");
    jb_printf(j, "\"cloaked\": %s,",       (p->flags & PLAYER_FLAGS_CLOAKED)   ? "true" : "false");
    jb_printf(j, "\"invulnerable\": %s",   (p->flags & PLAYER_FLAGS_INVULNERABLE) ? "true" : "false");

    jb_cat(j, "}");
}

static void serialize_position(jbuf_t *j) {
    if (!ConsoleObject) {
        jb_cat(j, "\"position\": null");
        return;
    }
    jb_cat(j, "\"position\": {");
    jb_printf(j, "\"x\": %.2f,", f2fl(ConsoleObject->pos.x));
    jb_printf(j, "\"y\": %.2f,", f2fl(ConsoleObject->pos.y));
    jb_printf(j, "\"z\": %.2f,", f2fl(ConsoleObject->pos.z));
    jb_printf(j, "\"segment\": %d,", (int)ConsoleObject->segnum);
    jb_printf(j, "\"shields\": %.1f", f2fl(ConsoleObject->shields));
    jb_cat(j, "}");
}

/* ── Main entry point ───────────────────────────────────────────────── */

char *game_introspect_get_state(void) {
    jbuf_t j;
    jb_init(&j);

    jb_cat(&j, "{");

    /* ── General state ──────────────────────────────────────────── */
    jb_cat(&j, "\"screen_mode\": ");
    jb_string(&j, screen_mode_name(Screen_mode));
    jb_cat(&j, ",");

    jb_printf(&j, "\"game_mode\": %d,", Game_mode);
    jb_printf(&j, "\"quitting\": %s,", Quitting ? "true" : "false");
    jb_printf(&j, "\"difficulty\": %d,", Difficulty_level);
    jb_printf(&j, "\"current_level_num\": %d,", Current_level_num);
    jb_cat(&j, "\"current_level_name\": ");
    jb_string(&j, Current_level_name);
    jb_cat(&j, ",");

    int in_game = (Game_wind != NULL && Screen_mode == SCREEN_GAME);
    jb_printf(&j, "\"in_game\": %s,", in_game ? "true" : "false");

    /* ── Window stack ───────────────────────────────────────────── */
    {
        int nwin = 0;
        window *w;
        for (w = window_get_front(); w; w = window_get_prev(w))
            nwin++;
        jb_printf(&j, "\"window_count\": %d,", nwin);
    }

    /* ── Front window (menu) analysis ───────────────────────────── */
    {
        window *front = window_get_front();
        int is_game_front = (front && front == Game_wind);

        jb_printf(&j, "\"game_window_is_front\": %s,", is_game_front ? "true" : "false");

        if (front && !is_game_front) {
            /*
             * The front window is probably a newmenu or listbox.
             * Compare the callback pointer to identify it.
             */
            int (*cb)(window *, d_event *, void *) = window_get_callback(front);
            void *data = window_get_data(front);

            if (cb == (int (*)(window *, d_event *, void *))newmenu_handler && data) {
                serialize_newmenu(&j, data);
                jb_cat(&j, ",");
            } else if (cb == (int (*)(window *, d_event *, void *))listbox_handler && data) {
                serialize_listbox(&j, data);
                jb_cat(&j, ",");
            } else {
                jb_cat(&j, "\"menu\": {\"type\": \"unknown_window\"},");
            }
        } else if (!front) {
            jb_cat(&j, "\"menu\": null,");
        }
    }

    /* ── Player & position (only meaningful when a level is loaded) ── */
    if (Current_level_num != 0) {
        serialize_player(&j);
        jb_cat(&j, ",");
        serialize_position(&j);
    } else {
        jb_cat(&j, "\"player\": null,\"position\": null");
    }

    jb_cat(&j, "}");

    return j.buf;   /* Caller must free() */
}

/* ── On-demand dump infrastructure ──────────────────────────────────── */

static char introspect_path[512] = "";
static volatile int introspect_requested = 0;

void game_introspect_set_path(const char *path) {
    if (path) {
        strncpy(introspect_path, path, sizeof(introspect_path) - 1);
        introspect_path[sizeof(introspect_path) - 1] = '\0';
    }
}

void game_introspect_request(void) {
    introspect_requested = 1;
}

void game_introspect_check_and_dump(void) {
    if (!introspect_requested || !introspect_path[0])
        return;
    introspect_requested = 0;

    char *json = game_introspect_get_state();
    if (!json)
        return;

    FILE *f = fopen(introspect_path, "w");
    if (f) {
        fputs(json, f);
        fclose(f);
    }
    free(json);
}

#endif /* INTROSPECT_ON */
