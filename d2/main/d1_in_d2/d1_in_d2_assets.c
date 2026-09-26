/*
 *
 * D1 asset preparation and publication for the D2 engine.
 *
 */

#include <string.h>

#include "pstypes.h"
#include "inferno.h"
#include "dxxerror.h"
#include "ai.h"
#include "args.h"
#include "object.h"
#include "gamemine.h"
#include "d1_in_d2_levels.h"
#include "piggy.h"
#include "polyobj.h"
#include "effects.h"
#include "endlevel.h"
#include "byteswap.h"
#include "bm.h"
#include "cntrlcen.h"
#include "gamepal.h"
#include "gamesave.h"
#include "gauges.h"
#include "hash.h"
#include "interp.h"
#include "mission.h"
#include "player.h"
#include "palette.h"
#include "powerup.h"
#include "rle.h"
#include "robot.h"
#include "sounds.h"
#include "strutil.h"
#include "textures.h"
#include "u_mem.h"
#include "vclip.h"
#include "wall.h"
#include "d1_in_d2.h"
#include "d1_custom.h"
#include "d1_in_d2_assets.h"
#include "d1_pig_validation.h"
#include "laser.h"
#include "weapon.h"
#include "android_log.h"
#ifdef OGL
#include "xmodel.h"
#endif

#ifdef ANDROID
#define D1_IN_D2_LOG(...) debug_log(DLOG_GAME, __VA_ARGS__)
#else
#define D1_IN_D2_LOG(...) ((void)0)
#endif

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

#define D1_MODEL_OP_EOF 0
#define D1_MODEL_OP_DEFPOINTS 1
#define D1_MODEL_OP_FLATPOLY 2
#define D1_MODEL_OP_TMAPPOLY 3
#define D1_MODEL_OP_SORTNORM 4
#define D1_MODEL_OP_RODBM 5
#define D1_MODEL_OP_SUBCALL 6
#define D1_MODEL_OP_DEFP_START 7
#define D1_MODEL_OP_GLOW 8

static d1_in_d2_asset_stats Last_stats;

extern hashtable AllDigiSndNames;
extern ubyte *SoundBits;
extern int Num_sound_files;
extern int SoundOffset[MAX_SOUND_FILES];

static void read_d1_robot_info(robot_info *ri, PHYSFS_file *fp);
static void read_d1_weapon_info(weapon_info *wi, int weapon_id, PHYSFS_file *fp);

static d1_asset_generation *Active_d1_assets;
static const char *D1_asset_validation_error = "not validated";

fix d1_in_d2_robot_drop_radius(void)
{
	Assert(Active_d1_assets != NULL);
	return Polygon_models[Robot_info[Active_d1_assets->object_ids[OBJ_ROBOT]].model_num].rad;
}

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

const char *d1_in_d2_source_edition_error(int64_t pig_size)
{
	switch (pig_size) {
		case D1_SHARE_BIG_PIGSIZE:
		case D1_SHARE_10_PIGSIZE:
		case D1_SHARE_PIGSIZE:
		case D1_10_BIG_PIGSIZE:
		case D1_10_PIGSIZE:
		case D1_MAC_PIGSIZE:
		case D1_MAC_SHARE_PIGSIZE:
			return "This Descent 1 edition is not supported in the Descent 2 engine. Launch it with Descent 1";
		default:
			return NULL;
	}
}

static PHYSFS_file *open_d1_registered_pig(const char *filename, const char **error)
{
	PHYSFS_file *fp;
	const char *edition_error;

	fp = PHYSFSX_openReadBuffered(filename);
	if (!fp)
		return NULL;

	edition_error = d1_in_d2_source_edition_error(PHYSFS_fileLength(fp));
	if (edition_error) {
		*error = edition_error;
		PHYSFS_close(fp);
		return NULL;
	}
	PHYSFSX_readInt(fp);
	return fp;
}

static int d1_pig_has_bytes(PHYSFS_file *fp, PHYSFS_sint64 pigsize, PHYSFS_sint64 size)
{
	PHYSFS_sint64 position = PHYSFS_tell(fp);

	return position >= 0 && size >= 0 && position <= pigsize && size <= pigsize - position;
}

void d1_in_d2_free_assets(d1_asset_generation *generation)
{
	int i;

	if (!generation)
		return;
	d1_in_d2_free_guidebot_source(generation->guidebot);
	if (generation->models) {
		for (i = 0; i < generation->num_polygon_models; i++)
			if (generation->models[i].model_data)
				d_free(generation->models[i].model_data);
		d_free(generation->models);
	}
	d1_in_d2_free_bitmaps(generation->bitmap_data);
	if (generation->sound_bank.data)
		d_free(generation->sound_bank.data);
	d_free(generation);
}

static int d1_vclip_reference_valid(const vclip *vclips, int count, int index, int optional)
{
	if (optional && index == -1)
		return 1;
	return index >= 0 && index < count &&
	       d1_pig_validate_timed_clip(vclips[index].num_frames, VCLIP_MAX_FRAMES,
	                                  vclips[index].play_time, vclips[index].frame_time);
}

static int read_d1_reactor_definition(PHYSFS_file *fp, int pigsize, int num_models, d1_asset_generation *generation)
{
	ubyte *types = generation->object_types, *ids = generation->object_ids;
	reactor *definition = &generation->control_center;
	int count, i;

	if (!d1_pig_has_bytes(fp, pigsize, sizeof(int)))
		return 0;
	count = PHYSFSX_readInt(fp);
	if (count < 0 || count > D1_MAX_OBJECT_TYPES ||
	    !d1_pig_has_bytes(fp, pigsize, 2 * D1_MAX_OBJECT_TYPES) ||
	    PHYSFS_read(fp, types, 1, D1_MAX_OBJECT_TYPES) != D1_MAX_OBJECT_TYPES ||
	    PHYSFS_read(fp, ids, 1, D1_MAX_OBJECT_TYPES) != D1_MAX_OBJECT_TYPES ||
	    !d1_pig_has_bytes(fp, pigsize, D1_MAX_OBJECT_TYPES * sizeof(fix) + 2 * sizeof(int) +
	                      2 * D1_MAX_CONTROLCEN_GUNS * sizeof(vms_vector)))
		return 0;
	generation->num_object_types = count;
	for (i = 0; i < D1_MAX_OBJECT_TYPES; i++)
		generation->object_strength[i] = PHYSFSX_readFix(fp);
	generation->first_multi_bitmap = PHYSFSX_readInt(fp);
	definition->model_num = -1;
	for (i = 0; i < count; i++)
		if (types[i] == D1_CONTROL_CENTER_OBJECT_TYPE) {
			if (!d1_pig_valid_model_index(ids[i], num_models))
				return 0;
			definition->model_num = ids[i];
			break;
		}
	definition->n_guns = PHYSFSX_readInt(fp);
	if (definition->n_guns < 0 || definition->n_guns > D1_MAX_CONTROLCEN_GUNS)
		return 0;
	for (i = 0; i < D1_MAX_CONTROLCEN_GUNS; i++)
		PHYSFSX_readVector(&definition->gun_points[i], fp);
	for (i = 0; i < D1_MAX_CONTROLCEN_GUNS; i++)
		PHYSFSX_readVector(&definition->gun_dirs[i], fp);
	return 1;
}

static int validate_d1_robot_references(const robot_info *robots, int num_robot_types,
                                        const polymodel *models, int num_polygon_models,
                                        const vclip *vclips, int num_vclips,
                                        int num_weapon_types, int num_robot_joints)
{
	int i, gun, state;
	for (i = 0; i < num_robot_types; i++) {
		const robot_info *robot = &robots[i];
		if (!d1_pig_valid_model_index(robot->model_num, num_polygon_models) ||
		    robot->n_guns < 0 || robot->n_guns > MAX_GUNS ||
		    robot->weapon_type < 0 || robot->weapon_type >= num_weapon_types ||
		    !d1_vclip_reference_valid(vclips, num_vclips, robot->exp1_vclip_num, 1) ||
		    !d1_vclip_reference_valid(vclips, num_vclips, robot->exp2_vclip_num, 1) ||
		    robot->exp1_sound_num < -1 || robot->exp1_sound_num >= D1_MAX_PIG_SOUNDS ||
		    robot->exp2_sound_num < -1 || robot->exp2_sound_num >= D1_MAX_PIG_SOUNDS ||
		    robot->see_sound >= D1_MAX_PIG_SOUNDS ||
		    robot->attack_sound >= D1_MAX_PIG_SOUNDS ||
		    robot->claw_sound >= D1_MAX_PIG_SOUNDS)
			return 0;
		for (gun = 0; gun < robot->n_guns; gun++)
			if (robot->gun_submodels[gun] >= models[robot->model_num].n_models)
				return 0;
		for (gun = 0; gun < MAX_GUNS + 1; gun++)
			for (state = 0; state < N_ANIM_STATES; state++) {
				const jointlist *joints = &robot->anim_states[gun][state];
				if (joints->n_joints < 0 || joints->offset < 0 ||
				    joints->offset > num_robot_joints ||
				    joints->n_joints > num_robot_joints - joints->offset)
					return 0;
			}
	}
	return 1;
}

static int validate_d1_robot_assets(PHYSFS_file *fp, int pigsize, int property_end,
	                                d1_asset_generation **staged_generation)
{
	d1_asset_generation *generation = d_malloc(sizeof(*generation));
	robot_info *robots;
	weapon_info *weapons;
	powerup_type_info *powerups;
	vclip *vclips;
	eclip *effects;
	polymodel *models;
	ubyte *model_data = NULL;
	ubyte sound_maps[2][D1_MAX_PIG_SOUNDS];
	int num_textures, num_vclips, num_effects, num_wall_anims, num_robot_types, num_robot_joints;
	int num_weapon_types, num_powerups, num_polygon_models, d1_gauge_count;
	int i, valid = 0;
	const char *stage = "texture tables";

	if (!generation)
		return 0;
	memset(generation, 0, sizeof(*generation));
	robots = generation->robots;
	weapons = generation->weapons;
	powerups = generation->powerups;
	vclips = generation->vclips;
	effects = generation->effects;
	models = NULL;

	if (!d1_pig_has_bytes(fp, property_end, sizeof(int)))
		goto done;
	num_textures = PHYSFSX_readInt(fp);
	if (num_textures < 0 || num_textures > D1_MAX_PIG_TEXTURES ||
	    !d1_pig_has_bytes(fp, property_end, D1_MAX_PIG_TEXTURES * (sizeof(bitmap_index) + D1_TMAP_INFO_SIZE) +
	                     2 * D1_MAX_PIG_SOUNDS + sizeof(int)))
		goto done;
	bitmap_index_read_n(generation->textures, D1_MAX_PIG_TEXTURES, fp);
	for (i = 0; i < D1_MAX_PIG_TEXTURES; i++) {
		tmap_info *texture = &generation->texture_info[i];
		int effect;
		if (PHYSFS_read(fp, generation->texture_names[i], 13, 1) != 1)
			goto done;
		texture->flags = PHYSFSX_readByte(fp) & TMI_VOLATILE;
		texture->lighting = PHYSFSX_readFix(fp);
		texture->damage = PHYSFSX_readFix(fp);
		effect = PHYSFSX_readInt(fp);
		if (i < num_textures && (effect < -1 || effect >= D1_MAX_EFFECTS))
			goto done;
		texture->eclip_num = effect;
		texture->destroyed = -1;
#ifdef EDITOR
		memcpy(texture->filename, generation->texture_names[i], sizeof(texture->filename));
#endif
	}
	if (PHYSFS_read(fp, generation->sound_maps, sizeof(generation->sound_maps), 1) != 1)
		goto done;
	num_vclips = PHYSFSX_readInt(fp);
	stage = "vclips";
	if (num_vclips == 0)
		num_vclips = D1_VCLIP_MAXNUM;
	if (num_vclips < 0 || num_vclips > D1_VCLIP_MAXNUM ||
	    !d1_pig_has_bytes(fp, property_end, D1_VCLIP_MAXNUM * D1_VCLIP_SIZE))
		goto done;
	vclip_read_n(vclips, D1_VCLIP_MAXNUM, fp);
	for (i = 0; i < num_vclips; i++) {
		int frame;

		if ((vclips[i].num_frames > 0 &&
		     !d1_pig_validate_timed_clip(vclips[i].num_frames, VCLIP_MAX_FRAMES,
		                                 vclips[i].play_time, vclips[i].frame_time)) ||
		    vclips[i].num_frames < -1 || vclips[i].num_frames > VCLIP_MAX_FRAMES ||
		    vclips[i].sound_num < -1 || vclips[i].sound_num >= D1_MAX_PIG_SOUNDS)
			goto done;
		for (frame = 0; frame < vclips[i].num_frames; frame++)
			if (vclips[i].frames[frame].index <= 0)
				goto done;
	}
	if (!d1_pig_has_bytes(fp, property_end, sizeof(int)))
		goto done;
	num_effects = PHYSFSX_readInt(fp);
	stage = "effects";
	if (num_effects < 0 || num_effects > D1_MAX_EFFECTS ||
	    !d1_pig_has_bytes(fp, property_end, D1_MAX_EFFECTS * (PHYSFS_sint64)sizeof(eclip)))
		goto done;
	eclip_read_n(effects, D1_MAX_EFFECTS, fp);
	for (i = 0; i < num_effects; i++) {
		vclip *vc = &effects[i].vc;
		if (((effects[i].changing_wall_texture >= 0 || effects[i].changing_object_texture >= 0) &&
		     !d1_pig_validate_timed_clip(vc->num_frames, VCLIP_MAX_FRAMES,
		                                 vc->play_time, vc->frame_time)) ||
		    effects[i].changing_wall_texture < -1 ||
		    effects[i].changing_wall_texture >= num_textures ||
		    effects[i].changing_object_texture < -1 ||
		    effects[i].changing_object_texture >= D1_MAX_OBJ_BITMAPS ||
		    effects[i].crit_clip < -1 || effects[i].crit_clip >= num_effects ||
		    effects[i].dest_bm_num < -1 || effects[i].dest_bm_num >= num_textures ||
		    !d1_vclip_reference_valid(vclips, num_vclips, effects[i].dest_vclip, 1) ||
		    effects[i].dest_eclip < -1 || effects[i].dest_eclip >= num_effects ||
		    effects[i].sound_num < -1 || effects[i].sound_num >= D1_MAX_PIG_SOUNDS)
			goto done;
	}
	if (!d1_pig_has_bytes(fp, property_end, sizeof(int)))
		goto done;

	num_wall_anims = PHYSFSX_readInt(fp);
	stage = "wall animations";
	if (num_wall_anims < 0 || num_wall_anims > D1_MAX_WALL_ANIMS ||
	    !d1_pig_has_bytes(fp, property_end, D1_MAX_WALL_ANIMS * D1_WCLIP_SIZE + sizeof(int)))
		goto done;
	{
		REWIND_PHYSFS_FILE(rewind_fp, fp);
		wclip_read_n_d1(generation->wall_anims, D1_MAX_WALL_ANIMS, rewind_fp);
	}

	num_robot_types = PHYSFSX_readInt(fp);
	stage = "robot records";
	if (num_robot_types < 0 || num_robot_types > D1_MAX_ROBOT_TYPES ||
	    !d1_pig_has_bytes(fp, property_end, D1_MAX_ROBOT_TYPES * D1_ROBOT_INFO_SIZE))
		goto done;
	for (i = 0; i < D1_MAX_ROBOT_TYPES; i++)
		read_d1_robot_info(&robots[i], fp);
	if (!d1_pig_has_bytes(fp, property_end, sizeof(int)))
		goto done;

	num_robot_joints = PHYSFSX_readInt(fp);
	stage = "robot joints";
	if (num_robot_joints < 0 || num_robot_joints > D1_MAX_ROBOT_JOINTS ||
	    !d1_pig_has_bytes(fp, property_end, D1_MAX_ROBOT_JOINTS * D1_JOINTPOS_SIZE))
		goto done;
	jointpos_read_n(generation->joints, D1_MAX_ROBOT_JOINTS, fp);
	if (!d1_pig_has_bytes(fp, property_end, sizeof(int)))
		goto done;

	num_weapon_types = PHYSFSX_readInt(fp);
	stage = "weapon records";
	if (num_weapon_types < 0 || num_weapon_types > D1_MAX_WEAPON_TYPES ||
	    !d1_pig_has_bytes(fp, property_end, D1_MAX_WEAPON_TYPES * D1_WEAPON_INFO_SIZE))
		goto done;
	for (i = 0; i < D1_MAX_WEAPON_TYPES; i++)
		read_d1_weapon_info(&weapons[i], i, fp);
	if (!d1_pig_has_bytes(fp, property_end, sizeof(int)))
		goto done;

	num_powerups = PHYSFSX_readInt(fp);
	stage = "powerup records";
	if (num_powerups < 0 || num_powerups > D1_MAX_POWERUP_TYPES ||
	    !d1_pig_has_bytes(fp, property_end, D1_MAX_POWERUP_TYPES * (PHYSFS_sint64)sizeof(powerup_type_info)))
		goto done;
	powerup_type_info_read_n(powerups, D1_MAX_POWERUP_TYPES, fp);
	if (!d1_pig_has_bytes(fp, property_end, sizeof(int)))
		goto done;

	num_polygon_models = PHYSFSX_readInt(fp);
	stage = "polygon models";
	if (num_polygon_models <= 0 || num_polygon_models > D1_MAX_POLYGON_MODELS ||
	    !d1_pig_has_bytes(fp, property_end, num_polygon_models * D1_POLYMODEL_SIZE))
		goto done;
	MALLOC(models, polymodel, num_polygon_models);
	if (!models)
		goto done;
	memset(models, 0, num_polygon_models * sizeof(*models));
	generation->models = models;
	generation->num_polygon_models = num_polygon_models;
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
		if (!d1_pig_has_bytes(fp, property_end, models[i].model_data_size))
			goto done;
		model_data = d_malloc(models[i].model_data_size);
		if (!model_data || PHYSFS_read(fp, model_data, 1, models[i].model_data_size) != models[i].model_data_size ||
		    !d1_pig_validate_model_stream(model_data, models[i].model_data_size, 0))
			goto done;
		for (submodel = 0; submodel < models[i].n_models; submodel++)
			if (!d1_pig_validate_model_stream(model_data, models[i].model_data_size,
			                                     models[i].submodel_ptrs[submodel]))
				goto done;
		models[i].model_data = model_data;
		model_data = NULL;
	}

	d1_gauge_count = (pigsize == D1_MAC_PIGSIZE || pigsize == D1_MAC_SHARE_PIGSIZE)
		? D1_MAX_GAUGE_BMS_MAC : D1_MAX_GAUGE_BMS_PC;
	stage = "trailing asset tables";
	if (!d1_pig_has_bytes(fp, property_end, d1_gauge_count * (PHYSFS_sint64)sizeof(bitmap_index) +
	                      2 * D1_MAX_POLYGON_MODELS * (PHYSFS_sint64)sizeof(int)))
		goto done;
	bitmap_index_read_n(generation->gauges, d1_gauge_count, fp);
	generation->num_gauges = d1_gauge_count;
	for (i = 0; i < D1_MAX_POLYGON_MODELS; i++)
		generation->dying_models[i] = PHYSFSX_readInt(fp);
	for (i = 0; i < D1_MAX_POLYGON_MODELS; i++)
		generation->dead_models[i] = PHYSFSX_readInt(fp);
	if (!d1_pig_has_bytes(fp, property_end, D1_MAX_OBJ_BITMAPS * (PHYSFS_sint64)sizeof(bitmap_index)))
		goto done;
	bitmap_index_read_n(generation->obj_bitmaps, D1_MAX_OBJ_BITMAPS, fp);
	if (!d1_pig_has_bytes(fp, property_end, D1_MAX_OBJ_BITMAPS * (PHYSFS_sint64)sizeof(short)))
		goto done;
	for (i = 0; i < D1_MAX_OBJ_BITMAPS; i++)
		generation->obj_bitmap_ptrs[i] = PHYSFSX_readShort(fp);
	if (!d1_pig_has_bytes(fp, property_end, D1_PLAYER_SHIP_SIZE))
		goto done;
	player_ship_read(&generation->ship, fp);
	if (!d1_pig_has_bytes(fp, property_end, sizeof(int)))
		goto done;
	generation->num_cockpits = PHYSFSX_readInt(fp);
	if (generation->num_cockpits < 0 || generation->num_cockpits > D1_N_COCKPIT_BITMAPS ||
	    !d1_pig_has_bytes(fp, property_end, D1_N_COCKPIT_BITMAPS * (PHYSFS_sint64)sizeof(bitmap_index) + sizeof(sound_maps)))
		goto done;
	bitmap_index_read_n(generation->cockpits, D1_N_COCKPIT_BITMAPS, fp);
	if (PHYSFS_read(fp, sound_maps, sizeof(sound_maps), 1) != 1)
		goto done;
	memcpy(generation->sound_maps, sound_maps, sizeof(sound_maps));
	stage = "reactor definition";
	if (!read_d1_reactor_definition(fp, property_end, num_polygon_models, generation))
		goto done;

	stage = "cross references";
	if (!d1_pig_valid_model_index(generation->ship.model_num, num_polygon_models) ||
	    !d1_vclip_reference_valid(vclips, num_vclips, generation->ship.expl_vclip_num, 0))
		goto done;
	for (i = 0; i < num_polygon_models; i++)
		if (!d1_pig_valid_optional_model_index(generation->dying_models[i], num_polygon_models) ||
		    !d1_pig_valid_optional_model_index(generation->dead_models[i], num_polygon_models))
			goto done;
	for (i = 0; i < D1_MAX_OBJ_BITMAPS; i++)
		if (generation->obj_bitmap_ptrs[i] >= D1_MAX_OBJ_BITMAPS)
			goto done;
	if (!validate_d1_robot_references(robots, num_robot_types, models, num_polygon_models,
	                                  vclips, num_vclips, num_weapon_types, num_robot_joints))
		goto done;
	for (i = 0; i < num_weapon_types; i++) {
		weapon_info *weapon = &weapons[i];
		if (weapon->render_type < WEAPON_RENDER_NONE || weapon->render_type > WEAPON_RENDER_VCLIP ||
		    (weapon->render_type == WEAPON_RENDER_POLYMODEL &&
		     (!d1_pig_valid_model_index(weapon->model_num, num_polygon_models) ||
		      !d1_pig_valid_optional_model_index(weapon->model_num_inner, num_polygon_models))) ||
		    !d1_vclip_reference_valid(vclips, num_vclips, weapon->flash_vclip, 1) ||
		    !d1_vclip_reference_valid(vclips, num_vclips, weapon->robot_hit_vclip, 1) ||
		    !d1_vclip_reference_valid(vclips, num_vclips, weapon->wall_hit_vclip, 1) ||
		    !d1_vclip_reference_valid(vclips, num_vclips, weapon->weapon_vclip, 1) ||
		    weapon->flash_sound < -1 || weapon->flash_sound >= D1_MAX_PIG_SOUNDS ||
		    weapon->robot_hit_sound < -1 || weapon->robot_hit_sound >= D1_MAX_PIG_SOUNDS ||
		    weapon->wall_hit_sound < -1 || weapon->wall_hit_sound >= D1_MAX_PIG_SOUNDS)
			goto done;
	}
	for (i = 0; i < num_powerups; i++)
		if (!d1_vclip_reference_valid(vclips, num_vclips, powerups[i].vclip_num, 0) ||
		    powerups[i].hit_sound < -1 || powerups[i].hit_sound >= D1_MAX_PIG_SOUNDS)
			goto done;
	if (!d1_pig_validate_sound_map(sound_maps[0], D1_MAX_PIG_SOUNDS, MAX_SOUND_FILES) ||
	    !d1_pig_validate_sound_map(sound_maps[1], D1_MAX_PIG_SOUNDS, D1_MAX_PIG_SOUNDS))
		goto done;
	generation->num_textures = num_textures;
	generation->num_wall_anims = num_wall_anims;
	generation->num_powerups = num_powerups;
	generation->num_robot_types = num_robot_types;
	generation->num_robot_joints = num_robot_joints;
	generation->num_weapon_types = num_weapon_types;
	generation->num_vclips = num_vclips;
	generation->num_effects = num_effects;
	generation->pigsize = pigsize;
	valid = 1;

done:
	if (!valid) {
		D1_asset_validation_error = stage;
		D1_IN_D2_LOG("D1-in-D2 asset validation failed in %s at offset %lld", stage,
		             (long long)PHYSFS_tell(fp));
	}
	if (model_data)
		d_free(model_data);
	if (valid && staged_generation) {
		*staged_generation = generation;
	} else {
		d1_in_d2_free_assets(generation);
	}
	return valid;
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

static int read_d1_sound_bank(PHYSFS_file *fp, int pigsize,
                              const ubyte maps[2][D1_MAX_PIG_SOUNDS],
                              d1_sound_generation *bank, const char **error)
{
	digi_sound *staged_sounds = bank->samples;
	char (*staged_names)[9] = bank->names;
	ubyte *staged_bits = NULL;
	PHYSFS_sint64 header_size, sound_header_start, sound_data_start, sound_bytes;
	PHYSFS_sint64 file_len;
	int pig_data_start, num_bitmaps, num_sounds, i;

	file_len = PHYSFS_fileLength(fp);
	*error = "sound table header";
	if (file_len < 0 || PHYSFSX_fseek(fp, 0, SEEK_SET)) {
		goto failed;
	}
	if (!d1_pig_data_start(fp, pigsize, &pig_data_start)) {
		goto failed;
	}
	if (pig_data_start < 0 || pig_data_start > file_len - 2 * (PHYSFS_sint64)sizeof(int)) {
		goto failed;
	}
	if (PHYSFSX_fseek(fp, pig_data_start, SEEK_SET)) {
		goto failed;
	}
	num_bitmaps = PHYSFSX_readInt(fp);
	num_sounds = PHYSFSX_readInt(fp);
	if (num_bitmaps < 0 || num_bitmaps > D1_MAX_BITMAP_FILES ||
	    num_sounds < 0 || num_sounds > MAX_SOUND_FILES) {
		goto failed;
	}
	header_size = (PHYSFS_sint64)num_bitmaps * D1_DISKBITMAPHEADER_SIZE +
	              (PHYSFS_sint64)num_sounds * D1_DISKSOUNDHEADER_SIZE;
	sound_header_start = (PHYSFS_sint64)pig_data_start + 2 * sizeof(int) +
	                     (PHYSFS_sint64)num_bitmaps * D1_DISKBITMAPHEADER_SIZE;
	sound_data_start = (PHYSFS_sint64)pig_data_start + 2 * sizeof(int) + header_size;
	if (!d1_pig_validate_span(file_len, sound_header_start,
	                          (PHYSFS_sint64)num_sounds * D1_DISKSOUNDHEADER_SIZE) ||
	    sound_data_start > file_len) {
		goto failed;
	}
	sound_bytes = 0;
	*error = "sample spans";
	for (i = 0; i < num_sounds; i++) {
		int length, data_length, offset;
		PHYSFS_sint64 header_offset = sound_header_start + (PHYSFS_sint64)i * D1_DISKSOUNDHEADER_SIZE;

		if (PHYSFSX_fseek(fp, (long)(header_offset + 8), SEEK_SET)) {
			goto failed;
		}
		length = PHYSFSX_readInt(fp);
		data_length = PHYSFSX_readInt(fp);
		offset = PHYSFSX_readInt(fp);
		if (length < 0 || data_length < 0 || offset < 0 ||
		    !d1_pig_validate_span(file_len, sound_data_start + offset, length) ||
		    sound_bytes > 0x7fffffff - length) {
			goto failed;
		}
		sound_bytes += length;
	}
	*error = "sound map references";
	if (!d1_pig_validate_sound_map(maps[0], D1_MAX_PIG_SOUNDS, num_sounds) ||
	    !d1_pig_validate_sound_map(maps[1], D1_MAX_PIG_SOUNDS, D1_MAX_PIG_SOUNDS)) {
		goto failed;
	}
	*error = "sample allocation";
	staged_bits = d_malloc((size_t)sound_bytes + 16);
	if (!staged_bits) {
		goto failed;
	}
	sound_bytes = 0;
	*error = "sample payloads";
	for (i = 0; i < num_sounds; i++) {
		int length, data_length, offset;
		PHYSFS_sint64 header_offset = sound_header_start + (PHYSFS_sint64)i * D1_DISKSOUNDHEADER_SIZE;

		if (PHYSFSX_fseek(fp, (long)header_offset, SEEK_SET) ||
		    PHYSFS_read(fp, staged_names[i], 8, 1) != 1) {
			goto failed;
		}
		staged_names[i][8] = 0;
		length = PHYSFSX_readInt(fp);
		data_length = PHYSFSX_readInt(fp);
		(void)data_length;
		offset = PHYSFSX_readInt(fp);
		if (PHYSFSX_fseek(fp, (long)(sound_data_start + offset), SEEK_SET) ||
		    PHYSFS_read(fp, staged_bits + sound_bytes, 1, length) != length) {
			goto failed;
		}
		staged_sounds[i].bits = 8;
		staged_sounds[i].freq = 11025;
		staged_sounds[i].length = length;
		staged_sounds[i].data = staged_bits + sound_bytes;
		sound_bytes += length;
	}

	bank->data = staged_bits;
	bank->count = num_sounds;
	bank->bytes = (size_t)sound_bytes;
	return 1;

failed:
	if (staged_bits)
		d_free(staged_bits);
	memset(bank, 0, sizeof(*bank));
	return 0;
}

/* Source bitmap zero is the engine fallback, including unused HUD/model slots */
static int d1_bitmap_reference_valid(const d1_asset_generation *generation, int index)
{
	return index >= 0 && index <= generation->bitmap_data->bitmap_count;
}

int d1_in_d2_validate_asset_references(const d1_asset_generation *generation, const char **error)
{
	int i, frame;

	*error = "texture bitmap references";
	for (i = 0; i < generation->num_textures; i++)
		if (!d1_bitmap_reference_valid(generation, generation->textures[i].index) ||
		    generation->texture_info[i].eclip_num >= generation->num_effects)
			return 0;
	*error = "vclip bitmap references";
	for (i = 0; i < generation->num_vclips; i++)
		for (frame = 0; frame < generation->vclips[i].num_frames; frame++)
			if (!d1_bitmap_reference_valid(generation, generation->vclips[i].frames[frame].index))
				return 0;
	*error = "effect bitmap references";
	for (i = 0; i < generation->num_effects; i++) {
		const vclip *clip = &generation->effects[i].vc;
		if (clip->num_frames < -1 || clip->num_frames > VCLIP_MAX_FRAMES ||
		    (clip->num_frames > 0 &&
		     !d1_pig_validate_timed_clip(clip->num_frames, VCLIP_MAX_FRAMES, clip->play_time, clip->frame_time)))
			return 0;
		for (frame = 0; frame < clip->num_frames; frame++)
			if (clip->frames[frame].index <= 0 ||
			    !d1_bitmap_reference_valid(generation, clip->frames[frame].index))
				return 0;
	}
	*error = "wall animation references";
	for (i = 0; i < generation->num_wall_anims; i++) {
		const wclip *clip = &generation->wall_anims[i];
		/* Registered D1 retains unused slots inside its wall animation table */
		if (clip->num_frames == -1)
			continue;
		if (clip->num_frames <= 0 || clip->num_frames > 20 || clip->play_time <= 0 ||
		    clip->open_sound < -1 || clip->open_sound >= D1_MAX_PIG_SOUNDS ||
		    clip->close_sound < -1 || clip->close_sound >= D1_MAX_PIG_SOUNDS)
			return 0;
		for (frame = 0; frame < clip->num_frames; frame++)
			if (clip->frames[frame] < 0 || clip->frames[frame] >= generation->num_textures)
				return 0;
	}
	*error = "presentation bitmap references";
	for (i = 0; i < generation->num_gauges; i++)
		if (!d1_bitmap_reference_valid(generation, generation->gauges[i].index))
			return 0;
	for (i = 0; i < generation->num_cockpits; i++)
		if (!d1_bitmap_reference_valid(generation, generation->cockpits[i].index))
			return 0;
	*error = "object bitmap references";
	for (i = 0; i < D1_MAX_OBJ_BITMAPS; i++)
		if (!d1_bitmap_reference_valid(generation, generation->obj_bitmaps[i].index))
			return 0;
	*error = "model definitions";
	for (i = 0; i < generation->num_polygon_models; i++) {
		const polymodel *model = &generation->models[i];
		int submodel;
		if (model->n_models <= 0 || model->n_models > MAX_SUBMODELS ||
		    !model->model_data || model->model_data_size <= 0 ||
		    model->first_texture + model->n_textures > D1_MAX_OBJ_BITMAPS ||
		    model->simpler_model > generation->num_polygon_models ||
		    !d1_pig_valid_optional_model_index(generation->dying_models[i], generation->num_polygon_models) ||
		    !d1_pig_valid_optional_model_index(generation->dead_models[i], generation->num_polygon_models) ||
		    !d1_pig_validate_model_stream(model->model_data, model->model_data_size, 0))
			return 0;
		for (submodel = 0; submodel < model->n_models; submodel++) {
			if ((submodel && model->submodel_parents[submodel] >= model->n_models) ||
			    model->submodel_ptrs[submodel] < 0 ||
			    !d1_pig_validate_model_stream(model->model_data, model->model_data_size, model->submodel_ptrs[submodel]) ||
			    !d1_pig_validate_model_textures(model->model_data, model->model_data_size,
			                                  model->submodel_ptrs[submodel], model->n_textures))
				return 0;
		}
	}
	*error = "robot references";
	if (generation->object_ids[OBJ_ROBOT] >= generation->num_robot_types)
		return 0;
	if (!validate_d1_robot_references(generation->robots, generation->num_robot_types,
	                                  generation->models, generation->num_polygon_models,
	                                  generation->vclips, generation->num_vclips,
	                                  generation->num_weapon_types, generation->num_robot_joints))
		return 0;
	/* A model/joint replacement also changes animation users which were not
	 * themselves replaced in the HX1 */
	for (i = 0; i < generation->num_robot_types; i++) {
		const robot_info *robot = &generation->robots[i];
		int gun, state, joint;
		for (gun = 0; gun < MAX_GUNS + 1; gun++)
			for (state = 0; state < N_ANIM_STATES; state++) {
				const jointlist *list = &robot->anim_states[gun][state];
				for (joint = 0; joint < list->n_joints; joint++) {
					int number = generation->joints[list->offset + joint].jointnum;
					if (number < 0 || number >= generation->models[robot->model_num].n_models)
						return 0;
				}
			}
	}
	*error = "weapon bitmap references";
	for (i = 0; i < generation->num_weapon_types; i++) {
		const weapon_info *weapon = &generation->weapons[i];
		if (!d1_bitmap_reference_valid(generation, weapon->picture.index) ||
		    (weapon->render_type == WEAPON_RENDER_BLOB &&
		     !d1_bitmap_reference_valid(generation, weapon->bitmap.index)))
			return 0;
	}
	return 1;
}

d1_asset_generation *d1_in_d2_read_assets(const char *pig_name, const char *palette_name, const char **error)
{
	d1_asset_generation *generation = NULL;
	PHYSFS_file *fp = NULL;
	PHYSFS_sint64 file_size;
	int property_end;
	const char *stage = "registered D1 PIG";

	if (!pig_name || !palette_name || !(fp = open_d1_registered_pig(pig_name, &stage)))
		goto failed;
	file_size = PHYSFS_fileLength(fp);
	if (file_size < 8 || file_size > 0x7fffffff || !PHYSFS_seek(fp, 0))
		goto failed;
	property_end = PHYSFSX_readInt(fp);
	if (property_end < 4 || property_end > file_size - 8)
		goto failed;
	if (!validate_d1_robot_assets(fp, (int)file_size, property_end, &generation)) {
		stage = D1_asset_validation_error;
		goto failed;
	}
	stage = "exit model references";
	if (!d1_pig_has_bytes(fp, property_end, 2 * sizeof(int)))
		goto failed;
	generation->exit_model = PHYSFSX_readInt(fp);
	generation->destroyed_exit_model = PHYSFSX_readInt(fp);
	if (!d1_pig_valid_optional_model_index(generation->exit_model, generation->num_polygon_models) ||
	    !d1_pig_valid_optional_model_index(generation->destroyed_exit_model, generation->num_polygon_models) ||
	    generation->first_multi_bitmap < -1 || generation->first_multi_bitmap >= D1_MAX_OBJ_BITMAPS)
		goto failed;
	stage = "bitmap collection or palette";
	generation->bitmap_data = d1_in_d2_read_bitmaps(pig_name, palette_name);
	if (!generation->bitmap_data || !d1_in_d2_validate_asset_references(generation, &stage) ||
	    !read_d1_sound_bank(fp, (int)file_size, generation->sound_maps, &generation->sound_bank, &stage))
		goto failed;
	stage = "base source identity";
	{
		const char *custom[3] = { NULL, NULL, NULL };
		if (!d1_in_d2_hash_base_source(pig_name, palette_name, generation->base_identity) ||
		    !d1_in_d2_hash_custom_sources(generation->base_identity, custom, generation->definition_identity))
			goto failed;
	}
	PHYSFS_close(fp);
	if (error)
		*error = NULL;
	return generation;

failed:
	if (fp)
		PHYSFS_close(fp);
	d1_in_d2_free_assets(generation);
	if (error)
		*error = stage;
	return NULL;
}

static void read_d1_robot_info(robot_info *ri, PHYSFS_file *fp)
{
	int j, gun, state;

	memset(ri, 0, sizeof(*ri));
	/* D2-only fields have explicit defaults, independent of any D2 HAM */
	ri->behavior = AIB_NORMAL;
	ri->aim = 255;
	ri->lightcast = 1;
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
	ri->see_sound = (ubyte)PHYSFSX_readByte(fp);
	ri->attack_sound = (ubyte)PHYSFSX_readByte(fp);
	ri->claw_sound = (ubyte)PHYSFSX_readByte(fp);
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

static ushort d1_model_runtime_color(ushort color)
{
	/* Retain palette slots across publication and briefing palette changes */
	return color < 256 ? G3_MODEL_COLOR_INDEXED | color : color;
}

static void convert_d1_model_flat_colors(ubyte *p)
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
				set_model_word(p + 28, d1_model_runtime_color(model_word(p + 28)));
				p += 30 + ((nv & ~1) + 1) * 2;
				break;

			case D1_MODEL_OP_TMAPPOLY:
				nv = model_word(p + 2);
				p += 30 + ((nv & ~1) + 1) * 2 + nv * 12;
				break;

			case D1_MODEL_OP_SORTNORM:
				convert_d1_model_flat_colors(p + model_word(p + 28));
				convert_d1_model_flat_colors(p + model_word(p + 30));
				p += 32;
				break;

			case D1_MODEL_OP_RODBM:
				p += 36;
				break;

			case D1_MODEL_OP_SUBCALL:
				convert_d1_model_flat_colors(p + model_word(p + 16));
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

int d1_in_d2_has_native_assets(void)
{
	return Active_d1_assets != NULL;
}

int d1_in_d2_read_level_bitmap_flags(int *flags, int capacity)
{
	int i;
	if (!flags || capacity < MAX_BITMAP_FILES)
		return 0;
	for (i = 0; i < capacity; ++i)
		flags[i] = -1;
	if (!Active_d1_assets)
		return 0;
	/* Publication includes custom replacements; live bitmap flags may change
	 * when rendering, and reopening files could describe a different source */
	for (i = 1; i <= Active_d1_assets->bitmap_data->bitmap_count; ++i)
		flags[i] = Active_d1_assets->bitmap_data->bitmaps[i].bm_flags;
	return 1;
}

int d1_in_d2_native_texture_count(void)
{
	return Active_d1_assets ? Active_d1_assets->num_textures : 0;
}

const ubyte *d1_in_d2_definition_identity(void)
{
	return Active_d1_assets ? Active_d1_assets->definition_identity : NULL;
}

void d1_custom_get_stats(d1_custom_texture_stats *stats)
{
	if (!stats)
		return;
	if (Active_d1_assets)
		*stats = Active_d1_assets->custom_stats;
	else
		memset(stats, 0, sizeof(*stats));
}

/* piggy_close calls this after stopping registry users; model bytes and the
 * sound arena have been transferred to their normal engine owners */
void d1_in_d2_release_asset_data(void)
{
	int i;
	d1_asset_generation *generation = Active_d1_assets;

	if (!generation)
		return;
	Active_d1_assets = NULL;
	for (i = 1; i <= generation->bitmap_data->bitmap_count; i++)
		if (GameBitmaps[i].bm_data == generation->bitmap_data->bitmaps[i].bm_data)
			gr_set_bitmap_data(&GameBitmaps[i], NULL);
	d1_in_d2_free_assets(generation);
	memset(&Last_stats, 0, sizeof(Last_stats));
}

/* The plain SDL backend advances one sample per device tick; unlike the mixer
 * it does not convert samples from their recorded frequency */
int d1_in_d2_prepare_sound_output(d1_sound_generation *bank, int target_rate)
{
	PHYSFS_sint64 bytes = 0;
	ubyte *output;
	int lengths[MAX_SOUND_FILES], i, needs_conversion = 0;
	if (!bank || bank->count < 0 || bank->count > MAX_SOUND_FILES || target_rate < 0)
		return 0;
	if (!target_rate)
		return 1;
	for (i = 0; i < bank->count; i++) {
		const digi_sound *sample = &bank->samples[i];
		PHYSFS_sint64 length;
		lengths[i] = 0;
		if (!sample->length)
			continue;
		if (sample->freq <= 0 || sample->bits != 8 || !sample->data)
			return 0;
		length = ((PHYSFS_sint64)sample->length * target_rate + sample->freq / 2) / sample->freq;
		/* A nonempty short sample must not vanish during downsampling */
		if (!length)
			length = 1;
		if (length < 0 || length > 0x7fffffff || bytes > 0x7fffffff - length)
			return 0;
		lengths[i] = (int)length;
		bytes += length;
		needs_conversion |= sample->freq != target_rate;
	}
	if (!needs_conversion)
		return 1;
	output = d_malloc((size_t)bytes + 16);
	if (!output)
		return 0;
	bytes = 0;
	for (i = 0; i < bank->count; i++) {
		digi_sound *sample = &bank->samples[i];
		int j;
		for (j = 0; j < lengths[i]; j++) {
			int source = (int)((PHYSFS_sint64)j * sample->freq / target_rate);
			if (source >= sample->length)
				source = sample->length - 1;
			output[bytes + j] = sample->data[source];
		}
		sample->data = output + bytes;
		sample->length = lengths[i];
		sample->freq = target_rate;
		bytes += lengths[i];
	}
	d_free(bank->data);
	bank->data = output;
	bank->bytes = (size_t)bytes;
	return 1;
}

int d1_in_d2_publish_asset_data(d1_asset_generation *generation, const char **error)
{
	int i, output_rate = GameArg.SndDigiSampleRate;
	const char *stage = "prepared D1 generation";
	d1_bitmap_generation *images;
	d1_sound_generation *sounds;
	extern int extra_bitmap_num;

	if (!generation || generation == Active_d1_assets || !generation->bitmap_data ||
	    !generation->models || !generation->sound_bank.data)
		goto failed;
	images = generation->bitmap_data;
	sounds = &generation->sound_bank;
	stage = "engine asset capacities";
	if (images->bitmap_count + 1 > MAX_BITMAP_FILES || sounds->count > MAX_SOUND_FILES ||
	    generation->num_textures > MAX_TEXTURES || generation->num_vclips > VCLIP_MAXNUM ||
	    generation->num_effects > MAX_EFFECTS || generation->num_wall_anims > MAX_WALL_ANIMS ||
	    generation->num_robot_types > MAX_ROBOT_TYPES || generation->num_robot_joints > MAX_ROBOT_JOINTS ||
	    generation->num_weapon_types > MAX_WEAPON_TYPES || generation->num_powerups > MAX_POWERUP_TYPES ||
	    generation->num_polygon_models > MAX_POLYGON_MODELS || generation->num_gauges > MAX_GAUGE_BMS ||
	    generation->num_cockpits > N_COCKPIT_BITMAPS || D1_MAX_OBJ_BITMAPS > MAX_OBJ_BITMAPS)
		goto failed;
	if (!d1_in_d2_validate_asset_references(generation, &stage))
		goto failed;
	stage = "optional extension base changed after preparation";
	if (!d1_in_d2_validate_guidebot_extension(generation))
		goto failed;
	stage = "sound output conversion";
#ifdef USE_SDLMIXER
	if (!GameArg.SndDisableSdlMixer)
		output_rate = 0;
#endif
	if (!d1_in_d2_prepare_sound_output(sounds, output_rate) ||
	    !d1_in_d2_prepare_guidebot_output(generation->guidebot, output_rate))
		goto failed;

	/* All source validation precedes the first mutation of the live registry */
	digi_stop_digi_sounds();
	digi_free_cached_sounds();
#ifdef OGL
	xmodel_free_all();
#endif
	free_polygon_models();
	bm_free_extra_objbitmaps();
	gr_copy_palette(gr_palette, images->palette, sizeof(images->palette));
	memcpy(gr_fade_table, images->fade_table, sizeof(images->fade_table));
	for (i = 0; i < GR_FADE_LEVELS; i++)
		gr_fade_table[i * 256 + TRANSPARENCY_COLOR] = TRANSPARENCY_COLOR;
	piggy_reset_asset_registry();
	for (i = 1; i <= images->bitmap_count; i++) {
		GameBitmaps[i] = images->bitmaps[i];
		piggy_bitmap_set_file_state(i, 0, GameBitmaps[i].bm_flags);
		piggy_register_bitmap(&GameBitmaps[i], images->names[i], 1);
		compute_average_rgb(&GameBitmaps[i], GameBitmaps[i].avg_color_rgb);
	}
	extra_bitmap_num = Num_bitmap_files;
	SoundBits = sounds->data;
	sounds->data = NULL;
	for (i = 0; i < sounds->count; i++) {
		SoundOffset[i] = -1;
		piggy_register_sound(&sounds->samples[i], sounds->names[i], 1);
	}

	/* Clear D2-only entries as well as copying the source counts */
	memset(Textures, 0, sizeof(Textures));
	memset(TmapInfo, 0, sizeof(TmapInfo));
	for (i = 0; i < MAX_TEXTURES; i++)
		TmapInfo[i].eclip_num = TmapInfo[i].destroyed = -1;
	memcpy(Textures, generation->textures, sizeof(generation->textures));
	memcpy(TmapInfo, generation->texture_info, sizeof(generation->texture_info));
	NumTextures = generation->num_textures;
	memset(Sounds, 255, sizeof(Sounds));
	memset(AltSounds, 255, sizeof(AltSounds));
	memcpy(Sounds, generation->sound_maps[0], sizeof(generation->sound_maps[0]));
	memcpy(AltSounds, generation->sound_maps[1], sizeof(generation->sound_maps[1]));
	memset(Vclip, 0, sizeof(Vclip));
	memcpy(Vclip, generation->vclips, sizeof(generation->vclips));
	Num_vclips = generation->num_vclips;
	memset(Effects, 0, sizeof(Effects));
	memcpy(Effects, generation->effects, sizeof(generation->effects));
	Num_effects = generation->num_effects;
	memset(WallAnims, 0, sizeof(WallAnims));
	memcpy(WallAnims, generation->wall_anims, sizeof(generation->wall_anims));
	Num_wall_anims = generation->num_wall_anims;
	memset(Robot_info, 0, sizeof(Robot_info));
	memcpy(Robot_info, generation->robots, sizeof(generation->robots));
	N_robot_types = generation->num_robot_types;
	memset(Robot_joints, 0, sizeof(Robot_joints));
	memcpy(Robot_joints, generation->joints, sizeof(generation->joints));
	N_robot_joints = generation->num_robot_joints;
	memset(Weapon_info, 0, sizeof(Weapon_info));
	memcpy(Weapon_info, generation->weapons, sizeof(generation->weapons));
	N_weapon_types = generation->num_weapon_types;
	memset(Powerup_info, 0, sizeof(Powerup_info));
	memcpy(Powerup_info, generation->powerups, sizeof(generation->powerups));
	N_powerup_types = generation->num_powerups;
	memset(Polygon_models, 0, sizeof(Polygon_models));
	memset(Pof_names, 0, sizeof(Pof_names));
	for (i = 0; i < generation->num_polygon_models; i++) {
		polymodel *model = &Polygon_models[i];
		*model = generation->models[i];
		generation->models[i].model_data = NULL;
#ifdef WORDS_NEED_ALIGNMENT
		align_polygon_model_data(model);
#endif
#ifdef WORDS_BIGENDIAN
		swap_polygon_model_data(model->model_data);
#endif
		convert_d1_model_flat_colors(model->model_data);
		g3_init_polygon_model(model->model_data);
	}
	N_polygon_models = generation->num_polygon_models;
	for (i = 0; i < MAX_POLYGON_MODELS; i++) {
		Dying_modelnums[i] = i < D1_MAX_POLYGON_MODELS ? generation->dying_models[i] : -1;
		Dead_modelnums[i] = i < D1_MAX_POLYGON_MODELS ? generation->dead_models[i] : -1;
	}
	memset(ObjBitmaps, 0, sizeof(ObjBitmaps));
	memset(ObjBitmapPtrs, 0, sizeof(ObjBitmapPtrs));
	memcpy(ObjBitmaps, generation->obj_bitmaps, sizeof(generation->obj_bitmaps));
	memcpy(ObjBitmapPtrs, generation->obj_bitmap_ptrs, sizeof(generation->obj_bitmap_ptrs));
	N_ObjBitmaps = D1_MAX_OBJ_BITMAPS;
	First_multi_bitmap_num = generation->first_multi_bitmap;
	memset(Gauges, 0, sizeof(Gauges));
	memset(Gauges_hires, 0, sizeof(Gauges_hires));
	memcpy(Gauges, generation->gauges, generation->num_gauges * sizeof(*Gauges));
	memcpy(Gauges_hires, generation->gauges, generation->num_gauges * sizeof(*Gauges_hires));
	memset(cockpit_bitmap, 0, sizeof(cockpit_bitmap));
	memcpy(cockpit_bitmap, generation->cockpits, sizeof(generation->cockpits));
	Num_cockpits = generation->num_cockpits;
	only_player_ship = generation->ship;
	Player_ship = &only_player_ship;
	memset(Reactors, 0, sizeof(Reactors));
	Reactors[0] = generation->control_center;
	Num_reactors = generation->control_center.model_num >= 0 ? 1 : 0;
	exit_modelnum = generation->exit_model;
	destroyed_exit_modelnum = generation->destroyed_exit_model;
	Marker_model_num = -1;
	d1_in_d2_publish_guidebot_extension(generation);
	Active_d1_assets = generation;
	memset(&Last_stats, 0, sizeof(Last_stats));
	Last_stats.effects_active = Last_stats.effects_loaded = 1;
	Last_stats.num_effects = Num_effects;
	for (i = 0; i < Num_effects; i++)
		if (Effects[i].vc.num_frames > 0)
			Last_stats.effect_frames_applied += Effects[i].vc.num_frames;
	Last_stats.powerup_vclips_active = Last_stats.powerup_vclips_loaded = 1;
	Last_stats.num_vclips = Num_vclips;
	Last_stats.wall_anims_active = Last_stats.wall_anims_loaded = 1;
	Last_stats.wall_anim_count = Num_wall_anims;
	Last_stats.robot_assets_active = Last_stats.robot_pig_present = 1;
	Last_stats.robot_pig_size = generation->pigsize;
	Last_stats.robot_types = N_robot_types;
	Last_stats.robot_joints = N_robot_joints;
	Last_stats.robot_models = N_polygon_models;
	Last_stats.weapon_records_active = Last_stats.player_ship_active = 1;
	Last_stats.weapon_types = N_weapon_types;
	Last_stats.robot_obj_bitmaps = Last_stats.robot_obj_bitmaps_applied = N_ObjBitmaps;
	Last_stats.sounds_active = Last_stats.sound_pig_present = 1;
	Last_stats.sound_pig_size = generation->pigsize;
	Last_stats.sound_map_entries = D1_MAX_PIG_SOUNDS;
	Last_stats.sound_files = sounds->count;
	Last_stats.sound_bytes = (int)sounds->bytes;
	Last_stats.cockpit_active = 1;
	Last_stats.cockpit_frames_applied = Num_cockpits;
	if (error)
		*error = NULL;
	return 1;

failed:
	if (error)
		*error = stage;
	return 0;
}

int d1_in_d2_ensure_spawnable_guidebot(void)
{
	return !Active_d1_assets || Active_d1_assets->guidebot != NULL;
}

int d1_in_d2_is_spawnable_guidebot_model(int model_num)
{
	return Active_d1_assets && d1_in_d2_guidebot_owns_model(model_num);
}
