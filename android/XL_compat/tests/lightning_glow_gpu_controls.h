#include "reference_lightning_composite.h"

static void lightning_composite_gpu_controls()
{
	using namespace xl_compat;
	namespace source = lightning_composite_source;
	// The extracted legacy renderer changes fixed-function texture enables;
	// isolate them from Redux's cached state before reusing the EGL context
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
	LightningGlowGpu compositor;
	LightningGpu primitive;
	ParticleDepth snapshot;
	GLuint scene, scene_color, scene_depth, corona, reference_targets[3], reference_colors[3];
	glGenFramebuffers(1, &scene);
	glGenTextures(1, &scene_color);
	glGenRenderbuffers(1, &scene_depth);
	glGenTextures(1, &corona);
	glGenTextures(3, reference_colors);
	glGenFramebuffers(3, reference_targets);
	glActiveTexture(GL_TEXTURE0);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	const auto allocate = [](GLuint texture, int w, int h, const void *data) {
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	};
	allocate(scene_color, 64, 64, nullptr);
	glBindFramebuffer(GL_FRAMEBUFFER, scene);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, scene_color, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, scene_depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 64, 64);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, scene_depth);
	check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Lightning composite scene incomplete");
	std::array<unsigned char, 16 * 16 * 4> art{};
	for (unsigned i = 0; i < 256; ++i) {
		art[i * 4] = static_cast<unsigned char>((i % 16) * 17);
		art[i * 4 + 1] = static_cast<unsigned char>((i / 16) * 17);
		art[i * 4 + 2] = 127;
		art[i * 4 + 3] = 191;
	}
	allocate(corona, 16, 16, art.data());
	for (unsigned i = 0; i < 3; ++i) {
		allocate(reference_colors[i], 64, 64, nullptr);
		glBindFramebuffer(GL_FRAMEBUFFER, reference_targets[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, reference_colors[i], 0);
		check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Lightning reference target incomplete");
		source::ogl.targets[i] = reference_targets[i];
		source::textures[i].texture = reference_colors[i];
	}
	source::ogl.scene = scene;
	source::gameData.renderData.screen = { 0, 0, 64, 64, 64 };
	source::CGlowRenderer reference;
	reference.shader = soft_test_program(blurVS, blurFS);
	const std::vector<LightningPoint> nodes{ { -16384, -9830, 0 }, { 0, 13107, 0 }, { 19661, -6554, 0 }, { 22000, 10000, 0 } };
	const auto core = lightning_core_geometry(nodes, [](const LightningVertex &p) { return p[2]; });
	const auto glow = lightning_glow_geometry(nodes, 1, .7f, true, { 0, 0, 3 }, 0, 9999);
	const std::array<float, 16> identity{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	unsigned cases = 0;
	for (const std::array<int, 4> view : { std::array<int, 4>{ 0, 0, 64, 64 }, std::array<int, 4>{ 3, 5, 48, 43 } })
		for (int extent_case = 0; extent_case < 5; ++extent_case)
			for (unsigned bolts : { 1u, 2u })
				for (unsigned zero_mask : { 0u, 1u, 2u, 3u })
					for (float depth_value : { .25f, .75f }) {
						if (zero_mask && (extent_case != 0 || bolts != 2)) continue;
						if (extent_case == 4 && bolts == 1) continue;
						const std::array<int, 4> extent = extent_case == 0 ? std::array<int, 4>{ 0, 0, view[2] - 1, view[3] - 1 } : extent_case != 2 ? std::array<int, 4>{ 12, 10, view[2] - 9, view[3] - 8 }
						                                                                                                                             : std::array<int, 4>{ -5, -3, view[2] + 8, view[3] + 4 };
						std::array<std::array<unsigned char, 64 * 64 * 4>, 2> pixels{};
						for (unsigned profile = 0; profile < 2; ++profile) {
							glUseProgram(0);
							glBindFramebuffer(GL_FRAMEBUFFER, scene);
							glViewport(0, 0, 64, 64);
							glDisable(GL_SCISSOR_TEST);
							glDisable(GL_STENCIL_TEST);
							glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
							glDepthMask(GL_TRUE);
							glClearDepth(depth_value);
							glClearColor(.03f, .05f, .07f, .2f);
							glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
							// Capture the full scene so offsets use the same texture coordinates
							snapshot.capture();
							glViewport(view[0], view[1], view[2], view[3]);
							if (!profile) {
								glBindFramebuffer(GL_FRAMEBUFFER, reference_targets[0]);
								glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, snapshot.texture, 0);
								glClearColor(0, 0, 0, 1);
								glClear(GL_COLOR_BUFFER_BIT);
							}
							for (unsigned bolt = 0; bolt < bolts; ++bolt) {
								const bool direct = (zero_mask & (1u << bolt)) != 0;
								auto bolt_glow = glow;
								if (direct) bolt_glow.distance_scale = 0;
								auto matrix = identity;
								matrix[12] = float(bolt) * .13f;
								matrix[13] = -float(bolt) * .09f;
								auto bolt_extent = extent;
								const int shift = extent_case == 4 ? (bolt ? 0 : 80) : int(bolt) * (extent_case == 3 ? 80 : 6);
								bolt_extent[0] += shift;
								bolt_extent[2] += shift;
								bolt_extent[1] -= int(bolt) * 5;
								bolt_extent[3] -= int(bolt) * 5;
								const std::array<float, 4> color{ .025f + float(bolt) * .01f, .07f, .11f, .2f };
								if (profile) {
									glBindFramebuffer(GL_FRAMEBUFFER, scene);
									if (direct) {
										compositor.draw_direct(core, bolt_glow, 1, color, .7f, matrix, bolt_extent);
										continue;
									}
									// A compatible Begin must retain the first bolt's accumulation
									compositor.begin(scene, snapshot.texture, GL_DEPTH_ATTACHMENT, 64, 64, view);
									check(compositor.draw(core, glow, 1, color, .7f, matrix, corona, bolt_extent) == !(extent_case == 4 && bolt == 0), "Lightning cumulative visibility differs from source");
								} else {
									glBindFramebuffer(GL_FRAMEBUFFER, direct ? scene : reference_targets[0]);
									primitive.draw(core, bolt_glow, 1, color, .7f, matrix, corona, true);
								}
							}
							if (profile) {
								check(compositor.end() == (zero_mask != 3), "Lightning batch lifetime differs for zero-scale input");
								check(!compositor.end(), "Lightning batch composed twice");
							} else if (zero_mask != 3) {
								source::gameData.renderData.scene = { view[0], view[1], view[2], view[3], 64 };
								source::CCanvas::current = &source::gameData.renderData.scene;
								reference.m_nStrength = 3;
								reference.m_bViewport = 1;
								reference.m_renderMin = { extent[0], extent[1] - int(bolts - 1) * 5 };
								reference.m_renderMax = { extent[2] + (extent_case == 4 ? 80 : int(bolts - 1) * (extent_case == 3 ? 80 : 6)), extent[3] };
								if (zero_mask == 1) {
									reference.m_renderMin.x += 6;
									reference.m_renderMax.y -= 5;
								}
								glBlendEquation(GL_FUNC_ADD);
								glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
								check(reference.End(), "Pinned lightning composition failed");
							}
							glBindFramebuffer(GL_FRAMEBUFFER, scene);
							glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels[profile].data());
							std::array<float, 64 * 64> observed_depth{};
							glReadPixels(0, 0, 64, 64, GL_DEPTH_COMPONENT, GL_FLOAT, observed_depth.data());
							for (float value : observed_depth) check(std::abs(value - depth_value) < .00001f, "Lightning compositor changed scene depth");
						}
						int worst = 0;
						unsigned index = 0;
						unsigned changed = 0;
						const std::array<unsigned char, 3> background{ 8, 13, 18 };
						for (unsigned p = 0; p < 64 * 64; ++p)
							for (unsigned c = 0; c < 3; ++c) changed += pixels[0][p * 4 + c] != background[c];
						check(depth_value > .5f ? changed > 0 : changed == 0, "Lightning composition failed to distinguish visible and occluded input");
						for (unsigned i = 0; i < pixels[0].size(); ++i) {
							const int error = std::abs(int(pixels[0][i]) - int(pixels[1][i]));
							if (error > worst) {
								worst = error;
								index = i;
							}
						}
						if (worst > 1) throw std::runtime_error("Lightning composition differs from pinned End/Render case=" + std::to_string(cases) + " index=" + std::to_string(index) + " expected=" + std::to_string(pixels[0][index]) + " actual=" + std::to_string(pixels[1][index]));
						++cases;
					}
	// Verify each public operation restores a deliberately different caller state
	GLuint sentinel_vao, sentinel_buffer;
	glGenVertexArrays(1, &sentinel_vao);
	glGenBuffers(1, &sentinel_buffer);
	glBindVertexArray(sentinel_vao);
	glBindBuffer(GL_ARRAY_BUFFER, sentinel_buffer);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sentinel_buffer);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, reference_targets[2]);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, scene);
	glViewport(7, 9, 23, 27);
	glEnable(GL_SCISSOR_TEST);
	glScissor(1, 2, 3, 4);
	glEnable(GL_STENCIL_TEST);
	glEnable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_GREATER);
	glDisable(GL_BLEND);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_DST_ALPHA, GL_ONE, GL_ZERO);
	glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_MAX);
	glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
	glClearColor(.13f, .27f, .39f, .51f);
	glLineWidth(2);
	glUseProgram(reference.shader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, reference_colors[0]);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, corona);
	const auto state = [] {
		std::vector<double> result;
		for (GLenum name : { GL_CURRENT_PROGRAM, GL_VERTEX_ARRAY_BINDING, GL_ARRAY_BUFFER_BINDING, GL_PIXEL_UNPACK_BUFFER_BINDING, GL_READ_FRAMEBUFFER_BINDING, GL_DRAW_FRAMEBUFFER_BINDING, GL_ACTIVE_TEXTURE, GL_BLEND_SRC_RGB, GL_BLEND_DST_RGB, GL_BLEND_SRC_ALPHA, GL_BLEND_DST_ALPHA, GL_BLEND_EQUATION_RGB, GL_BLEND_EQUATION_ALPHA, GL_DEPTH_FUNC }) {
			GLint value;
			glGetIntegerv(name, &value);
			result.push_back(value);
		}
		for (GLenum name : { GL_SCISSOR_TEST, GL_STENCIL_TEST, GL_CULL_FACE, GL_DEPTH_TEST, GL_BLEND, GL_ALPHA_TEST, GL_LINE_SMOOTH }) result.push_back(glIsEnabled(name));
		GLboolean write, mask[4];
		glGetBooleanv(GL_DEPTH_WRITEMASK, &write);
		result.push_back(write);
		glGetBooleanv(GL_COLOR_WRITEMASK, mask);
		for (auto value : mask) result.push_back(value);
		GLint box[4];
		glGetIntegerv(GL_VIEWPORT, box);
		for (auto value : box) result.push_back(value);
		glGetIntegerv(GL_SCISSOR_BOX, box);
		for (auto value : box) result.push_back(value);
		GLfloat clear[4], line;
		glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
		for (auto value : clear) result.push_back(value);
		glGetFloatv(GL_LINE_WIDTH, &line);
		result.push_back(line);
		GLint active;
		glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
		for (GLenum unit : { GL_TEXTURE0, GL_TEXTURE3 }) {
			glActiveTexture(unit);
			GLint texture;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
			result.push_back(texture);
		}
		glActiveTexture(active);
		return result;
	};
	const auto before = state();
	const std::array<int, 4> full_view{ 0, 0, 64, 64 };
	compositor.begin(scene, snapshot.texture, GL_DEPTH_ATTACHMENT, 64, 64, full_view);
	check(state() == before, "Lightning Begin leaked caller state");
	check(!compositor.draw(core, glow, 1, { .025f, .07f, .11f, .2f }, .7f, identity, corona, { -30, -30, -10, -10 }), "Offscreen lightning was accepted");
	check(state() == before && !compositor.end(), "Empty lightning batch changed caller state or composed");
	check(state() == before, "Empty lightning End leaked caller state");
	compositor.begin(scene, snapshot.texture, GL_DEPTH_ATTACHMENT, 64, 64, full_view);
	check(compositor.draw(core, glow, 1, { .025f, .07f, .11f, .2f }, .7f, identity, corona, { 12, 10, 55, 56 }), "State control lightning rejected");
	check(state() == before, "Lightning Draw leaked caller state");
	check(compositor.end() && state() == before, "Lightning End leaked caller state");
	glUseProgram(0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	glDeleteVertexArrays(1, &sentinel_vao);
	glDeleteBuffers(1, &sentinel_buffer);
	compositor.discard(false);
	compositor.discard(false);
	primitive.discard(false);
	snapshot.discard(false);
	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteProgram(reference.shader);
	glDeleteFramebuffers(1, &scene);
	glDeleteFramebuffers(3, reference_targets);
	glDeleteTextures(1, &scene_color);
	glDeleteTextures(1, &corona);
	glDeleteTextures(3, reference_colors);
	glDeleteRenderbuffers(1, &scene_depth);
	glPopClientAttrib();
	glPopAttrib();
	check(glGetError() == GL_NO_ERROR, "Lightning composition control raised GL error");
	std::cout << "Lightning compositor GPU: " << cases << " pinned End/Render batch, viewport and depth comparisons passed\n";
}
