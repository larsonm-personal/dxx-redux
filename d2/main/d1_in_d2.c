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
#include "gamesave.h"
#include "powerup.h"
#include "robot.h"
#include "vclip.h"
#include "d1_in_d2.h"

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
#define D1_TMAP_INFO_SIZE 26
#define D1_VCLIP_SIZE 82
#define D1_WCLIP_SIZE 66
#define D1_WEAPON_INFO_SIZE 115
#define D1_ROBOT_BITMAP_SLOT_BASE (MAX_BITMAP_FILES - D1_MAX_OBJ_BITMAPS)

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
static int D1_robot_assets_active = 0;
static int D1_robot_bitmap_slots_registered = 0;
static int D1_robot_polygon_models_loaded = 0;
static bitmap_index D1_robot_bitmap_slots[D1_MAX_OBJ_BITMAPS];
static d1_in_d2_asset_stats Last_stats;

extern int read_hamfile();

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

void d1_in_d2_apply_robot_assets(int active)
{
	PHYSFS_file *fp;
	bitmap_index d1_obj_bitmaps[D1_MAX_OBJ_BITMAPS];
	ushort d1_obj_bitmap_ptrs[D1_MAX_OBJ_BITMAPS];
	int i, num_wall_anims, num_robot_types, num_robot_joints, num_weapon_types;
	int num_powerups, num_polygon_models, d1_gauge_count, pigsize;
	int free_model_count;

	if (!active) {
		if (D1_robot_assets_active) {
			free_polygon_models();
			read_hamfile();
			D1_robot_assets_active = 0;
			D1_robot_polygon_models_loaded = 0;
		}
		Last_stats.robot_assets_active = 0;
		Last_stats.robot_pig_present = 0;
		Last_stats.robot_pig_size = 0;
		Last_stats.robot_types = 0;
		Last_stats.robot_joints = 0;
		Last_stats.robot_models = 0;
		Last_stats.robot_obj_bitmaps = 0;
		Last_stats.robot_obj_bitmaps_applied = 0;
		Last_stats.robot_obj_bitmaps_skipped = 0;
		Last_stats.robot_objects_updated = 0;
		return;
	}

	if (!D1_robot_bitmap_slots_registered) {
		for (i = 0; i < D1_MAX_OBJ_BITMAPS; i++)
			D1_robot_bitmap_slots[i].index = D1_ROBOT_BITMAP_SLOT_BASE + i;
		D1_robot_bitmap_slots_registered = 1;
	}

	Last_stats.robot_assets_active = D1_robot_assets_active;
	Last_stats.robot_pig_present = 0;
	Last_stats.robot_pig_size = 0;
	Last_stats.robot_types = 0;
	Last_stats.robot_joints = 0;
	Last_stats.robot_models = 0;
	Last_stats.robot_obj_bitmaps = 0;
	Last_stats.robot_obj_bitmaps_applied = 0;
	Last_stats.robot_obj_bitmaps_skipped = 0;
	Last_stats.robot_objects_updated = 0;
	fp = open_d1_registered_pig();
	if (!fp)
		return;
	pigsize = (int)PHYSFS_fileLength(fp);
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
	for (i = 0; i < D1_MAX_ROBOT_TYPES; i++)
		read_d1_robot_info(&Robot_info[i], fp);
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
	PHYSFSX_fseek(fp, D1_MAX_WEAPON_TYPES * D1_WEAPON_INFO_SIZE, SEEK_CUR);

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
	PHYSFS_close(fp);

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
	D1_robot_assets_active = 1;
	Last_stats.robot_assets_active = D1_robot_assets_active;
}
