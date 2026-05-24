/*
 * ISO 9660 reader for Mode 1 data tracks in raw BIN/CUE disc images.
 *
 * Raw CD sector layout (Mode 1, 2352 bytes):
 *   Bytes  0–11  : Sync pattern (00 FF FF FF FF FF FF FF FF FF FF 00)
 *   Bytes 12–15  : Header (minute, second, frame, mode)
 *   Bytes 16–2063: User data (2048 bytes)
 *   Bytes 2064–2351: ECC/EDC (288 bytes)
 *
 * ISO 9660 uses 2048-byte logical sectors.  The Primary Volume
 * Descriptor is at logical sector 16.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#define read(fd, buf, n) _read(fd, buf, (unsigned int) (n))
#define lseek            _lseek
#define close            _close
#define mkdir(d, m)      _mkdir(d)
#define open             _open
#define O_RDONLY         _O_RDONLY
#define O_WRONLY         _O_WRONLY
#define O_CREAT          _O_CREAT
#define O_TRUNC          _O_TRUNC
#define O_BINARY         _O_BINARY
#define strcasecmp       _stricmp
/* write() already available via _write */
#define write(fd, buf, n) _write(fd, buf, (unsigned int) (n))
#else
#include <unistd.h>
#include <fcntl.h>
#define O_BINARY 0
#endif

#include "iso9660_reader.h"

#ifdef ANDROID
#include <android/log.h>
#define ISO_LOG(...) __android_log_print(ANDROID_LOG_INFO, "ISO9660", __VA_ARGS__)
#else
#define ISO_LOG(...) ((void) 0)
#endif

/* ── Constants ───────────────────────────────────────────────────────── */

#define RAW_SECTOR_SIZE  2352
#define USER_DATA_OFFSET 16 /* Mode 1: skip sync(12) + header(4) */
#define USER_DATA_SIZE   2048

/* ISO 9660 PVD is at logical sector 16 */
#define PVD_SECTOR 16

/* Volume descriptor type codes */
#define VD_PRIMARY    1
#define VD_TERMINATOR 255

/* Directory record flag bits */
#define DR_FLAG_DIRECTORY 0x02

typedef struct {
	int fd;
	long long base_offset;
	int sector_stride;
	int user_data_offset;
	int num_logical_sectors;
} iso_reader_source_t;

/* ── Helpers ─────────────────────────────────────────────────────────── */

/* Get the byte length of an open file descriptor.
 * Returns -1 on error.
 */
static long long fd_length(int fd)
{
	if (fd < 0)
		return -1;

#ifdef _WIN32
	__int64 cur = _lseeki64(fd, 0, SEEK_CUR);
	__int64 end;

	if (cur < 0) cur = 0;
	end = _lseeki64(fd, 0, SEEK_END);
	_lseeki64(fd, cur, SEEK_SET);
	return end < 0 ? -1 : (long long) end;
#else
	off_t cur = lseek(fd, 0, SEEK_CUR);
	off_t end;

	if (cur < 0) cur = 0;
	end = lseek(fd, 0, SEEK_END);
	lseek(fd, cur, SEEK_SET);
	return end < 0 ? -1 : (long long) end;
#endif
}

static void init_raw_track_source(iso_reader_source_t *src,
                                  int fd,
                                  int track_start_sector,
                                  int track_num_sectors)
{
	memset(src, 0, sizeof(*src));
	src->fd = fd;
	src->base_offset = (long long) track_start_sector * RAW_SECTOR_SIZE;
	src->sector_stride = RAW_SECTOR_SIZE;
	src->user_data_offset = USER_DATA_OFFSET;
	src->num_logical_sectors = track_num_sectors;
}

static void init_iso_image_source(iso_reader_source_t *src, int fd)
{
	long long len;

	memset(src, 0, sizeof(*src));
	src->fd = fd;
	src->sector_stride = USER_DATA_SIZE;
	src->user_data_offset = 0;
	len = fd_length(fd);
	if (len > 0 && (len / USER_DATA_SIZE) <= 0x7fffffffLL)
		src->num_logical_sectors = (int) (len / USER_DATA_SIZE);
}

/* Read the 2048-byte logical sector from a source.
 * Returns 0 on success, -1 on error. */
static int read_user_sector(const iso_reader_source_t *src,
                            int logical_sector,
                            unsigned char *buf)
{
	long long offset;

	if (!src || src->fd < 0 || !buf || logical_sector < 0)
		return -1;
	if (src->num_logical_sectors > 0 && logical_sector >= src->num_logical_sectors)
		return -1;

	offset = src->base_offset + (long long) logical_sector * src->sector_stride + src->user_data_offset;
#ifdef _WIN32
	if (_lseeki64(src->fd, offset, SEEK_SET) != offset) return -1;
	int n = _read(src->fd, buf, USER_DATA_SIZE);
#else
	ssize_t n = pread(src->fd, buf, USER_DATA_SIZE, (off_t) offset);
#endif
	if (n != USER_DATA_SIZE) return -1;
	return 0;
}

/* Read little-endian uint32 from ISO directory record (both-endian fields) */
static unsigned int le32(const unsigned char *p)
{
	return (unsigned int) p[0] | ((unsigned int) p[1] << 8) | ((unsigned int) p[2] << 16) | ((unsigned int) p[3] << 24);
}

/* Ensure a directory exists, creating parents as needed */
static int mkdirs(const char *path)
{
	char tmp[ISO_PATH_LEN];
	char *p;
	size_t len;

	snprintf(tmp, sizeof(tmp), "%s", path);
	len = strlen(tmp);
	if (len > 0 && tmp[len - 1] == '/') tmp[len - 1] = '\0';

	for (p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			mkdir(tmp, 0755);
			*p = '/';
		}
	}
	return mkdir(tmp, 0755);
}

/* Clean an ISO 9660 filename: remove version suffix (;1), trailing dots,
 * and convert to lowercase.  Writes into dst (max dst_len). */
static void clean_iso_name(const char *src, int src_len,
                           char *dst, int dst_len)
{
	int i, o = 0;
	for (i = 0; i < src_len && o < dst_len - 1; i++) {
		char c = src[i];
		if (c == ';') break; /* version separator — stop */
		dst[o++] = (char) tolower((unsigned char) c);
	}
	/* Trim trailing dots */
	while (o > 0 && dst[o - 1] == '.') o--;
	dst[o] = '\0';
}

/* Check if a filename extension matches one in a filter list.
 * Returns 1 if it matches (or if extensions is NULL = accept all). */
static int ext_matches(const char *filename, const char **extensions)
{
	const char *dot;
	int i;

	if (!extensions) return 1;

	dot = strrchr(filename, '.');
	if (!dot) return 0;
	dot++; /* skip the dot */

	for (i = 0; extensions[i]; i++) {
		if (strcasecmp(dot, extensions[i]) == 0)
			return 1;
	}
	return 0;
}

/* D2 Windows CD installers carry a zero/ subtree of 1-byte placeholders.
 * These are installer cruft, not useful extracted content. */
static int should_skip_iso_directory(const char *clean_name)
{
	return clean_name && strcasecmp(clean_name, "zero") == 0;
}

static int join_iso_path(char *out, size_t out_size, const char *prefix,
	const char *name)
{
	size_t name_len = strlen(name);

	if (!prefix[0]) {
		if (name_len >= out_size)
			return -1;
		memcpy(out, name, name_len + 1);
		return 0;
	}

	size_t prefix_len = strlen(prefix);
	if (prefix_len + 1 + name_len >= out_size)
		return -1;
	memcpy(out, prefix, prefix_len);
	out[prefix_len] = '/';
	memcpy(out + prefix_len + 1, name, name_len + 1);
	return 0;
}

/* ── Directory tree walker ───────────────────────────────────────────── */

#define MAX_DIR_DEPTH 16

/* Recursively walk an ISO 9660 directory, appending entries to file_list.
 * dir_lba : LBA of the directory extent
 * dir_size: size of the directory extent in bytes
 * prefix  : path prefix (e.g., "" for root, "MISSIONS/" for subdir)
 * depth   : current recursion depth (0 at root) */
static int walk_directory(const iso_reader_source_t *src,
                          unsigned int dir_lba, unsigned int dir_size,
                          const char *prefix,
                          iso_file_list_t *out,
                          int depth)
{
	unsigned char sector[USER_DATA_SIZE];
	unsigned int bytes_read = 0;
	unsigned int sector_idx = 0;
	unsigned int sectors_needed;

	if (depth >= MAX_DIR_DEPTH) {
		ISO_LOG("Maximum directory depth %d exceeded", MAX_DIR_DEPTH);
		return -1;
	}

	sectors_needed = (dir_size + USER_DATA_SIZE - 1) / USER_DATA_SIZE;

	while (sector_idx < sectors_needed && bytes_read < dir_size) {
		unsigned int pos = 0;

		if (read_user_sector(src, (int) (dir_lba + sector_idx), sector) < 0) {
			ISO_LOG("Failed to read directory sector at LBA %u", dir_lba + sector_idx);
			return -1;
		}

		while (pos < USER_DATA_SIZE && bytes_read < dir_size) {
			unsigned char rec_len = sector[pos];
			int name_len;
			unsigned int extent_lba, data_size;
			unsigned char flags;
			char raw_name[256], clean_name[256], full_path[ISO_PATH_LEN];

			if (rec_len == 0) {
				/* Padding to sector boundary — advance to next sector */
				bytes_read += USER_DATA_SIZE - pos;
				break;
			}

			if (pos + rec_len > USER_DATA_SIZE) break;

			name_len = sector[pos + 32];
			extent_lba = le32(&sector[pos + 2]);
			data_size = le32(&sector[pos + 10]);
			flags = sector[pos + 25];

			/* Extract raw name */
			if (name_len > 0 && name_len < (int) sizeof(raw_name) &&
			    pos + 33 + name_len <= USER_DATA_SIZE) {
				memcpy(raw_name, &sector[pos + 33], name_len);
				raw_name[name_len] = '\0';
			} else {
				raw_name[0] = '\0';
			}

			/* Skip . and .. entries */
			if (name_len == 1 && (raw_name[0] == 0x00 || raw_name[0] == 0x01)) {
				pos += rec_len;
				bytes_read += rec_len;
				continue;
			}

			clean_iso_name(raw_name, name_len, clean_name, sizeof(clean_name));

			/* Build full path */
			if (join_iso_path(full_path, sizeof(full_path), prefix, clean_name) < 0) {
				ISO_LOG("Skipping overlong ISO path prefix=%s name=%s", prefix,
				        clean_name);
				pos += rec_len;
				bytes_read += rec_len;
				continue;
			}

			if (flags & DR_FLAG_DIRECTORY) {
				if (should_skip_iso_directory(clean_name)) {
					ISO_LOG("Skipping installer cruft directory %s", full_path);
					pos += rec_len;
					bytes_read += rec_len;
					continue;
				}
				/* Recurse into subdirectory */
				if (out->num_files < ISO_MAX_FILES) {
					iso_file_entry_t *e = &out->files[out->num_files];
					strncpy(e->path, full_path, ISO_PATH_LEN - 1);
					e->path[ISO_PATH_LEN - 1] = '\0';
					e->lba = extent_lba;
					e->size = data_size;
					e->is_dir = 1;
					out->num_files++;
				}
				walk_directory(src, extent_lba, data_size,
				               full_path, out, depth + 1);
			} else {
				/* Regular file */
				if (out->num_files < ISO_MAX_FILES) {
					iso_file_entry_t *e = &out->files[out->num_files];
					strncpy(e->path, full_path, ISO_PATH_LEN - 1);
					e->path[ISO_PATH_LEN - 1] = '\0';
					e->lba = extent_lba;
					e->size = data_size;
					e->is_dir = 0;
					out->num_files++;
					ISO_LOG("  File: %s  LBA=%u  size=%u", full_path, extent_lba, data_size);
				}
			}

			pos += rec_len;
			bytes_read += rec_len;
		}

		sector_idx++;
	}

	return 0;
}

/* ── Public API ──────────────────────────────────────────────────────── */

static int iso_list_files_from_source(const iso_reader_source_t *src,
                                      iso_file_list_t *out)
{
	unsigned char pvd[USER_DATA_SIZE];
	unsigned int root_lba, root_size;

	if (!src || src->fd < 0 || !out) return -1;

	memset(out, 0, sizeof(*out));

	/* Read Primary Volume Descriptor at logical sector 16 */
	if (read_user_sector(src, PVD_SECTOR, pvd) < 0) {
		ISO_LOG("Failed to read PVD at logical sector %d", PVD_SECTOR);
		return -1;
	}

	/* Validate: byte 0 = type (1 = primary), bytes 1-5 = "CD001" */
	if (pvd[0] != VD_PRIMARY ||
	    memcmp(&pvd[1], "CD001", 5) != 0) {
		ISO_LOG("Invalid PVD signature at logical sector %d", PVD_SECTOR);
		return -1;
	}

	ISO_LOG("Found ISO 9660 Primary Volume Descriptor");

	/* Root directory record is at PVD offset 156, 34 bytes */
	root_lba = le32(&pvd[156 + 2]);   /* extent location */
	root_size = le32(&pvd[156 + 10]); /* data length */

	ISO_LOG("Root directory: LBA=%u  size=%u", root_lba, root_size);

	/* Walk the directory tree */
	if (walk_directory(src, root_lba, root_size, "", out, 0) < 0)
		return -1;

	ISO_LOG("Listed %d entries total", out->num_files);
	return out->num_files;
}

static int iso_extract_files_from_source(const iso_reader_source_t *src,
                                         const iso_file_list_t *file_list,
                                         const char *output_dir,
                                         const char **extensions,
                                         iso_progress_fn progress,
                                         void *user_data)
{
	int i, extracted = 0;
	long long total_bytes = 0, done_bytes = 0;
	unsigned char sector[USER_DATA_SIZE];

	if (!src || src->fd < 0 || !file_list || !output_dir) return -1;

	/* Calculate total bytes for progress */
	for (i = 0; i < file_list->num_files; i++) {
		if (!file_list->files[i].is_dir &&
		    ext_matches(file_list->files[i].path, extensions))
			total_bytes += file_list->files[i].size;
	}

	for (i = 0; i < file_list->num_files; i++) {
		const iso_file_entry_t *entry = &file_list->files[i];
		char out_path[ISO_PATH_LEN * 2];
		int out_fd;

		/* Skip directories stored in the listing (created on demand) */
		if (entry->is_dir) continue;

		/* Filter by extension */
		if (!ext_matches(entry->path, extensions)) continue;

		/* Build output path */
		snprintf(out_path, sizeof(out_path), "%s/%s", output_dir, entry->path);

		/* Ensure parent directory exists */
		{
			char dir_path[ISO_PATH_LEN * 2];
			char *last_slash;
			strncpy(dir_path, out_path, sizeof(dir_path) - 1);
			dir_path[sizeof(dir_path) - 1] = '\0';
			last_slash = strrchr(dir_path, '/');
			if (last_slash) {
				*last_slash = '\0';
				mkdirs(dir_path);
			}
		}

		/* Open output file */
		out_fd = open(out_path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
		if (out_fd < 0) {
			ISO_LOG("Failed to create %s: %s", out_path, strerror(errno));
			continue;
		}

		ISO_LOG("Extracting %s (%u bytes)", entry->path, entry->size);

		/* Read file data sector by sector */
		{
			unsigned int remaining = entry->size;
			unsigned int lba = entry->lba;

			while (remaining > 0) {
				int to_write = (remaining > USER_DATA_SIZE) ? USER_DATA_SIZE : (int) remaining;

				if (read_user_sector(src, (int) lba, sector) < 0) {
					ISO_LOG("Read error at LBA %u for %s", lba, entry->path);
					break;
				}

				if (write(out_fd, sector, to_write) != to_write) {
					ISO_LOG("Write error for %s: %s", entry->path, strerror(errno));
					break;
				}

				remaining -= to_write;
				done_bytes += to_write;
				lba++;

				/* Progress callback */
				if (progress) {
					if (progress(entry->path, done_bytes, total_bytes, user_data) != 0) {
						close(out_fd);
						ISO_LOG("Extraction cancelled by user");
						return extracted;
					}
				}
			}
		}

		close(out_fd);
		extracted++;
	}

	ISO_LOG("Extracted %d files", extracted);
	return extracted;
}

int iso_list_files(int bin_fd, int track_start_sector, int track_num_sectors,
                   iso_file_list_t *out)
{
	iso_reader_source_t src;

	init_raw_track_source(&src, bin_fd, track_start_sector, track_num_sectors);
	return iso_list_files_from_source(&src, out);
}

int iso_list_image_files(int iso_fd, iso_file_list_t *out)
{
	iso_reader_source_t src;

	if (iso_fd < 0 || !out) return -1;

	init_iso_image_source(&src, iso_fd);
	return iso_list_files_from_source(&src, out);
}

int iso_extract_files(int bin_fd, int track_start_sector, int track_num_sectors,
                      const iso_file_list_t *file_list,
                      const char *output_dir,
                      const char **extensions,
                      iso_progress_fn progress, void *user_data)
{
	iso_reader_source_t src;

	init_raw_track_source(&src, bin_fd, track_start_sector, track_num_sectors);
	return iso_extract_files_from_source(&src, file_list, output_dir, extensions,
	                                     progress, user_data);
}

int iso_extract_image_files(int iso_fd,
                            const iso_file_list_t *file_list,
                            const char *output_dir,
                            const char **extensions,
                            iso_progress_fn progress,
                            void *user_data)
{
	iso_reader_source_t src;

	if (iso_fd < 0 || !file_list || !output_dir) return -1;

	init_iso_image_source(&src, iso_fd);
	return iso_extract_files_from_source(&src, file_list, output_dir, extensions,
	                                     progress, user_data);
}
