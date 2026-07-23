/*
 * sow_extract.c — Extract game files from Descent .sow (ARJ) archives.
 *
 * Self-contained ARJ decoder — no external library dependencies.
 * Supports ARJ methods 0 (stored) and 1-3 (LZSS+Huffman).
 *
 * The LZSS+Huffman decompressor is based on Haruhiko Okumura's
 * public-domain AR algorithm (1989).  The ARJ header parser handles
 * Interplay's concatenated-archive .sow format, where each file is
 * a separate ARJ archive and some entries store the filename in the
 * comment field rather than the filename field.
 *
 * Build (via CMake from repo root):
 *   cmake -S android/app/src/main/cpp/extract -B android/tests/build
 *   cmake --build android/tests/build --config Release
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#include <direct.h>
#include <windows.h>
#define mkdir_sow(d) _mkdir(d)
#define PATH_SEP     '\\'
#else
#include <dirent.h>
#include <unistd.h>
#define mkdir_sow(d) mkdir(d, 0755)
#define PATH_SEP     '/'
#endif

#include "sow_extract.h"

#include "extract_limits.h"

/* ── Directory scanning ─────────────────────────────────────────────── */

/* Case-insensitive extension check */
static int has_sow_ext(const char *name)
{
	const char *dot = strrchr(name, '.');
	if (!dot) return 0;
	dot++;
	return (
#ifdef _WIN32
	    _stricmp(dot, "sow") == 0
#else
	    strcasecmp(dot, "sow") == 0
#endif
	);
}

#ifdef _WIN32
/* Windows: use FindFirstFile/FindNextFile */
static void scan_dir_recursive(const char *base, sow_file_list_t *out)
{
	char pattern[SOW_PATH_LEN];
	WIN32_FIND_DATAA fd;
	HANDLE h;

	snprintf(pattern, sizeof(pattern), "%s\\*", base);
	h = FindFirstFileA(pattern, &fd);
	if (h == INVALID_HANDLE_VALUE) return;

	do {
		if (fd.cFileName[0] == '.' &&
		    (fd.cFileName[1] == '\0' ||
		     (fd.cFileName[1] == '.' && fd.cFileName[2] == '\0')))
			continue;

		char fullpath[SOW_PATH_LEN];
		snprintf(fullpath, sizeof(fullpath), "%s\\%s", base, fd.cFileName);

		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			scan_dir_recursive(fullpath, out);
		} else if (has_sow_ext(fd.cFileName) && out->count < SOW_MAX_FILES) {
			strncpy(out->paths[out->count], fullpath, SOW_PATH_LEN - 1);
			out->paths[out->count][SOW_PATH_LEN - 1] = '\0';
			out->count++;
		}
	} while (FindNextFileA(h, &fd));

	FindClose(h);
}
#else
/* POSIX: use opendir/readdir */
static void scan_dir_recursive(const char *base, sow_file_list_t *out)
{
	DIR *d = opendir(base);
	if (!d) return;

	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		if (ent->d_name[0] == '.') continue;

		char fullpath[SOW_PATH_LEN];
		snprintf(fullpath, sizeof(fullpath), "%s/%s", base, ent->d_name);

		struct stat st;
		if (stat(fullpath, &st) != 0) continue;

		if (S_ISDIR(st.st_mode)) {
			scan_dir_recursive(fullpath, out);
		} else if (has_sow_ext(ent->d_name) && out->count < SOW_MAX_FILES) {
			strncpy(out->paths[out->count], fullpath, SOW_PATH_LEN - 1);
			out->paths[out->count][SOW_PATH_LEN - 1] = '\0';
			out->count++;
		}
	}
	closedir(d);
}
#endif

int sow_scan_dir(const char *dir_path, sow_file_list_t *out)
{
	if (!dir_path || !out) return -1;
	memset(out, 0, sizeof(*out));
	scan_dir_recursive(dir_path, out);
	return out->count;
}

/* ── Path helpers ───────────────────────────────────────────────────── */

/* Get just the filename from a path (strip directories) */
static const char *basename_of(const char *path)
{
	const char *last = path;
	for (const char *p = path; *p; p++) {
		if (*p == '/' || *p == '\\') last = p + 1;
	}
	return last;
}

/* Check if a filename's extension matches the filter list */
static int ext_matches(const char *filename, const char **extensions)
{
	if (!extensions) return 1; /* NULL = extract all */

	const char *dot = strrchr(filename, '.');
	if (!dot) return 0;
	dot++;

	for (const char **ext = extensions; *ext; ext++) {
#ifdef _WIN32
		if (_stricmp(dot, *ext) == 0) return 1;
#else
		if (strcasecmp(dot, *ext) == 0) return 1;
#endif
	}
	return 0;
}

/* Ensure a directory exists, creating parent directories as needed */
static void mkdirs(const char *path)
{
	char tmp[SOW_PATH_LEN];
	strncpy(tmp, path, sizeof(tmp) - 1);
	tmp[sizeof(tmp) - 1] = '\0';

	for (char *p = tmp + 1; *p; p++) {
		if (*p == '/' || *p == '\\') {
			*p = '\0';
			mkdir_sow(tmp);
			*p = PATH_SEP;
		}
	}
	mkdir_sow(tmp);
}

/* ── ARJ format constants and structures ─────────────────────────────── */

#define ARJ_MAGIC_0       0x60
#define ARJ_MAGIC_1       0xEA
#define ARJ_MAX_HEADER    2600
#define ARJ_TYPE_BINARY   0
#define ARJ_TYPE_COMMENT  2
#define ARJ_METHOD_STORED 0

/* LZSS+Huffman decompressor constants — ARJ method 1 */
#define DICBIT        15
#define DICSIZ        (1u << DICBIT) /* 32768 for method 1 */
#define THRESHOLD     2
#define MAXMATCH      256
#define NC            (255 + MAXMATCH + 2 - THRESHOLD) /* 511 */
#define NP            (DICBIT + 1)                     /* 16 for method 1 */
#define NT            19
#define TBIT          5 /* bits to read NT count */
#define PBIT          5 /* bits to read NP count */
#define CBIT          9 /* bits to read NC count */
#define NPT           (NP > NT ? NP : NT)
#define PT_TABLE_BITS 8 /* table bits for pt/pos Huffman trees */

static unsigned char read_u8(const unsigned char *p)
{
	return p[0];
}

static unsigned short read_u16(const unsigned char *p)
{
	return (unsigned short) (p[0] | ((unsigned short) p[1] << 8));
}

static unsigned int read_u32(const unsigned char *p)
{
	return (unsigned int) p[0] | ((unsigned int) p[1] << 8) |
	       ((unsigned int) p[2] << 16) | ((unsigned int) p[3] << 24);
}

static unsigned int arj_crc32_update(unsigned int crc, const unsigned char *data,
                                     size_t len)
{
	while (len-- > 0) {
		crc ^= *data++;
		for (int bit = 0; bit < 8; bit++)
			crc = (crc & 1u) ? (crc >> 1) ^ 0xedb88320u : crc >> 1;
	}
	return crc;
}

static unsigned int arj_crc32(const unsigned char *data, size_t len)
{
	return ~arj_crc32_update(0xffffffffu, data, len);
}

/* ── Bit reader for Huffman stream ──────────────────────────────────── */
/*
 * Matches the canonical ARJ bitbuf model: a 16-bit left-aligned register
 * plus an 8-bit staging byte.  This is critical for correctness — the
 * Huffman lookup tables index directly into bitbuf>>shift.
 */

typedef struct {
	const unsigned char *data;
	unsigned int data_len;
	unsigned int pos;       /* byte position in input */
	unsigned int bitbuf;    /* 16-bit left-aligned bit buffer */
	unsigned char byte_buf; /* pending byte (MSB = next bit) */
	int bitcount;           /* valid bits remaining in byte_buf */
	uint64_t consumed_bits;
	int exhausted;
} bitreader_t;

/* Consume n bits from the stream and refill.  Mirrors ARJ fillbuf(). */
static void br_fillbuf(bitreader_t *br, int n)
{
	uint64_t total_bits = (uint64_t) br->data_len * 8u;
	if ((uint64_t) n > total_bits - br->consumed_bits) {
		br->consumed_bits = total_bits;
		br->exhausted = 1;
	} else {
		br->consumed_bits += (unsigned int) n;
	}
	while (br->bitcount < n) {
		br->bitbuf = ((br->bitbuf << br->bitcount) |
		              ((unsigned int) br->byte_buf >> (8 - br->bitcount))) &
		             0xFFFFu;
		n -= br->bitcount;
		br->byte_buf = (br->pos < br->data_len) ? br->data[br->pos++] : 0;
		br->bitcount = 8;
	}
	br->bitcount -= n;
	br->bitbuf = ((br->bitbuf << n) |
	              ((unsigned int) br->byte_buf >> (8 - n))) &
	             0xFFFFu;
	br->byte_buf <<= n;
}

/* Peek + consume: return the top n bits, then refill. */
static unsigned int br_getbits(bitreader_t *br, int n)
{
	unsigned int rc = br->bitbuf >> (16 - n);
	br_fillbuf(br, n);
	return rc;
}

static void br_init(bitreader_t *br, const unsigned char *data, unsigned int len)
{
	br->data = data;
	br->data_len = len;
	br->pos = 0;
	br->bitbuf = 0;
	br->byte_buf = 0;
	br->bitcount = 0;
	br->consumed_bits = 0;
	br->exhausted = 0;
	br_fillbuf(br, 16); /* prime the buffer with 2 bytes */
	br->consumed_bits = 0;
	br->exhausted = 0;
}

/* ── Huffman table decoder ──────────────────────────────────────────── */

#define HUFF_TABLE_BITS 12
#define HUFF_TABLE_SIZE (1 << HUFF_TABLE_BITS)

typedef struct {
	unsigned short left[2 * NC];
	unsigned short right[2 * NC];
	unsigned short table[HUFF_TABLE_SIZE];
	unsigned char len[NC];
	int n; /* number of symbols */
} hufftree_t;

static int huff_make_table(hufftree_t *ht, int nchar, const unsigned char *bitlen,
                           int tablebits)
{
	unsigned short count[17] = { 0 };
	unsigned short weight[17];
	unsigned int start[18];
	int avail = nchar;
	int i, ch, len, nextcode;

	if (!ht || !bitlen || nchar <= 0 || nchar > NC || tablebits <= 0 ||
	    tablebits > HUFF_TABLE_BITS)
		return -1;

	for (i = 0; i < nchar; i++) {
		if (bitlen[i] > 16) return -1;
		count[bitlen[i]]++;
	}

	start[1] = 0;
	for (i = 1; i <= 16; i++)
		start[i + 1] = start[i] + (count[i] << (16 - i));

	if (start[17] != 0 && start[17] != (1u << 16))
		return -1;

	int jutbits = 16 - tablebits;
	for (i = 1; i <= tablebits; i++) {
		start[i] >>= jutbits;
		weight[i] = 1u << (tablebits - i);
	}
	for (; i <= 16; i++)
		weight[i] = 1u << (16 - i);

	i = start[tablebits + 1] >> jutbits;
	if (i != 0) {
		int k = 1u << tablebits;
		while (i < k)
			ht->table[i++] = 0;
	}

	for (ch = 0; ch < nchar; ch++) {
		if ((len = bitlen[ch]) == 0) continue;
		nextcode = start[len] + weight[len];
		if (len <= tablebits) {
			if (nextcode > (1 << tablebits)) return -1;
			for (i = start[len]; i < nextcode; i++)
				ht->table[i] = (unsigned short) ch;
		} else {
			unsigned short *p;
			int k = start[len];
			p = &ht->table[k >> jutbits];
			i = len - tablebits;
			while (i > 0) {
				if (*p == 0) {
					if (avail >= 2 * NC) return -1;
					ht->right[avail] = ht->left[avail] = 0;
					*p = (unsigned short) avail++;
				}
				if (k & (1u << (15 - tablebits - (len - tablebits - i))))
					p = &ht->right[*p];
				else
					p = &ht->left[*p];
				i--;
			}
			*p = (unsigned short) ch;
		}
		start[len] = nextcode;
	}
	ht->n = nchar;
	return 0;
}

static unsigned short huff_decode(hufftree_t *ht, bitreader_t *br, int tablebits)
{
	unsigned short c = ht->table[br->bitbuf >> (16 - tablebits)];
	if (c >= (unsigned short) ht->n) {
		unsigned int mask = 1u << (16 - tablebits - 1);
		do {
			c = (br->bitbuf & mask) ? ht->right[c] : ht->left[c];
			mask >>= 1;
		} while (c >= (unsigned short) ht->n);
	}
	br_fillbuf(br, ht->len[c]);
	return c;
}

/* ── ARJ LZSS+Huffman decompressor (methods 1-3) ───────────────────── */

typedef struct {
	bitreader_t br;
	hufftree_t c_tree; /* literal/length tree */
	hufftree_t p_tree; /* position tree */
	unsigned char dic[DICSIZ];
	unsigned int dic_pos;
	unsigned int orig_size;
	unsigned int decoded;
} arj_decoder_t;

static int read_pt_len(arj_decoder_t *dec, int nn, int nbit, int i_special)
{
	unsigned char buf[NPT];
	memset(buf, 0, sizeof(buf));

	int n = br_getbits(&dec->br, nbit);
	if (n == 0) {
		int c = br_getbits(&dec->br, nbit);
		if (c >= nn) return -1;
		memset(dec->p_tree.len, 0, NPT);
		memset(dec->p_tree.table, 0, sizeof(dec->p_tree.table));
		for (int i = 0; i < (1 << PT_TABLE_BITS); i++)
			dec->p_tree.table[i] = (unsigned short) c;
		dec->p_tree.n = nn;
		return 0;
	}
	if (n > NPT) return -1; /* reference bounds at NPT, not nn */
	if (n > nn) return -1;

	int i = 0;
	while (i < n) {
		/* Variable-length code: 3-bit base, unary extension if == 7 */
		int c = dec->br.bitbuf >> 13; /* top 3 bits */
		if (c == 7) {
			unsigned int mask = 1u << 12;
			while (mask & dec->br.bitbuf) {
				mask >>= 1;
				c++;
			}
		}
		if (c > 16) return -1;
		br_fillbuf(&dec->br, (c < 7) ? 3 : c - 3);
		buf[i++] = (unsigned char) c;
		if (i == i_special) {
			int zeros = br_getbits(&dec->br, 2);
			while (--zeros >= 0 && i < nn) buf[i++] = 0;
		}
	}
	while (i < nn) buf[i++] = 0;
	memcpy(dec->p_tree.len, buf, nn);
	if (huff_make_table(&dec->p_tree, nn, buf, PT_TABLE_BITS) < 0) {
		fprintf(stderr, "  [ERROR] make_table failed for pt/pos nn=%d\n", nn);
		return -1;
	}
	return 0;
}

static int read_c_len(arj_decoder_t *dec)
{
	unsigned char buf[NC];
	memset(buf, 0, sizeof(buf));

	int n = br_getbits(&dec->br, CBIT);
	if (n == 0) {
		int c = br_getbits(&dec->br, CBIT);
		if (c >= NC) return -1;
		memset(dec->c_tree.len, 0, NC);
		memset(dec->c_tree.table, 0, sizeof(dec->c_tree.table));
		for (int i = 0; i < HUFF_TABLE_SIZE; i++)
			dec->c_tree.table[i] = (unsigned short) c;
		dec->c_tree.n = NC;
		return 0;
	}
	if (n > NC) return -1;

	int i = 0;
	while (i < n) {
		unsigned short c = huff_decode(&dec->p_tree, &dec->br, PT_TABLE_BITS);
		if (c <= 2) {
			int zeros;
			if (c == 0)
				zeros = 1;
			else if (c == 1)
				zeros = br_getbits(&dec->br, 4) + 3;
			else
				zeros = br_getbits(&dec->br, CBIT) + 20;
			while (--zeros >= 0 && i < NC) buf[i++] = 0;
		} else {
			buf[i++] = (unsigned char) (c - 2);
		}
	}
	while (i < NC) buf[i++] = 0;
	memcpy(dec->c_tree.len, buf, NC);
	if (huff_make_table(&dec->c_tree, NC, buf, HUFF_TABLE_BITS) < 0) {
		fprintf(stderr, "  [ERROR] make_table failed for c_tree\n");
		return -1;
	}
	return 0;
}

static int arj_decode_block(arj_decoder_t *dec, unsigned char *out, unsigned int out_cap)
{
	unsigned int blocksize = br_getbits(&dec->br, 16);
	if (dec->br.exhausted) return -1;
	if (blocksize == 0) return 0;

	if (read_pt_len(dec, NT, TBIT, 3) < 0 || read_c_len(dec) < 0 ||
	    read_pt_len(dec, NP, PBIT, -1) < 0 || dec->br.exhausted)
		return -1;

	unsigned int count = 0;
	while (count < blocksize && dec->decoded < dec->orig_size) {
		unsigned short c = huff_decode(&dec->c_tree, &dec->br, HUFF_TABLE_BITS);
		if (dec->br.exhausted) return -1;
		if (c <= 255) {
			/* literal byte */
			dec->dic[dec->dic_pos] = (unsigned char) c;
			if (dec->decoded < out_cap)
				out[dec->decoded] = (unsigned char) c;
			dec->dic_pos = (dec->dic_pos + 1) & (DICSIZ - 1);
			dec->decoded++;
			count++;
		} else {
			/* length + position reference.
			 * Descent SOW archives use LZH compression (min match = 3),
			 * not ARJ's THRESHOLD=2. Add 1 to the standard ARJ formula. */
			unsigned int len = c - (255 + 1 - THRESHOLD) + 1;

			/* decode position — matches decode_p() in reference */
			unsigned short p = huff_decode(&dec->p_tree, &dec->br, PT_TABLE_BITS);
			unsigned int pos = p;
			if (p != 0) {
				p--;
				pos = ((unsigned int) 1 << p) + br_getbits(&dec->br, p);
			}
			if (dec->br.exhausted) return -1;

			int src = (int) dec->dic_pos - (int) pos - 1;
			if (src < 0) src += DICSIZ;
			for (unsigned int k = 0; k < len && dec->decoded < dec->orig_size; k++) {
				unsigned char b = dec->dic[src];
				dec->dic[dec->dic_pos] = b;
				if (dec->decoded < out_cap)
					out[dec->decoded] = b;
				dec->dic_pos = (dec->dic_pos + 1) & (DICSIZ - 1);
				if (++src >= (int) DICSIZ) src = 0;
				dec->decoded++;
			}
			count++;
		}
	}
	return 1;
}

/* Decompress ARJ method 1-3 data.
 * Returns allocated buffer of orig_size bytes, or NULL on failure. */
static unsigned char *arj_decompress(const unsigned char *comp_data,
                                     unsigned int comp_size,
                                     unsigned int orig_size)
{
	if (!dxx_extract_entry_allowed(orig_size, comp_size) ||
	    !dxx_extract_memory_allowed(comp_size, orig_size))
		return NULL;
	unsigned char *out = (unsigned char *) malloc(orig_size);
	if (!out) return NULL;

	arj_decoder_t dec;
	memset(&dec, 0, sizeof(dec));
	br_init(&dec.br, comp_data, comp_size);
	dec.orig_size = orig_size;
	dec.decoded = 0;
	dec.dic_pos = 0;
	memset(dec.dic, 0x00, DICSIZ); /* zero-fill dictionary (LZH convention) */

	while (dec.decoded < orig_size) {
		if (arj_decode_block(&dec, out, orig_size) <= 0)
			break;
	}

	if (dec.decoded < orig_size) {
		free(out);
		return NULL;
	}
	return out;
}

#ifdef SOW_EXTRACT_TESTING
int sow_test_huff_make_table(const unsigned char *bitlen, int nchar, int tablebits)
{
	hufftree_t tree;
	memset(&tree, 0, sizeof(tree));
	return huff_make_table(&tree, nchar, bitlen, tablebits);
}

int sow_test_read_pt_len(const unsigned char *data, unsigned int size, int nn)
{
	arj_decoder_t dec;
	memset(&dec, 0, sizeof(dec));
	br_init(&dec.br, data, size);
	return read_pt_len(&dec, nn, TBIT, -1);
}

int sow_test_decode_block(const unsigned char *data, unsigned int size)
{
	unsigned char out = 0;
	arj_decoder_t dec;
	memset(&dec, 0, sizeof(dec));
	br_init(&dec.br, data, size);
	dec.orig_size = 1;
	return arj_decode_block(&dec, &out, 1);
}
#endif

/* ── SOW/ARJ file iterator ──────────────────────────────────────────── */

typedef struct {
	char filename[260];
	unsigned int comp_size;
	unsigned int orig_size;
	unsigned int original_crc;
	int method;       /* 0=stored, 1-3=compressed */
	int file_type;    /* 0=binary, 2=archive header */
	long data_offset; /* offset of compressed data in the file */
} arj_entry_t;

/* Read the next ARJ entry from a file.
 * fp must be positioned at the start of an ARJ marker (0x60 0xEA) or
 * within the gap between concatenated archives.
 * Returns 1 if an entry was found, 0 at end of file, -1 on error. */
static int arj_read_entry(FILE *fp, arj_entry_t *entry, long file_size)
{
	unsigned char magic[2];
	/* Scan forward for next 0x60 0xEA marker */
	for (;;) {
		long pos = ftell(fp);
		if (pos < 0 || pos >= file_size - 4) return 0;
		if (fread(magic, 1, 2, fp) != 2) return 0;
		if (magic[0] == ARJ_MAGIC_0 && magic[1] == ARJ_MAGIC_1)
			break;
		/* Rewind 1 byte (we consumed 2, want to advance by 1) */
		fseek(fp, -1, SEEK_CUR);
	}

	/* Read basic_header_size */
	unsigned char hsbuf[2];
	if (fread(hsbuf, 1, 2, fp) != 2) return 0;
	unsigned int header_size = read_u16(hsbuf);

	if (header_size == 0) {
		/* End-of-archive marker */
		return 0;
	}
	if (header_size > ARJ_MAX_HEADER) return -1;

	/* Read header data + CRC */
	unsigned char hdr[ARJ_MAX_HEADER + 4];
	if (fread(hdr, 1, header_size + 4, fp) != header_size + 4) return -1;

	if (arj_crc32(hdr, header_size) != read_u32(hdr + header_size)) return -1;

	unsigned int first_hdr_size = read_u8(hdr);
	if (header_size < 30 || first_hdr_size < 30 || first_hdr_size > header_size)
		return -1;

	entry->method = read_u8(hdr + 5);
	entry->file_type = read_u8(hdr + 6);

	/* For archive headers (type 2), offsets +12/+16 are dates, not sizes.
	 * No compressed data follows an archive header. */
	if (entry->file_type == ARJ_TYPE_COMMENT) {
		entry->comp_size = 0;
		entry->orig_size = 0;
		entry->original_crc = 0;
	} else {
		entry->comp_size = read_u32(hdr + 12);
		entry->orig_size = read_u32(hdr + 16);
		entry->original_crc = read_u32(hdr + 20);
	}

	/* Extract filename — first null-terminated string after fixed header */
	const unsigned char *name_start = hdr + first_hdr_size;
	const unsigned char *hdr_end = hdr + header_size;
	size_t name_len = 0;
	while (name_start + name_len < hdr_end && name_start[name_len] != 0)
		name_len++;

	if (name_len == 0 && name_start + 1 < hdr_end) {
		/* Empty filename — Interplay .sow quirk: actual name is in comment */
		const unsigned char *comment = name_start + 1;
		name_len = 0;
		while (comment + name_len < hdr_end && comment[name_len] != 0)
			name_len++;
		if (name_len > 0 && name_len < 260) {
			memcpy(entry->filename, comment, name_len);
			entry->filename[name_len] = '\0';
		} else {
			entry->filename[0] = '\0';
		}
	} else if (name_len > 0 && name_len < 260) {
		memcpy(entry->filename, name_start, name_len);
		entry->filename[name_len] = '\0';
	} else {
		entry->filename[0] = '\0';
	}

	/* Skip extended headers */
	for (;;) {
		unsigned char ehbuf[2];
		if (fread(ehbuf, 1, 2, fp) != 2) return -1;
		unsigned int ext_size = read_u16(ehbuf);
		if (ext_size == 0) break;
		long ext_offset = ftell(fp);
		if (ext_offset < 0 || ext_offset > file_size ||
		    ext_size > (uint64_t) file_size - (uint64_t) ext_offset ||
		    4u > (uint64_t) file_size - (uint64_t) ext_offset - ext_size)
			return -1;
		unsigned int crc = 0xffffffffu;
		unsigned int remaining = ext_size;
		while (remaining > 0) {
			unsigned char chunk[512];
			size_t amount = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
			if (fread(chunk, 1, amount, fp) != amount) return -1;
			crc = arj_crc32_update(crc, chunk, amount);
			remaining -= (unsigned int) amount;
		}
		unsigned char crcbuf[4];
		if (fread(crcbuf, 1, sizeof(crcbuf), fp) != sizeof(crcbuf) ||
		    ~crc != read_u32(crcbuf))
			return -1;
	}

	entry->data_offset = ftell(fp);
	return 1;
}

/* ── Main extraction function ──────────────────────────────────────── */

static int sow_extract_impl(const char *sow_path, const char *output_dir,
                            const char **extensions,
                            sow_progress_fn progress, void *user_data,
                            int append_existing)
{
	if (!sow_path || !output_dir) return -1;

	FILE *fp = fopen(sow_path, "rb");
	if (!fp) {
		fprintf(stderr, "sow_extract: cannot open '%s'\n", sow_path);
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	long file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	mkdirs(output_dir);

	/* First pass: count total bytes for progress */
	uint64_t total_bytes = 0;
	{
		arj_entry_t e;
		long saved_pos;
		unsigned int entry_count = 0;
		while ((saved_pos = ftell(fp)) >= 0) {
			int r = arj_read_entry(fp, &e, file_size);
			uint64_t output_size;

			if (r < 0) {
				fclose(fp);
				return -1;
			}
			if (r == 0) break;
			if (e.data_offset < 0 || e.data_offset > file_size ||
			    e.comp_size > (uint64_t) file_size - (uint64_t) e.data_offset) {
				fclose(fp);
				return -1;
			}
			if (e.file_type == ARJ_TYPE_BINARY && e.filename[0] != '\0') {
				if (ext_matches(e.filename, extensions)) {
					output_size = e.method == ARJ_METHOD_STORED ? e.comp_size : e.orig_size;
					if (++entry_count > DXX_EXTRACT_MAX_ENTRIES ||
					    (e.method == ARJ_METHOD_STORED && e.comp_size != e.orig_size) ||
					    !dxx_extract_entry_allowed(output_size, e.comp_size) ||
					    !dxx_extract_memory_allowed(e.comp_size,
					                                e.method == ARJ_METHOD_STORED ? 0 : e.orig_size) ||
					    dxx_extract_add_bytes(&total_bytes, output_size,
					                          DXX_EXTRACT_MAX_TOTAL_BYTES) < 0) {
						fclose(fp);
						return -1;
					}
				}
			}
			/* Skip compressed data to reach next entry */
			if (fseek(fp, e.data_offset + e.comp_size, SEEK_SET) != 0) {
				fclose(fp);
				return -1;
			}
		}
		fseek(fp, 0, SEEK_SET);
	}
	if (!dxx_extract_has_free_space(output_dir, total_bytes)) {
		fclose(fp);
		return -1;
	}

	/* Second pass: extract */
	int extracted = 0;
	long long bytes_done = 0;
	arj_entry_t e;

	while (1) {
		int r = arj_read_entry(fp, &e, file_size);
		if (r <= 0) break;

		/* Skip archive headers and unnamed entries */
		if (e.file_type != ARJ_TYPE_BINARY || e.filename[0] == '\0') {
			fseek(fp, e.data_offset + e.comp_size, SEEK_SET);
			continue;
		}

		const char *filename = basename_of(e.filename);

		/* Extension filter */
		if (!ext_matches(filename, extensions)) {
			bytes_done += e.orig_size;
			fseek(fp, e.data_offset + e.comp_size, SEEK_SET);
			continue;
		}

		/* Progress callback */
		if (progress) {
			if (progress(filename, bytes_done, (long long) total_bytes, user_data)) break;
		}

		/* Build output path — flatten to just filename */
		char out_path[SOW_PATH_LEN];
		snprintf(out_path, sizeof(out_path), "%s%c%s",
		         output_dir, PATH_SEP, filename);

		/* Read compressed data */
		unsigned char *comp_data = (unsigned char *) malloc(e.comp_size);
		if (!comp_data) {
			fseek(fp, e.data_offset + e.comp_size, SEEK_SET);
			continue;
		}
		fseek(fp, e.data_offset, SEEK_SET);
		if (fread(comp_data, 1, e.comp_size, fp) != e.comp_size) {
			free(comp_data);
			continue;
		}

		unsigned char *out_data = NULL;
		unsigned int out_size = 0;

		if (e.method == ARJ_METHOD_STORED) {
			out_data = comp_data;
			out_size = e.comp_size;
			comp_data = NULL; /* don't free — out_data owns it */
		} else if (e.method >= 1 && e.method <= 3) {
			out_data = arj_decompress(comp_data, e.comp_size, e.orig_size);
			out_size = e.orig_size;
			free(comp_data);
			comp_data = NULL;
		} else {
			fprintf(stderr, "sow_extract: unsupported method %d for '%s'\n",
			        e.method, filename);
			free(comp_data);
			continue;
		}

		if (!out_data) {
			fprintf(stderr, "sow_extract: decompression failed for '%s'\n",
			        filename);
			continue;
		}
		if (arj_crc32(out_data, out_size) != e.original_crc) {
			fprintf(stderr, "sow_extract: payload CRC mismatch for '%s'\n",
			        filename);
			free(out_data);
			continue;
		}

		/* Write output file */
		FILE *outf = fopen(out_path, append_existing ? "ab" : "wb");
		if (outf) {
			fwrite(out_data, 1, out_size, outf);
			fclose(outf);
			extracted++;
			bytes_done += out_size;
		} else {
			fprintf(stderr, "sow_extract: cannot create '%s'\n", out_path);
		}
		free(out_data);
	}

	fclose(fp);
	return extracted;
}

int sow_extract(const char *sow_path, const char *output_dir,
                const char **extensions,
                sow_progress_fn progress, void *user_data)
{
	return sow_extract_impl(sow_path, output_dir, extensions, progress,
	                        user_data, 0);
}

int sow_extract_with_mode(const char *sow_path, const char *output_dir,
                          const char **extensions,
                          sow_progress_fn progress, void *user_data,
                          int append_existing)
{
	return sow_extract_impl(sow_path, output_dir, extensions, progress,
	                        user_data, append_existing);
}
