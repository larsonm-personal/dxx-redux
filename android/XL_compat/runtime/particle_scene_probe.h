// Requested readback around real scene particle batches, never offscreen probes
namespace
{
xl_particle_scene_result particle_probe_result{};
std::string particle_probe_failure;
bool particle_probe_requested = false;
bool particle_probe_reading = false;
std::vector<unsigned char> particle_probe_pixels;

struct ParticleSceneReadback {
	GLint read, draw, renderbuffer, pack, alignment, row, skip_rows, skip_pixels;
	GLboolean scissor;
	GLuint fbo = 0, color = 0;
	ParticleSceneReadback()
	{
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
		glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
		glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack);
		glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
		glGetIntegerv(GL_PACK_ROW_LENGTH, &row);
		glGetIntegerv(GL_PACK_SKIP_ROWS, &skip_rows);
		glGetIntegerv(GL_PACK_SKIP_PIXELS, &skip_pixels);
		scissor = glIsEnabled(GL_SCISSOR_TEST);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glPixelStorei(GL_PACK_ROW_LENGTH, 0);
		glPixelStorei(GL_PACK_SKIP_ROWS, 0);
		glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
	}
	~ParticleSceneReadback()
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
		glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
		glDeleteFramebuffers(1, &fbo);
		glDeleteRenderbuffers(1, &color);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, pack);
		glPixelStorei(GL_PACK_ALIGNMENT, alignment);
		glPixelStorei(GL_PACK_ROW_LENGTH, row);
		glPixelStorei(GL_PACK_SKIP_ROWS, skip_rows);
		glPixelStorei(GL_PACK_SKIP_PIXELS, skip_pixels);
		if (scissor) glEnable(GL_SCISSOR_TEST);
	}
	std::vector<unsigned char> pixels()
	{
		GLint view[4], samples = 0, red = 0, alpha = 0;
		glGetIntegerv(GL_VIEWPORT, view);
		if (view[0] < 0 || view[1] < 0 || view[2] <= 0 || view[3] <= 0 || view[0] + view[2] > 4096 || view[1] + view[3] > 4096)
			throw std::runtime_error("Particle scene readback exceeds its bounded viewport");
		glBindFramebuffer(GL_FRAMEBUFFER, draw);
		glGetIntegerv(GL_SAMPLES, &samples);
		particle_probe_result.samples = unsigned(samples);
		if (samples) {
			glGetIntegerv(GL_RED_BITS, &red);
			glGetIntegerv(GL_ALPHA_BITS, &alpha);
			glGenFramebuffers(1, &fbo);
			glGenRenderbuffers(1, &color);
			glBindRenderbuffer(GL_RENDERBUFFER, color);
			glRenderbufferStorage(GL_RENDERBUFFER, red == 5 ? GL_RGB565 : alpha ? GL_RGBA8
			                                                                    : GL_RGB8,
			                      view[0] + view[2], view[1] + view[3]);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
			glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
			if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				throw std::runtime_error("Particle scene resolve framebuffer is incomplete");
			glDisable(GL_SCISSOR_TEST);
			glBlitFramebuffer(view[0], view[1], view[0] + view[2], view[1] + view[3], view[0], view[1], view[0] + view[2], view[1] + view[3], GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
		}
		std::vector<unsigned char> result(std::size_t(view[2]) * view[3] * 4);
		glReadPixels(view[0], view[1], view[2], view[3], GL_RGBA, GL_UNSIGNED_BYTE, result.data());
		if (glGetError() != GL_NO_ERROR) throw std::runtime_error("Particle scene readback raised a GPU error");
		return result;
	}
};

void particle_probe_before(const std::vector<xl_compat::ParticleQuad> &quads)
{
	particle_probe_reading = false;
	if (!particle_probe_requested || particle_probe_result.completed) return;
	const float *matrix = gles3_shim_get_mvp();
	bool visible = false;
	for (const auto &quad : quads) {
		unsigned outside = 63;
		for (const auto &vertex : quad.vertices) {
			std::array<float, 4> clip{};
			for (unsigned row = 0; row < 4; ++row)
				clip[row] = matrix[row] * vertex.position[0] + matrix[4 + row] * vertex.position[1] + matrix[8 + row] * vertex.position[2] + matrix[12 + row];
			unsigned flags = 0;
			for (unsigned axis = 0; axis < 3; ++axis) {
				if (clip[axis] < -clip[3]) flags |= 1u << (axis * 2);
				if (clip[axis] > clip[3]) flags |= 2u << (axis * 2);
			}
			outside &= flags;
		}
		if (!outside) {
			visible = true;
			break;
		}
	}
	// A quad wholly beyond any common clip plane cannot affect framebuffer RGB
	if (!visible) return;
	try {
		ParticleSceneReadback read;
		particle_probe_pixels = read.pixels();
		particle_probe_reading = true;
	} catch (const std::exception &error) {
		particle_probe_failure = error.what();
		particle_probe_result.completed = 1;
	}
}
void particle_probe_after(unsigned quads)
{
	if (!particle_probe_requested || particle_probe_result.completed || !particle_probe_reading) return;
	particle_probe_reading = false;
	try {
		std::vector<unsigned char> after;
		{
			ParticleSceneReadback read;
			after = read.pixels();
		}
		if (after.size() != particle_probe_pixels.size()) throw std::runtime_error("Particle batch changed its viewport during readback");
		++particle_probe_result.batches;
		particle_probe_result.quads += quads;
		for (std::size_t i = 0; i < after.size(); i += 4)
			if (!std::equal(after.begin() + i, after.begin() + i + 3, particle_probe_pixels.begin() + i)) ++particle_probe_result.changed;
		particle_probe_result.passed = particle_probe_result.changed != 0;
		particle_probe_result.completed = particle_probe_result.passed || particle_probe_result.batches >= 16;
		if (particle_probe_result.completed) particle_probe_pixels.clear();
	} catch (const std::exception &error) {
		particle_probe_failure = error.what();
		particle_probe_result.completed = 1;
	}
}
} // namespace
extern "C" void xl_particle_scene_probe_request(void)
{
	particle_probe_result = {};
	particle_probe_failure.clear();
	particle_probe_pixels.clear();
	particle_probe_requested = true;
	particle_probe_reading = false;
}
extern "C" void xl_particle_scene_probe_result(xl_particle_scene_result *result)
{
	*result = particle_probe_result;
}
extern "C" const char *xl_particle_scene_probe_error(void)
{
	return particle_probe_failure.c_str();
}
