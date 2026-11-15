/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/*
 *
 * Bitmap and palette loading functions.
 *
 */

#ifdef __ANDROID__
#include "android_sound_trace.h"
#endif

#include "input_demo_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pstypes.h"
#include "inferno.h"
#include "gr.h"
#include "bm.h"
#include "u_mem.h"
#ifdef DXX_XL_RUNTIME
#include "resource_validation.h"
#endif
#include "dxxerror.h"
#include "object.h"
#include "vclip.h"
#include "effects.h"
#include "polyobj.h"
#include "wall.h"
#include "textures.h"
#include "game.h"
#ifdef NETWORK
#include "multi.h"
#endif
#include "iff.h"
#include "powerup.h"
#include "sounds.h"
#include "piggy.h"
#include "d1_in_d2/d1_in_d2.h"
#include "aistruct.h"
#include "robot.h"
#include "weapon.h"
#include "gauges.h"
#include "player.h"
#include "endlevel.h"
#include "cntrlcen.h"
#include "makesig.h"
#include "interp.h"
#include "console.h"
#include "rle.h"
#include "xmodel.h"
#include "dxa_metadata_patch.h"
#include "d2_ham_validation.h"
#include "bm_ham.h"

ubyte Sounds[MAX_SOUNDS];
ubyte AltSounds[MAX_SOUNDS];

#ifdef EDITOR
int Num_object_subtypes = 1;
#endif

//for each model, a model number for dying & dead variants, or -1 if none
int Dying_modelnums[MAX_POLYGON_MODELS];
int Dead_modelnums[MAX_POLYGON_MODELS];

//the polygon model number to use for the marker
int	Marker_model_num = -1;

//right now there's only one player ship, but we can have another by
//adding an array and setting the pointer to the active ship.
player_ship only_player_ship,*Player_ship=&only_player_ship;

//----------------- Miscellaneous bitmap pointers ---------------
int             Num_cockpits = 0;
bitmap_index    cockpit_bitmap[N_COCKPIT_BITMAPS];

//---------------- Variables for wall textures ------------------
int             Num_tmaps;
tmap_info       TmapInfo[MAX_TEXTURES];

//---------------- Variables for object textures ----------------

int             First_multi_bitmap_num=-1;

int             N_ObjBitmaps;
bitmap_index    ObjBitmaps[MAX_OBJ_BITMAPS];
ushort          ObjBitmapPtrs[MAX_OBJ_BITMAPS];     // These point back into ObjBitmaps, since some are used twice.

/*
 * reads n tmap_info structs from a PHYSFS_file
 */
int tmap_info_read_n(tmap_info *ti, int n, PHYSFS_file *fp)
{
	int i;

	for (i = 0; i < n; i++) {
		ti[i].flags = PHYSFSX_readByte(fp);
		ti[i].pad[0] = PHYSFSX_readByte(fp);
		ti[i].pad[1] = PHYSFSX_readByte(fp);
		ti[i].pad[2] = PHYSFSX_readByte(fp);
		ti[i].lighting = PHYSFSX_readFix(fp);
		ti[i].damage = PHYSFSX_readFix(fp);
		ti[i].eclip_num = PHYSFSX_readShort(fp);
		ti[i].destroyed = PHYSFSX_readShort(fp);
		ti[i].slide_u = PHYSFSX_readShort(fp);
		ti[i].slide_v = PHYSFSX_readShort(fp);
	}
	return i;
}

int tmap_info_read_n_d1(tmap_info *ti, int n, PHYSFS_file *fp)
{
	int i;

	for (i = 0; i < n; i++) {
		PHYSFSX_fseek(fp, 13, SEEK_CUR);// skip filename
		ti[i].flags = PHYSFSX_readByte(fp);
		ti[i].lighting = PHYSFSX_readFix(fp);
		ti[i].damage = PHYSFSX_readFix(fp);
		ti[i].eclip_num = PHYSFSX_readInt(fp);
	}
	return i;
}

void gamedata_close()
{
	free_polygon_models();
	bm_free_extra_objbitmaps();
	free_endlevel_data();
	rle_cache_close();
	piggy_close();
}

//-----------------------------------------------------------------
// Initializes game properties data (including texture caching system) and sound data.
int gamedata_init()
{
	init_polygon_models();
	init_endlevel();
	if (d1_in_d2_prepare_base_assets())
		return 0;

#ifdef EDITOR
	// The pc_shareware argument is currently unused for Descent 2,
	// but *may* be useful for loading Descent 1 Shareware texture properties.
	if (!gamedata_read_tbl(0))
#endif
		if (!properties_init())				// This calls properties_read_cmp
				Error("Cannot open ham file\n");

	piggy_read_sounds();

#ifdef OGL
	xmodel_load_all();
#endif

	d1_in_d2_note_d2_base_loaded();
	return 0;
}

static int bm_read_ham_data(PHYSFS_file *fp, int version, const bm_ham_destination *destination,
                            int (*read_model)(polymodel *, PHYSFS_file *))
{
	int i,t;
	REWIND_PHYSFS_FILE(rewind_fp, fp);

	(*destination->NumTextures) = PHYSFSX_readInt(fp);
	bitmap_index_read_n(destination->Textures, (*destination->NumTextures), fp );
	tmap_info_read_n(destination->TmapInfo, (*destination->NumTextures), fp);

	t = PHYSFSX_readInt(fp);
	PHYSFS_read( fp, destination->Sounds, sizeof(ubyte), t );
	PHYSFS_read( fp, destination->AltSounds, sizeof(ubyte), t );

	(*destination->Num_vclips) = PHYSFSX_readInt(fp);
	vclip_read_n(destination->Vclip, (*destination->Num_vclips), fp);

	(*destination->Num_effects) = PHYSFSX_readInt(fp);
	eclip_read_n(destination->Effects, (*destination->Num_effects), fp);

	(*destination->Num_wall_anims) = PHYSFSX_readInt(fp);
	wclip_read_n(destination->WallAnims, (*destination->Num_wall_anims), rewind_fp);

	(*destination->N_robot_types) = PHYSFSX_readInt(fp);
	robot_info_read_n(destination->Robot_info, (*destination->N_robot_types), fp);

	(*destination->N_robot_joints) = PHYSFSX_readInt(fp);
	jointpos_read_n(destination->Robot_joints, (*destination->N_robot_joints), fp);

	(*destination->N_weapon_types) = PHYSFSX_readInt(fp);
	weapon_info_read_n(destination->Weapon_info, (*destination->N_weapon_types), fp, version);

	(*destination->N_powerup_types) = PHYSFSX_readInt(fp);
	powerup_type_info_read_n(destination->Powerup_info, (*destination->N_powerup_types), fp);

	(*destination->N_polygon_models) = PHYSFSX_readInt(fp);
	polymodel_read_n(destination->Polygon_models, (*destination->N_polygon_models), fp);

	for (i=0; i<(*destination->N_polygon_models); i++ )
		if (!read_model(&destination->Polygon_models[i], fp)) return 0;

	for (i = 0; i < (*destination->N_polygon_models); i++)
		destination->Dying_modelnums[i] = PHYSFSX_readInt(fp);
	for (i = 0; i < (*destination->N_polygon_models); i++)
		destination->Dead_modelnums[i] = PHYSFSX_readInt(fp);

	t = PHYSFSX_readInt(fp);
	bitmap_index_read_n(destination->Gauges, t, fp);
	bitmap_index_read_n(destination->Gauges_hires, t, fp);

	(*destination->N_ObjBitmaps) = PHYSFSX_readInt(fp);
	bitmap_index_read_n(destination->ObjBitmaps, (*destination->N_ObjBitmaps), fp);
	for (i = 0; i < (*destination->N_ObjBitmaps); i++)
		destination->ObjBitmapPtrs[i] = PHYSFSX_readShort(fp);

	player_ship_read(&(*destination->only_player_ship), fp);

	(*destination->Num_cockpits) = PHYSFSX_readInt(fp);
	bitmap_index_read_n(destination->cockpit_bitmap, (*destination->Num_cockpits), fp);

//@@	PHYSFS_read( fp, &Num_total_object_types, sizeof(int), 1 );
//@@	PHYSFS_read( fp, ObjType, sizeof(byte), Num_total_object_types );
//@@	PHYSFS_read( fp, ObjId, sizeof(byte), Num_total_object_types );
//@@	PHYSFS_read( fp, ObjStrength, sizeof(fix), Num_total_object_types );

	(*destination->First_multi_bitmap_num) = PHYSFSX_readInt(fp);

	(*destination->Num_reactors) = PHYSFSX_readInt(fp);
	reactor_read_n(destination->Reactors, (*destination->Num_reactors), rewind_fp);

	(*destination->Marker_model_num) = PHYSFSX_readInt(fp);

	//@@PHYSFS_read( fp, &N_controlcen_guns, sizeof(int), 1 );
	//@@PHYSFS_read( fp, controlcen_gun_points, sizeof(vms_vector), N_controlcen_guns );
	//@@PHYSFS_read( fp, controlcen_gun_dirs, sizeof(vms_vector), N_controlcen_guns );

	if (version < 3) {
		(*destination->exit_modelnum) = PHYSFSX_readInt(fp);
		(*destination->destroyed_exit_modelnum) = PHYSFSX_readInt(fp);
	}
	else
		(*destination->exit_modelnum) = (*destination->destroyed_exit_modelnum) = (*destination->N_polygon_models);

	return 1;
}

static int bm_read_live_model(polymodel *model, PHYSFS_file *fp)
{
	polygon_model_data_read(model, fp);
	return 1;
}

static bm_ham_destination bm_live_ham_destination(void)
{
	const bm_ham_destination destination = {
		&NumTextures,
		Textures,
		TmapInfo,
		Sounds,
		AltSounds,
		&Num_vclips,
		Vclip,
		&Num_effects,
		Effects,
		&Num_wall_anims,
		WallAnims,
		&N_robot_types,
		Robot_info,
		&N_robot_joints,
		Robot_joints,
		&N_weapon_types,
		Weapon_info,
		&N_powerup_types,
		Powerup_info,
		&N_polygon_models,
		Polygon_models,
		Dying_modelnums,
		Dead_modelnums,
		Gauges,
		Gauges_hires,
		&N_ObjBitmaps,
		ObjBitmaps,
		ObjBitmapPtrs,
		&only_player_ship,
		&Num_cockpits,
		cockpit_bitmap,
		&First_multi_bitmap_num,
		&Num_reactors,
		Reactors,
		&Marker_model_num,
		&exit_modelnum,
		&destroyed_exit_modelnum,
		NULL,
	};
	return destination;
}

static void bm_read_all_internal(PHYSFS_file *fp, int apply_global_patch)
{
	const bm_ham_destination destination = bm_live_ham_destination();
	bm_read_ham_data(fp, Piggy_hamfile_version, &destination, bm_read_live_model);
	if (apply_global_patch) dxa_metadata_patch_apply_d2_ham();
#if defined(__ANDROID__) || defined(DXX_XL_CHECKPOINT_EXPORT)
	extern void mod_save_capture_definitions(void);
	mod_save_capture_definitions();
#endif
}

void bm_read_all(PHYSFS_file *fp)
{
	bm_read_all_internal(fp, 1);
}

#ifdef DXX_XL_RUNTIME
static int bm_validate_main_ham_report(PHYSFS_file *fp, d2_ham_validation_report *report);
int bm_read_xl_ham_into(PHYSFS_file *fp, const bm_ham_destination *destination,
                      int (*read_model)(polymodel *, PHYSFS_file *))
{
	d2_ham_validation_report report;
	if (!fp || !destination || !destination->GameBitmapXlat || !read_model || !bm_validate_main_ham_report(fp, &report) || !PHYSFS_seek(fp, 0)) return 0;
	if (PHYSFSX_readInt(fp) != MAKE_SIG('!','M','A','H') || PHYSFSX_readInt(fp) != 3) return 0;
	/* Native HAM admission permits a short optional translation tail */
	if (!bm_read_ham_data(fp, 3, destination, read_model)) return 0;
	piggy_read_bitmap_translation(fp, destination->GameBitmapXlat);
	for (int i = 0; i < MAX_BITMAP_FILES; ++i)
		if (destination->GameBitmapXlat[i] >= MAX_BITMAP_FILES) return 0;
	return 1;
}

void bm_install_xl_ham(bm_ham_destination *source)
{
	int i;
	for (i = 0; i < N_polygon_models; ++i)
		if (Polygon_models[i].model_data) free_model(&Polygon_models[i]);
	init_polygon_models();
#define BM_COPY_COUNT(name) name = *source->name
#define BM_COPY_TABLE(name) memcpy(name, source->name, sizeof(name))
	BM_COPY_COUNT(NumTextures);
	BM_COPY_TABLE(Textures);
	BM_COPY_TABLE(TmapInfo);
	BM_COPY_TABLE(Sounds);
	BM_COPY_TABLE(AltSounds);
	BM_COPY_COUNT(Num_vclips);
	BM_COPY_TABLE(Vclip);
	BM_COPY_COUNT(Num_effects);
	BM_COPY_TABLE(Effects);
	BM_COPY_COUNT(Num_wall_anims);
	BM_COPY_TABLE(WallAnims);
	BM_COPY_COUNT(N_robot_types);
	BM_COPY_TABLE(Robot_info);
	BM_COPY_COUNT(N_robot_joints);
	BM_COPY_TABLE(Robot_joints);
	BM_COPY_COUNT(N_weapon_types);
	BM_COPY_TABLE(Weapon_info);
	BM_COPY_COUNT(N_powerup_types);
	BM_COPY_TABLE(Powerup_info);
	BM_COPY_COUNT(N_polygon_models);
	BM_COPY_TABLE(Polygon_models);
	BM_COPY_TABLE(Dying_modelnums);
	BM_COPY_TABLE(Dead_modelnums);
	BM_COPY_TABLE(Gauges);
	BM_COPY_TABLE(Gauges_hires);
	BM_COPY_COUNT(N_ObjBitmaps);
	BM_COPY_TABLE(ObjBitmaps);
	BM_COPY_TABLE(ObjBitmapPtrs);
	BM_COPY_COUNT(only_player_ship);
	BM_COPY_COUNT(Num_cockpits);
	BM_COPY_TABLE(cockpit_bitmap);
	BM_COPY_COUNT(First_multi_bitmap_num);
	BM_COPY_COUNT(Num_reactors);
	BM_COPY_TABLE(Reactors);
	BM_COPY_COUNT(Marker_model_num);
	BM_COPY_COUNT(exit_modelnum);
	BM_COPY_COUNT(destroyed_exit_modelnum);
	piggy_install_bitmap_translation(source->GameBitmapXlat);
#undef BM_COPY_TABLE
#undef BM_COPY_COUNT
	for (i = 0; i < N_polygon_models; ++i)
		source->Polygon_models[i].model_data = NULL;
	Piggy_hamfile_version = 3;
}

/* Mission-owned version-3 HAM only; validate the complete file before mutation */
int bm_read_xl_main_ham(PHYSFS_file *fp)
{
	int i;
	if (!fp || !bm_validate_main_ham(fp) || !PHYSFS_seek(fp, 0)) return 0;
	if (PHYSFSX_readInt(fp) != MAKE_SIG('!','M','A','H') || PHYSFSX_readInt(fp) != 3) return 0;
	for (i = 0; i < N_polygon_models; ++i)
		if (Polygon_models[i].model_data) free_model(&Polygon_models[i]);
	init_polygon_models();
	Piggy_hamfile_version = 3;
	/* Mission data overrides installation-wide metadata replacement packs */
	bm_read_all_internal(fp, 0);
	return 1;
}
#endif

//these values are the number of each item in the release of d2
//extra items added after the release get written in an additional hamfile

static d2_ham_validation_report Main_ham_validation_report;
static d2_ham_validation_report Mission_ham_validation_report;
static d2_ham_validation_report Level_hxm_validation_report;
static int Mission_ham_rejected;
static int Level_hxm_rejected;

extern int Num_bitmap_files;
int extra_bitmap_num = 0;

static d2_ham_validation_limits bm_validation_limits(void)
{
	d2_ham_validation_limits limits = {
		MAX_TEXTURES, MAX_SOUNDS, VCLIP_MAXNUM, MAX_EFFECTS, MAX_WALL_ANIMS,
		MAX_ROBOT_TYPES, MAX_ROBOT_JOINTS, MAX_WEAPON_TYPES, MAX_POWERUP_TYPES,
		MAX_POLYGON_MODELS, MAX_GAUGE_BMS, MAX_OBJ_BITMAPS, N_COCKPIT_BITMAPS,
		MAX_REACTORS, MAX_BITMAP_FILES, MAX_SUBMODELS
	};
	return limits;
}

static int bm_read_validation_bytes(PHYSFS_file *fp, ubyte **data, size_t *size)
{
	PHYSFS_sint64 length, position;
	ubyte *bytes;
	if (!fp || !data || !size) return 0;
	*data = NULL;
	*size = 0;
	length = PHYSFS_fileLength(fp);
	position = PHYSFS_tell(fp);
	if (length <= 0 || length > 64 * 1024 * 1024 || position < 0 ||
	    !PHYSFS_seek(fp, 0))
		return 0;
	bytes = d_malloc((size_t)length);
	if (!bytes) {
		PHYSFS_seek(fp, position);
		return 0;
	}
	if (PHYSFS_readBytes(fp, bytes, length) != length || !PHYSFS_seek(fp, position)) {
		d_free(bytes);
		return 0;
	}
	*data = bytes;
	*size = (size_t)length;
	return 1;
}

static int bm_validate_main_ham_report(PHYSFS_file *fp, d2_ham_validation_report *report)
{
	d2_ham_validation_limits limits = bm_validation_limits();
	ubyte *data;
	size_t size;
	int valid;
	if (!bm_read_validation_bytes(fp, &data, &size)) return 0;
	valid = d2_ham_validate_main(data, size, &limits, report);
	if (!valid) memset(report, 0, sizeof(*report));
	d_free(data);
	return valid;
}

int bm_validate_main_ham(PHYSFS_file *fp)
{
	return bm_validate_main_ham_report(fp, &Main_ham_validation_report);
}

static int bm_validate_extra_ham(PHYSFS_file *fp, int type, int xl_staged)
{
	d2_ham_validation_limits limits = bm_validation_limits();
	ubyte *data;
	size_t size;
	int valid;
	if (!bm_read_validation_bytes(fp, &data, &size)) return 0;
	if (xl_staged)
		valid = d2_ham_validate_xl_extra(data, size, &limits, &Mission_ham_validation_report);
	else
	valid = d2_ham_validate_extra(data, size, type, &limits,
	                              N_D2_WEAPON_TYPES, N_D2_ROBOT_TYPES,
	                              N_D2_ROBOT_JOINTS, N_D2_POLYGON_MODELS,
	                              N_D2_OBJBITMAPS, N_D2_OBJBITMAPPTRS,
	                              &Mission_ham_validation_report);
	if (!valid) memset(&Mission_ham_validation_report, 0, sizeof(Mission_ham_validation_report));
	d_free(data);
	return valid;
}

static int bm_validate_hxm(PHYSFS_file *fp)
{
	d2_ham_validation_limits limits = bm_validation_limits();
	ubyte *data;
	size_t size;
	int valid;
	if (!bm_read_validation_bytes(fp, &data, &size)) return 0;
	valid = d2_ham_validate_hxm(data, size, &limits,
	                            N_robot_types, N_robot_joints, N_polygon_models,
	                            &Level_hxm_validation_report);
	if (!valid) memset(&Level_hxm_validation_report, 0, sizeof(Level_hxm_validation_report));
	d_free(data);
	return valid;
}

/* Read-only companion preflight; do not replace the active warning report */
static int bm_validate_level_hxm_internal(PHYSFS_file *fp, int xl_deferred,
                                          int robots, int joints, int models)
{
	d2_ham_validation_limits limits = bm_validation_limits();
	d2_ham_validation_report report;
	ubyte *data;
	size_t size;
	int valid;
	if (!bm_read_validation_bytes(fp, &data, &size)) return 0;
	valid = (xl_deferred ? d2_ham_validate_xl_hxm : d2_ham_validate_hxm)(data, size, &limits,
	                          robots, joints, models, &report);
	d_free(data);
	/* Empty joint lists are not dereferenced; XL closes after the five tables */
	return valid && !report.inert_robot_records;
}

int bm_validate_level_hxm(PHYSFS_file *fp)
{
	return bm_validate_level_hxm_internal(fp, 0, N_robot_types, N_robot_joints, N_polygon_models);
}

int bm_validate_xl_level_hxm(PHYSFS_file *fp)
{
	return bm_validate_level_hxm_internal(fp, 1, N_robot_types, N_robot_joints, N_polygon_models);
}

int bm_validate_hxm_for_tables(PHYSFS_file *fp, int robots, int joints, int models, int xl_deferred)
{
	return bm_validate_level_hxm_internal(fp, xl_deferred, robots, joints, models);
}

void bm_reset_mission_compatibility_warnings(void)
{
	memset(&Mission_ham_validation_report, 0, sizeof(Mission_ham_validation_report));
	Mission_ham_rejected = 0;
}

void bm_reset_level_compatibility_warnings(void)
{
	memset(&Level_hxm_validation_report, 0, sizeof(Level_hxm_validation_report));
	Level_hxm_rejected = 0;
}

static int bm_validation_report_warning_count(const d2_ham_validation_report *report)
{
	return report->inert_robot_records || report->normalized_empty_joint_offsets ||
	       report->ignored_trailing_bytes;
}

int bm_compatibility_warning_count(void)
{
	/* Base HAM quirks are installation-wide format details, not level data warnings */
	return bm_validation_report_warning_count(&Mission_ham_validation_report) +
	       bm_validation_report_warning_count(&Level_hxm_validation_report) +
	       Mission_ham_rejected + Level_hxm_rejected;
}

static int bm_validation_report_warning_text(const d2_ham_validation_report *report,
	                                          const char *kind, int *warning_index,
	                                          char *buffer, unsigned buffer_size)
{
	unsigned used;
	int has_item = 0;
	if (!bm_validation_report_warning_count(report)) return 0;
	if ((*warning_index)-- != 0) return 0;
	snprintf(buffer, buffer_size, "%s legacy handling:", kind);
	used = (unsigned) strlen(buffer);
#define BM_APPEND_WARNING(...) do { \
	if (used < buffer_size) { \
		int written = snprintf(buffer + used, buffer_size - used, __VA_ARGS__); \
		if (written > 0) used += (unsigned) written; \
	} \
} while (0)
	if (report->inert_robot_records) {
		BM_APPEND_WARNING(" %u inert robot slot%s", report->inert_robot_records,
		                  report->inert_robot_records == 1 ? "" : "s");
		has_item = 1;
	}
	if (report->normalized_empty_joint_offsets) {
		BM_APPEND_WARNING("%s%u unused joint offset%s normalized", has_item ? "; " : " ",
		                  report->normalized_empty_joint_offsets,
		                  report->normalized_empty_joint_offsets == 1 ? "" : "s");
		has_item = 1;
	}
	if (report->ignored_trailing_bytes)
		BM_APPEND_WARNING("%s%u trailer byte%s ignored", has_item ? "; " : " ",
		                  (unsigned) report->ignored_trailing_bytes,
		                  report->ignored_trailing_bytes == 1 ? "" : "s");
#undef BM_APPEND_WARNING
	return 1;
}

int bm_compatibility_warning_text(int warning_index, char *buffer, unsigned buffer_size)
{
	if (!buffer || !buffer_size || warning_index < 0)
		return 0;
	if (bm_validation_report_warning_text(&Mission_ham_validation_report, "Mission HAM",
	                                      &warning_index, buffer, buffer_size) ||
	    bm_validation_report_warning_text(&Level_hxm_validation_report, "Level HXM",
	                                      &warning_index, buffer, buffer_size))
		return 1;
	if (Mission_ham_rejected) {
		if (!warning_index) {
			snprintf(buffer, buffer_size, "Rejected unsafe mission HAM companion");
			return 1;
		}
		--warning_index;
	}
	if (Level_hxm_rejected && !warning_index) {
		snprintf(buffer, buffer_size, "Rejected unsafe level HXM companion");
		return 1;
	}
	buffer[0] = '\0';
	return 0;
}

void bm_free_extra_objbitmaps()
{
	int i;

	if (!extra_bitmap_num)
		extra_bitmap_num = Num_bitmap_files;

	for (i = Num_bitmap_files; i < extra_bitmap_num; i++)
	{
		N_ObjBitmaps--;
		d_free(GameBitmaps[i].bm_data);
	}
	extra_bitmap_num = Num_bitmap_files;
}

void bm_free_extra_models()
{
	while (N_polygon_models > N_D2_POLYGON_MODELS)
		free_model(&Polygon_models[--N_polygon_models]);
	while (N_polygon_models > exit_modelnum)
		free_model(&Polygon_models[--N_polygon_models]);
}

/* Shared native extra-HAM record decoder; callers validate and own destinations */
static int bm_read_extra_ham_data(PHYSFS_file *fp, const bm_ham_destination *destination,
                                  int (*read_model)(polymodel *, PHYSFS_file *),
                                  int *bitmap_count, int *pointer_count)
{
	int t, i;
	//read extra weapons

	t = PHYSFSX_readInt(fp);
	(*destination->N_weapon_types) = N_D2_WEAPON_TYPES+t;
	weapon_info_read_n(&destination->Weapon_info[N_D2_WEAPON_TYPES], t, fp, 3);

	//now read robot info

	t = PHYSFSX_readInt(fp);
	(*destination->N_robot_types) = N_D2_ROBOT_TYPES+t;
	robot_info_read_n(&destination->Robot_info[N_D2_ROBOT_TYPES], t, fp);

	t = PHYSFSX_readInt(fp);
	(*destination->N_robot_joints) = N_D2_ROBOT_JOINTS+t;
	jointpos_read_n(&destination->Robot_joints[N_D2_ROBOT_JOINTS], t, fp);

	t = PHYSFSX_readInt(fp);
	(*destination->N_polygon_models) = N_D2_POLYGON_MODELS+t;
	polymodel_read_n(&destination->Polygon_models[N_D2_POLYGON_MODELS], t, fp);

	for (i=N_D2_POLYGON_MODELS; i<(*destination->N_polygon_models); i++ )
		if (!read_model(&destination->Polygon_models[i], fp)) return 0;

	for (i = N_D2_POLYGON_MODELS; i < (*destination->N_polygon_models); i++)
		destination->Dying_modelnums[i] = PHYSFSX_readInt(fp);
	for (i = N_D2_POLYGON_MODELS; i < (*destination->N_polygon_models); i++)
		destination->Dead_modelnums[i] = PHYSFSX_readInt(fp);

	t = PHYSFSX_readInt(fp);
	*bitmap_count = t;
	bitmap_index_read_n(&destination->ObjBitmaps[N_D2_OBJBITMAPS], t, fp);

	t = PHYSFSX_readInt(fp);
	*pointer_count = t;
	for (i = N_D2_OBJBITMAPPTRS; i < (N_D2_OBJBITMAPPTRS + t); i++)
		destination->ObjBitmapPtrs[i] = PHYSFSX_readShort(fp);

	return 1;
}

#ifdef DXX_XL_RUNTIME
int bm_read_xl_extra_ham_into(PHYSFS_file *fp, const bm_ham_destination *destination,
                             int (*read_model)(polymodel *, PHYSFS_file *),
                             int *bitmap_count, int *pointer_count)
{
	d2_ham_validation_limits limits = bm_validation_limits();
	d2_ham_validation_report report;
	ubyte *data;
	size_t size;
	int valid;
	if (!fp || !destination || !read_model || !bitmap_count || !pointer_count ||
	    !bm_read_validation_bytes(fp, &data, &size)) return 0;
	valid = d2_ham_validate_xl_extra(data, size, &limits, &report);
	d_free(data);
	if (!valid || !PHYSFS_seek(fp, 8)) return 0;
	return bm_read_extra_ham_data(fp, destination, read_model, bitmap_count, pointer_count);
}
#endif

//type==1 means 1.1, type==2 means 1.2 (with weapons)
static int bm_read_extra_robots_internal(const char *fname, int type, int xl_staged)
{
	PHYSFS_file *fp;
	int version;

	bm_reset_mission_compatibility_warnings();
	fp = PHYSFSX_openReadBuffered(fname);
	if (!fp)
	{
		con_printf(CON_URGENT, "Failed to open HAM file \"%s\"\n", fname);
		return 0;
	}
	if (!bm_validate_extra_ham(fp, type, xl_staged)) {
		Mission_ham_rejected = 1;
		con_printf(CON_URGENT, "Rejected malformed extra HAM file \"%s\"\n", fname);
		PHYSFS_close(fp);
		return 0;
	}

#ifdef __ANDROID__
	android_sound_trace_asset_open("extra_ham", fname);
#endif
#if defined(__ANDROID__) || defined(DXX_XL_CHECKPOINT_EXPORT)
	extern void mod_save_capture_extra_ham(const char *filename);
	mod_save_capture_extra_ham(fname);
#endif
	if (type == 2) {
		int sig;

		sig = PHYSFSX_readInt(fp);
		if (sig != MAKE_SIG('X','H','A','M')) {
			PHYSFS_close(fp);
			return 0;
		}
		version = PHYSFSX_readInt(fp);
	}
	else
		version = 0;
	(void)version; // NOTE: we do not need it, but keep it for possible further use

	bm_free_extra_models();
	bm_free_extra_objbitmaps();

	const bm_ham_destination destination = bm_live_ham_destination();
	int bitmap_count, pointer_count;
	if (!bm_read_extra_ham_data(fp, &destination, bm_read_live_model, &bitmap_count, &pointer_count)) {
		PHYSFS_close(fp);
		return 0;
	}

	input_demo_assets_extra_ham_loaded(fname, fp);
	PHYSFS_close(fp);
	return 1;
}

void bm_read_extra_robots(const char *fname, int type)
{
	(void)bm_read_extra_robots_internal(fname, type, 0);
}
#ifdef DXX_XL_RUNTIME
int bm_read_xl_extra_robots(const char *fname)
{
	return bm_read_extra_robots_internal(fname, 2, 1);
}
#endif

int Robot_replacements_loaded = 0;

void load_robot_replacements(char *level_name)
{
	PHYSFS_file *fp;
	int t,i,j;
	char ifile_name[FILENAME_LEN];

	bm_reset_level_compatibility_warnings();

#ifdef DXX_XL_RUNTIME
	if (xl_install_prepared_hxm(level_name)) return;
#endif
	change_filename_extension(ifile_name, level_name, ".HXM" );

	fp = PHYSFSX_openReadBuffered(ifile_name);

	if (!fp)		//no robot replacement file
		return;
	if (!bm_validate_hxm(fp)) {
		Level_hxm_rejected = 1;
		con_printf(CON_URGENT, "Rejected malformed robot replacement file \"%s\"\n", ifile_name);
		PHYSFS_close(fp);
		return;
	}

#ifdef __ANDROID__
	android_sound_trace_asset_open("hxm", ifile_name);
#endif
	t = PHYSFSX_readInt(fp);			//read id "HXM!"
	if (t!= 0x21584d48) {
		PHYSFS_close(fp);
		return;
	}

	t = PHYSFSX_readInt(fp);			//read version
	if (t != 1) {
		PHYSFS_close(fp);
		return;
	}

	t = PHYSFSX_readInt(fp);			//read number of robots
	for (j=0;j<t;j++) {
		i = PHYSFSX_readInt(fp);		//read robot number
		if (i<0 || i>=N_robot_types) {
			PHYSFS_close(fp);
			return;
		}
		robot_info_read_n(&Robot_info[i], 1, fp);
	}

	t = PHYSFSX_readInt(fp);			//read number of joints
	for (j=0;j<t;j++) {
		i = PHYSFSX_readInt(fp);		//read joint number
		if (i<0 || i>=N_robot_joints) {
			PHYSFS_close(fp);
			return;
		}
		jointpos_read_n(&Robot_joints[i], 1, fp);
	}

	t = PHYSFSX_readInt(fp);			//read number of polygon models
	for (j=0;j<t;j++)
	{
		i = PHYSFSX_readInt(fp);		//read model number
		if (i<0 || i>=N_polygon_models) {
			PHYSFS_close(fp);
			return;
		}

		free_model(&Polygon_models[i]);
		polymodel_read(&Polygon_models[i], fp);
		polygon_model_data_read(&Polygon_models[i], fp);

		Dying_modelnums[i] = PHYSFSX_readInt(fp);
		Dead_modelnums[i] = PHYSFSX_readInt(fp);
	}

	t = PHYSFSX_readInt(fp);			//read number of objbitmaps
	for (j=0;j<t;j++) {
		i = PHYSFSX_readInt(fp);		//read objbitmap number
		if (i<0 || i>=MAX_OBJ_BITMAPS) {
			PHYSFS_close(fp);
			return;
		}
		bitmap_index_read(&ObjBitmaps[i], fp);
	}

	t = PHYSFSX_readInt(fp);			//read number of objbitmapptrs
	for (j=0;j<t;j++) {
		i = PHYSFSX_readInt(fp);		//read objbitmapptr number
		if (i<0 || i>=MAX_OBJ_BITMAPS) {
			PHYSFS_close(fp);
			return;
		}
		ObjBitmapPtrs[i] = PHYSFSX_readShort(fp);
	}

	PHYSFS_close(fp);
	Robot_replacements_loaded = 1;
}


/*
 * Routines for loading exit models
 *
 * Used by d1 levels (including some add-ons), and by d2 shareware.
 * Could potentially be used by d2 add-on levels, but only if they
 * don't use "extra" robots...
 */

// formerly exitmodel_bm_load_sub
bitmap_index read_extra_bitmap_iff( char * filename )
{
	bitmap_index bitmap_num;
	grs_bitmap * n = &GameBitmaps[extra_bitmap_num];
	ubyte newpal[256*3];
	int iff_error;		//reference parm to avoid warning message

	bitmap_num.index = 0;

	//MALLOC( new, grs_bitmap, 1 );
	iff_error = iff_read_bitmap(filename,n,BM_LINEAR,newpal);
	n->bm_handle=0;
	if (iff_error != IFF_NO_ERROR)		{
		con_printf(CON_DEBUG, "Error loading exit model bitmap <%s> - IFF error: %s\n", filename, iff_errormsg(iff_error));
		return bitmap_num;
	}

	if ( iff_has_transparency )
		gr_remap_bitmap_good( n, newpal, iff_transparent_color, 254 );
	else
		gr_remap_bitmap_good( n, newpal, -1, 254 );

	n->avg_color = 0;	//compute_average_pixel(new);

	bitmap_num.index = extra_bitmap_num;

	GameBitmaps[extra_bitmap_num++] = *n;

	//d_free( n );
	return bitmap_num;
}

// formerly load_exit_model_bitmap
grs_bitmap *bm_load_extra_objbitmap(char *name)
{
	Assert(N_ObjBitmaps < MAX_OBJ_BITMAPS);

	{
		ObjBitmaps[N_ObjBitmaps] = read_extra_bitmap_iff(name);

		if (ObjBitmaps[N_ObjBitmaps].index == 0)
		{
			char *name2 = d_strdup(name);
			*strrchr(name2, '.') = '\0';
			ObjBitmaps[N_ObjBitmaps] = read_extra_bitmap_d1_pig(name2);
			d_free(name2);
		}
		if (ObjBitmaps[N_ObjBitmaps].index == 0)
			return NULL;

		if (GameBitmaps[ObjBitmaps[N_ObjBitmaps].index].bm_w!=64 || GameBitmaps[ObjBitmaps[N_ObjBitmaps].index].bm_h!=64)
			Error("Bitmap <%s> is not 64x64",name);
		ObjBitmapPtrs[N_ObjBitmaps] = N_ObjBitmaps;
		N_ObjBitmaps++;
		Assert(N_ObjBitmaps < MAX_OBJ_BITMAPS);
		return &GameBitmaps[ObjBitmaps[N_ObjBitmaps-1].index];
	}
}

#ifdef OGL
void ogl_cache_polymodel_textures(int model_num);
#endif

int load_exit_models()
{
	PHYSFS_file *exit_hamfile;
	int start_num;
	if (d1_in_d2_has_native_assets())
		return exit_modelnum >= 0 && destroyed_exit_modelnum >= 0;

	Robot_replacements_loaded = 1; // extra robots need reloading even if exit assets are missing
	bm_free_extra_models();
	bm_free_extra_objbitmaps();

	start_num = N_ObjBitmaps;
	if (!bm_load_extra_objbitmap("steel1.bbm") ||
		!bm_load_extra_objbitmap("rbot061.bbm") ||
		!bm_load_extra_objbitmap("rbot062.bbm") ||
		!bm_load_extra_objbitmap("steel1.bbm") ||
		!bm_load_extra_objbitmap("rbot061.bbm") ||
		!bm_load_extra_objbitmap("rbot063.bbm"))
	{
		con_printf(CON_NORMAL, "Can't load exit models!\n");
		return 0;
	}

	exit_hamfile = PHYSFSX_openReadBuffered("exit.ham");

	if (exit_hamfile) {
		exit_modelnum = N_polygon_models++;
		destroyed_exit_modelnum = N_polygon_models++;
		polymodel_read(&Polygon_models[exit_modelnum], exit_hamfile);
		polymodel_read(&Polygon_models[destroyed_exit_modelnum], exit_hamfile);
		Polygon_models[exit_modelnum].first_texture = start_num;
		Polygon_models[destroyed_exit_modelnum].first_texture = start_num+3;

		polygon_model_data_read(&Polygon_models[exit_modelnum], exit_hamfile);

		polygon_model_data_read(&Polygon_models[destroyed_exit_modelnum], exit_hamfile);

		PHYSFS_close(exit_hamfile);

	} else if (PHYSFSX_exists("exit01.pof",1) && PHYSFSX_exists("exit01d.pof",1)) {

		exit_modelnum = load_polygon_model("exit01.pof", 3, start_num, NULL);
		destroyed_exit_modelnum = load_polygon_model("exit01d.pof", 3, start_num + 3, NULL);

#ifdef OGL
		ogl_cache_polymodel_textures(exit_modelnum);
		ogl_cache_polymodel_textures(destroyed_exit_modelnum);
#endif
	}
	else if (PHYSFSX_exists(D1_PIGFILE,1))
	{
		int offset, offset2;
		int hamsize;

		exit_hamfile = PHYSFSX_openReadBuffered(D1_PIGFILE);
		hamsize = PHYSFS_fileLength(exit_hamfile);
		switch (hamsize) { //total hack for loading models
		case D1_PIGSIZE:
			offset = 91848;     /* and 92582  */
			offset2 = 383390;   /* and 394022 */
			break;
		default:
		case D1_SHARE_BIG_PIGSIZE:
		case D1_SHARE_10_PIGSIZE:
		case D1_SHARE_PIGSIZE:
		case D1_10_BIG_PIGSIZE:
		case D1_10_PIGSIZE:
			Int3();             /* exit models should be in .pofs */
		case D1_OEM_PIGSIZE:
		case D1_MAC_PIGSIZE:
		case D1_MAC_SHARE_PIGSIZE:
			con_printf(CON_NORMAL, "Can't load exit models!\n");
			return 0;
		}
		PHYSFSX_fseek(exit_hamfile, offset, SEEK_SET);
		exit_modelnum = N_polygon_models++;
		destroyed_exit_modelnum = N_polygon_models++;
		polymodel_read(&Polygon_models[exit_modelnum], exit_hamfile);
		polymodel_read(&Polygon_models[destroyed_exit_modelnum], exit_hamfile);
		Polygon_models[exit_modelnum].first_texture = start_num;
		Polygon_models[destroyed_exit_modelnum].first_texture = start_num+3;

		PHYSFSX_fseek(exit_hamfile, offset2, SEEK_SET);
		polygon_model_data_read(&Polygon_models[exit_modelnum], exit_hamfile);
		polygon_model_data_read(&Polygon_models[destroyed_exit_modelnum], exit_hamfile);

		PHYSFS_close(exit_hamfile);
	} else {
		con_printf(CON_NORMAL, "Can't load exit models!\n");
		return 0;
	}

	return 1;
}

void compute_average_rgb(grs_bitmap *bm, fix *rgb)
{
	ubyte *buf;
	int i, x, y, color;
	int count = 0;
	fix t_rgb[3] = { 0, 0, 0 };

	if (bm->bm_type == BM_RGBA) {
		rgb[0] = bm->avg_color_rgb[0];
		rgb[1] = bm->avg_color_rgb[1];
		rgb[2] = bm->avg_color_rgb[2];
		return;
	}
	rgb[0] = rgb[1] = rgb[2] = 0;

	if (!bm->bm_data)
		return;

	CALLOC(buf, ubyte, bm->bm_w*bm->bm_h);

	if (bm->bm_flags & BM_FLAG_RLE){
		unsigned char * dbits;
		unsigned char * sbits;
		int data_offset;

		data_offset = 1;
		if (bm->bm_flags & BM_FLAG_RLE_BIG)
			data_offset = 2;

		sbits = &bm->bm_data[4 + (bm->bm_h * data_offset)];
		dbits = buf;

		for (i=0; i < bm->bm_h; i++ )    {
			gr_rle_decode(sbits,dbits);
			if ( bm->bm_flags & BM_FLAG_RLE_BIG )
				sbits += (int)INTEL_SHORT(*((short *)&(bm->bm_data[4+(i*data_offset)])));
			else
				sbits += (int)bm->bm_data[4+i];
			dbits += bm->bm_w;
		}
	}
	else
	{
		memcpy(buf, bm->bm_data, sizeof(unsigned char)*(bm->bm_w*bm->bm_h));
	}

	i = 0;
	for (x = 0; x < bm->bm_h; x++)
	{
		for (y = 0; y < bm->bm_w; y++)
		{
			color = buf[i++];
			t_rgb[0] = gr_palette[color*3];
			t_rgb[1] = gr_palette[color*3+1];
			t_rgb[2] = gr_palette[color*3+2];
			if (!(color == TRANSPARENCY_COLOR || (t_rgb[0] == t_rgb[1] && t_rgb[0] == t_rgb[2])))
			{
				rgb[0] += t_rgb[0];
				rgb[1] += t_rgb[1];
				rgb[2] += t_rgb[2];
				count++;
			}
		}
	}
	d_free(buf);
}
