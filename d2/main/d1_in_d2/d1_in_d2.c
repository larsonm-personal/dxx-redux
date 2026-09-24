/* D1-in-D2 session and asset lifecycle */

#include "inferno.h"
#include "gr.h"
#include "mission.h"
#include "piggy.h"
#include "gamepal.h"
#include "palette.h"
#include "dxxerror.h"
#include "d1_custom.h"
#include "d1_in_d2.h"
#include "d1_in_d2_assets.h"
#include "d1_in_d2_presentation.h"
#include "d1_in_d2_cockpit.h"
#include "d1_in_d2_ai.h"
#ifdef OGL
#include "xmodel.h"
#endif

extern int Robot_replacements_loaded;
void piggy_init_pigfile(char *filename);

static int Startup_d1;
/* Zero means no definitions are installed, including before initial bootstrap
 * Only successful publication/load commits a profile; descriptors are requests */
static int Committed_game;

static int requested_d1(void)
{
	return Current_mission ? EMULATING_D1 : Startup_d1;
}

void d1_in_d2_release_assets(void)
{
	d1_in_d2_ai_reset_boss_state();
	d1_in_d2_close_cockpit();
	d1_in_d2_release_asset_data();
	Committed_game = 0;
}

int d1_in_d2_init_base_resources(int requested_version)
{
	if (requested_version == 1 || (!requested_version &&
		!PHYSFSX_exists("descent2.hog", 1) && !PHYSFSX_exists("d2demo.hog", 1))) {
		if (!PHYSFSX_exists(D1_PIGFILE, 1) || !PHYSFSX_contfile_init("descent.hog", 1))
			return 0;
		/* Preserve loose/mission precedence; presentation resolves base-game names
		 * explicitly even when another game's archive was mounted earlier */
		if (PHYSFSX_exists("descent2.hog", 1))
			PHYSFSX_contfile_init("descent2.hog", 1);
		else if (PHYSFSX_exists("d2demo.hog", 1))
			PHYSFSX_contfile_init("d2demo.hog", 1);
		Startup_d1 = 1;
		return 1;
	}
	if (!PHYSFSX_contfile_init("descent2.hog", 1) && !PHYSFSX_contfile_init("d2demo.hog", 1))
		return 0;
	Startup_d1 = 0;
	return 1;
}

int d1_in_d2_use_d1_gameplay(void)
{
	return Committed_game ? Committed_game == 1 : requested_d1();
}

int d1_in_d2_publish_assets(d1_asset_generation *generation, const char **error)
{
	if (!d1_in_d2_publish_asset_data(generation, error))
		return 0;
	Committed_game = 1;
	return 1;
}

void d1_in_d2_note_d2_base_loaded(void)
{
	Committed_game = 2;
}

char *d1_in_d2_startup_palette(void)
{
	return d1_in_d2_use_d1_gameplay() ? D1_DEFAULT_PALETTE : D2_DEFAULT_PALETTE;
}

void d1_in_d2_init_startup_bitmaps(void)
{
	if (!d1_in_d2_has_native_assets())
		piggy_init_pigfile("groupa.pig");
}

void d1_in_d2_restore_base_resources(void)
{
	char palette[FILENAME_LEN];
	/* The ordinary data initializer dispatches definition loading by content
	 * Keep bitmap/palette restoration beside that profile selection, rather
	 * than letting a platform reset infer content from its executable */
	gamedata_init();
	d1_in_d2_init_startup_bitmaps();
	last_palette_loaded[0] = last_palette_loaded_pig[0] = 0;
	/* load_palette's legacy path splitter temporarily writes into the name */
	strcpy(palette, d1_in_d2_use_d1_gameplay() ? D1_DEFAULT_PALETTE : DEFAULT_LEVEL_PALETTE);
	load_palette(palette, 1, 0);
	d1_in_d2_activate_presentation();
}

static int prepare_assets(const char *level_name)
{
	const char *error = NULL;
	d1_asset_generation *generation;

	if (!requested_d1())
		return 0;
	generation = d1_in_d2_read_assets(D1_PIGFILE, D1_DEFAULT_PALETTE, &error);
	if (!generation)
		Error("Cannot prepare D1 assets: invalid %s", error);
	if (level_name && !d1_custom_read_assets(generation, level_name, &error)) {
		d1_in_d2_free_assets(generation);
		Error("Cannot prepare D1 custom assets for %s: invalid %s", level_name, error);
	}
	d1_in_d2_prepare_available_guidebot(generation);
	if (!d1_in_d2_publish_assets(generation, &error)) {
		d1_in_d2_free_assets(generation);
		Error("Cannot publish D1 assets: invalid %s", error);
	}
	con_printf(CON_DEBUG, "Original D1 base assets initialized without D2 game data\n");
	d1_in_d2_activate_presentation();
	return 1;
}

int d1_in_d2_prepare_base_assets(void)
{
	return prepare_assets(NULL);
}

void d1_in_d2_prepare_intro_assets(void)
{
	/* Briefings precede LoadLevel and can render robots from the selected game
	 * Preserve selection-only deferral, then commit at actual intro entry */
	if (requested_d1()) {
		if (!d1_in_d2_has_native_assets())
			d1_in_d2_prepare_base_assets();
	} else if (d1_in_d2_has_native_assets())
		d1_in_d2_load_mission_assets();
}

int d1_in_d2_load_mission_assets(void)
{
	const int leaving_d1 = d1_in_d2_has_native_assets();
	int extra_robots;

	/* D1 base and custom definitions are published together at level load
	 * Selecting another D1 mission must not free the still-active models */
	if (requested_d1())
		return 0;
	if (leaving_d1) {
		char pigfile[] = "groupa.pig";
		digi_stop_digi_sounds();
		digi_free_cached_sounds();
#ifdef OGL
		xmodel_free_all();
#endif
		free_polygon_models();
		bm_free_extra_objbitmaps();
		gr_use_palette_table(DEFAULT_LEVEL_PALETTE);
		piggy_reset_asset_registry();
		piggy_init_pigfile(pigfile);
		Robot_replacements_loaded = 0;
	} else
		free_polygon_models();

	/* This service loads D2 definitions only; it never selects a profile or
	 * calls back into preparation, including when entered from level load */
	extra_robots = load_mission_ham();
#ifdef OGL
	if (leaving_d1)
		xmodel_load_all();
#endif
	d1_in_d2_note_d2_base_loaded();
	d1_in_d2_activate_presentation();
	return extra_robots;
}

int d1_in_d2_prepare_level_assets(char *level_name)
{
	if (!requested_d1()) {
		if (d1_in_d2_has_native_assets())
			d1_in_d2_load_mission_assets();
		return 0;
	}
	prepare_assets(level_name);
	Robot_replacements_loaded = 0;
	return 1;
}
