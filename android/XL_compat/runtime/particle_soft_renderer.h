#include "../particle_soft_shader.h"
#include "particle_depth.h"

namespace
{
struct ParticleSoft {
	ParticleDepth depth;
	GLuint program = 0, vao = 0, buffer = 0;
	bool active = false, captured = false, verified = false;
	void discard(bool abandoned)
	{
		depth.discard(abandoned);
		if (!abandoned) {
			if (program) glDeleteProgram(program);
			if (vao) glDeleteVertexArrays(1, &vao);
			if (buffer) glDeleteBuffers(1, &buffer);
		}
		*this = {};
	}
} particle_soft;

void particle_program(GLuint program)
{
#ifdef ANDROID
	gles3_shim_bind_program(program);
#else
	glUseProgram(program);
#endif
}

struct ParticleSoftRestore {
	GLint program, vao, buffer, active, textures[2], src, dst, src_alpha, dst_alpha, equation, equation_alpha;
	GLboolean depth, write, blend, cull;
#ifndef ANDROID
	GLboolean alpha;
#endif
	ParticleSoftRestore()
	{
		glGetIntegerv(GL_CURRENT_PROGRAM, &program);
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
		glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
		for (int i = 0; i < 2; ++i) {
			glActiveTexture(GL_TEXTURE0 + i);
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &textures[i]);
		}
		glActiveTexture(active);
		glGetIntegerv(GL_BLEND_SRC_RGB, &src);
		glGetIntegerv(GL_BLEND_DST_RGB, &dst);
		glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_alpha);
		glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_alpha);
		glGetIntegerv(GL_BLEND_EQUATION_RGB, &equation);
		glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &equation_alpha);
		glGetBooleanv(GL_DEPTH_WRITEMASK, &write);
		depth = glIsEnabled(GL_DEPTH_TEST);
		blend = glIsEnabled(GL_BLEND);
		cull = glIsEnabled(GL_CULL_FACE);
#ifndef ANDROID
		alpha = glIsEnabled(GL_ALPHA_TEST);
#endif
	}
	~ParticleSoftRestore()
	{
		particle_program(program);
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		for (int i = 0; i < 2; ++i) {
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D, textures[i]);
		}
		glActiveTexture(active);
		glBlendFuncSeparate(src, dst, src_alpha, dst_alpha);
		glBlendEquationSeparate(equation, equation_alpha);
		glDepthMask(write);
		if (depth) glEnable(GL_DEPTH_TEST);
		else glDisable(GL_DEPTH_TEST);
		if (blend) glEnable(GL_BLEND);
		else glDisable(GL_BLEND);
		if (cull) glEnable(GL_CULL_FACE);
		else glDisable(GL_CULL_FACE);
#ifndef ANDROID
		if (alpha) glEnable(GL_ALPHA_TEST);
		else glDisable(GL_ALPHA_TEST);
#endif
	}
};

GLuint particle_shader(GLenum type, const std::string &source)
{
	const GLuint shader = glCreateShader(type);
	const char *text = source.c_str();
	glShaderSource(shader, 1, &text, nullptr);
	glCompileShader(shader);
	GLint ok;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[1024]{};
		glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
		glDeleteShader(shader);
		throw std::runtime_error(std::string("XL particle shader compilation failed: ") + log);
	}
	return shader;
}

void prepare_particle_soft()
{
	if (particle_soft.program) return;
#ifdef ANDROID
	const std::string prefix = "#version 100\nprecision highp float; precision highp sampler2D;\n";
#else
	const std::string prefix = "#version 120\n";
#endif
	const GLuint vertex = particle_shader(GL_VERTEX_SHADER, prefix + "attribute vec3 position; attribute vec2 uv; attribute vec4 color; uniform mat4 mvp; varying vec2 particleUV; varying vec4 particleColor; void main() { particleUV = uv; particleColor = color; gl_Position = mvp * vec4(position, 1.0); }");
	GLuint fragment = 0, program = 0;
	try {
		fragment = particle_shader(GL_FRAGMENT_SHADER, prefix + xl_particle_soft_glsl);
		program = glCreateProgram();
		glAttachShader(program, vertex);
		glAttachShader(program, fragment);
		glBindAttribLocation(program, 0, "position");
		glBindAttribLocation(program, 1, "uv");
		glBindAttribLocation(program, 2, "color");
		glLinkProgram(program);
		GLint ok;
		glGetProgramiv(program, GL_LINK_STATUS, &ok);
		if (!ok) throw std::runtime_error("XL particle shader link failed");
		glGenVertexArrays(1, &particle_soft.vao);
		glGenBuffers(1, &particle_soft.buffer);
		particle_soft.program = program;
	} catch (...) {
		if (fragment) glDeleteShader(fragment);
		if (program) glDeleteProgram(program);
		glDeleteShader(vertex);
		throw;
	}
	glDeleteShader(vertex);
	glDeleteShader(fragment);
}

#include "particle_depth_probe.h"

void draw_soft_particles(grs_bitmap &image, const xl_compat::ParticleQuad *quads, std::size_t count)
{
	ParticleSoftRestore restore;
	prepare_particle_soft();
	verify_particle_soft_depth();
	if (!particle_soft.captured) {
		particle_soft.depth.capture();
		particle_soft.captured = true;
	}
	GLfloat matrix[16]{};
#ifdef ANDROID
	std::copy_n(gles3_shim_get_mvp(), 16, matrix);
#else
	GLfloat projection[16], model[16];
	glGetFloatv(GL_PROJECTION_MATRIX, projection);
	glGetFloatv(GL_MODELVIEW_MATRIX, model);
	for (unsigned column = 0; column < 4; ++column)
		for (unsigned row = 0; row < 4; ++row)
			for (unsigned k = 0; k < 4; ++k) matrix[column * 4 + row] += projection[k * 4 + row] * model[column * 4 + k];
	glDisable(GL_ALPHA_TEST);
#endif
	particle_program(particle_soft.program);
	glUniformMatrix4fv(glGetUniformLocation(particle_soft.program, "mvp"), 1, GL_FALSE, matrix);
	glUniform1i(glGetUniformLocation(particle_soft.program, "particleTex"), 0);
	glUniform1i(glGetUniformLocation(particle_soft.program, "depthTex"), 1);
	glUniform2f(glGetUniformLocation(particle_soft.program, "windowScale"), 1.f / particle_soft.depth.width, 1.f / particle_soft.depth.height);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, image.gltexture->handle);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, particle_soft.depth.texture);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBindVertexArray(particle_soft.vao);
	glBindBuffer(GL_ARRAY_BUFFER, particle_soft.buffer);
	using Vertex = xl_compat::ParticleVertex;
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, position)));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, uv)));
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, color)));
	std::vector<Vertex> triangles;
	triangles.reserve((std::min) (count, std::size_t(25000)) * 6);
	for (std::size_t i = 0; i < count;) {
		const bool smoke = quads[i].blend == xl_compat::ParticleBlend::multiply;
		const auto blend = quads[i].blend;
		triangles.clear();
		const auto limit = (std::min) (count - i, std::size_t(25000)) + i;
		// Preserve fan triangle order and blend boundaries; never exceed source capacity
		while (i < limit && quads[i].blend == blend) {
			for (unsigned vertex : { 0u, 1u, 2u, 0u, 2u, 3u }) triangles.push_back(quads[i].vertices[vertex]);
			++i;
		}
		glUniform1i(glGetUniformLocation(particle_soft.program, "blendMode"), smoke ? -1 : 2);
		glBlendFunc(smoke ? GL_DST_COLOR : GL_ONE, smoke ? GL_ZERO : GL_ONE_MINUS_SRC_COLOR);
		glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(triangles.size() * sizeof(Vertex)), triangles.data(), GL_STREAM_DRAW);
#ifdef ANDROID
#undef glDrawArrays
		glDrawArrays(GL_TRIANGLES, 0, GLsizei(triangles.size()));
#define glDrawArrays gles3_shim_draw_arrays
#else
		glDrawArrays(GL_TRIANGLES, 0, GLsizei(triangles.size()));
#endif
	}
	if (glGetError() != GL_NO_ERROR) throw std::runtime_error("XL soft particle draw failed");
}
} // namespace
