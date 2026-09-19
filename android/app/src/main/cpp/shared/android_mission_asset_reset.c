/* Complete Android asset-context resets; do not retain filename/index-only caches */
#include "android_mission_assets.h"
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
#include "d1_custom.h"
#include "gamepal.h"
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
extern void d1_in_d2_reset_asset_context(void);
extern void free_bitmap_replacements(void);
extern void piggy_init_pigfile(char *filename);
extern char last_palette_loaded[];
extern char last_palette_loaded_pig[];
#endif

void android_mission_asset_reset_before(void)
{
	songs_stop_all();
	digi_stop_digi_sounds();
#ifdef DXX_BUILD_DESCENT_II
	digi_free_cached_sounds();
	d1_custom_remove();
	d1_in_d2_reset_asset_context();
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
	gamedata_init();
#ifdef DXX_BUILD_DESCENT_II
	char pigfile[] = "groupa.pig";
	char palette[] = "groupa.256";
	last_palette_loaded[0] = last_palette_loaded_pig[0] = 0;
	piggy_init_pigfile(pigfile);
	load_palette(palette, 1, 0);
#else
	gr_use_palette_table("palette.256");
#endif
}
