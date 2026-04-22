#ifdef ANDROID

#include <stdio.h>
#include <stdlib.h>

#include <android/log.h>

#include "android_log.h"
#include "console.h"
#include "ogl_texture_android.h"
#include "pngfile.h"

extern int ogl_loadtexture(unsigned char *data, int dxo, int dyo,
                           ogl_texture *tex, int bm_flags, int data_format,
                           int texfilt, const char *bitmapname,
                           const char *diag_path);

void android_ogl_bind_texture_2d(const struct android_ogl_bind_texture_state *state,
                                 GLuint handle)
{
	if (!state || !state->last_bound_tex) {
		glBindTexture(GL_TEXTURE_2D, handle);
		return;
	}

	if (handle != *state->last_bound_tex) {
		glBindTexture(GL_TEXTURE_2D, handle);
		*state->last_bound_tex = handle;
		if (state->texbinds)
			(*state->texbinds)++;
	} else if (state->texbind_reuse) {
		(*state->texbind_reuse)++;
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

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_f);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}
		} else {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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

void android_ogl_load_dxa_mask(const char *bitmapname, grs_bitmap *bm, int texfilt)
{
	char maskname[256];
	png_data mdata;
	int loaded = 0;

	if (!bitmapname || !bitmapname[0] || !bm)
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
		ogl_loadtexture(mask, 0, 0, bm->gltexture_mask, BM_FLAG_TRANSPARENT, 0,
		                texfilt, NULL, NULL);
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
