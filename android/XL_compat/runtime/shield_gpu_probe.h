// Shared analytical GL/GLES control; the protected owner also compares context generations
unsigned shield_device_controls(std::vector<unsigned char> &retained, unsigned *overlap_rounding = nullptr)
{
	using namespace xl_compat;
	const auto require = [](bool condition, const char *message) { if (!condition) throw std::runtime_error(message); };
	LightningGlowState restore;
	struct Target {
		GLuint framebuffer = 0, buffers[2]{};
		GLint previous_renderbuffer = 0;
		Target()
		{
			glGetIntegerv(GL_RENDERBUFFER_BINDING, &previous_renderbuffer);
		}
		~Target()
		{
			glDeleteRenderbuffers(2, buffers);
			glDeleteFramebuffers(1, &framebuffer);
			glBindRenderbuffer(GL_RENDERBUFFER, previous_renderbuffer);
		}
	} target;
	glGenFramebuffers(1, &target.framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
	glGenRenderbuffers(2, target.buffers);
	glBindRenderbuffer(GL_RENDERBUFFER, target.buffers[0]);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 64, 64);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, target.buffers[0]);
	glBindRenderbuffer(GL_RENDERBUFFER, target.buffers[1]);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 64, 64);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, target.buffers[1]);
	require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Shield probe target incomplete");
	const std::array<unsigned char, 4> texel{ 96, 128, 192, 160 };
	static const PogBitmap bitmap = [] {
		PogBitmap image;
		image.width = image.height = 4;
		image.truecolor = true;
		image.pixels.resize(64);
		const std::array<unsigned char, 4> value{ 96, 128, 192, 160 };
		for (unsigned i = 0; i < image.pixels.size(); ++i) image.pixels[i] = value[i % 4];
		return image;
	}();
	// Persist the CPU resource across the protected owner's actual context loss
	static ShieldTexture texture(bitmap);
	glActiveTexture(GL_TEXTURE0);
	const std::array<float, 16> matrix{ .7f, 0, 0, 0, 0, .7f, 0, 0, 0, 0, .7f, 0, 0, 0, 0, 1 };
	const std::array<float, 4> color{ .25f, .5f, .75f, 1 };
	const HitRenderMatrix identity{ { { 65536, 0, 0 }, { 0, 65536, 0 }, { 0, 0, 65536 } } };
	CameraHitState state;
	state.times = { 4000, 4000, 4000 };
	state.points = { { { 0, 0, 65536 }, { -65536, 0, 0 }, { 65536, 0, 0 } } };
	struct Sample {
		std::array<float, 3> position;
		std::array<float, 2> uv;
		float depth;
		bool front;
	};
	GLint subpixel_bits;
	glGetIntegerv(GL_SUBPIXEL_BITS, &subpixel_bits);
	require(subpixel_bits >= 4 && subpixel_bits <= 16, "Unsupported shield raster precision");
	const float raster_margin = std::ldexp(1.f, -subpixel_bits) + 1.e-5f;
	unsigned cases = 0, checked = 0, visible = 0, hidden = 0;
	if (overlap_rounding) *overlap_rounding = 0;
	for (int quality : { 2, 3, 4 }) {
		std::array<std::vector<Sample>, 4096> samples;
		std::array<bool, 4096> boundary{};
		const auto mesh = shield_sphere_mesh(quality);
		for (unsigned t = 0; t < mesh.size(); t += 3) {
			std::array<std::array<float, 2>, 3> screen;
			for (unsigned j = 0; j < 3; ++j)
				for (unsigned c = 0; c < 2; ++c) screen[j][c] = (mesh[t + j].position[c] * .7f + 1) * 32;
			const auto edge = [](const auto &a, const auto &b, float x, float y) { return (b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0]); };
			const float area = edge(screen[0], screen[1], screen[2][0], screen[2][1]);
			if (std::abs(area) < 1.e-8f) continue;
			std::array<float, 3> margins;
			for (unsigned j = 0; j < 3; ++j) {
				const auto &a = screen[(j + 1) % 3], &b = screen[(j + 2) % 3];
				const float dx = b[0] - a[0], dy = b[1] - a[1];
				margins[j] = raster_margin * std::sqrt(dx * dx + dy * dy) / std::abs(area);
			}
			const int left = (std::max) (0, int(std::floor((std::min) ({ screen[0][0], screen[1][0], screen[2][0] }))));
			const int right = (std::min) (63, int(std::ceil((std::max) ({ screen[0][0], screen[1][0], screen[2][0] }))));
			const int bottom = (std::max) (0, int(std::floor((std::min) ({ screen[0][1], screen[1][1], screen[2][1] }))));
			const int top = (std::min) (63, int(std::ceil((std::max) ({ screen[0][1], screen[1][1], screen[2][1] }))));
			for (int y = bottom; y <= top; ++y)
				for (int x = left; x <= right; ++x) {
					const std::array<float, 3> w{ edge(screen[1], screen[2], x + .5f, y + .5f) / area,
						                          edge(screen[2], screen[0], x + .5f, y + .5f) / area, edge(screen[0], screen[1], x + .5f, y + .5f) / area };
					if (w[0] < -margins[0] || w[1] < -margins[1] || w[2] < -margins[2]) continue;
					const unsigned index = unsigned(y * 64 + x);
					if (w[0] < margins[0] || w[1] < margins[1] || w[2] < margins[2]) {
						boundary[index] = true;
						continue;
					}
					Sample sample{};
					for (unsigned j = 0; j < 3; ++j) {
						for (unsigned c = 0; c < 3; ++c) sample.position[c] += mesh[t + j].position[c] * w[j];
						for (unsigned c = 0; c < 2; ++c) sample.uv[c] += mesh[t + j].uv[c] * w[j];
					}
					sample.depth = sample.position[2] * .35f + .5f;
					sample.front = area > 0;
					samples[index].push_back(sample);
				}
		}
		for (GLint filter : { GL_NEAREST, GL_LINEAR }) {
			const auto border = texture.handle(0, filter == GL_NEAREST ? ShieldFilter::nearest : ShieldFilter::linear);
			glBindTexture(GL_TEXTURE_2D, border);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
			for (int age : { 0, 1000, 2000 }) {
				const auto hit = camera_hit_hardware(state, 297534, 4000 + age, 2000, identity);
				std::array<std::array<unsigned char, 16384>, 2> halves{};
				std::array<std::array<int, 16384>, 2> expected_halves{};
				std::array<std::array<bool, 4096>, 2> half_contributes{};
				for (int depth_profile : { 0, 1, 2, 3 }) {
					const float scene_depth = depth_profile == 0 ? .1f : depth_profile == 3 ? 1.f
					                                                                        : .5f;
					auto draw_matrix = matrix;
					if (depth_profile == 2) draw_matrix[10] = -draw_matrix[10];
					glViewport(0, 0, 64, 64);
					glDisable(GL_SCISSOR_TEST);
					glDisable(GL_STENCIL_TEST);
					glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
					glDepthMask(GL_TRUE);
					glClearDepthf(scene_depth);
					glClearColor(0, 0, 0, 0);
					glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
					draw_hit_shield(quality, hit, color, draw_matrix, border, 4, filter == GL_NEAREST);
					std::array<unsigned char, 16384> pixels{};
					glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
					if (depth_profile == 1 || depth_profile == 2) halves[depth_profile - 1] = pixels;
					unsigned changed = 0;
					for (unsigned p = 0; p < samples.size(); ++p) {
						if (boundary[p]) continue;
						std::array<int, 4> expected{};
						unsigned contributions = 0;
						bool depth_boundary = false;
						for (bool front : { true, false })
							for (const auto &sample : samples[p]) {
								if (sample.front != front) continue;
								const float sample_depth = depth_profile == 2 ? 1.f - sample.depth : sample.depth;
								if (std::abs(sample_depth - scene_depth) < 1.e-5f || (depth_profile == 3 && std::abs(sample.depth - .5f) < 1.e-5f)) depth_boundary = true;
								if (!hit.envelope.active || sample_depth > scene_depth) continue;
								float strength = 0;
								for (unsigned slot = 0; slot < 3; ++slot) {
									float distance = 0;
									for (unsigned c = 0; c < 3; ++c) {
										const float d = sample.position[c] - hit.vectors[slot][c];
										distance += d * d;
									}
									strength = (std::max) (strength, 1.f - (std::min) (1.f, std::sqrt(distance) * hit.envelope.inverse_radius[slot]));
								}
								float border = 1;
								if (filter == GL_LINEAR)
									for (float uv : sample.uv) border *= (std::min) (1.f, (std::min) (uv * 4 + .5f, 4.5f - uv * 4));
								contributions += strength > 0 && border > 0;
								for (unsigned c = 0; c < 4; ++c)
									expected[c] = (std::min) (255, expected[c] + int(std::lround(float(texel[c]) * color[c] * strength * border)));
							}
						if (depth_boundary) continue;
						if (depth_profile == 1 || depth_profile == 2) half_contributes[depth_profile - 1][p] = contributions != 0;
						for (unsigned c = 0; c < 4; ++c) {
							if (depth_profile == 1 || depth_profile == 2) expected_halves[depth_profile - 1][p * 4 + c] = expected[c];
							// Each isolated half must independently meet one-byte analytical accuracy
							// Only their additive overlap permits the sum of those two quantization errors
							const int limit = depth_profile == 3 && half_contributes[0][p] && half_contributes[1][p] ? 2 : 1;
							if (std::abs(int(pixels[p * 4 + c]) - expected[c]) > limit)
								throw std::runtime_error("Shield analytical mismatch quality=" + std::to_string(quality) + " filter=" + std::to_string(filter) + " age=" + std::to_string(age) + " depth_profile=" + std::to_string(depth_profile) + " pixel=" + std::to_string(p) + " channel=" + std::to_string(c) + " expected=" + std::to_string(expected[c]) + " actual=" + std::to_string(pixels[p * 4 + c]));
							if (depth_profile == 3) {
								require(expected[c] == (std::min) (255, expected_halves[0][p * 4 + c] + expected_halves[1][p * 4 + c]), "Shield CPU composition differs from independently checked halves");
								const int combined = (std::min) (255, int(halves[0][p * 4 + c]) + int(halves[1][p * 4 + c]));
								require(std::abs(int(pixels[p * 4 + c]) - combined) <= 1, "Shield additive composition differs from independently checked halves");
								if (overlap_rounding && std::abs(int(pixels[p * 4 + c]) - expected[c]) > 1) ++*overlap_rounding;
							}
							changed += pixels[p * 4 + c] != 0;
						}
						++checked;
					}
					if (scene_depth == .1f || age == 2000) {
						require(!changed, "Occluded or expired device shield remained visible");
						++hidden;
					} else {
						require(changed != 0, "Device shield produced no visible pixels");
						++visible;
					}
					retained.insert(retained.end(), pixels.begin(), pixels.end());
					require(glGetError() == GL_NO_ERROR, "Shield device control raised GL error");
					++cases;
				}
			}
		}
	}
	require(cases == 72 && checked > 200000 && visible && hidden, "Shield device control missed coverage");
	return cases;
}
