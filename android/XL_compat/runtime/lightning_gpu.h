#ifndef DXX_XL_RUNTIME_LIGHTNING_GPU_H
#define DXX_XL_RUNTIME_LIGHTNING_GPU_H
#include "../lightning_render.h"
#include "../lightning_line.h"
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
	GLint viewport[4], scissor_box[4];
	GLboolean depth, write, blend, cull, scissor;
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
		glGetIntegerv(GL_VIEWPORT, viewport);
		glGetIntegerv(GL_SCISSOR_BOX, scissor_box);
		scissor = glIsEnabled(GL_SCISSOR_TEST);
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
		glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
		glScissor(scissor_box[0], scissor_box[1], scissor_box[2], scissor_box[3]);
		if (scissor) glEnable(GL_SCISSOR_TEST);
		else glDisable(GL_SCISSOR_TEST);
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
	GLuint line_program = 0;
	using Vertex = LightningClipVertex;
	void prepare_line()
	{
		if (line_program) return;
#ifdef ANDROID
		const std::string prefix = "#version 100\nprecision highp float;\n";
#else
		const std::string prefix = "#version 120\n";
#endif
		const auto vertex = lightning_gpu_compile(GL_VERTEX_SHADER, prefix + "attribute vec4 position; attribute vec4 coverage; varying vec4 lineCoverage; varying float lineReciprocal; void main(){gl_Position=vec4(position.xyz,1.0);lineCoverage=coverage*position.w;lineReciprocal=position.w;}");
		GLuint fragment = 0, linked = 0;
		try {
			fragment = lightning_gpu_compile(GL_FRAGMENT_SHADER, prefix + "uniform vec4 color; varying vec4 lineCoverage; varying float lineReciprocal; void main(){vec4 coverage=lineCoverage/lineReciprocal;vec2 c=clamp(coverage.yw-abs(coverage.xz),0.0,1.0);c.y=min(c.y,2.0*coverage.w-1.0);gl_FragColor=vec4(color.rgb,color.a*c.x*c.y);}");
			linked = glCreateProgram();
			glAttachShader(linked, vertex);
			glAttachShader(linked, fragment);
			glBindAttribLocation(linked, 0, "position");
			glBindAttribLocation(linked, 1, "coverage");
			glLinkProgram(linked);
			GLint ok;
			glGetProgramiv(linked, GL_LINK_STATUS, &ok);
			if (!ok) throw std::runtime_error("XL lightning line program link failed");
			line_program = linked;
		} catch (...) {
			if (fragment) glDeleteShader(fragment);
			if (linked) glDeleteProgram(linked);
			glDeleteShader(vertex);
			throw;
		}
		glDeleteShader(vertex);
		glDeleteShader(fragment);
	}
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
			if (line_program) glDeleteProgram(line_program);
			if (vao) glDeleteVertexArrays(1, &vao);
			if (buffer) glDeleteBuffers(1, &buffer);
		}
		program = vao = buffer = 0;
		line_program = 0;
	}
	void draw(const LightningCore &core, const LightningGlow &glow, int random_nodes, const std::array<float, 4> &color,
	          float width, const std::array<float, 16> &matrix, GLuint corona, bool blur, const std::array<float, 2> &corona_map = { 1, 0 })
	{
		if (core.vertices.size() < 6 || random_nodes < 0 || std::size_t(random_nodes) + 6 > core.vertices.size() || glow.vertices.size() != glow.uv.size())
			throw std::runtime_error("XL lightning GPU received inconsistent geometry");
		const auto count = core.vertices.size() - 4 - random_nodes;
		const auto layer = (count - 1) * 4;
		const bool draw_glow = glow.distance_scale > 0;
		if (draw_glow && (!corona || glow.vertices.size() != layer * 2)) throw std::runtime_error("XL lightning glow requires two layers and its corona texture");
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
		for (int pass = draw_glow ? 1 : -1; pass >= 0; --pass) {
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
		prepare_line();
		lightning_gpu_program(line_program);
		glUniform4fv(glGetUniformLocation(line_program, "color"), 1, color.data());
		const float line = blur && glow.distance_scale != 0 ? width * (2.0f * glow.distance_scale) : width;
		std::array<int, 4> viewport;
		glGetIntegerv(GL_VIEWPORT, viewport.data());
		GLint maximum_viewport[2];
		glGetIntegerv(GL_MAX_VIEWPORT_DIMS, maximum_viewport);
		const double padding = std::ceil(double((std::max) (line, 1.f)) * .5 + .5) + 2;
		if (!std::isfinite(line) || viewport[2] <= 0 || viewport[3] <= 0 ||
		    padding > double(maximum_viewport[0] - viewport[2]) * .5 ||
		    padding > double(maximum_viewport[1] - viewport[3]) * .5)
			throw std::runtime_error("XL lightning expanded line exceeds viewport limits");
		const int margin = int(padding);
		if (std::int64_t(viewport[0]) - margin < INT32_MIN || std::int64_t(viewport[1]) - margin < INT32_MIN)
			throw std::runtime_error("XL lightning viewport origin overflows");
		const std::array<int, 4> raster_viewport{ viewport[0] - margin, viewport[1] - margin, viewport[2] + margin * 2, viewport[3] + margin * 2 };
		const auto lines = lightning_line_triangles(core.vertices, count, line, matrix, viewport, raster_viewport);
		// Source expands smooth lines after clipping; keep those corners inside
		// the triangle clip volume and restrict fragments to the original view
		const int left = restore.scissor ? (std::max) (viewport[0], restore.scissor_box[0]) : viewport[0];
		const int bottom = restore.scissor ? (std::max) (viewport[1], restore.scissor_box[1]) : viewport[1];
		const auto right = restore.scissor ? (std::min) (std::int64_t(viewport[0]) + viewport[2], std::int64_t(restore.scissor_box[0]) + restore.scissor_box[2]) : std::int64_t(viewport[0]) + viewport[2];
		const auto top = restore.scissor ? (std::min) (std::int64_t(viewport[1]) + viewport[3], std::int64_t(restore.scissor_box[1]) + restore.scissor_box[3]) : std::int64_t(viewport[1]) + viewport[3];
		glEnable(GL_SCISSOR_TEST);
		glScissor(left, bottom, int((std::max) (std::int64_t(0), right - left)), int((std::max) (std::int64_t(0), top - bottom)));
		glViewport(raster_viewport[0], raster_viewport[1], raster_viewport[2], raster_viewport[3]);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(LightningLineVertex), reinterpret_cast<void *>(offsetof(LightningLineVertex, position)));
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(LightningLineVertex), reinterpret_cast<void *>(offsetof(LightningLineVertex, coverage)));
		glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(lines.size() * sizeof(LightningLineVertex)), lines.data(), GL_STREAM_DRAW);
#ifdef ANDROID
#undef glDrawArrays
		glDrawArrays(GL_TRIANGLES, 0, GLsizei(lines.size()));
#define glDrawArrays gles3_shim_draw_arrays
#else
		glDrawArrays(GL_TRIANGLES, 0, GLsizei(lines.size()));
#endif
		if (glGetError() != GL_NO_ERROR) throw std::runtime_error("XL lightning GPU draw failed");
	}
};
} // namespace xl_compat
#endif
