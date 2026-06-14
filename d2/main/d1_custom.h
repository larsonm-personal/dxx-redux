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
} d1_custom_texture_stats;

void d1_custom_load_data(char *level_name);
void d1_custom_remove(void);
void d1_custom_get_stats(d1_custom_texture_stats *stats);

#endif
