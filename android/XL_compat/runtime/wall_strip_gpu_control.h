#ifndef DXX_XL_WALL_STRIP_GPU_CONTROL_H
#define DXX_XL_WALL_STRIP_GPU_CONTROL_H
#include "doors.h"
#include "pog_resources.h"

// Caller owns an isolated framebuffer and preserves GL state and matrices
inline unsigned xl_wall_strip_gpu_controls()
{
	const auto check = [](bool value, const char *message) { if (!value) throw std::runtime_error(message); };
	check(xl_level_extensions_active() && Num_walls > 3, "Wall strip GPU controls require the derived loaded fixture");
	const int independent = Num_walls - 2, clip = Walls[0].clip_num;
	check(Walls[independent].clip_num == clip && xl_pog_door_frames(clip) == 16, "Wall strip GPU fixture has different ownership");
	const int texture = WallAnims[clip].frames[0], bitmap = Textures[texture].index;
	struct Restore {
		nlohmann::json phases = xl_door_phases_snapshot();
		ubyte blend = grd_curcanv->cv_blend_func, fade = grd_curcanv->cv_fade_level;
		GLboolean dither = glIsEnabled(GL_DITHER);
		~Restore()
		{
			xl_door_phases_restore(phases, true);
			grd_curcanv->cv_blend_func = blend;
			grd_curcanv->cv_fade_level = fade;
			ogl_set_blending();
			if (dither) glEnable(GL_DITHER);
			else glDisable(GL_DITHER);
		}
	} restore;
	g3s_point vertices[4]{};
	const g3s_point *points[4]{};
	g3s_uvl uv[4]{};
	g3s_lrgb light[4]{};
	const int corners[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
	for (int i = 0; i < 4; ++i) {
		vertices[i].p3_vec = { (corners[i][0] * 2 - 1) * F1_0, (corners[i][1] * 2 - 1) * F1_0, 0 };
		points[i] = &vertices[i];
		uv[i].u = corners[i][0] * F1_0;
		uv[i].v = corners[i][1] * F1_0;
		light[i].r = light[i].g = light[i].b = F1_0;
	}
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glViewport(0, 0, 16, 16);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_DITHER);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	grd_curcanv->cv_blend_func = GR_BLEND_NORMAL;
	grd_curcanv->cv_fade_level = GR_FADE_OFF;
	ogl_set_blending();
	unsigned cases = 0;
	for (int frame : { 0, 2, 4, 1, 15, 14 }) {
		const auto &a = Walls[0], &b = Walls[independent];
		check(xl_door_record_frame(a.segnum, a.sidenum, clip, frame) && xl_door_record_frame(b.segnum, b.sidenum, clip, 6), "GPU fixture phase assignment failed");
		for (int index : { 0, independent }) {
			const auto &wall = Walls[index];
			auto *view = xl_door_side_bitmap(wall.segnum, wall.sidenum, texture, &GameBitmaps[bitmap]);
			const int selected = index ? 6 : frame;
			check(view == xl_pog_side_frame(bitmap, selected) && view->bm_w == 8 && view->bm_h == 8, "GPU wall selected a whole strip or another side's frame");
			for (bool reupload : { false, true }) {
				if (reupload) ogl_freebmtexture(view);
				glClearColor(73.f / 255, 119.f / 255, 167.f / 255, 1);
				glClear(GL_COLOR_BUFFER_BIT);
				g3_draw_tmap(4, points, uv, light, view);
				unsigned char pixel[4]{};
				glReadPixels(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
				const unsigned char expected[4] = { static_cast<unsigned char>(selected & 1 ? 73 : selected + 20),
					                                static_cast<unsigned char>(selected & 1 ? 119 : 50),
					                                static_cast<unsigned char>(selected & 1 ? 167 : 30), 255 };
				for (int channel = 0; channel < 4; ++channel)
					check(std::abs(int(pixel[channel]) - int(expected[channel])) <= 1, "Native wall strip GPU pixel differs from selected frame");
				check(glGetError() == GL_NO_ERROR, "Wall strip draw or reupload raised a GPU error");
				++cases;
			}
		}
	}
	return cases;
}
#endif
