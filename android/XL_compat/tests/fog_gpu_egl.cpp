// Public offscreen desktop control for the same renderer used by the game
#include <GL/glew.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <algorithm>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include "../runtime/fog.h"
extern "C" {
#include "3d.h"
#include "dxxerror.h"
}
namespace
{
bool active = true, zero_distance = false;
int boundary_type = 0, boundary_reverse = 0;
bool camera_inside = false;
void check(bool valid, const char *message)
{
	if (!valid) throw std::runtime_error(message);
}
} // namespace
extern "C" {
int gl_initialized = 0;
int xl_fog_active(void)
{
	return active;
}
int xl_fog_colors(float colors[4][4])
{
	const float data[4][4] = { { 34.f / 255, 112.f / 255, 200.f / 255, 100 }, { 209.f / 255, 71.f / 255, 13.f / 255, 60 }, { 53.f / 255, 117.f / 255, 221.f / 255, 120 }, { 161.f / 255, 89.f / 255, 209.f / 255, 40 } };
	std::memcpy(colors, data, sizeof(data));
	if (zero_distance)
		for (int i = 0; i < 4; ++i) colors[i][3] = 0;
	return active;
}
int xl_fog_boundary_points(int segment, int side, int near_cap, int32_t points[4][3], int *reversed)
{
	check(boundary_type != 0, "Public shader control unexpectedly requested game geometry");
	if (side || segment != near_cap || (near_cap && camera_inside)) return 0;
	const int distance = near_cap ? 10 : 90;
	const int xy[4][2] = { { -1, -1 }, { -1, 1 }, { 1, 1 }, { 1, -1 } };
	for (int i = 0; i < 4; ++i) {
		const int corner = boundary_reverse ? 3 - i : i;
		points[i][0] = xy[corner][0] * distance * 65536;
		points[i][1] = xy[corner][1] * distance * 65536;
		points[i][2] = distance * 65536;
	}
	*reversed = boundary_reverse;
	return boundary_type;
}
ubyte g3_rotate_point(g3s_point *point, const vms_vector *world)
{
	check(boundary_type != 0, "Public shader control unexpectedly requested game view transform");
	point->p3_vec = *world;
	return 0;
}
void Error(const char *format, ...)
{
	char text[1024];
	va_list args;
	va_start(args, format);
	std::vsnprintf(text, sizeof(text), format, args);
	va_end(args);
	throw std::runtime_error(text);
}
}
namespace
{
void production_geometry()
{
	const short segments[] = { 0, 1 };
	int cases = 0, max_error = 0;
	for (int zero = 0; zero < 2; ++zero)
		for (int inside = 0; inside < 2; ++inside)
			for (int reverse = 0; reverse < 2; ++reverse)
				for (int type = 1; type <= 4; ++type) {
					zero_distance = zero != 0;
					camera_inside = inside != 0;
					boundary_type = type;
					boundary_reverse = reverse;
					glBindFramebuffer(GL_FRAMEBUFFER, 0);
					glViewport(13, 11, 64, 64);
					glDisable(GL_SCISSOR_TEST);
					glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
					glDepthMask(GL_TRUE);
					glClearColor(.25f, .5f, .75f, 1.f);
					glClearDepth((1.f + (5001.f - 10000.f / 300.f) / 4999.f) * .5f);
					glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
					ogl_xl_fog_scene_request();
					ogl_xl_fog_volumes(segments, 2);
					ogl_xl_fog_composite();
					xl_fog_scene_result scene{};
					ogl_xl_fog_scene_result(&scene);
					check(scene.completed && scene.passed && scene.pixels == 25 && scene.covered == 25 && scene.changed == (zero ? 0 : 25), "Production fog geometry/culling did not cover its control view");
					float settings[4][4]{};
					xl_fog_colors(settings);
					const auto &color = settings[type - 1];
					const float opacity = zero ? 0.f : (std::min) (1.f, (inside ? 89.f : 80.f) / color[3]);
					const int background[] = { 64, 128, 191 };
					for (int sample = 0; sample < 2; ++sample) {
						unsigned char pixel[4]{};
						glReadPixels(sample ? 2 : 45, sample ? 2 : 43, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
						for (int c = 0; c < 3; ++c) {
							const int expected = sample ? background[c] : static_cast<int>(std::lround(color[c] * 255 * opacity + background[c] * (1 - opacity)));
							const int error = std::abs(int(pixel[c]) - expected);
							max_error = (std::max) (max_error, error);
							check(error <= 3, "Production fog geometry differs from its analytical world interval");
						}
						check(pixel[3] == 255, "Production fog geometry changed presentation opacity");
					}
					++cases;
				}
	boundary_type = 0;
	std::cout << "Production volume geometry: " << cases << " projected/culling controls, max error " << max_error << '\n';
}
} // namespace
int main()
{
	EGLDisplay display = EGL_NO_DISPLAY;
	EGLContext context = EGL_NO_CONTEXT;
	EGLSurface surface = EGL_NO_SURFACE;
	int result = 1;
	try {
		display = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
		check(display != EGL_NO_DISPLAY && eglInitialize(display, nullptr, nullptr), "Cannot initialize surfaceless EGL");
		check(eglBindAPI(EGL_OPENGL_API), "Cannot select desktop OpenGL");
		const EGLint attributes[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE };
		EGLConfig config;
		EGLint count = 0;
		check(eglChooseConfig(display, attributes, &config, 1, &count) && count == 1, "Missing EGL fog control format");
		const EGLint size[] = { EGL_WIDTH, 96, EGL_HEIGHT, 96, EGL_NONE };
		surface = eglCreatePbufferSurface(display, config, size);
		context = eglCreateContext(display, config, EGL_NO_CONTEXT, nullptr);
		check(surface != EGL_NO_SURFACE && context != EGL_NO_CONTEXT && eglMakeCurrent(display, surface, surface, context), "Cannot create current desktop fog context");
		const GLenum glew = glewInit();
		check((glew == GLEW_OK || glew == GLEW_ERROR_NO_GLX_DISPLAY) && GLEW_VERSION_3_0, "Cannot initialize desktop OpenGL entry points");
		while (glGetError() != GL_NO_ERROR) {}
		gl_initialized = 1;
		glViewport(0, 0, 96, 96);
		// Fixed-function alpha rejection must neither affect fog nor leak its state
		glAlphaFunc(GL_NEVER, 1.f);
		glEnable(GL_ALPHA_TEST);
		glDepthMask(GL_FALSE);
		glClearDepth(.25);
		if (const char *error = ogl_xl_fog_prepare()) throw std::runtime_error(error);
		GLfloat depth_clear = 0;
		GLboolean depth_mask = GL_TRUE;
		glGetFloatv(GL_DEPTH_CLEAR_VALUE, &depth_clear);
		glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
		check(glIsEnabled(GL_ALPHA_TEST) && depth_clear == .25f && depth_mask == GL_FALSE, "Fog admission changed caller GL state");
		std::cout << "GPU: " << glGetString(GL_RENDERER) << ", " << glGetString(GL_VERSION) << '\n';
		for (int pass = 0; pass < 2; ++pass) {
			zero_distance = pass != 0;
			ogl_xl_fog_scene_request();
			xl_fog_gpu_result report{};
			if (const char *error = ogl_xl_fog_gpu_probe(&report)) throw std::runtime_error(error);
			check(report.cases >= 60 && report.state_restored, "Incomplete fog GPU control");
			xl_fog_scene_result scene{};
			ogl_xl_fog_scene_result(&scene);
			check(scene.completed && scene.passed && scene.pixels == 25 && scene.covered == 25 && scene.changed == (pass ? 0 : 25), ogl_xl_fog_scene_error());
			std::cout << (pass ? "Zero-distance" : "Authored") << " fog GPU: " << report.cases << " cases, " << report.pixels << " pixels, " << report.multisample_cases << " multisample cases, max error " << report.max_error << '\n';
			std::cout << "Scene readback control: " << scene.pixels << " pixels, " << scene.covered << " covered, " << scene.changed << " changed, max error " << scene.max_error << '\n';
		}
		production_geometry();
		active = false;
		xl_fog_diagnostics before{}, after{};
		ogl_xl_fog_diagnostics(&before);
		ogl_xl_fog_volumes(nullptr, 0);
		ogl_xl_fog_composite();
		ogl_xl_fog_diagnostics(&after);
		check(before.volume_passes == after.volume_passes && before.composite_passes == after.composite_passes, "Inactive fog submitted GPU work");
		ogl_xl_fog_destroy(0);
		result = 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
	}
	if (display != EGL_NO_DISPLAY) {
		eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
		if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
		eglTerminate(display);
	}
	return result;
}
