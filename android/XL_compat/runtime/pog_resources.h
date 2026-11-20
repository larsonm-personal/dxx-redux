#ifndef DXX_XL_POG_RESOURCES_H
#define DXX_XL_POG_RESOURCES_H
#include "physfs.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct xl_pog_resources;
struct xl_pog_bitmap {
	unsigned index, width, height, flags, average_index;
	/* Keep field offsets identical under the engine's MSVC pack(1) and C++ defaults */
	unsigned char average_rgb[4];
	unsigned rgba, offset, size; /* The bounded owned pixel arena is at most 128 MiB */
	unsigned palette_flags;
	unsigned frames, frame_height; /* Source strip retained; static native view begins at frame zero */
	unsigned rgb_opaque;
	unsigned physics_height; /* TGA selection may expose the whole strip to physics */
	unsigned scoped_texture;
	unsigned effect_clock; /* Source texture effect owns frame selection for this POG alias */
	unsigned side_frames;  /* Immutable frame views selected by an XL side owner */
	unsigned transparent_frames[4], super_frames[4];
};
struct xl_pog_effect {
	int effect, frame_time, object_texture;
	unsigned source_frames, frames;
};
size_t xl_pog_pixel_bytes(const struct xl_pog_resources *resources);
int xl_pog_effect_count(const struct xl_pog_resources *resources);
const struct xl_pog_effect *xl_pog_effect_entry(const struct xl_pog_resources *resources, int row);
int xl_pog_effect_bitmap(const struct xl_pog_resources *resources, int row, int frame);
int xl_pog_scoped_owner(const struct xl_pog_resources *resources);
int xl_pog_scope_matches_live(const struct xl_pog_resources *resources);
struct eclip;
int xl_pog_effect_frames(const struct eclip *effect);
/* Return the active replacement bitmap, or -1 for the ordinary mapping */
int xl_pog_apply_effect(int effect, int frame);
struct xl_pog_clip {
	int clip, bitmap, frames, frame_time, play_time;
};
int xl_pog_clip_count(const struct xl_pog_resources *resources);
const struct xl_pog_clip *xl_pog_clip_entry(const struct xl_pog_resources *resources, int row);
/* Zero means this clip retains ordinary native playback */
int xl_pog_vclip_frames(int clip);
/* Select a retained per-object strip frame; -1 means ordinary playback */
int xl_pog_vclip_bitmap(int clip, int frame);
int xl_pog_vclip_lifetime_frame(int clip, int timeleft);
struct xl_pog_pixel_view {
	const unsigned char *pixels;
	unsigned width, height, row_size, rgb_opaque;
};
/* Borrow the active native owner's physics view; cleared on retirement */
int xl_pog_physics_view(int bitmap, struct xl_pog_pixel_view *view);
/* Exact source whole-image average, independent of the selected display frame */
int xl_pog_average_rgb(int bitmap, unsigned char rgb[3]);
/* Bounded preparation owns normalized pixels without changing live bitmaps */
struct xl_pog_resources *xl_pog_prepare(PHYSFS_file *file, unsigned max_bitmaps, char *error, size_t error_size);
void xl_pog_free(struct xl_pog_resources *resources);
int xl_pog_count(const struct xl_pog_resources *resources);
const struct xl_pog_bitmap *xl_pog_entry(const struct xl_pog_resources *resources, int row);
const unsigned char *xl_pog_pixels(const struct xl_pog_resources *resources, size_t *size);
int xl_pog_install(struct xl_pog_resources *resources);
/* Native storage is allocated before publication and transfers only once */
int xl_pog_prepare_storage(struct xl_pog_resources *resources);
unsigned char *xl_pog_take_storage(struct xl_pog_resources *resources, size_t *size);
/* Native bitmap layout and GPU handles remain owned by the native registry */
void *xl_pog_prepare_side_views(const struct xl_pog_resources *resources);
void xl_pog_free_side_views(void *views);
void *xl_pog_take_side_views(struct xl_pog_resources *resources);
struct _grs_bitmap;
struct _grs_bitmap *xl_pog_side_frame(int bitmap, int frame);
int xl_pog_side_frame_count(int bitmap);
void xl_pog_side_views_discard_context(void);
/* Zero retains the authored native wall clip clock */
int xl_pog_door_frames(int clip);
int xl_pog_prepared_door_frames(const struct xl_pog_resources *resources, int clip);
uint64_t xl_pog_owner_token(const struct xl_pog_resources *resources);
/* Repeating publication is allocation-free while this owner remains installed */
int xl_pog_install_prepared(struct xl_pog_resources *resources);
/* 0: unclocked static strip; 1: live animation clock; -1: invalid native table bounds */
int xl_pog_strip_clock(unsigned bitmap);
int xl_pog_non_effect_clock(unsigned bitmap);
/* Diagnostic receipt for the live installed payload, not source-file metadata */
int xl_pog_bitmap_receipt(const struct xl_pog_resources *resources, int bitmap, unsigned *bytes, unsigned *fnv1a);
#ifdef __cplusplus
}
#include <vector>
namespace xl_compat
{
struct PogBitmap;
struct ScopedTextureResources;
} // namespace xl_compat
// Decode once through the existing image codec; never synthesize a POG file
xl_pog_resources *xl_pog_prepare_rgba(const std::vector<xl_compat::PogBitmap> &images, unsigned max_bitmaps, bool scoped_texture = false);
namespace xl_assets
{
struct ObjectDependencies;
}
int xl_pog_target_strip_clock(unsigned bitmap, const xl_assets::ObjectDependencies &tables);
void xl_pog_bind_texture_effects(xl_pog_resources &resources, const xl_assets::ObjectDependencies &tables, int bitmap_count);
xl_pog_resources *xl_pog_prepare_scoped(const xl_compat::ScopedTextureResources &textures,
                                        const xl_assets::ObjectDependencies &tables, const xl_pog_resources *pog);
#endif
#endif
