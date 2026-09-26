#ifndef OGL_2D_BATCH_H
#define OGL_2D_BATCH_H

/* Only untextured lines and four-vertex rectangle fans are accepted */
void ogl_2d_batch_draw(GLenum mode, int count, const GLfloat *vertices,
                       const GLfloat *colors);

#ifdef ANDROID
/* Runs in the GLES probe's isolated 8x8 framebuffer with identity matrices */
int ogl_2d_batch_probe(void);
#endif

#endif
