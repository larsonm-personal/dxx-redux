// Native cached-merge transaction, accounting and lifecycle integration
extern "C" {
extern int r_texcount, r_mwall_cache_hits, r_mwall_cache_misses;
int ogl_get_texture_bytes(void);
void ogl_freetexture(ogl_texture *);
}
static int merge_cache_free_calls;
static void merge_cache_free(ogl_texture *texture)
{
	++merge_cache_free_calls;
	ogl_freetexture(texture);
}
static void merge_cache_source(ogl_texture &texture, int width, int height, int source)
{
	ogl_init_texture(&texture, width, height, OGL_FLAG_ALPHA);
	texture.tw = width;
	texture.th = height;
	texture.u = texture.v = 1;
	texture.is_png = 1;
	glGenTextures(1, &texture.handle);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture.handle);
	std::vector<unsigned char> pixels(width * height * 4);
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x) {
			int i = (y * width + x) * 4;
			pixels[i] = (x * 13 + y * 3 + source * 51) % 256;
			pixels[i + 1] = (x * 5 + y * 17 + source * 73) % 256;
			pixels[i + 2] = source ? 190 : 45;
			pixels[i + 3] = source ? ((x + y) % 3 ? 180 : 0) : 255;
		}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glGenerateMipmap(GL_TEXTURE_2D);
	texture.has_mipmaps = 1;
}
static void merge_cache_frame()
{
	glActiveTexture(GL_TEXTURE0);
	g3_start_frame();
	glClearColor(0.03f, 0.07f, 0.11f, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	vms_vector position{};
	vms_matrix orientation = vmd_identity_matrix;
	g3_set_view_matrix(&position, &orientation, F1_0);
}
static void merge_cache_draw(grs_bitmap &bottom, grs_bitmap &overlay, int orient)
{
	g3s_point points[4]{};
#ifdef DXX_BUILD_DESCENT_II
	const g3s_point *list[4];
#else
	g3s_point *list[4];
#endif
	g3s_uvl uv[4]{};
	g3s_lrgb light[4]{};
	for (int i = 0; i < 4; ++i) {
		vms_vector world{ (i == 0 || i == 3) ? -F1_0 : F1_0, i < 2 ? F1_0 : -F1_0, 4 * F1_0 };
		g3_rotate_point(&points[i], &world);
		g3_project_point(&points[i]);
		list[i] = &points[i];
		uv[i].u = (i == 1 || i == 2) ? F1_0 : 0;
		uv[i].v = i >= 2 ? F1_0 : 0;
		uv[i].l = F1_0;
		light[i] = { F1_0, F1_0, F1_0 };
	}
	g3_draw_tmap_2(4, list, uv, light, &bottom, &overlay, orient);
	require(glGetError() == GL_NO_ERROR, "native cache draw");
}
static merged_wall_cached_texmerge_entry *merge_cache_entry(grs_bitmap *bitmap)
{
	static_assert(offsetof(merged_wall_cached_texmerge_entry, bitmap) == 0, "public cache entry bitmap is first");
	return reinterpret_cast<merged_wall_cached_texmerge_entry *>(bitmap);
}
static void test_merge_cache(nlohmann::json &results, int generation)
{
	const grs_bitmap original_bottom = GameBitmaps[1], original_overlay = GameBitmaps[2];
	g_debug_tex_overlay_active = 0;
	GameArg.DbgAltTexMerge = 0;
	ogl_aniso_level = 0;
	const int sizes[][4] = { { 64, 64, 64, 64 }, { 512, 512, 512, 512 }, { 64, 64, 512, 512 }, { 512, 512, 64, 64 }, { 45, 63, 127, 95 } };
	for (int size = 0; size < 5; ++size)
		for (int filter = 0; filter < 3; ++filter)
			for (int orient = 0; orient < 4; ++orient) {
				android_merged_wall_cached_texmerge_clear_cache();
				ogl_texture sources[2]{};
				merge_cache_source(sources[0], sizes[size][0], sizes[size][1], 0);
				merge_cache_source(sources[1], sizes[size][2], sizes[size][3], 1);
				GameBitmaps[1].gltexture = &sources[0];
				GameBitmaps[2].gltexture = &sources[1];
				GameBitmaps[1].bm_flags = BM_FLAG_RLE;
				GameBitmaps[1].avg_color = 23;
				GameBitmaps[2].bm_flags = BM_FLAG_TRANSPARENT;
				GameCfg.TexFilt = filter;
				g_android_draw_face_ctx = {};
				merge_cache_frame();
				const std::string background = readback_hash();
				int count = r_texcount, bytes = ogl_get_texture_bytes();
				auto start = gles3_shim_get_draw_stats();
				merge_cache_draw(GameBitmaps[1], GameBitmaps[2], orient);
				auto cold = gles3_shim_get_draw_stats();
				require(r_texcount == count + 1 && r_mwall_cache_hits == 0 && r_mwall_cache_misses == 1, "cold cache accounting");
				require(cold.calls - start.calls == 2 && cold.triangle_vertices - start.triangle_vertices == 8, "cold compositor and scene draws");
				std::string first = readback_hash();
				require(first != background, "native cached scene changes pixels");
				int slot = -1;
				auto *bitmap = android_merged_wall_cached_texmerge_try_reuse_cache(&GameBitmaps[1], &GameBitmaps[2], orient, &slot);
				require(bitmap && slot == 0, "new cache entry admission");
				auto *entry = merge_cache_entry(bitmap);
				const int width = (std::max) (sources[0].w, sources[1].w), height = (std::max) (sources[0].h, sources[1].h);
				require(bitmap->bm_w == width && bitmap->bm_h == height && bitmap->bm_flags == 0 && bitmap->avg_color == 23, "cached bitmap dimensions/flags/color");
				require(entry->texture->bytes == width * height * 2 && ogl_get_texture_bytes() == bytes + width * height * 2, "native texture byte accounting");
				require(entry->texture->has_mipmaps == (filter > 0), "cached mipmap policy");
				GLuint handle = entry->texture->handle;
				glClearColor(0.03f, 0.07f, 0.11f, 1);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				int hits = r_mwall_cache_hits, misses = r_mwall_cache_misses;
				auto before_reuse = gles3_shim_get_draw_stats();
				merge_cache_draw(GameBitmaps[1], GameBitmaps[2], orient);
				auto reused = gles3_shim_get_draw_stats();
				require(r_texcount == count + 1 && r_mwall_cache_hits == hits + 1 && r_mwall_cache_misses == misses, "cache reuse accounting");
				require(entry->texture->handle == handle && reused.calls - before_reuse.calls == 1, "reuse retains texture without compositor draw");
				require(first == readback_hash(), "cold and cached scene pixels");
				results.push_back({ { "context", generation }, { "kind", "matrix" }, { "size", size }, { "filter", filter }, { "orient", orient }, { "scene_sha256", first }, { "width", width }, { "height", height }, { "texture_bytes", entry->texture->bytes }, { "cold_draws", cold.calls - start.calls }, { "reuse_draws", reused.calls - before_reuse.calls }, { "created_textures", r_texcount - count }, { "reused_same_texture", true } });
				g3_end_frame();
				android_merged_wall_cached_texmerge_clear_cache();
				require(r_texcount == count && ogl_get_texture_bytes() == bytes && !glIsTexture(handle), "clear frees cached native allocation");
				for (auto &source : sources) glDeleteTextures(1, &source.handle);
			}
	// Fill the actual global 32-entry cache with distinct bitmap identities
	ogl_texture sources[2]{};
	merge_cache_source(sources[0], 64, 64, 0);
	merge_cache_source(sources[1], 64, 64, 1);
	GameBitmaps[1].gltexture = &sources[0];
	GameBitmaps[1].bm_flags = 0;
	grs_bitmap overlays[34]{};
	for (auto &bitmap : overlays) {
		bitmap = original_overlay;
		bitmap.gltexture = &sources[1];
		bitmap.bm_flags = BM_FLAG_TRANSPARENT;
	}
	GameCfg.TexFilt = 0;
	android_merged_wall_cached_texmerge_clear_cache();
	merge_cache_frame();
	int count = r_texcount, bytes = ogl_get_texture_bytes();
	merged_wall_cached_texmerge_entry *entries[32]{};
	for (int i = 0; i < 32; ++i) {
		merge_cache_draw(GameBitmaps[1], overlays[i], i % 4);
		int slot = -1;
		auto *bitmap = android_merged_wall_cached_texmerge_try_reuse_cache(&GameBitmaps[1], &overlays[i], i % 4, &slot);
		require(bitmap && slot == i && r_texcount == count + i + 1, "fill native cache in slot order");
		entries[i] = merge_cache_entry(bitmap);
	}
	require(ogl_get_texture_bytes() == bytes + 32 * 64 * 64 * 2, "full cache bytes");
	// Controlled published ages avoid wall-clock-dependent LRU ordering
	for (int i = 0; i < 32; ++i) entries[i]->last_time_used = i + 1;
	entries[0]->last_time_used = 1000;
	GLuint evicted = entries[1]->texture->handle;
	merge_cache_draw(GameBitmaps[1], overlays[32], 0);
	int slot = -1;
	require(android_merged_wall_cached_texmerge_try_reuse_cache(&GameBitmaps[1], &overlays[32], 0, &slot) && slot == 1, "evict least-recent published age");
	require((entries[1]->texture->handle == evicted || !glIsTexture(evicted)) && r_texcount == count + 32 && ogl_get_texture_bytes() == bytes + 32 * 64 * 64 * 2, "eviction frees and replaces native allocation");
	for (auto *entry : entries) entry->last_time_used = 0;
	merge_cache_draw(GameBitmaps[1], overlays[33], 1);
	slot = -1;
	require(android_merged_wall_cached_texmerge_try_reuse_cache(&GameBitmaps[1], &overlays[33], 1, &slot) && slot == 0, "equal-age eviction retains first-slot priority");
	g3_end_frame();
	android_merged_wall_cached_texmerge_clear_cache();
	require(r_texcount == count && ogl_get_texture_bytes() == bytes, "clear full cache accounting");
	results.push_back({ { "context", generation }, { "kind", "lifecycle" }, { "filled_slots", 32 }, { "lru_evicted_slot", 1 }, { "tie_evicted_slot", 0 }, { "texture_count_restored", true }, { "texture_bytes_restored", true } });
	// A real texture name without level-zero storage produces an incomplete FBO
	auto *entry = android_merged_wall_cached_texmerge_reserve_cache_entry(ogl_freetexture);
	entry->texture = ogl_get_free_texture();
	ogl_init_texture(entry->texture, 64, 64, OGL_FLAG_ALPHA);
	glGenTextures(1, &entry->texture->handle);
	glBindTexture(GL_TEXTURE_2D, entry->texture->handle);
	++r_texcount;
	GLuint failed = entry->texture->handle;
	merge_cache_free_calls = 0;
	require(!android_merged_wall_cached_texmerge_finalize_entry(entry, &GameBitmaps[1], &overlays[0], 0, 64, 64, 0, 0, 1, 0, 23, nullptr, &slot, merge_cache_free), "incomplete FBO rejected");
	require(merge_cache_free_calls == 1 && !entry->texture && entry->slot == -1 && !glIsTexture(failed) && r_texcount == count, "failed FBO frees and resets entry");
	require(ogl_get_texture_bytes() == bytes, "failed FBO byte accounting");
	results.push_back({ { "context", generation }, { "kind", "incomplete_fbo" }, { "free_calls", merge_cache_free_calls }, { "entry_reset", true }, { "texture_count_restored", true }, { "texture_bytes_restored", true } });
	for (auto &source : sources) glDeleteTextures(1, &source.handle);
	GameBitmaps[1] = original_bottom;
	GameBitmaps[2] = original_overlay;
	android_merged_wall_cached_texmerge_clear_cache();
	require(glGetError() == GL_NO_ERROR, "cache fixture cleanup");
}
