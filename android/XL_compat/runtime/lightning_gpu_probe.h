// Android renderer control: retains an owned corona without admitting a level
std::shared_ptr<const xl_compat::ParticleImageResource> lightning_probe_image;
std::vector<unsigned char> lightning_probe_pixels;
void lightning_gpu_probe(const char *operation)
{
	using namespace xl_compat;
	require(Game_wind && ConsoleObject && !(Game_mode & GM_MULTI) && game_is_time_paused(), "Lightning GPU control requires paused singleplayer");
	if (!std::strcmp(operation, "capture")) {
		Inventory request;
		Object source{};
		source.type = 20;
		source.id = 1;
		source.render = 12;
		source.render_data = { 300, 50, 18, 5, 1, -1, 3, 0, 0, 8, 0, 2, 20, 0, 0, 0, 0, 1, 1, 1, 0, 0, 52, 149, 128, 1 };
		request.objects.push_back(source);
		lightning_probe_image = prepare_lightning_image(request);
		require(lightning_probe_image && lightning_probe_image->texture, "Lightning control did not prepare its owned corona");
		lightning_probe_pixels.clear();
		return;
	}
	if (!std::strcmp(operation, "release")) {
		discard_lightning_context(false);
		lightning_probe_image.reset();
		lightning_probe_pixels.clear();
		return;
	}
	require(!std::strcmp(operation, "gpu") && lightning_probe_image && gl_initialized, "Lightning GPU control lacks its original corona");
	require(glGetError() == GL_NO_ERROR, "Lightning GPU control requires a clean context");
	std::vector<unsigned char> pixels(64 * 64 * 4);
	unsigned changed = 0;
	GLfloat line_range[2];
	glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, line_range);
	{
		LightningGlowState restore;
		Readback readback;
		ParticleProbeTarget target;
		glBindRenderbuffer(GL_RENDERBUFFER, target.color);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 64, 64);
		struct NativeState {
			vms_vector eye = View_position;
			vms_matrix view = View_matrix;
			grs_screen *screen = grd_curscreen;
			grs_screen probe = *screen;
			GLuint depth = 0;
			unsigned random_state = 0, random_calls = d_rand_get_stream_call_count(D_RNG_FX);
			NativeState()
			{
				require(d_rand_get_stream_state(D_RNG_FX, &random_state) != 0, "Lightning control cannot preserve effects RNG");
				probe.sc_w = probe.sc_h = 64;
				grd_curscreen = &probe;
				View_position = {};
				View_matrix = { { 65536, 0, 0 }, { 0, 65536, 0 }, { 0, 0, 65536 } };
				glGenRenderbuffers(1, &depth);
				glBindRenderbuffer(GL_RENDERBUFFER, depth);
				glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 64, 64);
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
				d_srand_stream(D_RNG_FX, 12345);
			}
			~NativeState()
			{
				View_position = eye;
				View_matrix = view;
				grd_curscreen = screen;
				d_rand_set_stream_state(D_RNG_FX, random_state);
				d_rand_set_stream_call_count(D_RNG_FX, random_calls);
				glDeleteRenderbuffers(1, &depth);
			}
		} native;
		require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Lightning GPU control target is incomplete");
		glViewport(3, 5, 48, 43);
		const GLfloat projection[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -5001.f / 4999.f, -1, 0, 0, -10000.f / 4999.f, 0 };
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(projection);
		glMatrixMode(GL_MODELVIEW);
		glDepthMask(GL_TRUE);
		glClearDepthf(1);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		const std::vector<LightningPoint> nodes{ { -32768, -16384, 196608 }, { 0, 19661, 196608 }, { 32768, -6554, 196608 }, { 40000, 10000, 196608 } };
		LightningSceneItem item{};
		item.frame.core = lightning_core_geometry(nodes, [](const LightningVertex &p) { return p[2]; });
		item.frame.glow = lightning_glow_geometry(nodes, 1, .7f, true, { 0, 0, 0 }, 3, 4999);
		item.random_nodes = 1;
		item.width = .7f;
		item.color = { 0, 52.f / 255.f, 149.f / 255.f, 128.f / 255.f };
		const auto calls = d_rand_get_stream_call_count(D_RNG_FX);
		draw_lightning_scene_item(item, *lightning_probe_image->texture);
		finish_lightning_batch();
		require(d_rand_get_stream_call_count(D_RNG_FX) == calls + 3, "Lightning draw used the wrong effects RNG count");
		GLint framebuffer, view[4];
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);
		glGetIntegerv(GL_VIEWPORT, view);
		require(GLuint(framebuffer) == readback.framebuffer && view[0] == 3 && view[1] == 5 && view[2] == 48 && view[3] == 43, "Lightning draw lost caller target or viewport");
		glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		require(glGetError() == GL_NO_ERROR, "Lightning draw/readback raised a GPU error");
		for (unsigned p = 0; p < 64 * 64; ++p) changed += pixels[p * 4 + 2] != 0;
		require(changed > 0 && changed < 64 * 64, "Lightning GPU control produced no bounded visible glow");
	}
	require(glGetError() == GL_NO_ERROR, "Lightning GPU control restoration raised a GPU error");
	if (!lightning_probe_pixels.empty()) require(pixels == lightning_probe_pixels, "Lightning pixels changed across graphics context recreation");
	lightning_probe_pixels = std::move(pixels);
	++checks;
	report = nlohmann::json{ { "passed", true }, { "checks", checks }, { "lightning_changed", changed }, { "line_width_min", line_range[0] }, { "line_width_max", line_range[1] }, { "corona_width", lightning_probe_image->bitmap.width } }.dump();
}
