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
#include <limits.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#define read(fd, buf, n) _read(fd, buf, (unsigned int) (n))
#define lseek            _lseek
#define close            _close
#define strcasecmp       _stricmp
#else
#include <unistd.h>
#endif

#include "iso9660_reader.h"

#include "extract_limits.h"
#include "physical_output_file.h"

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
#define DR_FLAG_DIRECTORY    0x02
#define DR_FLAG_MULTI_EXTENT 0x80

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

static int init_track_source(iso_reader_source_t *src,
                             int fd,
                             int track_start_sector,
                             int track_num_sectors,
                             int sector_stride,
                             int user_data_offset)
{
	memset(src, 0, sizeof(*src));
	if (fd < 0 || track_start_sector < 0 || track_num_sectors <= 0 ||
	    sector_stride < USER_DATA_SIZE || user_data_offset < 0 ||
	    user_data_offset > sector_stride - USER_DATA_SIZE)
		return -1;
	src->fd = fd;
	src->base_offset = (long long) track_start_sector * sector_stride;
	src->sector_stride = sector_stride;
	src->user_data_offset = user_data_offset;
	src->num_logical_sectors = track_num_sectors;
	return 0;
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

typedef struct {
	unsigned int extent_lba;
	unsigned int data_size;
	unsigned char flags;
	const unsigned char *name;
	unsigned char name_len;
	unsigned char record_len;
} iso_directory_record_t;

static int parse_directory_record(const unsigned char *record,
                                  size_t available,
                                  iso_directory_record_t *out)
{
	size_t record_len;
	size_t name_len;
	size_t required_len;

	if (!record || !out || available == 0)
		return -1;
	record_len = record[0];
	if (record_len < 34 || record_len > available)
		return -1;
	name_len = record[32];
	if (name_len > record_len - 33)
		return -1;
	required_len = 33 + name_len;
	if ((name_len & 1u) == 0) {
		if (required_len >= record_len || record[required_len] != 0)
			return -1;
		required_len++;
	}
	if (required_len > record_len)
		return -1;

	out->extent_lba = le32(record + 2);
	out->data_size = le32(record + 10);
	out->flags = record[25];
	out->name = record + 33;
	out->name_len = (unsigned char) name_len;
	out->record_len = (unsigned char) record_len;
	return 0;
}

static int iso_name_byte_is_reserved(unsigned char c)
{
	return c < 0x20 || c > 0x7e || c == '/' || c == '\\' ||
	       c == ':' || c == '"' || c == '<' || c == '>' || c == '|' ||
	       c == '?' || c == '*';
}

/* Decode one ISO identifier into one platform-neutral path component.
 * The pipe is reserved by the JNI listing protocol. */
static int clean_iso_name(const unsigned char *src, size_t src_len,
                          char *dst, size_t dst_len)
{
	size_t i;
	size_t output_len = 0;
	int in_version = 0;

	if (!src || !dst || dst_len == 0 || src_len == 0)
		return -1;
	for (i = 0; i < src_len; i++) {
		unsigned char c = src[i];
		if (iso_name_byte_is_reserved(c))
			return -1;
		if (c == ';') {
			in_version = 1;
			continue;
		}
		if (in_version)
			continue;
		if (output_len + 1 >= dst_len)
			return -1;
		dst[output_len++] = (char) tolower(c);
	}
	while (output_len > 0 && dst[output_len - 1] == '.')
		output_len--;
	if (output_len == 0 || dst[output_len - 1] == ' ')
		return -1;
	dst[output_len] = '\0';
	if ((output_len == 1 && dst[0] == '.') ||
	    (output_len == 2 && dst[0] == '.' && dst[1] == '.'))
		return -1;
	return 0;
}

static int iso_relative_path_is_safe(const char *path)
{
	const char *component;
	const char *p;

	if (!path || !memchr(path, '\0', ISO_PATH_LEN) || path[0] == '\0' ||
	    path[0] == '/' || path[0] == '\\')
		return 0;
	component = path;
	for (p = path;; p++) {
		unsigned char c = (unsigned char) *p;
		if (c == '/' || c == '\0') {
			size_t component_len = (size_t) (p - component);
			if (component_len == 0 ||
			    (component_len == 1 && component[0] == '.') ||
			    (component_len == 2 && component[0] == '.' &&
			     component[1] == '.') ||
			    component[component_len - 1] == ' ')
				return 0;
			if (c == '\0')
				return 1;
			component = p + 1;
		} else if (iso_name_byte_is_reserved(c)) {
			return 0;
		}
	}
}

static int join_output_path(char *out, size_t out_size,
                            const char *output_dir, const char *relative_path)
{
	size_t root_len;
	size_t relative_len;
	int needs_separator;

	if (!out || !output_dir || output_dir[0] == '\0' ||
	    !iso_relative_path_is_safe(relative_path))
		return -1;
	root_len = strlen(output_dir);
	relative_len = strlen(relative_path);
	needs_separator =
	    output_dir[root_len - 1] != '/' && output_dir[root_len - 1] != '\\';
	if (root_len >= out_size ||
	    relative_len >= out_size - root_len - (size_t) needs_separator)
		return -1;
	memcpy(out, output_dir, root_len);
	if (needs_separator)
		out[root_len++] = '/';
	memcpy(out + root_len, relative_path, relative_len + 1);
	return 0;
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

#define MAX_DIR_DEPTH             16
#define ISO_MAX_TRAVERSAL_SECTORS (ISO_MAX_FILES * 8)
#define ISO_MAX_TRAVERSAL_RECORDS (ISO_MAX_FILES * 16)

typedef struct {
	unsigned int visited_lbas[ISO_MAX_FILES + 1];
	uint64_t visited_end_lbas[ISO_MAX_FILES + 1];
	int num_visited;
	unsigned int sectors_remaining;
	unsigned int records_remaining;
} iso_traversal_context_t;

static int begin_directory(const iso_reader_source_t *src,
                           unsigned int dir_lba, unsigned int dir_size,
                           iso_traversal_context_t *context,
                           unsigned int *sectors_needed)
{
	uint64_t sector_count;
	uint64_t end_lba;
	int i;

	if (!src || !context || !sectors_needed || dir_size == 0 ||
	    src->num_logical_sectors <= 0)
		return -1;
	sector_count = (uint64_t) dir_size / USER_DATA_SIZE +
	               (dir_size % USER_DATA_SIZE != 0);
	end_lba = (uint64_t) dir_lba + sector_count;
	if (sector_count == 0 || sector_count > context->sectors_remaining ||
	    end_lba > (uint64_t) src->num_logical_sectors ||
	    end_lba > (uint64_t) INT_MAX + 1)
		return -1;
	for (i = 0; i < context->num_visited; i++)
		if ((uint64_t) dir_lba < context->visited_end_lbas[i] &&
		    (uint64_t) context->visited_lbas[i] < end_lba)
			return -1;
	if (context->num_visited >= ISO_MAX_FILES + 1)
		return -1;
	context->visited_lbas[context->num_visited] = dir_lba;
	context->visited_end_lbas[context->num_visited++] = end_lba;
	context->sectors_remaining -= (unsigned int) sector_count;
	*sectors_needed = (unsigned int) sector_count;
	return 0;
}

static int file_extent_is_valid(const iso_reader_source_t *src,
                                unsigned int lba, unsigned int size,
                                uint64_t *end_lba)
{
	uint64_t sector_count;
	uint64_t end;

	if (!src || src->num_logical_sectors <= 0 ||
	    lba >= (unsigned int) src->num_logical_sectors)
		return 0;
	sector_count = (uint64_t) size / USER_DATA_SIZE +
	               (size % USER_DATA_SIZE != 0);
	end = (uint64_t) lba + sector_count;
	if (end > (uint64_t) src->num_logical_sectors ||
	    end > (uint64_t) INT_MAX + 1)
		return 0;
	if (end_lba)
		*end_lba = end;
	return 1;
}

static int append_file_extent(const iso_reader_source_t *src,
                              iso_file_list_t *out, iso_file_entry_t *entry,
                              unsigned int lba, unsigned int size,
                              int has_more)
{
	uint64_t total_size;
	uint64_t end_lba;
	unsigned int i;

	if (!src || !out || !entry || out->num_extents < 0 ||
	    out->num_extents >= ISO_MAX_EXTENTS ||
	    (has_more && (size == 0 || size % USER_DATA_SIZE != 0)) ||
	    !file_extent_is_valid(src, lba, size, &end_lba))
		return -1;
	total_size = (uint64_t) entry->size + size;
	if (total_size > UINT_MAX)
		return -1;
	for (i = 0; i < entry->extent_count; i++) {
		const iso_file_extent_t *previous =
		    &out->extents[entry->first_extent + i];
		uint64_t previous_end;
		if (!file_extent_is_valid(src, previous->lba, previous->size,
		                          &previous_end))
			return -1;
		if ((uint64_t) lba < previous_end &&
		    (uint64_t) previous->lba < end_lba)
			return -1;
	}
	if (entry->extent_count == 0) {
		entry->first_extent = (unsigned int) out->num_extents;
		entry->lba = lba;
	} else if (entry->first_extent + entry->extent_count !=
	           (unsigned int) out->num_extents) {
		return -1;
	}
	out->extents[out->num_extents].lba = lba;
	out->extents[out->num_extents].size = size;
	out->num_extents++;
	entry->extent_count++;
	entry->size = (unsigned int) total_size;
	return 0;
}

#ifdef ISO9660_READER_TESTING
int iso_test_append_extent_sizes(unsigned int first_size,
                                 unsigned int second_size)
{
	iso_reader_source_t src;
	iso_file_list_t *list;
	iso_file_entry_t entry;
	int result;
	unsigned int second_lba =
	    1u + first_size / USER_DATA_SIZE +
	    (first_size % USER_DATA_SIZE != 0);

	memset(&src, 0, sizeof(src));
	memset(&entry, 0, sizeof(entry));
	list = iso_file_list_create();
	if (!list)
		return -1;
	src.num_logical_sectors = INT_MAX;
	if (append_file_extent(&src, list, &entry, 1, first_size, 1) < 0) {
		iso_file_list_destroy(list);
		return -1;
	}
	result = append_file_extent(
	    &src, list, &entry, second_lba, second_size, 0);
	iso_file_list_destroy(list);
	return result;
}
#endif

/* Recursively walk an ISO 9660 directory, appending entries to file_list.
 * dir_lba : LBA of the directory extent
 * dir_size: size of the directory extent in bytes
 * prefix  : path prefix (e.g., "" for root, "MISSIONS/" for subdir)
 * depth   : current recursion depth (0 at root) */
static int walk_directory(const iso_reader_source_t *src,
                          unsigned int dir_lba, unsigned int dir_size,
                          const char *prefix,
                          iso_file_list_t *out,
                          int depth,
                          iso_traversal_context_t *context)
{
	unsigned char sector[USER_DATA_SIZE];
	unsigned int bytes_read = 0;
	unsigned int sector_idx = 0;
	unsigned int sectors_needed;
	int pending_file_index = -1;
	unsigned char pending_name[256];
	unsigned char pending_name_len = 0;

	if (depth >= MAX_DIR_DEPTH) {
		ISO_LOG("Maximum directory depth %d exceeded", MAX_DIR_DEPTH);
		return -1;
	}

	if (begin_directory(src, dir_lba, dir_size, context,
	                    &sectors_needed) < 0) {
		ISO_LOG("Invalid or repeated ISO directory extent LBA=%u size=%u",
		        dir_lba, dir_size);
		return -1;
	}

	while (sector_idx < sectors_needed && bytes_read < dir_size) {
		unsigned int pos = 0;
		unsigned int sector_limit = dir_size - bytes_read;
		if (sector_limit > USER_DATA_SIZE)
			sector_limit = USER_DATA_SIZE;

		unsigned int sector_lba = dir_lba + sector_idx;
		if (read_user_sector(src, (int) sector_lba, sector) < 0) {
			ISO_LOG("Failed to read directory sector at LBA %u", sector_lba);
			return -1;
		}

		while (pos < sector_limit && bytes_read < dir_size) {
			unsigned char rec_len = sector[pos];
			iso_directory_record_t record;
			char clean_name[256], full_path[ISO_PATH_LEN];

			if (rec_len == 0) {
				/* Padding to sector boundary — advance to next sector */
				bytes_read += sector_limit - pos;
				break;
			}

			if (context->records_remaining == 0)
				return -1;
			context->records_remaining--;
			if (parse_directory_record(sector + pos, sector_limit - pos,
			                           &record) < 0)
				return -1;
			/* Skip . and .. entries */
			if (record.name_len == 1 &&
			    (record.name[0] == 0x00 || record.name[0] == 0x01)) {
				if (pending_file_index >= 0)
					return -1;
				pos += record.record_len;
				bytes_read += record.record_len;
				continue;
			}

			if (clean_iso_name(record.name, record.name_len, clean_name,
			                   sizeof(clean_name)) < 0) {
				ISO_LOG("Unsafe ISO identifier rejected");
				return -1;
			}

			/* Build full path */
			if (join_iso_path(full_path, sizeof(full_path), prefix, clean_name) < 0) {
				ISO_LOG("Overlong ISO path rejected");
				return -1;
			}

			if (record.flags & DR_FLAG_DIRECTORY) {
				if (pending_file_index >= 0 ||
				    (record.flags & DR_FLAG_MULTI_EXTENT))
					return -1;
				if (should_skip_iso_directory(clean_name)) {
					ISO_LOG("Skipping installer cruft directory %s", full_path);
					pos += record.record_len;
					bytes_read += record.record_len;
					continue;
				}
				if (out->num_files >= ISO_MAX_FILES) {
					ISO_LOG("ISO catalog exceeds %d entries", ISO_MAX_FILES);
					return -1;
				}
				/* Recurse into subdirectory */
				iso_file_entry_t *e = &out->files[out->num_files];
				strncpy(e->path, full_path, ISO_PATH_LEN - 1);
				e->path[ISO_PATH_LEN - 1] = '\0';
				e->lba = record.extent_lba;
				e->size = record.data_size;
				e->is_dir = 1;
				out->num_files++;
				if (walk_directory(src, record.extent_lba, record.data_size,
				                   full_path, out, depth + 1,
				                   context) < 0)
					return -1;
			} else {
				int has_more =
				    (record.flags & DR_FLAG_MULTI_EXTENT) != 0;
				iso_file_entry_t *e;

				if (pending_file_index >= 0) {
					if (pending_file_index >= out->num_files ||
					    pending_name_len != record.name_len ||
					    memcmp(pending_name, record.name,
					           record.name_len) != 0)
						return -1;
					e = &out->files[pending_file_index];
					if (strcmp(e->path, full_path) != 0)
						return -1;
				} else {
					if (out->num_files >= ISO_MAX_FILES) {
						ISO_LOG("ISO catalog exceeds %d entries",
						        ISO_MAX_FILES);
						return -1;
					}
					e = &out->files[out->num_files];
					memset(e, 0, sizeof(*e));
					strncpy(e->path, full_path, ISO_PATH_LEN - 1);
					e->path[ISO_PATH_LEN - 1] = '\0';
					e->is_dir = 0;
				}
				if (append_file_extent(src, out, e, record.extent_lba,
				                       record.data_size, has_more) < 0)
					return -1;
				if (pending_file_index < 0) {
					int file_index = out->num_files++;
					if (has_more) {
						pending_file_index = file_index;
						pending_name_len = record.name_len;
						memcpy(pending_name, record.name,
						       record.name_len);
					}
				} else if (!has_more) {
					pending_file_index = -1;
					pending_name_len = 0;
				}
				ISO_LOG("  File: %s  LBA=%u  size=%u", full_path,
				        record.extent_lba, e->size);
			}

			pos += record.record_len;
			bytes_read += record.record_len;
		}

		sector_idx++;
	}

	return pending_file_index < 0 ? 0 : -1;
}

/* ── Public API ──────────────────────────────────────────────────────── */

iso_file_list_t *iso_file_list_create(void)
{
	return (iso_file_list_t *) calloc(1, sizeof(iso_file_list_t));
}

void iso_file_list_destroy(iso_file_list_t *list)
{
	free(list);
}

static int iso_list_files_from_source(const iso_reader_source_t *src,
                                      iso_file_list_t *out)
{
	unsigned char pvd[USER_DATA_SIZE];
	iso_directory_record_t root_record;
	iso_traversal_context_t context;

	if (!src || src->fd < 0 || !out) return -1;

	memset(out, 0, sizeof(*out));
	memset(&context, 0, sizeof(context));
	context.sectors_remaining = ISO_MAX_TRAVERSAL_SECTORS;
	context.records_remaining = ISO_MAX_TRAVERSAL_RECORDS;

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

	/* Root directory record is embedded at PVD offset 156 */
	if (parse_directory_record(pvd + 156, sizeof(pvd) - 156,
	                           &root_record) < 0 ||
	    !(root_record.flags & DR_FLAG_DIRECTORY) ||
	    (root_record.flags & DR_FLAG_MULTI_EXTENT) ||
	    root_record.name_len > 1 ||
	    (root_record.name_len == 1 && root_record.name[0] != 0))
		return -1;

	ISO_LOG("Root directory: LBA=%u  size=%u",
	        root_record.extent_lba, root_record.data_size);

	/* Walk the directory tree */
	if (walk_directory(src, root_record.extent_lba, root_record.data_size,
	                   "", out, 0, &context) < 0) {
		memset(out, 0, sizeof(*out));
		return -1;
	}

	ISO_LOG("Listed %d entries total", out->num_files);
	return out->num_files;
}

static int file_entry_is_valid(const iso_reader_source_t *src,
                               const iso_file_list_t *file_list,
                               const iso_file_entry_t *entry)
{
	uint64_t total_size = 0;
	unsigned int i;

	if (!src || !file_list || !entry)
		return 0;
	if (entry->is_dir)
		return entry->extent_count == 0;
	if (entry->extent_count == 0 ||
	    entry->first_extent >= (unsigned int) file_list->num_extents ||
	    entry->extent_count >
	        (unsigned int) file_list->num_extents - entry->first_extent)
		return 0;
	for (i = 0; i < entry->extent_count; i++) {
		const iso_file_extent_t *extent =
		    &file_list->extents[entry->first_extent + i];
		uint64_t extent_end;
		unsigned int j;
		if (!file_extent_is_valid(src, extent->lba, extent->size,
		                          &extent_end) ||
		    (i + 1 < entry->extent_count &&
		     (extent->size == 0 || extent->size % USER_DATA_SIZE != 0)) ||
		    dxx_extract_add_bytes(&total_size, extent->size, UINT_MAX) < 0)
			return 0;
		for (j = 0; j < i; j++) {
			const iso_file_extent_t *previous =
			    &file_list->extents[entry->first_extent + j];
			uint64_t previous_end;
			if (!file_extent_is_valid(src, previous->lba,
			                          previous->size, &previous_end) ||
			    ((uint64_t) extent->lba < previous_end &&
			     (uint64_t) previous->lba < extent_end))
				return 0;
		}
	}
	return entry->lba == file_list->extents[entry->first_extent].lba &&
	       entry->size == total_size;
}

static int iso_extract_files_from_source(const iso_reader_source_t *src,
                                         const iso_file_list_t *file_list,
                                         const char *output_dir,
                                         const char **extensions,
                                         iso_progress_fn progress,
                                         void *user_data)
{
	int i, extracted = 0;
	uint64_t output_bytes = 0;
	long long total_bytes = 0, done_bytes = 0;
	unsigned char sector[USER_DATA_SIZE];

	if (!src || src->fd < 0 || !file_list || !output_dir ||
	    file_list->num_files < 0 || file_list->num_files > ISO_MAX_FILES ||
	    file_list->num_extents < 0 ||
	    file_list->num_extents > ISO_MAX_EXTENTS)
		return -1;

	/* Calculate total bytes for progress */
	for (i = 0; i < file_list->num_files; i++) {
		if (!iso_relative_path_is_safe(file_list->files[i].path) ||
		    !file_entry_is_valid(src, file_list, &file_list->files[i]))
			return -1;
		if (file_list->files[i].is_dir ||
		    !ext_matches(file_list->files[i].path, extensions))
			continue;
		if (!dxx_extract_entry_allowed(file_list->files[i].size,
		                               file_list->files[i].size) ||
		    dxx_extract_add_bytes(&output_bytes, file_list->files[i].size,
		                          DXX_EXTRACT_MAX_TOTAL_BYTES) < 0)
			return -1;
	}
	if (!dxx_extract_has_free_space(output_dir, output_bytes))
		return -1;
	total_bytes = (long long) output_bytes;

	for (i = 0; i < file_list->num_files; i++) {
		const iso_file_entry_t *entry = &file_list->files[i];
		char out_path[ISO_PATH_LEN * 2];
		dxx_physical_output_file_t output_file;
		int file_ok = 1;

		/* Skip directories stored in the listing (created on demand) */
		if (entry->is_dir) continue;

		/* Filter by extension */
		if (!ext_matches(entry->path, extensions)) continue;

		/* Build a destination whose suffix is a validated relative path. */
		if (join_output_path(out_path, sizeof(out_path), output_dir,
		                     entry->path) < 0)
			return -1;

		/* Walk from an owned root handle and reject every filesystem link */
		if (dxx_physical_output_open(&output_file, output_dir,
		                             entry->path) < 0) {
			ISO_LOG("Failed to create %s: %s", out_path, strerror(errno));
			return -1;
		}

		ISO_LOG("Extracting %s (%u bytes)", entry->path, entry->size);

		/* Read each file extent in directory-record order */
		for (unsigned int extent_index = 0;
		     extent_index < entry->extent_count; extent_index++) {
			const iso_file_extent_t *extent =
			    &file_list->extents[entry->first_extent + extent_index];
			unsigned int remaining = extent->size;
			unsigned int lba = extent->lba;

			while (remaining > 0) {
				int to_write =
				    (remaining > USER_DATA_SIZE) ? USER_DATA_SIZE
				                                 : (int) remaining;

				if (read_user_sector(src, (int) lba, sector) < 0) {
					ISO_LOG("Read error at LBA %u for %s", lba,
					        entry->path);
					file_ok = 0;
					break;
				}

				if (dxx_physical_output_write(&output_file, sector,
				                              (size_t) to_write) < 0) {
					ISO_LOG("Write error for %s: %s", entry->path,
					        strerror(errno));
					file_ok = 0;
					break;
				}

				remaining -= to_write;
				done_bytes += to_write;
				lba++;

				/* Progress callback */
				if (progress) {
					if (progress(entry->path, done_bytes, total_bytes,
					             user_data) != 0) {
						dxx_physical_output_abort(&output_file);
						ISO_LOG("Extraction cancelled by user");
						return DXX_EXTRACT_CANCELLED;
					}
				}
			}
			if (!file_ok)
				break;
		}

		if (file_ok && dxx_physical_output_finish(&output_file) < 0)
			file_ok = 0;
		if (!file_ok) {
			dxx_physical_output_abort(&output_file);
			return -1;
		}
		extracted++;
	}

	ISO_LOG("Extracted %d files", extracted);
	return extracted;
}

int iso_list_files(int bin_fd, int track_start_sector, int track_num_sectors,
                   iso_file_list_t *out)
{
	return iso_list_track_files(bin_fd, track_start_sector, track_num_sectors,
	                            RAW_SECTOR_SIZE, USER_DATA_OFFSET, out);
}

int iso_list_track_files(int bin_fd, int track_start_sector, int track_num_sectors,
                         int sector_stride, int user_data_offset,
                         iso_file_list_t *out)
{
	iso_reader_source_t src;

	if (init_track_source(&src, bin_fd, track_start_sector, track_num_sectors,
	                      sector_stride, user_data_offset) < 0)
		return -1;
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
	return iso_extract_track_files(bin_fd, track_start_sector, track_num_sectors,
	                               RAW_SECTOR_SIZE, USER_DATA_OFFSET, file_list,
	                               output_dir, extensions, progress, user_data);
}

int iso_extract_track_files(int bin_fd, int track_start_sector, int track_num_sectors,
                            int sector_stride, int user_data_offset,
                            const iso_file_list_t *file_list,
                            const char *output_dir,
                            const char **extensions,
                            iso_progress_fn progress, void *user_data)
{
	iso_reader_source_t src;

	if (init_track_source(&src, bin_fd, track_start_sector, track_num_sectors,
	                      sector_stride, user_data_offset) < 0)
		return -1;
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
