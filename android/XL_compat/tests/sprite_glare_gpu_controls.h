// Optional original asset coverage within the existing EGL/source shader owner
static void sprite_glare_gpu_controls(const char *path)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	const auto length = input.tellg();
	check(input && length >= 18 && length <= 64 * 1024 * 1024, "Invalid sprite glare input size");
	std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
	input.seekg(0);
	input.read(reinterpret_cast<char *>(bytes.data()), length);
	check(bool(input), "Cannot read complete sprite glare input");
	const auto bitmap = xl_compat::read_particle_image(bytes.data(), bytes.size());
	const GLuint program = soft_test_program(glareVS, glareFS);
	GLuint target, color, depth, depth_sample;
	glGenFramebuffers(1, &target);
	glGenTextures(1, &color);
	glGenTextures(1, &depth_sample);
	glGenRenderbuffers(1, &depth);
	glBindFramebuffer(GL_FRAMEBUFFER, target);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, color);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, 64, 64);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
	check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Original glare target incomplete");
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, depth_sample);
	const float scene_depth = .7f;
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 1, 1, 0, GL_RED, GL_FLOAT, &scene_depth);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glViewport(0, 0, 64, 64);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);
	unsigned comparisons = 0, changed = 0;
	int maximum = 0;
	for (GLenum filter : { GL_NEAREST, GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR }) {
		xl_compat::ParticleTexture texture(bitmap, "original-powerup-glare", false);
		const auto original = texture.handle();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, original);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter == GL_NEAREST ? GL_NEAREST : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		for (float size : { .125f, .5f, 1.f, 1.2f })
			for (float z : { .2f, .6f }) {
				xl_compat::ParticleQuad quad{};
				quad.blend = xl_compat::ParticleBlend::additive;
				const std::array<std::array<float, 2>, 4> corners{ { { -1, 1 }, { 1, 1 }, { 1, -1 }, { -1, -1 } } };
				for (unsigned i = 0; i < 4; ++i) {
					quad.vertices[i].position = { corners[i][0] * size + .03125f, corners[i][1] * size - .015625f, z };
					quad.vertices[i].uv = { (corners[i][0] + 1) * .5f, (1 - corners[i][1]) * .5f };
					quad.vertices[i].color = { .3f, .4f, .2f, .5f };
				}
				std::array<std::array<unsigned char, 64 * 64 * 4>, 2> pixels{};
				for (unsigned profile = 0; profile < 2; ++profile) {
					glClearColor(.02f, .04f, .06f, 1);
					glDepthMask(GL_TRUE);
					glClearDepth(scene_depth);
					glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
					if (profile) {
						xl_particle_soft_begin_scene();
						texture.draw_soft_sprite(quad);
						xl_particle_soft_end_scene();
					} else {
						glUseProgram(program);
						glActiveTexture(GL_TEXTURE0);
						glBindTexture(GL_TEXTURE_2D, original);
						glActiveTexture(GL_TEXTURE1);
						glBindTexture(GL_TEXTURE_2D, depth_sample);
						glUniform1i(glGetUniformLocation(program, "glareTex"), 0);
						glUniform1i(glGetUniformLocation(program, "depthTex"), 1);
						glUniform2f(glGetUniformLocation(program, "windowScale"), 1.f / 64, 1.f / 64);
						glUniform1i(glGetUniformLocation(program, "blendMode"), 1);
						glUniform1i(glGetUniformLocation(program, "bSuspended"), 0);
						glUniform1f(glGetUniformLocation(program, "dMax"), 5);
						glColor4fv(quad.vertices[0].color.data());
						glBegin(GL_QUADS);
						for (const auto &vertex : quad.vertices) {
							glTexCoord2fv(vertex.uv.data());
							glVertex3fv(vertex.position.data());
						}
						glEnd();
					}
					glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels[profile].data());
				}
				for (unsigned i = 0; i < pixels[0].size(); ++i) {
					maximum = (std::max) (maximum, std::abs(int(pixels[0][i]) - int(pixels[1][i])));
					if (i % 4 == 0 && pixels[0][i] > 6) ++changed;
				}
				check(glGetError() == GL_NO_ERROR, "Original glare comparison raised GL error");
				++comparisons;
			}
	}
	std::cout << "Original sprite glare: " << comparisons << " size/depth/filter comparisons, " << changed << " changed pixels, max error " << maximum << '\n';
	check(changed > 0 && maximum <= 1, "Original glare sprite differs from pinned source shader/filter output");
	glUseProgram(0);
	glDeleteProgram(program);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &target);
	glDeleteRenderbuffers(1, &depth);
	glDeleteTextures(1, &color);
	glDeleteTextures(1, &depth_sample);
	glActiveTexture(GL_TEXTURE0);
}
