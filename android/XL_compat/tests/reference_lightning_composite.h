// Runtime boundaries for the extracted glow Render and End methods
namespace lightning_composite_source
{
constexpr int BLUR_SHADOW = 32, BLUR_OUTLINE = 16;
// Blend constants come from the existing pinned particle blend extraction
template <class T>
T Clamp(T value, T minimum, T maximum)
{
	return (std::max) (minimum, (std::min) (maximum, value));
}
template <class T>
void Swap(T &a, T &b)
{
	std::swap(a, b);
}
struct CCanvas {
	int x = 0, y = 0, w = 0, h = 0, screen_height = 0;
	static CCanvas *current;
	static CCanvas *Current()
	{
		return current;
	}
	int Width() const
	{
		return w;
	}
	int Height() const
	{
		return h;
	}
	int Left() const
	{
		return x;
	}
	int Top() const
	{
		return screen_height - y - h;
	}
	int Right() const
	{
		return x + w;
	}
	int Bottom() const
	{
		return Top() + h;
	}
	void SetViewport()
	{
		glViewport(x, y, w, h);
	}
	void Activate(const char *)
	{
		current = this;
		SetViewport();
	}
	void Deactivate()
	{
		SetViewport();
	}
};
CCanvas *CCanvas::current = nullptr;
struct {
	struct {
		CCanvas scene, window, screen;
	} renderData;
} gameData;
struct {
	struct {
		int nWindow[1]{};
	} render;
} gameStates;
struct Options {
	struct {
		struct {
			int bGlow = 1;
		} effects;
	} render;
} options;
Options *gameOpts = &options;
struct ShaderManager {
	void Deploy(int)
	{
		glUseProgram(0);
	}
} shaderManager;
struct Texture {
	GLuint texture = 0;
	GLuint ColorBuffer(bool)
	{
		return texture;
	}
} textures[3];
struct Ogl {
	GLuint scene = 0, targets[3]{};
	GLint viewport[4]{};
	Texture *BlurBuffer(int source)
	{
		return &textures[source < 0 ? 0 : source + 1];
	}
	void ChooseDrawBuffer()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, scene);
	}
	bool SelectBlurBuffer(int i)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, targets[i + 1]);
		return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	}
	void SaveViewport()
	{
		glGetIntegerv(GL_VIEWPORT, viewport);
	}
	void RestoreViewport()
	{
		glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
	}
	GLenum GetDepthMode()
	{
		GLint value;
		glGetIntegerv(GL_DEPTH_FUNC, &value);
		return GLenum(value);
	}
	bool GetDepthWrite()
	{
		GLboolean value;
		glGetBooleanv(GL_DEPTH_WRITEMASK, &value);
		return value;
	}
	bool GetFaceCulling()
	{
		return glIsEnabled(GL_CULL_FACE);
	}
	void GetBlendMode(GLenum &a, GLenum &b)
	{
		GLint x, y;
		glGetIntegerv(GL_BLEND_SRC_RGB, &x);
		glGetIntegerv(GL_BLEND_DST_RGB, &y);
		a = GLenum(x);
		b = GLenum(y);
	}
	void SetDepthWrite(bool value)
	{
		glDepthMask(value);
	}
	void SetAlphaTest(bool value)
	{
		if (value) glEnable(GL_ALPHA_TEST);
		else glDisable(GL_ALPHA_TEST);
	}
	void SetFaceCulling(bool value)
	{
		if (value) glEnable(GL_CULL_FACE);
		else glDisable(GL_CULL_FACE);
	}
	void SetStencilTest(bool value)
	{
		if (value) glEnable(GL_STENCIL_TEST);
		else glDisable(GL_STENCIL_TEST);
	}
	void SetDepthMode(GLenum value)
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(value);
	}
	void SetBlending(bool value)
	{
		if (value) glEnable(GL_BLEND);
		else glDisable(GL_BLEND);
	}
	void SetBlendMode(GLenum a, GLenum b)
	{
		glEnable(GL_BLEND);
		glBlendFunc(a, b);
	}
	void SetBlendMode(int mode)
	{
		SetBlendMode(mode == OGL_BLEND_ALPHA ? GL_SRC_ALPHA : GL_ONE, mode == OGL_BLEND_REPLACE ? GL_ZERO : mode == OGL_BLEND_MULTIPLY ? GL_SRC_COLOR
		                                                                                                : mode == OGL_BLEND_ALPHA      ? GL_ONE_MINUS_SRC_ALPHA
		                                                                                                                               : GL_ONE);
	}
	void ResetClientStates(int)
	{
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glDisableClientState(GL_COLOR_ARRAY);
	}
	void EnableClientStates(int textured, int, int, GLenum unit)
	{
		glActiveTexture(unit);
		glClientActiveTexture(unit);
		glEnableClientState(GL_VERTEX_ARRAY);
		if (textured) {
			glEnable(GL_TEXTURE_2D);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}
	}
	void DisableClientStates(int, int, int, GLenum)
	{
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	void BindTexture(GLuint value)
	{
		glBindTexture(GL_TEXTURE_2D, value);
	}
} ogl;
void OglTexCoordPointer(GLint n, GLenum type, GLsizei stride, const void *pointer)
{
	glTexCoordPointer(n, type, stride, pointer);
}
void OglVertexPointer(GLint n, GLenum type, GLsizei stride, const void *pointer)
{
	glVertexPointer(n, type, stride, pointer);
}
void OglDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	glDrawArrays(mode, first, count);
}
void ClearDrawBuffer(int)
{
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
}
struct CGlowRenderer {
	int m_nType = 1, m_nStrength = 3, m_bViewport = 1;
	bool m_bReplace = true;
	float m_brightness = 1.05f;
	struct Point {
		int x, y;
	} m_renderMin{}, m_renderMax{};
	GLuint shader = 0;
	bool UseViewport()
	{
		return true;
	}
	bool Visible()
	{
		return true;
	}
	bool Reset(int glow, int = 0)
	{
		m_nStrength = -1;
		return glow != 0;
	}
	void LoadShader(int direction, float)
	{
		glUseProgram(shader);
		glUniform1i(glGetUniformLocation(shader, "glowSource"), 0);
		glUniform1f(glGetUniformLocation(shader, "direction"), float(direction));
		glUniform1f(glGetUniformLocation(shader, "scale"), 1.0f / (direction ? gameData.renderData.screen.Height() : gameData.renderData.screen.Width()));
		glUniform1f(glGetUniformLocation(shader, "brightness"), m_brightness);
	}
	void Render(int source, int direction, float radius, bool clear = false);
	bool End(float alpha = 1);
};
#define USE_VIEWPORT 2
#define BLUR         2
#define BLUR_RADIUS  ((m_bViewport > 0) ? 3.0f : 0.0f)
#include "xl_reference_lightning_composite.h"
#undef BLUR_RADIUS
#undef BLUR
#undef USE_VIEWPORT
} // namespace lightning_composite_source
