#ifndef DXX_EXTRACT_SHA1_H
#define DXX_EXTRACT_SHA1_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	uint32_t state[5];
	uint64_t bytes;
	uint8_t block[64];
	size_t block_len;
} dxx_sha1_ctx_t;

void dxx_sha1_init(dxx_sha1_ctx_t *ctx);
void dxx_sha1_update(dxx_sha1_ctx_t *ctx, const uint8_t *data, size_t len);
void dxx_sha1_final(dxx_sha1_ctx_t *ctx, uint8_t digest[20]);
void dxx_sha1_hex(const uint8_t digest[20], char hex[41]);

#ifdef __cplusplus
}
#endif

#endif
