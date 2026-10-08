// Actual native glyph and automap-style ordered line integration
static void batch_clear()
{
	g3_start_frame();
	glClearColor(0.05f, 0.05f, 0.05f, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
}
static std::string batch_text(int count)
{
	std::string text;
	for (int i = 0; i < count; ++i) {
		if (i && i % 32 == 0) text += '\n';
		text += static_cast<char>('A' + i % 26);
	}
	return text;
}
static void test_ogl_batches(nlohmann::json &results, int generation, grs_font &mono, bool baseline)
{
	g_debug_tex_overlay_active = 0;
	grs_font color{};
	std::vector<unsigned char> color_pixels(95 * 5 * 7);
	for (size_t i = 0; i < color_pixels.size(); ++i) color_pixels[i] = i % 5 == 0 ? 255 : static_cast<unsigned char>(10 + (i / 35) % 40);
	color.ft_w = 5;
	color.ft_h = 7;
	color.ft_minchar = 32;
	color.ft_maxchar = 126;
	color.ft_flags = FT_COLOR;
	color.ft_data = color_pixels.data();
	ogl_init_font(&color);
	struct glyph_case {
		std::string text;
		int glyphs;
		int extra_draws;
	};
	const glyph_case cases[] = { { "", 0, 0 }, { "A", 1, 0 }, { "Batch glyphs", 12, 0 }, { "one\ntwo", 6, 0 }, { std::string("A\1\37B\4C"), 3, 0 }, { std::string("A\3B"), 2, 1 }, { "Scale", 5, 0 }, { "Scale", 5, 0 }, { "Centered", 8, 0 }, { "Color", 5, 0 }, { batch_text(255), 255, 0 }, { batch_text(256), 256, 0 }, { batch_text(257), 257, 0 }, { batch_text(513), 513, 0 } };
	for (int scenario = 0; scenario < 14; ++scenario) {
		gr_set_curfont(scenario == 9 ? &color : &mono);
		FNTScaleX = FNTScaleY = scenario == 6 ? 0.5f : scenario == 7 ? 2.0f
		                                                             : 1.0f;
		for (int pass = 0; pass < 2; ++pass) {
			batch_clear();
			g3_end_frame();
			gr_set_fontcolor(12, -1);
			auto before = gles3_shim_get_draw_stats();
			gr_ustring(scenario == 8 ? 0x8000 : 8, 8, cases[scenario].text.c_str());
			auto after = gles3_shim_get_draw_stats();
			int expected = cases[scenario].extra_draws + (cases[scenario].glyphs ? (baseline ? 1 : (cases[scenario].glyphs + 255) / 256) : 0);
			require(after.calls - before.calls == static_cast<uint64_t>(expected), "glyph batch draw count");
			require(after.triangle_vertices - before.triangle_vertices == static_cast<uint64_t>(cases[scenario].glyphs * 6 + cases[scenario].extra_draws * 4), "glyph vertex count");
			if (pass) {
				require(after.stream_allocations == before.stream_allocations, "no glyph stream growth after warmup");
				results.push_back({ { "kind", "glyph" }, { "context", generation }, { "scenario", scenario }, { "sha256", readback_hash() }, { "draws", after.calls - before.calls }, { "vertices", after.triangle_vertices - before.triangle_vertices }, { "stream_growth", after.stream_allocations - before.stream_allocations } });
			}
		}
	}
	FNTScaleX = FNTScaleY = 1;
	gr_set_curfont(&mono);
	const int counts[] = { 0, 1, 511, 512, 513, 1025 };
	for (int count : counts) {
		std::string reference;
		for (int batched = 0; batched < 2; ++batched)
			for (int pass = 0; pass < 2; ++pass) {
				batch_clear();
				auto before = gles3_shim_get_draw_stats();
				if (batched) g3_start_line_batch(count);
				// The dim pass precedes the bright pass, matching native automap ordering
				for (int phase = 0; phase < 2; ++phase)
					for (int i = phase ? count / 2 : 0; i < (phase ? count : count / 2); ++i) {
						gr_setcolor(phase ? 30 + i % 10 : 12);
						g3s_point a{}, b{};
						a.p3_vec = { -F1_0, -F1_0 + (i % 32) * F1_0 / 16, 3 * F1_0 };
						b.p3_vec = { F1_0, a.p3_vec.y, 3 * F1_0 };
						require(g3_draw_line(&a, &b), "native ordered line draw");
					}
				if (batched) g3_end_line_batch();
				auto after = gles3_shim_get_draw_stats();
				uint64_t expected = batched ? (count ? (baseline ? 1 : (count + 511) / 512) : 0) : count;
				require(after.calls - before.calls == expected, "line batch draw count");
				require(after.line_vertices - before.line_vertices == static_cast<uint64_t>(count * 2), "line vertex count");
				g3_end_frame();
				if (pass) {
					require(after.stream_allocations == before.stream_allocations, "no line stream growth after warmup");
					std::string hash = readback_hash();
					if (!batched) reference = hash;
					else {
						require(reference == hash, "ordered line pixels match native unbatched draws");
						results.push_back({ { "kind", "line" }, { "context", generation }, { "count", count }, { "sha256", hash }, { "draws", after.calls - before.calls }, { "vertices", after.line_vertices - before.line_vertices }, { "stream_growth", after.stream_allocations - before.stream_allocations } });
					}
				}
			}
	}
	std::string reference;
	for (int batched = 0; batched <= (baseline ? 0 : 1); ++batched)
		for (int pass = 0; pass < 2; ++pass) {
			batch_clear();
			g3_end_frame();
			auto before = gles3_shim_get_draw_stats();
			if (batched) ogl_ubitmap_batch_begin(3);
			for (int i = 0; i < 3; ++i) ogl_ubitmapm_cs(8 + i * 16, 8 + i * 8, 64, 64, &GameBitmaps[1 + i % 2], -1, F1_0);
			if (batched) ogl_ubitmap_batch_end();
			auto after = gles3_shim_get_draw_stats();
			require(after.calls - before.calls == 3, "texture transitions preserve three ordered draws");
			if (pass) {
				require(after.stream_allocations == before.stream_allocations, "no transition stream growth after warmup");
				std::string hash = readback_hash();
				if (!batched) reference = hash;
				else require(reference == hash, "texture-transition pixels match native unbatched draws");
				if (batched || baseline) results.push_back({ { "kind", "textures" }, { "context", generation }, { "sha256", hash }, { "draws", after.calls - before.calls }, { "stream_growth", after.stream_allocations - before.stream_allocations } });
			}
		}
	grs_bitmap bitmap = color.ft_parent_bitmap;
	gr_free_bitmap_data(&bitmap);
	d_free(color.ft_bitmaps);
}
