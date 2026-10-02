#ifdef ANDROID

#include <inttypes.h>
#include <string.h>

#include <android/log.h>
#include <android/native_window.h>

#include "android_crash_handler.h"
#include "android_egl_surface.h"
#include "android_graphics_safety.h"
#include "android_log.h"
#include "android_lifecycle_diagnostics.h"
#include "android_surface_lifecycle.h"
#include "console.h"
#include "gles3_shim.h"
#include "ogl_init.h"
#include "ogl_msaa_android.h"
#ifdef INTROSPECT_ON
#include "ogl_msaa_probe_android.h"
#endif

#define ANDROID_EGL_INITIAL_CLIENT_VERSION  3
#define ANDROID_EGL_RECREATE_CLIENT_VERSION 3

static int discarding_lost_context_resources;

int android_egl_discarding_lost_context_resources(void)
{
	return discarding_lost_context_resources;
}

static void android_egl_trace_call(const struct android_egl_surface_state *state,
                                   const char *operation, const char *event)
{
	debug_log(DLOG_GRAPHICS, "EGL call: operation=%s event=%s size=%dx%d generation=%" PRIu64,
	          operation, event, state->width, state->height, state->window_generation);
}

#ifdef INTROSPECT_ON
static int debug_fail_initialize;
static int debug_lose_context_on_resume;

void android_egl_surface_debug_lose_context_on_resume_once(void)
{
	debug_lose_context_on_resume = 1;
}

void android_egl_surface_debug_fail_initialize_once(void)
{
	debug_fail_initialize = 1;
}

void android_egl_surface_debug_fail_initialize_count(int count)
{
	debug_fail_initialize = count > 0 && count <= 4096 ? count : 0;
}
#endif

static int android_egl_recreate_surface(struct android_egl_surface_state *state,
                                        const struct android_surface_snapshot *snapshot)
{
	EGLint format;
	EGLint window_attributes[] = {
		EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE, EGL_NONE
	};

	con_printf(CON_DEBUG, "EGL: recreating surface after resume\n");

	android_egl_trace_call(state, "resume_unbind_context", "begin");
	eglMakeCurrent(*state->display, EGL_NO_SURFACE, EGL_NO_SURFACE, *state->context);
	android_egl_trace_call(state, "resume_unbind_context", "end");

#ifdef INTROSPECT_ON
	if (debug_lose_context_on_resume) {
		debug_lose_context_on_resume = 0;
		EGLBoolean detached = eglMakeCurrent(*state->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		EGLBoolean retired = detached && eglDestroyContext(*state->display, *state->context);
		debug_log_force(DLOG_GRAPHICS, "EGL fault: actual resume context retirement detached=%d retired=%d error=0x%x",
		                detached, retired, eglGetError());
		if (retired) *state->context = EGL_NO_CONTEXT;
	}
#endif

	if (*state->surface != EGL_NO_SURFACE) {
		android_egl_trace_call(state, "resume_destroy_surface", "begin");
		eglDestroySurface(*state->display, *state->surface);
		android_egl_trace_call(state, "resume_destroy_surface", "end");
		*state->surface = EGL_NO_SURFACE;
	}

	eglGetConfigAttrib(*state->display, *state->config, EGL_NATIVE_VISUAL_ID, &format);
	android_egl_trace_call(state, "resume_set_buffers_geometry", "begin");
	ANativeWindow_setBuffersGeometry(snapshot->window, state->width, state->height, format);
	android_egl_trace_call(state, "resume_set_buffers_geometry", "end");

	android_egl_trace_call(state, "resume_create_window_surface", "begin");
	*state->surface = eglCreateWindowSurface(*state->display, *state->config,
	                                         (EGLNativeWindowType) snapshot->window, window_attributes);
	if (*state->surface == EGL_NO_SURFACE) {
		debug_log_force(DLOG_GRAPHICS, "EGL resume failed: operation=eglCreateWindowSurface error=0x%x", eglGetError());
		con_printf(CON_URGENT, "EGL: failed to create new surface after resume\n");
		return 0;
	}
	android_egl_trace_call(state, "resume_create_window_surface", "end");

	android_egl_trace_call(state, "resume_make_current", "begin");
	if (!eglMakeCurrent(*state->display, *state->surface, *state->surface,
	                    *state->context)) {
		EGLint context_attributes[] = {
			EGL_CONTEXT_CLIENT_VERSION, ANDROID_EGL_RECREATE_CLIENT_VERSION,
			EGL_NONE, EGL_NONE
		};

		con_printf(CON_URGENT,
		           "EGL: context lost during resume, doing full re-init\n");
		debug_log(DLOG_GRAPHICS, "EGL resume make current failed: error=0x%x", eglGetError());
		android_egl_trace_call(state, "resume_destroy_context", "begin");
		if (*state->context != EGL_NO_CONTEXT) eglDestroyContext(*state->display, *state->context);
		android_egl_trace_call(state, "resume_destroy_context", "end");
		android_egl_trace_call(state, "resume_create_context", "begin");
		*state->context = eglCreateContext(*state->display, *state->config,
		                                   EGL_NO_CONTEXT, context_attributes);
		android_egl_trace_call(state, "resume_create_context", "end");
		if (*state->context != EGL_NO_CONTEXT)
			android_egl_trace_call(state, "resume_make_new_context_current", "begin");
		if (*state->context == EGL_NO_CONTEXT ||
		    !eglMakeCurrent(*state->display, *state->surface, *state->surface, *state->context)) {
			debug_log_force(DLOG_GRAPHICS, "EGL context recovery failed: operation=%s error=0x%x",
			                *state->context == EGL_NO_CONTEXT ? "eglCreateContext" : "eglMakeCurrent", eglGetError());
			android_graphics_safety_renderer_failed("egl_context_recovery_failed");
			return 0;
		}
		android_egl_trace_call(state, "resume_make_new_context_current", "end");
		android_lifecycle_diagnostics_note_context_created();
		discarding_lost_context_resources = 1;
		state->smash_textures();
		discarding_lost_context_resources = 0;
		gles3_shim_init();
		state->cache_textures();
		con_printf(CON_DEBUG,
		           "EGL: full re-init complete, textures re-cached\n");
	} else {
		con_printf(CON_DEBUG, "EGL: surface recreated, context preserved\n");
	}
	android_egl_trace_call(state, "resume_make_current", "end");
	if (debug_log_enabled[DLOG_GRAPHICS])
		debug_log(DLOG_GRAPHICS, "EGL resumed context: requested_client=%d GL=%s renderer=%s",
		          ANDROID_EGL_RECREATE_CLIENT_VERSION, glGetString(GL_VERSION), glGetString(GL_RENDERER));
	state->window_generation = snapshot->generation;
	android_lifecycle_diagnostics_set_window_generation(snapshot->generation);
	state->recreate_count++;
	__android_log_print(ANDROID_LOG_INFO, "DXX-EGL",
	                    "window generation=%" PRIu64 " active recreate_count=%d",
	                    state->window_generation, state->recreate_count);
	return 1;
}

int android_egl_surface_initialize(struct android_egl_surface_state *state,
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
	int snapshot_owned = 1;
	const char *operation = "eglGetDisplay";
	EGLint setup_error = EGL_SUCCESS;
	EGLBoolean made_current;

#ifdef INTROSPECT_ON
	if (debug_fail_initialize) {
		--debug_fail_initialize;
		debug_log_force(DLOG_GRAPHICS, "Graphics safety fault: EGL initialization failed after old context destruction width=%d height=%d remaining=%d", width, height, debug_fail_initialize);
		return 0;
	}
#endif
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

	android_egl_trace_call(state, operation, "begin");
	*state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (*state->display == EGL_NO_DISPLAY) {
		con_printf(CON_URGENT, "EGL: Error querying EGL Display\n");
		goto failed;
	}
	android_egl_trace_call(state, operation, "end");

	operation = "eglInitialize";
	android_egl_trace_call(state, operation, "begin");
	if (!eglInitialize(*state->display, &version_major, &version_minor)) {
		con_printf(CON_URGENT, "EGL: Error initializing EGL\n");
		goto failed;
	} else {
		con_printf(CON_DEBUG, "EGL: Initialized, version: major %i minor %i\n",
		           version_major, version_minor);
	}
	android_egl_trace_call(state, operation, "end");

	operation = "eglChooseConfig";
	android_egl_trace_call(state, operation, "begin");
	if (!eglChooseConfig(*state->display, config_attributes, state->config, 1,
	                     &config_count) ||
	    config_count != 1) {
		con_printf(CON_URGENT, "EGL: Error choosing config\n");
		goto failed;
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
	android_egl_trace_call(state, operation, "end");

	if (snapshot.window) {
		EGLint format;

		operation = "eglGetConfigAttrib_visual";
		android_egl_trace_call(state, operation, "begin");
		eglGetConfigAttrib(*state->display, *state->config,
		                   EGL_NATIVE_VISUAL_ID, &format);
		android_egl_trace_call(state, operation, "end");
		operation = "ANativeWindow_setBuffersGeometry";
		android_egl_trace_call(state, operation, "begin");
		ANativeWindow_setBuffersGeometry(snapshot.window, width, height, format);
		android_egl_trace_call(state, operation, "end");
		operation = "eglCreateWindowSurface";
		android_egl_trace_call(state, operation, "begin");
		*state->surface = eglCreateWindowSurface(*state->display, *state->config,
		                                         (EGLNativeWindowType) snapshot.window, window_attributes);
		if (*state->surface != EGL_NO_SURFACE)
			state->window_generation = snapshot.generation;
	} else {
		operation = "window_unavailable";
	}
	android_surface_release_snapshot(&snapshot);
	snapshot_owned = 0;

	if (*state->surface == EGL_NO_SURFACE) {
		con_printf(CON_URGENT, "EGL: Error creating window surface\n");
		goto failed;
	} else {
		con_printf(CON_DEBUG, "EGL: Created window surface\n");
	}
	android_egl_trace_call(state, operation, "end");

	operation = "eglCreateContext";
	android_egl_trace_call(state, operation, "begin");
	*state->context = eglCreateContext(*state->display, *state->config,
	                                   EGL_NO_CONTEXT, context_attributes);
	if (*state->context == EGL_NO_CONTEXT) {
		con_printf(CON_URGENT, "EGL: Error creating context\n");
		goto failed;
	} else {
		con_printf(CON_DEBUG, "EGL: Created context\n");
		android_lifecycle_diagnostics_note_context_created();
	}
	android_egl_trace_call(state, operation, "end");
	android_lifecycle_diagnostics_set_window_generation(state->window_generation);

	operation = "eglMakeCurrent";
	android_egl_trace_call(state, operation, "begin");
	made_current = eglMakeCurrent(*state->display, *state->surface, *state->surface, *state->context);
	setup_error = eglGetError();
	if (!made_current || setup_error != EGL_SUCCESS) {
		con_printf(CON_URGENT, "EGL: Error making current\n");
		goto failed;
	} else {
		con_printf(CON_DEBUG, "EGL: made context current\n");
	}
	android_egl_trace_call(state, operation, "end");

	gles3_shim_init();
	{
		const EGLint attributes[] = { EGL_CONFIG_ID, EGL_RED_SIZE, EGL_GREEN_SIZE, EGL_BLUE_SIZE,
			                          EGL_ALPHA_SIZE, EGL_DEPTH_SIZE, EGL_STENCIL_SIZE, EGL_SAMPLE_BUFFERS, EGL_SAMPLES };
		EGLint values[9] = { 0 }, surface_width = 0, surface_height = 0;
		for (int i = 0; i < 9; ++i) eglGetConfigAttrib(*state->display, *state->config, attributes[i], &values[i]);
		eglQuerySurface(*state->display, *state->surface, EGL_WIDTH, &surface_width);
		eglQuerySurface(*state->display, *state->surface, EGL_HEIGHT, &surface_height);
		debug_log(DLOG_GRAPHICS,
		          "EGL config: id=%d rgba=%d/%d/%d/%d depth=%d stencil=%d sample_buffers=%d samples=%d surface=%dx%d requested=%dx%d GL=%s renderer=%s",
		          values[0], values[1], values[2], values[3], values[4], values[5], values[6], values[7], values[8],
		          surface_width, surface_height, width, height, glGetString(GL_VERSION), glGetString(GL_RENDERER));
	}
	return 1;

failed:
	if (setup_error == EGL_SUCCESS) setup_error = eglGetError();
	debug_log_force(DLOG_GRAPHICS, "EGL initialization failed: operation=%s error=0x%x size=%dx%d generation=%" PRIu64,
	                operation, setup_error, width, height, state->window_generation);
	if (snapshot_owned) android_surface_release_snapshot(&snapshot);
	if (*state->context != EGL_NO_CONTEXT) {
		eglDestroyContext(*state->display, *state->context);
		*state->context = EGL_NO_CONTEXT;
	}
	if (*state->surface != EGL_NO_SURFACE) {
		eglDestroySurface(*state->display, *state->surface);
		*state->surface = EGL_NO_SURFACE;
	}
	return 0;
}

void android_egl_surface_swap(struct android_egl_surface_state *state)
{
	struct android_surface_snapshot snapshot;
	int trace_swap;
	struct android_ogl_msaa_diagnostics msaa = { 0 };

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
#ifdef INTROSPECT_ON
	android_ogl_graphics_debug_black_frame();
#endif
	int trace_result = 0;
	if (debug_log_enabled[DLOG_GRAPHICS]) {
		ogl_msaa_get_diagnostics(&msaa);
		trace_result = msaa.trace_remaining > 0 || state->swap_count <= 20;
	}
	if (trace_result)
		debug_log(DLOG_GRAPHICS, "EGL swap: flip=%llu swap=%d event=begin generation=%" PRIu64,
		          msaa.flip_serial, state->swap_count, state->window_generation);
	android_lifecycle_diagnostics_count(ANDROID_LIFECYCLE_COUNTER_SWAP_ATTEMPT);
	EGLBoolean presented = eglSwapBuffers(*state->display, *state->surface);
	EGLint error = presented ? EGL_SUCCESS : eglGetError();
	if (trace_result)
		debug_log(DLOG_GRAPHICS, "EGL swap: flip=%llu swap=%d event=end result=%d error=0x%x generation=%" PRIu64,
		          msaa.flip_serial, state->swap_count, presented, error, state->window_generation);
	if (presented)
		android_lifecycle_diagnostics_count(ANDROID_LIFECYCLE_COUNTER_SWAP_PRESENTED);
	else {
		debug_log_force(DLOG_GRAPHICS, "EGL swap failed: error=0x%x generation=%" PRIu64, error, state->window_generation);
		android_graphics_safety_renderer_failed("egl_swap_failed");
	}
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
