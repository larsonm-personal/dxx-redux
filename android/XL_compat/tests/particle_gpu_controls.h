// Reuse the existing EGL context and production RGBA uploader
#ifdef XL_POG_SHADER_REFERENCE
#include "xl_reference_particle_blend.h"
#include "particle_soft_gpu_controls.h"
#endif
#include "transparency_gpu_controls.h"
#include "particle_buffer_gpu_controls.h"
static void particle_gpu_controls()
{
#ifdef XL_POG_SHADER_REFERENCE
	particle_soft_shader_controls();
#endif
	Bitmap atlas(128, 128, true);
	const std::array<std::array<unsigned char, 4>, 4> cells{ { { 31, 103, 181, 255 }, { 61, 137, 19, 255 }, { 223, 151, 79, 255 }, { 17, 43, 67, 255 } } };
	for (unsigned y = 0; y < 128; ++y)
		for (unsigned x = 0; x < 128; ++x)
			std::copy(cells[y / 64 * 2 + x / 64].begin(), cells[y / 64 * 2 + x / 64].end(), atlas.data.begin() + (y * 128 + x) * 4);
	upload(atlas, 0);
	ogl_freebmtexture(&atlas.bitmap);
	xl_compat::PogBitmap pixels;
	pixels.width = pixels.height = 128;
	pixels.truecolor = true;
	pixels.pixels = atlas.data;
	const int textures_before = r_texcount;
	auto texture = std::make_unique<xl_compat::ParticleTexture>(pixels, "particle-gpu-control");
	texture->draw(nullptr, 0);
	check(r_texcount == textures_before, "Empty particle draw allocated a GPU texture");
	GLuint framebuffer, color, depth;
	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glGenTextures(1, &color);
	glBindTexture(GL_TEXTURE_2D, color);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
	glGenRenderbuffers(1, &depth);
	glBindRenderbuffer(GL_RENDERBUFFER, depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 16, 16);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
	check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Particle GPU framebuffer incomplete");
	glViewport(0, 0, 16, 16);
	glUseProgram(0);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GEQUAL, .125f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	const std::array<unsigned char, 4> background{ 73, 119, 167, 255 };
	xl_compat::ParticleView view{};
	view.basis = { { 65536, 0, 0 }, { 0, 65536, 0 } };
	view.rotate = false;
	xl_compat::FireParticle particle{};
	particle.width = particle.height = particle.decay = 1;
	unsigned comparisons = 0;
	for (int frame : { 0, 2, 4 })
		for (int orientation = 0; orientation < 4; ++orientation)
			for (float scene_depth : { .25f, .5f, 1.0f }) {
				const bool blocked = scene_depth < .5f;
				particle.frame = frame == 4 ? 0 : frame;
				particle.orientation = orientation;
				auto quad = xl_compat::fire_particle_quad(particle, view, 128, 128);
				const std::array<float, 4> tint{ .7f, .8f, .9f, .3f };
				if (frame == 4) {
					quad.blend = xl_compat::ParticleBlend::alpha;
					for (auto &vertex : quad.vertices) vertex.color = tint;
				}
#ifdef XL_POG_SHADER_REFERENCE
				const auto factors = source_particle_blend(frame == 4 ? 1 : frame < 2 ? 5
				                                                                      : PARTICLE_TYPES + 5,
				                                           frame == 4 ? 0 : 2);
				check(factors == (frame == 4 ? std::array<GLenum, 2>{ GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA } : frame < 2 ? std::array<GLenum, 2>{ GL_ONE, GL_ONE_MINUS_SRC_COLOR }
				                                                                                                         : std::array<GLenum, 2>{ GL_DST_COLOR, GL_ZERO }),
				      "Source particle blend differs from GPU comparison equation");
#endif
				glClearColor(background[0] / 255.f, background[1] / 255.f, background[2] / 255.f, 1);
				glClearDepth(scene_depth);
				glDepthMask(GL_TRUE);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				glEnable(GL_CULL_FACE);
				glDisable(GL_BLEND);
				glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
				glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_ADD);
				glBindTexture(GL_TEXTURE_2D, color);
				if (comparisons == 8) {
					xl_particle_images_discard_context(0);
					check(r_texcount == textures_before && glIsTexture(color), "Particle release leaked a texture or discarded another owner");
				}
				texture->draw(&quad, 1);
				check(r_texcount == textures_before + 1, "Particle upload/reload did not retain exactly one owned texture");
				GLboolean writes;
				glGetBooleanv(GL_DEPTH_WRITEMASK, &writes);
				GLint src, dst, src_alpha, dst_alpha, eq, binding, depth_function;
				glGetIntegerv(GL_DEPTH_FUNC, &depth_function);
				glGetIntegerv(GL_BLEND_SRC_RGB, &src);
				glGetIntegerv(GL_BLEND_DST_RGB, &dst);
				glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_alpha);
				glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_alpha);
				glGetIntegerv(GL_BLEND_EQUATION_RGB, &eq);
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
				GLfloat alpha;
				glGetFloatv(GL_ALPHA_TEST_REF, &alpha);
				check(writes && depth_function == GL_LESS && glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_CULL_FACE) && !glIsEnabled(GL_BLEND) && src == GL_SRC_ALPHA && dst == GL_ONE_MINUS_SRC_ALPHA && src_alpha == GL_ONE && dst_alpha == GL_ZERO && eq == GL_FUNC_REVERSE_SUBTRACT && GLuint(binding) == color && alpha == .125f, "Particle draw leaked GL state");
				std::array<unsigned char, 16 * 16 * 4> actual;
				std::array<float, 16 * 16> actual_depth;
				glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
				glReadPixels(0, 0, 16, 16, GL_DEPTH_COMPONENT, GL_FLOAT, actual_depth.data());
				check(glGetError() == GL_NO_ERROR, "Particle draw produced a GL error");
				for (unsigned p = 0; p < 16 * 16; ++p) {
					check(std::abs(actual_depth[p] - scene_depth) < .00001f, "Particle draw changed scene depth");
					for (unsigned c = 0; c < 4; ++c) {
						const double s = cells[unsigned(frame == 4 ? 0 : frame)][c] / 255. * (frame == 4 ? tint[c] : 1), d = background[c] / 255.;
						const double alpha = cells[0][3] / 255. * tint[3];
						const double want = blocked ? d : frame == 4 ? s * alpha + d * (1 - alpha)
						                              : frame == 0   ? s + d * (1 - s)
						                                             : s * d;
						check(std::abs(int(actual[p * 4 + c]) - int(want * 255 + .5)) <= 1, "Particle fire/smoke GPU blend differs from source equation");
					}
				}
				++comparisons;
			}
	// Mixed blending is order-sensitive; the backend must preserve its caller's
	// sorted sequence instead of grouping all fire and then all smoke
	std::array<xl_compat::ParticleQuad, 2> overlapping;
	for (unsigned reverse = 0; reverse < 2; ++reverse) {
		for (unsigned i = 0; i < 2; ++i) {
			particle.frame = int((i ^ reverse) * 2);
			overlapping[i] = xl_compat::fire_particle_quad(particle, view, 128, 128);
		}
		glClearDepth(1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		texture->draw(overlapping.data(), overlapping.size());
		std::array<unsigned char, 4> actual;
		glReadPixels(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
		for (unsigned c = 0; c < 4; ++c) {
			double want = background[c] / 255.;
			for (unsigned i = 0; i < 2; ++i) {
				const unsigned frame = (i ^ reverse) * 2;
				const double s = cells[frame][c] / 255.;
				want = frame ? s * want : s + want * (1 - s);
			}
			check(std::abs(int(actual[c]) - int(want * 255 + .5)) <= 1, "Particle backend reordered mixed fire and smoke");
		}
		++comparisons;
	}
	check(glGetError() == GL_NO_ERROR, "Mixed particle draw produced a GL error");
	particle_buffer_gpu_controls(*texture);
	transparency_gpu_controls(*texture, cells, background);
	// Independently apply the camera using OpenGL's matrix stack, then compare
	// the production CPU conversion. This keeps projection out of the oracle
	unsigned camera_cases = 0;
	for (int turn = 0; turn < 4; ++turn)
		for (int scale : { 32768, 65536, 98304 }) {
			const xl_compat::ParticlePoint eye{ 311777, -817711, 119331 };
			const int sine = turn == 1 ? 52428 : turn == 2 ? 65536
			                                 : turn == 3   ? -52428
			                                               : 0;
			const int cosine = turn == 1 || turn == 3 ? 39321 : turn == 2 ? 0
			                                                              : 65536;
			const std::array<xl_compat::ParticlePoint, 3> camera{ xl_compat::ParticlePoint{ cosine, 0, -sine }, xl_compat::ParticlePoint{ 0, 65536, 0 }, xl_compat::ParticlePoint{ sine, 0, cosine } };
			std::array<xl_compat::ParticlePoint, 3> scaled = camera;
			for (auto &row : scaled)
				for (auto &coordinate : row) coordinate = int(std::int64_t(coordinate) * scale / 65536);
			particle.position = eye;
			for (unsigned axis = 0; axis < 3; ++axis) particle.position[axis] += camera[2][axis] / 3;
			particle.frame = 0;
			particle.width = .373f;
			particle.height = .219f;
			view.basis = { camera[0], camera[1] };
			const auto world_quad = xl_compat::fire_particle_quad(particle, view, 128, 128);
			auto transformed = world_quad;
			xl_compat::particle_camera_quad(transformed, eye, scaled);
			std::array<float, 16> matrix{};
			matrix[15] = 1;
			for (unsigned row = 0; row < 3; ++row)
				for (unsigned column = 0; column < 3; ++column) matrix[column * 4 + row] = float(scaled[row][column]) / 65536.f * (row == 2 ? -1.f : 1.f);
			glLoadMatrixf(matrix.data());
			glTranslatef(-float(eye[0]) / 65536.f, -float(eye[1]) / 65536.f, -float(eye[2]) / 65536.f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			texture->draw(&world_quad, 1);
			std::array<unsigned char, 16 * 16 * 4> expected, actual;
			glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, expected.data());
			glLoadIdentity();
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			texture->draw(&transformed, 1);
			glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
			unsigned covered = 0;
			for (unsigned p = 0; p < 16 * 16; ++p) {
				covered += expected[p * 4] != background[0];
				for (unsigned c = 0; c < 4; ++c) check(std::abs(int(actual[p * 4 + c]) - int(expected[p * 4 + c])) <= 1, "Particle camera conversion differs from OpenGL matrix projection");
			}
			check(covered > 0 && covered < 16 * 16, "Particle camera comparison did not cover a bounded visible quad");
			++camera_cases;
		}
	check(glGetError() == GL_NO_ERROR, "Particle camera comparison produced a GL error");
	texture.reset();
	check(r_texcount == textures_before, "Particle image destruction leaked its GPU texture");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glDeleteRenderbuffers(1, &depth);
	glDeleteTextures(1, &color);
	glDeleteFramebuffers(1, &framebuffer);
	std::cout << "Production particle GPU: " << comparisons << " atlas/blend/depth cases, " << camera_cases << " independent camera projections, complete pixel comparison, preserved scene depth and restored GL state passed\n";
}

template <class Recreate>
static void particle_context_controls(Recreate recreate)
{
	xl_compat::PogBitmap pixels;
	pixels.width = pixels.height = 16;
	pixels.truecolor = true;
	pixels.pixels.resize(16 * 16 * 4);
	const std::array<unsigned char, 4> expected{ 41, 79, 137, 255 };
	for (unsigned p = 0; p < 16 * 16; ++p) std::copy(expected.begin(), expected.end(), pixels.pixels.begin() + p * 4);
	auto texture = std::make_unique<xl_compat::ParticleTexture>(pixels, "retained-particle-image");
	xl_compat::ParticleView view{};
	view.basis = { { 65536, 0, 0 }, { 0, 65536, 0 } };
	view.rotate = false;
	xl_compat::FireParticle particle{};
	particle.width = particle.height = particle.decay = 1;
	const auto quad = xl_compat::fire_particle_quad(particle, view, 16, 16);
	const auto draw = [&] {
		glViewport(0, 0, 32, 32);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glClearColor(0, 0, 0, 0);
		glClearDepth(1);
		glDepthMask(GL_TRUE);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		texture->draw(&quad, 1);
		std::array<unsigned char, 4> actual;
		glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
		check(actual == expected && glGetError() == GL_NO_ERROR, "Retained particle image failed after context replacement");
	};
	draw();
	// Destroy the real EGL context, keeping the owned decoded image alive.
	// The engine pool initialization hook must forget the old slot pointer
	recreate();
	const int baseline = r_texcount;
	Bitmap other(16, 16, true);
	for (unsigned p = 0; p < 16 * 16; ++p) {
		other.data[p * 4] = 211;
		other.data[p * 4 + 3] = 255;
	}
	upload(other, 0);
	const GLuint other_handle = other.bitmap.gltexture->handle;
	draw();
	check(r_texcount == baseline + 2 && glIsTexture(other_handle), "Particle context reload reused or deleted another image's slot");
	texture.reset();
	check(r_texcount == baseline + 1 && glIsTexture(other_handle), "Particle destruction freed the replacement context's other texture");
	ogl_freebmtexture(&other.bitmap);
	check(r_texcount == baseline, "Particle context replacement leaked a texture");
	std::cout << "Production particle context: retained pixels, real EGL recreation, reused-pool isolation and destruction passed\n";
}
