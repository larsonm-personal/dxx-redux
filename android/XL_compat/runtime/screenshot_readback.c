#ifdef _WIN32
#include <winsock2.h>
#endif
#include "internal.h"
#include "screenshot_readback.h"

#ifndef OGLES
void xl_screenshot_readback_begin(xl_screenshot_readback_state *state)
{
	GLint capture_framebuffer = 0;
	GLboolean double_buffered = GL_FALSE;
	state->previous_read_buffer = GL_FRONT;
	state->previous_read_framebuffer = 0;
	state->capture_read_buffer = GL_FRONT;
	state->framebuffer_readback = GLEW_VERSION_3_0 || GLEW_ARB_framebuffer_object;
	/* Read the rendered target without changing either framebuffer's read state */
	glGetIntegerv(GL_READ_BUFFER, &state->previous_read_buffer);
	if (state->framebuffer_readback) {
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &state->previous_read_framebuffer);
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &capture_framebuffer);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, capture_framebuffer);
		glGetIntegerv(GL_READ_BUFFER, &state->capture_read_buffer);
	}
	if (capture_framebuffer)
		glReadBuffer(GL_COLOR_ATTACHMENT0);
	else {
		glGetBooleanv(GL_DOUBLEBUFFER, &double_buffered);
#ifdef DXX_XL_DEVELOPER_PROBE
		glReadBuffer(double_buffered ? GL_BACK : GL_FRONT);
#else
		glReadBuffer(GL_FRONT);
#endif
	}
}

void xl_screenshot_readback_end(const xl_screenshot_readback_state *state)
{
	if (state->framebuffer_readback) {
		glReadBuffer(state->capture_read_buffer);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, state->previous_read_framebuffer);
	}
	glReadBuffer(state->previous_read_buffer);
}
#endif
