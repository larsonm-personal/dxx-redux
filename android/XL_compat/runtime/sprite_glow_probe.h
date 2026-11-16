#include "lightning_glow_gpu.h"
namespace sprite_glow_control
{
#include "particle_depth.h"

// Independently evaluate source Render viewport mapping and blurFS on UNORM8
// images; cropped/offset views resample the image even for a constant input
static int expected_channel(int original, int offset, int strength, bool replace, float brightness)
{
	using Pixels = std::array<double, 64 * 64>;
	std::array<Pixels, 3> images{};
	images[0].fill(original);
	const int size = 64 - offset;
	const auto sample = [](const Pixels &pixels, double u, double v) {
		const double x = u * 64 - .5, y = v * 64 - .5;
		const int ix = int(std::floor(x)), iy = int(std::floor(y));
		const double fx = x - ix, fy = y - iy;
		const auto at = [&](int x, int y) { return pixels[std::clamp(y, 0, 63) * 64 + std::clamp(x, 0, 63)]; };
		return (at(ix, iy) * (1 - fx) + at(ix + 1, iy) * fx) * (1 - fy) + (at(ix, iy + 1) * (1 - fx) + at(ix + 1, iy + 1) * fx) * fy;
	};
	const auto coordinate = [&](int pixel, int radius) {
		const double first = std::clamp(double(offset - radius) / 64, 0., 1.);
		const double last = std::clamp(double(offset + size + radius) / 64, 0., 1.);
		return first + (last - first) * (pixel + .5 - offset) / size;
	};
	for (int pass = 0; pass < 2 * strength; ++pass) {
		const auto &source = images[pass ? 1 + (pass - 1) % 2 : 0];
		auto &target = images[1 + pass % 2];
		const int radius = 3 * (1 + pass / 2);
		for (int y = offset; y < 64; ++y)
			for (int x = offset; x < 64; ++x) {
				const double u = coordinate(x, radius), v = coordinate(y, radius);
				const double dx = pass % 2 ? 0 : 1. / 64, dy = pass % 2 ? 1. / 64 : 0;
				double value = sample(source, u, v) * .2270270270;
				value += (sample(source, u + dx, v + dy) + sample(source, u - dx, v - dy)) * .3162162162;
				value += (sample(source, u + 2 * dx, v + 2 * dy) + sample(source, u - 2 * dx, v - 2 * dy)) * .0702702703;
				value = value * brightness + (pass < 2 ? 0 : target[y * 64 + x]);
				target[y * 64 + x] = std::clamp(std::round(value), 0., 255.);
			}
	}
	const double uv = coordinate(offset + 24, 3 * strength);
	const int blurred = int(std::lround(sample(images[2], uv, uv)));
	return (std::min) (255, blurred + (replace ? 0 : original));
}

static unsigned runtime_controls()
{
	using namespace xl_compat;
	struct Image {
		grs_bitmap bitmap{};
		std::array<unsigned char, 4> pixel{ 32, 64, 96, 128 };
		~Image()
		{
			ogl_freebmtexture(&bitmap);
			discard_sprite_glow(false);
		}
	} image;
	image.bitmap.bm_w = image.bitmap.bm_h = 1;
	image.bitmap.bm_rowsize = 4;
	image.bitmap.bm_type = BM_RGBA;
	image.bitmap.bm_data = image.pixel.data();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glViewport(0, 0, 64, 64);
	glDisable(GL_SCISSOR_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(.75);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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
	for (float center : { -.4f, .4f }) {
		ParticleQuad quad{};
		quad.blend = ParticleBlend::additive;
		const std::array<std::array<float, 2>, 4> corners{ { { -1, 1 }, { 1, 1 }, { 1, -1 }, { -1, -1 } } };
		for (unsigned i = 0; i < 4; ++i) {
			quad.vertices[i].position = { center + corners[i][0] * .125f, corners[i][1] * .125f, -.5f };
			quad.vertices[i].uv = { .5f, .5f };
			quad.vertices[i].color = { .5f, .5f, .5f, .4f };
		}
		draw_fireball_sprite(image.bitmap, quad);
	}
	std::array<unsigned char, 64 * 64 * 4> before{}, after{}, repeated{};
	glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, before.data());
	if (std::any_of(before.begin(), before.end(), [](unsigned char value) { return value != 0; }))
		throw std::runtime_error("Fireball glow batch composed before its boundary");
	finish_sprite_glow();
	glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, after.data());
	finish_sprite_glow();
	glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, repeated.data());
	if (after != repeated) throw std::runtime_error("Fireball glow was composed twice");
	for (unsigned x : { 19u, 24u, 39u, 44u })
		if (!after[(32 * 64 + x) * 4]) throw std::runtime_error("Fireball glow lost a retained sprite or its surrounding blur");
	float retained_depth = 0;
	glReadPixels(32, 32, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &retained_depth);
	if (std::abs(retained_depth - .75f) > .00001f) throw std::runtime_error("Fireball glow changed scene depth");
	return 2;
}

static unsigned bounds_controls()
{
	using namespace xl_compat;
	LightningGlowGpu compositor;
	ParticleDepth depth;
	struct Release {
		LightningGlowGpu &compositor;
		ParticleDepth &depth;
		~Release()
		{
			compositor.discard(false);
			depth.discard(false);
		}
	} release{ compositor, depth };
	GLint target = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &target);
	glViewport(0, 0, 64, 64);
	depth.capture();
	const std::array<int, 4> left{ -30, 10, -10, 20 }, middle{ 10, 10, 20, 20 }, right{ 70, 10, 90, 20 };
	const std::array<std::vector<std::array<int, 4>>, 4> sequences{ { { left, middle, right }, { middle, left, right }, { left, left }, { left, right } } };
	const std::array<unsigned, 4> expected{ 2, 3, 0, 1 };
	for (unsigned index = 0; index < sequences.size(); ++index) {
		compositor.begin(GLuint(target), depth.texture, GL_DEPTH_ATTACHMENT, 64, 64, { 0, 0, 64, 64 }, 2, false, .85f);
		unsigned draws = 0;
		for (const auto &extent : sequences[index]) compositor.draw_sprite(extent, true, [&] { ++draws; });
		if (draws != expected[index] || compositor.end() != (draws != 0))
			throw std::runtime_error("Sprite glow lost source cumulative viewport bounds: sequence=" + std::to_string(index));
	}
	return unsigned(sequences.size());
}

static unsigned controls()
{
	using namespace xl_compat;
	LightningGlowGpu compositor;
	ParticleDepth depth;
	struct Release {
		LightningGlowGpu &compositor;
		ParticleDepth &depth;
		~Release()
		{
			compositor.discard(false);
			depth.discard(false);
		}
	} release{ compositor, depth };
	GLint target = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &target);
	unsigned cases = 0;
	for (int profile : { 0, 1, 2 })
		for (int offset : { 0, 5 })
			for (bool samples_depth : { false, true }) {
				const int strength = profile == 0 ? 3 : profile == 1 ? 2
				                                                     : 1;
				const bool replace = profile == 0;
				const float brightness = profile == 0 ? 1.05f : profile == 1 ? .85f
				                                                             : .95f;
				glViewport(0, 0, 64, 64);
				glDisable(GL_SCISSOR_TEST);
				glDepthMask(GL_TRUE);
				glClearDepth(.75);
				glClearColor(0, 0, 0, 0);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				depth.capture();
				const std::array<int, 4> view{ offset, offset, 64 - offset, 64 - offset };
				glViewport(view[0], view[1], view[2], view[3]);
				compositor.begin(GLuint(target), depth.texture, GL_DEPTH_ATTACHMENT, 64, 64, view, strength, replace, brightness);
				compositor.draw_sprite({ 0, 0, view[2], view[3] }, samples_depth, [&] {
					GLint type = 0;
					glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
					if (type != (samples_depth ? GL_NONE : GL_TEXTURE)) throw std::runtime_error("Sprite glow depth attachment risks sampling feedback");
					glClearColor(8.f / 255, 16.f / 255, 24.f / 255, 1);
					glClear(GL_COLOR_BUFFER_BIT);
				});
				// A compatible begin must preserve the previously drawn accumulation
				compositor.begin(GLuint(target), depth.texture, GL_DEPTH_ATTACHMENT, 64, 64, view, strength, replace, brightness);
				if (!compositor.end() || compositor.end()) throw std::runtime_error("Sprite glow batch did not compose exactly once");
				std::array<unsigned char, 4> actual{};
				glReadPixels(offset + 24, offset + 24, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
				for (unsigned c = 0; c < 3; ++c) {
					// Avoid exact half-byte rounding ties in this analytical control
					const int original = 8 + 8 * c;
					const int expected = expected_channel(original, offset, strength, replace, brightness);
					if (std::abs(int(actual[c]) - expected) > 1)
						throw std::runtime_error("Sprite glow profile pixels differ: profile=" + std::to_string(profile) + " channel=" + std::to_string(c) + " actual=" + std::to_string(actual[c]) + " expected=" + std::to_string(expected));
				}
				float observed_depth = 0;
				glReadPixels(offset + 24, offset + 24, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &observed_depth);
				if (std::abs(observed_depth - .75f) > .00001f) throw std::runtime_error("Sprite glow modified original scene depth");
				++cases;
			}
	return cases;
}
} // namespace sprite_glow_control
