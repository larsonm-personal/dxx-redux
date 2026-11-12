// Paused original-hub integration: native geometry, visibility isolation and readback
unsigned camera_scene_device_controls()
{
	using namespace xl_compat;
	require(ConsoleObject && game_is_time_paused(), "Camera scene control requires a paused loaded level");
	LightningGlowState restore_gl;
	Readback readback;
	CameraTargetPattern sampler;
	struct State {
		int rear = Rear_view;
		GLuint sentinels[3]{};
		GLint textures[3]{};
		unsigned fx = 0, fx_calls = d_rand_get_stream_call_count(D_RNG_PRESENTATION);
		unsigned legacy_fx = 0, legacy_fx_calls = d_rand_get_stream_call_count(D_RNG_FX);
		State()
		{
			glGenTextures(3, sentinels);
			for (unsigned i = 0; i < 3; ++i) {
				glActiveTexture(GL_TEXTURE1 + i);
				glGetIntegerv(GL_TEXTURE_BINDING_2D, textures + i);
				glBindTexture(GL_TEXTURE_2D, sentinels[i]);
			}
			glActiveTexture(GL_TEXTURE0);
			require(d_rand_get_stream_state(D_RNG_FX, &legacy_fx), "Cannot preserve camera simulation-owned FX RNG");
			require(d_rand_get_stream_state(D_RNG_PRESENTATION, &fx), "Cannot preserve camera control effects RNG");
		}
		~State()
		{
			for (unsigned i = 0; i < 3; ++i) {
				glActiveTexture(GL_TEXTURE1 + i);
				glBindTexture(GL_TEXTURE_2D, textures[i]);
			}
			glDeleteTextures(3, sentinels);
			glActiveTexture(GL_TEXTURE0);
			Rear_view = rear;
			d_rand_set_stream_state(D_RNG_PRESENTATION, fx);
			d_rand_set_stream_call_count(D_RNG_PRESENTATION, fx_calls);
			d_rand_set_stream_state(D_RNG_FX, legacy_fx);
			d_rand_set_stream_call_count(D_RNG_FX, legacy_fx_calls);
		}
	} restore;
	static std::array<CameraTargetGpu, 2> targets;
	const std::vector<ubyte> visited_before(Automap_visited, Automap_visited + Num_segments);
	std::array<window_rendered_data, MAX_RENDERED_WINDOWS> windows;
	std::copy_n(Window_rendered_data, MAX_RENDERED_WINDOWS, windows.begin());
	unsigned simulation = 0;
	require(d_rand_get_stream_state(D_RNG_SIM, &simulation), "Cannot inspect camera control simulation RNG");
	const auto simulation_calls = d_rand_get_stream_call_count(D_RNG_SIM);
	auto *const caller_viewer = Viewer;
	auto *const caller_canvas = grd_curcanv;
	const auto caller_view = View_matrix;
	const auto caller_position = View_position;
	const auto caller_cockpit = PlayerCfg.CurrentCockpitMode;
	object camera = *ConsoleObject;
	camera.type = OBJ_CAMERA;
	unsigned cases = 0;
	for (unsigned i = 0; i < targets.size(); ++i) {
		auto &target = targets[i];
		require(!target.valid(), "Native scene image survived graphics-context loss");
		const auto spec = camera_target_spec(320, 180, i != 0);
		std::vector<unsigned char> first;
		for (int rear : { 0, 1 }) {
			Rear_view = rear;
			d_srand_stream(D_RNG_PRESENTATION, 12345);
			glActiveTexture(GL_TEXTURE3);
			const auto graphics = camera_target_gl_state();
			std::array<float, 16> projection, modelview;
			std::copy_n(gles3_shim_get_projection(), 16, projection.begin());
			std::copy_n(gles3_shim_get_modelview(), 16, modelview.begin());
			const auto matrix_mode = gles3_shim_get_matrix_mode();
			android_ogl_msaa_diagnostics msaa_before{}, msaa_after{};
			ogl_msaa_get_diagnostics(&msaa_before);
			const int bound_before = g_msaa_fbo_bound;
			capture_camera_scene(target, spec, camera, Render_zoom, true, true);
			require(graphics == camera_target_gl_state(), "Native scene capture leaked caller GL state");
			require(std::equal(projection.begin(), projection.end(), gles3_shim_get_projection()) &&
			            std::equal(modelview.begin(), modelview.end(), gles3_shim_get_modelview()) &&
			            matrix_mode == gles3_shim_get_matrix_mode(),
			        "Native scene capture leaked shim matrices");
			for (unsigned unit = 0; unit < 3; ++unit) {
				glActiveTexture(GL_TEXTURE1 + unit);
				GLint texture;
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
				require(GLuint(texture) == restore.sentinels[unit], "Native scene capture lost secondary texture bindings");
			}
			glActiveTexture(GL_TEXTURE0);
			ogl_msaa_get_diagnostics(&msaa_after);
			require(g_msaa_fbo_bound == bound_before && msaa_before.generation == msaa_after.generation &&
			            msaa_before.bound_frame_count == msaa_after.bound_frame_count && msaa_before.resolve_count == msaa_after.resolve_count &&
			            msaa_before.width == msaa_after.width && msaa_before.height == msaa_after.height &&
			            msaa_before.effective_samples == msaa_after.effective_samples && msaa_before.scene_error_count == msaa_after.scene_error_count,
			        "Native scene capture changed main MSAA allocation or accounting");
			require(target.valid() && !xl_camera_scene_active(), "Native camera scene did not complete its transaction");
			require(Viewer == caller_viewer && grd_curcanv == caller_canvas && PlayerCfg.CurrentCockpitMode == caller_cockpit &&
			            !std::memcmp(&View_matrix, &caller_view, sizeof(caller_view)) && !std::memcmp(&View_position, &caller_position, sizeof(caller_position)),
			        "Native camera scene leaked viewer, canvas or projection state");
			require(std::equal(visited_before.begin(), visited_before.end(), Automap_visited) &&
			            !std::memcmp(windows.data(), Window_rendered_data, sizeof(Window_rendered_data)),
			        "Camera scene altered automap or gameplay rendered-object history");
			unsigned after = 0;
			require(d_rand_get_stream_state(D_RNG_SIM, &after) && after == simulation && d_rand_get_stream_call_count(D_RNG_SIM) == simulation_calls,
			        "Camera scene consumed simulation randomness");
			const auto pixels = sampler.sample(target.texture(), spec.width, spec.height, 0);
			unsigned colored = 0;
			for (size_t p = 0; p < pixels.size(); p += 4) {
				colored += pixels[p] > 8 || pixels[p + 1] > 8 || pixels[p + 2] > 8;
				require(pixels[p + 3] == 255, "Camera scene color target did not retain opaque RGB alpha");
			}
			require(colored > unsigned(spec.width * spec.height / 8), "Native camera scene did not render visible mine geometry");
			if (rear) require(pixels == first, "Camera scene changed with player rear-view state or repeated rendering");
			else first = pixels;
			++cases;
		}
	}
	return cases;
}
