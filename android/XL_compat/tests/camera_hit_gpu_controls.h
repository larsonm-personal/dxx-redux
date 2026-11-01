#include "xl_reference_camera_hit_shader.h"
#include "../camera_hit_shader.h"

static void camera_hit_shader_controls()
{
	const char *vertex = "varying vec3 hitVertex; varying vec2 hitUV; varying vec4 hitColor; void main() { hitVertex=vec3(gl_Vertex); hitUV=gl_MultiTexCoord0.xy; hitColor=gl_Color; gl_Position=ftransform(); }";
	const GLuint programs[2] = { soft_test_program(pszSphereEffectVS[0], pszSphereEffectFS[0]), soft_test_program(vertex, xl_camera_hit_glsl) };
	GLuint textures[2], target;
	glGenTextures(2, textures);
	glGenFramebuffers(1, &target);
	glBindFramebuffer(GL_FRAMEBUFFER, target);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textures[1]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[1], 0);
	check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Hit shader framebuffer incomplete");
	std::array<unsigned char, 64> texels{};
	for (unsigned i = 0; i < 16; ++i) {
		texels[4 * i] = static_cast<unsigned char>(17 + i * 13);
		texels[4 * i + 1] = static_cast<unsigned char>(231 - i * 11);
		texels[4 * i + 2] = static_cast<unsigned char>(31 + i * 7);
		texels[4 * i + 3] = static_cast<unsigned char>((i % 4) * 85);
	}
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glViewport(0, 0, 16, 16);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_FRAMEBUFFER_SRGB);
	glShadeModel(GL_SMOOTH);
	glBlendEquation(GL_FUNC_ADD);
	const std::array<xl_compat::HitRenderMatrix, 2> matrices{ { { { { 65536, 0, 0 }, { 0, 65536, 0 }, { 0, 0, 65536 } } },
		                                                        { { { 65530, 19, -721 }, { -17, 65500, 1201 }, { 737, -1199, 65519 } } } } };
	unsigned comparisons = 0, visible = 0, inactive = 0;
	std::array<unsigned, 3> slot_witness{};
	for (int profile = 0; profile < 8; ++profile)
		for (int size : { 65536, 297534 })
			for (unsigned orientation = 0; orientation < matrices.size(); ++orientation)
				for (int age : { 0, 125, 499, 500, 1999, 2000 }) {
					xl_compat::CameraHitState state;
					const std::array<xl_compat::FixedLightPoint, 3> directions{ { { 0, 0, 65536 }, { -65536, 0, 0 }, { 65536, 0, 0 } } };
					if (profile >= 1 && profile <= 3) {
						state.times[profile - 1] = 4000;
						state.points[profile - 1] = directions[profile - 1];
					}
					if (profile == 4 || profile == 5)
						for (unsigned i = 0; i < 3; ++i) {
							state.times[i] = 4000;
							state.points[i] = directions[profile == 4 ? 0 : i];
						}
					if (profile == 6) {
						state.times = { 4001, 3990, 0 };
						state.points = { directions[0], directions[1], directions[2] };
					}
					if (profile == 7)
						for (unsigned i = 0; i < 3; ++i) state.register_hit(directions[i], {}, size, 4, 3990 + int(i), 2000);
					const auto uniforms = xl_compat::camera_hit_hardware(state, size, 4000 + age, 2000, matrices[orientation]);
					for (int blend = 0; blend < 3; ++blend)
						for (float depth : { 0.f, .8f })
							for (float alpha : { 0.f, .6f, 1.f }) {
								if (blend) {
									glEnable(GL_BLEND);
									ParticleBlendSource source_blend;
									source_blend.SetBlendMode(blend == 1 ? OGL_BLEND_ALPHA : OGL_BLEND_ADD);
									glBlendFunc(source_blend.factors[0], source_blend.factors[1]);
								} else glDisable(GL_BLEND);
								glLoadIdentity();
								if (orientation) {
									glTranslatef(.1f, -.1f, 0);
									glRotatef(17, 0, 0, 1);
									glScalef(.75f, .75f, 1);
								}
								std::array<std::array<unsigned char, 1024>, 2> pixels{};
								for (unsigned implementation = 0; implementation < 2; ++implementation) {
									const auto program = programs[implementation];
									glUseProgram(program);
									glUniform1i(glGetUniformLocation(program, "sphereTex"), 0);
									glUniform4fv(glGetUniformLocation(program, "vHit"), 3, uniforms.vectors[0].data());
									glUniform3fv(glGetUniformLocation(program, "fRad"), 1, uniforms.envelope.inverse_radius.data());
									glClearColor(.2f, .4f, .6f, 1);
									glClear(GL_COLOR_BUFFER_BIT);
									glBegin(GL_QUADS);
									glColor4f(.7f, .8f, .9f, alpha);
									glTexCoord2f(0, 0);
									glVertex3f(-1, -1, depth);
									glColor4f(.9f, .4f, .6f, alpha);
									glTexCoord2f(1, 0);
									glVertex3f(1, -1, depth);
									glColor4f(.3f, .7f, .5f, alpha);
									glTexCoord2f(1, 1);
									glVertex3f(1, 1, depth);
									glColor4f(.6f, .2f, .8f, alpha);
									glTexCoord2f(0, 1);
									glVertex3f(-1, 1, depth);
									glEnd();
									glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, pixels[implementation].data());
								}
								bool changed = false;
								const std::array<unsigned char, 4> background{ 51, 102, 153, 255 };
								for (unsigned i = 0; i < pixels[0].size(); ++i) {
									check(std::abs(int(pixels[0][i]) - int(pixels[1][i])) <= 1, "Hit shader pixels differ from pinned source");
									changed = changed || pixels[0][i] != background[i % 4];
								}
								if (!uniforms.envelope.active && blend) {
									check(!changed, "Inactive hit slots changed blended pixels");
									++inactive;
								}
								if (uniforms.envelope.active && changed) {
									++visible;
									if (profile >= 1 && profile <= 3) ++slot_witness[profile - 1];
								}
								check(glGetError() == GL_NO_ERROR, "Hit shader comparison raised GL error");
								++comparisons;
							}
				}
	check(visible && inactive && slot_witness[0] && slot_witness[1] && slot_witness[2], "Hit shader controls missed visibility, expiry or a hit slot");
	glUseProgram(0);
	for (auto program : programs) glDeleteProgram(program);
	glDeleteTextures(2, textures);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &target);
	glDisable(GL_BLEND);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
	std::cout << "Pinned sphere hit shader: " << comparisons << " framebuffer comparisons, " << visible << " visible and " << inactive << " inactive blended cases; all three slots passed\n";
}
