// Existing protected lightning owner: analytical sprite borders, depth and scissor
unsigned sprite_bitmap_device_controls()
{
	using namespace xl_compat;
	const auto require = [](bool condition, const char *message) { if (!condition) throw std::runtime_error(message); };
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	unsigned cases = 0;
	for (const auto shape : { std::array<int, 2>{ 1, 1 }, { 8, 4 }, { 4, 8 } }) {
		struct Image {
			grs_bitmap bitmap{};
			std::vector<unsigned char> pixels;
			~Image()
			{
				ogl_freebmtexture(&bitmap);
			}
		} image;
		image.pixels.resize(std::size_t(shape[0]) * shape[1] * 4);
		const std::array<unsigned char, 4> rgba{ 128, 64, 192, 128 };
		for (unsigned i = 0; i < image.pixels.size(); ++i) image.pixels[i] = rgba[i % 4];
		image.bitmap.bm_w = shape[0];
		image.bitmap.bm_h = shape[1];
		image.bitmap.bm_rowsize = shape[0] * 4;
		image.bitmap.bm_type = BM_RGBA;
		image.bitmap.bm_data = image.pixels.data();
		ogl_loadbmtexture_f(&image.bitmap, 2, nullptr);
		for (GLenum filter : { GL_NEAREST, GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR }) {
			glBindTexture(GL_TEXTURE_2D, image.bitmap.gltexture->handle);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter == GL_NEAREST ? GL_NEAREST : GL_LINEAR);
			for (float size : { .03125f, .25f, .75f })
				for (bool occluded : { false, true })
					for (bool scissored : { false, true }) {
						glViewport(0, 0, 64, 64);
						glDisable(GL_SCISSOR_TEST);
						glDepthMask(GL_TRUE);
						glClearDepthf(occluded ? .25f : 1.f);
						glClearColor(0, 0, 0, 0);
						glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
						glScissor(9, 12, 25, 25);
						if (scissored) glEnable(GL_SCISSOR_TEST);
						ParticleQuad quad{};
						quad.blend = ParticleBlend::alpha;
						const std::array<std::array<float, 2>, 4> corners{ { { -1, 1 }, { 1, 1 }, { 1, -1 }, { -1, -1 } } };
						for (unsigned i = 0; i < 4; ++i) {
							quad.vertices[i].position = { corners[i][0] * size + .0078125f, corners[i][1] * size - .0234375f, 0 };
							quad.vertices[i].uv = { (corners[i][0] + 1) * .5f, (1 - corners[i][1]) * .5f };
							quad.vertices[i].color = { 1, 1, 1, 2.f / 3.f };
						}
						glDepthFunc(GL_GREATER);
						draw_bitmap_sprite(image.bitmap, quad);
						GLint depth_function;
						glGetIntegerv(GL_DEPTH_FUNC, &depth_function);
						require(depth_function == GL_GREATER && bool(glIsEnabled(GL_SCISSOR_TEST)) == scissored, "Sprite body device control leaked draw state");
						std::array<unsigned char, 64 * 64 * 4> pixels{};
						glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
						const double left = 32.25 - size * 32, top = 31.25 + size * 32;
						const double footprint = double((std::max) (shape[0], shape[1])) / (size * 64);
						const double last = std::floor(std::log2((std::max) (shape[0], shape[1])));
						const double lod = filter == GL_LINEAR_MIPMAP_LINEAR ? std::clamp(std::log2(footprint), 0., last) : 0.;
						for (unsigned y = 0; y < 64; ++y)
							for (unsigned x = 0; x < 64; ++x) {
								const double u = (x + .5 - left) / (size * 64), v = (top - y - .5) / (size * 64);
								const bool visible = !occluded && u > 0 && u < 1 && v > 0 && v < 1 && (!scissored || (x >= 9 && x < 34 && y >= 12 && y < 37));
								const auto border = [&](int level) {
									if (filter == GL_NEAREST) return 1.;
									const int w = (std::max) (1, shape[0] >> level), h = (std::max) (1, shape[1] >> level);
									return std::clamp(u * w + .5, 0., 1.) * std::clamp((1 - u) * w + .5, 0., 1.) * std::clamp(v * h + .5, 0., 1.) * std::clamp((1 - v) * h + .5, 0., 1.);
								};
								const int lower = int(std::floor(lod));
								const double coverage = border(lower) * (1 - (lod - lower)) + border((std::min) (lower + 1, int(last))) * (lod - lower);
								const double alpha = rgba[3] / 255. * (2. / 3.) * coverage;
								for (unsigned c = 0; c < 4; ++c) {
									const int expected = visible ? int(std::lround(255 * (c == 3 ? alpha : rgba[c] / 255. * coverage) * alpha)) : 0;
									if (std::abs(int(pixels[(y * 64 + x) * 4 + c]) - expected) > 1)
										throw std::runtime_error("Sprite body device pixels differ: width=" + std::to_string(shape[0]) + " height=" + std::to_string(shape[1]) + " filter=" + std::to_string(filter) + " size=" + std::to_string(size) + " x=" + std::to_string(x) + " y=" + std::to_string(y) + " expected=" + std::to_string(expected) + " actual=" + std::to_string(pixels[(y * 64 + x) * 4 + c]));
								}
							}
						require(glGetError() == GL_NO_ERROR, "Sprite body device control raised a GPU error");
						++cases;
					}
		}
	}
	glDisable(GL_SCISSOR_TEST);
	return cases;
}
