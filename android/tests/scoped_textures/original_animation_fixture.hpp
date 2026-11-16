#ifdef DXX_BUILD_DESCENT_II
#include "../../XL_compat/runtime/scoped_texture_resources.h"
#include <iterator>

static std::vector<unsigned char> original_animation_bytes(const char *path)
{
	std::ifstream file(path, std::ios::binary);
	require(bool(file), "open original animation resource input");
	return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
}

template <typename RecreateContext>
static nlohmann::json test_original_animation(const char *owner, bool retained, RecreateContext recreate_context)
{
	// Independent samples of pinned BoilingPoint force01#0.tga, decoded with Pillow
	// Keep the trace verifier ORIGINAL_FORCE_SAMPLES in xl_private_assets.py synchronized
	// Screen (20,20), (32,32), (44,44) map to source (33,94), (65,62), (97,30)
	// The 64x64 viewport projects the +/-3 quad at depth 4 onto [8,56]
	static constexpr int samples[6][3][3] = {
		{ { 51, 0, 0 }, { 214, 42, 42 }, { 52, 0, 0 } },
		{ { 111, 9, 9 }, { 70, 0, 0 }, { 32, 0, 0 } },
		{ { 59, 0, 0 }, { 31, 0, 0 }, { 14, 0, 0 } },
		{ { 91, 4, 4 }, { 0, 0, 0 }, { 131, 14, 14 } },
		{ { 23, 0, 0 }, { 60, 0, 0 }, { 79, 0, 0 } },
		{ { 23, 0, 0 }, { 70, 0, 0 }, { 80, 1, 1 } },
	};
	const bool original = !std::strcmp(owner, "a");
	grs_screen screen{};
	screen.sc_w = screen.sc_h = 64;
	screen.sc_aspect = F1_0;
	gr_init_canvas(&screen.sc_canvas, nullptr, BM_OGL, 64, 64);
	screen.sc_canvas.cv_fade_level = GR_FADE_OFF;
	grd_curscreen = &screen;
	gr_set_current_canvas(&screen.sc_canvas);
	fixture_hog = "pilot.hog";
	fixture_xl_mission = true;
	GameCfg.TexFilt = 0;
	Control_center_destroyed = 0;
	Num_effects = NumTextures = 1;
	Num_walls = Num_wall_anims = 0;
	Effects[0] = {};
	auto &effect = Effects[0];
	effect.changing_wall_texture = 0;
	effect.changing_object_texture = effect.crit_clip = -1;
	effect.vc.num_frames = 8;
	effect.vc.frame_time = 6144;
	effect.vc.play_time = 49152;
	const int first_bitmap = retained ? 1998 : 90;
	for (int i = 0; i < 8; ++i) {
		auto &bitmap = GameBitmaps[first_bitmap + i];
		bitmap = {};
		mask_solid(bitmap, 10, 40, 30);
		std::snprintf(piggy_game_bitmap_name(&bitmap), 12, "force01#%d", i);
		effect.vc.frames[i].index = first_bitmap + i;
	}
	std::unique_ptr<XlResourceBanks> banks;
	std::unique_ptr<xl_pog_resources, decltype(&xl_pog_free)> prepared(nullptr, xl_pog_free);
	const int bitmap_count = Num_bitmap_files;
	const char *source = "a/mods/pilot/textures/force01#0.tga", *held = "a/force01-native-input.tga";
	if (retained) {
		banks = std::make_unique<XlResourceBanks>(original_animation_bytes("groupa.pig"), original_animation_bytes("descent2.s22"), 22050);
		Num_bitmap_files = banks->bitmap_count();
		for (int i = 0; i < 8; ++i)
			require(!std::strcmp(banks->bitmap_name(first_bitmap + i), piggy_game_bitmap_name(&GameBitmaps[first_bitmap + i])), "original forcefield bank alias identity");
		if (original) {
			const auto bytes = original_animation_bytes(source);
			xl_compat::ScopedTextureResources textures;
			textures.images.push_back({ "mods/pilot/textures/force01#0.tga", picosha2::hash256_hex_string(bytes), xl_compat::read_particle_image(bytes.data(), bytes.size()) });
			textures.bindings.push_back({ first_bitmap, 0 });
			xl_assets::ObjectDependencies graph;
			graph.resource_banks = banks.get();
			graph.require_owned_banks = true;
			prepared.reset(xl_pog_prepare_scoped(textures, graph, nullptr));
			require(xl_pog_prepare_storage(prepared.get()) && xl_pog_install_prepared(prepared.get()), "install actual retained original animation owner");
		}
		// Prove the native RGBA draw cannot pass by using Android's fallback file
		std::filesystem::rename(source, held);
		clear_texture_lookup_cache();
		require(xl_pog_effect_frames(&effect) == (original ? 6 : 8) && effect.vc.num_frames == 8 && effect.vc.frame_time == 6144, "retained original effective count preserves authored definitions");
	}
	nlohmann::json frames = nlohmann::json::array();
	for (int step = 0; step <= 6; ++step) {
		const int frame = step % 6;
		int count;
		fix left;
		effect_get_loop_state(&effect, step * (retained ? 6144 : 8192) + 1, &count, &left);
		effect.frame_count = count;
		effect.time_left = left;
		effect_apply_bitmap_state(0);
		if (retained && original) {
			xl_pog_pixel_view physics{};
			require(effect.frame_count == frame && GameBitmaps[Textures[0].index].bm_type == BM_RGBA &&
			            xl_pog_physics_view(first_bitmap, &physics) && physics.height == 768,
			        "original native frame draw retains separate whole-strip physics");
		}
		const eclip before = effect;
		const auto sim = d_rand_get_stream_call_count(D_RNG_SIM), fx = d_rand_get_stream_call_count(D_RNG_FX);
		const bool recreated = retained && original && step == 3;
		if (recreated) {
			xl_pog_pixel_view old_view{}, new_view{};
			require(xl_pog_physics_view(first_bitmap, &old_view), "live original physics view before context recreation");
			const auto hash = picosha2::hash256_hex_string(old_view.pixels, old_view.pixels + size_t(old_view.row_size) * old_view.height);
			const auto *display_pixels = GameBitmaps[first_bitmap].bm_data;
			recreate_context();
			require(xl_pog_physics_view(first_bitmap, &new_view) && new_view.pixels == old_view.pixels &&
			            new_view.height == old_view.height && new_view.row_size == old_view.row_size &&
			            GameBitmaps[first_bitmap].bm_data == display_pixels &&
			            picosha2::hash256_hex_string(new_view.pixels, new_view.pixels + size_t(new_view.row_size) * new_view.height) == hash,
			        "EGL recreation preserves live retained display and physics pixels");
			require(xl_pog_effect_frames(&effect) == 6 && !std::filesystem::exists(source), "EGL recreation retains original owner without fallback input");
		}
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
		mask_quad(GameBitmaps[Textures[0].index], nullptr, 0, 3 * F1_0, 4 * F1_0);
		nlohmann::json pixels = nlohmann::json::array();
		for (int sample = 0; sample < 3; ++sample) {
			unsigned char pixel[4];
			glReadPixels(20 + sample * 12, 20 + sample * 12, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
			for (int channel = 0; channel < 3; ++channel) {
				const int stock[] = { 10, 40, 30 };
				const int expected = original ? samples[frame][sample][channel] : stock[channel];
				if (std::abs(int(pixel[channel]) - expected) > 2)
					std::fprintf(stderr, "Original animation owner=%s frame=%d sample=%d channel=%d expected=%d actual=%u\n", owner, frame, sample, channel, expected, pixel[channel]);
				require(std::abs(int(pixel[channel]) - expected) <= 2, "original forcefield frame pixels or stock isolation");
			}
			require(pixel[3] == 255, "original opaque RGB animation alpha");
			pixels.push_back({ pixel[0], pixel[1], pixel[2], pixel[3] });
		}
		require(glGetError() == GL_NO_ERROR, "original animation draw GL state");
		require(!std::memcmp(&before, &effect, sizeof(effect)), "original animation rendering preserves native effect clock");
		require(sim == d_rand_get_stream_call_count(D_RNG_SIM) && fx == d_rand_get_stream_call_count(D_RNG_FX), "original animation rendering preserves SIM and FX RNG");
		frames.push_back({ { "frame", frame }, { "original", original }, { "pixels", std::move(pixels) } });
		if (retained) {
			frames.back()["native_frames"] = xl_pog_effect_frames(&effect);
			frames.back()["context_recreated"] = recreated;
		}
		g3_end_frame();
		android_merged_wall_clear_draw_face_context();
	}
	if (retained) {
		require(xl_pog_install(nullptr) && xl_pog_effect_frames(&effect) == 8, "retiring original native animation restores authored count");
		xl_pog_pixel_view physics{};
		for (int i = 0; i < 8; ++i) require(!xl_pog_physics_view(first_bitmap + i, &physics), "retired original animation releases all borrowed views");
		Num_bitmap_files = bitmap_count;
		std::filesystem::rename(held, source);
		clear_texture_lookup_cache();
	}
	return frames;
}
#endif
