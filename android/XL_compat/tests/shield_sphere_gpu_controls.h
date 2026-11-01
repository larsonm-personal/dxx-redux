// Minimal GL adapters for the unchanged source hit-effect draw branch
namespace shield_draw_source
{
struct CSphereVertex {
	float m_v[4], m_tc[2], m_c[4];
};
struct Vertices : std::vector<CSphereVertex> {
	CSphereVertex *Buffer()
	{
		return empty() ? nullptr : data();
	}
};
struct Color {
	struct {
		float vec[4] = { .7f, .8f, .9f, 1.f };
	} v;
	Color &operator*=(float scale)
	{
		for (auto &c : v.vec) c *= scale;
		return *this;
	}
};
struct Pulse {
	float Scale()
	{
		throw std::runtime_error("Unexpected source shield pulse");
	}
};
struct Bitmap {
	GLuint texture;
	int Bind(int)
	{
		glBindTexture(GL_TEXTURE_2D, texture);
		return 0;
	}
};
struct GL {
	std::vector<GLenum> culls;
	void SetCullMode(GLenum mode)
	{
		culls.push_back(mode);
		glCullFace(mode);
	}
	void EnableClientStates(bool texture, bool color, int, GLenum unit)
	{
		glClientActiveTexture(unit);
		glEnableClientState(GL_VERTEX_ARRAY);
		if (texture) glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		if (color) glEnableClientState(GL_COLOR_ARRAY);
	}
	void DisableClientStates(bool texture, bool color, int, GLenum unit)
	{
		glClientActiveTexture(unit);
		glDisableClientState(GL_VERTEX_ARRAY);
		if (texture) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		if (color) glDisableClientState(GL_COLOR_ARRAY);
	}
} ogl;
static void OglTexCoordPointer(int n, GLenum type, GLsizei stride, const void *p)
{
	glTexCoordPointer(n, type, stride, p);
}
static void OglColorPointer(int n, GLenum type, GLsizei stride, const void *p)
{
	glColorPointer(n, type, stride, p);
}
static void OglVertexPointer(int n, GLenum type, GLsizei stride, const void *p)
{
	glVertexPointer(n, type, stride, p);
}
static void OglDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	glDrawArrays(mode, first, count);
}
static void OglCullFace(int front)
{
	ogl.SetCullMode(front ? GL_FRONT : GL_BACK);
}
static void UnloadSphereShader()
{
	throw std::runtime_error("Unexpected non-hit shield shader");
}
struct SourceShieldDraw {
	Vertices m_worldVerts, m_viewVerts;
	int m_nFaces = 0;
	Bitmap *m_pBitmap = nullptr;
	Pulse *m_pPulse = nullptr;
	Color m_color;
	int FaceNodes()
	{
		return 3;
	}
	int SetupColor(float, int)
	{
		throw std::runtime_error("Unexpected non-hit shield color");
	}
	void DrawFaces(int32_t, int32_t, int32_t, int32_t, int32_t);
	void RenderFaces(float, int32_t, int32_t, int32_t, int32_t);
};
#define ENTER(a, b)
#define RETURN           return;
#define SPHERE_WIREFRAME 0
#include "xl_reference_shield_draw.h"
#undef SPHERE_WIREFRAME
#undef RETURN
#undef ENTER
} // namespace shield_draw_source

static std::vector<GLint> shield_gpu_state()
{
	std::vector<GLint> result;
	for (GLenum field : { GL_CURRENT_PROGRAM, GL_VERTEX_ARRAY_BINDING, GL_ARRAY_BUFFER_BINDING, GL_ACTIVE_TEXTURE,
	                      GL_PIXEL_UNPACK_BUFFER_BINDING, GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH, GL_UNPACK_SKIP_ROWS, GL_UNPACK_SKIP_PIXELS,
	                      GL_TEXTURE_BINDING_2D, GL_CULL_FACE_MODE, GL_FRONT_FACE, GL_DEPTH_FUNC, GL_DEPTH_WRITEMASK,
	                      GL_BLEND_SRC_RGB, GL_BLEND_DST_RGB, GL_BLEND_SRC_ALPHA, GL_BLEND_DST_ALPHA, GL_BLEND_EQUATION_RGB, GL_BLEND_EQUATION_ALPHA }) {
		GLint value;
		glGetIntegerv(field, &value);
		result.push_back(value);
	}
	for (GLenum capability : { GL_DEPTH_TEST, GL_BLEND, GL_CULL_FACE, GL_ALPHA_TEST }) result.push_back(glIsEnabled(capability));
	GLint active, texture;
	glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
	glActiveTexture(active);
	result.push_back(texture);
	return result;
}

static void shield_sphere_gpu_controls(const char *path)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	const auto length = input.tellg();
	check(input && length >= 18 && length <= 64 * 1024 * 1024, "Invalid shield image size");
	std::vector<unsigned char> bytes(static_cast<size_t>(length));
	input.seekg(0);
	input.read(reinterpret_cast<char *>(bytes.data()), length);
	check(bool(input), "Cannot read original shield image");
	const auto bitmap = xl_compat::read_particle_image(bytes.data(), bytes.size());
	check(bitmap.width == 128 && bitmap.height == 1024 && bitmap.frames == 8, "Original shield frame layout changed");
	const GLuint program = soft_test_program(pszSphereEffectVS[0], pszSphereEffectFS[0]);
	xl_compat::ShieldGpu gpu;
	xl_compat::ShieldTexture owned_texture(bitmap);
	for (unsigned invalid : { 8u, 128u, 0xffffffffu }) {
		bool rejected = false;
		try {
			owned_texture.handle(invalid, xl_compat::ShieldFilter::linear);
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		check(rejected, "Shield accepted an invalid frame index");
	}
	GLuint unpack_buffer;
	glGenBuffers(1, &unpack_buffer);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack_buffer);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, 1, nullptr, GL_STATIC_DRAW);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	GLuint textures[2], target, depth;
	glGenTextures(2, textures);
	glGenFramebuffers(1, &target);
	glGenRenderbuffers(1, &depth);
	glBindFramebuffer(GL_FRAMEBUFFER, target);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textures[1]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[1], 0);
	glBindRenderbuffer(GL_RENDERBUFFER, depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, 32, 32);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
	check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Shield target incomplete");
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glViewport(0, 0, 32, 32);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1, 1, -1, 1, -2, 2);
	glMatrixMode(GL_MODELVIEW);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_FRAMEBUFFER_SRGB);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	ParticleBlendSource source_blend;
	source_blend.SetBlendMode(OGL_BLEND_ADD);
	glBlendFunc(source_blend.factors[0], source_blend.factors[1]);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	const xl_compat::HitRenderMatrix identity{ { { 65536, 0, 0 }, { 0, 65536, 0 }, { 0, 0, 65536 } } };
	xl_compat::CameraHitState state;
	state.times = { 4000, 4000, 4000 };
	state.points = { { { 0, 0, 65536 }, { -65536, 0, 0 }, { 65536, 0, 0 } } };
	unsigned comparisons = 0, visible = 0, blocked = 0, expired = 0;
	std::array<unsigned, 8> frame_witness{};
	for (int quality : { 2, 3, 4 }) {
		const auto mesh = xl_compat::shield_sphere_mesh(quality);
		shield_draw_source::SourceShieldDraw source;
		shield_draw_source::Bitmap source_bitmap{ textures[0] };
		source.m_pBitmap = &source_bitmap;
		source.m_nFaces = int(mesh.size() / 3);
		source.m_viewVerts.resize(mesh.size());
		for (const auto &v : mesh) {
			shield_draw_source::CSphereVertex copy{};
			std::copy(v.position.begin(), v.position.end(), copy.m_v);
			std::copy(v.uv.begin(), v.uv.end(), copy.m_tc);
			source.m_worldVerts.push_back(copy);
		}
		for (unsigned frame = 0; frame < bitmap.frames; ++frame) {
			glBindTexture(GL_TEXTURE_2D, textures[0]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap.pixels.data() + frame * 128 * 128 * 4);
			glGenerateMipmap(GL_TEXTURE_2D);
			if (frame == 4) xl_compat::ShieldTexture::discard_context(false);
			if (frame == 4) gpu.discard(false);
			for (GLint filter : { GL_NEAREST, GL_LINEAR, GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR }) {
				const GLint mag = filter == GL_NEAREST ? GL_NEAREST : GL_LINEAR;
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, textures[1]);
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack_buffer);
				glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
				glPixelStorei(GL_UNPACK_ROW_LENGTH, 147);
				glPixelStorei(GL_UNPACK_SKIP_ROWS, 1);
				glPixelStorei(GL_UNPACK_SKIP_PIXELS, 2);
				const auto upload_before = shield_gpu_state();
				const auto sampling = filter == GL_NEAREST ? xl_compat::ShieldFilter::nearest : filter == GL_LINEAR              ? xl_compat::ShieldFilter::linear
				                                                                            : filter == GL_LINEAR_MIPMAP_NEAREST ? xl_compat::ShieldFilter::mip_nearest
				                                                                                                                 : xl_compat::ShieldFilter::trilinear;
				const GLuint border = owned_texture.handle(frame, sampling);
				check(upload_before == shield_gpu_state(), "Shield frame upload leaked GL state");
				check(border == owned_texture.handle(frame, sampling), "Shield frame cache missed repeated lookup");
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
				glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
				glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
				glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
				glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, textures[0]);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
				for (int age : { 0, 1000, 2000 }) {
					const auto uniforms = xl_compat::camera_hit_hardware(state, 297534, 4000 + age, 2000, identity);
					for (float rotation : { 0.f, 31.f }) {
						glLoadIdentity();
						glRotatef(rotation, 0, 1, 0);
						glScalef(.7f, .7f, .7f);
						std::array<float, 16> model{}, projection{}, matrix{};
						glGetFloatv(GL_MODELVIEW_MATRIX, model.data());
						glGetFloatv(GL_PROJECTION_MATRIX, projection.data());
						for (unsigned c = 0; c < 4; ++c)
							for (unsigned r = 0; r < 4; ++r)
								for (unsigned k = 0; k < 4; ++k) matrix[c * 4 + r] += projection[k * 4 + r] * model[c * 4 + k];
						for (float scene_depth : { 1.f, .5f, .1f }) {
							std::array<std::array<unsigned char, 4096>, 3> pixels{};
							for (unsigned implementation = 0; implementation < 3; ++implementation) {
								glUseProgram(program);
								glUniform1i(glGetUniformLocation(program, "sphereTex"), 0);
								glUniform4fv(glGetUniformLocation(program, "vHit"), 3, uniforms.vectors[0].data());
								glUniform3fv(glGetUniformLocation(program, "fRad"), 1, uniforms.envelope.inverse_radius.data());
								glClearColor(.2f, .4f, .6f, 1);
								glDepthMask(GL_TRUE);
								glClearDepth(scene_depth);
								glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
								glDepthMask(GL_FALSE);
								if (!implementation) {
									shield_draw_source::ogl.culls.clear();
									source.RenderFaces(1, source.m_nFaces, 1, 1, 0);
									check(shield_draw_source::ogl.culls == std::vector<GLenum>{ GL_BACK, GL_FRONT, GL_BACK }, "Source shield cull sequence changed");
								} else {
									glActiveTexture(GL_TEXTURE1);
									glBindTexture(GL_TEXTURE_2D, textures[1]);
									glCullFace(GL_FRONT);
									glFrontFace(GL_CW);
									glDepthFunc(GL_GREATER);
									glDepthMask(GL_TRUE);
									glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
									const auto before = shield_gpu_state();
									gpu.draw(quality, uniforms, { .7f, .8f, .9f, 1 }, matrix, implementation == 1 ? textures[0] : border,
									         implementation == 1 ? 0 : 128, filter == GL_NEAREST);
									check(before == shield_gpu_state(), "Production shield leaked caller GL state");
									glActiveTexture(GL_TEXTURE0);
									glCullFace(GL_BACK);
									glFrontFace(GL_CCW);
									glDepthFunc(GL_LEQUAL);
									glDepthMask(GL_FALSE);
									glBlendFunc(GL_ONE, GL_ONE);
								}
								glReadPixels(0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, pixels[implementation].data());
								std::array<float, 1024> depths{};
								glReadPixels(0, 0, 32, 32, GL_DEPTH_COMPONENT, GL_FLOAT, depths.data());
								for (auto value : depths) check(value == scene_depth, "Shield unexpectedly wrote scene depth");
							}
							bool changed = false;
							const std::array<unsigned char, 4> background{ 51, 102, 153, 255 };
							for (unsigned i = 0; i < pixels[0].size(); ++i) {
								if (std::abs(int(pixels[0][i]) - int(pixels[1][i])) > 1)
									throw std::runtime_error("Production shield mismatch q=" + std::to_string(quality) + " frame=" + std::to_string(frame) + " filter=" + std::to_string(filter) + " age=" + std::to_string(age) + " rotation=" + std::to_string(rotation) + " depth=" + std::to_string(scene_depth) + " byte=" + std::to_string(i) + " source=" + std::to_string(pixels[0][i]) + " actual=" + std::to_string(pixels[1][i]));
								if (std::abs(int(pixels[0][i]) - int(pixels[2][i])) > 1)
									throw std::runtime_error("Border-mapped shield mismatch q=" + std::to_string(quality) + " frame=" + std::to_string(frame) + " filter=" + std::to_string(filter) + " age=" + std::to_string(age) + " rotation=" + std::to_string(rotation) + " depth=" + std::to_string(scene_depth) + " byte=" + std::to_string(i) + " source=" + std::to_string(pixels[0][i]) + " actual=" + std::to_string(pixels[2][i]));
								changed = changed || pixels[0][i] != background[i % 4];
							}
							if (scene_depth == .1f) {
								check(!changed, "Occluded shield changed pixels");
								++blocked;
							}
							if (age == 2000) {
								check(!changed, "Expired shield changed pixels");
								++expired;
							}
							if (changed) {
								++visible;
								++frame_witness[frame];
							}
							check(glGetError() == GL_NO_ERROR, "Whole shield GL error");
							comparisons += 2;
						}
					}
				}
			}
		}
	}
	check(visible && blocked && expired, "Missing whole shield visibility/depth/expiry witnesses");
	for (auto count : frame_witness) check(count != 0, "Missing original shield frame witness");
	glUseProgram(0);
	glDepthMask(GL_TRUE);
	gpu.discard(false);
	xl_compat::ShieldTexture::discard_context(false);
	glDeleteProgram(program);
	glDeleteTextures(2, textures);
	glDeleteBuffers(1, &unpack_buffer);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &target);
	glDeleteRenderbuffers(1, &depth);
	std::cout << "Original whole shield: " << comparisons << " comparisons, " << visible << " visible, " << blocked << " occluded, " << expired << " expired; all eight frames, qualities 2/3/4 and two cull passes passed\n";
}
