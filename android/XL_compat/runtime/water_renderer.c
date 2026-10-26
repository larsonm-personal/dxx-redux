/* XL colored water boundaries, called during the transparent geometry pass */
#ifdef _WIN32
#include <winsock2.h>
#endif
#include "3d.h"
#include "internal.h"
#include "preview.h"

void ogl_draw_xl_water_portal(const int32_t points[4][3], const float colors[4][4])
{
	GLfloat positions[12];
	GLboolean depth_write;
	const GLboolean texture = glIsEnabled(GL_TEXTURE_2D);
	const int old_fade = grd_curcanv->cv_fade_level;
	const ubyte old_blend = grd_curcanv->cv_blend_func;
	int i;
	for (i = 0; i < 4; ++i) {
		vms_vector world = { points[i][0], points[i][1], points[i][2] };
		g3s_point point;
		/* GL clips the polygon, including faces crossing the near plane */
		g3_rotate_point(&point, &world);
		positions[i * 3] = f2fl(point.p3_vec.x);
		positions[i * 3 + 1] = f2fl(point.p3_vec.y);
		positions[i * 3 + 2] = -f2fl(point.p3_vec.z);
	}
	glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
	glDepthMask(GL_FALSE);
	OGL_DISABLE(TEXTURE_2D);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	gr_settransblend(GR_FADE_OFF, GR_BLEND_NORMAL);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, positions);
	glColorPointer(4, GL_FLOAT, 0, colors);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	xl_preview_record_water_draw(glIsEnabled(GL_DEPTH_TEST) != 0);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	gr_settransblend(old_fade, old_blend);
	glDepthMask(depth_write);
	if (texture) OGL_ENABLE(TEXTURE_2D);
}
