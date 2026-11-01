// Exercise production frame setup/teardown inside retained camera targets
unsigned camera_native_pass_controls(std::vector<unsigned char> &retained)
{
	using namespace xl_compat;
	LightningGlowState restore_gl;
	struct NativeState {
		grs_canvas *canvas = grd_curcanv;
		grs_screen *screen = grd_curscreen;
		int width = Canvas_width, height = Canvas_height;
		GLfloat projection[16], modelview[16], merged[2][16];
		GLint mode;
		GLuint programs[2]{ ogl_prog_tex2, ogl_prog_tex2m };
		NativeState()
		{
#ifdef ANDROID
			mode = gles3_shim_get_matrix_mode();
			std::copy_n(gles3_shim_get_projection(), 16, projection);
			std::copy_n(gles3_shim_get_modelview(), 16, modelview);
#else
			glGetIntegerv(GL_MATRIX_MODE, &mode);
			glGetFloatv(GL_PROJECTION_MATRIX, projection);
			glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
#endif
			for (unsigned i = 0; i < 2; ++i)
				if (programs[i]) glGetUniformfv(programs[i], glGetUniformLocation(programs[i], "umat"), merged[i]);
		}
		~NativeState()
		{
			grd_curcanv = canvas;
			grd_curscreen = screen;
			Canvas_width = width;
			Canvas_height = height;
			glMatrixMode(GL_PROJECTION);
			glLoadMatrixf(projection);
			glMatrixMode(GL_MODELVIEW);
			glLoadMatrixf(modelview);
			glMatrixMode(mode);
			for (unsigned i = 0; i < 2; ++i)
				if (programs[i]) {
					lightning_gpu_program(programs[i]);
					glUniformMatrix4fv(glGetUniformLocation(programs[i], "umat"), 1, GL_FALSE, merged[i]);
				}
		}
	} native;
	grs_canvas canvas{};
	grs_screen screen{};
	screen.sc_w = 640;
	screen.sc_h = 480;
	screen.sc_canvas.cv_bitmap.bm_w = 640;
	screen.sc_canvas.cv_bitmap.bm_h = 480;
	grd_curscreen = &screen;
	grd_curcanv = &canvas;
	CameraTargetPattern pattern;
	static std::array<CameraTargetGpu, 2> targets;
	unsigned cases = 0;
	for (unsigned i = 0; i < targets.size(); ++i) {
		auto &target = targets[i];
		if (target.valid()) throw std::runtime_error("Native camera target survived context loss");
		const auto spec = camera_target_spec(63, 47, i != 0);
		canvas.cv_bitmap.bm_x = 3;
		canvas.cv_bitmap.bm_y = 5;
		canvas.cv_bitmap.bm_w = short(Canvas_width = spec.width - 7);
		canvas.cv_bitmap.bm_h = short(Canvas_height = spec.height - 9);
		canvas.cv_fade_level = GR_FADE_OFF;
		canvas.cv_blend_func = GR_BLEND_NORMAL;
		const auto before = camera_target_gl_state();
#ifdef ANDROID
		android_ogl_msaa_diagnostics msaa_before{}, msaa_after{};
		ogl_msaa_get_diagnostics(&msaa_before);
		const int bound_before = g_msaa_fbo_bound;
		if (!msaa_before.create_complete || msaa_before.effective_samples < 2)
			throw std::runtime_error("Native camera control requires an allocated main MSAA target");
#endif
		target.capture(spec, [&] {
			GLint expected_fbo, actual_fbo, viewport[4];
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &expected_fbo);
			ogl_start_frame();
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &actual_fbo);
			glGetIntegerv(GL_VIEWPORT, viewport);
			if (expected_fbo != actual_fbo || viewport[0] != 3 || viewport[1] != 4 || viewport[2] != Canvas_width || viewport[3] != Canvas_height)
				throw std::runtime_error("Native frame setup replaced camera FBO or viewport");
			ogl_prepare_framebuffer_readback();
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &actual_fbo);
			if (actual_fbo != expected_fbo) throw std::runtime_error("Native readback resolved the main framebuffer during camera capture");
			pattern.paint(spec.width, spec.height, [&](int buffer) { target.select(buffer); });
			ogl_end_frame();
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &actual_fbo);
			glGetIntegerv(GL_VIEWPORT, viewport);
			if (actual_fbo != expected_fbo || viewport[0] || viewport[1] || viewport[2] != spec.width || viewport[3] != spec.height)
				throw std::runtime_error("Native frame teardown lost camera target dimensions");
		});
#ifdef ANDROID
		ogl_msaa_get_diagnostics(&msaa_after);
		if (g_msaa_fbo_bound != bound_before || msaa_before.generation != msaa_after.generation ||
		    msaa_before.bound_frame_count != msaa_after.bound_frame_count || msaa_before.resolve_count != msaa_after.resolve_count ||
		    msaa_before.width != msaa_after.width || msaa_before.height != msaa_after.height ||
		    msaa_before.effective_samples != msaa_after.effective_samples || msaa_before.scene_error_count != msaa_after.scene_error_count)
			throw std::runtime_error("Native camera pass changed main MSAA allocation or accounting");
#endif
		if (camera_target_gl_state() != before || xl_camera_target_active_dimensions(nullptr, nullptr))
			throw std::runtime_error("Native camera pass leaked capture scope or GL state");
		for (unsigned color = 0; color < 2; ++color) {
			const auto pixels = pattern.sample(target.texture(color), spec.width, spec.height, 0);
			const auto inside = color ? std::array<int, 4>{ 128, 160, 191, 255 } : std::array<int, 4>{ 223, 32, 16, 255 };
			for (unsigned channel = 0; channel < 4; ++channel)
				if (std::abs(int(pixels[channel]) - inside[channel]) > 1) throw std::runtime_error("Native camera pass did not retain rendered color");
			retained.insert(retained.end(), pixels.begin(), pixels.end());
			++cases;
		}
	}
	return cases;
}
