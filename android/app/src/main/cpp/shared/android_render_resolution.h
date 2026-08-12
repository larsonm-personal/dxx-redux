#ifndef ANDROID_RENDER_RESOLUTION_H
#define ANDROID_RENDER_RESOLUTION_H

#include <stddef.h>

#define ANDROID_RENDER_MIN_WIDTH     320u
#define ANDROID_RENDER_MIN_HEIGHT    200u
#define ANDROID_RENDER_MAX_DIMENSION 4096u
#define ANDROID_RENDER_MAX_PIXELS    (3840u * 2160u)

static inline int android_render_resolution_valid(unsigned int width, unsigned int height)
{
	return width >= ANDROID_RENDER_MIN_WIDTH && height >= ANDROID_RENDER_MIN_HEIGHT &&
	       width <= ANDROID_RENDER_MAX_DIMENSION && height <= ANDROID_RENDER_MAX_DIMENSION &&
	       (size_t) width * (size_t) height <= ANDROID_RENDER_MAX_PIXELS;
}

#endif
