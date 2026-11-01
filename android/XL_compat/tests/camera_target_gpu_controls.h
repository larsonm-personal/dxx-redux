namespace camera_buffer_source
{
#include "xl_reference_camera_buffer_layout.h"
struct COGL {
	void GenTextures(int n, GLuint *p)
	{
		glGenTextures(n, p);
	}
	void DeleteTextures(int n, GLuint *p)
	{
		glDeleteTextures(n, p);
	}
	void BindTexture(GLuint p)
	{
		glBindTexture(GL_TEXTURE_2D, p);
	}
	void SelectTMU(int p)
	{
		glActiveTexture(p);
	}
	void SetTexturing(bool) {}
	GLenum ClearError(bool)
	{
		return glGetError();
	}
	int DrawBufferWidth()
	{
		throw std::runtime_error("Unexpected source camera width fallback");
	}
	int DrawBufferHeight()
	{
		throw std::runtime_error("Unexpected source camera height fallback");
	}
	GLuint CopyDepthTexture(int)
	{
		throw std::runtime_error("Unexpected source camera depth copy");
	}
	GLuint CreateDepthTexture(int32_t, int32_t, int32_t, int32_t, int32_t);
} ogl;
struct CFBO {
	tFrameBuffer m_info{};
	int32_t CreateColorBuffers(int32_t);
	int32_t CreateDepthBuffer();
	void AttachBuffers();
	void SelectColorBuffers(int32_t);
	~CFBO()
	{
		glDeleteTextures(m_info.nColorBuffers, m_info.hColorBuffer);
		glDeleteTextures(1, &m_info.hDepthBuffer);
		glDeleteFramebuffers(1, &m_info.hFBO);
	}
};
#define PrintLog(...) ((void) 0)
#include "xl_reference_camera_buffers.h"
#undef PrintLog
#undef MAX_COLOR_BUFFERS
} // namespace camera_buffer_source
static void camera_target_source_controls()
{
	xl_compat::LightningGlowState restore;
	glActiveTexture(GL_TEXTURE0);
	GLint environment;
	glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &environment);
	CameraTargetPattern pattern;
	unsigned cases = 0;
	for (const auto &screen : { std::array<int, 2>{ 14, 10 }, { 32, 24 }, { 63, 35 }, { 64, 47 } })
		for (bool hires : { false, true }) {
			const auto spec = xl_compat::camera_target_spec(screen[0], screen[1], hires);
			camera_buffer_source::CFBO source;
			auto &info = source.m_info;
			info.nWidth = spec.width;
			info.nHeight = spec.height;
			info.nType = 1;
			glGenFramebuffers(1, &info.hFBO);
			glBindFramebuffer(GL_FRAMEBUFFER, info.hFBO);
			check(source.CreateColorBuffers(2) && source.CreateDepthBuffer(), "Source camera attachment allocation failed");
			source.AttachBuffers();
			check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Source camera target incomplete");
			pattern.paint(spec.width, spec.height, [&](int buffer) { source.SelectColorBuffers(buffer); });
			xl_compat::CameraTargetGpu target;
			target.capture(spec, [&] { pattern.paint(spec.width, spec.height, [&](int buffer) { target.select(buffer); }); });
			for (unsigned color = 0; color < 2; ++color)
				for (int offset : { -3, 0, 3 }) {
					const auto expected = pattern.sample(info.hColorBuffer[color], spec.width, spec.height, offset);
					const auto actual = pattern.sample(target.texture(color), spec.width, spec.height, offset);
					check(expected == actual, "Source and portable camera target pixels differ");
					++cases;
				}
		}
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, environment);
	check(glGetError() == GL_NO_ERROR, "Camera source controls raised a GL error");
	std::cout << "Source camera framebuffer: " << cases << " allocation/render/sample comparisons passed\n";
}
