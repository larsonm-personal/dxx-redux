#ifdef ANDROID

#include <android/log.h>
#include <stdio.h>
#include <time.h>

#include "ogl_init.h"
#include "ogl_msaa_android.h"

static void android_ogl_msaa_log(android_ogl_msaa_log_message_fn log_message,
                                 void *log_user_data,
                                 const char *message)
{
	if (log_message)
		log_message(message, log_user_data);
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
	if (bound)
		*bound = 0;
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
	char logbuf[160];

	if (!state)
		return 0;

	android_ogl_msaa_destroy_fbo(state, bound);

	if (max_samples > 0 && samples > max_samples)
		samples = max_samples;
	if (samples < 2) {
		snprintf(logbuf, sizeof(logbuf), "MSAA FBO create skipped: clamped_samples=%d", samples);
		android_ogl_msaa_log(log_message, log_user_data, logbuf);
		return 0;
	}

	{
		GLint rb = 0, gb = 0, bb = 0, ab = 0;

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glGetIntegerv(GL_RED_BITS, &rb);
		glGetIntegerv(GL_GREEN_BITS, &gb);
		glGetIntegerv(GL_BLUE_BITS, &bb);
		glGetIntegerv(GL_ALPHA_BITS, &ab);
		if (rb <= 5 && gb <= 6 && bb <= 5 && ab == 0)
			color_fmt = 0x8D62;
		else if (ab > 0)
			color_fmt = GL_RGBA8;
		else
			color_fmt = GL_RGB8;
		__android_log_print(ANDROID_LOG_INFO, "DXX",
		                    "MSAA: default FB bits r=%d g=%d b=%d a=%d -> fmt=0x%x",
		                    rb, gb, bb, ab, color_fmt);
	}

	glGenFramebuffers(1, &state->fbo);
	glGenRenderbuffers(1, &state->color_rbo);
	glGenRenderbuffers(1, &state->depth_rbo);

	glBindRenderbuffer(GL_RENDERBUFFER, state->color_rbo);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, color_fmt, w, h);

	glBindRenderbuffer(GL_RENDERBUFFER, state->depth_rbo);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT16, w, h);

	glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                          GL_RENDERBUFFER, state->color_rbo);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
	                          GL_RENDERBUFFER, state->depth_rbo);

	{
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		if (status != GL_FRAMEBUFFER_COMPLETE) {
			__android_log_print(ANDROID_LOG_ERROR, "DXX",
			                    "MSAA FBO incomplete: status=0x%x samples=%d %dx%d fmt=0x%x",
			                    status, samples, w, h, color_fmt);
			snprintf(logbuf, sizeof(logbuf),
			         "MSAA FBO incomplete: status=0x%x samples=%d size=%dx%d fmt=0x%x",
			         status, samples, w, h, color_fmt);
			android_ogl_msaa_log(log_message, log_user_data, logbuf);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			android_ogl_msaa_destroy_fbo(state, bound);
			return 0;
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	state->w = w;
	state->h = h;
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
		if (!state->fbo || state->w != w || state->h != h) {
			char logbuf[160];

			snprintf(logbuf, sizeof(logbuf),
			         "MSAA FBO create request: samples=%d max=%d size=%dx%d",
			         samples, max_samples, w, h);
			android_ogl_msaa_log(log_message, log_user_data, logbuf);
			android_ogl_msaa_create_fbo(state, bound, max_samples, samples,
			                            w, h, log_message, log_user_data);
		}
		if (state->fbo) {
			color_clear = !*bound;
			glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
			*bound = 1;
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

	if (!state || !bound)
		return 0;
	if (*bound && frame_depth == 0) {
		clock_gettime(CLOCK_MONOTONIC, &start);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, state->fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(0, 0, w, h, 0, 0, w, h,
		                  GL_COLOR_BUFFER_BIT, GL_NEAREST);
		{
			GLenum err = glGetError();
			if (err != GL_NO_ERROR)
				__android_log_print(ANDROID_LOG_ERROR, "DXX",
				                    "MSAA resolve error: 0x%x", err);
		}
		*bound = 0;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		clock_gettime(CLOCK_MONOTONIC, &end);
		return (int) ((end.tv_sec - start.tv_sec) * 1000000 +
		              (end.tv_nsec - start.tv_nsec) / 1000);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return 0;
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
