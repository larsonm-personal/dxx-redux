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
	int robot_assets_active;
	int robot_pig_present;
	int robot_pig_size;
	int robot_types;
	int robot_joints;
	int robot_models;
	int robot_obj_bitmaps;
	int robot_obj_bitmaps_applied;
	int robot_obj_bitmaps_skipped;
	int robot_objects_updated;
} d1_in_d2_asset_stats;

void d1_in_d2_apply_effects(int active);
void d1_in_d2_apply_powerup_vclips(int active);
void d1_in_d2_apply_robot_assets(int active);
void d1_in_d2_get_stats(d1_in_d2_asset_stats *stats);
int d1_in_d2_ensure_spawnable_guidebot(void);

#endif /* _D1_IN_D2_H */
