#include "xl_reference_particle_multi.h"
#include "../particle_multi_shader.h"

static void particle_multi_shader_controls()
{
	const char *vertex = "varying vec3 particleUV; varying vec4 particleColor; void main() { particleUV = gl_MultiTexCoord0.xyz; particleColor = gl_Color; gl_Position = ftransform(); }";
	const GLuint portable = soft_test_program(vertex, xl_particle_multi_glsl);
	const GLuint source[2] = { soft_test_program(particleVS, particleFS[0]), soft_test_program(particleVS, particleFS[2]) };
	GLuint textures[5], target;
	glGenTextures(5, textures);
	glGenFramebuffers(1, &target);
	glBindFramebuffer(GL_FRAMEBUFFER, target);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textures[4]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[4], 0);
	check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Multi-image particle target incomplete");
	const unsigned char colors[3][4] = { { 93, 151, 211, 73 }, { 217, 51, 109, 131 }, { 37, 229, 81, 193 } };
	for (unsigned i = 0; i < 4; ++i) {
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, textures[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		if (i < 3) glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors[i]);
	}
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glViewport(0, 0, 8, 8);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	ParticleBlendSource blend;
	blend.SetBlendMode(OGL_BLEND_ALPHA_CONTROLLED);
	check(blend.factors == std::array<GLenum, 2>{ GL_ONE, GL_SRC_ALPHA }, "Source alpha-controlled factors changed");
	glBlendFunc(blend.factors[0], blend.factors[1]);
	const auto depth = [](float eye) { return (5000.f / 4999.f) * (1.f - 1.f / eye); };
	unsigned comparisons = 0, visible = 0;
	for (int soft : { 0, 1 })
		for (int kind : { 0, 1, 2 })
			for (float alpha : { 0.f, .3f, .8f, 1.f })
				for (float scene : { 1.f, 2.f, 10.f, 100.f, 1000.f, 4990.f })
					for (float gap : { -10.f, -1.f, 0.f, .5f, 2.5f, 3.f, 4.9f, 5.f, 10.f }) {
						const float particle = scene + gap;
						if (particle < 1 || particle >= 5000) continue;
						const float scene_depth = depth(scene);
						glActiveTexture(GL_TEXTURE3);
						glBindTexture(GL_TEXTURE_2D, textures[3]);
						glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 1, 1, 0, GL_RED, GL_FLOAT, &scene_depth);
						std::array<std::array<unsigned char, 256>, 2> pixels{};
						for (unsigned implementation = 0; implementation < 2; ++implementation) {
							const GLuint program = implementation ? portable : source[soft];
							glUseProgram(program);
							glUniform1i(glGetUniformLocation(program, "particleTex"), 0);
							glUniform1i(glGetUniformLocation(program, "sparkTex"), 1);
							glUniform1i(glGetUniformLocation(program, "bubbleTex"), 2);
							glUniform1i(glGetUniformLocation(program, "depthTex"), 3);
							glUniform1i(glGetUniformLocation(program, "softDepth"), soft);
							glUniform1i(glGetUniformLocation(program, "bSuspended"), 0);
							glUniform2f(glGetUniformLocation(program, "windowScale"), 1.f / 8, 1.f / 8);
							glUniform3f(glGetUniformLocation(program, "dMax"), 5, 3, 5);
							glClearColor(.2f, .4f, .6f, 1);
							glClear(GL_COLOR_BUFFER_BIT);
							glColor4f(.7f, .8f, .9f, alpha);
							glBegin(GL_QUADS);
							glTexCoord3f(.5f, .5f, float(kind));
							for (const auto &corner : std::array<std::array<float, 2>, 4>{ { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } } })
								glVertex3f(corner[0], corner[1], depth(particle) * 2 - 1);
							glEnd();
							glReadPixels(0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, pixels[implementation].data());
						}
						for (unsigned i = 0; i < pixels[0].size(); ++i)
							check(std::abs(int(pixels[0][i]) - int(pixels[1][i])) <= 1, "Multi-image particle shader differs from pinned source");
						visible += pixels[0][0] != 51 || pixels[0][1] != 102 || pixels[0][2] != 153;
						check(glGetError() == GL_NO_ERROR, "Multi-image particle comparison raised GL error");
						++comparisons;
					}
	check(visible > 0, "Multi-image particle controls never changed framebuffer pixels");
	glUseProgram(0);
	glDeleteProgram(portable);
	for (auto program : source) glDeleteProgram(program);
	glDeleteTextures(5, textures);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &target);
	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_BLEND);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	std::cout << "Pinned multi-image particle shader: " << comparisons << " framebuffer comparisons, " << visible << " visible witnesses; smoke/spark/bubble selection, alpha-controlled blending and per-type soft depth passed\n";
}
