/*
 *
 * D1 compatibility overlays for running D1 missions in the D2 engine.
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

void d1_in_d2_apply_effects(int active);
void d1_in_d2_apply_powerup_vclips(int active);
void d1_in_d2_apply_wall_anims(int active);
void d1_in_d2_apply_robot_assets(int active);
void d1_in_d2_apply_sounds(int active);
void d1_in_d2_apply_cockpit(int active);
int d1_in_d2_prepare_guidebot_assets(void);
void d1_in_d2_get_stats(d1_in_d2_asset_stats *stats);
int d1_in_d2_ensure_spawnable_guidebot(void);
int d1_in_d2_is_spawnable_guidebot_model(int model_num);
void d1_in_d2_note_spawnable_guidebot_draw(int model_num);
int d1_in_d2_use_d1_gameplay(void);
int d1_in_d2_use_d1_robot_aiming(void);

#endif /* _D1_IN_D2_H */
