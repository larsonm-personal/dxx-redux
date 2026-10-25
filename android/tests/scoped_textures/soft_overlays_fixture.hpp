#ifdef DXX_BUILD_DESCENT_II
static void soft_overlay_pattern()
{
	image("a/mods/pilot/textures/softover#0.tga", 200, 128, 40, 20, 4, 16);
	// Three color/mask frames exceed the shared 32 MiB GPU budget after
	// successful uploads, exercising cleanup and binding recovery, not driver OOM.
	image("a/mods/pilot/textures/softfail#0.tga", 200, 128, 40, 20, 1024, 3072);
	std::fstream failure("a/mods/pilot/textures/softfail#0.tga", std::ios::in | std::ios::out | std::ios::binary);
	const unsigned char key[] = { 128, 88, 120, 255 };
	for (int frame = 0; frame < 3; ++frame) {
		failure.seekp(18 + frame * 1024 * 1024 * 4);
		failure.write(reinterpret_cast<const char *>(key), 4);
	}
	require(bool(failure), "write bounded mid-upload failure source");
	for (const char *owner : { "a", "b" })
		for (const char *name : { "softwall", "softanim#0", "softplain" }) {
			const bool animation = !strcmp(name, "softanim#0"), plain = !strcmp(name, "softplain");
			const std::string path = std::string(owner) + "/mods/pilot/textures/" + name + ".tga";
			image(path.c_str(), 200, 128, 40, 20, 4, animation ? 16 : 4);
			std::fstream file(path, std::ios::in | std::ios::out | std::ios::binary);
			file.seekp(17);
			file.put(0x28);
			for (int frame = 0; frame < (animation ? 4 : 1); ++frame)
				for (int y = 0; y < 4; ++y)
					for (int x = 0; x < 4; ++x) {
						const unsigned char soft[] = { 20, 40, static_cast<unsigned char>(owner[0] == 'a' ? 200 : 160), static_cast<unsigned char>(animation ? 64 + (frame % 3) * 64 : 128) };
						const unsigned char zero[] = { 255, 0, 255, 0 }, key[] = { 128, 88, 120, 255 }, opaque[] = { 20, 40, 200, 255 };
						file.write(reinterpret_cast<const char *>(plain || (x < 2 && y < 2) ? soft : y < 2 ? zero
						                                                                         : x < 2   ? key
						                                                                                   : opaque),
						           4);
					}
			require(bool(file), "write soft overlay source");
		}
}

static void test_soft_overlays(const char *owner)
{
	const bool owned = strcmp(owner, "stock") != 0;
	grs_screen screen{};
	screen.sc_w = screen.sc_h = 64;
	screen.sc_aspect = F1_0;
	gr_init_canvas(&screen.sc_canvas, nullptr, BM_OGL, 64, 64);
	grd_curscreen = &screen;
	gr_set_current_canvas(&screen.sc_canvas);
	grs_bitmap behind{};
	mask_solid(behind, 20, 20, 220);
	auto &base = GameBitmaps[70];
	base = {};
	mask_solid(base, 20, 220, 20);
	NumTextures = 2;
	Textures[0].index = 70;
	for (int i = 0; i < 4; ++i) {
		GameBitmaps[71 + i] = {};
		mask_solid(GameBitmaps[71 + i], 10, 40, 30);
	}
	if (!strcmp(owner, "a")) {
		const int previous_limit = ogl_max_texture_size;
		ogl_max_texture_size = 1024;
		for (const char *name : { "softover#0", "softfail#0" }) {
			ogl_smash_png_textures();
			Num_effects = 0;
			NumTextures = 4;
			for (int i = 1; i < 4; ++i) Textures[i].index = 70 + i;
			Num_segments = Num_walls = Num_wall_anims = 1;
			Highest_segment_index = 0;
			Segments[0] = {};
			Segments[0].sides[0].wall_num = 0;
			Segments[0].sides[0].tmap_num2 = 1;
			Walls[0] = {};
			Walls[0].type = WALL_DOOR;
			WallAnims[0] = {};
			WallAnims[0].num_frames = !strcmp(name, "softover#0") ? 2 : 3;
			for (int i = 0; i < 3; ++i) WallAnims[0].frames[i] = i + 1;
			snprintf(piggy_game_bitmap_name(&GameBitmaps[71]), 12, "%s", name);
			fixture_xl_mission = true;
			fixture_hog = "pilot.hog";
			Game_mode = 0;
			screen.sc_canvas.cv_fade_level = GR_FADE_OFF;
			screen.sc_canvas.cv_blend_func = GR_BLEND_NORMAL;
			g_android_draw_face_ctx = {};
			g_android_draw_face_ctx.valid = 1;
			g_android_draw_face_ctx.tmap2 = 1;
			int before = 0;
			for (int i = 0; i < OGL_TEXTURE_LIST_SIZE; ++i) before += ogl_texture_list[i].handle > 0;
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
			for (int repeat = 0; repeat < 2; ++repeat) {
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				mask_quad(base, &GameBitmaps[71], 0, 3 * F1_0, 4 * F1_0);
				unsigned char pixel[4];
				glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
				require(pixel[0] == 10 && pixel[1] == 40 && pixel[2] == 30, "rejected soft mapping/upload draws stock on first and cached attempt");
				for (int unit = 0; unit < 2; ++unit) {
					glActiveTexture(GL_TEXTURE0 + unit);
					GLint handle = 0;
					glGetIntegerv(GL_TEXTURE_BINDING_2D, &handle);
					require(handle == int(GameBitmaps[70 + unit].gltexture->handle), "rejected selector restores both texture bindings");
				}
				glActiveTexture(GL_TEXTURE0);
			}
			int after = 0;
			for (int i = 0; i < OGL_TEXTURE_LIST_SIZE; ++i) after += ogl_texture_list[i].handle > 0;
			if (!strcmp(name, "softfail#0")) require(before == after, "mid-upload budget failure releases all color and mask objects");
			g3_end_frame();
			android_merged_wall_clear_draw_face_context();
		}
		ogl_smash_png_textures();
		ogl_max_texture_size = previous_limit;
		Num_walls = Num_wall_anims = 0;
		NumTextures = 2;
	}
	for (int source = 0; source < 3; ++source) {
		Num_effects = source == 1 ? 1 : 0;
		Effects[0] = {};
		auto &effect = Effects[0];
		effect.changing_wall_texture = 1;
		effect.changing_object_texture = effect.crit_clip = -1;
		effect.vc.num_frames = 4;
		effect.vc.frame_time = F1_0 / 4;
		effect.vc.play_time = F1_0;
		for (int i = 0; i < 4; ++i) {
			effect.vc.frames[i].index = 71 + i;
			if (source == 1) snprintf(piggy_game_bitmap_name(&GameBitmaps[71 + i]), 12, "softanim#%d", i);
			else snprintf(piggy_game_bitmap_name(&GameBitmaps[71 + i]), 12, "%s", source ? "softplain" : "softwall");
		}
		for (int frame = 0; frame < (source == 1 ? 4 : 1); ++frame)
			for (int filter : { 0, 1, 2 })
				for (int orient = 0; orient < 4; ++orient)
					for (int negative = 0; negative < 7; ++negative) {
						if (negative && (frame || filter || orient)) continue;
						fixture_xl_mission = true;
						fixture_hog = "pilot.hog";
						effect.frame_count = frame;
						effect.time_left = effect.vc.frame_time - 1;
						Textures[1].index = 71 + frame;
						if (source == 1) effect_apply_bitmap_state(0);
						fixture_xl_mission = negative != 1;
						fixture_hog = negative == 2 ? "other.hog" : "pilot.hog";
						Game_mode = negative == 3 ? GM_MULTI : 0;
						base.gltexture->is_png = negative == 4;
						screen.sc_canvas.cv_fade_level = negative == 5 ? 16 : GR_FADE_OFF;
						screen.sc_canvas.cv_blend_func = negative == 6 ? GR_BLEND_ADDITIVE_A : GR_BLEND_NORMAL;
						GameCfg.TexFilt = filter;
						g_android_draw_face_ctx = {};
						g_android_draw_face_ctx.valid = 1;
						g_android_draw_face_ctx.tmap1 = 0;
						g_android_draw_face_ctx.tmap2 = 1;
						const auto saved = effect;
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
						auto surface = [&]() { mask_quad(base, &GameBitmaps[71 + frame], orient, 3 * F1_0, 4 * F1_0); };
						mask_quad(behind, nullptr, 0, 5 * F1_0, 5 * F1_0);
						surface();
						unsigned char pixels[64 * 64 * 4], reversed[sizeof(pixels)];
						glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
						const bool admitted = owned && !negative;
						// Top-left, top-right, bottom-left, bottom-right after each UV rotation.
						const char *quadrants[] = { "SGBR", "GRSB", "RBGS", "BSRG" };
						for (int y : { 48, 16 })
							for (int x : { 16, 48 }) {
								const char q = source == 2 ? 'S' : quadrants[orient][(y < 32 ? 2 : 0) + (x >= 32)];
								const int alpha = source == 1 ? 64 + (frame % 3) * 64 : 128;
								const int red = owner[0] == 'a' ? 200 : 160;
								int expected[3] = { 10, 40, 30 };
								if (admitted) {
									if (q == 'S') {
										expected[0] = red * alpha / 255 + 20 * (255 - alpha) / 255;
										expected[1] = 40 * alpha / 255 + 220 * (255 - alpha) / 255;
										expected[2] = 20;
									} else if (q == 'G') {
										expected[0] = 20;
										expected[1] = 220;
										expected[2] = 20;
									} else if (q == 'B') {
										expected[0] = 20;
										expected[1] = 20;
										expected[2] = 220;
									} else {
										expected[0] = 200;
										expected[1] = 40;
										expected[2] = 20;
									}
								}
								const auto *p = pixels + (y * 64 + x) * 4;
								if (negative < 5) {
									const bool correct = abs(int(p[0]) - expected[0]) <= 3 && abs(int(p[1]) - expected[1]) <= 3 && abs(int(p[2]) - expected[2]) <= 3;
									if (!correct) fprintf(stderr, "soft owner=%s source=%d frame=%d filter=%d orient=%d negative=%d xy=%d,%d got=%u,%u,%u expected=%d,%d,%d\n", owner, source, frame, filter, orient, negative, x, y, p[0], p[1], p[2], expected[0], expected[1], expected[2]);
									require(correct, "production soft source-over and super-hole composition");
								}
							}
						GLint handle = 0;
						glActiveTexture(GL_TEXTURE1);
						glGetIntegerv(GL_TEXTURE_BINDING_2D, &handle);
						if (!admitted) require(handle == int(GameBitmaps[71 + frame].gltexture->handle), "unsupported material/context retains stock overlay");
						glActiveTexture(GL_TEXTURE0);
						glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
						surface();
						mask_quad(behind, nullptr, 0, 5 * F1_0, 5 * F1_0);
						glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, reversed);
						if (!negative) require(!memcmp(pixels, reversed, sizeof(pixels)), "opaque-base soft composite has submission-order-independent depth");
						if (admitted && filter == 2 && !orient) {
							surface();
							GLint handles[2]{}, levels[2][2]{};
							const int count = source == 2 ? 1 : 2;
							for (int i = 0; i < count; ++i) {
								glActiveTexture(GL_TEXTURE1 + i);
								glGetIntegerv(GL_TEXTURE_BINDING_2D, &handles[i]);
								require(handles[i] && glIsTexture(handles[i]), "soft color/mask pair is live");
								glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, &levels[i][0]);
								glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, &levels[i][1]);
								glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 2);
								glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 2);
							}
							glActiveTexture(GL_TEXTURE0);
							glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
							mask_quad(behind, nullptr, 0, 5 * F1_0, 5 * F1_0);
							surface();
							unsigned char pixel[4];
							glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
							const int red = owner[0] == 'a' ? 200 : 160;
							require(source == 2 ? abs(int(pixel[0]) - (red * 128 / 255 + 20 * 127 / 255)) <= 3 && abs(int(pixel[1]) - 129) <= 3 : pixel[2] > 180 && pixel[0] < 80, "final mip retains premultiplied soft color and conservative separate holes");
							for (int i = 0; i < count; ++i) {
								glActiveTexture(GL_TEXTURE1 + i);
								glBindTexture(GL_TEXTURE_2D, handles[i]);
								glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, levels[i][0]);
								glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, levels[i][1]);
							}
							glActiveTexture(GL_TEXTURE0);
						}
						require(!memcmp(&saved, &effect, sizeof(effect)) && sim == d_rand_get_stream_call_count(D_RNG_SIM) && fx == d_rand_get_stream_call_count(D_RNG_FX), "soft presentation preserves effect state and RNG");
						g3_end_frame();
						android_merged_wall_clear_draw_face_context();
					}
	}
	// A cached soft source must never be reused as a base or standalone material.
	fixture_xl_mission = true;
	fixture_hog = "pilot.hog";
	Game_mode = 0;
	screen.sc_canvas.cv_fade_level = GR_FADE_OFF;
	screen.sc_canvas.cv_blend_func = GR_BLEND_NORMAL;
	Num_effects = 0;
	Textures[1].index = 71;
	for (bool paired : { false, true }) {
		g_android_draw_face_ctx = {};
		g_android_draw_face_ctx.valid = 1;
		g_android_draw_face_ctx.tmap1 = 1;
		g3_start_frame();
		vms_vector position{};
		vms_matrix matrix = vmd_identity_matrix;
		g3_set_view_matrix(&position, &matrix, F1_0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		mask_quad(GameBitmaps[71], paired ? &base : nullptr, 0, 3 * F1_0, 4 * F1_0);
		glActiveTexture(GL_TEXTURE0);
		GLint handle = 0;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &handle);
		require(handle == int(GameBitmaps[71].gltexture->handle), "cached soft overlay cannot activate as base or standalone material");
		g3_end_frame();
		android_merged_wall_clear_draw_face_context();
	}

	base.gltexture->is_png = 0;
	for (int i = 0; i < 5; ++i) ogl_freebmtexture(&GameBitmaps[70 + i]);
	ogl_freebmtexture(&behind);
	Num_effects = NumTextures = 0;
	grd_curscreen = nullptr;
	grd_curcanv = nullptr;
}
#endif
