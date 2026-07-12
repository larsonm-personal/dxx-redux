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
#include "game_automate.h"
#include "android_axis_mailbox.h"
#include "window.h"
#include "newmenu.h"
#include "object.h"
#include "player.h"
#include "game.h"
#include "gameseq.h"
#include "gameseg.h"
#include "inferno.h"
#include "screens.h"
#include "maths.h"
#include "vecmat.h"
#include "weapon.h"
#include "laser.h"
#include "cntrlcen.h"
#include "automap.h"
#include "segment.h"
#include "playsave.h"
#include "kconfig.h"
#include "joy.h"
#include "gr.h"
#include "multi.h"
#include "songs.h"
#include "songs_android_shared.h"
#include "ogl_init.h"
#include "piggy.h"
#include "textures.h"
#include "wall.h"
#include "secretarea.h"
#include "android_menu_scale.h"
#include "input_demo_recorder.h"
#ifdef DXX_BUILD_DESCENT_II
#include "ai.h"
#include "escort.h"
#include "multibot.h"
#include "d1_custom.h"
#include "d1_in_d2.h"
#include "gamepal.h"
#include "mission.h"
#endif

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

/* -- Android intro tracking globals (defined in android_input.c) -- */
extern "C" volatile int g_intro_active;
extern "C" volatile int g_intro_skip_applied;
extern "C" volatile int g_levelcomplete_active;
extern "C" volatile int g_cutscene_tap_suppress_arms;
extern "C" volatile int g_cutscene_tap_suppress_hits;
extern "C" int android_cutscene_tap_suppress_active(void);

/* -- Display surface dimensions (defined in android_surface.c) -- */
extern "C" int android_surface_get_display_width(void);
extern "C" int android_surface_get_display_height(void);

/* -- Redbook audio accessors (defined in rbaudio_bin.c / rbaudio.c) -- */
extern "C" {
int RBAEnabled(void);
int RBAGetNumberOfTracks(void);
int RBAGetNumAudioTracks(void);
int RBAGetTrackNum(void);
int RBAPeekPlayStatus(void);
const char *RBAGetInitStatus(void);
extern int g_startup_title_song_requested;
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
int androidaud_get_callback_last_us(void);
int androidaud_get_callback_max_us(void);
int androidaud_get_callback_overrun_count(void);
int androidaud_get_native_buffer_frames(void);
int androidaud_get_perf_mode_result(void);
int androidaud_get_sfx_last_delay_ms(void);
int androidaud_get_sfx_last_soundnum(void);
int androidaud_get_sfx_last_channel(void);
int androidaud_get_sfx_last_cb_delta(void);
int androidaud_get_sfx_last_queue_delay_ms(void);
int androidaud_get_sfx_last_estimated_output_ms(void);
int androidaud_get_sfx_probe_count(void);
int androidaud_get_initial_queued_buffers(void);
}

#include <SDL_mixer.h>

#include <physfs.h>

#include "console_ringbuf.h"
#include "android_log.h"
#include "android_texture_debug.h"
#include "overlay_ringbuf.h"
#include "debug_tex_overlay.h"
#include "merged_wall_debug.h"

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
		{ "android_readable_tiny", (bool) newmenu_get_android_wrapped_text(menu) },
		{ "android_wrapped_text", (bool) newmenu_get_android_wrapped_text(menu) },
		{ "android_original_num_items", newmenu_get_android_original_nitems(menu) },
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

#ifdef DXX_BUILD_DESCENT_II
static json serialize_fix_array(const fix values[NDL])
{
	json out = json::array();

	for (int i = 0; i < NDL; i++)
		out.push_back((int) values[i]);
	return out;
}

static json serialize_weapon_sample(int id)
{
	if (id < 0 || id >= MAX_WEAPON_TYPES)
		return {
			{ "id", id },
			{ "valid", false }
		};

	const weapon_info &wi = Weapon_info[id];
	return {
		{ "id", id },
		{ "valid", id < N_weapon_types },
		{ "render_type", (int) wi.render_type },
		{ "model_num", (int) wi.model_num },
		{ "model_num_inner", (int) wi.model_num_inner },
		{ "persistent", (int) wi.persistent },
		{ "flash_vclip", (int) wi.flash_vclip },
		{ "flash_sound", (int) wi.flash_sound },
		{ "robot_hit_vclip", (int) wi.robot_hit_vclip },
		{ "robot_hit_sound", (int) wi.robot_hit_sound },
		{ "wall_hit_vclip", (int) wi.wall_hit_vclip },
		{ "wall_hit_sound", (int) wi.wall_hit_sound },
		{ "fire_count", (int) wi.fire_count },
		{ "ammo_usage", (int) wi.ammo_usage },
		{ "weapon_vclip", (int) wi.weapon_vclip },
		{ "destroyable", (int) wi.destroyable },
		{ "matter", (int) wi.matter },
		{ "bounce", (int) wi.bounce },
		{ "homing_flag", (int) wi.homing_flag },
		{ "speedvar", (int) wi.speedvar },
		{ "flags", (int) wi.flags },
		{ "flash", (int) wi.flash },
		{ "afterburner_size", (int) wi.afterburner_size },
		{ "children", (int) wi.children },
		{ "energy_usage_raw", (int) wi.energy_usage },
		{ "fire_wait_raw", (int) wi.fire_wait },
		{ "multi_damage_scale_raw", (int) wi.multi_damage_scale },
		{ "bitmap_index", (int) wi.bitmap.index },
		{ "blob_size_raw", (int) wi.blob_size },
		{ "flash_size_raw", (int) wi.flash_size },
		{ "impact_size_raw", (int) wi.impact_size },
		{ "strength_raw", serialize_fix_array(wi.strength) },
		{ "speed_raw", serialize_fix_array(wi.speed) },
		{ "mass_raw", (int) wi.mass },
		{ "drag_raw", (int) wi.drag },
		{ "thrust_raw", (int) wi.thrust },
		{ "po_len_to_width_ratio_raw", (int) wi.po_len_to_width_ratio },
		{ "light_raw", (int) wi.light },
		{ "lifetime_raw", (int) wi.lifetime },
		{ "damage_radius_raw", (int) wi.damage_radius },
		{ "picture_index", (int) wi.picture.index },
		{ "hires_picture_index", (int) wi.hires_picture.index }
	};
}

static json serialize_weapon_samples()
{
	enum { D1_SPREADFIRE_PROJECTILE_ID = 20 };
	json samples;

	samples["laser_l1"] = serialize_weapon_sample(LASER_ID_L1);
	samples["laser_l4"] = serialize_weapon_sample(LASER_ID_L4);
	samples["vulcan"] = serialize_weapon_sample(VULCAN_ID);
	samples["primary_spreadfire"] = serialize_weapon_sample(Primary_weapon_to_weapon_info[SPREADFIRE_INDEX]);
	samples["d2_spreadfire_projectile"] = serialize_weapon_sample(SPREADFIRE_ID);
	samples["d1_spreadfire_projectile_id_20"] = serialize_weapon_sample(D1_SPREADFIRE_PROJECTILE_ID);
	samples["plasma"] = serialize_weapon_sample(PLASMA_ID);
	samples["fusion"] = serialize_weapon_sample(FUSION_ID);
	samples["concussion"] = serialize_weapon_sample(CONCUSSION_ID);
	samples["homing"] = serialize_weapon_sample(HOMING_ID);
	samples["smart"] = serialize_weapon_sample(SMART_ID);
	samples["mega"] = serialize_weapon_sample(MEGA_ID);
	samples["player_smart_homing"] = serialize_weapon_sample(PLAYER_SMART_HOMING_ID);
	samples["robot_smart_homing"] = serialize_weapon_sample(ROBOT_SMART_HOMING_ID);
	samples["control_center"] = serialize_weapon_sample(CONTROLCEN_WEAPON_NUM);
	return samples;
}
#endif

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
		{ "fvec_z", f2fl(ConsoleObject->orient.fvec.z) },
		/* Object state -- for diagnosing stuck controls after coop restore */
		{ "control_type", (int) ConsoleObject->control_type },
		{ "movement_type", (int) ConsoleObject->movement_type },
		{ "physics_flags", (unsigned) ConsoleObject->mtype.phys_info.flags }
	};
	return pos;
}

/* -- Serialize generated secret areas --------------------------------- */
static json serialize_secret_areas()
{
	const level_metadata_state *metadata = level_metadata_get_state();
	const secret_area_state *state = secret_area_get_state();
	json result;

	if (!state) {
		result["enabled"] = false;
		result["disabled_reason"] = "missing_state";
		return result;
	}
	result["enabled"] = (bool) state->enabled;
	result["disabled_reason"] = secret_area_disabled_reason_name(state->disabled_reason);
	result["raw_candidate_count"] = state->raw_candidate_count;
	result["final_candidate_count"] = state->final_candidate_count;
	result["energy_center_count"] = metadata ? metadata->energy_center_count : 0;
	result["energy_center_raw_count"] = metadata ? metadata->energy_center_raw_count : 0;
	result["energy_center_segment_count"] = metadata ? metadata->energy_center_segment_count : 0;
	result["energy_center_group_distance"] = metadata ? metadata->energy_center_group_distance : 0;
	result["energy_center_nearest_raw_distance"] = metadata ? metadata->energy_center_nearest_raw_distance : 0;
	result["matcen_count"] = metadata ? metadata->matcen_count : 0;
	result["matcen_raw_count"] = metadata ? metadata->matcen_raw_count : 0;
	result["matcen_segment_count"] = metadata ? metadata->matcen_segment_count : 0;
	result["found_count"] = secret_area_found_count(state);
	result["total"] = secret_area_total(state);
	result["reveal_unfound"] = (bool) secret_area_get_reveal_unfound();
	json secrets = json::array();
	if (state->enabled) {
		int drawable_label_count = 0;
		int drawable_segment_count = 0;
		for (int i = 0; i < state->final_candidate_count && i < SECRET_AREA_MAX_GENERATED; i++) {
			const secret_area_entry *entry = &state->secrets[i];
			bool drawable = state->found[i] != 0 || secret_area_get_reveal_unfound() != 0;
			json item;
			item["id"] = std::string("S") + std::to_string(entry->display_index);
			item["display_index"] = entry->display_index;
			item["found"] = state->found[i] != 0;
			item["drawable"] = drawable;
			item["entry_distance"] = entry->entry_distance;
			item["entry_seg"] = entry->entry_seg;
			item["entry_side"] = entry->entry_side;
			item["label_pos"] = { entry->label_pos[0], entry->label_pos[1], entry->label_pos[2] };
			item["robot_count"] = entry->robot_count;
			item["robotmaker_count"] = entry->robotmaker_count;
			item["item_count"] = entry->item_count;
			json secret_items = json::array();
			for (int p = 0; p < entry->item_count; p++) {
				const secret_area_item *secret_item = &entry->items[p];
				char fallback[32];
				snprintf(fallback, sizeof(fallback), "powerup %d", secret_item->id);
				secret_items.push_back({ { "id", secret_item->id },
				                         { "name", secret_item->name[0] ? secret_item->name : fallback },
				                         { "count", secret_item->count },
				                         { "direct_count", secret_item->direct_count },
				                         { "contained_count", secret_item->contained_count } });
			}
			item["items"] = std::move(secret_items);
			json segments = json::array();
			for (int s = 0; s < entry->segment_count; s++)
				segments.push_back(entry->segments[s]);
			item["segments"] = std::move(segments);
			json entrances = json::array();
			for (int e = 0; e < entry->entrance_count; e++) {
				const secret_area_entrance *entrance = &entry->entrances[e];
				entrances.push_back({ { "seg", entrance->seg },
				                      { "side", entrance->side },
				                      { "secret_seg", entrance->secret_seg },
				                      { "wall_num", entrance->wall_num } });
			}
			item["entrances"] = std::move(entrances);
			if (drawable) {
				drawable_label_count++;
				drawable_segment_count += entry->segment_count;
			}
			secrets.push_back(std::move(item));
		}
		result["drawable_label_count"] = drawable_label_count;
		result["drawable_segment_count"] = drawable_segment_count;
	} else {
		result["drawable_label_count"] = 0;
		result["drawable_segment_count"] = 0;
	}
	result["secrets"] = std::move(secrets);
	return result;
}

#ifdef DXX_BUILD_DESCENT_II
static const char *introspect_route_key_name(int key_index)
{
	switch (key_index) {
		case 0: return "blue";
		case 1: return "red";
		case 2: return "gold";
		default: return "";
	}
}

static std::string serialize_route_hash(unsigned long long hash)
{
	char text[17];
	std::snprintf(text, sizeof(text), "%016llx", hash);
	return text;
}

static json serialize_route_snapshot(const route_snapshot_summary &snapshot)
{
	return {
		{ "topology_hash", serialize_route_hash(snapshot.topology_hash) },
		{ "state_hash", serialize_route_hash(snapshot.state_hash) },
		{ "fingerprints",
		  {
		      { "start", serialize_route_hash(snapshot.start_hash) },
		      { "progression", serialize_route_hash(snapshot.progression_hash) },
		      { "navigation", serialize_route_hash(snapshot.navigation_hash) },
		      { "triggers", serialize_route_hash(snapshot.trigger_hash) },
		      { "objects", serialize_route_hash(snapshot.object_hash) },
		      { "automap", serialize_route_hash(snapshot.automap_hash) },
		  } },
		{ "generations",
		  {
		      { "topology", snapshot.topology_generation },
		      { "start", snapshot.start_generation },
		      { "progression", snapshot.progression_generation },
		      { "navigation", snapshot.navigation_generation },
		      { "triggers", snapshot.trigger_generation },
		      { "objects", snapshot.object_generation },
		      { "automap", snapshot.automap_generation },
		  } },
		{ "segment_count", snapshot.segment_count },
		{ "wall_count", snapshot.wall_count },
		{ "trigger_count", snapshot.trigger_count },
		{ "object_count", snapshot.object_count },
		{ "start_segment", snapshot.start_segment },
		{ "key_mask", snapshot.key_mask },
		{ "control_center_destroyed",
		  snapshot.control_center_destroyed != 0 },
	};
}

static json serialize_level_metadata_route()
{
	const level_metadata_state *metadata = level_metadata_get_state();
	json result;
	json steps = json::array();
	route_snapshot_summary snapshot = {};
	route_edge_shadow_summary edge_shadow = {};
	int count = 0;

	if (!metadata) {
		result["status"] = "failed";
		result["problem"] = "missing_state";
		result["steps"] = std::move(steps);
		return result;
	}
	result["status"] = level_metadata_route_status_name(metadata->route_status);
	result["problem"] = metadata->route_problem[0] ? metadata->route_problem : "";
	if (level_metadata_get_canonical_route_snapshot(&snapshot))
		result["canonical_snapshot"] = serialize_route_snapshot(snapshot);
	if (level_metadata_get_live_route_snapshot(&snapshot))
		result["live_snapshot"] = serialize_route_snapshot(snapshot);
	if (level_metadata_get_route_edge_shadow(&edge_shadow)) {
		result["edge_shadow"] = {
			{ "compared_edge_count", edge_shadow.compared_edge_count },
			{ "mismatch_count", edge_shadow.mismatch_count },
			{ "first_mismatch_segment", edge_shadow.first_mismatch_segment },
			{ "first_mismatch_side", edge_shadow.first_mismatch_side },
			{ "first_legacy_cost", edge_shadow.first_legacy_cost },
			{ "first_shared_cost", edge_shadow.first_shared_cost },
		};
	}
	count = metadata->route_step_count;
	if (count < 0)
		count = 0;
	if (count > LEVEL_METADATA_MAX_ROUTE_STEPS)
		count = LEVEL_METADATA_MAX_ROUTE_STEPS;
	result["step_count"] = count;
	for (int index = 0; index < count; index++) {
		const level_metadata_route_step *step = &metadata->route_steps[index];
		json item;
		item["index"] = index;
		item["kind"] = level_metadata_route_step_kind_name(step->kind);
		item["activation_kind"] = level_metadata_route_activation_kind_name(step->activation_kind);
		item["label"] = step->label;
		item["seg"] = step->seg;
		item["side"] = step->side;
		item["wall"] = step->wall_num;
		item["trigger"] = step->trigger_num;
		item["key"] = introspect_route_key_name(step->key_index);
		item["activation_pos"] = step->activation_pos_valid ? json::array({ step->activation_pos[0], step->activation_pos[1], step->activation_pos[2] }) : json(nullptr);
		item["aim_pos"] = step->aim_pos_valid ? json::array({ step->aim_pos[0], step->aim_pos[1], step->aim_pos[2] }) : json(nullptr);
		item["opened_link_count"] = step->opened_link_count;
		steps.push_back(std::move(item));
	}
	result["steps"] = std::move(steps);
	return result;
}

static json serialize_guidebot_route_analysis()
{
	const level_metadata_state *metadata = level_metadata_get_state();
	json result;
	json steps = json::array();
	int count = 0;
	int selected_index = -1;

	if (!metadata) {
		result["available"] = false;
		result["steps"] = std::move(steps);
		return result;
	}
	count = metadata->route_step_count;
	if (count < 0)
		count = 0;
	if (count > LEVEL_METADATA_MAX_ROUTE_STEPS)
		count = LEVEL_METADATA_MAX_ROUTE_STEPS;
	for (int index = 0; index < count; index++) {
		escort_route_step_analysis analysis;
		json item;
		json links = json::array();

		escort_route_step_analysis_clear(&analysis);
		if (!escort_route_analyze_step(index, &analysis))
			continue;
		if (analysis.selected_next)
			selected_index = index;
		item["index"] = index;
		item["kind"] = level_metadata_route_step_kind_name(analysis.kind);
		item["activation_kind"] = level_metadata_route_activation_kind_name(analysis.activation_kind);
		item["label"] = metadata->route_steps[index].label;
		item["activation_pos"] = metadata->route_steps[index].activation_pos_valid ? json::array({ metadata->route_steps[index].activation_pos[0], metadata->route_steps[index].activation_pos[1], metadata->route_steps[index].activation_pos[2] }) : json(nullptr);
		item["aim_pos"] = metadata->route_steps[index].aim_pos_valid ? json::array({ metadata->route_steps[index].aim_pos[0], metadata->route_steps[index].aim_pos[1], metadata->route_steps[index].aim_pos[2] }) : json(nullptr);
		item["satisfied"] = analysis.satisfied != 0;
		item["satisfied_reason"] = escort_route_step_satisfied_reason_name(analysis.satisfied_reason);
		item["selected_next"] = analysis.selected_next != 0;
		item["reachable"] = analysis.reachable;
		item["guidance_mode"] = analysis.guidance_mode;
		item["key_index"] = analysis.key_index;
		item["key_owned"] = analysis.key_owned;
		item["key_exists"] = analysis.key_exists;
		item["trigger"] = analysis.trigger_num;
		item["trigger_flags"] = analysis.trigger_flags;
		item["trigger_disabled"] = analysis.trigger_disabled;
		item["linked_wall_count"] = analysis.linked_wall_count;
		item["linked_walls_passable"] = analysis.linked_walls_passable;
		item["first_blocking_link"] = analysis.first_blocking_link;
		item["first_blocking_seg"] = analysis.first_blocking_seg;
		item["first_blocking_side"] = analysis.first_blocking_side;
		item["first_blocking_wall"] = analysis.first_blocking_wall;
		for (int link = 0; link < analysis.linked_wall_count && link < LEVEL_METADATA_MAX_ROUTE_LINKS; link++) {
			escort_route_link_analysis link_analysis;
			json link_item;

			escort_route_link_analysis_clear(&link_analysis);
			if (!escort_route_analyze_step_link(index, link, &link_analysis))
				continue;
			link_item["index"] = link;
			link_item["seg"] = link_analysis.seg;
			link_item["side"] = link_analysis.side;
			link_item["wall"] = link_analysis.wall;
			link_item["passable"] = link_analysis.passable != 0;
			links.push_back(std::move(link_item));
		}
		item["links"] = std::move(links);
		steps.push_back(std::move(item));
	}
	result["available"] = true;
	result["step_count"] = count;
	result["selected_index"] = selected_index;
	result["steps"] = std::move(steps);
	return result;
}

/* -- Serialize Guide-Bot state ---------------------------------------- */
static json serialize_guidebot()
{
	json result;

	result["buddy_objnum"] = Buddy_objnum;
	result["released"] = (bool) Buddy_allowed_to_talk;
	result["goal_object"] = Escort_goal_object;
	result["special_goal"] = Escort_special_goal;
	result["goal_index"] = Escort_goal_index;
#ifdef NETWORK
	result["owner_player"] = Escort_owner_player;
	result["owner_is_local"] = Escort_owner_player == Player_num;
	result["owner_generation"] = escort_get_owner_generation();
#endif
	result["secret_goal_display_index"] = escort_get_secret_goal_display_index();
	result["secret_goal_seg"] = escort_get_secret_goal_seg();
	result["secret_goal_side"] = escort_get_secret_goal_side();
#ifdef __ANDROID__
	const level_metadata_state *route_metadata = level_metadata_get_state();

	result["route_goal_active"] = (bool) escort_get_route_goal_active();
	result["route_goal_label"] = escort_get_route_goal_label();
	result["route_goal_seg"] = escort_get_route_goal_seg();
	result["route_goal_side"] = escort_get_route_goal_side();
	result["route_goal_wall"] = escort_get_route_goal_wall();
	result["route_goal_trigger"] = escort_get_route_goal_trigger();
	result["route_goal_objective_kind"] = escort_get_route_goal_objective_kind();
	result["route_goal_activation_kind"] = escort_get_route_goal_activation_kind();
	result["route_goal_activation_kind_name"] =
	    level_metadata_route_activation_kind_name(escort_get_route_goal_activation_kind());
	result["route_goal_objective_seg"] = escort_get_route_goal_objective_seg();
	result["route_goal_objective_side"] = escort_get_route_goal_objective_side();
	result["route_goal_objective_wall"] = escort_get_route_goal_objective_wall();
	result["route_goal_objective_trigger"] = escort_get_route_goal_objective_trigger();
	result["route_goal_guidance_mode"] = escort_get_route_goal_guidance_mode();
	result["route_goal_guidance_mode_name"] = escort_get_route_goal_guidance_mode_name();
	result["route_goal_guidance_seg"] = escort_get_route_goal_guidance_seg();
	result["route_goal_guidance_side"] = escort_get_route_goal_guidance_side();
	result["route_goal_path_endpoint_seg"] = escort_get_route_goal_path_endpoint_seg();
	result["route_goal_path_pending"] = (bool) escort_get_route_goal_path_pending();
	result["route_target_mode"] = escort_get_route_target_mode();
	result["route_target_mode_name"] = escort_get_route_target_mode_name();
	result["route_last_replan_reason"] = escort_get_route_last_replan_reason();
	result["route_metadata_rescan_count"] = escort_get_route_metadata_rescan_count();
	result["route_guidance_full_search_count"] = escort_get_route_guidance_full_search_count();
	result["unexplored_component_size"] = escort_get_unexplored_component_size();
	result["unexplored_target_seg"] = escort_get_unexplored_target_seg();
	result["unexplored_waypoint_seg"] = escort_get_unexplored_waypoint_seg();
	result["unexplored_direct_reachable"] = (bool) escort_get_unexplored_direct_reachable();
	result["route_status"] = route_metadata ? level_metadata_route_status_name(route_metadata->route_status) : "unavailable";
	result["route_problem"] = route_metadata && route_metadata->route_problem[0] ? route_metadata->route_problem : "";
	result["route_start_seg"] = route_metadata && route_metadata->route_step_count > 0 ? route_metadata->route_steps[0].seg : -1;
	result["route_start_matches_buddy"] =
	    route_metadata && route_metadata->route_step_count > 0 &&
	    Buddy_objnum >= 0 && Buddy_objnum <= Highest_object_index &&
	    level_metadata_get_route_start_objnum() == Buddy_objnum &&
	    level_metadata_get_route_start_seg() == route_metadata->route_steps[0].seg;
	result["route_analysis"] = serialize_guidebot_route_analysis();
#endif
	if (Buddy_objnum >= 0 && Buddy_objnum <= Highest_object_index) {
		result["segment"] = (int) Objects[Buddy_objnum].segnum;
		result["object_type"] = (int) Objects[Buddy_objnum].type;
		result["ai_mode"] = Ai_local_info[Buddy_objnum].mode;
		result["path_index"] = Objects[Buddy_objnum].ctype.ai_info.cur_path_index;
		result["path_length"] = Objects[Buddy_objnum].ctype.ai_info.path_length;
		result["path_direction"] = Objects[Buddy_objnum].ctype.ai_info.PATH_DIR;
#ifdef NETWORK
		int remote_slot = Objects[Buddy_objnum].ctype.ai_info.REMOTE_SLOT_NUM;
		result["remote_owner"] = (int) Objects[Buddy_objnum].ctype.ai_info.REMOTE_OWNER;
		result["remote_slot"] = remote_slot;
		result["local_control_slot_matches"] =
		    remote_slot >= 0 && remote_slot < MAX_ROBOTS_CONTROLLED &&
		    robot_controlled[remote_slot] == Buddy_objnum;
#endif
	} else {
		result["segment"] = nullptr;
		result["object_type"] = nullptr;
	}
	return result;
}
#endif

static const char *merged_wall_snapshot_cover_kind_name(int kind)
{
	switch (kind) {
		case 1: return "exact";
		case 2: return "bbox";
		default: return "unknown";
	}
}

static std::string hex32_string(unsigned int value)
{
	char buf[9];

	snprintf(buf, sizeof(buf), "%08x", value);
	return std::string(buf);
}

static json serialize_merged_wall_snapshot()
{
	const merged_wall_snapshot_result &snap = g_merged_wall_snapshot_result;
	json faces = json::array();
	json covers = json::array();
	json target_cover_gpu = nullptr;

	if (!snap.valid)
		return nullptr;

	for (int i = 0; i < MERGED_WALL_SNAPSHOT_FACE_MAX; ++i) {
		const merged_wall_snapshot_face &face = snap.faces[i];

		if (!face.valid)
			continue;
		faces.push_back({ { "rank", face.rank },
		                  { "center_hit", (bool) face.center_hit },
		                  { "dist2", face.dist2 },
		                  { "render_pass", face.render_pass },
		                  { "draw_seq", face.draw_seq },
		                  { "draw_order", face.draw_order },
		                  { "seg", face.seg },
		                  { "side", face.side },
		                  { "face", face.face },
		                  { "child", face.child },
		                  { "side_type", face.side_type },
		                  { "wid_flags", face.wid_flags },
		                  { "tmap1", face.tmap1 },
		                  { "tmap2", face.tmap2 },
		                  { "min_sx", face.min_sx },
		                  { "max_sx", face.max_sx },
		                  { "min_sy", face.min_sy },
		                  { "max_sy", face.max_sy },
		                  { "bbox_area", face.bbox_area },
		                  { "fan_area_012", face.fan_area_012 },
		                  { "fan_area_023", face.fan_area_023 },
		                  { "alt_area_013", face.alt_area_013 },
		                  { "alt_area_123", face.alt_area_123 },
		                  { "fan_flip", (bool) face.fan_flip },
		                  { "alt_flip", (bool) face.alt_flip },
		                  { "fan_flat", (bool) face.fan_flat },
		                  { "alt_flat", (bool) face.alt_flat },
		                  { "cull_sensitive", (bool) face.cull_sensitive },
		                  { "submit_nv", face.submit_nv },
		                  { "preferred_split", std::string(face.preferred_split) },
		                  { "route", std::string(face.route) },
		                  { "merge_impl", std::string(face.merge_impl) },
		                  { "decision_reason", std::string(face.decision_reason) } });
	}

	for (int i = 0; i < MERGED_WALL_SNAPSHOT_COVER_MAX; ++i) {
		const merged_wall_snapshot_cover &cover = snap.covers[i];

		if (!cover.valid)
			continue;
		covers.push_back({ { "kind", cover.kind },
		                   { "kind_name", merged_wall_snapshot_cover_kind_name(cover.kind) },
		                   { "rank", cover.rank },
		                   { "center_face", (bool) cover.center_face },
		                   { "center_cover", (bool) cover.center_cover },
		                   { "center_overlap", (bool) cover.center_overlap },
		                   { "overlap_area", cover.overlap_area },
		                   { "ordered", (bool) cover.ordered },
		                   { "render_pass", cover.render_pass },
		                   { "draw_seq", cover.draw_seq },
		                   { "draw_order", cover.draw_order },
		                   { "cover_order", cover.cover_order },
		                   { "seg", cover.seg },
		                   { "side", cover.side },
		                   { "face", cover.face },
		                   { "child", cover.child },
		                   { "wid_flags", cover.wid_flags },
		                   { "cover_seg", cover.cover_seg },
		                   { "cover_side", cover.cover_side },
		                   { "cover_face", cover.cover_face },
		                   { "cover_child", cover.cover_child },
		                   { "cover_wid_flags", cover.cover_wid_flags },
		                   { "cover_shader", std::string(cover.cover_shader) },
		                   { "cover_bot", std::string(cover.cover_bot) },
		                   { "cover_ovl", std::string(cover.cover_ovl) } });
	}

	if (snap.target_cover_gpu.valid) {
		const merged_wall_snapshot_target_cover &target = snap.target_cover_gpu;

		target_cover_gpu = {
			{ "valid", true },
			{ "ordered", (bool) target.ordered },
			{ "render_pass", target.render_pass },
			{ "draw_seq", target.draw_seq },
			{ "draw_order", target.draw_order },
			{ "cover_order", target.cover_order },
			{ "seg", target.seg },
			{ "side", target.side },
			{ "face", target.face },
			{ "child", target.child },
			{ "wid_flags", target.wid_flags },
			{ "cover_seg", target.cover_seg },
			{ "cover_side", target.cover_side },
			{ "cover_face", target.cover_face },
			{ "cover_child", target.cover_child },
			{ "cover_wid_flags", target.cover_wid_flags },
			{ "tmap1", target.tmap1 },
			{ "tex_w", target.tex_w },
			{ "tex_h", target.tex_h },
			{ "src_hash_hex", hex32_string(target.src_hash) },
			{ "gpu_hash_hex", hex32_string(target.gpu_hash) },
			{ "src_idx254", target.src_idx254 },
			{ "src_idx255", target.src_idx255 },
			{ "gpu_avg_r", target.gpu_avg_r },
			{ "gpu_avg_g", target.gpu_avg_g },
			{ "gpu_avg_b", target.gpu_avg_b },
			{ "gpu_avg_a", target.gpu_avg_a },
			{ "gpu_black", target.gpu_black },
			{ "p0_r", target.p0_r },
			{ "p0_g", target.p0_g },
			{ "p0_b", target.p0_b },
			{ "p0_a", target.p0_a },
			{ "center_r", target.center_r },
			{ "center_g", target.center_g },
			{ "center_b", target.center_b },
			{ "center_a", target.center_a },
			{ "overlap_area", target.overlap_area },
			{ "kind_name", std::string(target.kind_name) },
			{ "face_box", std::string(target.face_box) },
			{ "cover_shader", std::string(target.cover_shader) },
			{ "cover_bot", std::string(target.cover_bot) },
			{ "cover_ovl", std::string(target.cover_ovl) }
		};
	}

	return {
		{ "status", std::string(snap.status) },
		{ "frame_id", snap.frame_id },
		{ "request_frame", snap.request_frame },
		{ "screen_w", snap.screen_w },
		{ "screen_h", snap.screen_h },
		{ "center_x", snap.center_x },
		{ "center_y", snap.center_y },
		{ "sample_rgba", { snap.sample_r, snap.sample_g, snap.sample_b, snap.sample_a } },
		{ "avg_rgba", { snap.avg_r, snap.avg_g, snap.avg_b, snap.avg_a } },
		{ "tracked_count", snap.tracked_count },
		{ "center_hit_count", snap.center_hit_count },
		{ "cover_event_count", snap.cover_event_count },
		{ "selected_count", snap.selected_count },
		{ "relevant_cover_count", snap.relevant_cover_count },
		{ "omitted_cover_count", snap.omitted_cover_count },
		{ "target_cover_gpu", std::move(target_cover_gpu) },
		{ "faces", std::move(faces) },
		{ "covers", std::move(covers) }
	};
}

static json serialize_merged_wall_last_draw_state()
{
	const merged_wall_last_draw_state &state = g_merged_wall_last_draw_state;

	if (!state.valid)
		return nullptr;

	return {
		{ "frame_id", state.frame_id },
		{ "render_pass", state.render_pass },
		{ "draw_seq", state.draw_seq },
		{ "seg", state.seg },
		{ "side", state.side },
		{ "face", state.face },
		{ "child", state.child },
		{ "wid_flags", state.wid_flags },
		{ "tmap1", state.tmap1 },
		{ "tmap2", state.tmap2 },
		{ "depth_enabled", (bool) state.depth_enabled },
		{ "blend_enabled", (bool) state.blend_enabled },
		{ "cull_enabled", (bool) state.cull_enabled },
		{ "polygon_offset_enabled", (bool) state.polygon_offset_enabled },
		{ "polygon_offset_factor", state.polygon_offset_factor },
		{ "polygon_offset_units", state.polygon_offset_units },
		{ "depth_writemask", (bool) state.depth_writemask },
		{ "depth_func", state.depth_func },
		{ "front_face", state.front_face },
		{ "cull_mode", state.cull_mode },
		{ "draw_fbo", state.draw_fbo },
		{ "screen_area", state.screen_area },
		{ "route", std::string(state.route) },
		{ "merge_impl", std::string(state.merge_impl) }
	};
}

/* -- Main entry point ------------------------------------------------- */

extern "C" char *game_introspect_get_state(void)
{
	json j;

	/* -- General state -------------------------------------------- */
	j["screen_mode"] = screen_mode_name(Screen_mode);
	j["game_mode"] = Game_mode;
	j["quitting"] = (bool) Quitting;
	j["intro_active"] = (bool) g_intro_active;
	j["intro_skip_applied"] = (bool) g_intro_skip_applied;
	j["levelcomplete_active"] = (bool) g_levelcomplete_active;
	j["cutscene_tap_suppress_active"] = (bool) android_cutscene_tap_suppress_active();
	j["cutscene_tap_suppress_arms"] = (int) g_cutscene_tap_suppress_arms;
	j["cutscene_tap_suppress_hits"] = (int) g_cutscene_tap_suppress_hits;
	j["difficulty"] = Difficulty_level;
	j["difficulty_changed"] = Difficulty_level_changed != 0;
	j["difficulty_min"] = Difficulty_level_min_seen;
	j["difficulty_max"] = Difficulty_level_max_seen;
	j["current_level_num"] = Current_level_num;
	j["current_level_name"] = Current_level_name;
#ifdef DXX_BUILD_DESCENT_II
	j["level_metadata_route"] = serialize_level_metadata_route();
#endif

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
			mp["host_is_observer"] = Netgame.host_is_obs != 0;

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
		dbg["texture_target"] = android_texture_debug_get_target_display();
		dbg["texture_log"] = (bool) debug_log_enabled[DLOG_TEXTURE];
		dbg["merged_wall_mode"] = (int) g_merged_wall_debug_mode;
		dbg["merged_wall_force_two_pass"] = (int) g_merged_wall_force_two_pass;
		j["debug_flags"] = std::move(dbg);
	}
	j["merged_wall_last_draw_state"] = serialize_merged_wall_last_draw_state();

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
				{ "tangles_b", avi.tangles.b },
				{ "secret_reveal_unfound", (bool) avi.secret_reveal_unfound },
				{ "secret_edge_count", avi.secret_edge_count },
				{ "secret_visible_edge_count", avi.secret_visible_edge_count },
				{ "secret_too_far_edge_count", avi.secret_too_far_edge_count },
				{ "secret_edges_drawn_last_frame", avi.secret_edges_drawn_last_frame },
				{ "secret_edges_culled_far_dist_last_frame", avi.secret_edges_culled_far_dist_last_frame },
				{ "secret_label_candidate_count", avi.secret_label_candidate_count },
				{ "secret_label_projected_count", avi.secret_label_projected_count }
			};
		} else {
			j["automap"] = nullptr;
		}
	}

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
			{ "osl_buf_frames", androidaud_get_audio_buf_frames() },
			{ "osl_cb_last_us", androidaud_get_callback_last_us() },
			{ "osl_cb_max_us", androidaud_get_callback_max_us() },
			{ "osl_cb_overruns", androidaud_get_callback_overrun_count() },
			{ "osl_native_buf_frames", androidaud_get_native_buffer_frames() },
			{ "osl_perf_mode_result", androidaud_get_perf_mode_result() },
			{ "sfx_last_delay_ms", androidaud_get_sfx_last_delay_ms() },
			{ "sfx_last_soundnum", androidaud_get_sfx_last_soundnum() },
			{ "sfx_last_channel", androidaud_get_sfx_last_channel() },
			{ "sfx_last_cb_delta", androidaud_get_sfx_last_cb_delta() },
			{ "sfx_last_queue_ms", androidaud_get_sfx_last_queue_delay_ms() },
			{ "sfx_last_est_app_ms", androidaud_get_sfx_last_estimated_output_ms() },
			{ "sfx_probe_count", androidaud_get_sfx_probe_count() },
			{ "osl_initial_queued_buffers", androidaud_get_initial_queued_buffers() }
		};
		j["audio"] = std::move(audio);
	}

	/* -- Current music track --------------------------------------- */
	{
		const int song_playing = songs_is_playing();
		int type = 0;
		int track = -1;
		int total = 0;
		char name[PATH_MAX] = "";
		const int have_track = songs_get_track_info(&type, &track, &total, name, sizeof(name));
		json music = {
			{ "active", song_playing >= 0 },
			{ "song_playing", song_playing },
			{ "startup_title_requested", g_startup_title_song_requested != 0 },
			{ "type", type },
			{ "track", track },
			{ "total", total }
		};
		if (have_track == 0 && name[0])
			music["name"] = std::string(name);
		char *track_list = (char *) malloc(32768);
		if (track_list) {
			songs_get_track_list(track_list, 32768);
			json tracks = json::parse(track_list, nullptr, false);
			if (tracks.is_array())
				music["tracks"] = std::move(tracks);
			free(track_list);
		}
		j["music"] = std::move(music);
	}

	/* -- Redbook audio ---------------------------------------------- */
	{
		json rb = {
			{ "enabled", false },
			{ "init_status", std::string(RBAGetInitStatus() ? RBAGetInitStatus() : "") }
		};
		int enabled = RBAEnabled();
		if (enabled) {
			rb["enabled"] = true;
			int status = RBAPeekPlayStatus();
			rb["num_tracks"] = RBAGetNumberOfTracks();
			rb["num_audio_tracks"] = RBAGetNumAudioTracks();
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

	/* -- Asset source trace for D1-in-D2 overlay compatibility -------- */
#ifdef DXX_BUILD_DESCENT_II
	{
		d1_bitmap_replacement_stats d1_walls;
		d1_in_d2_asset_stats d1_compat;
		d1_custom_texture_stats d1_custom;
		const bool emulating_d1 = Current_mission && EMULATING_D1;

		d1_bitmap_replacement_get_stats(&d1_walls);
		d1_in_d2_get_stats(&d1_compat);
		d1_custom_get_stats(&d1_custom);

		json trace;
		trace["runtime_engine"] = "d2";
		trace["selected_base_game"] = emulating_d1 ? "d1" : "d2";
		trace["mode"] = emulating_d1 ? "d1-in-d2" : "d2";
		trace["current_level_palette"] = std::string(Current_level_palette);
		trace["last_palette_loaded"] = std::string(last_palette_loaded);
		trace["last_palette_loaded_pig"] = std::string(last_palette_loaded_pig);
		trace["current_pigfile"] = std::string(piggy_current_pigfile() ? piggy_current_pigfile() : "");

		trace["d1_base_walls"] = {
			{ "pig_present", (bool) d1_walls.pig_present },
			{ "pig_size", d1_walls.pig_size },
			{ "wall_entries", d1_walls.wall_entries },
			{ "wall_applied", d1_walls.wall_applied },
			{ "animated_clones", d1_walls.animated_clones },
			{ "colormap_failed", (bool) d1_walls.colormap_failed }
		};
		trace["d1_compat"] = {
			{ "effects_active", (bool) d1_compat.effects_active },
			{ "effects_loaded", (bool) d1_compat.effects_loaded },
			{ "num_effects", d1_compat.num_effects },
			{ "effect_frames_applied", d1_compat.effect_frames_applied },
			{ "effect_frames_skipped", d1_compat.effect_frames_skipped },
			{ "powerup_vclips_active", (bool) d1_compat.powerup_vclips_active },
			{ "powerup_vclips_loaded", (bool) d1_compat.powerup_vclips_loaded },
			{ "num_vclips", d1_compat.num_vclips },
			{ "powerup_frames_applied", d1_compat.powerup_frames_applied },
			{ "powerup_frames_skipped", d1_compat.powerup_frames_skipped },
			{ "sounds_active", (bool) d1_compat.sounds_active },
			{ "sound_pig_present", (bool) d1_compat.sound_pig_present },
			{ "sound_pig_size", d1_compat.sound_pig_size },
			{ "sound_map_entries", d1_compat.sound_map_entries },
			{ "sound_files", d1_compat.sound_files },
			{ "sound_bytes", d1_compat.sound_bytes },
			{ "cockpit_active", (bool) d1_compat.cockpit_active },
			{ "cockpit_frames_applied", d1_compat.cockpit_frames_applied },
			{ "cockpit_frames_skipped", d1_compat.cockpit_frames_skipped },
			{ "robot_assets_active", (bool) d1_compat.robot_assets_active },
			{ "robot_pig_present", (bool) d1_compat.robot_pig_present },
			{ "robot_pig_size", d1_compat.robot_pig_size },
			{ "robot_types", d1_compat.robot_types },
			{ "robot_joints", d1_compat.robot_joints },
			{ "robot_models", d1_compat.robot_models },
			{ "weapon_records_active", (bool) d1_compat.weapon_records_active },
			{ "weapon_types", d1_compat.weapon_types },
			{ "robot_obj_bitmaps", d1_compat.robot_obj_bitmaps },
			{ "robot_obj_bitmaps_applied", d1_compat.robot_obj_bitmaps_applied },
			{ "robot_obj_bitmaps_skipped", d1_compat.robot_obj_bitmaps_skipped },
			{ "robot_objects_updated", d1_compat.robot_objects_updated }
		};
		trace["d1_custom"] = {
			{ "files_found", d1_custom.files_found },
			{ "bitmap_entries", d1_custom.bitmap_entries },
			{ "bitmap_applied", d1_custom.bitmap_applied },
			{ "bitmap_unresolved", d1_custom.bitmap_unresolved },
			{ "sound_entries", d1_custom.sound_entries },
			{ "sound_applied", d1_custom.sound_applied },
			{ "sound_unresolved", d1_custom.sound_unresolved },
			{ "base_sound_entries", d1_custom.base_sound_entries },
			{ "base_sound_applied", d1_custom.base_sound_applied },
			{ "base_sound_unresolved", d1_custom.base_sound_unresolved },
			{ "base_sound_skipped", d1_custom.base_sound_skipped }
		};
		j["asset_trace"] = std::move(trace);
	}
#endif

	/* -- Gameplay table trace --------------------------------------- */
#ifdef DXX_BUILD_DESCENT_II
	{
		const bool emulating_d1 = Current_mission && EMULATING_D1;
		json gameplay;
		gameplay["runtime_engine"] = "d2";
		gameplay["selected_base_game"] = emulating_d1 ? "d1" : "d2";
		gameplay["mode"] = emulating_d1 ? "d1-in-d2" : "d2";
		gameplay["weapon_table_count"] = N_weapon_types;
		gameplay["weapon_samples"] = serialize_weapon_samples();
		j["gameplay_trace"] = std::move(gameplay);
	}
#endif

	/* -- D1 custom texture replacement stats (D2 D1-emulation path) -- */
#ifdef DXX_BUILD_DESCENT_II
	{
		d1_custom_texture_stats stats;
		d1_custom_get_stats(&stats);
		j["d1_custom_textures"] = {
			{ "files_found", stats.files_found },
			{ "bitmap_entries", stats.bitmap_entries },
			{ "applied", stats.bitmap_applied },
			{ "unresolved", stats.bitmap_unresolved },
			{ "sound_entries", stats.sound_entries },
			{ "sound_applied", stats.sound_applied },
			{ "sound_unresolved", stats.sound_unresolved },
			{ "base_sound_entries", stats.base_sound_entries },
			{ "base_sound_applied", stats.base_sound_applied },
			{ "base_sound_unresolved", stats.base_sound_unresolved },
			{ "base_sound_skipped", stats.base_sound_skipped }
		};
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
#if defined(ANDROID)
		json axis_button_down_edges = json::array();
		json axis_button_up_edges = json::array();
		for (int button = 0; button < 26; ++button) {
			axis_button_down_edges.push_back(joy_axisbutton_get_down_edges(button));
			axis_button_up_edges.push_back(joy_axisbutton_get_up_edges(button));
		}
		j["axis_button_down_edges"] = std::move(axis_button_down_edges);
		j["axis_button_up_edges"] = std::move(axis_button_up_edges);
#endif

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

		/* Stable sample captured when kconfig consumes the automation generation. */
		game_automate_axis_probe axis_probe = {};
		game_automate_get_axis_probe(&axis_probe);
		j["axis_probe"] = {
			{ "valid", axis_probe.valid != 0 },
			{ "first_generation", axis_probe.first_generation },
			{ "generation", axis_probe.generation },
			{ "axis", axis_probe.axis },
			{ "raw_value", axis_probe.raw_value },
			{ "touch_source", axis_probe.touch_source != 0 },
			{ "processed", axis_probe.processed != 0 },
			{ "queue_fill_count", axis_probe.queue_fill_count },
			{ "queue_saturated", axis_probe.queue_saturated != 0 },
			{ "sample_count", axis_probe.sample_count },
			{ "axis_button_down_edges", axis_probe.axis_button_down_edges },
			{ "axis_button_up_edges", axis_probe.axis_button_up_edges },
			{ "pitch_time", axis_probe.pitch_time },
			{ "heading_time", axis_probe.heading_time },
			{ "slide_lr_time", axis_probe.slide_lr_time },
			{ "slide_ud_time", axis_probe.slide_ud_time },
			{ "bank_time", axis_probe.bank_time },
			{ "throttle_time", axis_probe.throttle_time }
		};
		android_axis_mailbox_diagnostics axis_mailbox = {};
		android_axis_mailbox_get_diagnostics(&axis_mailbox);
		j["axis_mailbox"] = {
			{ "last_published_generation", axis_mailbox.last_published_generation },
			{ "last_applied_generation", axis_mailbox.last_applied_generation },
			{ "publish_count", axis_mailbox.publish_count },
			{ "coalesced_count", axis_mailbox.coalesced_count },
			{ "drain_count", axis_mailbox.drain_count },
			{ "dispatch_count", axis_mailbox.dispatch_count },
			{ "automation_active", axis_mailbox.automation_active != 0 },
			{ "pending_axis_count", axis_mailbox.pending_axis_count }
		};

		/* Diagnostic: raw axis state and modifier flags for axis test debugging */
		j["slide_on_state"] = (int) Controls.slide_on_state;
		j["bank_on_state"] = (int) Controls.bank_on_state;
		j["fire_primary_state"] = (int) Controls.fire_primary_state;
		j["fire_primary_count"] = (int) Controls.fire_primary_count;
		j["global_laser_firing_count"] = Global_laser_firing_count;
		j["next_laser_fire_delta"] = (long long) (Next_laser_fire_time - GameTime64);
		j["last_laser_fired_delta"] = (long long) (Last_laser_fired_time - GameTime64);
		j["control_type"] = (int) PlayerCfg.ControlType;
		j["cockpit_mode"] = (int) PlayerCfg.PreferredCockpitMode;
		j["current_cockpit_mode"] = (int) PlayerCfg.CurrentCockpitMode;
		j["auto_leveling"] = (bool) PlayerCfg.AutoLeveling;
#ifdef DXX_BUILD_DESCENT_II
		j["headlight_active_default"] = (bool) PlayerCfg.HeadlightActiveDefault;
#else
		j["headlight_active_default"] = false;
#endif
		j["msaa_samples"] = ogl_msaa_samples;
		j["msaa_max_samples"] = ogl_msaa_max_samples;
		j["msaa_fbo_bound"] = (bool) g_msaa_fbo_bound;
		{
			auto &rja = j["raw_joy_axis"];
			for (int a = 0; a < 8; a++)
				rja[a] = (int) Controls.raw_joy_axis[a];
		}
		{
			json buttons = json::array();
			int button_count = MIXER_BTN_BASE + kconfig_get_joystick_count();
			if (button_count < 26)
				button_count = 26;
			if (button_count > JOY_MAX_BUTTONS)
				button_count = JOY_MAX_BUTTONS;
			for (int b = 0; b < button_count; b++)
				buttons.push_back(joy_get_button_state(b));
			j["joy_buttons"] = std::move(buttons);
		}
	}

	/* -- Player & position (only meaningful when a level is loaded) -- */
	if (Current_level_num != 0) {
		j["player"] = serialize_player();
		j["position"] = serialize_position();
		j["secret_areas"] = serialize_secret_areas();
#ifdef DXX_BUILD_DESCENT_II
		j["guidebot"] = serialize_guidebot();
#endif
		j["merged_wall_snapshot"] = serialize_merged_wall_snapshot();

		/* Flat weapon keys for easy assertion */
		player *p = &Players[Player_num];
		j["primary_weapon"] = (int) p->primary_weapon;
		j["secondary_weapon"] = (int) p->secondary_weapon;
	} else {
		j["player"] = nullptr;
		j["position"] = nullptr;
		j["secret_areas"] = nullptr;
#ifdef DXX_BUILD_DESCENT_II
		j["guidebot"] = nullptr;
#endif
		j["merged_wall_snapshot"] = nullptr;
	}

	/* -- Keyboard viewport offset state -------------------------- */
	{
		extern volatile int g_blit_y_offset;
		/* These are in android_input.c -- declared static, so we
		 * expose them via a helper instead of extern */
		extern int android_get_keyboard_state(int *kb_native, int *scr_native, int *field_y);
		android_menu_scale_result menu_scale = {};
		int kb_native = 0, scr_native = 0, field_y_val = 0;
		int have_menu_scale = android_menu_scale_get_state(&menu_scale);
		android_get_keyboard_state(&kb_native, &scr_native, &field_y_val);
		json kb;
		kb["keyboard_height_native"] = kb_native;
		kb["screen_height_native"] = scr_native;
		kb["active_input_field_y"] = field_y_val;
		kb["blit_y_offset"] = (int) g_blit_y_offset;
		kb["scale_blit_active"] = have_menu_scale && menu_scale.active;
		j["keyboard_viewport"] = kb;

		json scale;
		scale["active"] = have_menu_scale && menu_scale.active;
		scale["direct_render"] = have_menu_scale && menu_scale.direct_render;
		scale["target_fill"] = android_menu_scale_get_target_fill();
		scale["scale"] = menu_scale.scale;
		scale["render_scale"] = menu_scale.render_scale;
		scale["render_w"] = menu_scale.render_w;
		scale["render_h"] = menu_scale.render_h;
		scale["crop_left"] = menu_scale.crop_left;
		scale["crop_top"] = menu_scale.crop_top;
		scale["box"] = { { "x", menu_scale.box.x }, { "y", menu_scale.box.y }, { "w", menu_scale.box.w }, { "h", menu_scale.box.h } };
		scale["src"] = { { "x", menu_scale.src.x }, { "y", menu_scale.src.y }, { "w", menu_scale.src.w }, { "h", menu_scale.src.h } };
		scale["dst"] = { { "x", menu_scale.dst.x }, { "y", menu_scale.dst.y }, { "w", menu_scale.dst.w }, { "h", menu_scale.dst.h } };
		j["menu_scale"] = scale;
	}

	/* -- PhysFS paths and mounted mods -------------------------------- */
	{
		json physfs;
		json search_path = json::array();
		json mods = json::array();
		const char *base_dir = PHYSFS_getBaseDir();
		const char *write_dir = PHYSFS_getWriteDir();
		char **list = PHYSFS_getSearchPath();
		if (list) {
			for (char **i = list; *i != NULL; i++) {
				const char *path = *i;
				size_t len = strlen(path);
				search_path.push_back(std::string(path));
				if (len > 4 && strcmp(path + len - 4, ".dxa") == 0)
					mods.push_back(std::string(path));
			}
			PHYSFS_freeList(list);
		}
		physfs["base_dir"] = base_dir ? base_dir : "";
		physfs["write_dir"] = write_dir ? write_dir : "";
		physfs["search_path"] = std::move(search_path);
		j["physfs"] = std::move(physfs);
		j["mounted_mods"] = std::move(mods);
	}

	/* -- Input demo recorder state ------------------------------------ */
	{
		json input_demo;
		input_demo["recording"] = input_demo_recorder_is_active() != 0;
		input_demo["frame_count"] = input_demo_recorder_is_active() ? input_demo_recorder_frame_count() : 0;
		j["input_demo"] = input_demo;
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

	/* -- Framebuffer pixel sample (center + 4x4 grid average) -------- */
	{
		extern volatile int g_fb_sample_r, g_fb_sample_g, g_fb_sample_b, g_fb_sample_a;
		extern volatile int g_fb_avg_r, g_fb_avg_g, g_fb_avg_b, g_fb_avg_a;
		if (g_fb_sample_r >= 0) {
			char buf[64];
			snprintf(buf, sizeof(buf), "(%d,%d,%d,%d)",
			         g_fb_sample_r, g_fb_sample_g, g_fb_sample_b, g_fb_sample_a);
			j["framebuffer_sample"] = std::string(buf);
		}
		if (g_fb_avg_r >= 0) {
			char buf[64];
			snprintf(buf, sizeof(buf), "(%d,%d,%d,%d)",
			         g_fb_avg_r, g_fb_avg_g, g_fb_avg_b, g_fb_avg_a);
			j["framebuffer_avg"] = std::string(buf);
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

	/* Serialize to string and return as malloc'd C string.
	 * Some legacy levels contain non-UTF-8 bytes in title fields. */
	std::string result = j.dump(-1, ' ', false, json::error_handler_t::replace);
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

extern "C" int game_introspect_dump_requested(void)
{
	return introspect_requested;
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
