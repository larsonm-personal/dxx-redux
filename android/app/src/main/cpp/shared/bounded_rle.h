#ifndef BOUNDED_RLE_H
#define BOUNDED_RLE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int bounded_rle_validate_bitmap(const unsigned char *data, size_t data_size,
                                int width, int height, int row_size_bytes);

#ifdef __cplusplus
}
#endif

#endif
