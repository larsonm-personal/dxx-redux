#pragma once
/* Rendering prerequisite only: the ordinary world renderer does not call this.
 * Opaque coverage precedes objects. The caller orders partial coverage far to
 * near after opaque draws. This API supplies no visibility or sorting policy. */
enum { XL_SOFT_BASE_OPAQUE = 1,
	   XL_SOFT_BASE_PARTIAL = 2 };
int ogl_draw_xl_soft_base_pass(int pass, int nv, const g3s_point **points,
                               g3s_uvl *uv, g3s_lrgb *light, grs_bitmap *bitmap);
