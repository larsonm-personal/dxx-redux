// One requested normal frame: scene colors, actual volume intervals and final depth
// are compared numerically before/after the production compositor
namespace
{
xl_fog_scene_result scene_result{};
std::string scene_failure;
bool scene_requested = false, scene_pending = false;
struct ScenePixel {
	int x, y;
	std::array<unsigned char, 4> before{};
	std::array<float, 3> expected{};
};
std::array<ScenePixel, 25> scene_pixels{};

struct SceneReadback {
	GLuint fbo = 0, color = 0;
	SceneReadback(GLuint source, int width, int height)
	{
		try {
			glBindFramebuffer(GL_FRAMEBUFFER, source);
			GLint samples = 0, alpha = 0, red = 0;
			glGetIntegerv(GL_SAMPLES, &samples);
			glGetIntegerv(GL_ALPHA_BITS, &alpha);
			glGetIntegerv(GL_RED_BITS, &red);
			scene_result.samples = samples;
			if (!samples) return;
			glGenFramebuffers(1, &fbo);
			glGenRenderbuffers(1, &color);
			glBindRenderbuffer(GL_RENDERBUFFER, color);
			glRenderbufferStorage(GL_RENDERBUFFER, red == 5 ? GL_RGB565 : alpha ? GL_RGBA8
			                                                                    : GL_RGB8,
			                      width, height);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
			glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
			require(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Cannot resolve normal fog scene colors");
			glDisable(GL_SCISSOR_TEST);
			glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
		} catch (...) {
			release();
			throw;
		}
	}
	void release()
	{
		glDeleteFramebuffers(1, &fbo);
		glDeleteRenderbuffers(1, &color);
	}
	~SceneReadback()
	{
		release();
	}
};
float scene_eye(float depth)
{
	return 10000.f / (5001.f - (2.f * depth - 1.f) * 4999.f);
}
void scene_before(GLuint scene, const GLint view[4])
{
	if (!scene_requested) return;
	scene_requested = false;
	scene_pending = false;
	GLuint probe = 0, target = 0, color = 0;
	SavedState saved;
	ReadbackState pack;
	try {
		scene_result.types = prepared_types;
		scene_result.depth_bits = buffers.bits;
		float settings[4][4]{};
		require(xl_fog_colors(settings), "Normal frame lost authored fog settings");
		{
			SceneReadback read(scene, buffers.width, buffers.height);
			for (int i = 0; i < 25; ++i) {
				auto &pixel = scene_pixels[i];
				pixel.x = view[0] + view[2] * (2 + i % 5) / 8;
				pixel.y = view[1] + view[3] * (2 + i / 5) / 8;
				glReadPixels(pixel.x, pixel.y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.before.data());
				for (int c = 0; c < 3; ++c) pixel.expected[c] = pixel.before[c] / 255.f;
			}
		}
		probe = compile("uniform sampler2D depthTex; uniform vec2 sampleUV; void main() { gl_FragColor = vec4(texture2D(depthTex, sampleUV).r); }");
		texture(&color, 1, 1, GL_RGBA32F, GL_RGBA, GL_FLOAT);
		glGenFramebuffers(1, &target);
		glBindFramebuffer(GL_FRAMEBUFFER, target);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
		require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Cannot sample normal fog scene depth");
		glViewport(0, 0, 1, 1);
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		use(probe, identity);
		glUniform1i(glGetUniformLocation(probe, "depthTex"), 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, buffers.depth);
		for (auto &pixel : scene_pixels) {
			glBindFramebuffer(GL_FRAMEBUFFER, target);
			glUniform2f(glGetUniformLocation(probe, "sampleUV"), (pixel.x + .5f) / buffers.width, (pixel.y + .5f) / buffers.height);
			draw(quad, 6);
			float depth[4]{};
			glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, depth);
			const float scene_depth = scene_eye(depth[0]);
			const bool center = &pixel == &scene_pixels[12];
			if (center) scene_result.center_depth = scene_depth;
			bool covered = false;
			for (int pair = 0; pair < 2; ++pair) {
				float caps[4]{};
				glBindFramebuffer(GL_READ_FRAMEBUFFER, buffers.fbo[pair]);
				glReadPixels(pixel.x, pixel.y, 1, 1, GL_RGBA, GL_FLOAT, caps);
				if (center) std::copy(caps, caps + 4, scene_result.center_caps + pair * 4);
				float rgb[3] = { 1, 1, 1 }, alpha = 0;
				for (int slot = 0; slot < 2; ++slot) {
					const int type = pair * 2 + slot;
					if (!(prepared_types & (1 << type))) continue;
					const float start = caps[slot * 2] > caps[slot * 2 + 1] ? 1.f : scene_eye(caps[slot * 2]);
					const float length = (std::min) (scene_eye(caps[slot * 2 + 1]), scene_depth) - start;
					scene_result.max_length = (std::max) (scene_result.max_length, length);
					if (length <= 0) continue;
					covered = true;
					if (settings[type][3] == 0) continue;
					for (int c = 0; c < 3; ++c) rgb[c] *= settings[type][c];
					alpha += (std::min) (1.f, length / settings[type][3]);
				}
				alpha = (std::min) (1.f, alpha);
				for (int c = 0; c < 3; ++c) pixel.expected[c] = rgb[c] * alpha + pixel.expected[c] * (1 - alpha);
			}
			if (covered) ++scene_result.covered;
		}
		errors("Normal fog frame inputs");
		scene_pending = true;
	} catch (const std::exception &error) {
		scene_failure = error.what();
		scene_result.completed = 1;
	}
	if (probe) glDeleteProgram(probe);
	glDeleteFramebuffers(1, &target);
	glDeleteTextures(1, &color);
}
void scene_after(GLuint scene, const GLint *)
{
	if (!scene_pending) return;
	scene_pending = false;
	SavedState saved;
	ReadbackState pack;
	try {
		SceneReadback read(scene, buffers.width, buffers.height);
		for (const auto &pixel : scene_pixels) {
			unsigned char actual[4]{};
			glReadPixels(pixel.x, pixel.y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, actual);
			bool changed = false;
			for (int c = 0; c < 3; ++c) {
				const int expected = static_cast<int>(std::lround(pixel.expected[c] * 255));
				const int error = std::abs(int(actual[c]) - expected);
				if (&pixel == &scene_pixels[12]) {
					scene_result.center_before[c] = pixel.before[c];
					scene_result.center_after[c] = actual[c];
					scene_result.center_expected[c] = expected;
				}
				scene_result.max_error = (std::max) (scene_result.max_error, error);
				require(error <= 3, "Normal fog scene differs from its source-derived volume/depth composition");
				changed |= actual[c] != pixel.before[c];
			}
			if (changed) ++scene_result.changed;
			++scene_result.pixels;
		}
		errors("Normal fog frame output");
		scene_result.passed = 1;
	} catch (const std::exception &error) {
		scene_failure = error.what();
	}
	scene_result.completed = 1;
}
} // namespace
extern "C" void ogl_xl_fog_scene_request(void)
{
	scene_result = {};
	scene_failure.clear();
	scene_requested = true;
	scene_pending = false;
}
extern "C" void ogl_xl_fog_scene_result(struct xl_fog_scene_result *out)
{
	*out = scene_result;
}
extern "C" const char *ogl_xl_fog_scene_error(void)
{
	return scene_failure.c_str();
}
