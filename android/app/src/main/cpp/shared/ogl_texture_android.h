#ifndef DXX_ANDROID_OGL_TEXTURE_ANDROID_H
#define DXX_ANDROID_OGL_TEXTURE_ANDROID_H

#ifdef ANDROID

#include "ogl_init.h"

struct android_ogl_texture_list_state {
	ogl_texture *texture_list;
	int texture_list_size;
};

struct android_ogl_bind_texture_state {
	GLuint *last_bound_tex;
	int *texbinds;
	int *texbind_reuse;
};

struct android_ogl_texture_runtime_state {
	struct android_ogl_bind_texture_state bind_state;
	int *texture_2d_enabled;
};

struct android_ogl_texture_anisotropy_state {
	struct android_ogl_texture_list_state texture_list_state;
	GLuint *last_bound_tex;
	GLfloat maxanisotropy;
	int aniso_level;
};

struct android_ogl_texture_texfilt_state {
	struct android_ogl_texture_list_state texture_list_state;
	GLuint *last_bound_tex;
	volatile int *pending_apply;
	int *requested_texfilt_level;
	int *applied_texfilt_level;
};

void android_ogl_bind_texture_2d(const struct android_ogl_bind_texture_state *state,
                                 GLuint handle);
void android_ogl_enable_texture_2d(int *texture_2d_enabled);
int android_ogl_get_texture_bytes(const struct android_ogl_texture_list_state *state);
void android_ogl_apply_anisotropy_all(struct android_ogl_texture_anisotropy_state *state);
void android_ogl_apply_texfilt_all(struct android_ogl_texture_texfilt_state *state);
void android_ogl_load_dxa_mask(const char *bitmapname, grs_bitmap *bm, int texfilt);

#endif

#endif
