// Existing protected renderer owner: real wall binding and native merged draw
unsigned additive_wall_device_controls()
{
	require(Num_walls > 0, "Additive wall GPU control requires a native wall");
	struct Restore {
		wall saved = Walls[0];
		ubyte blend = grd_curcanv->cv_blend_func;
		int color = grd_curcanv->cv_color;
		~Restore()
		{
			Walls[0] = saved;
			xl_preview_end_face();
			grd_curcanv->cv_blend_func = blend;
			grd_curcanv->cv_color = color;
			ogl_set_blending();
		}
	} restore;
	struct Image {
		grs_bitmap bitmap{};
		xl_compat::PogBitmap source;
		Image(std::vector<unsigned char> pixels)
		{
			source.width = source.height = 2;
			source.truecolor = true;
			source.pixels = std::move(pixels);
			xl_compat::prepare_pog_rgba(source);
			bitmap.bm_w = bitmap.bm_h = 2;
			bitmap.bm_rowsize = 8;
			bitmap.bm_type = BM_RGBA;
			bitmap.bm_flags = source.flags & 7;
			bitmap.bm_data = source.pixels.data();
			ogl_loadbmtexture_f(&bitmap, 0, nullptr);
			glBindTexture(GL_TEXTURE_2D, bitmap.gltexture->handle);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
		~Image()
		{
			ogl_freebmtexture(&bitmap);
		}
	} base({ 51, 102, 153, 64, 51, 102, 153, 128, 51, 102, 153, 255, 51, 102, 153, 0 }),
	    overlay({ 204, 51, 26, 128, 120, 88, 128, 255, 204, 51, 26, 0, 204, 51, 26, 255 });
	const std::array<double, 4> background{ 19. / 255, 27. / 255, 43. / 255, 1 };
	g3s_point vertices[4]{};
	const g3s_point *points[4]{};
	g3s_uvl uv[4]{};
	g3s_lrgb light[4]{};
	const int corners[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
	for (unsigned i = 0; i < 4; ++i) {
		vertices[i].p3_vec = { (corners[i][0] * 2 - 1) * F1_0, (corners[i][1] * 2 - 1) * F1_0, 0 };
		points[i] = &vertices[i];
		uv[i].u = corners[i][0] * F1_0;
		uv[i].v = corners[i][1] * F1_0;
		light[i].r = light[i].g = light[i].b = F1_0;
	}
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	auto &wall = Walls[0];
	wall.type = WALL_CLOSED;
	wall.state = WALL_DOOR_CLOSED;
	wall.hps = 0;
	unsigned cases = 0;
	for (int orientation = 0; orientation < 4; ++orientation)
		for (int cloak : { 0, 6 })
			for (auto stock : { GR_BLEND_NORMAL, GR_BLEND_ADDITIVE_C })
				for (bool additive : { true, false })
					for (bool occluded : { false, true }) {
						wall.cloak_value = cloak;
						wall.flags = 16 | (additive ? xl_compat::wall_render_additive : 0);
						grd_curcanv->cv_blend_func = stock;
						xl_preview_begin_face(wall.segnum, wall.sidenum);
						require(bool(xl_preview_additive_face()) == additive, "Device wall binding lost its authored flag");
						ogl_set_blending();
						GLint src, dst;
						glGetIntegerv(GL_BLEND_SRC_RGB, &src);
						glGetIntegerv(GL_BLEND_DST_RGB, &dst);
						require(src == (additive || stock == GR_BLEND_ADDITIVE_C ? GL_ONE : GL_SRC_ALPHA) &&
						            dst == (additive ? GL_ONE_MINUS_SRC_COLOR : stock == GR_BLEND_ADDITIVE_C ? GL_ONE
						                                                                                     : GL_ONE_MINUS_SRC_ALPHA),
						        "Device blend cache retained the previous wall mode");
						glViewport(0, 0, 64, 64);
						glDisable(GL_SCISSOR_TEST);
						glDisable(GL_CULL_FACE);
						glEnable(GL_DEPTH_TEST);
						glDepthFunc(GL_LEQUAL);
						glDepthMask(GL_TRUE);
						glClearDepthf(occluded ? .25f : 1.f);
						glClearColor(float(background[0]), float(background[1]), float(background[2]), 1);
						glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
						glDepthMask(GL_FALSE);
						glEnable(GL_BLEND);
						glBlendEquation(GL_FUNC_ADD);
						g3_draw_tmap_2(4, points, uv, light, &base.bitmap, &overlay.bitmap, orientation);
						xl_preview_end_face();
						std::array<unsigned char, 64 * 64 * 4> pixels{};
						glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
						for (unsigned y = 0; y < 64; ++y)
							for (unsigned x = 0; x < 64; ++x) {
								const unsigned bx = x / 32, by = y / 32;
								const unsigned tx = orientation == 1 ? 1 - by : orientation == 2 ? 1 - bx
								                                            : orientation == 3   ? by
								                                                                 : bx;
								const unsigned ty = orientation == 1 ? bx : orientation == 2 ? 1 - by
								                                        : orientation == 3   ? 1 - bx
								                                                             : by;
								const auto *b = base.source.pixels.data() + 4 * (by * 2 + bx);
								const auto *t = overlay.source.pixels.data() + 4 * (ty * 2 + tx);
								const bool key = t[0] == 120 && t[1] == 88 && t[2] == 128;
								std::array<double, 4> value{};
								for (unsigned c = 0; c < 3; ++c) value[c] = (double(b[c]) * (1 - double(t[3]) / 255) + double(t[c]) * t[3] / 255) / 255;
								value[3] = (std::min) (1., double(b[3] + t[3]) / 255) * (31 - cloak) / 31.;
								if (occluded || key || value[3] < .005) value = background;
								else {
									const double alpha = value[3];
									for (unsigned c = 0; c < 4; ++c)
										value[c] = additive ? value[c] + background[c] * (1 - value[c]) : stock == GR_BLEND_ADDITIVE_C ? (std::min) (1., value[c] + background[c])
										                                                                                               : value[c] * alpha + background[c] * (1 - alpha);
								}
								for (unsigned c = 0; c < 4; ++c)
									if (std::abs(int(pixels[(y * 64 + x) * 4 + c]) - int(std::lround(value[c] * 255))) > 1)
										throw std::runtime_error("Additive wall device pixel differs: case=" + std::to_string(cases) + " x=" + std::to_string(x) + " y=" + std::to_string(y) + " channel=" + std::to_string(c));
							}
						require(glGetError() == GL_NO_ERROR, "Additive wall device draw raised a GL error");
						++cases;
					}
	gr_setcolor(BM_XRGB(0, 0, 0));
	require(PAL2Tr(grd_curcanv->cv_color) == 0 && PAL2Tg(grd_curcanv->cv_color) == 0 && PAL2Tb(grd_curcanv->cv_color) == 0,
	        "Flat additive control requires the native palette's exact black");
	grd_curcanv->cv_blend_func = GR_BLEND_NORMAL;
	for (int state : { WALL_DOOR_CLOAKING, WALL_DOOR_DECLOAKING })
		for (int cloak : { 0, 6, 30, 31 })
			for (bool additive : { true, false })
				for (bool occluded : { false, true }) {
					wall.state = state;
					wall.cloak_value = cloak;
					wall.flags = additive ? xl_compat::wall_render_additive : 0;
					require(xl_preview_begin_cloak_face(wall.segnum, wall.sidenum), "Native additive transition lost its flat-face path");
					ogl_set_blending();
					glDepthMask(GL_TRUE);
					glClearDepthf(occluded ? .25f : 1.f);
					glClearColor(float(background[0]), float(background[1]), float(background[2]), 1);
					glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
					glDepthMask(GL_FALSE);
					g3_draw_poly(4, points);
					xl_preview_end_face();
					std::array<unsigned char, 64 * 64 * 4> pixels{};
					glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
					const double alpha = 1. - double(cloak) / 31;
					for (unsigned p = 0; p < 64 * 64; ++p)
						for (unsigned c = 0; c < 3; ++c) {
							const auto expected = int(std::lround(255 * background[c] * (occluded || additive ? 1 : 1 - alpha)));
							if (std::abs(int(pixels[p * 4 + c]) - expected) > 1)
								throw std::runtime_error("Flat additive transition pixel differs: case=" + std::to_string(cases) + " pixel=" + std::to_string(p));
						}
					require(glGetError() == GL_NO_ERROR, "Flat additive transition raised a GL error");
					++cases;
				}
	return cases;
}
