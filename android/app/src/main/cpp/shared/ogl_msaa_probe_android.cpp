#ifdef ANDROID

#include "ogl_msaa_probe_android.h"
#include "android_graphics_safety.h"
#include "android_gpu_capabilities.h"
extern "C" {
#include "android_log.h"
#include "ogl_msaa_android.h"
void ogl_prepare_framebuffer_readback(void);
}
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <string>
#include <vector>
#include <physfs.h>

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

std::string gl_string(GLenum name)
{
	const auto *value = glGetString(name);
	return value ? reinterpret_cast<const char *>(value) : "";
}

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
namespace
{
struct ProbePixel {
	int x, y;
	std::array<unsigned char, 4> expected;
};
bool menu_probe_active, menu_probe_source_pending, menu_probe_captured;
int menu_probe_frames, menu_probe_screen_width, menu_probe_screen_height;
unsigned long long menu_probe_serial;
std::vector<ProbePixel> menu_probe_pixels;
json menu_probe_report;
std::string menu_probe_result = "null";

json probe_pixels(const std::vector<ProbePixel> &points, int logical_width = 0, int logical_height = 0)
{
	json report = { { "prior_errors", errors() }, { "pixels", json::array() }, { "passed", false } };
	SavedState saved;
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	report["draw_framebuffer"] = saved.draw;
	report["viewport"] = { viewport[0], viewport[1], viewport[2], viewport[3] };
	GLuint capture_fbo = 0, capture_color = 0;
	EGLint width = 0, height = 0;
	eglQuerySurface(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW), EGL_WIDTH, &width);
	eglQuerySurface(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW), EGL_HEIGHT, &height);
	bool readable = saved.draw == 0;
	if (saved.draw != 0) {
		GLint type = 0, object = 0, format = 0;
		glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
		glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &object);
		if (type == GL_RENDERBUFFER && object) {
			glBindRenderbuffer(GL_RENDERBUFFER, object);
			glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &format);
			glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
			glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
			glGenFramebuffers(1, &capture_fbo);
			glGenRenderbuffers(1, &capture_color);
			glBindRenderbuffer(GL_RENDERBUFFER, capture_color);
			glRenderbufferStorage(GL_RENDERBUFFER, format, width, height);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, capture_fbo);
			glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, capture_color);
			readable = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
			if (readable) {
				// Copy for observation without invoking or changing the production resolve
				glBindFramebuffer(GL_READ_FRAMEBUFFER, saved.draw);
				glDisable(GL_SCISSOR_TEST);
				glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			}
		}
		report["capture_format"] = format;
		report["capture_errors"] = errors();
		readable = readable && report["capture_errors"].empty();
	}
	bool match = readable && !points.empty();
	if (readable) {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, capture_fbo);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glPixelStorei(GL_PACK_ROW_LENGTH, 0);
		glPixelStorei(GL_PACK_SKIP_ROWS, 0);
		glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
		for (const auto &point : points) {
			const int x = logical_width > 0 ? viewport[0] + int((point.x + 0.5) * viewport[2] / logical_width) : point.x;
			const int y = logical_height > 0 ? viewport[1] + viewport[3] - 1 - int((point.y + 0.5) * viewport[3] / logical_height) : point.y;
			std::array<unsigned char, 4> pixel = {};
			bool point_match = x >= 0 && y >= 0 && x < width && y < height;
			if (point_match) glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
			for (int channel = 0; channel < 4; ++channel)
				if (std::abs(int(pixel[channel]) - point.expected[channel]) > 8) point_match = false;
			match = match && point_match;
			report["pixels"].push_back({ { "at", { x, y } }, { "expected", point.expected }, { "actual", pixel }, { "match", point_match } });
		}
	}
	report["errors"] = errors();
	saved.restore();
	if (capture_fbo) glDeleteFramebuffers(1, &capture_fbo);
	if (capture_color) glDeleteRenderbuffers(1, &capture_color);
	SavedState restored;
	report["state_restored"] = saved.matches(restored);
	report["restore_errors"] = errors();
	report["passed"] = match && report["prior_errors"].empty() && report["errors"].empty() &&
	                   report["restore_errors"].empty() && saved.matches(restored);
	return report;
}

json menu_probe_sample()
{
	json result = probe_pixels(menu_probe_pixels, menu_probe_screen_width, menu_probe_screen_height);
	if (menu_probe_pixels.size() < 4) result["passed"] = false;
	return result;
}

bool scene_probe_active;
int scene_probe_min_passes, scene_probe_frames, scene_probe_passes;
GLint scene_probe_main_viewport[4];
std::vector<ProbePixel> scene_probe_points;
json scene_probe_report;
std::string scene_probe_result = "null";
unsigned long long scene_probe_serial;
} // namespace

extern "C" void android_ogl_scene_probe_request(int minimum_passes)
{
	scene_probe_active = true;
	scene_probe_min_passes = minimum_passes;
	scene_probe_frames = scene_probe_passes = 0;
	scene_probe_points.clear();
	scene_probe_report = { { "run", ++scene_probe_serial }, { "scope", "main_view_survives_subviews" }, { "complete", false }, { "passed", false }, { "passes", json::array() }, { "saw_subview", false }, { "discarded_base_passes", 0 } };
	scene_probe_result = scene_probe_report.dump();
}

extern "C" void android_ogl_scene_probe_discard_base_view(void)
{
	if (!scene_probe_active) return;
	// A custom FOV deliberately replaces the simulation view with a visual-only pass
	scene_probe_report["discarded_base_passes"] = scene_probe_report["discarded_base_passes"].get<int>() + scene_probe_passes;
	scene_probe_passes = 0;
	scene_probe_points.clear();
	scene_probe_report["passes"] = json::array();
	scene_probe_report["saw_subview"] = false;
}

extern "C" void android_ogl_scene_probe_after_pass(int depth)
{
	if (!scene_probe_active) return;
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	if (viewport[2] < 32 || viewport[3] < 32) return;
	if (scene_probe_passes++ == 0) {
		std::copy(viewport, viewport + 4, scene_probe_main_viewport);
		SavedState saved;
		scene_probe_points.clear();
		scene_probe_report["marker_prior_errors"] = errors();
		glEnable(GL_SCISSOR_TEST);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		for (int index = 0; index < 3; ++index) {
			ProbePixel point = { viewport[0] + viewport[2] * (index + 1) / 4, viewport[1] + viewport[3] * 3 / 4, {} };
			std::copy(colors[index], colors[index] + 4, point.expected.begin());
			glScissor(point.x - 2, point.y - 2, 5, 5);
			glClearColor(colors[index][0] / 255.f, colors[index][1] / 255.f, colors[index][2] / 255.f, 1.f);
			glClear(GL_COLOR_BUFFER_BIT);
			scene_probe_points.push_back(point);
		}
		scene_probe_report["marker_errors"] = errors();
		saved.restore();
		SavedState restored;
		scene_probe_report["marker_state_restored"] = saved.matches(restored);
		scene_probe_report["marker_restore_errors"] = errors();
	} else if (viewport[2] < scene_probe_main_viewport[2] || viewport[3] < scene_probe_main_viewport[3]) {
		scene_probe_report["saw_subview"] = true;
	}
	json pass = probe_pixels(scene_probe_points);
	pass["depth"] = depth;
	scene_probe_report["passes"].push_back(std::move(pass));
}

extern "C" void android_ogl_scene_probe_before_swap(unsigned long long flip)
{
	if (!scene_probe_active) return;
	++scene_probe_frames;
	if (scene_probe_passes >= scene_probe_min_passes &&
	    (scene_probe_min_passes == 1 || scene_probe_report["saw_subview"].get<bool>())) {
		scene_probe_report["before_swap"] = probe_pixels(scene_probe_points);
		bool passed = scene_probe_report["marker_prior_errors"].empty() &&
		              scene_probe_report["marker_errors"].empty() &&
		              scene_probe_report["marker_restore_errors"].empty() &&
		              scene_probe_report["marker_state_restored"].get<bool>() &&
		              scene_probe_report["before_swap"]["passed"].get<bool>();
		for (const auto &pass : scene_probe_report["passes"]) passed = passed && pass["passed"].get<bool>();
		scene_probe_report["passed"] = passed;
	} else if (scene_probe_frames < 120) {
		scene_probe_passes = 0;
		scene_probe_report["passes"] = json::array();
		scene_probe_report["saw_subview"] = false;
		return;
	} else {
		scene_probe_report["failure"] = "required_scene_passes_not_observed_within_120_frames";
	}
	scene_probe_active = false;
	scene_probe_report["complete"] = true;
	scene_probe_report["flip"] = flip;
	scene_probe_report["observed_frames"] = scene_probe_frames;
	scene_probe_report["pass_count"] = scene_probe_passes;
	scene_probe_result = scene_probe_report.dump();
	const std::string output = "MSAA scene composition probe\n" + scene_probe_report.dump(2);
	debug_log_batch_force(DLOG_GRAPHICS, output.c_str());
}

extern "C" const char *android_ogl_scene_probe_result_json(void)
{
	return scene_probe_result.c_str();
}

extern "C" void android_ogl_menu_probe_request(void)
{
	menu_probe_active = true;
	menu_probe_source_pending = menu_probe_captured = false;
	menu_probe_frames = 0;
	menu_probe_pixels.clear();
	menu_probe_report = { { "run", ++menu_probe_serial }, { "scope", "native_menu_pixels" }, { "complete", false }, { "passed", false }, { "source_count", 0 } };
	menu_probe_result = menu_probe_report.dump();
}

extern "C" int android_ogl_menu_probe_active(void)
{
	return menu_probe_active;
}

extern "C" void android_ogl_menu_probe_source(const unsigned char *pixels, int width, int height, int stride,
                                              int x, int y, int screen_width, int screen_height,
                                              const unsigned char *palette)
{
	if (!menu_probe_active) return;
	menu_probe_source_pending = true;
	menu_probe_screen_width = screen_width;
	menu_probe_screen_height = screen_height;
	menu_probe_pixels.clear();
	menu_probe_report["source_count"] = menu_probe_report["source_count"].get<int>() + 1;
	menu_probe_report["source_size"] = { width, height };
	menu_probe_report["destination"] = { x, y };
	if (!pixels || !palette || stride < width || screen_width <= 0 || screen_height <= 0) return;
	// Sample flat, opaque patches spread across the real paletted menu artwork
	for (int row = 0; row < 3; ++row) {
		for (int column = 0; column < 4; ++column) {
			bool found = false;
			for (int py = std::max(1, height * row / 3); py < std::min(height - 1, height * (row + 1) / 3) && !found; ++py) {
				for (int px = std::max(1, width * column / 4); px < std::min(width - 1, width * (column + 1) / 4); ++px) {
					if (x + px < 0 || y + py < 0 || x + px >= screen_width || y + py >= screen_height) continue;
					const int index = pixels[py * stride + px];
					if (index >= 254 || std::max({ palette[index * 3], palette[index * 3 + 1], palette[index * 3 + 2] }) < 10) continue;
					bool flat = true;
					for (int dy = -1; dy <= 1; ++dy)
						for (int dx = -1; dx <= 1; ++dx)
							if (pixels[(py + dy) * stride + px + dx] != index) flat = false;
					if (!flat) continue;
					ProbePixel point = { x + px, y + py, {} };
					for (int channel = 0; channel < 3; ++channel) point.expected[channel] = palette[index * 3 + channel] * 4;
					point.expected[3] = 255;
					menu_probe_pixels.push_back(point);
					found = true;
					break;
				}
			}
		}
	}
}

extern "C" void android_ogl_menu_probe_after_blit(void)
{
	if (!menu_probe_active || !menu_probe_source_pending) return;
	menu_probe_source_pending = false;
	menu_probe_report["sample_count"] = menu_probe_pixels.size();
	menu_probe_report["after_blit"] = menu_probe_sample();
	menu_probe_captured = true;
}

extern "C" void android_ogl_menu_probe_before_swap(unsigned long long flip)
{
	if (!menu_probe_active) return;
	if (!menu_probe_captured && ++menu_probe_frames < 120) return;
	menu_probe_report["flip"] = flip;
	menu_probe_report["complete"] = true;
	if (menu_probe_captured) {
		menu_probe_report["before_swap"] = menu_probe_sample();
		menu_probe_report["passed"] = menu_probe_report["after_blit"]["passed"].get<bool>() &&
		                              menu_probe_report["before_swap"]["passed"].get<bool>();
	} else {
		menu_probe_report["failure"] = "native_menu_not_drawn_within_120_frames";
	}
	menu_probe_active = false;
	menu_probe_result = menu_probe_report.dump();
	const std::string output = "MSAA native menu composition probe\n" + menu_probe_report.dump(2);
	debug_log_batch_force(DLOG_GRAPHICS, output.c_str());
}

extern "C" const char *android_ogl_menu_probe_result_json(void)
{
	return menu_probe_result.c_str();
}

static std::string loading_probe_phase = "startup";
static int loading_probe_remaining = 12;
static unsigned int loading_probe_serial;
static bool loading_background_test_pending;
static std::string loading_background_test_result = "null";

extern "C" void android_ogl_loading_background_test_begin(void)
{
	ogl_prepare_framebuffer_readback();
	SavedState saved;
	EGLint width = 0, height = 0;
	eglQuerySurface(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW), EGL_WIDTH, &width);
	eglQuerySurface(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW), EGL_HEIGHT, &height);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	pattern(0, width, height);
	saved.restore();
	loading_background_test_result = "null";
	loading_background_test_pending = true;
}

extern "C" const char *android_ogl_loading_background_test_result(void)
{
	return loading_background_test_result.c_str();
}

extern "C" void android_ogl_loading_probe_phase(const char *phase)
{
	loading_probe_phase = phase ? phase : "";
	loading_probe_remaining = 12;
}

extern "C" void android_ogl_loading_probe_frame(void)
{
	if (loading_background_test_pending) {
		EGLint width = 0, height = 0;
		eglQuerySurface(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW), EGL_WIDTH, &width);
		eglQuerySurface(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW), EGL_HEIGHT, &height);
		std::vector<ProbePixel> points;
		for (int i = 0; i < 4; ++i)
			points.push_back({ (i & 1) ? width - 2 : 1, (i & 2) ? height - 2 : 1, { 0, 0, 0, 255 } });
		const auto report = probe_pixels(points);
		loading_background_test_result = report.dump();
		debug_log_force(DLOG_GRAPHICS, "loading-background-test %s", loading_background_test_result.c_str());
		loading_background_test_pending = false;
	}
	/* Private diagnostic marker enables numerical readback, never image output */
	static const bool enabled = PHYSFS_exists("loading-frame-probe") != 0;
	if (!enabled || loading_probe_remaining <= 0) return;
	--loading_probe_remaining;
	const auto prior_errors = errors();
	SavedState saved;
	EGLint width = 0, height = 0, behavior = 0;
	const EGLDisplay display = eglGetCurrentDisplay();
	const EGLSurface surface = eglGetCurrentSurface(EGL_DRAW);
	eglQuerySurface(display, surface, EGL_WIDTH, &width);
	eglQuerySurface(display, surface, EGL_HEIGHT, &height);
	eglQuerySurface(display, surface, EGL_SWAP_BEHAVIOR, &behavior);
	if (width < 64 || height < 64) return;
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
	std::vector<unsigned char> pixels((size_t) width * height * 4);
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	const auto read_errors = errors();
	json regions = json::array();
	for (int region = 0; region < 5; ++region) {
		const int x0 = region == 4 ? width / 2 - 32 : (region & 1) ? width - 64
		                                                           : 0;
		const int y0 = region == 4 ? height / 2 - 32 : (region & 2) ? height - 64
		                                                            : 0;
		unsigned int hash = 2166136261u;
		int black = 0, white = 0, gray = 0, edges = 0;
		for (int y = y0; y < y0 + 64 && y < height; ++y) {
			int previous = -1;
			for (int x = x0; x < x0 + 64 && x < width; ++x) {
				const auto *p = &pixels[((size_t) y * width + x) * 4];
				const int lo = std::min({ p[0], p[1], p[2] });
				const int hi = std::max({ p[0], p[1], p[2] });
				black += hi < 8;
				white += lo > 247;
				gray += hi - lo < 8;
				if (previous >= 0 && std::abs(int(p[0]) - previous) > 128) ++edges;
				previous = p[0];
				for (int c = 0; c < 3; ++c) hash = (hash ^ p[c]) * 16777619u;
			}
		}
		regions.push_back({ { "region", region }, { "black", black }, { "white", white }, { "gray", gray }, { "edges", edges }, { "hash", hash } });
	}
	saved.restore();
	json report = { { "serial", ++loading_probe_serial }, { "phase", loading_probe_phase }, { "width", width }, { "height", height }, { "swap_behavior", behavior }, { "draw_fbo", saved.draw }, { "read_errors", read_errors }, { "prior_errors", prior_errors }, { "regions", regions }, { "restore_errors", errors() } };
	debug_log_force(DLOG_GRAPHICS, "loading-frame-probe %s", report.dump().c_str());
}

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
	if (width < 4 || height < 4 || (requested_samples != 2 && requested_samples != 4 && requested_samples != 8)) {
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
	GLenum format = android_ogl_msaa_color_format(rb, gb, bb, ab);
	if (!format) {
		report["first_failure"] = "unsupported_window_color_format";
		finish();
		return;
	}
	report["window_bits"] = { rb, gb, bb, ab };
	report["window_samples"] = samples;
	report["setup_errors"] = errors();
	GLint major = 0, minor = 0, encoding = 0, component = 0, read_buffer = 0, draw_buffer = 0, sample_buffers = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &encoding);
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &component);
	glGetIntegerv(GL_READ_BUFFER, &read_buffer);
	glGetIntegerv(GL_DRAW_BUFFER0, &draw_buffer);
	glGetIntegerv(GL_SAMPLE_BUFFERS, &sample_buffers);
	report["context"] = { { "major", major }, { "minor", minor }, { "vendor", gl_string(GL_VENDOR) }, { "renderer", gl_string(GL_RENDERER) }, { "version", gl_string(GL_VERSION) }, { "shading_language", gl_string(GL_SHADING_LANGUAGE_VERSION) } };
	report["window_target"] = { { "color_encoding", encoding }, { "component_type", component }, { "read_buffer", read_buffer }, { "draw_buffer", draw_buffer }, { "sample_buffers", sample_buffers } };
	report["window_diagnostic_errors"] = errors();
	report["window_diagnostics_valid"] = report["window_diagnostic_errors"].empty();
	report["color_format"] = format;
	report["color_supported_samples"] = supported_samples(format);
	report["depth_supported_samples"] = supported_samples(GL_DEPTH_COMPONENT16);
	report["format_query_errors"] = errors();
	const int selected_samples = android_gpu_msaa_samples(requested_samples);
	report["selected_samples"] = selected_samples;
	if (!selected_samples) {
		report["first_failure"] = "no_compatible_sample_count";
		finish();
		return;
	}
	if (samples != 0 || !report["setup_errors"].empty() || !report["format_query_errors"].empty()) {
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
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, selected_samples, format, width, height);
	GLint color_samples = 0, depth_samples = 0;
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &color_samples);
	report["color_allocation_errors"] = errors();
	glBindRenderbuffer(GL_RENDERBUFFER, depth);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, selected_samples, GL_DEPTH_COMPONENT16, width, height);
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
