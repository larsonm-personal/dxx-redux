#include <EGL/egl.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <dlfcn.h>
#include <GLES2/gl2.h>
#include "SDL.h"

namespace
{
EGLDisplay display = EGL_NO_DISPLAY;
EGLContext context = EGL_NO_CONTEXT;
EGLSurface surface = EGL_NO_SURFACE;
EGLConfig config;
int width, height;
std::vector<unsigned char> presented;
int presented_width, presented_height;
unsigned swaps, presented_swap;

void sample_present()
{
	if (!std::getenv("XL_REFERENCE_FRAME_PATH")) return;
	static void *gles = dlopen("libGLESv2.so", RTLD_NOW | RTLD_LOCAL);
	if (!gles) std::abort();
	static auto binding = reinterpret_cast<decltype(&glGetIntegerv)>(dlsym(gles, "glGetIntegerv"));
	static auto bind = reinterpret_cast<decltype(&glBindFramebuffer)>(dlsym(gles, "glBindFramebuffer"));
	static auto read_pixels = reinterpret_cast<decltype(&glReadPixels)>(dlsym(gles, "glReadPixels"));
	static auto get_error = reinterpret_cast<decltype(&glGetError)>(dlsym(gles, "glGetError"));
	if (!binding || !bind || !read_pixels || !get_error || width <= 0 || height <= 0) std::abort();
	GLint previous;
	binding(GL_FRAMEBUFFER_BINDING, &previous);
	bind(GL_FRAMEBUFFER, 0);
	presented.resize(size_t(width) * height * 4);
	read_pixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, presented.data());
	bind(GL_FRAMEBUFFER, previous);
	if (get_error() != GL_NO_ERROR) std::abort();
	presented_width = width;
	presented_height = height;
	presented_swap = swaps;
}
void size(int *w, int *h)
{
	*w = width;
	*h = height;
}
} // namespace
extern "C" {
void *gl4es_GetProcAddress(const char *);
void set_getmainfbsize(void (*)(int *, int *));
SDL_Surface *__real_SDL_SetVideoMode(int, int, int, Uint32);
int __real_SDL_VideoModeOK(int, int, int, Uint32);
__eglMustCastToProperFunctionPointerType __real_eglGetProcAddress(const char *);
__eglMustCastToProperFunctionPointerType __wrap_eglGetProcAddress(const char *name)
{
	if (!strncmp(name, "gl", 2)) return reinterpret_cast<__eglMustCastToProperFunctionPointerType>(gl4es_GetProcAddress(name));
	return __real_eglGetProcAddress(name);
}
void *__wrap_SDL_GL_GetProcAddress(const char *name)
{
	return gl4es_GetProcAddress(name);
}
SDL_Rect **__wrap_SDL_ListModes(SDL_PixelFormat *, Uint32 flags)
{
	static SDL_Rect candidates[] = { { 0, 0, 640, 480 }, { 0, 0, 800, 600 }, { 0, 0, 1024, 768 }, { 0, 0, 1280, 720 }, { 0, 0, 1920, 1080 } };
	static SDL_Rect *supported[6] = {};
	EGLDisplay probe = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	EGLConfig cfg;
	EGLint count = 0, w = 0, h = 0;
	const EGLint attrs[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE };
	if (eglInitialize(probe, nullptr, nullptr) && eglChooseConfig(probe, attrs, &cfg, 1, &count) && count) {
		eglGetConfigAttrib(probe, cfg, EGL_MAX_PBUFFER_WIDTH, &w);
		eglGetConfigAttrib(probe, cfg, EGL_MAX_PBUFFER_HEIGHT, &h);
	}
	int i = 0;
	for (auto &candidate : candidates)
		if (candidate.w <= w && candidate.h <= h) supported[i++] = &candidate;
	supported[i] = nullptr;
	std::fprintf(stderr, "Android XL mode enumeration flags=%#x pbuffer=%dx%d modes=%d\n", flags, w, h, i);
	return supported;
}
int __wrap_SDL_VideoModeOK(int w, int h, int bpp, Uint32)
{
	return __real_SDL_VideoModeOK(w, h, bpp, SDL_SWSURFACE);
}
SDL_Surface *__wrap_SDL_SetVideoMode(int w, int h, int bpp, Uint32)
{
	std::fprintf(stderr, "Android XL requested drawable %dx%dx%d\n", w, h, bpp);
	auto *frame = __real_SDL_SetVideoMode(w, h, bpp, SDL_SWSURFACE);
	if (!frame) return nullptr;
	if (display == EGL_NO_DISPLAY) {
		display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		EGLint count = 0;
		const EGLint attrs[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8, EGL_NONE };
		const EGLint ctx[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
		if (!eglInitialize(display, nullptr, nullptr) || !eglChooseConfig(display, attrs, &config, 1, &count) || count != 1 || (context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctx)) == EGL_NO_CONTEXT) {
			std::fprintf(stderr, "EGL context init error %#x\n", eglGetError());
			return nullptr;
		}
	}
	const EGLint dims[] = { EGL_WIDTH, w, EGL_HEIGHT, h, EGL_NONE };
	auto replacement = eglCreatePbufferSurface(display, config, dims);
	if (replacement == EGL_NO_SURFACE || !eglMakeCurrent(display, replacement, replacement, context)) {
		std::fprintf(stderr, "EGL surface error %#x\n", eglGetError());
		return nullptr;
	}
	if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
	surface = replacement;
	width = w;
	height = h;
	set_getmainfbsize(size);
	std::fprintf(stderr, "Android XL EGL drawable %dx%d\n", w, h);
	return frame;
}
void __wrap_SDL_GL_SwapBuffers()
{

	++swaps;
	sample_present();
	if (swaps == 1 || swaps % 120 == 0) std::fprintf(stderr, "Android XL EGL swap %u\n", swaps);
	if (!eglSwapBuffers(display, surface)) std::fprintf(stderr, "EGL swap error %#x\n", eglGetError());
}
}

// The observer calls this after its last simulation frame. Capture the latest
// presented default framebuffer before EGL can discard the surface contents.
void xl_reference_write_presented()
{
	const char *path = std::getenv("XL_REFERENCE_FRAME_PATH");
	if (!path) return;
	if (presented.empty()) std::abort();
	auto *file = std::fopen(path, "wb");
	if (!file) std::abort();
	std::fprintf(file, "P6\n%d %d\n255\n", presented_width, presented_height);
	std::vector<unsigned char> row(size_t(presented_width) * 3);
	for (int y = presented_height - 1; y >= 0; --y) {
		for (int x = 0; x < presented_width; ++x)
			std::memcpy(&row[x * 3], &presented[(size_t(y) * presented_width + x) * 4], 3);
		if (std::fwrite(row.data(), 1, row.size(), file) != row.size()) std::abort();
	}
	if (std::fclose(file)) std::abort();
	std::fprintf(stderr, "Captured presented swap %u at %dx%d\n", presented_swap, presented_width, presented_height);
}

// CloseGame has already released the original texture and framebuffer owners.
void xl_reference_release_drawable()
{
	if (display == EGL_NO_DISPLAY) return;
	if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) ||
	    !eglDestroySurface(display, surface) || !eglDestroyContext(display, context) || !eglTerminate(display))
		std::abort();
	surface = EGL_NO_SURFACE;
	context = EGL_NO_CONTEXT;
	display = EGL_NO_DISPLAY;
}
