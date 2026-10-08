#ifndef DXX_OGL_BATCH_IMPL_H
#define DXX_OGL_BATCH_IMPL_H

/* Compile through each native ogl.c so binding macros retain their game-local caches */
#define OGL_LINE_BATCH_LINES     512
#define OGL_BITMAP_BATCH_BITMAPS 256

static GLfloat line_batch_vertices[OGL_LINE_BATCH_LINES * 6];
static GLfloat line_batch_colors[OGL_LINE_BATCH_LINES * 8];
static int line_batch_count = -1;

static void ogl_line_batch_flush(void)
{
	if (line_batch_count > 0) {
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		OGL_DISABLE(TEXTURE_2D);
		glVertexPointer(3, GL_FLOAT, 0, line_batch_vertices);
		glColorPointer(4, GL_FLOAT, 0, line_batch_colors);
		glDrawArrays(GL_LINES, 0, line_batch_count * 2);
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
	}
	line_batch_count = 0;
}

void g3_start_line_batch(int max_lines)
{
	/* Retain the native size-hint ABI; storage no longer depends on it */
	(void) max_lines;
	line_batch_count = 0;
}

void g3_end_line_batch(void)
{
	ogl_line_batch_flush();
	line_batch_count = -1;
}

static int ogl_line_batch_append(const GLfloat *vertices, const GLfloat *colors)
{
	if (line_batch_count < 0)
		return 0;
	if (line_batch_count == OGL_LINE_BATCH_LINES)
		ogl_line_batch_flush();
	memcpy(&line_batch_vertices[line_batch_count * 6], vertices, 6 * sizeof(GLfloat));
	memcpy(&line_batch_colors[line_batch_count * 8], colors, 8 * sizeof(GLfloat));
	line_batch_count++;
	return 1;
}

static GLfloat ubitmap_batch_vertices[OGL_BITMAP_BATCH_BITMAPS * 12];
static GLfloat ubitmap_batch_colors[OGL_BITMAP_BATCH_BITMAPS * 24];
static GLfloat ubitmap_batch_texcoords[OGL_BITMAP_BATCH_BITMAPS * 12];
static int ubitmap_batch_count = -1;
static GLuint ubitmap_batch_texture;

static void ogl_ubitmap_batch_flush(void)
{
	if (ubitmap_batch_count > 0) {
		OGL_ENABLE(TEXTURE_2D);
		OGL_BINDTEXTURE(ubitmap_batch_texture);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glVertexPointer(2, GL_FLOAT, 0, ubitmap_batch_vertices);
		glColorPointer(4, GL_FLOAT, 0, ubitmap_batch_colors);
		glTexCoordPointer(2, GL_FLOAT, 0, ubitmap_batch_texcoords);
		glDrawArrays(GL_TRIANGLES, 0, ubitmap_batch_count * 6);
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	ubitmap_batch_count = 0;
	ubitmap_batch_texture = 0;
}

void ogl_ubitmap_batch_begin(int max_bitmaps)
{
	(void) max_bitmaps;
	ubitmap_batch_count = 0;
	ubitmap_batch_texture = 0;
}

void ogl_ubitmap_batch_end(void)
{
	ogl_ubitmap_batch_flush();
	ubitmap_batch_count = -1;
}

static int ogl_ubitmap_batch_append(GLuint texture, const GLfloat *vertices,
                                    const GLfloat *colors, const GLfloat *texcoords)
{
	static const int order[6] = { 0, 1, 2, 0, 2, 3 };
	if (ubitmap_batch_count < 0)
		return 0;
	if (ubitmap_batch_count == OGL_BITMAP_BATCH_BITMAPS || (ubitmap_batch_count && ubitmap_batch_texture != texture))
		ogl_ubitmap_batch_flush();
	ubitmap_batch_texture = texture;
	for (int i = 0; i < 6; ++i) {
		memcpy(&ubitmap_batch_vertices[ubitmap_batch_count * 12 + i * 2], &vertices[order[i] * 2], 2 * sizeof(GLfloat));
		memcpy(&ubitmap_batch_colors[ubitmap_batch_count * 24 + i * 4], &colors[order[i] * 4], 4 * sizeof(GLfloat));
		memcpy(&ubitmap_batch_texcoords[ubitmap_batch_count * 12 + i * 2], &texcoords[order[i] * 2], 2 * sizeof(GLfloat));
	}
	ubitmap_batch_count++;
	return 1;
}

#endif
