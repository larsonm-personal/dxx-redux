// Asset-free control: real engine upload, merged draw and GL shaders, with data-only game globals
#include <GL/glew.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "../pog.h"
#include "../hog.h"
#include "../runtime/pog_renderer.h"
#include "../runtime/particle_renderer.h"
#include "../particle_render.h"
#ifdef XL_POG_SHADER_REFERENCE
#include "xl_reference_pog_shaders.h"
#endif
extern "C" {
#include "3d.h"
#include "ogl_init.h"
#include "oglprog.h"
#include "args.h"
#include "config.h"
#include "game.h"
#include "piggy.h"
#include "pngfile.h"
#include "multi.h"
#include "dxxerror.h"
struct Arg GameArg{};
struct Cfg GameCfg{};
struct netgame_info Netgame{};
int Game_mode = 0, gl_initialized = 0;
extern int r_texcount;
ubyte gr_palette[768]{}, gr_current_pal[768]{};
grs_canvas *grd_curcanv = nullptr;
void ogl_init_texture_list_internal(void);
void ogl_init_pixel_buffers(int, int);
void ogl_close_pixel_buffers(void);
void ogl_bindbmtex(grs_bitmap *);
char *piggy_game_bitmap_name(grs_bitmap *)
{
	return nullptr;
}
int read_png(const char *, png_data *)
{
	return 0;
}
int piggy_bitmap_get_flags(grs_bitmap *bitmap)
{
	return bitmap->bm_flags;
}
void gr_rle_decode(ubyte *, ubyte *)
{
	throw std::runtime_error("unexpected RLE in GPU control");
}
int gr_find_closest_color(int, int, int)
{
	throw std::runtime_error("RGBA renderer quantized a source pixel");
}
float xl_preview_alpha(float value)
{
	return value;
}
void con_printf(int, const char *, ...) {}
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
unsigned cases = 0, pixels = 0, maximum_error = 0;
void check(bool value, const char *message)
{
	if (!value) throw std::runtime_error(message);
}
struct Bitmap {
	grs_bitmap bitmap{};
	std::vector<unsigned char> data;
	Bitmap(int width, int height, bool rgba) : data(size_t(width) * height * (rgba ? 4 : 1))
	{
		bitmap.bm_w = width;
		bitmap.bm_h = height;
		bitmap.bm_rowsize = width * (rgba ? 4 : 1);
		bitmap.bm_type = rgba ? BM_RGBA : BM_LINEAR;
		bitmap.bm_data = data.data();
	}
	~Bitmap()
	{
		ogl_freebmtexture(&bitmap);
	}
	void normalized()
	{
		xl_compat::PogBitmap source;
		source.width = bitmap.bm_w;
		source.height = bitmap.bm_h;
		source.truecolor = true;
		source.pixels = data;
		xl_compat::prepare_pog_rgba(source);
		data = std::move(source.pixels);
		bitmap.bm_data = data.data();
		bitmap.bm_flags = source.flags & 7;
	}
	std::array<double, 4> pixel(int x, int y) const
	{
		const auto offset = size_t(y) * bitmap.bm_w + x;
		if (bitmap.bm_type == BM_RGBA)
			return { data[offset * 4] / 255., data[offset * 4 + 1] / 255., data[offset * 4 + 2] / 255., data[offset * 4 + 3] / 255. };
		const auto index = data[offset];
		if (index == 254) return { 1, 1, 1, 0 };
		if (index == 255) return {};
		return { gr_palette[index * 3] * 4 / 255., gr_palette[index * 3 + 1] * 4 / 255., gr_palette[index * 3 + 2] * 4 / 255., 1 };
	}
	bool key(int x, int y) const
	{
		const auto offset = size_t(y) * bitmap.bm_w + x;
		return bitmap.bm_type == BM_RGBA ? data[offset * 4] == 120 && data[offset * 4 + 1] == 88 && data[offset * 4 + 2] == 128 : data[offset] == 254;
	}
};
void upload(Bitmap &image, int filter)
{
	ogl_freebmtexture(&image.bitmap);
	ogl_loadbmtexture_f(&image.bitmap, filter, nullptr);
	check(image.bitmap.gltexture && image.bitmap.gltexture->handle, "Production RGBA upload omitted its texture");
	glBindTexture(GL_TEXTURE_2D, image.bitmap.gltexture->handle);
	std::vector<unsigned char> actual(image.data.size());
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
	check(actual == image.data, "Production RGBA upload changed source pixels or dimensions");
	if (filter) {
		int width = 0;
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 1, GL_TEXTURE_WIDTH, &width);
		check(width == (std::max) (1, image.bitmap.bm_w / 2), "RGBA mipmaps are missing or resized to a power of two");
	}
	++cases;
	pixels += unsigned(image.bitmap.bm_w * image.bitmap.bm_h);
}
std::array<unsigned char, 8 * 8 * 4> draw(Bitmap &base, Bitmap &overlay, int orientation, bool blend)
{
	g3s_point vertices[4]{};
	const g3s_point *points[4]{};
	g3s_uvl uv[4]{};
	g3s_lrgb light[4]{};
	const int xy[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
	for (int i = 0; i < 4; ++i) {
		vertices[i].p3_vec = { (xy[i][0] * 2 - 1) * F1_0, (xy[i][1] * 2 - 1) * F1_0, F1_0 };
		points[i] = &vertices[i];
		uv[i].u = xy[i][0] * F1_0;
		uv[i].v = xy[i][1] * F1_0;
		light[i].r = light[i].g = light[i].b = F1_0;
	}
	glViewport(0, 0, 8, 8);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GEQUAL, .02f);
	if (blend) glEnable(GL_BLEND);
	else glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glClearColor(19.f / 255, 27.f / 255, 43.f / 255, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	const GLfloat identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	ogl_prog_set_matrix(const_cast<GLfloat *>(identity));
	g3_draw_tmap_2(4, points, uv, light, &base.bitmap, &overlay.bitmap, orientation);
	std::array<unsigned char, 8 * 8 * 4> result{};
	glReadPixels(0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
	check(glGetError() == GL_NO_ERROR, "Production merged RGBA draw raised a GL error");
	return result;
}
void composition(Bitmap &base, Bitmap &overlay, int orientation, bool blend)
{
	const auto actual = draw(base, overlay, orientation, blend);
	const std::array<double, 4> background{ 19 / 255., 27 / 255., 43 / 255., 1 };
	const bool xl = base.bitmap.bm_type == BM_RGBA || overlay.bitmap.bm_type == BM_RGBA;
	for (int y = 0; y < 8; ++y)
		for (int x = 0; x < 8; ++x) {
			const int tx = orientation == 1 ? 7 - y : orientation == 2 ? 7 - x
			                                      : orientation == 3   ? y
			                                                           : x;
			const int ty = orientation == 1 ? x : orientation == 2 ? 7 - y
			                                  : orientation == 3   ? 7 - x
			                                                       : y;
			const auto bottom = base.pixel(x, y), top = overlay.pixel(tx, ty);
			std::array<double, 4> expected{};
			expected[3] = xl ? (std::min) (1., bottom[3] + top[3]) : bottom[3] + top[3] - bottom[3] * top[3];
			for (int c = 0; c < 3; ++c) expected[c] = bottom[c] * (1 - top[3]) + top[c] * top[3];
			// The 8-bit compatibility framebuffer quantizes its alpha reference
			if (overlay.key(tx, ty) || expected[3] < (xl ? 1. / 255 : .02)) expected = background;
			else if (blend) {
				const auto alpha = expected[3];
				for (int c = 0; c < 4; ++c) expected[c] = expected[c] * alpha + background[c] * (1 - alpha);
			}
			for (int c = 0; c < 4; ++c) {
				const int want = int(expected[c] * 255 + .5);
				const int got = actual[(y * 8 + x) * 4 + c];
				const auto error = unsigned(std::abs(want - got));
				maximum_error = (std::max) (maximum_error, error);
				if (error > 2) throw std::runtime_error("RGBA composition differs at " + std::to_string(x) + "," + std::to_string(y) + " channel " + std::to_string(c) + ": " + std::to_string(got) + " expected " + std::to_string(want));
			}
		}
	++cases;
	pixels += 64;
}

#ifdef XL_POG_SHADER_REFERENCE
GLuint source_program(bool masked)
{
	const auto compile = [](GLenum type, const char *code) {
		const auto shader = glCreateShader(type);
		glShaderSource(shader, 1, &code, nullptr);
		glCompileShader(shader);
		int ok = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
		check(ok != 0, "Pinned XL texture shader failed to compile");
		return shader;
	};
	const auto vertex = compile(GL_VERTEX_SHADER, texMergeVS[masked ? 1 : 0]);
	const auto fragment = compile(GL_FRAGMENT_SHADER, texMergeFS[masked ? 4 : 3]);
	const auto program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);
	int ok = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &ok);
	check(ok != 0, "Pinned XL texture shader failed to link");
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	return program;
}
void source_comparison(Bitmap &base, Bitmap &overlay)
{
	GLuint mask = 0;
	glGenTextures(1, &mask);
	std::vector<unsigned char> mask_pixels(size_t(overlay.bitmap.bm_w) * overlay.bitmap.bm_h);
	for (int y = 0; y < overlay.bitmap.bm_h; ++y)
		for (int x = 0; x < overlay.bitmap.bm_w; ++x) mask_pixels[size_t(y) * overlay.bitmap.bm_w + x] = overlay.key(x, y) ? 0 : 255;
	glBindTexture(GL_TEXTURE_2D, mask);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, overlay.bitmap.bm_w, overlay.bitmap.bm_h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, mask_pixels.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	const bool masked = (overlay.bitmap.bm_flags & BM_FLAG_SUPER_TRANSPARENT) != 0;
	const auto program = source_program(masked);
	for (int orientation = 0; orientation < 4; ++orientation)
		for (int blend = 0; blend < 2; ++blend) {
			const auto production = draw(base, overlay, orientation, blend != 0);
			glUseProgram(program);
			glUniform1i(glGetUniformLocation(program, "baseTex"), 0);
			glUniform1i(glGetUniformLocation(program, "decalTex"), 1);
			glUniform1i(glGetUniformLocation(program, "maskTex"), 2);
			glUniform1i(glGetUniformLocation(program, "bMask"), masked);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, base.bitmap.gltexture->handle);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, overlay.bitmap.gltexture->handle);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, mask);
			glActiveTexture(GL_TEXTURE0);
			glClear(GL_COLOR_BUFFER_BIT);
			glEnable(GL_ALPHA_TEST);
			glAlphaFunc(GL_GEQUAL, .005f);
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glColor4f(1, 1, 1, 1);
			glBegin(GL_QUADS);
			for (const auto &xy : { std::array<int, 2>{ 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } }) {
				const float u = float(xy[0]), v = float(xy[1]);
				const float ou = orientation == 1 ? 1 - v : orientation == 2 ? 1 - u
				                                        : orientation == 3   ? v
				                                                             : u;
				const float ov = orientation == 1 ? u : orientation == 2 ? 1 - v
				                                    : orientation == 3   ? 1 - u
				                                                         : v;
				glMultiTexCoord2f(GL_TEXTURE0, u, v);
				glMultiTexCoord2f(GL_TEXTURE1, ou, ov);
				glMultiTexCoord2f(GL_TEXTURE2, ou, ov);
				glVertex2f(u * 2 - 1, v * 2 - 1);
			}
			glEnd();
			std::array<unsigned char, 8 * 8 * 4> reference{};
			glReadPixels(0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, reference.data());
			for (size_t i = 0; i < reference.size(); ++i)
				if (std::abs(int(reference[i]) - int(production[i])) > 1) {
					GLfloat alpha_ref = 0;
					glGetFloatv(GL_ALPHA_TEST_REF, &alpha_ref);
					std::cerr << "GL " << glGetString(GL_VERSION) << "; alpha enabled=" << int(glIsEnabled(GL_ALPHA_TEST))
					          << " ref=" << alpha_ref << " reference alpha=" << unsigned(reference[(i / 4) * 4 + 3]) << '\n';
					throw std::runtime_error("Production/source GPU mismatch: orientation=" + std::to_string(orientation) +
					                         " blend=" + std::to_string(blend) + " pixel=" + std::to_string(i / 4) +
					                         " channel=" + std::to_string(i % 4) + " got=" + std::to_string(production[i]) +
					                         " source=" + std::to_string(reference[i]));
				}
			check(glGetError() == GL_NO_ERROR, "Pinned XL shader comparison raised a GL error");
			++cases;
			pixels += 64;
		}
	glUseProgram(0);
	glDeleteProgram(program);
	glDeleteTextures(1, &mask);
	glDisable(GL_ALPHA_TEST);
}
#endif
void controls()
{
	Bitmap base(8, 8, true), overlay(8, 8, true), stock_base(8, 8, false), stock_overlay(8, 8, false);
	const unsigned char alpha[] = { 0, 1, 2, 5, 64, 128, 224, 255 };
	for (int y = 0; y < 8; ++y)
		for (int x = 0; x < 8; ++x) {
			const int i = (y * 8 + x) * 4;
			base.data[i] = 21 + x * 20;
			base.data[i + 1] = 161;
			base.data[i + 2] = 37;
			base.data[i + 3] = alpha[y];
			overlay.data[i] = 200;
			overlay.data[i + 1] = 30 + y * 10;
			overlay.data[i + 2] = 73;
			overlay.data[i + 3] = alpha[x];
			if (x == 3 && y == 2) {
				overlay.data[i] = 120;
				overlay.data[i + 1] = 88;
				overlay.data[i + 2] = 128;
			}
			stock_base.data[y * 8 + x] = y == 0 ? 255 : 30;
			stock_overlay.data[y * 8 + x] = x == 0 ? 254 : x < 4 ? 255
			                                                     : 45;
		}
	base.normalized();
	overlay.normalized();
	stock_base.bitmap.bm_flags = BM_FLAG_TRANSPARENT;
	stock_overlay.bitmap.bm_flags = BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT;
	const auto stock_before = draw(stock_base, stock_overlay, 0, false);
	for (int context = 0; context < 2; ++context) {
		upload(base, 0);
		upload(overlay, 0);
		for (int orientation = 0; orientation < 4; ++orientation)
			for (int blend = 0; blend < 2; ++blend) {
				composition(base, overlay, orientation, blend != 0);
				composition(stock_base, overlay, orientation, blend != 0);
				composition(base, stock_overlay, orientation, blend != 0);
				composition(stock_base, stock_overlay, orientation, blend != 0);
			}
		check(draw(stock_base, stock_overlay, 0, false) == stock_before, "RGBA drawing changed subsequent Redux pixels");
#ifdef XL_POG_SHADER_REFERENCE
		source_comparison(base, overlay);
		const auto previous = overlay.data;
		overlay.data[(2 * 8 + 3) * 4] = 119;
		overlay.normalized();
		upload(overlay, 0);
		source_comparison(base, overlay);
		overlay.data = previous;
		overlay.normalized();
#else
		throw std::runtime_error("GPU qualification requires the pinned XL shader source");
#endif
		ogl_freebmtexture(&base.bitmap);
		ogl_freebmtexture(&overlay.bitmap);
	}
	Bitmap rectangular(7, 5, true);
	for (size_t i = 0; i < rectangular.data.size(); ++i) rectangular.data[i] = static_cast<unsigned char>(i * 11 + 37);
	rectangular.normalized();
	upload(rectangular, 0);
	upload(rectangular, 2);
	Bitmap large(1024, 1024, true);
	for (size_t i = 0; i < large.data.size(); ++i) large.data[i] = static_cast<unsigned char>(i * 13 + 61);
	large.normalized();
	upload(large, 0);
	upload(large, 2);
	check(ogl_xl_rgba_prepare(ogl_max_texture_size + 1, 1), "GPU texture size overflow was admitted");
}

void original(const char *path)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	check(bool(input), "Cannot open original POG archive for GPU controls");
	const auto length = input.tellg();
	const auto read = [&](uint64_t offset, unsigned char *out, size_t size) {
		input.seekg(static_cast<std::streamoff>(offset));
		input.read(reinterpret_cast<char *>(out), static_cast<std::streamsize>(size));
		check(bool(input), "Cannot read original POG GPU resources");
	};
	Bitmap background(8, 8, true);
	for (size_t i = 0; i < background.data.size(); i += 4) {
		background.data[i] = 61;
		background.data[i + 1] = 87;
		background.data[i + 2] = 159;
		background.data[i + 3] = 255;
	}
	background.normalized();
	unsigned count = 0, resources = 0;
	for (const auto &entry : xl_compat::index_hog(static_cast<uint64_t>(length), read)) {
		const auto name = xl_compat::hog_key(entry.name);
		if (name.size() < 4 || name.substr(name.size() - 4) != ".pog") continue;
		check(entry.size <= xl_compat::pog_file_limit, "Original POG exceeds resource bound");
		std::vector<unsigned char> bytes(static_cast<size_t>(entry.size));
		read(entry.offset, bytes.data(), bytes.size());
		for (auto &image : xl_compat::read_pog(bytes.data(), bytes.size(), 65536)) {
			check(image.truecolor && image.frames == 1, "Original GPU fixture changed format/frame count");
			Bitmap texture(int(image.width), int(image.height), true);
			texture.data = std::move(image.pixels);
			texture.bitmap.bm_data = texture.data.data();
			texture.bitmap.bm_flags = image.flags & 7;
			upload(texture, 2);
			upload(texture, 0);
#ifdef XL_POG_SHADER_REFERENCE
			source_comparison(background, texture);
			source_comparison(texture, background);
#else
			throw std::runtime_error("Original GPU comparison requires pinned XL shaders");
#endif
			++count;
		}
		++resources;
	}
	check(resources && count, "Original GPU control had no RGBA POG replacements");
	std::cout << "Original GPU resources: " << resources << " POG files, " << count << " complete uploads/mipmaps and source shader comparisons\n";
}
#include "particle_gpu_controls.h"
} // namespace
int main(int argc, char **argv)
{
	EGLDisplay display = EGL_NO_DISPLAY;
	EGLContext context = EGL_NO_CONTEXT;
	EGLSurface surface = EGL_NO_SURFACE;
	int result = 1;
	try {
		display = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
		check(display != EGL_NO_DISPLAY && eglInitialize(display, nullptr, nullptr), "Cannot initialize surfaceless EGL");
		check(eglBindAPI(EGL_OPENGL_API), "Cannot select desktop OpenGL");
		const EGLint attributes[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE };
		EGLConfig config;
		EGLint count = 0;
		check(eglChooseConfig(display, attributes, &config, 1, &count) && count == 1, "Missing RGBA test framebuffer");
		const EGLint size[] = { EGL_WIDTH, 32, EGL_HEIGHT, 32, EGL_NONE };
		surface = eglCreatePbufferSurface(display, config, size);
		context = eglCreateContext(display, config, EGL_NO_CONTEXT, nullptr);
		check(surface != EGL_NO_SURFACE && context != EGL_NO_CONTEXT && eglMakeCurrent(display, surface, surface, context), "Cannot create RGBA test context");
		const GLenum glew = glewInit();
		check((glew == GLEW_OK || glew == GLEW_ERROR_NO_GLX_DISPLAY) && GLEW_VERSION_3_0, "Cannot initialize OpenGL entry points");
		while (glGetError() != GL_NO_ERROR) {}
		gl_initialized = 1;
		grs_canvas canvas{};
		canvas.cv_fade_level = GR_FADE_OFF;
		canvas.cv_blend_func = GR_BLEND_NORMAL;
		grd_curcanv = &canvas;
		for (int i = 0; i < 768; ++i) gr_palette[i] = gr_current_pal[i] = i % 64;
		ogl_init_texture_list_internal();
		ogl_init_pixel_buffers(32, 32);
		ogl_init_prog();
		if (argc == 1) {
			controls();
			particle_gpu_controls();
			particle_context_controls([&] {
				ogl_done_prog();
				ogl_close_pixel_buffers();
				check(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT), "Cannot release old particle context");
				check(eglDestroyContext(display, context), "Cannot destroy old particle context");
				context = eglCreateContext(display, config, EGL_NO_CONTEXT, nullptr);
				check(context != EGL_NO_CONTEXT && eglMakeCurrent(display, surface, surface, context), "Cannot replace particle context");
				ogl_init_texture_list_internal();
				ogl_init_pixel_buffers(32, 32);
				ogl_init_prog();
			});
		} else
			for (int i = 1; i < argc; ++i) original(argv[i]);
		ogl_done_prog();
		ogl_close_pixel_buffers();
		std::cout << "Production RGBA upload/draw: " << cases << " cases, " << pixels << " pixels, max analytical error " << maximum_error
		          << (argc == 1 ? "; indexed Redux pixels unchanged\n" : "; original source shader comparisons passed\n");
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
