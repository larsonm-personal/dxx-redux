/*
 * pkg_reader.c — Extract game files from Mac .pkg installers
 *
 * Format chain: XAR → gzip → cpio "odc" → game files
 * See PKG_PARSER_PLAN.md for format details.
 */

#include "pkg_reader.h"

#include "extract_limits.h"
#include "game_file_extensions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#define OPEN_RB(path)     _open((path), _O_RDONLY | _O_BINARY)
#define CLOSE_FD(fd)      _close(fd)
#define READ_FD(fd, b, n) _read((fd), (b), (unsigned) (n))
#define LSEEK(fd, off, w) _lseeki64((fd), (off), (w))
#define mkdir_p(d)        _mkdir(d)
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#define OPEN_RB(path)     open((path), O_RDONLY)
#define CLOSE_FD(fd)      close(fd)
#define READ_FD(fd, b, n) read((fd), (b), (n))
#define LSEEK(fd, off, w) lseek((fd), (off), (w))
#define mkdir_p(d)        mkdir((d), 0755)
#endif

#ifndef _WIN32
#include <strings.h>
#define _stricmp strcasecmp
#endif

#include <zlib.h>

#ifdef ANDROID
#include <android/log.h>
#define LOG_E(...) __android_log_print(ANDROID_LOG_ERROR, "pkg_reader", __VA_ARGS__)
#else
#define LOG_E(...) fprintf(stderr, __VA_ARGS__)
#endif

/* ── Byte swap helpers (XAR header is big-endian) ────────────────── */
static uint16_t read_be16(const uint8_t *p)
{
	return (uint16_t) ((p[0] << 8) | p[1]);
}
static uint32_t read_be32(const uint8_t *p)
{
	return ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16) |
	       ((uint32_t) p[2] << 8) | (uint32_t) p[3];
}
static uint64_t read_be64(const uint8_t *p)
{
	return ((uint64_t) read_be32(p) << 32) | read_be32(p + 4);
}

/* ── XAR header ──────────────────────────────────────────────────── */
#define XAR_MAGIC 0x78617221 /* "xar!" */

typedef struct {
	uint16_t header_size;
	uint16_t version;
	uint64_t toc_compressed;
	uint64_t toc_uncompressed;
} xar_header_t;

static int read_exact(int fd, void *buffer, size_t size)
{
	uint8_t *output = (uint8_t *) buffer;
	size_t done = 0;
	while (done < size) {
		size_t remaining = size - done;
		unsigned int chunk = remaining > INT_MAX ? INT_MAX : (unsigned int) remaining;
		int count = READ_FD(fd, output + done, chunk);
		if (count < 0 && errno == EINTR) continue;
		if (count <= 0) return -1;
		done += (size_t) count;
	}
	return 0;
}

static int xar_toc_bounds_valid(const xar_header_t *hdr, uint64_t file_size)
{
	if (hdr->header_size < 28 || hdr->version != 1 ||
	    hdr->toc_compressed == 0 || hdr->toc_uncompressed == 0 ||
	    hdr->toc_compressed > PKG_MAX_TOC_BYTES ||
	    hdr->toc_uncompressed > PKG_MAX_TOC_BYTES ||
	    hdr->toc_compressed > (uint64_t) ULONG_MAX ||
	    hdr->toc_uncompressed > (uint64_t) ULONG_MAX ||
	    hdr->toc_compressed > (uint64_t) SIZE_MAX ||
	    hdr->toc_uncompressed >= (uint64_t) SIZE_MAX ||
	    (uint64_t) hdr->header_size > file_size ||
	    hdr->toc_compressed > file_size - hdr->header_size ||
	    !dxx_extract_memory_allowed(hdr->toc_compressed, hdr->toc_uncompressed) ||
	    !dxx_extract_ratio_allowed(hdr->toc_uncompressed, hdr->toc_compressed)) {
		LOG_E("pkg: invalid XAR TOC bounds\n");
		return 0;
	}
	return 1;
}

static int xar_parse_header(int fd, uint64_t file_size, xar_header_t *hdr)
{
	uint8_t buf[28];
	if (LSEEK(fd, 0, SEEK_SET) < 0 || read_exact(fd, buf, sizeof(buf)) < 0) {
		LOG_E("pkg: failed to read XAR header\n");
		return -1;
	}
	if (read_be32(buf) != XAR_MAGIC) {
		LOG_E("pkg: bad XAR magic (expected 0x78617221)\n");
		return -1;
	}
	hdr->header_size = read_be16(buf + 4);
	hdr->version = read_be16(buf + 6);
	hdr->toc_compressed = read_be64(buf + 8);
	hdr->toc_uncompressed = read_be64(buf + 16);
	return xar_toc_bounds_valid(hdr, file_size) ? 0 : -1;
}

static int xar_decompress_toc(const uint8_t *compressed, size_t compressed_size,
                              uint8_t *output, size_t expected_size)
{
	z_stream stream;
	int status;
	if (!compressed || !output || compressed_size > UINT_MAX ||
	    expected_size > UINT_MAX)
		return -1;
	memset(&stream, 0, sizeof(stream));
	stream.next_in = (Bytef *) compressed;
	stream.avail_in = (uInt) compressed_size;
	stream.next_out = output;
	stream.avail_out = (uInt) expected_size;
	if (inflateInit(&stream) != Z_OK) return -1;
	status = inflate(&stream, Z_FINISH);
	inflateEnd(&stream);
	return status == Z_STREAM_END &&
	               stream.total_in == compressed_size &&
	               stream.total_out == expected_size
	           ? 0
	           : -1;
}

#ifdef PKG_READER_TESTING
int pkg_test_validate_xar_toc(uint16_t header_size, uint16_t version,
                              uint64_t compressed, uint64_t uncompressed,
                              uint64_t file_size)
{
	xar_header_t header = { header_size, version, compressed, uncompressed };
	return xar_toc_bounds_valid(&header, file_size);
}

int pkg_test_decompress_toc(const uint8_t *compressed, size_t compressed_size,
                            uint8_t *output, size_t expected_size)
{
	return xar_decompress_toc(compressed, compressed_size, output, expected_size);
}
#endif

/* ── XAR TOC XML parsing ────────────────────────────────────────── */

static const char *find_bounded(const char *begin, const char *end,
                                const char *text)
{
	size_t length = strlen(text);
	for (const char *p = begin; length <= (size_t) (end - p); p++)
		if (memcmp(p, text, length) == 0)
			return p;
	return NULL;
}

static const char *find_file_start(const char *begin, const char *end)
{
	const char *p = begin;
	while ((p = find_bounded(p, end, "<file")) != NULL) {
		if (p + 5 < end && (p[5] == '>' || p[5] == ' ' || p[5] == '\t' ||
		                    p[5] == '\r' || p[5] == '\n'))
			return p;
		p += 5;
	}
	return NULL;
}

static int xml_file_bounds(const char *open, const char *limit,
                           const char **content, const char **close)
{
	const char *p = find_bounded(open, limit, ">");
	unsigned int depth = 1;
	if (!p) return -1;
	*content = ++p;
	while (depth > 0) {
		const char *next_open = find_file_start(p, limit);
		const char *next_close = find_bounded(p, limit, "</file>");
		if (!next_close) return -1;
		if (next_open && next_open < next_close) {
			depth++;
			p = next_open + 5;
		} else {
			depth--;
			if (depth == 0) {
				*close = next_close;
				return 0;
			}
			p = next_close + 7;
		}
	}
	return -1;
}

static int xml_direct_value(const char *begin, const char *end,
                            const char *open_tag, const char *close_tag,
                            const char **value_begin, const char **value_end)
{
	const char *p = begin;
	int found = 0;
	while (p < end) {
		const char *nested = find_file_start(p, end);
		const char *tag = find_bounded(p, end, open_tag);
		if (nested && (!tag || nested < tag)) {
			const char *nested_content;
			const char *nested_close;
			if (xml_file_bounds(nested, end, &nested_content, &nested_close) < 0)
				return -1;
			p = nested_close + 7;
			continue;
		}
		if (!tag) break;
		const char *value = tag + strlen(open_tag);
		const char *close = find_bounded(value, end, close_tag);
		if (!close || found) return -1;
		*value_begin = value;
		*value_end = close;
		found = 1;
		p = close + strlen(close_tag);
	}
	return found ? 0 : -1;
}

static void trim_xml_text(const char **begin, const char **end)
{
	while (*begin < *end && (**begin == ' ' || **begin == '\t' ||
	                         **begin == '\r' || **begin == '\n'))
		(*begin)++;
	while (*end > *begin && ((*end)[-1] == ' ' || (*end)[-1] == '\t' ||
	                         (*end)[-1] == '\r' || (*end)[-1] == '\n'))
		(*end)--;
}

static int xml_text_equals(const char *begin, const char *end, const char *text)
{
	trim_xml_text(&begin, &end);
	return (size_t) (end - begin) == strlen(text) &&
	       memcmp(begin, text, strlen(text)) == 0;
}

static int parse_xml_decimal(const char *begin, const char *end, uint64_t *value)
{
	uint64_t result = 0;
	trim_xml_text(&begin, &end);
	if (begin == end) return -1;
	for (const char *p = begin; p < end; p++) {
		unsigned int digit;
		if (*p < '0' || *p > '9') return -1;
		digit = (unsigned int) (*p - '0');
		if (result > (UINT64_MAX - digit) / 10u) return -1;
		result = result * 10u + digit;
	}
	*value = result;
	return 0;
}

static int xar_find_scripts(const char *xml, size_t xml_size, uint64_t *offset,
                            uint64_t *size, uint64_t *length)
{
	const char *xml_end = xml + xml_size;
	const char *toc_open;
	const char *toc_close;
	const char *pkg_content = NULL;
	const char *pkg_close = NULL;
	const char *scripts_content = NULL;
	const char *scripts_close = NULL;
	const char *p = xml;

	if (find_bounded(xml, xml_end, "<!--") ||
	    find_bounded(xml, xml_end, "<![CDATA[") ||
	    find_bounded(xml, xml_end, "<!DOCTYPE"))
		goto invalid;
	toc_open = find_bounded(xml, xml_end, "<toc>");
	if (!toc_open ||
	    find_bounded(toc_open + 5, xml_end, "<toc>") ||
	    !(toc_close = find_bounded(toc_open + 5, xml_end, "</toc>")))
		goto invalid;

	p = toc_open + 5;
	while ((p = find_file_start(p, toc_close)) != NULL) {
		const char *content;
		const char *close;
		const char *name_begin;
		const char *name_end;
		if (xml_file_bounds(p, toc_close, &content, &close) < 0)
			goto invalid;
		if (xml_direct_value(content, close, "<name>", "</name>",
		                     &name_begin, &name_end) == 0 &&
		    xml_text_equals(name_begin, name_end, "package.pkg")) {
			if (pkg_content) goto invalid;
			pkg_content = content;
			pkg_close = close;
		}
		p = close + 7;
	}
	if (!pkg_content) goto invalid;

	p = pkg_content;
	while ((p = find_file_start(p, pkg_close)) != NULL) {
		const char *content;
		const char *close;
		const char *name_begin;
		const char *name_end;
		if (xml_file_bounds(p, pkg_close, &content, &close) < 0)
			goto invalid;
		if (xml_direct_value(content, close, "<name>", "</name>",
		                     &name_begin, &name_end) == 0 &&
		    xml_text_equals(name_begin, name_end, "Scripts")) {
			if (scripts_content) goto invalid;
			scripts_content = content;
			scripts_close = close;
		}
		p = close + 7;
	}
	if (!scripts_content) goto invalid;

	{
		const char *data_begin;
		const char *data_end;
		const char *value_begin;
		const char *value_end;
		const char *encoding;
		const char *encoding_end;
		const char *style;
		const char *last;
		if (xml_direct_value(scripts_content, scripts_close, "<data>", "</data>",
		                     &data_begin, &data_end) < 0)
			goto invalid;
		encoding = find_bounded(data_begin, data_end, "<encoding");
		if (!encoding ||
		    encoding + 9 >= data_end ||
		    !(encoding[9] == ' ' || encoding[9] == '\t' ||
		      encoding[9] == '\r' || encoding[9] == '\n') ||
		    !(encoding_end = find_bounded(encoding, data_end, ">")) ||
		    find_bounded(encoding_end + 1, data_end, "<encoding") ||
		    !(style = find_bounded(encoding, encoding_end,
		                           "style=\"application/octet-stream\"")) ||
		    !(style[-1] == ' ' || style[-1] == '\t' || style[-1] == '\r' ||
		      style[-1] == '\n'))
			goto invalid;
		last = encoding_end;
		while (last > encoding && (last[-1] == ' ' || last[-1] == '\t' ||
		                           last[-1] == '\r' || last[-1] == '\n'))
			last--;
		if (last == encoding || last[-1] != '/') goto invalid;
		if (xml_direct_value(data_begin, data_end, "<offset>", "</offset>",
		                     &value_begin, &value_end) < 0 ||
		    parse_xml_decimal(value_begin, value_end, offset) < 0 ||
		    xml_direct_value(data_begin, data_end, "<size>", "</size>",
		                     &value_begin, &value_end) < 0 ||
		    parse_xml_decimal(value_begin, value_end, size) < 0 ||
		    xml_direct_value(data_begin, data_end, "<length>", "</length>",
		                     &value_begin, &value_end) < 0 ||
		    parse_xml_decimal(value_begin, value_end, length) < 0 ||
		    *length == 0 || *size != *length)
			goto invalid;
	}
	return 0;

invalid:
	LOG_E("pkg: invalid package.pkg/Scripts TOC entry\n");
	return -1;
}

/* ── Gzip inflate stream ─────────────────────────────────────────── */
#define GZ_BUF_SIZE 65536
/* cpio archives may zero-pad the final 512-byte output block */
#define CPIO_PAD_MAX 511

typedef struct {
	int fd;
	z_stream strm;
	uint8_t in_buf[GZ_BUF_SIZE];
	uint64_t input_remaining;
	int initialized;
	int stream_end;
	int failed;
} gz_stream_t;

static int gz_open(gz_stream_t *gs, int fd, uint64_t offset, uint64_t length)
{
	memset(gs, 0, sizeof(*gs));
	gs->fd = fd;
	gs->input_remaining = length;
	if (length == 0 || offset > INT64_MAX ||
	    LSEEK(fd, (int64_t) offset, SEEK_SET) < 0) {
		LOG_E("pkg: failed to seek to Scripts member\n");
		return -1;
	}

	/* MAX_WBITS + 16 = auto-detect gzip header */
	if (inflateInit2(&gs->strm, MAX_WBITS + 16) != Z_OK) {
		LOG_E("pkg: inflateInit2 failed\n");
		return -1;
	}
	gs->initialized = 1;
	return 0;
}

/* Read up to size decompressed bytes, failing on physical or member exhaustion. */
static int gz_read(gz_stream_t *gs, void *buf, int size)
{
	if (size <= 0 || gs->failed) return -1;
	if (gs->stream_end) return 0;

	gs->strm.next_out = (Bytef *) buf;
	gs->strm.avail_out = (uInt) size;

	while (gs->strm.avail_out > 0) {
		if (gs->strm.avail_in == 0) {
			size_t chunk = gs->input_remaining > GZ_BUF_SIZE
			                   ? GZ_BUF_SIZE
			                   : (size_t) gs->input_remaining;
			if (chunk == 0 || read_exact(gs->fd, gs->in_buf, chunk) < 0) {
				LOG_E("pkg: truncated Scripts gzip member\n");
				gs->failed = 1;
				return -1;
			}
			gs->input_remaining -= chunk;
			gs->strm.next_in = gs->in_buf;
			gs->strm.avail_in = (uInt) chunk;
		}
		int ret = inflate(&gs->strm, Z_NO_FLUSH);
		if (ret == Z_STREAM_END) {
			gs->stream_end = 1;
			break;
		}
		if (ret != Z_OK) {
			LOG_E("pkg: gzip inflate error %d\n", ret);
			gs->failed = 1;
			return -1;
		}
	}
	return size - (int) gs->strm.avail_out;
}

/* Skip `size` bytes in the decompressed stream (read and discard). */
static int gz_skip(gz_stream_t *gs, uint64_t size)
{
	uint8_t discard[8192];
	while (size > 0) {
		int chunk = (size > sizeof(discard)) ? (int) sizeof(discard) : (int) size;
		int got = gz_read(gs, discard, chunk);
		if (got <= 0) return -1;
		size -= (uint64_t) got;
	}
	return 0;
}

static int gz_require_complete(gz_stream_t *gs)
{
	uint8_t padding[CPIO_PAD_MAX + 1];
	size_t padding_size = 0;
	int got;
	if (gs->failed) return -1;
	do {
		got = gz_read(gs, padding, sizeof(padding));
		if (got < 0) return -1;
		for (int i = 0; i < got; i++)
			if (padding[i] != 0) {
				LOG_E("pkg: nonzero data follows cpio trailer\n");
				return -1;
			}
		padding_size += (size_t) got;
		if (padding_size > CPIO_PAD_MAX) {
			LOG_E("pkg: excessive data follows cpio trailer\n");
			return -1;
		}
	} while (got > 0);
	if (!gs->stream_end || gs->strm.avail_in != 0 ||
	    gs->input_remaining != 0) {
		LOG_E("pkg: incomplete or trailing Scripts gzip data "
		      "(end=%d buffered=%u remaining=%llu)\n",
		      gs->stream_end, (unsigned int) gs->strm.avail_in,
		      (unsigned long long) gs->input_remaining);
		return -1;
	}
	return 0;
}

static void gz_close(gz_stream_t *gs)
{
	if (gs->initialized) {
		inflateEnd(&gs->strm);
		gs->initialized = 0;
	}
}

/* ── cpio "odc" parser ───────────────────────────────────────────── */
#define CPIO_ODC_MAGIC "070707"
#define CPIO_HDR_SIZE  76
#define CPIO_TRAILER   "TRAILER!!!"

typedef struct {
	char name[PKG_PATH_LEN];
	uint64_t filesize;
	uint32_t mode;
} cpio_entry_t;

static int octal_val(const char *s, int len)
{
	int v = 0;
	for (int i = 0; i < len; i++) {
		if (s[i] < '0' || s[i] > '7') return -1;
		v = v * 8 + (s[i] - '0');
	}
	return v;
}

static int64_t octal_val64(const char *s, int len)
{
	int64_t v = 0;
	for (int i = 0; i < len; i++) {
		if (s[i] < '0' || s[i] > '7') return -1;
		v = v * 8 + (s[i] - '0');
	}
	return v;
}

/* Read one cpio entry header from the gz stream.
 * Returns 1 if entry read, 0 at the trailer, -1 on error. */
static int cpio_read_entry(gz_stream_t *gs, cpio_entry_t *entry)
{
	char hdr[CPIO_HDR_SIZE];
	int n = gz_read(gs, hdr, CPIO_HDR_SIZE);
	if (n != CPIO_HDR_SIZE) {
		LOG_E("pkg: truncated cpio header (got %d)\n", n);
		return -1;
	}

	if (memcmp(hdr, CPIO_ODC_MAGIC, 6) != 0) {
		LOG_E("pkg: bad cpio magic (expected 070707, got %.6s)\n", hdr);
		return -1;
	}

	int namesize = octal_val(hdr + 59, 6);
	{
		int64_t filesize = octal_val64(hdr + 65, 11);
		if (filesize < 0) {
			LOG_E("pkg: invalid cpio filesize\n");
			return -1;
		}
		entry->filesize = (uint64_t) filesize;
	}
	entry->mode = (uint32_t) octal_val(hdr + 18, 6);

	if (namesize <= 0 || namesize >= PKG_PATH_LEN) {
		LOG_E("pkg: invalid cpio namesize %d\n", namesize);
		return -1;
	}

	n = gz_read(gs, entry->name, namesize);
	if (n != namesize || entry->name[namesize - 1] != '\0' ||
	    memchr(entry->name, '\0', (size_t) namesize - 1u)) {
		LOG_E("pkg: truncated cpio filename\n");
		return -1;
	}

	if (strcmp(entry->name, CPIO_TRAILER) == 0) {
		if (entry->filesize != 0) {
			LOG_E("pkg: cpio trailer has data\n");
			return -1;
		}
		return 0;
	}

	return 1;
}

/* ── Game file detection ─────────────────────────────────────────── */
static const char *game_prefix = "./payload/Contents/Resources/game/";

enum {
	PKG_FILE_INVALID = -1,
	PKG_FILE_IGNORED = 0,
	PKG_FILE_GAME = 1
};

/* Audio extensions — skipped when skip_audio is set */
static int is_audio_ext(const char *fname)
{
	return dxx_is_android_gog_audio_extension(fname);
}

static int is_safe_output_basename(const char *name)
{
	const unsigned char *p = (const unsigned char *) name;
	size_t length;
	if (!name || !*name || strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return 0;
	length = strlen(name);
	if (name[length - 1] == '.' || name[length - 1] == ' ')
		return 0;
	for (; *p; p++) {
		if (*p < 0x20 || *p == 0x7f || strchr("<>:\"/\\|?*", *p))
			return 0;
	}
	return 1;
}

static int is_game_file(const char *cpio_path, uint32_t mode,
                        const char **basename_out)
{
	/* Must be under the game directory */
	size_t pfx_len = strlen(game_prefix);
	if (strncmp(cpio_path, game_prefix, pfx_len) != 0)
		return PKG_FILE_IGNORED;
	if ((mode & 0170000u) == 0040000u)
		return PKG_FILE_IGNORED;

	const char *fname = cpio_path + pfx_len;
	/* Nested POSIX paths are package content, but are not flattened outputs. */
	if (strchr(fname, '\\')) {
		LOG_E("pkg: unsafe Windows separator in game output name\n");
		return PKG_FILE_INVALID;
	}
	if (strchr(fname, '/')) return PKG_FILE_IGNORED;
	if (!is_safe_output_basename(fname)) {
		LOG_E("pkg: unsafe game output name\n");
		return PKG_FILE_INVALID;
	}

	const char *dot = strrchr(fname, '.');
	if (!dot) return PKG_FILE_IGNORED;

	for (const char **ext = dxx_android_game_file_extensions; *ext; ext++) {
		if (_stricmp(dot, *ext) == 0) {
			*basename_out = fname;
			return PKG_FILE_GAME;
		}
	}
	return PKG_FILE_IGNORED;
}

static int build_output_path(char *out, size_t out_size,
                             const char *output_dir, const char *basename)
{
	int length;
	if (!out || out_size == 0 || !output_dir || !*output_dir ||
	    !is_safe_output_basename(basename))
		return -1;
	length = snprintf(out, out_size, "%s/%s", output_dir, basename);
	return length >= 0 && (size_t) length < out_size ? 0 : -1;
}

/* ── Scan pass: enumerate game files without extracting ──────────── */
static int pkg_scan_cpio(pkg_archive_t *arc)
{
	gz_stream_t gs;
	if (gz_open(&gs, arc->fd, arc->scripts_abs_offset,
	            arc->scripts_length) < 0)
		return -1;

	arc->file_count = 0;
	arc->scanned_bytes = 0;
	arc->output_bytes = 0;
	cpio_entry_t entry;
	unsigned int entry_count = 0;
	int ret;

	while ((ret = cpio_read_entry(&gs, &entry)) == 1) {
		const char *basename;
		int game_file;
		if (++entry_count > DXX_EXTRACT_MAX_ENTRIES ||
		    entry.filesize > DXX_EXTRACT_MAX_ENTRY_BYTES ||
		    dxx_extract_add_bytes(&arc->scanned_bytes, entry.filesize,
		                          DXX_EXTRACT_MAX_TOTAL_BYTES) < 0) {
			ret = -1;
			break;
		}
		game_file = is_game_file(entry.name, entry.mode, &basename);
		if (game_file == PKG_FILE_INVALID) {
			ret = -1;
			break;
		}
		if (game_file == PKG_FILE_GAME) {
			if (arc->file_count >= PKG_MAX_FILES ||
			    dxx_extract_add_bytes(&arc->output_bytes, entry.filesize,
			                          DXX_EXTRACT_MAX_TOTAL_BYTES) < 0) {
				ret = -1;
				break;
			}
			pkg_file_entry_t *f = &arc->files[arc->file_count++];
			snprintf(f->name, PKG_PATH_LEN, "%s", basename);
			f->size = entry.filesize;
		}
		/* Skip file data */
		if (entry.filesize > 0) {
			if (gz_skip(&gs, entry.filesize) < 0) {
				ret = -1;
				break;
			}
		}
	}

	if (ret == 0 && gz_require_complete(&gs) < 0)
		ret = -1;
	gz_close(&gs);
	if (ret >= 0 && !dxx_extract_ratio_allowed(arc->scanned_bytes, arc->scripts_length))
		return -1;
	return (ret < 0) ? -1 : arc->file_count;
}

/* ══════════════════════════════════════════════════════════════════ */
/* ── Public API ─────────────────────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════ */

int pkg_open(const char *pkg_path, pkg_archive_t *arc)
{
	memset(arc, 0, sizeof(*arc));
	arc->fd = -1;

	int fd = OPEN_RB(pkg_path);
	if (fd < 0) {
		LOG_E("pkg: cannot open %s: %s\n", pkg_path, strerror(errno));
		return -1;
	}
	arc->fd = fd;
	int64_t file_size = (int64_t) LSEEK(fd, 0, SEEK_END);
	if (file_size < 0)
		goto fail;

	/* Parse XAR header */
	xar_header_t xhdr;
	if (xar_parse_header(fd, (uint64_t) file_size, &xhdr) < 0)
		goto fail;

	/* Read and decompress TOC */
	uint8_t *toc_comp = (uint8_t *) malloc((size_t) xhdr.toc_compressed);
	if (!toc_comp) goto fail;

	if (LSEEK(fd, xhdr.header_size, SEEK_SET) < 0 ||
	    read_exact(fd, toc_comp, (size_t) xhdr.toc_compressed) < 0) {
		LOG_E("pkg: failed to read TOC\n");
		free(toc_comp);
		goto fail;
	}

	size_t toc_len = (size_t) xhdr.toc_uncompressed;
	char *toc_xml = (char *) malloc(toc_len + 1u);
	if (!toc_xml) {
		free(toc_comp);
		goto fail;
	}

	if (xar_decompress_toc(toc_comp, (size_t) xhdr.toc_compressed,
	                       (uint8_t *) toc_xml, toc_len) < 0) {
		LOG_E("pkg: TOC decompression failed\n");
		free(toc_xml);
		free(toc_comp);
		goto fail;
	}
	toc_xml[toc_len] = '\0';
	free(toc_comp);

	/* Find Scripts entry in TOC */
	uint64_t scr_offset, scr_size, scr_length;
	if (xar_find_scripts(toc_xml, toc_len, &scr_offset, &scr_size,
	                     &scr_length) < 0) {
		free(toc_xml);
		goto fail;
	}
	free(toc_xml);

	uint64_t heap_start = xhdr.header_size + xhdr.toc_compressed;
	if (heap_start > (uint64_t) file_size ||
	    scr_offset > (uint64_t) file_size - heap_start ||
	    scr_length > (uint64_t) file_size - heap_start - scr_offset) {
		LOG_E("pkg: Scripts member lies outside package\n");
		goto fail;
	}
	arc->scripts_abs_offset = heap_start + scr_offset;
	arc->scripts_length = scr_length;

	/* Scan cpio for game files */
	int n = pkg_scan_cpio(arc);
	if (n < 0) goto fail;

	return n;

fail:
	pkg_close(arc);
	return -1;
}

int pkg_extract_all(pkg_archive_t *arc, const char *output_dir,
                    pkg_progress_fn progress, void *user_data,
                    int skip_audio)
{
	if (arc->fd < 0 || arc->file_count == 0) return 0;
	if (!dxx_extract_has_free_space(output_dir, arc->output_bytes)) return -1;

	mkdir_p(output_dir);

	gz_stream_t gs;
	if (gz_open(&gs, arc->fd, arc->scripts_abs_offset,
	            arc->scripts_length) < 0)
		return -1;

	int extracted = 0;
	uint64_t scanned_bytes = 0;
	uint64_t output_bytes = 0;
	unsigned int entry_count = 0;
	cpio_entry_t entry;
	int ret;

	while ((ret = cpio_read_entry(&gs, &entry)) == 1) {
		const char *basename;
		int is_game = is_game_file(entry.name, entry.mode, &basename);
		if (++entry_count > DXX_EXTRACT_MAX_ENTRIES ||
		    entry.filesize > DXX_EXTRACT_MAX_ENTRY_BYTES ||
		    dxx_extract_add_bytes(&scanned_bytes, entry.filesize,
		                          DXX_EXTRACT_MAX_TOTAL_BYTES) < 0) {
			ret = -1;
			break;
		}
		if (is_game == PKG_FILE_INVALID) {
			ret = -1;
			break;
		}

		if (is_game != PKG_FILE_GAME || entry.filesize == 0 ||
		    (skip_audio && is_audio_ext(basename))) {
			if (entry.filesize > 0 && gz_skip(&gs, entry.filesize) < 0) {
				ret = -1;
				break;
			}
			continue;
		}
		if (dxx_extract_add_bytes(&output_bytes, entry.filesize,
		                          arc->output_bytes) < 0) {
			ret = -1;
			break;
		}

		/* Build output path */
		char out_path[1024];
		if (build_output_path(out_path, sizeof(out_path), output_dir, basename) < 0) {
			LOG_E("pkg: invalid or overlong output path\n");
			ret = -1;
			break;
		}

		if (progress)
			progress(basename, 0, (long long) entry.filesize, user_data);

		/* Open output file */
		FILE *fp = fopen(out_path, "wb");
		if (!fp) {
			LOG_E("pkg: cannot create %s: %s\n", out_path, strerror(errno));
			ret = -1;
			break;
		}

		/* Stream data to output file */
		uint64_t remaining = entry.filesize;
		uint8_t buf[65536];
		int ok = 1;
		while (remaining > 0) {
			int chunk = (remaining > sizeof(buf)) ? (int) sizeof(buf) : (int) remaining;
			int got = gz_read(&gs, buf, chunk);
			if (got <= 0) {
				ok = 0;
				break;
			}
			if (fwrite(buf, 1, (size_t) got, fp) != (size_t) got) {
				ok = 0;
				break;
			}
			remaining -= (uint64_t) got;

			if (progress)
				progress(basename, (long long) (entry.filesize - remaining),
				         (long long) entry.filesize, user_data);
		}
		if (fclose(fp) != 0)
			ok = 0;

		if (!ok) {
			LOG_E("pkg: failed to extract %s\n", basename);
			remove(out_path);
			ret = -1;
			break;
		}
		extracted++;
	}

	if (ret == 0 && gz_require_complete(&gs) < 0)
		ret = -1;
	gz_close(&gs);
	if (ret >= 0 && (!dxx_extract_ratio_allowed(scanned_bytes, arc->scripts_length) ||
	                 output_bytes > arc->output_bytes))
		return -1;
	return (ret < 0) ? -1 : extracted;
}

void pkg_close(pkg_archive_t *arc)
{
	if (arc->fd >= 0) {
		CLOSE_FD(arc->fd);
		arc->fd = -1;
	}
	arc->file_count = 0;
}
