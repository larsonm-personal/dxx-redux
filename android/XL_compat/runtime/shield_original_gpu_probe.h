// Original addon pixels through the retained draw binding; no mission admission
unsigned shield_original_device_controls(
    const std::array<std::shared_ptr<const xl_compat::ShieldImageResource>, 3> &images,
    std::array<std::unique_ptr<xl_compat::ShieldAddon>, 3> &addons, uint32_t ticks,
    std::vector<unsigned char> &retained)
{
	using namespace xl_compat;
	const auto require = [](bool condition, const char *message) { if (!condition) throw std::runtime_error(message); };
	LightningGlowState restore;
	struct Target {
		GLuint framebuffer = 0, buffers[2]{};
		GLint renderbuffer;
		Target()
		{
			glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
		}
		~Target()
		{
			glDeleteRenderbuffers(2, buffers);
			glDeleteFramebuffers(1, &framebuffer);
			glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
		}
	} target;
	glGenFramebuffers(1, &target.framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
	glGenRenderbuffers(2, target.buffers);
	glBindRenderbuffer(GL_RENDERBUFFER, target.buffers[0]);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 32, 32);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, target.buffers[0]);
	glBindRenderbuffer(GL_RENDERBUFFER, target.buffers[1]);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, target.buffers[1]);
	require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Original shield target incomplete");
	const std::array<uint64_t, 3> pixel_hashes{ 0xb762a08f7dddaceeULL, 0x3e3d91c43410cc4bULL, 0xa6097408e2a5fc49ULL };
	const std::array<float, 16> matrix{ .7f, 0, 0, 0, 0, .7f, 0, 0, 0, 0, .7f, 0, 0, 0, 0, 1 };
	const std::array<float, 3> color{ .7f, .8f, .9f };
	const HitRenderMatrix identity{ { { 65536, 0, 0 }, { 0, 65536, 0 }, { 0, 0, 65536 } } };
	CameraHitState state;
	state.points = { { { 0, 0, 65536 }, { -65536, 0, 0 }, { 65536, 0, 0 } } };
	unsigned cases = 0, visible = 0;
	for (unsigned index = 0; index < images.size(); ++index) {
		require(images[index] && addons[index], "Original shield resource was not captured");
		const auto &bitmap = images[index]->bitmap;
		require(bitmap.width == 128 && bitmap.height == 1024 && bitmap.frames == 8, "Original shield dimensions differ");
		uint64_t hash = 14695981039346656037ULL;
		for (auto byte : bitmap.pixels) hash = (hash ^ byte) * 1099511628211ULL;
		require(hash == pixel_hashes[index], "Original shield decoded pixels differ");
		for (unsigned frame = 0; frame < (index < 2 ? 8u : 1u); ++frame) {
			const uint32_t now = ticks + frame * 100;
			for (int quality : { 2, 3, 4 })
				for (auto filter : { ShieldFilter::nearest, ShieldFilter::linear, ShieldFilter::mip_nearest, ShieldFilter::trilinear })
					for (unsigned profile = 0; profile < 3; ++profile) {
						const bool expired = profile == 2, occluded = profile == 1;
						const int hit_time = int(now) - (expired ? 2000 : 0);
						state.times = { hit_time, hit_time, hit_time };
						const auto hit = camera_hit_hardware(state, 297534, int(now), 2000, identity);
						glViewport(0, 0, 32, 32);
						glDisable(GL_SCISSOR_TEST);
						glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
						glDepthMask(GL_TRUE);
#ifdef ANDROID
						glClearDepthf(occluded ? .1f : 1.f);
#else
						glClearDepth(occluded ? .1 : 1.);
#endif
						glClearColor(0, 0, 0, 0);
						glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
						// Expired hits must not consume an otherwise elapsed addon timer
						addons[index]->draw(now + (index == 2 ? cases * 100 : 0) + (expired ? 100 : 0), quality, hit, color, matrix, filter);
						require(addons[index]->current_frame() == frame, "Shield draw advanced the wrong addon frame");
						std::array<unsigned char, 4096> pixels{};
						glReadPixels(0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
						bool changed = false;
						for (auto byte : pixels) changed = changed || byte != 0;
						require(changed == (!expired && !occluded), "Original shield visibility/depth/expiry differs");
						visible += changed;
						retained.insert(retained.end(), pixels.begin(), pixels.end());
						require(glGetError() == GL_NO_ERROR, "Original shield draw raised a GL error");
						++cases;
					}
		}
	}
	// Replacement resets the bitmap frame, while its logical addon's clock survives
	ShieldAddon replacement(0);
	replacement.replace_image(images[0]);
	state.times = { 4000, 4000, 4000 };
	const auto hit = camera_hit_hardware(state, 297534, 4000, 2000, identity);
	const auto draw = [&](uint32_t now) { replacement.draw(now, 2, hit, color, matrix, ShieldFilter::linear); };
	draw(1000);
	draw(1100);
	require(replacement.current_frame() == 1, "Shield replacement control did not advance");
	replacement.replace_image(images[0]);
	require(replacement.current_frame() == 1, "Repeated shield binding reset its frame");
	replacement.replace_image(images[1]);
	require(replacement.current_frame() == 0, "New shield bitmap retained the old frame");
	draw(1200);
	require(replacement.current_frame() == 1, "Shield bitmap replacement reset the addon clock");
	replacement.replace_image(nullptr);
	replacement.replace_image(images[0]);
	draw(1300);
	require(replacement.current_frame() == 1, "Shield bitmap unload reset the addon clock");
	require(cases == 612 && visible == 204, "Original shield control missed coverage");
	return cases;
}
