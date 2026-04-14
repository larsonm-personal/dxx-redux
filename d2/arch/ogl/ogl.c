/*
 *
 * Graphics support functions for OpenGL.
 *
 */

//#include <stdio.h>
#ifdef _WIN32
 #include <winsock2.h>
#include <windows.h>
#include <stddef.h>
#endif
#ifndef OGLES
#include <GL/glew.h>
#endif
#if defined(__APPLE__) && defined(__MACH__)
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#ifdef OGLES
#ifdef ANDROID
#include <GLES3/gl3.h>
#else
#include <GLES/gl.h>
#endif
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif
#endif
#include <string.h>
#include <math.h>
#include <stdio.h>

#ifdef ANDROID
#include <android/log.h>
#include <time.h>
#include "debug_tex_overlay.h"
#include "android_crash_handler.h"
#include "android_log.h"
#endif

#include "3d.h"
#include "piggy.h"
#include "../../3d/globvars.h"
#include "../../3d/clipper.h"
#include "dxxerror.h"
#include "texmap.h"
#include "palette.h"
#include "rle.h"
#include "console.h"
#include "u_mem.h"
#ifdef HAVE_LIBPNG
#include "pngfile.h"
#endif

#include "segment.h"
#include "textures.h"
#include "texmerge.h"
#include "effects.h"
#include "weapon.h"
#include "powerup.h"
#include "laser.h"
#include "player.h"
#include "polyobj.h"
#include "gamefont.h"
#include "byteswap.h"
#include "internal.h"
#include "gauges.h"
#include "playsave.h"
#include "args.h"
#include "timer.h"
#include "strutil.h"
#include "xmodel.h"
#include "oglprog.h"

//change to 1 for lots of spew.
#if 0
#define glmprintf(0,a) con_printf(CON_DEBUG, a)
#else
#define glmprintf(a)
#endif

#ifndef M_PI
#define M_PI 3.14159
#endif

#if defined(_WIN32) || (defined(__APPLE__) && defined(__MACH__)) || defined(__sun__) || defined(macintosh)
#define cosf(a) cos(a)
#define sinf(a) sin(a)
#endif

#define MAX_VERTS 128

unsigned char *ogl_pal=gr_palette;

int last_width=-1,last_height=-1;
#ifdef ANDROID
int last_kb_off=0;
#endif
int GL_TEXTURE_2D_enabled=-1;
int GL_texclamp_enabled=-1;
GLfloat ogl_maxanisotropy = 0;
int ogl_aniso_level = 0; /* 0=off, 2/4/8/16 for anisotropic filtering */
volatile int g_aniso_pending_apply = 0; /* set from JNI, consumed by GL thread */
volatile int g_texfilt_pending_apply = 0; /* set from JNI, consumed by GL thread */
int g_texfilt_level = 0; /* desired TexFilt value from overlay */
int ogl_max_texture_size = 1024;
#ifdef ANDROID
/* android port: MSAA via FBO -- runtime-toggleable anti-aliasing */
int ogl_msaa_samples = 0;       /* desired sample count: 0/2/4 */
int ogl_msaa_max_samples = 0;   /* queried from GL_MAX_SAMPLES */
volatile int g_msaa_pending_apply = 0;
static GLuint ogl_msaa_fbo = 0, ogl_msaa_color_rbo = 0, ogl_msaa_depth_rbo = 0;
static int ogl_msaa_w = 0, ogl_msaa_h = 0;
int g_msaa_fbo_bound = 0;       /* 1 while rendering to MSAA FBO */
static int g_msaa_frame_depth = 0; /* nesting depth: >0 = sub-window render */
/* android port: GPU timer via EXT_disjoint_timer_query */
#define GL_TIME_ELAPSED_EXT  0x88BF
#define GL_GPU_DISJOINT_EXT  0x8FBB
int ogl_gpu_timer_available = 0;
int g_gpu_time_us = 0;          /* last completed GPU frame time in microseconds */
/* Triple-buffered queries: write_idx advances each frame, read oldest completed */
#define GPU_QUERY_COUNT 3
static GLuint ogl_gpu_queries[GPU_QUERY_COUNT] = {0, 0, 0};
static int ogl_gpu_query_write = 0;  /* next slot to begin a query in */
static int ogl_gpu_query_count = 0;  /* number of in-flight + completed queries */
static int ogl_gpu_query_in_flight = 0; /* 1 while a query is between begin/end */
int ogl_color_depth = 16; /* actual framebuffer color depth: 16=RGB565, 32=RGBA8888 */
#endif
#ifdef ANDROID
volatile int g_fb_sample_r = -1, g_fb_sample_g = -1, g_fb_sample_b = -1, g_fb_sample_a = -1;
volatile int g_fb_avg_r = -1, g_fb_avg_g = -1, g_fb_avg_b = -1, g_fb_avg_a = -1;
/* android port: manual override to disable ETC2 texture loading */
int ogl_etc2_broken = 0;
#endif

int r_texcount = 0, r_cachedtexcount = 0;
#ifdef OGLES
int ogl_rgba_internalformat = GL_RGBA;
int ogl_rgb_internalformat = GL_RGB;
#else
int ogl_rgba_internalformat = GL_RGBA8;
int ogl_rgb_internalformat = GL_RGB8;
#endif
GLfloat *sphere_va = NULL, *circle_va = NULL, *disk_va = NULL;
GLfloat *secondary_lva[3]={NULL, NULL, NULL};
int r_polyc,r_tpolyc,r_bitmapc,r_ubitbltc,r_upixelc;
extern int linedotscale;
#define f2glf(x) (f2fl(x))

#if defined(ANDROID) && defined(OGL_MERGE)
static unsigned int metl154_source_log_mask = 0;
static GLuint metl154_first_gl_handle = 0;
#define METL154_VSLICE_SAMPLES 5
#define METL154_LOG_PT_COUNT 4
#define METL154_TRACKED_FACE_MAX 32

struct metl154_tracked_face {
	int metl_pass;
	int metl_seq;
	int draw_order;
	int nv;
	int cover_logged;
	fix pts[METL154_LOG_PT_COUNT][3];
};

static struct metl154_tracked_face metl154_tracked_faces[METL154_TRACKED_FACE_MAX];
static int metl154_tracked_face_count = 0;
static int metl154_tracked_frame_id = -1;
static int metl154_draw_order = 0;

static void ogl_metl154_begin_frame_tracking(void)
{
	if (metl154_tracked_frame_id == g_metl154_frame_id)
		return;
	metl154_tracked_frame_id = g_metl154_frame_id;
	metl154_tracked_face_count = 0;
	metl154_draw_order = 0;
}

static int ogl_metl154_next_draw_order(void)
{
	ogl_metl154_begin_frame_tracking();
	return ++metl154_draw_order;
}

static void ogl_metl154_track_face(const g3s_point **pointlist, int nv, int draw_order)
{
	struct metl154_tracked_face *track;
	int i;

	if (nv <= 0 || nv > METL154_LOG_PT_COUNT
	    || metl154_tracked_face_count >= METL154_TRACKED_FACE_MAX)
		return;

	ogl_metl154_begin_frame_tracking();
	track = &metl154_tracked_faces[metl154_tracked_face_count++];
	track->metl_pass = g_metl154_render_pass;
	track->metl_seq = g_metl154_draw_seq;
	track->draw_order = draw_order;
	track->nv = nv;
	track->cover_logged = 0;
	for (i = 0; i < nv; i++) {
		track->pts[i][0] = pointlist[i]->p3_vec.x;
		track->pts[i][1] = pointlist[i]->p3_vec.y;
		track->pts[i][2] = pointlist[i]->p3_vec.z;
	}
}

static int ogl_metl154_face_matches(const struct metl154_tracked_face *track,
	const g3s_point **pointlist, int nv, int *ordered)
{
	int i, j;
	int used[METL154_LOG_PT_COUNT] = {0, 0, 0, 0};

	if (track->nv != nv || nv <= 0 || nv > METL154_LOG_PT_COUNT)
		return 0;

	*ordered = 1;
	for (i = 0; i < nv; i++) {
		if (track->pts[i][0] != pointlist[i]->p3_vec.x
		    || track->pts[i][1] != pointlist[i]->p3_vec.y
		    || track->pts[i][2] != pointlist[i]->p3_vec.z) {
			*ordered = 0;
			break;
		}
	}
	if (*ordered)
		return 1;

	for (i = 0; i < nv; i++) {
		for (j = 0; j < nv; j++) {
			if (!used[j]
			    && track->pts[j][0] == pointlist[i]->p3_vec.x
			    && track->pts[j][1] == pointlist[i]->p3_vec.y
			    && track->pts[j][2] == pointlist[i]->p3_vec.z) {
				used[j] = 1;
				break;
			}
		}
		if (j == nv)
			return 0;
	}

	return 1;
}

static void ogl_log_metl154_cover(const char *shader_kind, const char *botname,
	const char *ovlname, const g3s_point **pointlist, int nv, int draw_order)
{
	int i, ordered;

	ogl_metl154_begin_frame_tracking();
	for (i = 0; i < metl154_tracked_face_count; i++) {
		struct metl154_tracked_face *track = &metl154_tracked_faces[i];

		if (track->cover_logged || draw_order <= track->draw_order)
			continue;
		if (!ogl_metl154_face_matches(track, pointlist, nv, &ordered))
			continue;

		track->cover_logged = 1;
		debug_log(DLOG_TEXTURE,
			"[metl154cover] frame=%d metl_pass=%d metl_seq=%d metl_order=%d cover_order=%d cover_shader=%s cover_bot=%s cover_ovl=%s ordered=%d",
			g_metl154_frame_id,
			track->metl_pass,
			track->metl_seq,
			track->draw_order,
			draw_order,
			shader_kind ? shader_kind : "<none>",
			botname ? botname : "<none>",
			ovlname ? ovlname : "<none>",
			ordered);
		return;
	}
}

static int ogl_is_metl154_bitmap(grs_bitmap *bm)
{
	const char *name = bm ? piggy_game_bitmap_name(bm) : NULL;
	return name && !d_stricmp(name, "metl154");
}

#ifdef ANDROID
static const char *ogl_metl154_experiment_name(int mode)
{
	switch (mode) {
		case METL154_EXPERIMENT_KTX2_NOMIP:
			return "ktx2_nomip";
		case METL154_EXPERIMENT_RGBA:
			return "rgba";
		case METL154_EXPERIMENT_RGBA_NOMIP:
			return "rgba_nomip";
		case METL154_EXPERIMENT_STOCK:
			return "stock";
		default:
			return "default";
	}
}

static int ogl_metl154_disable_mips(int mode)
{
	return mode == METL154_EXPERIMENT_KTX2_NOMIP
		|| mode == METL154_EXPERIMENT_RGBA_NOMIP;
}

static int ogl_metl154_force_rgba(int mode)
{
	return mode == METL154_EXPERIMENT_RGBA
		|| mode == METL154_EXPERIMENT_RGBA_NOMIP;
}

static int ogl_metl154_force_stock(int mode)
{
	return mode == METL154_EXPERIMENT_STOCK;
}

static int ogl_reload_metl154_textures(void)
{
	int i, invalidated = 0;

	for (i = 0; i < Num_bitmap_files; i++) {
		grs_bitmap *bm = &GameBitmaps[i];

		if (!ogl_is_metl154_bitmap(bm))
			continue;
		if ((bm->gltexture && bm->gltexture->handle > 0)
			|| (bm->gltexture_mask && bm->gltexture_mask->handle > 0)) {
			ogl_freebmtexture(bm);
			invalidated++;
		}
	}
	return invalidated;
}
#endif

static grs_bitmap *ogl_get_metl154_source_bitmap(grs_bitmap *bm)
{
	grs_bitmap *src = bm;

	if (src->bm_flags & BM_FLAG_RLE)
		src = rle_expand_texture(src);
	if (!src || !src->bm_data || src->bm_w <= 0 || src->bm_h <= 0)
		return NULL;
	return src;
}

static void ogl_get_metl154_palette_counts(grs_bitmap *bm, int *idx254, int *idx255, int *real_flags)
{
	grs_bitmap *src = ogl_get_metl154_source_bitmap(bm);
	int total, i;

	*idx254 = 0;
	*idx255 = 0;
	*real_flags = piggy_bitmap_get_flags(bm);

	if (!src)
		return;

	total = src->bm_w * src->bm_h;
	for (i = 0; i < total; i++) {
		if (src->bm_data[i] == 254)
			(*idx254)++;
		else if (src->bm_data[i] == 255)
			(*idx255)++;
	}
}

static int ogl_get_metl154_alpha_class(unsigned char idx)
{
	if (idx == 255)
		return 0;
	if (idx == 254)
		return 2;
	return 1;
}

static GLfloat ogl_get_metl154_alpha_value(unsigned char idx)
{
	return idx == 255 ? 0.0f : 1.0f;
}

static void ogl_get_metl154_source_sample(grs_bitmap *bm, GLfloat *texcoordovl_array, int nv,
	GLfloat *avg_u, GLfloat *avg_v, int *sample_x, int *sample_y, int *sample_idx)
{
	grs_bitmap *src = ogl_get_metl154_source_bitmap(bm);
	GLfloat sum_u = 0.0f, sum_v = 0.0f;
	GLfloat wrapped_u, wrapped_v;
	int i;

	*avg_u = 0.0f;
	*avg_v = 0.0f;
	*sample_x = -1;
	*sample_y = -1;
	*sample_idx = -1;

	if (!src || nv <= 0)
		return;

	for (i = 0; i < nv; i++) {
		sum_u += texcoordovl_array[i * 2];
		sum_v += texcoordovl_array[i * 2 + 1];
	}
	*avg_u = sum_u / nv;
	*avg_v = sum_v / nv;
	wrapped_u = *avg_u - floorf(*avg_u);
	wrapped_v = *avg_v - floorf(*avg_v);
	if (wrapped_u < 0.0f)
		wrapped_u += 1.0f;
	if (wrapped_v < 0.0f)
		wrapped_v += 1.0f;

	*sample_x = (int)(wrapped_u * src->bm_w);
	*sample_y = (int)(wrapped_v * src->bm_h);
	if (*sample_x >= src->bm_w)
		*sample_x = src->bm_w - 1;
	if (*sample_y >= src->bm_h)
		*sample_y = src->bm_h - 1;
	*sample_idx = src->bm_data[*sample_y * src->bm_w + *sample_x];
}

static void ogl_get_metl154_source_filter_sample(grs_bitmap *bm, GLfloat sample_u,
	GLfloat sample_v, int *idx00, int *idx10, int *idx01, int *idx11,
	GLfloat *alpha, int *wrap_u, int *wrap_v)
{
	grs_bitmap *src = ogl_get_metl154_source_bitmap(bm);
	GLfloat wrapped_u, wrapped_v, tex_u, tex_v, frac_u, frac_v, alpha0, alpha1;
	int x0, x1, y0, y1;

	*idx00 = -1;
	*idx10 = -1;
	*idx01 = -1;
	*idx11 = -1;
	*alpha = -1.0f;
	*wrap_u = 0;
	*wrap_v = 0;

	if (!src || src->bm_w <= 0 || src->bm_h <= 0)
		return;

	wrapped_u = sample_u - floorf(sample_u);
	wrapped_v = sample_v - floorf(sample_v);
	if (wrapped_u < 0.0f)
		wrapped_u += 1.0f;
	if (wrapped_v < 0.0f)
		wrapped_v += 1.0f;

	tex_u = wrapped_u * src->bm_w - 0.5f;
	tex_v = wrapped_v * src->bm_h - 0.5f;
	x0 = (int)floorf(tex_u);
	y0 = (int)floorf(tex_v);
	frac_u = tex_u - floorf(tex_u);
	frac_v = tex_v - floorf(tex_v);
	x1 = x0 + 1;
	y1 = y0 + 1;

	*wrap_u = (x0 < 0 || x1 >= src->bm_w);
	*wrap_v = (y0 < 0 || y1 >= src->bm_h);

	x0 = ((x0 % src->bm_w) + src->bm_w) % src->bm_w;
	x1 = ((x1 % src->bm_w) + src->bm_w) % src->bm_w;
	y0 = ((y0 % src->bm_h) + src->bm_h) % src->bm_h;
	y1 = ((y1 % src->bm_h) + src->bm_h) % src->bm_h;

	*idx00 = src->bm_data[y0 * src->bm_w + x0];
	*idx10 = src->bm_data[y0 * src->bm_w + x1];
	*idx01 = src->bm_data[y1 * src->bm_w + x0];
	*idx11 = src->bm_data[y1 * src->bm_w + x1];

	alpha0 = ogl_get_metl154_alpha_value((unsigned char)*idx00) * (1.0f - frac_u)
		+ ogl_get_metl154_alpha_value((unsigned char)*idx10) * frac_u;
	alpha1 = ogl_get_metl154_alpha_value((unsigned char)*idx01) * (1.0f - frac_u)
		+ ogl_get_metl154_alpha_value((unsigned char)*idx11) * frac_u;
	*alpha = alpha0 * (1.0f - frac_v) + alpha1 * frac_v;
}

static void ogl_get_metl154_source_vslice(grs_bitmap *bm, GLfloat sample_u,
	GLfloat min_v, GLfloat max_v, int *sample_rows, int *sample_idxs, int nsamples)
{
	grs_bitmap *src = ogl_get_metl154_source_bitmap(bm);
	GLfloat wrapped_u, span_v;
	int sample_x, i;

	for (i = 0; i < nsamples; i++) {
		sample_rows[i] = -1;
		sample_idxs[i] = -1;
	}
	if (!src || nsamples <= 0)
		return;

	wrapped_u = sample_u - floorf(sample_u);
	if (wrapped_u < 0.0f)
		wrapped_u += 1.0f;
	sample_x = (int)(wrapped_u * src->bm_w);
	if (sample_x >= src->bm_w)
		sample_x = src->bm_w - 1;
	span_v = max_v - min_v;

	for (i = 0; i < nsamples; i++) {
		GLfloat sample_v = min_v + span_v * ((i + 0.5f) / nsamples);
		GLfloat wrapped_v = sample_v - floorf(sample_v);
		int sample_y;

		if (wrapped_v < 0.0f)
			wrapped_v += 1.0f;
		sample_y = (int)(wrapped_v * src->bm_h);
		if (sample_y >= src->bm_h)
			sample_y = src->bm_h - 1;
		sample_rows[i] = sample_y;
		sample_idxs[i] = src->bm_data[sample_y * src->bm_w + sample_x];
	}
}

static void ogl_get_metl154_filter_state(ogl_texture *tex, GLint *min_filter, GLint *mag_filter)
{
	GLint active_tex = GL_TEXTURE0, prev_bind = 0;

	*min_filter = -1;
	*mag_filter = -1;
	if (!tex || tex->handle <= 0)
		return;

	glGetIntegerv(GL_ACTIVE_TEXTURE, &active_tex);
	glActiveTexture(GL_TEXTURE1);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_bind);
	if ((GLuint)prev_bind != tex->handle)
		glBindTexture(GL_TEXTURE_2D, tex->handle);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
	if ((GLuint)prev_bind != tex->handle)
		glBindTexture(GL_TEXTURE_2D, (GLuint)prev_bind);
	glActiveTexture((GLenum)active_tex);
}



static void ogl_get_metl154_draw_state(ogl_texture *tex, GLint *active_prog,
	GLint *bound_tex0, GLint *bound_tex1, GLint *bound_tex2, GLint *mip1_w)
{
	GLint active_tex = GL_TEXTURE0, prev_bind1 = 0;

	*active_prog = -1;
	*bound_tex0 = -1;
	*bound_tex1 = -1;
	*bound_tex2 = -1;
	*mip1_w = -1;
	glGetIntegerv(GL_CURRENT_PROGRAM, active_prog);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &active_tex);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, bound_tex0);
	glActiveTexture(GL_TEXTURE1);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_bind1);
	*bound_tex1 = prev_bind1;
	glActiveTexture(GL_TEXTURE2);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, bound_tex2);
	if (tex && tex->handle > 0)
		*mip1_w = tex->has_mipmaps ? (tex->tw > 1 ? tex->tw / 2 : 1) : 0;
	glActiveTexture((GLenum)active_tex);
}

static void ogl_get_metl154_gl_state(GLint *depth_enabled, GLint *blend_enabled,
	GLint *cull_enabled, GLboolean *depth_writemask, GLint *depth_func,
	GLint *front_face, GLint *cull_mode, GLboolean color_mask[4], GLint *draw_fbo)
{
	*depth_enabled = glIsEnabled(GL_DEPTH_TEST);
	*blend_enabled = glIsEnabled(GL_BLEND);
	*cull_enabled = glIsEnabled(GL_CULL_FACE);
	glGetBooleanv(GL_DEPTH_WRITEMASK, depth_writemask);
	glGetIntegerv(GL_DEPTH_FUNC, depth_func);
	glGetIntegerv(GL_FRONT_FACE, front_face);
	glGetIntegerv(GL_CULL_FACE_MODE, cull_mode);
	glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, draw_fbo);
}

static GLfloat ogl_get_metl154_screen_area(const g3s_point **pointlist, int nv)
{
	double area = 0.0;
	int i;

	if (!pointlist || nv < 3)
		return 0.0f;

	for (i = 0; i < nv; i++) {
		int j = (i + 1) % nv;
		area += (double)f2fl(pointlist[i]->p3_sx) * (double)f2fl(pointlist[j]->p3_sy)
			- (double)f2fl(pointlist[j]->p3_sx) * (double)f2fl(pointlist[i]->p3_sy);
	}

	return (GLfloat)(area * 0.5);
}

static GLfloat ogl_get_metl154_triangle_area(const g3s_point *p0,
	const g3s_point *p1, const g3s_point *p2)
{
	double x0 = f2fl(p0->p3_sx), y0 = f2fl(p0->p3_sy);
	double x1 = f2fl(p1->p3_sx), y1 = f2fl(p1->p3_sy);
	double x2 = f2fl(p2->p3_sx), y2 = f2fl(p2->p3_sy);

	return (GLfloat)(((x0 * (y1 - y2)) + (x1 * (y2 - y0)) + (x2 * (y0 - y1))) * 0.5);
}

static void ogl_log_metl154_split(const g3s_point **pointlist, int nv)
{
	GLfloat sx[4], sy[4];
	GLfloat fan012, fan023, alt013, alt123;
	const char *pick = "same";
	int i, fan_flip, alt_flip, fan_flat, alt_flat;

	if (!pointlist || nv != 4)
		return;

	for (i = 0; i < 4; i++) {
		sx[i] = f2fl(pointlist[i]->p3_sx);
		sy[i] = f2fl(pointlist[i]->p3_sy);
	}

	fan012 = ogl_get_metl154_triangle_area(pointlist[0], pointlist[1], pointlist[2]);
	fan023 = ogl_get_metl154_triangle_area(pointlist[0], pointlist[2], pointlist[3]);
	alt013 = ogl_get_metl154_triangle_area(pointlist[0], pointlist[1], pointlist[3]);
	alt123 = ogl_get_metl154_triangle_area(pointlist[1], pointlist[2], pointlist[3]);
	fan_flip = (fan012 < 0.0f && fan023 > 0.0f) || (fan012 > 0.0f && fan023 < 0.0f);
	alt_flip = (alt013 < 0.0f && alt123 > 0.0f) || (alt013 > 0.0f && alt123 < 0.0f);
	fan_flat = fabsf(fan012) < 0.5f || fabsf(fan023) < 0.5f;
	alt_flat = fabsf(alt013) < 0.5f || fabsf(alt123) < 0.5f;
	if ((fan_flip || fan_flat) != (alt_flip || alt_flat))
		pick = (fan_flip || fan_flat) ? "alt" : "fan";

	debug_log(DLOG_TEXTURE,
		"[metl154split] frame=%d pass=%d seq=%d fan=%.1f/%.1f alt=%.1f/%.1f fan_flip=%d alt_flip=%d fan_flat=%d alt_flat=%d pick=%s sx=%.1f/%.1f/%.1f/%.1f sy=%.1f/%.1f/%.1f/%.1f",
		g_metl154_frame_id,
		g_metl154_render_pass,
		g_metl154_draw_seq,
		fan012,
		fan023,
		alt013,
		alt123,
		fan_flip,
		alt_flip,
		fan_flat,
		alt_flat,
		pick,
		sx[0], sx[1], sx[2], sx[3],
		sy[0], sy[1], sy[2], sy[3]);
}

static void ogl_get_metl154_uv_points(g3s_uvl *uvl_list, GLfloat *texcoordovl_array, int nv,
	GLfloat *raw_pts, GLfloat *ovl_pts, int *uv_bad)
{
	int i;

	*uv_bad = 0;
	for (i = 0; i < METL154_LOG_PT_COUNT * 2; i++) {
		raw_pts[i] = -99.0f;
		ovl_pts[i] = -99.0f;
	}
	for (i = 0; i < nv && i < METL154_LOG_PT_COUNT; i++) {
		raw_pts[i * 2] = f2glf(uvl_list[i].u);
		raw_pts[i * 2 + 1] = f2glf(uvl_list[i].v);
		ovl_pts[i * 2] = texcoordovl_array[i * 2];
		ovl_pts[i * 2 + 1] = texcoordovl_array[i * 2 + 1];
		if (!isfinite(raw_pts[i * 2]) || !isfinite(raw_pts[i * 2 + 1])
		    || !isfinite(ovl_pts[i * 2]) || !isfinite(ovl_pts[i * 2 + 1]))
			*uv_bad = 1;
	}
}

static int ogl_mark_metl154_source_log(const char *bitmapname, unsigned int bit)
{
	if (!bitmapname || d_stricmp(bitmapname, "metl154"))
		return 0;
	if (metl154_source_log_mask & bit)
		return 0;
	metl154_source_log_mask |= bit;
	return 1;
}

static void ogl_log_metl154_palette_source(const char *bitmapname, const unsigned char *data,
	int width, int height, int bm_flags, unsigned int bit, const char *source)
{
	int total, idx254, idx255, left255, right255, top255, bottom255;
	int seam_lr_alpha, seam_tb_alpha, i;

	if (!data || width <= 0 || height <= 0 || !ogl_mark_metl154_source_log(bitmapname, bit))
		return;

	total = width * height;
	idx254 = 0;
	idx255 = 0;
	left255 = 0;
	right255 = 0;
	top255 = 0;
	bottom255 = 0;
	seam_lr_alpha = 0;
	seam_tb_alpha = 0;
	for (i = 0; i < total; i++) {
		if (data[i] == 254)
			idx254++;
		else if (data[i] == 255)
			idx255++;
	}
	for (i = 0; i < height; i++) {
		unsigned char left = data[i * width];
		unsigned char right = data[i * width + width - 1];

		if (left == 255)
			left255++;
		if (right == 255)
			right255++;
		if (ogl_get_metl154_alpha_class(left) != ogl_get_metl154_alpha_class(right))
			seam_lr_alpha++;
	}
	for (i = 0; i < width; i++) {
		unsigned char top = data[i];
		unsigned char bottom = data[(height - 1) * width + i];

		if (top == 255)
			top255++;
		if (bottom == 255)
			bottom255++;
		if (ogl_get_metl154_alpha_class(top) != ogl_get_metl154_alpha_class(bottom))
			seam_tb_alpha++;
	}

	debug_log(DLOG_TEXTURE,
		"[metl154src] source=%s kind=palette flags=0x%x size=%dx%d idx254=%d idx255=%d opaque=%d edge255=%d/%d/%d/%d seam_alpha=%d/%d",
		source, bm_flags, width, height, idx254, idx255, total - idx254 - idx255,
		left255, right255, top255, bottom255, seam_lr_alpha, seam_tb_alpha);
}

static void ogl_log_metl154_alpha_source(const char *bitmapname, const unsigned char *data,
	int width, int height, int channels, int bm_flags, unsigned int bit, const char *source)
{
	int total, alpha0, alpha255, alpha_partial, i;

	if (!data || width <= 0 || height <= 0 || !ogl_mark_metl154_source_log(bitmapname, bit))
		return;

	if (channels < 4) {
		debug_log(DLOG_TEXTURE,
			"[metl154src] source=%s kind=rgb flags=0x%x size=%dx%d channels=%d",
			source, bm_flags, width, height, channels);
		return;
	}

	total = width * height;
	alpha0 = 0;
	alpha255 = 0;
	alpha_partial = 0;
	for (i = 0; i < total; i++) {
		unsigned char alpha = data[i * channels + 3];
		if (!alpha)
			alpha0++;
		else if (alpha == 255)
			alpha255++;
		else
			alpha_partial++;
	}

	debug_log(DLOG_TEXTURE,
		"[metl154src] source=%s kind=rgba flags=0x%x size=%dx%d alpha0=%d alpha255=%d alpha_partial=%d",
		source, bm_flags, width, height, alpha0, alpha255, alpha_partial);
}

static void ogl_log_metl154_diag(grs_bitmap *bmbot, grs_bitmap *bmovl,
	g3s_uvl *uvl_list, GLfloat *texcoordovl_array, int nv, int orient, int super,
	GLuint prog, int tex2_debug_mode)
{
	const char *botname;
	fix min_u, max_u, min_v, max_v;
	GLfloat min_ou, max_ou, min_ov, max_ov;
	GLfloat raw_pts[METL154_LOG_PT_COUNT * 2], ovl_pts[METL154_LOG_PT_COUNT * 2];
	GLfloat avg_ou, avg_ov;
	GLfloat sample_alpha;
	int src254, src255, real_flags;
	int sample_x, sample_y, sample_idx;
	int bilerp00, bilerp10, bilerp01, bilerp11, sample_wrap_u, sample_wrap_v;
	int uv_bad, handle_changed;
	int vslice_rows[METL154_VSLICE_SAMPLES], vslice_idxs[METL154_VSLICE_SAMPLES];
	GLint min_filter, mag_filter, active_prog, bound_tex0, bound_tex1, bound_tex2, mip1_w;
	GLuint tex0_expected, tex1_expected, tex2_expected, current_handle;
	int i;

	if (!ogl_is_metl154_bitmap(bmovl) || nv <= 0)
		return;

	min_u = max_u = uvl_list[0].u;
	min_v = max_v = uvl_list[0].v;
	min_ou = max_ou = texcoordovl_array[0];
	min_ov = max_ov = texcoordovl_array[1];
	for (i = 1; i < nv; i++) {
		if (uvl_list[i].u < min_u) min_u = uvl_list[i].u;
		if (uvl_list[i].u > max_u) max_u = uvl_list[i].u;
		if (uvl_list[i].v < min_v) min_v = uvl_list[i].v;
		if (uvl_list[i].v > max_v) max_v = uvl_list[i].v;
		if (texcoordovl_array[i * 2] < min_ou) min_ou = texcoordovl_array[i * 2];
		if (texcoordovl_array[i * 2] > max_ou) max_ou = texcoordovl_array[i * 2];
		if (texcoordovl_array[i * 2 + 1] < min_ov) min_ov = texcoordovl_array[i * 2 + 1];
		if (texcoordovl_array[i * 2 + 1] > max_ov) max_ov = texcoordovl_array[i * 2 + 1];
	}

	botname = piggy_game_bitmap_name(bmbot);
	ogl_get_metl154_palette_counts(bmovl, &src254, &src255, &real_flags);
	ogl_get_metl154_source_sample(bmovl, texcoordovl_array, nv, &avg_ou, &avg_ov,
		&sample_x, &sample_y, &sample_idx);
	ogl_get_metl154_source_filter_sample(bmovl, avg_ou, avg_ov,
		&bilerp00, &bilerp10, &bilerp01, &bilerp11,
		&sample_alpha, &sample_wrap_u, &sample_wrap_v);
	ogl_get_metl154_source_vslice(bmovl, avg_ou, min_ov, max_ov,
		vslice_rows, vslice_idxs, METL154_VSLICE_SAMPLES);
	ogl_get_metl154_uv_points(uvl_list, texcoordovl_array, nv, raw_pts, ovl_pts, &uv_bad);
	ogl_get_metl154_filter_state(bmovl->gltexture, &min_filter, &mag_filter);
	ogl_get_metl154_draw_state(bmovl->gltexture, &active_prog,
		&bound_tex0, &bound_tex1, &bound_tex2, &mip1_w);
	current_handle = bmovl->gltexture ? bmovl->gltexture->handle : 0;
	if (!metl154_first_gl_handle && current_handle)
		metl154_first_gl_handle = current_handle;
	handle_changed = current_handle && metl154_first_gl_handle
		&& current_handle != metl154_first_gl_handle;
	tex0_expected = bmbot->gltexture ? bmbot->gltexture->handle : 0;
	tex1_expected = bmovl->gltexture ? bmovl->gltexture->handle : 0;
	tex2_expected = bmovl->gltexture_mask ? bmovl->gltexture_mask->handle : 0;
	debug_log(DLOG_TEXTURE,
		"[metl154diag] frame=%d pass=%d seq=%d dbg=%d orient=%d shader=%s bot=%s raw_uv=%.3f..%.3f/%.3f..%.3f ovl_uv=%.3f..%.3f/%.3f..%.3f flags=0x%x real_flags=0x%x src254=%d src255=%d sample_uv=%.3f/%.3f sample_xy=%d/%d sample_idx=%d vslice_y=%d/%d/%d/%d/%d vslice_idx=%d/%d/%d/%d/%d filt=%d/%d mips=%d texfilt=%d aniso=%d mip1_est=%d tex_handle=%u first_handle=%u handle_changed=%d tex_bytes=%d tex_lw=%d ovl_png=%d ovl_wh=%dx%d tex_wh=%dx%d tex_p2=%dx%d tex_uv=%.3f/%.3f mask=%u",
		g_metl154_frame_id,
		g_metl154_render_pass,
		g_metl154_draw_seq,
		tex2_debug_mode,
		orient,
		super ? "mask" : "plain",
		botname ? botname : "<none>",
		f2fl(min_u), f2fl(max_u), f2fl(min_v), f2fl(max_v),
		min_ou, max_ou, min_ov, max_ov,
		bmovl->bm_flags,
		real_flags,
		src254,
		src255,
		avg_ou,
		avg_ov,
		sample_x,
		sample_y,
		sample_idx,
		vslice_rows[0],
		vslice_rows[1],
		vslice_rows[2],
		vslice_rows[3],
		vslice_rows[4],
		vslice_idxs[0],
		vslice_idxs[1],
		vslice_idxs[2],
		vslice_idxs[3],
		vslice_idxs[4],
		min_filter,
		mag_filter,
		bmovl->gltexture ? bmovl->gltexture->has_mipmaps : -1,
		GameCfg.TexFilt,
		ogl_aniso_level,
		mip1_w,
		current_handle,
		metl154_first_gl_handle,
		handle_changed,
		bmovl->gltexture ? bmovl->gltexture->bytes : 0,
		bmovl->gltexture ? bmovl->gltexture->lw : 0,
		bmovl->gltexture ? bmovl->gltexture->is_png : -1,
		bmovl->bm_w, bmovl->bm_h,
		bmovl->gltexture ? bmovl->gltexture->w : 0,
		bmovl->gltexture ? bmovl->gltexture->h : 0,
		bmovl->gltexture ? bmovl->gltexture->tw : 0,
		bmovl->gltexture ? bmovl->gltexture->th : 0,
		bmovl->gltexture ? bmovl->gltexture->u : 0.0f,
		bmovl->gltexture ? bmovl->gltexture->v : 0.0f,
		bmovl->gltexture_mask ? bmovl->gltexture_mask->handle : 0);
	debug_log(DLOG_TEXTURE,
		"[metl154gl] frame=%d pass=%d seq=%d prog=%u/%d tex0=%u/%d tex1=%u/%d tex2=%u/%d",
		g_metl154_frame_id,
		g_metl154_render_pass,
		g_metl154_draw_seq,
		prog,
		active_prog,
		tex0_expected,
		bound_tex0,
		tex1_expected,
		bound_tex1,
		tex2_expected,
		bound_tex2);
	debug_log(DLOG_TEXTURE,
		"[metl154uv] frame=%d pass=%d seq=%d nv=%d uv_bad=%d raw0=%.3f/%.3f raw1=%.3f/%.3f raw2=%.3f/%.3f raw3=%.3f/%.3f ovl0=%.3f/%.3f ovl1=%.3f/%.3f ovl2=%.3f/%.3f ovl3=%.3f/%.3f",
		g_metl154_frame_id,
		g_metl154_render_pass,
		g_metl154_draw_seq,
		nv,
		uv_bad,
		raw_pts[0], raw_pts[1],
		raw_pts[2], raw_pts[3],
		raw_pts[4], raw_pts[5],
		raw_pts[6], raw_pts[7],
		ovl_pts[0], ovl_pts[1],
		ovl_pts[2], ovl_pts[3],
		ovl_pts[4], ovl_pts[5],
		ovl_pts[6], ovl_pts[7]);
	debug_log(DLOG_TEXTURE,
		"[metl154alpha] frame=%d pass=%d seq=%d avg_uv=%.3f/%.3f bilerp=%d/%d/%d/%d alpha=%.3f wrap=%d/%d",
		g_metl154_frame_id,
		g_metl154_render_pass,
		g_metl154_draw_seq,
		avg_ou,
		avg_ov,
		bilerp00,
		bilerp10,
		bilerp01,
		bilerp11,
		sample_alpha,
		sample_wrap_u,
		sample_wrap_v);
}

static void ogl_log_metl154_state(GLfloat screen_area,
	GLint depth_enabled, GLint blend_enabled, GLint cull_enabled,
	GLboolean depth_writemask, GLint depth_func, GLint front_face,
	GLint cull_mode, const GLboolean color_mask[4], GLint draw_fbo,
	int force_cull_off, int force_depth_off)
{
	debug_log(DLOG_TEXTURE,
		"[metl154state] frame=%d pass=%d seq=%d depth=%d depthmask=%d depthfunc=0x%x blend=%d cull=%d front=0x%x cullmode=0x%x colormask=%d%d%d%d fbo=%d area=%.1f force_cull_off=%d force_depth_off=%d",
		g_metl154_frame_id,
		g_metl154_render_pass,
		g_metl154_draw_seq,
		depth_enabled,
		depth_writemask ? 1 : 0,
		depth_func,
		blend_enabled,
		cull_enabled,
		front_face,
		cull_mode,
		color_mask[0] ? 1 : 0,
		color_mask[1] ? 1 : 0,
		color_mask[2] ? 1 : 0,
		color_mask[3] ? 1 : 0,
		draw_fbo,
		screen_area,
		force_cull_off,
		force_depth_off);
}
#endif

#ifdef ANDROID
/* Debug texture overlay globals -- android port only */
struct debug_tex_label g_debug_tex_labels[DEBUG_TEX_MAX_LABELS];
int g_debug_tex_label_count = 0;
volatile int g_debug_tex_overlay_active = 0;
volatile int g_metl154_debug_mode = 0;
volatile int g_metl154_experiment_mode = 0;
volatile int g_metl154_experiment_pending_apply = 0;
volatile int g_metl154_render_pass = 0;
volatile int g_metl154_frame_id = 0;
volatile int g_metl154_draw_seq = 0;
/* Direct RGB font color: bypasses palette round-trip for bright overlay text */
float g_font_rgb_override[3] = {-1.f, -1.f, -1.f}; /* negative = use palette */
/* Render context for selective texture filtering.
 * 0 = MENU (menus/briefings, default), 1 = 3D world, 2 = HUD */
int g_ogl_render_context = 0;
#endif

#ifdef ANDROID
/* android port: per-frame texture bind counter + bind cache */
int r_texbinds = 0;
int r_texbind_reuse = 0;
int r_shader_switches = 0;
int r_mask_draws = 0;
static GLuint ogl_last_bound_tex = 0;
#define OGL_BINDTEXTURE(a) do { \
	if ((GLuint)(a) != ogl_last_bound_tex) { \
		glBindTexture(GL_TEXTURE_2D, a); \
		ogl_last_bound_tex = (a); \
		r_texbinds++; \
	} else { r_texbind_reuse++; } \
} while(0)
#else
#define OGL_BINDTEXTURE(a) glBindTexture(GL_TEXTURE_2D, a);
#endif


ogl_texture ogl_texture_list[OGL_TEXTURE_LIST_SIZE];
int ogl_texture_list_cur;

static inline float minf(float x, float y) { return x < y ? x : y; }

/* some function prototypes */

#define GL_TEXTURE0_ARB 0x84C0
extern GLubyte *pixels;
extern GLubyte *texbuf;
void ogl_filltexbuf(unsigned char *data, GLubyte *texp, int truewidth, int width, int height, int dxo, int dyo, int twidth, int theight, int type, int bm_flags, int data_format);
void ogl_loadbmtexture(grs_bitmap *bm);
int ogl_loadtexture(unsigned char *data, int dxo, int dyo, ogl_texture *tex, int bm_flags, int data_format, int texfilt);
void ogl_freetexture(ogl_texture *gltexture);
void ogl_freebmtexture(grs_bitmap *bm);

#ifdef OGLES
// Replacement for gluPerspective
void perspective(double fovy, double aspect, double zNear, double zFar)
{
	double xmin, xmax, ymin, ymax;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	ymax = zNear * tan(fovy * M_PI / 360.0);
	ymin = -ymax;
	xmin = ymin * aspect;
	xmax = ymax * aspect;

	glFrustumf(xmin, xmax, ymin, ymax, zNear, zFar);
	glMatrixMode(GL_MODELVIEW);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);	
	glDepthMask(GL_TRUE);
}
#endif

void ogl_init_texture_stats(ogl_texture* t){
	t->prio=0.3;//default prio
	t->numrend=0;
}

void ogl_init_texture(ogl_texture* t, int w, int h, int flags)
{
	t->handle = 0;
#ifndef OGLES
	if (flags & OGL_FLAG_NOCOLOR)
	{
		// use GL_INTENSITY instead of GL_RGB
		if (flags & OGL_FLAG_ALPHA)
		{
			if (GameArg.DbgGlIntensity4Ok)
			{
				t->internalformat = GL_INTENSITY4;
				t->format = GL_LUMINANCE;
			}
			else if (GameArg.DbgGlLuminance4Alpha4Ok)
			{
				t->internalformat = GL_LUMINANCE4_ALPHA4;
				t->format = GL_LUMINANCE_ALPHA;
			}
			else if (GameArg.DbgGlRGBA2Ok)
			{
				t->internalformat = GL_RGBA2;
				t->format = GL_RGBA;
			}
			else
			{
				t->internalformat = ogl_rgba_internalformat;
				t->format = GL_RGBA;
			}
		}
		else
		{
			// there are certainly smaller formats we could use here, but nothing needs it ATM.
			t->internalformat = ogl_rgb_internalformat;
			t->format = GL_RGB;
		}
	}
	else
	{
#endif
		if (flags & OGL_FLAG_ALPHA)
		{
			t->internalformat = ogl_rgba_internalformat;
			t->format = GL_RGBA;
		}
		else
		{
			t->internalformat = ogl_rgb_internalformat;
			t->format = GL_RGB;
		}
#ifndef OGLES
	}
#endif
	t->wrapstate = -1;
	t->lw = t->w = w;
	t->h = h;
	ogl_init_texture_stats(t);
	t->is_png = 0;
	t->flags = flags;
}

void ogl_reset_texture(ogl_texture* t)
{
	ogl_init_texture(t, 0, 0, 0);
}

#ifdef ANDROID
/* Android port: track hires replacement coverage for introspection */
int r_hires_found = 0; /* PNG files found on disk */
int r_hires_loaded = 0; /* PNG textures successfully uploaded */
int r_etc2_zero_data = 0; /* ETC2 textures with all-zero compressed payload */
static int r_etc2_render_log_count = 0; /* limit per-frame render logging */
int g_cache_time_ms = 0; /* time spent in ogl_cache_level_textures */
#endif

void ogl_reset_texture_stats_internal(void){
	int i;
	for (i=0;i<OGL_TEXTURE_LIST_SIZE;i++)
		if (ogl_texture_list[i].handle>0){
			ogl_init_texture_stats(&ogl_texture_list[i]);
		}
#ifdef ANDROID
	r_hires_found = 0;
	r_hires_loaded = 0;
	r_etc2_zero_data = 0;
	r_etc2_render_log_count = 0;
#endif
}

void ogl_init_texture_list_internal(void){
	int i;
	ogl_texture_list_cur=0;
	for (i=0;i<OGL_TEXTURE_LIST_SIZE;i++)
		ogl_reset_texture(&ogl_texture_list[i]);
}

void ogl_smash_texture_list_internal(void){
	int i;
	if (sphere_va != NULL)
	{
		d_free(sphere_va);
		sphere_va = NULL;
	}
	if (circle_va != NULL)
	{
		d_free(circle_va);
		circle_va = NULL;
	}
	if (disk_va != NULL)
	{
		d_free(disk_va);
		disk_va = NULL;
	}
	for(i = 0; i < 3; i++) {
		if (secondary_lva[i] != NULL)
		{
			d_free(secondary_lva[i]);
			secondary_lva[i] = NULL;
		}
	}
	for (i=0;i<OGL_TEXTURE_LIST_SIZE;i++){
		if (ogl_texture_list[i].handle>0){
			glDeleteTextures( 1, &ogl_texture_list[i].handle );
			ogl_texture_list[i].handle=0;
		}
		ogl_texture_list[i].wrapstate = -1;
		ogl_texture_list[i].is_png = 0;
	}

	xmodel_free_gl_all();

#ifdef OGL_MERGE
	ogl_done_prog();
#endif
}

int ogl_allow_png(void){
	return !(Game_mode & GM_MULTI) || Netgame.AllowCustomModelsTextures;
}

void ogl_smash_png_textures(void){
	int i;
	for (i=0;i<OGL_TEXTURE_LIST_SIZE;i++){
		if (ogl_texture_list[i].handle>0 && ogl_texture_list[i].is_png){
			glDeleteTextures( 1, &ogl_texture_list[i].handle );
			ogl_texture_list[i].handle=0;
		}
	}
}

ogl_texture* ogl_get_free_texture(void){
	int i;
	for (i=0;i<OGL_TEXTURE_LIST_SIZE;i++){
		if (ogl_texture_list[ogl_texture_list_cur].handle<=0 && ogl_texture_list[ogl_texture_list_cur].w==0)
			return &ogl_texture_list[ogl_texture_list_cur];
		if (++ogl_texture_list_cur>=OGL_TEXTURE_LIST_SIZE)
			ogl_texture_list_cur=0;
	}
	Error("OGL: texture list full!\n");
}

void ogl_texture_stats(void)
{
	int used = 0, usedother = 0, usedidx = 0, usedrgb = 0, usedrgba = 0;
	int databytes = 0, truebytes = 0, datatexel = 0, truetexel = 0, i;
	int prio0=0,prio1=0,prio2=0,prio3=0,prioh=0;
	GLint idx, r, g, b, a, dbl, depth;
	int res, colorsize, depthsize;
	ogl_texture* t;

	for (i=0;i<OGL_TEXTURE_LIST_SIZE;i++){
		t=&ogl_texture_list[i];
		if (t->handle>0){
			used++;
			datatexel+=t->w*t->h;
			truetexel+=t->tw*t->th;
			databytes+=t->bytesu;
			truebytes+=t->bytes;
			if (t->prio<0.299)prio0++;
			else if (t->prio<0.399)prio1++;
			else if (t->prio<0.499)prio2++;
			else if (t->prio<0.599)prio3++;
			else prioh++;
			if (t->format == GL_RGBA)
				usedrgba++;
			else if (t->format == GL_RGB)
				usedrgb++;
#ifndef OGLES
			else if (t->format == GL_COLOR_INDEX)
				usedidx++;
#endif
			else
				usedother++;
		}
	}

	res = SWIDTH * SHEIGHT;
#ifndef OGLES
	glGetIntegerv(GL_INDEX_BITS, &idx);
#endif
	glGetIntegerv(GL_RED_BITS, &r);
	glGetIntegerv(GL_GREEN_BITS, &g);
	glGetIntegerv(GL_BLUE_BITS, &b);
	glGetIntegerv(GL_ALPHA_BITS, &a);
#ifndef OGLES
	glGetIntegerv(GL_DOUBLEBUFFER, &dbl);
#endif
	dbl += 1;
	glGetIntegerv(GL_DEPTH_BITS, &depth);
	gr_set_current_canvas(NULL);
	gr_set_curfont( GAME_FONT );
	gr_set_fontcolor( BM_XRGB(255,255,255),-1 );
	colorsize = (idx * res * dbl) / 8;
	depthsize = res * depth / 8;
	gr_printf(FSPACX(2),FSPACY(1),"%i flat %i tex %i sprites %i bitmaps",r_polyc,r_tpolyc,r_bitmapc);
	gr_printf(FSPACX(2), FSPACY(1)+LINE_SPACING, "%i(%i,%i,%i,%i) %iK(%iK wasted) (%i postcachedtex)", used, usedrgba, usedrgb, usedidx, usedother, truebytes / 1024, (truebytes - databytes) / 1024, r_texcount - r_cachedtexcount);
	gr_printf(FSPACX(2), FSPACY(1)+(LINE_SPACING*2), "%ibpp(r%i,g%i,b%i,a%i)x%i=%iK depth%i=%iK", idx, r, g, b, a, dbl, colorsize / 1024, depth, depthsize / 1024);
	gr_printf(FSPACX(2), FSPACY(1)+(LINE_SPACING*3), "total=%iK", (colorsize + depthsize + truebytes) / 1024);
}

void ogl_bindbmtex(grs_bitmap *bm){
	if (bm->gltexture==NULL || bm->gltexture->handle<=0)
		ogl_loadbmtexture(bm);
	if (bm->gltexture==NULL) {
#ifdef ANDROID
		crash_breadcrumb_v("ogl_bindbmtex: gltexture NULL after load, bm=%p flags=0x%x", (void*)bm, bm->bm_flags);
#endif
		return;
	}
	OGL_BINDTEXTURE(bm->gltexture->handle);
#ifdef ANDROID
	/* Selective filtering: set the correct texture filter for the current
	 * render context. Must be bidirectional -- if a prior context set
	 * GL_NEAREST on this texture object, we must restore the original
	 * mipmap filter when returning to a context that wants filtering.
	 *
	 * Font/text textures (OGL_FLAG_NOCOLOR) never have mipmaps -- they
	 * use MenuTexFilt regardless of render context so that text filtering
	 * is grouped with menus/briefings/videos/reticle (default off). */
	if (GameCfg.TexFilt > 0) {
		if (bm->gltexture->flags & OGL_FLAG_NOCOLOR) {
			/* Font texture: filter controlled by MenuTexFilt in all contexts */
			if (GameCfg.MenuTexFilt) {
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			} else {
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			}
		} else if (bm->gltexture->has_mipmaps) {
			/* World/HUD texture with mipmaps */
			int use_nearest = 0;
			if (g_ogl_render_context == 0 && !GameCfg.MenuTexFilt)
				use_nearest = 1;
			else if (g_ogl_render_context == 2 && !GameCfg.HudTexFilt)
				use_nearest = 1;
			if (use_nearest) {
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			} else {
				GLenum min_f = GameCfg.TexFilt >= 2
					? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST;
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_f);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}
		}
	}
#endif
	bm->gltexture->numrend++;
}

#ifdef ANDROID
/* Return total GPU texture memory in use (bytes) */
int ogl_get_texture_bytes(void)
{
	int total = 0, i;
	for (i = 0; i < OGL_TEXTURE_LIST_SIZE; i++)
		if (ogl_texture_list[i].handle > 0)
			total += ogl_texture_list[i].bytes;
	return total;
}

/* Re-apply anisotropic filtering to all loaded textures that have mipmaps.
 * Adreno GPUs ignore AF on textures without a complete mipmap chain. */
void ogl_apply_anisotropy_all(void)
{
	int i, count = 0, total = 0;
	GLfloat level = (ogl_aniso_level > 1 && ogl_maxanisotropy > 1.0f)
		? (GLfloat)(ogl_aniso_level < ogl_maxanisotropy ? ogl_aniso_level : (int)ogl_maxanisotropy)
		: 1.0f;
	for (i = 0; i < OGL_TEXTURE_LIST_SIZE; i++) {
		if (ogl_texture_list[i].handle > 0 && ogl_texture_list[i].has_mipmaps) {
			glBindTexture(GL_TEXTURE_2D, ogl_texture_list[i].handle);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, level);
			count++;
		}
		if (ogl_texture_list[i].handle > 0) total++;
	}
	ogl_last_bound_tex = 0; /* invalidate bind cache */
	__android_log_print(ANDROID_LOG_INFO, "DXX",
	    "anisotropy: applied level %.0f to %d/%d mipmapped textures", level, count, total);
}

/* android port: MSAA via FBO -- create/destroy/resolve helpers */
static void ogl_msaa_destroy_fbo(void)
{
	if (ogl_msaa_fbo) {
		glDeleteFramebuffers(1, &ogl_msaa_fbo);
		ogl_msaa_fbo = 0;
	}
	if (ogl_msaa_color_rbo) {
		glDeleteRenderbuffers(1, &ogl_msaa_color_rbo);
		ogl_msaa_color_rbo = 0;
	}
	if (ogl_msaa_depth_rbo) {
		glDeleteRenderbuffers(1, &ogl_msaa_depth_rbo);
		ogl_msaa_depth_rbo = 0;
	}
	ogl_msaa_w = ogl_msaa_h = 0;
	g_msaa_fbo_bound = 0;
}

static int ogl_msaa_create_fbo(int samples, int w, int h)
{
	ogl_msaa_destroy_fbo();

	/* Clamp to hardware max -- exceeding it can crash some drivers */
	if (ogl_msaa_max_samples > 0 && samples > ogl_msaa_max_samples)
		samples = ogl_msaa_max_samples;
	if (samples < 2) return 0;

	/* Query default framebuffer bit depths to match format for glBlitFramebuffer.
	 * GLES 3.0 requires identical formats for multisample resolve */
	GLenum color_fmt;
	{
		GLint rb = 0, gb = 0, bb = 0, ab = 0;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glGetIntegerv(GL_RED_BITS, &rb);
		glGetIntegerv(GL_GREEN_BITS, &gb);
		glGetIntegerv(GL_BLUE_BITS, &bb);
		glGetIntegerv(GL_ALPHA_BITS, &ab);
		if (rb <= 5 && gb <= 6 && bb <= 5 && ab == 0)
			color_fmt = 0x8D62; /* GL_RGB565 */
		else if (ab > 0)
			color_fmt = GL_RGBA8;
		else
			color_fmt = GL_RGB8;
		__android_log_print(ANDROID_LOG_INFO, "DXX",
		    "MSAA: default FB bits r=%d g=%d b=%d a=%d -> fmt=0x%x",
		    rb, gb, bb, ab, color_fmt);
	}

	glGenFramebuffers(1, &ogl_msaa_fbo);
	glGenRenderbuffers(1, &ogl_msaa_color_rbo);
	glGenRenderbuffers(1, &ogl_msaa_depth_rbo);

	glBindRenderbuffer(GL_RENDERBUFFER, ogl_msaa_color_rbo);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, color_fmt, w, h);

	glBindRenderbuffer(GL_RENDERBUFFER, ogl_msaa_depth_rbo);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT16, w, h);

	glBindFramebuffer(GL_FRAMEBUFFER, ogl_msaa_fbo);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	    GL_RENDERBUFFER, ogl_msaa_color_rbo);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
	    GL_RENDERBUFFER, ogl_msaa_depth_rbo);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		__android_log_print(ANDROID_LOG_ERROR, "DXX",
		    "MSAA FBO incomplete: status=0x%x samples=%d %dx%d fmt=0x%x",
		    status, samples, w, h, color_fmt);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		ogl_msaa_destroy_fbo();
		return 0;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	ogl_msaa_w = w;
	ogl_msaa_h = h;
	__android_log_print(ANDROID_LOG_INFO, "DXX",
	    "MSAA FBO created: %dx samples, %dx%d fmt=0x%x", samples, w, h, color_fmt);
	return 1;
}
#endif

//gltexture MUST be bound first
void ogl_texwrap(ogl_texture *gltexture,int state)
{
	if (gltexture->wrapstate != state || gltexture->numrend < 1)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, state);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, state);
		gltexture->wrapstate = state;
	}
}

void ogl_cache_polymodel_textures(int model_num)
{
	polymodel *po;
	int i;

	if (model_num < 0)
		return;
	po = &Polygon_models[model_num];
	for (i=0;i<po->n_textures;i++)  {
		PIGGY_PAGE_IN(ObjBitmaps[ObjBitmapPtrs[po->first_texture + i]]);
		ogl_loadbmtexture(&GameBitmaps[ObjBitmaps[ObjBitmapPtrs[po->first_texture + i]].index]);
	}
}

void ogl_cache_level_textures(void)
{
	int i;
#ifdef ANDROID
	struct timespec cache_start;
	clock_gettime(CLOCK_MONOTONIC, &cache_start);
#endif
	
	ogl_reset_texture_stats_internal();//loading a new lev should reset textures

	if (!ogl_allow_png())
		ogl_smash_png_textures();

#ifdef ANDROID
	crash_breadcrumb_v("ogl_cache: starting, %i bitmaps", Num_bitmap_files);
	__android_log_print(ANDROID_LOG_INFO, "DXX",
	    "ogl_cache: starting, %i bitmaps, allow_png=%i",
	    Num_bitmap_files, ogl_allow_png());
	{
		int before_hires = r_hires_loaded;
		int n_already = 0, n_paged_skip = 0, n_bm_upload = 0, n_png_fail = 0;
		int n_masks = 0;
		for (i = 0; i < Num_bitmap_files; i++) {
			grs_bitmap *bm = &GameBitmaps[i];
			const char *cname = piggy_game_bitmap_name(bm);
			if (cname && !d_stricmp(cname, "metl154"))
				debug_log(DLOG_TEXTURE,
					"[metl154cache] pre-load: i=%d bm_flags=0x%x GameBitmapFlags=0x%x gltex=%p mask=%p",
					i, bm->bm_flags,
					piggy_bitmap_get_flags(bm),
					(void *)bm->gltexture,
					(void *)bm->gltexture_mask);
			int had_tex = (bm->gltexture && bm->gltexture->handle > 0);
			ogl_loadbmtexture(bm);
			if (cname && !d_stricmp(cname, "metl154"))
				debug_log(DLOG_TEXTURE,
					"[metl154cache] post-load: i=%d bm_flags=0x%x gltex=%p mask=%p",
					i, bm->bm_flags,
					(void *)bm->gltexture,
					(void *)bm->gltexture_mask);
			if (had_tex)
				n_already++;
			else if (bm->gltexture && bm->gltexture->handle > 0) {
				if (!bm->gltexture->is_png)
					n_bm_upload++;
			} else {
				if (bm->gltexture && bm->gltexture->handle <= 0)
					n_png_fail++;
				else
					n_paged_skip++;
			}
			if (bm->gltexture_mask && bm->gltexture_mask->handle > 0)
				n_masks++;
		}
		crash_breadcrumb("ogl_cache: done");
		__android_log_print(ANDROID_LOG_INFO, "DXX",
		    "ogl_cache: done. hires=%i masks=%i already=%i bitmap=%i skipped=%i png_fail=%i etc2_zero=%i",
		    r_hires_loaded - before_hires, n_masks, n_already, n_bm_upload, n_paged_skip, n_png_fail,
		    r_etc2_zero_data);
	}
#else
	for (i = 0; i < Num_bitmap_files; i++) {
		if (!(GameBitmaps[i].bm_flags & BM_FLAG_PAGED_OUT))
			ogl_loadbmtexture(&GameBitmaps[i]);
	}
#endif

#ifdef ANDROID
	{
		struct timespec cache_end;
		clock_gettime(CLOCK_MONOTONIC, &cache_end);
		g_cache_time_ms = (int)((cache_end.tv_sec - cache_start.tv_sec) * 1000 +
			(cache_end.tv_nsec - cache_start.tv_nsec) / 1000000);
	}
#endif

	xmodel_load_gl_all();
	glmprintf((0,"finished caching\n"));
	r_cachedtexcount = r_texcount;
}

bool g3_draw_line(const g3s_point *p0,const g3s_point *p1)
{
	int c;
	GLfloat color_r, color_g, color_b;
	GLfloat color_array[] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	GLfloat vertex_array[] = { f2glf(p0->p3_vec.x),f2glf(p0->p3_vec.y),-f2glf(p0->p3_vec.z), f2glf(p1->p3_vec.x),f2glf(p1->p3_vec.y),-f2glf(p1->p3_vec.z) };
  
	c=grd_curcanv->cv_color;
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	OGL_DISABLE(TEXTURE_2D);
	color_r = PAL2Tr(c);
	color_g = PAL2Tg(c);
	color_b = PAL2Tb(c);
	color_array[0] = color_array[4] = color_r;
	color_array[1] = color_array[5] = color_g;
	color_array[2] = color_array[6] = color_b;
	color_array[3] = color_array[7] = 1.0;
	glVertexPointer(3, GL_FLOAT, 0, vertex_array);
	glColorPointer(4, GL_FLOAT, 0, color_array);
	glDrawArrays(GL_LINES, 0, 2);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	return 1;
}

void ogl_drawcircle(int nsides, int type, GLfloat *vertex_array)
{
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, vertex_array);
	glDrawArrays(type, 0, nsides);
	glDisableClientState(GL_VERTEX_ARRAY);
}

GLfloat *circle_array_init(int nsides)
{
	int i;
	float ang;
	GLfloat *vertex_array = (GLfloat *) d_malloc(sizeof(GLfloat) * nsides * 2);
	
	for(i = 0; i < nsides; i++) {
		ang = 2.0 * M_PI * i / nsides;
		vertex_array[i * 2] = cosf(ang);
        	vertex_array[i * 2 + 1] = sinf(ang);
	}
	
	return vertex_array;
}
 
GLfloat *circle_array_init_2(int nsides, float xsc, float xo, float ysc, float yo)
{
 	int i;
 	float ang;
	GLfloat *vertex_array = (GLfloat *) d_malloc(sizeof(GLfloat) * nsides * 2);
	
	for(i = 0; i < nsides; i++) {
		ang = 2.0 * M_PI * i / nsides;
		vertex_array[i * 2] = cosf(ang) * xsc + xo;
		vertex_array[i * 2 + 1] = sinf(ang) * ysc + yo;
	}
	
	return vertex_array;
}

void ogl_draw_vertex_reticle(int cross,int primary,int secondary,int color,int alpha,int size_offs)
{
	int size=270+(size_offs*20), i;
	float scale = ((float)SWIDTH/SHEIGHT), ret_rgba[4], ret_dark_rgba[4];
	GLfloat cross_lva[8 * 2] = {
		-4.0, 2.0, -2.0, 0, -3.0, -4.0, -2.0, -3.0, 4.0, 2.0, 2.0, 0, 3.0, -4.0, 2.0, -3.0,
	};
	GLfloat primary_lva[4][4 * 2] = {
		{ -5.5, -5.0, -6.5, -7.5, -10.0, -7.0, -10.0, -8.7 },
		{ -10.0, -7.0, -10.0, -8.7, -15.0, -8.5, -15.0, -9.5 },
		{ 5.5, -5.0, 6.5, -7.5, 10.0, -7.0, 10.0, -8.7 },
		{ 10.0, -7.0, 10.0, -8.7, 15.0, -8.5, 15.0, -9.5 }
	};
	GLfloat dark_lca[16 * 4] = {
		0.125, 0.54, 0.125, 0.6, 0.125, 0.54, 0.125, 0.6, 0.125, 0.54, 0.125, 0.6, 0.125, 0.54, 0.125, 0.6,
		0.125, 0.54, 0.125, 0.6, 0.125, 0.54, 0.125, 0.6, 0.125, 0.54, 0.125, 0.6, 0.125, 0.54, 0.125, 0.6,
		0.125, 0.54, 0.125, 0.6, 0.125, 0.54, 0.125, 0.6, 0.125, 0.54, 0.125, 0.6, 0.125, 0.54, 0.125, 0.6,
		0.125, 0.54, 0.125, 0.6, 0.125, 0.54, 0.125, 0.6, 0.125, 0.54, 0.125, 0.6, 0.125, 0.54, 0.125, 0.6
	};
	GLfloat bright_lca[16 * 4] = {
		0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0,
		0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0,
		0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0,
		0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0
	};
	GLfloat cross_lca[8 * 4] = {
		0.125, 0.54, 0.125, 0.6, 0.125, 1.0, 0.125, 1.0,
		0.125, 0.54, 0.125, 0.6, 0.125, 1.0, 0.125, 1.0,
		0.125, 0.54, 0.125, 0.6, 0.125, 1.0, 0.125, 1.0,
		0.125, 0.54, 0.125, 0.6, 0.125, 1.0, 0.125, 1.0
	};
	GLfloat primary_lca[2][4 * 4] = {
		{0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 0.54, 0.125, 0.6, 0.125, 0.54, 0.125, 0.6},
		{0.125, 0.54, 0.125, 0.6, 0.125, 0.54, 0.125, 0.6, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0, 0.125, 1.0}
	};
	
	ret_rgba[0] = PAL2Tr(color);
	ret_dark_rgba[0] = ret_rgba[0]/2;
	ret_rgba[1] = PAL2Tg(color);
	ret_dark_rgba[1] = ret_rgba[1]/2;
	ret_rgba[2] = PAL2Tb(color);
	ret_dark_rgba[2] = ret_rgba[2]/2;
	ret_rgba[3] = 1.0 - ((float)alpha / ((float)GR_FADE_LEVELS));
	ret_dark_rgba[3] = ret_rgba[3]/2;

	for (i = 0; i < 16*4; i += 4)
	{
		bright_lca[i] = ret_rgba[0];
		dark_lca[i] = ret_dark_rgba[0];
		bright_lca[i+1] = ret_rgba[1];
		dark_lca[i+1] = ret_dark_rgba[1];
		bright_lca[i+2] = ret_rgba[2];
		dark_lca[i+2] = ret_dark_rgba[2];
		bright_lca[i+3] = ret_rgba[3];
		dark_lca[i+3] = ret_dark_rgba[3];
	}
	for (i = 0; i < 8*4; i += 8)
	{
		cross_lca[i] = ret_dark_rgba[0];
		cross_lca[i+1] = ret_dark_rgba[1];
		cross_lca[i+2] = ret_dark_rgba[2];
		cross_lca[i+3] = ret_dark_rgba[3];
		cross_lca[i+4] = ret_rgba[0];
		cross_lca[i+5] = ret_rgba[1];
		cross_lca[i+6] = ret_rgba[2];
		cross_lca[i+7] = ret_rgba[3];
	}

	primary_lca[0][0] = primary_lca[0][4] = primary_lca[1][8] = primary_lca[1][12] = ret_rgba[0];
	primary_lca[0][1] = primary_lca[0][5] = primary_lca[1][9] = primary_lca[1][13] = ret_rgba[1];
	primary_lca[0][2] = primary_lca[0][6] = primary_lca[1][10] = primary_lca[1][14] = ret_rgba[2];
	primary_lca[0][3] = primary_lca[0][7] = primary_lca[1][11] = primary_lca[1][15] = ret_rgba[3];
	primary_lca[1][0] = primary_lca[1][4] = primary_lca[0][8] = primary_lca[0][12] = ret_dark_rgba[0];
	primary_lca[1][1] = primary_lca[1][5] = primary_lca[0][9] = primary_lca[0][13] = ret_dark_rgba[1];
	primary_lca[1][2] = primary_lca[1][6] = primary_lca[0][10] = primary_lca[0][14] = ret_dark_rgba[2];
	primary_lca[1][3] = primary_lca[1][7] = primary_lca[0][11] = primary_lca[0][15] = ret_dark_rgba[3];

	glPushMatrix();
	glTranslatef((grd_curcanv->cv_bitmap.bm_w/2+grd_curcanv->cv_bitmap.bm_x)/(float)last_width,1.0-(grd_curcanv->cv_bitmap.bm_h/2+grd_curcanv->cv_bitmap.bm_y)/(float)last_height,0);

	if (scale >= 1)
	{
		size/=scale;
		glScalef(f2glf(size),f2glf(size*scale),f2glf(size));
	}
	else
	{
		size*=scale;
		glScalef(f2glf(size/scale),f2glf(size),f2glf(size));
	}

	glLineWidth(linedotscale*2);
	OGL_DISABLE(TEXTURE_2D);
	glDisable(GL_CULL_FACE);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	
	//cross
	if(cross)
		glColorPointer(4, GL_FLOAT, 0, cross_lca);
	else
		glColorPointer(4, GL_FLOAT, 0, dark_lca);
	glVertexPointer(2, GL_FLOAT, 0, cross_lva);
	glDrawArrays(GL_LINES, 0, 8);
	
	//left primary bar
	if(primary == 0)
		glColorPointer(4, GL_FLOAT, 0, dark_lca);
	else
		glColorPointer(4, GL_FLOAT, 0, primary_lca[0]);
	glVertexPointer(2, GL_FLOAT, 0, primary_lva[0]);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	if(primary != 2)
		glColorPointer(4, GL_FLOAT, 0, dark_lca);
	else
		glColorPointer(4, GL_FLOAT, 0, primary_lca[1]);
	glVertexPointer(2, GL_FLOAT, 0, primary_lva[1]);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	//right primary bar
	if(primary == 0)
		glColorPointer(4, GL_FLOAT, 0, dark_lca);
	else
		glColorPointer(4, GL_FLOAT, 0, primary_lca[0]);
	glVertexPointer(2, GL_FLOAT, 0, primary_lva[2]);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	if(primary != 2)
		glColorPointer(4, GL_FLOAT, 0, dark_lca);
	else
		glColorPointer(4, GL_FLOAT, 0, primary_lca[1]);
	glVertexPointer(2, GL_FLOAT, 0, primary_lva[3]);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	
	if (secondary<=2){
		//left secondary
		if (secondary != 1)
			glColorPointer(4, GL_FLOAT, 0, dark_lca);
		else
			glColorPointer(4, GL_FLOAT, 0, bright_lca);
		if(!secondary_lva[0])
			secondary_lva[0] = circle_array_init_2(16, 2.0, -10.0, 2.0, -2.0);
		ogl_drawcircle(16, GL_LINE_LOOP, secondary_lva[0]);
		//right secondary
		if (secondary != 2)
			glColorPointer(4, GL_FLOAT, 0, dark_lca);
		else
			glColorPointer(4, GL_FLOAT, 0, bright_lca);
		if(!secondary_lva[1])
			secondary_lva[1] = circle_array_init_2(16, 2.0, 10.0, 2.0, -2.0);
		ogl_drawcircle(16, GL_LINE_LOOP, secondary_lva[1]);
	}
	else {
		//bottom/middle secondary
		if (secondary != 4)
			glColorPointer(4, GL_FLOAT, 0, dark_lca);
		else
			glColorPointer(4, GL_FLOAT, 0, bright_lca);
		if(!secondary_lva[2])
			secondary_lva[2] = circle_array_init_2(16, 2.0, 0.0, 2.0, -8.0);
		ogl_drawcircle(16, GL_LINE_LOOP, secondary_lva[2]);
	}
	
	//glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glPopMatrix();
	glLineWidth(linedotscale);
}

/*
 * Stars on heaven in exit sequence, automap objects
 */
int g3_draw_sphere(g3s_point *pnt,fix rad){
	int c=grd_curcanv->cv_color, i;
	float scale = ((float)grd_curcanv->cv_bitmap.bm_w/grd_curcanv->cv_bitmap.bm_h);
	GLfloat color_array[20*4];
	
	for (i = 0; i < 20*4; i += 4)
	{
		color_array[i] = CPAL2Tr(c);
		color_array[i+1] = CPAL2Tg(c);
		color_array[i+2] = CPAL2Tb(c);
		color_array[i+3] = 1.0;
	}
	OGL_DISABLE(TEXTURE_2D);
	glDisable(GL_CULL_FACE);
	glPushMatrix();
	glTranslatef(f2glf(pnt->p3_vec.x),f2glf(pnt->p3_vec.y),-f2glf(pnt->p3_vec.z));
	if (scale >= 1)
	{
		rad/=scale;
		glScalef(f2glf(rad),f2glf(rad*scale),f2glf(rad));
	}
	else
	{
		rad*=scale;
		glScalef(f2glf(rad/scale),f2glf(rad),f2glf(rad));
	}
	if(!sphere_va)
		sphere_va = circle_array_init(20);
	glEnableClientState(GL_COLOR_ARRAY);
	glColorPointer(4, GL_FLOAT, 0, color_array);
	ogl_drawcircle(20, GL_TRIANGLE_FAN, sphere_va);
	glDisableClientState(GL_COLOR_ARRAY);
	glPopMatrix();
	return 0;
}

int gr_ucircle(fix xc1, fix yc1, fix r1)
{
	int c, nsides;
	c=grd_curcanv->cv_color;
	OGL_DISABLE(TEXTURE_2D);
	glColor4f(CPAL2Tr(c),CPAL2Tg(c),CPAL2Tb(c),(grd_curcanv->cv_fade_level >= GR_FADE_OFF)?1.0:1.0 - (float)grd_curcanv->cv_fade_level / ((float)GR_FADE_LEVELS - 1.0));
	glPushMatrix();
	glTranslatef(
	             (f2fl(xc1) + grd_curcanv->cv_bitmap.bm_x + 0.5) / (float)last_width,
	             1.0 - (f2fl(yc1) + grd_curcanv->cv_bitmap.bm_y + 0.5) / (float)last_height,0);
	glScalef(f2fl(r1) / last_width, f2fl(r1) / last_height, 1.0);
	nsides = 10 + 2 * (int)(M_PI * f2fl(r1) / 19);
	if(!circle_va)
		circle_va = circle_array_init(nsides);
	ogl_drawcircle(nsides, GL_LINE_LOOP, circle_va);
	glPopMatrix();
	return 0;
}

int gr_circle(fix xc1,fix yc1,fix r1){
	return gr_ucircle(xc1,yc1,r1);
}

int gr_disk(fix x,fix y,fix r)
{
	int c, nsides;
	c=grd_curcanv->cv_color;
	OGL_DISABLE(TEXTURE_2D);
	glColor4f(CPAL2Tr(c),CPAL2Tg(c),CPAL2Tb(c),(grd_curcanv->cv_fade_level >= GR_FADE_OFF)?1.0:1.0 - (float)grd_curcanv->cv_fade_level / ((float)GR_FADE_LEVELS - 1.0));
	glPushMatrix();
	glTranslatef(
	             (f2fl(x) + grd_curcanv->cv_bitmap.bm_x + 0.5) / (float)last_width,
	             1.0 - (f2fl(y) + grd_curcanv->cv_bitmap.bm_y + 0.5) / (float)last_height,0);
	glScalef(f2fl(r) / last_width, f2fl(r) / last_height, 1.0);
	nsides = 10 + 2 * (int)(M_PI * f2fl(r) / 19);
	if(!disk_va)
		disk_va = circle_array_init(nsides);
	ogl_drawcircle(nsides, GL_TRIANGLE_FAN, disk_va);
	glPopMatrix();
	return 0;
}

/*
 * Draw flat-shaded Polygon (Lasers, Drone-arms, Driller-ears)
 */
bool g3_draw_poly(int nv,const g3s_point **pointlist)
{
	int c, index3, index4;
	float color_r, color_g, color_b, color_a;
	GLfloat vertex_array[MAX_VERTS * 3], color_array[MAX_VERTS * 4];

	if (nv > MAX_VERTS)
		Error("Too many vertices %d", nv);

	r_polyc++;
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	c = grd_curcanv->cv_color;
	OGL_DISABLE(TEXTURE_2D);
	color_r = PAL2Tr(c);
	color_g = PAL2Tg(c);
	color_b = PAL2Tb(c);

	if (grd_curcanv->cv_fade_level >= GR_FADE_OFF)
		color_a = 1.0;
	else
		color_a = 1.0 - (float)grd_curcanv->cv_fade_level / ((float)GR_FADE_LEVELS - 1.0);

	for (c=0; c<nv; c++){
		index3 = c * 3;
		index4 = c * 4;
		color_array[index4]    = color_r;
		color_array[index4+1]  = color_g;
		color_array[index4+2]  = color_b;
		color_array[index4+3]  = color_a;
		vertex_array[index3]   = f2glf(pointlist[c]->p3_vec.x);
		vertex_array[index3+1] = f2glf(pointlist[c]->p3_vec.y);
		vertex_array[index3+2] = -f2glf(pointlist[c]->p3_vec.z);
	}

	glVertexPointer(3, GL_FLOAT, 0, vertex_array);
	glColorPointer(4, GL_FLOAT, 0, color_array);
	glDrawArrays(GL_TRIANGLE_FAN, 0, nv);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	return 0;
}

void gr_upoly_tmap(int nverts, int *vert ){
		glmprintf((0,"gr_upoly_tmap: unhandled\n"));//should never get called
}

void draw_tmap_flat(grs_bitmap *bm,int nv,g3s_point **vertlist){
		glmprintf((0,"draw_tmap_flat: unhandled\n"));//should never get called
}

extern void (*tmap_drawer_ptr)(grs_bitmap *bm,int nv,g3s_point **vertlist);


/*
 * Everything texturemapped (walls, robots, ship)
 */ 
bool g3_draw_tmap(int nv,const g3s_point **pointlist,g3s_uvl *uvl_list,g3s_lrgb *light_rgb,grs_bitmap *bm)
{
	int c, index2, index3, index4;
	GLfloat vertex_array[MAX_VERTS * 3], color_array[MAX_VERTS * 4], texcoord_array[MAX_VERTS * 2];
	GLfloat color_alpha = 1.0;
#if defined(ANDROID) && defined(OGL_MERGE)
	int draw_order = ogl_metl154_next_draw_order();
	const char *cover_shader = NULL;
#endif

	if (nv > MAX_VERTS)
		Error("Too many vertices: %d", nv);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	
	if (tmap_drawer_ptr == draw_tmap) {
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		OGL_ENABLE(TEXTURE_2D);
		ogl_bindbmtex(bm);
		if (bm->gltexture == NULL)
			return 0;
		ogl_texwrap(bm->gltexture, GL_REPEAT);
		r_tpolyc++;
#ifdef ANDROID
		/* android port: log first few 3D texture bindings per level for debugging */
		if (r_etc2_render_log_count < 5) {
			const char *bname = piggy_game_bitmap_name(bm);
			GLint cur_min_filter = 0;
			glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &cur_min_filter);
			__android_log_print(ANDROID_LOG_INFO, "DXX-TEX",
			    "3D render bind #%d: %s handle=%u is_png=%d w=%d h=%d u=%.3f v=%.3f min_filter=0x%x",
			    r_etc2_render_log_count, bname ? bname : "?",
			    bm->gltexture->handle, bm->gltexture->is_png,
			    bm->gltexture->w, bm->gltexture->h,
			    bm->gltexture->u, bm->gltexture->v,
			    cur_min_filter);
			r_etc2_render_log_count++;
		}
#endif
		color_alpha = (grd_curcanv->cv_fade_level >= GR_FADE_OFF)?1.0:(1.0 - (float)grd_curcanv->cv_fade_level / ((float)GR_FADE_LEVELS - 1.0));
#if defined(ANDROID) && defined(OGL_MERGE)
		cover_shader = "single";
#endif
	} else if (tmap_drawer_ptr == draw_tmap_flat) {
		OGL_DISABLE(TEXTURE_2D);
		/* for cloaked state faces */
		color_alpha = 1.0 - (grd_curcanv->cv_fade_level/(GLfloat)NUM_LIGHTING_LEVELS);
#if defined(ANDROID) && defined(OGL_MERGE)
		cover_shader = "flat";
#endif
	} else {
		glmprintf((0,"g3_draw_tmap: unhandled tmap_drawer %p\n",tmap_drawer_ptr));
		return 0;
	}

	for (c=0; c<nv; c++) {
		index2 = c * 2;
		index3 = c * 3;
		index4 = c * 4;
		
		vertex_array[index3]     = f2glf(pointlist[c]->p3_vec.x);
		vertex_array[index3+1]   = f2glf(pointlist[c]->p3_vec.y);
		vertex_array[index3+2]   = -f2glf(pointlist[c]->p3_vec.z);
		if (tmap_drawer_ptr == draw_tmap_flat) {
			color_array[index4]      = 0;
			color_array[index4+1]    = color_array[index4];
			color_array[index4+2]    = color_array[index4];
			color_array[index4+3]    = color_alpha;
			
		} else { 
			color_array[index4]      = bm->bm_flags & BM_FLAG_NO_LIGHTING ? 1.0 : f2glf(light_rgb[c].r);
			color_array[index4+1]    = bm->bm_flags & BM_FLAG_NO_LIGHTING ? 1.0 : f2glf(light_rgb[c].g);
			color_array[index4+2]    = bm->bm_flags & BM_FLAG_NO_LIGHTING ? 1.0 : f2glf(light_rgb[c].b);
			color_array[index4+3]    = color_alpha;
		}
		texcoord_array[index2]   = f2glf(uvl_list[c].u);
		texcoord_array[index2+1] = f2glf(uvl_list[c].v);
	}
	
	glVertexPointer(3, GL_FLOAT, 0, vertex_array);
	glColorPointer(4, GL_FLOAT, 0, color_array);
	if (tmap_drawer_ptr == draw_tmap) {
		glTexCoordPointer(2, GL_FLOAT, 0, texcoord_array);  
	}
	
	glDrawArrays(GL_TRIANGLE_FAN, 0, nv);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

#if defined(ANDROID) && defined(OGL_MERGE)
	ogl_log_metl154_cover(cover_shader, piggy_game_bitmap_name(bm), NULL,
		pointlist, nv, draw_order);
#endif

#ifdef ANDROID
	/* Debug texture label overlay: accumulate screen-space labels */
	if (g_debug_tex_overlay_active && tmap_drawer_ptr == draw_tmap
	    && g_debug_tex_label_count < DEBUG_TEX_MAX_LABELS && nv >= 3)
	{
		/* Compute centroid in view space */
		fix cx = 0, cy = 0, cz = 0;
		int i;
		for (i = 0; i < nv; i++) {
			cx += pointlist[i]->p3_vec.x / nv;
			cy += pointlist[i]->p3_vec.y / nv;
			cz += pointlist[i]->p3_vec.z / nv;
		}
		/* Project to screen (same formula as g3_project_point) */
		if (cz > F1_0 / 4) { /* in front of camera */
			fix sx_fix, sy_fix;
			int checkmuldiv(fix *r, fix a, fix b, fix c);
			if (checkmuldiv(&sx_fix, cx, Canv_w2, cz)
			    && checkmuldiv(&sy_fix, cy, Canv_h2, cz))
			{
				int sx = f2i(Canv_w2 + sx_fix);
				int sy = f2i(Canv_h2 - sy_fix);
				int sw = grd_curcanv->cv_bitmap.bm_w;
				int sh = grd_curcanv->cv_bitmap.bm_h;
				if (sx >= 0 && sx < sw && sy >= 0 && sy < sh) {
					struct debug_tex_label *lbl = &g_debug_tex_labels[g_debug_tex_label_count];
					lbl->sx = sx;
					lbl->sy = sy;
					lbl->is_hires = (bm->gltexture && bm->gltexture->is_png) ? 1 : 0;
					const char *bname = piggy_game_bitmap_name(bm);
					if (bname) {
						strncpy(lbl->name, bname, sizeof(lbl->name) - 1);
						lbl->name[sizeof(lbl->name) - 1] = '\0';
						g_debug_tex_label_count++;
					}
				}
			}
		}
	}
#endif

	return 0;
}

/*
 * Everything texturemapped with secondary texture (walls with secondary texture)
 */
static bool ogl_draw_tmap_2_internal(int nv, const g3s_point **pointlist, g3s_uvl *uvl_list, g3s_lrgb *light_rgb, grs_bitmap *bmbot, grs_bitmap *bmovl, int orient)
{
	int c, index2, index3, index4;
	GLfloat vertex_array[MAX_VERTS * 3], color_array[MAX_VERTS * 4], texcoordovl_array[MAX_VERTS * 2];
#ifdef OGL_MERGE
	GLfloat texcoordbot_array[MAX_VERTS * 2];
	int super;
#if defined(ANDROID)
	int draw_order = ogl_metl154_next_draw_order();
	int is_metl154_plain = 0;
	int metl154_force_cull_off = 0, metl154_force_depth_off = 0;
	GLint metl154_depth_enabled = 0, metl154_blend_enabled = 0, metl154_cull_enabled = 0;
	GLint metl154_depth_func = 0, metl154_front_face = 0, metl154_cull_mode = 0, metl154_draw_fbo = 0;
	GLboolean metl154_depth_writemask = GL_TRUE, metl154_color_mask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
	GLfloat metl154_screen_area = 0.0f;
#endif
#endif

	if (nv > MAX_VERTS)
		Error("Too many vertices: %d", nv);

#ifndef OGL_MERGE
	g3_draw_tmap(nv,pointlist,uvl_list,light_rgb,bmbot);//draw the bottom texture first.. could be optimized with multitexturing..
	
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	
	OGL_ENABLE(TEXTURE_2D);
	ogl_bindbmtex(bmovl);
	if (bmovl->gltexture == NULL)
		return 0;
	ogl_texwrap(bmovl->gltexture,GL_REPEAT);
#else
	ogl_bindbmtex(bmbot);
	if (bmbot->gltexture == NULL)
		return 0;
	ogl_texwrap(bmbot->gltexture,GL_REPEAT);

	glActiveTexture(GL_TEXTURE1);
	ogl_bindbmtex(bmovl);
	if (bmovl->gltexture == NULL) {
		glActiveTexture(GL_TEXTURE0);
		return 0;
	}
	ogl_texwrap(bmovl->gltexture,GL_REPEAT);

	/* Compute super after bindbmtex -- on first encounter bm_flags may be
	 * BM_FLAG_PAGED_OUT (no SUPER_TRANSPARENT) and gltexture_mask NULL.
	 * bindbmtex pages in the bitmap and generates the mask. */
	super = (bmovl->bm_flags & BM_FLAG_SUPER_TRANSPARENT) && bmovl->gltexture_mask;
	#if defined(ANDROID)
	is_metl154_plain = !super && ogl_is_metl154_bitmap(bmovl);
	if (ogl_is_metl154_bitmap(bmovl))
		debug_log(DLOG_TEXTURE,
			"[metl154super] idx=%d bm_flags=0x%x real=0x%x mask=%p mask_h=%u super=%d",
			(int)(bmovl - GameBitmaps),
			bmovl->bm_flags,
			piggy_bitmap_get_flags(bmovl),
			(void *)bmovl->gltexture_mask,
			bmovl->gltexture_mask ? bmovl->gltexture_mask->handle : 0,
			super);
	#endif

	if (super) {
		glActiveTexture(GL_TEXTURE2);
		OGL_BINDTEXTURE(bmovl->gltexture_mask->handle);
		ogl_texwrap(bmovl->gltexture_mask,GL_REPEAT);
	}

	glActiveTexture(GL_TEXTURE0);
#endif
	
	for (c=0; c<nv; c++) {
		index2 = c * 2;
		index3 = c * 3;
		index4 = c * 4;
		
#ifdef OGL_MERGE
		texcoordbot_array[index2]   = f2glf(uvl_list[c].u);
		texcoordbot_array[index2+1] = f2glf(uvl_list[c].v);
#endif

		switch(orient){
			case 1:
				texcoordovl_array[index2]   = 1.0-f2glf(uvl_list[c].v);
				texcoordovl_array[index2+1] = f2glf(uvl_list[c].u);
				break;
			case 2:
				texcoordovl_array[index2]   = 1.0-f2glf(uvl_list[c].u);
				texcoordovl_array[index2+1] = 1.0-f2glf(uvl_list[c].v);
				break;
			case 3:
				texcoordovl_array[index2]   = f2glf(uvl_list[c].v);
				texcoordovl_array[index2+1] = 1.0-f2glf(uvl_list[c].u);
				break;
			default:
				texcoordovl_array[index2]   = f2glf(uvl_list[c].u);
				texcoordovl_array[index2+1] = f2glf(uvl_list[c].v);
				break;
		}
		
		color_array[index4]      = bmbot->bm_flags & BM_FLAG_NO_LIGHTING ? 1.0 : minf(1.0, f2glf(light_rgb[c].r));
		color_array[index4+1]    = bmbot->bm_flags & BM_FLAG_NO_LIGHTING ? 1.0 : minf(1.0, f2glf(light_rgb[c].g));
		color_array[index4+2]    = bmbot->bm_flags & BM_FLAG_NO_LIGHTING ? 1.0 : minf(1.0, f2glf(light_rgb[c].b));
		color_array[index4+3]    = (grd_curcanv->cv_fade_level >= GR_FADE_OFF)?1.0:(1.0 - (float)grd_curcanv->cv_fade_level / ((float)GR_FADE_LEVELS - 1.0));
		
		vertex_array[index3]     = f2glf(pointlist[c]->p3_vec.x);
		vertex_array[index3+1]   = f2glf(pointlist[c]->p3_vec.y);
		vertex_array[index3+2]   = -f2glf(pointlist[c]->p3_vec.z);
	}

#ifndef OGL_MERGE
	glVertexPointer(3, GL_FLOAT, 0, vertex_array);
	glColorPointer(4, GL_FLOAT, 0, color_array);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoordovl_array);
	glDrawArrays(GL_TRIANGLE_FAN, 0, nv);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

#ifdef ANDROID
	/* Debug texture label overlay: accumulate label for overlay/tmap2 bitmap.
	 * The bottom texture (tmap1) label is already handled by g3_draw_tmap above. */
	if (g_debug_tex_overlay_active
	    && g_debug_tex_label_count < DEBUG_TEX_MAX_LABELS && nv >= 3)
	{
		fix cx = 0, cy = 0, cz = 0;
		int i;
		for (i = 0; i < nv; i++) {
			cx += pointlist[i]->p3_vec.x / nv;
			cy += pointlist[i]->p3_vec.y / nv;
			cz += pointlist[i]->p3_vec.z / nv;
		}
		if (cz > F1_0 / 4) {
			fix sx_fix, sy_fix;
			int checkmuldiv(fix *r, fix a, fix b, fix c);
			if (checkmuldiv(&sx_fix, cx, Canv_w2, cz)
			    && checkmuldiv(&sy_fix, cy, Canv_h2, cz))
			{
				int sx = f2i(Canv_w2 + sx_fix);
				int sy = f2i(Canv_h2 - sy_fix) + 10; /* offset below tmap1 label */
				int sw = grd_curcanv->cv_bitmap.bm_w;
				int sh = grd_curcanv->cv_bitmap.bm_h;
				if (sx >= 0 && sx < sw && sy >= 0 && sy < sh) {
					struct debug_tex_label *lbl = &g_debug_tex_labels[g_debug_tex_label_count];
					lbl->sx = sx;
					lbl->sy = sy;
					lbl->is_hires = (bmovl->gltexture && bmovl->gltexture->is_png) ? 1 : 0;
					const char *bname = piggy_game_bitmap_name(bmovl);
					if (bname) {
						strncpy(lbl->name, bname, sizeof(lbl->name) - 1);
						lbl->name[sizeof(lbl->name) - 1] = '\0';
						g_debug_tex_label_count++;
					}
				}
			}
		}
	}
#endif
#else
	GLuint prog = super ? ogl_prog_tex2m : ogl_prog_tex2;
	#ifdef ANDROID
	int tex2_debug_mode = (!super && ogl_is_metl154_bitmap(bmovl)) ? g_metl154_debug_mode : METL154_DEBUG_NONE;
	#else
	int tex2_debug_mode = 0;
	#endif
#ifdef ANDROID
	gles3_shim_use_external(prog);
	r_shader_switches++;
	if (super) r_mask_draws++;
#else
	glUseProgram(prog);
#endif
	if (!super) {
		/* Alpha cutoff 0.5: overlay textures are palette-indexed with binary
		 * alpha (fully opaque or fully transparent). Hires KTX2 mipmaps use
		 * standard box-filter downsampling which averages transparent/opaque
		 * pixels, degrading overlay alpha at higher mip levels. Thresholding
		 * at 0.5 restores the intended binary alpha and prevents bars from
		 * disappearing at steep viewing angles. */
		ogl_prog_set_tex2_alpha_cutoff(0.5f);
		ogl_prog_set_tex2_debug_mode(tex2_debug_mode);
	}

#if defined(ANDROID)
	if (is_metl154_plain) {
		ogl_get_metl154_gl_state(&metl154_depth_enabled, &metl154_blend_enabled,
			&metl154_cull_enabled, &metl154_depth_writemask, &metl154_depth_func,
			&metl154_front_face, &metl154_cull_mode, metl154_color_mask,
			&metl154_draw_fbo);
		metl154_screen_area = ogl_get_metl154_screen_area(pointlist, nv);
		if (metl154_cull_enabled) {
			glDisable(GL_CULL_FACE);
			metl154_force_cull_off = 1;
		}
		if (tex2_debug_mode != METL154_DEBUG_NONE) {
			if (metl154_depth_enabled)
				glDisable(GL_DEPTH_TEST);
			glDepthMask(GL_FALSE);
			metl154_force_depth_off = 1;
		}
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1.0f, -1.0f);
	}
#endif

	glEnableVertexAttribArray(OGL_APOS);
	glEnableVertexAttribArray(OGL_ACOLOR);
	glEnableVertexAttribArray(OGL_ATEXCOORD);
	glEnableVertexAttribArray(OGL_ATEXCOORD2);

#ifdef ANDROID
	{
		/* GLES 3.0 requires vertex data in buffer objects */
		static GLuint merge_vbo = 0;
		if (!merge_vbo)
			glGenBuffers(1, &merge_vbo);
		int vb = nv * 3 * (int)sizeof(GLfloat);
		int cb = nv * 4 * (int)sizeof(GLfloat);
		int tb = nv * 2 * (int)sizeof(GLfloat);
		int t2b = nv * 2 * (int)sizeof(GLfloat);
		int total = vb + cb + tb + t2b;
		int off = 0;
		glBindBuffer(GL_ARRAY_BUFFER, merge_vbo);
		glBufferData(GL_ARRAY_BUFFER, total, NULL, GL_STREAM_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, off, vb, vertex_array);
		glVertexAttribPointer(OGL_APOS, 3, GL_FLOAT, GL_FALSE, 0, (const void *)(intptr_t)off);
		off += vb;
		glBufferSubData(GL_ARRAY_BUFFER, off, cb, color_array);
		glVertexAttribPointer(OGL_ACOLOR, 4, GL_FLOAT, GL_FALSE, 0, (const void *)(intptr_t)off);
		off += cb;
		glBufferSubData(GL_ARRAY_BUFFER, off, tb, texcoordbot_array);
		glVertexAttribPointer(OGL_ATEXCOORD, 2, GL_FLOAT, GL_FALSE, 0, (const void *)(intptr_t)off);
		off += tb;
		glBufferSubData(GL_ARRAY_BUFFER, off, t2b, texcoordovl_array);
		glVertexAttribPointer(OGL_ATEXCOORD2, 2, GL_FLOAT, GL_FALSE, 0, (const void *)(intptr_t)off);
	}
#else
	glVertexAttribPointer(OGL_APOS, 3, GL_FLOAT, GL_FALSE, 0, vertex_array);
	glVertexAttribPointer(OGL_ACOLOR, 4, GL_FLOAT, GL_FALSE, 0, color_array);
	glVertexAttribPointer(OGL_ATEXCOORD, 2, GL_FLOAT, GL_FALSE, 0, texcoordbot_array);
	glVertexAttribPointer(OGL_ATEXCOORD2, 2, GL_FLOAT, GL_FALSE, 0, texcoordovl_array);
#endif

#if defined(ANDROID) && defined(OGL_MERGE)
	ogl_log_metl154_diag(bmbot, bmovl, uvl_list, texcoordovl_array, nv, orient, super,
		prog, tex2_debug_mode);
	if (is_metl154_plain)
		ogl_log_metl154_state(metl154_screen_area,
			metl154_depth_enabled, metl154_blend_enabled, metl154_cull_enabled,
			metl154_depth_writemask, metl154_depth_func, metl154_front_face,
			metl154_cull_mode, metl154_color_mask, metl154_draw_fbo,
			metl154_force_cull_off, metl154_force_depth_off);
	if (is_metl154_plain)
		ogl_log_metl154_split(pointlist, nv);
#endif

	glDrawArrays(GL_TRIANGLE_FAN, 0, nv);

#if defined(ANDROID)
	if (is_metl154_plain) {
		if (metl154_force_depth_off) {
			glDepthMask(metl154_depth_writemask);
			if (metl154_depth_enabled)
				glEnable(GL_DEPTH_TEST);
		}
		if (metl154_force_cull_off)
			glEnable(GL_CULL_FACE);
		glPolygonOffset(0.0f, 0.0f);
		glDisable(GL_POLYGON_OFFSET_FILL);
		ogl_metl154_track_face(pointlist, nv, draw_order);
	} else {
		ogl_log_metl154_cover(super ? "mask" : "plain",
			piggy_game_bitmap_name(bmbot), piggy_game_bitmap_name(bmovl),
			pointlist, nv, draw_order);
	}
#endif

	glDisableVertexAttribArray(OGL_APOS);
	glDisableVertexAttribArray(OGL_ACOLOR);
	glDisableVertexAttribArray(OGL_ATEXCOORD);
	glDisableVertexAttribArray(OGL_ATEXCOORD2);
#ifdef ANDROID
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	gles3_shim_use_external(0);

	/* Debug texture label overlay for overlay/tmap2 bitmap */
	if (g_debug_tex_overlay_active
	    && g_debug_tex_label_count < DEBUG_TEX_MAX_LABELS && nv >= 3)
	{
		fix cx = 0, cy = 0, cz = 0;
		int i;
		for (i = 0; i < nv; i++) {
			cx += pointlist[i]->p3_vec.x / nv;
			cy += pointlist[i]->p3_vec.y / nv;
			cz += pointlist[i]->p3_vec.z / nv;
		}
		if (cz > F1_0 / 4) {
			fix sx_fix, sy_fix;
			int checkmuldiv(fix *r, fix a, fix b, fix c);
			if (checkmuldiv(&sx_fix, cx, Canv_w2, cz)
			    && checkmuldiv(&sy_fix, cy, Canv_h2, cz))
			{
				int sx = f2i(Canv_w2 + sx_fix);
				int sy = f2i(Canv_h2 - sy_fix) + 10;
				int sw = grd_curcanv->cv_bitmap.bm_w;
				int sh = grd_curcanv->cv_bitmap.bm_h;
				if (sx >= 0 && sx < sw && sy >= 0 && sy < sh) {
					struct debug_tex_label *lbl = &g_debug_tex_labels[g_debug_tex_label_count];
					lbl->sx = sx;
					lbl->sy = sy;
					lbl->is_hires = (bmovl->gltexture && bmovl->gltexture->is_png) ? 1 : 0;
					const char *bname = piggy_game_bitmap_name(bmovl);
					if (bname) {
						strncpy(lbl->name, bname, sizeof(lbl->name) - 1);
						lbl->name[sizeof(lbl->name) - 1] = '\0';
						g_debug_tex_label_count++;
					}
				}
			}
		}
	}
#else
	glUseProgram(0);
#endif
#endif
	r_tpolyc++;

	return 0;
}

#if defined(ANDROID) && defined(OGL_MERGE)
static bool ogl_clip_and_draw_metl154_merge(int nv, const g3s_point **pointlist,
	g3s_uvl *uvl_list, g3s_lrgb *light_rgb, grs_bitmap *bmbot,
	grs_bitmap *bmovl, int orient)
{
	g3s_point *clip_src[MAX_POINTS_IN_POLY], *clip_dest[MAX_POINTS_IN_POLY];
	const g3s_point *draw_points[MAX_POINTS_IN_POLY];
	g3s_uvl clipped_uvl[MAX_POINTS_IN_POLY];
	g3s_lrgb clipped_light[MAX_POINTS_IN_POLY];
	g3s_point **bufptr;
	g3s_codes cc;
	int clipped_nv = nv, i;
	bool result = 0;

	if (nv < 3 || nv > MAX_POINTS_IN_POLY)
		return ogl_draw_tmap_2_internal(nv, pointlist, uvl_list, light_rgb,
			bmbot, bmovl, orient);

	cc.uor = 0;
	cc.uand = 0xff;
	for (i = 0; i < nv; i++) {
		g3s_point *p = clip_src[i] = (g3s_point *)pointlist[i];

		cc.uand &= p->p3_codes;
		cc.uor |= p->p3_codes;
		p->p3_u = uvl_list[i].u;
		p->p3_v = uvl_list[i].v;
		p->p3_l = (light_rgb[i].r + light_rgb[i].g + light_rgb[i].b) / 3;
		p->p3_flags |= PF_UVS + PF_LS;
	}

	if (cc.uand)
		return 1;

	if (!cc.uor)
		return ogl_draw_tmap_2_internal(nv, pointlist, uvl_list, light_rgb,
			bmbot, bmovl, orient);

	bufptr = clip_polygon(clip_src, clip_dest, &clipped_nv, &cc);
	if (clipped_nv && !(cc.uor & CC_BEHIND) && !cc.uand) {
		for (i = 0; i < clipped_nv; i++) {
			g3s_point *p = bufptr[i];

			if (!(p->p3_flags & PF_PROJECTED))
				g3_project_point(p);

			if (p->p3_flags & PF_OVERFLOW)
				goto free_points;

			draw_points[i] = p;
			clipped_uvl[i].u = p->p3_u;
			clipped_uvl[i].v = p->p3_v;
			clipped_uvl[i].l = p->p3_l;
			clipped_light[i].r = p->p3_l;
			clipped_light[i].g = p->p3_l;
			clipped_light[i].b = p->p3_l;
		}

		result = ogl_draw_tmap_2_internal(clipped_nv, draw_points, clipped_uvl,
			clipped_light, bmbot, bmovl, orient);
	}

free_points:
	for (i = 0; i < clipped_nv; i++)
		if (bufptr[i]->p3_flags & PF_TEMP_POINT)
			free_temp_point(bufptr[i]);

	return result;
}
#endif

bool g3_draw_tmap_2(int nv, const g3s_point **pointlist, g3s_uvl *uvl_list, g3s_lrgb *light_rgb, grs_bitmap *bmbot, grs_bitmap *bmovl, int orient)
{
#if defined(ANDROID) && defined(OGL_MERGE)
	if (ogl_is_metl154_bitmap(bmovl)
		&& !(bmovl->bm_flags & BM_FLAG_SUPER_TRANSPARENT))
		return ogl_clip_and_draw_metl154_merge(nv, pointlist, uvl_list,
			light_rgb, bmbot, bmovl, orient);
#endif

	return ogl_draw_tmap_2_internal(nv, pointlist, uvl_list, light_rgb,
		bmbot, bmovl, orient);
}

/*
 * 2d Sprites (Fireaballs, powerups, explosions). NOT hostages
 */
bool g3_draw_bitmap_full(vms_vector *pos,fix width,fix height,grs_bitmap *bm,
	GLfloat r, GLfloat g, GLfloat b)
{
	vms_vector pv,v1;
	int i;
	GLfloat vertex_array[12], color_array[16], texcoord_array[8];

	r_bitmapc++;
	v1.z=0;
	
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	OGL_ENABLE(TEXTURE_2D);
	ogl_bindbmtex(bm);
	if (bm->gltexture == NULL)
		return 0;
	ogl_texwrap(bm->gltexture,GL_CLAMP_TO_EDGE);

	width = fixmul(width,Matrix_scale.x);
	height = fixmul(height,Matrix_scale.y);
	for (i=0;i<4;i++){
		vm_vec_sub(&v1,pos,&View_position);
		vm_vec_rotate(&pv,&v1,&View_matrix);
		switch (i){
			case 0:
				texcoord_array[i*2] = 0.0;
				texcoord_array[i*2+1] = 0.0;
				pv.x+=-width;
				pv.y+=height;
				break;
			case 1:
				texcoord_array[i*2] = bm->gltexture->u;
				texcoord_array[i*2+1] = 0.0;
				pv.x+=width;
				pv.y+=height;
				break;
			case 2:
				texcoord_array[i*2] = bm->gltexture->u;
				texcoord_array[i*2+1] = bm->gltexture->v;
				pv.x+=width;
				pv.y+=-height;
				break;
			case 3:
				texcoord_array[i*2] = 0.0;
				texcoord_array[i*2+1] = bm->gltexture->v;
				pv.x+=-width;
				pv.y+=-height;
				break;
		}

		color_array[i*4]    = r;
		color_array[i*4+1]  = g;
		color_array[i*4+2]  = b;
		color_array[i*4+3]  = (grd_curcanv->cv_fade_level >= GR_FADE_OFF)?1.0:(1.0 - (float)grd_curcanv->cv_fade_level / ((float)GR_FADE_LEVELS - 1.0));
		
		vertex_array[i*3]   = f2glf(pv.x);
		vertex_array[i*3+1] = f2glf(pv.y);
		vertex_array[i*3+2] = -f2glf(pv.z);
	}
	glVertexPointer(3, GL_FLOAT, 0, vertex_array);
	glColorPointer(4, GL_FLOAT, 0, color_array);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoord_array);  
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4); // Replaced GL_QUADS
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	return 0;
}

bool g3_draw_bitmap_colorwarp(vms_vector *pos,fix width,fix height,grs_bitmap *bm,
	   float r, float g, float b) 
{
	return g3_draw_bitmap_full(pos, width, height, bm, r, g, b); 
}

bool g3_draw_bitmap(vms_vector *pos,fix width,fix height,grs_bitmap *bm)
{
	return g3_draw_bitmap_full(pos, width, height, bm, 1.0, 1.0, 1.0); 
}

/*
 * Movies
 * Since this function will create a new texture each call, mipmapping can be very GPU intensive - so it has an optional setting for texture filtering.
 */
bool ogl_ubitblt_i(int dw,int dh,int dx,int dy, int sw, int sh, int sx, int sy, grs_bitmap * src, grs_bitmap * dest, int texfilt)
{
	GLfloat xo,yo,xs,ys,u1,v1;
	GLfloat color_array[] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
	GLfloat texcoord_array[] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	GLfloat vertex_array[] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	ogl_texture tex;
	r_ubitbltc++;

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	ogl_init_texture(&tex, sw, sh, OGL_FLAG_ALPHA);
	tex.prio = 0.0;
	tex.lw=src->bm_rowsize;

	u1=v1=0;
	
	dx+=dest->bm_x;
	dy+=dest->bm_y;
	xo=dx/(float)last_width;
	xs=dw/(float)last_width;
	yo=1.0-dy/(float)last_height;
	ys=dh/(float)last_height;
	
	OGL_ENABLE(TEXTURE_2D);
	
	ogl_pal=gr_current_pal;
	ogl_loadtexture(src->bm_data, sx, sy, &tex, src->bm_flags, 0, texfilt);
	ogl_pal=gr_palette;
	OGL_BINDTEXTURE(tex.handle);
	
	ogl_texwrap(&tex,GL_CLAMP_TO_EDGE);

	vertex_array[0] = xo;
	vertex_array[1] = yo;
	vertex_array[2] = xo+xs;
	vertex_array[3] = yo;
	vertex_array[4] = xo+xs;
	vertex_array[5] = yo-ys;
	vertex_array[6] = xo;
	vertex_array[7] = yo-ys;

	texcoord_array[0] = u1;
	texcoord_array[1] = v1;
	texcoord_array[2] = tex.u;
	texcoord_array[3] = v1;
	texcoord_array[4] = tex.u;
	texcoord_array[5] = tex.v;
	texcoord_array[6] = u1;
	texcoord_array[7] = tex.v;

	glVertexPointer(2, GL_FLOAT, 0, vertex_array);
	glColorPointer(4, GL_FLOAT, 0, color_array);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoord_array);  
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);//replaced GL_QUADS

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	ogl_freetexture(&tex);
	return 0;
}

bool ogl_ubitblt(int w,int h,int dx,int dy, int sx, int sy, grs_bitmap * src, grs_bitmap * dest){
	return ogl_ubitblt_i(w,h,dx,dy,w,h,sx,sy,src,dest,0);
}

/*
 * set depth testing on or off
 */
void ogl_toggle_depth_test(int enable)
{
	if (enable)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);
}

/* 
 * set blending function
 */
#ifdef ANDROID
static int ogl_last_blend_mode = -1; /* blend func cache; reset in ogl_start_frame */
#endif
void ogl_set_blending()
{
#ifdef ANDROID
	/* android port: skip redundant glBlendFunc calls */
	int cur = grd_curcanv->cv_blend_func;
	if (cur == ogl_last_blend_mode) return;
	ogl_last_blend_mode = cur;
#endif
	switch ( grd_curcanv->cv_blend_func )
	{
		case GR_BLEND_ADDITIVE_A:
			glBlendFunc( GL_SRC_ALPHA, GL_ONE );
			break;
		case GR_BLEND_ADDITIVE_C:
			glBlendFunc( GL_ONE, GL_ONE );
			break;
		case GR_BLEND_NORMAL:
		default:
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			break;
	}
}

GLubyte *pixels = NULL;

void ogl_start_frame(void){
	r_polyc=0;r_tpolyc=0;r_bitmapc=0;r_ubitbltc=0;r_upixelc=0;
#ifdef ANDROID
	int msaa_color_clear = 0;
	r_texbinds=0;r_texbind_reuse=0;ogl_last_bound_tex=0;
	r_shader_switches=0;r_mask_draws=0;
	g_ogl_render_context = 1; /* 3D world */
	if (g_metl154_experiment_pending_apply) {
		int invalidated;

		g_metl154_experiment_pending_apply = 0;
		__sync_synchronize();
		invalidated = ogl_reload_metl154_textures();
		ogl_last_bound_tex = 0;
		debug_log(DLOG_TEXTURE,
			"[metl154exp] apply: mode=%d(%s) invalidated=%d",
			(int)g_metl154_experiment_mode,
			ogl_metl154_experiment_name((int)g_metl154_experiment_mode),
			invalidated);
		if (invalidated)
			con_printf(CON_DEBUG, "metl154 experiment: %s invalidated %d texture(s)",
				ogl_metl154_experiment_name((int)g_metl154_experiment_mode),
				invalidated);
	}
	if (g_aniso_pending_apply) {
		g_aniso_pending_apply = 0;
		__sync_synchronize(); /* ensure we read values written before flag */
		if (ogl_aniso_level > 0) {
			/* AF on: generate mipmaps in-place for textures that lack them
			 * (needed for AF to have any effect). Skip font textures
			 * (NOCOLOR) -- they never need mipmaps */
			int upgraded = 0;
			for (int i = 0; i < OGL_TEXTURE_LIST_SIZE; i++) {
				if (ogl_texture_list[i].handle > 0 && !ogl_texture_list[i].has_mipmaps
				    && !(ogl_texture_list[i].flags & OGL_FLAG_NOCOLOR)) {
					glBindTexture(GL_TEXTURE_2D, ogl_texture_list[i].handle);
					glGenerateMipmap(GL_TEXTURE_2D);
					ogl_texture_list[i].has_mipmaps = 1;
					upgraded++;
				}
			}
			ogl_last_bound_tex = 0;
			if (upgraded)
				con_printf(CON_DEBUG, "anisotropy: generated mipmaps for %d textures", upgraded);
		}
		/* AF off: mipmaps are harmless, just update the aniso parameter.
		 * ogl_bindbmtex will use the correct filter at bind time */
		ogl_apply_anisotropy_all();
	}
	if (g_texfilt_pending_apply) {
		g_texfilt_pending_apply = 0;
		__sync_synchronize(); /* ensure we read values written before flag */
		GameCfg.TexFilt = g_texfilt_level;
		/* Update filter parameters in-place instead of flushing textures.
		 * Deleting and recreating textures causes driver-level corruption
		 * on some mobile GPUs (Adreno, Mali) due to stale cached state */
		int updated = 0;
		for (int i = 0; i < OGL_TEXTURE_LIST_SIZE; i++) {
			if (ogl_texture_list[i].handle <= 0)
				continue;
			if (ogl_texture_list[i].flags & OGL_FLAG_NOCOLOR)
				continue; /* font: filter set at bind time */
			glBindTexture(GL_TEXTURE_2D, ogl_texture_list[i].handle);
			if (GameCfg.TexFilt > 0) {
				if (!ogl_texture_list[i].has_mipmaps) {
					glGenerateMipmap(GL_TEXTURE_2D);
					ogl_texture_list[i].has_mipmaps = 1;
				}
				GLenum min_f = GameCfg.TexFilt >= 2
					? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST;
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_f);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			} else {
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			}
			updated++;
		}
		ogl_last_bound_tex = 0;
		if (updated)
			con_printf(CON_DEBUG, "texfilt: updated %d textures in-place (TexFilt=%d)", updated, GameCfg.TexFilt);
	}
	g_texfilt_level = GameCfg.TexFilt;
	if (g_msaa_pending_apply) {
		g_msaa_pending_apply = 0;
		ogl_msaa_destroy_fbo(); /* recreate on next check below */
	}
	/* Bind MSAA FBO if enabled; create/resize as needed.
	 * Only the outermost active 3D pass binds here. ogl_end_frame balances
	 * the depth counter, so later cockpit subviews in the same game frame can
	 * rebind safely before the final resolve in gr_flip. */
	if (ogl_msaa_samples > 0 && g_msaa_frame_depth == 0) {
		int w = grd_curscreen->sc_w, h = grd_curscreen->sc_h;
		if (!ogl_msaa_fbo || ogl_msaa_w != w || ogl_msaa_h != h)
			ogl_msaa_create_fbo(ogl_msaa_samples, w, h);
		if (ogl_msaa_fbo) {
			msaa_color_clear = !g_msaa_fbo_bound;
			glBindFramebuffer(GL_FRAMEBUFFER, ogl_msaa_fbo);
			g_msaa_fbo_bound = 1;
		}
	}
	g_msaa_frame_depth++;
	/* GPU timer: read oldest completed query, begin new query */
	if (ogl_gpu_timer_available) {
		if (!ogl_gpu_queries[0])
			glGenQueries(GPU_QUERY_COUNT, ogl_gpu_queries);
		/* Try to read the oldest completed query */
		if (ogl_gpu_query_count > 0) {
			int read_idx = (ogl_gpu_query_write - ogl_gpu_query_count + GPU_QUERY_COUNT) % GPU_QUERY_COUNT;
			GLuint avail = 0;
			glGetQueryObjectuiv(ogl_gpu_queries[read_idx],
			    GL_QUERY_RESULT_AVAILABLE, &avail);
			if (avail) {
				GLuint ns = 0;
				glGetQueryObjectuiv(ogl_gpu_queries[read_idx],
				    GL_QUERY_RESULT, &ns);
				GLint disjoint = 0;
				glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);
				if (!disjoint)
					g_gpu_time_us = (int)(ns / 1000);
				/* else: keep last valid reading */
				ogl_gpu_query_count--;
			} else if (ogl_gpu_query_count >= GPU_QUERY_COUNT - 1) {
				/* All slots full and oldest still not ready: blocking read
				 * to avoid stalling the pipeline */
				GLuint ns = 0;
				glGetQueryObjectuiv(ogl_gpu_queries[read_idx],
				    GL_QUERY_RESULT, &ns);
				GLint disjoint = 0;
				glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);
				if (!disjoint)
					g_gpu_time_us = (int)(ns / 1000);
				ogl_gpu_query_count--;
			}
		}
		/* Begin new query if we have a free slot */
		if (ogl_gpu_query_count < GPU_QUERY_COUNT) {
			glBeginQuery(GL_TIME_ELAPSED_EXT, ogl_gpu_queries[ogl_gpu_query_write]);
			ogl_gpu_query_in_flight = 1;
		}
	}
#endif

	OGL_VIEWPORT(grd_curcanv->cv_bitmap.bm_x,grd_curcanv->cv_bitmap.bm_y,Canvas_width,Canvas_height);
#ifdef ANDROID
	/* Android compositor uses framebuffer alpha for surface blending;
	 * clear to alpha=1 to prevent the activity background showing through */
	glClearColor(0.0, 0.0, 0.0, 1.0);
#else
	glClearColor(0.0, 0.0, 0.0, 0.0);
#endif

	glLineWidth(linedotscale);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#ifdef ANDROID
	ogl_last_blend_mode = -1; /* invalidate cache; ogl_do_palfx changes blend directly */
#endif

#if defined(OGLES) || !defined(OGL_MERGE)
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GEQUAL,0.02);
#endif

	if (!GameCfg.ClassicDepth || (Game_mode & GM_MULTI))
		glEnable(GL_DEPTH_TEST);
	/* glDepthFunc(GL_LEQUAL) moved to ogl_init_state -- never changes */

	/* Clear depth every 3D pass. Clear color only on the first MSAA-backed
	 * pass of the frame, otherwise cockpit missile / rear-view subrenders wipe
	 * the already-rendered main scene in the shared MSAA FBO. */
	glClear(GL_DEPTH_BUFFER_BIT |
	        (msaa_color_clear ? GL_COLOR_BUFFER_BIT : 0));

	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CW);

#ifdef OGL_MERGE
	{
		/* Build perspective matrix on CPU to avoid GLES 1 matrix stack calls
		 * that generate GL_INVALID_OPERATION on Android (GLES 1/2 mixing) */
		double n = 0.1, f = 5000.0;
		double ymax = n * tan(90.0 * M_PI / 360.0);
		double xmax = ymax; /* aspect = 1 */
		GLfloat mat[16];
		memset(mat, 0, sizeof(mat));
		mat[0]  = (GLfloat)(n / xmax);
		mat[5]  = (GLfloat)(n / ymax);
		mat[10] = (GLfloat)(-(f + n) / (f - n));
		mat[11] = -1.0f;
		mat[14] = (GLfloat)(-2.0 * f * n / (f - n));
		ogl_prog_set_matrix(mat);
	}
#endif
#if defined(OGLES) || !defined(OGL_MERGE)
	/* Set up fixed-function / shim projection matrix.
	 * On OGLES with OGL_MERGE, the shim matrix is still needed for
	 * single-texture draws (g3_draw_tmap) that use the shim shader. */
	glShadeModel(GL_SMOOTH);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();//clear matrix
#ifdef OGLES
	perspective(90.0,1.0,0.1,5000.0);   
#else
	gluPerspective(90.0,1.0,0.1,5000.0);
#endif
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();//clear matrix
#endif
}

void ogl_end_frame(void){
	OGL_VIEWPORT(0,0,grd_curscreen->sc_w,grd_curscreen->sc_h);
#ifdef ANDROID
	if (g_msaa_frame_depth > 0)
		g_msaa_frame_depth--;
	{
		extern volatile int g_blit_y_offset;
		int direct_off = android_get_keyboard_y_offset(grd_curscreen->sc_canvas.cv_bitmap.bm_h);
		g_blit_y_offset = direct_off;
	}
#endif
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();//clear matrix
#ifdef OGLES
	glOrthof(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
#else
	glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
#endif
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();//clear matrix
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

#ifdef OGL_MERGE
	ogl_prog_set_matrix(ogl_mat_ortho);
#endif
#ifdef ANDROID
	/* Drain GL errors from GLES 1/2 API mixing on Android */
	while (glGetError() != GL_NO_ERROR) {}
#endif
}

void gr_flip(void)
{
#ifdef ANDROID
	{
		static int s_flip_count = 0;
		s_flip_count++;
		if (s_flip_count <= 5 || (s_flip_count % 60) == 0)
			crash_breadcrumb_v("gr_flip #%d", s_flip_count);
	}
#endif
	if (GameArg.DbgRenderStats)
		ogl_texture_stats();

	ogl_do_palfx();
	/* android port: end GPU timer query before resolve/swap */
#ifdef ANDROID
	if (ogl_gpu_query_in_flight) {
		glEndQuery(GL_TIME_ELAPSED_EXT);
		ogl_gpu_query_write = (ogl_gpu_query_write + 1) % GPU_QUERY_COUNT;
		ogl_gpu_query_count++;
		ogl_gpu_query_in_flight = 0;
	}
#endif
	/* android port: resolve MSAA FBO to default framebuffer before readback.
	 * Frame depth is balanced in ogl_start_frame/ogl_end_frame for every 3D
	 * render pass, including cockpit subviews. Resolve only after all passes
	 * for the frame have unwound back to depth 0. */
#ifdef ANDROID
	if (g_msaa_fbo_bound && g_msaa_frame_depth == 0) {
		int w = grd_curscreen->sc_w, h = grd_curscreen->sc_h;
		glBindFramebuffer(GL_READ_FRAMEBUFFER, ogl_msaa_fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(0, 0, w, h, 0, 0, w, h,
		    GL_COLOR_BUFFER_BIT, GL_NEAREST);
		{
			GLenum err = glGetError();
			if (err != GL_NO_ERROR)
				__android_log_print(ANDROID_LOG_ERROR, "DXX",
				    "MSAA resolve error: 0x%x", err);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		g_msaa_fbo_bound = 0;
	}
#endif
	/* android port: sample framebuffer for introspection (center pixel + 4x4 grid average) */
#ifdef ANDROID
	{
		extern volatile int g_fb_sample_r, g_fb_sample_g, g_fb_sample_b, g_fb_sample_a;
		extern volatile int g_fb_avg_r, g_fb_avg_g, g_fb_avg_b, g_fb_avg_a;
		int w = grd_curscreen->sc_w, h = grd_curscreen->sc_h;
		if (w > 0 && h > 0) {
			unsigned char rgba[4] = {0};
			glReadPixels(w/2, h/2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
			g_fb_sample_r = rgba[0];
			g_fb_sample_g = rgba[1];
			g_fb_sample_b = rgba[2];
			g_fb_sample_a = rgba[3];
			/* 4x4 grid average across the framebuffer */
			int sr = 0, sg = 0, sb = 0, sa = 0, n = 0;
			for (int gy = 1; gy <= 4; gy++) {
				for (int gx = 1; gx <= 4; gx++) {
					int px = w * gx / 5, py = h * gy / 5;
					glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
					sr += rgba[0]; sg += rgba[1]; sb += rgba[2]; sa += rgba[3];
					n++;
				}
			}
			g_fb_avg_r = sr / n; g_fb_avg_g = sg / n;
			g_fb_avg_b = sb / n; g_fb_avg_a = sa / n;
		}
	}
#endif
	ogl_swap_buffers_internal();
	glClear(GL_COLOR_BUFFER_BIT);
#ifdef ANDROID
	g_ogl_render_context = 0; /* reset to menu context for next frame */
	/* Apply keyboard viewport offset for the next frame (menus, HUD, etc.).
	 * ogl_start_frame/ogl_end_frame handle this for 3D rendering, but menus
	 * render without those calls so the offset must be applied here. */
	{
		extern volatile int g_blit_y_offset;
		int koff = android_get_keyboard_y_offset(grd_curscreen->sc_canvas.cv_bitmap.bm_h);
		int h = grd_curscreen->sc_h;
		int w = grd_curscreen->sc_w;
		glViewport(0, grd_curscreen->sc_canvas.cv_bitmap.bm_h - h + koff, w, h);
		last_width = w;
		last_height = h;
		last_kb_off = koff;
		g_blit_y_offset = koff;
	}
#endif
}

int tex_format_supported(int iformat,int format)
{
#ifndef OGLES
	switch (iformat){
		case GL_INTENSITY4:
			if (!GameArg.DbgGlIntensity4Ok) return 0; break;
		case GL_LUMINANCE4_ALPHA4:
			if (!GameArg.DbgGlLuminance4Alpha4Ok) return 0; break;
		case GL_RGBA2:
			if (!GameArg.DbgGlRGBA2Ok) return 0; break;
	}
	if (GameArg.DbgGlGetTexLevelParamOk){
		GLint internalFormat;
		glTexImage2D(GL_PROXY_TEXTURE_2D, 0, iformat, 64, 64, 0,
				format, GL_UNSIGNED_BYTE, texbuf);//NULL?
		glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0,
				GL_TEXTURE_INTERNAL_FORMAT,
				&internalFormat);
		return (internalFormat==iformat);
	}else
#endif
		return 1;
}

//little hack to find the nearest bigger power of 2 for a given number
int pow2ize(int x){
	int i;
	for (i=2; i<x; i*=2) {}; //corrected by MD2211: was previously limited to 4096
	return i;
}

GLubyte *texbuf = NULL;

// Allocate the pixel buffers 'pixels' and 'texbuf' based on current screen resolution
void ogl_init_pixel_buffers(int w, int h)
{
	w = pow2ize(w);	// convert to OpenGL texture size
	h = pow2ize(h);

	if (pixels)
		d_free(pixels);
	pixels = d_malloc(w*h*4);

	if (texbuf)
		d_free(texbuf);
	texbuf = d_malloc(max(w, ogl_max_texture_size)*max(h, ogl_max_texture_size)*4);

	if ((pixels == NULL) || (texbuf == NULL))
		Error("Not enough memory for current resolution");
}

void ogl_close_pixel_buffers(void)
{
	d_free(pixels);
	d_free(texbuf);
}

void ogl_filltexbuf(unsigned char *data, GLubyte *texp, int truewidth, int width, int height, int dxo, int dyo, int twidth, int theight, int type, int bm_flags, int data_format)
{
	int x,y,c,i;

	if (width > ogl_max_texture_size || height > ogl_max_texture_size)
		Error("Texture is too big: %ix%i (limit %i)", width, height, ogl_max_texture_size);

	if (data_format) { // true color bitmap?
		if (width == truewidth && width == twidth) {
			memcpy(texp, data, data_format * width * height);
		} else {
			for (y = 0; y < height; y++) {
				memcpy(texp + twidth * y * data_format,
					data + truewidth * y * data_format,
					width * data_format);
				if (width < twidth) { // repeat last pixel, clear rest
					memcpy(texp + (y * twidth + width) * data_format,
						data + (y * truewidth + width - 1) * data_format,
						data_format);
					if (width + 1 < twidth)
						memset(texp + (y * twidth + width + 1) * data_format,
							0, (twidth - width - 1) * data_format);
				}
			}
		}
		if (height < theight) { // repeat last row, clear rest
			memcpy(texp + height * twidth * data_format,
				data + (height - 1) * truewidth * data_format,
				width * data_format);
			memset(texp + (height * twidth + width) * data_format,
				0, (twidth - width) * data_format);
			if (height + 1 < theight)
				memset(texp + (height + 1) * twidth * data_format,
					0, (theight - height - 1) * twidth * data_format);
		}
		return;
	}

	i=0;
	for (y=0;y<theight;y++)
	{
		i=dxo+truewidth*(y+dyo);
		for (x=0;x<twidth;x++)
		{
			if (x<width && y<height)
			{
				c = data[i++];
			}
			else if (x == width && y < height) // end of bitmap reached - fill this pixel with last color to make a clean border when filtering this texture
			{
				c = data[(width*(y+1))-1];
			}
			else if (y == height && x < width) // end of bitmap reached - fill this row with color or last row to make a clean border when filtering this texture
			{
				c = data[(width*(height-1))+x];
			}
			else
			{
				c = 256; // fill the pad space with transparency (or blackness)
			}

			if (c == 254 && (bm_flags & BM_FLAG_SUPER_TRANSPARENT))
			{
				switch (type)
				{
					case GL_LUMINANCE_ALPHA:
						(*(texp++)) = 255;
						(*(texp++)) = 0;
						break;
					case GL_RGBA:
						(*(texp++)) = 255;
						(*(texp++)) = 255;
						(*(texp++)) = 255;
						(*(texp++)) = 0; // transparent pixel
						break;
#ifndef OGLES
					case GL_COLOR_INDEX:
						(*(texp++)) = c;
						break;
#endif
					default:
						Error("ogl_filltexbuf unhandled super-transparent texformat\n");
						break;
				}
			}
			else if ((c == 255 && (bm_flags & BM_FLAG_TRANSPARENT)) || c == 256)
			{
				switch (type)
				{
					case GL_LUMINANCE:
						(*(texp++))=0;
						break;
					case GL_LUMINANCE_ALPHA:
						(*(texp++))=0;
						(*(texp++))=0;
						break;
					case GL_RGB:
						(*(texp++)) = 0;
						(*(texp++)) = 0;
						(*(texp++)) = 0;
						break;
					case GL_RGBA:
						(*(texp++))=0;
						(*(texp++))=0;
						(*(texp++))=0;
						(*(texp++))=0;//transparent pixel
						break;
#ifndef OGLES
					case GL_COLOR_INDEX:
						(*(texp++)) = c;
						break;
#endif
					default:
						Error("ogl_filltexbuf unknown texformat\n");
						break;
				}
			}
			else
			{
				switch (type)
				{
					case GL_LUMINANCE://these could prolly be done to make the intensity based upon the intensity of the resulting color, but its not needed for anything (yet?) so no point. :)
						(*(texp++))=255;
						break;
					case GL_LUMINANCE_ALPHA:
						(*(texp++))=255;
						(*(texp++))=255;
						break;
					case GL_RGB:
						(*(texp++)) = ogl_pal[c * 3] * 4;
						(*(texp++)) = ogl_pal[c * 3 + 1] * 4;
						(*(texp++)) = ogl_pal[c * 3 + 2] * 4;
						break;
					case GL_RGBA:
						(*(texp++))=ogl_pal[c*3]*4;
						(*(texp++))=ogl_pal[c*3+1]*4;
						(*(texp++))=ogl_pal[c*3+2]*4;
						(*(texp++))=255;//not transparent
						break;
#ifndef OGLES
					case GL_COLOR_INDEX:
						(*(texp++)) = c;
						break;
#endif
					default:
						Error("ogl_filltexbuf unknown texformat\n");
						break;
				}
			}
		}
	}
}

int tex_format_verify(ogl_texture *tex){
	while (!tex_format_supported(tex->internalformat,tex->format)){
		glmprintf((0,"tex format %x not supported",tex->internalformat));
		switch (tex->internalformat){
#ifdef OGLES
			case GL_RGB:
				tex->format=GL_RGB;
				break;
			case GL_RGBA:
				tex->format=GL_RGBA;
#else
			case GL_INTENSITY4:
				if (GameArg.DbgGlLuminance4Alpha4Ok){
					tex->internalformat=GL_LUMINANCE4_ALPHA4;
					tex->format=GL_LUMINANCE_ALPHA;
					break;
				}//note how it will fall through here if the statement is false
			case GL_LUMINANCE4_ALPHA4:
				if (GameArg.DbgGlRGBA2Ok){
					tex->internalformat=GL_RGBA2;
					tex->format=GL_RGBA;
					break;
				}//note how it will fall through here if the statement is false
			case GL_RGBA2:
#if defined(__APPLE__) && defined(__MACH__)
			case GL_RGB8:	// Quartz doesn't support RGB only
#endif
				tex->internalformat = ogl_rgba_internalformat;
				tex->format=GL_RGBA;
				break;
#endif // OGLES
			default:
				glmprintf((0,"...no tex format to fall back on\n"));
				return 1;
		}
		glmprintf((0,"...falling back to %x\n",tex->internalformat));
	}
	return 0;
}

void tex_set_size1(ogl_texture *tex,int dbits,int bits,int w, int h){
	int u;
	if (tex->tw!=w || tex->th!=h){
		u=(tex->w/(float)tex->tw*w) * (tex->h/(float)tex->th*h);
		glmprintf((0,"shrunken texture?\n"));
	}else
		u=tex->w*tex->h;
	if (bits<=0){//the beta nvidia GLX server. doesn't ever return any bit sizes, so just use some assumptions.
		tex->bytes=((float)w*h*dbits)/8.0;
		tex->bytesu=((float)u*dbits)/8.0;
	}else{
		tex->bytes=((float)w*h*bits)/8.0;
		tex->bytesu=((float)u*bits)/8.0;
	}
	glmprintf((0,"tex_set_size1: %ix%i, %ib(%i) %iB\n",w,h,bits,dbits,tex->bytes));
}

void tex_set_size(ogl_texture *tex){
	GLint w,h;
	int bi=16,a=0;
#ifndef OGLES
	if (GameArg.DbgGlGetTexLevelParamOk){
		GLint t;
		glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&w);
		glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&h);
		glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_LUMINANCE_SIZE,&t);a+=t;
		glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTENSITY_SIZE,&t);a+=t;
		glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_RED_SIZE,&t);a+=t;
		glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_GREEN_SIZE,&t);a+=t;
		glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_BLUE_SIZE,&t);a+=t;
		glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_ALPHA_SIZE,&t);a+=t;
	}
	else
#endif
	{
		w=tex->tw;
		h=tex->th;
	}
	switch (tex->format){
		case GL_LUMINANCE:
			bi=8;
			break;
		case GL_LUMINANCE_ALPHA:
			bi=8;
			break;
		case GL_RGB:
		case GL_RGBA:
			bi=16;
			break;
#ifndef OGLES
		case GL_COLOR_INDEX:
			bi = 8;
			break;
#endif
		default:
			Error("tex_set_size unknown texformat\n");
			break;
	}
	tex_set_size1(tex,bi,a,w,h);
}

//loads a palettized bitmap into a ogl RGBA texture.
//Sizes and pads dimensions to multiples of 2 if necessary.
//In theory this could be a problem for repeating textures, but all real
//textures (not sprites, etc) in descent are 64x64, so we are ok.
//stores OpenGL textured id in *texid and u/v values required to get only the real data in *u/*v
int ogl_loadtexture (unsigned char *data, int dxo, int dyo, ogl_texture *tex, int bm_flags, int data_format, int texfilt)
{
	GLubyte	*bufP = texbuf;
	tex->tw = pow2ize (tex->w);
	tex->th = pow2ize (tex->h);//calculate smallest texture size that can accomodate us (must be multiples of 2)

	/* Skip textures that exceed GL_MAX_TEXTURE_SIZE to avoid GL errors
	 * or hangs (e.g. animated sprite strips can be very tall) */
	if (tex->tw > ogl_max_texture_size || tex->th > ogl_max_texture_size) {
		con_printf(CON_URGENT, "Skipping oversized texture: %ix%i (pow2: %ix%i, limit %i)\n",
		           tex->w, tex->h, tex->tw, tex->th, ogl_max_texture_size);
#ifdef ANDROID
		__android_log_print(ANDROID_LOG_WARN, "DXX",
		    "Skipping oversized texture: %ix%i (pow2: %ix%i, limit %i)",
		    tex->w, tex->h, tex->tw, tex->th, ogl_max_texture_size);
#endif
		return 1;
	}

	//calculate u/v values that would make the resulting texture correctly sized
	tex->u = (float) ((double) tex->w / (double) tex->tw);
	tex->v = (float) ((double) tex->h / (double) tex->th);

	if (data) {
		if (bm_flags >= 0) {
			ogl_filltexbuf (data, texbuf, tex->lw, tex->w, tex->h, dxo, dyo, tex->tw, tex->th, 
								 tex->format, bm_flags, data_format);
#ifdef ANDROID
			/* On OGLES, NOCOLOR font textures use GL_RGBA instead of
			 * GL_LUMINANCE. Palette lookup produces near-white (252,252,252)
			 * due to 6-bit to 8-bit scaling (*4). Force pure white so
			 * vertex color tinting (yellow labels, green HUD) is exact */
			if ((tex->flags & OGL_FLAG_NOCOLOR) && tex->format == GL_RGBA) {
				int npixels = tex->tw * tex->th;
				GLubyte *p = texbuf;
				for (int pi = 0; pi < npixels; pi++, p += 4)
					if (p[3] > 0) { p[0] = p[1] = p[2] = 255; }
			}
#endif
		} else {
			if (!dxo && !dyo && (tex->w == tex->tw) && (tex->h == tex->th))
				bufP = data;
			else {
				int h, w, tw;
				
				h = tex->lw / tex->w;
				w = (tex->w - dxo) * h;
				data += tex->lw * dyo + h * dxo;
				bufP = texbuf;
				tw = tex->tw * h;
				h = tw - w;
				for (; dyo < tex->h; dyo++, data += tex->lw) {
					memcpy (bufP, data, w);
					bufP += w;
					memset (bufP, 0, h);
					bufP += h;
				}
				memset (bufP, 0, tex->th * tw - (bufP - texbuf));
				bufP = texbuf;
			}
		}
	}
	// Generate OpenGL texture IDs.
	glGenTextures (1, &tex->handle);
#ifndef OGLES
	//set priority
	glPrioritizeTextures (1, &tex->handle, &tex->prio);
#endif
	// Give our data to OpenGL.
	OGL_BINDTEXTURE(tex->handle);
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (texfilt
#ifdef ANDROID
		/* android port: never mipmap font textures (OGL_FLAG_NOCOLOR).
		 * Font atlases are sparse RGBA with thin glyph strokes --
		 * mipmapping averages them with surrounding transparent pixels,
		 * destroying alpha at lower mip levels and making text invisible */
		&& !(tex->flags & OGL_FLAG_NOCOLOR)
#endif
	)
	{
#ifdef OGLES
#ifndef ANDROID
		// in OpenGL ES 1.1 the mipmaps are automatically generated by a parameter
		glTexParameteri (GL_TEXTURE_2D, GL_GENERATE_MIPMAP, texfilt ? GL_TRUE : GL_FALSE);
#endif
#endif
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (texfilt>=2?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR_MIPMAP_NEAREST));
#ifndef ANDROID
#ifndef OGLES
		if (texfilt >= 3 && ogl_maxanisotropy > 1.0)
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, ogl_maxanisotropy);
#endif
#endif
	}
	else
	{
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}

#ifndef OGLES // see comment above
	if (texfilt)
	{
		gluBuild2DMipmaps (
				GL_TEXTURE_2D, tex->internalformat, 
				tex->tw, tex->th, tex->format, 
				GL_UNSIGNED_BYTE, 
				bufP);
	}
	else
#endif
	{
		glTexImage2D (
			GL_TEXTURE_2D, 0, tex->internalformat,
			tex->tw, tex->th, 0, tex->format, // RGBA textures.
			GL_UNSIGNED_BYTE, // imageData is a GLubyte pointer.
			bufP);
#ifdef ANDROID
		if (texfilt && !(tex->flags & OGL_FLAG_NOCOLOR)) {
			glGenerateMipmap(GL_TEXTURE_2D);
			tex->has_mipmaps = 1;
			/* Set AF after mipmaps exist -- Adreno ignores AF on textures
			 * without a complete mipmap chain */
			if (ogl_aniso_level > 1 && ogl_maxanisotropy > 1.0)
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
					(GLfloat)(ogl_aniso_level < ogl_maxanisotropy ? ogl_aniso_level : (int)ogl_maxanisotropy));
		}
#endif
	}

	tex_set_size (tex);
	r_texcount++;
	return 0;
}

#ifdef ANDROID
static void ogl_apply_nomip_filter(ogl_texture *tex, int texfilt)
{
	if (!tex || tex->handle <= 0)
		return;
	OGL_BINDTEXTURE(tex->handle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	if (texfilt > 0) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}
	tex->has_mipmaps = 0;
}
#endif

unsigned char decodebuf[1024*1024];

#ifdef OGL_MERGE
void ogl_loadpngmask(png_data *pdata, grs_bitmap *bm, int texfilt)
{
	unsigned char *mask;
	int size = pdata->width * pdata->height;
	unsigned char *buf = pdata->data;

	if (bm->gltexture_mask == NULL)
		ogl_init_texture(bm->gltexture_mask = ogl_get_free_texture(), pdata->width, pdata->height, OGL_FLAG_ALPHA);

	MALLOC(mask, unsigned char, size);
	if (pdata->paletted)
		for (int i = 0; i < size; i++)
			mask[i] = buf[i] == 254 ? 255 : 0;
	else if (pdata->channels == 4) // d2x-xl hack: #785880 is supertransparency
		for (int i = 0; i < size; i++)
			mask[i] = buf[i * 4] == 120 && buf[i * 4 + 1] == 88 && buf[i * 4 + 2] == 128 ? 255 : 0;
	else
		for (int i = 0; i < size; i++)
			mask[i] = buf[i * 3] == 120 && buf[i * 3 + 1] == 88 && buf[i * 3 + 2] == 128 ? 255 : 0;
	ogl_loadtexture(mask, 0, 0, bm->gltexture_mask, BM_FLAG_TRANSPARENT, 0, texfilt);
	bm->gltexture_mask->is_png = 1;
	d_free(mask);
}

#ifdef ANDROID
/* Load pre-generated super-transparent mask from DXA texture pack.
 * The mask is a PNG with alpha=0 for super-transparent pixels. */
static void ogl_load_dxa_mask(const char *bitmapname, grs_bitmap *bm, int texfilt)
{
	char maskname[256];
	png_data mdata;
	int loaded = 0;

	sprintf(maskname, "%s_mask.png", bitmapname);
	loaded = read_png(maskname, &mdata);
	if (!loaded) {
		debug_log(DLOG_TEXTURE, "Mask not found: %s", maskname);
		return;
	}
	if (mdata.depth == 8) {
		int size = mdata.width * mdata.height;
		int ch = mdata.paletted ? 1 : mdata.channels;
		unsigned char *mask;

		MALLOC(mask, unsigned char, size);
		/* Convert to single-byte mask matching ogl_loadpngmask convention:
		 * 255 where super-transparent, 0 elsewhere.  Upload via palette
		 * path with BM_FLAG_TRANSPARENT so 255->alpha=0, 0->alpha=1.
		 * Mask PNGs have white=keep, black=super-transparent, so invert */
		for (int i = 0; i < size; i++)
			mask[i] = mdata.data[i * ch] > 128 ? 0 : 255;

		if (bm->gltexture_mask == NULL)
			ogl_init_texture(bm->gltexture_mask = ogl_get_free_texture(),
				mdata.width, mdata.height, OGL_FLAG_ALPHA);
		ogl_loadtexture(mask, 0, 0, bm->gltexture_mask, BM_FLAG_TRANSPARENT, 0, texfilt);
		bm->gltexture_mask->is_png = 1;
		d_free(mask);

		debug_log(DLOG_TEXTURE, "Loaded mask: %s %dx%d ch=%d",
			maskname, mdata.width, mdata.height, ch);
	} else {
		debug_log(DLOG_TEXTURE, "Mask format unsupported: %s depth=%d color=%d",
			maskname, mdata.depth, mdata.color);
	}
	free(mdata.data);
	if (mdata.palette)
		free(mdata.palette);
}
#endif /* ANDROID */
#endif /* OGL_MERGE */

void ogl_loadbmtexture_f(grs_bitmap *bm, int texfilt)
{
	unsigned char *buf;
	const char *bitmapname = piggy_game_bitmap_name(bm);

#ifdef ANDROID
	/* AF requires mipmap filtering to have any effect on real hardware.
	 * If AF is on but texfilt is too low, upgrade to trilinear */
	if (ogl_aniso_level > 0 && texfilt < 2)
		texfilt = 2;
	int metl154_experiment = ogl_is_metl154_bitmap(bm)
		? (int)g_metl154_experiment_mode
		: METL154_EXPERIMENT_DEFAULT;
	int metl154_disable_mips = ogl_metl154_disable_mips(metl154_experiment);
	int metl154_force_rgba = ogl_metl154_force_rgba(metl154_experiment);
	int metl154_force_stock = ogl_metl154_force_stock(metl154_experiment);
	int load_texfilt = metl154_disable_mips ? 0 : texfilt;
	if (bitmapname && !d_stricmp(bitmapname, "metl154"))
		debug_log(DLOG_TEXTURE,
			"[metl154exp] request: mode=%d(%s) texfilt=%d load_texfilt=%d no_mips=%d force_rgba=%d force_stock=%d",
			metl154_experiment,
			ogl_metl154_experiment_name(metl154_experiment),
			texfilt,
			load_texfilt,
			metl154_disable_mips,
			metl154_force_rgba,
			metl154_force_stock);
#else
	int metl154_disable_mips = 0;
	int metl154_force_stock = 0;
	int load_texfilt = texfilt;
#endif

	{
		grs_bitmap *orig_bm = bm;
		while (bm->bm_parent)
			bm=bm->bm_parent;
#if defined(ANDROID) && defined(OGL_MERGE)
		if (bitmapname && !d_stricmp(bitmapname, "metl154"))
			debug_log(DLOG_TEXTURE,
				"[metl154load] entry: orig=%p bm=%p parent_traversed=%d idx=%d bm_flags=0x%x gltex=%p",
				(void *)orig_bm, (void *)bm, (bm != orig_bm),
				(int)(bm - GameBitmaps),
				bm->bm_flags,
				(void *)bm->gltexture);
#else
		(void)orig_bm;
#endif
	}
	if (bm->gltexture && bm->gltexture->handle > 0)
		return;
#ifdef OGL_MERGE
	/* During cache pass, bm_flags is BM_FLAG_PAGED_OUT; the real
	 * transparency flags live in the pig file's GameBitmapFlags[] */
	int real_flags = piggy_bitmap_get_flags(bm);
#if defined(ANDROID)
	if (bitmapname && !d_stricmp(bitmapname, "metl154"))
		debug_log(DLOG_TEXTURE,
			"[metl154load] flags: idx=%d bm_flags=0x%x real_flags=0x%x super=%d",
			(int)(bm - GameBitmaps), bm->bm_flags, real_flags,
			!!(real_flags & BM_FLAG_SUPER_TRANSPARENT));
#endif
#endif
	buf=bm->bm_data;
#ifdef HAVE_LIBPNG
	if (ogl_allow_png() && bitmapname && !(bm->gltexture && bm->gltexture->is_png)
		&& !metl154_force_stock)
	{
		char filename[64];
		png_data pdata;
		int png_loaded = 0;

#ifdef ANDROID
		/* Try pre-compressed ETC2 first (from .dxa texture packs). */
		if (bitmapname && !d_stricmp(bitmapname, "metl154")
			&& (metl154_force_rgba || metl154_force_stock))
			debug_log(DLOG_TEXTURE,
				"[metl154exp] skip_ktx2: mode=%s reason=%s",
				ogl_metl154_experiment_name(metl154_experiment),
				metl154_force_stock ? "stock" : "force_rgba");
		if (!ogl_etc2_broken && !metl154_force_rgba && !metl154_force_stock)
		{
			etc2_file_data edata;
			sprintf(filename, "%s.ktx2", bitmapname);
			if (read_ktx2_file(filename, &edata)) {
				/* android port: if bitmap needs transparency but KTX2 has
				 * no alpha channel (RGB-only ETC2), skip it and fall back
				 * to the base texture which correctly handles palette
				 * transparency via ogl_filltexbuf */
				int needs_alpha = (bm->bm_flags & (BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT));
				if (needs_alpha && !edata.format) {
					free(edata.filedata);
					debug_log(DLOG_TEXTURE,
						"ETC2 skip: %s needs alpha but KTX2 is RGB-only",
						bitmapname);
					goto skip_ktx2;
				}
				int flags = edata.format ? OGL_FLAG_ALPHA : 0;
				if (bm->bm_flags & BM_FLAG_TRANSPARENT)
					flags |= OGL_FLAG_ALPHA;
				if (bm->gltexture == NULL)
					ogl_init_texture(bm->gltexture = ogl_get_free_texture(),
						edata.width, edata.height, flags);

				/* Parse mip0 data */
				unsigned char *p = edata.filedata;
				unsigned char *end = p + edata.filedata_size;
				int mw = edata.width, mh = edata.height;
				int ok = 0;
				if (p + 4 <= end) {
					unsigned int sz = p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
					p += 4;
					if (p + sz <= end) {
						/* Hardware compressed upload */
						glGenTextures(1, &bm->gltexture->handle);
						OGL_BINDTEXTURE(bm->gltexture->handle);
						glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
						GLenum gl_fmt = edata.format
							? GL_COMPRESSED_RGBA8_ETC2_EAC
							: GL_COMPRESSED_RGB8_ETC2;
						int mips = edata.mip_count > 1 ? edata.mip_count : 1;
						int upload_mips = metl154_disable_mips ? 1 : mips;
						if (load_texfilt && upload_mips > 1) {
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
								(load_texfilt >= 2 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST));
						} else if (load_texfilt) {
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
						} else {
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
						}
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, upload_mips - 1);
						/* Upload all mip levels from packed [uint32_le size][data] buffer */
						{
							unsigned char *mp = p - 4; /* rewind to mip0 size prefix */
							unsigned char *mend = edata.filedata + edata.filedata_size;
							int lw = mw, lh = mh;
							int level;
							for (level = 0; level < upload_mips && mp + 4 <= mend; level++) {
								unsigned int msz = mp[0] | (mp[1]<<8) | (mp[2]<<16) | (mp[3]<<24);
								mp += 4;
								if (mp + msz > mend) break;
								glCompressedTexImage2D(GL_TEXTURE_2D, level, gl_fmt,
									lw, lh, 0, (GLsizei)msz, mp);
								mp += msz;
								if (lw > 1) lw >>= 1;
								if (lh > 1) lh >>= 1;
							}
						}
						/* android port: check for upload errors + data integrity */
							{
								GLenum gl_err = glGetError();
								if (gl_err != GL_NO_ERROR) {
									con_printf(CON_URGENT,
										"ETC2 upload FAILED: %s %dx%d fmt=0x%x sz=%u err=0x%x\n",
										bitmapname, mw, mh, gl_fmt, sz, gl_err);
									__android_log_print(ANDROID_LOG_ERROR, "DXX-TEX",
										"ETC2 upload FAILED: %s %dx%d fmt=0x%x sz=%u err=0x%x",
										bitmapname, mw, mh, gl_fmt, sz, gl_err);
								}
								/* Check if compressed data is non-zero (all-zero = black) */
								int nz64 = 0;
								{
									unsigned int check = sz < 64 ? sz : 64;
									unsigned int ci;
									for (ci = 0; ci < check; ci++)
										if (p[ci] != 0) nz64++;
								}
								debug_log(DLOG_TEXTURE,
									"ETC2 upload: %s %dx%d fmt=0x%x sz=%u handle=%u err=0x%x data0=%02x%02x%02x%02x nz64=%d",
									bitmapname, mw, mh, gl_fmt, sz,
									bm->gltexture->handle, gl_err,
									sz >= 4 ? p[0] : 0, sz >= 4 ? p[1] : 0,
									sz >= 4 ? p[2] : 0, sz >= 4 ? p[3] : 0, nz64);
								if (nz64 == 0) {
									__android_log_print(ANDROID_LOG_WARN, "DXX-TEX",
										"ETC2 ALL-ZERO data: %s %dx%d sz=%u -- texture will be black",
										bitmapname, mw, mh, sz);
									r_etc2_zero_data++;
								}
							}
						tex_set_size(bm->gltexture);
						r_texcount++;
						/* Set AF after all mip levels are uploaded -- Adreno
						 * ignores AF on textures without complete mipmap data */
						if (load_texfilt && upload_mips > 1) {
							bm->gltexture->has_mipmaps = 1;
							if (ogl_aniso_level > 0 && ogl_maxanisotropy > 0) {
								int af = ogl_aniso_level;
								if (af > (int)ogl_maxanisotropy) af = (int)ogl_maxanisotropy;
								glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, (float)af);
							}
						} else if (metl154_disable_mips) {
							ogl_apply_nomip_filter(bm->gltexture, texfilt);
						}
						if (bitmapname && !d_stricmp(bitmapname, "metl154"))
							debug_log(DLOG_TEXTURE,
								"[metl154exp] path=ktx2 mode=%s src_mips=%d upload_mips=%d texfilt=%d load_texfilt=%d",
								ogl_metl154_experiment_name(metl154_experiment),
								mips,
								upload_mips,
								texfilt,
								load_texfilt);
						ok = 1;
					}
				}
				free(edata.filedata);
				if (ok) {
					bm->gltexture->u = (float)edata.orig_width / (float)edata.width;
					bm->gltexture->v = (float)edata.orig_height / (float)edata.height;
					bm->gltexture->is_png = 1;
					r_hires_found++;
					r_hires_loaded++;
					/* android port: one-shot self-test -- render from this
					 * ETC2 texture and readback the center pixel via FBO.
					 * Confirms whether the GPU actually decodes non-black. */
					if (r_hires_loaded == 1) {
						GLuint fbo = 0, rbo = 0;
						glGenFramebuffers(1, &fbo);
						glGenRenderbuffers(1, &rbo);
						glBindRenderbuffer(GL_RENDERBUFFER, rbo);
						glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 4, 4);
						glBindFramebuffer(GL_FRAMEBUFFER, fbo);
						glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
						                          GL_RENDERBUFFER, rbo);
						if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
							int was_tex2d = GL_TEXTURE_2D_enabled;
							OGL_ENABLE(TEXTURE_2D);
							glViewport(0, 0, 4, 4);
							glDisable(GL_BLEND);
							glDisable(GL_DEPTH_TEST);
							glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
							/* White vertex color so GL_MODULATE shows texture color */
							GLfloat verts[] = {-1,-1, 1,-1, 1,1, -1,1};
							GLfloat uvs[] = {0.25f,0.25f, 0.75f,0.25f, 0.75f,0.75f, 0.25f,0.75f};
							GLfloat cols[] = {1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1};
							glEnableClientState(GL_VERTEX_ARRAY);
							glEnableClientState(GL_TEXTURE_COORD_ARRAY);
							glEnableClientState(GL_COLOR_ARRAY);
							OGL_BINDTEXTURE(bm->gltexture->handle);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
							glVertexPointer(2, GL_FLOAT, 0, verts);
							glTexCoordPointer(2, GL_FLOAT, 0, uvs);
							glColorPointer(4, GL_FLOAT, 0, cols);
							glClearColor(0, 0, 0, 0);
							glClear(GL_COLOR_BUFFER_BIT);
							glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
							glDisableClientState(GL_VERTEX_ARRAY);
							glDisableClientState(GL_TEXTURE_COORD_ARRAY);
							glDisableClientState(GL_COLOR_ARRAY);
							glFinish();
							unsigned char px[4] = {0};
							glReadPixels(2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
							__android_log_print(ANDROID_LOG_WARN, "DXX-TEX",
							    "ETC2 self-test: %s readback=(%d,%d,%d,%d) %s",
							    bitmapname, px[0], px[1], px[2], px[3],
							    (px[0] || px[1] || px[2]) ? "OK" : "BLACK -- GPU decodes to black");
							if (!was_tex2d) OGL_DISABLE(TEXTURE_2D);
						} else {
							__android_log_print(ANDROID_LOG_WARN, "DXX-TEX",
							    "ETC2 self-test: FBO incomplete, skipped");
						}
						glBindFramebuffer(GL_FRAMEBUFFER, 0);
						glDeleteRenderbuffers(1, &rbo);
						glDeleteFramebuffers(1, &fbo);
						/* Drain any stale GL errors from the self-test so
						 * they don't pollute the next upload's error check */
						while (glGetError() != GL_NO_ERROR) {}
						/* Re-bind the texture and restore sensible filter state */
						OGL_BINDTEXTURE(bm->gltexture->handle);
						if (texfilt) {
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
						} else {
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
						}
					}
#ifdef OGL_MERGE
					debug_log(DLOG_TEXTURE,
						"KTX2 mask check: %s bm_flags=0x%x real_flags=0x%x super=%d",
						bitmapname, bm->bm_flags, real_flags,
						!!(real_flags & BM_FLAG_SUPER_TRANSPARENT));
					if (real_flags & BM_FLAG_SUPER_TRANSPARENT)
						ogl_load_dxa_mask(bitmapname, bm, texfilt);
#endif
					return;
				}
				/* Decode/upload failed -- fall through to try PNG */
			}
		}
		skip_ktx2:
		/* Try multiple extensions -- stb_image handles all formats */
		{
			static const char *exts[] = {".png", ".jpg", ".tga"};
			int ei;
			for (ei = 0; ei < 3 && !png_loaded; ei++) {
				sprintf(filename, "%s%s", bitmapname, exts[ei]);
				png_loaded = read_png(filename, &pdata);
			}
		}
#else
		sprintf(filename, /*"textures/"*/ "%s.png", bitmapname);
		png_loaded = read_png(filename, &pdata);
#endif
		if (png_loaded)
		{
#ifdef ANDROID
			r_hires_found++;
#endif
			con_printf(CON_DEBUG,"%s: %ux%ux%i p=%i(%i) c=%i a=%i chans=%i\n", filename, pdata.width, pdata.height, pdata.depth, pdata.paletted, pdata.num_palette, pdata.color, pdata.alpha, pdata.channels);
			if (pdata.depth == 8 && pdata.color)
			{
				int df = pdata.paletted ? 0 : pdata.channels;
				if (!df)
					ogl_log_metl154_palette_source(bitmapname, pdata.data,
						pdata.width, pdata.height, bm->bm_flags, 1u << 1, "png-pal");
				else
					ogl_log_metl154_alpha_source(bitmapname, pdata.data,
						pdata.width, pdata.height, df, bm->bm_flags,
						df >= 4 ? 1u << 2 : 1u << 3,
						df >= 4 ? "png-rgba" : "png-rgb");
				if (bm->gltexture == NULL)
					ogl_init_texture(bm->gltexture = ogl_get_free_texture(), pdata.width, pdata.height, ((pdata.alpha || bm->bm_flags & BM_FLAG_TRANSPARENT) ? OGL_FLAG_ALPHA : 0));
				if (ogl_loadtexture(pdata.data, 0, 0, bm->gltexture, bm->bm_flags, df, load_texfilt)) {
					/* Upload failed (e.g. oversized) -- reinit with bitmap dims so
					 * the regular path can upload the original data. Set is_png=1
					 * to skip PNG search on future calls (avoids per-frame retry).
					 * Always use OGL_FLAG_ALPHA: bm_flags may be BM_FLAG_PAGED_OUT
					 * during cache pass, losing the real transparency flags. */
					ogl_init_texture(bm->gltexture, bm->bm_w, bm->bm_h, OGL_FLAG_ALPHA);
					bm->gltexture->is_png = 1;
					free(pdata.data);
					if (pdata.palette)
						free(pdata.palette);
				} else {
				#ifdef ANDROID
				if (metl154_disable_mips)
					ogl_apply_nomip_filter(bm->gltexture, texfilt);
				if (bitmapname && !d_stricmp(bitmapname, "metl154"))
					debug_log(DLOG_TEXTURE,
						"[metl154exp] path=rgba mode=%s texfilt=%d load_texfilt=%d df=%d",
						ogl_metl154_experiment_name(metl154_experiment),
						texfilt,
						load_texfilt,
						df);
				#endif
				#ifdef OGL_MERGE
				if (real_flags & BM_FLAG_SUPER_TRANSPARENT) {
#ifdef ANDROID
					ogl_load_dxa_mask(bitmapname, bm, texfilt);
#else
					ogl_loadpngmask(&pdata, bm, texfilt);
#endif
				}
				#endif
					ogl_log_metl154_palette_source(bitmapname, buf, bm->bm_w, bm->bm_h,
						bm->bm_flags, 1u << 0, "stock-pal");
				free(pdata.data);
				if (pdata.palette)
					free(pdata.palette);
				bm->gltexture->is_png = 1;
#ifdef ANDROID
				r_hires_loaded++;
#endif
				return;
				}
			}
			else
			{
				con_printf(CON_DEBUG,"%s: unsupported texture format: must be rgb, rgba, or paletted, and depth 8\n", filename);
				free(pdata.data);
				if (pdata.palette)
					free(pdata.palette);
			}
		}
	}
#ifdef ANDROID
	else if (bitmapname && !d_stricmp(bitmapname, "metl154") && metl154_force_stock)
		debug_log(DLOG_TEXTURE,
			"[metl154exp] skip_hires: mode=%s reason=stock",
			ogl_metl154_experiment_name(metl154_experiment));
#endif
#endif
#ifdef ANDROID
	/* Paged-out bitmap with no PNG replacement -- page in on demand.
	 * Desktop callers use PIGGY_PAGE_IN before rendering; on Android
	 * some paths skip it. Page in here so gltexture is never left NULL,
	 * which would cause SIGSEGV in ogl_bindbmtex. */
	if (bm->bm_flags & BM_FLAG_PAGED_OUT) {
		if (bm >= GameBitmaps && bm < &GameBitmaps[Num_bitmap_files]) {
			bitmap_index bi;
			bi.index = (unsigned short)(bm - GameBitmaps);
			piggy_bitmap_page_in(bi);
		} else {
			return;
		}
	}
#endif
	if (bm->gltexture == NULL){
 		ogl_init_texture(bm->gltexture = ogl_get_free_texture(), bm->bm_w, bm->bm_h, ((bm->bm_flags & (BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT))? OGL_FLAG_ALPHA : 0));
	}
	else {
		if (bm->gltexture->handle>0)
			return;
		if (bm->gltexture->w==0){
			bm->gltexture->lw=bm->bm_w;
			bm->gltexture->w=bm->bm_w;
			bm->gltexture->h=bm->bm_h;
		}
	}

	if (bm->bm_flags & BM_FLAG_RLE){
		unsigned char * dbits;
		unsigned char * sbits;
		int i, data_offset;

		data_offset = 1;
		if (bm->bm_flags & BM_FLAG_RLE_BIG)
			data_offset = 2;

		sbits = &bm->bm_data[4 + (bm->bm_h * data_offset)];
		dbits = decodebuf;

		for (i=0; i < bm->bm_h; i++ )    {
			gr_rle_decode(sbits,dbits);
			if ( bm->bm_flags & BM_FLAG_RLE_BIG )
				sbits += (int)INTEL_SHORT(*((short *)&(bm->bm_data[4+(i*data_offset)])));
			else
				sbits += (int)bm->bm_data[4+i];
			dbits += bm->bm_w;
		}
		buf=decodebuf;


		if(Game_mode & GM_MULTI && Netgame.BlackAndWhitePyros) {
			if(bm->bm_w == 64 && bm->bm_h == 64) {
				char is_purple_tex1 = bitmapname && !strcmp(bitmapname, "ship6-4");
				char is_purple_tex2 = bitmapname && !strcmp(bitmapname, "ship6-5");

				if(is_purple_tex1 || is_purple_tex2) {
					for(i=0; i < bm->bm_h * bm->bm_w; i++) {
						ubyte r = gr_current_pal[buf[i]*3];
						ubyte g = gr_current_pal[buf[i]*3+1];
						ubyte b = gr_current_pal[buf[i]*3+2];

						ubyte max = r;
						if(g > max) { max = g; }
						if(b > max) { max = b; }

						if((r > g*6/5) && (g > b*2)) {
							int replace = gr_find_closest_color(max/4,max/10,max/3);
							buf[i] = replace;
						}
					}
				}

				char is_white_tex1 = bitmapname && !strcmp(bitmapname, "ship7-4");
				char is_white_tex2 = bitmapname && !strcmp(bitmapname, "ship7-5");

				if(is_white_tex1 || is_white_tex2) {
					for(i=0; i < bm->bm_h * bm->bm_w; i++) {
						ubyte r = gr_current_pal[buf[i]*3];
						ubyte g = gr_current_pal[buf[i]*3+1];
						ubyte b = gr_current_pal[buf[i]*3+2];

						ubyte max = r;
						if(g > max) { max = g; }
						if(b > max) { max = b; }

						max = max * 4 / 3;
						if((g > r*4/3) && (g > b*4/3)) {
							int replace = gr_find_closest_color(max,max,max);
							buf[i] = replace;
						}
					}
				}

				char is_blue_tex2 = bitmapname && !strcmp(bitmapname, "ship1-5");
				if(is_blue_tex2) {
					static const int lower_bound[24]   = {28,27,26,25,24,23,22,21,20,19,19,18,17,16,15,14,13,13,12,11,10,9,8}; //bos
					static const int upper_bound[24]   = {57,55,54,52,50,49,48,47,45,44,42,41,39,38,36,35,33,32,30,29,27,25,23}; // fos
					for(i=0; i < bm->bm_h * bm->bm_w; i++) {
						int r = i / bm->bm_w;
						int c = i % bm->bm_w; 

						int in_filter_area_1 = 0;
						int in_filter_area_2 = 0;

						if(r >= 2 && r <= 6) {
							in_filter_area_1 = 1;
						}

						if(r >= 36 && r <= 58) {
							if(lower_bound[r-36] <= c && c <= upper_bound[r-36]) {
								in_filter_area_2 = 1;
							}
						}

						if(in_filter_area_1) {
							ubyte b = gr_current_pal[buf[i]*3+2];
							int replace = gr_find_closest_color(b/2,b/2,b); 
							buf[i] = replace; 
						}

						if(in_filter_area_2) {
							ubyte b = gr_current_pal[buf[i]*3+2];
							int replace = gr_find_closest_color(b,b,b*2); 
							buf[i] = replace; 
						}					
					}
				}	
			}
		}
	}
	ogl_loadtexture(buf, 0, 0, bm->gltexture, bm->bm_flags, 0, load_texfilt);
#ifdef ANDROID
	if (bitmapname && !d_stricmp(bitmapname, "metl154")) {
		if (metl154_disable_mips)
			ogl_apply_nomip_filter(bm->gltexture, texfilt);
		debug_log(DLOG_TEXTURE,
			"[metl154exp] path=stock mode=%s texfilt=%d load_texfilt=%d",
			ogl_metl154_experiment_name(metl154_experiment),
			texfilt,
			load_texfilt);
	}
#endif
	ogl_log_metl154_palette_source(bitmapname, buf, bm->bm_w, bm->bm_h,
		bm->bm_flags, 1u << 4, "stock-native");

#ifdef OGL_MERGE
	debug_log(DLOG_TEXTURE,
		"Stock mask check: %s bm_flags=0x%x real_flags=0x%x super=%d",
		bitmapname ? bitmapname : "<null>", bm->bm_flags, real_flags,
		!!(bm->bm_flags & BM_FLAG_SUPER_TRANSPARENT));
	if (bm->bm_flags & BM_FLAG_SUPER_TRANSPARENT) {
		unsigned char *mask;
		int size = bm->bm_w * bm->bm_h;

		if (bm->gltexture_mask == NULL)
			ogl_init_texture(bm->gltexture_mask = ogl_get_free_texture(), bm->bm_w, bm->bm_h, OGL_FLAG_ALPHA);

		MALLOC(mask, unsigned char, size);
		for (int i = 0; i < size; i++)
			mask[i] = buf[i] == 254 ? 255 : 0;
		ogl_loadtexture(mask, 0, 0, bm->gltexture_mask, BM_FLAG_TRANSPARENT, 0, texfilt);
		d_free(mask);
	}
#endif
}

void ogl_loadbmtexture(grs_bitmap *bm)
{
	ogl_loadbmtexture_f(bm, GameCfg.TexFilt);
}

void ogl_freetexture(ogl_texture *gltexture)
{
	if (gltexture->handle>0) {
		r_texcount--;
		glmprintf((0,"ogl_freetexture(%p):%i (%i left)\n",gltexture,gltexture->handle,r_texcount));
		glDeleteTextures( 1, &gltexture->handle );
//		gltexture->handle=0;
		ogl_reset_texture(gltexture);
	}
}
void ogl_freebmtexture(grs_bitmap *bm){
	if (bm->gltexture){
		ogl_freetexture(bm->gltexture);
		bm->gltexture=NULL;
	}
	if (bm->gltexture_mask){
		ogl_freetexture(bm->gltexture_mask);
		bm->gltexture_mask=NULL;
	}
}

/*
 * Menu / gauges 
 */
bool ogl_ubitmapm_cs(int x, int y,int dw, int dh, grs_bitmap *bm,int c, int scale) // to scale bitmaps
{
	GLfloat xo,yo,xf,yf,u1,u2,v1,v2,color_r,color_g,color_b,h;
	GLfloat color_array[] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	GLfloat texcoord_array[] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	GLfloat vertex_array[] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	
	x+=grd_curcanv->cv_bitmap.bm_x;
	y+=grd_curcanv->cv_bitmap.bm_y;
	xo=x/(float)last_width;
	xf=(bm->bm_w+x)/(float)last_width;
	yo=1.0-y/(float)last_height;
	yf=1.0-(bm->bm_h+y)/(float)last_height;

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	if (dw < 0)
		dw = grd_curcanv->cv_bitmap.bm_w;
	else if (dw == 0)
		dw = bm->bm_w;
	if (dh < 0)
		dh = grd_curcanv->cv_bitmap.bm_h;
	else if (dh == 0)
		dh = bm->bm_h;

	h = (double) scale / (double) F1_0;

	xo = x / ((double) last_width * h);
	xf = (dw + x) / ((double) last_width * h);
	yo = 1.0 - y / ((double) last_height * h);
	yf = 1.0 - (dh + y) / ((double) last_height * h);

	OGL_ENABLE(TEXTURE_2D);
	ogl_bindbmtex(bm);
	if (bm->gltexture == NULL) {
#ifdef ANDROID
		crash_breadcrumb_v("ogl_ubitmapm_cs: NULL gltex bm=%p flags=0x%x", (void*)bm, bm->bm_flags);
#endif
		return 0;
	}
	ogl_texwrap(bm->gltexture,GL_CLAMP_TO_EDGE);
	
	if (bm->bm_x==0){
		u1=0;
		if (bm->bm_w==bm->gltexture->w || !bm->bm_parent)
			u2=bm->gltexture->u;
		else
			u2=(bm->bm_w+bm->bm_x)/(float)bm->gltexture->tw;
	}else {
		u1=bm->bm_x/(float)bm->gltexture->tw;
		u2=(bm->bm_w+bm->bm_x)/(float)bm->gltexture->tw;
	}
	if (bm->bm_y==0){
		v1=0;
		if (bm->bm_h==bm->gltexture->h || !bm->bm_parent)
			v2=bm->gltexture->v;
		else
			v2=(bm->bm_h+bm->bm_y)/(float)bm->gltexture->th;
	}else{
		v1=bm->bm_y/(float)bm->gltexture->th;
		v2=(bm->bm_h+bm->bm_y)/(float)bm->gltexture->th;
	}

	if (c < 0) {
		color_r = 1.0;
		color_g = 1.0;
		color_b = 1.0;
	} else {
#ifdef ANDROID
		if (g_font_rgb_override[0] >= 0.f) {
			color_r = g_font_rgb_override[0];
			color_g = g_font_rgb_override[1];
			color_b = g_font_rgb_override[2];
		} else
#endif
		{
			color_r = CPAL2Tr(c);
			color_g = CPAL2Tg(c);
			color_b = CPAL2Tb(c);
		}
	}  

	color_array[0] = color_array[4] = color_array[8] = color_array[12] = color_r;
	color_array[1] = color_array[5] = color_array[9] = color_array[13] = color_g;
	color_array[2] = color_array[6] = color_array[10] = color_array[14] = color_b;
	color_array[3] = color_array[7] = color_array[11] = color_array[15] = 1.0;

	vertex_array[0] = xo;
	vertex_array[1] = yo;
	vertex_array[2] = xf;
	vertex_array[3] = yo;
	vertex_array[4] = xf;
	vertex_array[5] = yf;
	vertex_array[6] = xo;
	vertex_array[7] = yf;

	texcoord_array[0] = u1;
	texcoord_array[1] = v1;
	texcoord_array[2] = u2;
	texcoord_array[3] = v1;
	texcoord_array[4] = u2;
	texcoord_array[5] = v2;
	texcoord_array[6] = u1;
	texcoord_array[7] = v2;

	glVertexPointer(2, GL_FLOAT, 0, vertex_array);
	glColorPointer(4, GL_FLOAT, 0, color_array);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoord_array);  
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);//replaced GL_QUADS
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	
	return 0;
}

void ogl_update_window_clip()
{
	extern int Window_clip_left, Window_clip_top, Window_clip_right, Window_clip_bot;
	int cw = grd_curcanv->cv_bitmap.bm_w, ch = grd_curcanv->cv_bitmap.bm_h;

	if (!Window_clip_left && !Window_clip_top &&
		Window_clip_right == cw - 1 && Window_clip_bot == ch - 1) {
		glDisable(GL_SCISSOR_TEST);
	} else {
		glScissor(Window_clip_left + grd_curcanv->cv_bitmap.bm_x,
			grd_curscreen->sc_h - grd_curcanv->cv_bitmap.bm_y - Window_clip_bot - 1,
			Window_clip_right - Window_clip_left + 1,
			Window_clip_bot - Window_clip_top + 1);
		glEnable(GL_SCISSOR_TEST);
	}
}
