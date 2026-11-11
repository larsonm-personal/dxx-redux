// Protected original-hub owner: captured geometry through native single/merged draws
unsigned camera_monitor_device_controls()
{
	using namespace xl_compat;
	require(ConsoleObject && game_is_time_paused(), "Camera monitor control requires a paused level");
	LightningGlowState restore_gl;
	Readback readback;
	const int segment = ConsoleObject->segnum, side = 0, owner = int(ConsoleObject - Objects);
	auto &face = Segments[segment].sides[side];
	struct Restore {
		object *actor = ConsoleObject;
		object saved_actor = *actor;
		int segment;
		std::array<uvl, 4> uv;
		grs_screen *screen = grd_curscreen;
		grs_screen probe = *screen;
		grs_canvas *canvas = grd_curcanv;
		ubyte fade = canvas->cv_fade_level, blend = canvas->cv_blend_func;
		std::array<float, 16> projection, modelview;
		int mode = gles3_shim_get_matrix_mode();
		unsigned fx = 0, fx_calls = d_rand_get_stream_call_count(D_RNG_FX);
		Restore(int s) : segment(s)
		{
			std::copy_n(Segments[s].sides[0].uvls, 4, uv.begin());
			std::copy_n(gles3_shim_get_projection(), 16, projection.begin());
			std::copy_n(gles3_shim_get_modelview(), 16, modelview.begin());
			require(d_rand_get_stream_state(D_RNG_FX, &fx), "Cannot preserve monitor effects RNG");
			probe.sc_w = probe.sc_h = 64;
			grd_curscreen = &probe;
		}
		~Restore()
		{
			xl_camera_renderer_clear();
			*actor = saved_actor;
			std::copy(uv.begin(), uv.end(), Segments[segment].sides[0].uvls);
			grd_curscreen = screen;
			canvas->cv_fade_level = fade;
			canvas->cv_blend_func = blend;
			glMatrixMode(GL_PROJECTION);
			glLoadMatrixf(projection.data());
			glMatrixMode(GL_MODELVIEW);
			glLoadMatrixf(modelview.data());
			glMatrixMode(mode);
			d_rand_set_stream_state(D_RNG_FX, fx);
			d_rand_set_stream_call_count(D_RNG_FX, fx_calls);
			xl_camera_controller_bind_level();
		}
	} restore(segment);
	Inventory level;
	level.segments.resize(Num_segments);
	level.objects.resize(Highest_object_index + 1);
	level.objects[owner].type = 16;
	ObjectTrigger trigger;
	trigger.object = owner;
	trigger.action.type = 16;
	trigger.action.targets.push_back({ segment, side });
	level.object_trigger_records.push_back(trigger);
	int ids[4];
	get_side_verts(ids, segment, side);
	CameraTargetPattern sampler;
	unsigned cases = 0;
	const int xy[4][2] = { { 0, 0 }, { 0, 1 }, { 1, 1 }, { 1, 0 } };
	for (int orientation = 0; orientation < 4; ++orientation) {
		// Reuse the existing pixel cases for CAMBOT, robot and effect attachments
		auto &actor = *ConsoleObject;
		actor = restore.saved_actor;
		actor.type = orientation < 2 ? 16 : orientation == 2 ? OBJ_ROBOT
		                                                     : 20;
		actor.id = orientation == 3 ? 1 : 0;
		actor.control_type = orientation < 2 ? 18 : orientation == 2 ? CT_AI
		                                                             : CT_NONE;
		actor.movement_type = orientation == 2 ? MT_PHYSICS : MT_NONE;
		actor.render_type = orientation == 3 ? RT_NONE : RT_POLYOBJ;
		if (orientation == 2) {
			actor.rtype.pobj_info.model_num = Robot_info[0].model_num;
			actor.pos.x += F1_0 / 4;
		}
		level.objects[owner].type = actor.type;
		level.objects[owner].id = level.objects[owner].reader_id = actor.id;
		xl_camera_controller_load(level);
		XlCameraControllerState attachment;
		require(xl_camera_controller_get(owner, attachment) && attachment.type == actor.type && attachment.id == actor.id,
		        "Native monitor did not retain its actual owner profile");
		std::array<CameraUV, 6> source_uv{};
		for (int i = 0; i < 4; ++i) {
			const int corner = (i + orientation) % 4;
			face.uvls[i].u = xy[corner][0] * F1_0;
			face.uvls[i].v = xy[corner][1] * F1_0;
			source_uv[i] = { float(xy[corner][0]), float(xy[corner][1]) };
		}
		xl_camera_renderer_load(level);
		xl_camera_renderer_frame(); // Establish dimensions before the first visible face
		require(xl_camera_monitor_face(segment, side), "Authored monitor binding was lost");
		xl_camera_monitor_begin(segment, side, 4, ids, 0);
		grs_bitmap original{};
		require(xl_camera_monitor_bitmap(0, &original) == &original, "Uncaptured monitor replaced its authored image");
		xl_camera_monitor_end();
		xl_camera_renderer_frame();
		xl_camera_monitor_begin(segment, side, 4, ids, 0);
		auto *image = xl_camera_monitor_bitmap(0, &original);
		require(image != &original && image->gltexture && image->bm_w == 32 && image->bm_h == 32,
		        "Visible camera did not capture its native half-resolution view");
		const auto captured = sampler.sample(image->gltexture->handle, 32, 32, 0);
		unsigned colored = 0;
		for (std::size_t i = 0; i < captured.size(); i += 4) {
			colored += captured[i] > 8 || captured[i + 1] > 8 || captured[i + 2] > 8;
			require(captured[i + 3] == 255, "Camera monitor source is not opaque");
		}
		require(colored > 128, "Camera monitor source lacks visible geometry");
		xl_camera_monitor_end();
		auto &original_base = GameBitmaps[Textures[0].index];
		ogl_loadbmtexture_f(&original_base, 0, nullptr);
		grs_bitmap background = original_base;
		background.bm_flags &= ~BM_FLAG_NO_LIGHTING;
		CameraAlignment expected;
		expected.align(source_uv, false, false, false, true, false, 64, 64, 32, 32);
		ParticleProbeTarget target;
		glBindRenderbuffer(GL_RENDERBUFFER, target.color);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 64, 64);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		grd_curcanv->cv_fade_level = GR_FADE_OFF;
		grd_curcanv->cv_blend_func = GR_BLEND_NORMAL;
		for (bool overlay : { false, true })
			for (int brightness : { F1_0, F1_0 / 2 })
				for (int split : { 0, 1, 2 }) {
					glViewport(0, 0, 64, 64);
					glDisable(GL_DEPTH_TEST);
					glDisable(GL_CULL_FACE);
					glDisable(GL_BLEND);
					glDisable(GL_SCISSOR_TEST);
					glClearColor(1, 0, 1, 1);
					glClear(GL_COLOR_BUFFER_BIT);
					for (int triangle = 0; triangle < (split ? 2 : 1); ++triangle) {
						const int diagonal02[2][3] = { { 0, 1, 2 }, { 0, 2, 3 } }, diagonal13[2][3] = { { 0, 1, 3 }, { 1, 2, 3 } };
						const int count = split ? 3 : 4;
						g3s_point points[4]{};
						const g3s_point *pointers[4]{};
						g3s_uvl uv[4]{};
						g3s_lrgb light[4]{};
						int vertices[4]{};
						for (int i = 0; i < count; ++i) {
							const int corner = split == 1 ? diagonal02[triangle][i] : split == 2 ? diagonal13[triangle][i]
							                                                                     : i;
							vertices[i] = ids[corner];
							points[i].p3_vec = { (xy[corner][0] * 2 - 1) * F1_0, (xy[corner][1] * 2 - 1) * F1_0, 0 };
							pointers[i] = points + i;
							uv[i].u = face.uvls[corner].u;
							uv[i].v = face.uvls[corner].v;
							light[i] = { brightness, brightness, brightness };
						}
						xl_camera_monitor_begin(segment, side, count, vertices, overlay ? 1 : 0);
						std::array<float, 8> independent{};
						for (int i = 0; i < 2 * count; ++i) independent[i] = float(i) + .125f;
						const auto untouched = independent;
						xl_camera_monitor_uv(overlay ? 0 : 1, count, independent.data());
						require(independent == untouched, "Camera monitor overwrote the other texture coordinate stream");
						auto *base = xl_camera_monitor_bitmap(0, &background);
						auto *top = overlay ? xl_camera_monitor_bitmap(1, &background) : nullptr;
						require(overlay ? base == &background && top == image : base == image,
						        "Camera monitor replaced the wrong texture layer");
						if (overlay) g3_draw_tmap_2(count, pointers, uv, light, base, top, orientation);
						else g3_draw_tmap(count, pointers, uv, light, base);
						xl_camera_monitor_end();
					}
					std::array<unsigned char, 64 * 64 * 4> pixels{};
					glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
					for (int y = 0; y < 64; ++y)
						for (int x = 0; x < 64; ++x) {
							const float fx = (float(x) + .5f) / 64, fy = (float(y) + .5f) / 64;
							const auto &a = expected.coordinates[0], &b = expected.coordinates[1], &d = expected.coordinates[3];
							const int sx = int((a.u + fx * (d.u - a.u) + fy * (b.u - a.u)) * 32) & 31;
							const int sy = int((a.v + fx * (d.v - a.v) + fy * (b.v - a.v)) * 32) & 31;
							for (int c = 0; c < 3; ++c) {
								const int value = int(std::lround(double(captured[(sy * 32 + sx) * 4 + c]) * brightness / F1_0));
								if (std::abs(int(pixels[(y * 64 + x) * 4 + c]) - value) > 1)
									throw std::runtime_error("Native camera monitor pixel differs: case=" + std::to_string(cases) + " x=" + std::to_string(x) + " y=" + std::to_string(y));
							}
						}
					require(glGetError() == GL_NO_ERROR, "Native camera monitor raised a GL error");
					++cases;
				}
		CameraTargetGpu::discard_context(false);
		xl_camera_monitor_begin(segment, side, 4, ids, 0);
		require(xl_camera_monitor_bitmap(0, &background) == &background, "Lost camera context retained a stale monitor image");
		xl_camera_monitor_end();
		xl_camera_renderer_frame();
		xl_camera_monitor_begin(segment, side, 4, ids, 0);
		auto *rebuilt = xl_camera_monitor_bitmap(0, &background);
		require(rebuilt != &background && rebuilt->gltexture && rebuilt->gltexture->handle,
		        "Visible monitor did not rebuild its lost camera target");
		const auto recovered = sampler.sample(rebuilt->gltexture->handle, 32, 32, 0);
		require(recovered.size() == captured.size() && glGetError() == GL_NO_ERROR, "Recovered camera monitor cannot be sampled");
		xl_camera_monitor_end();
		++cases;
	}
	return cases;
}
