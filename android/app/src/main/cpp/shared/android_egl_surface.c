#ifdef ANDROID

#include <inttypes.h>
#include <string.h>

#include <android/log.h>
#include <android/native_window.h>

#include "android_crash_handler.h"
#include "android_egl_surface.h"
#include "android_lifecycle_diagnostics.h"
#include "android_surface_lifecycle.h"
#include "console.h"
#include "gles3_shim.h"
#include "ogl_init.h"

#define ANDROID_EGL_INITIAL_CLIENT_VERSION  3
#define ANDROID_EGL_RECREATE_CLIENT_VERSION 1

static int android_egl_test_error(const char *location)
{
	EGLint error = eglGetError();

	if (error != EGL_SUCCESS) {
		con_printf(CON_URGENT, "%s failed (%d).\n", location, error);
		return 0;
	}
	return 1;
}

static int android_egl_recreate_surface(struct android_egl_surface_state *state,
                                        const struct android_surface_snapshot *snapshot)
{
	EGLint format;
	EGLint window_attributes[] = {
		EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE, EGL_NONE
	};

	con_printf(CON_DEBUG, "EGL: recreating surface after resume\n");

	eglMakeCurrent(*state->display, EGL_NO_SURFACE, EGL_NO_SURFACE, *state->context);

	if (*state->surface != EGL_NO_SURFACE) {
		eglDestroySurface(*state->display, *state->surface);
		*state->surface = EGL_NO_SURFACE;
	}

	eglGetConfigAttrib(*state->display, *state->config, EGL_NATIVE_VISUAL_ID, &format);
	ANativeWindow_setBuffersGeometry(snapshot->window, state->width, state->height, format);

	*state->surface = eglCreateWindowSurface(*state->display, *state->config,
	                                         (EGLNativeWindowType) snapshot->window, window_attributes);
	if (*state->surface == EGL_NO_SURFACE) {
		con_printf(CON_URGENT, "EGL: failed to create new surface after resume\n");
		return 0;
	}

	if (!eglMakeCurrent(*state->display, *state->surface, *state->surface,
	                    *state->context)) {
		EGLint context_attributes[] = {
			EGL_CONTEXT_CLIENT_VERSION, ANDROID_EGL_RECREATE_CLIENT_VERSION,
			EGL_NONE, EGL_NONE
		};

		con_printf(CON_URGENT,
		           "EGL: context lost during resume, doing full re-init\n");
		eglDestroyContext(*state->display, *state->context);
		*state->context = eglCreateContext(*state->display, *state->config,
		                                   EGL_NO_CONTEXT, context_attributes);
		if (eglMakeCurrent(*state->display, *state->surface, *state->surface,
		                   *state->context) == EGL_TRUE)
			android_lifecycle_diagnostics_note_context_created();
		state->smash_textures();
		state->cache_textures();
		con_printf(CON_DEBUG,
		           "EGL: full re-init complete, textures re-cached\n");
	} else {
		con_printf(CON_DEBUG, "EGL: surface recreated, context preserved\n");
	}
	state->window_generation = snapshot->generation;
	android_lifecycle_diagnostics_set_window_generation(snapshot->generation);
	state->recreate_count++;
	__android_log_print(ANDROID_LOG_INFO, "DXX-EGL",
	                    "window generation=%" PRIu64 " active recreate_count=%d",
	                    state->window_generation, state->recreate_count);
	return 1;
}

void android_egl_surface_initialize(struct android_egl_surface_state *state,
                                    int width, int height, int use_rgba8888, int *out_color_depth)
{
	struct android_surface_snapshot snapshot;
	EGLint version_major, version_minor;
	EGLint config_attributes[] = {
		EGL_RED_SIZE, 5,
		EGL_GREEN_SIZE, 6,
		EGL_BLUE_SIZE, 5,
		EGL_DEPTH_SIZE, 16,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_NONE, EGL_NONE
	};
	EGLint context_attributes[] = {
		EGL_CONTEXT_CLIENT_VERSION, ANDROID_EGL_INITIAL_CLIENT_VERSION,
		EGL_NONE, EGL_NONE
	};
	EGLint window_attributes[] = {
		EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE, EGL_NONE
	};
	int config_count;

	state->width = width;
	state->height = height;
	state->window_generation = 0;
	android_surface_acquire_snapshot(&snapshot);
	__android_log_print(ANDROID_LOG_INFO, "DXX-EGL",
	                    "initialize window generation=%" PRIu64 " window=%d paused=%d",
	                    snapshot.generation, snapshot.window != NULL, snapshot.paused);
	if (use_rgba8888) {
		config_attributes[1] = 8;
		config_attributes[3] = 8;
		config_attributes[5] = 8;
	}

	*state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (*state->display == EGL_NO_DISPLAY)
		con_printf(CON_URGENT, "EGL: Error querying EGL Display\n");

	if (!eglInitialize(*state->display, &version_major, &version_minor)) {
		con_printf(CON_URGENT, "EGL: Error initializing EGL\n");
	} else {
		con_printf(CON_DEBUG, "EGL: Initialized, version: major %i minor %i\n",
		           version_major, version_minor);
	}

	if (!eglChooseConfig(*state->display, config_attributes, state->config, 1,
	                     &config_count) ||
	    config_count != 1) {
		con_printf(CON_URGENT, "EGL: Error choosing config\n");
	} else {
		EGLint red = 0, green = 0, blue = 0;

		con_printf(CON_DEBUG, "EGL: config chosen\n");
		eglGetConfigAttrib(*state->display, *state->config, EGL_RED_SIZE, &red);
		eglGetConfigAttrib(*state->display, *state->config, EGL_GREEN_SIZE, &green);
		eglGetConfigAttrib(*state->display, *state->config, EGL_BLUE_SIZE, &blue);
		*out_color_depth = red + green + blue;
		con_printf(CON_DEBUG, "EGL: color depth R%d G%d B%d (%d-bit)\n",
		           red, green, blue, *out_color_depth);
	}

	if (snapshot.window) {
		EGLint format;

		eglGetConfigAttrib(*state->display, *state->config,
		                   EGL_NATIVE_VISUAL_ID, &format);
		ANativeWindow_setBuffersGeometry(snapshot.window, width, height, format);
		*state->surface = eglCreateWindowSurface(*state->display, *state->config,
		                                         (EGLNativeWindowType) snapshot.window, window_attributes);
		if (*state->surface != EGL_NO_SURFACE)
			state->window_generation = snapshot.generation;
	}
	android_surface_release_snapshot(&snapshot);

	if (*state->surface == EGL_NO_SURFACE) {
		con_printf(CON_URGENT, "EGL: Error creating window surface\n");
	} else {
		con_printf(CON_DEBUG, "EGL: Created window surface\n");
	}

	*state->context = eglCreateContext(*state->display, *state->config,
	                                   EGL_NO_CONTEXT, context_attributes);
	if (*state->context == EGL_NO_CONTEXT) {
		con_printf(CON_URGENT, "EGL: Error creating context\n");
	} else {
		con_printf(CON_DEBUG, "EGL: Created context\n");
		android_lifecycle_diagnostics_note_context_created();
	}
	android_lifecycle_diagnostics_set_window_generation(state->window_generation);

	eglMakeCurrent(*state->display, *state->surface, *state->surface,
	               *state->context);
	if (!android_egl_test_error("eglMakeCurrent")) {
		con_printf(CON_URGENT, "EGL: Error making current\n");
	} else {
		con_printf(CON_DEBUG, "EGL: made context current\n");
	}

	gles3_shim_init();
}

void android_egl_surface_swap(struct android_egl_surface_state *state)
{
	struct android_surface_snapshot snapshot;
	int trace_swap;

	state->swap_count++;
	trace_swap = state->swap_count <= 20 || (state->swap_count % 60) == 0;
	if (trace_swap)
		crash_breadcrumb_v("ogl_swap_buffers_internal #%d", state->swap_count);
	android_surface_acquire_snapshot(&snapshot);
	if (snapshot.paused || !snapshot.window) {
		if (trace_swap)
			crash_breadcrumb(snapshot.paused ? "ogl_swap: paused" : "ogl_swap: no surface");
		android_surface_release_snapshot(&snapshot);
		return;
	}
	if (state->window_generation != snapshot.generation) {
		__android_log_print(ANDROID_LOG_INFO, "DXX-EGL",
		                    "window generation changed from %" PRIu64 " to %" PRIu64,
		                    state->window_generation, snapshot.generation);
		if (trace_swap)
			crash_breadcrumb("ogl_swap: recreate_egl");
		if (!android_egl_recreate_surface(state, &snapshot)) {
			android_surface_release_snapshot(&snapshot);
			return;
		}
	}
	if (trace_swap)
		crash_breadcrumb("ogl_swap: eglSwapBuffers");
	android_lifecycle_diagnostics_count(ANDROID_LIFECYCLE_COUNTER_SWAP_ATTEMPT);
	if (eglSwapBuffers(*state->display, *state->surface) == EGL_TRUE)
		android_lifecycle_diagnostics_count(ANDROID_LIFECYCLE_COUNTER_SWAP_PRESENTED);
	android_surface_release_snapshot(&snapshot);
}

int android_egl_surface_get_recreate_count(const struct android_egl_surface_state *state)
{
	return state->recreate_count;
}

uint64_t android_egl_surface_get_window_generation(const struct android_egl_surface_state *state)
{
	return state->window_generation;
}

void android_egl_surface_log_renderer(void)
{
	const char *renderer = (const char *) glGetString(GL_RENDERER);

	con_printf(CON_NORMAL, "GL_RENDERER: %s", renderer ? renderer : "(null)");
}

void android_egl_surface_query_capabilities(float *out_max_anisotropy,
                                            int *out_max_msaa_samples, int *out_gpu_timer_available)
{
	const char *extensions;
	GLint max_samples = 0;

	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, out_max_anisotropy);
	__android_log_print(ANDROID_LOG_INFO, "DXX",
	                    "anisotropy: max=%.0f", *out_max_anisotropy);
	glGetIntegerv(0x8D57, &max_samples);
	*out_max_msaa_samples = (int) max_samples;
	__android_log_print(ANDROID_LOG_INFO, "DXX",
	                    "msaa: max_samples=%d", *out_max_msaa_samples);
	extensions = (const char *) glGetString(GL_EXTENSIONS);
	*out_gpu_timer_available = extensions &&
	                                   strstr(extensions, "GL_EXT_disjoint_timer_query")
	                               ? 1
	                               : 0;
	__android_log_print(ANDROID_LOG_INFO, "DXX", "gpu_timer: %s",
	                    *out_gpu_timer_available ? "available" : "not available");
}

#endif
