#include "../particle_multi_shader.h"

namespace
{
struct ParticleMulti {
	GLuint program = 0, vao = 0, buffer = 0;
	void discard(bool abandoned)
	{
		if (!abandoned) {
			if (program) glDeleteProgram(program);
			if (vao) glDeleteVertexArrays(1, &vao);
			if (buffer) glDeleteBuffers(1, &buffer);
		}
		*this = {};
	}
} particle_multi;

void prepare_particle_multi()
{
	if (particle_multi.program) return;
#ifdef ANDROID
	const std::string prefix = "#version 100\nprecision highp float; precision highp sampler2D;\n";
#else
	const std::string prefix = "#version 120\n";
#endif
	const GLuint vertex = particle_shader(GL_VERTEX_SHADER, prefix + "attribute vec3 position; attribute vec3 uv; attribute vec4 color; uniform mat4 mvp; varying vec3 particleUV; varying vec4 particleColor; void main() { particleUV = uv; particleColor = color; gl_Position = mvp * vec4(position, 1.0); }");
	GLuint fragment = 0, program = 0;
	try {
		fragment = particle_shader(GL_FRAGMENT_SHADER, prefix + xl_particle_multi_glsl);
		program = glCreateProgram();
		glAttachShader(program, vertex);
		glAttachShader(program, fragment);
		glBindAttribLocation(program, 0, "position");
		glBindAttribLocation(program, 1, "uv");
		glBindAttribLocation(program, 2, "color");
		glLinkProgram(program);
		GLint ok;
		glGetProgramiv(program, GL_LINK_STATUS, &ok);
		if (!ok) throw std::runtime_error("XL multi-image particle shader link failed");
		glGenVertexArrays(1, &particle_multi.vao);
		glGenBuffers(1, &particle_multi.buffer);
		particle_multi.program = program;
	} catch (...) {
		if (fragment) glDeleteShader(fragment);
		if (program) glDeleteProgram(program);
		glDeleteShader(vertex);
		throw;
	}
	glDeleteShader(vertex);
	glDeleteShader(fragment);
}

void draw_particle_multi(GLuint smoke, GLuint bubble, const xl_compat::ParticleQuad *quads, std::size_t count, bool soft)
{
	ParticleSoftRestore restore;
	prepare_particle_multi();
	if (soft) {
		prepare_particle_soft();
		verify_particle_soft_depth();
	}
	ParticleDepthRead depth(particle_soft.depth, particle_soft.captured, soft);
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
	const GLuint program = particle_multi.program;
	particle_program(program);
	glUniformMatrix4fv(glGetUniformLocation(program, "mvp"), 1, GL_FALSE, matrix);
	glUniform1i(glGetUniformLocation(program, "particleTex"), 0);
	glUniform1i(glGetUniformLocation(program, "sparkTex"), 1);
	glUniform1i(glGetUniformLocation(program, "bubbleTex"), 2);
	glUniform1i(glGetUniformLocation(program, "depthTex"), 3);
	glUniform1i(glGetUniformLocation(program, "softDepth"), soft ? 1 : 0);
	glUniform1i(glGetUniformLocation(program, "bSuspended"), 0);
	glUniform3f(glGetUniformLocation(program, "dMax"), 5, 3, 5);
	if (soft) glUniform2f(glGetUniformLocation(program, "windowScale"), 1.f / depth.width, 1.f / depth.height);
	// The active source table uses smoke.tga for both smoke and spark samplers
	const GLuint images[]{ smoke, smoke, bubble, soft ? depth.texture : smoke };
	for (unsigned unit = 0; unit < 4; ++unit) {
		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(GL_TEXTURE_2D, images[unit]);
	}
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_SRC_ALPHA);
	glBindVertexArray(particle_multi.vao);
	glBindBuffer(GL_ARRAY_BUFFER, particle_multi.buffer);
	struct Vertex {
		std::array<float, 3> position, uv;
		std::array<float, 4> color;
	};
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, position)));
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, uv)));
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, color)));
	std::vector<Vertex> triangles;
	triangles.reserve((std::min) (count, std::size_t(25000)) * 6);
	for (std::size_t i = 0; i < count;) {
		triangles.clear();
		const auto limit = (std::min) (count - i, std::size_t(25000)) + i;
		for (; i < limit; ++i) {
			if (quads[i].image < 0 || quads[i].image > 2 || quads[i].blend != xl_compat::ParticleBlend::alpha)
				throw std::runtime_error("XL multi-image particle batch contains an incompatible quad");
			for (unsigned corner : { 0u, 1u, 2u, 0u, 2u, 3u }) {
				const auto &vertex = quads[i].vertices[corner];
				triangles.push_back({ vertex.position, { vertex.uv[0], vertex.uv[1], float(quads[i].image) }, vertex.color });
			}
		}
		glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(triangles.size() * sizeof(Vertex)), triangles.data(), GL_STREAM_DRAW);
#ifdef ANDROID
#undef glDrawArrays
		glDrawArrays(GL_TRIANGLES, 0, GLsizei(triangles.size()));
#define glDrawArrays gles3_shim_draw_arrays
#else
		glDrawArrays(GL_TRIANGLES, 0, GLsizei(triangles.size()));
#endif
	}
	if (glGetError() != GL_NO_ERROR) throw std::runtime_error("XL multi-image particle draw failed");
}
} // namespace
