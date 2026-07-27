/* gles3_shim.h -- GLES 1.x fixed-function compatibility shim for GLES 3.0
 *
 * Android-only. Redirects removed GLES 1.x calls (matrix stack, client-state
 * vertex arrays, fixed-function state) to a thin shader-based emulation layer.
 *
 * Include this AFTER <GLES3/gl3.h> and BEFORE any code that uses the old calls.
 * The shim uses attribute locations 0-2 (position, color, texcoord) matching
 * the existing OGL_APOS/OGL_ACOLOR/OGL_ATEXCOORD convention in oglprog.h.
 */

#ifndef GLES3_SHIM_H
#define GLES3_SHIM_H

#include <GLES3/gl3.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- lifecycle ---------- */
void gles3_shim_init(void);
void gles3_shim_shutdown(void);

/* ---------- matrix stack ---------- */
void gles3_shim_matrix_mode(GLenum mode);
void gles3_shim_load_identity(void);
void gles3_shim_push_matrix(void);
void gles3_shim_pop_matrix(void);
void gles3_shim_translatef(GLfloat x, GLfloat y, GLfloat z);
void gles3_shim_scalef(GLfloat x, GLfloat y, GLfloat z);
void gles3_shim_rotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void gles3_shim_orthof(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f);
void gles3_shim_frustumf(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f);
void gles3_shim_load_matrixf(const GLfloat *m);
void gles3_shim_mult_matrixf(const GLfloat *m);

/* ---------- client-state vertex arrays ---------- */
void gles3_shim_enable_client_state(GLenum cap);
void gles3_shim_disable_client_state(GLenum cap);
void gles3_shim_vertex_pointer(GLint size, GLenum type, GLsizei stride, const void *ptr);
void gles3_shim_color_pointer(GLint size, GLenum type, GLsizei stride, const void *ptr);
void gles3_shim_texcoord_pointer(GLint size, GLenum type, GLsizei stride, const void *ptr);

/* ---------- fixed-function state ---------- */
void gles3_shim_color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void gles3_shim_alpha_func(GLenum func, GLfloat ref);
void gles3_shim_shade_model(GLenum mode);
void gles3_shim_tex_envi(GLenum target, GLenum pname, GLint param);

/* Pre-draw: uploads MVP + state uniforms if dirty. Called automatically by
 * the glDrawArrays redirect but can also be called explicitly. */
void gles3_shim_flush_state(void);

/* Draw wrapper -- flushes shim state then calls the real glDrawArrays.
 * Must use a different name because the .c file needs the real call. */
void gles3_shim_draw_arrays(GLenum mode, GLint first, GLsizei count);

/* Override: when an external shader program is active (e.g. OGL_MERGE
 * multi-texture shader), the shim's own program must be suspended.
 * gles3_shim_use_external(prog) saves current state; passing 0 restores. */
void gles3_shim_use_external(GLuint prog);

/* Avoid redundant glUseProgram calls on the Android hot path. */
void gles3_shim_bind_program(GLuint prog);

/* Expose the computed MVP for external programs (e.g. OGL_MERGE) */
const float *gles3_shim_get_mvp(void);

/* External program helpers for Android OGL_MERGE draws that want to reuse the
 * shim's streaming VBO / VAO path instead of maintaining a second upload path. */
void gles3_shim_external_texcoord2_pointer(GLint size, GLenum type, GLsizei stride, const void *ptr);
GLuint gles3_shim_get_stream_vbo(void);
int gles3_shim_probe_vbo_arrays(void);

/* ---------- redirect macros ---------- */

/* Matrix stack */
#define glMatrixMode   gles3_shim_matrix_mode
#define glLoadIdentity gles3_shim_load_identity
#define glPushMatrix   gles3_shim_push_matrix
#define glPopMatrix    gles3_shim_pop_matrix
#define glTranslatef   gles3_shim_translatef
#define glScalef       gles3_shim_scalef
#define glRotatef      gles3_shim_rotatef
#define glOrthof       gles3_shim_orthof
#define glFrustumf     gles3_shim_frustumf
#define glLoadMatrixf  gles3_shim_load_matrixf
#define glMultMatrixf  gles3_shim_mult_matrixf

/* Client-state vertex arrays */
#define glEnableClientState  gles3_shim_enable_client_state
#define glDisableClientState gles3_shim_disable_client_state
#define glVertexPointer      gles3_shim_vertex_pointer
#define glColorPointer       gles3_shim_color_pointer
#define glTexCoordPointer    gles3_shim_texcoord_pointer

/* Draw call wrapper */
#define glDrawArrays gles3_shim_draw_arrays

/* Fixed-function state */
#define glColor4f    gles3_shim_color4f
#define glAlphaFunc  gles3_shim_alpha_func
#define glShadeModel gles3_shim_shade_model
#define glTexEnvi    gles3_shim_tex_envi

/* GL_TEXTURE_2D enable/disable goes through shim for tracking.
 * GL_ALPHA_TEST is also intercepted (doesn't exist in GLES 3).
 * We redefine glEnable/glDisable to go through wrapper functions that
 * intercept these specific caps and pass through everything else. */
void gles3_shim_glEnable(GLenum cap);
void gles3_shim_glDisable(GLenum cap);
#define glEnable  gles3_shim_glEnable
#define glDisable gles3_shim_glDisable

/* Hint: GL_PERSPECTIVE_CORRECTION_HINT doesn't exist in GLES 3 -- no-op */
#define glHint(target, mode) ((void) 0)

/* glPointSize doesn't exist in GLES 3 core -- point size is set via
 * gl_PointSize in the vertex shader. No-op for now (always 1.0). */
#define glPointSize(size) ((void) 0)

/* GL_GENERATE_MIPMAP texture parameter doesn't exist in GLES 3.
 * We no-op it; mipmap generation uses glGenerateMipmap() instead. */
/* This is handled in the ogl_loadtexture ifdef block, not a global redefine */

/* Constants that don't exist in GLES3/gl3.h but are used by the code */
#ifndef GL_MODELVIEW
#define GL_MODELVIEW  0x1700
#define GL_PROJECTION 0x1701
#endif

#ifndef GL_VERTEX_ARRAY
#define GL_VERTEX_ARRAY        0x8074
#define GL_COLOR_ARRAY         0x8076
#define GL_TEXTURE_COORD_ARRAY 0x8078
#endif

#ifndef GL_ALPHA_TEST
#define GL_ALPHA_TEST 0x0BC0
#endif

#ifndef GL_MODULATE
#define GL_MODULATE 0x2100
#endif

#ifndef GL_TEXTURE_ENV
#define GL_TEXTURE_ENV      0x2300
#define GL_TEXTURE_ENV_MODE 0x2200
#endif

#ifndef GL_PERSPECTIVE_CORRECTION_HINT
#define GL_PERSPECTIVE_CORRECTION_HINT 0x0C50
#endif

#ifndef GL_SMOOTH
#define GL_SMOOTH 0x1D01
#endif

#ifndef GL_GENERATE_MIPMAP
#define GL_GENERATE_MIPMAP 0x8191
#endif

#ifndef GL_INTENSITY4
#define GL_INTENSITY4 GL_LUMINANCE
#endif
#ifndef GL_INTENSITY8
#define GL_INTENSITY8 GL_LUMINANCE
#endif
#ifndef GL_LUMINANCE4_ALPHA4
#define GL_LUMINANCE4_ALPHA4 GL_LUMINANCE_ALPHA
#endif
#ifndef GL_RGBA2
#define GL_RGBA2 GL_RGBA4
#endif
#ifndef GL_COLOR_INDEX
#define GL_COLOR_INDEX GL_LUMINANCE
#endif

#ifdef __cplusplus
}
#endif

#endif /* GLES3_SHIM_H */
