#ifndef DXX_XL_RGBA_PIXELS_H
#define DXX_XL_RGBA_PIXELS_H
#include <stdint.h>

/* POG pixels retain XL's purple super-transparency key independently of alpha */
static inline int dxx_xl_rgba_transparency(const unsigned char *pixel)
{
	if (pixel[0] == 120 && pixel[1] == 88 && pixel[2] == 128) return -1;
	return pixel[3] ? 0 : 1;
}

/* Defined 32-bit wrapping reproduces PixelTranspType's fixed-point coordinates */
static inline unsigned dxx_xl_texture_coordinate(int32_t coordinate, unsigned extent)
{
	uint32_t whole = ((uint32_t) coordinate * extent) >> 16;
	if (whole & 0x8000u) whole |= 0xffff0000u;
	return whole % extent;
}
#endif
