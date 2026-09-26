/* Complete Android asset-context resets; do not retain filename/index-only caches */
#include "android_mission_assets.h"
#include "android_log.h"
#include "args.h"
#include "bm.h"
#include "digi.h"
#include "dxxerror.h"
#include "gr.h"
#include "mission.h"
#include "newmenu.h"
#include "palette.h"
#include "piggy.h"
#include "songs.h"
#include "texmerge.h"
#ifdef OGL
#include "internal.h"
#include "xmodel.h"
#ifdef OGL_MERGE
extern void ogl_init_prog(void);
#endif
#endif
#ifdef DXX_BUILD_DESCENT_II
#include "d1_in_d2/d1_in_d2.h"
#endif

extern void piggy_android_reset_tables(void);
extern void clear_texture_lookup_cache(void);
#ifndef DXX_BUILD_DESCENT_II
extern void custom_remove(void);
extern void gamedata_android_reset_tbl(void);
#ifdef USE_SDLMIXER
extern void digi_mixer_free_cached_sounds(void);
#endif
#else
extern void free_bitmap_replacements(void);
#endif

void android_mission_asset_reset_before(void)
{
	songs_stop_all();
	digi_stop_digi_sounds();
#ifdef DXX_BUILD_DESCENT_II
	digi_free_cached_sounds();
	free_bitmap_replacements();
#else
#ifdef USE_SDLMIXER
	if (!GameArg.SndDisableSdlMixer) digi_mixer_free_cached_sounds();
#endif
	custom_remove();
#endif
	newmenu_free_background();
#ifdef OGL
	ogl_smash_texture_list_internal();
#ifdef OGL_MERGE
	/* Texture teardown also deletes merge shaders; restore them before rendering */
	if (gl_initialized) ogl_init_prog();
#endif
	xmodel_free_all();
#endif
	clear_texture_lookup_cache();
	texmerge_flush();
	gamedata_close();
#ifndef DXX_BUILD_DESCENT_II
	gamedata_android_reset_tbl();
#endif
	piggy_android_reset_tables();
}

void android_mission_asset_reset_baseline(void)
{
#ifdef DXX_BUILD_DESCENT_II
	debug_log_force(DLOG_GAME, "[ASSETS] baseline reset content=d%d mission='%s' native_d1=%d",
	                d1_in_d2_use_d1_gameplay() ? 1 : 2,
	                Current_mission ? Current_mission_filename : "", d1_in_d2_has_native_assets());
	d1_in_d2_restore_base_resources();
#else
	gamedata_init();
	gr_use_palette_table("palette.256");
#endif
}
