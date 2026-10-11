#ifdef ANDROID

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <limits.h>

#include <android/log.h>

#include "android_log.h"
#include "console.h"
#include "ogl_texture_android.h"
#include "ogl_texture_filename.h"
#include "pngfile.h"
#include "physfs.h"
#include "android_texture_usage.h"
#include "piggy.h"
#include "rle.h"
#include "textures.h"
#include "vclip.h"

static ogl_texture transient_blit_texture;

static int replacement_sprite_additive(const char *image, const grs_bitmap *bitmap)
{
	char marker[272], value[9];
	const char *owner = android_texture_source(image);
	const char *marker_owner;
	int sprite = 0;
	int clip_count = Num_vclips;
#ifndef DXX_BUILD_DESCENT_II
	/* Retail D1 stores zero here but loads the full fixed-size vclip table */
	if (!clip_count) clip_count = VCLIP_MAXNUM;
#endif
	if (!owner || bitmap < GameBitmaps || bitmap >= &GameBitmaps[Num_bitmap_files]) return 0;
	const int index = (int) (bitmap - GameBitmaps);
	/* A material hint must never make an opaque wall replacement eligible */
	for (int i = 0; i < NumTextures && i < MAX_TEXTURES; ++i)
		if (Textures[i].index == index) return 0;
	for (int i = 0; i < clip_count && i < VCLIP_MAXNUM; ++i) {
		for (int f = 0; f < Vclip[i].num_frames && f < VCLIP_MAX_FRAMES; ++f) {
			if (Vclip[i].frames[f].index != index) continue;
			if (Vclip[i].flags & VF_ROD) return 0;
			sprite = 1;
		}
	}
	if (!sprite || snprintf(marker, sizeof(marker), "%s.blend", image) >= (int) sizeof(marker)) return 0;
	marker_owner = android_texture_source(marker);
	if (!marker_owner || strcmp(owner, marker_owner)) return 0;
	PHYSFS_File *file = PHYSFS_openRead(marker);
	if (!file) return 0;
	const int valid = PHYSFS_fileLength(file) == 9 && PHYSFS_readBytes(file, value, sizeof(value)) == sizeof(value) &&
	                  !memcmp(value, "additive\n", sizeof(value));
	PHYSFS_close(file);
	return valid;
}

int android_ogl_begin_sprite_blend(ogl_texture *texture)
{
	if (!texture || !texture->sprite_additive) return -1;
	if (texture->additive_draws < UINT_MAX) ++texture->additive_draws;
	const int previous = grd_curcanv->cv_blend_func;
	gr_settransblend(grd_curcanv->cv_fade_level, GR_BLEND_ADDITIVE_A);
	return previous;
}

void android_ogl_end_sprite_blend(int previous)
{
	if (previous >= 0) gr_settransblend(grd_curcanv->cv_fade_level, previous);
}

static int bitmap_needs_super_mask(grs_bitmap *bitmap)
{
	/* Several stock doors retain the super flag despite containing no index 254 */
	if (!(piggy_bitmap_get_flags(bitmap) & BM_FLAG_SUPER_TRANSPARENT)) return 0;
	if (bitmap->bm_flags & BM_FLAG_PAGED_OUT) {
		if (bitmap < GameBitmaps || bitmap >= &GameBitmaps[Num_bitmap_files]) return 1;
		bitmap_index index = { (ushort) (bitmap - GameBitmaps) };
		PIGGY_PAGE_IN(index);
	}
	if (!bitmap->bm_data) return 1;
	if (bitmap->bm_flags & BM_FLAG_RLE) {
		if (bitmap->bm_w != 64 || bitmap->bm_h != 64 || (bitmap->bm_flags & BM_FLAG_RLE_BIG)) return 1;
		bitmap = rle_expand_texture(bitmap);
	}
	for (int y = 0; y < bitmap->bm_h; ++y)
		if (memchr(bitmap->bm_data + y * bitmap->bm_rowsize, 254, bitmap->bm_w)) return 1;
	return 0;
}

static int replacement_mask_name(const char *image, char *mask, size_t capacity)
{
	const char *dot = strrchr(image, '.');
	const char *owner = android_texture_source(image);
	const char *mask_owner;
	if (!dot || !owner || (size_t) (dot - image) + sizeof("_mask.png") > capacity) return 0;
	memcpy(mask, image, dot - image);
	strcpy(mask + (dot - image), "_mask.png");
	mask_owner = android_texture_source(mask);
	return mask_owner && !strcmp(owner, mask_owner);
}

static void android_ogl_texture_clock_now(struct timespec *ts)
{
	clock_gettime(CLOCK_MONOTONIC, ts);
}

static int android_ogl_texture_elapsed_us(const struct timespec *start,
                                          const struct timespec *end)
{
	return (int) ((end->tv_sec - start->tv_sec) * 1000000 +
	              (end->tv_nsec - start->tv_nsec) / 1000);
}

int android_ogl_read_texture_with_extensions(
    char *filename, const char *basename, png_data *pdata,
    struct android_profile_texture_lookup_metrics *lookup, int slot,
    size_t filename_capacity)
{
	static const char *exts[] = { ".png", ".jpg", ".tga" };
	int ei;

	if (lookup)
		lookup->png_attempts++;

	for (ei = 0; ei < 3; ei++) {
		struct timespec stage_start, stage_end;
		int loaded;
		long long elapsed_us;

		if (!android_ogl_texture_filename(filename, filename_capacity, basename,
		                                  exts[ei]))
			continue;
		android_ogl_texture_clock_now(&stage_start);
		loaded = read_png(filename, pdata);
		android_ogl_texture_clock_now(&stage_end);
		elapsed_us = android_ogl_texture_elapsed_us(&stage_start, &stage_end);
		if (lookup) {
			if (slot >= 0 && slot < ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_COUNT)
				lookup->png_slot_us[slot] += elapsed_us;
			if (ei < ANDROID_PROFILE_TEXTURE_LOOKUP_EXT_COUNT)
				lookup->png_ext_us[ei] += elapsed_us;
			if (loaded) {
				lookup->png_hit_slot = slot;
				lookup->png_hit_ext = ei;
			}
		}
		if (loaded)
			return 1;
	}
	return 0;
}

int android_ogl_read_replacement(char *filename, size_t capacity,
                                 const char *const *basenames, const int *slots, int count, int allow_ktx2,
                                 grs_bitmap *bitmap, png_data *png, etc2_file_data *ktx,
                                 struct android_profile_texture_lookup_metrics *lookup,
                                 long long *ktx_us, long long *png_us)
{
	static const char *extensions[] = { ".ktx2", ".png", ".jpg", ".tga" };
	struct candidate {
		char path[256];
		int rank, slot, extension;
	} candidates[12];
	int used = 0;
	const int bitmap_flags = piggy_bitmap_get_flags(bitmap);
	char **search = PHYSFS_getSearchPath();
	if (!search || count < 0 || count > 3) {
		PHYSFS_freeList(search);
		return 0;
	}
	for (int base = 0; base < count; ++base) {
		if (!basenames[base]) continue;
		for (int ext = allow_ktx2 ? 0 : 1; ext < 4; ++ext) {
			struct candidate item;
			if (!android_ogl_texture_filename(item.path, sizeof(item.path), basenames[base], extensions[ext])) continue;
			const char *owner = android_texture_source(item.path);
			if (!owner) continue;
			for (item.rank = 0; search[item.rank] && strcmp(search[item.rank], owner); ++item.rank) {}
			if (!search[item.rank]) continue;
			item.slot = slots[base];
			item.extension = ext;
			int position = used++;
			/* Stable insertion retains basename and extension order within an archive */
			while (position > 0 && candidates[position - 1].rank > item.rank) {
				candidates[position] = candidates[position - 1];
				--position;
			}
			candidates[position] = item;
		}
	}
	PHYSFS_freeList(search);
	for (int i = 0; i < used; ++i) {
		const struct candidate *item = &candidates[i];
		struct timespec start, end;
		int loaded;
		long long elapsed;
		if (strlen(item->path) >= capacity) continue;
		if (bitmap_flags & BM_FLAG_SUPER_TRANSPARENT) {
			char mask[256];
			if (!replacement_mask_name(item->path, mask, sizeof(mask)) && bitmap_needs_super_mask(bitmap)) {
				debug_log(DLOG_TEXTURE, "Replacement skipped: %s needs a mask from the same source", item->path);
				continue;
			}
		}
		android_ogl_texture_clock_now(&start);
		loaded = item->extension == 0 ? read_ktx2_file(item->path, ktx) : read_png(item->path, png);
		android_ogl_texture_clock_now(&end);
		elapsed = android_ogl_texture_elapsed_us(&start, &end);
		if (item->extension == 0) {
			if (loaded) ktx->sprite_additive = replacement_sprite_additive(item->path, bitmap);
			*ktx_us += elapsed;
			android_profile_texture_lookup_note_ktx2(lookup, item->slot, elapsed, loaded);
			if (loaded && (bitmap_flags & (BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT)) && !ktx->format && !ktx->sprite_additive) {
				free(ktx->filedata);
				loaded = 0;
			}
		} else {
			*png_us += elapsed;
			lookup->png_attempts++;
			lookup->png_slot_us[item->slot] += elapsed;
			lookup->png_ext_us[item->extension - 1] += elapsed;
			if (loaded && (bitmap_flags & (BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT)) && !png->alpha) {
				free(png->data);
				free(png->palette);
				loaded = 0;
			}
			if (loaded) {
				lookup->png_hit_slot = item->slot;
				lookup->png_hit_ext = item->extension - 1;
			}
		}
		if (!loaded) continue;
		strcpy(filename, item->path);
		return item->extension == 0 ? 2 : 1;
	}
	return 0;
}

static void apply_bound_min_mag_filter(ogl_texture *texture, GLenum min_filter,
                                       GLenum mag_filter)
{
	if (texture->min_filter != min_filter) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
		texture->min_filter = min_filter;
	}
	if (texture->mag_filter != mag_filter) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
		texture->mag_filter = mag_filter;
	}
}

int android_ogl_upload_ktx2(ogl_texture *texture, const etc2_file_data *data,
                            int texfilt, int max_size, const char *filename)
{
	const unsigned char *levels[15];
	unsigned int sizes[15];
	size_t offset = 0;
	unsigned int width, height, first = 0, level, bytes = 0;
	GLint hardware_limit = 0;
	GLenum error, format;
	if (!texture || !data || !data->filedata || !data->width || !data->height ||
	    !data->mip_count || data->mip_count > 15 || data->format > 1)
		return 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &hardware_limit);
	if (max_size <= 0 || max_size > hardware_limit) max_size = hardware_limit;
	if (max_size <= 0) return 0;
	width = data->width;
	height = data->height;
	/* Validate the entire chain before allocating a GL object */
	for (level = 0; level < data->mip_count; ++level) {
		const unsigned char *p;
		size_t expected = ((size_t) width + 3) / 4 * (((size_t) height + 3) / 4) * (data->format ? 16 : 8);
		if (offset > data->filedata_size || data->filedata_size - offset < 4) return 0;
		p = data->filedata + offset;
		sizes[level] = (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
		offset += 4;
		if (sizes[level] != expected || sizes[level] > data->filedata_size - offset) return 0;
		levels[level] = data->filedata + offset;
		offset += sizes[level];
		if (width > (unsigned int) max_size || height > (unsigned int) max_size) first = level + 1;
		if (width == 1 && height == 1 && level + 1 < data->mip_count) return 0;
		width = width > 1 ? width / 2 : 1;
		height = height > 1 ? height / 2 : 1;
	}
	if (first >= data->mip_count || offset != data->filedata_size) return 0;
	width = data->width >> first;
	height = data->height >> first;
	if (!width) width = 1;
	if (!height) height = 1;
	texture->w = texture->tw = texture->lw = (int) width;
	texture->h = texture->th = (int) height;
	texture->u = (float) data->orig_width / (float) data->width;
	texture->v = (float) data->orig_height / (float) data->height;
	texture->has_mipmaps = 0;
	texture->authored_mip_count = 0;
	texture->mip_base_level = (int) first;
	texture->min_filter = texture->mag_filter = 0;
	texture->sprite_additive = data->sprite_additive;
	texture->additive_draws = 0;
	format = data->format ? GL_COMPRESSED_RGBA8_ETC2_EAC : GL_COMPRESSED_RGB8_ETC2;
	while (glGetError() != GL_NO_ERROR) {}
	glGenTextures(1, &texture->handle);
	glBindTexture(GL_TEXTURE_2D, texture->handle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, data->mip_count - first - 1);
	for (level = first; level < data->mip_count; ++level) {
		glCompressedTexImage2D(GL_TEXTURE_2D, (GLint) (level - first), format,
		                       (GLsizei) width, (GLsizei) height, 0, (GLsizei) sizes[level], levels[level]);
		error = glGetError();
		if (error != GL_NO_ERROR) {
			debug_log(DLOG_TEXTURE, "KTX2 upload failed: %s level=%u error=0x%x", filename, level, error);
			glDeleteTextures(1, &texture->handle);
			texture->handle = 0;
			return 0;
		}
		bytes += sizes[level];
		width = width > 1 ? width / 2 : 1;
		height = height > 1 ? height / 2 : 1;
	}
	texture->authored_mip_count = data->mip_count - first;
	texture->has_mipmaps = texture->authored_mip_count > 1;
	texture->bytes = texture->bytesu = (int) bytes;
	apply_bound_min_mag_filter(texture, !texfilt ? GL_NEAREST : !texture->has_mipmaps ? GL_LINEAR
	                                                        : texfilt >= 2            ? GL_LINEAR_MIPMAP_LINEAR
	                                                                                  : GL_LINEAR_MIPMAP_NEAREST,
	                           texfilt ? GL_LINEAR : GL_NEAREST);
	if (ogl_maxanisotropy > 1.0f)
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
		                ogl_aniso_level > 1 ? (GLfloat) (ogl_aniso_level < ogl_maxanisotropy ? ogl_aniso_level : (int) ogl_maxanisotropy) : 1.0f);
	debug_log(DLOG_TEXTURE, "KTX2 uploaded: %s size=%dx%d levels=%d source_base=%u bytes=%u",
	          filename, texture->w, texture->h, texture->authored_mip_count, first, bytes);
	return 1;
}

void android_ogl_bind_texture_2d(const struct android_ogl_bind_texture_state *state,
                                 GLuint handle)
{
	if (!state) {
		glBindTexture(GL_TEXTURE_2D, handle);
		return;
	}
	if (gles3_shim_bind_texture_2d_cached(handle)) {
		if (state->texbind_reuse)
			(*state->texbind_reuse)++;
	} else if (state->texbinds) {
		(*state->texbinds)++;
	}
}

void android_ogl_active_texture(const struct android_ogl_bind_texture_state *state,
                                GLenum texture)
{
	(void) state;
	glActiveTexture(texture);
}

void android_ogl_reset_texture_bindings(const struct android_ogl_bind_texture_state *state)
{
	(void) state;
	gles3_shim_reset_texture_bindings();
}

void android_ogl_enable_texture_2d(int *texture_2d_enabled)
{
	if (!texture_2d_enabled || *texture_2d_enabled != 1) {
		glEnable(GL_TEXTURE_2D);
		if (texture_2d_enabled)
			*texture_2d_enabled = 1;
	}
}

int android_ogl_effective_texfilt(int texfilt, int aniso_level)
{
	/* AF requires mipmap filtering to have any effect on real hardware.
	 * If AF is on but texfilt is too low, upgrade to trilinear. */
	if (aniso_level > 0 && texfilt < 2)
		return 2;
	return texfilt;
}

void android_ogl_apply_bound_texture_filter(ogl_texture *texture, int effective_texfilt,
                                            int menu_texfilt, int hud_texfilt,
                                            int render_context)
{
	int use_nearest = 0;

	if (!texture || effective_texfilt <= 0)
		return;

	/* Font/text textures never have mipmaps and always use MenuTexFilt.
	 * Fullscreen menu and loading art can also arrive without mipmaps on
	 * Android, notably through the ETC2/KTX path. */
	if (texture->flags & OGL_FLAG_NOCOLOR) {
		apply_bound_min_mag_filter(texture,
		                           menu_texfilt ? GL_LINEAR : GL_NEAREST,
		                           menu_texfilt ? GL_LINEAR : GL_NEAREST);
		return;
	}

	if (render_context == 0 && !menu_texfilt)
		use_nearest = 1;
	else if (render_context == 2 && !hud_texfilt)
		use_nearest = 1;

	if (use_nearest) {
		apply_bound_min_mag_filter(texture, GL_NEAREST, GL_NEAREST);
	} else if (texture->has_mipmaps) {
		GLenum min_f = effective_texfilt >= 2
		                   ? GL_LINEAR_MIPMAP_LINEAR
		                   : GL_LINEAR_MIPMAP_NEAREST;

		apply_bound_min_mag_filter(texture, min_f, GL_LINEAR);
	} else {
		apply_bound_min_mag_filter(texture, GL_LINEAR, GL_LINEAR);
	}
}

int android_ogl_get_texture_bytes(const struct android_ogl_texture_list_state *state)
{
	int total = transient_blit_texture.handle ? transient_blit_texture.bytes : 0;
	int i;

	if (!state || !state->texture_list || state->texture_list_size <= 0)
		return 0;

	for (i = 0; i < state->texture_list_size; i++)
		if (state->texture_list[i].handle > 0)
			total += state->texture_list[i].bytes;

	return total;
}

void android_ogl_apply_anisotropy_all(struct android_ogl_texture_anisotropy_state *state)
{
	int i;
	int count = 0;
	int total = 0;
	GLfloat level = 1.0f;

	if (!state || !state->texture_list_state.texture_list ||
	    state->texture_list_state.texture_list_size <= 0 ||
	    state->maxanisotropy <= 1.0f)
		return;

	level = (state->aniso_level > 1 && state->maxanisotropy > 1.0f)
	            ? (GLfloat) (state->aniso_level < state->maxanisotropy ? state->aniso_level : (int) state->maxanisotropy)
	            : 1.0f;
	for (i = 0; i < state->texture_list_state.texture_list_size; i++) {
		ogl_texture *texture = &state->texture_list_state.texture_list[i];

		if (texture->handle > 0 && texture->has_mipmaps) {
			glBindTexture(GL_TEXTURE_2D, texture->handle);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, level);
			count++;
		}
		if (texture->handle > 0)
			total++;
	}
	__android_log_print(ANDROID_LOG_INFO, "DXX",
	                    "anisotropy: applied level %.0f to %d/%d mipmapped textures", level, count, total);
	debug_log(DLOG_GRAPHICS,
	          "anisotropy parameter: level=%.0f applied=%d total=%d",
	          level, count, total);
}

static int apply_texture_filter(ogl_texture *texture, int texfilt, int *generated)
{
	if (!texture || texture->handle <= 0)
		return 0;
	if (texture->flags & OGL_FLAG_NOCOLOR)
		return 0;

	glBindTexture(GL_TEXTURE_2D, texture->handle);
	if (texfilt > 0) {
		GLenum min_f = texfilt >= 2
		                   ? GL_LINEAR_MIPMAP_LINEAR
		                   : GL_LINEAR_MIPMAP_NEAREST;

		if (!texture->has_mipmaps && !texture->authored_mip_count) {
			while (glGetError() != GL_NO_ERROR) {}
			glGenerateMipmap(GL_TEXTURE_2D);
			texture->has_mipmaps = glGetError() == GL_NO_ERROR;
			if (generated && texture->has_mipmaps)
				(*generated)++;
		}
		if (!texture->has_mipmaps) min_f = GL_LINEAR;
		apply_bound_min_mag_filter(texture, min_f, GL_LINEAR);
	} else {
		apply_bound_min_mag_filter(texture, GL_NEAREST, GL_NEAREST);
	}
	return 1;
}

static int apply_texture_filters_all(struct android_ogl_texture_filter_state *state,
                                     int texfilt, int *generated)
{
	int i;
	int updated = 0;

	if (generated)
		*generated = 0;
	for (i = 0; i < state->texture_list_state.texture_list_size; i++)
		updated += apply_texture_filter(&state->texture_list_state.texture_list[i],
		                                texfilt, generated);
	return updated;
}

void android_ogl_apply_texfilt_all(struct android_ogl_texture_texfilt_state *state)
{
	int texfilt_level;
	int updated = 0;
	int i;

	if (!state || !state->texture_list_state.texture_list ||
	    state->texture_list_state.texture_list_size <= 0 ||
	    !state->pending_apply || !state->requested_texfilt_level ||
	    !state->applied_texfilt_level)
		return;
	if (!*state->pending_apply)
		return;

	*state->pending_apply = 0;
	__sync_synchronize();
	texfilt_level = *state->requested_texfilt_level;
	*state->applied_texfilt_level = texfilt_level;
	for (i = 0; i < state->texture_list_state.texture_list_size; i++)
		updated += apply_texture_filter(&state->texture_list_state.texture_list[i], texfilt_level, NULL);
	if (updated)
		con_printf(CON_DEBUG,
		           "texfilt: updated %d textures in-place (TexFilt=%d)",
		           updated, texfilt_level);
}

void android_ogl_apply_pending_texture_options(
    struct android_ogl_texture_filter_state *state, const char *source)
{
	if (!state || !state->texture_list_state.texture_list ||
	    state->texture_list_state.texture_list_size <= 0 ||
	    !state->aniso_pending_apply || !state->texfilt_pending_apply ||
	    !state->aniso_level || !state->maxanisotropy ||
	    !state->requested_texfilt_level || !state->applied_texfilt_level)
		return;

	if (*state->aniso_pending_apply) {
		int generated = 0;
		int effective_texfilt;
		int filter_updated;
		struct android_ogl_texture_anisotropy_state anisotropy_state;

		*state->aniso_pending_apply = 0;
		__sync_synchronize();
		effective_texfilt = android_ogl_effective_texfilt(
		    *state->applied_texfilt_level, *state->aniso_level);
		filter_updated = apply_texture_filters_all(state, effective_texfilt,
		                                           &generated);
		debug_log(DLOG_GRAPHICS,
		          "graphics apply[%s]: aniso=%d effective_texfilt=%d filters=%d mipmaps=%d",
		          source ? source : "unknown", *state->aniso_level,
		          effective_texfilt, filter_updated, generated);

		anisotropy_state.texture_list_state = state->texture_list_state;
		anisotropy_state.maxanisotropy = *state->maxanisotropy;
		anisotropy_state.aniso_level = *state->aniso_level;
		android_ogl_apply_anisotropy_all(&anisotropy_state);
	}

	if (*state->texfilt_pending_apply) {
		int requested_texfilt = *state->requested_texfilt_level;
		int effective_texfilt;
		struct android_ogl_texture_texfilt_state texfilt_state = {
			state->texture_list_state,
			state->texfilt_pending_apply,
			state->requested_texfilt_level,
			state->applied_texfilt_level
		};

		debug_log(DLOG_GRAPHICS,
		          "graphics apply[%s]: tex_filt request=%d aniso=%d",
		          source ? source : "unknown", requested_texfilt,
		          *state->aniso_level);
		android_ogl_apply_texfilt_all(&texfilt_state);
		effective_texfilt = android_ogl_effective_texfilt(
		    *state->applied_texfilt_level, *state->aniso_level);
		if (effective_texfilt != *state->applied_texfilt_level) {
			int generated = 0;
			int filter_updated = apply_texture_filters_all(
			    state, effective_texfilt, &generated);

			debug_log(DLOG_GRAPHICS,
			          "graphics apply[%s]: tex_filt effective=%d filters=%d mipmaps=%d",
			          source ? source : "unknown", effective_texfilt,
			          filter_updated, generated);
		}
	}
	*state->requested_texfilt_level = *state->applied_texfilt_level;
}

int android_ogl_load_dxa_mask(const char *image_filename, grs_bitmap *bm, int texfilt,
                              android_ogl_loadtexture_fn loadtexture)
{
	char maskname[256];
	png_data mdata;
	int loaded = 0;

	if (!image_filename || !bm || !loadtexture)
		return 0;
	if (!replacement_mask_name(image_filename, maskname, sizeof(maskname))) {
		if (bitmap_needs_super_mask(bm)) return 0;
		/* No cutout pixels: keep the complete underlying face */
		unsigned char keep = 0;
		if (!bm->gltexture_mask)
			ogl_init_texture(bm->gltexture_mask = ogl_get_free_texture(), 1, 1, OGL_FLAG_ALPHA);
		return !loadtexture(&keep, 0, 0, bm->gltexture_mask, BM_FLAG_TRANSPARENT, 0, texfilt, NULL);
	}
	loaded = read_png(maskname, &mdata);
	if (!loaded) {
		debug_log(DLOG_TEXTURE, "Mask not found: %s", maskname);
		return 0;
	}
	if (mdata.depth == 8) {
		int size = (int) (mdata.width * mdata.height);
		int ch = mdata.paletted ? 1 : (int) mdata.channels;
		unsigned char *mask = (unsigned char *) malloc((size_t) size);

		if (!mask) {
			free(mdata.data);
			if (mdata.palette)
				free(mdata.palette);
			return 0;
		}
		/* Convert to single-byte mask matching ogl_loadpngmask convention:
		 * 255 where super-transparent, 0 elsewhere. Upload via palette path
		 * with BM_FLAG_TRANSPARENT so 255->alpha=0, 0->alpha=1.
		 * Mask PNGs have white=keep, black=super-transparent, so invert */
		for (int i = 0; i < size; i++)
			mask[i] = mdata.data[i * ch] > 128 ? 0 : 255;

		if (bm->gltexture_mask == NULL)
			ogl_init_texture(bm->gltexture_mask = ogl_get_free_texture(),
			                 (int) mdata.width, (int) mdata.height, OGL_FLAG_ALPHA);
		loaded = !loadtexture(mask, 0, 0, bm->gltexture_mask, BM_FLAG_TRANSPARENT, 0,
		                      texfilt, NULL);
		bm->gltexture_mask->is_png = 1;
		free(mask);

		debug_log(DLOG_TEXTURE, "Loaded mask: %s %dx%d ch=%d",
		          maskname, (int) mdata.width, (int) mdata.height, ch);
	} else {
		loaded = 0;
		debug_log(DLOG_TEXTURE, "Mask format unsupported: %s depth=%d color=%d",
		          maskname, (int) mdata.depth, (int) mdata.color);
	}
	free(mdata.data);
	if (mdata.palette)
		free(mdata.palette);
	return loaded;
}

/* Texture contents are converted and replaced on every blit */
ogl_texture *android_ogl_transient_blit_texture(int width, int height)
{
	int tw = 1, th = 1;
	if (width <= 0 || height <= 0 || width > 2048 || height > 2048)
		return NULL;
	while (tw < width) tw *= 2;
	while (th < height) th *= 2;
	if (tw * th > 2 * 1024 * 1024)
		return NULL;
	return &transient_blit_texture;
}

void android_ogl_reset_transient_blit_texture(
    const struct android_ogl_bind_texture_state *state, int delete_handle)
{
	if (delete_handle && transient_blit_texture.handle)
		glDeleteTextures(1, &transient_blit_texture.handle);
	memset(&transient_blit_texture, 0, sizeof(transient_blit_texture));
	android_ogl_reset_texture_bindings(state);
}

#endif
