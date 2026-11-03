// Production soft draw comparisons for copied depth and borrowed depth/stencil
static void particle_direct_depth_controls()
{
	xl_compat::PogBitmap pixels;
	pixels.width = pixels.height = 2;
	pixels.truecolor = true;
	for (int i = 0; i < 4; ++i) pixels.pixels.insert(pixels.pixels.end(), { 200, 100, 50, 180 });
	xl_compat::ParticleTexture image(pixels, "direct-depth-control");
	GLuint target, color, depth, copied;
	glGenFramebuffers(1, &target);
	glGenRenderbuffers(1, &color);
	glGenRenderbuffers(1, &copied);
	glGenTextures(1, &depth);
	glBindFramebuffer(GL_FRAMEBUFFER, target);
	glBindRenderbuffer(GL_RENDERBUFFER, color);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 16, 16);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
	glViewport(0, 0, 16, 16);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_SCISSOR_TEST);
	xl_compat::ParticleQuad quad{};
	const float xy[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
	quad.blend = xl_compat::ParticleBlend::alpha;
	for (int i = 0; i < 4; ++i) {
		quad.vertices[i].position = { xy[i][0], xy[i][1], .8f };
		quad.vertices[i].uv = { .5f, .5f };
		quad.vertices[i].color = { 1, 1, 1, 1 };
	}
	unsigned checks = 0;
	for (GLenum format : { GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT24, GL_DEPTH24_STENCIL8 }) {
		const bool stencil = format == GL_DEPTH24_STENCIL8;
		for (bool borrow : { false, true }) {
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
			if (borrow) {
				glBindTexture(GL_TEXTURE_2D, depth);
				glTexImage2D(GL_TEXTURE_2D, 0, format, 16, 16, 0, stencil ? GL_DEPTH_STENCIL : GL_DEPTH_COMPONENT, stencil ? GL_UNSIGNED_INT_24_8 : GL_UNSIGNED_INT, nullptr);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
				if (stencil) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
			} else {
				glBindRenderbuffer(GL_RENDERBUFFER, copied);
				glRenderbufferStorage(GL_RENDERBUFFER, format, 16, 16);
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, copied);
				if (stencil) glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, copied);
			}
			check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Direct particle control target incomplete");
			for (float scene : { .5f, .89f, 1.f }) {
				glDepthMask(GL_TRUE);
				glClearDepth(scene);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				glClearColor(.1f, .2f, .3f, 1);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				xl_particle_soft_begin_scene();
				image.draw(&quad, 1);
				xl_particle_soft_end_scene();
				GLint restored = 0;
				glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &restored);
				check(GLuint(restored) == (borrow ? depth : copied), "Soft draw lost scene depth ownership");
				if (stencil) {
					glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &restored);
					check(GLuint(restored) == (borrow ? depth : copied), "Soft draw lost scene stencil ownership");
				}
				unsigned char actual[4];
				float actual_depth;
				glReadPixels(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, actual);
				glReadPixels(8, 8, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &actual_depth);
				check(std::abs(actual_depth - scene) < .00002f, "Soft draw changed captured scene depth");
				const auto eye = [](double d) { return 10000. / (5001. + (2 * d - 1) * -4999.); };
				const double fade = 1 - std::clamp(eye(.9) - eye(scene), 0., 5.) / 5.;
				const double alpha = 180. / 255 * fade;
				for (int c = 0; c < 3; ++c) {
					const double wanted = pixels.pixels[c] * alpha + (c + 1) * .1 * 255 * (1 - alpha);
					check(std::abs(actual[c] - wanted) <= 2, "Production soft particle fade differs from captured scene depth");
				}
				check(glGetError() == GL_NO_ERROR, "Direct particle control GL error");
				++checks;
			}
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &target);
	glDeleteRenderbuffers(1, &color);
	glDeleteRenderbuffers(1, &copied);
	glDeleteTextures(1, &depth);
	std::cout << "Production soft particles: " << checks << " copied/direct depth and stencil cases passed\n";
}
