#ifdef DXX_BUILD_DESCENT_II
extern "C" {
#include "effects.h"
#include "textures.h"
#include "cntrlcen.h"
#include "debug_tex_overlay.h"
#include "merged_wall_debug.h"
#include "segment.h"
#include "xl_animation_phase.h"
}
static void animation_pattern()
{
	for (const char *owner : { "a", "b" }) {
		const std::string path = std::string(owner) + "/mods/pilot/textures/anim#0.tga";
		std::filesystem::create_directories(std::filesystem::path(path).parent_path());
		const int channels = owner[0] == 'a' ? 4 : 3;
		unsigned char header[18]{};
		header[2] = 2;
		header[12] = 4;
		header[14] = 24;
		header[16] = channels * 8;
		header[17] = channels == 4 ? 0x28 : 0x20;
		std::ofstream file(path, std::ios::binary);
		file.write(reinterpret_cast<const char *>(header), sizeof(header));
		for (int frame = 0; frame < 6; ++frame)
			for (int pixel = 0; pixel < 16; ++pixel) {
				const unsigned char color[] = { 30, 40, static_cast<unsigned char>((owner[0] == 'a' ? 40 : 100) + frame * 20), 255 };
				file.write(reinterpret_cast<const char *>(color), channels);
			}
		require(bool(file), "write opaque animation strip");
	}
	image("a/mods/pilot/textures/badanim#0.tga", 120, 255, 88, 128, 4, 8);
	image("a/mods/pilot/textures/short#0.tga", 210, 255, 40, 30, 4, 7);
	image("a/mods/pilot/textures/alpha#0.tga", 210, 100, 40, 30, 4, 8);
	for (int count : { 1, 2, 128, 129 }) {
		const std::string path = "a/mods/pilot/textures/bound" + std::to_string(count) + "#0.tga";
		image(path.c_str(), 210, 255, 40, 30, 2, 2 * count);
	}
	image("a/mods/pilot/textures/nonpot#0.tga", 210, 255, 40, 30, 3, 6);
	image("a/mods/pilot/textures/latekey#0.tga", 210, 255, 40, 30, 8, 16);
	{
		std::fstream file("a/mods/pilot/textures/latekey#0.tga", std::ios::in | std::ios::out | std::ios::binary);
		file.seekp(18 + 8 * 8 * 4);
		const unsigned char key[] = { 128, 88, 120, 255 };
		file.write(reinterpret_cast<const char *>(key), 4);
	}
}
static void test_animation(const char *owner)
{
	const bool scoped = std::strcmp(owner, "stock") != 0;
	if (!std::strcmp(owner, "a")) {
		for (const char *name : { "badanim", "short", "alpha", "bound129", "nonpot", "latekey" }) {
			png_data invalid{};
			const std::string path = std::string("mods/pilot/textures/") + name + "#0.tga";
			require(!read_png_scoped_animation(path.c_str(), &invalid), "unsupported strip rejected before fitting");
		}
		for (int count : { 1, 2, 128 }) {
			png_data bounded{};
			const std::string path = "mods/pilot/textures/bound" + std::to_string(count) + "#0.tga";
			require(read_png_scoped_animation(path.c_str(), &bounded) && bounded.height / bounded.width == unsigned(count), "validated source frame bound");
			free(bounded.data);
		}
		png_data valid{};
		require(read_png_scoped_animation("mods/pilot/textures/anim#0.tga", &valid), "owned opaque strip decoded");
		require(valid.width == 4 && valid.height == 24 && valid.data[0] == 40 && valid.data[5 * 16 * 4] == 140, "frame order and original dimensions retained");
		free(valid.data);
	}
	grs_screen screen{};
	screen.sc_w = screen.sc_h = 64;
	screen.sc_aspect = F1_0;
	gr_init_canvas(&screen.sc_canvas, nullptr, BM_OGL, 64, 64);
	screen.sc_canvas.cv_fade_level = GR_FADE_OFF;
	grd_curscreen = &screen;
	gr_set_current_canvas(&screen.sc_canvas);
	Num_effects = 1;
	NumTextures = 2;
	Effects[0] = {};
	auto &effect = Effects[0];
	effect.changing_wall_texture = 0;
	effect.changing_object_texture = -1;
	effect.crit_clip = -1;
	effect.vc.num_frames = 8;
	effect.vc.frame_time = 6144;
	effect.vc.play_time = 49152;
	for (int i = 0; i < 8; ++i) {
		auto &bitmap = GameBitmaps[10 + i];
		bitmap = {};
		mask_solid(bitmap, 10, 40, 30);
		std::snprintf(piggy_game_bitmap_name(&bitmap), 12, "anim#%d", i);
		effect.vc.frames[i].index = 10 + i;
	}
	grs_bitmap overlay{};
	mask_solid(overlay, 0, 0, 0, 0);
	for (bool critical : { false, true })
		for (int filter : { 0, 1, 2 }) {
			GameCfg.TexFilt = filter;
			for (int identity : { 0, 1, 2 }) {
				const bool enabled = identity != 0;
				fixture_hog = identity == 1 ? "other.hog" : "pilot.hog";
				fixture_xl_mission = enabled;
				for (int source_frame = 0; source_frame < 6; ++source_frame) {
					const int elapsed = source_frame * 8192 + 1;
					int count;
					fix left;
					effect_get_loop_state(&effect, elapsed, &count, &left);
					effect.frame_count = count;
					effect.time_left = left;
					Control_center_destroyed = critical;
					Num_effects = critical ? 2 : 1;
					effect.crit_clip = critical ? 1 : -1;
					Effects[1] = effect;
					Effects[1].changing_wall_texture = -1;
					Effects[1].flags = EF_CRITICAL;
					effect_apply_bitmap_state(0);
					const eclip before = effect, before_critical = Effects[1];
					const auto rng = d_rand_get_stream_call_count(D_RNG_SIM);
					g_android_draw_face_ctx = {};
					g_android_draw_face_ctx.valid = 1;
					g_android_draw_face_ctx.tmap1 = 0;
					g3_start_frame();
					vms_vector position{};
					vms_matrix matrix = vmd_identity_matrix;
					g3_set_view_matrix(&position, &matrix, F1_0);
					glViewport(0, 0, 64, 64);
					glDisable(GL_CULL_FACE);
					glDisable(GL_BLEND);
					glDisable(GL_DEPTH_TEST);
					glClearColor(0, 0, 0, 1);
					glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
					mask_quad(GameBitmaps[Textures[0].index], source_frame & 1 ? &overlay : nullptr, source_frame % 4, 3 * F1_0, 4 * F1_0);
					unsigned char pixel[4];
					glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
					const int expected = identity == 2 && scoped ? (owner[0] == 'a' ? 40 : 100) + source_frame * 20 : 10;
					if (std::abs(int(pixel[0]) - expected) > 2) std::fprintf(stderr, "animation owner=%s enabled=%d filter=%d frame=%d expected=%d rgba=%u,%u,%u,%u\n", owner, enabled, filter, source_frame, expected, pixel[0], pixel[1], pixel[2], pixel[3]);
					require(std::abs(int(pixel[0]) - expected) <= 2 && pixel[1] == 40 && pixel[2] == 30, "production wall draw selects replacement phase or classic stock");
					require(rng == d_rand_get_stream_call_count(D_RNG_SIM), "render animation does not consume SIM RNG");
					require(!std::memcmp(&before_critical, &Effects[1], sizeof(effect)), "render sampling leaves critical effect unchanged");
					require(!std::memcmp(&before, &effect, sizeof(effect)), "render sampling leaves authoritative effect state unchanged");
					g3_end_frame();
					android_merged_wall_clear_draw_face_context();
				}
			}
		}
	// Rendering must not trigger the real effect loop's one-shot side change.
	Num_effects = 1;
	Control_center_destroyed = 0;
	effect.crit_clip = -1;
	effect.flags = EF_ONE_SHOT;
	effect.frame_count = 7;
	effect.time_left = 1;
	effect.segnum = 0;
	effect.sidenum = 0;
	effect.dest_bm_num = 2;
	Segments[0].sides[0].tmap_num2 = 1;
	for (int i = 0; i < 17; ++i)
		require(dxx_xl_animation_frame(8, 6144, effect.frame_count, effect.time_left, 0, 6) == 5, "one-shot remains on final presentation frame before simulation advances");
	require(Segments[0].sides[0].tmap_num2 == 1, "render sampling cannot complete a one-shot");
	FrameTime = 2;
	do_special_effects();
	require(effect.frame_count == 0 && !(effect.flags & EF_ONE_SHOT) && effect.segnum == -1 && Segments[0].sides[0].tmap_num2 == 2,
	        "native effect loop alone performs one-shot completion");
	stop_effect(0);
	const eclip stopped = effect;
	FrameTime = F1_0;
	do_special_effects();
	require(!std::memcmp(&stopped, &effect, sizeof(effect)) && dxx_xl_animation_frame(8, 6144, effect.frame_count, effect.time_left, 1, 6) == 0,
	        "stopped effect preserves simulation state and first replacement frame");

	// Load a real minimal POG, then prove the animation adapter respects its ownership.
	std::vector<unsigned char> pog(36, 0);
	std::memcpy(pog.data(), "DPOG", 4);
	pog[4] = 1;
	pog[8] = 1;
	pog[12] = 10;
	pog[23] = pog[24] = 2;
	std::ofstream pog_file("animtest.POG", std::ios::binary);
	pog_file.write(reinterpret_cast<const char *>(pog.data()), pog.size());
	pog_file.close();
	const int previous_count = Num_bitmap_files;
	Num_bitmap_files = 18;
	char level[] = "animtest.rl2";
	load_bitmap_replacements(level);
	require(piggy_bitmap_is_custom(&GameBitmaps[10]), "native POG establishes custom bitmap ownership");
	mask_solid(GameBitmaps[10], 10, 40, 30);
	g3_start_frame();
	vms_vector pos{};
	vms_matrix matrix = vmd_identity_matrix;
	g3_set_view_matrix(&pos, &matrix, F1_0);
	g_android_draw_face_ctx = {};
	g_android_draw_face_ctx.valid = 1;
	g_android_draw_face_ctx.tmap1 = 0;
	fixture_xl_mission = true;
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	mask_quad(GameBitmaps[10], nullptr, 0, 3 * F1_0, 4 * F1_0);
	unsigned char pog_pixel[4];
	glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pog_pixel);
	require(pog_pixel[0] == 10 && pog_pixel[1] == 40, "POG bitmap takes priority over cached owned animation");
	g3_end_frame();
	android_merged_wall_clear_draw_face_context();
	free_bitmap_replacements();
	Num_bitmap_files = previous_count;
	GameBitmaps[10].bm_data = nullptr;
	for (int i = 0; i < 8; ++i) ogl_freebmtexture(&GameBitmaps[10 + i]);
	ogl_freebmtexture(&overlay);
	Num_effects = NumTextures = 0;
	Control_center_destroyed = 0;
	fixture_xl_mission = true;
	fixture_hog = "pilot.hog";
	grd_curscreen = nullptr;
	grd_curcanv = nullptr;
}
#endif
