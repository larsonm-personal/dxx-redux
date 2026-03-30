/* etc2_compress.c -- lightweight ETC2 texture compression for GLES 3.0
 *
 * Fast-path ETC2 compressor for 128x128 game textures on Android.
 * Prioritizes speed over optimal quality -- uses individual mode with
 * per-subblock average colors and closest modifiers.
 *
 * References:
 * - Khronos ETC2 spec (OpenGL ES 3.0, Appendix C.1)
 * - "Ericsson Texture Compression" technical papers
 */

#include "etc2_compress.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ETC1/ETC2 modifier table (from spec Table C.7) */
static const int modifier_table[8][4] = {
	{ 2, 8, -2, -8 },
	{ 5, 17, -5, -17 },
	{ 9, 29, -9, -29 },
	{ 13, 42, -13, -42 },
	{ 18, 56, -18, -56 },
	{ 24, 71, -24, -71 },
	{ 33, 92, -33, -92 },
	{ 47, 124, -47, -124 },
};

/* EAC modifier table for alpha (from spec Table C.12) */
static const int eac_modifier_table[16][8] = {
	{ -3, -6, -9, -15, 2, 5, 8, 14 },
	{ -3, -7, -10, -13, 2, 6, 9, 12 },
	{ -2, -5, -8, -13, 1, 4, 7, 12 },
	{ -2, -4, -6, -13, 1, 3, 5, 12 },
	{ -3, -6, -8, -12, 2, 5, 7, 11 },
	{ -3, -7, -9, -11, 2, 6, 8, 10 },
	{ -4, -7, -8, -11, 3, 6, 7, 10 },
	{ -3, -5, -8, -11, 2, 4, 7, 10 },
	{ -2, -6, -8, -10, 1, 5, 7, 9 },
	{ -2, -5, -8, -10, 1, 4, 7, 9 },
	{ -2, -4, -8, -10, 1, 3, 7, 9 },
	{ -2, -5, -7, -10, 1, 4, 6, 9 },
	{ -3, -4, -7, -10, 2, 3, 6, 9 },
	{ -1, -2, -3, -10, 0, 1, 2, 9 },
	{ -4, -6, -8, -9, 3, 5, 7, 8 },
	{ -3, -5, -7, -9, 2, 4, 6, 8 },
};

static int clamp255(int v)
{
	return v < 0 ? 0 : (v > 255 ? 255 : v);
}

/* Compute average RGB of a 4x4 block or 2x4/4x2 sub-block.
 * pixels: pointer to top-left of the 4x4 block in RGBA8888 image
 * stride: image row stride in bytes (width * 4)
 * x0,y0,x1,y1: sub-block rectangle within the 4x4 block */
static void subblock_average(const uint8_t *pixels, int stride,
                             int x0, int y0, int x1, int y1,
                             int *avg_r, int *avg_g, int *avg_b)
{
	int sr = 0, sg = 0, sb = 0, count = 0;
	for (int y = y0; y < y1; y++) {
		const uint8_t *row = pixels + y * stride;
		for (int x = x0; x < x1; x++) {
			sr += row[x * 4 + 0];
			sg += row[x * 4 + 1];
			sb += row[x * 4 + 2];
			count++;
		}
	}
	*avg_r = (sr + count / 2) / count;
	*avg_g = (sg + count / 2) / count;
	*avg_b = (sb + count / 2) / count;
}

/* Quantize an 8-bit color to 4 bits, returning both the 4-bit value and
 * the expanded-back 8-bit value */
static int quantize4(int v8, int *expanded)
{
	int v4 = (v8 + 8) >> 4;
	if (v4 > 15) v4 = 15;
	*expanded = (v4 << 4) | v4;
	return v4;
}

/* Find the best modifier table index for a sub-block.
 * Returns table index (0-7) and sets pixel_indices[count] to the
 * chosen 2-bit index for each pixel. */
static int find_best_table(const uint8_t *pixels, int stride,
                           int x0, int y0, int x1, int y1,
                           int base_r, int base_g, int base_b,
                           uint8_t *pixel_indices)
{
	int best_table = 0;
	int best_error = 0x7fffffff;
	uint8_t best_indices[16];
	int pidx = 0;

	for (int t = 0; t < 8; t++) {
		int error = 0;
		pidx = 0;
		uint8_t indices[16];
		/* Iterate column-major to match ETC2 pixel scanning order:
		 * col 0 rows 0-3, col 1 rows 0-3, ... */
		for (int x = x0; x < x1; x++) {
			for (int y = y0; y < y1; y++) {
				const uint8_t *row = pixels + y * stride;
				int pr = row[x * 4 + 0];
				int pg = row[x * 4 + 1];
				int pb = row[x * 4 + 2];
				int best_d = 0x7fffffff;
				int best_i = 0;
				for (int i = 0; i < 4; i++) {
					int mr = clamp255(base_r + modifier_table[t][i]);
					int mg = clamp255(base_g + modifier_table[t][i]);
					int mb = clamp255(base_b + modifier_table[t][i]);
					int dr = pr - mr;
					int dg = pg - mg;
					int db = pb - mb;
					int d = dr * dr + dg * dg + db * db;
					if (d < best_d) {
						best_d = d;
						best_i = i;
					}
				}
				indices[pidx++] = (uint8_t) best_i;
				error += best_d;
			}
		} /* end column-major iteration */
		if (error < best_error) {
			best_error = error;
			best_table = t;
			memcpy(best_indices, indices, (size_t) pidx);
		}
	}
	memcpy(pixel_indices, best_indices, (size_t) pidx);
	return best_table;
}

/* Encode one ETC2 RGB block (8 bytes) from a 4x4 pixel region.
 * Uses "individual" mode (bit 33 = 0): two 4-bit base colors + modifiers.
 * Sub-blocks are left (columns 0-1) and right (columns 2-3). */
static void encode_etc2_rgb_block(const uint8_t *pixels, int stride,
                                  uint8_t *out)
{
	int avg_r1, avg_g1, avg_b1;
	int avg_r2, avg_g2, avg_b2;
	int exp_r1, exp_g1, exp_b1;
	int exp_r2, exp_g2, exp_b2;

	/* Sub-block 1: columns 0-1, Sub-block 2: columns 2-3 */
	subblock_average(pixels, stride, 0, 0, 2, 4, &avg_r1, &avg_g1, &avg_b1);
	subblock_average(pixels, stride, 2, 0, 4, 4, &avg_r2, &avg_g2, &avg_b2);

	int r1_4 = quantize4(avg_r1, &exp_r1);
	int g1_4 = quantize4(avg_g1, &exp_g1);
	int b1_4 = quantize4(avg_b1, &exp_b1);
	int r2_4 = quantize4(avg_r2, &exp_r2);
	int g2_4 = quantize4(avg_g2, &exp_g2);
	int b2_4 = quantize4(avg_b2, &exp_b2);

	/* Find best modifier tables */
	uint8_t idx1[8], idx2[8];
	int table1 = find_best_table(pixels, stride, 0, 0, 2, 4,
	                             exp_r1, exp_g1, exp_b1, idx1);
	int table2 = find_best_table(pixels, stride, 2, 0, 4, 4,
	                             exp_r2, exp_g2, exp_b2, idx2);

	/* Pack header bytes (individual mode: bit 33 = 0, flip = 0) */
	out[0] = (uint8_t) ((r1_4 << 4) | r2_4);
	out[1] = (uint8_t) ((g1_4 << 4) | g2_4);
	out[2] = (uint8_t) ((b1_4 << 4) | b2_4);
	/* byte 3: table1[2:0] table2[2:0] diff=0 flip=0 */
	out[3] = (uint8_t) ((table1 << 5) | (table2 << 2) | 0x00);

	/* Pack pixel indices -- ETC2 pixel order is column-major within
	 * each sub-block, MSB and LSB planes separated.
	 * Pixel indices (2 bits each): bit 1 goes to MSB plane (bytes 4-5),
	 * bit 0 goes to LSB plane (bytes 6-7).
	 *
	 * Sub-block layout (columns 0-1 = sub1, columns 2-3 = sub2):
	 * Pixel scanning order within the 4x4 block (for the index tables):
	 * col 0: rows 0-3, col 1: rows 0-3, col 2: rows 0-3, col 3: rows 0-3
	 */
	uint32_t msb_plane = 0;
	uint32_t lsb_plane = 0;

	/* Sub-block 1: columns 0-1 */
	for (int col = 0; col < 2; col++) {
		for (int row = 0; row < 4; row++) {
			int bit_pos = col * 4 + row;
			int idx_pos = col * 4 + row;
			int idx = idx1[idx_pos];
			msb_plane |= ((idx >> 1) & 1) << (15 - bit_pos);
			lsb_plane |= ((idx >> 0) & 1) << (15 - bit_pos);
		}
	}

	/* Sub-block 2: columns 2-3 */
	for (int col = 2; col < 4; col++) {
		for (int row = 0; row < 4; row++) {
			int bit_pos = col * 4 + row;
			int idx_pos = (col - 2) * 4 + row;
			int idx = idx2[idx_pos];
			msb_plane |= ((idx >> 1) & 1) << (15 - bit_pos);
			lsb_plane |= ((idx >> 0) & 1) << (15 - bit_pos);
		}
	}

	out[4] = (uint8_t) (msb_plane >> 8);
	out[5] = (uint8_t) (msb_plane & 0xFF);
	out[6] = (uint8_t) (lsb_plane >> 8);
	out[7] = (uint8_t) (lsb_plane & 0xFF);
}

/* Encode one EAC alpha block (8 bytes) from a 4x4 pixel region.
 * Uses a simple base + multiplier + table approach. */
static void encode_eac_alpha_block(const uint8_t *pixels, int stride,
                                   uint8_t *out)
{
	/* Compute average alpha as base value */
	int sum_a = 0;
	int min_a = 255, max_a = 0;
	for (int y = 0; y < 4; y++) {
		const uint8_t *row = pixels + y * stride;
		for (int x = 0; x < 4; x++) {
			int a = row[x * 4 + 3];
			sum_a += a;
			if (a < min_a) min_a = a;
			if (a > max_a) max_a = a;
		}
	}
	int base = (sum_a + 8) / 16;

	/* Find best multiplier and table */
	int best_table = 0;
	int best_mult = 1;
	int best_error = 0x7fffffff;
	uint8_t best_indices[16];

	for (int t = 0; t < 16; t++) {
		/* Try a few multiplier values */
		int range = max_a - min_a;
		int mult_try;
		if (range == 0)
			mult_try = 1;
		else {
			/* Find max absolute modifier in this table */
			int max_mod = 0;
			for (int i = 0; i < 8; i++) {
				int am = eac_modifier_table[t][i];
				if (am < 0) am = -am;
				if (am > max_mod) max_mod = am;
			}
			mult_try = max_mod > 0 ? (range / 2 + max_mod - 1) / max_mod : 1;
		}
		if (mult_try < 1) mult_try = 1;
		if (mult_try > 15) mult_try = 15;

		/* Try mult_try and neighbors */
		for (int m = (mult_try > 1 ? mult_try - 1 : 1);
		     m <= (mult_try < 15 ? mult_try + 1 : 15); m++) {
			int error = 0;
			uint8_t indices[16];
			int pidx = 0;
			for (int y = 0; y < 4; y++) {
				const uint8_t *row = pixels + y * stride;
				for (int x = 0; x < 4; x++) {
					int a = row[x * 4 + 3];
					int best_d = 0x7fffffff;
					int best_i = 0;
					for (int i = 0; i < 8; i++) {
						int val = clamp255(base + eac_modifier_table[t][i] * m);
						int d = (a - val) * (a - val);
						if (d < best_d) {
							best_d = d;
							best_i = i;
						}
					}
					indices[pidx++] = (uint8_t) best_i;
					error += best_d;
				}
			}
			if (error < best_error) {
				best_error = error;
				best_table = t;
				best_mult = m;
				memcpy(best_indices, indices, 16);
			}
		}
	}

	/* Pack: byte 0 = base, byte 1 = multiplier[7:4] | table[3:0] */
	out[0] = (uint8_t) base;
	out[1] = (uint8_t) ((best_mult << 4) | best_table);

	/* Pack 16 x 3-bit indices into bytes 2-7 (48 bits total).
	 * Pixel order is column-major: col 0 rows 0-3, col 1 rows 0-3, ... */
	uint64_t bits = 0;
	for (int col = 0; col < 4; col++) {
		for (int row = 0; row < 4; row++) {
			int pidx = row * 4 + col; /* row-major in our indices array */
			/* But we stored them row-major, and EAC wants column-major.
			 * Pixel index in column-major: col*4+row = bit position
			 * (MSB first) */
			int bit_pos = col * 4 + row;
			bits |= ((uint64_t) (best_indices[pidx] & 7)) << (45 - bit_pos * 3);
		}
	}
	out[2] = (uint8_t) (bits >> 40);
	out[3] = (uint8_t) (bits >> 32);
	out[4] = (uint8_t) (bits >> 24);
	out[5] = (uint8_t) (bits >> 16);
	out[6] = (uint8_t) (bits >> 8);
	out[7] = (uint8_t) (bits);
}

uint8_t *etc2_compress_rgba(const uint8_t *src, int width, int height,
                            int has_alpha, size_t *out_size)
{
	if (!src || width <= 0 || height <= 0 || (width & 3) || (height & 3))
		return NULL;

	int blocks_x = width / 4;
	int blocks_y = height / 4;
	int total_blocks = blocks_x * blocks_y;
	size_t block_size = has_alpha ? ETC2_RGBA_BLOCK_SIZE : ETC2_RGB_BLOCK_SIZE;
	size_t size = (size_t) total_blocks * block_size;
	uint8_t *out = (uint8_t *) malloc(size);
	if (!out)
		return NULL;

	int stride = width * 4;
	uint8_t *dst = out;

	for (int by = 0; by < blocks_y; by++) {
		for (int bx = 0; bx < blocks_x; bx++) {
			const uint8_t *block = src + (by * 4) * stride + (bx * 4) * 4;
			if (has_alpha) {
				encode_eac_alpha_block(block, stride, dst);
				dst += 8;
				encode_etc2_rgb_block(block, stride, dst);
				dst += 8;
			} else {
				encode_etc2_rgb_block(block, stride, dst);
				dst += 8;
			}
		}
	}

	*out_size = size;
	return out;
}
