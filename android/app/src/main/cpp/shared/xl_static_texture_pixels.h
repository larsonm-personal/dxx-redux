#ifndef DXX_XL_STATIC_TEXTURE_PIXELS_H
#define DXX_XL_STATIC_TEXTURE_PIXELS_H
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
/* Renderer-only source coverage. Mask aliases data; free only data. */
typedef struct dxx_xl_static_pixels {
	unsigned char *data, *mask;
	int width, height, super;
} dxx_xl_static_pixels;

static inline int dxx_xl_fit_static_coverage(const unsigned char *pixels, const unsigned char *coverage, int w, int h,
                                             int channels, int limit, dxx_xl_static_pixels *out)
{
	if (!out) return 0;
	memset(out, 0, sizeof(*out));
	/* Color plus mask stays within 64 MiB; source decode has its own bound. */
	if (!pixels || w <= 0 || h <= 0 || w > 16384 || h > w ||
	    (channels != 3 && channels != 4) || limit <= 0 || limit > 16384 ||
	    (size_t) w * h > (64u * 1024u * 1024u) / 5) return 0;
	for (size_t i = 0; i < (size_t) w * h; ++i)
		if (channels == 4 && pixels[i * 4 + 3] != 0 && pixels[i * 4 + 3] != 255 &&
		    !(coverage ? coverage[i] : pixels[i * 4] == 120 && pixels[i * 4 + 1] == 88 && pixels[i * 4 + 2] == 128)) return 0;
	const int divisor = ((w > h ? w : h) + limit - 1) / limit;
	const int nw = w / divisor > 0 ? w / divisor : 1;
	const int nh = h / divisor > 0 ? h / divisor : 1;
	unsigned char *fitted = (unsigned char *) malloc((size_t) nw * nh * 5);
	if (!fitted) return 0;
	unsigned char *mask = fitted + (size_t) nw * nh * 4;
	/* Classify before filtering, preserving even sparse holes. Conservative
	 * min-coverage reduction can widen subpixel holes; it never fills them.
	 * Opaque color averaging excludes key/zero-alpha RGB to avoid purple fringes. */
	for (int y = 0; y < nh; ++y)
		for (int x = 0; x < nw; ++x) {
			unsigned int sums[3] = { 0 }, count = 0;
			int hole = 0, cutout = 0;
			for (int sy = y * h / nh; sy < (y + 1) * h / nh; ++sy)
				for (int sx = x * w / nw; sx < (x + 1) * w / nw; ++sx) {
					const unsigned char *p = pixels + ((size_t) sy * w + sx) * channels;
					const int key = coverage ? coverage[(size_t) sy * w + sx] != 0 : p[0] == 120 && p[1] == 88 && p[2] == 128;
					hole |= key;
					cutout |= key || (channels == 4 && !p[3]);
					if (!key && (channels == 3 || p[3])) {
						for (int c = 0; c < 3; ++c) sums[c] += p[c];
						++count;
					}
				}
			const size_t index = (size_t) y * nw + x;
			for (int c = 0; c < 3; ++c) fitted[index * 4 + c] = count ? sums[c] / count : 0;
			fitted[index * 4 + 3] = cutout ? 0 : 255;
			mask[index] = hole ? 255 : 0;
			out->super |= hole;
		}
	out->data = fitted;
	out->mask = mask;
	out->width = nw;
	out->height = nh;
	return 1;
}
static inline int dxx_xl_fit_static_pixels(const unsigned char *pixels, int w, int h,
                                           int channels, int limit, dxx_xl_static_pixels *out)
{
	return dxx_xl_fit_static_coverage(pixels, NULL, w, h, channels, limit, out);
}

/* Soft color is premultiplied before filtering. Coverage remains independent:
 * a super-hole discards both wall layers even when its color alpha is zero. */
static inline int dxx_xl_fit_soft_coverage(const unsigned char *pixels, const unsigned char *coverage,
                                           int w, int h, int channels, int limit, int premultiplied,
                                           dxx_xl_static_pixels *out)
{
	if (!out) return 0;
	memset(out, 0, sizeof(*out));
	if (!pixels || w <= 0 || h <= 0 || w > 16384 || h > w ||
	    (channels != 3 && channels != 4) || limit <= 0 || limit > 16384 ||
	    (premultiplied && !coverage) || premultiplied < 0 || premultiplied > 1 ||
	    (size_t) w * h > (64u * 1024u * 1024u) / 5) return 0;
	const int divisor = (w + limit - 1) / limit;
	const int nw = w / divisor > 0 ? w / divisor : 1;
	const int nh = h / divisor > 0 ? h / divisor : 1;
	unsigned char *data = (unsigned char *) malloc((size_t) nw * nh * 5);
	if (!data) return 0;
	unsigned char *mask = data + (size_t) nw * nh * 4;
	for (int y = 0; y < nh; ++y)
		for (int x = 0; x < nw; ++x) {
			uint64_t sums[3] = { 0 }, alpha = 0, count = 0;
			int hole = 0;
			for (int sy = y * h / nh; sy < (y + 1) * h / nh; ++sy)
				for (int sx = x * w / nw; sx < (x + 1) * w / nw; ++sx) {
					const size_t index = (size_t) sy * w + sx;
					const unsigned char *p = pixels + index * channels;
					const int key = coverage ? coverage[index] != 0 : p[0] == 120 && p[1] == 88 && p[2] == 128;
					const unsigned a = key ? 0 : channels == 4 ? p[3]
					                                           : 255;
					hole |= key;
					alpha += a;
					++count;
					if (!key)
						for (int c = 0; c < 3; ++c) sums[c] += (uint64_t) p[c] * (premultiplied ? 255 : a);
				}
			const size_t index = (size_t) y * nw + x;
			for (int c = 0; c < 3; ++c) data[index * 4 + c] = sums[c] / (count * 255);
			data[index * 4 + 3] = alpha / count;
			mask[index] = hole ? 255 : 0;
			out->super |= hole;
		}
	out->data = data;
	out->mask = mask;
	out->width = nw;
	out->height = nh;
	return 1;
}

static inline int dxx_xl_rgba_opaque(const unsigned char *pixels, int w, int h)
{
	if (!pixels || w <= 0 || h <= 0 || (size_t) w * h > (64u * 1024u * 1024u) / 4) return 0;
	for (size_t i = 0; i < (size_t) w * h; ++i)
		if (pixels[i * 4 + 3] != 255) return 0;
	return 1;
}
#endif
