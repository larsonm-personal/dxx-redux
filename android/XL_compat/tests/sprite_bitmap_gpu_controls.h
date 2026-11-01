#include "../runtime/sprite_bitmap_gpu_probe.h"
#include "sprite_mip_diagnostic.h"
static bool sprite_nearest_boundary_diagnostic(const Bitmap &image, const xl_compat::ParticleQuad &quad, unsigned pixel, const std::array<unsigned char, 64 * 64 * 4> &actual)
{
	const double left = (double(quad.vertices[0].position[0]) + 1) * 32;
	const double right = (double(quad.vertices[1].position[0]) + 1) * 32;
	const double top = (double(quad.vertices[0].position[1]) + 1) * 32;
	const double bottom = (double(quad.vertices[3].position[1]) + 1) * 32;
	const double s = (double(pixel % 64) + .5 - left) / (right - left) * image.bitmap.bm_w;
	const double t = (top - double(pixel / 64) - .5) / (top - bottom) * image.bitmap.bm_h;
	// Only adjacent texels at a narrowly bounded floating-point sampling boundary qualify
	const double sx = 32 * 1.1920928955078125e-7 * (std::max) (1.0, std::abs(s));
	const double tx = 32 * 1.1920928955078125e-7 * (std::max) (1.0, std::abs(t));
	if (std::abs(s - std::round(s)) > sx && std::abs(t - std::round(t)) > tx) return false;
	for (int y = int(std::floor(t - tx)); y <= int(std::floor(t + tx)); ++y)
		for (int x = int(std::floor(s - sx)); x <= int(std::floor(s + sx)); ++x) {
			if (x < 0 || y < 0 || x >= image.bitmap.bm_w || y >= image.bitmap.bm_h) continue;
			auto value = image.pixel(x, y);
			for (unsigned channel = 0; channel < 4; ++channel) value[channel] *= quad.vertices[0].color[channel];
			const std::array<double, 4> background{ .2, .4, .6, 1 };
			bool matches = true;
			for (unsigned channel = 0; channel < 4; ++channel) {
				const int expected = int(std::lround(255 * (value[channel] * value[3] + background[channel] * (1 - value[3]))));
				if (std::abs(expected - int(actual[pixel * 4 + channel])) > 1) matches = false;
			}
			if (matches) return true;
		}
	return false;
}
static void sprite_bitmap_gpu_controls(bool source_quads = true)
{
	GLuint target, color, depth;
	glGenFramebuffers(1, &target);
	glGenTextures(1, &color);
	glGenRenderbuffers(1, &depth);
	glBindFramebuffer(GL_FRAMEBUFFER, target);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, color);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 64, 64);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
	check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Sprite bitmap target incomplete");
	glViewport(0, 0, 64, 64);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	const auto previous_textured = glIsEnabled(GL_TEXTURE_2D);
	const int previous_texture_cache = GL_TEXTURE_2D_enabled;
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	unsigned cases = 0, failures = 0, nearest_pixels = 0, sampler_pixels = 0;
	int maximum = 0;
	for (const auto shape : { std::array<int, 2>{ 1, 1 }, { 8, 8 }, { 8, 4 }, { 4, 8 }, { 7, 3 } }) {
		Bitmap image(shape[0], shape[1], true);
		for (unsigned i = 0; i < image.data.size(); ++i) image.data[i] = static_cast<unsigned char>((i * 17 + i / 4 * 31 + 43) % 256);
		// A 1x1 image has no level1 for the upload helper's mip-size assertion
		upload(image, shape[0] == 1 && shape[1] == 1 ? 0 : 2);
		const SpriteMipDiagnostic math(image);
		for (GLenum min_filter : { GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR })
			for (GLenum mag_filter : { GL_NEAREST, GL_LINEAR }) {
				glBindTexture(GL_TEXTURE_2D, image.bitmap.gltexture->handle);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
				for (float center : { .015625f, 0.f, .037f })
					for (float size : { .03125f, .07f, .2f, .7f, 1.3f }) {
						xl_compat::ParticleQuad quad{};
						quad.blend = xl_compat::ParticleBlend::alpha;
						const std::array<std::array<float, 2>, 4> corners{ { { -1, 1 }, { 1, 1 }, { 1, -1 }, { -1, -1 } } };
						for (unsigned i = 0; i < 4; ++i) {
							quad.vertices[i].position = { corners[i][0] * size + center, corners[i][1] * size - .03125f, 0 };
							quad.vertices[i].uv = { (corners[i][0] + 1) * .5f, (1 - corners[i][1]) * .5f };
							quad.vertices[i].color = { .7f, .8f, .9f, 2.f / 3.f };
						}
						std::array<std::array<unsigned char, 64 * 64 * 4>, 2> pixels{};
						for (unsigned profile = 0; profile < 2; ++profile) {
							glClearColor(.2f, .4f, .6f, 1);
							glDepthMask(GL_TRUE);
							glClearDepth(1);
							glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
							if (profile) {
								glDepthFunc(GL_GREATER);
								xl_compat::draw_bitmap_sprite(image.bitmap, quad);
								GLint function;
								glGetIntegerv(GL_DEPTH_FUNC, &function);
								check(function == GL_GREATER, "Sprite bitmap leaked depth function");
							} else {
								glUseProgram(0);
								glDepthFunc(GL_LEQUAL);
								glDepthMask(GL_FALSE);
								glBindTexture(GL_TEXTURE_2D, image.bitmap.gltexture->handle);
								glColor4fv(quad.vertices[0].color.data());
								glBegin(source_quads ? GL_QUADS : GL_TRIANGLES);
								const std::array<unsigned, 6> triangles{ 0, 1, 3, 1, 2, 3 };
								for (unsigned entry = 0; entry < (source_quads ? 4u : 6u); ++entry) {
									const unsigned index = source_quads ? entry : triangles[entry];
									const auto &vertex = quad.vertices[index];
									glTexCoord2f(vertex.uv[0] * image.bitmap.gltexture->u, vertex.uv[1] * image.bitmap.gltexture->v);
									glVertex3fv(vertex.position.data());
								}
								glEnd();
							}
							glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels[profile].data());
						}
						int error = 0;
						for (unsigned i = 0; i < pixels[0].size(); ++i) error = (std::max) (error, std::abs(int(pixels[0][i]) - int(pixels[1][i])));
						maximum = (std::max) (maximum, error);
						if (error > 1) {
							unsigned mismatches = 0, first = 0, boundary_pixels = 0, math_pixels = 0, other_pixels = 0;
							for (unsigned pixel = 0; pixel < 64 * 64; ++pixel) {
								bool differs = false;
								for (unsigned channel = 0; channel < 4; ++channel)
									if (std::abs(int(pixels[0][pixel * 4 + channel]) - int(pixels[1][pixel * 4 + channel])) > 1) differs = true;
								if (!differs) continue;
								if (mag_filter == GL_NEAREST && (std::max) (shape[0], shape[1]) <= double(quad.vertices[1].position[0] - quad.vertices[0].position[0]) * 32 && sprite_nearest_boundary_diagnostic(image, quad, pixel, pixels[0]) && sprite_nearest_boundary_diagnostic(image, quad, pixel, pixels[1])) ++boundary_pixels;
								else if (error <= 2 && (min_filter == GL_NEAREST_MIPMAP_LINEAR || min_filter == GL_LINEAR_MIPMAP_LINEAR) && math.matches(quad, pixel, min_filter, mag_filter, true, pixels[0]) && math.matches(quad, pixel, min_filter, mag_filter, false, pixels[1])) ++math_pixels;
								else ++other_pixels;
							}
							nearest_pixels += boundary_pixels;
							sampler_pixels += math_pixels;
							for (unsigned i = 0; i < pixels[0].size(); ++i)
								if (std::abs(int(pixels[0][i]) - int(pixels[1][i])) > 1) {
									if (!mismatches) first = i;
									++mismatches;
								}
							if (other_pixels) {
								++failures;
								std::cerr << std::string("Sprite GL_CLAMP differs: width=" + std::to_string(shape[0]) + " height=" + std::to_string(shape[1]) + " min=" + std::to_string(min_filter) + " mag=" + std::to_string(mag_filter) + " center=" + std::to_string(center) + " size=" + std::to_string(size) + " error=" + std::to_string(error) + " channels=" + std::to_string(mismatches) + " x=" + std::to_string(first / 4 % 64) + " y=" + std::to_string(first / 256) + " source=" + std::to_string(pixels[0][first]) + " native=" + std::to_string(pixels[1][first])) << '\n';
							}
						}
						check(glGetError() == GL_NO_ERROR, "Sprite bitmap control raised GL error");
						++cases;
					}
			}
	}
	if (failures) throw std::runtime_error("Sprite GL_CLAMP failed " + std::to_string(failures) + "/" + std::to_string(cases) + " cases; max error " + std::to_string(maximum));
	check(sprite_bitmap_device_controls() == 108, "Analytical sprite body controls did not complete");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &target);
	glDeleteRenderbuffers(1, &depth);
	glDeleteTextures(1, &color);
	if (previous_textured) glEnable(GL_TEXTURE_2D);
	else glDisable(GL_TEXTURE_2D);
	GL_TEXTURE_2D_enabled = previous_texture_cache;
	std::cout << "Sprite bitmap GL_CLAMP (" << (source_quads ? "source quads" : "diagnostic triangles") << "): " << cases << " shape/size/filter comparisons, nearest-boundary pixels " << nearest_pixels << ", sampler-precision pixels " << sampler_pixels << ", raw max error " << maximum << '\n';
}
