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

/* Find Scripts entry in the TOC XML.
 * We look for <name>package.pkg</name> then nested <name>Scripts</name>
 * and extract offset/size/length from its <data> block.
 * Returns 0 on success, -1 if not found. */
static int xar_find_scripts(const char *xml, uint64_t *offset,
                            uint64_t *size, uint64_t *length)
{
	/* Find the package.pkg directory entry */
	const char *pkg = strstr(xml, "<name>package.pkg</name>");
	if (!pkg) {
		LOG_E("pkg: TOC has no package.pkg entry\n");
		return -1;
	}

	/* Find Scripts within the package.pkg scope */
	const char *scripts = strstr(pkg, "<name>Scripts</name>");
	if (!scripts) {
		LOG_E("pkg: TOC has no Scripts entry under package.pkg\n");
		return -1;
	}

	/* Find the <data> block near Scripts (search backwards from <name>Scripts)
	 * In the XML, <data> appears before <name> within the same <file> element */
	const char *data_tag = NULL;
	/* Search backwards from scripts for <data> */
	for (const char *p = scripts - 1; p > pkg; p--) {
		if (strncmp(p, "<data>", 6) == 0) {
			/* Make sure this isn't from a different file element.
			 * Check that there's no </file> between this <data> and Scripts */
			const char *check = strstr(p, "</file>");
			if (check && check < scripts) continue; /* belongs to different entry */
			data_tag = p;
			break;
		}
	}
	if (!data_tag) {
		LOG_E("pkg: no <data> block found for Scripts entry\n");
		return -1;
	}

	/* Extract offset, size, length from the <data> block */
	const char *data_end = strstr(data_tag, "</data>");
	if (!data_end) {
		LOG_E("pkg: unclosed <data> block\n");
		return -1;
	}

	*offset = 0;
	*size = 0;
	*length = 0;

	const char *p;
	if ((p = strstr(data_tag, "<offset>")) != NULL && p < data_end)
		*offset = (uint64_t) strtoull(p + 8, NULL, 10);
	if ((p = strstr(data_tag, "<size>")) != NULL && p < data_end)
		*size = (uint64_t) strtoull(p + 6, NULL, 10);
	if ((p = strstr(data_tag, "<length>")) != NULL && p < data_end)
		*length = (uint64_t) strtoull(p + 8, NULL, 10);

	if (*length == 0) {
		LOG_E("pkg: Scripts entry has zero length\n");
		return -1;
	}

	return 0;
}

/* ── Gzip inflate stream ─────────────────────────────────────────── */
#define GZ_BUF_SIZE 65536

typedef struct {
	int fd;
	z_stream strm;
	uint8_t in_buf[GZ_BUF_SIZE];
	int initialized;
	int finished;
} gz_stream_t;

static int gz_open(gz_stream_t *gs, int fd, uint64_t offset)
{
	memset(gs, 0, sizeof(*gs));
	gs->fd = fd;
	LSEEK(fd, (long long) offset, SEEK_SET);

	/* MAX_WBITS + 16 = auto-detect gzip header */
	if (inflateInit2(&gs->strm, MAX_WBITS + 16) != Z_OK) {
		LOG_E("pkg: inflateInit2 failed\n");
		return -1;
	}
	gs->initialized = 1;
	return 0;
}

/* Read exactly `size` decompressed bytes. Returns bytes read (may be < size at EOF). */
static int gz_read(gz_stream_t *gs, void *buf, int size)
{
	if (gs->finished) return 0;

	gs->strm.next_out = (Bytef *) buf;
	gs->strm.avail_out = (uInt) size;

	while (gs->strm.avail_out > 0) {
		if (gs->strm.avail_in == 0) {
			int n = READ_FD(gs->fd, gs->in_buf, GZ_BUF_SIZE);
			if (n <= 0) {
				gs->finished = 1;
				break;
			}
			gs->strm.next_in = gs->in_buf;
			gs->strm.avail_in = (uInt) n;
		}
		int ret = inflate(&gs->strm, Z_NO_FLUSH);
		if (ret == Z_STREAM_END) {
			gs->finished = 1;
			break;
		}
		if (ret != Z_OK) {
			LOG_E("pkg: gzip inflate error %d\n", ret);
			gs->finished = 1;
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
 * Returns 1 if entry read, 0 at trailer/EOF, -1 on error. */
static int cpio_read_entry(gz_stream_t *gs, cpio_entry_t *entry)
{
	char hdr[CPIO_HDR_SIZE];
	int n = gz_read(gs, hdr, CPIO_HDR_SIZE);
	if (n <= 0) return 0;
	if (n < CPIO_HDR_SIZE) {
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
	if (n < namesize) {
		LOG_E("pkg: truncated cpio filename\n");
		return -1;
	}
	entry->name[namesize - 1] = '\0'; /* namesize includes NUL */

	if (strcmp(entry->name, CPIO_TRAILER) == 0)
		return 0;

	return 1;
}

/* ── Game file detection ─────────────────────────────────────────── */
static const char *game_prefix = "./payload/Contents/Resources/game/";

/* Audio extensions — skipped when skip_audio is set */
static int is_audio_ext(const char *fname)
{
	return dxx_is_android_gog_audio_extension(fname);
}

static int is_game_file(const char *cpio_path, const char **basename_out)
{
	/* Must be under the game directory */
	size_t pfx_len = strlen(game_prefix);
	if (_stricmp(cpio_path, game_prefix) == 0) return 0; /* the dir itself */
	if (strncmp(cpio_path, game_prefix, pfx_len) != 0) return 0;

	const char *fname = cpio_path + pfx_len;
	if (strchr(fname, '/')) return 0; /* skip subdirectories */

	const char *dot = strrchr(fname, '.');
	if (!dot) return 0;

	for (const char **ext = dxx_android_game_file_extensions; *ext; ext++) {
		if (_stricmp(dot, *ext) == 0) {
			*basename_out = fname;
			return 1;
		}
	}
	return 0;
}

/* ── Scan pass: enumerate game files without extracting ──────────── */
static int pkg_scan_cpio(pkg_archive_t *arc)
{
	gz_stream_t gs;
	if (gz_open(&gs, arc->fd, arc->scripts_abs_offset) < 0)
		return -1;

	arc->file_count = 0;
	arc->scanned_bytes = 0;
	arc->output_bytes = 0;
	cpio_entry_t entry;
	unsigned int entry_count = 0;
	int ret;

	while ((ret = cpio_read_entry(&gs, &entry)) == 1) {
		const char *basename;
		if (++entry_count > DXX_EXTRACT_MAX_ENTRIES ||
		    entry.filesize > DXX_EXTRACT_MAX_ENTRY_BYTES ||
		    dxx_extract_add_bytes(&arc->scanned_bytes, entry.filesize,
		                          DXX_EXTRACT_MAX_TOTAL_BYTES) < 0) {
			ret = -1;
			break;
		}
		if (is_game_file(entry.name, &basename)) {
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
	if (xar_find_scripts(toc_xml, &scr_offset, &scr_size, &scr_length) < 0) {
		free(toc_xml);
		goto fail;
	}
	free(toc_xml);

	uint64_t heap_start = xhdr.header_size + xhdr.toc_compressed;
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
	if (gz_open(&gs, arc->fd, arc->scripts_abs_offset) < 0)
		return -1;

	int extracted = 0;
	uint64_t scanned_bytes = 0;
	uint64_t output_bytes = 0;
	unsigned int entry_count = 0;
	cpio_entry_t entry;
	int ret;

	while ((ret = cpio_read_entry(&gs, &entry)) == 1) {
		const char *basename;
		int is_game = is_game_file(entry.name, &basename);
		if (++entry_count > DXX_EXTRACT_MAX_ENTRIES ||
		    entry.filesize > DXX_EXTRACT_MAX_ENTRY_BYTES ||
		    dxx_extract_add_bytes(&scanned_bytes, entry.filesize,
		                          DXX_EXTRACT_MAX_TOTAL_BYTES) < 0) {
			ret = -1;
			break;
		}

		if (!is_game || entry.filesize == 0 ||
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
		snprintf(out_path, sizeof(out_path), "%s/%s", output_dir, basename);

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
