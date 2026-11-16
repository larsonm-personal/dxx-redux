#ifndef DXX_XL_TEXTURE_FRAME_H
#define DXX_XL_TEXTURE_FRAME_H

/* CreateFrames keeps only the two transparency bits on an RGBA child view */
static inline unsigned dxx_xl_texture_frame_flags(const unsigned transparent[4], const unsigned super[4], unsigned frame)
{
	unsigned mask;
	if (frame >= 128) return 0;
	mask = 1u << (frame % 32);
	return ((transparent[frame / 32] & mask) ? 1u : 0u) |
	       ((super[frame / 32] & mask) ? 2u : 0u);
}
#endif
