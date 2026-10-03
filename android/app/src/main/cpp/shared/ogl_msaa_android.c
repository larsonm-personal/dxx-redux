#ifdef ANDROID

#include <android/log.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "ogl_init.h"
#include "ogl_msaa_android.h"
#include "android_graphics_safety.h"
#include "android_log.h"
#include "android_gpu_capabilities.h"
#include "android_gpu_policy.h"

#ifdef INTROSPECT_ON
static int debug_fail_color_allocation;

void android_ogl_msaa_debug_fail_color_allocation_once(void)
{
	debug_fail_color_allocation = 1;
}
#endif

static void android_ogl_msaa_log(android_ogl_msaa_log_message_fn log_message,
                                 void *log_user_data,
                                 const char *message)
{
	if (log_message)
		log_message(message, log_user_data);
}

unsigned int android_ogl_msaa_color_format(int red, int green, int blue, int alpha)
{
	/* EGL minimum sizes can select RGB10_A2, including on the Retroid Pocket 4 Pro
	 * ES multisample resolves cannot convert it from an RGBA8 source */
	return android_gpu_color_format(red, green, blue, alpha);
}

unsigned int android_ogl_msaa_capture_errors(const char *stage)
{
	GLenum first = GL_NO_ERROR;
	GLenum error;

	while ((error = glGetError()) != GL_NO_ERROR) {
		if (first == GL_NO_ERROR)
			first = error;
		debug_log(DLOG_GRAPHICS, "MSAA GL error: stage=%s error=0x%x", stage, error);
	}
	return first;
}

void android_ogl_msaa_capture_scene_errors(struct android_ogl_msaa_state *state)
{
	GLenum error = android_ogl_msaa_capture_errors("scene_end");
	if (error != GL_NO_ERROR) {
		state->last_scene_gl_error = error;
		state->scene_error_count++;
	}
}

static void android_ogl_msaa_creation_check(GLenum *first_error, const char *stage)
{
	GLenum error = android_ogl_msaa_capture_errors(stage);
	if (*first_error == GL_NO_ERROR) *first_error = error;
	if (error != GL_NO_ERROR)
		debug_log_force(DLOG_GRAPHICS, "MSAA creation failed: stage=%s error=0x%x first_error=0x%x", stage, error, *first_error);
	else
		debug_log(DLOG_GRAPHICS, "MSAA creation: stage=%s error=0x%x first_error=0x%x", stage, error, *first_error);
}

static void android_ogl_msaa_log_format_support(GLenum format)
{
	if (!debug_log_enabled[DLOG_GRAPHICS]) return;
	GLint count = 0, samples[32] = { 0 };
	char values[384] = { 0 };
	glGetInternalformativ(GL_RENDERBUFFER, format, GL_NUM_SAMPLE_COUNTS, 1, &count);
	int bounded_count = count > 32 ? 32 : count > 0 ? count
	                                                : 0;
	if (bounded_count) glGetInternalformativ(GL_RENDERBUFFER, format, GL_SAMPLES, bounded_count, samples);
	for (int i = 0; i < bounded_count; ++i) {
		size_t used = strlen(values);
		snprintf(values + used, sizeof(values) - used, "%s%d", i ? "," : "", samples[i]);
	}
	GLenum error = android_ogl_msaa_capture_errors("format_support_query");
	debug_log(DLOG_GRAPHICS, "MSAA format support: format=0x%x count=%d captured=%d samples=%s error=0x%x",
	          format, count, bounded_count, values, error);
}

static void android_ogl_msaa_log_renderbuffer(const char *attachment, GLuint renderbuffer)
{
	if (!debug_log_enabled[DLOG_GRAPHICS]) return;
	GLint width = 0, height = 0, format = 0, samples = 0;
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &format);
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &samples);
	GLenum error = android_ogl_msaa_capture_errors("renderbuffer_diagnostic_query");
	debug_log(DLOG_GRAPHICS, "MSAA renderbuffer: attachment=%s object=%u size=%dx%d format=0x%x samples=%d error=0x%x",
	          attachment, renderbuffer, width, height, format, samples, error);
}

static void android_ogl_msaa_log_window_target(void)
{
	if (!debug_log_enabled[DLOG_GRAPHICS]) return;
	GLint encoding = 0, component = 0, read_buffer = 0, draw_buffer = 0, samples = 0, sample_buffers = 0;
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &encoding);
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &component);
	glGetIntegerv(GL_READ_BUFFER, &read_buffer);
	glGetIntegerv(GL_DRAW_BUFFER0, &draw_buffer);
	glGetIntegerv(GL_SAMPLES, &samples);
	glGetIntegerv(GL_SAMPLE_BUFFERS, &sample_buffers);
	GLenum error = android_ogl_msaa_capture_errors("window_target_diagnostic_query");
	debug_log(DLOG_GRAPHICS,
	          "MSAA window target: encoding=0x%x component=0x%x read_buffer=0x%x draw_buffer=0x%x sample_buffers=%d samples=%d query_error=0x%x",
	          encoding, component, read_buffer, draw_buffer, sample_buffers, samples, error);
}

void android_ogl_msaa_trace_stage(struct android_ogl_msaa_state *state, const char *stage, int bound, int depth)
{
	if (state->trace_remaining <= 0 || !debug_log_enabled[DLOG_GRAPHICS]) return;
	GLint read = 0, draw = 0, clip[4] = { 0 }, viewport[4] = { 0 };
	GLboolean mask[4] = { 0 }, depth_mask = 0, coverage_invert = 0;
	GLfloat clear[4] = { 0 }, coverage = 0;
	GLint read_buffer = 0, draw_buffer = 0;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
	glGetIntegerv(GL_SCISSOR_BOX, clip);
	glGetIntegerv(GL_VIEWPORT, viewport);
	glGetBooleanv(GL_COLOR_WRITEMASK, mask);
	glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
	glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
	glGetIntegerv(GL_READ_BUFFER, &read_buffer);
	glGetIntegerv(GL_DRAW_BUFFER0, &draw_buffer);
	glGetFloatv(GL_SAMPLE_COVERAGE_VALUE, &coverage);
	glGetBooleanv(GL_SAMPLE_COVERAGE_INVERT, &coverage_invert);
	debug_log(DLOG_GRAPHICS,
	          "MSAA trace: flip=%llu event=%u stage=%s generation=%llu bind_serial=%llu target=%u read=%d draw=%d bound=%d depth=%d size=%dx%d scissor=%d box=%d,%d,%d,%d viewport=%d,%d,%d,%d mask=%d,%d,%d,%d depth_mask=%d clear=%.2f,%.2f,%.2f,%.2f read_buffer=0x%x draw_buffer=0x%x coverage=%d/%.2f/%d alpha_to_coverage=%d",
	          state->flip_serial, state->flip_event++, stage, state->generation, state->active_frame_serial, state->fbo, read, draw, bound, depth,
	          state->w, state->h, glIsEnabled(GL_SCISSOR_TEST), clip[0], clip[1], clip[2], clip[3],
	          viewport[0], viewport[1], viewport[2], viewport[3], mask[0], mask[1], mask[2], mask[3], depth_mask,
	          clear[0], clear[1], clear[2], clear[3], read_buffer, draw_buffer,
	          glIsEnabled(GL_SAMPLE_COVERAGE), coverage, coverage_invert, glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE));
}

void android_ogl_msaa_presented(struct android_ogl_msaa_state *state)
{
	state->flip_serial++;
	state->flip_event = 0;
	if (state->trace_remaining > 0) state->trace_remaining--;
}

void android_ogl_msaa_destroy_fbo(struct android_ogl_msaa_state *state, int *bound)
{
	if (!state)
		return;
	if (state->fbo) {
		glDeleteFramebuffers(1, &state->fbo);
		state->fbo = 0;
	}
	if (state->color_rbo) {
		glDeleteRenderbuffers(1, &state->color_rbo);
		state->color_rbo = 0;
	}
	if (state->depth_rbo) {
		glDeleteRenderbuffers(1, &state->depth_rbo);
		state->depth_rbo = 0;
	}
	state->w = 0;
	state->h = 0;
	state->effective_samples = 0;
	state->failure_latched = 0;
	state->active_frame_serial = 0;
	if (bound)
		*bound = 0;
}

void android_ogl_msaa_forget_context(struct android_ogl_msaa_state *state, int *bound, int *frame_depth)
{
	if (!state) return;
	debug_log(DLOG_GRAPHICS, "MSAA context reset: fbo=%u color=%u depth=%u generation=%llu",
	          state->fbo, state->color_rbo, state->depth_rbo, state->generation);
	state->fbo = state->color_rbo = state->depth_rbo = 0;
	android_ogl_msaa_destroy_fbo(state, bound);
	state->last_create_status = 0;
	state->last_gl_error = GL_NO_ERROR;
	state->last_resolved_frame_serial = 0;
	/* A cold context without MSAA needs no expensive per-frame MSAA trace */
	state->trace_remaining = state->generation ? 120 : 0;
	if (frame_depth) *frame_depth = 0;
}

int android_ogl_msaa_create_fbo(struct android_ogl_msaa_state *state,
                                int *bound,
                                int max_samples,
                                int samples,
                                int w,
                                int h,
                                android_ogl_msaa_log_message_fn log_message,
                                void *log_user_data)
{
	GLenum color_fmt;
	GLenum error = GL_NO_ERROR;
	GLenum status;
	GLint color_samples = 0;
	GLint depth_samples = 0;
	char logbuf[160];

	if (!state)
		return 0;

	android_ogl_msaa_destroy_fbo(state, bound);
	state->last_create_status = 0;
	state->last_gl_error = GL_NO_ERROR;
	state->trace_remaining = 120;

	(void) max_samples; /* The global maximum does not describe attachment support */
	const int requested_samples = samples;
	samples = android_gpu_msaa_samples(samples);
	const int limit = android_gpu_max_renderbuffer_size();
	if (samples < 2 || w <= 0 || h <= 0 || w > limit || h > limit) {
		snprintf(logbuf, sizeof(logbuf), "MSAA FBO unsupported: requested=%d effective=%d size=%dx%d limit=%d", requested_samples, samples, w, h, limit);
		android_ogl_msaa_log(log_message, log_user_data, logbuf);
		state->failure_latched = 1;
		android_graphics_safety_renderer_failed("msaa_configuration_unsupported");
		return 0;
	}

	{
		GLint rb = 0, gb = 0, bb = 0, ab = 0;

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glGetIntegerv(GL_RED_BITS, &rb);
		glGetIntegerv(GL_GREEN_BITS, &gb);
		glGetIntegerv(GL_BLUE_BITS, &bb);
		glGetIntegerv(GL_ALPHA_BITS, &ab);
		color_fmt = android_ogl_msaa_color_format(rb, gb, bb, ab);
		__android_log_print(ANDROID_LOG_INFO, "DXX",
		                    "MSAA: default FB bits r=%d g=%d b=%d a=%d -> fmt=0x%x",
		                    rb, gb, bb, ab, color_fmt);
	}

	if (!color_fmt || color_fmt != android_gpu_msaa_format()) {
		state->failure_latched = 1;
		android_graphics_safety_renderer_failed("msaa_window_format_changed");
		return 0;
	}
	android_ogl_msaa_capture_errors("prior_operation");
	android_ogl_msaa_log_window_target();
	android_ogl_msaa_log_format_support(color_fmt);
	android_ogl_msaa_log_format_support(GL_DEPTH_COMPONENT16);
	glGenFramebuffers(1, &state->fbo);
	android_ogl_msaa_creation_check(&error, "generate_framebuffer");
	glGenRenderbuffers(1, &state->color_rbo);
	android_ogl_msaa_creation_check(&error, "generate_color_renderbuffer");
	glGenRenderbuffers(1, &state->depth_rbo);
	android_ogl_msaa_creation_check(&error, "generate_depth_renderbuffer");

	glBindRenderbuffer(GL_RENDERBUFFER, state->color_rbo);
	android_ogl_msaa_creation_check(&error, "bind_color_renderbuffer");
	int color_width = w;
#ifdef INTROSPECT_ON
	if (debug_fail_color_allocation) {
		debug_fail_color_allocation = 0;
		color_width = -1;
		debug_log_force(DLOG_GRAPHICS, "MSAA fault: rejecting actual color allocation with negative width requested=%dx%d samples=%d", w, h, samples);
	}
#endif
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, color_fmt, color_width, h);
	android_ogl_msaa_creation_check(&error, "allocate_color_multisample");
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &color_samples);
	android_ogl_msaa_creation_check(&error, "query_color_samples");
	android_ogl_msaa_log_renderbuffer("color", state->color_rbo);

	glBindRenderbuffer(GL_RENDERBUFFER, state->depth_rbo);
	android_ogl_msaa_creation_check(&error, "bind_depth_renderbuffer");
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT16, w, h);
	android_ogl_msaa_creation_check(&error, "allocate_depth_multisample");
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &depth_samples);
	android_ogl_msaa_creation_check(&error, "query_depth_samples");
	android_ogl_msaa_log_renderbuffer("depth", state->depth_rbo);

	glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
	android_ogl_msaa_creation_check(&error, "bind_created_framebuffer");
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                          GL_RENDERBUFFER, state->color_rbo);
	android_ogl_msaa_creation_check(&error, "attach_color_renderbuffer");
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
	                          GL_RENDERBUFFER, state->depth_rbo);
	android_ogl_msaa_creation_check(&error, "attach_depth_renderbuffer");

	status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	android_ogl_msaa_creation_check(&error, "framebuffer_completeness");
	state->last_create_status = status;
	state->last_gl_error = error;
	if (error != GL_NO_ERROR || status != GL_FRAMEBUFFER_COMPLETE ||
	    color_samples < 2 || depth_samples < 2 || color_samples != depth_samples) {
		if (error != GL_NO_ERROR) {
			__android_log_print(ANDROID_LOG_ERROR, "DXX",
			                    "MSAA FBO create error: error=0x%x samples=%d/%d requested=%d",
			                    error, color_samples, depth_samples, samples);
			snprintf(logbuf, sizeof(logbuf),
			         "MSAA FBO create error: error=0x%x samples=%d/%d requested=%d",
			         error, color_samples, depth_samples, samples);
			android_ogl_msaa_log(log_message, log_user_data, logbuf);
		} else if (status != GL_FRAMEBUFFER_COMPLETE) {
			__android_log_print(ANDROID_LOG_ERROR, "DXX",
			                    "MSAA FBO incomplete: status=0x%x samples=%d %dx%d fmt=0x%x",
			                    status, samples, w, h, color_fmt);
			snprintf(logbuf, sizeof(logbuf),
			         "MSAA FBO incomplete: status=0x%x samples=%d size=%dx%d fmt=0x%x",
			         status, samples, w, h, color_fmt);
			android_ogl_msaa_log(log_message, log_user_data, logbuf);
		} else {
			snprintf(logbuf, sizeof(logbuf),
			         "MSAA FBO invalid effective samples: color=%d depth=%d requested=%d",
			         color_samples, depth_samples, samples);
			android_ogl_msaa_log(log_message, log_user_data, logbuf);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		android_ogl_msaa_destroy_fbo(state, bound);
		state->failure_latched = 1;
		android_graphics_safety_renderer_failed("msaa_framebuffer_failed");
		return 0;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	state->w = w;
	state->h = h;
	state->effective_samples = color_samples;
	state->generation++;
	__android_log_print(ANDROID_LOG_INFO, "DXX",
	                    "MSAA FBO created: %dx samples, %dx%d fmt=0x%x", samples, w, h, color_fmt);
	snprintf(logbuf, sizeof(logbuf), "MSAA FBO created: samples=%d size=%dx%d fmt=0x%x",
	         samples, w, h, color_fmt);
	android_ogl_msaa_log(log_message, log_user_data, logbuf);
	return 1;
}

int android_ogl_msaa_begin_frame(struct android_ogl_msaa_state *state,
                                 int *bound, int *frame_depth,
                                 int max_samples, int samples, int w, int h,
                                 android_ogl_msaa_log_message_fn log_message,
                                 void *log_user_data)
{
	int color_clear = 0;

	if (!state || !bound || !frame_depth)
		return 0;
	if (samples > 0 && *frame_depth == 0) {
		if (!state->failure_latched && (!state->fbo || state->w != w || state->h != h)) {
			char logbuf[160];

			snprintf(logbuf, sizeof(logbuf),
			         "MSAA FBO create request: samples=%d max=%d size=%dx%d",
			         samples, max_samples, w, h);
			android_ogl_msaa_log(log_message, log_user_data, logbuf);
			android_ogl_msaa_create_fbo(state, bound, max_samples, samples,
			                            w, h, log_message, log_user_data);
		}
		if (state->fbo && !state->failure_latched) {
			android_ogl_msaa_capture_errors("before_bind");
			glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
			state->last_gl_error = android_ogl_msaa_capture_errors("framebuffer_bind");
			if (state->last_gl_error == GL_NO_ERROR) {
				color_clear = !*bound;
				*bound = 1;
				state->bound_frame_count++;
				state->active_frame_serial = state->bound_frame_count;
				android_ogl_msaa_trace_stage(state, "begin", *bound, *frame_depth);
			} else {
				state->failure_latched = 1;
				android_graphics_safety_renderer_failed("msaa_bind_failed");
			}
		}
	}
	(*frame_depth)++;
	return color_clear;
}

void android_ogl_msaa_end_frame(int *frame_depth)
{
	if (frame_depth && *frame_depth > 0)
		(*frame_depth)--;
}

int android_ogl_msaa_resolve(struct android_ogl_msaa_state *state,
                             int *bound, int frame_depth, int w, int h)
{
	struct timespec start, end;
	GLenum error;

	if (!state || !bound)
		return 0;
	if (*bound && frame_depth == 0) {
		clock_gettime(CLOCK_MONOTONIC, &start);
		android_ogl_msaa_trace_stage(state, "before_resolve", *bound, frame_depth);
		android_ogl_msaa_capture_errors("before_resolve");
		glBindFramebuffer(GL_READ_FRAMEBUFFER, state->fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(0, 0, w, h, 0, 0, w, h,
		                  GL_COLOR_BUFFER_BIT, GL_NEAREST);
		error = android_ogl_msaa_capture_errors("resolve");
		if (error != GL_NO_ERROR)
			__android_log_print(ANDROID_LOG_ERROR, "DXX",
			                    "MSAA resolve error: 0x%x", error);
		*bound = 0;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		android_ogl_msaa_trace_stage(state, "after_resolve", *bound, frame_depth);
		if (error == GL_NO_ERROR)
			error = android_ogl_msaa_capture_errors("after_resolve");
		state->last_gl_error = error;
		if (error == GL_NO_ERROR) {
			state->resolve_count++;
			state->last_resolved_frame_serial = state->active_frame_serial;
		} else {
			state->resolve_failures++;
			state->failure_latched = 1;
			android_graphics_safety_renderer_failed("msaa_resolve_failed");
		}
		clock_gettime(CLOCK_MONOTONIC, &end);
		return (int) ((end.tv_sec - start.tv_sec) * 1000000 +
		              (end.tv_nsec - start.tv_nsec) / 1000);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return 0;
}

void android_ogl_msaa_get_diagnostics(
    const struct android_ogl_msaa_state *state,
    struct android_ogl_msaa_diagnostics *diagnostics)
{
	if (!diagnostics)
		return;
	memset(diagnostics, 0, sizeof(*diagnostics));
	if (!state)
		return;
	diagnostics->effective_samples = state->effective_samples;
	diagnostics->width = state->w;
	diagnostics->height = state->h;
	diagnostics->create_complete =
	    state->last_create_status == GL_FRAMEBUFFER_COMPLETE && state->fbo != 0;
	diagnostics->last_frame_resolved =
	    state->active_frame_serial != 0 &&
	    state->last_resolved_frame_serial == state->active_frame_serial;
	diagnostics->last_create_status = state->last_create_status;
	diagnostics->last_gl_error = state->last_gl_error;
	diagnostics->last_scene_gl_error = state->last_scene_gl_error;
	diagnostics->scene_error_count = state->scene_error_count;
	diagnostics->generation = state->generation;
	diagnostics->bound_frame_count = state->bound_frame_count;
	diagnostics->resolve_count = state->resolve_count;
	diagnostics->failure_latched = state->failure_latched;
	diagnostics->resolve_failures = state->resolve_failures;
	diagnostics->flip_serial = state->flip_serial;
	diagnostics->trace_remaining = state->trace_remaining;
}

void android_ogl_msaa_bind_window_backing(
    const struct android_ogl_msaa_state *state, int bound)
{
	glBindFramebuffer(GL_FRAMEBUFFER,
	                  bound && state && state->fbo ? state->fbo : 0);
}

void android_ogl_msaa_bind_overlay_target(
    const struct android_ogl_msaa_state *state, int bound)
{
	glBindFramebuffer(GL_FRAMEBUFFER,
	                  bound && state ? state->fbo : 0);
}

#endif
