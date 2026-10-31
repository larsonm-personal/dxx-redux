#include "xl_reference_particle_soft.h"
#include "../particle_soft_shader.h"
#include "../runtime/particle_depth.h"

static void particle_depth_controls()
{
	ParticleDepth snapshot;
	GLuint scene, color, depth, unpack;
	glGenFramebuffers(1, &scene);
	glGenRenderbuffers(1, &color);
	glGenRenderbuffers(1, &depth);
	glGenBuffers(1, &unpack);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, 16, nullptr, GL_STATIC_DRAW);
	GLint maximum;
	glGetIntegerv(GL_MAX_SAMPLES, &maximum);
	unsigned cases = 0;
	for (GLenum format : { GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT32F, GL_DEPTH24_STENCIL8 })
		for (int samples : { 0, 4 }) {
			if (samples > maximum) continue;
			glBindFramebuffer(GL_FRAMEBUFFER, scene);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
			glBindRenderbuffer(GL_RENDERBUFFER, color);
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, 11, 13);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
			glBindRenderbuffer(GL_RENDERBUFFER, depth);
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, format, 11, 13);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, format == GL_DEPTH24_STENCIL8 ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
			check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Particle depth test target incomplete");
			glViewport(3, 5, 8, 8);
			glDisable(GL_SCISSOR_TEST);
			glDepthMask(GL_TRUE);
			glClearDepth(.625);
			glClear(GL_DEPTH_BUFFER_BIT);
			glScissor(0, 0, 1, 1);
			glEnable(GL_SCISSOR_TEST);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			snapshot.capture();
			GLint read, draw, bound_unpack;
			glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
			glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &bound_unpack);
			check(read == 0 && GLuint(draw) == scene && GLuint(bound_unpack) == unpack && glIsEnabled(GL_SCISSOR_TEST), "Particle depth capture leaked caller state");
			glBindFramebuffer(GL_READ_FRAMEBUFFER, snapshot.framebuffer);
			std::array<float, 64> observed{};
			glReadPixels(3, 5, 8, 8, GL_DEPTH_COMPONENT, GL_FLOAT, observed.data());
			for (float value : observed) check(std::abs(value - .625f) < .0001f, "Particle depth snapshot changed depth values");
			check(glGetError() == GL_NO_ERROR, "Particle depth snapshot control raised GL error");
			++cases;
		}
	snapshot.discard(false);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	glDeleteFramebuffers(1, &scene);
	glDeleteRenderbuffers(1, &color);
	glDeleteRenderbuffers(1, &depth);
	glDeleteBuffers(1, &unpack);
	glDisable(GL_SCISSOR_TEST);
	std::cout << "Particle depth snapshots: " << cases << " depth format/MSAA cases, viewport offsets and restored read/draw/unpack/scissor state passed\n";
}

static GLuint soft_test_program(const char *vertex, const char *fragment)
{
	const auto compile = [](GLenum type, const char *code) {
		const GLuint result = glCreateShader(type);
		glShaderSource(result, 1, &code, nullptr);
		glCompileShader(result);
		GLint ok = 0;
		glGetShaderiv(result, GL_COMPILE_STATUS, &ok);
		check(ok != 0, "Soft particle shader failed to compile");
		return result;
	};
	const GLuint v = compile(GL_VERTEX_SHADER, vertex), f = compile(GL_FRAGMENT_SHADER, fragment);
	const GLuint result = glCreateProgram();
	glAttachShader(result, v);
	glAttachShader(result, f);
	glLinkProgram(result);
	GLint ok = 0;
	glGetProgramiv(result, GL_LINK_STATUS, &ok);
	glDeleteShader(v);
	glDeleteShader(f);
	check(ok != 0, "Soft particle shader failed to link");
	return result;
}

static void particle_soft_shader_controls()
{
	particle_depth_controls();
	const char *portable_vertex = "varying vec2 particleUV; varying vec4 particleColor; void main() { particleUV = gl_MultiTexCoord0.xy; particleColor = gl_Color; gl_Position = ftransform(); }";
	const GLuint programs[2] = { soft_test_program(glareVS, glareFS), soft_test_program(portable_vertex, xl_particle_soft_glsl) };
	GLuint textures[3], target, scene_depth_buffer;
	glGenTextures(3, textures);
	glGenFramebuffers(1, &target);
	glBindFramebuffer(GL_FRAMEBUFFER, target);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0], 0);
	glGenRenderbuffers(1, &scene_depth_buffer);
	glBindRenderbuffer(GL_RENDERBUFFER, scene_depth_buffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, 8, 8);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, scene_depth_buffer);
	check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Soft particle control target incomplete");
	const unsigned char color[4] = { 93, 151, 211, 73 };
	xl_compat::PogBitmap bitmap;
	bitmap.width = bitmap.height = 1;
	bitmap.truecolor = true;
	bitmap.pixels.assign(color, color + 4);
	xl_compat::ParticleTexture production(bitmap, "soft-particle-source-control");
	glBindTexture(GL_TEXTURE_2D, textures[1]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, color);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, textures[2]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glViewport(0, 0, 8, 8);
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
	const auto depth = [](float eye) { return (5000.f / 4999.f) * (1.f - 1.f / eye); };
	unsigned comparisons = 0, behind_visible = 0;
	for (float scene : { 1.f, 2.f, 10.f, 100.f, 1000.f, 4990.f })
		for (float gap : { -10.f, -1.f, 0.f, .5f, 2.5f, 4.9f, 5.f, 10.f }) {
			const float particle = scene + gap;
			if (particle < 1 || particle >= 5000) continue;
			const float scene_depth = depth(scene);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 1, 1, 0, GL_RED, GL_FLOAT, &scene_depth);
			for (int blend : { 2, -1, 0 }) {
				std::array<std::array<unsigned char, 8 * 8 * 4>, 3> pixels{};
				for (unsigned profile = 0; profile < 3; ++profile) {
					glClearDepth(scene_depth);
					glDepthMask(GL_TRUE);
					glClearColor(.2f, .4f, .6f, 1);
					glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
					if (profile == 2) {
						glUseProgram(0);
						glEnable(GL_DEPTH_TEST);
						glEnable(GL_CULL_FACE);
						glDisable(GL_BLEND);
						glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
						glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
						xl_compat::ParticleQuad quad;
						quad.blend = blend > 0 ? xl_compat::ParticleBlend::weak_additive : blend < 0 ? xl_compat::ParticleBlend::multiply
						                                                                             : xl_compat::ParticleBlend::alpha;
						const std::array<std::array<float, 2>, 4> corners{ { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } } };
						for (unsigned i = 0; i < 4; ++i) {
							quad.vertices[i].position = { corners[i][0], corners[i][1], depth(particle) * 2 - 1 };
							quad.vertices[i].uv = { .5f, .5f };
							quad.vertices[i].color = { .7f, .8f, .9f, .3f };
						}
						xl_particle_soft_begin_scene();
						production.draw(&quad, 1);
						xl_particle_soft_end_scene();
						GLint current, equation;
						GLboolean writes;
						glGetIntegerv(GL_CURRENT_PROGRAM, &current);
						glGetIntegerv(GL_BLEND_EQUATION_RGB, &equation);
						glGetBooleanv(GL_DEPTH_WRITEMASK, &writes);
						check(current == 0 && equation == GL_FUNC_REVERSE_SUBTRACT && writes && glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_CULL_FACE) && !glIsEnabled(GL_BLEND), "Soft particle backend leaked GL state");
						glReadPixels(0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, pixels[profile].data());
						float retained_depth;
						glReadPixels(4, 4, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &retained_depth);
						check(std::abs(retained_depth - scene_depth) < .000001f, "Soft particle backend wrote scene depth");
						glDisable(GL_DEPTH_TEST);
						glDisable(GL_CULL_FACE);
						glEnable(GL_BLEND);
						glBlendEquation(GL_FUNC_ADD);
						continue;
					}
					const GLuint program = programs[profile];
					glUseProgram(program);
					glUniform1i(glGetUniformLocation(program, profile ? "particleTex" : "glareTex"), 0);
					glUniform1i(glGetUniformLocation(program, "depthTex"), 1);
					glUniform2f(glGetUniformLocation(program, "windowScale"), 1.f / 8, 1.f / 8);
					glUniform1i(glGetUniformLocation(program, "blendMode"), blend);
					if (!profile) {
						glUniform1f(glGetUniformLocation(program, "dMax"), 5);
						glUniform1i(glGetUniformLocation(program, "bSuspended"), 0);
					}
					const auto factors = source_particle_blend(blend > 0 ? 5 : blend < 0 ? PARTICLE_TYPES + 5
					                                                                     : 1,
					                                           blend == 0 ? 0 : 2);
					glBlendFunc(factors[0], factors[1]);
					glClearColor(.2f, .4f, .6f, 1);
					glClear(GL_COLOR_BUFFER_BIT);
					glColor4f(.7f, .8f, .9f, .3f);
					glBegin(GL_QUADS);
					glTexCoord2f(.5f, .5f);
					for (const auto &corner : std::array<std::array<float, 2>, 4>{ { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } } })
						glVertex3f(corner[0], corner[1], depth(particle) * 2 - 1);
					glEnd();
					glReadPixels(0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, pixels[profile].data());
				}
				for (unsigned i = 0; i < pixels[0].size(); ++i)
					check(std::abs(int(pixels[0][i]) - int(pixels[1][i])) <= 1 && std::abs(int(pixels[0][i]) - int(pixels[2][i])) <= 1, "Soft particle shader/backend differs from pinned source GPU output");
				if (gap > 0 && gap < 4 && pixels[0][0] != 51) ++behind_visible;
				check(glGetError() == GL_NO_ERROR, "Soft particle shader comparison raised GL error");
				++comparisons;
			}
		}
	check(behind_visible > 0, "Source soft particle control failed to distinguish hard depth rejection");
	glUseProgram(0);
	glDeleteProgram(programs[0]);
	glDeleteProgram(programs[1]);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &target);
	glDeleteRenderbuffers(1, &scene_depth_buffer);
	glDeleteTextures(3, textures);
	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_BLEND);
	glColor4f(1, 1, 1, 1);
	std::cout << "Soft particle source GPU: " << comparisons << " depth/blend comparisons, " << behind_visible << " distinguish hard depth rejection\n";
}
