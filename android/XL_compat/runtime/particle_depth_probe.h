// A successful blit alone does not prove this driver's depth transfer works
void verify_particle_soft_depth()
{
	if (particle_soft.verified) return;
	ParticleSoftRestore restore_draw;
	struct RestoreProbe {
		GLint read, draw, renderbuffer, view[4], pack, alignment, row, skip_rows, skip_pixels;
		GLfloat clear;
		GLboolean scissor, mask[4];
		RestoreProbe()
		{
			glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
			glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
			glGetIntegerv(GL_VIEWPORT, view);
			glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack);
			glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
			glGetIntegerv(GL_PACK_ROW_LENGTH, &row);
			glGetIntegerv(GL_PACK_SKIP_ROWS, &skip_rows);
			glGetIntegerv(GL_PACK_SKIP_PIXELS, &skip_pixels);
			glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clear);
			glGetBooleanv(GL_COLOR_WRITEMASK, mask);
			scissor = glIsEnabled(GL_SCISSOR_TEST);
		}
		~RestoreProbe()
		{
			glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
			glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
			glViewport(view[0], view[1], view[2], view[3]);
			glBindBuffer(GL_PIXEL_PACK_BUFFER, pack);
			glPixelStorei(GL_PACK_ALIGNMENT, alignment);
			glPixelStorei(GL_PACK_ROW_LENGTH, row);
			glPixelStorei(GL_PACK_SKIP_ROWS, skip_rows);
			glPixelStorei(GL_PACK_SKIP_PIXELS, skip_pixels);
			glColorMask(mask[0], mask[1], mask[2], mask[3]);
			if (scissor) glEnable(GL_SCISSOR_TEST);
#ifdef ANDROID
			glClearDepthf(clear);
#else
			glClearDepth(clear);
#endif
		}
	} restore_probe;
	struct Objects {
		GLuint shader = 0, vertex = 0, program = 0, fbo = 0, color = 0, depth = 0;
		ParticleDepth snapshot;
		~Objects()
		{
			snapshot.discard(false);
			if (shader) glDeleteShader(shader);
			if (vertex) glDeleteShader(vertex);
			if (program) glDeleteProgram(program);
			glDeleteFramebuffers(1, &fbo);
			glDeleteRenderbuffers(1, &color);
			glDeleteRenderbuffers(1, &depth);
		}
	} probe;
#ifdef ANDROID
	const std::string prefix = "#version 100\nprecision highp float; precision highp sampler2D;\n";
#else
	const std::string prefix = "#version 120\n";
#endif
	probe.vertex = particle_shader(GL_VERTEX_SHADER, prefix + "attribute vec2 position; void main() { gl_Position = vec4(position, 0.0, 1.0); }");
	probe.shader = particle_shader(GL_FRAGMENT_SHADER, prefix + "uniform sampler2D depthTex; void main() { gl_FragColor = vec4(texture2D(depthTex, vec2(0.5)).r); }");
	probe.program = glCreateProgram();
	glAttachShader(probe.program, probe.vertex);
	glAttachShader(probe.program, probe.shader);
	glBindAttribLocation(probe.program, 0, "position");
	glLinkProgram(probe.program);
	GLint linked;
	glGetProgramiv(probe.program, GL_LINK_STATUS, &linked);
	if (!linked) throw std::runtime_error("XL particle depth verification shader failed to link");
	glGenFramebuffers(1, &probe.fbo);
	glGenRenderbuffers(1, &probe.color);
	glGenRenderbuffers(1, &probe.depth);
	glBindFramebuffer(GL_FRAMEBUFFER, probe.fbo);
	glBindRenderbuffer(GL_RENDERBUFFER, probe.color);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 1);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, probe.color);
	glViewport(0, 0, 1, 1);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
#ifndef ANDROID
	glDisable(GL_ALPHA_TEST);
#endif
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
	const GLfloat vertices[8] = { -1, -1, 1, -1, 1, 1, -1, 1 };
	glBindVertexArray(particle_soft.vao);
	glBindBuffer(GL_ARRAY_BUFFER, particle_soft.buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	particle_program(probe.program);
	glUniform1i(glGetUniformLocation(probe.program, "depthTex"), 0);
	for (GLenum format : { GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT24 }) {
		glBindRenderbuffer(GL_RENDERBUFFER, probe.depth);
		glRenderbufferStorage(GL_RENDERBUFFER, format, 1, 1);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, probe.depth);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error("XL particle depth verification target is incomplete");
#ifdef ANDROID
		glClearDepthf(.625f);
#else
		glClearDepth(.625);
#endif
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);
		probe.snapshot.capture();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, probe.snapshot.texture);
#ifdef ANDROID
#undef glDrawArrays
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
#define glDrawArrays gles3_shim_draw_arrays
#else
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
#endif
		unsigned char observed[4]{};
		glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, observed);
		if (glGetError() != GL_NO_ERROR || std::abs(observed[0] / 255.f - .625f) > .004f)
			throw std::runtime_error("XL soft particles require working scene-depth transfer; this graphics driver did not preserve depth");
	}
	particle_soft.verified = true;
}
