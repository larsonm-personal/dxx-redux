#ifdef DXX_BUILD_DESCENT_II
static void door_pattern()
{
	for (const char *owner : { "a", "b" }) {
		const std::string root = std::string(owner) + "/mods/pilot/textures/";
		image((root + "doorx#0.tga").c_str(), 0, 255, 40, 30, 4, 16);
		std::fstream file(root + "doorx#0.tga", std::ios::in | std::ios::out | std::ios::binary);
		file.seekp(17);
		file.put(0x28);
		for (int frame = 0; frame < 4; ++frame)
			for (int pixel = 0; pixel < 16; ++pixel) {
				const unsigned char rgba[] = { 30, 40, static_cast<unsigned char>((owner[0] == 'a' ? 50 : 100) + 30 * frame), 255 };
				file.write(reinterpret_cast<const char *>(rgba), 4);
			}
		require(bool(file), "write opaque wclip strip");
		image((root + "manydoor#0.tga").c_str(), 220, 255, 40, 30, 4, 24);
	}
}

static void test_door_animation(const char *owner)
{
	const bool owned = std::strcmp(owner, "stock") != 0;
	const int first_red = owner[0] == 'a' ? 50 : 100;
	grs_screen screen{};
	screen.sc_w = screen.sc_h = 64;
	screen.sc_aspect = F1_0;
	gr_init_canvas(&screen.sc_canvas, nullptr, BM_OGL, 64, 64);
	screen.sc_canvas.cv_fade_level = GR_FADE_OFF;
	grd_curscreen = &screen;
	gr_set_current_canvas(&screen.sc_canvas);
	Num_effects = 0;
	NumTextures = 8;
	Highest_segment_index = 1;
	Num_segments = 2;
	Num_walls = 2;
	Num_wall_anims = 1;
	for (int i = 0; i < 5; ++i) {
		auto &bitmap = GameBitmaps[30 + i];
		bitmap = {};
		mask_solid(bitmap, 10, 40, 30);
		std::snprintf(piggy_game_bitmap_name(&bitmap), 12, "doorx#%d", i);
		Textures[2 + i].index = 30 + i;
	}
	grs_bitmap base{};
	mask_solid(base, 5, 10, 15);
	auto setup = [&](bool overlay, bool automatic) {
		std::memset(Segments, 0, 2 * sizeof(segment));
		std::memset(Walls, 0, 2 * sizeof(wall));
		std::memset(ActiveDoors, 0, sizeof(ActiveDoors));
		Num_open_doors = 0;
		WallAnims[0] = {};
		auto &clip = WallAnims[0];
		clip.num_frames = 5;
		clip.play_time = F1_0;
		clip.open_sound = clip.close_sound = -1;
		clip.flags = overlay ? 0 : WCF_TMAP1;
		for (int i = 0; i < 5; ++i) clip.frames[i] = 2 + i;
		for (int i = 0; i < 2; ++i) {
			for (int side = 0; side < 6; ++side) {
				Segments[i].children[side] = -1;
				Segments[i].sides[side].wall_num = -1;
			}
			Segments[i].objects = -1;
			Segments[i].children[i] = 1 - i;
			Segments[i].sides[i].wall_num = i;
			Segments[i].sides[i].tmap_num = overlay ? 1 : 2;
			Segments[i].sides[i].tmap_num2 = overlay ? 2 : 0;
			Walls[i].segnum = Walls[i].sidenum = i;
			Walls[i].type = WALL_DOOR;
			Walls[i].linked_wall = -1;
			Walls[i].clip_num = 0;
			Walls[i].flags = automatic ? WALL_DOOR_AUTO : 0;
			Walls[i].hps = WALL_HPS;
		}
	};
	auto draw = [&](int seg, bool overlay, int source_frame, bool admitted, const char *message) {
		const wall before_walls[] = { Walls[0], Walls[1] };
		const active_door before_door = ActiveDoors[0];
		const int before_count = Num_open_doors;
		const auto sim = d_rand_get_stream_call_count(D_RNG_SIM), fx = d_rand_get_stream_call_count(D_RNG_FX);
		const auto frame_time = FrameTime;
		const auto game_time = GameTime64;
		const auto before_side = Segments[seg].sides[seg];
		g3_start_frame();
		vms_vector position{};
		vms_matrix matrix = vmd_identity_matrix;
		g3_set_view_matrix(&position, &matrix, F1_0);
		g_android_draw_face_ctx = {};
		g_android_draw_face_ctx.valid = 1;
		g_android_draw_face_ctx.seg = g_android_draw_face_ctx.side = seg;
		g_android_draw_face_ctx.tmap1 = before_side.tmap_num;
		g_android_draw_face_ctx.tmap2 = before_side.tmap_num2;
		const int mapped = overlay ? (before_side.tmap_num2 & 0x3fff) : before_side.tmap_num;
		auto &bitmap = GameBitmaps[Textures[mapped].index];
		glViewport(0, 0, 64, 64);
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		if (overlay) mask_quad(base, &bitmap, seg, 3 * F1_0, 4 * F1_0);
		else mask_quad(bitmap, nullptr, 0, 3 * F1_0, 4 * F1_0);
		unsigned char pixel[4];
		glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
		const int expected = admitted && owned ? first_red + 30 * source_frame : 10;
		if (std::abs(int(pixel[0]) - expected) > 2)
			std::fprintf(stderr, "%s owner=%s expected=%d rgba=%u,%u,%u,%u\n", message, owner, expected, pixel[0], pixel[1], pixel[2], pixel[3]);
		require(std::abs(int(pixel[0]) - expected) <= 2 && std::abs(int(pixel[1]) - 40) <= 2, message);
		require(!std::memcmp(before_walls, Walls, sizeof(before_walls)) && !std::memcmp(&before_door, &ActiveDoors[0], sizeof(before_door)) &&
		            !std::memcmp(&before_side, &Segments[seg].sides[seg], sizeof(before_side)) && before_count == Num_open_doors,
		        "wclip rendering preserves native wall, side and active-door state");
		require(FrameTime == frame_time && GameTime64 == game_time && sim == d_rand_get_stream_call_count(D_RNG_SIM) && fx == d_rand_get_stream_call_count(D_RNG_FX),
		        "wclip rendering preserves timers and SIM/FX RNG");
		g3_end_frame();
		android_merged_wall_clear_draw_face_context();
	};
	fixture_hog = "pilot.hog";
	fixture_xl_mission = true;
	for (bool overlay : { false, true })
		for (int filter : { 0, 1, 2 }) {
			GameCfg.TexFilt = filter;
			setup(overlay, false);
			draw(0, overlay, 0, true, "closed door replacement endpoint");
			wall_open_door(&Segments[0], 0);
			const int selected[] = { 0, 0, 1, 2, 3 };
			FrameTime = F1_0 / 5;
			for (int stock = 1; stock < 5; ++stock) {
				do_door_open(0);
				require((overlay ? Segments[0].sides[0].tmap_num2 : Segments[0].sides[0].tmap_num) == 2 + stock, "native opening applies expected stock frame");
				draw(0, overlay, selected[stock], true, "opening follows applied frame");
				draw(1, overlay, selected[stock], true, "back side follows its applied frame");
			}
			require(Walls[0].state == WALL_DOOR_OPEN && !Num_open_doors, "native manual door completion unchanged");
			wall_close_door(&Segments[0], 0);
			for (int stock = 3; stock >= 0; --stock) {
				do_door_close(0);
				require((overlay ? Segments[0].sides[0].tmap_num2 : Segments[0].sides[0].tmap_num) == 2 + stock, "native closing applies expected stock frame");
				draw(0, overlay, selected[stock], true, "closing follows applied frame");
			}
			require(Walls[0].state == WALL_DOOR_CLOSED && !Num_open_doors, "native closed completion unchanged");
			setup(overlay, false);
			wall_open_door(&Segments[0], 0);
			FrameTime = 2 * (F1_0 / 5);
			do_door_open(0);
			draw(0, overlay, 1, true, "mid-opening replacement");
			wall_close_door(&Segments[0], 0);
			draw(0, overlay, 1, true, "reversal retains last applied mapping before next tick");
			wall_open_door(&Segments[0], 0);
			draw(0, overlay, 1, true, "reverse again does not invent a presentation clock");
			FrameTime = F1_0 / 5;
			do_door_open(0);
			require((overlay ? Segments[0].sides[0].tmap_num2 : Segments[0].sides[0].tmap_num) == 5, "reversed opening advances native mapping");
			draw(0, overlay, 2, true, "replacement advances after opening reversal");
			wall_close_door(&Segments[0], 0);
			do_door_close(0);
			require((overlay ? Segments[0].sides[0].tmap_num2 : Segments[0].sides[0].tmap_num) == 3, "reversed closing advances native mapping");
			draw(0, overlay, 0, true, "replacement advances after closing reversal");
			setup(overlay, true);
			wall_open_door(&Segments[0], 0);
			FrameTime = 4 * (F1_0 / 5);
			do_door_open(0);
			require(Walls[0].state == WALL_DOOR_WAITING && Num_open_doors == 1, "automatic door enters native waiting state");
			for (int repeat = 0; repeat < 3; ++repeat) draw(0, overlay, 3, true, "waiting door remains on final frame");
			FrameTime = DOOR_WAIT_TIME + 1;
			wall_frame_process();
			require(Walls[0].state == WALL_DOOR_CLOSING && Num_open_doors == 1 && ActiveDoors[0].time == 0, "native waiting expiry starts automatic closing");
			draw(0, overlay, 3, true, "wait expiry preserves applied endpoint before closing tick");
			FrameTime = F1_0 / 5;
			for (int stock = 3; stock >= 0; --stock) {
				wall_frame_process();
				require((overlay ? Segments[0].sides[0].tmap_num2 : Segments[0].sides[0].tmap_num) == 2 + stock, "automatic close applies expected stock frame");
				draw(0, overlay, selected[stock], true, "automatic close follows applied frame");
			}
			require(Walls[0].state == WALL_DOOR_CLOSED && !Num_open_doors, "automatic door finishes closing natively");
			setup(overlay, false);
			Walls[0].type = Walls[1].type = WALL_BLASTABLE;
			WallAnims[0].flags |= WCF_BLASTABLE;
			wall_damage(&Segments[0], 0, 30 * F1_0);
			draw(0, overlay, 0, true, "blast damage retains native first damage frame");
			wall_damage(&Segments[0], 0, 20 * F1_0);
			draw(0, overlay, 1, true, "blast damage maps middle frame");
			wall_damage(&Segments[0], 0, 20 * F1_0);
			draw(0, overlay, 2, true, "blast damage maps later frame");
			wall_damage(&Segments[0], 0, 20 * F1_0);
			require(Walls[0].flags & WALL_BLASTED, "native damage alone destroys blastable wall");
			draw(0, overlay, 3, true, "destroyed mapping retains final authored frame");
		}
	setup(false, false);
	// The native wall record reader plus saved side mapping requires no new state.
	Segments[0].sides[0].tmap_num = 5;
	Walls[0].state = WALL_DOOR_CLOSING;
	rewind_memory_buffer saved{};
	rewind_file file;
	rewind_file_init_memory_write(&file, &saved);
	wall_write(&Walls[0], 20, &file);
	const auto mapped = Segments[0].sides[0].tmap_num;
	require(rewind_file_write(&file, &mapped, sizeof(mapped), 1) == 1, "write mapped side control");
	Walls[0] = {};
	Segments[0].sides[0].tmap_num = 2;
	rewind_file_init_memory_read(&file, saved.data, saved.size);
	wall_read(&Walls[0], &file);
	require(rewind_file_read(&file, &Segments[0].sides[0].tmap_num, sizeof(mapped), 1) == 1, "restore mapped side control");
	draw(0, false, 2, true, "native wall record and restored mapping reconstruct replacement frame");
	rewind_memory_buffer_discard(&saved);
	fixture_xl_mission = false;
	draw(0, false, 2, false, "classic custom mission cannot activate wclip replacement");
	fixture_xl_mission = true;
	fixture_hog = "other.hog";
	draw(0, false, 2, false, "other selected HOG cannot consume wclip replacement");
	fixture_hog = "pilot.hog";
	Walls[0].type = WALL_CLOSED;
	draw(0, false, 2, false, "closed non-clip wall does not gain animation from filename");
	Walls[0].type = WALL_DOOR;
	WallAnims[0].frames[0] = 5;
	draw(0, false, 2, false, "ambiguous repeated stock mapping falls back");
	WallAnims[0].frames[0] = 2;
	std::strcpy(piggy_game_bitmap_name(&GameBitmaps[33]), "manydoor#3");
	draw(0, false, 2, false, "more source frames than stock clip remains unsupported");
	std::strcpy(piggy_game_bitmap_name(&GameBitmaps[33]), "doorx#3");
	WallAnims[0].flags = 0;
	draw(0, false, 2, false, "clip cannot replace the wrong texture slot");
	WallAnims[0].flags = WCF_TMAP1;
	WallAnims[0].num_frames = MAX_CLIP_FRAMES + 1;
	draw(0, false, 2, false, "invalid clip frame count falls back before indexing");
	WallAnims[0].num_frames = 5;
	Walls[0].segnum = 1;
	draw(0, false, 2, false, "mismatched wall ownership cannot select replacement");
	Walls[0].segnum = 0;
	const auto texture_zero = Textures[0];
	Textures[0] = Textures[2];
	WallAnims[0].flags = 0;
	WallAnims[0].frames[0] = 0;
	Segments[0].sides[0].tmap_num = 1;
	Segments[0].sides[0].tmap_num2 = 0x4000;
	draw(0, true, 0, false, "rotation-only absent overlay cannot activate texture zero");
	Textures[0] = texture_zero;
	WallAnims[0].flags = WCF_TMAP1;
	WallAnims[0].frames[0] = 2;
	Segments[0].sides[0].tmap_num = 5;
	Segments[0].sides[0].tmap_num2 = 0;
	std::vector<unsigned char> pog(36, 0);
	std::memcpy(pog.data(), "DPOG", 4);
	pog[4] = pog[8] = 1;
	pog[12] = 33;
	pog[23] = pog[24] = 2;
	std::ofstream pog_file("doortest.POG", std::ios::binary);
	pog_file.write(reinterpret_cast<const char *>(pog.data()), pog.size());
	pog_file.close();
	const int previous_count = Num_bitmap_files;
	Num_bitmap_files = 35;
	char level[] = "doortest.rl2";
	load_bitmap_replacements(level);
	require(piggy_bitmap_is_custom(&GameBitmaps[33]), "native POG owns wclip bitmap");
	mask_solid(GameBitmaps[33], 10, 40, 30);
	draw(0, false, 2, false, "POG overrides cached wclip replacement");
	free_bitmap_replacements();
	Num_bitmap_files = previous_count;
	GameBitmaps[33].bm_data = nullptr;
	for (int i = 0; i < 5; ++i) ogl_freebmtexture(&GameBitmaps[30 + i]);
	ogl_freebmtexture(&base);
	Num_effects = NumTextures = Num_walls = Num_wall_anims = Num_open_doors = 0;
	grd_curscreen = nullptr;
	grd_curcanv = nullptr;
}
#endif
