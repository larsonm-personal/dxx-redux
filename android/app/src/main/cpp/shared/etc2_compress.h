/* etc2_compress.h -- lightweight ETC2 texture compression for GLES 3.0
 *
 * CPU-side ETC2 compressor for real-time texture upload on Android.
 * Compresses RGBA8888 pixels into GL_COMPRESSED_RGB8_ETC2 (no alpha) or
 * GL_COMPRESSED_RGBA8_ETC2_EAC (with alpha) blocks.
 *
 * Quality is "fast" -- optimized for speed over quality. This is acceptable
 * because the source textures are only 128x128 low-poly game art.
 */

#ifndef ETC2_COMPRESS_H
#define ETC2_COMPRESS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compress RGBA8888 pixels into ETC2 format.
 *
 * src:        RGBA8888 pixel data (4 bytes per pixel, row-major)
 * width:      image width in pixels (must be multiple of 4)
 * height:     image height in pixels (must be multiple of 4)
 * has_alpha:  if nonzero, output GL_COMPRESSED_RGBA8_ETC2_EAC (16 bytes/block)
 *             otherwise output GL_COMPRESSED_RGB8_ETC2 (8 bytes/block)
 * out_size:   receives the size in bytes of the compressed output
 *
 * Returns malloc'd compressed data, or NULL on failure. Caller must free().
 */
uint8_t *etc2_compress_rgba(const uint8_t *src, int width, int height,
                            int has_alpha, size_t *out_size);

/* Compressed block sizes */
#define ETC2_RGB_BLOCK_SIZE  8  /* GL_COMPRESSED_RGB8_ETC2 */
#define ETC2_RGBA_BLOCK_SIZE 16 /* GL_COMPRESSED_RGBA8_ETC2_EAC */

#ifdef __cplusplus
}
#endif

#endif /* ETC2_COMPRESS_H */
