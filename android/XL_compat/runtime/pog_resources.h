#ifndef DXX_XL_POG_RESOURCES_H
#define DXX_XL_POG_RESOURCES_H
#include "physfs.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
struct xl_pog_resources;
struct xl_pog_bitmap {
	unsigned index, width, height, flags, average_index;
	/* Keep field offsets identical under the engine's MSVC pack(1) and C++ defaults */
	unsigned char average_rgb[4];
	unsigned rgba, offset, size; /* The bounded owned pixel arena is at most 128 MiB */
};
/* Bounded preparation owns normalized pixels without changing live bitmaps */
struct xl_pog_resources *xl_pog_prepare(PHYSFS_file *file, unsigned max_bitmaps, char *error, size_t error_size);
void xl_pog_free(struct xl_pog_resources *resources);
int xl_pog_count(const struct xl_pog_resources *resources);
const struct xl_pog_bitmap *xl_pog_entry(const struct xl_pog_resources *resources, int row);
const unsigned char *xl_pog_pixels(const struct xl_pog_resources *resources, size_t *size);
int xl_pog_install(struct xl_pog_resources *resources);
/* Diagnostic receipt for the live installed payload, not source-file metadata */
int xl_pog_bitmap_receipt(const struct xl_pog_resources *resources, int bitmap, unsigned *bytes, unsigned *fnv1a);
#ifdef __cplusplus
}
#endif
#endif
