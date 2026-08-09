/*
 *
 * D1 compatibility overlays for running D1 missions in the D2 engine.
 *
 */

#include <string.h>

#include "pstypes.h"
#include "inferno.h"
#include "object.h"
#include "gamemine.h"
#include "piggy.h"
#include "polyobj.h"
#include "effects.h"
#include "byteswap.h"
#include "bm.h"
#include "gamepal.h"
#include "gamesave.h"
#include "gauges.h"
#include "hash.h"
#include "mission.h"
#include "player.h"
#include "palette.h"
#include "powerup.h"
#include "rle.h"
#include "robot.h"
#include "sounds.h"
#include "strutil.h"
#include "u_mem.h"
#include "vclip.h"
#include "wall.h"
#include "d1_in_d2.h"
#include "d1_pig_validation.h"
#include "laser.h"
#include "weapon.h"
#include "android_log.h"

#ifdef ANDROID
#define D1_IN_D2_LOG(...) debug_log(DLOG_GAME, __VA_ARGS__)
#else
#define D1_IN_D2_LOG(...) ((void)0)
#endif

#define D1_MAX_EFFECTS 60
#define D1_MAX_PIG_TEXTURES 800
#define D1_MAX_PIG_SOUNDS 250
#define D1_VCLIP_MAXNUM 70
#define D1_MAX_ROBOT_TYPES 30
#define D1_MAX_ROBOT_JOINTS 600
#define D1_MAX_WEAPON_TYPES 30
#define D1_MAX_POWERUP_TYPES 29
#define D1_MAX_POLYGON_MODELS 85
#define D1_MAX_OBJ_BITMAPS 210
#define D1_MAX_WALL_ANIMS 30
#define D1_MAX_GAUGE_BMS_PC 80
#define D1_MAX_GAUGE_BMS_MAC 85
#define D1_N_COCKPIT_BITMAPS 4
#define D1_TMAP_INFO_SIZE 26
#define D1_VCLIP_SIZE 82
#define D1_WCLIP_SIZE 66
#define D1_DISKBITMAPHEADER_SIZE 17
#define D1_DISKSOUNDHEADER_SIZE 20
#define D1_WEAPON_INFO_SIZE 115
#define D1_ROBOT_INFO_SIZE 486
#define D1_JOINTPOS_SIZE 8
#define D1_POLYMODEL_SIZE 734
#define D1_PLAYER_SHIP_SIZE 132
#define D1_ROBOT_BITMAP_SLOT_BASE (MAX_BITMAP_FILES - D1_MAX_OBJ_BITMAPS)

#define D1_MODEL_OP_EOF 0
#define D1_MODEL_OP_DEFPOINTS 1
#define D1_MODEL_OP_FLATPOLY 2
#define D1_MODEL_OP_TMAPPOLY 3
#define D1_MODEL_OP_SORTNORM 4
#define D1_MODEL_OP_RODBM 5
#define D1_MODEL_OP_SUBCALL 6
#define D1_MODEL_OP_DEFP_START 7
#define D1_MODEL_OP_GLOW 8

static int D1_effects_active = 0;
static int D1_effects_saved = 0;
static int D1_effects_loaded = 0;
static eclip D1_original_effects[MAX_EFFECTS];
static eclip D1_effects[D1_MAX_EFFECTS];
static int D1_num_effects = 0;
static int D1_powerup_vclips_active = 0;
static int D1_powerup_vclips_saved = 0;
static int D1_powerup_vclips_loaded = 0;
static vclip D1_original_vclips[VCLIP_MAXNUM];
static vclip D1_vclips[D1_VCLIP_MAXNUM];
static int D1_num_vclips = 0;
static int D1_wall_anims_active = 0;
static int D1_wall_anims_saved = 0;
static int D1_wall_anims_loaded = 0;
static int D1_original_num_wall_anims = 0;
static wclip D1_original_wall_anims[MAX_WALL_ANIMS];
static wclip D1_wall_anims[D1_MAX_WALL_ANIMS];
static int D1_num_wall_anims = 0;
static int D1_robot_assets_active = 0;
static int D1_robot_bitmap_slots_registered = 0;
static int D1_robot_polygon_models_loaded = 0;
static bitmap_index D1_robot_bitmap_slots[D1_MAX_OBJ_BITMAPS];
static int D1_player_ship_active = 0;
static int D1_player_ship_saved = 0;
static player_ship D1_original_player_ship;
static int D1_sounds_active = 0;
static int D1_cockpit_active = 0;
static int D1_cockpit_saved = 0;
static grs_bitmap D1_original_cockpit_bitmaps[N_COCKPIT_BITMAPS];
static int D1_original_cockpit_offsets[N_COCKPIT_BITMAPS];
static ubyte D1_original_cockpit_file_flags[N_COCKPIT_BITMAPS];
static ubyte D1_original_cockpit_valid[N_COCKPIT_BITMAPS];
static grs_bitmap D2_cockpit_bitmaps[N_COCKPIT_BITMAPS];
static ubyte D2_cockpit_bitmap_valid[N_COCKPIT_BITMAPS];
static ubyte *D1_cockpit_live_bitmap_data[N_COCKPIT_BITMAPS];
static grs_bitmap D1_original_gauge_bitmaps[2][MAX_GAUGE_BMS];
static int D1_original_gauge_offsets[2][MAX_GAUGE_BMS];
static ubyte D1_original_gauge_file_flags[2][MAX_GAUGE_BMS];
static ubyte D1_original_gauge_valid[2][MAX_GAUGE_BMS];
static grs_bitmap D2_gauge_bitmaps[2][MAX_GAUGE_BMS];
static ubyte D2_gauge_bitmap_valid[2][MAX_GAUGE_BMS];
static ubyte *D1_gauge_live_bitmap_data[2][MAX_GAUGE_BMS];
static int D2_guidebot_assets_saved = 0;
static robot_info D2_guidebot_robot_info;
static jointpos D2_guidebot_joints[MAX_ROBOT_JOINTS];
static int D2_guidebot_joint_count = 0;
static polymodel D2_guidebot_model;
static bitmap_index D2_guidebot_obj_bitmaps[MAX_POLYOBJ_TEXTURES];
static grs_bitmap D2_guidebot_bitmaps[MAX_POLYOBJ_TEXTURES];
static ubyte D2_guidebot_bitmap_valid[MAX_POLYOBJ_TEXTURES];
static ubyte *D2_guidebot_live_bitmap_data[MAX_POLYOBJ_TEXTURES];
static int D2_d1_robot_tuning_saved = 0;
static int D2_d1_robot_tuning_count = 0;
static ubyte D2_d1_robot_behavior[D1_MAX_ROBOT_TYPES];
static sbyte D2_d1_robot_lightcast[D1_MAX_ROBOT_TYPES];
static int D1_spawnable_guidebot_model_index = -1;
static int D1_spawnable_guidebot_obj_bitmap_base = -1;
static int D1_spawnable_guidebot_obj_bitmap_count = 0;
static int D1_spawnable_guidebot_draw_logged = 0;
static d1_in_d2_asset_stats Last_stats;

extern int read_hamfile();
extern hashtable AllDigiSndNames;
extern ubyte *SoundBits;
extern int Num_sound_files;
extern int SoundOffset[MAX_SOUND_FILES];

static void read_d1_robot_info(robot_info *ri, PHYSFS_file *fp);
static void read_d1_weapon_info(weapon_info *wi, int weapon_id, PHYSFS_file *fp);

static ushort model_word(ubyte *p)
{
	return *(ushort *)p;
}

static void set_model_word(ubyte *p, ushort value)
{
	*(ushort *)p = value;
}

void d1_in_d2_get_stats(d1_in_d2_asset_stats *stats)
{
	if (stats)
		*stats = Last_stats;
}

static PHYSFS_file *open_d1_registered_pig()
{
	PHYSFS_file *fp;
	int pigsize;

	fp = PHYSFSX_openReadBuffered(D1_PIGFILE);
	if (!fp)
		return NULL;

	pigsize = (int)PHYSFS_fileLength(fp);
	switch (pigsize) {
		case D1_SHARE_BIG_PIGSIZE:
		case D1_SHARE_10_PIGSIZE:
		case D1_SHARE_PIGSIZE:
		case D1_10_BIG_PIGSIZE:
		case D1_10_PIGSIZE:
			PHYSFS_close(fp);
			return NULL;
		case D1_PIGSIZE:
		case D1_OEM_PIGSIZE:
		case D1_MAC_PIGSIZE:
		case D1_MAC_SHARE_PIGSIZE:
		default:
			PHYSFSX_readInt(fp);
			break;
	}
	return fp;
}

static void seek_d1_vclip_table(PHYSFS_file *fp)
{
	PHYSFSX_readInt(fp);
	PHYSFSX_fseek(fp, D1_MAX_PIG_TEXTURES * sizeof(bitmap_index), SEEK_CUR);
	PHYSFSX_fseek(fp, D1_MAX_PIG_TEXTURES * D1_TMAP_INFO_SIZE, SEEK_CUR);
	PHYSFSX_fseek(fp, 2 * D1_MAX_PIG_SOUNDS, SEEK_CUR);
}

static void skip_d1_vclips_and_effects(PHYSFS_file *fp)
{
	int num_effects;

	PHYSFSX_readInt(fp);
	PHYSFSX_fseek(fp, D1_VCLIP_MAXNUM * D1_VCLIP_SIZE, SEEK_CUR);
	num_effects = PHYSFSX_readInt(fp);
	(void)num_effects;
	PHYSFSX_fseek(fp, D1_MAX_EFFECTS * sizeof(eclip), SEEK_CUR);
}

static int d1_pig_has_bytes(PHYSFS_file *fp, PHYSFS_sint64 pigsize, PHYSFS_sint64 size)
{
	PHYSFS_sint64 position = PHYSFS_tell(fp);

	return position >= 0 && size >= 0 && position <= pigsize && size <= pigsize - position;
}

static int d1_pig_skip(PHYSFS_file *fp, PHYSFS_sint64 pigsize, PHYSFS_sint64 size)
{
	if (!d1_pig_has_bytes(fp, pigsize, size) || size > 0x7fffffff)
		return 0;
	return PHYSFSX_fseek(fp, (long)size, SEEK_CUR) == 0;
}

static int validate_d1_robot_assets(PHYSFS_file *fp, int pigsize)
{
	robot_info robots[D1_MAX_ROBOT_TYPES];
	polymodel *models = NULL;
	ubyte *model_data = NULL;
	player_ship ship;
	int dying_models[D1_MAX_POLYGON_MODELS];
	int dead_models[D1_MAX_POLYGON_MODELS];
	ushort obj_bitmap_ptrs[D1_MAX_OBJ_BITMAPS];
	int num_textures, num_vclips, num_effects, num_wall_anims, num_robot_types, num_robot_joints;
	int num_weapon_types, num_powerups, num_polygon_models, d1_gauge_count;
	int i, gun, state, valid = 0;

	if (!d1_pig_has_bytes(fp, pigsize, sizeof(int)))
		goto done;
	num_textures = PHYSFSX_readInt(fp);
	if (num_textures < 0 || num_textures > D1_MAX_PIG_TEXTURES ||
	    !d1_pig_skip(fp, pigsize, D1_MAX_PIG_TEXTURES * (PHYSFS_sint64)sizeof(bitmap_index)) ||
	    !d1_pig_skip(fp, pigsize, D1_MAX_PIG_TEXTURES * D1_TMAP_INFO_SIZE) ||
	    !d1_pig_skip(fp, pigsize, 2 * D1_MAX_PIG_SOUNDS) ||
	    !d1_pig_has_bytes(fp, pigsize, sizeof(int)))
		goto done;
	num_vclips = PHYSFSX_readInt(fp);
	if (num_vclips == 0)
		num_vclips = D1_VCLIP_MAXNUM;
	if (num_vclips < 0 || num_vclips > D1_VCLIP_MAXNUM ||
	    !d1_pig_skip(fp, pigsize, D1_VCLIP_MAXNUM * D1_VCLIP_SIZE) ||
	    !d1_pig_has_bytes(fp, pigsize, sizeof(int)))
		goto done;
	num_effects = PHYSFSX_readInt(fp);
	if (num_effects < 0 || num_effects > D1_MAX_EFFECTS ||
	    !d1_pig_skip(fp, pigsize, D1_MAX_EFFECTS * (PHYSFS_sint64)sizeof(eclip)) ||
	    !d1_pig_has_bytes(fp, pigsize, sizeof(int)))
		goto done;

	num_wall_anims = PHYSFSX_readInt(fp);
	if (num_wall_anims < 0 || num_wall_anims > D1_MAX_WALL_ANIMS ||
	    !d1_pig_skip(fp, pigsize, D1_MAX_WALL_ANIMS * D1_WCLIP_SIZE) ||
	    !d1_pig_has_bytes(fp, pigsize, sizeof(int)))
		goto done;

	num_robot_types = PHYSFSX_readInt(fp);
	if (num_robot_types < 0 || num_robot_types > D1_MAX_ROBOT_TYPES ||
	    !d1_pig_has_bytes(fp, pigsize, D1_MAX_ROBOT_TYPES * D1_ROBOT_INFO_SIZE))
		goto done;
	for (i = 0; i < D1_MAX_ROBOT_TYPES; i++)
		read_d1_robot_info(&robots[i], fp);
	if (!d1_pig_has_bytes(fp, pigsize, sizeof(int)))
		goto done;

	num_robot_joints = PHYSFSX_readInt(fp);
	if (num_robot_joints < 0 || num_robot_joints > D1_MAX_ROBOT_JOINTS ||
	    !d1_pig_skip(fp, pigsize, D1_MAX_ROBOT_JOINTS * D1_JOINTPOS_SIZE) ||
	    !d1_pig_has_bytes(fp, pigsize, sizeof(int)))
		goto done;

	num_weapon_types = PHYSFSX_readInt(fp);
	if (num_weapon_types < 0 || num_weapon_types > D1_MAX_WEAPON_TYPES ||
	    !d1_pig_skip(fp, pigsize, D1_MAX_WEAPON_TYPES * D1_WEAPON_INFO_SIZE) ||
	    !d1_pig_has_bytes(fp, pigsize, sizeof(int)))
		goto done;

	num_powerups = PHYSFSX_readInt(fp);
	if (num_powerups < 0 || num_powerups > D1_MAX_POWERUP_TYPES ||
	    !d1_pig_skip(fp, pigsize, D1_MAX_POWERUP_TYPES * (PHYSFS_sint64)sizeof(powerup_type_info)) ||
	    !d1_pig_has_bytes(fp, pigsize, sizeof(int)))
		goto done;

	num_polygon_models = PHYSFSX_readInt(fp);
	if (num_polygon_models <= 0 || num_polygon_models > D1_MAX_POLYGON_MODELS ||
	    !d1_pig_has_bytes(fp, pigsize, num_polygon_models * D1_POLYMODEL_SIZE))
		goto done;
	MALLOC(models, polymodel, num_polygon_models);
	if (!models)
		goto done;
	memset(models, 0, num_polygon_models * sizeof(*models));
	polymodel_read_n(models, num_polygon_models, fp);
	for (i = 0; i < num_polygon_models; i++) {
		int submodel;

		if (models[i].n_models <= 0 || models[i].n_models > MAX_SUBMODELS ||
		    models[i].model_data_size <= 0 ||
		    models[i].first_texture + models[i].n_textures > D1_MAX_OBJ_BITMAPS ||
		    (models[i].simpler_model && models[i].simpler_model > num_polygon_models))
			goto done;
		for (submodel = 0; submodel < models[i].n_models; submodel++) {
			if (models[i].submodel_ptrs[submodel] < 0 ||
			    models[i].submodel_ptrs[submodel] >= models[i].model_data_size ||
			    (submodel && models[i].submodel_parents[submodel] >= models[i].n_models))
				goto done;
		}
		if (!d1_pig_has_bytes(fp, pigsize, models[i].model_data_size))
			goto done;
		model_data = d_malloc(models[i].model_data_size);
		if (!model_data || PHYSFS_read(fp, model_data, 1, models[i].model_data_size) != models[i].model_data_size ||
		    !d1_pig_validate_model_stream(model_data, models[i].model_data_size, 0))
			goto done;
		for (submodel = 0; submodel < models[i].n_models; submodel++)
			if (!d1_pig_validate_model_stream(model_data, models[i].model_data_size,
			                                     models[i].submodel_ptrs[submodel]))
				goto done;
		d_free(model_data);
		model_data = NULL;
	}

	d1_gauge_count = (pigsize == D1_MAC_PIGSIZE || pigsize == D1_MAC_SHARE_PIGSIZE)
		? D1_MAX_GAUGE_BMS_MAC : D1_MAX_GAUGE_BMS_PC;
	if (!d1_pig_skip(fp, pigsize, d1_gauge_count * (PHYSFS_sint64)sizeof(bitmap_index)) ||
	    !d1_pig_has_bytes(fp, pigsize, 2 * D1_MAX_POLYGON_MODELS * (PHYSFS_sint64)sizeof(int)))
		goto done;
	for (i = 0; i < D1_MAX_POLYGON_MODELS; i++)
		dying_models[i] = PHYSFSX_readInt(fp);
	for (i = 0; i < D1_MAX_POLYGON_MODELS; i++)
		dead_models[i] = PHYSFSX_readInt(fp);
	if (!d1_pig_skip(fp, pigsize, D1_MAX_OBJ_BITMAPS * (PHYSFS_sint64)sizeof(bitmap_index)) ||
	    !d1_pig_has_bytes(fp, pigsize, D1_MAX_OBJ_BITMAPS * (PHYSFS_sint64)sizeof(short)))
		goto done;
	for (i = 0; i < D1_MAX_OBJ_BITMAPS; i++)
		obj_bitmap_ptrs[i] = PHYSFSX_readShort(fp);
	if (!d1_pig_has_bytes(fp, pigsize, D1_PLAYER_SHIP_SIZE))
		goto done;
	player_ship_read(&ship, fp);

	if (!d1_pig_valid_model_index(ship.model_num, num_polygon_models) ||
	    ship.expl_vclip_num < 0 || ship.expl_vclip_num >= num_vclips)
		goto done;
	for (i = 0; i < num_polygon_models; i++)
		if (!d1_pig_valid_optional_model_index(dying_models[i], num_polygon_models) ||
		    !d1_pig_valid_optional_model_index(dead_models[i], num_polygon_models))
			goto done;
	for (i = 0; i < D1_MAX_OBJ_BITMAPS; i++)
		if (obj_bitmap_ptrs[i] >= D1_MAX_OBJ_BITMAPS)
			goto done;
	for (i = 0; i < num_robot_types; i++) {
		robot_info *robot = &robots[i];
		if (!d1_pig_valid_model_index(robot->model_num, num_polygon_models) ||
		    robot->n_guns < 0 || robot->n_guns > MAX_GUNS ||
		    robot->weapon_type < 0 || robot->weapon_type >= num_weapon_types)
			goto done;
		for (gun = 0; gun < robot->n_guns; gun++)
			if (robot->gun_submodels[gun] >= models[robot->model_num].n_models)
				goto done;
		for (gun = 0; gun < MAX_GUNS + 1; gun++)
			for (state = 0; state < N_ANIM_STATES; state++) {
				jointlist *joints = &robot->anim_states[gun][state];
				if (joints->n_joints < 0 || joints->offset < 0 ||
				    joints->offset > num_robot_joints ||
				    joints->n_joints > num_robot_joints - joints->offset)
					goto done;
			}
	}
	valid = 1;

done:
	if (model_data)
		d_free(model_data);
	if (models)
		d_free(models);
	return valid;
}

static int seek_d1_final_sound_maps(PHYSFS_file *fp, int pigsize, bitmap_index d1_cockpit[D1_N_COCKPIT_BITMAPS])
{
	robot_info temp_robot;
	weapon_info temp_weapon;
	player_ship temp_ship;
	bitmap_index temp_cockpit[D1_N_COCKPIT_BITMAPS];
	jointpos *temp_joints;
	polymodel *temp_models;
	int num_wall_anims, num_robot_types, num_robot_joints, num_weapon_types;
	int num_powerups, num_polygon_models, num_cockpits, d1_gauge_count;
	int i;

	seek_d1_vclip_table(fp);
	skip_d1_vclips_and_effects(fp);

	num_wall_anims = PHYSFSX_readInt(fp);
	if (num_wall_anims < 0 || num_wall_anims > D1_MAX_WALL_ANIMS)
		return 0;
	PHYSFSX_fseek(fp, D1_MAX_WALL_ANIMS * D1_WCLIP_SIZE, SEEK_CUR);

	num_robot_types = PHYSFSX_readInt(fp);
	if (num_robot_types < 0 || num_robot_types > D1_MAX_ROBOT_TYPES)
		return 0;
	for (i = 0; i < D1_MAX_ROBOT_TYPES; i++)
		read_d1_robot_info(&temp_robot, fp);

	num_robot_joints = PHYSFSX_readInt(fp);
	if (num_robot_joints < 0 || num_robot_joints > D1_MAX_ROBOT_JOINTS)
		return 0;
	MALLOC(temp_joints, jointpos, D1_MAX_ROBOT_JOINTS);
	if (!temp_joints)
		return 0;
	jointpos_read_n(temp_joints, D1_MAX_ROBOT_JOINTS, fp);
	d_free(temp_joints);

	num_weapon_types = PHYSFSX_readInt(fp);
	if (num_weapon_types < 0 || num_weapon_types > D1_MAX_WEAPON_TYPES)
		return 0;
	for (i = 0; i < D1_MAX_WEAPON_TYPES; i++)
		read_d1_weapon_info(&temp_weapon, i, fp);

	num_powerups = PHYSFSX_readInt(fp);
	if (num_powerups < 0 || num_powerups > D1_MAX_POWERUP_TYPES)
		return 0;
	PHYSFSX_fseek(fp, D1_MAX_POWERUP_TYPES * sizeof(powerup_type_info), SEEK_CUR);

	num_polygon_models = PHYSFSX_readInt(fp);
	if (num_polygon_models < 0 || num_polygon_models > D1_MAX_POLYGON_MODELS)
		return 0;
	MALLOC(temp_models, polymodel, num_polygon_models ? num_polygon_models : 1);
	if (!temp_models)
		return 0;
	polymodel_read_n(temp_models, num_polygon_models, fp);
	for (i = 0; i < num_polygon_models; i++)
		PHYSFSX_fseek(fp, temp_models[i].model_data_size, SEEK_CUR);
	d_free(temp_models);

	d1_gauge_count = (pigsize == D1_MAC_PIGSIZE || pigsize == D1_MAC_SHARE_PIGSIZE)
		? D1_MAX_GAUGE_BMS_MAC : D1_MAX_GAUGE_BMS_PC;
	PHYSFSX_fseek(fp, d1_gauge_count * sizeof(bitmap_index), SEEK_CUR);
	PHYSFSX_fseek(fp, D1_MAX_POLYGON_MODELS * sizeof(int), SEEK_CUR);
	PHYSFSX_fseek(fp, D1_MAX_POLYGON_MODELS * sizeof(int), SEEK_CUR);
	PHYSFSX_fseek(fp, D1_MAX_OBJ_BITMAPS * sizeof(bitmap_index), SEEK_CUR);
	PHYSFSX_fseek(fp, D1_MAX_OBJ_BITMAPS * sizeof(short), SEEK_CUR);

	player_ship_read(&temp_ship, fp);
	num_cockpits = PHYSFSX_readInt(fp);
	if (num_cockpits < 0 || num_cockpits > D1_N_COCKPIT_BITMAPS)
		return 0;
	bitmap_index_read_n(d1_cockpit ? d1_cockpit : temp_cockpit, D1_N_COCKPIT_BITMAPS, fp);
	return 1;
}

static int read_d1_sound_maps(PHYSFS_file *fp, int pigsize, ubyte d1_sounds[D1_MAX_PIG_SOUNDS], ubyte d1_alt_sounds[D1_MAX_PIG_SOUNDS])
{
	if (!seek_d1_final_sound_maps(fp, pigsize, NULL))
		return 0;
	if (PHYSFS_read(fp, d1_sounds, sizeof(ubyte), D1_MAX_PIG_SOUNDS) != D1_MAX_PIG_SOUNDS)
		return 0;
	if (PHYSFS_read(fp, d1_alt_sounds, sizeof(ubyte), D1_MAX_PIG_SOUNDS) != D1_MAX_PIG_SOUNDS)
		return 0;
	return 1;
}

static int d1_pig_data_start(PHYSFS_file *fp, int pigsize, int *pig_data_start)
{
	switch (pigsize) {
		case D1_SHARE_BIG_PIGSIZE:
		case D1_SHARE_10_PIGSIZE:
		case D1_SHARE_PIGSIZE:
		case D1_10_BIG_PIGSIZE:
		case D1_10_PIGSIZE:
		case D1_MAC_PIGSIZE:
		case D1_MAC_SHARE_PIGSIZE:
			return 0;
		case D1_PIGSIZE:
		case D1_OEM_PIGSIZE:
		default:
			*pig_data_start = PHYSFSX_readInt(fp);
			return 1;
	}
}

static void save_d2_d1_robot_tuning(void)
{
	int i, count;

	if (D2_d1_robot_tuning_saved)
		return;

	count = N_robot_types < D1_MAX_ROBOT_TYPES ? N_robot_types : D1_MAX_ROBOT_TYPES;
	for (i = 0; i < count; i++) {
		D2_d1_robot_behavior[i] = Robot_info[i].behavior;
		D2_d1_robot_lightcast[i] = Robot_info[i].lightcast;
	}
	D2_d1_robot_tuning_count = count;
	D2_d1_robot_tuning_saved = 1;
}

static void apply_d1_robot_d2_tuning(robot_info *ri, int robot_id)
{
	ri->weapon_type2 = -1;
	ri->aim = 255;
	ri->lightcast = 1;
	if (robot_id < 0 || robot_id >= D2_d1_robot_tuning_count)
		return;

	ri->behavior = D2_d1_robot_behavior[robot_id];
	ri->lightcast = D2_d1_robot_lightcast[robot_id];
}

static void save_d2_player_ship(void)
{
	if (D1_player_ship_saved || !Player_ship)
		return;
	D1_original_player_ship = *Player_ship;
	D1_player_ship_saved = 1;
}

static void apply_d1_player_ship(player_ship *ship)
{
	if (!Player_ship)
		return;
	save_d2_player_ship();
	*Player_ship = *ship;
	D1_player_ship_active = 1;
}

static void restore_d2_player_ship(void)
{
	if (!D1_player_ship_active || !D1_player_ship_saved || !Player_ship)
		return;
	*Player_ship = D1_original_player_ship;
	D1_player_ship_active = 0;
}

int d1_in_d2_use_d1_gameplay(void)
{
	return Current_mission && EMULATING_D1;
}

int d1_in_d2_use_d1_robot_aiming(void)
{
	return d1_in_d2_use_d1_gameplay();
}

void d1_in_d2_apply_sounds(int active)
{
	PHYSFS_file *fp;
	ubyte d1_sounds[D1_MAX_PIG_SOUNDS];
	ubyte d1_alt_sounds[D1_MAX_PIG_SOUNDS];
	digi_sound temp_sound;
	char name[9];
	ubyte *ptr;
	int pigsize, pig_data_start, num_bitmaps, num_sounds, header_size;
	int sound_header_start, sound_data_start, sound_bytes;
	int file_len;
	int i;

	if (!active) {
		if (D1_sounds_active) {
			digi_stop_digi_sounds();
			read_hamfile();
			if (Piggy_hamfile_version >= 3)
				hashtable_free(&AllDigiSndNames);
			read_sndfile();
			piggy_read_sounds();
			digi_free_cached_sounds();
			D1_sounds_active = 0;
		}
		Last_stats.sounds_active = 0;
		Last_stats.sound_pig_present = 0;
		Last_stats.sound_pig_size = 0;
		Last_stats.sound_map_entries = 0;
		Last_stats.sound_files = 0;
		Last_stats.sound_bytes = 0;
		return;
	}

	Last_stats.sounds_active = D1_sounds_active;
	if (D1_sounds_active)
		return;
	Last_stats.sound_pig_present = 0;
	Last_stats.sound_pig_size = 0;
	Last_stats.sound_map_entries = 0;
	Last_stats.sound_files = 0;
	Last_stats.sound_bytes = 0;

	fp = open_d1_registered_pig();
	if (!fp)
		return;
	pigsize = (int)PHYSFS_fileLength(fp);
	Last_stats.sound_pig_present = 1;
	Last_stats.sound_pig_size = pigsize;
	if (!read_d1_sound_maps(fp, pigsize, d1_sounds, d1_alt_sounds)) {
		PHYSFS_close(fp);
		return;
	}
	PHYSFS_close(fp);

	fp = PHYSFSX_openReadBuffered(D1_PIGFILE);
	if (!fp)
		return;
	file_len = (int)PHYSFS_fileLength(fp);
	if (!d1_pig_data_start(fp, pigsize, &pig_data_start)) {
		PHYSFS_close(fp);
		return;
	}
	if (pig_data_start < 0 || pig_data_start > file_len) {
		PHYSFS_close(fp);
		return;
	}
	PHYSFSX_fseek(fp, pig_data_start, SEEK_SET);
	num_bitmaps = PHYSFSX_readInt(fp);
	num_sounds = PHYSFSX_readInt(fp);
	if (num_bitmaps < 0 || num_sounds < 0 || num_sounds > MAX_SOUND_FILES) {
		PHYSFS_close(fp);
		return;
	}
	sound_header_start = pig_data_start + 2 * sizeof(int) + num_bitmaps * D1_DISKBITMAPHEADER_SIZE;
	header_size = num_bitmaps * D1_DISKBITMAPHEADER_SIZE + num_sounds * D1_DISKSOUNDHEADER_SIZE;
	sound_data_start = pig_data_start + 2 * sizeof(int) + header_size;
	if (sound_header_start < pig_data_start || sound_data_start < sound_header_start || sound_data_start > file_len) {
		PHYSFS_close(fp);
		return;
	}
	sound_bytes = 0;
	for (i = 0; i < num_sounds; i++) {
		int length, data_length, offset;

		if (PHYSFSX_fseek(fp, sound_header_start + i * D1_DISKSOUNDHEADER_SIZE + 8, SEEK_SET)) {
			PHYSFS_close(fp);
			return;
		}
		length = PHYSFSX_readInt(fp);
		data_length = PHYSFSX_readInt(fp);
		offset = PHYSFSX_readInt(fp);
		if (length < 0 || data_length < 0 || offset < 0 || sound_data_start + offset < sound_data_start || sound_data_start + offset + length > file_len || sound_bytes + length < sound_bytes) {
			PHYSFS_close(fp);
			return;
		}
		sound_bytes += length;
	}

	digi_stop_digi_sounds();
	if (SoundBits)
		d_free(SoundBits);
	SoundBits = d_malloc(sound_bytes + 16);
	if (!SoundBits) {
		PHYSFS_close(fp);
		Error("Not enough memory to load D1 sounds\n");
	}
	ptr = SoundBits;
	for (i = 0; i < MAX_SOUND_FILES; i++) {
		GameSounds[i].bits = 0;
		GameSounds[i].freq = 0;
		GameSounds[i].length = 0;
		GameSounds[i].data = NULL;
		SoundOffset[i] = 0;
	}
	Num_sound_files = 0;
	hashtable_free(&AllDigiSndNames);
	hashtable_init(&AllDigiSndNames, MAX_SOUND_FILES);
	for (i = 0; i < num_sounds; i++) {
		int length, data_length, offset;

		PHYSFSX_fseek(fp, sound_header_start + i * D1_DISKSOUNDHEADER_SIZE, SEEK_SET);
		PHYSFS_read(fp, name, 8, 1);
		name[8] = 0;
		length = PHYSFSX_readInt(fp);
		data_length = PHYSFSX_readInt(fp);
		(void)data_length;
		offset = PHYSFSX_readInt(fp);
		PHYSFSX_fseek(fp, sound_data_start + offset, SEEK_SET);
		if (PHYSFS_read(fp, ptr, 1, length) != length) {
			PHYSFS_close(fp);
			return;
		}
		temp_sound.bits = 8;
		temp_sound.freq = 11025;
		temp_sound.length = length;
		temp_sound.data = ptr;
		SoundOffset[Num_sound_files] = -1;
		piggy_register_sound(&temp_sound, name, 1);
		ptr += length;
	}
	PHYSFS_close(fp);

	for (i = 0; i < MAX_SOUNDS; i++) {
		Sounds[i] = 255;
		AltSounds[i] = 255;
	}
	for (i = 0; i < D1_MAX_PIG_SOUNDS; i++) {
		Sounds[i] = d1_sounds[i];
		AltSounds[i] = d1_alt_sounds[i];
	}
	digi_free_cached_sounds();
	D1_sounds_active = 1;
	Last_stats.sounds_active = 1;
	Last_stats.sound_map_entries = D1_MAX_PIG_SOUNDS;
	Last_stats.sound_files = num_sounds;
	Last_stats.sound_bytes = sound_bytes;
}

static int read_palette_file(const char *filename, ubyte palette[256 * 3])
{
	PHYSFS_file *fp = PHYSFSX_openReadBuffered(filename);

	if (!fp)
		return 0;
	if (PHYSFS_read(fp, palette, 256, 3) != 3) {
		PHYSFS_close(fp);
		return 0;
	}
	PHYSFS_close(fp);
	return 1;
}

static const char *d1_in_d2_target_palette_name(void)
{
	return Current_level_palette[0] ? Current_level_palette : D1_DEFAULT_PALETTE;
}

static int read_d1_in_d2_target_palette(ubyte palette[256 * 3])
{
	const char *name = d1_in_d2_target_palette_name();

	if (read_palette_file(name, palette))
		return 1;
	return d_stricmp(name, D1_DEFAULT_PALETTE) != 0 &&
	       read_palette_file(D1_DEFAULT_PALETTE, palette);
}

static int read_d2_retained_asset_source_palette(ubyte palette[256 * 3],
                                                 const char **palette_name)
{
	if (read_palette_file(DEFAULT_LEVEL_PALETTE, palette)) {
		*palette_name = DEFAULT_LEVEL_PALETTE;
		return 1;
	}
	if (read_palette_file(D2_DEFAULT_PALETTE, palette)) {
		*palette_name = D2_DEFAULT_PALETTE;
		return 1;
	}
	*palette_name = DEFAULT_LEVEL_PALETTE;
	return 0;
}

static ubyte closest_color_in_palette(const ubyte palette[256 * 3], int r, int g, int b)
{
	int i, best_index = 0;
	long best_value;

	best_value = (long)(r - palette[0]) * (r - palette[0]) +
	             (long)(g - palette[1]) * (g - palette[1]) +
	             (long)(b - palette[2]) * (b - palette[2]);
	for (i = 1; i < 256; i++) {
		const ubyte *rgb = &palette[i * 3];
		long value = (long)(r - rgb[0]) * (r - rgb[0]) +
		             (long)(g - rgb[1]) * (g - rgb[1]) +
		             (long)(b - rgb[2]) * (b - rgb[2]);

		if (value < best_value) {
			best_value = value;
			best_index = i;
			if (!value)
				break;
		}
	}
	return (ubyte)best_index;
}

static void build_colormap_between_palettes(const ubyte source_palette[256 * 3],
                                            const ubyte target_palette[256 * 3],
                                            ubyte colormap[256])
{
	int i;

	for (i = 0; i < 256; i++) {
		const ubyte *rgb = &source_palette[i * 3];

		colormap[i] = closest_color_in_palette(target_palette, rgb[0], rgb[1], rgb[2]);
	}
}

static int bitmap_data_size(const grs_bitmap *bmp)
{
	int size;

	if (!bmp->bm_data)
		return 0;
	if (bmp->bm_flags & BM_FLAG_RLE) {
		memcpy(&size, bmp->bm_data, sizeof(size));
		return size > 0 ? size : 0;
	}
	return bmp->bm_rowsize * bmp->bm_h;
}

static int duplicate_bitmap_data_for_remap(const grs_bitmap *src, ubyte **data)
{
	int capacity, size;

	size = bitmap_data_size(src);
	if (size <= 0)
		return 0;
	capacity = size;
	if (src->bm_flags & BM_FLAG_RLE) {
		int rle_capacity = MAX_BMP_SIZE(src->bm_w, src->bm_h) + 30000;

		if (capacity < rle_capacity)
			capacity = rle_capacity;
	}
	MALLOC(*data, ubyte, capacity);
	if (!*data)
		return 0;
	memcpy(*data, src->bm_data, size);
	return 1;
}

static void release_live_bitmap_data(grs_bitmap *bmp, ubyte **live_data)
{
	if (!*live_data)
		return;
	if (bmp->bm_data == *live_data)
		gr_free_bitmap_data(bmp);
	else
		d_free(*live_data);
	*live_data = NULL;
}

static void remap_bitmap_from_palette(grs_bitmap *bmp, ubyte source_palette[256 * 3],
                                      ubyte target_palette[256 * 3])
{
	ubyte colormap[256];

	build_colormap_between_palettes(source_palette, target_palette, colormap);
	colormap[TRANSPARENCY_COLOR] = TRANSPARENCY_COLOR;
	if (bmp->bm_flags & BM_FLAG_RLE) {
		rle_remap(bmp, colormap);
	} else {
		int transparent_seen = 0;
		int y;
		ubyte *p = bmp->bm_data;

		for (y = 0; y < bmp->bm_h; y++, p += bmp->bm_rowsize) {
			int x;

			for (x = 0; x < bmp->bm_w; x++) {
				if (p[x] == TRANSPARENCY_COLOR)
					transparent_seen = 1;
				p[x] = colormap[p[x]];
			}
		}
		if (transparent_seen)
			gr_set_transparent(bmp, 1);
	}
}

static int install_remapped_bitmap_copy(int bitmap_index, const grs_bitmap *src,
                                        ubyte source_palette[256 * 3],
                                        ubyte target_palette[256 * 3], ubyte **live_data)
{
	grs_bitmap *bmp;
	grs_bitmap remapped;
	ubyte *data;

	if (bitmap_index < 0 || bitmap_index >= MAX_BITMAP_FILES)
		return 0;
	if (!duplicate_bitmap_data_for_remap(src, &data))
		return 0;

	remapped = *src;
	remapped.bm_data = data;
	remapped.bm_parent = NULL;
#ifdef OGL
	remapped.gltexture = NULL;
	remapped.gltexture_mask = NULL;
#endif
	remap_bitmap_from_palette(&remapped, source_palette, target_palette);

	bmp = &GameBitmaps[bitmap_index];
	release_live_bitmap_data(bmp, live_data);
	gr_set_bitmap_data(bmp, NULL);
	*bmp = remapped;
	piggy_bitmap_set_file_state(bitmap_index, 0, bmp->bm_flags);
	*live_data = data;
	return 1;
}

static bitmap_index d1_in_d2_gauge_bitmap(int hires, int gauge)
{
	return hires ? Gauges_hires[gauge] : Gauges[gauge];
}

static void save_original_gauge_bitmaps(void)
{
	int hires, i;

	for (hires = 0; hires < 2; hires++)
		for (i = 0; i < MAX_GAUGE_BMS; i++) {
			bitmap_index bi = d1_in_d2_gauge_bitmap(hires, i);
			int bitmap_index = bi.index;
			grs_bitmap *bmp;
			ubyte *data;

			if (!bitmap_index || bitmap_index < 0 || bitmap_index >= MAX_BITMAP_FILES ||
			    D1_original_gauge_valid[hires][i])
				continue;
			PIGGY_PAGE_IN(bi);
			bmp = &GameBitmaps[bitmap_index];
			D1_original_gauge_bitmaps[hires][i] = *bmp;
			D1_original_gauge_offsets[hires][i] = piggy_bitmap_get_offset(bitmap_index);
			D1_original_gauge_file_flags[hires][i] = piggy_bitmap_get_file_flags(bitmap_index);
			D1_original_gauge_valid[hires][i] = 1;
			if (duplicate_bitmap_data_for_remap(bmp, &data)) {
				D2_gauge_bitmaps[hires][i] = *bmp;
				D2_gauge_bitmaps[hires][i].bm_data = data;
				D2_gauge_bitmaps[hires][i].bm_parent = NULL;
#ifdef OGL
				D2_gauge_bitmaps[hires][i].gltexture = NULL;
				D2_gauge_bitmaps[hires][i].gltexture_mask = NULL;
#endif
				D2_gauge_bitmap_valid[hires][i] = 1;
			}
		}
}

static void restore_original_gauge_bitmaps(void)
{
	int hires, i;

	for (hires = 0; hires < 2; hires++)
		for (i = 0; i < MAX_GAUGE_BMS; i++) {
			bitmap_index bi = d1_in_d2_gauge_bitmap(hires, i);
			int bitmap_index = bi.index;
			grs_bitmap *bmp;

			if (!D1_original_gauge_valid[hires][i] ||
			    bitmap_index < 0 || bitmap_index >= MAX_BITMAP_FILES)
				continue;
			bmp = &GameBitmaps[bitmap_index];
			release_live_bitmap_data(bmp, &D1_gauge_live_bitmap_data[hires][i]);
			gr_set_bitmap_data(bmp, NULL);
			*bmp = D1_original_gauge_bitmaps[hires][i];
			piggy_bitmap_set_file_state(bitmap_index, D1_original_gauge_offsets[hires][i],
			                            D1_original_gauge_file_flags[hires][i]);
			if (D1_original_gauge_offsets[hires][i]) {
				gr_set_bitmap_flags(bmp, BM_FLAG_PAGED_OUT);
				gr_set_bitmap_data(bmp, Piggy_bitmap_cache_data);
			} else
				gr_set_bitmap_flags(bmp, D1_original_gauge_bitmaps[hires][i].bm_flags);
		}
}

static int remap_gauge_bitmaps(ubyte d2_palette[256 * 3], ubyte target_palette[256 * 3],
                               int *skipped)
{
	int applied = 0;
	int hires, i;

	for (hires = 0; hires < 2; hires++)
		for (i = 0; i < MAX_GAUGE_BMS; i++) {
			bitmap_index bi = d1_in_d2_gauge_bitmap(hires, i);
			int bitmap_index = bi.index;

			if (!D2_gauge_bitmap_valid[hires][i] ||
			    bitmap_index < 0 || bitmap_index >= MAX_BITMAP_FILES) {
				if (skipped)
					(*skipped)++;
				continue;
			}
			if (install_remapped_bitmap_copy(bitmap_index, &D2_gauge_bitmaps[hires][i],
			                                  d2_palette, target_palette,
			                                  &D1_gauge_live_bitmap_data[hires][i]))
				applied++;
			else if (skipped)
				(*skipped)++;
		}
	return applied;
}

static void save_original_cockpit_bitmaps(void)
{
	int i;

	if (D1_cockpit_saved)
		return;
	for (i = 0; i < N_COCKPIT_BITMAPS; i++) {
		int bitmap_index = cockpit_bitmap[i].index;
		grs_bitmap *bmp;
		ubyte *data;

		if (bitmap_index < 0 || bitmap_index >= MAX_BITMAP_FILES)
			continue;
		PIGGY_PAGE_IN(cockpit_bitmap[i]);
		bmp = &GameBitmaps[bitmap_index];
		D1_original_cockpit_bitmaps[i] = GameBitmaps[bitmap_index];
		D1_original_cockpit_offsets[i] = piggy_bitmap_get_offset(bitmap_index);
		D1_original_cockpit_file_flags[i] = piggy_bitmap_get_file_flags(bitmap_index);
		D1_original_cockpit_valid[i] = 1;
		if (duplicate_bitmap_data_for_remap(bmp, &data)) {
			D2_cockpit_bitmaps[i] = *bmp;
			D2_cockpit_bitmaps[i].bm_data = data;
			D2_cockpit_bitmaps[i].bm_parent = NULL;
#ifdef OGL
			D2_cockpit_bitmaps[i].gltexture = NULL;
			D2_cockpit_bitmaps[i].gltexture_mask = NULL;
#endif
			D2_cockpit_bitmap_valid[i] = 1;
		}
	}
	save_original_gauge_bitmaps();
	D1_cockpit_saved = 1;
}

void d1_in_d2_apply_cockpit(int active)
{
	ubyte d2_palette[256 * 3];
	ubyte target_palette[256 * 3];
	const char *source_palette_name;
	int i;

	Last_stats.cockpit_active = D1_cockpit_active;
	Last_stats.cockpit_frames_applied = 0;
	Last_stats.cockpit_frames_skipped = 0;

	if (!active) {
		if (D1_cockpit_active) {
			restore_original_gauge_bitmaps();
			for (i = 0; i < N_COCKPIT_BITMAPS; i++) {
				int bitmap_index = cockpit_bitmap[i].index;
				grs_bitmap *bmp;

				if (!D1_original_cockpit_valid[i] ||
				    bitmap_index < 0 || bitmap_index >= MAX_BITMAP_FILES)
					continue;
				bmp = &GameBitmaps[bitmap_index];
				release_live_bitmap_data(bmp, &D1_cockpit_live_bitmap_data[i]);
				gr_set_bitmap_data(bmp, NULL);
				*bmp = D1_original_cockpit_bitmaps[i];
				piggy_bitmap_set_file_state(bitmap_index, D1_original_cockpit_offsets[i],
				                            D1_original_cockpit_file_flags[i]);
				if (D1_original_cockpit_offsets[i]) {
					gr_set_bitmap_flags(bmp, BM_FLAG_PAGED_OUT);
					gr_set_bitmap_data(bmp, Piggy_bitmap_cache_data);
				} else
					gr_set_bitmap_flags(bmp, D1_original_cockpit_bitmaps[i].bm_flags);
			}
			D1_cockpit_active = 0;
		}
		Last_stats.cockpit_active = 0;
		return;
	}

	save_original_cockpit_bitmaps();
	if (!read_d2_retained_asset_source_palette(d2_palette, &source_palette_name)) {
		D1_IN_D2_LOG("D1-in-D2 cockpit palette remap skipped: missing %s and %s",
		             DEFAULT_LEVEL_PALETTE, D2_DEFAULT_PALETTE);
		return;
	}
	if (!read_d1_in_d2_target_palette(target_palette)) {
		D1_IN_D2_LOG("D1-in-D2 cockpit palette remap skipped: missing target %s",
		             d1_in_d2_target_palette_name());
		return;
	}
	D1_IN_D2_LOG("D1-in-D2 cockpit/gauge palette remap begin num_cockpits=%d source='%s' target='%s' loaded='%s' pig='%s'",
	             Num_cockpits, source_palette_name, d1_in_d2_target_palette_name(),
	             last_palette_loaded, last_palette_loaded_pig);

	for (i = 0; i < N_COCKPIT_BITMAPS; i++) {
		bitmap_index d2_bitmap = cockpit_bitmap[i];

		if (!D2_cockpit_bitmap_valid[i] || d2_bitmap.index >= MAX_BITMAP_FILES) {
			Last_stats.cockpit_frames_skipped++;
			continue;
		}
		if (install_remapped_bitmap_copy(d2_bitmap.index, &D2_cockpit_bitmaps[i], d2_palette,
		                                  target_palette, &D1_cockpit_live_bitmap_data[i]))
			Last_stats.cockpit_frames_applied++;
		else
			Last_stats.cockpit_frames_skipped++;
	}
	Last_stats.cockpit_frames_applied += remap_gauge_bitmaps(d2_palette, target_palette,
	                                                         &Last_stats.cockpit_frames_skipped);
	D1_IN_D2_LOG("D1-in-D2 cockpit/gauge palette remap applied %d skipped %d",
	             Last_stats.cockpit_frames_applied, Last_stats.cockpit_frames_skipped);
	D1_cockpit_active = 1;
	Last_stats.cockpit_active = 1;
}

static void read_d1_robot_info(robot_info *ri, PHYSFS_file *fp)
{
	int j, gun, state;

	memset(ri, 0, sizeof(*ri));
	ri->model_num = PHYSFSX_readInt(fp);
	ri->n_guns = PHYSFSX_readInt(fp);
	for (j = 0; j < MAX_GUNS; j++)
		PHYSFSX_readVector(&ri->gun_points[j], fp);
	for (j = 0; j < MAX_GUNS; j++)
		ri->gun_submodels[j] = PHYSFSX_readByte(fp);
	ri->exp1_vclip_num = PHYSFSX_readShort(fp);
	ri->exp1_sound_num = PHYSFSX_readShort(fp);
	ri->exp2_vclip_num = PHYSFSX_readShort(fp);
	ri->exp2_sound_num = PHYSFSX_readShort(fp);
	ri->weapon_type = (sbyte)PHYSFSX_readShort(fp);
	ri->weapon_type2 = -1;
	ri->contains_id = PHYSFSX_readByte(fp);
	ri->contains_count = PHYSFSX_readByte(fp);
	ri->contains_prob = PHYSFSX_readByte(fp);
	ri->contains_type = PHYSFSX_readByte(fp);
	ri->score_value = (short)PHYSFSX_readInt(fp);
	ri->lighting = PHYSFSX_readFix(fp);
	ri->strength = PHYSFSX_readFix(fp);
	ri->mass = PHYSFSX_readFix(fp);
	ri->drag = PHYSFSX_readFix(fp);
	for (j = 0; j < NDL; j++)
		ri->field_of_view[j] = PHYSFSX_readFix(fp);
	for (j = 0; j < NDL; j++)
		ri->firing_wait[j] = PHYSFSX_readFix(fp);
	for (j = 0; j < NDL; j++)
		ri->turn_time[j] = PHYSFSX_readFix(fp);
	for (j = 0; j < NDL; j++)
		PHYSFSX_readFix(fp);
	for (j = 0; j < NDL; j++)
		PHYSFSX_readFix(fp);
	for (j = 0; j < NDL; j++)
		ri->max_speed[j] = PHYSFSX_readFix(fp);
	for (j = 0; j < NDL; j++)
		ri->circle_distance[j] = PHYSFSX_readFix(fp);
	for (j = 0; j < NDL; j++)
		ri->rapidfire_count[j] = PHYSFSX_readByte(fp);
	for (j = 0; j < NDL; j++)
		ri->evade_speed[j] = PHYSFSX_readByte(fp);
	ri->cloak_type = PHYSFSX_readByte(fp);
	ri->attack_type = PHYSFSX_readByte(fp);
	ri->boss_flag = PHYSFSX_readByte(fp);
	ri->see_sound = PHYSFSX_readByte(fp);
	ri->attack_sound = PHYSFSX_readByte(fp);
	ri->claw_sound = PHYSFSX_readByte(fp);
	for (gun = 0; gun < MAX_GUNS + 1; gun++)
		for (state = 0; state < N_ANIM_STATES; state++) {
			ri->anim_states[gun][state].n_joints = PHYSFSX_readShort(fp);
			ri->anim_states[gun][state].offset = PHYSFSX_readShort(fp);
		}
	ri->always_0xabcd = PHYSFSX_readInt(fp);
}

static void read_d1_weapon_info(weapon_info *wi, int weapon_id, PHYSFS_file *fp)
{
	int j;

	memset(wi, 0, sizeof(*wi));
	wi->render_type = (sbyte)PHYSFSX_readByte(fp);
	wi->model_num = (sbyte)PHYSFSX_readByte(fp);
	wi->model_num_inner = (sbyte)PHYSFSX_readByte(fp);
	wi->persistent = (sbyte)PHYSFSX_readByte(fp);
	wi->flash_vclip = (sbyte)PHYSFSX_readByte(fp);
	wi->flash_sound = PHYSFSX_readShort(fp);
	wi->robot_hit_vclip = (sbyte)PHYSFSX_readByte(fp);
	wi->robot_hit_sound = PHYSFSX_readShort(fp);
	wi->wall_hit_vclip = (sbyte)PHYSFSX_readByte(fp);
	wi->wall_hit_sound = PHYSFSX_readShort(fp);
	wi->fire_count = (sbyte)PHYSFSX_readByte(fp);
	wi->ammo_usage = (sbyte)PHYSFSX_readByte(fp);
	wi->weapon_vclip = (sbyte)PHYSFSX_readByte(fp);
	wi->destroyable = (sbyte)PHYSFSX_readByte(fp);
	wi->matter = (sbyte)PHYSFSX_readByte(fp);
	wi->bounce = (sbyte)PHYSFSX_readByte(fp);
	wi->homing_flag = (sbyte)PHYSFSX_readByte(fp);
	PHYSFSX_fseek(fp, 3, SEEK_CUR);
	wi->speedvar = 128;
	wi->flags = 0;
	wi->flash = 0;
	wi->afterburner_size = 0;
	wi->children = weapon_id == SMART_ID ? PLAYER_SMART_HOMING_ID : -1;
	wi->energy_usage = PHYSFSX_readFix(fp);
	wi->fire_wait = PHYSFSX_readFix(fp);
	wi->multi_damage_scale = F1_0;
	bitmap_index_read(&wi->bitmap, fp);
	wi->blob_size = PHYSFSX_readFix(fp);
	wi->flash_size = PHYSFSX_readFix(fp);
	wi->impact_size = PHYSFSX_readFix(fp);
	for (j = 0; j < NDL; j++)
		wi->strength[j] = PHYSFSX_readFix(fp);
	for (j = 0; j < NDL; j++)
		wi->speed[j] = PHYSFSX_readFix(fp);
	wi->mass = PHYSFSX_readFix(fp);
	wi->drag = PHYSFSX_readFix(fp);
	wi->thrust = PHYSFSX_readFix(fp);
	wi->po_len_to_width_ratio = PHYSFSX_readFix(fp);
	wi->light = PHYSFSX_readFix(fp);
	wi->lifetime = PHYSFSX_readFix(fp);
	wi->damage_radius = PHYSFSX_readFix(fp);
	bitmap_index_read(&wi->picture, fp);
	wi->hires_picture = wi->picture;
}

static int read_d1_palette(ubyte palette[256 * 3])
{
	return read_palette_file(D1_DEFAULT_PALETTE, palette);
}

static ushort d1_palette_index_to_15bpp(ubyte palette[256 * 3], ushort color)
{
	ubyte *rgb;

	if (color >= 256)
		return color;

	rgb = &palette[color * 3];
	return (ushort)(((rgb[0] >> 1) << 10) | ((rgb[1] >> 1) << 5) | (rgb[2] >> 1));
}

static void convert_d1_model_flat_colors(ubyte *p, ubyte palette[256 * 3])
{
	ushort opcode;
	ushort nv;

	while ((opcode = model_word(p)) != D1_MODEL_OP_EOF) {
		switch (opcode) {
			case D1_MODEL_OP_DEFPOINTS:
				p += model_word(p + 2) * sizeof(vms_vector) + 4;
				break;

			case D1_MODEL_OP_DEFP_START:
				p += model_word(p + 2) * sizeof(vms_vector) + 8;
				break;

			case D1_MODEL_OP_FLATPOLY:
				nv = model_word(p + 2);
				set_model_word(p + 28, d1_palette_index_to_15bpp(palette, model_word(p + 28)));
				p += 30 + ((nv & ~1) + 1) * 2;
				break;

			case D1_MODEL_OP_TMAPPOLY:
				nv = model_word(p + 2);
				p += 30 + ((nv & ~1) + 1) * 2 + nv * 12;
				break;

			case D1_MODEL_OP_SORTNORM:
				convert_d1_model_flat_colors(p + model_word(p + 28), palette);
				convert_d1_model_flat_colors(p + model_word(p + 30), palette);
				p += 32;
				break;

			case D1_MODEL_OP_RODBM:
				p += 36;
				break;

			case D1_MODEL_OP_SUBCALL:
				convert_d1_model_flat_colors(p + model_word(p + 16), palette);
				p += 20;
				break;

			case D1_MODEL_OP_GLOW:
				p += 4;
				break;

			default:
				return;
		}
	}
}

static void convert_d1_robot_model_flat_colors(int num_polygon_models)
{
	ubyte palette[256 * 3];
	int i;

	if (!read_d1_palette(palette))
		return;

	for (i = 0; i < num_polygon_models; i++)
		convert_d1_model_flat_colors(Polygon_models[i].model_data, palette);
}

static int copy_model_data(polymodel *dest, const polymodel *src)
{
	*dest = *src;
	dest->model_data = d_malloc(src->model_data_size);
	if (!dest->model_data)
		return 0;
	memcpy(dest->model_data, src->model_data, src->model_data_size);
	return 1;
}

static void remove_spawnable_guidebot_assets(void)
{
	if (D1_spawnable_guidebot_model_index >= 0 &&
	    D1_spawnable_guidebot_model_index < N_polygon_models) {
		free_model(&Polygon_models[D1_spawnable_guidebot_model_index]);
		if (D1_spawnable_guidebot_model_index == N_polygon_models - 1)
			N_polygon_models--;
	}
	if (D1_spawnable_guidebot_obj_bitmap_base >= 0 &&
	    N_ObjBitmaps == D1_spawnable_guidebot_obj_bitmap_base + D1_spawnable_guidebot_obj_bitmap_count)
		N_ObjBitmaps = D1_spawnable_guidebot_obj_bitmap_base;
	D1_spawnable_guidebot_model_index = -1;
	D1_spawnable_guidebot_obj_bitmap_base = -1;
	D1_spawnable_guidebot_obj_bitmap_count = 0;
	D1_spawnable_guidebot_draw_logged = 0;
}

static int save_guidebot_bitmap_copy(int texture_index, bitmap_index src)
{
	grs_bitmap *src_bmp;
	ubyte *data;
	int size;

	if (texture_index < 0 || texture_index >= MAX_POLYOBJ_TEXTURES || src.index >= MAX_BITMAP_FILES)
		return 0;
	PIGGY_PAGE_IN(src);
	src_bmp = &GameBitmaps[src.index];
	size = bitmap_data_size(src_bmp);
	if (!size)
		return 0;
	MALLOC(data, ubyte, size);
	if (!data)
		return 0;
	memcpy(data, src_bmp->bm_data, size);

	D2_guidebot_bitmaps[texture_index] = *src_bmp;
	D2_guidebot_bitmaps[texture_index].bm_data = data;
	D2_guidebot_bitmaps[texture_index].bm_parent = NULL;
#ifdef OGL
	D2_guidebot_bitmaps[texture_index].gltexture = NULL;
	D2_guidebot_bitmaps[texture_index].gltexture_mask = NULL;
#endif
	D2_guidebot_bitmap_valid[texture_index] = 1;
	return 1;
}

static void restore_guidebot_bitmap_copies(void)
{
	ubyte d2_palette[256 * 3];
	ubyte target_palette[256 * 3];
	const char *source_palette_name;
	int restored = 0;
	int i;

	if (!D2_guidebot_assets_saved) {
		D1_IN_D2_LOG("D1-in-D2 guidebot texture restore skipped: no saved D2 assets");
		return;
	}
	if (!read_d2_retained_asset_source_palette(d2_palette, &source_palette_name)) {
		D1_IN_D2_LOG("D1-in-D2 guidebot texture restore skipped: missing %s and %s",
		             DEFAULT_LEVEL_PALETTE, D2_DEFAULT_PALETTE);
		return;
	}
	if (!read_d1_in_d2_target_palette(target_palette)) {
		D1_IN_D2_LOG("D1-in-D2 guidebot texture restore skipped: missing target %s",
		             d1_in_d2_target_palette_name());
		return;
	}
	D1_IN_D2_LOG("D1-in-D2 guidebot texture restore begin textures=%d source='%s' target='%s' loaded='%s' pig='%s'",
	             D2_guidebot_model.n_textures, source_palette_name,
	             d1_in_d2_target_palette_name(), last_palette_loaded,
	             last_palette_loaded_pig);
	for (i = 0; i < D2_guidebot_model.n_textures; i++) {
		int bitmap_index = D2_guidebot_obj_bitmaps[i].index;

		if (!D2_guidebot_bitmap_valid[i] || bitmap_index < 0 || bitmap_index >= MAX_BITMAP_FILES)
			continue;
		if (install_remapped_bitmap_copy(bitmap_index, &D2_guidebot_bitmaps[i], d2_palette,
		                                  target_palette, &D2_guidebot_live_bitmap_data[i])) {
			D1_IN_D2_LOG("D1-in-D2 guidebot texture restored tex=%d bitmap=%d size=%dx%d flags=0x%x",
			             i, bitmap_index, GameBitmaps[bitmap_index].bm_w,
			             GameBitmaps[bitmap_index].bm_h, GameBitmaps[bitmap_index].bm_flags);
			restored++;
		}
	}
	D1_IN_D2_LOG("D1-in-D2 guidebot textures restored %d", restored);
}

static void release_guidebot_live_bitmap_copies(void)
{
	int i;

	for (i = 0; i < MAX_POLYOBJ_TEXTURES; i++) {
		int bitmap_index = D2_guidebot_obj_bitmaps[i].index;

		if (bitmap_index < 0 || bitmap_index >= MAX_BITMAP_FILES)
			continue;
		release_live_bitmap_data(&GameBitmaps[bitmap_index], &D2_guidebot_live_bitmap_data[i]);
	}
}

static int save_d2_guidebot_assets(void)
{
	int buddy_id, gun, state, i;
	int model_num;
	int bitmap_copies = 0;
	polymodel *model;

	if (D2_guidebot_assets_saved)
		return 1;

	for (buddy_id = 0; buddy_id < N_robot_types; buddy_id++)
		if (Robot_info[buddy_id].companion)
			break;
	if (buddy_id == N_robot_types)
		return 0;

	model_num = Robot_info[buddy_id].model_num;
	if (model_num < 0 || model_num >= N_polygon_models)
		return 0;
	model = &Polygon_models[model_num];
	if (!model->model_data || model->n_textures > MAX_POLYOBJ_TEXTURES)
		return 0;

	D2_guidebot_robot_info = Robot_info[buddy_id];
	D2_guidebot_joint_count = 0;
	for (gun = 0; gun < MAX_GUNS + 1; gun++)
		for (state = 0; state < N_ANIM_STATES; state++) {
			jointlist *jl = &D2_guidebot_robot_info.anim_states[gun][state];
			int offset = jl->offset;

			if (!jl->n_joints) {
				jl->offset = 0;
				continue;
			}
			if (offset < 0 || offset + jl->n_joints > N_robot_joints ||
			    D2_guidebot_joint_count + jl->n_joints > MAX_ROBOT_JOINTS)
				return 0;
			jl->offset = D2_guidebot_joint_count;
			memcpy(&D2_guidebot_joints[D2_guidebot_joint_count], &Robot_joints[offset],
			       jl->n_joints * sizeof(jointpos));
			D2_guidebot_joint_count += jl->n_joints;
		}

	for (i = 0; i < model->n_textures; i++) {
		int obj_bitmap_ptr = model->first_texture + i;
		int obj_bitmap;

		if (obj_bitmap_ptr < 0 || obj_bitmap_ptr >= MAX_OBJ_BITMAPS)
			return 0;
		obj_bitmap = ObjBitmapPtrs[obj_bitmap_ptr];
		if (obj_bitmap < 0 || obj_bitmap >= MAX_OBJ_BITMAPS)
			return 0;
		D2_guidebot_obj_bitmaps[i] = ObjBitmaps[obj_bitmap];
		if (save_guidebot_bitmap_copy(i, D2_guidebot_obj_bitmaps[i]))
			bitmap_copies++;
	}

	if (!copy_model_data(&D2_guidebot_model, model))
		return 0;
	D2_guidebot_assets_saved = 1;
	D1_IN_D2_LOG("D1-in-D2 guidebot assets saved robot=%d model=%d textures=%d copied=%d first_texture=%d",
	             buddy_id, model_num, model->n_textures, bitmap_copies, model->first_texture);
	return 1;
}

int d1_in_d2_prepare_guidebot_assets(void)
{
	return save_d2_guidebot_assets();
}

int d1_in_d2_ensure_spawnable_guidebot(void)
{
	int i, robot_index, model_index, first_texture, joint_offset;
	polymodel *model;

	if (!D1_robot_assets_active)
		return 1;

	for (i = 0; i < N_robot_types; i++)
		if (Robot_info[i].companion)
			return 1;

	if (!D2_guidebot_assets_saved)
		return 0;

	model = &D2_guidebot_model;
	if (N_robot_types >= MAX_ROBOT_TYPES || N_polygon_models >= MAX_POLYGON_MODELS ||
	    N_ObjBitmaps + model->n_textures > MAX_OBJ_BITMAPS ||
	    N_robot_joints + D2_guidebot_joint_count > MAX_ROBOT_JOINTS)
		return 0;

	robot_index = N_robot_types++;
	model_index = N_polygon_models++;
	first_texture = N_ObjBitmaps;
	joint_offset = N_robot_joints;

	Robot_info[robot_index] = D2_guidebot_robot_info;
	Robot_info[robot_index].model_num = model_index;
	Robot_info[robot_index].companion = 1;
	for (i = 0; i < MAX_GUNS + 1; i++) {
		int state;
		for (state = 0; state < N_ANIM_STATES; state++)
			if (Robot_info[robot_index].anim_states[i][state].n_joints)
				Robot_info[robot_index].anim_states[i][state].offset += joint_offset;
	}
	memcpy(&Robot_joints[N_robot_joints], D2_guidebot_joints, D2_guidebot_joint_count * sizeof(jointpos));
	N_robot_joints += D2_guidebot_joint_count;

	if (!copy_model_data(&Polygon_models[model_index], model)) {
		N_robot_types--;
		N_polygon_models--;
		N_robot_joints = joint_offset;
		return 0;
	}
	Polygon_models[model_index].first_texture = first_texture;
	Polygon_models[model_index].simpler_model = 0;
	Dying_modelnums[model_index] = -1;
	Dead_modelnums[model_index] = -1;
	D1_spawnable_guidebot_model_index = model_index;
	D1_spawnable_guidebot_obj_bitmap_base = first_texture;
	D1_spawnable_guidebot_obj_bitmap_count = model->n_textures;
	D1_spawnable_guidebot_draw_logged = 0;

	for (i = 0; i < model->n_textures; i++) {
		ObjBitmaps[N_ObjBitmaps] = D2_guidebot_obj_bitmaps[i];
		ObjBitmapPtrs[N_ObjBitmaps] = N_ObjBitmaps;
		N_ObjBitmaps++;
	}

	D1_IN_D2_LOG("D1-in-D2 guidebot spawnable robot=%d model=%d first_texture=%d textures=%d joints=%d speed=%d/%d/%d/%d/%d",
	             robot_index, model_index, first_texture, model->n_textures,
	             D2_guidebot_joint_count,
	             Robot_info[robot_index].max_speed[0] / F1_0,
	             Robot_info[robot_index].max_speed[1] / F1_0,
	             Robot_info[robot_index].max_speed[2] / F1_0,
	             Robot_info[robot_index].max_speed[3] / F1_0,
	             Robot_info[robot_index].max_speed[4] / F1_0);
	return 1;
}

int d1_in_d2_is_spawnable_guidebot_model(int model_num)
{
	return model_num >= 0 && model_num == D1_spawnable_guidebot_model_index;
}

void d1_in_d2_note_spawnable_guidebot_draw(int model_num)
{
	int i;
	polymodel *po;

	if (D1_spawnable_guidebot_draw_logged ||
	    !d1_in_d2_is_spawnable_guidebot_model(model_num) ||
	    model_num < 0 || model_num >= N_polygon_models)
		return;

	D1_spawnable_guidebot_draw_logged = 1;
	po = &Polygon_models[model_num];
	D1_IN_D2_LOG("D1-in-D2 guidebot draw model=%d first_texture=%d n_textures=%d objbase=%d objcount=%d",
	             model_num, po->first_texture, po->n_textures,
	             D1_spawnable_guidebot_obj_bitmap_base,
	             D1_spawnable_guidebot_obj_bitmap_count);
	for (i = 0; i < po->n_textures && i < MAX_POLYOBJ_TEXTURES; i++) {
		int obj_bitmap_ptr = po->first_texture + i;
		int obj_bitmap = -1;
		bitmap_index bi;
		grs_bitmap *bmp = NULL;

		bi.index = -1;
		if (obj_bitmap_ptr >= 0 && obj_bitmap_ptr < MAX_OBJ_BITMAPS) {
			obj_bitmap = ObjBitmapPtrs[obj_bitmap_ptr];
			if (obj_bitmap >= 0 && obj_bitmap < MAX_OBJ_BITMAPS) {
				bi = ObjBitmaps[obj_bitmap];
				if (bi.index >= 0 && bi.index < MAX_BITMAP_FILES)
					bmp = &GameBitmaps[bi.index];
			}
		}
		D1_IN_D2_LOG("D1-in-D2 guidebot draw tex=%d ptr=%d obj=%d bitmap=%d size=%dx%d flags=0x%x offset=%d",
		             i, obj_bitmap_ptr, obj_bitmap, bi.index,
		             bmp ? bmp->bm_w : 0, bmp ? bmp->bm_h : 0,
		             bmp ? bmp->bm_flags : 0,
		             bi.index >= 0 && bi.index < MAX_BITMAP_FILES ?
		                     piggy_bitmap_get_offset(bi.index) : -1);
	}
}

static int read_d1_effects()
{
	PHYSFS_file *fp;
	int num_effects;

	if (D1_effects_loaded)
		return D1_num_effects > 0;

	D1_effects_loaded = 1;
	fp = open_d1_registered_pig();
	if (!fp)
		return 0;

	seek_d1_vclip_table(fp);
	PHYSFSX_readInt(fp);
	PHYSFSX_fseek(fp, D1_VCLIP_MAXNUM * D1_VCLIP_SIZE, SEEK_CUR);
	num_effects = PHYSFSX_readInt(fp);
	if (num_effects < 0 || num_effects > D1_MAX_EFFECTS) {
		PHYSFS_close(fp);
		return 0;
	}

	eclip_read_n(D1_effects, D1_MAX_EFFECTS, fp);
	PHYSFS_close(fp);

	D1_num_effects = num_effects;
	return D1_num_effects > 0;
}

static int read_d1_powerup_vclips()
{
	PHYSFS_file *fp;

	if (D1_powerup_vclips_loaded)
		return D1_num_vclips > 0;

	D1_powerup_vclips_loaded = 1;
	fp = open_d1_registered_pig();
	if (!fp)
		return 0;

	seek_d1_vclip_table(fp);
	D1_num_vclips = PHYSFSX_readInt(fp);
	if (D1_num_vclips == 0)
		D1_num_vclips = D1_VCLIP_MAXNUM;
	if (D1_num_vclips < 0 || D1_num_vclips > D1_VCLIP_MAXNUM) {
		PHYSFS_close(fp);
		return 0;
	}
	vclip_read_n(D1_vclips, D1_VCLIP_MAXNUM, fp);
	PHYSFS_close(fp);
	return D1_num_vclips > 0;
}

void d1_in_d2_apply_effects(int active)
{
	int i, j;

	Last_stats.effects_active = active;
	Last_stats.effects_loaded = D1_effects_loaded;
	Last_stats.num_effects = D1_num_effects;
	Last_stats.effect_frames_applied = 0;
	Last_stats.effect_frames_skipped = 0;
	if (!D1_effects_saved) {
		for (i = 0; i < MAX_EFFECTS; i++)
			D1_original_effects[i] = Effects[i];
		D1_effects_saved = 1;
	}
	if (!active && !D1_effects_active)
		return;
	if (active && !read_d1_effects()) {
		Last_stats.effects_active = D1_effects_active;
		return;
	}
	Last_stats.effects_loaded = D1_effects_loaded;
	Last_stats.num_effects = D1_num_effects;
	for (i = 0; i < D1_num_effects && i < MAX_EFFECTS; i++) {
		if (!active) {
			Effects[i] = D1_original_effects[i];
			continue;
		}
		if (D1_effects[i].changing_wall_texture >= 0)
			Effects[i].changing_wall_texture = convert_d1_tmap_num(D1_effects[i].changing_wall_texture);
		if (D1_effects[i].changing_object_texture >= 0 && D1_effects[i].changing_object_texture < MAX_OBJ_BITMAPS)
			Effects[i].changing_object_texture = D1_effects[i].changing_object_texture;
		else
			Effects[i].changing_object_texture = -1;
		if (D1_effects[i].dest_bm_num >= 0)
			Effects[i].dest_bm_num = convert_d1_tmap_num(D1_effects[i].dest_bm_num);
		else
			Effects[i].dest_bm_num = -1;
		Effects[i].vc.play_time = D1_effects[i].vc.play_time;
		Effects[i].vc.frame_time = D1_effects[i].vc.frame_time;
		Effects[i].vc.num_frames = D1_effects[i].vc.num_frames < D1_original_effects[i].vc.num_frames
			? D1_effects[i].vc.num_frames : D1_original_effects[i].vc.num_frames;
		for (j = 0; j < Effects[i].vc.num_frames; j++) {
			if (load_d1_bitmap_frame(D1_effects[i].vc.frames[j].index, D1_original_effects[i].vc.frames[j])) {
				Effects[i].vc.frames[j] = D1_original_effects[i].vc.frames[j];
				Last_stats.effect_frames_applied++;
			} else
				Last_stats.effect_frames_skipped++;
		}
	}
	D1_effects_active = active;
	Last_stats.effects_active = D1_effects_active;
}

void d1_in_d2_apply_powerup_vclips(int active)
{
	int i, j;

	Last_stats.powerup_vclips_active = active;
	Last_stats.powerup_vclips_loaded = D1_powerup_vclips_loaded;
	Last_stats.num_vclips = D1_num_vclips;
	Last_stats.powerup_frames_applied = 0;
	Last_stats.powerup_frames_skipped = 0;
	if (!D1_powerup_vclips_saved) {
		for (i = 0; i < VCLIP_MAXNUM; i++)
			D1_original_vclips[i] = Vclip[i];
		D1_powerup_vclips_saved = 1;
	}
	if (!active && !D1_powerup_vclips_active)
		return;
	if (active && !read_d1_powerup_vclips()) {
		Last_stats.powerup_vclips_active = D1_powerup_vclips_active;
		return;
	}
	Last_stats.powerup_vclips_loaded = D1_powerup_vclips_loaded;
	Last_stats.num_vclips = D1_num_vclips;

	for (i = 0; i < VCLIP_MAXNUM; i++) {
		if (!active) {
			Vclip[i] = D1_original_vclips[i];
			continue;
		}
		if (i >= D1_num_vclips)
			continue;
		Vclip[i].play_time = D1_vclips[i].play_time;
		Vclip[i].frame_time = D1_vclips[i].frame_time;
		Vclip[i].flags = D1_vclips[i].flags;
		Vclip[i].light_value = D1_vclips[i].light_value;
		Vclip[i].num_frames = D1_vclips[i].num_frames < D1_original_vclips[i].num_frames
			? D1_vclips[i].num_frames : D1_original_vclips[i].num_frames;
		for (j = 0; j < Vclip[i].num_frames; j++) {
			if (load_d1_bitmap_frame(D1_vclips[i].frames[j].index, D1_original_vclips[i].frames[j])) {
				Vclip[i].frames[j] = D1_original_vclips[i].frames[j];
				Last_stats.powerup_frames_applied++;
			} else
				Last_stats.powerup_frames_skipped++;
		}
	}

	for (i = 0; i <= Highest_object_index; i++)
		if (Objects[i].type == OBJ_POWERUP && Objects[i].id < MAX_POWERUP_TYPES)
			Objects[i].rtype.vclip_info.frametime = Vclip[Objects[i].rtype.vclip_info.vclip_num].frame_time;

	D1_powerup_vclips_active = active;
	Last_stats.powerup_vclips_active = D1_powerup_vclips_active;
}

static int read_d1_wall_anims(void)
{
	PHYSFS_file *fp;
	int num_wall_anims;

	if (D1_wall_anims_loaded)
		return 1;
	fp = open_d1_registered_pig();
	if (!fp)
		return 0;
	seek_d1_vclip_table(fp);
	skip_d1_vclips_and_effects(fp);

	num_wall_anims = PHYSFSX_readInt(fp);
	if (num_wall_anims < 0 || num_wall_anims > D1_MAX_WALL_ANIMS) {
		PHYSFS_close(fp);
		return 0;
	}
	memset(D1_wall_anims, 0, sizeof(D1_wall_anims));
	REWIND_PHYSFS_FILE(rewind_fp, fp);
	wclip_read_n_d1(D1_wall_anims, D1_MAX_WALL_ANIMS, rewind_fp);
	PHYSFS_close(fp);
	D1_num_wall_anims = num_wall_anims;
	D1_wall_anims_loaded = 1;
	return 1;
}

static void d1_in_d2_reapply_wall_anim_frames(void)
{
	int i, j;

	for (i = 0; i < Num_segments; i++)
		for (j = 0; j < MAX_SIDES_PER_SEGMENT; j++) {
			side *sidep = &Segments[i].sides[j];
			int wall_num = sidep->wall_num;
			int clip_num;

			if (wall_num < 0 || wall_num >= Num_walls)
				continue;
			clip_num = Walls[wall_num].clip_num;
			if (clip_num < 0 || clip_num >= Num_wall_anims)
				continue;
			if (!(WallAnims[clip_num].flags & WCF_TMAP1))
				continue;
			sidep->tmap_num = WallAnims[clip_num].frames[0];
			sidep->tmap_num2 = 0;
		}
}

void d1_in_d2_apply_wall_anims(int active)
{
	int i, j;

	Last_stats.wall_anims_active = active;
	Last_stats.wall_anims_loaded = D1_wall_anims_loaded;
	Last_stats.wall_anim_count = D1_num_wall_anims;
	Last_stats.wall_anim_frames_converted = 0;
	if (!D1_wall_anims_saved) {
		D1_original_num_wall_anims = Num_wall_anims;
		for (i = 0; i < MAX_WALL_ANIMS; i++)
			D1_original_wall_anims[i] = WallAnims[i];
		D1_wall_anims_saved = 1;
	}
	if (!active) {
		if (D1_wall_anims_active) {
			Num_wall_anims = D1_original_num_wall_anims;
			for (i = 0; i < MAX_WALL_ANIMS; i++)
				WallAnims[i] = D1_original_wall_anims[i];
		}
		D1_wall_anims_active = 0;
		Last_stats.wall_anims_active = D1_wall_anims_active;
		return;
	}
	if (!read_d1_wall_anims()) {
		Last_stats.wall_anims_active = D1_wall_anims_active;
		Last_stats.wall_anims_loaded = D1_wall_anims_loaded;
		return;
	}
	for (i = 0; i < D1_num_wall_anims && i < MAX_WALL_ANIMS; i++) {
		WallAnims[i] = D1_wall_anims[i];
		if (WallAnims[i].num_frames > MAX_CLIP_FRAMES)
			WallAnims[i].num_frames = MAX_CLIP_FRAMES;
		for (j = 0; j < WallAnims[i].num_frames; j++) {
			if (WallAnims[i].frames[j] >= 0)
				WallAnims[i].frames[j] = convert_d1_tmap_num(WallAnims[i].frames[j]);
			Last_stats.wall_anim_frames_converted++;
		}
	}
	Num_wall_anims = D1_num_wall_anims;
	d1_in_d2_reapply_wall_anim_frames();
	D1_wall_anims_active = 1;
	Last_stats.wall_anims_active = D1_wall_anims_active;
	Last_stats.wall_anims_loaded = D1_wall_anims_loaded;
	Last_stats.wall_anim_count = D1_num_wall_anims;
}

void d1_in_d2_apply_robot_assets(int active)
{
	PHYSFS_file *fp;
	bitmap_index d1_obj_bitmaps[D1_MAX_OBJ_BITMAPS];
	ushort d1_obj_bitmap_ptrs[D1_MAX_OBJ_BITMAPS];
	player_ship d1_player_ship;
	int i, num_wall_anims, num_robot_types, num_robot_joints, num_weapon_types;
	int num_powerups, num_polygon_models, d1_gauge_count, pigsize;
	int free_model_count;

	if (!active) {
		remove_spawnable_guidebot_assets();
		if (D1_robot_assets_active) {
			release_guidebot_live_bitmap_copies();
			free_polygon_models();
			read_hamfile();
			restore_d2_player_ship();
			D1_robot_assets_active = 0;
			D1_robot_polygon_models_loaded = 0;
		}
		Last_stats.robot_assets_active = 0;
		Last_stats.robot_pig_present = 0;
		Last_stats.robot_pig_size = 0;
		Last_stats.robot_types = 0;
		Last_stats.robot_joints = 0;
		Last_stats.robot_models = 0;
		Last_stats.weapon_records_active = 0;
		Last_stats.weapon_types = 0;
		Last_stats.player_ship_active = 0;
		Last_stats.robot_obj_bitmaps = 0;
		Last_stats.robot_obj_bitmaps_applied = 0;
		Last_stats.robot_obj_bitmaps_skipped = 0;
		Last_stats.robot_objects_updated = 0;
		return;
	}

	Last_stats.robot_assets_active = D1_robot_assets_active;
	Last_stats.robot_pig_present = 0;
	Last_stats.robot_pig_size = 0;
	Last_stats.robot_types = 0;
	Last_stats.robot_joints = 0;
	Last_stats.robot_models = 0;
	Last_stats.weapon_records_active = 0;
	Last_stats.weapon_types = 0;
	Last_stats.player_ship_active = D1_player_ship_active;
	Last_stats.robot_obj_bitmaps = 0;
	Last_stats.robot_obj_bitmaps_applied = 0;
	Last_stats.robot_obj_bitmaps_skipped = 0;
	Last_stats.robot_objects_updated = 0;
	fp = open_d1_registered_pig();
	if (!fp)
		return;
	pigsize = (int)PHYSFS_fileLength(fp);
	if (!validate_d1_robot_assets(fp, pigsize)) {
		D1_IN_D2_LOG("D1-in-D2 robot assets rejected: invalid D1 PIG tables");
		PHYSFS_close(fp);
		return;
	}
	if (PHYSFSX_fseek(fp, sizeof(int), SEEK_SET)) {
		PHYSFS_close(fp);
		return;
	}

	remove_spawnable_guidebot_assets();
	if (!D1_robot_bitmap_slots_registered) {
		for (i = 0; i < D1_MAX_OBJ_BITMAPS; i++)
			D1_robot_bitmap_slots[i].index = D1_ROBOT_BITMAP_SLOT_BASE + i;
		D1_robot_bitmap_slots_registered = 1;
	}
	save_d2_guidebot_assets();
	save_d2_d1_robot_tuning();

	Last_stats.robot_pig_present = 1;
	Last_stats.robot_pig_size = pigsize;
	Last_stats.robot_obj_bitmaps = 0;
	Last_stats.robot_obj_bitmaps_applied = 0;
	Last_stats.robot_obj_bitmaps_skipped = 0;
	Last_stats.robot_objects_updated = 0;

	seek_d1_vclip_table(fp);
	skip_d1_vclips_and_effects(fp);

	num_wall_anims = PHYSFSX_readInt(fp);
	if (num_wall_anims < 0 || num_wall_anims > D1_MAX_WALL_ANIMS) {
		PHYSFS_close(fp);
		return;
	}
	PHYSFSX_fseek(fp, D1_MAX_WALL_ANIMS * D1_WCLIP_SIZE, SEEK_CUR);

	num_robot_types = PHYSFSX_readInt(fp);
	if (num_robot_types < 0 || num_robot_types > D1_MAX_ROBOT_TYPES) {
		PHYSFS_close(fp);
		return;
	}
	for (i = 0; i < D1_MAX_ROBOT_TYPES; i++) {
		read_d1_robot_info(&Robot_info[i], fp);
		apply_d1_robot_d2_tuning(&Robot_info[i], i);
	}
	N_robot_types = num_robot_types;
	Last_stats.robot_types = num_robot_types;

	num_robot_joints = PHYSFSX_readInt(fp);
	if (num_robot_joints < 0 || num_robot_joints > D1_MAX_ROBOT_JOINTS) {
		PHYSFS_close(fp);
		return;
	}
	jointpos_read_n(Robot_joints, D1_MAX_ROBOT_JOINTS, fp);
	N_robot_joints = num_robot_joints;
	Last_stats.robot_joints = num_robot_joints;

	num_weapon_types = PHYSFSX_readInt(fp);
	if (num_weapon_types < 0 || num_weapon_types > D1_MAX_WEAPON_TYPES) {
		PHYSFS_close(fp);
		return;
	}
	for (i = 0; i < D1_MAX_WEAPON_TYPES; i++)
		read_d1_weapon_info(&Weapon_info[i], i, fp);
	Last_stats.weapon_records_active = 1;
	Last_stats.weapon_types = num_weapon_types;

	num_powerups = PHYSFSX_readInt(fp);
	if (num_powerups < 0 || num_powerups > D1_MAX_POWERUP_TYPES) {
		PHYSFS_close(fp);
		return;
	}
	PHYSFSX_fseek(fp, D1_MAX_POWERUP_TYPES * sizeof(powerup_type_info), SEEK_CUR);

	num_polygon_models = PHYSFSX_readInt(fp);
	if (num_polygon_models < 0 || num_polygon_models > D1_MAX_POLYGON_MODELS) {
		PHYSFS_close(fp);
		return;
	}
	free_model_count = D1_robot_assets_active ? D1_robot_polygon_models_loaded : 0;
	if (free_model_count < num_polygon_models)
		free_model_count = num_polygon_models;
	for (i = 0; i < free_model_count; i++)
		free_model(&Polygon_models[i]);
	if (N_polygon_models < num_polygon_models)
		N_polygon_models = num_polygon_models;
	polymodel_read_n(Polygon_models, num_polygon_models, fp);
	for (i = 0; i < num_polygon_models; i++)
		polygon_model_data_read(&Polygon_models[i], fp);
	convert_d1_robot_model_flat_colors(num_polygon_models);
	D1_robot_polygon_models_loaded = num_polygon_models;
	Last_stats.robot_models = num_polygon_models;

	d1_gauge_count = (pigsize == D1_MAC_PIGSIZE || pigsize == D1_MAC_SHARE_PIGSIZE)
		? D1_MAX_GAUGE_BMS_MAC : D1_MAX_GAUGE_BMS_PC;
	PHYSFSX_fseek(fp, d1_gauge_count * sizeof(bitmap_index), SEEK_CUR);

	for (i = 0; i < MAX_POLYGON_MODELS; i++)
		Dying_modelnums[i] = i < D1_MAX_POLYGON_MODELS ? PHYSFSX_readInt(fp) : -1;
	for (i = 0; i < MAX_POLYGON_MODELS; i++)
		Dead_modelnums[i] = i < D1_MAX_POLYGON_MODELS ? PHYSFSX_readInt(fp) : -1;

	bitmap_index_read_n(d1_obj_bitmaps, D1_MAX_OBJ_BITMAPS, fp);
	for (i = 0; i < D1_MAX_OBJ_BITMAPS; i++)
		d1_obj_bitmap_ptrs[i] = PHYSFSX_readShort(fp);
	player_ship_read(&d1_player_ship, fp);
	PHYSFS_close(fp);
	apply_d1_player_ship(&d1_player_ship);
	Last_stats.player_ship_active = D1_player_ship_active;

	if (N_ObjBitmaps < D1_MAX_OBJ_BITMAPS)
		N_ObjBitmaps = D1_MAX_OBJ_BITMAPS;
	for (i = 0; i < D1_MAX_OBJ_BITMAPS; i++) {
		Last_stats.robot_obj_bitmaps++;
		if (d1_obj_bitmaps[i].index && load_d1_bitmap_frame(d1_obj_bitmaps[i].index, D1_robot_bitmap_slots[i])) {
			ObjBitmaps[i] = D1_robot_bitmap_slots[i];
			Last_stats.robot_obj_bitmaps_applied++;
		} else {
			ObjBitmaps[i].index = 0;
			Last_stats.robot_obj_bitmaps_skipped++;
		}
		ObjBitmapPtrs[i] = d1_obj_bitmap_ptrs[i];
	}

	for (i = 0; i <= Highest_object_index; i++) {
		if (Objects[i].type == OBJ_PLAYER && Player_ship) {
			Objects[i].rtype.pobj_info.model_num = Player_ship->model_num;
			Objects[i].size = Polygon_models[Player_ship->model_num].rad;
			if (Objects[i].movement_type == MT_PHYSICS) {
				Objects[i].mtype.phys_info.mass = Player_ship->mass;
				Objects[i].mtype.phys_info.drag = Player_ship->drag;
			}
			continue;
		}
		if (Objects[i].type != OBJ_ROBOT || Objects[i].id >= N_robot_types)
			continue;
		Objects[i].rtype.pobj_info.model_num = Robot_info[Objects[i].id].model_num;
		Objects[i].size = Polygon_models[Objects[i].rtype.pobj_info.model_num].rad;
		Last_stats.robot_objects_updated++;
		if (Objects[i].movement_type == MT_PHYSICS) {
			Objects[i].mtype.phys_info.mass = Robot_info[Objects[i].id].mass;
			Objects[i].mtype.phys_info.drag = Robot_info[Objects[i].id].drag;
		}
	}
	restore_guidebot_bitmap_copies();
	D1_robot_assets_active = 1;
	Last_stats.robot_assets_active = D1_robot_assets_active;
}
