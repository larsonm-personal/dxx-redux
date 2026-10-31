#ifndef DXX_XL_RUNTIME_LIGHTNING_GPU_H
#define DXX_XL_RUNTIME_LIGHTNING_GPU_H
#include "../lightning_render.h"
#include <string>

// Included after the platform GL declarations, like the shared depth owner
namespace xl_compat
{
inline void lightning_gpu_program(GLuint program)
{
#ifdef ANDROID
	gles3_shim_bind_program(program);
#else
	glUseProgram(program);
#endif
}
struct LightningDrawState {
	GLint program, vao, buffer, active, texture, src, dst, src_alpha, dst_alpha, equation, equation_alpha, depth_function;
	GLboolean depth, write, blend, cull;
	GLfloat line_width;
#ifndef ANDROID
	GLboolean alpha, smooth;
#endif
	LightningDrawState()
	{
		glGetIntegerv(GL_CURRENT_PROGRAM, &program);
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
		glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
		glActiveTexture(GL_TEXTURE0);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
		glGetIntegerv(GL_BLEND_SRC_RGB, &src);
		glGetIntegerv(GL_BLEND_DST_RGB, &dst);
		glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_alpha);
		glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_alpha);
		glGetIntegerv(GL_BLEND_EQUATION_RGB, &equation);
		glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &equation_alpha);
		glGetIntegerv(GL_DEPTH_FUNC, &depth_function);
		glGetBooleanv(GL_DEPTH_WRITEMASK, &write);
		glGetFloatv(GL_LINE_WIDTH, &line_width);
		depth = glIsEnabled(GL_DEPTH_TEST);
		blend = glIsEnabled(GL_BLEND);
		cull = glIsEnabled(GL_CULL_FACE);
#ifndef ANDROID
		alpha = glIsEnabled(GL_ALPHA_TEST);
		smooth = glIsEnabled(GL_LINE_SMOOTH);
#endif
	}
	~LightningDrawState()
	{
		lightning_gpu_program(program);
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture);
		glActiveTexture(active);
		glBlendFuncSeparate(src, dst, src_alpha, dst_alpha);
		glBlendEquationSeparate(equation, equation_alpha);
		glDepthFunc(depth_function);
		glDepthMask(write);
		glLineWidth(line_width);
		if (depth) glEnable(GL_DEPTH_TEST);
		else glDisable(GL_DEPTH_TEST);
		if (blend) glEnable(GL_BLEND);
		else glDisable(GL_BLEND);
		if (cull) glEnable(GL_CULL_FACE);
		else glDisable(GL_CULL_FACE);
#ifndef ANDROID
		if (alpha) glEnable(GL_ALPHA_TEST);
		else glDisable(GL_ALPHA_TEST);
		if (smooth) glEnable(GL_LINE_SMOOTH);
		else glDisable(GL_LINE_SMOOTH);
#endif
	}
};

inline GLuint lightning_gpu_compile(GLenum type, const std::string &source)
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
		throw std::runtime_error(std::string("XL lightning shader compilation failed: ") + log);
	}
	return shader;
}

class LightningGpu
{
	GLuint program = 0, vao = 0, buffer = 0;
	using Vertex = LightningClipVertex;
	void prepare()
	{
		if (program) return;
#ifdef ANDROID
		const std::string prefix = "#version 100\nprecision highp float;\n";
#else
		const std::string prefix = "#version 120\n";
#endif
		const auto vertex = lightning_gpu_compile(GL_VERTEX_SHADER, prefix + "attribute vec4 position; attribute vec2 uv; uniform mat4 mvp; uniform vec2 coronaMap; varying vec2 glowUV; void main(){glowUV=uv*coronaMap.x+coronaMap.y;gl_Position=mvp*position;}");
		GLuint fragment = 0, linked = 0;
		try {
			fragment = lightning_gpu_compile(GL_FRAGMENT_SHADER, prefix + "uniform sampler2D corona; uniform vec4 color; uniform int textured; varying vec2 glowUV; void main(){gl_FragColor=textured!=0?texture2D(corona,glowUV)*color:color;}");
			linked = glCreateProgram();
			glAttachShader(linked, vertex);
			glAttachShader(linked, fragment);
			glBindAttribLocation(linked, 0, "position");
			glBindAttribLocation(linked, 1, "uv");
			glLinkProgram(linked);
			GLint ok;
			glGetProgramiv(linked, GL_LINK_STATUS, &ok);
			if (!ok) throw std::runtime_error("XL lightning program link failed");
			glGenVertexArrays(1, &vao);
			glGenBuffers(1, &buffer);
			program = linked;
		} catch (...) {
			if (fragment) glDeleteShader(fragment);
			if (linked) glDeleteProgram(linked);
			glDeleteShader(vertex);
			throw;
		}
		glDeleteShader(vertex);
		glDeleteShader(fragment);
	}
	void upload(const std::vector<Vertex> &vertices, GLenum mode)
	{
		glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(vertices.size() * sizeof(Vertex)), vertices.data(), GL_STREAM_DRAW);
#ifdef ANDROID
#undef glDrawArrays
		glDrawArrays(mode, 0, GLsizei(vertices.size()));
#define glDrawArrays gles3_shim_draw_arrays
#else
		glDrawArrays(mode, 0, GLsizei(vertices.size()));
#endif
	}

  public:
	LightningGpu() = default;
	LightningGpu(const LightningGpu &) = delete;
	LightningGpu &operator=(const LightningGpu &) = delete;
	void discard(bool abandoned)
	{
		if (!abandoned) {
			if (program) glDeleteProgram(program);
			if (vao) glDeleteVertexArrays(1, &vao);
			if (buffer) glDeleteBuffers(1, &buffer);
		}
		program = vao = buffer = 0;
	}
	void draw(const LightningCore &core, const LightningGlow &glow, int random_nodes, const std::array<float, 4> &color,
	          float width, const std::array<float, 16> &matrix, GLuint corona, bool blur, const std::array<float, 2> &corona_map = { 1, 0 })
	{
		if (core.vertices.size() < 6 || random_nodes < 0 || std::size_t(random_nodes) + 6 > core.vertices.size() || glow.vertices.size() != glow.uv.size())
			throw std::runtime_error("XL lightning GPU received inconsistent geometry");
		const auto count = core.vertices.size() - 4 - random_nodes;
		const auto layer = (count - 1) * 4;
		if (!corona || glow.vertices.size() != layer * 2) throw std::runtime_error("XL lightning glow requires two layers and its corona texture");
		// Source UVs are bounded: border clamp matches GL_CLAMP without changing LOD gradients
		for (const auto &uv : glow.uv)
			for (float value : uv)
				if (!std::isfinite(value) || value < 0 || value > 1) throw std::runtime_error("XL lightning UV exceeds its source range");
		LightningDrawState restore;
		prepare();
		lightning_gpu_program(program);
		glUniformMatrix4fv(glGetUniformLocation(program, "mvp"), 1, GL_FALSE, matrix.data());
		glUniform1i(glGetUniformLocation(program, "corona"), 0);
		glUniform2fv(glGetUniformLocation(program, "coronaMap"), 1, corona_map.data());
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, corona);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE);
		glDisable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		glBlendEquation(blur ? GL_MAX : GL_FUNC_ADD);
#ifndef ANDROID
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_LINE_SMOOTH);
#endif
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, position)));
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, uv)));
		glUniform1i(glGetUniformLocation(program, "textured"), 1);
		glUniform4f(glGetUniformLocation(program, "color"), color[0] / 2, color[1] / 2, color[2] / 2, 1);
		std::vector<Vertex> vertices;
		const std::array<float, 16> identity{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		glUniformMatrix4fv(glGetUniformLocation(program, "mvp"), 1, GL_FALSE, identity.data());
		vertices.reserve(layer * 3 / 2);
		for (int pass = 1; pass >= 0; --pass) {
			vertices.clear();
			std::vector<Vertex> transformed;
			transformed.reserve(layer);
			bool clipped = false;
			for (std::size_t i = pass * layer; i < (pass + 1) * layer; ++i) {
				transformed.push_back(lightning_clip_vertex(glow.vertices[i], glow.uv[i], matrix));
				clipped = clipped || lightning_outside_clip(transformed.back());
			}
			// Match the reference GL_QUADS batch: clipping selects its alternate diagonal
			const std::array<unsigned, 6> indices = clipped ? std::array<unsigned, 6>{ 3, 0, 1, 3, 1, 2 } : std::array<unsigned, 6>{ 0, 1, 2, 0, 2, 3 };
			for (std::size_t quad = 0; quad < layer; quad += 4)
				for (auto corner : indices) vertices.push_back(transformed[quad + corner]);
			upload(vertices, GL_TRIANGLES);
		}
		glUniform1i(glGetUniformLocation(program, "textured"), 0);
		glUniformMatrix4fv(glGetUniformLocation(program, "mvp"), 1, GL_FALSE, matrix.data());
		glUniform4fv(glGetUniformLocation(program, "color"), 1, color.data());
		const float line = blur && glow.distance_scale != 0 ? width * (2.0f * glow.distance_scale) : width;
		glLineWidth(line > 1 ? line : 1);
#ifndef ANDROID
		glEnable(GL_LINE_SMOOTH);
#endif
		vertices.clear();
		for (std::size_t i = 0; i < count; ++i) vertices.push_back({ { core.vertices[i][0], core.vertices[i][1], core.vertices[i][2], 1 }, { 0, 0 } });
		upload(vertices, GL_LINE_STRIP);
		if (glGetError() != GL_NO_ERROR) throw std::runtime_error("XL lightning GPU draw failed");
	}
};
} // namespace xl_compat
#endif
