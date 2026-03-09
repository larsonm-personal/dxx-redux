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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
  #include <io.h>
  #include <fcntl.h>
  #define OPEN_RB(path)  _open((path), _O_RDONLY | _O_BINARY)
  #define READ_FD(fd, buf, n)  _read((fd), (buf), (unsigned)(n))
  #define LSEEK(fd, off, w)    _lseeki64((fd), (off), (w))
  #define CLOSE_FD(fd)         _close(fd)
  #include <direct.h>
  #define MKDIR(d) _mkdir(d)
#else
  #include <unistd.h>
  #include <fcntl.h>
  #include <sys/stat.h>
  #define OPEN_RB(path)  open((path), O_RDONLY)
  #define READ_FD(fd, buf, n)  read((fd), (buf), (n))
  #define LSEEK(fd, off, w)    lseek((fd), (off), (w))
  #define CLOSE_FD(fd)         close(fd)
  #define MKDIR(d) mkdir((d), 0755)
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
static void *lzma_alloc(ISzAllocPtr p, size_t size) { (void)p; return malloc(size); }
static void  lzma_free(ISzAllocPtr p, void *addr) { (void)p; free(addr); }
static const ISzAlloc g_lzma_alloc = { lzma_alloc, lzma_free };

/* ── CRC32 (IEEE 802.3, same as zlib/InnoSetup) ─────────────────── */
static uint32_t inno_crc32(const uint8_t *data, size_t len) {
    return (uint32_t)crc32(crc32(0, Z_NULL, 0), data, (uInt)len);
}

/* ── Helpers: read from fd at a specific offset ──────────────────── */
static int read_at(int fd, uint64_t offset, void *buf, size_t len) {
    if (LSEEK(fd, (long long)offset, SEEK_SET) < 0) return -1;
    size_t total = 0;
    while (total < len) {
        int n = READ_FD(fd, (uint8_t *)buf + total, len - total);
        if (n <= 0) return -1;
        total += (size_t)n;
    }
    return 0;
}

static uint16_t get_u16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t get_u32(const uint8_t *p) { return p[0] | (p[1]<<8) | (p[2]<<16) | ((uint32_t)p[3]<<24); }
static uint64_t get_u64(const uint8_t *p) { return (uint64_t)get_u32(p) | ((uint64_t)get_u32(p+4) << 32); }

/* ── InnoSetup version constants ─────────────────────────────────── */
#define INNO_VER(a,b,c) (((a)*10000) + ((b)*100) + (c))

/* ── PE Resource reader (find RT_RCDATA / name 11111) ────────────── */

static int find_pe_resource_11111(int fd, uint64_t *out_offset) {
    uint8_t hdr[4096];
    if (read_at(fd, 0, hdr, sizeof(hdr)) < 0) return -1;

    /* Check MZ signature */
    if (hdr[0] != 'M' || hdr[1] != 'Z') return -1;
    uint32_t pe_off = get_u32(hdr + 0x3C);
    if (pe_off + 24 > sizeof(hdr)) return -1;

    uint8_t pe_hdr[512];
    if (read_at(fd, pe_off, pe_hdr, sizeof(pe_hdr)) < 0) return -1;
    if (pe_hdr[0]!='P' || pe_hdr[1]!='E' || pe_hdr[2]!=0 || pe_hdr[3]!=0) return -1;

    uint16_t num_sections = get_u16(pe_hdr + 6);
    uint16_t opt_hdr_size = get_u16(pe_hdr + 20);

    /* Optional header: find resource directory RVA */
    uint8_t *opt = pe_hdr + 24;
    uint16_t opt_magic = get_u16(opt);
    int rva_entries_offset;
    if (opt_magic == 0x10b)      rva_entries_offset = 96;  /* PE32 */
    else if (opt_magic == 0x20b) rva_entries_offset = 112; /* PE32+ */
    else return -1;

    if ((int)opt_hdr_size < rva_entries_offset + 24) return -1;
    uint32_t num_rva = get_u32(opt + rva_entries_offset - 4);
    if (num_rva < 3) return -1;
    /* Resource directory is entry index 2 */
    uint32_t rsrc_rva  = get_u32(opt + rva_entries_offset + 8 * 2);
    uint32_t rsrc_size = get_u32(opt + rva_entries_offset + 8 * 2 + 4);
    if (rsrc_rva == 0 || rsrc_size == 0) return -1;

    /* Find the .rsrc section to map RVA → file offset */
    uint8_t *sec_table = pe_hdr + 24 + opt_hdr_size;
    uint32_t rsrc_file_offset = 0;
    for (int i = 0; i < num_sections && i < 16; i++) {
        /* Read section header from file if needed */
        uint8_t sec[40];
        uint64_t sec_pos = pe_off + 24 + opt_hdr_size + (uint64_t)i * 40;
        if (read_at(fd, sec_pos, sec, 40) < 0) continue;
        uint32_t sec_rva  = get_u32(sec + 12);
        uint32_t sec_vsize = get_u32(sec + 8);
        uint32_t sec_rawoff = get_u32(sec + 20);
        if (rsrc_rva >= sec_rva && rsrc_rva < sec_rva + sec_vsize) {
            rsrc_file_offset = sec_rawoff + (rsrc_rva - sec_rva);
            break;
        }
    }
    if (rsrc_file_offset == 0) return -1;

    /* Read the resource directory tree (3 levels: type → name → language) */
    uint8_t *rsrc = (uint8_t *)malloc(rsrc_size);
    if (!rsrc) return -1;
    if (read_at(fd, rsrc_file_offset, rsrc, rsrc_size) < 0) { free(rsrc); return -1; }

    /* Level 1: find RT_RCDATA (type 10) */
    uint16_t num_named = get_u16(rsrc + 12);
    uint16_t num_id    = get_u16(rsrc + 14);
    int found = -1;
    for (int i = 0; i < num_named + num_id; i++) {
        uint8_t *entry = rsrc + 16 + i * 8;
        uint32_t name_or_id = get_u32(entry);
        uint32_t offset_or_dir = get_u32(entry + 4);
        if (name_or_id == 10 && (offset_or_dir & 0x80000000)) {
            /* RT_RCDATA directory found */
            uint32_t dir2_off = offset_or_dir & 0x7FFFFFFF;
            if (dir2_off + 16 > rsrc_size) { free(rsrc); return -1; }

            /* Level 2: find name 11111 (0x2B67) */
            uint16_t n2_named = get_u16(rsrc + dir2_off + 12);
            uint16_t n2_id    = get_u16(rsrc + dir2_off + 14);
            for (int j = 0; j < n2_named + n2_id; j++) {
                uint8_t *e2 = rsrc + dir2_off + 16 + j * 8;
                uint32_t nid2 = get_u32(e2);
                uint32_t off2 = get_u32(e2 + 4);
                if (nid2 == 11111 && (off2 & 0x80000000)) {
                    /* Level 3: take first language entry */
                    uint32_t dir3_off = off2 & 0x7FFFFFFF;
                    if (dir3_off + 20 > rsrc_size) { free(rsrc); return -1; }
                    uint16_t n3_named = get_u16(rsrc + dir3_off + 12);
                    uint16_t n3_id    = get_u16(rsrc + dir3_off + 14);
                    if (n3_named + n3_id < 1) { free(rsrc); return -1; }
                    uint8_t *e3 = rsrc + dir3_off + 16;
                    uint32_t off3 = get_u32(e3 + 4);
                    if (off3 & 0x80000000) { free(rsrc); return -1; } /* expect leaf */
                    /* Leaf: 4 bytes RVA of data, 4 bytes size */
                    if (off3 + 8 > rsrc_size) { free(rsrc); return -1; }
                    uint32_t data_rva = get_u32(rsrc + off3);
                    /* Convert RVA to file offset */
                    *out_offset = rsrc_file_offset + (data_rva - rsrc_rva);
                    free(rsrc);
                    return 0;
                }
            }
        }
    }
    free(rsrc);
    return -1;
}

/* ── Offset table parsing ────────────────────────────────────────── */

/* Known loader magic IDs (12 bytes each) */
static const uint8_t loader_magic_1[] = {
    'r','D','l','P','t','S', 0xcd, 0xe6, 0xd7, 0x7b, 0x0b, 0x2a
};
static const uint8_t loader_magic_2[] = {
    'n','S','5','W','7','d','T', 0x83, 0xaa, 0x1b, 0x0f, 0x6a
};
/* Older IDs (for broader compatibility) */
static const uint8_t loader_magic_07[] = {
    'r','D','l','P','t','S','0','7', 0x87, 'e', 'V', 'x'
};
static const uint8_t loader_magic_06[] = {
    'r','D','l','P','t','S','0','6', 0x87, 'e', 'V', 'x'
};
static const uint8_t loader_magic_05[] = {
    'r','D','l','P','t','S','0','5', 0x87, 'e', 'V', 'x'
};

/* Loader version enum */
typedef enum {
    LOADER_VER_UNKNOWN = 0,
    LOADER_VER_4_0_3,    /* magic_05 */
    LOADER_VER_4_0_10,   /* magic_06 */
    LOADER_VER_4_1_6,    /* magic_07 */
    LOADER_VER_5_1_5,    /* magic_1 or magic_2 */
} loader_version_t;

static loader_version_t identify_loader(const uint8_t magic[12]) {
    if (!memcmp(magic, loader_magic_1, 12)) return LOADER_VER_5_1_5;
    if (!memcmp(magic, loader_magic_2, 12)) return LOADER_VER_5_1_5;
    if (!memcmp(magic, loader_magic_07, 12)) return LOADER_VER_4_1_6;
    if (!memcmp(magic, loader_magic_06, 12)) return LOADER_VER_4_0_10;
    if (!memcmp(magic, loader_magic_05, 12)) return LOADER_VER_4_0_3;
    return LOADER_VER_UNKNOWN;
}

static int parse_offset_table(int fd, uint64_t table_offset,
                               uint64_t *header_offset, uint64_t *data_offset) {
    uint8_t buf[64];
    if (read_at(fd, table_offset, buf, 64) < 0) {
        INNO_LOG("cannot read offset table");
        return -1;
    }

    loader_version_t lver = identify_loader(buf);
    if (lver == LOADER_VER_UNKNOWN) {
        INNO_LOG("unknown loader magic at 0x%llx", (unsigned long long)table_offset);
        return -1;
    }

    uint8_t *p = buf + 12; /* skip 12-byte magic */

    if (lver >= LOADER_VER_5_1_5) {
        uint32_t revision = get_u32(p); p += 4;
        if (revision != 1) {
            INNO_LOG("unexpected loader revision %u", revision);
        }
    }

    /* uint32_t exe_total_size = */ get_u32(p); p += 4;
    /* uint32_t exe_offset = */ get_u32(p); p += 4;

    if (lver < LOADER_VER_4_1_6) {
        /* uint32_t exe_compressed_size = */ get_u32(p); p += 4;
    }

    /* uint32_t exe_uncompressed_size = */ get_u32(p); p += 4;
    /* uint32_t exe_checksum = */ get_u32(p); p += 4;

    *header_offset = get_u32(p); p += 4;
    *data_offset   = get_u32(p); p += 4;

    /* CRC32 validation for >= 4.0.10 */
    if (lver >= LOADER_VER_4_0_10) {
        uint32_t expected_crc = get_u32(p);
        size_t crc_len = (size_t)(p - buf);
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
                                         size_t *bytes_consumed) {
    /* Read the 9-byte block header */
    uint8_t hdr[9];
    if (read_at(fd, offset, hdr, 9) < 0) {
        INNO_LOG("cannot read block header at 0x%llx", (unsigned long long)offset);
        return NULL;
    }

    uint32_t hdr_crc = get_u32(hdr);
    uint32_t stored_size = get_u32(hdr + 4);
    uint8_t  compressed = hdr[8];

    /* Validate header CRC (covers stored_size + compressed = 5 bytes) */
    uint32_t check_crc = inno_crc32(hdr + 4, 5);
    if (check_crc != hdr_crc) {
        INNO_LOG("block header CRC mismatch at 0x%llx", (unsigned long long)offset);
        return NULL;
    }

    *bytes_consumed = 9 + stored_size;

    /* Read all stored data */
    uint8_t *stored = (uint8_t *)malloc(stored_size);
    if (!stored) return NULL;
    if (read_at(fd, offset + 9, stored, stored_size) < 0) {
        INNO_LOG("cannot read %u bytes of block data", stored_size);
        free(stored);
        return NULL;
    }

    /* Strip CRC32 prefixes from 4096-byte sub-blocks.
     * Format: [CRC32 4 bytes][data up to 4096 bytes]... */
    size_t raw_cap = stored_size; /* upper bound (raw < stored) */
    uint8_t *raw = (uint8_t *)malloc(raw_cap);
    if (!raw) { free(stored); return NULL; }
    size_t raw_len = 0;
    size_t pos = 0;
    while (pos < stored_size) {
        if (pos + 4 > stored_size) {
            INNO_LOG("truncated sub-block CRC at pos %zu", pos);
            free(raw); free(stored);
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
            free(raw); free(stored);
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

    uint8_t *lzma_data = raw + 5;
    size_t lzma_data_len = raw_len - 5;

    /* Start with 4x compressed size estimate, grow if needed */
    size_t decomp_cap = lzma_data_len * 4;
    if (decomp_cap < 65536) decomp_cap = 65536;
    uint8_t *decomp = (uint8_t *)malloc(decomp_cap);
    if (!decomp) { free(raw); return NULL; }

    CLzmaDec dec;
    LzmaDec_Construct(&dec);
    SRes res = LzmaDec_Allocate(&dec, lzma_props, 5, &g_lzma_alloc);
    if (res != SZ_OK) {
        INNO_LOG("LZMA alloc failed: %d", res);
        free(decomp); free(raw);
        return NULL;
    }
    LzmaDec_Init(&dec);

    size_t decomp_len = 0;
    size_t src_pos = 0;
    ELzmaStatus status;

    while (src_pos < lzma_data_len) {
        /* Grow output buffer if needed */
        if (decomp_len >= decomp_cap) {
            decomp_cap *= 2;
            uint8_t *tmp = (uint8_t *)realloc(decomp, decomp_cap);
            if (!tmp) {
                LzmaDec_Free(&dec, &g_lzma_alloc);
                free(decomp); free(raw);
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
            free(decomp); free(raw);
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

    *out_len = decomp_len;
    return decomp;
}

/* ── Version string parser ───────────────────────────────────────── */

static int parse_version_string(const uint8_t id[64], inno_version_t *ver) {
    memset(ver, 0, sizeof(*ver));

    /* Expected: "Inno Setup Setup Data (X.Y.Z) (u)" */
    const char *s = (const char *)id;
    const char *prefix = "Inno Setup Setup Data (";
    if (strncmp(s, prefix, strlen(prefix)) != 0) {
        INNO_LOG("invalid version ID: %.64s", s);
        return -1;
    }
    s += strlen(prefix);
    ver->major = (int)strtol(s, (char **)&s, 10);
    if (*s++ != '.') return -1;
    ver->minor = (int)strtol(s, (char **)&s, 10);
    if (*s == '.') { s++; ver->patch = (int)strtol(s, (char **)&s, 10); }
    if (*s++ != ')') return -1;
    /* Look for (u) or (U) suffix */
    const char *u = strstr(s, "(u)");
    const char *U = strstr(s, "(U)");
    ver->unicode = (u || U) ? 1 : 0;

    return 0;
}

/* ── Header stream parser ────────────────────────────────────────── */

/* Read a length-prefixed string from a buffer.
 * For Unicode builds, converts UTF-16LE to ASCII (only low bytes).
 * Advances *pos by the consumed bytes. */
static int read_string(const uint8_t *buf, size_t buf_len, size_t *pos,
                       char *out, size_t out_size, int unicode) {
    if (*pos + 4 > buf_len) return -1;
    uint32_t len = get_u32(buf + *pos);
    *pos += 4;
    if (*pos + len > buf_len) return -1;

    if (out && out_size > 0) {
        if (unicode) {
            /* UTF-16LE → narrow (take low bytes, skip NUL chars) */
            size_t chars = len / 2;
            size_t j = 0;
            for (size_t i = 0; i < chars && j < out_size - 1; i++) {
                uint16_t ch = get_u16(buf + *pos + i * 2);
                if (ch > 0 && ch < 128) out[j++] = (char)ch;
                else if (ch >= 128) out[j++] = '?';
            }
            out[j] = '\0';
        } else {
            size_t n = len < out_size - 1 ? len : out_size - 1;
            memcpy(out, buf + *pos, n);
            out[n] = '\0';
        }
    }
    *pos += len;
    return 0;
}

/* Skip a string without storing it */
static int skip_string(const uint8_t *buf, size_t buf_len, size_t *pos) {
    return read_string(buf, buf_len, pos, NULL, 0, 0);
}

/* Skip N strings */
static int skip_strings(const uint8_t *buf, size_t buf_len, size_t *pos, int count) {
    for (int i = 0; i < count; i++) {
        if (skip_string(buf, buf_len, pos) < 0) return -1;
    }
    return 0;
}

/* Read N bytes, advancing pos */
static int skip_bytes(const uint8_t *buf, size_t buf_len, size_t *pos, size_t n) {
    if (*pos + n > buf_len) return -1;
    *pos += n;
    return 0;
}

/* ── Flag reader helper ──────────────────────────────────────────── */

/* Read a packed bitfield of 'count' flags (1 byte per 8 flags).
 * If count % 8 is in range [17,24] (exactly 3 bytes), reads 4 bytes. */
static int skip_flags(const uint8_t *buf, size_t buf_len, size_t *pos, int count) {
    int bytes = (count + 7) / 8;
    if (bytes == 3) bytes = 4; /* InnoSetup padding: 3 flag bytes → 4 */
    return skip_bytes(buf, buf_len, pos, bytes);
}

/* ── Main header parser (setup-0 block stream 1) ─────────────────── */

static int parse_header_stream1(const uint8_t *buf, size_t buf_len,
                                inno_version_t *ver, inno_archive_t *arc) {
    size_t pos = 0;
    int v = INNO_VER(ver->major, ver->minor, ver->patch);

    /* ── Main header strings ── */
    int num_header_strings;
    if (v >= INNO_VER(5,6,1)) num_header_strings = 34;
    else if (v >= INNO_VER(5,5,6)) num_header_strings = 32;
    else if (v >= INNO_VER(5,5,0)) num_header_strings = 31;
    else if (v >= INNO_VER(5,3,10)) num_header_strings = 30;
    else if (v >= INNO_VER(5,3,8)) num_header_strings = 29;
    else if (v >= INNO_VER(5,2,5)) num_header_strings = 28;
    else if (v >= INNO_VER(5,1,13)) num_header_strings = 25;
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
    uint32_t language_count   = get_u32(buf + pos); pos += 4;
    uint32_t message_count    = get_u32(buf + pos); pos += 4;
    uint32_t permission_count = get_u32(buf + pos); pos += 4;
    uint32_t type_count       = get_u32(buf + pos); pos += 4;
    uint32_t component_count  = get_u32(buf + pos); pos += 4;
    uint32_t task_count       = get_u32(buf + pos); pos += 4;
    uint32_t directory_count  = get_u32(buf + pos); pos += 4;
    uint32_t file_count       = get_u32(buf + pos); pos += 4;
    uint32_t data_entry_count = get_u32(buf + pos); pos += 4;
    uint32_t icon_count       = get_u32(buf + pos); pos += 4;
    uint32_t ini_entry_count  = get_u32(buf + pos); pos += 4;
    uint32_t registry_entry_count = get_u32(buf + pos); pos += 4;
    uint32_t delete_entry_count   = get_u32(buf + pos); pos += 4;
    uint32_t uninstall_delete_entry_count = get_u32(buf + pos); pos += 4;
    uint32_t run_entry_count  = get_u32(buf + pos); pos += 4;

    /* uninstall_run_entry_count (>= 2.0.0) */
    if (pos + 4 > buf_len) return -1;
    uint32_t uninstall_run_entry_count = get_u32(buf + pos); pos += 4;

    arc->data_entry_count = (int)data_entry_count;

    /* ── Windows version range (20 bytes) ── */
    if (skip_bytes(buf, buf_len, &pos, 20) < 0) return -1;

    /* ── Colors and misc ── */
    /* back_color, back_color2 */
    if (skip_bytes(buf, buf_len, &pos, 8) < 0) return -1;

    if (v < INNO_VER(5,5,7)) {
        /* image_back_color: present in < 5.5.7 */
        if (skip_bytes(buf, buf_len, &pos, 4) < 0) return -1;
    }

    if (v >= INNO_VER(5,5,7)) {
        /* image_alpha_format: uint8 enum, added in 5.5.7 */
        if (skip_bytes(buf, buf_len, &pos, 1) < 0) return -1;
    }

    /* password_sha1 (20) + password_salt (8) */
    if (skip_bytes(buf, buf_len, &pos, 28) < 0) return -1;

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
    arc->compression = (inno_compress_method_t)buf[pos];
    pos += 1;

    /* architectures_allowed + architectures_installed_64bit flags */
    int arch_flag_bits = (v >= INNO_VER(5,6,0)) ? 5 : 4;
    int arch_bytes = (arch_flag_bits + 7) / 8;
    /* Two sets of arch flags */
    if (skip_bytes(buf, buf_len, &pos, arch_bytes * 2) < 0) return -1;

    /* disable_dir_page (1) + disable_program_group_page (1) - for >= 5.3.3 */
    if (v >= INNO_VER(5,3,3)) {
        if (skip_bytes(buf, buf_len, &pos, 2) < 0) return -1;
    }

    /* uninstall_display_size (8) for >= 5.5.0, (4) for >= 5.3.6 */
    if (v >= INNO_VER(5,5,0)) {
        if (skip_bytes(buf, buf_len, &pos, 8) < 0) return -1;
    } else if (v >= INNO_VER(5,3,6)) {
        if (skip_bytes(buf, buf_len, &pos, 4) < 0) return -1;
    }

    /* Header flags bitfield (variable count) */
    int num_flags;
    if (v >= INNO_VER(5,5,7)) num_flags = 44;
    else if (v >= INNO_VER(5,5,0)) num_flags = 43;
    else if (v >= INNO_VER(5,3,3)) num_flags = 37;
    else num_flags = 35;
    if (skip_flags(buf, buf_len, &pos, num_flags) < 0) return -1;

    /* ── Now skip through entry arrays to reach file entries ── */

    /* Each language entry: 10 strings + scalars */
    for (uint32_t i = 0; i < language_count; i++) {
        int lang_strings = (v >= INNO_VER(4,2,1)) ? 10 : 8;
        if (skip_strings(buf, buf_len, &pos, lang_strings) < 0) return -1;
        /* language_id (4) */
        if (skip_bytes(buf, buf_len, &pos, 4) < 0) return -1;
        /* codepage: NOT read for unicode >= 5.3.0 */
        if (!(ver->unicode && v >= INNO_VER(5,3,0))) {
            if (v >= INNO_VER(4,2,1)) {
                if (skip_bytes(buf, buf_len, &pos, 4) < 0) return -1;
            }
        }
        /* dialog_font_size(4) + title_font_size(4) + welcome_font_size(4) + copyright_font_size(4) */
        if (skip_bytes(buf, buf_len, &pos, 16) < 0) return -1;
        /* right_to_left (1 byte bool, >= 5.2.3) */
        if (v >= INNO_VER(5,2,3)) {
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
        int type_strings = (v >= INNO_VER(4,0,1)) ? 4 : 3;
        if (skip_strings(buf, buf_len, &pos, type_strings) < 0) return -1;
        /* winver (20) */
        if (skip_bytes(buf, buf_len, &pos, 20) < 0) return -1;
        /* options: stored_flags (1 flag) → 1 byte */
        if (skip_bytes(buf, buf_len, &pos, 1) < 0) return -1;
        /* type: stored_enum (>= 4.0.3) → 1 byte */
        if (v >= INNO_VER(4,0,3)) {
            if (skip_bytes(buf, buf_len, &pos, 1) < 0) return -1;
        }
        /* size: uint64 (>= 4.0.0) */
        if (v >= INNO_VER(4,0,0)) {
            if (skip_bytes(buf, buf_len, &pos, 8) < 0) return -1;
        }
    }
    /* Skip components */
    for (uint32_t i = 0; i < component_count; i++) {
        int comp_strings = (v >= INNO_VER(4,0,1)) ? 5 : 4;
        if (skip_strings(buf, buf_len, &pos, comp_strings) < 0) return -1;
        /* extra_disk_space_required (8, >= 4.0.0) */
        if (v >= INNO_VER(4,0,0)) {
            if (skip_bytes(buf, buf_len, &pos, 8) < 0) return -1;
        }
        /* level (4, >= 4.0.0) */
        if (v >= INNO_VER(4,0,0)) {
            if (skip_bytes(buf, buf_len, &pos, 4) < 0) return -1;
        }
        /* used (1 byte bool, >= 4.0.0) */
        if (v >= INNO_VER(4,0,0)) {
            if (skip_bytes(buf, buf_len, &pos, 1) < 0) return -1;
        }
        /* winver (20) */
        if (skip_bytes(buf, buf_len, &pos, 20) < 0) return -1;
        /* options: stored_flags (5 flags for >= 4.2.3, 2 for older) */
        int comp_flags = (v >= INNO_VER(4,2,3)) ? 5 : 2;
        if (skip_flags(buf, buf_len, &pos, comp_flags) < 0) return -1;
        /* size (8, >= 4.0.0) */
        if (v >= INNO_VER(4,0,0)) {
            if (skip_bytes(buf, buf_len, &pos, 8) < 0) return -1;
        }
    }
    /* Skip tasks */
    for (uint32_t i = 0; i < task_count; i++) {
        int task_strings = (v >= INNO_VER(4,0,1)) ? 6 : 5;
        if (skip_strings(buf, buf_len, &pos, task_strings) < 0) return -1;
        /* level (4, >= 4.0.0) */
        if (v >= INNO_VER(4,0,0)) {
            if (skip_bytes(buf, buf_len, &pos, 4) < 0) return -1;
        }
        /* used (1 byte bool, >= 4.0.0) */
        if (v >= INNO_VER(4,0,0)) {
            if (skip_bytes(buf, buf_len, &pos, 1) < 0) return -1;
        }
        /* winver (20) */
        if (skip_bytes(buf, buf_len, &pos, 20) < 0) return -1;
        /* options: stored_flags (5 flags for >= 4.2.3) */
        int task_flags = (v >= INNO_VER(4,2,3)) ? 5 : 3;
        if (skip_flags(buf, buf_len, &pos, task_flags) < 0) return -1;
    }
    /* Skip directories */
    for (uint32_t i = 0; i < directory_count; i++) {
        int dir_strings = (v >= INNO_VER(4,1,0)) ? 7 : 5;
        if (skip_strings(buf, buf_len, &pos, dir_strings) < 0) return -1;
        /* attributes (4, >= 2.0.11) */
        if (v >= INNO_VER(2,0,11)) {
            if (skip_bytes(buf, buf_len, &pos, 4) < 0) return -1;
        }
        /* winver (20) */
        if (skip_bytes(buf, buf_len, &pos, 20) < 0) return -1;
        /* permission (int16, >= 4.1.0) */
        if (v >= INNO_VER(4,1,0)) {
            if (skip_bytes(buf, buf_len, &pos, 2) < 0) return -1;
        }
        /* options: stored_flags (5 flags for >= 5.2.0, 3 for older) */
        int dir_flags = (v >= INNO_VER(5,2,0)) ? 5 : 3;
        if (skip_flags(buf, buf_len, &pos, dir_flags) < 0) return -1;
    }
    /* ── Parse file entries (the ones we care about!) ── */
    if (file_count > INNO_MAX_FILES) {
        INNO_LOG("too many files: %u (max %d)", file_count, INNO_MAX_FILES);
        file_count = INNO_MAX_FILES;
    }
    arc->file_count = (int)file_count;

    for (uint32_t i = 0; i < file_count; i++) {
        inno_file_entry_t *fe = &arc->files[i];
        /* source (encoded_string) */
        if (skip_string(buf, buf_len, &pos) < 0) return -1;
        /* destination (encoded_string) */
        if (read_string(buf, buf_len, &pos, fe->destination,
                        sizeof(fe->destination), ver->unicode) < 0) return -1;
        /* install_font_name */
        if (skip_string(buf, buf_len, &pos) < 0) return -1;
        /* strong_assembly_name (>= 5.2.5) */
        if (v >= INNO_VER(5,2,5)) {
            if (skip_string(buf, buf_len, &pos) < 0) return -1;
        }

        /* Condition data (base item) */
        /* components, tasks (>= 2.0.0) */
        if (skip_strings(buf, buf_len, &pos, 2) < 0) return -1;
        /* languages (>= 4.0.1) */
        if (v >= INNO_VER(4,0,1)) { if (skip_string(buf, buf_len, &pos) < 0) return -1; }
        /* check (>= 4.0.0) */
        if (v >= INNO_VER(4,0,0)) { if (skip_string(buf, buf_len, &pos) < 0) return -1; }
        /* after_install (>= 4.1.0) */
        if (v >= INNO_VER(4,1,0)) {
            if (skip_string(buf, buf_len, &pos) < 0) return -1;
        }
        /* before_install (>= 4.1.0) — GOG Galaxy stores real filename here */
        if (v >= INNO_VER(4,1,0)) {
            char before_install[512];
            if (read_string(buf, buf_len, &pos, before_install,
                            sizeof(before_install), ver->unicode) < 0) return -1;
            /* GOG Galaxy pattern: before_install('hash', 'real/path', 'N')
             * Extract arg 2 (the real path) to overwrite the hash destination */
            if (before_install[0]) {
                fe->gog_galaxy = 1;
                /* Find second argument: skip first 'xxx', then find next 'xxx' */
                const char *p = strchr(before_install, '\'');
                if (p) p = strchr(p + 1, '\''); /* end of arg1 */
                if (p) p = strchr(p + 1, '\''); /* start of arg2 */
                if (p) {
                    p++; /* skip the opening quote */
                    const char *end = strchr(p, '\'');
                    if (end && end > p) {
                        size_t len = (size_t)(end - p);
                        if (len < sizeof(fe->destination)) {
                            memcpy(fe->destination, p, len);
                            fe->destination[len] = '\0';
                        }
                    }
                }
            }
        }

        /* Windows version range (20 bytes) */
        if (skip_bytes(buf, buf_len, &pos, 20) < 0) return -1;

        /* location (uint32) */
        if (pos + 4 > buf_len) return -1;
        fe->location = get_u32(buf + pos); pos += 4;

        /* attributes (uint32) */
        if (skip_bytes(buf, buf_len, &pos, 4) < 0) return -1;

        /* external_size (uint64, >= 4.0.0) */
        if (v >= INNO_VER(4,0,0)) {
            if (pos + 8 > buf_len) return -1;
            fe->external_size = get_u64(buf + pos); pos += 8;
        }

        /* permission (int16, >= 4.1.0) */
        if (v >= INNO_VER(4,1,0)) {
            if (skip_bytes(buf, buf_len, &pos, 2) < 0) return -1;
        }

        /* file flags (~32 for 5.5.x) */
        int file_flags = 32;  /* 4 bytes for 5.5.x/5.6.x */
        if (skip_flags(buf, buf_len, &pos, file_flags) < 0) return -1;

        /* file type enum (uint8) */
        if (skip_bytes(buf, buf_len, &pos, 1) < 0) return -1;
    }

    /* We don't need to parse entries AFTER file entries for extraction.
     * The remaining entries (icons, ini, registry, etc.) are in this
     * same block stream but we skip them all. */

    (void)icon_count;
    (void)ini_entry_count;
    (void)registry_entry_count;
    (void)delete_entry_count;
    (void)uninstall_delete_entry_count;
    (void)run_entry_count;
    (void)uninstall_run_entry_count;


    return 0;
}

/* ── Data entry parser (setup-0 block stream 2) ──────────────────── */

static int parse_data_entries(const uint8_t *buf, size_t buf_len,
                              inno_version_t *ver, inno_archive_t *arc) {
    size_t pos = 0;
    int v = INNO_VER(ver->major, ver->minor, ver->patch);

    for (int i = 0; i < arc->data_entry_count; i++) {
        inno_data_entry_t *de = &arc->data_entries[i];

        /* first_slice, last_slice */
        if (pos + 8 > buf_len) return -1;
        de->first_slice = get_u32(buf + pos); pos += 4;
        de->last_slice  = get_u32(buf + pos); pos += 4;

        /* chunk_offset (uint32 for < 4.0.0, really uint32 in our range) */
        if (pos + 4 > buf_len) return -1;
        de->chunk_offset = get_u32(buf + pos); pos += 4;

        /* file_offset (uint64 for >= 4.0.1) */
        if (v >= INNO_VER(4,0,1)) {
            if (pos + 8 > buf_len) return -1;
            de->file_offset = get_u64(buf + pos); pos += 8;
        }

        /* file_size (uint64 for >= 4.0.0) */
        if (pos + 8 > buf_len) return -1;
        de->file_size = get_u64(buf + pos); pos += 8;

        /* chunk_compressed_size (uint64) */
        if (pos + 8 > buf_len) return -1;
        de->chunk_compressed_size = get_u64(buf + pos); pos += 8;

        /* SHA-1 hash (20 bytes, >= 5.3.9) */
        if (v >= INNO_VER(5,3,9)) {
            if (pos + 20 > buf_len) return -1;
            memcpy(de->sha1, buf + pos, 20); pos += 20;
        }

        /* timestamp (int64) */
        if (skip_bytes(buf, buf_len, &pos, 8) < 0) return -1;

        /* file version (8 bytes) */
        if (skip_bytes(buf, buf_len, &pos, 8) < 0) return -1;

        /* data flags */
        int num_data_flags;
        if (v >= INNO_VER(5,5,7)) num_data_flags = 11;
        else if (v >= INNO_VER(5,1,13)) num_data_flags = 9;
        else if (v >= INNO_VER(4,2,5)) num_data_flags = 8;
        else if (v >= INNO_VER(4,2,2)) num_data_flags = 7;
        else if (v >= INNO_VER(4,2,0)) num_data_flags = 6;
        else if (v >= INNO_VER(4,1,8)) num_data_flags = 5;
        else if (v >= INNO_VER(4,1,0)) num_data_flags = 4;
        else if (v >= INNO_VER(4,0,10)) num_data_flags = 3;
        else num_data_flags = 2;

        /* Read the raw flag bytes */
        int flag_bytes = (num_data_flags + 7) / 8;
        if (flag_bytes == 3) flag_bytes = 4;
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
        de->chunk_compressed = 0;

        if (num_data_flags > 4) {
            de->call_instruction_optimized = (buf[pos + 0] >> 4) & 1; /* bit 4 */
        }
        if (num_data_flags > 7) {
            de->chunk_compressed = (buf[pos + 0] >> 7) & 1; /* bit 7 */
        }

        pos += flag_bytes;
    }
    return 0;
}

/* ── Chunk decompressor (for setup-1.bin data) ───────────────────── */

static uint8_t *decompress_chunk(int fd, uint64_t data_offset,
                                  const inno_data_entry_t *de,
                                  inno_compress_method_t method,
                                  size_t *out_len) {
    uint64_t chunk_pos = data_offset + de->chunk_offset;

    /* Read and verify zlb magic */
    uint8_t magic[4];
    if (read_at(fd, chunk_pos, magic, 4) < 0) return NULL;
    if (magic[0] != 'z' || magic[1] != 'l' || magic[2] != 'b' || magic[3] != 0x1A) {
        INNO_LOG("missing zlb magic at 0x%llx", (unsigned long long)chunk_pos);
        return NULL;
    }

    uint64_t comp_size = de->chunk_compressed_size; /* size after zlb magic */
    uint8_t *comp_data = (uint8_t *)malloc((size_t)comp_size);
    if (!comp_data) return NULL;
    if (read_at(fd, chunk_pos + 4, comp_data, (size_t)comp_size) < 0) {
        free(comp_data);
        return NULL;
    }

    inno_compress_method_t actual_method = de->chunk_compressed ? method : INNO_COMPRESS_STORED;

    if (actual_method == INNO_COMPRESS_STORED) {
        *out_len = (size_t)comp_size;
        return comp_data;
    }

    if (actual_method == INNO_COMPRESS_ZLIB) {
        /* Zlib decompression */
        size_t decomp_cap = (size_t)(de->file_offset + de->file_size) * 2;
        if (decomp_cap < 65536) decomp_cap = 65536;
        uint8_t *decomp = (uint8_t *)malloc(decomp_cap);
        if (!decomp) { free(comp_data); return NULL; }

        z_stream zs;
        memset(&zs, 0, sizeof(zs));
        if (inflateInit(&zs) != Z_OK) { free(decomp); free(comp_data); return NULL; }

        zs.next_in = comp_data;
        zs.avail_in = (uInt)comp_size;
        zs.next_out = decomp;
        zs.avail_out = (uInt)decomp_cap;

        int zret;
        size_t total_out = 0;
        while ((zret = inflate(&zs, Z_NO_FLUSH)) == Z_OK || zret == Z_BUF_ERROR) {
            total_out = zs.total_out;
            if (zs.avail_out == 0) {
                decomp_cap *= 2;
                uint8_t *tmp = (uint8_t *)realloc(decomp, decomp_cap);
                if (!tmp) { inflateEnd(&zs); free(decomp); free(comp_data); return NULL; }
                decomp = tmp;
                zs.next_out = decomp + total_out;
                zs.avail_out = (uInt)(decomp_cap - total_out);
            }
            if (zs.avail_in == 0) break;
        }
        if (zret == Z_STREAM_END) total_out = zs.total_out;
        inflateEnd(&zs);
        free(comp_data);

        *out_len = total_out;
        return decomp;
    }

    if (actual_method == INNO_COMPRESS_LZMA1) {
        if (comp_size < 5) { free(comp_data); return NULL; }
        uint8_t lzma_props[5];
        memcpy(lzma_props, comp_data, 5);

        size_t decomp_cap = (size_t)(de->file_offset + de->file_size) * 2;
        if (decomp_cap < 65536) decomp_cap = 65536;
        uint8_t *decomp = (uint8_t *)malloc(decomp_cap);
        if (!decomp) { free(comp_data); return NULL; }

        CLzmaDec dec;
        LzmaDec_Construct(&dec);
        if (LzmaDec_Allocate(&dec, lzma_props, 5, &g_lzma_alloc) != SZ_OK) {
            free(decomp); free(comp_data);
            return NULL;
        }
        LzmaDec_Init(&dec);

        size_t src_pos = 5;
        size_t decomp_len = 0;
        while (src_pos < comp_size) {
            if (decomp_len >= decomp_cap) {
                decomp_cap *= 2;
                uint8_t *tmp = (uint8_t *)realloc(decomp, decomp_cap);
                if (!tmp) { LzmaDec_Free(&dec, &g_lzma_alloc); free(decomp); free(comp_data); return NULL; }
                decomp = tmp;
            }
            size_t dest_len = decomp_cap - decomp_len;
            size_t src_len = comp_size - src_pos;
            ELzmaStatus status;
            SRes res = LzmaDec_DecodeToBuf(&dec, decomp + decomp_len, &dest_len,
                                           comp_data + src_pos, &src_len,
                                           LZMA_FINISH_ANY, &status);
            decomp_len += dest_len;
            src_pos += src_len;
            if (res != SZ_OK) break;
            if (status == LZMA_STATUS_FINISHED_WITH_MARK ||
                status == LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK) break;
            if (dest_len == 0 && src_len == 0) break;
        }

        LzmaDec_Free(&dec, &g_lzma_alloc);
        free(comp_data);
        *out_len = decomp_len;
        return decomp;
    }

    if (actual_method == INNO_COMPRESS_LZMA2) {
        if (comp_size < 1) { free(comp_data); return NULL; }
        uint8_t lzma2_prop = comp_data[0]; /* single property byte */

        size_t decomp_cap = (size_t)(de->file_offset + de->file_size) * 2;
        if (decomp_cap < 65536) decomp_cap = 65536;
        uint8_t *decomp = (uint8_t *)malloc(decomp_cap);
        if (!decomp) { free(comp_data); return NULL; }

        CLzma2Dec dec;
        Lzma2Dec_Construct(&dec);
        if (Lzma2Dec_Allocate(&dec, lzma2_prop, &g_lzma_alloc) != SZ_OK) {
            free(decomp); free(comp_data);
            return NULL;
        }
        Lzma2Dec_Init(&dec);

        size_t src_pos = 1;
        size_t decomp_len = 0;
        while (src_pos < comp_size) {
            if (decomp_len >= decomp_cap) {
                decomp_cap *= 2;
                uint8_t *tmp = (uint8_t *)realloc(decomp, decomp_cap);
                if (!tmp) { Lzma2Dec_Free(&dec, &g_lzma_alloc); free(decomp); free(comp_data); return NULL; }
                decomp = tmp;
            }
            size_t dest_len = decomp_cap - decomp_len;
            size_t src_len = comp_size - src_pos;
            ELzmaStatus status;
            SRes res = Lzma2Dec_DecodeToBuf(&dec, decomp + decomp_len, &dest_len,
                                            comp_data + src_pos, &src_len,
                                            LZMA_FINISH_ANY, &status);
            decomp_len += dest_len;
            src_pos += src_len;
            if (res != SZ_OK) break;
            if (status == LZMA_STATUS_FINISHED_WITH_MARK ||
                status == LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK) break;
            if (dest_len == 0 && src_len == 0) break;
        }

        Lzma2Dec_Free(&dec, &g_lzma_alloc);
        free(comp_data);
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

int inno_open(const char *exe_path, inno_archive_t *arc) {
    if (!exe_path || !arc) return -1;
    memset(arc, 0, sizeof(*arc));
    arc->fd = -1;

    int fd = OPEN_RB(exe_path);
    if (fd < 0) {
        INNO_LOG("cannot open %s: %s", exe_path, strerror(errno));
        return -1;
    }
    arc->fd = fd;

    /* ── Find offset table ── */
    uint64_t table_offset = 0;
    int found = 0;

    /* Method 1: PE resource 11111 */
    if (find_pe_resource_11111(fd, &table_offset) == 0) {
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
        INNO_LOG("cannot find InnoSetup offset table in %s", exe_path);
        CLOSE_FD(fd); arc->fd = -1;
        return -1;
    }

    /* ── Parse offset table ── */
    uint64_t header_offset, data_offset;
    if (parse_offset_table(fd, table_offset, &header_offset, &data_offset) < 0) {
        CLOSE_FD(fd); arc->fd = -1;
        return -1;
    }
    arc->header_offset = header_offset;
    arc->data_offset = data_offset;

    /* ── Read version ID (64 bytes at header_offset) ── */
    uint8_t version_id[64];
    if (read_at(fd, header_offset, version_id, 64) < 0) {
        INNO_LOG("cannot read version ID at 0x%llx", (unsigned long long)header_offset);
        CLOSE_FD(fd); arc->fd = -1;
        return -1;
    }
    if (parse_version_string(version_id, &arc->version) < 0) {
        CLOSE_FD(fd); arc->fd = -1;
        return -1;
    }

    INNO_LOG("version: %d.%d.%d unicode=%d",
             arc->version.major, arc->version.minor, arc->version.patch,
             arc->version.unicode);

    int v = INNO_VER(arc->version.major, arc->version.minor, arc->version.patch);
    if (v < INNO_VER(5,3,0) || v > INNO_VER(5,6,99)) {
        INNO_LOG("unsupported version %d.%d.%d (need 5.3.x - 5.6.x)",
                 arc->version.major, arc->version.minor, arc->version.patch);
        CLOSE_FD(fd); arc->fd = -1;
        return -1;
    }

    /* ── Decompress block stream 1 (main header + file entries) ── */
    size_t stream1_len = 0, stream1_consumed = 0;
    uint8_t *stream1 = decompress_block_stream(fd, header_offset + 64,
                                                &stream1_len, &stream1_consumed);
    if (!stream1) {
        INNO_LOG("failed to decompress header block stream 1");
        CLOSE_FD(fd); arc->fd = -1;
        return -1;
    }

    /* Parse the header and file entries */
    if (parse_header_stream1(stream1, stream1_len, &arc->version, arc) < 0) {
        INNO_LOG("failed to parse header");
        free(stream1);
        CLOSE_FD(fd); arc->fd = -1;
        return -1;
    }
    free(stream1);

    /* ── Decompress block stream 2 (data entries) ── */
    arc->data_entries = (inno_data_entry_t *)calloc(arc->data_entry_count, sizeof(inno_data_entry_t));
    if (!arc->data_entries && arc->data_entry_count > 0) {
        CLOSE_FD(fd); arc->fd = -1;
        return -1;
    }

    size_t stream2_len = 0, stream2_consumed = 0;
    uint8_t *stream2 = decompress_block_stream(fd, header_offset + 64 + stream1_consumed,
                                                &stream2_len, &stream2_consumed);
    if (!stream2) {
        INNO_LOG("failed to decompress header block stream 2");
        free(arc->data_entries); arc->data_entries = NULL;
        CLOSE_FD(fd); arc->fd = -1;
        return -1;
    }

    if (parse_data_entries(stream2, stream2_len, &arc->version, arc) < 0) {
        INNO_LOG("failed to parse data entries");
        free(stream2);
        free(arc->data_entries); arc->data_entries = NULL;
        CLOSE_FD(fd); arc->fd = -1;
        return -1;
    }
    free(stream2);

    return arc->file_count;
}

int inno_extract_file(inno_archive_t *arc, int file_index,
                       const char *output_path,
                       inno_progress_fn progress, void *user_data) {
    if (!arc || file_index < 0 || file_index >= arc->file_count) return -1;
    if (!output_path) return -1;

    inno_file_entry_t *fe = &arc->files[file_index];
    if (fe->location == 0xFFFFFFFF) {
        INNO_LOG("file %d has no data (location=0xFFFFFFFF)", file_index);
        return -1;
    }
    if ((int)fe->location >= arc->data_entry_count) {
        INNO_LOG("file %d: location %u out of range (%d entries)",
                 file_index, fe->location, arc->data_entry_count);
        return -1;
    }

    inno_data_entry_t *de = &arc->data_entries[fe->location];

    if (de->call_instruction_optimized) {
        INNO_LOG("exe instruction filter not implemented (file %s)", fe->destination);
        return -1;
    }

    if (progress) {
        progress(fe->destination, 0, (long long)de->file_size, user_data);
    }

    /* Decompress the chunk */
    size_t chunk_len = 0;
    uint8_t *chunk = decompress_chunk(arc->fd, arc->data_offset, de,
                                       arc->compression, &chunk_len);
    if (!chunk) {
        INNO_LOG("failed to decompress chunk for %s", fe->destination);
        return -1;
    }

    /* Verify we have enough data */
    if (de->file_offset + de->file_size > chunk_len) {
        INNO_LOG("chunk too small for %s: need %llu+%llu, have %zu",
                 fe->destination,
                 (unsigned long long)de->file_offset,
                 (unsigned long long)de->file_size,
                 chunk_len);
        free(chunk);
        return -1;
    }

    /* Write to output file — GOG Galaxy files need zlib decompression */
    uint8_t *file_data = chunk + de->file_offset;
    size_t file_len = (size_t)de->file_size;
    uint8_t *inflated = NULL;

    if (fe->gog_galaxy) {
        /* GOG Galaxy double-compression: data in chunk is zlib-compressed */
        size_t inf_cap = file_len * 4;
        if (inf_cap < 65536) inf_cap = 65536;
        inflated = (uint8_t *)malloc(inf_cap);
        if (!inflated) { free(chunk); return -1; }

        z_stream zs;
        memset(&zs, 0, sizeof(zs));
        if (inflateInit(&zs) != Z_OK) { free(inflated); free(chunk); return -1; }

        zs.next_in = file_data;
        zs.avail_in = (uInt)file_len;
        zs.next_out = inflated;
        zs.avail_out = (uInt)inf_cap;

        int zret;
        while ((zret = inflate(&zs, Z_NO_FLUSH)) == Z_OK || zret == Z_BUF_ERROR) {
            if (zs.avail_out == 0) {
                size_t done = zs.total_out;
                inf_cap *= 2;
                uint8_t *tmp = (uint8_t *)realloc(inflated, inf_cap);
                if (!tmp) { inflateEnd(&zs); free(inflated); free(chunk); return -1; }
                inflated = tmp;
                zs.next_out = inflated + done;
                zs.avail_out = (uInt)(inf_cap - done);
            }
            if (zs.avail_in == 0) break;
        }
        inflateEnd(&zs);

        file_data = inflated;
        file_len = zs.total_out;
    }

    FILE *out = fopen(output_path, "wb");
    if (!out) {
        INNO_LOG("cannot create %s: %s", output_path, strerror(errno));
        free(inflated); free(chunk);
        return -1;
    }

    size_t written = fwrite(file_data, 1, file_len, out);
    fclose(out);

    if (written != file_len) {
        INNO_LOG("write error for %s", output_path);
        free(inflated); free(chunk);
        return -1;
    }

    free(inflated);
    free(chunk);

    if (progress) {
        progress(fe->destination, (long long)de->file_size,
                 (long long)de->file_size, user_data);
    }

    return 0;
}

void inno_close(inno_archive_t *arc) {
    if (!arc) return;
    if (arc->fd >= 0) {
        CLOSE_FD(arc->fd);
        arc->fd = -1;
    }
    free(arc->data_entries);
    arc->data_entries = NULL;
    arc->file_count = 0;
    arc->data_entry_count = 0;
}
