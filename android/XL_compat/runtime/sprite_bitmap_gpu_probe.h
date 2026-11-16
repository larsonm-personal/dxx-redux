// Source glareFS uses eye-depth distance, disables hardware depth while soft,
// and fixes additive output alpha at one regardless of texture/material alpha
unsigned sprite_bitmap_soft_controls()
{
	using namespace xl_compat;
	struct Image {
		grs_bitmap bitmap{};
		std::array<unsigned char, 4> pixel{ 128, 64, 192, 128 };
		~Image()
		{
			ogl_freebmtexture(&bitmap);
		}
	} image;
	image.bitmap.bm_w = image.bitmap.bm_h = 1;
	image.bitmap.bm_rowsize = 4;
	image.bitmap.bm_type = BM_RGBA;
	image.bitmap.bm_data = image.pixel.data();
	ogl_loadbmtexture_f(&image.bitmap, 0, nullptr);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_SCISSOR_TEST);
	const auto depth = [](double eye) { return (5001. - 10000. / eye) / 4999. * .5 + .5; };
	unsigned cases = 0;
	for (int offset : { 0, 7 })
		for (float radius : { 0.f, 5.f, 10.f })
			for (float gap : { -2.f, 0.f, 2.5f, 5.f, 7.5f, 10.f, 12.f })
				for (bool additive : { false, true }) {
					glViewport(offset, offset, 48, 48);
					glDepthMask(GL_TRUE);
#ifdef ANDROID
					glClearDepthf(float(depth(10)));
#else
					glClearDepth(depth(10));
#endif
					glClearColor(32.f / 255, 64.f / 255, 96.f / 255, 128.f / 255);
					glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
					ParticleQuad quad{};
					quad.blend = additive ? ParticleBlend::additive : ParticleBlend::alpha;
					const std::array<std::array<float, 2>, 4> corners{ { { -1, 1 }, { 1, 1 }, { 1, -1 }, { -1, -1 } } };
					for (unsigned i = 0; i < 4; ++i) {
						quad.vertices[i].position = { corners[i][0], corners[i][1], float(2 * depth(10 + gap) - 1) };
						quad.vertices[i].uv = { .5f, .5f };
						quad.vertices[i].color = { .5f, .5f, .5f, .3f };
					}
					struct Scene {
						Scene()
						{
							xl_particle_soft_begin_scene();
						}
						~Scene()
						{
							xl_particle_soft_end_scene();
						}
					} scene;
					glEnable(GL_DEPTH_TEST);
					glDepthFunc(GL_GREATER);
					draw_bitmap_sprite(image.bitmap, quad, radius);
					GLint function = 0;
					glGetIntegerv(GL_DEPTH_FUNC, &function);
					if (!glIsEnabled(GL_DEPTH_TEST) || function != GL_GREATER)
						throw std::runtime_error("Soft sprite leaked caller depth state");
					std::array<unsigned char, 4> pixel{};
					glReadPixels(offset + 24, offset + 24, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
					const double fade = radius ? 1 - std::clamp(double(gap) / radius, 0., 1.) : 1;
					const double alpha = radius && additive ? 1. : 128. / 255 * .3 * (radius ? fade : 1);
					for (unsigned c = 0; c < 4; ++c) {
						const double background = (c + 1) * 32. / 255;
						const double foreground = c == 3 ? alpha : image.pixel[c] / 255. * .5 * (radius && additive ? fade : 1);
						const double value = !radius && gap > 0 ? background : additive ? foreground + background
						                                                                : foreground * alpha + background * (1 - alpha);
						if (std::abs(int(pixel[c]) - int(std::lround(255 * std::clamp(value, 0., 1.)))) > 1)
							throw std::runtime_error("Soft sprite radius/depth/material pixels differ: radius=" + std::to_string(radius) + " gap=" + std::to_string(gap) + " channel=" + std::to_string(c) + " additive=" + std::to_string(additive) + " actual=" + std::to_string(pixel[c]) + " expected=" + std::to_string(255 * std::clamp(value, 0., 1.)));
					}
					++cases;
				}
	// A real missing depth attachment must select the ordinary sprite fallback
	GLint attachment_type = 0, attachment = 0;
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &attachment_type);
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &attachment);
	if (attachment_type != GL_RENDERBUFFER) throw std::runtime_error("Sprite fallback control requires its owned depth renderbuffer");
	struct RestoreDepth {
		GLint attachment;
		~RestoreDepth()
		{
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, GLuint(attachment));
		}
	} restore_depth{ attachment };
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
	for (bool additive : { false, true }) {
		std::array<std::array<unsigned char, 4>, 2> pixels{};
		for (unsigned profile = 0; profile < 2; ++profile) {
			glClearColor(.1f, .2f, .3f, .4f);
			glClear(GL_COLOR_BUFFER_BIT);
			ParticleQuad quad{};
			quad.blend = additive ? ParticleBlend::additive : ParticleBlend::alpha;
			const std::array<std::array<float, 2>, 4> corners{ { { -1, 1 }, { 1, 1 }, { 1, -1 }, { -1, -1 } } };
			for (unsigned i = 0; i < 4; ++i) {
				quad.vertices[i].position = { corners[i][0], corners[i][1], 0 };
				quad.vertices[i].uv = { .5f, .5f };
				quad.vertices[i].color = { .5f, .5f, .5f, .3f };
			}
			xl_particle_soft_begin_scene();
			draw_bitmap_sprite(image.bitmap, quad, profile ? 10.f : 0.f);
			xl_particle_soft_end_scene();
			glReadPixels(31, 31, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels[profile].data());
		}
		if (pixels[0] != pixels[1]) throw std::runtime_error("Sprite missing-depth fallback differs from ordinary draw");
		++cases;
	}
	return cases;
}

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
					for (bool additive : { false, true })
						for (bool scissored : { false, true }) {
							glViewport(0, 0, 64, 64);
							glDisable(GL_SCISSOR_TEST);
							glDepthMask(GL_TRUE);
#ifdef ANDROID
							glClearDepthf(occluded ? .25f : 1.f);
#else
							glClearDepth(occluded ? .25 : 1.);
#endif
							const std::array<unsigned char, 4> background{ 32, 64, 96, 128 };
							glClearColor(background[0] / 255.f, background[1] / 255.f, background[2] / 255.f, background[3] / 255.f);
							glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
							glScissor(9, 12, 25, 25);
							if (scissored) glEnable(GL_SCISSOR_TEST);
							ParticleQuad quad{};
							quad.blend = additive ? ParticleBlend::additive : ParticleBlend::alpha;
							const std::array<std::array<float, 2>, 4> corners{ { { -1, 1 }, { 1, 1 }, { 1, -1 }, { -1, -1 } } };
							for (unsigned i = 0; i < 4; ++i) {
								quad.vertices[i].position = { corners[i][0] * size + .0078125f, corners[i][1] * size - .0234375f, 0 };
								quad.vertices[i].uv = { (corners[i][0] + 1) * .5f, (1 - corners[i][1]) * .5f };
								const float modulation = additive ? .5f : 1.f;
								quad.vertices[i].color = { modulation, modulation, modulation, 2.f / 3.f };
							}
							glDepthFunc(GL_GREATER);
							glDisable(GL_BLEND);
							glBlendFuncSeparate(GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_ZERO, GL_ONE);
							glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_SUBTRACT);
							draw_bitmap_sprite(image.bitmap, quad);
							GLint src_rgb, dst_rgb, src_alpha, dst_alpha, equation_rgb, equation_alpha;
							glGetIntegerv(GL_BLEND_SRC_RGB, &src_rgb);
							glGetIntegerv(GL_BLEND_DST_RGB, &dst_rgb);
							glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_alpha);
							glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_alpha);
							glGetIntegerv(GL_BLEND_EQUATION_RGB, &equation_rgb);
							glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &equation_alpha);
							require(!glIsEnabled(GL_BLEND) && src_rgb == GL_DST_ALPHA && dst_rgb == GL_ONE_MINUS_DST_ALPHA &&
							            src_alpha == GL_ZERO && dst_alpha == GL_ONE && equation_rgb == GL_FUNC_REVERSE_SUBTRACT && equation_alpha == GL_FUNC_SUBTRACT,
							        "Sprite body device control leaked blend state");
							GLint actual_min, actual_mag;
							glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &actual_min);
							glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &actual_mag);
							require(actual_min == GLint(filter) && actual_mag == (filter == GL_NEAREST ? GL_NEAREST : GL_LINEAR), "Sprite body device control lost its explicit filter");
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
										const double foreground = c == 3 ? alpha : rgba[c] / 255. * coverage * (additive ? .5 : 1.);
										const double result = additive ? foreground + background[c] / 255. : foreground * alpha + background[c] / 255. * (1 - alpha);
										const int expected = visible ? int(std::lround(255 * (std::min) (1., result))) : background[c];
										if (std::abs(int(pixels[(y * 64 + x) * 4 + c]) - expected) > 1)
											throw std::runtime_error("Sprite body device pixels differ: width=" + std::to_string(shape[0]) + " height=" + std::to_string(shape[1]) + " filter=" + std::to_string(filter) + " size=" + std::to_string(size) + " x=" + std::to_string(x) + " y=" + std::to_string(y) + " expected=" + std::to_string(expected) + " actual=" + std::to_string(pixels[(y * 64 + x) * 4 + c]) + " rgba=" + std::to_string(pixels[(y * 64 + x) * 4]) + "," + std::to_string(pixels[(y * 64 + x) * 4 + 1]) + "," + std::to_string(pixels[(y * 64 + x) * 4 + 2]) + "," + std::to_string(pixels[(y * 64 + x) * 4 + 3]));
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
