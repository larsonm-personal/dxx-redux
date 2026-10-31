// Local depth ownership for the particle pass; never retain a scene attachment
struct ParticleDepth {
	GLuint texture = 0, framebuffer = 0;
	int width = 0, height = 0, bits = 0, stencil = 0;
	void discard(bool abandoned)
	{
		if (!abandoned) {
			if (texture) glDeleteTextures(1, &texture);
			if (framebuffer) glDeleteFramebuffers(1, &framebuffer);
		}
		*this = {};
	}
	void capture()
	{
		struct Restore {
			GLint read, draw, binding, unpack;
			GLboolean scissor;
			Restore()
			{
				glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
				glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
				glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);
				scissor = glIsEnabled(GL_SCISSOR_TEST);
			}
			~Restore()
			{
				glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
				glBindTexture(GL_TEXTURE_2D, binding);
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack);
				if (scissor) glEnable(GL_SCISSOR_TEST);
			}
		} saved;
		GLint view[4], depth_bits, stencil_bits;
		glGetIntegerv(GL_VIEWPORT, view);
		glBindFramebuffer(GL_FRAMEBUFFER, saved.draw);
		glGetIntegerv(GL_DEPTH_BITS, &depth_bits);
		glGetIntegerv(GL_STENCIL_BITS, &stencil_bits);
		if (view[0] < 0 || view[1] < 0 || view[2] <= 0 || view[3] <= 0 || view[0] > 16384 - view[2] || view[1] > 16384 - view[3] || depth_bits < 16)
			throw std::runtime_error("XL particle soft blending requires a bounded depth-backed viewport");
		const int w = view[0] + view[2], h = view[1] + view[3];
		if (!texture || width != w || height != h || bits != depth_bits || stencil != stencil_bits) {
			discard(false);
			glGenTextures(1, &texture);
			glBindTexture(GL_TEXTURE_2D, texture);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
			const GLenum format = stencil_bits ? GL_DEPTH_STENCIL : GL_DEPTH_COMPONENT;
			const GLenum internal = stencil_bits ? (depth_bits > 24 ? GL_DEPTH32F_STENCIL8 : GL_DEPTH24_STENCIL8) : depth_bits <= 16 ? GL_DEPTH_COMPONENT16
			                                                                                                    : depth_bits <= 24   ? GL_DEPTH_COMPONENT24
			                                                                                                                         : GL_DEPTH_COMPONENT32F;
			const GLenum type = stencil_bits ? (depth_bits > 24 ? GL_FLOAT_32_UNSIGNED_INT_24_8_REV : GL_UNSIGNED_INT_24_8) : depth_bits <= 16 ? GL_UNSIGNED_SHORT
			                                                                                                              : depth_bits <= 24   ? GL_UNSIGNED_INT
			                                                                                                                                   : GL_FLOAT;
			glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, format, type, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glGenFramebuffers(1, &framebuffer);
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, stencil_bits ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture, 0);
			const GLenum none = GL_NONE;
			glDrawBuffers(1, &none);
			glReadBuffer(GL_NONE);
			if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				throw std::runtime_error("XL particle depth snapshot framebuffer is incomplete");
			width = w;
			height = h;
			bits = depth_bits;
			stencil = stencil_bits;
		}
		glDisable(GL_SCISSOR_TEST);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, saved.draw);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
		// Multisample depth resolves require identical bounds and depth format
		glBlitFramebuffer(view[0], view[1], w, h, view[0], view[1], w, h, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		if (glGetError() != GL_NO_ERROR) throw std::runtime_error("XL particle scene-depth transfer failed");
	}
};
