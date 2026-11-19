#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
int main()
{
	EGLDisplay d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	EGLConfig config;
	EGLint n = 0;
	const EGLint attrs[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE };
	if (!eglInitialize(d, nullptr, nullptr) || !eglChooseConfig(d, attrs, &config, 1, &n) || n != 1) return 2;
	const EGLint size[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE }, version[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	EGLSurface s = eglCreatePbufferSurface(d, config, size);
	EGLContext c = eglCreateContext(d, config, EGL_NO_CONTEXT, version);
	if (!eglMakeCurrent(d, s, s, c)) return 3;
	void *lib = dlopen("./libGL4ES.so", RTLD_NOW | RTLD_LOCAL);
	if (!lib) {
		puts(dlerror());
		return 4;
	}
	auto fn = (void *(*) (const char *) ) dlsym(lib, "gl4es_GetProcAddress");
	if (!fn) return 5;
	printf("EGL context preserved after load %d\n", eglGetCurrentContext() == c);
	if (!eglMakeCurrent(d, s, s, c)) return 6;
	((void (*)(int, int, int, int)) fn("glViewport"))(0, 0, 16, 16);
	((void (*)(float, float, float, float)) fn("glClearColor"))(0, 0, 1, 1);
	((void (*)(unsigned)) fn("glClear"))(GL_COLOR_BUFFER_BIT);
	((void (*)(unsigned)) fn("glBegin"))(GL_TRIANGLES);
	((void (*)(float, float, float, float)) fn("glColor4f"))(1, 0, 0, 1);
	auto vertex = (void (*)(float, float)) fn("glVertex2f");
	vertex(-1, -1);
	vertex(1, -1);
	vertex(-1, 1);
	((void (*)()) fn("glEnd"))();
	((void (*)()) fn("glFinish"))();
	unsigned char inside[4] = {}, outside[4] = {};
	glReadPixels(2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, inside);
	glReadPixels(13, 13, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, outside);
	unsigned err = glGetError();
	printf("triangle %u %u %u; background %u %u %u; GLES error %u\n", inside[0], inside[1], inside[2], outside[0], outside[1], outside[2], err);
	fflush(nullptr);
	std::_Exit(inside[0] == 255 && inside[1] == 0 && inside[2] == 0 && outside[0] == 0 && outside[1] == 0 && outside[2] == 255 && !err ? 0 : 1);
}
