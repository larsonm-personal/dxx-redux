// Source-qualified receipts distinguish buffered rendering from drawing every quad
static void particle_buffer_gpu_controls(xl_compat::ParticleTexture &texture)
{
	xl_compat::ParticleView view{};
	view.basis = { { 65536, 0, 0 }, { 0, 65536, 0 } };
	view.rotate = false;
	std::array<xl_compat::ParticleQuad, 4> quads;
	for (unsigned i = 0; i < quads.size(); ++i) {
		xl_compat::FireParticle particle{};
		particle.width = particle.height = particle.decay = 1;
		particle.frame = i % 2 ? 2 : 0;
		quads[i] = xl_compat::fire_particle_quad(particle, view, 128, 128);
	}
	unsigned comparisons = 0, distinguished = 0;
	for (bool soft : { false, true })
		for (unsigned profile = 0; profile < 3; ++profile) {
			const unsigned count = profile ? 4 : 1;
			const std::vector<std::size_t> expected = profile == 0 ? std::vector<std::size_t>{} : profile == 1 ? std::vector<std::size_t>{ 0, 2 }
			                                                                                                   : std::vector<std::size_t>{ 1, 3, 0, 2 };
			std::array<std::array<unsigned char, 16 * 16 * 4>, 3> images{};
			for (unsigned method = 0; method < 3; ++method) {
				glClearColor(.2f, .4f, .6f, 1);
				glClearDepth(1);
				glDepthMask(GL_TRUE);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				if (soft) xl_particle_soft_begin_scene();
				const auto draw = [&](const std::vector<std::size_t> &indices) {
					std::vector<xl_compat::ParticleQuad> batch;
					for (auto i : indices) batch.push_back(quads[i]);
					texture.draw(batch.data(), batch.size());
				};
				if (method == 0) {
					xl_compat::FireParticleBuffers buffers({ 0, 0, 0 });
					for (unsigned i = 0; i < count; ++i) {
						const float x = profile == 2 ? float((i % 2 ? 100 : 10) + i / 2) : 10.f;
						buffers.add(i, { x, 0, 0 }, 1, i % 2 != 0, i % 2 == 0, draw);
					}
					buffers.flush(draw);
				} else if (method == 1) {
					// Independent one-quad submissions also qualify GPU batch coalescing
					for (auto index : expected) texture.draw(&quads[index], 1);
				} else texture.draw(quads.data(), count);
				if (soft) xl_particle_soft_end_scene();
				glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, images[method].data());
				std::array<float, 16 * 16> depths;
				glReadPixels(0, 0, 16, 16, GL_DEPTH_COMPONENT, GL_FLOAT, depths.data());
				for (float depth : depths) check(depth == 1, "Buffered particle draw changed scene depth");
				check(glGetError() == GL_NO_ERROR, "Buffered particle GPU control raised GL error");
			}
			check(images[0] == images[1], "Buffered particle pixels differ from pinned dispatch receipt");
			if (images[0] != images[2]) ++distinguished;
			++comparisons;
		}
	check(distinguished == comparisons, "Buffering GPU controls failed to distinguish prior per-quad dispatch");
	// Neutral fire quads leave RGB unchanged; the final colored quad must survive
	// a same-blend upload split at the source's 25,000-quad capacity
	std::vector<xl_compat::ParticleQuad> capacity(25001, quads[0]);
	for (std::size_t i = 0; i + 1 < capacity.size(); ++i)
		for (auto &vertex : capacity[i].vertices) vertex.color = { 0, 0, 0, 1 };
	std::array<std::array<unsigned char, 16 * 16 * 4>, 2> capacity_images;
	for (unsigned method = 0; method < 2; ++method) {
		glClearColor(.2f, .4f, .6f, 1);
		glClearDepth(1);
		glDepthMask(GL_TRUE);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		xl_particle_soft_begin_scene();
		if (!method) texture.draw(capacity.data(), capacity.size());
		else texture.draw(&capacity.back(), 1);
		xl_particle_soft_end_scene();
		glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, capacity_images[method].data());
	}
	check(capacity_images[0] == capacity_images[1] && capacity_images[0][0] != 51 && glGetError() == GL_NO_ERROR,
	      "Soft particle bounded upload lost or altered its final same-blend quad");
	std::cout << "Particle buffer GPU: " << comparisons << " complete-frame hard/soft comparisons distinguish singleton suppression and source reorder, scene depth preserved\n";
	std::cout << "Particle soft upload: 25001 same-blend quads preserve the final colored quad across the bounded split\n";
}
