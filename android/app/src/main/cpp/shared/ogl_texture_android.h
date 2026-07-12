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

struct android_ogl_texture_filter_state {
	struct android_ogl_texture_list_state texture_list_state;
	GLuint *last_bound_tex;
	volatile int *aniso_pending_apply;
	volatile int *texfilt_pending_apply;
	int *aniso_level;
	GLfloat *maxanisotropy;
	int *requested_texfilt_level;
	int *applied_texfilt_level;
};

typedef int (*android_ogl_loadtexture_fn)(unsigned char *data, int dxo, int dyo,
                                         ogl_texture *tex, int bm_flags,
                                         int data_format, int texfilt,
                                         const char *bitmapname);

void android_ogl_bind_texture_2d(const struct android_ogl_bind_texture_state *state,
                                 GLuint handle);
void android_ogl_enable_texture_2d(int *texture_2d_enabled);
int android_ogl_get_texture_bytes(const struct android_ogl_texture_list_state *state);
void android_ogl_apply_anisotropy_all(struct android_ogl_texture_anisotropy_state *state);
void android_ogl_apply_texfilt_all(struct android_ogl_texture_texfilt_state *state);
int android_ogl_effective_texfilt(int texfilt, int aniso_level);
void android_ogl_apply_bound_texture_filter(ogl_texture *texture, int effective_texfilt,
                                            int menu_texfilt, int hud_texfilt,
                                            int render_context);
void android_ogl_apply_pending_texture_options(
    struct android_ogl_texture_filter_state *state, const char *source);
void android_ogl_load_dxa_mask(const char *bitmapname, grs_bitmap *bm, int texfilt,
                               android_ogl_loadtexture_fn loadtexture);

#endif

#endif
