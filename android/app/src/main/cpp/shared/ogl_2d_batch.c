#include <string.h>

#include "gr.h"
#include "ogl_init.h"
#include "dxxerror.h"
#include "ogl_2d_batch.h"

/* Bounded storage avoids allocation and also handles gauges at large resolutions */
#define OGL_2D_BATCH_VERTICES 1536

static GLfloat batch_vertices[OGL_2D_BATCH_VERTICES * 2];
static GLfloat batch_colors[OGL_2D_BATCH_VERTICES * 4];
static GLenum batch_mode;
static int batch_count;
static int batch_active;

static void ogl_2d_batch_flush(void)
{
	if (!batch_count)
		return;
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, batch_vertices);
	glColorPointer(4, GL_FLOAT, 0, batch_colors);
	glDrawArrays(batch_mode, 0, batch_count);
	glDisableClientState(GL_VERTEX_ARRAY);
	/* Match ogl_ulinec / ogl_urect's final client state */
	if (batch_mode == GL_LINES)
		glDisableClientState(GL_COLOR_ARRAY);
	batch_count = 0;
}

void gr_begin_2d_batch(void)
{
	Assert(!batch_active);
	batch_active = 1;
}

void gr_end_2d_batch(void)
{
	ogl_2d_batch_flush();
	batch_active = 0;
}

void ogl_2d_batch_draw(GLenum mode, int count, const GLfloat *vertices,
                       const GLfloat *colors)
{
	static const int rectangle_order[6] = { 0, 1, 2, 0, 2, 3 };
	int vertex_count, i;
	GLenum output_mode;

	if (!batch_active) {
		glVertexPointer(2, GL_FLOAT, 0, vertices);
		glColorPointer(4, GL_FLOAT, 0, colors);
		glDrawArrays(mode, 0, count);
		return;
	}
	Assert((mode == GL_LINES && count == 2) ||
	       (mode == GL_TRIANGLE_FAN && count == 4));
	output_mode = mode == GL_LINES ? GL_LINES : GL_TRIANGLES;
	vertex_count = mode == GL_LINES ? 2 : 6;
	if (batch_mode != output_mode || batch_count + vertex_count > OGL_2D_BATCH_VERTICES)
		ogl_2d_batch_flush();
	batch_mode = output_mode;
	for (i = 0; i < vertex_count; ++i) {
		int source = mode == GL_LINES ? i : rectangle_order[i];
		memcpy(&batch_vertices[batch_count * 2], &vertices[source * 2], 2 * sizeof(GLfloat));
		memcpy(&batch_colors[batch_count * 4], &colors[source * 4], 4 * sizeof(GLfloat));
		++batch_count;
	}
}

#ifdef ANDROID
int ogl_2d_batch_probe(void)
{
	unsigned char pixels[2][8 * 8 * 4];
	GLint blend_src_rgb, blend_dst_rgb, blend_src_alpha, blend_dst_alpha;
	int scenario, pass, i, vertex, passed = 1;

	glGetIntegerv(GL_BLEND_SRC_RGB, &blend_src_rgb);
	glGetIntegerv(GL_BLEND_DST_RGB, &blend_dst_rgb);
	glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend_src_alpha);
	glGetIntegerv(GL_BLEND_DST_ALPHA, &blend_dst_alpha);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	for (scenario = 0; scenario < 4; ++scenario) {
		for (pass = 0; pass < 2; ++pass) {
			glClear(GL_COLOR_BUFFER_BIT);
			if (pass)
				gr_begin_2d_batch();
			/* Exceed the bounded buffer and exercise empty and mixed-mode batches */
			for (i = 0; i < (scenario ? 800 : 0); ++i) {
				GLfloat y = -0.875f + (i % 8) * 0.25f;
				GLfloat vertices[8] = { -0.875f, y, 0.875f, y, 0.875f, y + 0.25f, -0.875f, y + 0.25f };
				GLfloat colors[16];
				GLenum mode = scenario == 1 || (scenario == 3 && i % 2) ? GL_LINES : GL_TRIANGLE_FAN;
				for (vertex = 0; vertex < 4; ++vertex) {
					colors[vertex * 4] = (i % 3) * 0.25f;
					colors[vertex * 4 + 1] = 0.25f + vertex * 0.125f;
					colors[vertex * 4 + 2] = 0.75f;
					colors[vertex * 4 + 3] = 0.5f;
				}
				glEnableClientState(GL_VERTEX_ARRAY);
				glEnableClientState(GL_COLOR_ARRAY);
				ogl_2d_batch_draw(mode, mode == GL_LINES ? 2 : 4, vertices, colors);
				glDisableClientState(GL_VERTEX_ARRAY);
				glDisableClientState(GL_COLOR_ARRAY);
			}
			if (pass)
				gr_end_2d_batch();
			glReadPixels(0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, pixels[pass]);
		}
		if (memcmp(pixels[0], pixels[1], sizeof(pixels[0])) != 0)
			passed = 0;
		if (scenario && pixels[1][(4 * 8 + 4) * 4 + 2] == 0)
			passed = 0;
	}
	glBlendFuncSeparate(blend_src_rgb, blend_dst_rgb, blend_src_alpha, blend_dst_alpha);
	glDisable(GL_BLEND);
	return passed && glGetError() == GL_NO_ERROR;
}
#endif
