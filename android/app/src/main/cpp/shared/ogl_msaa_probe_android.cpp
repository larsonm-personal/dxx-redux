#ifdef ANDROID

#include "ogl_msaa_probe_android.h"
#include "android_graphics_safety.h"
extern "C" {
#include "android_log.h"
}
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <string>
#include <vector>

using nlohmann::json;

namespace
{
std::string last_result = "null";
unsigned long long run_serial;
const unsigned char colors[4][4] = {
	{ 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 0, 255, 255 }, { 255, 255, 255, 255 }
};

struct SavedState {
	GLint read, draw, renderbuffer, pack_buffer, pack_alignment, pack_row_length, pack_skip_rows, pack_skip_pixels;
	GLint scissor_box[4];
	GLfloat clear_color[4];
	GLboolean scissor, color_mask[4];

	SavedState()
	{
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
		glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
		glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack_buffer);
		glGetIntegerv(GL_PACK_ALIGNMENT, &pack_alignment);
		glGetIntegerv(GL_PACK_ROW_LENGTH, &pack_row_length);
		glGetIntegerv(GL_PACK_SKIP_ROWS, &pack_skip_rows);
		glGetIntegerv(GL_PACK_SKIP_PIXELS, &pack_skip_pixels);
		glGetIntegerv(GL_SCISSOR_BOX, scissor_box);
		glGetFloatv(GL_COLOR_CLEAR_VALUE, clear_color);
		glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);
		scissor = glIsEnabled(GL_SCISSOR_TEST);
	}

	void restore() const
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
		glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, pack_buffer);
		glPixelStorei(GL_PACK_ALIGNMENT, pack_alignment);
		glPixelStorei(GL_PACK_ROW_LENGTH, pack_row_length);
		glPixelStorei(GL_PACK_SKIP_ROWS, pack_skip_rows);
		glPixelStorei(GL_PACK_SKIP_PIXELS, pack_skip_pixels);
		glScissor(scissor_box[0], scissor_box[1], scissor_box[2], scissor_box[3]);
		if (scissor) glEnable(GL_SCISSOR_TEST);
		else glDisable(GL_SCISSOR_TEST);
		glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
		glColorMask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
	}

	bool matches(const SavedState &other) const
	{
		return read == other.read && draw == other.draw && renderbuffer == other.renderbuffer &&
		       pack_buffer == other.pack_buffer && pack_alignment == other.pack_alignment &&
		       pack_row_length == other.pack_row_length && pack_skip_rows == other.pack_skip_rows &&
		       pack_skip_pixels == other.pack_skip_pixels && scissor == other.scissor &&
		       std::equal(scissor_box, scissor_box + 4, other.scissor_box) &&
		       std::equal(clear_color, clear_color + 4, other.clear_color) &&
		       std::equal(color_mask, color_mask + 4, other.color_mask);
	}
};

json errors()
{
	json result = json::array();
	for (GLenum error; (error = glGetError()) != GL_NO_ERROR;) result.push_back(error);
	return result;
}

void pattern(GLuint framebuffer, int width, int height)
{
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glEnable(GL_SCISSOR_TEST);
	for (int i = 0; i < 4; ++i) {
		const int x = (i & 1) ? width / 2 : 0;
		const int y = (i & 2) ? height / 2 : 0;
		glScissor(x, y, (i & 1) ? width - x : width / 2, (i & 2) ? height - y : height / 2);
		glClearColor(colors[i][0] / 255.f, colors[i][1] / 255.f, colors[i][2] / 255.f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	glDisable(GL_SCISSOR_TEST);
}

void sentinel(GLuint framebuffer)
{
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glDisable(GL_SCISSOR_TEST);
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
}

json sample(GLuint framebuffer, int width, int height, bool alpha)
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
	json result = { { "pixels", json::array() }, { "pixels_match", true } };
	for (int i = 0; i < 4; ++i) {
		unsigned char pixel[4] = {};
		glReadPixels((i & 1) ? width * 3 / 4 : width / 4,
		             (i & 2) ? height * 3 / 4 : height / 4,
		             1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
		result["pixels"].push_back({ pixel[0], pixel[1], pixel[2], pixel[3] });
		for (int channel = 0; channel < (alpha ? 4 : 3); ++channel)
			if (std::abs(int(pixel[channel]) - colors[i][channel]) > 8) result["pixels_match"] = false;
	}
	result["errors"] = errors();
	result["passed"] = result["pixels_match"].get<bool>() && result["errors"].empty();
	return result;
}

json supported_samples(GLenum format)
{
	GLint count = 0;
	glGetInternalformativ(GL_RENDERBUFFER, format, GL_NUM_SAMPLE_COUNTS, 1, &count);
	if (count < 0 || count > 64) return json::array();
	std::vector<GLint> values(count);
	if (count) glGetInternalformativ(GL_RENDERBUFFER, format, GL_SAMPLES, count, values.data());
	return values;
}
} // namespace

#ifdef INTROSPECT_ON
extern "C" void android_ogl_graphics_debug_black_frame(void)
{
	if (!android_graphics_safety_debug_black_active()) return;
	const auto prior_errors = errors();
	SavedState saved;
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	sentinel(0);
	unsigned char pixel[4] = {};
	glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
	const auto operation_errors = errors();
	saved.restore();
	SavedState restored;
	const bool valid = prior_errors.empty() && operation_errors.empty() && errors().empty() &&
	                   saved.matches(restored) && pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0 && pixel[3] == 255;
	android_graphics_safety_debug_black_result(valid);
}
#endif

extern "C" const char *android_ogl_msaa_probe_result_json(void)
{
	return last_result.c_str();
}

extern "C" void android_ogl_msaa_probe(int requested_samples, int logical_width, int logical_height)
{
	json report = { { "run", ++run_serial }, { "requested_samples", requested_samples }, { "logical_size", { logical_width, logical_height } }, { "passed", false }, { "first_failure", "" }, { "stages", json::object() } };
	char safety[8192];
	report["scope"] = "owned_targets_before_next_frame";
	android_graphics_safety_state_json(safety, sizeof(safety));
	report["graphics_safety"] = json::parse(safety, nullptr, false);
	const std::string start = "MSAA known-color probe start\n" + report.dump(2);
	debug_log_batch_force(DLOG_GRAPHICS, start.c_str());
	if (eglGetCurrentContext() == EGL_NO_CONTEXT || eglGetCurrentSurface(EGL_DRAW) == EGL_NO_SURFACE) {
		report["first_failure"] = "context_unavailable";
		report["state_restored"] = false;
		last_result = report.dump(2);
		const std::string output = "MSAA known-color probe result\n" + last_result;
		debug_log_batch_force(DLOG_GRAPHICS, output.c_str());
		return;
	}
	report["prior_errors"] = errors();
	SavedState saved;
	GLuint source = 0, target = 0, color = 0, depth = 0, single = 0;
	GLint width = 0, height = 0, rb = 0, gb = 0, bb = 0, ab = 0, samples = 0, max_samples = 0;
	EGLDisplay display = eglGetCurrentDisplay();
	EGLSurface surface = eglGetCurrentSurface(EGL_DRAW);
	eglQuerySurface(display, surface, EGL_WIDTH, &width);
	eglQuerySurface(display, surface, EGL_HEIGHT, &height);
	report["surface_size"] = { width, height };

	auto finish = [&] {
		saved.restore();
		glDeleteFramebuffers(1, &source);
		glDeleteFramebuffers(1, &target);
		glDeleteRenderbuffers(1, &color);
		glDeleteRenderbuffers(1, &depth);
		glDeleteRenderbuffers(1, &single);
		SavedState restored;
		report["state_restored"] = saved.matches(restored);
		report["restore_errors"] = errors();
		if (!report["restore_errors"].empty() || !report["state_restored"].get<bool>()) {
			report["passed"] = false;
			if (report["first_failure"] == "") report["first_failure"] = "state_restore";
		}
		last_result = report.dump(2);
		const std::string output = "MSAA known-color probe result\n" + last_result;
		debug_log_batch_force(DLOG_GRAPHICS, output.c_str());
	};
	if (width < 4 || height < 4 || requested_samples < 2 || requested_samples > 4) {
		report["first_failure"] = "invalid_surface_or_samples";
		finish();
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glGetIntegerv(GL_RED_BITS, &rb);
	glGetIntegerv(GL_GREEN_BITS, &gb);
	glGetIntegerv(GL_BLUE_BITS, &bb);
	glGetIntegerv(GL_ALPHA_BITS, &ab);
	glGetIntegerv(GL_SAMPLES, &samples);
	glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
	GLenum format = rb <= 5 && gb <= 6 && bb <= 5 && !ab ? GL_RGB565 : ab ? GL_RGBA8
	                                                                      : GL_RGB8;
	report["window_bits"] = { rb, gb, bb, ab };
	report["window_samples"] = samples;
	report["color_format"] = format;
	report["color_supported_samples"] = supported_samples(format);
	report["depth_supported_samples"] = supported_samples(GL_DEPTH_COMPONENT16);
	report["setup_errors"] = errors();
	if (samples != 0 || !report["setup_errors"].empty()) {
		report["first_failure"] = "window_not_single_sample_or_query_failed";
		finish();
		return;
	}
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);

	auto stage = [&](const char *name, GLuint framebuffer) {
		json operations = errors();
		json result = sample(framebuffer, width, height, ab > 0);
		result["operation_errors"] = operations;
		result["passed"] = result["passed"].get<bool>() && operations.empty();
		if (!result["passed"].get<bool>() && report["first_failure"] == "") report["first_failure"] = name;
		report["stages"][name] = std::move(result);
	};

	glGenFramebuffers(1, &target);
	glGenRenderbuffers(1, &single);
	glBindRenderbuffer(GL_RENDERBUFFER, single);
	glRenderbufferStorage(GL_RENDERBUFFER, format, width, height);
	glBindFramebuffer(GL_FRAMEBUFFER, target);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, single);
	report["single_status"] = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	report["single_errors"] = errors();
	if (report["single_status"] != GL_FRAMEBUFFER_COMPLETE || !report["single_errors"].empty()) {
		report["first_failure"] = "single_sample_allocation";
		finish();
		return;
	}
	pattern(target, width, height);
	stage("offscreen_control", target);
	pattern(0, width, height);
	stage("window_control", 0);

	glGenFramebuffers(1, &source);
	glGenRenderbuffers(1, &color);
	glGenRenderbuffers(1, &depth);
	glBindRenderbuffer(GL_RENDERBUFFER, color);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, std::min(requested_samples, max_samples), format, width, height);
	GLint color_samples = 0, depth_samples = 0;
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &color_samples);
	report["color_allocation_errors"] = errors();
	glBindRenderbuffer(GL_RENDERBUFFER, depth);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, std::min(requested_samples, max_samples), GL_DEPTH_COMPONENT16, width, height);
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &depth_samples);
	report["depth_allocation_errors"] = errors();
	glBindFramebuffer(GL_FRAMEBUFFER, source);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
	report["source_status"] = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	report["source_errors"] = errors();
	report["effective_samples"] = color_samples;
	report["depth_samples"] = depth_samples;
	if (report["source_status"] != GL_FRAMEBUFFER_COMPLETE || color_samples < 2 || color_samples != depth_samples ||
	    !report["color_allocation_errors"].empty() || !report["depth_allocation_errors"].empty() || !report["source_errors"].empty()) {
		if (report["first_failure"] == "") report["first_failure"] = "multisample_allocation";
		finish();
		return;
	}
	pattern(source, width, height);
	report["pattern_errors"] = errors();
	if (!report["pattern_errors"].empty() && report["first_failure"] == "") report["first_failure"] = "multisample_pattern";
	for (int window = 0; window < 2; ++window) {
		GLuint destination = window ? 0 : target;
		sentinel(destination);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, source);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destination);
		glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		stage(window ? "window_resolve" : "offscreen_resolve", destination);
	}
	report["passed"] = report["first_failure"] == "";
	finish();
}

#endif
