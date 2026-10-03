#ifdef ANDROID
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
#include <sys/system_properties.h>
#include <unistd.h>
#include <nlohmann/json.hpp>
extern "C" {
#include "ogl_init.h"
#include "config.h"
#include "android_gpu_capabilities.h"
#include "android_gpu_policy.h"
#include "android_log.h"
}

namespace
{
using nlohmann::json;
std::string files_root;
std::mutex report_mutex;
std::string last_report = "{}";
std::vector<int> color_samples, depth_samples;
unsigned int color_format;
int max_size;
#ifdef INTROSPECT_ON
bool debug_disabled;
#endif

std::vector<int> samples(GLenum format)
{
	GLint count = 0;
	glGetInternalformativ(GL_RENDERBUFFER, format, GL_NUM_SAMPLE_COUNTS, 1, &count);
	if (count <= 0 || count > 256) return {};
	std::vector<int> values(count);
	glGetInternalformativ(GL_RENDERBUFFER, format, GL_SAMPLES, count, values.data());
	return values;
}

const char *gl_string(GLenum name)
{
	const char *value = reinterpret_cast<const char *>(glGetString(name));
	return value ? value : "";
}
} // namespace

extern "C" void android_gpu_capabilities_set_root(const char *root)
{
	files_root = root ? root : "";
}

extern "C" int android_gpu_msaa_samples(int requested)
{
	return color_format ? android_gpu_common_samples(color_samples.data(), static_cast<int>(color_samples.size()),
	                                                 depth_samples.data(), static_cast<int>(depth_samples.size()), requested)
	                    : 0;
}

extern "C" unsigned int android_gpu_msaa_format(void)
{
	return color_format;
}
extern "C" int android_gpu_max_renderbuffer_size(void)
{
	return max_size;
}

extern "C" void android_gpu_capabilities_query(float *max_anisotropy, int *max_samples, int *timer_available)
{
	/* Isolate capability errors from earlier work, retaining them in the graphics log */
	for (GLenum error; (error = glGetError()) != GL_NO_ERROR;)
		debug_log(DLOG_GRAPHICS, "GPU capability prior GL error: 0x%x", error);
	*max_anisotropy = 1.0f;
	const char *extensions = gl_string(GL_EXTENSIONS);
	bool aniso = android_gpu_has_extension(extensions, "GL_EXT_texture_filter_anisotropic");
#ifdef INTROSPECT_ON
	if (debug_disabled) aniso = false;
#endif
	if (aniso) glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, max_anisotropy);
	*timer_available = android_gpu_has_extension(extensions, "GL_EXT_disjoint_timer_query");
	GLint read = 0, draw = 0, r = 0, g = 0, b = 0, a = 0, encoding = 0, component = 0, window_samples = 0;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glGetIntegerv(GL_RED_BITS, &r);
	glGetIntegerv(GL_GREEN_BITS, &g);
	glGetIntegerv(GL_BLUE_BITS, &b);
	glGetIntegerv(GL_ALPHA_BITS, &a);
	glGetIntegerv(GL_SAMPLES, &window_samples);
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &encoding);
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &component);
	glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &max_size);
	color_format = android_gpu_color_format(r, g, b, a);
	const char *reason = "";
	if (!color_format || encoding != GL_LINEAR || component != GL_UNSIGNED_NORMALIZED) {
		color_format = 0;
		reason = "MSAA is unavailable for this display's color format";
	} else if (window_samples != 0) {
		color_format = 0;
		reason = "MSAA is unavailable with this display's existing multisampling";
	}
	color_samples = color_format ? samples(color_format) : std::vector<int>{};
	depth_samples = color_format ? samples(GL_DEPTH_COMPONENT16) : std::vector<int>{};
	glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
	GLenum query_error = glGetError();
	if (query_error != GL_NO_ERROR) {
		color_format = 0;
		*max_anisotropy = 1.0f;
		*timer_available = 0;
		reason = "The graphics driver could not report reliable MSAA support";
		while (glGetError() != GL_NO_ERROR) {}
	}
#ifdef INTROSPECT_ON
	if (debug_disabled) {
		color_format = 0;
		*max_anisotropy = 1.0f;
		reason = "MSAA is unavailable (diagnostic capability override)";
	}
#endif
	*max_samples = 0;
	for (int value : color_samples)
		if (android_gpu_msaa_samples(value) == value) *max_samples = (*max_samples > value ? *max_samples : value);
	if (*max_samples < 2 && !*reason) reason = "MSAA is unavailable: color and depth buffers have no shared sample count";
	char fingerprint[PROP_VALUE_MAX] = {};
	__system_property_get("ro.build.fingerprint", fingerprint);
	json report;
	report["schema"] = 1;
	report["fingerprint"] = fingerprint;
	report["color_depth"] = GameCfg.ColorDepth;
	report["vendor"] = gl_string(GL_VENDOR);
	report["renderer"] = gl_string(GL_RENDERER);
	report["version"] = gl_string(GL_VERSION);
	report["window_bits"] = { r, g, b, a };
	report["color_format"] = color_format;
	report["color_encoding"] = encoding;
	report["component_type"] = component;
	report["window_samples"] = window_samples;
	report["max_renderbuffer_size"] = max_size;
	report["aniso_max"] = *max_anisotropy;
	report["aniso_reason"] = *max_anisotropy > 1 ? "" : query_error != GL_NO_ERROR ? "The graphics driver could not report reliable AF support"
	                                                                               : "Anisotropic filtering is unavailable on this graphics driver";
	report["msaa_max"] = *max_samples;
	report["msaa_reason"] = reason;
	report["msaa_2"] = android_gpu_msaa_samples(2);
	report["msaa_4"] = android_gpu_msaa_samples(4);
	report["color_samples"] = color_samples;
	report["depth_samples"] = depth_samples;
	report["query_error"] = query_error;
	const auto text = report.dump(2) + "\n";
	{
		std::lock_guard<std::mutex> lock(report_mutex);
		last_report = text;
	}
	debug_log_batch_force(DLOG_GRAPHICS, ("GPU capabilities\n" + text).c_str());
	if (files_root.empty()) return;
	const auto path = files_root + "/graphics-capabilities-" + std::to_string(GameCfg.ColorDepth) + ".json";
	const auto temporary = path + "." + std::to_string(getpid()) + ".tmp";
	FILE *file = fopen(temporary.c_str(), "wb");
	if (!file) return;
	bool good = fwrite(text.data(), 1, text.size(), file) == text.size();
	good = fclose(file) == 0 && good;
	if (!good || rename(temporary.c_str(), path.c_str())) remove(temporary.c_str());
}

extern "C" void android_gpu_capabilities_json(char *buffer, unsigned int size)
{
	std::lock_guard<std::mutex> lock(report_mutex);
	if (size) snprintf(buffer, size, "%s", last_report.c_str());
}

#ifdef INTROSPECT_ON
extern "C" void android_gpu_capabilities_debug_disable(int disabled)
{
	debug_disabled = disabled != 0;
	extern GLfloat ogl_maxanisotropy;
	extern int ogl_msaa_max_samples, ogl_gpu_timer_available;
	android_gpu_capabilities_query(&ogl_maxanisotropy, &ogl_msaa_max_samples, &ogl_gpu_timer_available);
}
#endif
#endif
