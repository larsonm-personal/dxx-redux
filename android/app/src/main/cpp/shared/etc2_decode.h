/* Software ETC2 decoder -- android port work */
#ifndef ETC2_DECODE_H
#define ETC2_DECODE_H

#include <stdint.h>

/* Decode ETC2 RGB8 compressed data to RGBA8 (alpha = 255).
 * Returns malloc'd buffer of width*height*4 bytes, or NULL on failure.
 * Caller must free() the result. */
uint8_t *etc2_decode_rgb(const uint8_t *data, int width, int height);

/* Decode ETC2 RGBA8 (EAC) compressed data to RGBA8.
 * Returns malloc'd buffer of width*height*4 bytes, or NULL on failure.
 * Caller must free() the result. */
uint8_t *etc2_decode_rgba(const uint8_t *data, int width, int height);

#endif
