/* Owned D1 data for the compatibility implementation
 * Engine callers use the small d1_in_d2.h lifecycle facade */
#ifndef D1_IN_D2_ASSETS_H
#define D1_IN_D2_ASSETS_H

#include "object.h"
#include "bm.h"
#include "effects.h"
#include "wall.h"
#include "robot.h"
#include "weapon.h"
#include "powerup.h"
#include "polyobj.h"
#include "player.h"
#include "cntrlcen.h"
#include "d1_in_d2_bitmaps.h"
#include "d1_custom.h"

#define D1_MAX_EFFECTS 60
#define D1_MAX_PIG_TEXTURES 800
#define D1_MAX_BITMAP_FILES 1630
#define D1_MAX_PIG_SOUNDS 250
#define D1_VCLIP_MAXNUM 70
#define D1_MAX_ROBOT_TYPES 30
#define D1_MAX_ROBOT_JOINTS 600
#define D1_MAX_WEAPON_TYPES 30
#define D1_MAX_POWERUP_TYPES 29
#define D1_MAX_POLYGON_MODELS 85
#define D1_MAX_OBJ_BITMAPS 210
#define D1_MAX_OBJECT_TYPES 100
#define D1_CONTROL_CENTER_OBJECT_TYPE 4
#define D1_MAX_CONTROLCEN_GUNS 4
#define D1_MAX_WALL_ANIMS 30
#define D1_MAX_GAUGE_BMS_PC 80
#define D1_MAX_GAUGE_BMS_MAC 85
#define D1_N_COCKPIT_BITMAPS 4

typedef struct d1_sound_generation {
	digi_sound samples[MAX_SOUND_FILES];
	char names[MAX_SOUND_FILES][9];
	ubyte *data;
	int count;
	size_t bytes;
} d1_sound_generation;

/* Plain audio backends need samples at the output rate; zero retains source
 * rates for a mixer. Preparation validates before replacing the owned arena */
int d1_in_d2_prepare_sound_output(d1_sound_generation *bank, int target_rate);

typedef struct d1_asset_generation {
	struct d1_guidebot_assets *guidebot;
	d1_custom_texture_stats custom_stats;
	bitmap_index textures[D1_MAX_PIG_TEXTURES];
	tmap_info texture_info[D1_MAX_PIG_TEXTURES];
	char texture_names[D1_MAX_PIG_TEXTURES][14];
	wclip wall_anims[D1_MAX_WALL_ANIMS];
	bitmap_index gauges[D1_MAX_GAUGE_BMS_MAC];
	bitmap_index cockpits[D1_N_COCKPIT_BITMAPS];
	ubyte sound_maps[2][D1_MAX_PIG_SOUNDS];
	ubyte object_types[D1_MAX_OBJECT_TYPES];
	ubyte object_ids[D1_MAX_OBJECT_TYPES];
	fix object_strength[D1_MAX_OBJECT_TYPES];
	int num_textures;
	int num_wall_anims;
	int num_gauges;
	int num_cockpits;
	int num_powerups;
	int num_object_types;
	int first_multi_bitmap;
	int exit_model;
	int destroyed_exit_model;
	d1_bitmap_generation *bitmap_data;
	d1_sound_generation sound_bank;
	vclip vclips[D1_VCLIP_MAXNUM];
	eclip effects[D1_MAX_EFFECTS];
	robot_info robots[D1_MAX_ROBOT_TYPES];
	jointpos joints[D1_MAX_ROBOT_JOINTS];
	weapon_info weapons[D1_MAX_WEAPON_TYPES];
	powerup_type_info powerups[D1_MAX_POWERUP_TYPES];
	polymodel *models;
	int dying_models[D1_MAX_POLYGON_MODELS];
	int dead_models[D1_MAX_POLYGON_MODELS];
	bitmap_index obj_bitmaps[D1_MAX_OBJ_BITMAPS];
	ushort obj_bitmap_ptrs[D1_MAX_OBJ_BITMAPS];
	player_ship ship;
	reactor control_center;
	int num_robot_types;
	int num_robot_joints;
	int num_weapon_types;
	int num_vclips;
	int num_effects;
	int num_polygon_models;
	int pigsize;
} d1_asset_generation;

/* Optional D2 content is read from explicit paths into unpublished storage
 * No source is inferred from live tables or the current gameplay profile */
typedef struct d1_guidebot_source {
	const char *ham_path, *pig_path, *palette_path, *sound_path;
	int sample_rate;
} d1_guidebot_source;

typedef struct d1_guidebot_assets d1_guidebot_assets;
typedef struct d1_guidebot_asset_stats {
	int source_robot;
	int robots, models, joints, weapons, powerups, vclips, effects, textures;
	int bitmaps, sounds, logical_sounds;
	size_t model_bytes, sound_bytes;
} d1_guidebot_asset_stats;

d1_guidebot_assets *d1_in_d2_read_guidebot_source(const d1_guidebot_source *source, const char **error);
void d1_in_d2_free_guidebot_source(d1_guidebot_assets *assets);
void d1_in_d2_guidebot_source_stats(const d1_guidebot_assets *assets, d1_guidebot_asset_stats *stats);
/* Attach only after custom definitions; failure leaves the base unchanged */
int d1_in_d2_prepare_guidebot_extension(d1_asset_generation *base, const d1_guidebot_source *source, const char **error);
int d1_in_d2_validate_guidebot_extension(const d1_asset_generation *base);
/* All fallible preparation precedes publication of either bank */
int d1_in_d2_prepare_guidebot_output(d1_guidebot_assets *assets, int target_rate);
void d1_in_d2_publish_guidebot_extension(d1_asset_generation *base);
int d1_in_d2_guidebot_owns_model(int model_num);

/* Preparation owns all buffers and never publishes live tables. Publication
 * transfers model_data to Polygon_models/free_polygon_models and the sample
 * arena to SoundBits/piggy_close. The active generation retains bitmap pixels
 * borrowed by GameBitmaps until registry users and caches have been released */
d1_asset_generation *d1_in_d2_read_assets(const char *pig_name, const char *palette_name, const char **error);
void d1_in_d2_free_assets(d1_asset_generation *generation);
/* Shared by base/custom preparation and publication, before touching live data */
int d1_in_d2_validate_asset_references(const d1_asset_generation *generation, const char **error);
void d1_in_d2_release_asset_data(void);
/* Consumes the prepared generation on success; caller has released level users */
int d1_in_d2_publish_assets(d1_asset_generation *generation, const char **error);
/* Data owner called only by the facade above; does not select the session profile */
int d1_in_d2_publish_asset_data(d1_asset_generation *generation, const char **error);

/* Transitional overlay operations; only the session facade orders these */
void d1_in_d2_apply_effects(int active);
void d1_in_d2_apply_powerup_vclips(int active);
void d1_in_d2_apply_wall_anims(int active);
void d1_in_d2_apply_robot_assets(int active);
int d1_in_d2_apply_sounds(int active);
const char *d1_in_d2_sound_validation_error(void);
void d1_in_d2_apply_cockpit(int active);
int d1_in_d2_prepare_guidebot_assets(void);
int d1_in_d2_validate_assets(void);
const char *d1_in_d2_asset_validation_error(void);

#endif
