#ifndef DXX_XL_CAMERA_TARGET_GPU_H
#define DXX_XL_CAMERA_TARGET_GPU_H
#include "lightning_glow_gpu.h"
#include "../camera_capture.h"
namespace xl_compat
{
// GL target only; the scene adapter owns viewer, canvas and engine render globals
class CameraTargetGpu
{
	inline static CameraTargetGpu *head = nullptr;
	inline static CameraTargetGpu *active = nullptr;
	CameraTargetGpu *previous = nullptr, *next = nullptr;
	GLuint framebuffer = 0, colors[2]{}, depth = 0;
	int width = 0, height = 0;
	bool captured = false, capturing = false;
	struct Restore {
		LightningGlowState common;
		GLint clear_stencil, front[7], back[7], cull, winding;
		GLfloat clear_depth;
		Restore()
		{
			glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clear_depth);
			glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &clear_stencil);
			glGetIntegerv(GL_CULL_FACE_MODE, &cull);
			glGetIntegerv(GL_FRONT_FACE, &winding);
			const GLenum f[] = { GL_STENCIL_FUNC, GL_STENCIL_REF, GL_STENCIL_VALUE_MASK, GL_STENCIL_WRITEMASK, GL_STENCIL_FAIL, GL_STENCIL_PASS_DEPTH_FAIL, GL_STENCIL_PASS_DEPTH_PASS };
			const GLenum b[] = { GL_STENCIL_BACK_FUNC, GL_STENCIL_BACK_REF, GL_STENCIL_BACK_VALUE_MASK, GL_STENCIL_BACK_WRITEMASK, GL_STENCIL_BACK_FAIL, GL_STENCIL_BACK_PASS_DEPTH_FAIL, GL_STENCIL_BACK_PASS_DEPTH_PASS };
			for (unsigned i = 0; i < 7; ++i) {
				glGetIntegerv(f[i], front + i);
				glGetIntegerv(b[i], back + i);
			}
		}
		~Restore()
		{
#ifdef ANDROID
			glClearDepthf(clear_depth);
#else
			glClearDepth(clear_depth);
#endif
			glClearStencil(clear_stencil);
			glCullFace(cull);
			glFrontFace(winding);
			for (unsigned i = 0; i < 2; ++i) {
				const auto *s = i ? back : front;
				const GLenum face = i ? GL_BACK : GL_FRONT;
				glStencilFuncSeparate(face, s[0], s[1], GLuint(s[2]));
				glStencilMaskSeparate(face, GLuint(s[3]));
				glStencilOpSeparate(face, s[4], s[5], s[6]);
			}
		}
	};
	void prepare(const CameraTargetSpec &spec)
	{
		if (spec.buffer_type != 1 || spec.color_buffers != 2 || spec.width <= 0 || spec.height <= 0)
			throw std::runtime_error("Unsupported XL camera target specification");
		if (framebuffer && width == spec.width && height == spec.height) return;
		GLint maximum;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
		if (spec.width > maximum || spec.height > maximum) throw std::runtime_error("XL camera target exceeds GPU dimensions");
		GLuint target = 0, images[3]{};
		try {
			glActiveTexture(GL_TEXTURE0);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
			glGenTextures(3, images);
			for (unsigned i = 0; i < 3; ++i) {
				glBindTexture(GL_TEXTURE_2D, images[i]);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				if (i < 2) {
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, spec.width, spec.height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
					glGenerateMipmap(GL_TEXTURE_2D);
				} else {
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
					glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, spec.width, spec.height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
				}
			}
			glGenFramebuffers(1, &target);
			glBindFramebuffer(GL_FRAMEBUFFER, target);
			for (unsigned i = 0; i < 2; ++i) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, images[i], 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, images[2], 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, images[2], 0);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE || glGetError() != GL_NO_ERROR)
				throw std::runtime_error("XL camera framebuffer is incomplete");
		} catch (...) {
			glDeleteTextures(3, images);
			if (target) glDeleteFramebuffers(1, &target);
			throw;
		}
		discard(false);
		framebuffer = target;
		colors[0] = images[0];
		colors[1] = images[1];
		depth = images[2];
		width = spec.width;
		height = spec.height;
	}

  public:
	CameraTargetGpu()
	{
		next = head;
		if (next) next->previous = this;
		head = this;
	}
	CameraTargetGpu(const CameraTargetGpu &) = delete;
	CameraTargetGpu &operator=(const CameraTargetGpu &) = delete;
	~CameraTargetGpu()
	{
		discard(false);
		if (previous) previous->next = next;
		else head = next;
		if (next) next->previous = previous;
	}
	void discard(bool abandoned)
	{
		if (!abandoned) {
			if (colors[0] || colors[1]) glDeleteTextures(2, colors);
			if (depth) glDeleteTextures(1, &depth);
			if (framebuffer) glDeleteFramebuffers(1, &framebuffer);
		}
		framebuffer = colors[0] = colors[1] = depth = 0;
		captured = false;
		width = height = 0;
	}
	static void discard_context(bool abandoned)
	{
		for (auto *target = head; target; target = target->next) target->discard(abandoned);
	}
	static bool active_dimensions(int &w, int &h)
	{
		if (!active) return false;
		w = active->width;
		h = active->height;
		return true;
	}
	bool valid() const
	{
		return captured;
	}
	GLuint texture(unsigned index = 0) const
	{
		if (index > 1 || (!captured && !capturing)) throw std::runtime_error("XL camera has no captured color image");
		return colors[index];
	}
	// GLES cannot route fragment output zero directly to attachment one
	// Remap the two physical attachments while preserving their logical identities
	void select(int buffer)
	{
		if (!capturing || buffer < -1 || buffer > 1) throw std::runtime_error("Invalid XL camera draw-buffer selection");
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		for (unsigned i = 0; i < 2; ++i)
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colors[buffer == 1 ? 1 - i : i], 0);
		const GLenum attachments[2]{ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		glDrawBuffers(buffer < 0 ? 2 : 1, attachments);
	}
	template <class Draw>
	void capture(const CameraTargetSpec &spec, Draw draw)
	{
		if (capturing) throw std::runtime_error("Nested capture of the same XL camera target");
		Restore restore;
		prepare(spec);
		captured = false;
		capturing = true;
		struct ActiveScope {
			CameraTargetGpu *previous = active;
			~ActiveScope()
			{
				active = previous;
			}
		} active_scope;
		active = this;
		try {
			select(0);
			glViewport(0, 0, width, height);
			draw();
			select(0);
			if (glGetError() != GL_NO_ERROR) throw std::runtime_error("XL camera capture raised a GL error");
			captured = true;
			capturing = false;
		} catch (...) {
			capturing = false;
			throw;
		}
	}
};
} // namespace xl_compat
#endif
