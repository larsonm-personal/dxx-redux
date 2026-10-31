#ifndef DXX_XL_RUNTIME_LIGHTNING_GLOW_GPU_H
#define DXX_XL_RUNTIME_LIGHTNING_GLOW_GPU_H
#include "lightning_gpu.h"
#include "../lightning_blur_shader.h"

namespace xl_compat
{
struct LightningGlowState {
	LightningDrawState draw;
	GLint read, target, viewport[4], unpack;
	GLboolean scissor, stencil, mask[4];
	GLfloat clear[4];
	LightningGlowState()
	{
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &target);
		glGetIntegerv(GL_VIEWPORT, viewport);
		glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);
		glGetBooleanv(GL_COLOR_WRITEMASK, mask);
		glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
		scissor = glIsEnabled(GL_SCISSOR_TEST);
		stencil = glIsEnabled(GL_STENCIL_TEST);
	}
	~LightningGlowState()
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target);
		glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack);
		glColorMask(mask[0], mask[1], mask[2], mask[3]);
		glClearColor(clear[0], clear[1], clear[2], clear[3]);
		if (scissor) glEnable(GL_SCISSOR_TEST);
		else glDisable(GL_SCISSOR_TEST);
		if (stencil) glEnable(GL_STENCIL_TEST);
		else glDisable(GL_STENCIL_TEST);
	}
};

// Depth is a caller-owned snapshot retained until end; color buffers are private
class LightningGlowGpu
{
	LightningGpu primitive;
	GLuint colors[3]{}, targets[3]{}, blur_program = 0, copy_program = 0, vao = 0, buffer = 0;
	GLuint scene = 0, depth = 0;
	GLenum depth_attachment = GL_DEPTH_ATTACHMENT;
	int width = 0, height = 0;
	std::array<int, 4> view{}, bounds{};
	bool active = false, visible = false, has_extent = false;
	static GLuint program(const std::string &fragment_body)
	{
#ifdef ANDROID
		const std::string prefix = "#version 100\nprecision highp float;\n";
#else
		const std::string prefix = "#version 120\n";
#endif
		const GLuint vertex = lightning_gpu_compile(GL_VERTEX_SHADER, prefix + "attribute vec2 position; attribute vec2 uv; varying vec2 glowUV; void main(){glowUV=uv;gl_Position=vec4(position,0.0,1.0);}");
		GLuint fragment = 0, linked = 0;
		try {
			fragment = lightning_gpu_compile(GL_FRAGMENT_SHADER, prefix + fragment_body);
			linked = glCreateProgram();
			glAttachShader(linked, vertex);
			glAttachShader(linked, fragment);
			glBindAttribLocation(linked, 0, "position");
			glBindAttribLocation(linked, 1, "uv");
			glLinkProgram(linked);
			GLint ok;
			glGetProgramiv(linked, GL_LINK_STATUS, &ok);
			if (!ok) throw std::runtime_error("XL lightning glow program link failed");
		} catch (...) {
			if (linked) glDeleteProgram(linked);
			if (fragment) glDeleteShader(fragment);
			glDeleteShader(vertex);
			throw;
		}
		glDeleteShader(vertex);
		glDeleteShader(fragment);
		return linked;
	}
	void prepare(int w, int h)
	{
		if (!blur_program) blur_program = program(xl_lightning_blur_glsl);
		if (!copy_program) copy_program = program("uniform sampler2D glowSource; varying vec2 glowUV; void main(){gl_FragColor=texture2D(glowSource,glowUV);}");
		if (!vao) glGenVertexArrays(1, &vao);
		if (!buffer) glGenBuffers(1, &buffer);
		if (width == w && height == h && colors[0]) return;
		glDeleteTextures(3, colors);
		glDeleteFramebuffers(3, targets);
		glGenTextures(3, colors);
		glGenFramebuffers(3, targets);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		for (unsigned i = 0; i < 3; ++i) {
			glBindTexture(GL_TEXTURE_2D, colors[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindFramebuffer(GL_FRAMEBUFFER, targets[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colors[i], 0);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				throw std::runtime_error("XL lightning glow color target is incomplete");
		}
		width = w;
		height = h;
	}
	void select(GLuint target)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, target);
		glViewport(view[0], view[1], view[2], view[3]);
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_STENCIL_TEST);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}
	void rectangle(GLuint source, int direction, int radius)
	{
		const float l = float((std::max) (0, bounds[0]) - radius), r = float((std::min) (view[2], bounds[2]) + radius);
		const float b = float((std::max) (0, bounds[1]) - radius), t = float((std::min) (view[3], bounds[3]) + radius);
		const auto clamp = [](float value) { return (std::max) (0.0f, (std::min) (1.0f, value)); };
		const auto vertex = [&](float x, float y) {
			return std::array<float, 4>{ 2 * clamp(x / view[2]) - 1, 2 * clamp(y / view[3]) - 1,
				                         clamp((x + view[0]) / width), clamp((y + view[1]) / height) };
		};
		const std::array<std::array<float, 4>, 6> vertices{ vertex(l, b), vertex(r, b), vertex(r, t), vertex(l, b), vertex(r, t), vertex(l, t) };
		const GLuint shader = direction < 0 ? copy_program : blur_program;
		lightning_gpu_program(shader);
		glUniform1i(glGetUniformLocation(shader, "glowSource"), 0);
		if (direction >= 0) {
			glUniform1f(glGetUniformLocation(shader, "direction"), float(direction));
			glUniform1f(glGetUniformLocation(shader, "scale"), 1.0f / (direction ? height : width));
			glUniform1f(glGetUniformLocation(shader, "brightness"), 1.05f);
		}
		glBindTexture(GL_TEXTURE_2D, source);
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STREAM_DRAW);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertices[0]), nullptr);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertices[0]), reinterpret_cast<void *>(2 * sizeof(float)));
#ifdef ANDROID
#undef glDrawArrays
		glDrawArrays(GL_TRIANGLES, 0, 6);
#define glDrawArrays gles3_shim_draw_arrays
#else
		glDrawArrays(GL_TRIANGLES, 0, 6);
#endif
	}

  public:
	bool batch_active() const
	{
		return active;
	}
	LightningGlowGpu() = default;
	LightningGlowGpu(const LightningGlowGpu &) = delete;
	LightningGlowGpu &operator=(const LightningGlowGpu &) = delete;
	void discard(bool abandoned)
	{
		primitive.discard(abandoned);
		if (!abandoned) {
			if (colors[0]) glDeleteTextures(3, colors);
			if (targets[0]) glDeleteFramebuffers(3, targets);
			if (blur_program) glDeleteProgram(blur_program);
			if (copy_program) glDeleteProgram(copy_program);
			if (vao) glDeleteVertexArrays(1, &vao);
			if (buffer) glDeleteBuffers(1, &buffer);
		}
		for (auto &value : colors) value = 0;
		for (auto &value : targets) value = 0;
		blur_program = copy_program = vao = buffer = scene = depth = 0;
		width = height = 0;
		active = visible = has_extent = false;
	}
	void begin(GLuint scene_target, GLuint depth_texture, GLenum attachment, int w, int h, const std::array<int, 4> &viewport)
	{
		if (!depth_texture || (attachment != GL_DEPTH_ATTACHMENT && attachment != GL_DEPTH_STENCIL_ATTACHMENT) || w <= 0 || h <= 0 || w > 16384 || h > 16384 || viewport[0] < 0 || viewport[1] < 0 || viewport[2] <= 0 || viewport[3] <= 0 || viewport[0] > w - viewport[2] || viewport[1] > h - viewport[3])
			throw std::runtime_error("XL lightning glow requires a bounded depth-backed viewport");
		if (active && scene == scene_target && depth == depth_texture && depth_attachment == attachment && width == w && height == h && view == viewport) return;
		if (active) end();
		LightningGlowState restore;
		prepare(w, h);
		scene = scene_target;
		depth = depth_texture;
		depth_attachment = attachment;
		view = viewport;
		select(targets[0]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, depth, 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error("XL lightning glow depth target is incomplete");
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		active = true;
		visible = has_extent = false;
	}
	bool draw(const LightningCore &core, const LightningGlow &glow, int random_nodes, const std::array<float, 4> &color,
	          float line_width, const std::array<float, 16> &matrix, GLuint corona, const std::array<int, 4> &extent, const std::array<float, 2> &corona_map = { 1, 0 })
	{
		if (!active) throw std::runtime_error("XL lightning glow draw requires an active batch");
		if (extent[0] > extent[2] || extent[1] > extent[3]) throw std::runtime_error("XL lightning glow extent is inverted");
		if (!has_extent) bounds = extent;
		else {
			bounds[0] = (std::min) (bounds[0], extent[0]);
			bounds[1] = (std::min) (bounds[1], extent[1]);
			bounds[2] = (std::max) (bounds[2], extent[2]);
			bounds[3] = (std::max) (bounds[3], extent[3]);
		}
		has_extent = true;
		visible = !(bounds[2] < 0 || bounds[0] >= view[2] || bounds[3] < 0 || bounds[1] >= view[3]);
		LightningGlowState restore;
		select(targets[0]);
		// Source Draw rejects SetViewport < 0, but this overload returns only 0 or 1
		// Keep hidden input: a later member can make the cumulative batch visible
		primitive.draw(core, glow, random_nodes, color, line_width, matrix, corona, true, corona_map);
		return visible;
	}
	bool end()
	{
		if (!active) return false;
		active = false;
		LightningGlowState restore;
		// Do not retain the caller's depth allocation after this batch
		glBindFramebuffer(GL_FRAMEBUFFER, targets[0]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
		depth = 0;
		if (!visible) return false;
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glDisable(GL_CULL_FACE);
#ifndef ANDROID
		glDisable(GL_ALPHA_TEST);
#endif
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		for (unsigned pass = 0; pass < 6; ++pass) {
			select(targets[1 + pass % 2]);
			if (pass < 2) {
				glClearColor(0, 0, 0, 1);
				glClear(GL_COLOR_BUFFER_BIT);
			}
			glBlendFunc(GL_ONE, pass < 2 ? GL_ZERO : GL_ONE);
			rectangle(colors[pass ? 1 + (pass - 1) % 2 : 0], int(pass % 2), 3 * (1 + pass / 2));
		}
		select(scene);
		glBlendFunc(GL_ONE, GL_ONE);
		rectangle(colors[2], -1, 9);
		if (glGetError() != GL_NO_ERROR) throw std::runtime_error("XL lightning glow composition failed");
		visible = false;
		return true;
	}
};
} // namespace xl_compat
#endif
