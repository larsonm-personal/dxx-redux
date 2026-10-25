// Production bitmap upload and wall composition, under the fixture's owned context.
#ifdef DXX_BUILD_DESCENT_II
static bool fixture_xl_mission = true;
extern "C" {
#include "3d.h"
#include "config.h"
#include "args.h"
void ogl_loadbmtexture_f(grs_bitmap *, int, const char *);
void ogl_freebmtexture(grs_bitmap *);
void ogl_init_prog(void);
void ogl_done_prog(void);
void ogl_smash_png_textures(void);
#include "game.h"
extern int r_mask_draws;
extern GLuint ogl_prog_tex2, ogl_prog_tex2m;
// Identity injection only: this renderer fixture does not load a retail mission.
const char *mission_resource_archive(void)
{
	return fixture_xl_mission ? "pilot.hog" : "";
}
}
static void mask_pattern()
{
	image("a/mods/pilot/textures/pattern.tga", 210);
	std::fstream f("a/mods/pilot/textures/pattern.tga", std::ios::in | std::ios::out | std::ios::binary);
	f.seekp(17);
	f.put(0x28); // top-left origin, eight alpha bits
	for (int y = 0; y < 4; ++y)
		for (int x = 0; x < 4; ++x) {
			const unsigned char opaque[] = { 30, 40, 210, 255 }, cutout[] = { 99, 88, 77, 0 }, key[] = { 128, 88, 120, 255 };
			f.write(reinterpret_cast<const char *>(y >= 2 && x < 2 ? key : y < 2 && x >= 2 ? cutout
			                                                                               : opaque),
			        4);
		}
	require(bool(f), "write mixed binary/key pattern");
	image("global/pattern.tga", 10);
	image("a/mods/pilot/textures/tinyopaque.tga", 210, 255, 40, 30, 1);
	image("a/mods/pilot/textures/tinycutout.tga", 210, 0, 40, 30, 1);
	image("a/mods/pilot/textures/tinykey.tga", 120, 255, 88, 128, 1);
}
static void mask_quad(grs_bitmap &base, grs_bitmap *overlay, int orient, int extent, int depth)
{
	g3s_point points[4]{};
	const g3s_point *list[4];
	g3s_uvl uv[4]{};
	g3s_lrgb light[4]{};
	for (int i = 0; i < 4; ++i) {
		vms_vector v{ (i == 0 || i == 3) ? -extent : extent, i < 2 ? extent : -extent, depth };
		g3_rotate_point(&points[i], &v);
		g3_project_point(&points[i]);
		list[i] = &points[i];
		uv[i].u = (i == 1 || i == 2) ? F1_0 : 0;
		uv[i].v = i >= 2 ? F1_0 : 0;
		uv[i].l = F1_0;
		light[i] = { F1_0, F1_0, F1_0 };
	}
	if (overlay) g3_draw_tmap_2(4, list, uv, light, &base, overlay, orient);
	else g3_draw_tmap(4, list, uv, light, &base);
}
static void mask_solid(grs_bitmap &bitmap, unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
{
	// Use an unpadded constant surface; 1x1 engine padding has a clear corner.
	unsigned char rgba[] = { r, g, b, a, r, g, b, a, r, g, b, a, r, g, b, a };
	bitmap.bm_w = bitmap.bm_h = 2;
	bitmap.gltexture = ogl_get_free_texture();
	ogl_init_texture(bitmap.gltexture, 2, 2, OGL_FLAG_ALPHA);
	require(!ogl_loadtexture(rgba, 0, 0, bitmap.gltexture, 0, 4, 0, nullptr), "solid layer upload");
}
static void test_masks(bool owned)
{
	for (GLuint program : { ogl_prog_tex2, ogl_prog_tex2m }) {
		GLint linked = 0;
		require(program && glIsProgram(program), "production wall program recreated after asset reset");
		glGetProgramiv(program, GL_LINK_STATUS, &linked);
		require(linked, "production wall program linked");
	}
	GameArg.DbgAltTexMerge = 0;
	grs_screen screen{};
	screen.sc_w = screen.sc_h = 64;
	screen.sc_aspect = F1_0;
	gr_init_canvas(&screen.sc_canvas, nullptr, BM_OGL, 64, 64);
	screen.sc_canvas.cv_fade_level = GR_FADE_OFF;
	grd_curscreen = &screen;
	gr_set_current_canvas(&screen.sc_canvas);
	grs_bitmap base{}, behind{}, overlay{}, empty{}, coverage{};
	mask_solid(empty, 0, 0, 0, 0);
	mask_solid(coverage, 255, 255, 255);
	mask_solid(base, 20, 220, 20);
	mask_solid(behind, 20, 20, 220);
	unsigned char stock[16]{};
	overlay.bm_w = overlay.bm_h = overlay.bm_rowsize = 4;
	overlay.bm_data = stock;
	for (int filter = 0; filter < 3; ++filter) {
		GameCfg.TexFilt = filter;
		ogl_loadbmtexture_f(&overlay, filter, "pattern");
		require(overlay.gltexture && overlay.gltexture->scoped_static == owned && overlay.gltexture->scoped_super == owned, "production upload carries source classification with opaque stock flags");
		require(overlay.bm_flags == 0 && bool(overlay.gltexture_mask) == owned, "stock flags unchanged; generated same-source mask");
		if (owned) {
			const GLuint color = overlay.gltexture->handle, mask = overlay.gltexture_mask->handle;
			char rejected[256];
			require(!android_mission_assets_prepare("missions/_packs/missing/no.mn2", rejected, sizeof(rejected)), "failed candidate rejected before replacing live context");
			require(glIsTexture(color) && glIsTexture(mask) && !std::strcmp(android_mission_assets_owner(), "a"), "failed mission preparation preserves live color and mask together");
		}
		for (int orient = 0; orient < 4; ++orient) {
			g3_start_frame();
			vms_vector pos{};
			vms_matrix matrix = vmd_identity_matrix;
			g3_set_view_matrix(&pos, &matrix, F1_0);
			glViewport(0, 0, 64, 64);
			glClearColor(0, 0, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glDisable(GL_CULL_FACE);
			glDisable(GL_BLEND);
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LESS);
			glDepthMask(GL_TRUE);
			mask_quad(behind, nullptr, 0, 5 * F1_0, 5 * F1_0);
			const int before = r_mask_draws;
			mask_quad(base, &overlay, orient, 3 * F1_0, 4 * F1_0);
			require(r_mask_draws - before == (owned ? 1 : 0), "production renderer selects mask for stock-opaque replacement");
			unsigned char pixels[64 * 64 * 4];
			glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
			int red = 0, green = 0, blue = 0, global = 0;
			for (int y : { 16, 48 })
				for (int x : { 16, 48 }) {
					const auto *p = pixels + (y * 64 + x) * 4;
					red += p[0] > 180 && p[1] < 80;
					green += p[1] > 180 && p[0] < 80;
					blue += p[2] > 180 && p[0] < 80;
					global += p[0] == 10 && p[1] == 40 && p[2] == 30;
					if (owned) {
						const char *expected[] = { "RGBR", "GRRB", "RBGR", "BRRG" };
						const char color = expected[orient][(y >= 32 ? 0 : 2) + (x >= 32)];
						const bool correct = color == 'R' ? p[0] > 180 && p[1] < 80 : color == 'G' ? p[1] > 180 && p[0] < 80
						                                                                           : p[2] > 180 && p[0] < 80;
						if (!correct) {
							std::fprintf(stderr, "quadrant filter=%d orient=%d xy=%d,%d expected=%c rgba=%u,%u,%u,%u gl_error=%u\n", filter, orient, x, y, color, p[0], p[1], p[2], p[3], glGetError());
							for (int sy : { 48, 16 })
								for (int sx : { 16, 48 }) {
									const auto *q = pixels + (sy * 64 + sx) * 4;
									std::fprintf(stderr, "  sample %d,%d rgba=%u,%u,%u,%u\n", sx, sy, q[0], q[1], q[2], q[3]);
								}
						}
						require(correct, "authored quadrant position after each overlay rotation");
					}
				}
			if (!(owned ? red == 2 && green == 1 && blue == 1 : global == 4)) std::fprintf(stderr, "mask readback owned=%d filter=%d orient=%d red=%d base=%d behind=%d global=%d\n", owned, filter, orient, red, green, blue, global);
			require(owned ? red == 2 && green == 1 && blue == 1 : global == 4, "ordinary cutout reveals base; super-hole reveals behind both layers");
			// Reverse submission order: discarded holes must not write depth.
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			mask_quad(base, &overlay, orient, 3 * F1_0, 4 * F1_0);
			mask_quad(behind, nullptr, 0, 5 * F1_0, 5 * F1_0);
			unsigned char after[sizeof(pixels)];
			glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, after);
			require(!std::memcmp(pixels, after, sizeof(pixels)), "hole discard preserves depth for geometry behind");
			g3_end_frame();
		}
		if (owned)
			for (int mode = 0; mode < 2; ++mode) {
				g3_start_frame();
				vms_vector pos{};
				vms_matrix matrix = vmd_identity_matrix;
				g3_set_view_matrix(&pos, &matrix, F1_0);
				glViewport(0, 0, 64, 64);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				glDisable(GL_CULL_FACE);
				glDisable(GL_BLEND);
				mask_quad(behind, nullptr, 0, 5 * F1_0, 5 * F1_0);
				mask_quad(overlay, mode ? &base : nullptr, 0, 3 * F1_0, 4 * F1_0);
				unsigned char pixels[64 * 64 * 4];
				glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
				int red = 0, green = 0, blue = 0;
				for (int y : { 16, 48 })
					for (int x : { 16, 48 }) {
						const auto *p = pixels + (y * 64 + x) * 4;
						red += p[0] > 180 && p[1] < 80;
						green += p[1] > 180 && p[0] < 80;
						blue += p[2] > 180 && p[0] < 80;
					}
				require(mode ? green == 4 : red == 2 && blue == 2, "standalone holes reveal behind; opaque overlay covers base holes");
				g3_end_frame();
			}
		if (owned && filter > 0)
			for (int mode = 0; mode < 3; ++mode) {
				for (int legacy = 0; legacy < 2; ++legacy) {
					overlay.gltexture->scoped_static = !legacy;
					g3_start_frame();
					vms_vector pos{};
					vms_matrix matrix = vmd_identity_matrix;
					g3_set_view_matrix(&pos, &matrix, F1_0);
					glViewport(0, 0, 64, 64);
					glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
					glDisable(GL_CULL_FACE);
					glDisable(GL_BLEND);
					mask_quad(behind, nullptr, 0, 5 * F1_0, 5 * F1_0);
					// An all-covered synthetic mask selects tex2m without hiding the base edge.
					empty.gltexture_mask = mode == 2 ? coverage.gltexture : nullptr;
					empty.gltexture->scoped_super = mode == 2;
					const int before = r_mask_draws;
					mask_quad(overlay, mode ? &empty : nullptr, 0, 3 * F1_0, 4 * F1_0);
					require(r_mask_draws - before == (mode == 2), "binary base edge exercises selected plain or masked production program");
					empty.gltexture_mask = nullptr;
					empty.gltexture->scoped_super = 0;
					unsigned char edge[4];
					glReadPixels(34, 48, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, edge);
					const bool background = edge[2] > 180 && edge[0] < 80;
					if (background == bool(legacy)) std::fprintf(stderr, "edge mode=%d filter=%d legacy=%d rgba=%u,%u,%u,%u\n", mode, filter, legacy, edge[0], edge[1], edge[2], edge[3]);
					require(background != bool(legacy), "source binary edge discards before depth; legacy partial coverage remains unchanged");
					require(gles3_shim_set_binary_texture(0) == 0, "standalone binary draw restores previous shim state");
					g3_end_frame();
				}
				overlay.gltexture->scoped_static = 1;
			}
		if (owned && filter == 2) {
			for (auto *texture : { overlay.gltexture, overlay.gltexture_mask }) {
				glBindTexture(GL_TEXTURE_2D, texture->handle);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 2);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 2);
			}
			g3_start_frame();
			vms_vector pos{};
			vms_matrix matrix = vmd_identity_matrix;
			g3_set_view_matrix(&pos, &matrix, F1_0);
			glViewport(0, 0, 64, 64);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glDisable(GL_CULL_FACE);
			glDisable(GL_BLEND);
			mask_quad(behind, nullptr, 0, 5 * F1_0, 5 * F1_0);
			mask_quad(base, &overlay, 0, 3 * F1_0, 4 * F1_0);
			unsigned char p[4];
			glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
			require(p[2] > 180 && p[0] < 80, "conservative final mip retains super-hole coverage");
			g3_end_frame();
		}
		if (owned) {
			ogl_smash_png_textures();
			const int old_mode = Game_mode;
			Game_mode = GM_MULTI;
			ogl_loadbmtexture_f(&overlay, filter, "pattern");
			require(overlay.gltexture && !overlay.gltexture->scoped_static && !overlay.gltexture->scoped_super && !overlay.gltexture_mask && overlay.gltexture->w == 4, "replacement flush and disabled cosmetics clear classification and mask before stock fallback");
			Game_mode = old_mode;
		}
		ogl_freebmtexture(&overlay);
	}
	if (owned)
		for (const char *name : { "tinyopaque", "tinycutout", "tinykey" }) {
			grs_bitmap tiny{};
			unsigned char pixel = 0;
			tiny.bm_w = tiny.bm_h = tiny.bm_rowsize = 1;
			tiny.bm_data = &pixel;
			ogl_loadbmtexture_f(&tiny, 0, name);
			require(tiny.gltexture && tiny.gltexture->scoped_static && tiny.gltexture->tw == 2 && tiny.gltexture->th == 2, "engine 1x1 upload uses padded 2x2 storage");
			for (auto *texture : { tiny.gltexture, tiny.gltexture_mask })
				if (texture) {
					glBindTexture(GL_TEXTURE_2D, texture->handle);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
				}
			g3_start_frame();
			vms_vector pos{};
			vms_matrix matrix = vmd_identity_matrix;
			g3_set_view_matrix(&pos, &matrix, F1_0);
			glViewport(0, 0, 64, 64);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glDisable(GL_CULL_FACE);
			glDisable(GL_BLEND);
			mask_quad(behind, nullptr, 0, 5 * F1_0, 5 * F1_0);
			mask_quad(base, &tiny, 0, 3 * F1_0, 4 * F1_0);
			unsigned char result[4];
			glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, result);
			const bool ok = !strcmp(name, "tinyopaque") ? result[0] > 180 : !strcmp(name, "tinycutout") ? result[1] > 180
			                                                                                            : result[2] > 180;
			require(ok, "1x1 opaque/cutout/key filter-off to mip-filter transition has complete semantic chain");
			g3_end_frame();
			ogl_freebmtexture(&tiny);
		}
	ogl_freebmtexture(&coverage);
	ogl_freebmtexture(&empty);
	ogl_freebmtexture(&base);
	ogl_freebmtexture(&behind);
	grd_curscreen = nullptr;
	grd_curcanv = nullptr;
	require(glGetError() == GL_NO_ERROR, "static mask production composition cleanup");
}
#endif
