/*
 *
 * D1 session integration for running original D1 content in the D2 engine
 *
 */

#ifndef _D1_IN_D2_H
#define _D1_IN_D2_H

typedef struct d1_in_d2_asset_stats {
	int effects_active;
	int effects_loaded;
	int num_effects;
	int effect_frames_applied;
	int effect_frames_skipped;
	int powerup_vclips_active;
	int powerup_vclips_loaded;
	int num_vclips;
	int powerup_frames_applied;
	int powerup_frames_skipped;
	int wall_anims_active;
	int wall_anims_loaded;
	int wall_anim_count;
	int wall_anim_frames_converted;
	int robot_assets_active;
	int robot_pig_present;
	int robot_pig_size;
	int robot_types;
	int robot_joints;
	int robot_models;
	int weapon_records_active;
	int weapon_types;
	int player_ship_active;
	int robot_obj_bitmaps;
	int robot_obj_bitmaps_applied;
	int robot_obj_bitmaps_skipped;
	int robot_objects_updated;
	int sounds_active;
	int sound_pig_present;
	int sound_pig_size;
	int sound_map_entries;
	int sound_files;
	int sound_bytes;
	int cockpit_active;
	int cockpit_frames_applied;
	int cockpit_frames_skipped;
} d1_in_d2_asset_stats;

int d1_in_d2_prepare_level_assets(char *level_name);
/* At intro entry, with the prior game hidden, publish the requested base profile
 * Level preparation still applies that level's custom definitions before decode */
void d1_in_d2_prepare_intro_assets(void);
/* Select before text, palette, fonts or game definitions; 0 chooses available data */
int d1_in_d2_init_base_resources(int requested_version);
int d1_in_d2_prepare_base_assets(void);
/* Notify after ordinary D2 base initialization installs definitions and sounds
 * Mission/profile transitions commit through the facade's loading operation */
void d1_in_d2_note_d2_base_loaded(void);
char *d1_in_d2_startup_palette(void);
void d1_in_d2_init_startup_bitmaps(void);
/* Rebuild the selected content baseline after a full game-data teardown
 * With no mission selected, retain the explicit startup content choice */
void d1_in_d2_restore_base_resources(void);
/* Reload the selected mission's definitions; D1 publication is deferred to
 * intro/level preparation. Returns whether a D2 extra-robot movie is needed
 * Call at a load boundary with the old world no longer executing */
int d1_in_d2_load_mission_assets(void);
int d1_in_d2_has_native_assets(void);
void d1_in_d2_release_assets(void);

void d1_in_d2_get_stats(d1_in_d2_asset_stats *stats);
int d1_in_d2_ensure_spawnable_guidebot(void);
int d1_in_d2_is_spawnable_guidebot_model(int model_num);
void d1_in_d2_note_spawnable_guidebot_draw(int model_num);
/* Installed content identity survives descriptor changes until asset retirement
 * Before publication/after retirement, bootstrap uses the requested content */
int d1_in_d2_use_d1_gameplay(void);
/* Extended logical references are owned by the published optional generation
 * Invalid/unavailable references return -1 without changing native sound maps */
int d1_in_d2_translate_extended_sound(int soundno);
int d1_in_d2_untranslate_extended_sound(int sample);

#endif /* _D1_IN_D2_H */
