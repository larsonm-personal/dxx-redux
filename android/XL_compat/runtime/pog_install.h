/* Included by D2 piggy.c to install prepared pixels into its native registry */
#include "pog_resources.h"
#include "vclip.h"
#include "wall.h"
#include "effects.h"
#include "../xl_texture_frame.h"
#include "../xl_vclip_frame.h"

static struct {
	int present, offset, flags;
	grs_bitmap bitmap;
	struct xl_pog_pixel_view physics;
	unsigned char average_rgb[3];
	unsigned frame_bytes, frames, current_frame;
	unsigned transparent_frames[4], super_frames[4];
} xl_pog_previous[MAX_BITMAP_FILES];
static uint64_t xl_pog_active_token;
static struct {
	unsigned frames, count;
	int bitmaps[VCLIP_MAX_FRAMES];
} xl_pog_live_effects[MAX_EFFECTS];
static struct xl_pog_clip xl_pog_live_clips[VCLIP_MAXNUM];

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
	xl_pog_active_token = 0;
	memset(xl_pog_live_effects, 0, sizeof(xl_pog_live_effects));
	memset(xl_pog_live_clips, 0, sizeof(xl_pog_live_clips));
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
		memset(&xl_pog_previous[i].physics, 0, sizeof(xl_pog_previous[i].physics));
	}
}

int xl_pog_install_prepared(struct xl_pog_resources *resources)
{
	int i;
	size_t size = 0;
	unsigned char *installed = NULL;
	if (resources) {
		if (!xl_pog_scope_matches_live(resources)) return 0;
		for (i = 0; i < xl_pog_count(resources); ++i) {
			const struct xl_pog_bitmap *entry = xl_pog_entry(resources, i);
			if (entry->index >= (unsigned) Num_bitmap_files) return 0;
			if (entry->frames > 1 && !(entry->scoped_texture && xl_pog_scoped_owner(resources)) && xl_pog_strip_clock(entry->index)) return 0;
		}
		installed = xl_pog_take_storage(resources, &size);
		if (!installed) return xl_pog_active_token == xl_pog_owner_token(resources);
	}
	/* Preparation and allocation finish before any old resource is retired */
	free_bitmap_replacements();
	if (!resources) return 1;
	Bitmap_replacement_data = installed;
	xl_pog_active_token = xl_pog_owner_token(resources);
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
		               entry->width, entry->frame_height, entry->width * (entry->rgba ? 4 : 1), installed + entry->offset);
		bitmap->bm_rgb_opaque = entry->rgb_opaque != 0;
		xl_pog_previous[entry->index].frame_bytes = entry->size / entry->frames;
		xl_pog_previous[entry->index].frames = entry->frames;
		xl_pog_previous[entry->index].current_frame = 0;
		memcpy(xl_pog_previous[entry->index].transparent_frames, entry->transparent_frames, sizeof(entry->transparent_frames));
		memcpy(xl_pog_previous[entry->index].super_frames, entry->super_frames, sizeof(entry->super_frames));
		if (entry->rgba) {
			memcpy(xl_pog_previous[entry->index].average_rgb, entry->average_rgb, 3);
			xl_pog_previous[entry->index].physics.pixels = installed + entry->offset;
			xl_pog_previous[entry->index].physics.width = entry->width;
			xl_pog_previous[entry->index].physics.height = entry->physics_height;
			xl_pog_previous[entry->index].physics.row_size = entry->width * 4;
			xl_pog_previous[entry->index].physics.rgb_opaque = entry->rgb_opaque;
		} else memset(&xl_pog_previous[entry->index].physics, 0, sizeof(xl_pog_previous[entry->index].physics));
		bitmap->avg_color = entry->rgba ? gr_find_closest_color(entry->average_rgb[0] / 4, entry->average_rgb[1] / 4, entry->average_rgb[2] / 4)
		                                : entry->average_index;
		if (entry->rgba) {
			int c;
			for (c = 0; c < 3; ++c) bitmap->avg_color_rgb[c] = (fix) ((unsigned) entry->average_rgb[c] * F1_0 / 255);
		}
		gr_set_bitmap_flags(bitmap, entry->flags);
		if (entry->scoped_texture && entry->frames > 1)
			gr_set_bitmap_flags(bitmap, dxx_xl_texture_frame_flags(entry->transparent_frames, entry->super_frames, 0));
		if (!entry->rgba) {
			/* XL LoadReplacementBitmaps expands indexed pixels then SetPalette
			 * recounts key colors. A stale transparent header must not queue an
			 * opaque wall into the depth-write-disabled transparency pass. */
			unsigned mask = BM_FLAG_TRANSPARENT;
			int texture;
			for (texture = 0; texture < NumTextures; ++texture)
				if (Textures[texture].index == entry->index) {
					/* Source SetSuperTransparent changes only wall texture bitmaps. */
					mask |= BM_FLAG_SUPER_TRANSPARENT;
					break;
				}
			gr_set_bitmap_flags(bitmap, (entry->flags & ~mask) | (entry->palette_flags & mask));
		}
		GameBitmapOffset[entry->index] = 0;
	}
	for (i = 0; i < xl_pog_effect_count(resources); ++i) {
		const struct xl_pog_effect *clock = xl_pog_effect_entry(resources, i);
		unsigned frame;
		xl_pog_live_effects[clock->effect].frames = clock->frames;
		xl_pog_live_effects[clock->effect].count = clock->source_frames;
		for (frame = 0; frame < clock->source_frames; ++frame)
			xl_pog_live_effects[clock->effect].bitmaps[frame] = xl_pog_effect_bitmap(resources, i, frame);
	}
	for (i = 0; i < xl_pog_clip_count(resources); ++i) {
		const struct xl_pog_clip *clip = xl_pog_clip_entry(resources, i);
		xl_pog_live_clips[clip->clip] = *clip;
	}
	last_palette_loaded_pig[0] = 0;
	texmerge_flush();
	return 1;
}

int xl_pog_install(struct xl_pog_resources *resources)
{
	return xl_pog_prepare_storage(resources) && xl_pog_install_prepared(resources);
}

int xl_pog_physics_view(int bitmap, struct xl_pog_pixel_view *view)
{
	if (!view || !xl_pog_active_token || !Bitmap_replacement_data || bitmap < 0 || bitmap >= Num_bitmap_files ||
	    !xl_pog_previous[bitmap].present || !xl_pog_previous[bitmap].physics.pixels) return 0;
	*view = xl_pog_previous[bitmap].physics;
	return 1;
}

int xl_pog_average_rgb(int bitmap, unsigned char rgb[3])
{
	struct xl_pog_pixel_view view;
	if (!rgb || !xl_pog_physics_view(bitmap, &view)) return 0;
	memcpy(rgb, xl_pog_previous[bitmap].average_rgb, 3);
	return 1;
}

int xl_pog_effect_frames(const struct eclip *effect)
{
	int i;
	if (xl_pog_active_token)
		for (i = 0; i < Num_effects; ++i)
			if (effect == &Effects[i] && xl_pog_live_effects[i].frames)
				return (int) xl_pog_live_effects[i].frames;
	return effect->vc.num_frames;
}

static void xl_pog_select_bitmap_frame(int bitmap, unsigned frame)
{
	if (xl_pog_previous[bitmap].current_frame == frame) return;
	ogl_freebmtexture(&GameBitmaps[bitmap]);
	GameBitmaps[bitmap].bm_data = (unsigned char *) xl_pog_previous[bitmap].physics.pixels +
	                              xl_pog_previous[bitmap].frame_bytes * frame;
	xl_pog_previous[bitmap].current_frame = frame;
	gr_set_bitmap_flags(&GameBitmaps[bitmap], dxx_xl_texture_frame_flags(
	                                              xl_pog_previous[bitmap].transparent_frames, xl_pog_previous[bitmap].super_frames, frame));
}

int xl_pog_vclip_frames(int clip)
{
	return xl_pog_active_token && clip >= 0 && clip < Num_vclips ? xl_pog_live_clips[clip].frames : 0;
}

int xl_pog_vclip_bitmap(int clip, int frame)
{
	int bitmap;
	if (!xl_pog_vclip_frames(clip) || frame < 0 || frame >= xl_pog_live_clips[clip].frames) return -1;
	bitmap = xl_pog_live_clips[clip].bitmap;
	xl_pog_select_bitmap_frame(bitmap, (unsigned) frame);
	return bitmap;
}

int xl_pog_vclip_lifetime_frame(int clip, int timeleft)
{
	const struct xl_pog_clip *animation;
	if (!xl_pog_vclip_frames(clip)) return -1;
	animation = &xl_pog_live_clips[clip];
	return dxx_xl_vclip_lifetime_frame(animation->play_time, animation->frame_time, animation->frames, timeleft);
}

int xl_pog_apply_effect(int effect, int frame)
{
	unsigned i;
	if (!xl_pog_active_token || effect < 0 || effect >= Num_effects || !xl_pog_live_effects[effect].frames) return -1;
	if (frame < 0) frame = 0;
	frame %= (int) xl_pog_live_effects[effect].frames;
	for (i = 0; i < xl_pog_live_effects[effect].count; ++i) {
		const int bitmap = xl_pog_live_effects[effect].bitmaps[i];
		xl_pog_select_bitmap_frame(bitmap, (unsigned) frame);
	}
	return xl_pog_live_effects[effect].bitmaps[0];
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
		if (bitmap->bm_data != Bitmap_replacement_data + entry->offset + (entry->size / entry->frames) * xl_pog_previous[index].current_frame || GameBitmapOffset[index] != 0 ||
		    bitmap->bm_rgb_opaque != (entry->rgb_opaque != 0) ||
		    bitmap->bm_type != (entry->rgba ? BM_RGBA : BM_LINEAR) || bitmap->bm_w != (int) entry->width || bitmap->bm_h != (int) entry->frame_height)
			return 0;
		for (k = 0; k < entry->size / entry->frames; ++k) hash = (hash ^ bitmap->bm_data[k]) * 16777619u;
		*bytes = (unsigned) (entry->size / entry->frames);
		*fnv1a = hash;
		return 1;
	}
	return 0;
}
