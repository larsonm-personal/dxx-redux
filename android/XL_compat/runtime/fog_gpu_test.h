// Diagnostic control: actual production shaders, depth snapshots and compositor
// with known view-axis intervals. No mission traversal or synthetic success flag
namespace
{
float test_depth(float distance)
{
	return (1.f + (5001.f - 10000.f / distance) / 4999.f) * .5f;
}
struct ProbeTarget {
	GLuint scene = 0, color = 0, depth = 0, resolved = 0, resolved_color = 0;
	GLint old_renderbuffer = 0;
	GLfloat old_clear_depth = 1;
	ProbeTarget(int samples, int bits)
	{
		glGetIntegerv(GL_RENDERBUFFER_BINDING, &old_renderbuffer);
		glGetFloatv(GL_DEPTH_CLEAR_VALUE, &old_clear_depth);
		try {
			glGenFramebuffers(1, &scene);
			glBindFramebuffer(GL_FRAMEBUFFER, scene);
			glGenRenderbuffers(1, &color);
			glBindRenderbuffer(GL_RENDERBUFFER, color);
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, 96, 96);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
			glGenRenderbuffers(1, &depth);
			glBindRenderbuffer(GL_RENDERBUFFER, depth);
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, bits == 16 ? GL_DEPTH_COMPONENT16 : GL_DEPTH_COMPONENT24, 96, 96);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
			require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Fog control scene framebuffer is incomplete");
			glGenFramebuffers(1, &resolved);
			glBindFramebuffer(GL_FRAMEBUFFER, resolved);
			texture(&resolved_color, 96, 96, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resolved_color, 0);
			require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Fog control readback framebuffer is incomplete");
		} catch (...) {
			release();
			throw;
		}
	}
	void release()
	{
		glDeleteFramebuffers(1, &scene);
		glDeleteFramebuffers(1, &resolved);
		glDeleteRenderbuffers(1, &color);
		glDeleteRenderbuffers(1, &depth);
		glDeleteTextures(1, &resolved_color);
		glBindRenderbuffer(GL_RENDERBUFFER, old_renderbuffer);
		clear_depth(old_clear_depth);
	}
	~ProbeTarget()
	{
		release();
	}
};
struct ProbeCap {
	int type;
	bool near_cap;
	float distance;
};
struct FogCase {
	const char *name;
	float geometry = 300, objects = 300;
	std::vector<ProbeCap> caps;
};
void gpu_cases(xl_fog_gpu_result &result)
{
	require(xl_fog_active(), "Fog GPU controls require a loaded fog fixture");
	setup();
	ReadbackState pack;
	float colors[4][4]{};
	require(xl_fog_colors(colors), "Fog GPU controls lost owned settings");
	std::vector<FogCase> cases;
	for (int type = 0; type < 4; ++type) {
		cases.push_back({ "outside", 300, 300, { { type, true, 10 }, { type, false, 90 } } });
		cases.push_back({ "inside", 300, 300, { { type, false, 90 } } });
		cases.push_back({ "object-occluded", 300, 50, { { type, true, 10 }, { type, false, 90 } } });
		cases.push_back({ "geometry-before", 5, 5, { { type, true, 10 }, { type, false, 90 } } });
		cases.push_back({ "geometry-inside", 50, 50, { { type, true, 10 }, { type, false, 90 } } });
		cases.push_back({ "disconnected-envelope", 300, 300, { { type, true, 10 }, { type, false, 50 }, { type, true, 80 }, { type, false, 150 } } });
		cases.push_back({ "zero-thickness", 300, 300, { { type, true, 30 }, { type, false, 30 } } });
	}
	cases.push_back({ "both-pairs", 300, 70, { { 0, true, 10 }, { 0, false, 90 }, { 1, true, 20 }, { 1, false, 100 }, { 2, true, 30 }, { 2, false, 110 }, { 3, true, 40 }, { 3, false, 120 } } });
	cases.push_back({ "no-boundaries", 300, 300, {} });
	GLint max_samples = 0;
	glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
	for (int bits : { 16, 24 })
		for (int samples : { 0, 2, 4 }) {
			if (samples > max_samples) continue;
			ProbeTarget target(samples, bits);
			const GLint view[4] = { 13, 11, 64, 64 };
			resize(view[0] + view[2], view[1] + view[3], bits, 0);
			for (const auto &test : cases) {
				glBindFramebuffer(GL_FRAMEBUFFER, target.scene);
				glDisable(GL_SCISSOR_TEST);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				glClearColor(.25f, .5f, .75f, 1.f);
				glDepthMask(GL_TRUE);
				clear_depth(test_depth(test.geometry));
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				depth_snapshot(target.scene, view);
				glDisable(GL_DEPTH_TEST);
				glDepthMask(GL_FALSE);
				glDisable(GL_CULL_FACE);
				glEnable(GL_BLEND);
				use(volume_program, projection);
				glUniform1i(glGetUniformLocation(volume_program, "sceneDepthTex"), 0);
				glUniform2f(glGetUniformLocation(volume_program, "windowSize"), static_cast<float>(buffers.width), static_cast<float>(buffers.height));
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, buffers.depth);
				int types = 0;
				std::array<float, 4> first{ 5000, 5000, 5000, 5000 }, last{ 1, 1, 1, 1 };
				for (int pair = 0; pair < 2; ++pair) {
					volume_begin(pair, view);
					for (const auto &cap : test.caps) {
						if (cap.type / 2 != pair) continue;
						types |= 1 << cap.type;
						cap_mode(cap.type, cap.near_cap);
						GLfloat triangle[18];
						for (int corner = 0; corner < 6; ++corner) {
							triangle[corner * 3] = quad[corner * 3] * cap.distance;
							triangle[corner * 3 + 1] = quad[corner * 3 + 1] * cap.distance;
							triangle[corner * 3 + 2] = -cap.distance;
						}
						draw(triangle, 6);
						if (cap.distance <= test.geometry) {
							if (cap.near_cap) first[cap.type] = (std::min) (first[cap.type], cap.distance);
							else last[cap.type] = (std::max) (last[cap.type], cap.distance);
						}
					}
				}
				glBindFramebuffer(GL_FRAMEBUFFER, target.scene);
				glViewport(view[0], view[1], view[2], view[3]);
				glDepthMask(GL_TRUE);
				clear_depth(test_depth(test.objects));
				glClear(GL_DEPTH_BUFFER_BIT);
				prepared_types = types;
				prepared_target = target.scene;
				std::copy(view, view + 4, prepared_view);
				// Deliberately nondefault state around the actual production compositor
				glEnable(GL_SCISSOR_TEST);
				glScissor(3, 4, 89, 88);
				glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
				glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ZERO, GL_ONE);
				glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_SUBTRACT);
				{
					SavedState before;
					ogl_xl_fog_composite();
					SavedState after;
					require(before.same(after), "Fog compositor leaked GL state");
				}
				glDisable(GL_SCISSOR_TEST);
				glBindFramebuffer(GL_READ_FRAMEBUFFER, target.scene);
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target.resolved);
				glBlitFramebuffer(0, 0, 96, 96, 0, 0, 96, 96, GL_COLOR_BUFFER_BIT, GL_NEAREST);
				glBindFramebuffer(GL_READ_FRAMEBUFFER, target.resolved);
				float expected[3] = { 64.f / 255, 128.f / 255, 191.f / 255 };
				for (int pair = 0; pair < 2; ++pair) {
					float mixed[3] = { 1, 1, 1 }, opacity = 0;
					for (int type = pair * 2; type < pair * 2 + 2; ++type) {
						const float start = first[type] > last[type] ? 1.f : first[type];
						const float length = (std::min) (last[type], test.objects) - start;
						if (length <= 0 || colors[type][3] == 0) continue;
						for (int c = 0; c < 3; ++c) mixed[c] *= colors[type][c];
						opacity += (std::min) (1.f, length / colors[type][3]);
					}
					opacity = (std::min) (1.f, opacity);
					for (int c = 0; c < 3; ++c) expected[c] = mixed[c] * opacity + expected[c] * (1 - opacity);
				}
				for (int sample = 0; sample < 3; ++sample) {
					const int x = sample == 2 ? 2 : view[0] + (sample ? 48 : 16);
					const int y = sample == 2 ? 2 : view[1] + (sample ? 16 : 48);
					unsigned char pixel[4]{};
					glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
					const int background[3] = { 64, 128, 191 };
					for (int c = 0; c < 3; ++c) {
						const int wanted = sample == 2 ? background[c] : static_cast<int>(std::lround(expected[c] * 255));
						const int difference = std::abs(int(pixel[c]) - wanted);
						result.max_error = (std::max) (result.max_error, difference);
						if (difference > 3) {
							throw std::runtime_error(std::string("Fog GPU ") + test.name + " samples=" + std::to_string(samples) + " depth=" + std::to_string(bits) + " channel=" + std::to_string(c) + " actual=" + std::to_string(pixel[c]) + " expected=" + std::to_string(wanted));
						}
					}
					require(pixel[3] == 255, "Fog changed presentation opacity");
					++result.pixels;
				}
				errors("Fog GPU controls");
				++result.cases;
				if (samples) ++result.multisample_cases;
			}
		}
}
} // namespace

extern "C" const char *ogl_xl_fog_gpu_probe(struct xl_fog_gpu_result *out)
{
	*out = {};
	try {
		SavedState caller;
		{
			SavedState saved;
			gpu_cases(*out);
		}
		SavedState restored;
		require(caller.same(restored), "Fog GPU controls leaked caller GL state");
		out->state_restored = 1;
		return nullptr;
	} catch (const std::exception &error) {
		failure = error.what();
		return failure.c_str();
	}
}
