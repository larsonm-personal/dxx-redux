/* XL-only OpenGL backend; built into arch_ogl only with DXX_XL_PILOT */
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <math.h>
#include "3d.h"
#include "internal.h"
#include "playsave.h"
#include "preview.h"
#include "../../../d2/3d/globvars.h"

/* Preserve the existing desktop trigonometric precision */
#if defined(_WIN32) || (defined(__APPLE__) && defined(__MACH__)) || defined(__sun__) || defined(macintosh)
#define cosf(a) cos(a)
#define sinf(a) sin(a)
#endif

void ogl_record_xl_corona_depth(void)
{
	xl_preview_record_corona_depth(glIsEnabled(GL_DEPTH_TEST) != 0, GameCfg.ClassicDepth);
}

int ogl_draw_xl_corona(const int32_t center[3], int32_t radius, const float rgb[3], float intensity)
{
	/* Procedural soft halo: authored RGB/state without an external artwork dependency */
	enum { slices = 24,
		   vertices = slices + 2 };
	GLfloat positions[vertices * 3], colors[vertices * 4];
	vms_vector world = { center[0], center[1], center[2] };
	g3s_point point;
	GLboolean depth_write;
	const int old_fade = grd_curcanv->cv_fade_level;
	const ubyte old_blend = grd_curcanv->cv_blend_func;
	int i, c;
	if (g3_rotate_point(&point, &world) & CC_BEHIND) return 0;
	for (i = 0; i < vertices; ++i) {
		/* Match ogl_start_frame's clockwise front faces */
		const float angle = (float) (i - 1) * (-2.0f * 3.14159265358979323846f / slices);
		positions[i * 3] = f2fl(point.p3_vec.x) + (i ? cosf(angle) * f2fl(fixmul(radius, Matrix_scale.x)) : 0);
		positions[i * 3 + 1] = f2fl(point.p3_vec.y) + (i ? sinf(angle) * f2fl(fixmul(radius, Matrix_scale.y)) : 0);
		positions[i * 3 + 2] = -f2fl(point.p3_vec.z);
		for (c = 0; c < 3; ++c) colors[i * 4 + c] = rgb[c] * intensity;
		colors[i * 4 + 3] = i ? 0 : 1;
	}
	glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
	glDepthMask(GL_FALSE);
	OGL_DISABLE(TEXTURE_2D);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	gr_settransblend(GR_FADE_OFF, GR_BLEND_ADDITIVE_A);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, positions);
	glColorPointer(4, GL_FLOAT, 0, colors);
	glDrawArrays(GL_TRIANGLE_FAN, 0, vertices);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	gr_settransblend(old_fade, old_blend);
	glDepthMask(depth_write);
	return 1;
}
