/* D1 bitmap ownership and lifecycle integration */
#ifndef D1_IN_D2_BITMAPS_H
#define D1_IN_D2_BITMAPS_H

#include "gr.h"

/* Source indices are retained, with index zero reserved for the engine fallback
 * A prepared generation owns its storage and does not mutate live engine tables */
typedef struct d1_bitmap_generation {
	int bitmap_count;
	grs_bitmap *bitmaps;
	char (*names)[13];
	ubyte palette[256 * 3];
	ubyte fade_table[256 * GR_FADE_LEVELS];
} d1_bitmap_generation;

d1_bitmap_generation *d1_in_d2_read_bitmaps(const char *pig_name, const char *palette_name);
/* Private optional-content reader; selected uses source bitmap IDs, retains
 * source palette indices, and leaves unselected entries empty */
d1_bitmap_generation *d1_in_d2_read_feature_bitmaps(const char *pig_name, const char *palette_name,
	const ubyte *selected, int selected_count);
int d1_in_d2_remap_feature_bitmaps(d1_bitmap_generation *images, const ubyte *palette);
void d1_in_d2_free_bitmaps(d1_bitmap_generation *generation);
void d1_in_d2_reset_bitmap_replacements(void);

#endif
