#ifdef ANDROID

#include <android/log.h>
#include <stdio.h>

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

#endif