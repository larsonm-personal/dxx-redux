#ifdef DXX_BUILD_DESCENT_II
static const unsigned char base_alpha[4][4] = {
	{ 255, 128, 0, 64 }, { 0, 255, 192, 128 }, { 128, 0, 255, 254 }, { 64, 192, 128, 255 }
};
static void soft_base_pattern()
{
	for (const char *owner : { "a", "b" }) {
		const std::string root = std::string(owner) + "/mods/pilot/textures/";
		image((root + "baseanim#0.tga").c_str(), 200, 128, 40, 20, 4, 16);
		std::fstream file(root + "baseanim#0.tga", std::ios::in | std::ios::out | std::ios::binary);
		file.seekp(17);
		file.put(0x28);
		for (int frame = 0; frame < 4; ++frame)
			for (int y = 0; y < 4; ++y)
				for (int x = 0; x < 4; ++x) {
					const unsigned char color[] = { 20, 40, static_cast<unsigned char>(owner[0] == 'a' ? 200 : 160), base_alpha[frame][(y >= 2 ? 2 : 0) + (x >= 2)] };
					file.write(reinterpret_cast<const char *>(color), 4);
				}
		require(bool(file), "write soft blastable-base strip");
		image((root + "baseplain.tga").c_str(), 200, 128, 40, 20);
		image((root + "basenear.tga").c_str(), 20, 128, 40, 200);
	}
}
static void test_soft_bases(const char *owner)
{
	const bool owned = strcmp(owner, "stock") != 0;
	fixture_xl_mission = true;
	fixture_hog = "pilot.hog";
	Game_mode = 0;
	grs_screen screen{};
	screen.sc_w = screen.sc_h = 64;
	screen.sc_aspect = F1_0;
	gr_init_canvas(&screen.sc_canvas, nullptr, BM_OGL, 64, 64);
	screen.sc_canvas.cv_fade_level = GR_FADE_OFF;
	grd_curscreen = &screen;
	gr_set_current_canvas(&screen.sc_canvas);
	grs_bitmap behind{}, front{};
	mask_solid(behind, 20, 220, 20);
	mask_solid(front, 20, 20, 220);
	Num_effects = 0;
	NumTextures = 7;
	Num_segments = 2;
	Highest_segment_index = 1;
	Num_walls = 2;
	Num_wall_anims = 1;
	for (int i = 0; i < 7; ++i) {
		GameBitmaps[80 + i] = {};
		mask_solid(GameBitmaps[80 + i], 10, 40, 30);
		Textures[i].index = 80 + i;
		if (i < 5) snprintf(piggy_game_bitmap_name(&GameBitmaps[80 + i]), 12, "baseanim#%d", i);
		else strcpy(piggy_game_bitmap_name(&GameBitmaps[80 + i]), i == 5 ? "baseplain" : "basenear");
	}
	WallAnims[0] = {};
	WallAnims[0].num_frames = 5;
	WallAnims[0].play_time = F1_0;
	WallAnims[0].open_sound = WallAnims[0].close_sound = -1;
	WallAnims[0].flags = WCF_TMAP1 | WCF_BLASTABLE;
	for (int i = 0; i < 5; ++i) WallAnims[0].frames[i] = i;
	for (int i = 0; i < 2; ++i) {
		Segments[i] = {};
		Segments[i].objects = -1;
		for (int side = 0; side < 6; ++side) {
			Segments[i].children[side] = -1;
			Segments[i].sides[side].wall_num = -1;
		}
		Segments[i].children[i] = 1 - i;
		Segments[i].sides[i].wall_num = i;
		Walls[i] = {};
		Walls[i].segnum = Walls[i].sidenum = i;
		Walls[i].type = WALL_BLASTABLE;
		Walls[i].linked_wall = -1;
		Walls[i].clip_num = 0;
		Walls[i].hps = WALL_HPS;
	}
	auto context = [&](int texture) {
		Segments[0].sides[0].tmap_num = texture;
		Segments[0].sides[0].tmap_num2 = 0;
		g_android_draw_face_ctx = {};
		g_android_draw_face_ctx.valid = 1;
		g_android_draw_face_ctx.tmap1 = texture;
	};
	auto start = [&]() {
		g3_start_frame();
		vms_vector pos{};
		vms_matrix matrix = vmd_identity_matrix;
		g3_set_view_matrix(&pos, &matrix, F1_0);
		glViewport(0, 0, 64, 64);
		glDisable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	};
	// Existing mission drawing remains a stock fallback, even with eligible artwork.
	context(0);
	start();
	mask_quad(GameBitmaps[80], nullptr, 0, 3 * F1_0, 4 * F1_0);
	unsigned char pixel[4];
	glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
	require(pixel[0] == 10 && pixel[1] == 40, "normal world path still rejects soft base admission");
	g3_end_frame();
	for (int stock = 0; stock < 5; ++stock) {
		const int frame = stock * 3 / 4;
		for (int filter : { 0, 1, 2 }) {
			GameCfg.TexFilt = filter;
			context(stock);
			start();
			const auto saved_wall = Walls[0];
			const auto saved_clip = WallAnims[0];
			const auto sim = d_rand_get_stream_call_count(D_RNG_SIM), fx = d_rand_get_stream_call_count(D_RNG_FX);
			mask_quad(behind, nullptr, 0, 6 * F1_0, 6 * F1_0);
			require(bool(mask_quad(GameBitmaps[80 + stock], nullptr, 0, 3 * F1_0, 4 * F1_0, F1_0, XL_SOFT_BASE_OPAQUE)) == owned, "explicit opaque pass ownership");
			// An opaque foreground object must win over both soft and opaque coverage.
			mask_quad(front, nullptr, 0, F1_0 / 4, 2 * F1_0);
			const GLboolean before_depth = GL_TRUE;
			glEnable(GL_BLEND);
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
			glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_ADD);
			require(bool(mask_quad(GameBitmaps[80 + stock], nullptr, 0, 3 * F1_0, 4 * F1_0, F1_0, XL_SOFT_BASE_PARTIAL)) == owned, "explicit partial pass ownership");
			GLboolean depth;
			GLint src, dst, eq;
			glGetBooleanv(GL_DEPTH_WRITEMASK, &depth);
			glGetIntegerv(GL_BLEND_SRC_RGB, &src);
			glGetIntegerv(GL_BLEND_DST_RGB, &dst);
			glGetIntegerv(GL_BLEND_EQUATION_RGB, &eq);
			require(depth == before_depth && glIsEnabled(GL_BLEND) && src == GL_SRC_ALPHA && dst == GL_ONE && eq == GL_FUNC_REVERSE_SUBTRACT, "soft pass restores caller blend and depth state");
			glDisable(GL_BLEND);
			glBlendEquation(GL_FUNC_ADD);
			unsigned char pixels[64 * 64 * 4];
			glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
			for (int y : { 48, 16 })
				for (int x : { 16, 48 }) {
					const unsigned a = owned ? base_alpha[frame][(y < 32 ? 2 : 0) + (x >= 32)] : 0;
					const int red = owner[0] == 'a' ? 200 : 160;
					const int expected[] = { red * int(a) / 255 + 20 * int(255 - a) / 255, 40 * int(a) / 255 + 220 * int(255 - a) / 255, 20 };
					const auto *p = pixels + (y * 64 + x) * 4;
					const bool correct = abs(int(p[0]) - expected[0]) <= 3 && abs(int(p[1]) - expected[1]) <= 3 && abs(int(p[2]) - expected[2]) <= 3;
					if (!correct) fprintf(stderr, "soft base owner=%s stock=%d filter=%d xy=%d,%d alpha=%u got=%u,%u,%u expected=%d,%d,%d\n", owner, stock, filter, x, y, a, p[0], p[1], p[2], expected[0], expected[1], expected[2]);
					require(correct, "opaque and partial coverage compose exactly once");
				}
			const auto *center = pixels + (32 * 64 + 32) * 4;
			require(center[2] > 180 && center[0] < 80, "foreground object depth survives soft pass");
			mask_quad(front, nullptr, 0, 3 * F1_0, 2 * F1_0);
			glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
			require(pixel[2] > 180 && pixel[0] < 80, "ordinary opaque draw after partial pass proves shader mode reset");
			if (owned && filter == 2) {
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				mask_quad(behind, nullptr, 0, 6 * F1_0, 6 * F1_0);
				for (int pass : { XL_SOFT_BASE_OPAQUE, XL_SOFT_BASE_PARTIAL })
					require(mask_quad(GameBitmaps[80 + stock], nullptr, 0, 3 * F1_0, 4 * F1_0, F1_0, pass, 64), "minified base pass");
				glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
				int alpha = 0, color[3] = {};
				const int rgb[] = { owner[0] == 'a' ? 200 : 160, 40, 20 };
				for (int q = 0; q < 4; ++q) {
					alpha += base_alpha[frame][q];
					for (int c = 0; c < 3; ++c) color[c] += rgb[c] * base_alpha[frame][q] / 255;
				}
				alpha /= 4;
				for (int c = 0; c < 3; ++c) {
					color[c] /= 4;
					color[c] += (c == 1 ? 220 : 20) * (255 - alpha) / 255;
				}
				require(abs(int(pixel[0]) - color[0]) <= 3 && abs(int(pixel[1]) - color[1]) <= 3 && abs(int(pixel[2]) - color[2]) <= 3, "final mip remains partial, premultiplied and drawn once");
			}
			require(!memcmp(&saved_wall, &Walls[0], sizeof(wall)) && !memcmp(&saved_clip, &WallAnims[0], sizeof(wclip)) && sim == d_rand_get_stream_call_count(D_RNG_SIM) && fx == d_rand_get_stream_call_count(D_RNG_FX), "base rendering preserves authoritative state and RNG");
			g3_end_frame();
		}
	}
	if (owned) {
		// Two partial surfaces need explicit far-to-near submission. Reverse order
		// is a negative control, not evidence for automatic scene sorting.
		unsigned char ordered[4], reversed[4];
		for (bool reverse : { false, true }) {
			start();
			mask_quad(behind, nullptr, 0, 6 * F1_0, 6 * F1_0);
			for (int n = 0; n < 2; ++n) {
				const int texture = (reverse ? 1 - n : n) + 5;
				context(texture);
				require(mask_quad(GameBitmaps[80 + texture], nullptr, 0, 3 * F1_0, (texture == 5 ? 5 : 4) * F1_0, F1_0, XL_SOFT_BASE_PARTIAL), "explicit static soft base selection");
			}
			glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, reverse ? reversed : ordered);
			g3_end_frame();
		}
		require(ordered[2] > ordered[0] && reversed[0] > reversed[2], "ordered soft layers and reversed-order negative control differ predictably");
	}
	if (owned) {
		// Drive damage mappings through native wall code, without adding a clock.
		Segments[0].sides[0].tmap_num = Segments[1].sides[1].tmap_num = 0;
		Walls[0].hps = Walls[1].hps = WALL_HPS;
		Walls[0].flags = Walls[1].flags = 0;
		for (int step = 0; step < 4; ++step) {
			wall_damage(&Segments[0], 0, (step ? 20 : 30) * F1_0);
			const int mapped = Segments[0].sides[0].tmap_num;
			require(mapped >= 0 && mapped < 5 && mapped * 3 / 4 == step, "native blast damage applies expected replacement phase");
			context(mapped);
			start();
			mask_quad(behind, nullptr, 0, 6 * F1_0, 6 * F1_0);
			GameCfg.TexFilt = 0;
			for (int pass : { XL_SOFT_BASE_OPAQUE, XL_SOFT_BASE_PARTIAL }) require(mask_quad(GameBitmaps[80 + mapped], nullptr, 0, 3 * F1_0, 4 * F1_0, F1_0, pass), "native blastable mapping draws through explicit passes");
			glReadPixels(16, 48, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
			const int a = base_alpha[step][0], r = (owner[0] == 'a' ? 200 : 160) * a / 255 + 20 * (255 - a) / 255;
			require(abs(int(pixel[0]) - r) <= 3, "rendered phase follows native damage mapping");
			g3_end_frame();
		}
		require(Walls[0].flags & WALL_BLASTED, "native damage, not rendering, destroys wall");
		ogl_smash_png_textures();
		context(0);
		start();
		WallAnims[0].num_frames = 2;
		GLint bindings[3];
		for (int unit = 0; unit < 3; ++unit) {
			glActiveTexture(GL_TEXTURE0 + unit);
			glBindTexture(GL_TEXTURE_2D, behind.gltexture->handle);
			bindings[unit] = behind.gltexture->handle;
		}
		glActiveTexture(GL_TEXTURE1);
		for (int pass : { XL_SOFT_BASE_OPAQUE, XL_SOFT_BASE_PARTIAL }) {
			require(!mask_quad(GameBitmaps[80], nullptr, 0, 3 * F1_0, 4 * F1_0, F1_0, pass), "overlong source rejects after upload without drawing");
			GLint active;
			glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
			require(active == GL_TEXTURE1, "rejected base selection restores active unit");
			for (int unit = 0; unit < 3; ++unit) {
				glActiveTexture(GL_TEXTURE0 + unit);
				GLint bound;
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
				require(bound == bindings[unit], "rejected base selection restores each texture unit");
			}
			glActiveTexture(GL_TEXTURE1);
		}
		glActiveTexture(GL_TEXTURE0);
		WallAnims[0].num_frames = 5;
		g3_end_frame();
	}
	for (int negative = 0; negative < 7; ++negative) {
		context(0);
		start();
		fixture_xl_mission = negative != 0;
		fixture_hog = negative == 1 ? "other.hog" : "pilot.hog";
		Game_mode = negative == 2 ? GM_MULTI : 0;
		if (negative == 3) g_android_draw_face_ctx.tmap2 = 1;
		if (negative == 4) screen.sc_canvas.cv_fade_level = 16;
		if (negative == 5) glDisable(GL_DEPTH_TEST);
		require(!mask_quad(GameBitmaps[80], nullptr, 0, 3 * F1_0, 4 * F1_0, F1_0, negative == 6 ? 3 : XL_SOFT_BASE_PARTIAL), "unsupported context rejects explicit base pass");
		g3_end_frame();
		screen.sc_canvas.cv_fade_level = GR_FADE_OFF;
	}
	fixture_xl_mission = true;
	fixture_hog = "pilot.hog";
	Game_mode = 0;
	android_merged_wall_clear_draw_face_context();
	for (int i = 0; i < 7; ++i) ogl_freebmtexture(&GameBitmaps[80 + i]);
	ogl_freebmtexture(&behind);
	ogl_freebmtexture(&front);
	Num_effects = NumTextures = Num_walls = Num_wall_anims = 0;
	grd_curscreen = nullptr;
	grd_curcanv = nullptr;
}
#endif
