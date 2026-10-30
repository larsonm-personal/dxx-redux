#ifdef _WIN32
#include <winsock2.h>
#endif
#include "pog_renderer.h"
#include "../xl_rgba_pixels.h"
#include <exception>
#include <string>
#include <vector>
extern "C" {
#include "ogl_init.h"
int ogl_loadtexture(unsigned char *, int, int, ogl_texture *, int, int, int, const char *);
}

extern "C" void ogl_xl_rgba_alpha_begin(int active, xl_rgba_alpha_state *state)
{
	*state = {};
	if (!active) return;
#ifdef ANDROID
	state->function = GL_GEQUAL;
	state->reference = gles3_shim_set_alpha_reference(.005f);
#else
	glGetIntegerv(GL_ALPHA_TEST_FUNC, &state->function);
	glGetFloatv(GL_ALPHA_TEST_REF, &state->reference);
	glAlphaFunc(GL_GEQUAL, .005f);
#endif
}

extern "C" void ogl_xl_rgba_alpha_end(const xl_rgba_alpha_state *state)
{
	if (!state->function) return;
#ifdef ANDROID
	gles3_shim_set_alpha_reference(state->reference);
#else
	glAlphaFunc(state->function, state->reference);
#endif
}

extern "C" const char *ogl_xl_rgba_prepare(int width, int height)
{
	// Inspection and native CPU contracts can prepare pixels without a context
	if (!gl_initialized) return nullptr;
#if !defined(OGL_MERGE) || (defined(OGLES) && !defined(ANDROID))
	return "RGBA POG textures require the OpenGL 3 / OpenGL ES 3 merged renderer";
#else
#ifndef ANDROID
	if (!GLEW_VERSION_3_0 && !GLEW_ARB_framebuffer_object)
		return "RGBA POG textures require framebuffer/mipmap support from OpenGL 3";
#endif
	if (width <= 0 || height <= 0 || width > ogl_max_texture_size || height > ogl_max_texture_size)
		return "RGBA POG dimensions exceed this GPU's texture limit";
	return nullptr;
#endif
}

extern "C" const char *ogl_xl_rgba_load(grs_bitmap *bitmap, int filter, const char *name)
{
	static std::string failure;
	try {
		if (const char *error = ogl_xl_rgba_prepare(bitmap->bm_w, bitmap->bm_h)) return error;
		if (!gl_initialized || bitmap->bm_type != BM_RGBA || !bitmap->bm_data)
			return "RGBA bitmap or active graphics context is unavailable";
		ogl_freebmtexture(bitmap);
		bitmap->gltexture = ogl_get_free_texture();
		ogl_init_texture(bitmap->gltexture, bitmap->bm_w, bitmap->bm_h, OGL_FLAG_ALPHA);
		bitmap->gltexture->xl_rgba = 1;
		bitmap->gltexture->lw = bitmap->bm_rowsize / 4;
		if (ogl_loadtexture(bitmap->bm_data, 0, 0, bitmap->gltexture, bitmap->bm_flags, 4, filter, name) || glGetError() != GL_NO_ERROR) {
			ogl_freebmtexture(bitmap);
			return "RGBA POG color upload failed";
		}
		if (bitmap->bm_flags & BM_FLAG_SUPER_TRANSPARENT) {
			std::vector<unsigned char> mask(size_t(bitmap->bm_w) * bitmap->bm_h);
			for (int y = 0; y < bitmap->bm_h; ++y)
				for (int x = 0; x < bitmap->bm_w; ++x)
					mask[size_t(y) * bitmap->bm_w + x] = dxx_xl_rgba_transparency(bitmap->bm_data + y * bitmap->bm_rowsize + x * 4) < 0 ? 255 : 0;
			bitmap->gltexture_mask = ogl_get_free_texture();
			ogl_init_texture(bitmap->gltexture_mask, bitmap->bm_w, bitmap->bm_h, OGL_FLAG_ALPHA);
			bitmap->gltexture_mask->xl_rgba = 1;
			if (ogl_loadtexture(mask.data(), 0, 0, bitmap->gltexture_mask, BM_FLAG_TRANSPARENT, 0, filter, name) || glGetError() != GL_NO_ERROR) {
				ogl_freebmtexture(bitmap);
				return "RGBA POG super-transparency mask upload failed";
			}
		}
		return nullptr;
	} catch (const std::exception &error) {
		ogl_freebmtexture(bitmap);
		failure = error.what();
		return failure.c_str();
	}
}
