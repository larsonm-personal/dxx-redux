/*
 * game_introspect.cpp -- Debug introspection API for AI-assisted testing.
 *
 * Serializes the current game state into a JSON string so that
 * automated tools can query menus, player stats, position, etc.
 * without resorting to screenshot / image analysis.
 *
 * Uses nlohmann/json for serialization.
 * Guarded by INTROSPECT_ON -- only compiled into debug Android builds.
 */

#ifdef INTROSPECT_ON

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

/* Engine headers are pure C -- wrap them for C++ linkage. */
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
#include "kconfig.h"
#include "gr.h"
#include "multi.h"
#include "ogl_init.h"
#include "piggy.h"

/* Android port: hires texture tracking counters from ogl.c */
extern int r_hires_found;
extern int r_hires_loaded;
}

/* D1 does not have SCREEN_MOVIE */
#ifndef SCREEN_MOVIE
#define SCREEN_MOVIE 99
#endif

/* -- EGL surface recreation counter (defined in arch/ogl/gr.c) -- */
extern "C" int ogl_get_egl_recreate_count(void);

/* -- Display surface dimensions (defined in android_surface.c) -- */
extern "C" int android_surface_get_display_width(void);
extern "C" int android_surface_get_display_height(void);

/* -- Redbook audio accessors (defined in rbaudio_bin.c / rbaudio.c) -- */
extern "C" {
int RBAEnabled(void);
int RBAGetNumberOfTracks(void);
int RBAGetTrackNum(void);
int RBAPeekPlayStatus(void);
}

/* -- Movie tracking globals (defined in movie.c, D2 only) -- */
#ifdef DXX_BUILD_DESCENT_II
extern "C" {
extern char g_current_movie_name[];
extern int g_last_movie_result;
extern char g_last_movie_name[];
}
#endif /* DXX_BUILD_DESCENT_II movie globals */

/* -- Audio diagnostic accessors (defined in digi_tsf_music.c / SDL_androidaudio.c) -- */
extern "C" {
int tsf_music_get_output_rate(void);
int tsf_music_get_playing(void);
int tsf_music_get_paused(void);
int tsf_music_get_cb_count(void);
int tsf_music_get_cb_overrun_count(void);
int tsf_music_get_clip_count(void);
int tsf_music_get_sample_count(void);
int tsf_music_get_peak_sample(void);
int tsf_music_get_active_voices_max(void);
int tsf_music_get_max_voices(void);
int tsf_music_get_rb_fill(void);
int tsf_music_get_rb_capacity(void);
float tsf_music_get_gain_db(void);
void tsf_music_set_gain_db(float db);
void tsf_music_set_max_voices(int n);
int androidaud_get_play_count(void);
int androidaud_get_enqueue_fail(void);
int androidaud_get_audio_freq(void);
int androidaud_get_audio_buf_frames(void);
}

#include <SDL_mixer.h>

#include <physfs.h>

#include "console_ringbuf.h"
#include "overlay_ringbuf.h"
#include "debug_tex_overlay.h"

/* -- Helpers to identify front-window types --------------------------- */

/*
 * newmenu_handler / listbox_handler are non-static in newmenu.c, but
 * have no public declaration.  Declare them here so we can compare
 * a window's callback pointer to identify window type.
 */
extern "C" int newmenu_handler(window *wind, d_event *event, void *data);
extern "C" int listbox_handler(window *wind, d_event *event, void *data);
extern "C" int kconfig_handler(window *wind, d_event *event, void *data);

/* -- Joystick binding introspection helpers (defined in kconfig.c) -- */
extern "C" int kconfig_get_joystick_count(void);
extern "C" void kconfig_get_joystick_item(int idx, const char **name, int *type, int *value);

/* -- BT_TYPE -> string (kconfig control types) ------------------------ */
static const char *bt_type_name(int type)
{
	switch (type) {
		case 0: return "key";
		case 1: return "mouse_button";
		case 2: return "mouse_axis";
		case 3: return "joy_button";
		case 4: return "joy_axis";
		case 5: return "invert";
		default: return "unknown";
	}
}

/* -- NM_TYPE -> string ------------------------------------------------- */
static const char *nm_type_name(int type)
{
	switch (type) {
		case NM_TYPE_MENU: return "menu";
		case NM_TYPE_INPUT: return "input";
		case NM_TYPE_CHECK: return "check";
		case NM_TYPE_RADIO: return "radio";
		case NM_TYPE_TEXT: return "text";
		case NM_TYPE_NUMBER: return "number";
		case NM_TYPE_INPUT_MENU: return "input_menu";
		case NM_TYPE_SLIDER: return "slider";
		default: return "unknown";
	}
}

/* -- Screen mode -> string --------------------------------------------- */
static const char *screen_mode_name(int mode)
{
	switch (mode) {
		case SCREEN_MENU: return "menu";
		case SCREEN_GAME: return "game";
		case SCREEN_EDITOR: return "editor";
		case SCREEN_MOVIE: return "movie";
		default: return "unknown";
	}
}

/* -- Serialize a newmenu ---------------------------------------------- */
static json serialize_newmenu(void *data)
{
	newmenu *menu = (newmenu *) data;
	newmenu_item *items = newmenu_get_items(menu);
	int nitems = newmenu_get_nitems(menu);
	int citem = newmenu_get_citem(menu);
	const char *title = newmenu_get_title(menu);
	const char *subtitle = newmenu_get_subtitle(menu);

	/* Get canvas offset so we can report absolute screen positions */
	window *wind = newmenu_get_window(menu);
	grs_canvas *canvas = wind ? window_get_canvas(wind) : NULL;
	int cx = canvas ? canvas->cv_bitmap.bm_x : 0;
	int cy = canvas ? canvas->cv_bitmap.bm_y : 0;

	json menu_items = json::array();
	for (int i = 0; i < nitems; i++) {
		json item = {
			{ "index", i },
			{ "type", nm_type_name(items[i].type) },
			{ "text", items[i].text ? items[i].text : "" },
			{ "value", items[i].value },
			{ "selected", i == citem },
			{ "x", cx + (int) items[i].x },
			{ "y", cy + (int) items[i].y },
			{ "w", (int) items[i].w },
			{ "h", (int) items[i].h }
		};
		if (items[i].type == NM_TYPE_SLIDER || items[i].type == NM_TYPE_NUMBER) {
			item["min"] = items[i].min_value;
			item["max"] = items[i].max_value;
		}
		menu_items.push_back(std::move(item));
	}

	return {
		{ "type", "newmenu" },
		{ "title", title ? title : "" },
		{ "subtitle", subtitle ? subtitle : "" },
		{ "selected_index", citem },
		{ "num_items", nitems },
		{ "scroll_offset", newmenu_get_scroll_offset(menu) },
		{ "is_scroll_box", (bool) newmenu_get_is_scroll_box(menu) },
		{ "items", std::move(menu_items) }
	};
}

/* -- Serialize a listbox ---------------------------------------------- */
static json serialize_listbox_data(void *data)
{
	listbox *lb = (listbox *) data;
	char **items = listbox_get_items(lb);
	int nitems = listbox_get_nitems(lb);
	int citem = listbox_get_citem(lb);
	const char *title = listbox_get_title(lb);

	json menu_items = json::array();
	for (int i = 0; i < nitems; i++) {
		menu_items.push_back({ { "index", i },
		                       { "text", items[i] ? items[i] : "" },
		                       { "selected", i == citem } });
	}

	return {
		{ "type", "listbox" },
		{ "title", title ? title : "" },
		{ "selected_index", citem },
		{ "num_items", nitems },
		{ "items", std::move(menu_items) }
	};
}

/* -- Serialize player ------------------------------------------------- */
static json serialize_player()
{
	player *p = &Players[Player_num];

	json primary_ammo = json::array();
	for (int i = 0; i < MAX_PRIMARY_WEAPONS; i++)
		primary_ammo.push_back((unsigned) p->primary_ammo[i]);

	json secondary_ammo = json::array();
	for (int i = 0; i < MAX_SECONDARY_WEAPONS; i++)
		secondary_ammo.push_back((unsigned) p->secondary_ammo[i]);

	return {
		{ "callsign", p->callsign },
		{ "energy", f2fl(p->energy) },
		{ "shields", f2fl(p->shields) },
		{ "score", p->score },
		{ "lives", (int) p->lives },
		{ "level", (int) p->level },
		{ "laser_level", (int) p->laser_level },
		{ "flags", p->flags },
		{ "primary_weapon", (int) p->primary_weapon },
		{ "secondary_weapon", (int) p->secondary_weapon },
		{ "primary_weapon_flags", (unsigned) p->primary_weapon_flags },
		{ "secondary_weapon_flags", (unsigned) p->secondary_weapon_flags },
		{ "hostages_on_board", (int) p->hostages_on_board },
		{ "hostages_level", (int) p->hostages_level },
#ifdef DXX_BUILD_DESCENT_II
		{ "afterburner_charge", f2fl(p->afterburner_charge) },
#endif
		{ "primary_ammo", std::move(primary_ammo) },
		{ "secondary_ammo", std::move(secondary_ammo) },
		{ "has_blue_key", (bool) (p->flags & PLAYER_FLAGS_BLUE_KEY) },
		{ "has_red_key", (bool) (p->flags & PLAYER_FLAGS_RED_KEY) },
		{ "has_gold_key", (bool) (p->flags & PLAYER_FLAGS_GOLD_KEY) },
		{ "cloaked", (bool) (p->flags & PLAYER_FLAGS_CLOAKED) },
		{ "invulnerable", (bool) (p->flags & PLAYER_FLAGS_INVULNERABLE) }
	};
}

/* -- Serialize position ----------------------------------------------- */
static json serialize_position()
{
	if (!ConsoleObject)
		return nullptr;

	json pos = {
		{ "x", f2fl(ConsoleObject->pos.x) },
		{ "y", f2fl(ConsoleObject->pos.y) },
		{ "z", f2fl(ConsoleObject->pos.z) },
		{ "segment", (int) ConsoleObject->segnum },
		{ "shields", f2fl(ConsoleObject->shields) },
		/* Forward vector -- lets tests detect orientation changes */
		{ "fvec_x", f2fl(ConsoleObject->orient.fvec.x) },
		{ "fvec_y", f2fl(ConsoleObject->orient.fvec.y) },
		{ "fvec_z", f2fl(ConsoleObject->orient.fvec.z) }
	};
	return pos;
}

/* -- Main entry point ------------------------------------------------- */

extern "C" char *game_introspect_get_state(void)
{
	json j;

	/* -- General state -------------------------------------------- */
	j["screen_mode"] = screen_mode_name(Screen_mode);
	j["game_mode"] = Game_mode;
	j["quitting"] = (bool) Quitting;
	j["difficulty"] = Difficulty_level;
	j["current_level_num"] = Current_level_num;
	j["current_level_name"] = Current_level_name;

	bool in_game = (Game_wind != NULL && Screen_mode == SCREEN_GAME);
	j["in_game"] = in_game;

	/* -- Multiplayer state ---------------------------------------- */
	{
		bool is_network = (Game_mode & GM_NETWORK) != 0;
		j["is_network"] = is_network;
		if (is_network) {
			json mp;
			mp["num_players"] = (int) N_players;
			mp["num_connected"] = (int) Netgame.numconnected;
			mp["max_players"] = (int) Netgame.max_numplayers;
			mp["game_name"] = std::string(Netgame.game_name);
			mp["mission_title"] = std::string(Netgame.mission_title);
			mp["mission_name"] = std::string(Netgame.mission_name);
			mp["level_num"] = (int) Netgame.levelnum;
			mp["gamemode"] = (int) Netgame.gamemode;
			const char *mode_name;
			switch (Netgame.gamemode) {
				case NETGAME_ANARCHY: mode_name = "anarchy"; break;
				case NETGAME_TEAM_ANARCHY: mode_name = "team_anarchy"; break;
				case NETGAME_ROBOT_ANARCHY: mode_name = "robot_anarchy"; break;
				case NETGAME_COOPERATIVE: mode_name = "cooperative"; break;
#ifdef NETGAME_CAPTURE_FLAG
				case NETGAME_CAPTURE_FLAG: mode_name = "capture_flag"; break;
#endif
#ifdef NETGAME_HOARD
				case NETGAME_HOARD: mode_name = "hoard"; break;
#endif
#ifdef NETGAME_TEAM_HOARD
				case NETGAME_TEAM_HOARD: mode_name = "team_hoard"; break;
#endif
				case NETGAME_BOUNTY: mode_name = "bounty"; break;
				default: mode_name = "unknown"; break;
			}
			mp["gamemode_name"] = mode_name;
			mp["difficulty"] = (int) Netgame.difficulty;
			mp["game_status"] = (int) Netgame.game_status;
			mp["network_status"] = Network_status;
			mp["my_player_num"] = Player_num;

			json players_arr = json::array();
			for (int i = 0; i < N_players && i < MAX_PLAYERS; i++) {
				json pl;
				pl["slot"] = i;
				pl["callsign"] = std::string(Players[i].callsign);
				pl["connected"] = (int) Players[i].connected;
				pl["score"] = (int) Players[i].score;
				pl["kills"] = (int) Players[i].net_kills_total;
				pl["deaths"] = (int) Players[i].net_killed_total;
				pl["shields"] = f2fl(Players[i].shields);
				pl["energy"] = f2fl(Players[i].energy);
				pl["is_me"] = (i == Player_num);
				players_arr.push_back(std::move(pl));
			}
			mp["players"] = std::move(players_arr);
			j["multiplayer"] = std::move(mp);
		}
	}

	j["egl_recreate_count"] = ogl_get_egl_recreate_count();

	/* -- Render and display resolution -------------------------------- */
	{
		json res;
		if (grd_curscreen) {
			res["render_width"] = (int) grd_curscreen->sc_w;
			res["render_height"] = (int) grd_curscreen->sc_h;
		}
		res["display_width"] = android_surface_get_display_width();
		res["display_height"] = android_surface_get_display_height();
		j["resolution"] = std::move(res);
	}

	/* -- Death state -------------------------------------------- */
	j["player_dead"] = (bool) Player_is_dead;
	j["player_exploded"] = (bool) Player_exploded;

	/* -- Debug flags -------------------------------------------- */
	{
		json dbg;
		dbg["tex_overlay"] = (bool) g_debug_tex_overlay_active;
		j["debug_flags"] = std::move(dbg);
	}

	/* -- Endlevel sequence --------------------------------------- */
	{
		extern int Endlevel_sequence;
		j["endlevel_sequence"] = Endlevel_sequence;
	}

	/* -- Window stack --------------------------------------------- */
	{
		int nwin = 0;
		window *w;
		for (w = window_get_front(); w; w = window_get_prev(w))
			nwin++;
		j["window_count"] = nwin;
	}

	/* -- Front window (menu) analysis ----------------------------- */
	{
		window *front = window_get_front();
		bool is_game_front = (front && front == Game_wind);
		j["game_window_is_front"] = is_game_front;

		if (front && !is_game_front) {
			int (*cb)(window *, d_event *, void *) = window_get_callback(front);
			void *data = window_get_data(front);

			if (cb == (int (*)(window *, d_event *, void *)) newmenu_handler && data) {
				j["menu"] = serialize_newmenu(data);
			} else if (cb == (int (*)(window *, d_event *, void *)) listbox_handler && data) {
				j["menu"] = serialize_listbox_data(data);
			} else if (cb == (int (*)(window *, d_event *, void *)) kconfig_handler) {
				j["menu"] = { { "type", "kconfig" } };
			} else {
				j["menu"] = { { "type", "unknown_window" } };
			}
		} else if (!front) {
			j["menu"] = nullptr;
		}

		/* Flat menu keys for easy assertion */
		if (j.contains("menu") && j["menu"].is_object()) {
			if (j["menu"].contains("type"))
				j["menu_type"] = j["menu"]["type"];
			if (j["menu"].contains("title"))
				j["menu_title"] = j["menu"]["title"];
			if (j["menu"].contains("subtitle"))
				j["menu_subtitle"] = j["menu"]["subtitle"];
		}
	}

	/* -- Joystick control bindings (always available) ------------- */
	{
		int n = kconfig_get_joystick_count();
		json items = json::array();
		int bound_count = 0;
		int bound_controls = 0; /* excludes invert flags */
		for (int i = 0; i < n; i++) {
			const char *name = "";
			int type = -1, value = 255;
			kconfig_get_joystick_item(i, &name, &type, &value);
			bool bound = (value != 255);
			if (bound) bound_count++;
			if (bound && type != 5 /* BT_INVERT */) bound_controls++;
			items.push_back({ { "index", i },
			                  { "name", name ? name : "" },
			                  { "type", bt_type_name(type) },
			                  { "value", value },
			                  { "bound", bound } });
		}
		j["joystick_controls"] = {
			{ "control_type", (int) PlayerCfg.ControlType },
			{ "bound_count", bound_count },
			{ "bound_controls", bound_controls },
			{ "total_count", n },
			{ "items", std::move(items) }
		};
	}

	/* -- Automap ------------------------------------------------ */
	j["automap_active"] = (bool) Automap_active;
#ifdef DXX_BUILD_DESCENT_II
	{
		automap_view_info avi;
		if (automap_get_view_info(&avi)) {
			j["automap"] = {
				{ "freeflight", (bool) avi.freeflight },
				{ "view_x", f2fl(avi.view_pos.x) },
				{ "view_y", f2fl(avi.view_pos.y) },
				{ "view_z", f2fl(avi.view_pos.z) },
				{ "target_x", f2fl(avi.view_target.x) },
				{ "target_y", f2fl(avi.view_target.y) },
				{ "target_z", f2fl(avi.view_target.z) },
				{ "view_dist", f2fl(avi.viewDist) },
				{ "zoom", f2fl(avi.zoom) },
				{ "tangles_p", avi.tangles.p },
				{ "tangles_h", avi.tangles.h },
				{ "tangles_b", avi.tangles.b }
			};
		} else {
			j["automap"] = nullptr;
		}
	}
#endif /* DXX_BUILD_DESCENT_II automap_view_info */

	/* -- Audio diagnostics ----------------------------------------- */
	{
		int freq = 0;
		Uint16 fmt = 0;
		int ch = 0;
		int mixer_open = Mix_QuerySpec(&freq, &fmt, &ch);
		json audio = {
			{ "mixer_open", (bool) mixer_open },
			{ "mixer_freq", freq },
			{ "mixer_format", (int) fmt },
			{ "mixer_channels", ch },
			{ "tsf_output_rate", tsf_music_get_output_rate() },
			{ "tsf_playing", (bool) tsf_music_get_playing() },
			{ "tsf_paused", (bool) tsf_music_get_paused() },
			{ "tsf_cb_count", tsf_music_get_cb_count() },
			{ "tsf_rb_underruns", tsf_music_get_cb_overrun_count() },
			{ "tsf_rb_fill", tsf_music_get_rb_fill() },
			{ "tsf_rb_capacity", tsf_music_get_rb_capacity() },
			{ "tsf_rb_fill_pct", tsf_music_get_rb_capacity() > 0
			                         ? (int) (tsf_music_get_rb_fill() * 100 / tsf_music_get_rb_capacity())
			                         : 0 },
			{ "tsf_clip_count", tsf_music_get_clip_count() },
			{ "tsf_sample_count", tsf_music_get_sample_count() },
			{ "tsf_peak_sample", tsf_music_get_peak_sample() },
			{ "tsf_peak_pct", tsf_music_get_sample_count() > 0
			                      ? (int) (tsf_music_get_peak_sample() * 100 / 32767)
			                      : 0 },
			{ "tsf_active_voices_max", tsf_music_get_active_voices_max() },
			{ "tsf_max_voices", tsf_music_get_max_voices() },
			{ "tsf_gain_db", tsf_music_get_gain_db() },
			{ "osl_play_count", androidaud_get_play_count() },
			{ "osl_enqueue_fail", androidaud_get_enqueue_fail() },
			{ "osl_freq", androidaud_get_audio_freq() },
			{ "osl_buf_frames", androidaud_get_audio_buf_frames() }
		};
		j["audio"] = std::move(audio);
	}

	/* -- Redbook audio ---------------------------------------------- */
	{
		json rb = { { "enabled", false } };
		int enabled = RBAEnabled();
		if (enabled) {
			rb["enabled"] = true;
			int status = RBAPeekPlayStatus();
			rb["num_tracks"] = RBAGetNumberOfTracks();
			rb["current_track"] = RBAGetTrackNum();
			rb["play_status"] = (status == 1) ? "playing" : (status == -1) ? "paused"
			                                                               : "stopped";
		}
		j["redbook"] = std::move(rb);
	}

	/* -- Movie state (D2 only) ----------------------------------- */
#ifdef DXX_BUILD_DESCENT_II
	{
		json mv;
		if (g_current_movie_name[0])
			mv["current"] = std::string(g_current_movie_name);
		if (g_last_movie_name[0])
			mv["last_name"] = std::string(g_last_movie_name);
		const char *result_str;
		switch (g_last_movie_result) {
			case 1: result_str = "played_full"; break;
			case 2: result_str = "aborted"; break;
			default: result_str = "not_played"; break;
		}
		mv["last_result"] = result_str;
		j["movie"] = std::move(mv);
	}
#endif

	/* -- Live axis state (always available -- useful for binding tests) -- */
	{
		json axes = json::array();
		for (int i = 0; i < 8; i++) {
			axes.push_back({ { "axis", i },
			                 { "raw", Controls.raw_joy_axis[i] },
			                 { "value", Controls.joy_axis[i] } });
		}
		j["joy_axes"] = std::move(axes);

		/* Flat keys for easy assertion: axis_bind_pitch, etc.
		 * These are the axis numbers that each control reads from. */
		auto get_bind = [](int idx) -> int {
			const char *n;
			int t, v = 255;
			kconfig_get_joystick_item(idx, &n, &t, &v);
			return v;
		};
		j["axis_bind_pitch"] = get_bind(13);
		j["axis_bind_turn"] = get_bind(15);
		j["axis_bind_slide_lr"] = get_bind(17);
		j["axis_bind_slide_ud"] = get_bind(19);
		j["axis_bind_bank"] = get_bind(21);
		j["axis_bind_throttle"] = get_bind(23);

		/* Control timing -- nonzero means the ship is actively rotating/thrusting */
		j["heading_time"] = (int) Controls.heading_time;
		j["pitch_time"] = (int) Controls.pitch_time;
		j["slide_lr_time"] = (int) Controls.sideways_thrust_time;
		j["slide_ud_time"] = (int) Controls.vertical_thrust_time;
		j["bank_time"] = (int) Controls.bank_time;
		j["throttle_time"] = (int) Controls.forward_thrust_time;

		/* Diagnostic: raw axis state and modifier flags for axis test debugging */
		j["slide_on_state"] = (int) Controls.slide_on_state;
		j["bank_on_state"] = (int) Controls.bank_on_state;
		j["control_type"] = (int) PlayerCfg.ControlType;
		{
			auto &rja = j["raw_joy_axis"];
			for (int a = 0; a < 8; a++)
				rja[a] = (int) Controls.raw_joy_axis[a];
		}
	}

	/* -- Player & position (only meaningful when a level is loaded) -- */
	if (Current_level_num != 0) {
		j["player"] = serialize_player();
		j["position"] = serialize_position();

		/* Flat weapon keys for easy assertion */
		player *p = &Players[Player_num];
		j["primary_weapon"] = (int) p->primary_weapon;
		j["secondary_weapon"] = (int) p->secondary_weapon;
	} else {
		j["player"] = nullptr;
		j["position"] = nullptr;
	}

	/* -- Keyboard viewport offset state -------------------------- */
	{
		extern volatile int g_blit_y_offset;
		extern int g_menu_scale_active;
		/* These are in android_input.c -- declared static, so we
		 * expose them via a helper instead of extern. */
		extern int android_get_keyboard_state(int *kb_native, int *scr_native, int *field_y);
		int kb_native = 0, scr_native = 0, field_y_val = 0;
		android_get_keyboard_state(&kb_native, &scr_native, &field_y_val);
		json kb;
		kb["keyboard_height_native"] = kb_native;
		kb["screen_height_native"] = scr_native;
		kb["active_input_field_y"] = field_y_val;
		kb["blit_y_offset"] = (int) g_blit_y_offset;
		kb["scale_blit_active"] = (bool) g_menu_scale_active;
		j["keyboard_viewport"] = kb;
	}

	/* -- Mounted mods (PhysFS search path .dxa entries) --------------- */
	{
		json mods = json::array();
		char **list = PHYSFS_getSearchPath();
		if (list) {
			for (char **i = list; *i != NULL; i++) {
				const char *path = *i;
				size_t len = strlen(path);
				if (len > 4 && strcmp(path + len - 4, ".dxa") == 0)
					mods.push_back(std::string(path));
			}
			PHYSFS_freeList(list);
		}
		j["mounted_mods"] = std::move(mods);
	}

	/* -- Hi-res texture stats (count PNG/JPG replacements) ------------ */
	if (Current_level_num != 0) {
		int total = 0, replaced = 0;
		int max_w = 0, max_h = 0;
		for (int i = 0; i < Num_bitmap_files; i++) {
			ogl_texture *t = GameBitmaps[i].gltexture;
			if (!t || t->w == 0)
				continue;
			total++;
			if (t->is_png) {
				replaced++;
				if (t->w > max_w) max_w = t->w;
				if (t->h > max_h) max_h = t->h;
			}
		}
		json tex;
		tex["total_loaded"] = total;
		tex["hires_count"] = replaced;
		tex["hires_pct"] = (total > 0) ? (replaced * 100 / total) : 0;
		tex["hires_found"] = r_hires_found;
		tex["hires_uploaded"] = r_hires_loaded;
		tex["replacement_pct"] = (r_hires_found > 0) ? (r_hires_loaded * 100 / r_hires_found) : 0;
		tex["max_hires_w"] = max_w;
		tex["max_hires_h"] = max_h;
		j["hires_textures"] = tex;
	}

	/* -- Framebuffer pixel sample (android port: ETC2 debug) -------- */
	{
		extern volatile int g_fb_sample_r, g_fb_sample_g, g_fb_sample_b, g_fb_sample_a;
		if (g_fb_sample_r >= 0) {
			char buf[64];
			snprintf(buf, sizeof(buf), "(%d,%d,%d,%d)",
			         g_fb_sample_r, g_fb_sample_g, g_fb_sample_b, g_fb_sample_a);
			j["framebuffer_sample"] = std::string(buf);
		}
	}

	/* -- Recent console output (last 50 con_printf lines) ----------- */
	{
		char *console_json = console_ringbuf_get_json(0, 50);
		if (console_json) {
			try {
				j["console"] = json::parse(console_json);
			} catch (...) {
				j["console"] = nullptr;
			}
			free(console_json);
		}
	}

	/* -- Recent overlay messages (last 32 track/level popups) ------- */
	{
		char *overlay_json = overlay_ringbuf_get_json(0, 32);
		if (overlay_json) {
			try {
				j["overlays"] = json::parse(overlay_json);
			} catch (...) {
				j["overlays"] = nullptr;
			}
			free(overlay_json);
		}
	}

	/* Serialize to string and return as malloc'd C string */
	std::string result = j.dump();
	char *buf = (char *) malloc(result.size() + 1);
	if (buf) {
		memcpy(buf, result.c_str(), result.size() + 1);
	}
	return buf;
}

/* -- On-demand dump infrastructure ------------------------------------ */

static char introspect_path[512] = "";
static volatile int introspect_requested = 0;

extern "C" void game_introspect_set_path(const char *path)
{
	if (path) {
		strncpy(introspect_path, path, sizeof(introspect_path) - 1);
		introspect_path[sizeof(introspect_path) - 1] = '\0';
	}
}

extern "C" void game_introspect_request(void)
{
	introspect_requested = 1;
}

extern "C" void game_introspect_check_and_dump(void)
{
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
