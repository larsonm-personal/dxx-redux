/*
 * D1 custom data support for D1 missions running in the D2 executable.
 */

#ifndef _D1_CUSTOM_H
#define _D1_CUSTOM_H

typedef struct d1_custom_texture_stats {
	int files_found;
	int bitmap_entries;
	int bitmap_applied;
	int bitmap_unresolved;
	int sound_entries;
	int sound_applied;
	int sound_unresolved;
	int base_sound_entries;
	int base_sound_applied;
	int base_sound_unresolved;
	int base_sound_skipped;
} d1_custom_texture_stats;

struct d1_asset_generation;
/* Applies optional PG1, DTX and HX1 assets only to an unpublished generation
 * On failure the caller must discard that generation; live assets are untouched */
int d1_custom_read_assets(struct d1_asset_generation *generation, const char *level_name, const char **error);
void d1_custom_get_stats(d1_custom_texture_stats *stats);

#endif
