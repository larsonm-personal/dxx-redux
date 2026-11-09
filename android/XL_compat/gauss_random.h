#ifndef DXX_XL_GAUSS_RANDOM_H
#define DXX_XL_GAUSS_RANDOM_H

// One existing simulation draw, interpreted for the ordinary damage profile
static inline int xl_gauss_signed_draw(int draw, int xl_singleplayer)
{
	return xl_singleplayer ? 16383 - draw : draw - 16384;
}
#endif
