#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <string>
#include <vector>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include "gles3_shim.h"
#include "corona_probe.h"
#include "preview.h"
extern "C" {
#include "3d.h"
#include "object.h"
extern vms_vector View_position;
extern vms_matrix View_matrix;
}

namespace
{
std::string result = "null";
std::string scene_result = "null";
bool scene_requested = false;
bool object_requested = false;
std::string object_result = "null";
struct ObjectSample {
	int object;
	int32_t center[3], radius;
};
std::vector<ObjectSample> object_samples;
GLint object_framebuffer = 0;
constexpr int size = 64;
} // namespace

void xl_corona_object_probe_request(void)
{
	object_result = "null";
	object_samples.clear();
	object_requested = true;
}

const char *xl_corona_object_probe_result(void)
{
	return object_result.c_str();
}

void xl_corona_object_probe(int after_objects, const int32_t eye[3])
{
	if (!object_requested || !xl_preview_pilot_active()) return;
	xl_texture_emitter emitter{};
	if (!xl_preview_texture_emitter(40, 4, &emitter)) return;
	GLint framebuffer, current_query, front_face, cull_face, depth_func;
	GLboolean mask[4];
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);
	glGetQueryiv(GL_ANY_SAMPLES_PASSED, GL_CURRENT_QUERY, &current_query);
	glGetIntegerv(GL_FRONT_FACE, &front_face);
	glGetIntegerv(GL_CULL_FACE_MODE, &cull_face);
	glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
	glGetBooleanv(GL_COLOR_WRITEMASK, mask);
	const GLboolean depth = glIsEnabled(GL_DEPTH_TEST), scissor = glIsEnabled(GL_SCISSOR_TEST), cull = glIsEnabled(GL_CULL_FACE);
	const GLenum prior_error = glGetError();
	if (current_query || prior_error || !depth) {
		object_result = nlohmann::json({ { "passed", false }, { "failure", "Invalid object-pass GL state" }, { "prior_error", prior_error } }).dump();
		object_requested = false;
		return;
	}
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDisable(GL_SCISSOR_TEST);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CW);
	glCullFace(GL_BACK);
	glDepthFunc(GL_LEQUAL);
	GLuint query;
	glGenQueries(1, &query);
	auto samples = [&](const ObjectSample &sample, bool use_depth) {
		(use_depth ? glEnable : glDisable)(GL_DEPTH_TEST);
		glBeginQuery(GL_ANY_SAMPLES_PASSED, query);
		ogl_draw_xl_corona(sample.center, sample.radius, emitter.rgb, 0.5f);
		glEndQuery(GL_ANY_SAMPLES_PASSED);
		GLuint visible = 0;
		glGetQueryObjectuiv(query, GL_QUERY_RESULT, &visible);
		return visible != 0;
	};
	nlohmann::json blocked = nlohmann::json::array();
	if (!after_objects) {
		object_framebuffer = framebuffer;
		object_samples.clear();
		const vms_vector observer = { eye[0], eye[1], eye[2] };
		for (int i = 0; i <= Highest_object_index; ++i) {
			const auto &object = Objects[i];
			if (object.type != OBJ_ROBOT || object.render_type != RT_POLYOBJ || object.size <= 0) continue;
			vms_vector direction, point = object.pos;
			vm_vec_normalized_dir(&direction, &point, &observer);
			vm_vec_scale_add2(&point, &direction, object.size);
			// A robot's bounding-sphere center need not lie inside its current
			// silhouette. Sample its projected body instead of assuming a solid center
			for (int y = -2; y <= 2; ++y)
				for (int x = -2; x <= 2; ++x) {
					vms_vector sample_point = point;
					vm_vec_scale_add2(&sample_point, &View_matrix.rvec, x * object.size / 4);
					vm_vec_scale_add2(&sample_point, &View_matrix.uvec, y * object.size / 4);
					ObjectSample sample{ i, { sample_point.x, sample_point.y, sample_point.z }, object.size / 32 };
					// Only retain halos that walls do not already hide
					if (samples(sample, true)) object_samples.push_back(sample);
				}
		}
	} else {
		for (const auto &sample : object_samples)
			if (!samples(sample, true) && samples(sample, false))
				blocked.push_back({ { "object", sample.object }, { "center", sample.center }, { "radius", sample.radius } });
	}
	glDeleteQueries(1, &query);
	glColorMask(mask[0], mask[1], mask[2], mask[3]);
	glFrontFace(front_face);
	glCullFace(cull_face);
	glDepthFunc(depth_func);
	(depth ? glEnable : glDisable)(GL_DEPTH_TEST);
	(scissor ? glEnable : glDisable)(GL_SCISSOR_TEST);
	(cull ? glEnable : glDisable)(GL_CULL_FACE);
	const GLenum error = glGetError();
	if (after_objects || error) {
		object_result = nlohmann::json({ { "passed", after_objects && framebuffer == object_framebuffer && !blocked.empty() && !error },
		                                 { "scope", "actual_object_pass_depth" },
		                                 { "visible_before_objects", object_samples.size() },
		                                 { "blocked_by_objects", blocked },
		                                 { "same_framebuffer", framebuffer == object_framebuffer },
		                                 { "gl_error", error } })
		                    .dump();
		object_requested = false;
	}
}

void xl_corona_scene_probe_request(void)
{
	scene_result = "null";
	scene_requested = true;
}

const char *xl_corona_scene_probe_result(void)
{
	return scene_result.c_str();
}

void xl_corona_scene_probe(const int32_t eye[3], int eye_segment)
{
	if (!scene_requested || !xl_preview_pilot_active()) return;
	scene_requested = false;
	nlohmann::json report = { { "passed", false }, { "scope", "actual_scene_depth_wall" } };
	xl_texture_emitter emitter{};
	int32_t center[3], radius;
	float facing;
	if (eye_segment != 40 || !xl_preview_texture_emitter(40, 4, &emitter) ||
	    !xl_preview_corona_geometry(40, 4, eye, eye_segment, center, &radius, &facing)) {
		report["failure"] = "Requires controlled segment 40 view of texture 405";
		scene_result = report.dump();
		return;
	}
	GLboolean color_write[4];
	glGetBooleanv(GL_COLOR_WRITEMASK, color_write);
	const GLboolean depth = glIsEnabled(GL_DEPTH_TEST);
	GLint current_query = 0;
	glGetQueryiv(GL_ANY_SAMPLES_PASSED, GL_CURRENT_QUERY, &current_query);
	const GLenum prior_error = glGetError();
	if (current_query || prior_error) {
		report["failure"] = "Existing sample query or GL error";
		report["prior_error"] = prior_error;
		scene_result = report.dump();
		return;
	}
	GLuint query;
	glGenQueries(1, &query);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	// Same authored halo, moved beyond the opaque wall along the camera ray
	int32_t behind[3];
	for (int i = 0; i < 3; ++i) behind[i] = center[i] + (center[i] - eye[i]);
	auto samples = [&](const int32_t point[3], bool use_depth) {
		(use_depth ? glEnable : glDisable)(GL_DEPTH_TEST);
		glBeginQuery(GL_ANY_SAMPLES_PASSED, query);
		ogl_draw_xl_corona(point, radius, emitter.rgb, emitter.intensity * facing * 0.5f);
		glEndQuery(GL_ANY_SAMPLES_PASSED);
		GLuint visible = 0;
		glGetQueryObjectuiv(query, GL_QUERY_RESULT, &visible);
		return visible != 0;
	};
	const bool visible_control = samples(center, false);
	const bool visible_scene = samples(center, true);
	const bool behind_control = samples(behind, false);
	const bool behind_scene = samples(behind, true);
	glDeleteQueries(1, &query);
	glColorMask(color_write[0], color_write[1], color_write[2], color_write[3]);
	(depth ? glEnable : glDisable)(GL_DEPTH_TEST);
	const GLenum error = glGetError();
	report["scene_depth_enabled"] = depth != 0;
	report["visible_without_depth"] = visible_control;
	report["visible_with_depth"] = visible_scene;
	report["behind_without_depth"] = behind_control;
	report["behind_with_depth"] = behind_scene;
	report["gl_error"] = error;
	report["passed"] = depth && visible_control && visible_scene && behind_control && !behind_scene && !error;
	scene_result = report.dump();
}

const char *xl_corona_pixel_probe_result(void)
{
	return result.c_str();
}

void xl_corona_pixel_probe(void)
{
	using nlohmann::json;
	json report = { { "passed", false }, { "scope", "controlled_owned_framebuffer" } };
	xl_texture_emitter emitter{};
	if (!xl_preview_pilot_active() || !xl_preview_texture_emitter(40, 4, &emitter) || emitter.texture != 405 || eglGetCurrentContext() == EGL_NO_CONTEXT) {
		report["failure"] = "Requires original pilot texture 405 and current GL context";
		result = report.dump();
		return;
	}
	GLint read_fbo, draw_fbo, renderbuffer, viewport[4], depth_function, blend_source, blend_destination, scissor[4];
	GLint pack_buffer, pack_alignment, pack_row_length, pack_skip_pixels, pack_skip_rows, blend_source_alpha, blend_destination_alpha;
	GLint front_face, cull_face;
	GLfloat clear_color[4], clear_depth;
	GLboolean depth_write, color_write[4];
	const GLboolean depth = glIsEnabled(GL_DEPTH_TEST), blend = glIsEnabled(GL_BLEND), cull = glIsEnabled(GL_CULL_FACE), scissor_enabled = glIsEnabled(GL_SCISSOR_TEST);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
	glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
	glGetIntegerv(GL_VIEWPORT, viewport);
	glGetIntegerv(GL_SCISSOR_BOX, scissor);
	glGetIntegerv(GL_DEPTH_FUNC, &depth_function);
	glGetIntegerv(GL_FRONT_FACE, &front_face);
	glGetIntegerv(GL_CULL_FACE_MODE, &cull_face);
	glGetIntegerv(GL_BLEND_SRC_RGB, &blend_source);
	glGetIntegerv(GL_BLEND_DST_RGB, &blend_destination);
	glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend_source_alpha);
	glGetIntegerv(GL_BLEND_DST_ALPHA, &blend_destination_alpha);
	glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack_buffer);
	glGetIntegerv(GL_PACK_ALIGNMENT, &pack_alignment);
	glGetIntegerv(GL_PACK_ROW_LENGTH, &pack_row_length);
	glGetIntegerv(GL_PACK_SKIP_PIXELS, &pack_skip_pixels);
	glGetIntegerv(GL_PACK_SKIP_ROWS, &pack_skip_rows);
	glGetFloatv(GL_COLOR_CLEAR_VALUE, clear_color);
	glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clear_depth);
	glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
	glGetBooleanv(GL_COLOR_WRITEMASK, color_write);
	const auto view = View_matrix;
	const auto position = View_position, scale = Matrix_scale;
	const auto prior_error = glGetError();
	report["prior_error"] = prior_error;
	GLuint fbo = 0, color = 0, depth_buffer = 0;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glGenRenderbuffers(1, &color);
	glBindRenderbuffer(GL_RENDERBUFFER, color);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, size, size);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
	glGenRenderbuffers(1, &depth_buffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depth_buffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_buffer);
	const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrthof(-1, 1, -1, 1, 0, 4);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	View_matrix = vmd_identity_matrix;
	View_position = {};
	Matrix_scale = { F1_0, F1_0, F1_0 };
	glViewport(0, 0, size, size);
	// Match ogl_start_frame rather than hiding winding bugs in a two-sided fixture
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CW);
	glCullFace(GL_BACK);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glDepthFunc(GL_LEQUAL);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	std::array<unsigned char, size * size * 4> pixels{};
	const int32_t center[3] = { 0, 0, 2 * F1_0 };
	bool valid = complete && prior_error == GL_NO_ERROR;
	for (int mode = 0; mode < 3; ++mode) {
		glDisable(GL_SCISSOR_TEST);
		glDepthMask(GL_TRUE);
		glClearColor(0, 0, 0, 1);
		glClearDepthf(1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		if (mode) {
			// Known nearer depth: left half or entire halo, independent of scene brightness
			glEnable(GL_SCISSOR_TEST);
			glScissor(0, 0, mode == 1 ? size / 2 : size, size);
			glClearDepthf(0.25f);
			glClear(GL_DEPTH_BUFFER_BIT);
			glDisable(GL_SCISSOR_TEST);
		}
		const int submitted = ogl_draw_xl_corona(center, F1_0 * 3 / 4, emitter.rgb, 0.5f);
		glReadPixels(0, 0, size, size, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		int left = 0, right = 0, invalid_color = 0;
		for (int y = 0; y < size; ++y)
			for (int x = 0; x < size; ++x) {
				const auto *p = &pixels[(y * size + x) * 4];
				if (p[0] || p[1] || p[2]) {
					(x < size / 2 ? left : right)++;
					// Quantization tolerance for authored orange, including low-alpha edges
					if (p[2] > 1 || std::abs(float(p[1]) - float(p[0]) * emitter.rgb[1] / emitter.rgb[0]) > 2) ++invalid_color;
				}
			}
		GLboolean after_depth_write;
		glGetBooleanv(GL_DEPTH_WRITEMASK, &after_depth_write);
		const GLenum error = glGetError();
		const bool visible = mode == 0 ? left > 100 && right > 100 : mode == 1 ? left == 0 && right > 100
		                                                                       : left == 0 && right == 0;
		valid = valid && submitted && visible && !invalid_color && after_depth_write && error == GL_NO_ERROR;
		report[mode == 0 ? "visible" : mode == 1 ? "partial"
		                                         : "blocked"] = {
			{ "left_pixels", left }, { "right_pixels", right }, { "invalid_color", invalid_color }, { "depth_write_restored", after_depth_write != 0 }, { "gl_error", error }
		};
	}
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	View_matrix = view;
	View_position = position;
	Matrix_scale = scale;
	glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fbo);
	glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
	glDeleteFramebuffers(1, &fbo);
	glDeleteRenderbuffers(1, &color);
	glDeleteRenderbuffers(1, &depth_buffer);
	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
	glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
	glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
	glClearDepthf(clear_depth);
	glDepthMask(depth_write);
	glColorMask(color_write[0], color_write[1], color_write[2], color_write[3]);
	glDepthFunc(depth_function);
	glFrontFace(front_face);
	glCullFace(cull_face);
	glBlendFuncSeparate(blend_source, blend_destination, blend_source_alpha, blend_destination_alpha);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, pack_buffer);
	glPixelStorei(GL_PACK_ALIGNMENT, pack_alignment);
	glPixelStorei(GL_PACK_ROW_LENGTH, pack_row_length);
	glPixelStorei(GL_PACK_SKIP_PIXELS, pack_skip_pixels);
	glPixelStorei(GL_PACK_SKIP_ROWS, pack_skip_rows);
	(depth ? glEnable : glDisable)(GL_DEPTH_TEST);
	(blend ? glEnable : glDisable)(GL_BLEND);
	(cull ? glEnable : glDisable)(GL_CULL_FACE);
	(scissor_enabled ? glEnable : glDisable)(GL_SCISSOR_TEST);
	report["restore_error"] = glGetError();
	report["passed"] = valid && report["restore_error"] == GL_NO_ERROR;
	result = report.dump();
}
