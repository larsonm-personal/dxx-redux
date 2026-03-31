/* etc2tool.cpp -- offline ETC2 texture compressor
 *
 * Reads PNG/JPG/TGA/BMP via stb_image, compresses to ETC2 via etc2comp
 * at effort 80 (out of 100), writes a .etc2 file with full mip chain.
 *
 * .etc2 file format:
 *   Header (16 bytes):
 *     magic:     4 bytes  "ETC2"
 *     width:     uint16   texture width (power-of-2 padded)
 *     height:    uint16   texture height (power-of-2 padded)
 *     format:    uint8    0 = GL_COMPRESSED_RGB8_ETC2
 *                         1 = GL_COMPRESSED_RGBA8_ETC2_EAC
 *     mip_count: uint8    number of mip levels
 *     reserved:  6 bytes  zero
 *
 *   For each mip level (largest first):
 *     data_size: uint32   byte size of compressed data
 *     data:      [bytes]  ETC2 block data
 *
 * Build: cmake -B build && cmake --build build --config Release
 * Usage: etc2tool input.png output.etc2 [--no-mips]
 */

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_PSD
#define STBI_NO_GIF
#include "stb_image.h"
#include "Etc.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

/* Round up to next power of 2 */
static int pow2ize(int v)
{
	int p = 1;
	while (p < v)
		p <<= 1;
	return p;
}

/* Write uint16 little-endian */
static void write_u16(FILE *f, uint16_t v)
{
	uint8_t b[2] = {(uint8_t)(v & 0xFF), (uint8_t)(v >> 8)};
	fwrite(b, 1, 2, f);
}

/* Write uint32 little-endian */
static void write_u32(FILE *f, uint32_t v)
{
	uint8_t b[4] = {(uint8_t)(v & 0xFF), (uint8_t)((v >> 8) & 0xFF),
	                (uint8_t)((v >> 16) & 0xFF), (uint8_t)(v >> 24)};
	fwrite(b, 1, 4, f);
}

/* Box-filter downsample RGBA by 2x */
static uint8_t *downsample_rgba(const uint8_t *src, int w, int h,
                                int *out_w, int *out_h)
{
	int nw = w / 2, nh = h / 2;
	if (nw < 1) nw = 1;
	if (nh < 1) nh = 1;
	uint8_t *dst = (uint8_t *)malloc(nw * nh * 4);
	if (!dst) return NULL;
	for (int y = 0; y < nh; y++)
		for (int x = 0; x < nw; x++)
			for (int c = 0; c < 4; c++) {
				int x0 = x * 2, y0 = y * 2;
				int x1 = (x0 + 1 < w) ? x0 + 1 : x0;
				int y1 = (y0 + 1 < h) ? y0 + 1 : y0;
				int v = src[(y0 * w + x0) * 4 + c]
				      + src[(y0 * w + x1) * 4 + c]
				      + src[(y1 * w + x0) * 4 + c]
				      + src[(y1 * w + x1) * 4 + c];
				dst[(y * nw + x) * 4 + c] = (uint8_t)((v + 2) / 4);
			}
	*out_w = nw;
	*out_h = nh;
	return dst;
}

/* Compress RGBA pixels to ETC2 via etc2comp, max effort */
static unsigned char *compress_etc2(const uint8_t *rgba, int w, int h,
                                    bool has_alpha, unsigned int *out_bytes)
{
	int npix = w * h;
	float *fsrc = (float *)malloc((size_t)npix * 4 * sizeof(float));
	if (!fsrc) return NULL;
	for (int i = 0; i < npix * 4; i++)
		fsrc[i] = rgba[i] / 255.0f;

	unsigned char *enc_bits = NULL;
	unsigned int enc_bytes = 0;
	unsigned int ext_w = 0, ext_h = 0;
	int enc_time_ms = 0;

	Etc::Image::Format fmt = has_alpha ? Etc::Image::Format::RGBA8
	                                   : Etc::Image::Format::RGB8;

	Etc::Encode(fsrc, (unsigned int)w, (unsigned int)h,
	            fmt, Etc::RGBA,
	            80.0f, /* high effort for offline quality */
	            16, 16, /* use multiple threads for speed */
	            &enc_bits, &enc_bytes,
	            &ext_w, &ext_h,
	            &enc_time_ms);

	free(fsrc);

	if (!enc_bits || enc_bytes == 0)
		return NULL;

	*out_bytes = enc_bytes;
	return enc_bits;
}

/* Pad image to power-of-2 dimensions with transparent black */
static uint8_t *pad_to_pow2(const uint8_t *src, int w, int h, int channels,
                            int tw, int th, bool *has_alpha_out)
{
	uint8_t *dst = (uint8_t *)calloc(tw * th * 4, 1);
	if (!dst) return NULL;

	bool has_alpha = (channels == 4 || channels == 2);
	*has_alpha_out = has_alpha;

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int si = (y * w + x) * channels;
			int di = (y * tw + x) * 4;
			if (channels >= 3) {
				dst[di + 0] = src[si + 0];
				dst[di + 1] = src[si + 1];
				dst[di + 2] = src[si + 2];
				dst[di + 3] = (channels == 4) ? src[si + 3] : 255;
			} else if (channels == 2) {
				dst[di + 0] = dst[di + 1] = dst[di + 2] = src[si + 0];
				dst[di + 3] = src[si + 1];
			} else {
				dst[di + 0] = dst[di + 1] = dst[di + 2] = src[si + 0];
				dst[di + 3] = 255;
			}
		}
	}
	return dst;
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "Usage: etc2tool input.(png|jpg|tga) output.etc2 [--no-mips]\n");
		return 1;
	}

	const char *input_path = argv[1];
	const char *output_path = argv[2];
	bool gen_mips = true;

	for (int i = 3; i < argc; i++) {
		if (strcmp(argv[i], "--no-mips") == 0)
			gen_mips = false;
	}

	/* Load image */
	int w, h, channels;
	uint8_t *pixels = stbi_load(input_path, &w, &h, &channels, 0);
	if (!pixels) {
		fprintf(stderr, "Failed to load: %s\n", input_path);
		return 1;
	}

	/* Power-of-2 dimensions (ETC2 needs multiple of 4, pow2 guarantees that) */
	int tw = pow2ize(w);
	int th = pow2ize(h);

	/* Minimum 4x4 for ETC2 blocks */
	if (tw < 4) tw = 4;
	if (th < 4) th = 4;

	/* Pad to pow2 and convert to RGBA */
	bool has_alpha;
	uint8_t *rgba = pad_to_pow2(pixels, w, h, channels, tw, th, &has_alpha);
	stbi_image_free(pixels);
	if (!rgba) {
		fprintf(stderr, "Out of memory\n");
		return 1;
	}

	/* Count mip levels */
	int mip_count = 1;
	if (gen_mips) {
		int mw = tw, mh = th;
		while (mw > 4 && mh > 4) {
			mw /= 2;
			mh /= 2;
			mip_count++;
		}
	}

	/* Open output */
	FILE *out = fopen(output_path, "wb");
	if (!out) {
		fprintf(stderr, "Cannot open output: %s\n", output_path);
		free(rgba);
		return 1;
	}

	/* Write header */
	fwrite("ETC2", 1, 4, out);
	write_u16(out, (uint16_t)tw);
	write_u16(out, (uint16_t)th);
	uint8_t fmt_byte = has_alpha ? 1 : 0;
	fwrite(&fmt_byte, 1, 1, out);
	uint8_t mc = (uint8_t)mip_count;
	fwrite(&mc, 1, 1, out);
	uint8_t reserved[6] = {0};
	fwrite(reserved, 1, 6, out);

	/* Compress and write each mip level */
	uint8_t *cur = rgba;
	int mw = tw, mh = th;
	for (int level = 0; level < mip_count; level++) {
		unsigned int comp_bytes = 0;
		unsigned char *comp = compress_etc2(cur, mw, mh, has_alpha, &comp_bytes);
		if (!comp) {
			fprintf(stderr, "Compression failed at mip level %d (%dx%d)\n",
			        level, mw, mh);
			fclose(out);
			free(cur == rgba ? rgba : cur);
			return 1;
		}

		write_u32(out, comp_bytes);
		fwrite(comp, 1, comp_bytes, out);
		delete[] comp; /* etc2comp allocates with new[] */

		if (level + 1 < mip_count) {
			int nmw, nmh;
			uint8_t *next = downsample_rgba(cur, mw, mh, &nmw, &nmh);
			if (cur != rgba) free(cur);
			cur = next;
			mw = nmw;
			mh = nmh;
		}
	}

	if (cur != rgba) free(cur);
	free(rgba);
	fclose(out);

	printf("OK: %dx%d %s %d mips -> %s\n",
	       tw, th, has_alpha ? "RGBA" : "RGB", mip_count, output_path);
	return 0;
}
