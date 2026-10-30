// Reuse the existing EGL context and production RGBA uploader
#ifdef XL_POG_SHADER_REFERENCE
#include "xl_reference_particle_blend.h"
#endif
static void particle_gpu_controls()
{
	Bitmap atlas(128, 128, true);
	const std::array<std::array<unsigned char, 4>, 4> cells{ { { 31, 103, 181, 255 }, { 61, 137, 19, 255 }, { 223, 151, 79, 255 }, { 17, 43, 67, 255 } } };
	for (unsigned y = 0; y < 128; ++y)
		for (unsigned x = 0; x < 128; ++x)
			std::copy(cells[y / 64 * 2 + x / 64].begin(), cells[y / 64 * 2 + x / 64].end(), atlas.data.begin() + (y * 128 + x) * 4);
	upload(atlas, 0);
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
	for (int frame : { 0, 2 })
		for (int orientation = 0; orientation < 4; ++orientation)
			for (bool blocked : { false, true }) {
				particle.frame = frame;
				particle.orientation = orientation;
				const auto quad = xl_compat::fire_particle_quad(particle, view, 128, 128);
#ifdef XL_POG_SHADER_REFERENCE
				const auto factors = source_particle_blend(frame < 2 ? 5 : PARTICLE_TYPES + 5, 2);
				check(factors == (frame < 2 ? std::array<GLenum, 2>{ GL_ONE, GL_ONE_MINUS_SRC_COLOR } : std::array<GLenum, 2>{ GL_DST_COLOR, GL_ZERO }), "Source particle blend differs from GPU comparison equation");
#endif
				glClearColor(background[0] / 255.f, background[1] / 255.f, background[2] / 255.f, 1);
				glClearDepth(blocked ? .25 : 1);
				glDepthMask(GL_TRUE);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				glEnable(GL_CULL_FACE);
				glDisable(GL_BLEND);
				glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
				glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_ADD);
				glBindTexture(GL_TEXTURE_2D, color);
				xl_compat::draw_particle_quads(atlas.bitmap, &quad, 1);
				GLboolean writes;
				glGetBooleanv(GL_DEPTH_WRITEMASK, &writes);
				GLint src, dst, src_alpha, dst_alpha, eq, binding;
				glGetIntegerv(GL_BLEND_SRC_RGB, &src);
				glGetIntegerv(GL_BLEND_DST_RGB, &dst);
				glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_alpha);
				glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_alpha);
				glGetIntegerv(GL_BLEND_EQUATION_RGB, &eq);
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
				GLfloat alpha;
				glGetFloatv(GL_ALPHA_TEST_REF, &alpha);
				check(writes && glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_CULL_FACE) && !glIsEnabled(GL_BLEND) && src == GL_SRC_ALPHA && dst == GL_ONE_MINUS_SRC_ALPHA && src_alpha == GL_ONE && dst_alpha == GL_ZERO && eq == GL_FUNC_REVERSE_SUBTRACT && GLuint(binding) == color && alpha == .125f, "Particle draw leaked GL state");
				std::array<unsigned char, 16 * 16 * 4> actual;
				std::array<float, 16 * 16> actual_depth;
				glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
				glReadPixels(0, 0, 16, 16, GL_DEPTH_COMPONENT, GL_FLOAT, actual_depth.data());
				check(glGetError() == GL_NO_ERROR, "Particle draw produced a GL error");
				for (unsigned p = 0; p < 16 * 16; ++p) {
					check(std::abs(actual_depth[p] - (blocked ? .25f : 1.0f)) < .00001f, "Particle draw changed scene depth");
					for (unsigned c = 0; c < 4; ++c) {
						const double s = cells[unsigned(frame)][c] / 255., d = background[c] / 255.;
						const double want = blocked ? d : frame == 0 ? s + d * (1 - s)
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
		xl_compat::draw_particle_quads(atlas.bitmap, overlapping.data(), overlapping.size());
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
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glDeleteRenderbuffers(1, &depth);
	glDeleteTextures(1, &color);
	glDeleteFramebuffers(1, &framebuffer);
	std::cout << "Production particle GPU: " << comparisons << " atlas/blend/depth cases, complete pixel comparison, preserved scene depth and restored GL state passed\n";
}
