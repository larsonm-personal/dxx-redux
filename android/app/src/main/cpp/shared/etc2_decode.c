/* Software ETC2 RGB8 / RGBA8 (EAC) decoder
 * Used as fallback when GPU ETC2 decoding is broken (emulator GLES translator)
 *
 * Reference: Khronos ETC2 specification (OpenGL ES 3.0, Appendix C)
 * android port work */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ETC1 modifier tables (Table C.3) */
static const int etc1_modifier_table[8][4] = {
	{  2,   8,  -2,   -8},
	{  5,  17,  -5,  -17},
	{  9,  29,  -9,  -29},
	{ 13,  42, -13,  -42},
	{ 18,  60, -18,  -60},
	{ 24,  80, -24,  -80},
	{ 33, 106, -33, -106},
	{ 47, 150, -47, -150},
};

/* EAC modifier table (Table C.12) */
static const int eac_modifier_table[16][8] = {
	{ -3, -6,  -9, -15, 2, 5, 8, 14},
	{ -3, -7, -10, -13, 2, 6, 9, 12},
	{ -2, -5,  -8, -13, 1, 4, 7, 12},
	{ -2, -4,  -6, -13, 1, 3, 5, 12},
	{ -3, -6,  -8, -12, 2, 5, 7, 11},
	{ -3, -7,  -9, -11, 2, 6, 8, 10},
	{ -4, -7,  -8, -11, 3, 6, 7, 10},
	{ -3, -5,  -8, -11, 2, 4, 7, 10},
	{ -2, -6,  -8, -10, 1, 5, 7,  9},
	{ -2, -5,  -8, -10, 1, 4, 7,  9},
	{ -2, -4,  -8, -10, 1, 3, 7,  9},
	{ -2, -5,  -7, -10, 1, 4, 6,  9},
	{ -3, -4,  -7, -10, 2, 3, 6,  9},
	{ -1, -2,  -3, -10, 0, 1, 2,  9},
	{ -4, -6,  -8,  -9, 3, 5, 7,  8},
	{ -3, -5,  -7,  -9, 2, 4, 6,  8},
};

/* "Distance" table for T and H modes (Table C.8) */
static const int etc2_distance_table[8] = {
	3, 6, 11, 16, 23, 32, 47, 64
};

static int clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

static uint64_t read_be64(const uint8_t *p)
{
	return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) |
	       ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) |
	       ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) |
	       ((uint64_t)p[6] <<  8) | (uint64_t)p[7];
}

/* Extend n-bit value to 8-bit by replicating high bits into low */
static int extend_4to8(int v) { return (v << 4) | v; }
static int extend_5to8(int v) { return (v << 3) | (v >> 2); }
static int extend_6to8(int v) { return (v << 2) | (v >> 4); }
static int extend_7to8(int v) { return (v << 1) | (v >> 6); }

/* Sign-extend a 3-bit value */
static int sign_extend_3(int v) { return (v & 4) ? (v | ~7) : v; }

/* Pixel index for a given (x,y) within a 4x4 block */
static int get_pixel_index(uint64_t block, int x, int y)
{
	int bit = y * 4 + x;
	int msb = (int)((block >> (bit + 16)) & 1);
	int lsb = (int)((block >> bit) & 1);
	return (msb << 1) | lsb;
}

/* Decode an ETC2 RGB8 4x4 block into rgba output (stride = row bytes) */
static void decode_etc2_rgb_block(const uint8_t *src, uint8_t *out, int stride)
{
	uint64_t b = read_be64(src);
	int diff = (int)((b >> 33) & 1);
	int flip = (int)((b >> 32) & 1);

	int R1 = (int)((b >> 59) & 0x1F);
	int G1 = (int)((b >> 51) & 0x1F);
	int B1 = (int)((b >> 43) & 0x1F);

	if (!diff) {
		/* Individual mode: two independent 4-bit base colors */
		int r1 = extend_4to8((int)((b >> 60) & 0xF));
		int g1 = extend_4to8((int)((b >> 52) & 0xF));
		int b1 = extend_4to8((int)((b >> 44) & 0xF));
		int r2 = extend_4to8((int)((b >> 56) & 0xF));
		int g2 = extend_4to8((int)((b >> 48) & 0xF));
		int b2 = extend_4to8((int)((b >> 40) & 0xF));
		int tw1 = (int)((b >> 37) & 7);
		int tw2 = (int)((b >> 34) & 7);
		int x, y;
		for (y = 0; y < 4; y++) {
			for (x = 0; x < 4; x++) {
				int sub = flip ? (y >= 2) : (x >= 2);
				int idx = get_pixel_index(b, x, y);
				int cr = sub ? r2 : r1;
				int cg = sub ? g2 : g1;
				int cb = sub ? b2 : b1;
				int tw = sub ? tw2 : tw1;
				int mod = etc1_modifier_table[tw][idx];
				uint8_t *p = out + y * stride + x * 4;
				p[0] = (uint8_t)clamp255(cr + mod);
				p[1] = (uint8_t)clamp255(cg + mod);
				p[2] = (uint8_t)clamp255(cb + mod);
				p[3] = 255;
			}
		}
	} else {
		int dR = sign_extend_3((int)((b >> 56) & 7));
		int dG = sign_extend_3((int)((b >> 48) & 7));
		int dB = sign_extend_3((int)((b >> 40) & 7));
		int R2 = R1 + dR;
		int G2 = G1 + dG;
		int B2 = B1 + dB;

		if (R2 < 0 || R2 > 31) {
			/* T mode */
			/* Reconstruct base colors from scrambled bits */
			int r1a = (int)((b >> 59) & 3);
			int r1b = (int)((b >> 56) & 3);
			int tr1 = extend_4to8((r1a << 2) | r1b);
			int tg1 = extend_4to8((int)((b >> 52) & 0xF));
			int tb1 = extend_4to8((int)((b >> 48) & 0xF));
			int tr2 = extend_4to8((int)((b >> 44) & 0xF));
			int tg2 = extend_4to8((int)((b >> 40) & 0xF));
			int tb2 = extend_4to8((int)((b >> 36) & 0xF));
			int da = (int)((b >> 34) & 1);
			int db_msb = (int)((b >> 33) & 1);
			int db_lsb = (int)((b >> 32) & 1);
			int dist = etc2_distance_table[(da << 2) | (db_msb << 1) | db_lsb];

			/* T mode paint colors:
			 * 0: color1
			 * 1: color2 + dist
			 * 2: color2
			 * 3: color2 - dist */
			int colors[4][3];
			colors[0][0] = tr1; colors[0][1] = tg1; colors[0][2] = tb1;
			colors[1][0] = clamp255(tr2 + dist);
			colors[1][1] = clamp255(tg2 + dist);
			colors[1][2] = clamp255(tb2 + dist);
			colors[2][0] = tr2; colors[2][1] = tg2; colors[2][2] = tb2;
			colors[3][0] = clamp255(tr2 - dist);
			colors[3][1] = clamp255(tg2 - dist);
			colors[3][2] = clamp255(tb2 - dist);

			int x, y;
			for (y = 0; y < 4; y++) {
				for (x = 0; x < 4; x++) {
					int idx = get_pixel_index(b, x, y);
					uint8_t *p = out + y * stride + x * 4;
					p[0] = (uint8_t)colors[idx][0];
					p[1] = (uint8_t)colors[idx][1];
					p[2] = (uint8_t)colors[idx][2];
					p[3] = 255;
				}
			}
		} else if (G2 < 0 || G2 > 31) {
			/* H mode */
			int hr1 = extend_4to8((int)((b >> 59) & 0xF));
			int hg1a = (int)((b >> 56) & 7);
			int hg1b = (int)((b >> 52) & 1);
			int hg1 = extend_4to8((hg1a << 1) | hg1b);
			int hb1a = (int)((b >> 49) & 1);
			int hb1b = (int)((b >> 44) & 7);
			int hb1 = extend_4to8((hb1a << 3) | hb1b);
			int hr2 = extend_4to8((int)((b >> 40) & 0xF));
			int hg2 = extend_4to8((int)((b >> 36) & 0xF));
			int hb2 = extend_4to8((int)((b >> 32) & 0xF));
			/* Distance index from bit 34 and bit 32 */
			int da = (int)((b >> 34) & 1);
			int db = (int)((b >> 32) & 1);
			/* Ordering bit: compare packed RGB values of color1 vs color2 */
			int v1 = (hr1 << 16) | (hg1 << 8) | hb1;
			int v2 = (hr2 << 16) | (hg2 << 8) | hb2;
			int ordering_bit = (v1 >= v2) ? 1 : 0;
			int dist = etc2_distance_table[(da << 2) | (db << 1) | ordering_bit];

			/* H mode paint colors:
			 * 0: color1 + dist
			 * 1: color1 - dist
			 * 2: color2 + dist
			 * 3: color2 - dist */
			int colors[4][3];
			colors[0][0] = clamp255(hr1 + dist); colors[0][1] = clamp255(hg1 + dist); colors[0][2] = clamp255(hb1 + dist);
			colors[1][0] = clamp255(hr1 - dist); colors[1][1] = clamp255(hg1 - dist); colors[1][2] = clamp255(hb1 - dist);
			colors[2][0] = clamp255(hr2 + dist); colors[2][1] = clamp255(hg2 + dist); colors[2][2] = clamp255(hb2 + dist);
			colors[3][0] = clamp255(hr2 - dist); colors[3][1] = clamp255(hg2 - dist); colors[3][2] = clamp255(hb2 - dist);

			int x, y;
			for (y = 0; y < 4; y++) {
				for (x = 0; x < 4; x++) {
					int idx = get_pixel_index(b, x, y);
					uint8_t *p = out + y * stride + x * 4;
					p[0] = (uint8_t)colors[idx][0];
					p[1] = (uint8_t)colors[idx][1];
					p[2] = (uint8_t)colors[idx][2];
					p[3] = 255;
				}
			}
		} else if (B2 < 0 || B2 > 31) {
			/* Planar mode -- bit positions from Khronos ETC2 spec Table C.7d
			 * Bits are numbered MSB=63..LSB=0 in the 64-bit big-endian block.
			 * Mode-detection bits (57, 46:45, 40) are skipped in color fields. */
			int RO = extend_6to8((int)((b >> 58) & 0x3F));
			int GO = extend_7to8((int)((b >> 50) & 0x7F));
			/* BO: bits[49:48](2) + bit[47](1) + bits[44:42](3) = 6 bits */
			int BO = extend_6to8(((int)((b >> 48) & 3) << 4) |
			                     ((int)((b >> 47) & 1) << 3) |
			                     (int)((b >> 42) & 7));
			/* RH: bit[41](1) + bits[39:35](5) = 6 bits */
			int RH = extend_6to8(((int)((b >> 41) & 1) << 5) |
			                     (int)((b >> 35) & 0x1F));
			int GH = extend_7to8((int)((b >> 28) & 0x7F));
			int BH = extend_6to8((int)((b >> 22) & 0x3F));
			int RV = extend_6to8((int)((b >> 16) & 0x3F));
			int GV = extend_7to8((int)((b >> 9) & 0x7F));
			int BV = extend_6to8((int)((b >> 3) & 0x3F));

			int x, y;
			for (y = 0; y < 4; y++) {
				for (x = 0; x < 4; x++) {
					uint8_t *p = out + y * stride + x * 4;
					p[0] = (uint8_t)clamp255((x * (RH - RO) + y * (RV - RO) + 4 * RO + 2) >> 2);
					p[1] = (uint8_t)clamp255((x * (GH - GO) + y * (GV - GO) + 4 * GO + 2) >> 2);
					p[2] = (uint8_t)clamp255((x * (BH - BO) + y * (BV - BO) + 4 * BO + 2) >> 2);
					p[3] = 255;
				}
			}
		} else {
			/* Differential mode */
			int r1 = extend_5to8(R1);
			int g1 = extend_5to8(G1);
			int b1 = extend_5to8(B1);
			int r2 = extend_5to8(R2);
			int g2 = extend_5to8(G2);
			int b2 = extend_5to8(B2);
			int tw1 = (int)((b >> 37) & 7);
			int tw2 = (int)((b >> 34) & 7);
			int x, y;
			for (y = 0; y < 4; y++) {
				for (x = 0; x < 4; x++) {
					int sub = flip ? (y >= 2) : (x >= 2);
					int idx = get_pixel_index(b, x, y);
					int cr = sub ? r2 : r1;
					int cg = sub ? g2 : g1;
					int cb = sub ? b2 : b1;
					int tw = sub ? tw2 : tw1;
					int mod = etc1_modifier_table[tw][idx];
					uint8_t *p = out + y * stride + x * 4;
					p[0] = (uint8_t)clamp255(cr + mod);
					p[1] = (uint8_t)clamp255(cg + mod);
					p[2] = (uint8_t)clamp255(cb + mod);
					p[3] = 255;
				}
			}
		}
	}
}

/* Decode EAC alpha block (8 bytes) into alpha channel of RGBA output */
static void decode_eac_alpha_block(const uint8_t *src, uint8_t *out, int stride)
{
	int base = src[0];
	int multiplier = (src[1] >> 4) & 0xF;
	int table_idx = src[1] & 0xF;
	/* 48 bits of 3-bit indices, packed MSB first starting at bit 47 */
	uint64_t bits = ((uint64_t)src[2] << 40) | ((uint64_t)src[3] << 32) |
	                ((uint64_t)src[4] << 24) | ((uint64_t)src[5] << 16) |
	                ((uint64_t)src[6] <<  8) | (uint64_t)src[7];

	int x, y;
	for (y = 0; y < 4; y++) {
		for (x = 0; x < 4; x++) {
			int pixel_idx = y * 4 + x;
			int shift = 45 - pixel_idx * 3;
			int idx = (int)((bits >> shift) & 7);
			int mod = eac_modifier_table[table_idx][idx];
			int alpha = base + (multiplier ? multiplier * mod : mod);
			out[y * stride + x * 4 + 3] = (uint8_t)clamp255(alpha);
		}
	}
}

/* Decode a complete ETC2 RGB8 image from compressed blocks to RGBA */
uint8_t *etc2_decode_rgb(const uint8_t *data, int width, int height)
{
	int bw = width / 4, bh = height / 4;
	int stride = width * 4;
	uint8_t *out = (uint8_t *)malloc(stride * height);
	if (!out) return NULL;

	int bx, by;
	for (by = 0; by < bh; by++) {
		for (bx = 0; bx < bw; bx++) {
			decode_etc2_rgb_block(data, out + by * 4 * stride + bx * 16, stride);
			data += 8;
		}
	}
	return out;
}

/* Decode a complete ETC2 RGBA8 (EAC) image from compressed blocks to RGBA */
uint8_t *etc2_decode_rgba(const uint8_t *data, int width, int height)
{
	int bw = width / 4, bh = height / 4;
	int stride = width * 4;
	uint8_t *out = (uint8_t *)malloc(stride * height);
	if (!out) return NULL;

	int bx, by;
	for (by = 0; by < bh; by++) {
		for (bx = 0; bx < bw; bx++) {
			uint8_t *dst = out + by * 4 * stride + bx * 16;
			decode_eac_alpha_block(data, dst, stride);  /* first 8 bytes: alpha */
			decode_etc2_rgb_block(data + 8, dst, stride); /* next 8 bytes: RGB */
			data += 16;
		}
	}
	return out;
}
