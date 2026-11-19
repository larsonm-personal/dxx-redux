// Offscreen window system for replaying WGL traces with apitrace and GL4ES on Android
#include "glws.hpp"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace
{
EGLDisplay display = EGL_NO_DISPLAY;
EGLConfig config;
void require_egl(bool success, const char *operation)
{
	if (!success) {
		std::fprintf(stderr, "%s failed (EGL %#x)\n", operation, eglGetError());
		std::abort();
	}
}
void drawable_size(int *width, int *height)
{
	const auto surface = eglGetCurrentSurface(EGL_DRAW);
	const auto current = eglGetCurrentDisplay();
	require_egl(eglQuerySurface(current, surface, EGL_WIDTH, width) && eglQuerySurface(current, surface, EGL_HEIGHT, height), "query EGL drawable");
}
void *resolve(const char *name)
{
	using GetProc = void *(*) (const char *);
	static const GetProc get = [] {
		const auto current_display = eglGetCurrentDisplay();
		const auto current_context = eglGetCurrentContext();
		const auto draw = eglGetCurrentSurface(EGL_DRAW);
		const auto read = eglGetCurrentSurface(EGL_READ);
		void *library = dlopen("./libGL4ES.so", RTLD_NOW | RTLD_LOCAL);
		if (!library) {
			std::fprintf(stderr, "Opening GL4ES failed: %s\n", dlerror());
			std::abort();
		}
		// GL4ES probes hardware in a temporary context during library initialization
		if (current_context != EGL_NO_CONTEXT)
			require_egl(eglMakeCurrent(current_display, draw, read, current_context), "restore drawable after GL4ES initialization");
		const auto register_size = reinterpret_cast<void (*)(void (*)(int *, int *))>(dlsym(library, "set_getmainfbsize"));
		const auto lookup = reinterpret_cast<GetProc>(dlsym(library, "gl4es_GetProcAddress"));
		if (!register_size || !lookup) std::abort();
		register_size(drawable_size);
		return lookup;
	}();
	return get(name);
}
} // namespace

void *_getPublicProcAddress(const char *name)
{
	return resolve(name);
}
void *_getPrivateProcAddress(const char *name)
{
	return resolve(name);
}

namespace glws
{
class AndroidDrawable final : public Drawable
{
	void allocate()
	{
		const EGLint dimensions[] = { EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE };
		surface = eglCreatePbufferSurface(display, config, dimensions);
		require_egl(surface != EGL_NO_SURFACE, "create pbuffer");
	}

  public:
	EGLSurface surface = EGL_NO_SURFACE;
	AndroidDrawable(const Visual *visual, int width, int height, bool pbuffer) : Drawable(visual, width, height, pbuffer)
	{
		allocate();
	}
	~AndroidDrawable() override
	{
		eglDestroySurface(display, surface);
	}
	void resize(int w, int h) override
	{
		if (width == w && height == h) return;
		const auto previous = surface;
		auto draw = eglGetCurrentSurface(EGL_DRAW);
		auto read = eglGetCurrentSurface(EGL_READ);
		const auto context = eglGetCurrentContext();
		Drawable::resize(w, h);
		allocate();
		if (draw == previous || read == previous)
			require_egl(eglMakeCurrent(display, draw == previous ? surface : draw, read == previous ? surface : read, context), "resize pbuffer");
		eglDestroySurface(display, previous);
	}
	void swapBuffers() override
	{
		require_egl(eglSwapBuffers(display, surface), "swap pbuffer");
	}
};
class AndroidContext final : public Context
{
  public:
	EGLContext context;
	AndroidContext(const Visual *visual, Context *shared) : Context(visual)
	{
		const EGLint attributes[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
		context = eglCreateContext(display, config, shared ? static_cast<AndroidContext *>(shared)->context : EGL_NO_CONTEXT, attributes);
		require_egl(context != EGL_NO_CONTEXT, "create GLES context");
	}
	~AndroidContext() override
	{
		eglDestroyContext(display, context);
	}
};
void init()
{
	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	require_egl(eglInitialize(display, nullptr, nullptr), "initialize EGL");
	require_egl(eglBindAPI(EGL_OPENGL_ES_API), "bind GLES");
	const EGLint attributes[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8, EGL_NONE };
	EGLint count = 0;
	require_egl(eglChooseConfig(display, attributes, &config, 1, &count) && count == 1, "choose RGBA depth/stencil config");
}
void cleanup()
{
	eglTerminate(display);
}
Visual *createVisual(bool double_buffer, unsigned, Profile profile)
{
	auto *visual = new Visual(profile);
	visual->doubleBuffer = double_buffer;
	return visual;
}
Drawable *createDrawable(const Visual *visual, int width, int height, const pbuffer_info *info)
{
	return new AndroidDrawable(visual, width, height, info != nullptr);
}
Context *createContext(const Visual *visual, Context *shared, bool)
{
	return new AndroidContext(visual, shared);
}
bool makeCurrentInternal(Drawable *draw, Drawable *read, Context *context)
{
	return eglMakeCurrent(display, draw ? static_cast<AndroidDrawable *>(draw)->surface : EGL_NO_SURFACE, read ? static_cast<AndroidDrawable *>(read)->surface : EGL_NO_SURFACE, context ? static_cast<AndroidContext *>(context)->context : EGL_NO_CONTEXT);
}
bool processEvents()
{
	return true;
}
bool bindTexImage(Drawable *, int)
{
	return false;
}
bool releaseTexImage(Drawable *, int)
{
	return false;
}
bool setPbufferAttrib(Drawable *, const int *)
{
	return false;
}
} // namespace glws

// Called just before the named WGL swap; stop after writing the selected frame
void xl_capture_swap(unsigned call)
{
	const char *setting = std::getenv("XL_CAPTURE_CALL");
	if (!setting || std::strtoul(setting, nullptr, 10) != call) return;
	reinterpret_cast<void (*)()>(resolve("glFinish"))();
	int width, height;
	drawable_size(&width, &height);
	if (width < 1 || height < 1 || width > 8192 || height > 8192) std::abort();
	GLint framebuffer = -1;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
	std::fprintf(stderr, "Capturing call %u, %d x %d, framebuffer %d, prior GLES error %#x\n", call, width, height, framebuffer, glGetError());
	if (framebuffer != 0) std::abort();
	std::vector<unsigned char> rgba(size_t(width) * height * 4), row(size_t(width) * 3);
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
	if (glGetError() != GL_NO_ERROR) std::abort();
	FILE *file = std::fopen("frame.ppm", "wb");
	if (!file) std::abort();
	std::fprintf(file, "P6\n%d %d\n255\n", width, height);
	for (int y = height - 1; y >= 0; --y) {
		for (int x = 0; x < width; ++x) std::memcpy(&row[x * 3], &rgba[(size_t(y) * width + x) * 4], 3);
		if (std::fwrite(row.data(), 1, row.size(), file) != row.size()) std::abort();
	}
	if (std::fclose(file)) std::abort();
	std::fflush(nullptr);
	std::_Exit(0);
}
