// Runs inside the particle owner's existing color/depth framebuffer
static void transparency_gpu_controls(xl_compat::ParticleTexture &texture, const std::array<std::array<unsigned char, 4>, 4> &cells, const std::array<unsigned char, 4> &background)
{
	unsigned comparisons = 0;
	for (int material = 0; material < 3; ++material) {
		Bitmap base(2, 2, true), overlay(2, 2, true);
		for (int p = 0; p < 4; ++p) {
			base.data[p * 4] = 57;
			base.data[p * 4 + 1] = 131;
			base.data[p * 4 + 2] = 83;
			base.data[p * 4 + 3] = material == 2 ? (p % 2 ? 255 : 0) : 128;
			if (material == 1 && !(p % 2)) {
				overlay.data[p * 4] = 120;
				overlay.data[p * 4 + 1] = 88;
				overlay.data[p * 4 + 2] = 128;
			}
		}
		base.bitmap.bm_flags = BM_FLAG_TRANSPARENT | (material == 2 ? xl_compat::pog_see_through : 0);
		overlay.bitmap.bm_flags = BM_FLAG_TRANSPARENT | (material == 1 ? BM_FLAG_SUPER_TRANSPARENT : 0);
		upload(base, 0);
		upload(overlay, 0);
		const bool queued = xl_compat::transparent_face(base.bitmap.bm_flags, overlay.bitmap.bm_flags, false, false, 1);
		check(queued == (material != 2), "GPU material selected the wrong transparency pass");
		g3s_point vertices[4]{};
		const g3s_point *points[4]{};
		g3s_uvl uv[4]{};
		g3s_lrgb light[4]{};
		const int xy[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
		for (int i = 0; i < 4; ++i) {
			vertices[i].p3_vec = { (xy[i][0] * 2 - 1) * F1_0, (xy[i][1] * 2 - 1) * F1_0, F1_0 / 4 };
			points[i] = &vertices[i];
			uv[i].u = xy[i][0] * F1_0;
			uv[i].v = xy[i][1] * F1_0;
			light[i].r = light[i].g = light[i].b = F1_0;
		}
		for (int distance : { 5, 15, 30, 35 })
			for (int frame : { 0, 2 }) {
				glViewport(0, 0, 16, 16);
				glMatrixMode(GL_PROJECTION);
				glLoadIdentity();
				glScalef(1, 1, -1);
				glMatrixMode(GL_MODELVIEW);
				glLoadIdentity();
				GLfloat projection[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1 };
				ogl_prog_set_matrix(projection);
				glClearColor(background[0] / 255.f, background[1] / 255.f, background[2] / 255.f, 1);
				glClearDepth(1);
				glDepthMask(GL_TRUE);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_LEQUAL);
				glDisable(GL_CULL_FACE);
				glEnable(GL_ALPHA_TEST);
				glAlphaFunc(GL_GEQUAL, .02f);
				glEnable(GL_BLEND);
				glBlendEquation(GL_FUNC_ADD);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				xl_compat::ParticleView view{};
				view.basis = { { 65536, 0, 0 }, { 0, 65536, 0 } };
				view.rotate = false;
				xl_compat::FireParticle particle{};
				particle.width = particle.height = particle.decay = 1;
				particle.frame = frame;
				auto quad = xl_compat::fire_particle_quad(particle, view, 128, 128);
				for (auto &vertex : quad.vertices) vertex.position[2] = -distance / 80.f;
				const auto wall = [&] { g3_draw_tmap_2(4, points, uv, light, &base.bitmap, &overlay.bitmap, 0); };
				std::vector<xl_compat::TransparencyPosition> positions;
				if (queued) positions.push_back({ { 0, 0, 20 * 65536 }, 0 });
				else wall();
				glDepthMask(GL_FALSE);
				positions.push_back({ { 0, 0, distance * 65536 }, 10 * 65536 });
				for (auto index : xl_compat::transparency_draw_order(positions, {}, 60 * 65536)) {
					if (queued && index == 0) wall();
					else texture.draw(&quad, 1);
				}
				std::array<unsigned char, 16 * 16 * 4> actual;
				std::array<float, 16 * 16> depth;
				glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
				glReadPixels(0, 0, 16, 16, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
				for (unsigned p = 0; p < 256; ++p) {
					const bool hole = material != 0 && p % 16 < 8;
					const double alpha = material == 2 ? 1 : 128. / 255;
					const bool particle_shown = queued || hole || distance <= 20;
					const bool wall_first = !queued || distance < 30;
					const std::array<double, 4> wall_color{ 57. / 255, 131. / 255, 83. / 255, alpha };
					for (unsigned c = 0; c < 4; ++c) {
						double want = background[c] / 255.;
						const auto wall_blend = [&] { if (!hole) want = wall_color[c] * alpha + want * (1 - alpha); };
						if (wall_first) wall_blend();
						if (particle_shown) {
							const double value = cells[unsigned(frame)][c] / 255.;
							want = frame ? value * want : value + want * (1 - value);
						}
						if (!wall_first) wall_blend();
						check(std::abs(int(actual[p * 4 + c]) - int(want * 255 + .5)) <= 2, "Mixed merged-wall/particle pixel differs from independent blend/order equation");
					}
					check(std::abs(depth[p] - ((!queued && !hole) ? .625f : 1.f)) < .00001f, "Mixed transparency lost opaque depth or wrote blended depth");
				}
				check(glGetError() == GL_NO_ERROR, "Mixed transparency produced a GL error");
				++comparisons;
			}
	}
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	std::cout << "Production mixed transparency GPU: " << comparisons << " merged-wall/fire/smoke scenes, biased depth ties, super-key holes, see-through opaque cutouts and full color/depth comparisons passed\n";
}
