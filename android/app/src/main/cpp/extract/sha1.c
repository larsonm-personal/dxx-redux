/*
 * Copyright (C) 2011-2020 Daniel Scharrer
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the author(s) be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

/* SHA-1 for installer integrity and CD fingerprints */
#include "sha1.h"

#include <string.h>

static uint32_t sha1_rotate_left(uint32_t value, unsigned bits)
{
	return (value << bits) | (value >> (32 - bits));
}

static void dxx_sha1_transform(dxx_sha1_ctx_t *ctx, const uint8_t block[64])
{
	uint32_t words[80];
	for (int i = 0; i < 16; i++) {
		words[i] = ((uint32_t) block[i * 4] << 24) |
		           ((uint32_t) block[i * 4 + 1] << 16) |
		           ((uint32_t) block[i * 4 + 2] << 8) |
		           (uint32_t) block[i * 4 + 3];
	}
	for (int i = 16; i < 80; i++)
		words[i] = sha1_rotate_left(words[i - 3] ^ words[i - 8] ^
		                                words[i - 14] ^ words[i - 16],
		                            1);

	uint32_t a = ctx->state[0];
	uint32_t b = ctx->state[1];
	uint32_t c = ctx->state[2];
	uint32_t d = ctx->state[3];
	uint32_t e = ctx->state[4];
	for (int i = 0; i < 80; i++) {
		uint32_t f;
		uint32_t k;
		if (i < 20) {
			f = (b & c) | ((~b) & d);
			k = 0x5A827999U;
		} else if (i < 40) {
			f = b ^ c ^ d;
			k = 0x6ED9EBA1U;
		} else if (i < 60) {
			f = (b & c) | (b & d) | (c & d);
			k = 0x8F1BBCDCU;
		} else {
			f = b ^ c ^ d;
			k = 0xCA62C1D6U;
		}
		uint32_t next = sha1_rotate_left(a, 5) + f + e + k + words[i];
		e = d;
		d = c;
		c = sha1_rotate_left(b, 30);
		b = a;
		a = next;
	}
	ctx->state[0] += a;
	ctx->state[1] += b;
	ctx->state[2] += c;
	ctx->state[3] += d;
	ctx->state[4] += e;
}

void dxx_sha1_init(dxx_sha1_ctx_t *ctx)
{
	ctx->state[0] = 0x67452301U;
	ctx->state[1] = 0xEFCDAB89U;
	ctx->state[2] = 0x98BADCFEU;
	ctx->state[3] = 0x10325476U;
	ctx->state[4] = 0xC3D2E1F0U;
	ctx->bytes = 0;
	ctx->block_len = 0;
}

void dxx_sha1_update(dxx_sha1_ctx_t *ctx, const uint8_t *data, size_t len)
{
	ctx->bytes += len;
	while (len > 0) {
		size_t count = sizeof(ctx->block) - ctx->block_len;
		if (count > len) count = len;
		memcpy(ctx->block + ctx->block_len, data, count);
		ctx->block_len += count;
		data += count;
		len -= count;
		if (ctx->block_len == sizeof(ctx->block)) {
			dxx_sha1_transform(ctx, ctx->block);
			ctx->block_len = 0;
		}
	}
}

void dxx_sha1_final(dxx_sha1_ctx_t *ctx, uint8_t digest[20])
{
	uint64_t bit_count = ctx->bytes * 8;
	uint8_t padding[72] = { 0x80 };
	size_t padding_len = ctx->block_len < 56 ? 56 - ctx->block_len : 120 - ctx->block_len;
	for (int i = 0; i < 8; i++)
		padding[padding_len + i] = (uint8_t) (bit_count >> (56 - i * 8));
	dxx_sha1_update(ctx, padding, padding_len + 8);
	for (int i = 0; i < 5; i++) {
		digest[i * 4] = (uint8_t) (ctx->state[i] >> 24);
		digest[i * 4 + 1] = (uint8_t) (ctx->state[i] >> 16);
		digest[i * 4 + 2] = (uint8_t) (ctx->state[i] >> 8);
		digest[i * 4 + 3] = (uint8_t) ctx->state[i];
	}
}

void dxx_sha1_hex(const uint8_t digest[20], char hex[41])
{
	static const char digits[] = "0123456789abcdef";
	for (int i = 0; i < 20; i++) {
		hex[i * 2] = digits[digest[i] >> 4];
		hex[i * 2 + 1] = digits[digest[i] & 15];
	}
	hex[40] = '\0';
}
