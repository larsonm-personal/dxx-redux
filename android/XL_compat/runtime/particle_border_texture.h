#ifndef DXX_XL_PARTICLE_BORDER_TEXTURE_H
#define DXX_XL_PARTICLE_BORDER_TEXTURE_H
namespace xl_compat
{
inline void particle_border_program(GLuint program)
{
#ifdef ANDROID
	gles3_shim_bind_program(program);
#else
	glUseProgram(program);
#endif
}
// Copy each original mip into a four-times-larger transparent image. At level L
// the source occupies [N,N*2) in a 4N square, including the final 1x1 source mip
// Sampling uv/4+1/4 preserves the source footprint and implicit LOD gradients
inline GLuint copy_particle_border_texture(GLuint source, int side)
{
	GLint maximum;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
	if (side <= 0 || (side & (side - 1)) || side > maximum / 4) throw std::runtime_error("XL corona border storage requires a bounded power-of-two square");
	struct Restore {
		GLint read, draw, active, binding, unpack, program, viewport[4];
		GLfloat clear[4];
		GLboolean mask[4], enabled[6];
		const std::array<GLenum, 6> capabilities{ GL_SCISSOR_TEST, GL_DEPTH_TEST, GL_STENCIL_TEST, GL_BLEND, GL_CULL_FACE, GL_DITHER };
#ifndef ANDROID
		GLboolean alpha;
#endif
		Restore()
		{
			glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
			glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
			glActiveTexture(GL_TEXTURE0);
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
			glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);
			glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
			glGetBooleanv(GL_COLOR_WRITEMASK, mask);
			glGetIntegerv(GL_CURRENT_PROGRAM, &program);
			glGetIntegerv(GL_VIEWPORT, viewport);
			for (unsigned i = 0; i < capabilities.size(); ++i) enabled[i] = glIsEnabled(capabilities[i]);
#ifndef ANDROID
			alpha = glIsEnabled(GL_ALPHA_TEST);
#endif
		}
		~Restore()
		{
			glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
			glBindTexture(GL_TEXTURE_2D, binding);
			glActiveTexture(active);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack);
			glClearColor(clear[0], clear[1], clear[2], clear[3]);
			glColorMask(mask[0], mask[1], mask[2], mask[3]);
			particle_border_program(program);
			glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
			for (unsigned i = 0; i < capabilities.size(); ++i) {
				if (enabled[i]) glEnable(capabilities[i]);
				else glDisable(capabilities[i]);
			}
#ifndef ANDROID
			if (alpha) glEnable(GL_ALPHA_TEST);
			else glDisable(GL_ALPHA_TEST);
#endif
		}
	} restore;
	GLint min_filter, mag_filter;
	glBindTexture(GL_TEXTURE_2D, source);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &min_filter);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &mag_filter);
	GLuint texture = 0, framebuffer = 0, program = 0, vertex = 0, fragment = 0;
	try {
#ifdef ANDROID
		const std::string prefix = "#version 300 es\nprecision highp float; precision highp int; precision highp sampler2D;\n";
#else
		const std::string prefix = "#version 130\n";
#endif
		const auto compile = [&](GLenum type, const std::string &body) {
			const auto text = prefix + body;
			const char *source_text = text.c_str();
			const GLuint shader = glCreateShader(type);
			glShaderSource(shader, 1, &source_text, nullptr);
			glCompileShader(shader);
			GLint ok;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
			if (!ok) {
				char log[512]{};
				glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
				glDeleteShader(shader);
				throw std::runtime_error(std::string("XL corona copy shader failed: ") + log);
			}
			return shader;
		};
		vertex = compile(GL_VERTEX_SHADER, "void main(){gl_Position=vec4(gl_VertexID==1?3.0:-1.0,gl_VertexID==2?3.0:-1.0,0.0,1.0);}");
		fragment = compile(GL_FRAGMENT_SHADER, "uniform sampler2D original; uniform int side; uniform int level; out vec4 pixel; void main(){pixel=texelFetch(original,ivec2(gl_FragCoord.xy)-ivec2(side),level);}");
		program = glCreateProgram();
		glAttachShader(program, vertex);
		glAttachShader(program, fragment);
		glLinkProgram(program);
		GLint linked;
		glGetProgramiv(program, GL_LINK_STATUS, &linked);
		if (!linked) throw std::runtime_error("XL corona copy program did not link");
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		int last = 0;
		for (int size = side; size > 1; size /= 2) ++last;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, last);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		for (int level = 0, size = side; level <= last; ++level, size /= 2)
			glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA8, size * 4, size * 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glGenFramebuffers(1, &framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		for (auto capability : restore.capabilities) glDisable(capability);
#ifndef ANDROID
		glDisable(GL_ALPHA_TEST);
#endif
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glClearColor(0, 0, 0, 0);
		particle_border_program(program);
		glUniform1i(glGetUniformLocation(program, "original"), 0);
		glBindTexture(GL_TEXTURE_2D, source);
		for (int level = 0, size = side; level <= last; ++level, size /= 2) {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, level);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error("XL corona border target is incomplete");
			glClear(GL_COLOR_BUFFER_BIT);
			// Some ES translators cannot attach automatically generated source mips
			// Integer texel fetch copies their actual colors without filtering
			glViewport(size, size, size, size);
			glUniform1i(glGetUniformLocation(program, "side"), size);
			glUniform1i(glGetUniformLocation(program, "level"), level);
#ifdef ANDROID
#undef glDrawArrays
			glDrawArrays(GL_TRIANGLES, 0, 3);
#define glDrawArrays gles3_shim_draw_arrays
#else
			glDrawArrays(GL_TRIANGLES, 0, 3);
#endif
		}
		if (glGetError() != GL_NO_ERROR) throw std::runtime_error("XL corona border copy failed");
		glDeleteFramebuffers(1, &framebuffer);
		glDeleteProgram(program);
		glDeleteShader(vertex);
		glDeleteShader(fragment);
		return texture;
	} catch (...) {
		if (framebuffer) glDeleteFramebuffers(1, &framebuffer);
		if (texture) glDeleteTextures(1, &texture);
		if (program) glDeleteProgram(program);
		if (vertex) glDeleteShader(vertex);
		if (fragment) glDeleteShader(fragment);
		throw;
	}
}
} // namespace xl_compat
#endif
