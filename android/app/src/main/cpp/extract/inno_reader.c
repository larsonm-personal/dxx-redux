/*
 * inno_reader.c — InnoSetup 5.3–5.6 archive reader.
 * this is a single file AI-slop implementation in order to keep this app simple and not have boost
 * the AI tool mostly used the innoextract codebase, which is zlib-style-licensed
 * because the tool didn't use much else, I'm choosing to include the original license notice
 * https://github.com/dscharrer/innoextract
 *
 * Implements the minimal reading pipeline:
 *   1. Find offset table (PE resource 11111 or fixed offset 0x30)
 *   2. Read setup-0.bin header (CRC-chunked + LZMA1-compressed)
 *   3. Parse file/data entries from the decompressed header
 *   4. Extract files from setup-1.bin data chunks
 *
 * Only LZMA1 and zlib decompression are implemented.
 * BZip2, LZMA2, encryption, and exe-call filters return errors.
 */

/*
 * Maintenance note: the historical preamble above is preserved verbatim.
 * Its capability claims are superseded by the implementation-checked matrix
 * in INNO_READER_CAPABILITIES.md.
 */

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

#include "inno_reader.h"

#include "extract_limits.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#define OPEN_RB(path)       _open((path), _O_RDONLY | _O_BINARY)
#define READ_FD(fd, buf, n) _read((fd), (buf), (unsigned) (n))
#define LSEEK(fd, off, w)   _lseeki64((fd), (off), (w))
#define CLOSE_FD(fd)        _close(fd)
#define DUP_FD(fd)          _dup(fd)
#define GET_PID()           _getpid()
#include <direct.h>
#define MKDIR(d) _mkdir(d)
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#define OPEN_RB(path)       open((path), O_RDONLY)
#define READ_FD(fd, buf, n) read((fd), (buf), (n))
#define LSEEK(fd, off, w)   lseek((fd), (off), (w))
#define CLOSE_FD(fd)        close(fd)
#define DUP_FD(fd)          dup(fd)
#define GET_PID()           getpid()
#define MKDIR(d)            mkdir((d), 0755)
#ifndef O_BINARY
#define O_BINARY 0
#endif
#endif

#include <LzmaDec.h>
#include <Lzma2Dec.h>
#include <zlib.h>

/* ── Logging ─────────────────────────────────────────────────────── */
#ifdef ANDROID
#include <android/log.h>
#define INNO_LOG(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, "inno_reader", fmt, ##__VA_ARGS__)
#else
#define INNO_LOG(fmt, ...) fprintf(stderr, "inno_reader: " fmt "\n", ##__VA_ARGS__)
#endif

/* ── LZMA SDK allocator ──────────────────────────────────────────── */
static const uint64_t INNO_STREAM_DIRECT_THRESHOLD = 64ULL * 1024ULL * 1024ULL;
#define INNO_STREAM_BUFFER_SIZE 65536
#define INNO_VERSION_ID_SIZE    64
/* Ten empty length-prefixed strings plus 43 fixed bytes in supported versions */
#define INNO_MIN_FILE_ENTRY_SIZE 83

#ifdef INNO_READER_TESTING
static int inno_test_allocations_before_failure = -1;
#endif

static void *inno_calloc(size_t count, size_t size)
{
#ifdef INNO_READER_TESTING
	if (inno_test_allocations_before_failure == 0)
		return NULL;
	if (inno_test_allocations_before_failure > 0)
		inno_test_allocations_before_failure--;
#endif
	return calloc(count, size);
}

static void *lzma_alloc(ISzAllocPtr p, size_t size)
{
	(void) p;
	return malloc(size);
}
static void lzma_free(ISzAllocPtr p, void *addr)
{
	(void) p;
	free(addr);
}
static const ISzAlloc g_lzma_alloc = { lzma_alloc, lzma_free };

/* ── CRC32 (IEEE 802.3, same as zlib/InnoSetup) ─────────────────── */
static uint32_t inno_crc32(const uint8_t *data, size_t len)
{
	return (uint32_t) crc32(crc32(0, Z_NULL, 0), data, (uInt) len);
}

/* -- SHA-1 (RFC 3174, used by Inno 5.3.9 and newer) ---------------- */
typedef struct {
	uint32_t state[5];
	uint64_t bytes;
	uint8_t block[64];
	size_t block_len;
} inno_sha1_ctx_t;

static uint32_t sha1_rotate_left(uint32_t value, unsigned bits)
{
	return (value << bits) | (value >> (32 - bits));
}

static void inno_sha1_transform(inno_sha1_ctx_t *ctx, const uint8_t block[64])
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

static void inno_sha1_init(inno_sha1_ctx_t *ctx)
{
	ctx->state[0] = 0x67452301U;
	ctx->state[1] = 0xEFCDAB89U;
	ctx->state[2] = 0x98BADCFEU;
	ctx->state[3] = 0x10325476U;
	ctx->state[4] = 0xC3D2E1F0U;
	ctx->bytes = 0;
	ctx->block_len = 0;
}

static void inno_sha1_update(inno_sha1_ctx_t *ctx, const uint8_t *data, size_t len)
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
			inno_sha1_transform(ctx, ctx->block);
			ctx->block_len = 0;
		}
	}
}

static void inno_sha1_final(inno_sha1_ctx_t *ctx, uint8_t digest[20])
{
	uint64_t bit_count = ctx->bytes * 8;
	uint8_t padding[72] = { 0x80 };
	size_t padding_len = ctx->block_len < 56 ? 56 - ctx->block_len : 120 - ctx->block_len;
	for (int i = 0; i < 8; i++)
		padding[padding_len + i] = (uint8_t) (bit_count >> (56 - i * 8));
	inno_sha1_update(ctx, padding, padding_len + 8);
	for (int i = 0; i < 5; i++) {
		digest[i * 4] = (uint8_t) (ctx->state[i] >> 24);
		digest[i * 4 + 1] = (uint8_t) (ctx->state[i] >> 16);
		digest[i * 4 + 2] = (uint8_t) (ctx->state[i] >> 8);
		digest[i * 4 + 3] = (uint8_t) ctx->state[i];
	}
}

/* -- MD5 (RFC 1321, used for Inno file integrity before 5.3.9) ----- */
typedef struct {
	uint32_t state[4];
	uint64_t bytes;
	uint8_t block[64];
	size_t block_len;
} inno_md5_ctx_t;

static uint32_t md5_rotate_left(uint32_t value, unsigned bits)
{
	return (value << bits) | (value >> (32 - bits));
}

static void inno_md5_transform(inno_md5_ctx_t *ctx, const uint8_t block[64])
{
	static const uint32_t constants[64] = {
		0xd76aa478U, 0xe8c7b756U, 0x242070dbU, 0xc1bdceeeU,
		0xf57c0fafU, 0x4787c62aU, 0xa8304613U, 0xfd469501U,
		0x698098d8U, 0x8b44f7afU, 0xffff5bb1U, 0x895cd7beU,
		0x6b901122U, 0xfd987193U, 0xa679438eU, 0x49b40821U,
		0xf61e2562U, 0xc040b340U, 0x265e5a51U, 0xe9b6c7aaU,
		0xd62f105dU, 0x02441453U, 0xd8a1e681U, 0xe7d3fbc8U,
		0x21e1cde6U, 0xc33707d6U, 0xf4d50d87U, 0x455a14edU,
		0xa9e3e905U, 0xfcefa3f8U, 0x676f02d9U, 0x8d2a4c8aU,
		0xfffa3942U, 0x8771f681U, 0x6d9d6122U, 0xfde5380cU,
		0xa4beea44U, 0x4bdecfa9U, 0xf6bb4b60U, 0xbebfbc70U,
		0x289b7ec6U, 0xeaa127faU, 0xd4ef3085U, 0x04881d05U,
		0xd9d4d039U, 0xe6db99e5U, 0x1fa27cf8U, 0xc4ac5665U,
		0xf4292244U, 0x432aff97U, 0xab9423a7U, 0xfc93a039U,
		0x655b59c3U, 0x8f0ccc92U, 0xffeff47dU, 0x85845dd1U,
		0x6fa87e4fU, 0xfe2ce6e0U, 0xa3014314U, 0x4e0811a1U,
		0xf7537e82U, 0xbd3af235U, 0x2ad7d2bbU, 0xeb86d391U
	};
	static const uint8_t shifts[64] = {
		7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
		5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
		4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
		6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
	};
	uint32_t words[16];
	for (int i = 0; i < 16; i++) {
		words[i] = (uint32_t) block[i * 4] |
		           ((uint32_t) block[i * 4 + 1] << 8) |
		           ((uint32_t) block[i * 4 + 2] << 16) |
		           ((uint32_t) block[i * 4 + 3] << 24);
	}
	uint32_t a = ctx->state[0];
	uint32_t b = ctx->state[1];
	uint32_t c = ctx->state[2];
	uint32_t d = ctx->state[3];
	for (int i = 0; i < 64; i++) {
		uint32_t f;
		int word;
		if (i < 16) {
			f = (b & c) | ((~b) & d);
			word = i;
		} else if (i < 32) {
			f = (d & b) | ((~d) & c);
			word = (5 * i + 1) & 15;
		} else if (i < 48) {
			f = b ^ c ^ d;
			word = (3 * i + 5) & 15;
		} else {
			f = c ^ (b | (~d));
			word = (7 * i) & 15;
		}
		uint32_t next = d;
		d = c;
		c = b;
		b += md5_rotate_left(a + f + constants[i] + words[word], shifts[i]);
		a = next;
	}
	ctx->state[0] += a;
	ctx->state[1] += b;
	ctx->state[2] += c;
	ctx->state[3] += d;
}

static void inno_md5_init(inno_md5_ctx_t *ctx)
{
	ctx->state[0] = 0x67452301U;
	ctx->state[1] = 0xefcdab89U;
	ctx->state[2] = 0x98badcfeU;
	ctx->state[3] = 0x10325476U;
	ctx->bytes = 0;
	ctx->block_len = 0;
}

static void inno_md5_update(inno_md5_ctx_t *ctx, const uint8_t *data, size_t len)
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
			inno_md5_transform(ctx, ctx->block);
			ctx->block_len = 0;
		}
	}
}

static void inno_md5_final(inno_md5_ctx_t *ctx, uint8_t digest[16])
{
	uint64_t bit_count = ctx->bytes * 8;
	uint8_t padding[72] = { 0x80 };
	size_t padding_len = ctx->block_len < 56 ? 56 - ctx->block_len : 120 - ctx->block_len;
	for (int i = 0; i < 8; i++)
		padding[padding_len + i] = (uint8_t) (bit_count >> (i * 8));
	inno_md5_update(ctx, padding, padding_len + 8);
	for (int i = 0; i < 4; i++) {
		digest[i * 4] = (uint8_t) ctx->state[i];
		digest[i * 4 + 1] = (uint8_t) (ctx->state[i] >> 8);
		digest[i * 4 + 2] = (uint8_t) (ctx->state[i] >> 16);
		digest[i * 4 + 3] = (uint8_t) (ctx->state[i] >> 24);
	}
}

typedef struct {
	inno_checksum_type_t type;
	union {
		inno_md5_ctx_t md5;
		inno_sha1_ctx_t sha1;
	} state;
} inno_checksum_ctx_t;

static int inno_checksum_init(inno_checksum_ctx_t *ctx,
                              inno_checksum_type_t type)
{
	ctx->type = type;
	if (type == INNO_CHECKSUM_MD5)
		inno_md5_init(&ctx->state.md5);
	else if (type == INNO_CHECKSUM_SHA1)
		inno_sha1_init(&ctx->state.sha1);
	else
		return -1;
	return 0;
}

static void inno_checksum_update(inno_checksum_ctx_t *ctx,
                                 const uint8_t *data, size_t len)
{
	if (ctx->type == INNO_CHECKSUM_MD5)
		inno_md5_update(&ctx->state.md5, data, len);
	else
		inno_sha1_update(&ctx->state.sha1, data, len);
}

static int inno_checksum_matches(inno_checksum_ctx_t *ctx,
                                 const inno_data_entry_t *de,
                                 const char *destination)
{
	uint8_t digest[20];
	size_t digest_size;
	if (ctx->type != de->checksum_type) {
		INNO_LOG("missing supported file checksum for %s", destination);
		return -1;
	}
	if (ctx->type == INNO_CHECKSUM_MD5) {
		inno_md5_final(&ctx->state.md5, digest);
		digest_size = 16;
	} else {
		inno_sha1_final(&ctx->state.sha1, digest);
		digest_size = 20;
	}
	if (memcmp(digest, de->checksum, digest_size) != 0) {
		INNO_LOG("file checksum mismatch for %s", destination);
		return -1;
	}
	return 0;
}

#ifdef _WIN32
static wchar_t *utf8_to_wide_path(const char *path)
{
	int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
	                                path, -1, NULL, 0);
	if (count <= 0 || (size_t) count > SIZE_MAX / sizeof(wchar_t))
		return NULL;
	wchar_t *wide = (wchar_t *) malloc((size_t) count * sizeof(*wide));
	if (!wide)
		return NULL;
	if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
	                        path, -1, wide, count) != count) {
		free(wide);
		return NULL;
	}
	return wide;
}
#endif

static int remove_output_path(const char *path)
{
#ifdef _WIN32
	wchar_t *wide = utf8_to_wide_path(path);
	if (!wide)
		return -1;
	int result = _wremove(wide);
	free(wide);
	return result;
#else
	return remove(path);
#endif
}

static FILE *open_temporary_output(const char *output_path, char **temporary_path_out)
{
	size_t path_len = strlen(output_path);
	char *temporary_path = (char *) malloc(path_len + 48);
	if (!temporary_path) return NULL;
	for (unsigned attempt = 0; attempt < 100; attempt++) {
		snprintf(temporary_path, path_len + 48, "%s.inno.%ld.%u.tmp",
		         output_path, (long) GET_PID(), attempt);
#ifdef _WIN32
		wchar_t *wide = utf8_to_wide_path(temporary_path);
		if (!wide) {
			free(temporary_path);
			return NULL;
		}
		FILE *out = _wfopen(wide, L"wbx");
		free(wide);
#else
		FILE *out = fopen(temporary_path, "wbx");
#endif
		if (out) {
			*temporary_path_out = temporary_path;
			return out;
		}
		if (errno != EEXIST) break;
	}
	INNO_LOG("cannot create temporary output for %s: %s", output_path, strerror(errno));
	free(temporary_path);
	return NULL;
}

static int commit_temporary_output(const char *temporary_path, const char *output_path)
{
#ifdef _WIN32
	wchar_t *wide_temporary = utf8_to_wide_path(temporary_path);
	wchar_t *wide_output = utf8_to_wide_path(output_path);
	if (!wide_temporary || !wide_output ||
	    !MoveFileExW(wide_temporary, wide_output,
	                 MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		INNO_LOG("cannot commit %s: Windows error %lu", output_path,
		         (unsigned long) GetLastError());
		free(wide_temporary);
		free(wide_output);
		return -1;
	}
	free(wide_temporary);
	free(wide_output);
#else
	if (rename(temporary_path, output_path) != 0) {
		INNO_LOG("cannot commit %s: %s", output_path, strerror(errno));
		return -1;
	}
#endif
	return 0;
}

/* ── Helpers: read from fd at a specific offset ──────────────────── */
static int read_at(int fd, uint64_t offset, void *buf, size_t len)
{
	if (LSEEK(fd, (long long) offset, SEEK_SET) < 0) return -1;
	size_t total = 0;
	while (total < len) {
		int n = READ_FD(fd, (uint8_t *) buf + total, len - total);
		if (n <= 0) return -1;
		total += (size_t) n;
	}
	return 0;
}

static int get_fd_size(int fd, uint64_t *size_out)
{
	long long end = LSEEK(fd, 0, SEEK_END);
	if (end < 0) return -1;
	*size_out = (uint64_t) end;
	return 0;
}

static uint16_t get_u16(const uint8_t *p)
{
	return (uint16_t) (p[0] | (p[1] << 8));
}
static uint32_t get_u32(const uint8_t *p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t) p[3] << 24);
}
static uint64_t get_u64(const uint8_t *p)
{
	return (uint64_t) get_u32(p) | ((uint64_t) get_u32(p + 4) << 32);
}

/* ── InnoSetup version constants ─────────────────────────────────── */
#define INNO_VER(a, b, c) (((a) * 10000) + ((b) * 100) + (c))

/* ── PE Resource reader (find RT_RCDATA / name 11111) ────────────── */

#define INNO_PE_MAX_SECTIONS        96u
#define INNO_PE_MAX_OPTIONAL_HEADER 4096u

typedef struct {
	int fd;
	uint64_t source_size;
	uint64_t file_offset;
	uint32_t size;
} inno_pe_resource_view_t;

static int read_file_span(int fd, uint64_t source_size, uint64_t offset,
                          void *buffer, size_t size)
{
	if (offset > source_size || size > source_size - offset)
		return -1;
	return read_at(fd, offset, buffer, size);
}

static int read_resource_span(const inno_pe_resource_view_t *view,
                              uint32_t offset, void *buffer, size_t size)
{
	if (!view || offset > view->size || size > view->size - offset)
		return -1;
	return read_file_span(view->fd, view->source_size,
	                      view->file_offset + offset, buffer, size);
}

static int validate_resource_name(const inno_pe_resource_view_t *view,
                                  uint32_t name)
{
	if ((name & 0x80000000U) == 0)
		return 0;
	uint32_t offset = name & 0x7FFFFFFFU;
	uint8_t length_bytes[2];
	if (read_resource_span(view, offset, length_bytes,
	                       sizeof(length_bytes)) < 0)
		return -1;
	uint32_t byte_count = (uint32_t) get_u16(length_bytes) * 2u;
	return offset + 2u <= view->size &&
	               byte_count <= view->size - (offset + 2u)
	           ? 0
	           : -1;
}

static int read_resource_directory(const inno_pe_resource_view_t *view,
                                   uint32_t offset, uint32_t *entry_offset,
                                   uint32_t *entry_count)
{
	uint8_t header[16];
	if (read_resource_span(view, offset, header, sizeof(header)) < 0)
		return -1;
	uint32_t count = (uint32_t) get_u16(header + 12) +
	                 (uint32_t) get_u16(header + 14);
	if (offset > view->size - sizeof(header))
		return -1;
	uint32_t entries = offset + (uint32_t) sizeof(header);
	if (count > (view->size - entries) / 8u)
		return -1;
	*entry_offset = entries;
	*entry_count = count;
	return 0;
}

static int read_resource_entry(const inno_pe_resource_view_t *view,
                               uint32_t entries, uint32_t index,
                               uint32_t *name, uint32_t *target)
{
	uint8_t entry[8];
	uint64_t offset = (uint64_t) entries + (uint64_t) index * sizeof(entry);
	if (offset > UINT32_MAX ||
	    read_resource_span(view, (uint32_t) offset, entry, sizeof(entry)) < 0)
		return -1;
	*name = get_u32(entry);
	*target = get_u32(entry + 4);
	return validate_resource_name(view, *name);
}

static int map_pe_rva(int fd, uint64_t source_size, uint64_t section_table,
                      uint16_t section_count, uint32_t rva, uint32_t size,
                      uint64_t *file_offset)
{
	for (uint16_t i = 0; i < section_count; i++) {
		uint8_t section[40];
		uint64_t position = section_table + (uint64_t) i * sizeof(section);
		if (read_file_span(fd, source_size, position, section,
		                   sizeof(section)) < 0)
			return -1;
		uint32_t virtual_size = get_u32(section + 8);
		uint32_t section_rva = get_u32(section + 12);
		uint32_t raw_size = get_u32(section + 16);
		uint32_t raw_offset = get_u32(section + 20);
		if (rva < section_rva)
			continue;
		uint32_t delta = rva - section_rva;
		if (delta > raw_size || size > raw_size - delta)
			continue;
		if (virtual_size != 0 &&
		    (delta > virtual_size || size > virtual_size - delta))
			continue;
		if ((uint64_t) raw_offset > source_size ||
		    raw_size > source_size - (uint64_t) raw_offset)
			return -1;
		*file_offset = (uint64_t) raw_offset + delta;
		return 0;
	}
	return -1;
}

static int find_pe_resource_11111(int fd, uint64_t source_size,
                                  uint64_t *out_offset)
{
	uint8_t dos_header[64];
	uint8_t pe_header[24];
	uint8_t value[8];
	if (!out_offset ||
	    read_file_span(fd, source_size, 0, dos_header,
	                   sizeof(dos_header)) < 0 ||
	    dos_header[0] != 'M' || dos_header[1] != 'Z')
		return -1;
	uint64_t pe_offset = get_u32(dos_header + 0x3C);
	if (read_file_span(fd, source_size, pe_offset, pe_header,
	                   sizeof(pe_header)) < 0 ||
	    pe_header[0] != 'P' || pe_header[1] != 'E' ||
	    pe_header[2] != 0 || pe_header[3] != 0)
		return -1;

	uint16_t section_count = get_u16(pe_header + 6);
	uint16_t optional_size = get_u16(pe_header + 20);
	if (section_count == 0 || section_count > INNO_PE_MAX_SECTIONS ||
	    optional_size > INNO_PE_MAX_OPTIONAL_HEADER)
		return -1;
	uint64_t optional_offset = pe_offset + sizeof(pe_header);
	if (optional_offset < pe_offset ||
	    optional_offset > source_size ||
	    optional_size > source_size - optional_offset ||
	    read_file_span(fd, source_size, optional_offset, value, 2) < 0)
		return -1;

	uint32_t directory_offset;
	if (get_u16(value) == 0x10b)
		directory_offset = 96u;
	else if (get_u16(value) == 0x20b)
		directory_offset = 112u;
	else
		return -1;
	if (optional_size < directory_offset + 24u ||
	    read_file_span(fd, source_size,
	                   optional_offset + directory_offset - 4u,
	                   value, 4) < 0 ||
	    get_u32(value) < 3u ||
	    read_file_span(fd, source_size,
	                   optional_offset + directory_offset + 16u,
	                   value, sizeof(value)) < 0)
		return -1;
	uint32_t resource_rva = get_u32(value);
	uint32_t resource_size = get_u32(value + 4);
	if (resource_rva == 0 || resource_size == 0 ||
	    resource_size > DXX_EXTRACT_MAX_METADATA_BYTES)
		return -1;

	uint64_t section_table = optional_offset + optional_size;
	uint64_t section_bytes = (uint64_t) section_count * 40u;
	if (section_table < optional_offset ||
	    section_table > source_size ||
	    section_bytes > source_size - section_table)
		return -1;
	inno_pe_resource_view_t view = { fd, source_size, 0, resource_size };
	if (map_pe_rva(fd, source_size, section_table, section_count,
	               resource_rva, resource_size, &view.file_offset) < 0)
		return -1;

	uint32_t root_entries;
	uint32_t root_count;
	if (read_resource_directory(&view, 0, &root_entries, &root_count) < 0)
		return -1;
	for (uint32_t i = 0; i < root_count; i++) {
		uint32_t type;
		uint32_t type_target;
		if (read_resource_entry(&view, root_entries, i,
		                        &type, &type_target) < 0)
			return -1;
		if (type != 10u || (type_target & 0x80000000U) == 0)
			continue;
		uint32_t name_entries;
		uint32_t name_count;
		if (read_resource_directory(&view, type_target & 0x7FFFFFFFU,
		                            &name_entries, &name_count) < 0)
			return -1;
		for (uint32_t j = 0; j < name_count; j++) {
			uint32_t name;
			uint32_t name_target;
			if (read_resource_entry(&view, name_entries, j,
			                        &name, &name_target) < 0)
				return -1;
			if (name != 11111u ||
			    (name_target & 0x80000000U) == 0)
				continue;
			uint32_t language_entries;
			uint32_t language_count;
			if (read_resource_directory(
			        &view, name_target & 0x7FFFFFFFU,
			        &language_entries, &language_count) < 0 ||
			    language_count == 0)
				return -1;
			uint32_t language;
			uint32_t data_target;
			if (read_resource_entry(&view, language_entries, 0,
			                        &language, &data_target) < 0 ||
			    (data_target & 0x80000000U) != 0)
				return -1;
			uint8_t data_entry[16];
			if (read_resource_span(&view, data_target, data_entry,
			                       sizeof(data_entry)) < 0)
				return -1;
			uint32_t data_rva = get_u32(data_entry);
			uint32_t data_size = get_u32(data_entry + 4);
			if (data_size == 0 ||
			    map_pe_rva(fd, source_size, section_table,
			               section_count, data_rva, data_size,
			               out_offset) < 0)
				return -1;
			return 0;
		}
	}
	return -1;
}

/* ── Offset table parsing ────────────────────────────────────────── */

/* Known loader magic IDs (12 bytes each) */
static const uint8_t loader_magic_1[] = {
	'r', 'D', 'l', 'P', 't', 'S', 0xcd, 0xe6, 0xd7, 0x7b, 0x0b, 0x2a
};
static const uint8_t loader_magic_2[] = {
	'n', 'S', '5', 'W', '7', 'd', 'T', 0x83, 0xaa, 0x1b, 0x0f, 0x6a
};
/* Older IDs (for broader compatibility) */
static const uint8_t loader_magic_07[] = {
	'r', 'D', 'l', 'P', 't', 'S', '0', '7', 0x87, 'e', 'V', 'x'
};
static const uint8_t loader_magic_06[] = {
	'r', 'D', 'l', 'P', 't', 'S', '0', '6', 0x87, 'e', 'V', 'x'
};
static const uint8_t loader_magic_05[] = {
	'r', 'D', 'l', 'P', 't', 'S', '0', '5', 0x87, 'e', 'V', 'x'
};

/* Loader version enum */
typedef enum {
	LOADER_VER_UNKNOWN = 0,
	LOADER_VER_4_0_3,  /* magic_05 */
	LOADER_VER_4_0_10, /* magic_06 */
	LOADER_VER_4_1_6,  /* magic_07 */
	LOADER_VER_5_1_5,  /* magic_1 or magic_2 */
} loader_version_t;

static loader_version_t identify_loader(const uint8_t magic[12])
{
	if (!memcmp(magic, loader_magic_1, 12)) return LOADER_VER_5_1_5;
	if (!memcmp(magic, loader_magic_2, 12)) return LOADER_VER_5_1_5;
	if (!memcmp(magic, loader_magic_07, 12)) return LOADER_VER_4_1_6;
	if (!memcmp(magic, loader_magic_06, 12)) return LOADER_VER_4_0_10;
	if (!memcmp(magic, loader_magic_05, 12)) return LOADER_VER_4_0_3;
	return LOADER_VER_UNKNOWN;
}

static int parse_offset_table(int fd, uint64_t table_offset,
                              uint64_t *header_offset, uint64_t *data_offset)
{
	uint8_t buf[64];
	if (read_at(fd, table_offset, buf, 64) < 0) {
		INNO_LOG("cannot read offset table");
		return -1;
	}

	loader_version_t lver = identify_loader(buf);
	if (lver == LOADER_VER_UNKNOWN) {
		INNO_LOG("unknown loader magic at 0x%llx", (unsigned long long) table_offset);
		return -1;
	}

	uint8_t *p = buf + 12; /* skip 12-byte magic */

	if (lver >= LOADER_VER_5_1_5) {
		uint32_t revision = get_u32(p);
		p += 4;
		if (revision != 1) {
			INNO_LOG("unexpected loader revision %u", revision);
		}
	}

	/* uint32_t exe_total_size = */ get_u32(p);
	p += 4;
	/* uint32_t exe_offset = */ get_u32(p);
	p += 4;

	if (lver < LOADER_VER_4_1_6) {
		/* uint32_t exe_compressed_size = */ get_u32(p);
		p += 4;
	}

	/* uint32_t exe_uncompressed_size = */ get_u32(p);
	p += 4;
	/* uint32_t exe_checksum = */ get_u32(p);
	p += 4;

	*header_offset = get_u32(p);
	p += 4;
	*data_offset = get_u32(p);
	p += 4;

	/* CRC32 validation for >= 4.0.10 */
	if (lver >= LOADER_VER_4_0_10) {
		uint32_t expected_crc = get_u32(p);
		size_t crc_len = (size_t) (p - buf);
		uint32_t actual_crc = inno_crc32(buf, crc_len);
		if (actual_crc != expected_crc) {
			INNO_LOG("offset table CRC mismatch: expected 0x%08x, got 0x%08x",
			         expected_crc, actual_crc);
			return -1;
		}
	}

	return 0;
}

/* ── Block stream decompression ──────────────────────────────────── */

/*
 * Decompress a block stream at a given file offset.
 * The block stream consists of:
 *   - 9-byte header: CRC32(5 bytes) + stored_size(4) + compressed(1)
 *   - stored_size bytes of CRC32-chunked data (4-byte CRC + up to 4096 bytes)
 *
 * The raw data (after removing CRCs) is LZMA1-compressed.
 *
 * Returns the decompressed data (caller must free), or NULL on error.
 * Sets *out_len to the decompressed size.
 * Sets *bytes_consumed to total bytes read from the stream (header + data).
 */
static uint8_t *decompress_block_stream(int fd, uint64_t offset,
                                        size_t *out_len,
                                        size_t *bytes_consumed)
{
	/* Read the 9-byte block header */
	uint8_t hdr[9];
	if (read_at(fd, offset, hdr, 9) < 0) {
		INNO_LOG("cannot read block header at 0x%llx", (unsigned long long) offset);
		return NULL;
	}

	uint32_t hdr_crc = get_u32(hdr);
	uint32_t stored_size = get_u32(hdr + 4);
	uint8_t compressed = hdr[8];

	/* Validate header CRC (covers stored_size + compressed = 5 bytes) */
	uint32_t check_crc = inno_crc32(hdr + 4, 5);
	if (check_crc != hdr_crc) {
		INNO_LOG("block header CRC mismatch at 0x%llx", (unsigned long long) offset);
		return NULL;
	}
	if (stored_size > DXX_EXTRACT_MAX_METADATA_BYTES ||
	    !dxx_extract_memory_allowed(stored_size, stored_size)) {
		INNO_LOG("metadata block exceeds extraction budget: %u", stored_size);
		return NULL;
	}

	*bytes_consumed = 9 + stored_size;

	/* Read all stored data */
	uint8_t *stored = (uint8_t *) malloc(stored_size);
	if (!stored) return NULL;
	if (read_at(fd, offset + 9, stored, stored_size) < 0) {
		INNO_LOG("cannot read %u bytes of block data", stored_size);
		free(stored);
		return NULL;
	}

	/* Strip CRC32 prefixes from 4096-byte sub-blocks.
	 * Format: [CRC32 4 bytes][data up to 4096 bytes]... */
	size_t raw_cap = stored_size; /* upper bound (raw < stored) */
	uint8_t *raw = (uint8_t *) malloc(raw_cap);
	if (!raw) {
		free(stored);
		return NULL;
	}
	size_t raw_len = 0;
	size_t pos = 0;
	while (pos < stored_size) {
		if (pos + 4 > stored_size) {
			INNO_LOG("truncated sub-block CRC at pos %zu", pos);
			free(raw);
			free(stored);
			return NULL;
		}
		uint32_t sub_crc = get_u32(stored + pos);
		pos += 4;
		size_t chunk_len = stored_size - pos;
		if (chunk_len > 4096) chunk_len = 4096;
		/* Validate sub-block CRC */
		uint32_t actual_sub_crc = inno_crc32(stored + pos, chunk_len);
		if (actual_sub_crc != sub_crc) {
			INNO_LOG("sub-block CRC mismatch at pos %zu", pos - 4);
			free(raw);
			free(stored);
			return NULL;
		}
		memcpy(raw + raw_len, stored + pos, chunk_len);
		raw_len += chunk_len;
		pos += chunk_len;
	}
	free(stored);

	if (!compressed) {
		/* Stored (uncompressed) */
		*out_len = raw_len;
		return raw;
	}

	/* LZMA1 decompression (Inno-specific: 5-byte header, no size field) */
	if (raw_len < 5) {
		INNO_LOG("LZMA data too small (%zu bytes)", raw_len);
		free(raw);
		return NULL;
	}

	/* LZMA properties: 1 byte props + 4 bytes dict_size */
	uint8_t lzma_props[5];
	memcpy(lzma_props, raw, 5);
	if (get_u32(lzma_props + 1) > DXX_EXTRACT_MAX_METADATA_BYTES) {
		free(raw);
		return NULL;
	}

	uint8_t *lzma_data = raw + 5;
	size_t lzma_data_len = raw_len - 5;

	/* Start with 4x compressed size estimate, grow if needed */
	size_t decomp_cap = lzma_data_len > DXX_EXTRACT_MAX_METADATA_BYTES / 4u
	                        ? (size_t) DXX_EXTRACT_MAX_METADATA_BYTES
	                        : lzma_data_len * 4u;
	if (decomp_cap < 65536) decomp_cap = 65536;
	uint8_t *decomp = (uint8_t *) malloc(decomp_cap);
	if (!decomp) {
		free(raw);
		return NULL;
	}

	CLzmaDec dec;
	LzmaDec_Construct(&dec);
	SRes res = LzmaDec_Allocate(&dec, lzma_props, 5, &g_lzma_alloc);
	if (res != SZ_OK) {
		INNO_LOG("LZMA alloc failed: %d", res);
		free(decomp);
		free(raw);
		return NULL;
	}
	LzmaDec_Init(&dec);

	size_t decomp_len = 0;
	size_t src_pos = 0;
	ELzmaStatus status;

	while (src_pos < lzma_data_len) {
		/* Grow output buffer if needed */
		if (decomp_len >= decomp_cap) {
			if (decomp_cap >= DXX_EXTRACT_MAX_METADATA_BYTES) {
				LzmaDec_Free(&dec, &g_lzma_alloc);
				free(decomp);
				free(raw);
				return NULL;
			}
			decomp_cap *= 2u;
			if (decomp_cap > DXX_EXTRACT_MAX_METADATA_BYTES)
				decomp_cap = (size_t) DXX_EXTRACT_MAX_METADATA_BYTES;
			uint8_t *tmp = (uint8_t *) realloc(decomp, decomp_cap);
			if (!tmp) {
				LzmaDec_Free(&dec, &g_lzma_alloc);
				free(decomp);
				free(raw);
				return NULL;
			}
			decomp = tmp;
		}

		size_t dest_len = decomp_cap - decomp_len;
		size_t src_len = lzma_data_len - src_pos;
		res = LzmaDec_DecodeToBuf(&dec, decomp + decomp_len, &dest_len,
		                          lzma_data + src_pos, &src_len,
		                          LZMA_FINISH_ANY, &status);
		decomp_len += dest_len;
		src_pos += src_len;

		if (res != SZ_OK) {
			INNO_LOG("LZMA decode error: %d", res);
			LzmaDec_Free(&dec, &g_lzma_alloc);
			free(decomp);
			free(raw);
			return NULL;
		}
		if (status == LZMA_STATUS_FINISHED_WITH_MARK ||
		    status == LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK) {
			break;
		}
		if (dest_len == 0 && src_len == 0) break; /* no progress */
	}

	LzmaDec_Free(&dec, &g_lzma_alloc);
	free(raw);
	if (!dxx_extract_ratio_allowed(decomp_len, lzma_data_len)) {
		free(decomp);
		return NULL;
	}

	*out_len = decomp_len;
	return decomp;
}

/* ── Version string parser ───────────────────────────────────────── */

static int parse_version_component(const uint8_t *id, size_t id_len,
                                   size_t *position, int *component)
{
	unsigned int value = 0;
	size_t pos;

	if (!id || !position || !component || *position >= id_len ||
	    id[*position] < '0' || id[*position] > '9')
		return -1;
	pos = *position;
	while (pos < id_len && id[pos] >= '0' && id[pos] <= '9') {
		unsigned int digit = id[pos] - '0';
		if (value > ((unsigned int) INT_MAX - digit) / 10)
			return -1;
		value = value * 10 + digit;
		pos++;
	}
	*position = pos;
	*component = (int) value;
	return 0;
}

static int parse_version_string(const uint8_t id[INNO_VERSION_ID_SIZE],
                                inno_version_t *ver)
{
	static const uint8_t prefix[] = "Inno Setup Setup Data (";
	const uint8_t *terminator;
	size_t id_len;
	size_t pos = sizeof(prefix) - 1;

	if (!id || !ver)
		return -1;
	memset(ver, 0, sizeof(*ver));
	terminator = (const uint8_t *) memchr(id, '\0', INNO_VERSION_ID_SIZE);
	if (!terminator) {
		INNO_LOG("unterminated version ID");
		return -1;
	}
	id_len = (size_t) (terminator - id);
	for (size_t i = id_len + 1; i < INNO_VERSION_ID_SIZE; i++) {
		if (id[i] != 0) {
			INNO_LOG("nonzero version ID padding");
			return -1;
		}
	}
	if (id_len < sizeof(prefix) - 1 ||
	    memcmp(id, prefix, sizeof(prefix) - 1) != 0 ||
	    parse_version_component(id, id_len, &pos, &ver->major) < 0 ||
	    pos >= id_len || id[pos++] != '.' ||
	    parse_version_component(id, id_len, &pos, &ver->minor) < 0 ||
	    pos >= id_len || id[pos++] != '.' ||
	    parse_version_component(id, id_len, &pos, &ver->patch) < 0 ||
	    pos >= id_len || id[pos++] != ')') {
		INNO_LOG("invalid version ID: %.*s", (int) id_len, (const char *) id);
		return -1;
	}

	if (pos == id_len) {
		ver->unicode = 0;
	} else if (id_len - pos == 4 && id[pos] == ' ' &&
	           id[pos + 1] == '(' &&
	           (id[pos + 2] == 'u' || id[pos + 2] == 'U') &&
	           id[pos + 3] == ')') {
		ver->unicode = 1;
	} else {
		INNO_LOG("unsupported version ID suffix: %.*s",
		         (int) id_len, (const char *) id);
		return -1;
	}

	/* innoextract identifies this unofficial string as the 5.5.7 layout */
	if (ver->unicode && ver->major == 5 && ver->minor == 5 && ver->patch == 8)
		ver->patch = 7;

	return 0;
}

typedef struct {
	inno_checksum_type_t type;
	size_t digest_size;
} inno_checksum_layout_t;

static inno_checksum_layout_t checksum_layout_for_version(const inno_version_t *ver)
{
	inno_checksum_layout_t layout;
	int version = INNO_VER(ver->major, ver->minor, ver->patch);
	if (version >= INNO_VER(5, 3, 9)) {
		layout.type = INNO_CHECKSUM_SHA1;
		layout.digest_size = 20;
	} else {
		layout.type = INNO_CHECKSUM_MD5;
		layout.digest_size = 16;
	}
	return layout;
}

static int entry_count_allowed(uint32_t entry_count)
{
	return entry_count <= DXX_EXTRACT_MAX_ENTRIES;
}

static int data_location_valid(const inno_archive_t *arc, uint32_t location)
{
	return arc && arc->data_entries && location != UINT32_MAX &&
	       location < arc->data_entry_count;
}

static size_t data_flag_bytes_for_version(int version)
{
	int flag_count;

	if (version >= INNO_VER(5, 5, 7)) flag_count = 11;
	else if (version >= INNO_VER(5, 1, 13)) flag_count = 9;
	else if (version >= INNO_VER(4, 2, 5)) flag_count = 8;
	else if (version >= INNO_VER(4, 2, 2)) flag_count = 7;
	else if (version >= INNO_VER(4, 2, 0)) flag_count = 6;
	else if (version >= INNO_VER(4, 1, 8)) flag_count = 5;
	else if (version >= INNO_VER(4, 1, 0)) flag_count = 4;
	else if (version >= INNO_VER(4, 0, 10)) flag_count = 3;
	else flag_count = 2;
	size_t bytes = (size_t) ((flag_count + 7) / 8);
	return bytes == 3 ? 4 : bytes;
}

static size_t data_entry_size_for_version(const inno_version_t *ver)
{
	return 52 + checksum_layout_for_version(ver).digest_size +
	       data_flag_bytes_for_version(INNO_VER(ver->major, ver->minor, ver->patch));
}

/* ── Header stream parser ────────────────────────────────────────── */

static int append_utf8(char *out, size_t out_size, size_t *out_pos,
                       uint32_t codepoint)
{
	uint8_t encoded[4];
	size_t count;
	if (codepoint <= 0x7Fu) {
		encoded[0] = (uint8_t) codepoint;
		count = 1;
	} else if (codepoint <= 0x7FFu) {
		encoded[0] = (uint8_t) (0xC0u | (codepoint >> 6));
		encoded[1] = (uint8_t) (0x80u | (codepoint & 0x3Fu));
		count = 2;
	} else if (codepoint <= 0xFFFFu) {
		encoded[0] = (uint8_t) (0xE0u | (codepoint >> 12));
		encoded[1] = (uint8_t) (0x80u | ((codepoint >> 6) & 0x3Fu));
		encoded[2] = (uint8_t) (0x80u | (codepoint & 0x3Fu));
		count = 3;
	} else {
		encoded[0] = (uint8_t) (0xF0u | (codepoint >> 18));
		encoded[1] = (uint8_t) (0x80u | ((codepoint >> 12) & 0x3Fu));
		encoded[2] = (uint8_t) (0x80u | ((codepoint >> 6) & 0x3Fu));
		encoded[3] = (uint8_t) (0x80u | (codepoint & 0x3Fu));
		count = 4;
	}
	if (*out_pos >= out_size || count > out_size - 1u - *out_pos)
		return -1;
	memcpy(out + *out_pos, encoded, count);
	*out_pos += count;
	return 0;
}

static int utf16le_to_utf8(const uint8_t *input, size_t input_size,
                           char *out, size_t out_size)
{
	if (!input || !out || out_size == 0 || (input_size & 1u) != 0)
		return -1;
	size_t out_pos = 0;
	for (size_t i = 0; i < input_size; i += 2u) {
		uint32_t codepoint = get_u16(input + i);
		if (codepoint == 0)
			return -1;
		if (codepoint >= 0xD800u && codepoint <= 0xDBFFu) {
			if (i + 4u > input_size)
				return -1;
			uint32_t low = get_u16(input + i + 2u);
			if (low < 0xDC00u || low > 0xDFFFu)
				return -1;
			codepoint = 0x10000u + ((codepoint - 0xD800u) << 10) +
			            (low - 0xDC00u);
			i += 2u;
		} else if (codepoint >= 0xDC00u && codepoint <= 0xDFFFu) {
			return -1;
		}
		if (append_utf8(out, out_size, &out_pos, codepoint) < 0)
			return -1;
	}
	out[out_pos] = '\0';
	return 0;
}

/* Read a length-prefixed string and convert Unicode strings to UTF-8.
 * Advances *pos by the consumed bytes only after validating the complete span. */
static int read_string(const uint8_t *buf, size_t buf_len, size_t *pos,
                       char *out, size_t out_size, int unicode)
{
	if (!buf || !pos || *pos > buf_len || buf_len - *pos < 4u)
		return -1;
	uint32_t len = get_u32(buf + *pos);
	*pos += 4;
	if (len > buf_len - *pos)
		return -1;

	if (out && out_size > 0) {
		if (unicode) {
			if (utf16le_to_utf8(buf + *pos, len, out, out_size) < 0)
				return -1;
		} else {
			if (len >= out_size || memchr(buf + *pos, 0, len))
				return -1;
			memcpy(out, buf + *pos, len);
			out[len] = '\0';
		}
	}
	*pos += len;
	return 0;
}

static int valid_destination_path(const char *path)
{
	if (!path || !path[0])
		return 0;
	const char *component = path;
	for (const unsigned char *p = (const unsigned char *) path;; p++) {
		unsigned char ch = *p;
		if (ch == '\0' || ch == '/' || ch == '\\') {
			if ((const char *) p == component ||
			    p[-1] == ' ' || p[-1] == '.')
				return 0;
			if (ch == '\0')
				return 1;
			component = (const char *) p + 1;
			continue;
		}
		if (ch < 0x20u || ch == 0x7Fu ||
		    ch == '<' || ch == '>' || ch == ':' || ch == '"' ||
		    ch == '|' || ch == '?' || ch == '*')
			return 0;
	}
}

#define INNO_GALAXY_SCRIPT_LEN 4096

static void skip_script_space(const char **cursor)
{
	while (**cursor == ' ' || **cursor == '\t') (*cursor)++;
}

static int consume_script_token(const char **cursor, const char *token)
{
	const char *input = *cursor;
	while (*token) {
		unsigned char actual = (unsigned char) *input++;
		unsigned char expected = (unsigned char) *token++;
		if (actual >= 'A' && actual <= 'Z') actual = (unsigned char) (actual + ('a' - 'A'));
		if (actual != expected) return 0;
	}
	*cursor = input;
	return 1;
}

static int parse_script_string(const char **cursor, char *output, size_t output_size)
{
	const char *input = *cursor;
	size_t length = 0;

	if (*input++ != '\'' || !output || output_size == 0) return 0;
	for (;;) {
		unsigned char ch = (unsigned char) *input++;
		if (ch == '\0') return 0;
		if (ch == '\'') {
			if (*input == '\'') {
				input++;
				ch = '\'';
			} else {
				output[length] = '\0';
				*cursor = input;
				return 1;
			}
		}
		if (length + 1 >= output_size) return 0;
		output[length++] = (char) ch;
	}
}

static int parse_script_part_count(const char **cursor, uint32_t *part_count)
{
	const char *input = *cursor;
	uint32_t count = 0;

	if (*input < '0' || *input > '9') return 0;
	do {
		uint32_t digit = (uint32_t) (*input++ - '0');
		if (count > (DXX_EXTRACT_MAX_ENTRIES - digit) / 10u) return 0;
		count = count * 10u + digit;
	} while (*input >= '0' && *input <= '9');
	if (count == 0) return 0;
	*cursor = input;
	*part_count = count;
	return 1;
}

static int parse_gog_galaxy_before_install(const char *script,
                                           char *destination,
                                           size_t destination_size,
                                           uint32_t *part_count)
{
	char hash[33];
	char parsed_destination[INNO_PATH_LEN];
	const char *cursor = script;
	uint32_t count = 0;

	if (!script || !destination || destination_size == 0) return 0;
	skip_script_space(&cursor);
	if (!consume_script_token(&cursor, "before_install")) return 0;
	skip_script_space(&cursor);
	if (*cursor++ != '(') return 0;
	skip_script_space(&cursor);
	if (!parse_script_string(&cursor, hash, sizeof(hash))) return 0;
	skip_script_space(&cursor);
	if (*cursor++ != ',') return 0;
	skip_script_space(&cursor);
	if (!parse_script_string(&cursor, parsed_destination,
	                         sizeof(parsed_destination)))
		return 0;
	skip_script_space(&cursor);
	if (*cursor++ != ',') return 0;
	skip_script_space(&cursor);
	if (!parse_script_part_count(&cursor, &count)) return 0;
	skip_script_space(&cursor);
	if (*cursor++ != ')') return 0;
	skip_script_space(&cursor);
	if (*cursor != '\0' || strlen(hash) != 32 ||
	    !valid_destination_path(parsed_destination))
		return 0;
	for (size_t i = 0; hash[i]; i++)
		if (!((hash[i] >= '0' && hash[i] <= '9') ||
		      (hash[i] >= 'a' && hash[i] <= 'f') ||
		      (hash[i] >= 'A' && hash[i] <= 'F')))
			return 0;
	if (strlen(parsed_destination) >= destination_size) return 0;
	memcpy(destination, parsed_destination, strlen(parsed_destination) + 1u);
	if (part_count) *part_count = count;
	return 1;
}

/* BeforeInstall is ordinary opaque Inno script data unless it is small enough
 * to test against the exact Galaxy convention. */
static int read_before_install(const uint8_t *buf, size_t buf_len, size_t *pos,
                               char *output, size_t output_size, int unicode,
                               int *captured)
{
	uint32_t length;

	if (!buf || !pos || *pos > buf_len || buf_len - *pos < 4u ||
	    !output || output_size == 0 || !captured)
		return -1;
	length = get_u32(buf + *pos);
	*pos += 4u;
	if (length > buf_len - *pos) return -1;
	output[0] = '\0';
	*captured = 0;
	if (length <= INNO_GALAXY_SCRIPT_LEN) {
		if (unicode) {
			if (utf16le_to_utf8(buf + *pos, length, output, output_size) == 0)
				*captured = 1;
		} else if (length < output_size && !memchr(buf + *pos, 0, length)) {
			memcpy(output, buf + *pos, length);
			output[length] = '\0';
			*captured = 1;
		}
	}
	*pos += length;
	return 0;
}

/* Skip a string without storing it */
static int skip_string(const uint8_t *buf, size_t buf_len, size_t *pos)
{
	return read_string(buf, buf_len, pos, NULL, 0, 0);
}

/* Skip N strings */
static int skip_strings(const uint8_t *buf, size_t buf_len, size_t *pos, int count)
{
	for (int i = 0; i < count; i++) {
		if (skip_string(buf, buf_len, pos) < 0) return -1;
	}
	return 0;
}

/* Read N bytes, advancing pos */
static int skip_bytes(const uint8_t *buf, size_t buf_len, size_t *pos, size_t n)
{
	if (*pos + n > buf_len) return -1;
	*pos += n;
	return 0;
}

/* ── Flag reader helper ──────────────────────────────────────────── */

/* Read a packed bitfield of 'count' flags (1 byte per 8 flags).
 * If count % 8 is in range [17,24] (exactly 3 bytes), reads 4 bytes. */
static int skip_flags(const uint8_t *buf, size_t buf_len, size_t *pos, int count)
{
	int bytes = (count + 7) / 8;
	if (bytes == 3) bytes = 4; /* InnoSetup padding: 3 flag bytes → 4 */
	return skip_bytes(buf, buf_len, pos, bytes);
}

/* ── Main header parser (setup-0 block stream 1) ─────────────────── */

static int parse_header_stream1(const uint8_t *buf, size_t buf_len,
                                inno_version_t *ver, inno_archive_t *arc)
{
	size_t pos = 0;
	int v = INNO_VER(ver->major, ver->minor, ver->patch);
	inno_checksum_layout_t checksum_layout = checksum_layout_for_version(ver);

	/* ── Main header strings ── */
	int num_header_strings;
	if (v >= INNO_VER(5, 6, 1)) num_header_strings = 34;
	else if (v >= INNO_VER(5, 5, 6)) num_header_strings = 32;
	else if (v >= INNO_VER(5, 5, 0)) num_header_strings = 31;
	else if (v >= INNO_VER(5, 3, 10)) num_header_strings = 30;
	else if (v >= INNO_VER(5, 3, 8)) num_header_strings = 29;
	else if (v >= INNO_VER(5, 2, 5)) num_header_strings = 28;
	else if (v >= INNO_VER(5, 1, 13)) num_header_strings = 25;
	else num_header_strings = 24;

	if (skip_strings(buf, buf_len, &pos, num_header_strings) < 0) {
		INNO_LOG("failed to skip %d header strings (pos=%zu, buf_len=%zu)",
		         num_header_strings, pos, buf_len);
		return -1;
	}

	/* ── Entry counts ── */
	size_t counts_size = 15 * 4; /* 15 x uint32 */
	if (pos + counts_size > buf_len) {
		INNO_LOG("truncated header at entry counts");
		return -1;
	}
	uint32_t language_count = get_u32(buf + pos);
	pos += 4;
	uint32_t message_count = get_u32(buf + pos);
	pos += 4;
	uint32_t permission_count = get_u32(buf + pos);
	pos += 4;
	uint32_t type_count = get_u32(buf + pos);
	pos += 4;
	uint32_t component_count = get_u32(buf + pos);
	pos += 4;
	uint32_t task_count = get_u32(buf + pos);
	pos += 4;
	uint32_t directory_count = get_u32(buf + pos);
	pos += 4;
	uint32_t file_count = get_u32(buf + pos);
	pos += 4;
	uint32_t data_entry_count = get_u32(buf + pos);
	pos += 4;
	uint32_t icon_count = get_u32(buf + pos);
	pos += 4;
	uint32_t ini_entry_count = get_u32(buf + pos);
	pos += 4;
	uint32_t registry_entry_count = get_u32(buf + pos);
	pos += 4;
	uint32_t delete_entry_count = get_u32(buf + pos);
	pos += 4;
	uint32_t uninstall_delete_entry_count = get_u32(buf + pos);
	pos += 4;
	uint32_t run_entry_count = get_u32(buf + pos);
	pos += 4;

	/* uninstall_run_entry_count (>= 2.0.0) */
	if (pos + 4 > buf_len) return -1;
	uint32_t uninstall_run_entry_count = get_u32(buf + pos);
	pos += 4;

	if (!entry_count_allowed(data_entry_count)) {
		INNO_LOG("too many data entries: %u", data_entry_count);
		return -1;
	}
	arc->data_entry_count = data_entry_count;

	/* ── Windows version range (20 bytes) ── */
	if (skip_bytes(buf, buf_len, &pos, 20) < 0) return -1;

	/* ── Colors and misc ── */
	/* back_color, back_color2 */
	if (skip_bytes(buf, buf_len, &pos, 8) < 0) return -1;

	if (v < INNO_VER(5, 5, 7)) {
		/* image_back_color: present in < 5.5.7 */
		if (skip_bytes(buf, buf_len, &pos, 4) < 0) return -1;
	}

	if (v >= INNO_VER(5, 5, 7)) {
		/* image_alpha_format: uint8 enum, added in 5.5.7 */
		if (skip_bytes(buf, buf_len, &pos, 1) < 0) return -1;
	}

	/* password_sha1 (20) + password_salt (8) */
	/* The version layout supersedes the historical SHA-1-only label above */
	if (skip_bytes(buf, buf_len, &pos, checksum_layout.digest_size + 8) < 0) return -1;

	/* extra_disk_space_required (8) + slices_per_disk (4) */
	if (skip_bytes(buf, buf_len, &pos, 12) < 0) return -1;

	/* uninstall_log_mode (1) */
	if (skip_bytes(buf, buf_len, &pos, 1) < 0) return -1;

	/* dir_exists_warning (1) - stored_enum */
	if (skip_bytes(buf, buf_len, &pos, 1) < 0) return -1;

	/* privileges_required (1) - enum with <= 4 values for >= 5.3.7 */
	if (skip_bytes(buf, buf_len, &pos, 1) < 0) return -1;

	/* show_language_dialog (1) + language_detection (1) */
	if (skip_bytes(buf, buf_len, &pos, 2) < 0) return -1;

	/* compression method (1 byte enum) */
	if (pos >= buf_len) return -1;
	arc->compression = (inno_compress_method_t) buf[pos];
	pos += 1;

	/* architectures_allowed + architectures_installed_64bit flags */
	int arch_flag_bits = (v >= INNO_VER(5, 6, 0)) ? 5 : 4;
	int arch_bytes = (arch_flag_bits + 7) / 8;
	/* Two sets of arch flags */
	if (skip_bytes(buf, buf_len, &pos, arch_bytes * 2) < 0) return -1;

	/* disable_dir_page (1) + disable_program_group_page (1) - for >= 5.3.3 */
	if (v >= INNO_VER(5, 3, 3)) {
		if (skip_bytes(buf, buf_len, &pos, 2) < 0) return -1;
	}

	/* uninstall_display_size (8) for >= 5.5.0, (4) for >= 5.3.6 */
	if (v >= INNO_VER(5, 5, 0)) {
		if (skip_bytes(buf, buf_len, &pos, 8) < 0) return -1;
	} else if (v >= INNO_VER(5, 3, 6)) {
		if (skip_bytes(buf, buf_len, &pos, 4) < 0) return -1;
	}

	/* Header flags bitfield (variable count) */
	int num_flags;
	if (v >= INNO_VER(5, 5, 7)) num_flags = 44;
	else if (v >= INNO_VER(5, 5, 0)) num_flags = 43;
	else if (v >= INNO_VER(5, 3, 3)) num_flags = 37;
	else num_flags = 35;
	if (skip_flags(buf, buf_len, &pos, num_flags) < 0) return -1;

	/* ── Now skip through entry arrays to reach file entries ── */

	/* Each language entry: 10 strings + scalars */
	for (uint32_t i = 0; i < language_count; i++) {
		int lang_strings = (v >= INNO_VER(4, 2, 1)) ? 10 : 8;
		if (skip_strings(buf, buf_len, &pos, lang_strings) < 0) return -1;
		/* language_id (4) */
		if (skip_bytes(buf, buf_len, &pos, 4) < 0) return -1;
		/* codepage: NOT read for unicode >= 5.3.0 */
		if (!(ver->unicode && v >= INNO_VER(5, 3, 0))) {
			if (v >= INNO_VER(4, 2, 1)) {
				if (skip_bytes(buf, buf_len, &pos, 4) < 0) return -1;
			}
		}
		/* dialog_font_size(4) + title_font_size(4) + welcome_font_size(4) + copyright_font_size(4) */
		if (skip_bytes(buf, buf_len, &pos, 16) < 0) return -1;
		/* right_to_left (1 byte bool, >= 5.2.3) */
		if (v >= INNO_VER(5, 2, 3)) {
			if (skip_bytes(buf, buf_len, &pos, 1) < 0) return -1;
		}
	}
	/* Skip messages */
	for (uint32_t i = 0; i < message_count; i++) {
		/* 2 strings: name + value */
		if (skip_strings(buf, buf_len, &pos, 2) < 0) return -1;
		/* language_index (4) */
		if (skip_bytes(buf, buf_len, &pos, 4) < 0) return -1;
	}
	/* Skip permissions (>= 4.1.0: single binary_string per entry) */
	for (uint32_t i = 0; i < permission_count; i++) {
		if (skip_string(buf, buf_len, &pos) < 0) return -1;
	}
	/* Skip types */
	for (uint32_t i = 0; i < type_count; i++) {
		int type_strings = (v >= INNO_VER(4, 0, 1)) ? 4 : 3;
		if (skip_strings(buf, buf_len, &pos, type_strings) < 0) return -1;
		/* winver (20) */
		if (skip_bytes(buf, buf_len, &pos, 20) < 0) return -1;
		/* options: stored_flags (1 flag) → 1 byte */
		if (skip_bytes(buf, buf_len, &pos, 1) < 0) return -1;
		/* type: stored_enum (>= 4.0.3) → 1 byte */
		if (v >= INNO_VER(4, 0, 3)) {
			if (skip_bytes(buf, buf_len, &pos, 1) < 0) return -1;
		}
		/* size: uint64 (>= 4.0.0) */
		if (v >= INNO_VER(4, 0, 0)) {
			if (skip_bytes(buf, buf_len, &pos, 8) < 0) return -1;
		}
	}
	/* Skip components */
	for (uint32_t i = 0; i < component_count; i++) {
		int comp_strings = (v >= INNO_VER(4, 0, 1)) ? 5 : 4;
		if (skip_strings(buf, buf_len, &pos, comp_strings) < 0) return -1;
		/* extra_disk_space_required (8, >= 4.0.0) */
		if (v >= INNO_VER(4, 0, 0)) {
			if (skip_bytes(buf, buf_len, &pos, 8) < 0) return -1;
		}
		/* level (4, >= 4.0.0) */
		if (v >= INNO_VER(4, 0, 0)) {
			if (skip_bytes(buf, buf_len, &pos, 4) < 0) return -1;
		}
		/* used (1 byte bool, >= 4.0.0) */
		if (v >= INNO_VER(4, 0, 0)) {
			if (skip_bytes(buf, buf_len, &pos, 1) < 0) return -1;
		}
		/* winver (20) */
		if (skip_bytes(buf, buf_len, &pos, 20) < 0) return -1;
		/* options: stored_flags (5 flags for >= 4.2.3, 2 for older) */
		int comp_flags = (v >= INNO_VER(4, 2, 3)) ? 5 : 2;
		if (skip_flags(buf, buf_len, &pos, comp_flags) < 0) return -1;
		/* size (8, >= 4.0.0) */
		if (v >= INNO_VER(4, 0, 0)) {
			if (skip_bytes(buf, buf_len, &pos, 8) < 0) return -1;
		}
	}
	/* Skip tasks */
	for (uint32_t i = 0; i < task_count; i++) {
		int task_strings = (v >= INNO_VER(4, 0, 1)) ? 6 : 5;
		if (skip_strings(buf, buf_len, &pos, task_strings) < 0) return -1;
		/* level (4, >= 4.0.0) */
		if (v >= INNO_VER(4, 0, 0)) {
			if (skip_bytes(buf, buf_len, &pos, 4) < 0) return -1;
		}
		/* used (1 byte bool, >= 4.0.0) */
		if (v >= INNO_VER(4, 0, 0)) {
			if (skip_bytes(buf, buf_len, &pos, 1) < 0) return -1;
		}
		/* winver (20) */
		if (skip_bytes(buf, buf_len, &pos, 20) < 0) return -1;
		/* options: stored_flags (5 flags for >= 4.2.3) */
		int task_flags = (v >= INNO_VER(4, 2, 3)) ? 5 : 3;
		if (skip_flags(buf, buf_len, &pos, task_flags) < 0) return -1;
	}
	/* Skip directories */
	for (uint32_t i = 0; i < directory_count; i++) {
		int dir_strings = (v >= INNO_VER(4, 1, 0)) ? 7 : 5;
		if (skip_strings(buf, buf_len, &pos, dir_strings) < 0) return -1;
		/* attributes (4, >= 2.0.11) */
		if (v >= INNO_VER(2, 0, 11)) {
			if (skip_bytes(buf, buf_len, &pos, 4) < 0) return -1;
		}
		/* winver (20) */
		if (skip_bytes(buf, buf_len, &pos, 20) < 0) return -1;
		/* permission (int16, >= 4.1.0) */
		if (v >= INNO_VER(4, 1, 0)) {
			if (skip_bytes(buf, buf_len, &pos, 2) < 0) return -1;
		}
		/* options: stored_flags (5 flags for >= 5.2.0, 3 for older) */
		int dir_flags = (v >= INNO_VER(5, 2, 0)) ? 5 : 3;
		if (skip_flags(buf, buf_len, &pos, dir_flags) < 0) return -1;
	}
	/* ── Parse file entries (the ones we care about!) ── */
	if (!entry_count_allowed(file_count)) {
		INNO_LOG("too many files: %u (max %u)",
		         file_count, DXX_EXTRACT_MAX_ENTRIES);
		return -1;
	}
	if (file_count > (buf_len - pos) / INNO_MIN_FILE_ENTRY_SIZE) {
		INNO_LOG("file catalog cannot contain %u complete entries", file_count);
		return -1;
	}
	if (file_count > 0 && sizeof(*arc->files) > SIZE_MAX / file_count)
		return -1;
	if (file_count > 0) {
		arc->files =
		    (inno_file_entry_t *) inno_calloc(file_count, sizeof(*arc->files));
		if (!arc->files) {
			INNO_LOG("cannot allocate %u file entries", file_count);
			return -1;
		}
	}
	arc->file_count = file_count;

	for (uint32_t i = 0; i < file_count; i++) {
		inno_file_entry_t *fe = &arc->files[i];
		/* source (encoded_string) */
		if (skip_string(buf, buf_len, &pos) < 0) return -1;
		/* destination (encoded_string) */
		if (read_string(buf, buf_len, &pos, fe->destination,
		                sizeof(fe->destination), ver->unicode) < 0)
			return -1;
		/* install_font_name */
		if (skip_string(buf, buf_len, &pos) < 0) return -1;
		/* strong_assembly_name (>= 5.2.5) */
		if (v >= INNO_VER(5, 2, 5)) {
			if (skip_string(buf, buf_len, &pos) < 0) return -1;
		}

		/* Condition data (base item) */
		/* components, tasks (>= 2.0.0) */
		if (skip_strings(buf, buf_len, &pos, 2) < 0) return -1;
		/* languages (>= 4.0.1) */
		if (v >= INNO_VER(4, 0, 1)) {
			if (skip_string(buf, buf_len, &pos) < 0) return -1;
		}
		/* check (>= 4.0.0) */
		if (v >= INNO_VER(4, 0, 0)) {
			if (skip_string(buf, buf_len, &pos) < 0) return -1;
		}
		/* after_install (>= 4.1.0) */
		if (v >= INNO_VER(4, 1, 0)) {
			if (skip_string(buf, buf_len, &pos) < 0) return -1;
		}
		/* before_install (>= 4.1.0) — GOG Galaxy stores real filename here */
		if (v >= INNO_VER(4, 1, 0)) {
			char before_install[INNO_GALAXY_SCRIPT_LEN + 1u];
			char galaxy_destination[INNO_PATH_LEN];
			int captured;
			if (read_before_install(buf, buf_len, &pos, before_install,
			                        sizeof(before_install), ver->unicode,
			                        &captured) < 0)
				return -1;
			if (captured &&
			    parse_gog_galaxy_before_install(before_install,
			                                    galaxy_destination,
			                                    sizeof(galaxy_destination), NULL)) {
				memcpy(fe->destination, galaxy_destination,
				       strlen(galaxy_destination) + 1u);
				fe->gog_galaxy = 1;
			}
		}

		/* Windows version range (20 bytes) */
		if (skip_bytes(buf, buf_len, &pos, 20) < 0) return -1;

		/* location (uint32) */
		if (pos + 4 > buf_len) return -1;
		fe->location = get_u32(buf + pos);
		pos += 4;

		/* attributes (uint32) */
		if (skip_bytes(buf, buf_len, &pos, 4) < 0) return -1;

		/* external_size (uint64, >= 4.0.0) */
		if (v >= INNO_VER(4, 0, 0)) {
			if (pos + 8 > buf_len) return -1;
			fe->external_size = get_u64(buf + pos);
			pos += 8;
		}
		/* permission (int16, >= 4.1.0) */
		if (v >= INNO_VER(4, 1, 0)) {
			if (skip_bytes(buf, buf_len, &pos, 2) < 0) return -1;
		}

		/* file flags (~32 for 5.5.x) */
		int file_flags = 32; /* 4 bytes for 5.5.x/5.6.x */
		if (skip_flags(buf, buf_len, &pos, file_flags) < 0) return -1;

		/* file type enum (uint8) */
		if (skip_bytes(buf, buf_len, &pos, 1) < 0) return -1;
	}

	/* We don't need to parse entries AFTER file entries for extraction.
	 * The remaining entries (icons, ini, registry, etc.) are in this
	 * same block stream but we skip them all. */

	(void) icon_count;
	(void) ini_entry_count;
	(void) registry_entry_count;
	(void) delete_entry_count;
	(void) uninstall_delete_entry_count;
	(void) run_entry_count;
	(void) uninstall_run_entry_count;

	return 0;
}

/* ── Data entry parser (setup-0 block stream 2) ──────────────────── */

static int parse_data_entries(const uint8_t *buf, size_t buf_len,
                              inno_version_t *ver, inno_archive_t *arc)
{
	size_t pos = 0;
	int v = INNO_VER(ver->major, ver->minor, ver->patch);
	inno_checksum_layout_t checksum_layout = checksum_layout_for_version(ver);

	for (uint32_t i = 0; i < arc->data_entry_count; i++) {
		inno_data_entry_t *de = &arc->data_entries[i];

		/* first_slice, last_slice */
		if (pos + 8 > buf_len) return -1;
		de->first_slice = get_u32(buf + pos);
		pos += 4;
		de->last_slice = get_u32(buf + pos);
		pos += 4;

		/* chunk_offset (uint32 for < 4.0.0, really uint32 in our range) */
		if (pos + 4 > buf_len) return -1;
		de->chunk_offset = get_u32(buf + pos);
		pos += 4;

		/* file_offset (uint64 for >= 4.0.1) */
		if (v >= INNO_VER(4, 0, 1)) {
			if (pos + 8 > buf_len) return -1;
			de->file_offset = get_u64(buf + pos);
			pos += 8;
		}

		/* file_size (uint64 for >= 4.0.0) */
		if (pos + 8 > buf_len) return -1;
		de->file_size = get_u64(buf + pos);
		pos += 8;

		/* chunk_compressed_size (uint64) */
		if (pos + 8 > buf_len) return -1;
		de->chunk_compressed_size = get_u64(buf + pos);
		pos += 8;

		/* SHA-1 hash (20 bytes, >= 5.3.9) */
		/* Earlier supported versions carry MD5 in the same layout slot */
		if (pos + checksum_layout.digest_size > buf_len) return -1;
		memcpy(de->checksum, buf + pos, checksum_layout.digest_size);
		de->checksum_type = checksum_layout.type;
		pos += checksum_layout.digest_size;

		/* timestamp (int64) */
		if (skip_bytes(buf, buf_len, &pos, 8) < 0) return -1;

		/* file version (8 bytes) */
		if (skip_bytes(buf, buf_len, &pos, 8) < 0) return -1;

		/* data flags */
		/* Read the raw flag bytes */
		size_t flag_bytes = data_flag_bytes_for_version(v);
		if (pos + flag_bytes > buf_len) return -1;

		/* Parse relevant flags by bit position */
		/* Bit layout for 5.5.7+, flags are in LSB-first order:
		 * 0: VersionInfoValid
		 * 1: VersionInfoNotValid
		 * 2: TimeStampInUTC
		 * 3: IsUninstallerExe
		 * 4: CallInstructionOptimized
		 * 5: Touch
		 * 6: ChunkEncrypted
		 * 7: ChunkCompressed
		 * 8: SolidBreak
		 * 9: Sign
		 * 10: SignOnce
		 */
		de->call_instruction_optimized = 0;
		de->chunk_encrypted = 0;
		de->chunk_compressed = 0;

		if (v >= INNO_VER(4, 1, 8)) {
			de->call_instruction_optimized = (buf[pos + 0] >> 4) & 1; /* bit 4 */
		}
		if (v >= INNO_VER(4, 2, 2)) {
			de->chunk_encrypted = (buf[pos + 0] >> 6) & 1; /* bit 6 */
		}
		if (v >= INNO_VER(4, 2, 5)) {
			de->chunk_compressed = (buf[pos + 0] >> 7) & 1; /* bit 7 */
		}

		pos += flag_bytes;
	}
	return 0;
}

#ifdef INNO_READER_TESTING
void inno_test_set_allocation_fail_after(int allocations)
{
	inno_test_allocations_before_failure = allocations;
}

int inno_test_find_pe_resource_11111(int fd, uint64_t source_size,
                                     uint64_t *offset_out)
{
	return find_pe_resource_11111(fd, source_size, offset_out);
}

int inno_test_read_string(const uint8_t *buffer, size_t buffer_size,
                          char *output, size_t output_size, int unicode)
{
	size_t position = 0;
	if (read_string(buffer, buffer_size, &position, output,
	                output_size, unicode) < 0)
		return -1;
	return position == buffer_size ? 0 : -1;
}

int inno_test_destination_path_valid(const char *path)
{
	return valid_destination_path(path);
}

int inno_test_parse_gog_galaxy_before_install(const char *script,
                                              char *destination,
                                              size_t destination_size,
                                              uint32_t *part_count)
{
	return parse_gog_galaxy_before_install(script, destination,
	                                       destination_size, part_count);
}

int inno_test_parse_version_id(const uint8_t id[INNO_VERSION_ID_SIZE],
                               inno_version_t *version)
{
	return parse_version_string(id, version);
}

int inno_test_checksum_layout(const inno_version_t *version,
                              inno_checksum_type_t *type_out,
                              size_t *digest_size_out)
{
	if (!version || !type_out || !digest_size_out) return -1;
	inno_checksum_layout_t layout = checksum_layout_for_version(version);
	*type_out = layout.type;
	*digest_size_out = layout.digest_size;
	return 0;
}

int inno_test_parse_header_stream(const uint8_t *buffer, size_t buffer_size,
                                  const inno_version_t *version,
                                  inno_compress_method_t *compression_out)
{
	if (!buffer || !version || !compression_out) return -1;
	inno_archive_t archive;
	inno_version_t mutable_version = *version;
	memset(&archive, 0, sizeof(archive));
	if (parse_header_stream1(buffer, buffer_size, &mutable_version, &archive) < 0) {
		free(archive.files);
		return -1;
	}
	*compression_out = archive.compression;
	free(archive.files);
	return 0;
}

int inno_test_parse_file_catalog(const uint8_t *buffer, size_t buffer_size,
                                 const inno_version_t *version,
                                 uint32_t *file_count_out,
                                 char *last_destination,
                                 size_t last_destination_size,
                                 int *last_gog_galaxy)
{
	inno_archive_t archive;
	inno_version_t mutable_version;
	int result;

	if (!buffer || !version || !file_count_out ||
	    !last_destination || last_destination_size == 0)
		return -1;
	memset(&archive, 0, sizeof(archive));
	mutable_version = *version;
	result = parse_header_stream1(buffer, buffer_size,
	                              &mutable_version, &archive);
	if (result == 0) {
		*file_count_out = archive.file_count;
		if (archive.file_count > 0) {
			snprintf(last_destination, last_destination_size, "%s",
			         archive.files[archive.file_count - 1].destination);
			if (last_gog_galaxy)
				*last_gog_galaxy = archive.files[archive.file_count - 1].gog_galaxy;
		} else {
			last_destination[0] = '\0';
			if (last_gog_galaxy) *last_gog_galaxy = 0;
		}
	}
	free(archive.files);
	return result;
}

int inno_test_parse_data_entries(const uint8_t *buffer, size_t buffer_size,
                                 const inno_version_t *version,
                                 inno_data_entry_t *entries,
                                 uint32_t entry_count)
{
	if (!buffer || !version || !entries ||
	    !entry_count_allowed(entry_count))
		return -1;
	inno_archive_t archive;
	inno_version_t mutable_version = *version;
	memset(&archive, 0, sizeof(archive));
	memset(entries, 0, (size_t) entry_count * sizeof(*entries));
	archive.data_entries = entries;
	archive.data_entry_count = entry_count;
	return parse_data_entries(buffer, buffer_size, &mutable_version, &archive);
}

int inno_test_entry_count_allowed(uint32_t entry_count)
{
	return entry_count_allowed(entry_count);
}

int inno_test_data_location_valid(uint32_t data_entry_count,
                                  int has_backing_array,
                                  uint32_t location)
{
	inno_archive_t archive;
	inno_data_entry_t entry;

	memset(&archive, 0, sizeof(archive));
	archive.data_entry_count = data_entry_count;
	archive.data_entries = has_backing_array ? &entry : NULL;
	return data_location_valid(&archive, location);
}
#endif

/* ── Chunk decompressor (for setup-1.bin data) ───────────────────── */

static int grow_decompressed_buffer(uint8_t **buffer, size_t *capacity,
                                    size_t output_limit)
{
	size_t next_capacity;
	uint8_t *resized;

	if (!buffer || !*buffer || !capacity || *capacity >= output_limit)
		return -1;
	next_capacity = *capacity > output_limit / 2u ? output_limit : *capacity * 2u;
	resized = (uint8_t *) realloc(*buffer, next_capacity);
	if (!resized)
		return -1;
	*buffer = resized;
	*capacity = next_capacity;
	return 0;
}

static int lzma_buffered_decode_finished(SRes result, ELzmaStatus status,
                                         size_t consumed, size_t input_size)
{
	return result == SZ_OK && consumed == input_size &&
	       (status == LZMA_STATUS_FINISHED_WITH_MARK ||
	        status == LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK);
}

static int validate_chunk_file_range(const inno_archive_t *arc,
                                     const inno_data_entry_t *de,
                                     inno_compress_method_t method,
                                     uint64_t *chunk_pos_out,
                                     uint64_t *payload_pos_out,
                                     uint64_t *range_end_out)
{
	uint64_t chunk_pos;
	uint64_t payload_pos;
	if (!arc || !de || arc->fd < 0 ||
	    arc->data_offset > UINT64_MAX - de->chunk_offset)
		return -1;
	chunk_pos = arc->data_offset + de->chunk_offset;
	if (chunk_pos > arc->source_size ||
	    arc->source_size - chunk_pos < 4)
		return -1;
	payload_pos = chunk_pos + 4;
	if (de->chunk_compressed_size > arc->source_size - payload_pos ||
	    de->file_size > UINT64_MAX - de->file_offset)
		return -1;
	if (method == INNO_COMPRESS_STORED &&
	    (de->file_offset > de->chunk_compressed_size ||
	     de->file_size > de->chunk_compressed_size - de->file_offset))
		return -1;
	if (chunk_pos_out) *chunk_pos_out = chunk_pos;
	if (payload_pos_out) *payload_pos_out = payload_pos;
	if (range_end_out) *range_end_out = de->file_offset + de->file_size;
	return 0;
}

static uint8_t *decompress_chunk(inno_archive_t *arc,
                                 const inno_data_entry_t *de,
                                 inno_compress_method_t method,
                                 size_t *out_len,
                                 inno_progress_fn progress, void *progress_data,
                                 const char *progress_name)
{
	uint64_t chunk_pos;
	uint64_t payload_pos;
	uint64_t required_output;
	inno_compress_method_t actual_method =
	    de->chunk_compressed ? method : INNO_COMPRESS_STORED;
	if (validate_chunk_file_range(arc, de, actual_method, &chunk_pos,
	                              &payload_pos, &required_output) < 0)
		return NULL;

	/* Read and verify zlb magic */
	uint8_t magic[4];
	if (read_at(arc->fd, chunk_pos, magic, 4) < 0) return NULL;
	if (magic[0] != 'z' || magic[1] != 'l' || magic[2] != 'b' || magic[3] != 0x1A) {
		INNO_LOG("missing zlb magic at 0x%llx", (unsigned long long) chunk_pos);
		return NULL;
	}

	uint64_t comp_size = de->chunk_compressed_size; /* size after zlb magic */
	if (comp_size > DXX_EXTRACT_MAX_MEMORY_BYTES ||
	    required_output > DXX_EXTRACT_MAX_ENTRY_BYTES ||
	    (actual_method == INNO_COMPRESS_STORED && required_output > comp_size) ||
	    (actual_method != INNO_COMPRESS_STORED &&
	     required_output > DXX_EXTRACT_MAX_MEMORY_BYTES - comp_size) ||
	    !dxx_extract_ratio_allowed(required_output, comp_size))
		return NULL;
	uint8_t *comp_data = (uint8_t *) malloc((size_t) comp_size);
	if (!comp_data) return NULL;
	if (read_at(arc->fd, payload_pos, comp_data, (size_t) comp_size) < 0) {
		free(comp_data);
		return NULL;
	}

	/* Cap per-iteration output size so decompression loops iterate
	   frequently enough for progress callbacks to fire. */
	const size_t DECOMP_STEP = 262144; /* 256 KB */

	if (actual_method == INNO_COMPRESS_STORED) {
		*out_len = (size_t) comp_size;
		return comp_data;
	}

	if (actual_method == INNO_COMPRESS_ZLIB) {
		/* Zlib decompression */
		size_t output_limit = (size_t) (DXX_EXTRACT_MAX_MEMORY_BYTES - comp_size);
		size_t decomp_cap = required_output > output_limit / 2u
		                        ? output_limit
		                        : (size_t) required_output * 2u;
		if (decomp_cap < 65536) decomp_cap = 65536;
		if (decomp_cap > output_limit) decomp_cap = output_limit;
		uint8_t *decomp = (uint8_t *) malloc(decomp_cap);
		if (!decomp) {
			free(comp_data);
			return NULL;
		}

		z_stream zs;
		memset(&zs, 0, sizeof(zs));
		if (inflateInit(&zs) != Z_OK) {
			free(decomp);
			free(comp_data);
			return NULL;
		}

		zs.next_in = comp_data;
		zs.avail_in = (uInt) comp_size;
		zs.next_out = decomp;
		zs.avail_out = (uInt) (decomp_cap < DECOMP_STEP ? decomp_cap : DECOMP_STEP);

		int zret = Z_OK;
		int decode_finished = 0;
		size_t total_out = 0;
		size_t last_progress_out = 0;
		for (;;) {
			unsigned long previous_in = zs.total_in;
			unsigned long previous_out = zs.total_out;
			zret = inflate(&zs, Z_NO_FLUSH);
			total_out = zs.total_out;
			if (progress && progress_name &&
			    total_out - last_progress_out >= 1048576) {
				progress(progress_name, (long long) zs.total_in,
				         (long long) comp_size, progress_data);
				last_progress_out = total_out;
			}
			if (zs.avail_out == 0) {
				if (total_out >= decomp_cap) {
					if (grow_decompressed_buffer(&decomp, &decomp_cap,
					                             output_limit) < 0) {
						inflateEnd(&zs);
						free(decomp);
						free(comp_data);
						return NULL;
					}
				}
				zs.next_out = decomp + total_out;
				size_t remain = decomp_cap - total_out;
				zs.avail_out = (uInt) (remain < DECOMP_STEP ? remain : DECOMP_STEP);
			}
			if (zret == Z_STREAM_END) {
				decode_finished = zs.total_in == comp_size;
				break;
			}
			if (zret != Z_OK ||
			    (zs.total_in == previous_in && zs.total_out == previous_out) ||
			    zs.avail_in == 0)
				break;
		}
		inflateEnd(&zs);
		free(comp_data);

		if (!decode_finished ||
		    !dxx_extract_ratio_allowed(total_out, comp_size)) {
			free(decomp);
			return NULL;
		}
		*out_len = total_out;
		return decomp;
	}

	if (actual_method == INNO_COMPRESS_LZMA1) {
		if (comp_size < 5) {
			free(comp_data);
			return NULL;
		}
		uint8_t lzma_props[5];
		memcpy(lzma_props, comp_data, 5);

		size_t output_limit = (size_t) (DXX_EXTRACT_MAX_MEMORY_BYTES - comp_size);
		size_t decomp_cap = required_output > output_limit / 2u
		                        ? output_limit
		                        : (size_t) required_output * 2u;
		if (decomp_cap < 65536) decomp_cap = 65536;
		if (decomp_cap > output_limit) decomp_cap = output_limit;
		uint8_t *decomp = (uint8_t *) malloc(decomp_cap);
		if (!decomp) {
			free(comp_data);
			return NULL;
		}

		CLzmaDec dec;
		LzmaDec_Construct(&dec);
		if (LzmaDec_Allocate(&dec, lzma_props, 5, &g_lzma_alloc) != SZ_OK) {
			free(decomp);
			free(comp_data);
			return NULL;
		}
		LzmaDec_Init(&dec);

		size_t src_pos = 5;
		size_t decomp_len = 0;
		size_t last_progress_pos = 0;
		int decode_finished = 0;
		while (src_pos < comp_size) {
			if (progress && progress_name &&
			    src_pos - last_progress_pos >= 1048576) {
				progress(progress_name, (long long) src_pos,
				         (long long) comp_size, progress_data);
				last_progress_pos = src_pos;
			}
			if (decomp_len >= decomp_cap) {
				if (grow_decompressed_buffer(&decomp, &decomp_cap,
				                             output_limit) < 0) {
					LzmaDec_Free(&dec, &g_lzma_alloc);
					free(decomp);
					free(comp_data);
					return NULL;
				}
			}
			size_t avail = decomp_cap - decomp_len;
			size_t dest_len = avail < DECOMP_STEP ? avail : DECOMP_STEP;
			size_t src_len = comp_size - src_pos;
			ELzmaStatus status;
			SRes res = LzmaDec_DecodeToBuf(&dec, decomp + decomp_len, &dest_len,
			                               comp_data + src_pos, &src_len,
			                               LZMA_FINISH_ANY, &status);
			decomp_len += dest_len;
			src_pos += src_len;
			if (lzma_buffered_decode_finished(res, status, src_pos, comp_size)) {
				decode_finished = 1;
				break;
			}
			if (res != SZ_OK) break;
			if (status == LZMA_STATUS_FINISHED_WITH_MARK ||
			    status == LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK) break;
			if (dest_len == 0 && src_len == 0) break;
		}

		LzmaDec_Free(&dec, &g_lzma_alloc);
		free(comp_data);
		if (!decode_finished ||
		    !dxx_extract_ratio_allowed(decomp_len, comp_size)) {
			free(decomp);
			return NULL;
		}
		*out_len = decomp_len;
		return decomp;
	}

	if (actual_method == INNO_COMPRESS_LZMA2) {
		if (comp_size < 1) {
			free(comp_data);
			return NULL;
		}
		uint8_t lzma2_prop = comp_data[0]; /* single property byte */

		size_t output_limit = (size_t) (DXX_EXTRACT_MAX_MEMORY_BYTES - comp_size);
		size_t decomp_cap = required_output > output_limit / 2u
		                        ? output_limit
		                        : (size_t) required_output * 2u;
		if (decomp_cap < 65536) decomp_cap = 65536;
		if (decomp_cap > output_limit) decomp_cap = output_limit;
		uint8_t *decomp = (uint8_t *) malloc(decomp_cap);
		if (!decomp) {
			free(comp_data);
			return NULL;
		}

		CLzma2Dec dec;
		Lzma2Dec_Construct(&dec);
		if (Lzma2Dec_Allocate(&dec, lzma2_prop, &g_lzma_alloc) != SZ_OK) {
			free(decomp);
			free(comp_data);
			return NULL;
		}
		Lzma2Dec_Init(&dec);

		size_t src_pos = 1;
		size_t decomp_len = 0;
		size_t last_progress_pos2 = 0;
		int decode_finished = 0;
		while (src_pos < comp_size) {
			if (progress && progress_name &&
			    src_pos - last_progress_pos2 >= 1048576) {
				progress(progress_name, (long long) src_pos,
				         (long long) comp_size, progress_data);
				last_progress_pos2 = src_pos;
			}
			if (decomp_len >= decomp_cap) {
				if (grow_decompressed_buffer(&decomp, &decomp_cap,
				                             output_limit) < 0) {
					Lzma2Dec_Free(&dec, &g_lzma_alloc);
					free(decomp);
					free(comp_data);
					return NULL;
				}
			}
			size_t avail2 = decomp_cap - decomp_len;
			size_t dest_len = avail2 < DECOMP_STEP ? avail2 : DECOMP_STEP;
			size_t src_len = comp_size - src_pos;
			ELzmaStatus status;
			SRes res = Lzma2Dec_DecodeToBuf(&dec, decomp + decomp_len, &dest_len,
			                                comp_data + src_pos, &src_len,
			                                LZMA_FINISH_ANY, &status);
			decomp_len += dest_len;
			src_pos += src_len;
			if (lzma_buffered_decode_finished(res, status, src_pos, comp_size)) {
				decode_finished = 1;
				break;
			}
			if (res != SZ_OK) break;
			if (status == LZMA_STATUS_FINISHED_WITH_MARK ||
			    status == LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK) break;
			if (dest_len == 0 && src_len == 0) break;
		}

		Lzma2Dec_Free(&dec, &g_lzma_alloc);
		free(comp_data);
		if (!decode_finished ||
		    !dxx_extract_ratio_allowed(decomp_len, comp_size)) {
			free(decomp);
			return NULL;
		}
		*out_len = decomp_len;
		return decomp;
	}

	if (actual_method == INNO_COMPRESS_BZIP2) {
		INNO_LOG("BZip2 decompression not implemented");
		free(comp_data);
		return NULL;
	}

	INNO_LOG("unknown compression method %d", actual_method);
	free(comp_data);
	return NULL;
}

/* ── Public API ──────────────────────────────────────────────────── */

static int inno_open_owned_fd(int fd, const char *source_name, inno_archive_t *arc)
{
	if (fd < 0 || !arc) return -1;
	memset(arc, 0, sizeof(*arc));
	arc->fd = -1;
	arc->fd = fd;
	if (!source_name) source_name = "<fd>";
	if (get_fd_size(fd, &arc->source_size) < 0) {
		INNO_LOG("cannot determine installer size for %s", source_name);
		CLOSE_FD(fd);
		arc->fd = -1;
		return -1;
	}

	/* ── Find offset table ── */
	uint64_t table_offset = 0;
	int found = 0;

	/* Method 1: PE resource 11111 */
	if (find_pe_resource_11111(fd, arc->source_size, &table_offset) == 0) {
		found = 1;
	}

	/* Method 2: fixed offset 0x30 (older InnoSetup) */
	if (!found) {
		uint8_t buf[12];
		if (read_at(fd, 0x30, buf, 4) == 0 && get_u32(buf) == 0x6F6E6E49) {
			if (read_at(fd, 0x34, buf, 8) == 0) {
				uint32_t off = get_u32(buf);
				uint32_t noff = get_u32(buf + 4);
				if (off == ~noff) {
					table_offset = off;
					found = 1;
				}
			}
		}
	}

	/* Method 3: scan for known magic (fallback) */
	if (!found) {
		uint8_t scan_buf[65536];
		size_t scan_len = 65536;
		if (read_at(fd, 0, scan_buf, scan_len) == 0) {
			for (size_t i = 0; i + 12 <= scan_len; i++) {
				if (identify_loader(scan_buf + i) != LOADER_VER_UNKNOWN) {
					table_offset = i;
					found = 1;
					break;
				}
			}
		}
	}

	if (!found) {
		INNO_LOG("cannot find InnoSetup offset table in %s", source_name);
		CLOSE_FD(fd);
		arc->fd = -1;
		return -1;
	}

	/* ── Parse offset table ── */
	uint64_t header_offset, data_offset;
	if (parse_offset_table(fd, table_offset, &header_offset, &data_offset) < 0) {
		CLOSE_FD(fd);
		arc->fd = -1;
		return -1;
	}
	arc->header_offset = header_offset;
	arc->data_offset = data_offset;

	/* ── Read version ID (64 bytes at header_offset) ── */
	uint8_t version_id[INNO_VERSION_ID_SIZE];
	if (read_at(fd, header_offset, version_id, sizeof(version_id)) < 0) {
		INNO_LOG("cannot read version ID at 0x%llx", (unsigned long long) header_offset);
		CLOSE_FD(fd);
		arc->fd = -1;
		return -1;
	}
	if (parse_version_string(version_id, &arc->version) < 0) {
		CLOSE_FD(fd);
		arc->fd = -1;
		return -1;
	}

	INNO_LOG("version: %d.%d.%d unicode=%d",
	         arc->version.major, arc->version.minor, arc->version.patch,
	         arc->version.unicode);

	int v = INNO_VER(arc->version.major, arc->version.minor, arc->version.patch);
	if (v < INNO_VER(5, 3, 0) || v > INNO_VER(5, 6, 99)) {
		INNO_LOG("unsupported version %d.%d.%d (need 5.3.x - 5.6.x)",
		         arc->version.major, arc->version.minor, arc->version.patch);
		CLOSE_FD(fd);
		arc->fd = -1;
		return -1;
	}

	/* ── Decompress block stream 1 (main header + file entries) ── */
	size_t stream1_len = 0, stream1_consumed = 0;
	uint8_t *stream1 = decompress_block_stream(fd, header_offset + INNO_VERSION_ID_SIZE,
	                                           &stream1_len, &stream1_consumed);
	if (!stream1) {
		INNO_LOG("failed to decompress header block stream 1");
		CLOSE_FD(fd);
		arc->fd = -1;
		return -1;
	}

	/* Parse the header and file entries */
	if (parse_header_stream1(stream1, stream1_len, &arc->version, arc) < 0) {
		INNO_LOG("failed to parse header");
		free(stream1);
		inno_close(arc);
		return -1;
	}
	free(stream1);

	/* ── Decompress block stream 2 (data entries) ── */
	size_t stream2_len = 0, stream2_consumed = 0;
	uint8_t *stream2 = decompress_block_stream(
	    fd, header_offset + INNO_VERSION_ID_SIZE + stream1_consumed,
	    &stream2_len, &stream2_consumed);
	if (!stream2) {
		INNO_LOG("failed to decompress header block stream 2");
		inno_close(arc);
		return -1;
	}
	size_t data_entry_size = data_entry_size_for_version(&arc->version);
	if (arc->data_entry_count > stream2_len / data_entry_size ||
	    (arc->data_entry_count > 0 &&
	     sizeof(*arc->data_entries) > SIZE_MAX / arc->data_entry_count)) {
		INNO_LOG("data stream cannot contain %u complete entries",
		         arc->data_entry_count);
		free(stream2);
		inno_close(arc);
		return -1;
	}
	if (arc->data_entry_count > 0) {
		arc->data_entries = (inno_data_entry_t *) inno_calloc(
		    arc->data_entry_count, sizeof(*arc->data_entries));
		if (!arc->data_entries) {
			free(stream2);
			inno_close(arc);
			return -1;
		}
	}

	if (parse_data_entries(stream2, stream2_len, &arc->version, arc) < 0) {
		INNO_LOG("failed to parse data entries");
		free(stream2);
		inno_close(arc);
		return -1;
	}
	free(stream2);

	return (int) arc->file_count;
}

int inno_open(const char *exe_path, inno_archive_t *arc)
{
	if (!exe_path || !arc) return -1;
	int fd = OPEN_RB(exe_path);
	if (fd < 0) {
		INNO_LOG("cannot open %s: %s", exe_path, strerror(errno));
		return -1;
	}
	return inno_open_owned_fd(fd, exe_path, arc);
}

int inno_open_fd(int source_fd, inno_archive_t *arc)
{
	if (source_fd < 0 || !arc) return -1;
	int fd = DUP_FD(source_fd);
	if (fd < 0) {
		INNO_LOG("cannot duplicate installer fd: %s", strerror(errno));
		return -1;
	}
	return inno_open_owned_fd(fd, "<fd>", arc);
}

typedef int (*inno_chunk_sink_fn)(const uint8_t *data, size_t len, void *user_data);

static int stream_chunk_file_range(const inno_archive_t *arc,
                                   const inno_file_entry_t *fe,
                                   const inno_data_entry_t *de,
                                   inno_compress_method_t method,
                                   inno_progress_fn progress,
                                   void *progress_data,
                                   inno_chunk_sink_fn sink,
                                   void *sink_data);

typedef struct {
	const inno_file_entry_t *fe;
	const inno_data_entry_t *de;
	FILE *out;
	inno_progress_fn progress;
	void *progress_data;
	z_stream zs;
	size_t inner_size;
	size_t written;
	size_t last_progress_in;
	int finished;
	int initialized;
	inno_checksum_ctx_t checksum;
} gog_galaxy_stream_writer_t;

static int emit_chunk_range(const uint8_t *buf, size_t buf_len,
                            uint64_t *stream_pos,
                            uint64_t range_start,
                            uint64_t range_end,
                            inno_chunk_sink_fn sink,
                            void *sink_data)
{
	uint64_t chunk_start = *stream_pos;
	uint64_t chunk_end = chunk_start + buf_len;
	if (chunk_end > range_start && chunk_start < range_end) {
		size_t start = 0;
		if (chunk_start < range_start)
			start = (size_t) (range_start - chunk_start);
		uint64_t overlap_end = chunk_end < range_end ? chunk_end : range_end;
		size_t count = (size_t) (overlap_end - (chunk_start + start));
		if (count > 0 && sink(buf + start, count, sink_data) < 0)
			return -1;
	}
	*stream_pos = chunk_end;
	return 0;
}

typedef struct {
	int fd;
	uint64_t next_offset;
	uint64_t remaining;
	uint64_t total_in;
	uint8_t buf[INNO_STREAM_BUFFER_SIZE];
	size_t pos;
	size_t len;
} inno_stream_input_t;

static void init_stream_input(inno_stream_input_t *input, int fd,
                              uint64_t offset, uint64_t size,
                              uint64_t initial_total_in)
{
	input->fd = fd;
	input->next_offset = offset;
	input->remaining = size;
	input->total_in = initial_total_in;
	input->pos = 0;
	input->len = 0;
}

static int refill_stream_input(inno_stream_input_t *input)
{
	if (input->remaining == 0) {
		input->pos = 0;
		input->len = 0;
		return 0;
	}
	size_t want = sizeof(input->buf);
	if ((uint64_t) want > input->remaining)
		want = (size_t) input->remaining;
	if (read_at(input->fd, input->next_offset, input->buf, want) < 0)
		return -1;
	input->next_offset += want;
	input->remaining -= want;
	input->pos = 0;
	input->len = want;
	return 1;
}

static int gog_galaxy_stream_writer_init(gog_galaxy_stream_writer_t *writer,
                                         const inno_file_entry_t *fe,
                                         const inno_data_entry_t *de,
                                         FILE *out,
                                         inno_progress_fn progress,
                                         void *progress_data)
{
	memset(writer, 0, sizeof(*writer));
	writer->fe = fe;
	writer->de = de;
	writer->out = out;
	writer->progress = progress;
	writer->progress_data = progress_data;
	writer->inner_size = (size_t) de->file_size;
	if (inno_checksum_init(&writer->checksum, de->checksum_type) < 0) {
		INNO_LOG("unsupported file checksum for %s", fe->destination);
		return -1;
	}
	if (inflateInit(&writer->zs) != Z_OK) {
		INNO_LOG("inflateInit failed for GOG Galaxy file %s", fe->destination);
		return -1;
	}
	writer->initialized = 1;
	INNO_LOG("streaming GOG Galaxy file %s without outer chunk buffer: inner_compressed=%llu expected_output=%llu",
	         fe->destination,
	         (unsigned long long) de->file_size,
	         (unsigned long long) fe->external_size);
	return 0;
}

static int gog_galaxy_stream_writer_feed(const uint8_t *data, size_t len, void *user_data)
{
	gog_galaxy_stream_writer_t *writer = (gog_galaxy_stream_writer_t *) user_data;
	uint8_t out_buf[INNO_STREAM_BUFFER_SIZE];
	int zret;

	if (writer->finished) {
		INNO_LOG("trailing inner zlib data for %s after stream end",
		         writer->fe->destination);
		return -1;
	}
	inno_checksum_update(&writer->checksum, data, len);

	writer->zs.next_in = (Bytef *) data;
	writer->zs.avail_in = (uInt) len;
	while (writer->zs.avail_in > 0) {
		unsigned long prev_in = writer->zs.total_in;
		unsigned long prev_out = writer->zs.total_out;
		writer->zs.next_out = out_buf;
		writer->zs.avail_out = (uInt) sizeof(out_buf);
		zret = inflate(&writer->zs, Z_NO_FLUSH);
		if (zret != Z_OK && zret != Z_STREAM_END) {
			INNO_LOG("inner zlib inflate failed for %s: zret=%d total_in=%lu total_out=%lu",
			         writer->fe->destination,
			         zret,
			         (unsigned long) writer->zs.total_in,
			         (unsigned long) writer->zs.total_out);
			return -1;
		}

		size_t produced = sizeof(out_buf) - writer->zs.avail_out;
		if (produced > 0) {
			if (writer->written > DXX_EXTRACT_MAX_ENTRY_BYTES - produced ||
			    (writer->fe->external_size > 0 &&
			     writer->written + produced > writer->fe->external_size)) {
				INNO_LOG("GOG Galaxy output exceeds extraction budget for %s",
				         writer->fe->destination);
				return -1;
			}
			if (writer->out && fwrite(out_buf, 1, produced, writer->out) != produced) {
				INNO_LOG("write error while streaming GOG Galaxy file %s: %s",
				         writer->fe->destination,
				         strerror(errno));
				return -1;
			}
			writer->written += produced;
		}

		if (writer->progress && writer->inner_size > 0 &&
		    (size_t) writer->zs.total_in - writer->last_progress_in >= 1048576) {
			long long done =
			    (long long) (((uint64_t) writer->zs.total_in * writer->de->chunk_compressed_size) /
			                 writer->inner_size);
			if (done > (long long) writer->de->chunk_compressed_size)
				done = (long long) writer->de->chunk_compressed_size;
			writer->progress(writer->fe->destination, done,
			                 (long long) writer->de->chunk_compressed_size,
			                 writer->progress_data);
			writer->last_progress_in = (size_t) writer->zs.total_in;
		}

		if (zret == Z_STREAM_END) {
			writer->finished = 1;
			break;
		}

		if (writer->zs.total_in == prev_in && writer->zs.total_out == prev_out) {
			INNO_LOG("inner zlib made no progress for %s",
			         writer->fe->destination);
			return -1;
		}
	}

	return 0;
}

static int gog_galaxy_stream_writer_finish(gog_galaxy_stream_writer_t *writer,
                                           size_t *written_out)
{
	if (writer->initialized)
		inflateEnd(&writer->zs);
	if (!writer->finished) {
		INNO_LOG("inner zlib stream ended early for %s: total_in=%lu total_out=%lu",
		         writer->fe->destination,
		         (unsigned long) writer->zs.total_in,
		         (unsigned long) writer->zs.total_out);
		return -1;
	}
	if (written_out)
		*written_out = writer->written;
	if (writer->fe->external_size > 0 && writer->written != (size_t) writer->fe->external_size) {
		INNO_LOG("GOG Galaxy output size mismatch for %s: expected %llu got %zu",
		         writer->fe->destination,
		         (unsigned long long) writer->fe->external_size,
		         writer->written);
	}
	return 0;
}

typedef struct {
	const inno_file_entry_t *fe;
	FILE *out;
	size_t written;
	inno_checksum_ctx_t checksum;
} raw_file_stream_writer_t;

static int raw_file_stream_writer_feed(const uint8_t *data, size_t len, void *user_data)
{
	raw_file_stream_writer_t *writer = (raw_file_stream_writer_t *) user_data;
	inno_checksum_update(&writer->checksum, data, len);
	if (fwrite(data, 1, len, writer->out) != len) {
		INNO_LOG("write error while streaming file %s: %s",
		         writer->fe->destination,
		         strerror(errno));
		return -1;
	}
	writer->written += len;
	return 0;
}

static int extract_regular_file_streamed(inno_archive_t *arc,
                                         const inno_file_entry_t *fe,
                                         const inno_data_entry_t *de,
                                         const char *output_path,
                                         inno_progress_fn progress,
                                         void *user_data,
                                         size_t *written_out)
{
	raw_file_stream_writer_t writer;
	char *temporary_path = NULL;
	FILE *out = open_temporary_output(output_path, &temporary_path);
	if (!out) {
		return -1;
	}
	writer.fe = fe;
	writer.out = out;
	writer.written = 0;
	if (inno_checksum_init(&writer.checksum, de->checksum_type) < 0) {
		fclose(out);
		remove_output_path(temporary_path);
		free(temporary_path);
		return -1;
	}
	if (stream_chunk_file_range(arc, fe, de,
	                            arc->compression, progress, user_data,
	                            raw_file_stream_writer_feed, &writer) < 0) {
		fclose(out);
		remove_output_path(temporary_path);
		free(temporary_path);
		return -1;
	}
	if (writer.written != (size_t) de->file_size) {
		INNO_LOG("streamed file size mismatch for %s: expected %llu got %zu",
		         fe->destination,
		         (unsigned long long) de->file_size,
		         writer.written);
		fclose(out);
		remove_output_path(temporary_path);
		free(temporary_path);
		return -1;
	}
	if (inno_checksum_matches(&writer.checksum, de, fe->destination) < 0) {
		fclose(out);
		remove_output_path(temporary_path);
		free(temporary_path);
		return -1;
	}
	if (fclose(out) != 0) {
		INNO_LOG("write error while closing %s: %s", output_path, strerror(errno));
		remove_output_path(temporary_path);
		free(temporary_path);
		return -1;
	}
	if (commit_temporary_output(temporary_path, output_path) < 0) {
		remove_output_path(temporary_path);
		free(temporary_path);
		return -1;
	}
	free(temporary_path);
	if (written_out)
		*written_out = writer.written;
	return 0;
}

static int stream_chunk_file_range(const inno_archive_t *arc,
                                   const inno_file_entry_t *fe,
                                   const inno_data_entry_t *de,
                                   inno_compress_method_t method,
                                   inno_progress_fn progress,
                                   void *progress_data,
                                   inno_chunk_sink_fn sink,
                                   void *sink_data)
{
	uint64_t chunk_pos;
	uint64_t payload_pos;
	uint64_t range_start = de->file_offset;
	uint64_t range_end;
	uint8_t magic[4];
	uint64_t comp_size = de->chunk_compressed_size;
	inno_compress_method_t actual_method =
	    de->chunk_compressed ? method : INNO_COMPRESS_STORED;
	uint64_t outer_pos = 0;
	if (validate_chunk_file_range(arc, de, actual_method, &chunk_pos,
	                              &payload_pos, &range_end) < 0)
		return -1;
	if (comp_size > DXX_EXTRACT_MAX_ENTRY_BYTES ||
	    range_end > DXX_EXTRACT_MAX_ENTRY_BYTES ||
	    !dxx_extract_ratio_allowed(range_end, comp_size) ||
	    (actual_method == INNO_COMPRESS_STORED && range_end > comp_size))
		return -1;

	if (read_at(arc->fd, chunk_pos, magic, 4) < 0) return -1;
	if (magic[0] != 'z' || magic[1] != 'l' || magic[2] != 'b' || magic[3] != 0x1A) {
		INNO_LOG("missing zlb magic at 0x%llx", (unsigned long long) chunk_pos);
		return -1;
	}

	if (actual_method == INNO_COMPRESS_STORED) {
		uint8_t in_buf[INNO_STREAM_BUFFER_SIZE];
		uint64_t copy_start = payload_pos + range_start;
		uint64_t remaining = de->file_size;
		uint64_t copied = range_start;
		while (remaining > 0) {
			size_t want = sizeof(in_buf);
			if ((uint64_t) want > remaining)
				want = (size_t) remaining;
			if (read_at(arc->fd, copy_start, in_buf, want) < 0)
				return -1;
			if (emit_chunk_range(in_buf, want, &copied,
			                     range_start, range_end, sink, sink_data) < 0)
				return -1;
			copy_start += want;
			remaining -= want;
			if (progress && fe->destination[0]) {
				long long done = (long long) copied;
				if (done > (long long) comp_size)
					done = (long long) comp_size;
				progress(fe->destination, done,
				         (long long) comp_size, progress_data);
			}
		}
		return 0;
	}

	if (actual_method == INNO_COMPRESS_ZLIB) {
		inno_stream_input_t input;
		uint8_t out_buf[INNO_STREAM_BUFFER_SIZE];
		z_stream zs;
		int zret;
		size_t last_progress_in = 0;
		memset(&zs, 0, sizeof(zs));
		init_stream_input(&input, arc->fd, payload_pos, comp_size, 0);
		if (inflateInit(&zs) != Z_OK) {
			return -1;
		}
		do {
			if (zs.avail_in == 0) {
				int fill = refill_stream_input(&input);
				if (fill < 0) {
					inflateEnd(&zs);
					return -1;
				}
				if (fill == 0) break;
				zs.next_in = input.buf;
				zs.avail_in = (uInt) input.len;
			}
			unsigned long prev_in = zs.total_in;
			unsigned long prev_out = zs.total_out;
			uInt prev_avail_in = zs.avail_in;
			zs.next_out = out_buf;
			zs.avail_out = (uInt) sizeof(out_buf);
			zret = inflate(&zs, Z_NO_FLUSH);
			if (zret != Z_OK && zret != Z_STREAM_END) {
				inflateEnd(&zs);
				return -1;
			}
			input.pos += (size_t) (prev_avail_in - zs.avail_in);
			input.total_in += (uint64_t) (prev_avail_in - zs.avail_in);
			size_t produced = sizeof(out_buf) - zs.avail_out;
			if (produced > 0 &&
			    emit_chunk_range(out_buf, produced, &outer_pos,
			                     range_start, range_end, sink, sink_data) < 0) {
				inflateEnd(&zs);
				return -1;
			}
			if (progress && fe->destination[0] &&
			    (size_t) input.total_in - last_progress_in >= 1048576) {
				progress(fe->destination, (long long) input.total_in,
				         (long long) comp_size, progress_data);
				last_progress_in = (size_t) input.total_in;
			}
			if (outer_pos >= range_end) break;
			if (zs.total_in == prev_in && zs.total_out == prev_out) {
				inflateEnd(&zs);
				return -1;
			}
		} while (zret != Z_STREAM_END);
		inflateEnd(&zs);
		if (outer_pos < range_end) {
			INNO_LOG("zlib chunk too small for %s: need %llu bytes, have %llu",
			         fe->destination,
			         (unsigned long long) range_end,
			         (unsigned long long) outer_pos);
			return -1;
		}
		return 0;
	}

	if (actual_method == INNO_COMPRESS_LZMA1) {
		inno_stream_input_t input;
		uint8_t lzma_props[5];
		if (comp_size < 5) {
			return -1;
		}
		if (read_at(arc->fd, payload_pos, lzma_props, 5) < 0)
			return -1;
		uint8_t out_buf[INNO_STREAM_BUFFER_SIZE];
		CLzmaDec dec;
		size_t last_progress_pos = 0;
		init_stream_input(&input, arc->fd, payload_pos + 5, comp_size - 5, 5);
		LzmaDec_Construct(&dec);
		if (LzmaDec_Allocate(&dec, lzma_props, 5, &g_lzma_alloc) != SZ_OK) {
			return -1;
		}
		LzmaDec_Init(&dec);
		while (input.total_in < comp_size) {
			if (input.pos == input.len) {
				int fill = refill_stream_input(&input);
				if (fill < 0) {
					LzmaDec_Free(&dec, &g_lzma_alloc);
					return -1;
				}
				if (fill == 0) break;
			}
			size_t dest_len = sizeof(out_buf);
			size_t src_len = input.len - input.pos;
			ELzmaStatus status;
			if (progress && fe->destination[0] &&
			    (size_t) input.total_in - last_progress_pos >= 1048576) {
				progress(fe->destination, (long long) input.total_in,
				         (long long) comp_size, progress_data);
				last_progress_pos = (size_t) input.total_in;
			}
			SRes res = LzmaDec_DecodeToBuf(&dec, out_buf, &dest_len,
			                               input.buf + input.pos, &src_len,
			                               LZMA_FINISH_ANY, &status);
			input.pos += src_len;
			input.total_in += src_len;
			if (dest_len > 0 &&
			    emit_chunk_range(out_buf, dest_len, &outer_pos,
			                     range_start, range_end, sink, sink_data) < 0) {
				LzmaDec_Free(&dec, &g_lzma_alloc);
				return -1;
			}
			if (outer_pos >= range_end) break;
			if (res != SZ_OK) {
				LzmaDec_Free(&dec, &g_lzma_alloc);
				return -1;
			}
			if (status == LZMA_STATUS_FINISHED_WITH_MARK ||
			    status == LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK)
				break;
			if (dest_len == 0 && src_len == 0) {
				LzmaDec_Free(&dec, &g_lzma_alloc);
				return -1;
			}
		}
		LzmaDec_Free(&dec, &g_lzma_alloc);
		if (outer_pos < range_end) {
			INNO_LOG("LZMA chunk too small for %s: need %llu bytes, have %llu",
			         fe->destination,
			         (unsigned long long) range_end,
			         (unsigned long long) outer_pos);
			return -1;
		}
		return 0;
	}

	if (actual_method == INNO_COMPRESS_LZMA2) {
		inno_stream_input_t input;
		uint8_t lzma2_prop;
		if (comp_size < 1) {
			return -1;
		}
		if (read_at(arc->fd, payload_pos, &lzma2_prop, 1) < 0)
			return -1;
		uint8_t out_buf[INNO_STREAM_BUFFER_SIZE];
		CLzma2Dec dec;
		size_t last_progress_pos2 = 0;
		init_stream_input(&input, arc->fd, payload_pos + 1, comp_size - 1, 1);
		Lzma2Dec_Construct(&dec);
		if (Lzma2Dec_Allocate(&dec, lzma2_prop, &g_lzma_alloc) != SZ_OK) {
			return -1;
		}
		Lzma2Dec_Init(&dec);
		while (input.total_in < comp_size) {
			if (input.pos == input.len) {
				int fill = refill_stream_input(&input);
				if (fill < 0) {
					Lzma2Dec_Free(&dec, &g_lzma_alloc);
					return -1;
				}
				if (fill == 0) break;
			}
			size_t dest_len = sizeof(out_buf);
			size_t src_len = input.len - input.pos;
			ELzmaStatus status;
			if (progress && fe->destination[0] &&
			    (size_t) input.total_in - last_progress_pos2 >= 1048576) {
				progress(fe->destination, (long long) input.total_in,
				         (long long) comp_size, progress_data);
				last_progress_pos2 = (size_t) input.total_in;
			}
			SRes res = Lzma2Dec_DecodeToBuf(&dec, out_buf, &dest_len,
			                                input.buf + input.pos, &src_len,
			                                LZMA_FINISH_ANY, &status);
			input.pos += src_len;
			input.total_in += src_len;
			if (dest_len > 0 &&
			    emit_chunk_range(out_buf, dest_len, &outer_pos,
			                     range_start, range_end, sink, sink_data) < 0) {
				Lzma2Dec_Free(&dec, &g_lzma_alloc);
				return -1;
			}
			if (outer_pos >= range_end) break;
			if (res != SZ_OK) {
				Lzma2Dec_Free(&dec, &g_lzma_alloc);
				return -1;
			}
			if (status == LZMA_STATUS_FINISHED_WITH_MARK ||
			    status == LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK)
				break;
			if (dest_len == 0 && src_len == 0) {
				Lzma2Dec_Free(&dec, &g_lzma_alloc);
				return -1;
			}
		}
		Lzma2Dec_Free(&dec, &g_lzma_alloc);
		if (outer_pos < range_end) {
			INNO_LOG("LZMA2 chunk too small for %s: need %llu bytes, have %llu",
			         fe->destination,
			         (unsigned long long) range_end,
			         (unsigned long long) outer_pos);
			return -1;
		}
		return 0;
	}

	INNO_LOG("unknown compression method %d", actual_method);
	return -1;
}

static int extract_gog_galaxy_file_streamed(inno_archive_t *arc,
                                            const inno_file_entry_t *fe,
                                            const inno_data_entry_t *de,
                                            const char *output_path,
                                            inno_progress_fn progress,
                                            void *user_data,
                                            size_t *written_out)
{
	gog_galaxy_stream_writer_t writer;
	char *temporary_path = NULL;
	FILE *out = open_temporary_output(output_path, &temporary_path);
	if (!out) {
		return -1;
	}
	if (gog_galaxy_stream_writer_init(&writer, fe, de, out, progress, user_data) < 0) {
		fclose(out);
		remove_output_path(temporary_path);
		free(temporary_path);
		return -1;
	}
	if (stream_chunk_file_range(arc, fe, de,
	                            arc->compression, progress, user_data,
	                            gog_galaxy_stream_writer_feed, &writer) < 0) {
		gog_galaxy_stream_writer_finish(&writer, NULL);
		fclose(out);
		remove_output_path(temporary_path);
		free(temporary_path);
		return -1;
	}
	if (gog_galaxy_stream_writer_finish(&writer, written_out) < 0) {
		fclose(out);
		remove_output_path(temporary_path);
		free(temporary_path);
		return -1;
	}
	if (inno_checksum_matches(&writer.checksum, de, fe->destination) < 0) {
		fclose(out);
		remove_output_path(temporary_path);
		free(temporary_path);
		return -1;
	}
	if (fclose(out) != 0) {
		INNO_LOG("write error while closing %s: %s", output_path, strerror(errno));
		remove_output_path(temporary_path);
		free(temporary_path);
		return -1;
	}
	if (commit_temporary_output(temporary_path, output_path) < 0) {
		remove_output_path(temporary_path);
		free(temporary_path);
		return -1;
	}
	free(temporary_path);
	return 0;
}

static int measure_gog_galaxy_file_streamed(inno_archive_t *arc,
                                            const inno_file_entry_t *fe,
                                            const inno_data_entry_t *de,
                                            size_t *written_out)
{
	gog_galaxy_stream_writer_t writer;
	if (gog_galaxy_stream_writer_init(&writer, fe, de, NULL, NULL, NULL) < 0)
		return -1;
	if (stream_chunk_file_range(arc, fe, de, arc->compression, NULL, NULL,
	                            gog_galaxy_stream_writer_feed, &writer) < 0) {
		gog_galaxy_stream_writer_finish(&writer, NULL);
		return -1;
	}
	if (gog_galaxy_stream_writer_finish(&writer, written_out) < 0)
		return -1;
	return inno_checksum_matches(&writer.checksum, de, fe->destination);
}

const inno_data_entry_t *inno_file_data_entry(const inno_archive_t *arc,
                                              uint32_t file_index)
{
	const inno_file_entry_t *file;

	if (!arc || !arc->files || file_index >= arc->file_count)
		return NULL;
	file = &arc->files[file_index];
	if (!data_location_valid(arc, file->location))
		return NULL;
	return &arc->data_entries[file->location];
}

static const char *inno_basename(const char *path)
{
	const char *last = path;
	for (const char *p = path; *p; p++) {
		if (*p == '/' || *p == '\\')
			last = p + 1;
	}
	return last;
}

static int inno_output_name_equal(const char *first, const char *second)
{
	while (*first && *second) {
		unsigned char a = (unsigned char) *first++;
		unsigned char b = (unsigned char) *second++;
		if (a >= 'A' && a <= 'Z') a = (unsigned char) (a + ('a' - 'A'));
		if (b >= 'A' && b <= 'Z') b = (unsigned char) (b + ('a' - 'A'));
		if (a != b) return 0;
	}
	return *first == *second;
}

int inno_output_names_unique(const inno_archive_t *arc,
                             inno_path_select_fn select_path,
                             void *user_data)
{
	if (!arc || (arc->file_count > 0 && !arc->files))
		return 0;
	for (uint32_t i = 0; i < arc->file_count; i++) {
		const char *first = arc->files[i].destination;
		if (select_path && !select_path(first, user_data))
			continue;
		if (!valid_destination_path(first))
			return 0;
		for (uint32_t j = i + 1u; j < arc->file_count; j++) {
			const char *second = arc->files[j].destination;
			if (select_path && !select_path(second, user_data))
				continue;
			if (!valid_destination_path(second))
				return 0;
			if (inno_output_name_equal(inno_basename(first),
			                           inno_basename(second)))
				return 0;
		}
	}
	return 1;
}

int inno_extract_file(inno_archive_t *arc, int file_index,
                      const char *output_path,
                      inno_progress_fn progress, void *user_data)
{
	if (!arc || !arc->files || file_index < 0 ||
	    (uint32_t) file_index >= arc->file_count)
		return -1;
	if (!output_path) return -1;

	inno_file_entry_t *fe = &arc->files[file_index];
	if (fe->location == 0xFFFFFFFF) {
		INNO_LOG("file %d has no data (location=0xFFFFFFFF)", file_index);
		return -1;
	}
	const inno_data_entry_t *de =
	    inno_file_data_entry(arc, (uint32_t) file_index);
	if (!de) {
		INNO_LOG("file %d: location %u out of range (%u entries)",
		         file_index, fe->location, arc->data_entry_count);
		return -1;
	}
	if (de->chunk_encrypted) {
		INNO_LOG("encrypted chunks not supported (file %s)", fe->destination);
		return -1;
	}

	uint64_t output_size;
	uint64_t compressed_size;
	uint64_t extracted_after = arc->extracted_bytes;
	inno_compress_method_t actual_method =
	    de->chunk_compressed ? arc->compression : INNO_COMPRESS_STORED;
	if (de->checksum_type != INNO_CHECKSUM_MD5 &&
	    de->checksum_type != INNO_CHECKSUM_SHA1) {
		INNO_LOG("unsupported file checksum for %s", fe->destination);
		return -1;
	}
	if (validate_chunk_file_range(arc, de, actual_method, NULL, NULL, NULL) < 0) {
		INNO_LOG("invalid chunk range for %s", fe->destination);
		return -1;
	}

	if (de->call_instruction_optimized) {
		INNO_LOG("exe instruction filter not implemented (file %s)", fe->destination);
		return -1;
	}
	if (fe->gog_galaxy && fe->external_size == 0) {
		size_t measured_size = 0;
		if (measure_gog_galaxy_file_streamed(arc, fe, de, &measured_size) < 0)
			return -1;
		output_size = measured_size;
	} else {
		output_size = fe->gog_galaxy ? fe->external_size : de->file_size;
	}
	compressed_size = fe->gog_galaxy ? de->file_size : de->chunk_compressed_size;
	if (!dxx_extract_entry_allowed(output_size, compressed_size) ||
	    !dxx_extract_entry_allowed(de->file_size, de->chunk_compressed_size) ||
	    arc->extracted_files >= DXX_EXTRACT_MAX_ENTRIES ||
	    dxx_extract_add_bytes(&extracted_after, output_size,
	                          DXX_EXTRACT_MAX_TOTAL_BYTES) < 0 ||
	    !dxx_extract_has_free_space(output_path, output_size)) {
		INNO_LOG("file exceeds extraction budget: %s", fe->destination);
		return -1;
	}

	if (progress) {
		if (progress(fe->destination, 0,
		             (long long) de->chunk_compressed_size, user_data))
			return DXX_EXTRACT_CANCELLED;
	}

	if (!fe->gog_galaxy &&
	    (de->chunk_compressed_size >= INNO_STREAM_DIRECT_THRESHOLD ||
	     de->file_size >= INNO_STREAM_DIRECT_THRESHOLD)) {
		size_t written = 0;
		const int stream_result = extract_regular_file_streamed(
		    arc, fe, de, output_path, progress, user_data, &written);
		if (stream_result < 0) {
			return stream_result;
		}
		if (progress) {
			if (progress(fe->destination,
			             (long long) de->chunk_compressed_size,
			             (long long) de->chunk_compressed_size, user_data)) {
				remove_output_path(output_path);
				return DXX_EXTRACT_CANCELLED;
			}
		}
		arc->extracted_bytes = extracted_after;
		arc->extracted_files++;
		return 0;
	}

	if (fe->gog_galaxy) {
		size_t written = 0;
		const int stream_result = extract_gog_galaxy_file_streamed(
		    arc, fe, de, output_path, progress, user_data, &written);
		if (stream_result < 0) {
			return stream_result;
		}
		if (written != output_size) {
			INNO_LOG("GOG Galaxy measured size changed for %s: expected %llu got %zu",
			         fe->destination,
			         (unsigned long long) output_size,
			         written);
			remove_output_path(output_path);
			return -1;
		}
		if (progress) {
			if (progress(fe->destination,
			             (long long) de->chunk_compressed_size,
			             (long long) de->chunk_compressed_size, user_data)) {
				remove_output_path(output_path);
				return DXX_EXTRACT_CANCELLED;
			}
		}
		arc->extracted_bytes = extracted_after;
		arc->extracted_files++;
		return 0;
	}

	/* Decompress the chunk */
	size_t chunk_len = 0;
	uint8_t *chunk = decompress_chunk(arc, de, arc->compression, &chunk_len,
	                                  progress, user_data, fe->destination);
	if (!chunk) {
		INNO_LOG("failed to decompress chunk for %s", fe->destination);
		return -1;
	}

	/* Verify we have enough data */
	if (de->file_offset > chunk_len ||
	    de->file_size > chunk_len - (size_t) de->file_offset) {
		INNO_LOG("chunk too small for %s: need %llu+%llu, have %zu",
		         fe->destination,
		         (unsigned long long) de->file_offset,
		         (unsigned long long) de->file_size,
		         chunk_len);
		free(chunk);
		return -1;
	}

	/* Write to output file */
	uint8_t *file_data = chunk + de->file_offset;
	size_t file_len = (size_t) de->file_size;
	inno_checksum_ctx_t checksum;
	inno_checksum_init(&checksum, de->checksum_type);
	inno_checksum_update(&checksum, file_data, file_len);
	if (inno_checksum_matches(&checksum, de, fe->destination) < 0) {
		free(chunk);
		return -1;
	}

	char *temporary_path = NULL;
	FILE *out = open_temporary_output(output_path, &temporary_path);
	if (!out) {
		free(chunk);
		return -1;
	}

	size_t written = 0;
	size_t last_progress_written = 0;
	{
		while (written < file_len) {
			size_t write_chunk = file_len - written;
			if (write_chunk > 262144) write_chunk = 262144;
			size_t n = fwrite(file_data + written, 1, write_chunk, out);
			if (n == 0) break;
			written += n;
			if (progress && !de->chunk_compressed &&
			    written - last_progress_written >= 1048576) {
				long long done = (long long) written;
				if (done > (long long) de->chunk_compressed_size)
					done = (long long) de->chunk_compressed_size;
				if (progress(fe->destination, done,
				             (long long) de->chunk_compressed_size, user_data)) {
					fclose(out);
					remove_output_path(temporary_path);
					free(temporary_path);
					free(chunk);
					return DXX_EXTRACT_CANCELLED;
				}
				last_progress_written = written;
			}
		}
	}
	int close_failed = fclose(out) != 0;

	if (written != file_len || close_failed) {
		INNO_LOG("write error for %s", output_path);
		remove_output_path(temporary_path);
		free(temporary_path);
		free(chunk);
		return -1;
	}

	free(chunk);
	if (commit_temporary_output(temporary_path, output_path) < 0) {
		remove_output_path(temporary_path);
		free(temporary_path);
		return -1;
	}
	free(temporary_path);

	if (progress) {
		if (progress(fe->destination,
		             (long long) de->chunk_compressed_size,
		             (long long) de->chunk_compressed_size, user_data)) {
			remove_output_path(output_path);
			return DXX_EXTRACT_CANCELLED;
		}
	}
	arc->extracted_bytes = extracted_after;
	arc->extracted_files++;

	return 0;
}

void inno_close(inno_archive_t *arc)
{
	if (!arc) return;
	if (arc->fd >= 0) {
		CLOSE_FD(arc->fd);
		arc->fd = -1;
	}
	free(arc->data_entries);
	arc->data_entries = NULL;
	free(arc->files);
	arc->files = NULL;
	arc->file_count = 0;
	arc->data_entry_count = 0;
}
