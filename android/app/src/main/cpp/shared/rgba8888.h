#ifndef DXX_RGBA8888_H
#define DXX_RGBA8888_H

#include <stdint.h>

static inline void rgba8888_store(uint8_t *destination, uint8_t red, uint8_t green,
                                  uint8_t blue, uint8_t alpha)
{
	destination[0] = red;
	destination[1] = green;
	destination[2] = blue;
	destination[3] = alpha;
}

#endif
