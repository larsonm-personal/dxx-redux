#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>
#include <nlohmann/json.hpp>
#include <EGL/egl.h>
extern "C" {
#include "ogl_init.h"
#include "ogl_texture_android.h"
#include "android_mission_assets.h"
#include "pngfile.h"
#include "piggy.h"
#include "physfs.h"
#include "u_mem.h"
void ogl_init_texture_list_internal(void);
void ogl_smash_texture_list_internal(void);
void ogl_init_pixel_buffers(int, int);
void ogl_close_pixel_buffers(void);
int ogl_loadtexture(unsigned char *, int, int, ogl_texture *, int, int, int, const char *);
}
// The sampling shader uses gl_VertexID, not the legacy client-array shim.
#undef glDrawArrays
static unsigned resets;
// This isolated renderer has no game definitions. Use real GPU/lookup invalidation,
// with a baseline spy instead of attempting to load retail data into the fixture.
extern "C" void android_mission_asset_reset_before(void)
{
	++resets;
	ogl_smash_texture_list_internal();
	clear_texture_lookup_cache();
}
extern "C" void android_mission_asset_reset_baseline(void) {}
static void require(bool value, const char *message)
{
	if (!value) {
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}
static void image(const char *path, unsigned char red, unsigned char alpha = 255, unsigned char green = 40, unsigned char blue = 30, unsigned dimension = 4, unsigned height = 0)
{
	std::filesystem::create_directories(std::filesystem::path(path).parent_path());
	if (!height) height = dimension;
	std::vector<unsigned char> bytes(18 + dimension * height * 4);
	bytes[2] = 2;
	bytes[12] = dimension;
	bytes[14] = height;
	bytes[16] = 32;
	bytes[17] = 8;
	for (size_t p = 18; p < bytes.size(); p += 4) {
		bytes[p] = blue;
		bytes[p + 1] = green;
		bytes[p + 2] = red;
		bytes[p + 3] = alpha;
	}
	std::ofstream out(path, std::ios::binary);
	out.write(reinterpret_cast<char *>(bytes.data()), bytes.size());
	require(bool(out), "write synthetic TGA");
}
static GLuint shader(GLenum type, const char *text)
{
	GLuint result = glCreateShader(type);
	glShaderSource(result, 1, &text, nullptr);
	glCompileShader(result);
	GLint compiled = 0;
	glGetShaderiv(result, GL_COMPILE_STATUS, &compiled);
	require(compiled, "compile surface shader");
	return result;
}

#include "masks_fixture.hpp"

int main(int argc, char **argv)
{
	if (argc != 2) return 1;
	mem_init();
	require(PHYSFS_init(nullptr) && PHYSFS_setWriteDir(".") && PHYSFS_mount(".", nullptr, 1), "isolated files");
	image("a/mods/pilot/textures/CEIL002.TGA", 210);
#ifdef DXX_BUILD_DESCENT_II
	mask_pattern();
#endif
	const bool original = std::filesystem::is_regular_file("ceil002.tga");
	if (original) std::filesystem::copy_file("ceil002.tga", "a/mods/pilot/textures/CEIL002.TGA", std::filesystem::copy_options::overwrite_existing);
	image("b/mods/pilot/textures/CEIL002.TGA", 110);
	image("global/ceil002.tga", 10);
	image("a/mods/pilot/textures/translucent.tga", 230, 100);
	image("global/translucent.tga", 10);
	image("a/mods/pilot/textures/keyed.tga", 120, 255, 88, 128);
	image("global/keyed.tga", 10);
	image("a/mods/pilot/textures/mixed.tga", 210, 255, 40, 30, 8);
	{
		std::fstream mixed("a/mods/pilot/textures/mixed.tga", std::ios::in | std::ios::out | std::ios::binary);
		const unsigned char key[] = { 128, 88, 120, 255 };
		mixed.seekp(18);
		mixed.write(reinterpret_cast<const char *>(key), sizeof(key));
		require(bool(mixed), "write single transparency key among opaque neighbors");
	}
	std::filesystem::copy_file("a/mods/pilot/textures/mixed.tga", "global/mixed-source.tga", std::filesystem::copy_options::overwrite_existing);
	image("global/mixed.tga", 10);
	image("a/mods/pilot/textures/fitted.tga", 210, 255, 40, 30, 8);
	image("global/fitted.tga", 10);
	image("a/mods/pilot/textures/strip.tga", 210, 255, 40, 30, 4, 8);
	image("global/strip.tga", 10);
	image("global/broken.tga", 10);
	std::ofstream("a/mods/pilot/textures/broken.tga") << "truncated";
	std::ofstream("a/mods/pilot/textures/ceil002.bmp") << "not a selected format";
	std::ofstream("a/test.mn2") << "name = A\n";
	std::ofstream("b/test.mn2") << "name = B\n";
	nlohmann::json entries = nlohmann::json::array();
	for (const char *owner : { "a", "b" }) entries.push_back({ { "alias", std::string("missions/_packs/") + owner + "/test.mn2" }, { "descriptor", "test.mn2" }, { "key", owner }, { "owner", owner }, { "revision", "synthetic" }, { "mounts", { std::filesystem::absolute(owner).string() } } });
	std::ofstream(".mission_assets.json") << nlohmann::json({ { "schema", 1 }, { "entries", entries } });
	require(PHYSFS_mount("global", nullptr, 1), "global control mount");
	android_mission_assets_init();
	EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	require(eglInitialize(display, nullptr, nullptr), "initialize EGL");
	const EGLint attributes[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE };
	EGLConfig config;
	EGLint count;
	require(eglChooseConfig(display, attributes, &config, 1, &count) && count == 1, "pbuffer config");
	const EGLint dimensions[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE }, version[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	nlohmann::json results = nlohmann::json::array();
	for (int generation = 0; generation < 2; ++generation) {
		EGLSurface surface = eglCreatePbufferSurface(display, config, dimensions);
		EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, version);
		require(eglMakeCurrent(display, surface, surface, context), "render context");
		gles3_shim_init();
		ogl_init_texture_list_internal();
		ogl_init_pixel_buffers(64, 64);
#ifdef DXX_BUILD_DESCENT_II
		ogl_init_prog();
#endif
		GLuint vertex = shader(GL_VERTEX_SHADER, "#version 300 es\nout vec2 uv;void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=p;gl_Position=vec4(p*2.0-1.0,0,1);}");
		GLuint fragment = shader(GL_FRAGMENT_SHADER, "#version 300 es\nprecision highp float;in vec2 uv;uniform sampler2D source;out vec4 color;void main(){color=texture(source,uv);}");
		GLuint program = glCreateProgram(), vao;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glAttachShader(program, vertex);
		glAttachShader(program, fragment);
		glLinkProgram(program);
		GLint linked = 0;
		glGetProgramiv(program, GL_LINK_STATUS, &linked);
		require(linked, "link surface shader");
		GLuint previous = 0;
		for (const char *owner : { "a", "b", "stock", "a" }) {
			if (!std::strcmp(owner, "stock")) {
				android_mission_assets_free_begin();
				android_mission_assets_free_end();
			} else {
				char descriptor[256];
				require(android_mission_assets_prepare((std::string("missions/_packs/") + owner + "/test.mn2").c_str(), descriptor, sizeof(descriptor)) && android_mission_assets_activate(), "activate real mission context");
			}
			require(resets > 0 && (!previous || !glIsTexture(previous)), "context change clears GPU replacement");
			grs_bitmap bitmap{};
			const char *names[] = { nullptr, nullptr, "ceil002" };
			const int slots[] = { 0, 1, 2 };
			png_data png{};
			etc2_file_data ktx{};
			android_profile_texture_lookup_metrics metrics{};
			long long ktx_us = 0, png_us = 0;
			char path[256];
#ifdef DXX_BUILD_DESCENT_II
			const char *hog = "missions/PILOT.HOG";
			const bool use_original = original && !std::strcmp(owner, "a");
			const int red = !std::strcmp(owner, "a") ? (use_original ? 228 : 210) : !std::strcmp(owner, "b") ? 110
			                                                                                                 : 10;
#else
			const char *hog = nullptr;
			const bool use_original = false;
			const int red = 10;
#endif
			require(android_ogl_read_replacement(path, sizeof(path), hog, names, slots, 3, 0, &bitmap, &png, &ktx, &metrics, &ktx_us, &png_us) == 1, "production candidate decoder");
			require(png.width == (use_original ? 64u : 4u) && png.height == png.width && png.channels == 4 && png.data[0] == (use_original ? 80 : red), "selected decoded pixels");
			ogl_texture *texture = ogl_get_free_texture();
			ogl_init_texture(texture, png.width, png.height, png.alpha ? OGL_FLAG_ALPHA : 0);
			require(ogl_loadtexture(png.data, 0, 0, texture, 0, png.channels, 0, "ceil002") == 0, "engine texture upload");
			const std::string selected_path(path);
			free(png.data);
			free(png.palette);
			previous = texture->handle;
			glViewport(0, 0, 4, 4);
			glDisable(GL_BLEND);
			gles3_shim_bind_program(program);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, previous);
			glUniform1i(glGetUniformLocation(program, "source"), 0);
			glDrawArrays(GL_TRIANGLES, 0, 3);
			unsigned char pixel[4];
			glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
			require(glGetError() == GL_NO_ERROR && pixel[0] == red && pixel[1] == (use_original ? 220 : 40) && pixel[2] == (use_original ? 220 : 30) && pixel[3] == 255, "actual controlled surface readback");
			png_data foreign{};
			require(!read_png_scoped_opaque("ceil002.tga", &foreign), "scoped decoder revalidates ownership and refuses global source");
			const int previous_limit = ogl_max_texture_size;
			ogl_max_texture_size = 4;
			if (!std::strcmp(owner, "a")) {
				png_data ordinary{};
				require(!read_png("mods/pilot/textures/fitted.tga", &ordinary), "global decoder still excludes mission scope");
				require(read_png("mixed-source.tga", &ordinary) && ordinary.width == 4, "ordinary lookup still fits mixed keyed pixels");
				for (size_t p = 0; p < ordinary.width * ordinary.height; ++p)
					require(!(ordinary.data[p * 4] == 120 && ordinary.data[p * 4 + 1] == 88 && ordinary.data[p * 4 + 2] == 128), "resizing erased source key: regression is meaningful");
				free(ordinary.data);
			}
			for (const char *name : { "broken", "translucent", "fitted", "strip" }) {
				const char *fallback_names[] = { nullptr, nullptr, name };
				png_data fallback{};
				require(android_ogl_read_replacement(path, sizeof(path), hog, fallback_names, slots, 3, 0, &bitmap, &fallback, &ktx, &metrics, &ktx_us, &png_us) == 1 && fallback.width == 4 && fallback.data[0] == (hog && !std::strcmp(owner, "a") && !std::strcmp(name, "fitted") ? 210 : 10), "source transparency rejects before fitting; opaque sources fit normally");
				free(fallback.data);
				free(fallback.palette);
			}
			if (hog && !std::strcmp(owner, "a")) {
				png_data masked{};
				require(read_png_scoped_static("mods/pilot/textures/mixed.tga", &masked), "sparse key classification before fitting");
				require(masked.width == 4 && masked.scoped_super && masked.scoped_mask, "source mask retained with fitted color");
				int holes = 0;
				for (unsigned i = 0; i < 16; ++i) {
					holes += masked.scoped_mask[i] == 255;
					if (masked.scoped_mask[i]) require(masked.data[i * 4 + 3] == 0, "key removes color coverage");
				}
				require(holes == 1, "single sparse key survives reduction without RGB key search");
				free(masked.data);
			}
			ogl_max_texture_size = previous_limit;
#ifdef DXX_BUILD_DESCENT_II
			test_masks(!std::strcmp(owner, "a"));
#endif
			require(!android_ogl_read_replacement(path, sizeof(path), hog, names, slots, 4, 0, &bitmap, &png, &ktx, &metrics, &ktx_us, &png_us), "basename bound preserved");
			results.push_back({ { "context", generation }, { "owner", owner }, { "red", red }, { "path", selected_path }, { "original", use_original }, { "correct", true } });
		}
		android_mission_assets_free_begin();
		android_mission_assets_free_end();
		glDeleteVertexArrays(1, &vao);
#ifdef DXX_BUILD_DESCENT_II
		ogl_done_prog();
#endif
		ogl_close_pixel_buffers();
		glDeleteProgram(program);
		glDeleteShader(vertex);
		glDeleteShader(fragment);
		gles3_shim_shutdown();
		require(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT), "release context");
		eglDestroyContext(display, context);
		eglDestroySurface(display, surface);
	}
	android_mission_assets_shutdown();
	eglTerminate(display);
	PHYSFS_deinit();
	std::ofstream(argv[1]) << results.dump(2) << '\n';
	std::printf("Scoped static textures passed: %zu cases\n", results.size());
}
