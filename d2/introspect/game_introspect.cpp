/*
 * game_introspect.cpp — Debug introspection API for AI-assisted testing.
 *
 * Serializes the current game state into a JSON string so that
 * automated tools can query menus, player stats, position, etc.
 * without resorting to screenshot / image analysis.
 *
 * Uses nlohmann/json for serialization.
 * Guarded by INTROSPECT_ON — only compiled into debug Android builds.
 */

#ifdef INTROSPECT_ON

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

/* Engine headers are pure C — wrap them for C++ linkage. */
extern "C" {
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
#include "automap.h"
#include "playsave.h"
}

/* ── Helpers to identify front-window types ─────────────────────────── */

/*
 * newmenu_handler / listbox_handler are non-static in newmenu.c, but
 * have no public declaration.  Declare them here so we can compare
 * a window's callback pointer to identify window type.
 */
extern "C" int newmenu_handler(window *wind, d_event *event, void *data);
extern "C" int listbox_handler(window *wind, d_event *event, void *data);

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
static json serialize_newmenu(void *data) {
    newmenu *menu = (newmenu *)data;
    newmenu_item *items = newmenu_get_items(menu);
    int nitems = newmenu_get_nitems(menu);
    int citem  = newmenu_get_citem(menu);
    const char *title    = newmenu_get_title(menu);
    const char *subtitle = newmenu_get_subtitle(menu);

    json menu_items = json::array();
    for (int i = 0; i < nitems; i++) {
        json item = {
            {"index", i},
            {"type", nm_type_name(items[i].type)},
            {"text", items[i].text ? items[i].text : ""},
            {"value", items[i].value},
            {"selected", i == citem}
        };
        if (items[i].type == NM_TYPE_SLIDER || items[i].type == NM_TYPE_NUMBER) {
            item["min"] = items[i].min_value;
            item["max"] = items[i].max_value;
        }
        menu_items.push_back(std::move(item));
    }

    return {
        {"type", "newmenu"},
        {"title", title ? title : ""},
        {"subtitle", subtitle ? subtitle : ""},
        {"selected_index", citem},
        {"num_items", nitems},
        {"scroll_offset", newmenu_get_scroll_offset(menu)},
        {"is_scroll_box", (bool)newmenu_get_is_scroll_box(menu)},
        {"items", std::move(menu_items)}
    };
}

/* ── Serialize a listbox ────────────────────────────────────────────── */
static json serialize_listbox_data(void *data) {
    listbox *lb = (listbox *)data;
    char **items = listbox_get_items(lb);
    int nitems   = listbox_get_nitems(lb);
    int citem    = listbox_get_citem(lb);
    const char *title = listbox_get_title(lb);

    json menu_items = json::array();
    for (int i = 0; i < nitems; i++) {
        menu_items.push_back({
            {"index", i},
            {"text", items[i] ? items[i] : ""},
            {"selected", i == citem}
        });
    }

    return {
        {"type", "listbox"},
        {"title", title ? title : ""},
        {"selected_index", citem},
        {"num_items", nitems},
        {"items", std::move(menu_items)}
    };
}

/* ── Serialize player ───────────────────────────────────────────────── */
static json serialize_player() {
    player *p = &Players[Player_num];

    json primary_ammo = json::array();
    for (int i = 0; i < MAX_PRIMARY_WEAPONS; i++)
        primary_ammo.push_back((unsigned)p->primary_ammo[i]);

    json secondary_ammo = json::array();
    for (int i = 0; i < MAX_SECONDARY_WEAPONS; i++)
        secondary_ammo.push_back((unsigned)p->secondary_ammo[i]);

    return {
        {"callsign", p->callsign},
        {"energy", f2fl(p->energy)},
        {"shields", f2fl(p->shields)},
        {"score", p->score},
        {"lives", (int)p->lives},
        {"level", (int)p->level},
        {"laser_level", (int)p->laser_level},
        {"flags", p->flags},
        {"primary_weapon", (int)p->primary_weapon},
        {"secondary_weapon", (int)p->secondary_weapon},
        {"primary_weapon_flags", (unsigned)p->primary_weapon_flags},
        {"secondary_weapon_flags", (unsigned)p->secondary_weapon_flags},
        {"hostages_on_board", (int)p->hostages_on_board},
        {"hostages_level", (int)p->hostages_level},
        {"afterburner_charge", f2fl(p->afterburner_charge)},
        {"primary_ammo", std::move(primary_ammo)},
        {"secondary_ammo", std::move(secondary_ammo)},
        {"has_blue_key", (bool)(p->flags & PLAYER_FLAGS_BLUE_KEY)},
        {"has_red_key", (bool)(p->flags & PLAYER_FLAGS_RED_KEY)},
        {"has_gold_key", (bool)(p->flags & PLAYER_FLAGS_GOLD_KEY)},
        {"cloaked", (bool)(p->flags & PLAYER_FLAGS_CLOAKED)},
        {"invulnerable", (bool)(p->flags & PLAYER_FLAGS_INVULNERABLE)}
    };
}

/* ── Serialize position ─────────────────────────────────────────────── */
static json serialize_position() {
    if (!ConsoleObject)
        return nullptr;

    return {
        {"x", f2fl(ConsoleObject->pos.x)},
        {"y", f2fl(ConsoleObject->pos.y)},
        {"z", f2fl(ConsoleObject->pos.z)},
        {"segment", (int)ConsoleObject->segnum},
        {"shields", f2fl(ConsoleObject->shields)}
    };
}

/* ── Main entry point ───────────────────────────────────────────────── */

extern "C" char *game_introspect_get_state(void) {
    json j;

    /* ── General state ──────────────────────────────────────────── */
    j["screen_mode"] = screen_mode_name(Screen_mode);
    j["game_mode"] = Game_mode;
    j["quitting"] = (bool)Quitting;
    j["difficulty"] = Difficulty_level;
    j["current_level_num"] = Current_level_num;
    j["current_level_name"] = Current_level_name;

    bool in_game = (Game_wind != NULL && Screen_mode == SCREEN_GAME);
    j["in_game"] = in_game;

    /* ── Window stack ───────────────────────────────────────────── */
    {
        int nwin = 0;
        window *w;
        for (w = window_get_front(); w; w = window_get_prev(w))
            nwin++;
        j["window_count"] = nwin;
    }

    /* ── Front window (menu) analysis ───────────────────────────── */
    {
        window *front = window_get_front();
        bool is_game_front = (front && front == Game_wind);
        j["game_window_is_front"] = is_game_front;

        if (front && !is_game_front) {
            int (*cb)(window *, d_event *, void *) = window_get_callback(front);
            void *data = window_get_data(front);

            if (cb == (int (*)(window *, d_event *, void *))newmenu_handler && data) {
                j["menu"] = serialize_newmenu(data);
            } else if (cb == (int (*)(window *, d_event *, void *))listbox_handler && data) {
                j["menu"] = serialize_listbox_data(data);
            } else {
                j["menu"] = {{"type", "unknown_window"}};
            }
        } else if (!front) {
            j["menu"] = nullptr;
        }
    }

    /* ── Automap ──────────────────────────────────────────────── */
    j["automap_active"] = (bool)Automap_active;
    {
        automap_view_info avi;
        if (automap_get_view_info(&avi)) {
            j["automap"] = {
                {"freeflight", (bool)avi.freeflight},
                {"view_x", f2fl(avi.view_pos.x)},
                {"view_y", f2fl(avi.view_pos.y)},
                {"view_z", f2fl(avi.view_pos.z)},
                {"target_x", f2fl(avi.view_target.x)},
                {"target_y", f2fl(avi.view_target.y)},
                {"target_z", f2fl(avi.view_target.z)},
                {"view_dist", f2fl(avi.viewDist)},
                {"zoom", f2fl(avi.zoom)},
                {"tangles_p", avi.tangles.p},
                {"tangles_h", avi.tangles.h},
                {"tangles_b", avi.tangles.b}
            };
        } else {
            j["automap"] = nullptr;
        }
    }

    /* ── Player & position (only meaningful when a level is loaded) ── */
    if (Current_level_num != 0) {
        j["player"] = serialize_player();
        j["position"] = serialize_position();
    } else {
        j["player"] = nullptr;
        j["position"] = nullptr;
    }

    /* Serialize to string and return as malloc'd C string */
    std::string result = j.dump();
    char *buf = (char *)malloc(result.size() + 1);
    if (buf) {
        memcpy(buf, result.c_str(), result.size() + 1);
    }
    return buf;
}

/* ── On-demand dump infrastructure ──────────────────────────────────── */

static char introspect_path[512] = "";
static volatile int introspect_requested = 0;

extern "C" void game_introspect_set_path(const char *path) {
    if (path) {
        strncpy(introspect_path, path, sizeof(introspect_path) - 1);
        introspect_path[sizeof(introspect_path) - 1] = '\0';
    }
}

extern "C" void game_introspect_request(void) {
    introspect_requested = 1;
}

extern "C" void game_introspect_check_and_dump(void) {
    if (!introspect_requested || !introspect_path[0])
        return;
    introspect_requested = 0;

    char *json_str = game_introspect_get_state();
    if (!json_str)
        return;

    FILE *f = fopen(introspect_path, "w");
    if (f) {
        fputs(json_str, f);
        fclose(f);
    }
    free(json_str);
}

#endif /* INTROSPECT_ON */
