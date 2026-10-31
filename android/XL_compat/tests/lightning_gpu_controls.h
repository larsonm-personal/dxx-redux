#include "../lightning_blur_shader.h"
#include "xl_reference_lightning_blur.h"
#include "lightning_glow_gpu_controls.h"

static void lightning_primitive_gpu_controls()
{
	using namespace xl_compat;
	LightningGpu renderer;
	GLuint textures[2], framebuffer, depth;
	glGenTextures(2, textures);
	glGenFramebuffers(1, &framebuffer);
	glGenRenderbuffers(1, &depth);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0], 0);
	glBindRenderbuffer(GL_RENDERBUFFER, depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 64, 64);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
	check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Lightning primitive target incomplete");
	std::array<unsigned char, 16 * 16 * 4> artwork{};
	for (unsigned i = 0; i < 16 * 16; ++i) {
		artwork[i * 4] = static_cast<unsigned char>((i % 16) * 17);
		artwork[i * 4 + 1] = static_cast<unsigned char>((i / 16) * 17);
		artwork[i * 4 + 2] = 127;
		artwork[i * 4 + 3] = 191;
	}
	glBindTexture(GL_TEXTURE_2D, textures[1]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, artwork.data());
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	const auto padded_corona = copy_particle_border_texture(textures[1], 16);
	glViewport(0, 0, 64, 64);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	const std::array<float, 16> identity{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	const std::vector<LightningPoint> nodes{ { -32768, -16384, 0 }, { 0, 19661, 0 }, { 32768, -6554, 0 }, { 40000, 10000, 0 } };
	const auto original_core = lightning_core_geometry(nodes, [](const LightningVertex &p) { return p[2]; });
	const std::array<float, 4> color{ .12f, .37f, .53f, .41f };
	unsigned cases = 0, mismatches = 0;
	int maximum_error = 0;
	std::string first_failure;
	std::array<std::array<float, 16>, 8> cameras;
	cameras.fill(identity);
	cameras[1][12] = .7f;
	cameras[2][12] = -.7f;
	cameras[3][13] = .8f;
	cameras[4][0] = cameras[4][5] = 2.5f;
	cameras[5][3] = .3f;
	cameras[5][7] = .2f;
	cameras[6][12] = .6f;
	cameras[6][7] = -.4f;
	cameras[7][0] = cameras[7][5] = .125f;
	for (const auto &camera : cameras)
		for (float width : { .2f, 1.0f, 3.0f })
			for (bool blur : { false, true })
				for (float scene_depth : { .25f, .75f })
					for (unsigned sampling = 0; sampling < 3; ++sampling)
						for (unsigned isolated = 0; isolated < 4; ++isolated) {
							auto core = original_core;
							auto glow = lightning_glow_geometry(nodes, 1, width, true, { 0, 0, 3 }, core.average_depth, 9999);
							if (isolated == 3) glow.distance_scale = 0;
							if (isolated == 1 || isolated == 2) {
								for (auto &vertex : core.vertices) vertex = { 100, 0, 0 };
								const auto layer = glow.vertices.size() / 2;
								for (std::size_t i = (isolated - 1) * layer; i < isolated * layer; ++i) glow.vertices[i] = { 100, 0, 0 };
							}
							std::array<std::array<unsigned char, 64 * 64 * 4>, 2> pixels{};
							for (unsigned profile = 0; profile < 2; ++profile) {
								glActiveTexture(GL_TEXTURE0);
								glBindTexture(GL_TEXTURE_2D, profile ? padded_corona : textures[1]);
								glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sampling == 2 ? GL_LINEAR_MIPMAP_LINEAR : sampling == 1 ? GL_LINEAR_MIPMAP_NEAREST
								                                                                                                              : GL_LINEAR);
								glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, profile ? GL_CLAMP_TO_EDGE : GL_CLAMP);
								glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, profile ? GL_CLAMP_TO_EDGE : GL_CLAMP);
								glUseProgram(0);
								glMatrixMode(GL_PROJECTION);
								glLoadMatrixf(camera.data());
								glMatrixMode(GL_MODELVIEW);
								glDisable(GL_SCISSOR_TEST);
								glDepthMask(GL_TRUE);
								glClearDepth(scene_depth);
								glClearColor(.03f, .05f, .07f, .2f);
								glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
								glEnable(GL_DEPTH_TEST);
								glDepthFunc(GL_LEQUAL);
								glDepthMask(GL_FALSE);
								glDisable(GL_CULL_FACE);
								glDisable(GL_ALPHA_TEST);
								glEnable(GL_BLEND);
								glBlendFunc(GL_ONE, GL_ONE);
								glBlendEquation(blur ? GL_MAX : GL_FUNC_ADD);
								glDisable(GL_LINE_SMOOTH);
								glLineWidth(1);
								if (profile) {
									renderer.draw(core, glow, 1, color, width, camera, padded_corona, blur, { .25f, .25f });
									GLint program, equation;
									GLfloat line;
									glGetIntegerv(GL_CURRENT_PROGRAM, &program);
									glGetIntegerv(GL_BLEND_EQUATION_RGB, &equation);
									glGetFloatv(GL_LINE_WIDTH, &line);
									check(!program && equation == (blur ? GL_MAX : GL_FUNC_ADD) && line == 1 && !glIsEnabled(GL_LINE_SMOOTH), "Lightning draw leaked program, blend or line state");
								} else {
									glEnable(GL_TEXTURE_2D);
									glBindTexture(GL_TEXTURE_2D, textures[1]);
									glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
									glColor3f(color[0] / 2, color[1] / 2, color[2] / 2);
									const auto layer = glow.vertices.size() / 2;
									glBindVertexArray(0);
									glBindBuffer(GL_ARRAY_BUFFER, 0);
									glEnableClientState(GL_VERTEX_ARRAY);
									glEnableClientState(GL_TEXTURE_COORD_ARRAY);
									glVertexPointer(3, GL_FLOAT, sizeof(LightningVertex), glow.vertices.data());
									glTexCoordPointer(2, GL_FLOAT, sizeof(std::array<float, 2>), glow.uv.data());
									for (int pass = glow.distance_scale > 0 ? 1 : -1; pass >= 0; --pass) {
										glDrawArrays(GL_QUADS, GLint(pass * layer), GLsizei(layer));
									}
									glDisableClientState(GL_TEXTURE_COORD_ARRAY);
									glDisable(GL_TEXTURE_2D);
									glColor4fv(color.data());
									const float line = blur && glow.distance_scale != 0 ? width * (2 * glow.distance_scale) : width;
									glLineWidth(line > 1 ? line : 1);
									glEnable(GL_LINE_SMOOTH);
									glVertexPointer(3, GL_FLOAT, sizeof(LightningVertex), core.vertices.data());
									glDrawArrays(GL_LINE_STRIP, 0, GLsizei(nodes.size() - 1));
									glDisableClientState(GL_VERTEX_ARRAY);
								}
								glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels[profile].data());
								std::array<float, 64 * 64> observed_depth{};
								glReadPixels(0, 0, 64, 64, GL_DEPTH_COMPONENT, GL_FLOAT, observed_depth.data());
								for (float value : observed_depth) check(std::abs(value - scene_depth) < .00001f, "Lightning modified scene depth");
							}
							unsigned worst = 0, differences = 0;
							int error = 0;
							for (unsigned i = 0; i < pixels[0].size(); ++i) {
								const int delta = std::abs(int(pixels[0][i]) - int(pixels[1][i]));
								if (delta > 1) ++differences;
								if (delta > error) {
									error = delta;
									worst = i;
								}
							}
							maximum_error = (std::max) (maximum_error, error);
							// Each layer may round by one unit; GL_MAX preserves that bound, addition sums it
							if (error > (isolated || blur ? 1 : 2)) {
								++mismatches;
								if (first_failure.empty()) first_failure = ("Lightning pixel mismatch camera=" + std::to_string(&camera - cameras.data()) + " isolated=" + std::to_string(isolated) + " sampling=" + std::to_string(sampling) + " width=" + std::to_string(width) + " blur=" + std::to_string(blur) + " depth=" + std::to_string(scene_depth) + " index=" + std::to_string(worst) + " expected=" + std::to_string(pixels[0][worst]) + " actual=" + std::to_string(pixels[1][worst]) + " differences=" + std::to_string(differences));
							}
							++cases;
						}
	if (mismatches) throw std::runtime_error(first_failure + " maximum=" + std::to_string(maximum_error) + " cases=" + std::to_string(mismatches));
	check(glGetError() == GL_NO_ERROR, "Lightning primitive test raised GL error");
	renderer.discard(false);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &framebuffer);
	glDeleteRenderbuffers(1, &depth);
	glDeleteTextures(2, textures);
	glDeleteTextures(1, &padded_corona);
	glDisable(GL_LINE_SMOOTH);
	glLineWidth(1);
	std::cout << "Lightning production GPU: " << cases << " core/glow blend/depth comparisons passed; isolated layers and blurred batches <=1 RGBA8 unit, additive combined maximum " << maximum_error << '\n';
}

static void lightning_blur_shader_controls()
{
	lightning_primitive_gpu_controls();
	lightning_composite_gpu_controls();
	const char *vertex = "varying vec2 glowUV; void main() { glowUV=gl_MultiTexCoord0.xy; gl_Position=ftransform(); }";
	const GLuint programs[2] = { soft_test_program(blurVS, blurFS), soft_test_program(vertex, xl_lightning_blur_glsl) };
	GLuint textures[3], framebuffer;
	glGenTextures(3, textures);
	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glActiveTexture(GL_TEXTURE0);
	constexpr int width = 17, height = 13;
	std::array<unsigned char, width * height * 4> input{};
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x) {
			const int i = (y * width + x) * 4;
			input[i] = static_cast<unsigned char>((x * 17 + y * 3) % 93);
			input[i + 1] = static_cast<unsigned char>((x * 7 + y * 11) % 61);
			input[i + 2] = static_cast<unsigned char>((x == 8 && y == 6) ? 255 : 0);
			input[i + 3] = static_cast<unsigned char>((x * 13 + y * 17) % 256);
		}
	for (unsigned i = 0; i < 3; ++i) {
		glBindTexture(GL_TEXTURE_2D, textures[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, i ? nullptr : input.data());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	unsigned cases = 0, mismatches = 0;
	int maximum_error = 0;
	std::string first_failure;
	for (float brightness : { 0.5f, 1.0f, 1.05f }) {
		std::array<std::array<std::array<unsigned char, width * height * 4>, 6>, 2> pixels{};
		for (unsigned profile = 0; profile < 2; ++profile) {
			glUseProgram(programs[profile]);
			glUniform1i(glGetUniformLocation(programs[profile], "glowSource"), 0);
			glUniform1f(glGetUniformLocation(programs[profile], "brightness"), brightness);
			for (int pass = 0; pass < 6; ++pass) {
				const int destination = 1 + pass % 2;
				const int source = pass ? 1 + (pass - 1) % 2 : 0;
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[destination], 0);
				check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Lightning blur target incomplete");
				if (pass < 2) {
					glClearColor(0, 0, 0, 0);
					glClear(GL_COLOR_BUFFER_BIT);
				}
				glBlendFunc(GL_ONE, pass < 2 ? GL_ZERO : GL_ONE);
				glBindTexture(GL_TEXTURE_2D, textures[source]);
				glUniform1f(glGetUniformLocation(programs[profile], "direction"), float(pass % 2));
				glUniform1f(glGetUniformLocation(programs[profile], "scale"), 1.0f / (pass % 2 ? height : width));
				glBegin(GL_QUADS);
				glTexCoord2f(0, 0);
				glVertex2f(-1, -1);
				glTexCoord2f(1, 0);
				glVertex2f(1, -1);
				glTexCoord2f(1, 1);
				glVertex2f(1, 1);
				glTexCoord2f(0, 1);
				glVertex2f(-1, 1);
				glEnd();
				glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels[profile][pass].data());
			}
		}
		check(pixels[0] == pixels[1], "Portable lightning blur differs from pinned shader pixels");
		check(pixels[1][5][(6 * width + 7) * 4 + 2] > 0, "Lightning blur did not spread the source impulse");
		++cases;
	}
	check(glGetError() == GL_NO_ERROR, "Lightning blur comparison raised GL error");
	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &framebuffer);
	glDeleteTextures(3, textures);
	for (auto program : programs) glDeleteProgram(program);
	std::cout << "Lightning blur GPU: " << cases << " six-pass source/portable pixel comparisons passed\n";
}
