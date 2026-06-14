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
#include "powerup.h"
#include "robot.h"
#include "u_mem.h"
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
static int D1_robot_assets_active = 0;
static int D1_robot_bitmap_slots_registered = 0;
static int D1_robot_polygon_models_loaded = 0;
static bitmap_index D1_robot_bitmap_slots[D1_MAX_OBJ_BITMAPS];
static int D2_guidebot_assets_saved = 0;
static robot_info D2_guidebot_robot_info;
static jointpos D2_guidebot_joints[MAX_ROBOT_JOINTS];
static int D2_guidebot_joint_count = 0;
static polymodel D2_guidebot_model;
static bitmap_index D2_guidebot_obj_bitmaps[MAX_POLYOBJ_TEXTURES];
static int D2_d1_robot_tuning_saved = 0;
static int D2_d1_robot_tuning_count = 0;
static ubyte D2_d1_robot_aim[D1_MAX_ROBOT_TYPES];
static ubyte D2_d1_robot_behavior[D1_MAX_ROBOT_TYPES];
static sbyte D2_d1_robot_lightcast[D1_MAX_ROBOT_TYPES];
static int D1_spawnable_guidebot_model_index = -1;
static int D1_spawnable_guidebot_obj_bitmap_base = -1;
static int D1_spawnable_guidebot_obj_bitmap_count = 0;
static d1_in_d2_asset_stats Last_stats;

extern int read_hamfile();

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

static void save_d2_d1_robot_tuning(void)
{
	int i, count;

	if (D2_d1_robot_tuning_saved)
		return;

	count = N_robot_types < D1_MAX_ROBOT_TYPES ? N_robot_types : D1_MAX_ROBOT_TYPES;
	for (i = 0; i < count; i++) {
		D2_d1_robot_aim[i] = Robot_info[i].aim;
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

	ri->aim = D2_d1_robot_aim[robot_id];
	ri->behavior = D2_d1_robot_behavior[robot_id];
	ri->lightcast = D2_d1_robot_lightcast[robot_id];
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

static int read_d1_palette(ubyte palette[256 * 3])
{
	PHYSFS_file *fp = PHYSFSX_openReadBuffered(D1_DEFAULT_PALETTE);

	if (!fp)
		return 0;
	if (PHYSFS_read(fp, palette, 256, 3) != 3) {
		PHYSFS_close(fp);
		return 0;
	}
	PHYSFS_close(fp);
	return 1;
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
}

static int save_d2_guidebot_assets(void)
{
	int buddy_id, gun, state, i;
	int model_num;
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
	}

	if (!copy_model_data(&D2_guidebot_model, model))
		return 0;
	D2_guidebot_assets_saved = 1;
	return 1;
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

	for (i = 0; i < model->n_textures; i++) {
		ObjBitmaps[N_ObjBitmaps] = D2_guidebot_obj_bitmaps[i];
		ObjBitmapPtrs[N_ObjBitmaps] = N_ObjBitmaps;
		N_ObjBitmaps++;
	}

	return 1;
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

	remove_spawnable_guidebot_assets();
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
	save_d2_guidebot_assets();
	save_d2_d1_robot_tuning();

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
