// Protected renderer controls retain original images without admitting a level
std::shared_ptr<const xl_compat::ParticleImageResource> bubble_probe_image, bubble_probe_smoke;

struct BubbleProbeTextures {
	GLint active, bindings[4];
	GLuint sentinels[4]{};
	BubbleProbeTextures()
	{
		glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
		for (unsigned i = 0; i < 4; ++i) {
			glActiveTexture(GL_TEXTURE0 + i);
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &bindings[i]);
		}
		glActiveTexture(active);
		glGenTextures(4, sentinels);
	}
	~BubbleProbeTextures()
	{
		for (unsigned i = 0; i < 4; ++i) {
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D, bindings[i]);
		}
		glActiveTexture(active);
		glDeleteTextures(4, sentinels);
	}
	void bind() const
	{
		for (unsigned i = 0; i < 4; ++i) {
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D, sentinels[i]);
		}
	}
};

std::vector<GLint> bubble_probe_state()
{
	std::vector<GLint> result;
	for (GLenum field : { GL_CURRENT_PROGRAM, GL_VERTEX_ARRAY_BINDING, GL_ARRAY_BUFFER_BINDING, GL_ACTIVE_TEXTURE,
	                      GL_BLEND_SRC_RGB, GL_BLEND_DST_RGB, GL_BLEND_SRC_ALPHA, GL_BLEND_DST_ALPHA,
	                      GL_BLEND_EQUATION_RGB, GL_BLEND_EQUATION_ALPHA, GL_DEPTH_FUNC, GL_DEPTH_WRITEMASK }) {
		GLint value;
		glGetIntegerv(field, &value);
		result.push_back(value);
	}
	const GLint active = result[3];
	for (unsigned i = 0; i < 4; ++i) {
		glActiveTexture(GL_TEXTURE0 + i);
		GLint binding;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
		result.push_back(binding);
	}
	glActiveTexture(active);
	for (GLenum field : { GL_DEPTH_TEST, GL_BLEND, GL_CULL_FACE }) result.push_back(glIsEnabled(field));
	return result;
}

void bubble_gpu_probe()
{
	using namespace xl_compat;
	require(game_is_time_paused() && bubble_probe_image && bubble_probe_smoke && gl_initialized && glGetError() == GL_NO_ERROR,
	        "Bubble GPU probe requires retained original atlases and a paused clean context");
	unsigned samples = 0, visible = 0, mixed = 0;
	{
		BubbleProbeTextures textures;
		LightningGlowState restore;
		Readback readback;
		ParticleProbeTarget target;
		struct Depth {
			GLuint buffer = 0;
			GLfloat clear;
			Depth()
			{
				glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clear);
				glGenRenderbuffers(1, &buffer);
				glBindRenderbuffer(GL_RENDERBUFFER, buffer);
				glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 16, 16);
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, buffer);
			}
			~Depth()
			{
				xl_particle_soft_end_scene();
				glClearDepthf(clear);
				glDeleteRenderbuffers(1, &buffer);
			}
		} depth_target;
		require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Bubble probe framebuffer is incomplete");
		const std::array<unsigned char, 4> background{ 37, 71, 113, 255 };
		const auto depth = [](float eye) { return (5000.f / 4999.f) * (1.f - 1.f / eye); };
		const auto sample_at = [&](int kind, unsigned cell) {
			const auto &image = (kind == 2 ? bubble_probe_image : bubble_probe_smoke)->bitmap;
			const unsigned side = kind == 2 ? 4 : 8;
			const unsigned left = (cell % side) * image.width / side, top = (cell / side) * image.height / side;
			std::array<unsigned, 2> point{ left, top };
			unsigned opacity = 0;
			// Choose an opaque witness in each cell, including hollow bubble rings
			for (unsigned y = top; y < top + image.height / side; ++y)
				for (unsigned x = left; x < left + image.width / side; ++x) {
					const auto alpha = image.pixels[(std::size_t(y) * image.width + x) * 4 + 3];
					if (alpha > opacity) {
						opacity = alpha;
						point = { x, y };
					}
				}
			return point;
		};
		const auto quad_at = [&](int kind, unsigned cell, float alpha, float gap, float left = -1, float right = 1) {
			const auto &image = (kind == 2 ? bubble_probe_image : bubble_probe_smoke)->bitmap;
			const auto point = sample_at(kind, cell);
			ParticleQuad quad{};
			quad.image = kind;
			quad.blend = ParticleBlend::alpha;
			const float z = depth(10 + gap) * 2 - 1;
			quad.vertices[0].position = { left, -1, z };
			quad.vertices[1].position = { right, -1, z };
			quad.vertices[2].position = { right, 1, z };
			quad.vertices[3].position = { left, 1, z };
			for (auto &vertex : quad.vertices) {
				vertex.uv = { (point[0] + .5f) / image.width, (point[1] + .5f) / image.height };
				vertex.color = { .7f, .8f, .9f, alpha };
			}
			return quad;
		};
		const auto begin = [&] {
			glDepthMask(GL_TRUE);
			glClearDepthf(depth(10));
			glClearColor(background[0] / 255.f, background[1] / 255.f, background[2] / 255.f, 1);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			xl_particle_soft_begin_scene();
			textures.bind();
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_GREATER);
			glEnable(GL_CULL_FACE);
			glDisable(GL_BLEND);
			glBlendFuncSeparate(GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_ZERO, GL_ONE);
			glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_SUBTRACT);
		};
		const auto verify_pixel = [&](int kind, unsigned cell, float alpha, float gap, bool soft, int x) {
			const auto &image = (kind == 2 ? bubble_probe_image : bubble_probe_smoke)->bitmap;
			const auto point = sample_at(kind, cell);
			const auto offset = (std::size_t(point[1]) * image.width + point[0]) * 4;
			std::array<unsigned char, 4> actual{};
			glReadPixels(x, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
			const float maximum = kind == 1 ? 3.f : 5.f;
			const float fade = soft ? 1.f - std::clamp(gap, 0.f, maximum) / maximum : 1.f;
			const float opacity = image.pixels[offset + 3] / 255.f;
			const float a = opacity * alpha * fade;
			for (unsigned channel = 0; channel < 4; ++channel) {
				float expected;
				if (channel == 3) expected = alpha == 0 ? 1.f : 2.f * (1.f - a);
				else {
					const float source = image.pixels[offset + channel] / 255.f * (channel == 0 ? .7f : channel == 1 ? .8f
					                                                                                                 : .9f) *
					                     fade * (kind == 0 ? opacity : 1.f);
					expected = alpha == 0 ? source + background[channel] / 255.f : source * a + background[channel] / 255.f * (1.f - a);
				}
				const int byte = int(std::clamp(expected, 0.f, 1.f) * 255.f + .5f);
				if (std::abs(int(actual[channel]) - byte) > 1)
					throw std::runtime_error("Bubble shader pixel mismatch: kind=" + std::to_string(kind) + " cell=" + std::to_string(cell) + " alpha=" + std::to_string(alpha) + " gap=" + std::to_string(gap) + " soft=" + std::to_string(soft) + " channel=" + std::to_string(channel) + " expected=" + std::to_string(byte) + " actual=" + std::to_string(actual[channel]));
			}
			visible += actual != background;
			++samples;
		};
		for (bool soft : { false, true })
			for (int kind : { 0, 1, 2 })
				for (unsigned cell = 0; cell < 16; ++cell)
					for (float alpha : { 0.f, .6f })
						for (float gap : { -1.f, 1.5f, 4.f, 6.f }) {
							auto quad = quad_at(kind, cell, alpha, gap);
							begin();
							const auto state = bubble_probe_state();
							draw_multi_particle_quads(*bubble_probe_smoke->texture, *bubble_probe_image->texture, &quad, 1, soft);
							require(bubble_probe_state() == state, "Bubble draw leaked GL program, buffer, four-unit texture, depth or blend state");
							verify_pixel(kind, cell, alpha, gap, soft, 8);
							xl_particle_soft_end_scene();
						}
		for (bool soft : { false, true }) {
			std::array<ParticleQuad, 3> quads;
			for (int kind = 0; kind < 3; ++kind) quads[kind] = quad_at(kind, 7, .6f, 1.5f, -1.f + kind * 2.f / 3, -1.f + (kind + 1) * 2.f / 3);
			begin();
			const auto state = bubble_probe_state();
			draw_multi_particle_quads(*bubble_probe_smoke->texture, *bubble_probe_image->texture, quads.data(), quads.size(), soft);
			require(bubble_probe_state() == state, "Mixed particle batch leaked GL state");
			for (int kind = 0; kind < 3; ++kind) verify_pixel(kind, 7, .6f, 1.5f, soft, kind == 0 ? 2 : kind == 1 ? 8
				                                                                                                  : 13);
			++mixed;
			xl_particle_soft_end_scene();
		}
	}
	require(samples == 774 && visible > 100 && mixed == 2 && glGetError() == GL_NO_ERROR, "Bubble GPU controls lack visible pixels or clean restoration");
	++checks;
	report = nlohmann::json{ { "passed", true }, { "bubble_samples", samples }, { "bubble_visible", visible }, { "bubble_mixed", mixed }, { "bubble_width", bubble_probe_image->bitmap.width }, { "checks", checks } }.dump();
}
