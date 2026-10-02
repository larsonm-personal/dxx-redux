#ifndef DXX_ANDROID_EGL_SURFACE_H
#define DXX_ANDROID_EGL_SURFACE_H

#ifdef ANDROID

#include <EGL/egl.h>
#include <stdint.h>

typedef void (*android_egl_resource_callback)(void);

/* Game-thread cleanup must forget names from the retired context, not delete new objects */
int android_egl_discarding_lost_context_resources(void);

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
	uint64_t window_generation;
};

/* Returns zero without issuing GL calls when EGL setup fails */
int android_egl_surface_initialize(struct android_egl_surface_state *state,
                                   int width, int height, int use_rgba8888, int *out_color_depth);
#ifdef INTROSPECT_ON
/* Game-thread-only one-shot failure after the caller has destroyed its old context */
void android_egl_surface_debug_fail_initialize_once(void);
/* Bounded game-thread-only failures, including accepted-mode reconstruction */
void android_egl_surface_debug_fail_initialize_count(int count);
/* Retire the real current context on the next game-thread surface recreation */
void android_egl_surface_debug_lose_context_on_resume_once(void);
#endif
void android_egl_surface_swap(struct android_egl_surface_state *state);
int android_egl_surface_get_recreate_count(const struct android_egl_surface_state *state);
uint64_t android_egl_surface_get_window_generation(const struct android_egl_surface_state *state);
void android_egl_surface_log_renderer(void);
void android_egl_surface_query_capabilities(float *out_max_anisotropy,
                                            int *out_max_msaa_samples, int *out_gpu_timer_available);

#endif

#endif
