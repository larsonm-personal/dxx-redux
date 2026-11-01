#ifndef DXX_XL_RUNTIME_SHIELD_GPU_H
#define DXX_XL_RUNTIME_SHIELD_GPU_H
#include "lightning_gpu.h"
#include "../shield_sphere.h"
#include "../camera_hit_render.h"
#include "../camera_hit_shader.h"
#include <cstddef>

namespace xl_compat
{
// Caller owns the current frame texture and supplies the complete local-to-clip matrix
class ShieldGpu
{
	GLuint program = 0, vao = 0, buffer = 0;
	int quality = 0;
	GLsizei count = 0;
	void prepare()
	{
		if (program) return;
#ifdef ANDROID
		const std::string prefix = "#version 100\nprecision highp float; precision highp sampler2D;\n";
#else
		const std::string prefix = "#version 120\n";
#endif
		const GLuint vertex = lightning_gpu_compile(GL_VERTEX_SHADER, prefix +
		                                                                  "attribute vec3 position; attribute vec2 uv; uniform mat4 mvp; uniform vec4 color; varying vec3 hitVertex; varying vec2 hitUV; varying vec4 hitColor; void main(){hitVertex=position;hitUV=uv;hitColor=color;gl_Position=mvp*vec4(position,1.0);}");
		GLuint fragment = 0, linked = 0;
		try {
			fragment = lightning_gpu_compile(GL_FRAGMENT_SHADER, prefix + "uniform vec3 textureMap;\n#define XL_SHIELD_UV ((textureMap.z<1.0?clamp(hitUV,vec2(0.0),vec2(textureMap.z)):hitUV)*textureMap.x+textureMap.y)\n" + xl_camera_hit_glsl);
			linked = glCreateProgram();
			glAttachShader(linked, vertex);
			glAttachShader(linked, fragment);
			glBindAttribLocation(linked, 0, "position");
			glBindAttribLocation(linked, 1, "uv");
			glLinkProgram(linked);
			GLint ok;
			glGetProgramiv(linked, GL_LINK_STATUS, &ok);
			if (!ok) throw std::runtime_error("XL shield program link failed");
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

  public:
	ShieldGpu() = default;
	ShieldGpu(const ShieldGpu &) = delete;
	ShieldGpu &operator=(const ShieldGpu &) = delete;
	void discard(bool abandoned)
	{
		if (!abandoned) {
			if (program) glDeleteProgram(program);
			if (vao) glDeleteVertexArrays(1, &vao);
			if (buffer) glDeleteBuffers(1, &buffer);
		}
		program = vao = buffer = 0;
		quality = count = 0;
	}
	void draw(int requested_quality, const CameraHitHardware &hit, const std::array<float, 4> &color,
	          const std::array<float, 16> &matrix, GLuint texture, int border_side = 0, bool nearest = false)
	{
		if (requested_quality < 2 || requested_quality > 4 || !texture)
			throw std::runtime_error("XL shield requires a qualified mesh and frame texture");
		for (float value : color)
			if (!std::isfinite(value)) throw std::runtime_error("XL shield color is not finite");
		for (float value : matrix)
			if (!std::isfinite(value)) throw std::runtime_error("XL shield matrix is not finite");
		for (const auto &vector : hit.vectors)
			for (float value : vector)
				if (!std::isfinite(value)) throw std::runtime_error("XL shield hit vector is not finite");
		for (float value : hit.envelope.inverse_radius)
			if (!std::isfinite(value) || value < 0) throw std::runtime_error("XL shield hit radius is invalid");
		if (border_side < 0 || border_side > 4096 || (border_side && (border_side & (border_side - 1))))
			throw std::runtime_error("XL shield border size is not source-qualified");
		// GL_CLAMP nearest sampling at UV=1 selects the final source texel, not the border
		// Filtered UVs must retain their unclamped derivatives for source mip selection
		const std::array<float, 3> texture_map{ border_side ? .25f : 1.f, border_side ? .25f : 0.f,
			                                    border_side && nearest ? 1.f - .5f / float(border_side) : 1.f };
		if (!hit.envelope.active) return;
		LightningDrawState restore;
		struct CullState {
			GLint mode, front;
			CullState()
			{
				glGetIntegerv(GL_CULL_FACE_MODE, &mode);
				glGetIntegerv(GL_FRONT_FACE, &front);
			}
			~CullState()
			{
				glCullFace(mode);
				glFrontFace(front);
			}
		} cull_restore;
		prepare();
		lightning_gpu_program(program);
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		if (quality != requested_quality) {
			const auto vertices = shield_sphere_mesh(requested_quality);
			glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(vertices.size() * sizeof(ShieldSphereVertex)), vertices.data(), GL_STATIC_DRAW);
			count = GLsizei(vertices.size());
			quality = requested_quality;
		}
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ShieldSphereVertex), reinterpret_cast<const void *>(offsetof(ShieldSphereVertex, position)));
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ShieldSphereVertex), reinterpret_cast<const void *>(offsetof(ShieldSphereVertex, uv)));
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture);
		glUniform1i(glGetUniformLocation(program, "sphereTex"), 0);
		glUniform4fv(glGetUniformLocation(program, "vHit"), 3, hit.vectors[0].data());
		glUniform3fv(glGetUniformLocation(program, "fRad"), 1, hit.envelope.inverse_radius.data());
		glUniform4fv(glGetUniformLocation(program, "color"), 1, color.data());
		glUniform3fv(glGetUniformLocation(program, "textureMap"), 1, texture_map.data());
		glUniformMatrix4fv(glGetUniformLocation(program, "mvp"), 1, GL_FALSE, matrix.data());
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_ONE, GL_ONE);
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CCW);
#ifndef ANDROID
		glDisable(GL_ALPHA_TEST);
#endif
		for (GLenum cull : { GL_BACK, GL_FRONT }) {
			glCullFace(cull);
#ifdef ANDROID
#undef glDrawArrays
			glDrawArrays(GL_TRIANGLES, 0, count);
#define glDrawArrays gles3_shim_draw_arrays
#else
			glDrawArrays(GL_TRIANGLES, 0, count);
#endif
		}
	}
};
} // namespace xl_compat
#endif
