#ifdef ANDROID

#include <string.h>

#include "gr.h"
#include "ogl_init.h"
#include "piggy.h"
#include "strutil.h"

#include "android_log.h"
#include "android_texture_debug.h"

static char g_android_texture_debug_target_name[ANDROID_TEXTURE_DEBUG_TARGET_NAME_MAX] = { 0 };
volatile int g_android_texture_debug_target_mode = ANDROID_TEXTURE_DEBUG_TARGET_CROSSHAIR;

static void android_texture_debug_copy_string(char *dst, int dst_size,
                                              const char *src)
{
	int i;

	if (!dst || dst_size <= 0)
		return;
	if (!src)
		src = "";
	for (i = 0; i + 1 < dst_size && src[i]; i++)
		dst[i] = src[i];
	dst[i] = '\0';
}

void android_texture_debug_set_target(const char *value)
{
	int mode = ANDROID_TEXTURE_DEBUG_TARGET_CROSSHAIR;
	char next_name[ANDROID_TEXTURE_DEBUG_TARGET_NAME_MAX] = { 0 };

	if (!value || !value[0] || !d_stricmp(value, "crosshair") || !d_stricmp(value, "auto")) {
		mode = ANDROID_TEXTURE_DEBUG_TARGET_CROSSHAIR;
	} else if (!d_stricmp(value, "none") || !d_stricmp(value, "off") || !d_stricmp(value, "disabled")) {
		mode = ANDROID_TEXTURE_DEBUG_TARGET_NONE;
	} else {
		mode = ANDROID_TEXTURE_DEBUG_TARGET_NAME;
		android_texture_debug_copy_string(next_name, sizeof(next_name), value);
	}

	android_texture_debug_copy_string(g_android_texture_debug_target_name,
	                                  sizeof(g_android_texture_debug_target_name), next_name);
	__sync_synchronize();
	g_android_texture_debug_target_mode = mode;
}

const char *android_texture_debug_get_target_display(void)
{
	switch ((int) g_android_texture_debug_target_mode) {
		case ANDROID_TEXTURE_DEBUG_TARGET_NAME:
			return g_android_texture_debug_target_name;
		case ANDROID_TEXTURE_DEBUG_TARGET_CROSSHAIR:
			return "crosshair";
		default:
			return "";
	}
}

int android_texture_debug_target_is_crosshair(void)
{
	return (int) g_android_texture_debug_target_mode == ANDROID_TEXTURE_DEBUG_TARGET_CROSSHAIR;
}

int android_texture_debug_matches_target_name(const char *bitmapname)
{
	if ((int) g_android_texture_debug_target_mode != ANDROID_TEXTURE_DEBUG_TARGET_NAME)
		return 0;
	if (!bitmapname || !g_android_texture_debug_target_name[0])
		return 0;
	return !d_stricmp(bitmapname, g_android_texture_debug_target_name);
}

static unsigned int android_texture_debug_fnv1a_append(unsigned int hash,
                                                       const unsigned char *data, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		hash ^= (unsigned int) data[i];
		hash *= 16777619u;
	}
	return hash;
}

static int android_texture_debug_texture_channels(GLenum format)
{
	switch (format) {
		case GL_LUMINANCE:
			return 1;
		case GL_LUMINANCE_ALPHA:
			return 2;
		case GL_RGB:
			return 3;
		case GL_RGBA:
			return 4;
		default:
			return 0;
	}
}

static void android_texture_debug_unpack_pixel(const unsigned char *src,
                                               int channels, int *r, int *g, int *b, int *a)
{
	if (!src || channels <= 0) {
		*r = *g = *b = 0;
		*a = 255;
		return;
	}

	switch (channels) {
		case 1:
			*r = *g = *b = src[0];
			*a = 255;
			break;
		case 2:
			*r = *g = *b = src[0];
			*a = src[1];
			break;
		case 3:
			*r = src[0];
			*g = src[1];
			*b = src[2];
			*a = 255;
			break;
		default:
			*r = src[0];
			*g = src[1];
			*b = src[2];
			*a = src[3];
			break;
	}
}

static int android_texture_debug_count_mip_levels(int width, int height)
{
	int levels = 0;
	int dim = width > height ? width : height;

	while (dim > 0) {
		levels++;
		dim >>= 1;
	}
	return levels;
}

void android_texture_debug_log_upload_source(const char *bitmapname,
                                             const char *path, const char *source_name, const unsigned char *data,
                                             int width, int height, int row_stride, int bm_flags, int real_flags)
{
	unsigned int hash = 2166136261u;
	int idx254 = 0, idx255 = 0;
	int x, y;

	if (!android_texture_debug_matches_target_name(bitmapname) || !data || width <= 0 || height <= 0)
		return;
	if (row_stride <= 0)
		row_stride = width;

	for (y = 0; y < height; y++) {
		const unsigned char *row = data + y * row_stride;

		hash = android_texture_debug_fnv1a_append(hash, row, width);
		for (x = 0; x < width; x++) {
			if (row[x] == 254)
				idx254++;
			else if (row[x] == 255)
				idx255++;
		}
	}

	debug_log(DLOG_TEXTURE,
	          "[mwall_upload_src] name=%s path=%s source=%s w=%d h=%d rowsize=%d bytes=%d bm_flags=0x%x real_flags=0x%x idx254=%d idx255=%d hash=0x%08x",
	          bitmapname ? bitmapname : "<none>",
	          path ? path : "",
	          source_name ? source_name : "",
	          width,
	          height,
	          row_stride,
	          width * height,
	          bm_flags,
	          real_flags,
	          idx254,
	          idx255,
	          hash);
}

void android_texture_debug_log_upload_expanded(const char *bitmapname,
                                               const char *path, ogl_texture *tex, const unsigned char *data,
                                               int bm_flags, int data_format)
{
	unsigned int hash;
	unsigned int sum_r = 0, sum_g = 0, sum_b = 0, sum_a = 0;
	int channels, pixel_count, black_rgb = 0;
	int alpha0 = 0, alpha255 = 0, alpha_partial = 0;
	int p0_r = 0, p0_g = 0, p0_b = 0, p0_a = 255;
	int center_r = 0, center_g = 0, center_b = 0, center_a = 255;
	int i;

	if (!android_texture_debug_matches_target_name(bitmapname) || !tex || !data)
		return;
	channels = android_texture_debug_texture_channels(tex->format);
	if (!channels || tex->tw <= 0 || tex->th <= 0)
		return;

	pixel_count = tex->tw * tex->th;
	hash = android_texture_debug_fnv1a_append(2166136261u, data,
	                                          pixel_count * channels);
	for (i = 0; i < pixel_count; i++) {
		int r, g, b, a;

		android_texture_debug_unpack_pixel(data + i * channels, channels,
		                                   &r, &g, &b, &a);
		sum_r += (unsigned int) r;
		sum_g += (unsigned int) g;
		sum_b += (unsigned int) b;
		sum_a += (unsigned int) a;
		if (r == 0 && g == 0 && b == 0)
			black_rgb++;
		if (a == 0)
			alpha0++;
		else if (a == 255)
			alpha255++;
		else
			alpha_partial++;
	}
	android_texture_debug_unpack_pixel(data, channels, &p0_r, &p0_g, &p0_b,
	                                   &p0_a);
	android_texture_debug_unpack_pixel(data + (((tex->th / 2) * tex->tw + (tex->tw / 2)) * channels), channels, &center_r, &center_g,
	                                   &center_b, &center_a);

	debug_log(DLOG_TEXTURE,
	          "[mwall_upload_cpu] name=%s path=%s format=0x%x internal=0x%x channels=%d bm_flags=0x%x data_format=%d tex=%dx%d p2=%dx%d bytes=%d hash=0x%08x avg=%d/%d/%d/%d black=%d alpha0=%d alpha255=%d alpha_partial=%d p0=%d/%d/%d/%d center=%d/%d/%d/%d",
	          bitmapname ? bitmapname : "<none>",
	          path ? path : "",
	          tex->format,
	          tex->internalformat,
	          channels,
	          bm_flags,
	          data_format,
	          tex->w,
	          tex->h,
	          tex->tw,
	          tex->th,
	          pixel_count * channels,
	          hash,
	          (int) (sum_r / (unsigned int) pixel_count),
	          (int) (sum_g / (unsigned int) pixel_count),
	          (int) (sum_b / (unsigned int) pixel_count),
	          (int) (sum_a / (unsigned int) pixel_count),
	          black_rgb,
	          alpha0,
	          alpha255,
	          alpha_partial,
	          p0_r,
	          p0_g,
	          p0_b,
	          p0_a,
	          center_r,
	          center_g,
	          center_b,
	          center_a);
}

void android_texture_debug_log_mip_upload(const char *bitmapname,
                                          const char *path, grs_bitmap *bm, int texfilt, int load_texfilt,
                                          int source_levels, int uploaded_levels, int generated_mips,
                                          int compressed_upload)
{
	ogl_texture *tex;
	GLint active_tex = GL_TEXTURE0, prev_bind = 0;
	GLint min_filter = -1, mag_filter = -1, base_level = -1, max_level = -1;
	int expected_levels;

	if (!android_texture_debug_matches_target_name(bitmapname) || !bm || !bm->gltexture || bm->gltexture->handle <= 0)
		return;

	tex = bm->gltexture;
	expected_levels = android_texture_debug_count_mip_levels(tex->tw, tex->th);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &active_tex);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_bind);
	if ((GLuint) prev_bind != tex->handle)
		glBindTexture(GL_TEXTURE_2D, tex->handle);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &min_filter);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &mag_filter);
#ifdef GL_TEXTURE_BASE_LEVEL
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, &base_level);
#endif
#ifdef GL_TEXTURE_MAX_LEVEL
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, &max_level);
#endif
	if ((GLuint) prev_bind != tex->handle)
		glBindTexture(GL_TEXTURE_2D, (GLuint) prev_bind);
	glActiveTexture((GLenum) active_tex);

	debug_log(DLOG_TEXTURE,
	          "[mwall_mip_upload] name=%s path=%s texfilt=%d load_texfilt=%d upgrade=%d aniso=%d source_levels=%d uploaded_levels=%d generated=%d compressed=%d expected_full=%d handle=%u has_mips=%d min=0x%x mag=0x%x base=%d max=%d src=%dx%d tex=%dx%d p2=%dx%d bytes=%d is_png=%d bm_flags=0x%x real_flags=0x%x",
	          bitmapname ? bitmapname : "<none>",
	          path ? path : "",
	          texfilt,
	          load_texfilt,
	          load_texfilt != texfilt,
	          ogl_aniso_level,
	          source_levels,
	          uploaded_levels,
	          generated_mips,
	          compressed_upload,
	          expected_levels,
	          tex->handle,
	          tex->has_mipmaps,
	          min_filter,
	          mag_filter,
	          base_level,
	          max_level,
	          bm->bm_w,
	          bm->bm_h,
	          tex->w,
	          tex->h,
	          tex->tw,
	          tex->th,
	          tex->bytes,
	          tex->is_png,
	          bm->bm_flags,
	          piggy_bitmap_get_flags(bm));
}

#endif /* ANDROID */
