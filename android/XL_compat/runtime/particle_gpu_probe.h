// Android renderer-only control, reused by the POG lifecycle owner
// Retention here is deliberate test ownership, not loaded-level particle admission
std::shared_ptr<const xl_compat::ParticleImageResource> particle_probe_image;

struct ParticleProbeTarget {
	GLint renderbuffer, viewport[4], program, vao, buffer;
	GLfloat clear[4];
	GLboolean scissor, mask[4];
	GLuint color = 0;
	int external = gles3_shim_external_active();
	ParticleProbeTarget()
	{
		glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
		glGetIntegerv(GL_VIEWPORT, viewport);
		glGetIntegerv(GL_CURRENT_PROGRAM, &program);
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
		glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
		glGetBooleanv(GL_COLOR_WRITEMASK, mask);
		scissor = glIsEnabled(GL_SCISSOR_TEST);
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		glGenRenderbuffers(1, &color);
		glBindRenderbuffer(GL_RENDERBUFFER, color);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 16, 16);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
		glViewport(0, 0, 16, 16);
		glDisable(GL_SCISSOR_TEST);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}
	~ParticleProbeTarget()
	{
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
		glDeleteRenderbuffers(1, &color);
		glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
		glClearColor(clear[0], clear[1], clear[2], clear[3]);
		glColorMask(mask[0], mask[1], mask[2], mask[3]);
		if (scissor) glEnable(GL_SCISSOR_TEST);
		gles3_shim_use_external(external ? GLuint(program) : 0);
		gles3_shim_bind_program(GLuint(program));
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
	}
};

void particle_gpu_probe(const xl_compat::ParticleImageResource *prepared = particle_probe_image.get())
{
	require(prepared && gl_initialized && glGetError() == GL_NO_ERROR, "Particle GPU probe requires an owned atlas and clean context");
	const auto &image = prepared->bitmap;
	unsigned samples = 0;
	{
		Readback readback;
		ParticleProbeTarget target;
		require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Particle probe framebuffer is incomplete");
		const std::array<unsigned char, 4> background{ 73, 119, 167, 255 };
		xl_compat::ParticleQuad quad{};
		quad.vertices[0].position = { -1, -1, 0 };
		quad.vertices[1].position = { 1, -1, 0 };
		quad.vertices[2].position = { 1, 1, 0 };
		quad.vertices[3].position = { -1, 1, 0 };
		// Pixel-center constant UVs isolate real atlas upload and both blend equations
		// from the already-qualified desktop billboard/atlas coordinate generator
		for (unsigned cell = 0; cell < 4; ++cell) {
			const unsigned x = (cell % 2 * 2 + 1) * image.width / 4;
			const unsigned y = (cell / 2 * 2 + 1) * image.height / 4;
			const auto offset = (std::size_t(y) * image.width + x) * 4;
			for (auto &vertex : quad.vertices) vertex.uv = { (x + .5f) / image.width, (y + .5f) / image.height };
			for (const auto blend : { xl_compat::ParticleBlend::weak_additive, xl_compat::ParticleBlend::multiply }) {
				quad.blend = blend;
				glClearColor(background[0] / 255.f, background[1] / 255.f, background[2] / 255.f, 1);
				glClear(GL_COLOR_BUFFER_BIT);
				prepared->texture->draw(&quad, 1);
				std::array<unsigned char, 4> actual{};
				glReadPixels(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
				require(glGetError() == GL_NO_ERROR, "Particle probe draw/readback raised a GPU error");
				for (unsigned channel = 0; channel < 4; ++channel) {
					const double source = image.pixels[offset + channel] / 255., destination = background[channel] / 255.;
					const double expected = blend == xl_compat::ParticleBlend::multiply ? source * destination : source + destination * (1 - source);
					require(std::abs(int(actual[channel]) - int(expected * 255 + .5)) <= 1, "Android particle atlas/blend differs from decoded original pixels and source equation");
				}
				++samples;
			}
		}
	}
	require(glGetError() == GL_NO_ERROR, "Particle probe restoration raised a GPU error");
	++checks;
	report = nlohmann::json{ { "passed", true }, { "particle_samples", samples }, { "particle_width", image.width }, { "checks", checks } }.dump();
}
