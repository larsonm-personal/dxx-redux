#ifdef ANDROID

#include <stdio.h>
#include <stdlib.h>

#include <android/log.h>

#include "android_log.h"
#include "console.h"
#include "ogl_texture_android.h"
#include "pngfile.h"

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

void android_ogl_bind_texture_2d(const struct android_ogl_bind_texture_state *state,
                                 GLuint handle)
{
	/* GL_TEXTURE_BINDING_2D is scoped to the active texture unit.  The
	 * legacy scalar cache has no unit identity, so it cannot safely skip a
	 * bind after glActiveTexture transitions.  Keep the scalar diagnostic
	 * value synchronized, but bind unconditionally until the caller owns a
	 * per-unit cache. */
	glBindTexture(GL_TEXTURE_2D, handle);
	if (state) {
		if (state->last_bound_tex)
			*state->last_bound_tex = handle;
		if (state->texbinds)
			(*state->texbinds)++;
	}
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
	int total = 0;
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
	    state->texture_list_state.texture_list_size <= 0 || !state->last_bound_tex)
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
	*state->last_bound_tex = 0;
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

		if (!texture->has_mipmaps) {
			glGenerateMipmap(GL_TEXTURE_2D);
			texture->has_mipmaps = 1;
			if (generated)
				(*generated)++;
		}
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
	if (updated && state->last_bound_tex)
		*state->last_bound_tex = 0;
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
	for (i = 0; i < state->texture_list_state.texture_list_size; i++) {
		ogl_texture *texture = &state->texture_list_state.texture_list[i];

		if (texture->handle <= 0)
			continue;
		if (texture->flags & OGL_FLAG_NOCOLOR)
			continue;
		glBindTexture(GL_TEXTURE_2D, texture->handle);
		if (texfilt_level > 0) {
			if (!texture->has_mipmaps) {
				glGenerateMipmap(GL_TEXTURE_2D);
				texture->has_mipmaps = 1;
			}
			{
				GLenum min_f = texfilt_level >= 2
				                   ? GL_LINEAR_MIPMAP_LINEAR
				                   : GL_LINEAR_MIPMAP_NEAREST;

				apply_bound_min_mag_filter(texture, min_f, GL_LINEAR);
			}
		} else {
			apply_bound_min_mag_filter(texture, GL_NEAREST, GL_NEAREST);
		}
		updated++;
	}
	if (state->last_bound_tex)
		*state->last_bound_tex = 0;
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
		anisotropy_state.last_bound_tex = state->last_bound_tex;
		anisotropy_state.maxanisotropy = *state->maxanisotropy;
		anisotropy_state.aniso_level = *state->aniso_level;
		android_ogl_apply_anisotropy_all(&anisotropy_state);
	}

	if (*state->texfilt_pending_apply) {
		int requested_texfilt = *state->requested_texfilt_level;
		int effective_texfilt;
		struct android_ogl_texture_texfilt_state texfilt_state = {
			state->texture_list_state,
			state->last_bound_tex,
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

void android_ogl_load_dxa_mask(const char *bitmapname, grs_bitmap *bm, int texfilt,
                               android_ogl_loadtexture_fn loadtexture)
{
	char maskname[256];
	png_data mdata;
	int loaded = 0;

	if (!bitmapname || !bitmapname[0] || !bm || !loadtexture)
		return;

	sprintf(maskname, "%s_mask.png", bitmapname);
	loaded = read_png(maskname, &mdata);
	if (!loaded) {
		debug_log(DLOG_TEXTURE, "Mask not found: %s", maskname);
		return;
	}
	if (mdata.depth == 8) {
		int size = (int) (mdata.width * mdata.height);
		int ch = mdata.paletted ? 1 : (int) mdata.channels;
		unsigned char *mask = (unsigned char *) malloc((size_t) size);

		if (!mask) {
			free(mdata.data);
			if (mdata.palette)
				free(mdata.palette);
			return;
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
		loadtexture(mask, 0, 0, bm->gltexture_mask, BM_FLAG_TRANSPARENT, 0,
		            texfilt, NULL);
		bm->gltexture_mask->is_png = 1;
		free(mask);

		debug_log(DLOG_TEXTURE, "Loaded mask: %s %dx%d ch=%d",
		          maskname, (int) mdata.width, (int) mdata.height, ch);
	} else {
		debug_log(DLOG_TEXTURE, "Mask format unsupported: %s depth=%d color=%d",
		          maskname, (int) mdata.depth, (int) mdata.color);
	}
	free(mdata.data);
	if (mdata.palette)
		free(mdata.palette);
}

#endif
