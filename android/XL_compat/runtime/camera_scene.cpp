#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#endif
#include "camera_scene.h"
#include "transparency_scene.h"
#include "particle_renderer.h"
#include "../camera_view_policy.h"
#include <algorithm>
#include <stdexcept>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "segment.h"
#include "3d.h"
#include "render.h"
#include "game.h"
#include "playsave.h"
#include "endlevel.h"
#include "newdemo.h"
#include "ogl_init.h"
#include "internal.h"
#include "../../../d2/arch/ogl/oglprog.h"
#include "../../../d2/3d/globvars.h"
extern vms_vector Viewer_eye;
}
#undef min
#undef max
#if !defined(OGLES) || defined(ANDROID)
#include "camera_target_gpu.h"
namespace
{
bool scene_active = false;
xl_compat::CameraEffectsPolicy policy;
struct SceneState {
	xl_compat::LightningDrawState draw;
	object *viewer = Viewer;
	grs_canvas *canvas = grd_curcanv;
	int width = Canvas_width, height = Canvas_height;
	fix half_width = Canv_w2, half_height = Canv_h2, zoom = View_zoom, render_zoom = Render_zoom;
	vms_vector window_scale = Window_scale, matrix_scale = Matrix_scale, position = View_position, eye = Viewer_eye;
	vms_matrix view = View_matrix, unscaled = Unscaled_matrix;
	int cockpit = PlayerCfg.CurrentCockpitMode;
	int texture_enabled = GL_TEXTURE_2D_enabled;
	GLfloat projection[16], modelview[16], merged[2][16];
	GLint mode, textures[3];
	GLuint programs[2]{ ogl_prog_tex2, ogl_prog_tex2m };
	SceneState()
	{
		for (unsigned i = 0; i < 3; ++i) {
			glActiveTexture(GL_TEXTURE1 + i);
			glGetIntegerv(GL_TEXTURE_BINDING_2D, textures + i);
		}
		glActiveTexture(GL_TEXTURE0);
#ifdef ANDROID
		mode = gles3_shim_get_matrix_mode();
		std::copy_n(gles3_shim_get_projection(), 16, projection);
		std::copy_n(gles3_shim_get_modelview(), 16, modelview);
#else
		glGetIntegerv(GL_MATRIX_MODE, &mode);
		glGetFloatv(GL_PROJECTION_MATRIX, projection);
		glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
#endif
		for (unsigned i = 0; i < 2; ++i)
			if (programs[i]) glGetUniformfv(programs[i], glGetUniformLocation(programs[i], "umat"), merged[i]);
	}
	~SceneState()
	{
		scene_active = false;
		Viewer = viewer;
		grd_curcanv = canvas;
		Canvas_width = width;
		Canvas_height = height;
		Canv_w2 = half_width;
		Canv_h2 = half_height;
		View_zoom = zoom;
		Render_zoom = render_zoom;
		Window_scale = window_scale;
		Matrix_scale = matrix_scale;
		View_position = position;
		Viewer_eye = eye;
		View_matrix = view;
		Unscaled_matrix = unscaled;
		PlayerCfg.CurrentCockpitMode = cockpit;
		for (unsigned i = 0; i < 3; ++i) {
			glActiveTexture(GL_TEXTURE1 + i);
			glBindTexture(GL_TEXTURE_2D, textures[i]);
		}
		glActiveTexture(GL_TEXTURE0);
		if (texture_enabled == 1) glEnable(GL_TEXTURE_2D);
		else glDisable(GL_TEXTURE_2D);
		GL_TEXTURE_2D_enabled = texture_enabled;
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(projection);
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(modelview);
		glMatrixMode(mode);
		for (unsigned i = 0; i < 2; ++i)
			if (programs[i]) {
				xl_compat::lightning_gpu_program(programs[i]);
				glUniformMatrix4fv(glGetUniformLocation(programs[i], "umat"), 1, GL_FALSE, merged[i]);
			}
	}
};
} // namespace
extern "C" int xl_camera_scene_active(void)
{
	return scene_active;
}
extern "C" int xl_camera_scene_particles(void)
{
	return !scene_active || policy.particles;
}
extern "C" int xl_camera_scene_lightning(void)
{
	return !scene_active || policy.lightning;
}
namespace xl_compat
{
void capture_camera_scene(CameraTargetGpu &target, const CameraTargetSpec &spec, object &actor,
                          int zoom, bool particles, bool lightning)
{
	if (!gl_initialized || !grd_curcanv || !grd_curscreen || free_point_num || scene_active || xl_transparency_collecting() ||
	    xl_camera_target_active_dimensions(nullptr, nullptr) || (Game_mode & GM_MULTI) || Endlevel_sequence ||
	    Newdemo_state != ND_STATE_NORMAL || actor.segnum < 0 || actor.segnum > Highest_segment_index || zoom <= 0)
		throw std::runtime_error("XL camera capture requires an ordinary singleplayer frame boundary and valid viewer");
	if (spec.width < 1 || spec.height < 1 || spec.width > 32767 || spec.height > 32767)
		throw std::runtime_error("XL camera canvas dimensions are invalid");
	SceneState restore;
	grs_canvas canvas = *grd_curcanv;
	canvas.cv_bitmap.bm_x = canvas.cv_bitmap.bm_y = 0;
	canvas.cv_bitmap.bm_w = short(spec.width);
	canvas.cv_bitmap.bm_h = short(spec.height);
	canvas.cv_fade_level = GR_FADE_OFF;
	canvas.cv_blend_func = GR_BLEND_NORMAL;
	grd_curcanv = &canvas;
	Viewer = &actor;
	Render_zoom = zoom;
	PlayerCfg.CurrentCockpitMode = CM_FULL_SCREEN;
	CameraEffectsInput options;
	options.camera = true;
	options.particle_monitors = particles;
	options.lightning_monitors = lightning;
	policy = camera_effects_policy(options);
	scene_active = true;
	try {
		target.capture(spec, [] { render_frame(0, 0); });
	} catch (...) {
		xl_transparency_clear();
		throw;
	}
}
} // namespace xl_compat

#else
extern "C" int xl_camera_scene_active(void)
{
	return 0;
}
extern "C" int xl_camera_scene_particles(void)
{
	return 1;
}
extern "C" int xl_camera_scene_lightning(void)
{
	return 1;
}
namespace xl_compat
{
void capture_camera_scene(CameraTargetGpu &, const CameraTargetSpec &, object &, int, bool, bool)
{
	throw std::runtime_error("XL camera capture requires programmable OpenGL");
}
} // namespace xl_compat
#endif
