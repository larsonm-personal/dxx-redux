/* Included by D2 piggy.c to install prepared pixels into its native registry */
#include "pog_resources.h"

static struct {
	int present, offset, flags;
	grs_bitmap bitmap;
} xl_pog_previous[MAX_BITMAP_FILES];

static void xl_pog_bank_will_change(void)
{
	int i;
	/* Restore and retire the old owner before the new PIG overwrites its slots */
	for (i = 0; i < MAX_BITMAP_FILES; ++i) {
		if (!xl_pog_previous[i].present) continue;
		free_bitmap_replacements();
		return;
	}
}

static void xl_pog_clear_installed(void)
{
	int i;
	for (i = 0; i < MAX_BITMAP_FILES; ++i) {
		if (!xl_pog_previous[i].present) continue;
		ogl_freebmtexture(&GameBitmaps[i]);
		GameBitmaps[i] = xl_pog_previous[i].bitmap;
		GameBitmapOffset[i] = xl_pog_previous[i].offset;
		GameBitmapFlags[i] = xl_pog_previous[i].flags;
		/* The paging arena may have been reused while a POG owned this slot */
		if (GameBitmapOffset[i]) {
			GameBitmaps[i].bm_data = Piggy_bitmap_cache_data;
			GameBitmaps[i].bm_flags = BM_FLAG_PAGED_OUT;
		}
		xl_pog_previous[i].present = 0;
	}
}

int xl_pog_install(struct xl_pog_resources *resources)
{
	int i;
	size_t size = 0;
	const unsigned char *source = xl_pog_pixels(resources, &size);
	unsigned char *installed = NULL;
	if (resources) {
		installed = d_malloc(size ? size : 1);
		if (!installed) return 0;
		if (size) memcpy(installed, source, size);
	}
	/* Preparation and allocation finish before any old resource is retired */
	free_bitmap_replacements();
	if (!resources) return 1;
	Bitmap_replacement_data = installed;
#ifdef __ANDROID__
	Bitmap_replacement_data_size = size;
#endif
	for (i = 0; i < xl_pog_count(resources); ++i) {
		const struct xl_pog_bitmap *entry = xl_pog_entry(resources, i);
		grs_bitmap *bitmap = &GameBitmaps[entry->index];
		if (!xl_pog_previous[entry->index].present) {
			xl_pog_previous[entry->index].present = 1;
			xl_pog_previous[entry->index].offset = GameBitmapOffset[entry->index];
			xl_pog_previous[entry->index].flags = GameBitmapFlags[entry->index];
			xl_pog_previous[entry->index].bitmap = *bitmap;
			xl_pog_previous[entry->index].bitmap.gltexture = NULL;
			xl_pog_previous[entry->index].bitmap.gltexture_mask = NULL;
		}
		ogl_freebmtexture(bitmap);
		gr_init_bitmap(bitmap, entry->rgba ? BM_RGBA : BM_LINEAR, 0, 0,
		               entry->width, entry->height, entry->width * (entry->rgba ? 4 : 1), installed + entry->offset);
		bitmap->avg_color = entry->rgba ? gr_find_closest_color(entry->average_rgb[0] / 4, entry->average_rgb[1] / 4, entry->average_rgb[2] / 4)
		                                : entry->average_index;
		if (entry->rgba) {
			int c;
			for (c = 0; c < 3; ++c) bitmap->avg_color_rgb[c] = (fix) ((unsigned) entry->average_rgb[c] * F1_0 / 255);
		}
		gr_set_bitmap_flags(bitmap, entry->flags);
		GameBitmapOffset[entry->index] = 0;
	}
	last_palette_loaded_pig[0] = 0;
	texmerge_flush();
	return 1;
}

int xl_pog_bitmap_receipt(const struct xl_pog_resources *resources, int index, unsigned *bytes, unsigned *fnv1a)
{
	int i;
	if (!resources || !bytes || !fnv1a || !Bitmap_replacement_data || index < 0 || index >= Num_bitmap_files) return 0;
	/* The last directory entry owns duplicate indices, as in LoadReplacementBitmaps */
	for (i = xl_pog_count(resources) - 1; i >= 0; --i) {
		const struct xl_pog_bitmap *entry = xl_pog_entry(resources, i);
		const grs_bitmap *bitmap = &GameBitmaps[index];
		size_t k;
		unsigned hash = 2166136261u;
		if (entry->index != (unsigned) index) continue;
		if (bitmap->bm_data != Bitmap_replacement_data + entry->offset || GameBitmapOffset[index] != 0 ||
		    bitmap->bm_type != (entry->rgba ? BM_RGBA : BM_LINEAR) || bitmap->bm_w != (int) entry->width || bitmap->bm_h != (int) entry->height)
			return 0;
		for (k = 0; k < entry->size; ++k) hash = (hash ^ bitmap->bm_data[k]) * 16777619u;
		*bytes = (unsigned) entry->size;
		*fnv1a = hash;
		return 1;
	}
	return 0;
}
