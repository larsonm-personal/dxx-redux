#ifndef DXX_XL_TEXTURE_PIXELS_H
#define DXX_XL_TEXTURE_PIXELS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int xl_texture_pixel_transparency(int texture, int orientation, int32_t u, int32_t v);
int xl_side_pixel_transparency(int base, int overlay, int32_t u, int32_t v);
int xl_side_texture_transparency(int segment, int side);
#ifdef __cplusplus
}
#endif
#endif
