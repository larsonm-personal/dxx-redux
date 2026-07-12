#ifndef DXX_ANDROID_EGL_SURFACE_H
#define DXX_ANDROID_EGL_SURFACE_H

#ifdef ANDROID

#include <EGL/egl.h>

typedef void (*android_egl_resource_callback)(void);

struct android_egl_surface_state {
	EGLDisplay *display;
	EGLConfig *config;
	EGLSurface *surface;
	EGLContext *context;
	android_egl_resource_callback smash_textures;
	android_egl_resource_callback cache_textures;
	int width;
	int height;
	int recreate_count;
	int swap_count;
};

void android_egl_surface_initialize(struct android_egl_surface_state *state,
                                    int width, int height, int use_rgba8888, int *out_color_depth);
void android_egl_surface_swap(struct android_egl_surface_state *state);
int android_egl_surface_get_recreate_count(const struct android_egl_surface_state *state);
void android_egl_surface_log_renderer(void);
void android_egl_surface_query_capabilities(float *out_max_anisotropy,
                                            int *out_max_msaa_samples, int *out_gpu_timer_available);

#endif

#endif
