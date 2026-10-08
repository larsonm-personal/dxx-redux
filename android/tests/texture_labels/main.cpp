#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <nlohmann/json.hpp>
#include <EGL/egl.h>
#include "input_demo_codec.h"
extern "C" {
#include "args.h"
#include "config.h"
#include "dxxerror.h"
#include "gamefont.h"
#include "ogl_init.h"
#include "palette.h"
#include "physfs.h"
#include "playsave.h"
#include "segpoint.h"
#include "textures.h"
#include "texmerge.h"
#include "u_mem.h"
#include "android_render_fov.h"
#include "android_texture_debug.h"
#include "debug_tex_overlay.h"
#include "merged_wall_debug.h"
#include "ogl_texture_android.h"
void ogl_init_texture_list_internal(void);
void ogl_init_pixel_buffers(int, int);
void ogl_init_prog(void);
void ogl_init_font(grs_font *);
void ogl_smash_texture_list_internal(void);
void ogl_close_pixel_buffers(void);
void ogl_bindbmtex(grs_bitmap *);
extern int r_tpolyc;
extern int GL_TEXTURE_2D_enabled, GL_texclamp_enabled;
#ifdef DXX_BUILD_DESCENT_II
void render_face(int, int, int, int *, int, int, uvl *, int);
#else
void render_face(int, int, int, int *, int, int, uvl *, vms_vector *);
#endif
}
static void require(bool value, const char *message)
{
	if (!value) {
		std::fprintf(stderr, "FAIL: %s (EGL 0x%x, GL 0x%x)\n", message, eglGetError(), glGetError());
		std::exit(1);
	}
}
static std::string readback_hash()
{
	std::vector<unsigned char> rgba(320 * 240 * 4);
	glFinish();
	glReadPixels(0, 0, 320, 240, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
	require(glGetError() == GL_NO_ERROR, "read framebuffer");
	std::string hash, error;
	require(input_demo_sha256_hex(rgba.data(), rgba.size(), &hash, &error), "hash exact RGBA readback");
	return hash;
}
#include "../ogl_batches_fixture.hpp"
#include "../merged_wrap_fixture.hpp"
#include "../merge_cache_fixture.hpp"
#include "../texture_bindings_fixture.hpp"
int main(int argc, char **argv)
{
	if (argc < 2 || argc > 3) return 1;
	const bool mipmaps = argc == 3 && !std::strncmp(argv[2], "mipmaps", 7);
	const bool bindings = argc == 3 && !std::strncmp(argv[2], "bindings", 8);
	const bool merge_cache = argc == 3 && std::strcmp(argv[2], "merge-cache") == 0;
	const bool merged_wrap = argc == 3 && !std::strncmp(argv[2], "merged-wrap", 11);
	const bool batches = argc == 3 && !std::strncmp(argv[2], "batches", 7);
	const bool baseline = argc == 3 && (std::strcmp(argv[2], "batches-baseline") == 0 || std::strcmp(argv[2], "merged-wrap-baseline") == 0 || std::strcmp(argv[2], "bindings-baseline") == 0 || std::strcmp(argv[2], "mipmaps-baseline") == 0);
	mem_init();
	error_init([](const char *message) { std::fprintf(stderr, "%s\n", message); });
	require(PHYSFS_init(nullptr) && PHYSFS_setWriteDir(".") && PHYSFS_mount(".", nullptr, 1), "isolated filesystem");
	EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	require(eglInitialize(display, nullptr, nullptr), "initialize EGL");
	const EGLint attributes[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		                          EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE };
	EGLConfig config;
	EGLint count;
	require(eglChooseConfig(display, attributes, &config, 1, &count) && count == 1, "pbuffer config");
	const EGLint dimensions[] = { EGL_WIDTH, 320, EGL_HEIGHT, 240, EGL_NONE };
	const EGLint version[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	nlohmann::json results = nlohmann::json::array();
	for (int generation = 0; generation < 2; ++generation) {
		EGLSurface surface = eglCreatePbufferSurface(display, config, dimensions);
		EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, version);
		require(surface != EGL_NO_SURFACE && context != EGL_NO_CONTEXT && eglMakeCurrent(display, surface, surface, context), "current offscreen context");
#ifdef GLES3_SHIM_TEXTURE_UNIT_COUNT
		if (bindings && !baseline) glActiveTexture(GL_TEXTURE2);
#endif
		gles3_shim_init();
#ifdef GLES3_SHIM_TEXTURE_UNIT_COUNT
		if (bindings && !baseline) {
			require(!gles3_shim_bind_texture_2d_cached(0) && gles3_shim_bind_texture_2d_cached(0), "context init discovers actual active unit");
			glActiveTexture(GL_TEXTURE0);
			require(!gles3_shim_bind_texture_2d_cached(0), "context init leaves other units unknown");
			glActiveTexture(GL_TEXTURE2);
			require(gles3_shim_bind_texture_2d_cached(0), "context init tracks nonzero unit independently");
			glActiveTexture(GL_TEXTURE0);
			results.push_back({ { "context", generation }, { "kind", "lifecycle" }, { "correct", true } });
		}
#endif
		// This fixture owns EGL directly, so invalidate the native context state caches
		GL_TEXTURE_2D_enabled = GL_texclamp_enabled = -1;
		ogl_init_prog();
		ogl_init_texture_list_internal();
		ogl_init_pixel_buffers(320, 240);
		grs_screen screen{};
		screen.sc_w = 320;
		screen.sc_h = 240;
		screen.sc_aspect = (4 * F1_0) / 3;
		gr_init_canvas(&screen.sc_canvas, nullptr, BM_OGL, 320, 240);
		screen.sc_canvas.cv_fade_level = GR_FADE_OFF;
		grd_curscreen = &screen;
		gr_set_current_canvas(&screen.sc_canvas);
		for (int i = 0; i < 256; ++i) {
			gr_palette[i * 3] = i % 64;
			gr_palette[i * 3 + 1] = (i * 3) % 64;
			gr_palette[i * 3 + 2] = (i * 7) % 64;
		}
		grs_font font{};
		unsigned char glyphs[95 * 7];
		for (int i = 0; i < 95 * 7; ++i) glyphs[i] = static_cast<unsigned char>((i % 7 == 0 || i % 7 == 6) ? 0xf8 : 0x88);
		font.ft_w = 5;
		font.ft_h = 7;
		font.ft_minchar = 32;
		font.ft_maxchar = 126;
		font.ft_bytewidth = 1;
		font.ft_data = glyphs;
		ogl_init_font(&font);
		Gamefonts[GFONT_SMALL] = &font;
		FNTScaleX = FNTScaleY = 1;
		unsigned char pixels[2][64 * 64];
		for (int texture = 0; texture < 2; ++texture) {
			for (int i = 0; i < 64 * 64; ++i) pixels[texture][i] = static_cast<unsigned char>(texture ? (i % 3 ? 255 : 30) : 12);
			gr_init_bitmap(&GameBitmaps[texture + 1], BM_LINEAR, 0, 0, 64, 64, 64, pixels[texture]);
			std::strcpy(piggy_game_bitmap_name(&GameBitmaps[texture + 1]), texture ? "overlay" : "base");
			Textures[texture].index = texture + 1;
		}
		NumTextures = 2;
		texmerge_init(2);
		GameCfg.TexFilt = GameCfg.MenuTexFilt = GameCfg.HudTexFilt = 0;
		GameCfg.ClassicDepth = 1;
		g_debug_tex_overlay_active = 1;
		if (mipmaps) test_xmodel_mipmaps(results, generation, baseline);
		else if (bindings) {
			gr_set_curfont(&font);
			test_texture_bindings(results, generation, baseline);
		} else if (merge_cache) test_merge_cache(results, generation);
		else if (merged_wrap) test_merged_wrap(results, generation, baseline);
		else if (batches) test_ogl_batches(results, generation, font, baseline);
		else {
			for (int fov : { 0, 100, 110, 120 })
				for (int path = 0; path < 3; ++path)
					for (int scenario = 0; scenario < 18; ++scenario) {
						// Valid native texture mappings always have a name pointer, including blank names
						std::strcpy(piggy_game_bitmap_name(&GameBitmaps[1]), (scenario == 7 || scenario == 9) ? "" : "base");
						std::strcpy(piggy_game_bitmap_name(&GameBitmaps[2]), (scenario == 8 || scenario == 9) ? "" : "overlay");
						int initial = scenario == 4 ? 254 : scenario == 5 ? 255
						                                : scenario == 6   ? 256
						                                                  : 0;
						std::memset(g_debug_tex_labels, 0, sizeof(g_debug_tex_labels));
						g_debug_tex_label_count = initial;
						GameArg.DbgAltTexMerge = path == 1;
						android_render_set_main_view_fov(fov);
						g3_start_frame();
						glClearColor(0.05f, 0.05f, 0.05f, 1);
						glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
						glDisable(GL_CULL_FACE);
						vms_vector view_position{};
						vms_matrix view_orientation = vmd_identity_matrix;
						g3_set_view_matrix(&view_position, &view_orientation, android_render_main_view_zoom(F1_0));
						int vertices[] = { 0, 1, 2, 3 };
						uvl uv[4]{};
						for (int i = 0; i < 4; ++i) {
							auto &p = Segment_points[i];
							p = {};
							vms_vector world{ (i == 0 || i == 3) ? -F1_0 : F1_0, i < 2 ? F1_0 : -F1_0, 4 * F1_0 };
							g3_rotate_point(&p, &world);
							g3_project_point(&p);
							int mask = scenario == 1 ? 0 : scenario == 2 ? 1
							                           : scenario == 3   ? 5
							                                             : 15;
							if (!(mask & (1 << i))) p.p3_flags &= ~PF_PROJECTED;
							if (scenario == 2 || scenario == 3) {
								p.p3_sx = (10 + i) * F1_0 + 3 * F1_0 / 4;
								p.p3_sy = (20 + i * 2) * F1_0 + 3 * F1_0 / 4;
							}
							if (scenario >= 10 && scenario <= 13) p.p3_sx = (scenario == 10 ? -1 : scenario == 11 ? 0
								                                                               : scenario == 12   ? 319
								                                                                                  : 320) *
								                                            F1_0;
							if (scenario == 14 || scenario == 15) p.p3_sy = (scenario == 14 ? 239 : 240) * F1_0;
							uv[i].u = (i == 1 || i == 2) ? F1_0 : 0;
							uv[i].v = i >= 2 ? F1_0 : 0;
							uv[i].l = F1_0;
						}
						g_android_draw_face_ctx = {};
						g_android_draw_face_ctx.valid = 1;
						g_android_draw_face_ctx.seg = 0;
						g_android_draw_face_ctx.side = 2;
						g_android_draw_face_ctx.face = 1;
						g_android_draw_face_ctx.tmap1 = 0;
						g_android_draw_face_ctx.tmap2 = 1;
						if (scenario == 17) g_android_draw_face_ctx.valid = 0;
						// Synthetic resident hires metadata exercises the native non-hires label rule
						for (int i = 1; i <= 2; ++i) {
							ogl_bindbmtex(&GameBitmaps[i]);
							require(GameBitmaps[i].gltexture != nullptr, "resident original texture");
							GameBitmaps[i].gltexture->is_png = scenario == 16;
						}
						r_tpolyc = 0;
#ifdef DXX_BUILD_DESCENT_II
						render_face(0, 2, 4, vertices, 0, path == 2 ? 0 : 1, uv, 0);
#else
						vms_vector normal{ 0, 0, -F1_0 };
						render_face(0, 2, 4, vertices, 0, path == 2 ? 0 : 1, uv, &normal);
#endif
						if (path == 0) {
							bool admitted = initial <= 254 && scenario != 1 && scenario != 10 && scenario != 13 && scenario != 15;
							require(g_debug_tex_label_count == initial + (admitted ? 2 : 0), "merged label admission");
							for (int i = initial; i < g_debug_tex_label_count; ++i) require(g_debug_tex_labels[i].is_hires == 0, "merged output labels stay non-hires");
							if (admitted) require(g_debug_tex_labels[initial + 1].sy == g_debug_tex_labels[initial].sy + 10, "second label retains offset");
							if (scenario == 2 || scenario == 3) require(g_debug_tex_labels[initial].sx == (scenario == 2 ? 10 : 11), "integer conversion precedes averaging");
						}
						std::string scene_hash = readback_hash();
						g3_end_frame();
						android_texture_debug_draw_overlay();
						require(glGetError() == GL_NO_ERROR, "render and overlay GL status");
						nlohmann::json result = { { "context", generation }, { "fov", fov }, { "path", path }, { "scenario", scenario }, { "initial", initial }, { "count", g_debug_tex_label_count }, { "labels", nlohmann::json::array() }, { "textured_polygons", r_tpolyc }, { "scene_sha256", scene_hash }, { "overlay_sha256", readback_hash() } };
						if (path == 0 && scenario == 0) require(result["scene_sha256"] != result["overlay_sha256"], "overlay changes rendered pixels");
						for (int i = initial; i < g_debug_tex_label_count; ++i) {
							auto &label = g_debug_tex_labels[i];
							result["labels"].push_back({ { "name", label.name }, { "sx", label.sx }, { "sy", label.sy }, { "hires", label.is_hires }, { "seg", label.seg }, { "side", label.side }, { "face", label.face }, { "anchor_group", label.anchor_group }, { "anchor_samples", label.anchor_samples } });
						}
						results.push_back(result);
					}
		}
		texmerge_close();
		grs_bitmap font_bitmap = font.ft_parent_bitmap;
		gr_free_bitmap_data(&font_bitmap);
		d_free(font.ft_bitmaps);
		ogl_smash_texture_list_internal();
		gles3_shim_shutdown();
		require(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT), "release context");
		require(eglDestroyContext(display, context) && eglDestroySurface(display, surface), "close EGL context");
	}
	ogl_close_pixel_buffers();
	require(eglTerminate(display), "terminate EGL display");
	FILE *trace = std::fopen(argv[1], "wb");
	std::string text = results.dump(2) + "\n";
	require(trace && std::fwrite(text.data(), 1, text.size(), trace) == text.size(), "write trace");
	std::fclose(trace);
	PHYSFS_deinit();
	std::printf("Texture label rendering passed: %zu cases\n", results.size());
	return 0;
}
