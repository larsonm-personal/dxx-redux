#include "xl_soft_base_pass.h"

int ogl_draw_xl_soft_base_pass(int pass, int nv, const g3s_point **points,
                               g3s_uvl *uv, g3s_lrgb *light, grs_bitmap *bitmap)
{
	if ((pass != XL_SOFT_BASE_OPAQUE && pass != XL_SOFT_BASE_PARTIAL) || nv < 3 || nv > MAX_VERTS ||
	    !points || !uv || !light || !bitmap || !grd_curcanv || tmap_drawer_ptr != draw_tmap || !xl_resource_texture_scope_enabled() ||
	    d1_in_d2_use_d1_gameplay() || (Game_mode & GM_MULTI) || !ogl_allow_png() ||
	    !g_android_draw_face_ctx.valid || g_android_draw_face_ctx.tmap2 ||
	    grd_curcanv->cv_blend_func != GR_BLEND_NORMAL || grd_curcanv->cv_fade_level < GR_FADE_OFF ||
	    XL_FACE_ALPHA(1.0f) != 1.0f || gles3_shim_external_active() ||
	    gles3_shim_debug_mode || !glIsEnabled(GL_DEPTH_TEST) || glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE)) return 0;
	for (int i = 0; i < nv; ++i)
		if (!points[i]) return 0;
	const int seg = g_android_draw_face_ctx.seg, side = g_android_draw_face_ctx.side;
	if (Num_segments < 0 || Num_segments > MAX_SEGMENTS || seg < 0 || seg >= Num_segments ||
	    seg > Highest_segment_index || side < 0 || side >= 6 ||
	    Segments[seg].sides[side].tmap_num != g_android_draw_face_ctx.tmap1 ||
	    Segments[seg].sides[side].tmap_num2) return 0;
	GLint depth_func, active, textures[3];
	glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
	if (depth_func != GL_LESS && depth_func != GL_LEQUAL) return 0;
	glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
	for (int i = 0; i < 3; ++i) {
		OGL_ACTIVETEXTURE(GL_TEXTURE0 + i);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &textures[i]);
	}
	grs_bitmap *selected = xl_wall_animation_bitmap(bitmap, 0, 1);
	const int accepted = selected != bitmap && selected->gltexture && selected->gltexture->scoped_soft;
	if (accepted) {
		GLboolean depth_write;
		const GLboolean blend = glIsEnabled(GL_BLEND);
		GLint src_rgb, dst_rgb, src_alpha, dst_alpha, equation_rgb, equation_alpha;
		glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
		glGetIntegerv(GL_BLEND_SRC_RGB, &src_rgb);
		glGetIntegerv(GL_BLEND_DST_RGB, &dst_rgb);
		glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_alpha);
		glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_alpha);
		glGetIntegerv(GL_BLEND_EQUATION_RGB, &equation_rgb);
		glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &equation_alpha);
		const int previous_pass = gles3_shim_set_soft_surface_pass(pass);
		OGL_ACTIVETEXTURE(GL_TEXTURE0);
		android_ogl_reset_texture_bindings(&ogl_bind_texture_state);
		if (pass == XL_SOFT_BASE_PARTIAL) {
			glEnable(GL_BLEND);
			glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
			glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			glDepthMask(GL_FALSE);
		} else {
			glDisable(GL_BLEND);
			glDepthMask(GL_TRUE);
		}
		g3_draw_tmap(nv, points, uv, light, selected);
		gles3_shim_set_soft_surface_pass(previous_pass);
		gles3_shim_flush_state();
		glDepthMask(depth_write);
		glBlendEquationSeparate(equation_rgb, equation_alpha);
		glBlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha);
		if (blend) glEnable(GL_BLEND);
		else glDisable(GL_BLEND);
	}
	/* Uploads can alter bindings even when frame admission later fails. */
	for (int i = 0; i < 3; ++i) {
		OGL_ACTIVETEXTURE(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, textures[i] && glIsTexture(textures[i]) ? textures[i] : 0);
	}
	OGL_ACTIVETEXTURE(active);
	android_ogl_reset_texture_bindings(&ogl_bind_texture_state);
	return accepted;
}
