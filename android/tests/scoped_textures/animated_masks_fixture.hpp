#ifdef DXX_BUILD_DESCENT_II
#include "xl_static_texture_pixels.h"
extern "C" {
#include "internal.h"
}
static void animated_mask_pattern()
{
	for (const char *owner : { "a", "b" }) {
		const std::string root = std::string(owner) + "/mods/pilot/textures/";
		image((root + "maskanim#0.tga").c_str(), 210, 255, 40, 30, 4, 16);
		std::fstream file(root + "maskanim#0.tga", std::ios::in | std::ios::out | std::ios::binary);
		file.seekp(17);
		file.put(0x28);
		for (int frame = 0; frame < 4; ++frame)
			for (int y = 0; y < 4; ++y)
				for (int x = 0; x < 4; ++x) {
					const unsigned char opaque[] = { 30, 40, static_cast<unsigned char>(owner[0] == 'a' ? 210 : 150), 255 };
					const unsigned char cutout[] = { 99, 88, 77, 0 }, key[] = { 128, 88, 120, 255 };
					const bool hole = frame >= 2 && y >= 2 && x < 2;
					const bool cut = (frame == 1 || frame == 3) && y < 2 && x >= 2;
					file.write(reinterpret_cast<const char *>(hole ? key : cut ? cutout
					                                                           : opaque),
					           4);
				}
		require(bool(file), "write animated binary coverage");
		file.close();
		std::filesystem::copy_file(root + "maskanim#0.tga", root + "latealpha#0.tga", std::filesystem::copy_options::overwrite_existing);
		std::fstream partial(root + "latealpha#0.tga", std::ios::in | std::ios::out | std::ios::binary);
		partial.seekp(18 + 3 * 16 * 4 + 3);
		partial.put(100);
		require(bool(partial), "write late partial-alpha rejection control");
		image((root + "sparse#0.tga").c_str(), 210, 255, 40, 30, 8, 16);
		std::fstream sparse(root + "sparse#0.tga", std::ios::in | std::ios::out | std::ios::binary);
		sparse.seekp(17);
		sparse.put(0x28);
		sparse.seekp(18 + 8 * 8 * 4);
		const unsigned char key[] = { 128, 88, 120, 255 };
		sparse.write(reinterpret_cast<const char *>(key), 4);
		require(bool(sparse), "write later-frame sparse key");
	}
}

static void test_animated_masks(const char *owner)
{
	const bool owned = std::strcmp(owner, "stock") != 0;
	const int red = owner[0] == 'a' ? 210 : 150;
	if (owned) {
		png_data source{};
		require(read_png_scoped_binary_animation("mods/pilot/textures/maskanim#0.tga", &source) && source.scoped_super && source.width == 4 && source.height == 16,
		        "binary strip retains original frames and source keys");
		free(source.data);
		require(!read_png_scoped_binary_animation("mods/pilot/textures/latealpha#0.tga", &source), "partial alpha in final frame rejects entire strip before upload");
	}
	if (!std::strcmp(owner, "a")) {
		for (const char *name : { "short", "alpha", "bound129", "nonpot" }) {
			png_data invalid{};
			const std::string path = std::string("mods/pilot/textures/") + name + "#0.tga";
			require(!read_png_scoped_binary_animation(path.c_str(), &invalid), "binary strip shape, alpha and count bounds");
		}
		for (int count : { 1, 2, 128 }) {
			png_data bounded{};
			const std::string path = "mods/pilot/textures/bound" + std::to_string(count) + "#0.tga";
			require(read_png_scoped_binary_animation(path.c_str(), &bounded) && bounded.height / bounded.width == unsigned(count), "binary animation frame bounds");
			free(bounded.data);
		}
		png_data sparse{};
		require(read_png_scoped_binary_animation("mods/pilot/textures/sparse#0.tga", &sparse), "later-frame sparse key is admitted before fitting");
		dxx_xl_static_pixels fitted{};
		require(dxx_xl_fit_static_pixels(sparse.data + 8 * 8 * sparse.channels, 8, 8, sparse.channels, 4, &fitted) && fitted.super && fitted.mask[0], "later-frame key survives independent downsize");
		free(fitted.data);
		free(sparse.data);
	}
	grs_screen screen{};
	screen.sc_w = screen.sc_h = 64;
	screen.sc_aspect = F1_0;
	gr_init_canvas(&screen.sc_canvas, nullptr, BM_OGL, 64, 64);
	screen.sc_canvas.cv_fade_level = GR_FADE_OFF;
	grd_curscreen = &screen;
	gr_set_current_canvas(&screen.sc_canvas);
	grs_bitmap base{}, behind{}, empty{};
	mask_solid(base, 20, 220, 20);
	mask_solid(behind, 20, 20, 220);
	mask_solid(empty, 0, 0, 0, 0);
	NumTextures = 6;
	for (int i = 0; i < 4; ++i) {
		auto &bitmap = GameBitmaps[50 + i];
		bitmap = {};
		mask_solid(bitmap, 10, 40, 30);
		std::snprintf(piggy_game_bitmap_name(&bitmap), 12, "maskanim#%d", i);
		Textures[2 + i].index = 50 + i;
	}
	fixture_xl_mission = true;
	fixture_hog = "pilot.hog";
	for (bool door : { false, true }) {
		Num_effects = door ? 0 : 1;
		Effects[0] = {};
		auto &effect = Effects[0];
		effect.changing_wall_texture = 1;
		effect.changing_object_texture = effect.crit_clip = -1;
		effect.vc.num_frames = 4;
		effect.vc.frame_time = F1_0 / 4;
		effect.vc.play_time = F1_0;
		for (int i = 0; i < 4; ++i) effect.vc.frames[i].index = 50 + i;
		Num_segments = Num_walls = Num_wall_anims = 1;
		Highest_segment_index = 0;
		Segments[0] = {};
		Segments[0].sides[0].wall_num = 0;
		Walls[0] = {};
		Walls[0].type = WALL_DOOR;
		Walls[0].clip_num = 0;
		WallAnims[0] = {};
		WallAnims[0].num_frames = 4;
		for (int i = 0; i < 4; ++i) WallAnims[0].frames[i] = 2 + i;
		for (int filter : { 0, 1, 2 })
			for (int frame = 0; frame < 4; ++frame)
				for (int mode = 0; mode < 3; ++mode)
					for (int orient = 0; orient < (mode ? 1 : 4); ++orient)
						for (int identity : { 0, 1, 2, 3 }) {
							if (identity == 3 && (filter || frame || mode || orient || !owned)) continue;
							fixture_xl_mission = true;
							fixture_hog = "pilot.hog";
							GameCfg.TexFilt = filter;
							effect.frame_count = frame;
							effect.time_left = effect.vc.frame_time - 1; // one native tick beyond the strict-negative frame boundary
							if (!door) effect_apply_bitmap_state(0);
							fixture_xl_mission = identity != 1;
							fixture_hog = identity == 2 ? "other.hog" : "pilot.hog";
							const bool admitted = owned && !identity;
							std::vector<int> reserved;
							if (identity == 3) {
								int available = 0;
								for (int i = 0; i < OGL_TEXTURE_LIST_SIZE; ++i)
									if (ogl_texture_list[i].handle <= 0 && !ogl_texture_list[i].w && ++available > 5) {
										ogl_texture_list[i].w = 1;
										reserved.push_back(i);
									}
								require(!reserved.empty(), "reserve pool capacity for atomic color-mask allocation control");
							}
							const int texture = door ? 2 + frame : 1;
							WallAnims[0].flags = mode ? WCF_TMAP1 : 0;
							Segments[0].sides[0].tmap_num = mode ? texture : 0;
							Segments[0].sides[0].tmap_num2 = mode ? 0 : texture;
							g_android_draw_face_ctx = {};
							g_android_draw_face_ctx.valid = 1;
							g_android_draw_face_ctx.tmap1 = Segments[0].sides[0].tmap_num;
							g_android_draw_face_ctx.tmap2 = Segments[0].sides[0].tmap_num2;
							const auto saved_effect = effect;
							const auto saved_wall = Walls[0];
							const auto saved_side = Segments[0].sides[0];
							const auto sim = d_rand_get_stream_call_count(D_RNG_SIM), fx = d_rand_get_stream_call_count(D_RNG_FX);
							g3_start_frame();
							vms_vector position{};
							vms_matrix matrix = vmd_identity_matrix;
							g3_set_view_matrix(&position, &matrix, F1_0);
							glViewport(0, 0, 64, 64);
							glDisable(GL_CULL_FACE);
							glDisable(GL_BLEND);
							glEnable(GL_DEPTH_TEST);
							glDepthFunc(GL_LESS);
							glDepthMask(GL_TRUE);
							glClearColor(0, 0, 0, 1);
							glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
							auto &bitmap = GameBitmaps[50 + frame];
							auto surface = [&]() {
								if (!mode) mask_quad(base, &bitmap, orient, 3 * F1_0, 4 * F1_0);
								else mask_quad(bitmap, mode == 1 ? &empty : nullptr, 0, 3 * F1_0, 4 * F1_0);
							};
							mask_quad(behind, nullptr, 0, 5 * F1_0, 5 * F1_0);
							surface();
							unsigned char pixels[64 * 64 * 4], reversed[sizeof(pixels)];
							glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
							const char *quadrants[] = { "RGBR", "GRRB", "RBGR", "BRRG" };
							for (int y : { 48, 16 })
								for (int x : { 16, 48 }) {
									char expected = quadrants[orient][(y < 32 ? 2 : 0) + (x >= 32)];
									if ((expected == 'G' && frame != 1 && frame != 3) || (expected == 'B' && frame < 2)) expected = 'R';
									if (mode && expected == 'G') expected = 'B';
									const auto *p = pixels + (y * 64 + x) * 4;
									const bool correct = !admitted ? p[0] == 10 && p[1] == 40 : expected == 'R' ? std::abs(int(p[0]) - red) <= 2 && p[1] < 80
									                                                        : expected == 'G'   ? p[1] > 180 && p[0] < 80
									                                                                            : p[2] > 180 && p[0] < 80;
									if (!correct) std::fprintf(stderr, "animated mask owner=%s door=%d filter=%d frame=%d mode=%d orient=%d xy=%d,%d expected=%c rgba=%u,%u,%u,%u\n", owner, door, filter, frame, mode, orient, x, y, expected, p[0], p[1], p[2], p[3]);
									require(correct, "animated color and coverage select same authored frame");
								}
							glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
							surface();
							mask_quad(behind, nullptr, 0, 5 * F1_0, 5 * F1_0);
							glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, reversed);
							require(!std::memcmp(pixels, reversed, sizeof(pixels)), "animated holes preserve depth regardless of submission order");
							if (admitted && filter == 2 && !mode && !orient) {
								// Inspect the production-selected frame, including its paired mask.
								surface();
								GLint handles[2]{}, levels[2][2]{};
								const int count = frame >= 2 ? 2 : 1;
								for (int i = 0; i < count; ++i) {
									glActiveTexture(GL_TEXTURE1 + i);
									glGetIntegerv(GL_TEXTURE_BINDING_2D, &handles[i]);
									require(handles[i] && glIsTexture(handles[i]), "selected animation color/mask has a live GPU object");
									glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, &levels[i][0]);
									glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, &levels[i][1]);
									glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 2);
									glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 2);
								}
								glActiveTexture(GL_TEXTURE0);
								glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
								mask_quad(behind, nullptr, 0, 5 * F1_0, 5 * F1_0);
								surface();
								unsigned char mip[4];
								glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, mip);
								const bool correct = !frame ? std::abs(int(mip[0]) - red) <= 2 : frame == 1 ? mip[1] > 180 && mip[0] < 80
								                                                                            : mip[2] > 180 && mip[0] < 80;
								if (!correct) std::fprintf(stderr, "animated final mip owner=%s door=%d frame=%d rgba=%u,%u,%u,%u\n", owner, door, frame, mip[0], mip[1], mip[2], mip[3]);
								require(correct, "each frame retains its opaque/cutout/super-hole semantics through final mip");
								for (int i = 0; i < count; ++i) {
									glActiveTexture(GL_TEXTURE1 + i);
									glBindTexture(GL_TEXTURE_2D, handles[i]);
									glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, levels[i][0]);
									glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, levels[i][1]);
								}
								glActiveTexture(GL_TEXTURE0);
							}
							require(!std::memcmp(&effect, &saved_effect, sizeof(effect)) && !std::memcmp(&Walls[0], &saved_wall, sizeof(wall)) && !std::memcmp(&Segments[0].sides[0], &saved_side, sizeof(saved_side)) && sim == d_rand_get_stream_call_count(D_RNG_SIM) && fx == d_rand_get_stream_call_count(D_RNG_FX), "binary animation rendering preserves authoritative state and RNG");
							g3_end_frame();
							android_merged_wall_clear_draw_face_context();
							for (int slot : reserved) {
								require(!ogl_texture_list[slot].handle && ogl_texture_list[slot].w == 1, "failed strip leaves reserved pool slots untouched");
								ogl_texture_list[slot].w = 0;
							}
							if (identity == 3) ogl_smash_png_textures(); // recovery uses the production replacement-reset boundary
						}
	}
	fixture_xl_mission = true;
	fixture_hog = "pilot.hog";
	for (int i = 0; i < 4; ++i) ogl_freebmtexture(&GameBitmaps[50 + i]);
	ogl_freebmtexture(&base);
	ogl_freebmtexture(&behind);
	ogl_freebmtexture(&empty);
	Num_effects = NumTextures = Num_walls = Num_wall_anims = 0;
	grd_curscreen = nullptr;
	grd_curcanv = nullptr;
}
#endif
