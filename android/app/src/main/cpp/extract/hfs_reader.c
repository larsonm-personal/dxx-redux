/*
 * Read Apple partition maps and classic HFS volume metadata from raw Mode 1 CD tracks.
 * This is an AI-generated standalone implementation based on public HFS/APM
 * structure documentation and the general approach used by machfs (MIT).
 * No machfs code is copied here.
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#define close_fd(fd)               _close(fd)
#define mkdir_one(path, mode)      _mkdir(path)
#define open_fd(path, flags, mode) _open(path, flags, mode)
#define read_fd(fd, buf, n)        _read(fd, buf, (unsigned int) (n))
#define write_fd(fd, buf, n)       _write(fd, buf, (unsigned int) (n))
#define O_BINARY                   _O_BINARY
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#define close_fd(fd)               close(fd)
#define mkdir_one(path, mode)      mkdir((path), (mode))
#define open_fd(path, flags, mode) open((path), (flags), (mode))
#define write_fd(fd, buf, n)       write(fd, buf, (n))
#define O_BINARY                   0
#endif

#include "hfs_reader.h"
#include "extract_limits.h"

#ifdef ANDROID
#include <android/log.h>
#define HFS_LOG(...) __android_log_print(ANDROID_LOG_INFO, "HFSReader", __VA_ARGS__)
#else
#define HFS_LOG(...) ((void) 0)
#endif

#define RAW_SECTOR_SIZE  2352
#define USER_DATA_OFFSET 16
#define USER_DATA_SIZE   2048

#define DDR_SIG 0x4552
#define APM_SIG 0x504d
#define HFS_SIG 0x4244

#define HFS_MAX_BLOCK_SIZE       8192
#define HFS_MAX_MAP_ENTRIES      128
#define HFS_LOGICAL_BLOCK_SIZE   512
#define HFS_NODE_SIZE            512
#define HFS_NODE_DESCRIPTOR_SIZE 14
#define HFS_ROOT_PARENT_ID       1u
#define HFS_ROOT_DIR_ID          2u
#define HFS_MAX_DEPTH            32

typedef struct {
	unsigned int id;
	unsigned int parent_id;
	unsigned int data_size;
	unsigned int resource_size;
	hfs_extent_t data_extents[HFS_MAX_EXTENTS];
	hfs_extent_t resource_extents[HFS_MAX_EXTENTS];
	char name[HFS_NAME_LEN];
	int is_dir;
} hfs_catalog_entry_t;

typedef struct {
	int track_start_sector;
	int track_num_sectors;
	unsigned int physical_block_size;
	unsigned int partition_start_block;
	unsigned int partition_block_count;
	unsigned int allocation_block_size;
	unsigned int allocation_block_count;
	unsigned int first_allocation_block;
	unsigned int catalog_file_size;
	hfs_extent_t catalog_extents[HFS_MAX_EXTENTS];
	long long partition_offset;
	long long partition_end_offset;
	long long allocation_area_offset;
	char partition_name[HFS_PARTITION_TEXT_LEN];
	char partition_type[HFS_PARTITION_TEXT_LEN];
	char volume_name[HFS_VOLUME_NAME_LEN];
} hfs_volume_t;

struct hfs_catalog {
	int bin_fd;
	hfs_volume_t volume;
	hfs_catalog_entry_t *entries;
	int entry_count;
	int entry_capacity;
	hfs_file_list_t files;
	int file_capacity;
};

#ifdef HFS_READER_TESTING
static int hfs_test_allocations_before_failure = -1;
static int hfs_test_scan_count;
#endif

static int hfs_allocation_should_fail(void)
{
#ifdef HFS_READER_TESTING
	if (hfs_test_allocations_before_failure == 0)
		return 1;
	if (hfs_test_allocations_before_failure > 0)
		hfs_test_allocations_before_failure--;
#endif
	return 0;
}

static void *hfs_catalog_calloc(size_t count, size_t size)
{
	if (hfs_allocation_should_fail())
		return NULL;
	return calloc(count, size);
}

static void *hfs_catalog_realloc(void *ptr, size_t size)
{
	if (hfs_allocation_should_fail())
		return NULL;
	return realloc(ptr, size);
}

static unsigned int be16(const unsigned char *p)
{
	return ((unsigned int) p[0] << 8) | (unsigned int) p[1];
}

static unsigned int be32(const unsigned char *p)
{
	return ((unsigned int) p[0] << 24) |
	       ((unsigned int) p[1] << 16) |
	       ((unsigned int) p[2] << 8) |
	       (unsigned int) p[3];
}

static int read_at(int fd, long long offset, unsigned char *buf, int len)
{
#ifdef _WIN32
	if (_lseeki64(fd, offset, SEEK_SET) != offset)
		return -1;
	if (read_fd(fd, buf, len) != len)
		return -1;
	return 0;
#else
	if (pread(fd, buf, (size_t) len, (off_t) offset) != len)
		return -1;
	return 0;
#endif
}

static int read_track_bytes(int fd, int track_start_sector, int track_num_sectors,
                            long long track_offset, unsigned char *buf, int len)
{
	long long track_bytes;

	if (fd < 0 || !buf || len < 0 || track_start_sector < 0 || track_num_sectors <= 0)
		return -1;

	track_bytes = (long long) track_num_sectors * USER_DATA_SIZE;
	if (track_offset < 0 || track_offset > track_bytes ||
	    (long long) len > track_bytes - track_offset)
		return -1;

	while (len > 0) {
		int logical_sector = (int) (track_offset / USER_DATA_SIZE);
		int sector_offset = (int) (track_offset % USER_DATA_SIZE);
		int chunk = USER_DATA_SIZE - sector_offset;
		long long absolute_offset;

		if (chunk > len)
			chunk = len;

		absolute_offset = ((long long) track_start_sector + logical_sector) * RAW_SECTOR_SIZE +
		                  USER_DATA_OFFSET + sector_offset;
		if (read_at(fd, absolute_offset, buf, chunk) < 0)
			return -1;

		buf += chunk;
		track_offset += chunk;
		len -= chunk;
	}

	return 0;
}

static void copy_fixed_string(const unsigned char *src, int src_len,
                              char *dst, int dst_len)
{
	int len = 0;

	while (len < src_len && src[len] != '\0')
		len++;
	while (len > 0 && src[len - 1] == ' ')
		len--;
	if (len >= dst_len)
		len = dst_len - 1;
	memcpy(dst, src, (size_t) len);
	dst[len] = '\0';
}

static void copy_pstring(const unsigned char *src, int src_len,
                         char *dst, int dst_len)
{
	int len;

	if (src_len <= 0 || dst_len <= 0) {
		if (dst_len > 0)
			dst[0] = '\0';
		return;
	}

	len = src[0];
	if (len > src_len - 1)
		len = src_len - 1;
	if (len >= dst_len)
		len = dst_len - 1;
	memcpy(dst, src + 1, (size_t) len);
	dst[len] = '\0';
}

static int catalog_name_is_safe(const char *name)
{
	return name && name[0] && strcmp(name, ".") != 0 && strcmp(name, "..") != 0;
}

static int copy_catalog_name(const unsigned char *src, int src_len,
                             char *dst, int dst_len)
{
	int i;
	int out_len = 0;

	if (!src || !dst || src_len <= 0 || dst_len <= 0)
		return -1;

	for (i = 0; i < src_len && out_len < dst_len - 1; i++) {
		unsigned char c = src[i];

		if (c == '/' || c == '\\')
			dst[out_len++] = '_';
		else if (c >= 32 && c <= 126)
			dst[out_len++] = (char) c;
		else
			dst[out_len++] = '?';
	}
	dst[out_len] = '\0';
	return catalog_name_is_safe(dst) ? 0 : -1;
}

#ifdef HFS_READER_TESTING
int hfs_test_copy_catalog_name(const unsigned char *src, int src_len,
                               char *dst, int dst_len)
{
	return copy_catalog_name(src, src_len, dst, dst_len);
}
#endif

static void parse_extent_record(const unsigned char *p, hfs_extent_t extents[HFS_MAX_EXTENTS])
{
	int i;

	for (i = 0; i < HFS_MAX_EXTENTS; i++) {
		extents[i].start_block = be16(p + i * 4);
		extents[i].block_count = be16(p + i * 4 + 2);
	}
}

static int volume_allocation_bounds_valid(const hfs_volume_t *vol)
{
	long long partition_bytes;
	long long allocation_area_relative;
	long long allocation_bytes;

	if (!vol || vol->physical_block_size == 0 ||
	    vol->partition_block_count == 0 ||
	    vol->allocation_block_size == 0 ||
	    vol->allocation_block_count == 0)
		return -1;

	partition_bytes = (long long) vol->partition_block_count *
	                  vol->physical_block_size;
	if (vol->partition_offset < 0 ||
	    partition_bytes > LLONG_MAX - vol->partition_offset ||
	    vol->partition_end_offset != vol->partition_offset + partition_bytes)
		return -1;

	allocation_area_relative = (long long) vol->first_allocation_block *
	                           HFS_LOGICAL_BLOCK_SIZE;
	if (allocation_area_relative > partition_bytes ||
	    vol->allocation_area_offset !=
	        vol->partition_offset + allocation_area_relative)
		return -1;

	allocation_bytes = (long long) vol->allocation_block_count *
	                   vol->allocation_block_size;
	if (allocation_bytes > partition_bytes - allocation_area_relative)
		return -1;

	return 0;
}

static int partition_range_valid(const hfs_volume_t *vol,
                                 long long track_offset, long long len)
{
	if (!vol || track_offset < vol->partition_offset || len < 0 ||
	    track_offset > vol->partition_end_offset ||
	    len > vol->partition_end_offset - track_offset)
		return -1;
	return 0;
}

static int validate_extent_record(const hfs_volume_t *vol,
                                  const hfs_extent_t extents[HFS_MAX_EXTENTS])
{
	int i;

	if (volume_allocation_bounds_valid(vol) < 0 || !extents)
		return -1;

	for (i = 0; i < HFS_MAX_EXTENTS; i++) {
		unsigned int start_block = extents[i].start_block;
		unsigned int block_count = extents[i].block_count;
		long long extent_relative;
		long long extent_bytes;
		long long extent_offset;

		if (block_count == 0)
			continue;
		if (start_block > vol->allocation_block_count ||
		    block_count > vol->allocation_block_count - start_block)
			return -1;

		extent_relative = (long long) start_block * vol->allocation_block_size;
		extent_bytes = (long long) block_count * vol->allocation_block_size;
		if (extent_relative >
		    vol->partition_end_offset - vol->allocation_area_offset)
			return -1;
		extent_offset = vol->allocation_area_offset + extent_relative;
		if (extent_bytes > vol->partition_end_offset - extent_offset)
			return -1;
	}

	return 0;
}

static long long extent_record_capacity(const hfs_volume_t *vol,
                                        const hfs_extent_t extents[HFS_MAX_EXTENTS])
{
	long long total = 0;
	int i;

	if (validate_extent_record(vol, extents) < 0)
		return -1;

	for (i = 0; i < HFS_MAX_EXTENTS; i++)
		total += (long long) extents[i].block_count * vol->allocation_block_size;

	return total;
}

static int read_fork_bytes(int fd, const hfs_volume_t *vol,
                           const hfs_extent_t extents[HFS_MAX_EXTENTS],
                           long long logical_offset, unsigned char *buf, int len)
{
	int i;

	if (!vol || !buf || len < 0 || logical_offset < 0)
		return -1;
	if (validate_extent_record(vol, extents) < 0)
		return -1;

	for (i = 0; i < HFS_MAX_EXTENTS && len > 0; i++) {
		long long extent_bytes;
		long long chunk_offset;
		int chunk;
		long long track_offset;

		if (extents[i].block_count == 0)
			continue;

		extent_bytes = (long long) extents[i].block_count * vol->allocation_block_size;
		if (logical_offset >= extent_bytes) {
			logical_offset -= extent_bytes;
			continue;
		}

		chunk_offset = logical_offset;
		chunk = (int) ((extent_bytes - chunk_offset) < len ? (extent_bytes - chunk_offset) : len);
		track_offset = vol->allocation_area_offset +
		               (long long) extents[i].start_block * vol->allocation_block_size +
		               chunk_offset;
		if (partition_range_valid(vol, track_offset, chunk) < 0)
			return -1;
		if (read_track_bytes(fd, vol->track_start_sector, vol->track_num_sectors,
		                     track_offset, buf, chunk) < 0)
			return -1;

		buf += chunk;
		len -= chunk;
		logical_offset = 0;
	}

	return len == 0 ? 0 : -1;
}

static int mkdirs_for_file(const char *path)
{
	char tmp[HFS_PATH_LEN * 2];
	char *p;

	if (!path)
		return -1;

	snprintf(tmp, sizeof(tmp), "%s", path);
	p = strrchr(tmp, '/');
	if (!p)
		p = strrchr(tmp, '\\');
	if (!p)
		return 0;
	*p = '\0';

	for (p = tmp + 1; *p; p++) {
		if (*p == '/' || *p == '\\') {
			char saved = *p;
			*p = '\0';
			if (tmp[0] && mkdir_one(tmp, 0755) != 0 && errno != EEXIST)
				return -1;
			*p = saved;
		}
	}

	if (tmp[0] && mkdir_one(tmp, 0755) != 0 && errno != EEXIST)
		return -1;

	return 0;
}

static int path_equals_ignore_case(const char *a, const char *b)
{
	while (*a && *b) {
		char ca = *a;
		char cb = *b;

		if (ca >= 'A' && ca <= 'Z') ca = (char) (ca - 'A' + 'a');
		if (cb >= 'A' && cb <= 'Z') cb = (char) (cb - 'A' + 'a');
		if (ca != cb)
			return 0;
		a++;
		b++;
	}

	return *a == '\0' && *b == '\0';
}

static int find_catalog_entry_by_id(const hfs_catalog_entry_t *entries, int count, unsigned int id)
{
	int i;

	for (i = 0; i < count; i++) {
		if (entries[i].id == id)
			return i;
	}

	return -1;
}

static int add_catalog_entry(hfs_catalog_t *catalog,
                             const hfs_catalog_entry_t *entry)
{
	hfs_catalog_entry_t *grown;
	int new_capacity;
	int existing;

	if (!catalog || !entry)
		return -1;

	existing = find_catalog_entry_by_id(catalog->entries, catalog->entry_count, entry->id);
	if (existing >= 0)
		return -1;
	if ((unsigned int) catalog->entry_count >= DXX_EXTRACT_MAX_ENTRIES)
		return -1;
	if (catalog->entry_count == catalog->entry_capacity) {
		new_capacity = catalog->entry_capacity ? catalog->entry_capacity * 2 : 128;
		if ((unsigned int) new_capacity > DXX_EXTRACT_MAX_ENTRIES)
			new_capacity = (int) DXX_EXTRACT_MAX_ENTRIES;
		grown = (hfs_catalog_entry_t *) hfs_catalog_realloc(
		    catalog->entries, (size_t) new_capacity * sizeof(*grown));
		if (!grown)
			return -1;
		catalog->entries = grown;
		catalog->entry_capacity = new_capacity;
	}

	catalog->entries[catalog->entry_count] = *entry;
	return catalog->entry_count++;
}

static int build_catalog_path(const hfs_catalog_entry_t *entries, int count, int index,
                              char *out, int out_len, int depth)
{
	char parent_path[HFS_PATH_LEN];
	int parent_index;

	if (!entries || !out || out_len <= 0 || index < 0 || index >= count || depth > HFS_MAX_DEPTH)
		return -1;
	if (!catalog_name_is_safe(entries[index].name)) {
		out[0] = '\0';
		return -1;
	}

	if (entries[index].id == HFS_ROOT_DIR_ID) {
		out[0] = '\0';
		return 0;
	}

	if (entries[index].parent_id == HFS_ROOT_DIR_ID || entries[index].parent_id == HFS_ROOT_PARENT_ID) {
		snprintf(out, out_len, "%s", entries[index].name);
		return 0;
	}

	parent_index = find_catalog_entry_by_id(entries, count, entries[index].parent_id);
	if (parent_index < 0) {
		snprintf(out, out_len, "%s", entries[index].name);
		return -1;
	}

	if (build_catalog_path(entries, count, parent_index, parent_path, sizeof(parent_path), depth + 1) < 0 ||
	    !parent_path[0]) {
		snprintf(out, out_len, "%s", entries[index].name);
		return 0;
	}

	snprintf(out, out_len, "%s/%s", parent_path, entries[index].name);
	return 0;
}

static int output_has_path(const hfs_file_list_t *out, const char *path, int is_dir)
{
	int i;

	for (i = 0; i < out->num_files; i++) {
		if (out->files[i].is_dir == is_dir && strcmp(out->files[i].path, path) == 0)
			return 1;
	}

	return 0;
}

static int load_volume(int bin_fd, int track_start_sector, int track_num_sectors,
                       hfs_volume_t *vol, hfs_partition_info_t *info_out,
                       int require_catalog)
{
	unsigned char ddr[512];
	unsigned char mdb[512];
	unsigned char *entry_buf = NULL;
	unsigned int block_size;
	unsigned int total_blocks;
	unsigned int map_entries;
	unsigned long long max_blocks_in_track;
	unsigned int i;

	if (vol)
		memset(vol, 0, sizeof(*vol));
	if (info_out)
		memset(info_out, 0, sizeof(*info_out));

	if (!vol)
		return -1;

	if (read_track_bytes(bin_fd, track_start_sector, track_num_sectors, 0, ddr, sizeof(ddr)) < 0)
		return -1;
	if (be16(ddr) != DDR_SIG)
		return -1;

	block_size = be16(ddr + 2);
	total_blocks = be32(ddr + 4);
	if (block_size < 512 || block_size > HFS_MAX_BLOCK_SIZE || (block_size % 512) != 0)
		return -1;

	max_blocks_in_track =
	    (unsigned long long) track_num_sectors * USER_DATA_SIZE / block_size;
	if (max_blocks_in_track < 2 || total_blocks == 0)
		return -1;

	entry_buf = (unsigned char *) malloc(block_size);
	if (!entry_buf)
		return -1;

	if (read_track_bytes(bin_fd, track_start_sector, track_num_sectors,
	                     (long long) block_size, entry_buf, (int) block_size) < 0)
		goto fail;
	if (be16(entry_buf) != APM_SIG)
		goto fail;

	map_entries = be32(entry_buf + 4);
	if (map_entries == 0 || map_entries > max_blocks_in_track - 1 || map_entries > HFS_MAX_MAP_ENTRIES)
		goto fail;

	for (i = 1; i <= map_entries; i++) {
		unsigned int partition_start_block;
		unsigned int partition_block_count;
		long long partition_offset;
		long long partition_bytes;
		char partition_name[HFS_PARTITION_TEXT_LEN];
		char partition_type[HFS_PARTITION_TEXT_LEN];

		if (read_track_bytes(bin_fd, track_start_sector, track_num_sectors,
		                     (long long) i * block_size, entry_buf, (int) block_size) < 0)
			goto fail;
		if (be16(entry_buf) != APM_SIG)
			continue;

		partition_start_block = be32(entry_buf + 8);
		partition_block_count = be32(entry_buf + 12);
		copy_fixed_string(entry_buf + 16, 32, partition_name, sizeof(partition_name));
		copy_fixed_string(entry_buf + 48, 32, partition_type, sizeof(partition_type));

		if (strcmp(partition_type, "Apple_HFS") != 0)
			continue;
		if (partition_start_block >= max_blocks_in_track ||
		    partition_block_count == 0 ||
		    partition_block_count >
		        max_blocks_in_track - partition_start_block)
			goto fail;

		partition_offset = (long long) partition_start_block * block_size;
		partition_bytes = (long long) partition_block_count * block_size;
		if (partition_bytes < 1024 + (long long) sizeof(mdb))
			goto fail;
		if (read_track_bytes(bin_fd, track_start_sector, track_num_sectors,
		                     partition_offset + 1024, mdb, sizeof(mdb)) < 0)
			goto fail;
		if (be16(mdb) != HFS_SIG)
			goto fail;

		vol->track_start_sector = track_start_sector;
		vol->track_num_sectors = track_num_sectors;
		vol->physical_block_size = block_size;
		vol->partition_start_block = partition_start_block;
		vol->partition_block_count = partition_block_count;
		vol->allocation_block_size = be32(mdb + 20);
		vol->allocation_block_count = be16(mdb + 18);
		vol->first_allocation_block = be16(mdb + 28);
		vol->catalog_file_size = be32(mdb + 146);
		vol->partition_offset = partition_offset;
		vol->partition_end_offset = partition_offset + partition_bytes;
		vol->allocation_area_offset = partition_offset +
		                              (long long) vol->first_allocation_block *
		                                  HFS_LOGICAL_BLOCK_SIZE;
		copy_pstring(mdb + 36, 28, vol->volume_name, sizeof(vol->volume_name));
		memcpy(vol->partition_name, partition_name, sizeof(vol->partition_name));
		memcpy(vol->partition_type, partition_type, sizeof(vol->partition_type));
		parse_extent_record(mdb + 150, vol->catalog_extents);

		if (vol->allocation_block_size % 512 != 0 ||
		    volume_allocation_bounds_valid(vol) < 0 ||
		    extent_record_capacity(vol, vol->catalog_extents) <
		        vol->catalog_file_size)
			goto fail;

		if (info_out) {
			info_out->physical_block_size = block_size;
			info_out->total_blocks = total_blocks;
			info_out->map_entry_count = map_entries;
			info_out->partition_index = i;
			info_out->partition_start_block = partition_start_block;
			info_out->partition_block_count = partition_block_count;
			info_out->allocation_block_size = vol->allocation_block_size;
			info_out->first_allocation_block = vol->first_allocation_block;
			memcpy(info_out->partition_name, partition_name, sizeof(info_out->partition_name));
			memcpy(info_out->partition_type, partition_type, sizeof(info_out->partition_type));
			memcpy(info_out->volume_name, vol->volume_name, sizeof(info_out->volume_name));
		}

		if (require_catalog &&
		    (vol->catalog_file_size == 0 ||
		     vol->catalog_extents[0].block_count == 0))
			goto fail;

		HFS_LOG("Found Apple_HFS partition at block %u (%u blocks), volume '%s'",
		        partition_start_block, partition_block_count, vol->volume_name);
		free(entry_buf);
		return 0;
	}

fail:
	free(entry_buf);
	return -1;
}

static unsigned int catalog_node_record_offset(const unsigned char *node,
                                               unsigned int index)
{
	return be16(node + HFS_NODE_SIZE - 2u * (index + 1u));
}

static int validate_catalog_node_offsets(const unsigned char *node,
                                         unsigned int *record_count)
{
	unsigned int offset_table_start;
	unsigned int previous_offset = 0;
	unsigned int rec_count;
	unsigned int rec_index;

	if (!node || !record_count)
		return -1;

	rec_count = be16(node + 10);
	if (rec_count > (HFS_NODE_SIZE - HFS_NODE_DESCRIPTOR_SIZE) / 2u - 1u)
		return -1;
	offset_table_start = HFS_NODE_SIZE - 2u * (rec_count + 1u);
	for (rec_index = 0; rec_index <= rec_count; rec_index++) {
		unsigned int offset = catalog_node_record_offset(node, rec_index);

		if (offset < HFS_NODE_DESCRIPTOR_SIZE || offset > offset_table_start ||
		    (rec_index > 0 && offset <= previous_offset))
			return -1;
		previous_offset = offset;
	}
	*record_count = rec_count;
	return 0;
}

static int scan_catalog_node(hfs_catalog_t *catalog, const unsigned char *node)
{
	unsigned int rec_count;
	unsigned int rec_index;

	if (!catalog || !node)
		return -1;
	if (node[8] != 0xff)
		return 0;
	if (validate_catalog_node_offsets(node, &rec_count) < 0)
		return -1;

	for (rec_index = 0; rec_index < rec_count; rec_index++) {
		unsigned int rec_off = catalog_node_record_offset(node, rec_index);
		unsigned int rec_end = catalog_node_record_offset(node, rec_index + 1u);
		unsigned int record_size = rec_end - rec_off;
		unsigned int key_len;
		unsigned int name_len;
		unsigned int data_off;
		unsigned int payload_size;
		unsigned int record_type;
		hfs_catalog_entry_t entry;

		if (record_size < 7u)
			return -1;
		key_len = node[rec_off];
		if (key_len < 6u || key_len > 37u || 1u + key_len > record_size)
			return -1;
		name_len = node[rec_off + 6u];
		if (name_len > 31u || name_len + 6u > key_len)
			return -1;

		data_off = rec_off + 1u + key_len;
		if (data_off & 1u)
			data_off++;
		if (data_off >= rec_end)
			return -1;
		payload_size = rec_end - data_off;
		record_type = node[data_off];
		if ((record_type == 1u && payload_size < 10u) ||
		    (record_type == 2u && payload_size < 98u))
			return -1;
		if (record_type != 1u && record_type != 2u)
			continue;

		memset(&entry, 0, sizeof(entry));
		entry.parent_id = be32(node + rec_off + 2u);
		if (copy_catalog_name(node + rec_off + 7u, (int) name_len,
		                      entry.name, sizeof(entry.name)) < 0)
			return -1;

		if (record_type == 1u) {
			entry.id = be32(node + data_off + 6u);
			entry.is_dir = 1;
		} else {
			long long data_capacity;
			long long resource_capacity;

			entry.id = be32(node + data_off + 20u);
			entry.data_size = be32(node + data_off + 26u);
			entry.resource_size = be32(node + data_off + 36u);
			parse_extent_record(node + data_off + 74u, entry.data_extents);
			parse_extent_record(node + data_off + 86u, entry.resource_extents);
			data_capacity =
			    extent_record_capacity(&catalog->volume, entry.data_extents);
			resource_capacity =
			    extent_record_capacity(&catalog->volume, entry.resource_extents);
			if (data_capacity < entry.data_size ||
			    resource_capacity < entry.resource_size)
				return -1;
		}
		if (entry.id != 0 && add_catalog_entry(catalog, &entry) < 0)
			return -1;
	}

	return 0;
}

typedef int (*catalog_node_reader_fn)(void *context, unsigned int node_index,
                                      unsigned char node[HFS_NODE_SIZE]);

static int catalog_map_bit(const unsigned char *map, unsigned int node_index)
{
	return (map[node_index / 8u] & (0x80u >> (node_index % 8u))) != 0;
}

static void set_catalog_map_bit(unsigned char *map, unsigned int node_index)
{
	map[node_index / 8u] |= (unsigned char) (0x80u >> (node_index % 8u));
}

static int copy_catalog_map_record(const unsigned char *node,
                                   unsigned int record_index,
                                   unsigned char *allocation_map,
                                   unsigned int allocation_map_size,
                                   unsigned int *bytes_copied)
{
	unsigned int record_start;
	unsigned int record_end;
	unsigned int record_size;
	unsigned int remaining;

	if (!node || !allocation_map || !bytes_copied ||
	    *bytes_copied > allocation_map_size)
		return -1;
	record_start = catalog_node_record_offset(node, record_index);
	record_end = catalog_node_record_offset(node, record_index + 1u);
	record_size = record_end - record_start;
	remaining = allocation_map_size - *bytes_copied;
	if (record_size > remaining)
		record_size = remaining;
	memcpy(allocation_map + *bytes_copied, node + record_start, record_size);
	*bytes_copied += record_size;
	return 0;
}

static int scan_catalog_tree(hfs_catalog_t *catalog, unsigned int node_count,
                             catalog_node_reader_fn read_node, void *context)
{
	unsigned char node[HFS_NODE_SIZE];
	unsigned char *allocation_map = NULL;
	unsigned char *visited_nodes = NULL;
	unsigned int allocation_map_size;
	unsigned int allocated_nodes = 0;
	unsigned int bytes_copied = 0;
	unsigned int first_leaf;
	unsigned int free_nodes;
	unsigned int last_leaf;
	unsigned int leaf_records;
	unsigned int map_node;
	unsigned int node_index;
	unsigned int previous_node;
	unsigned int records_seen = 0;
	unsigned int root_node;
	unsigned int total_nodes;
	unsigned int tree_depth;
	unsigned int rec_count;
	int result = -1;

	if (!catalog || !read_node || node_count == 0)
		return -1;
	catalog->entry_count = 0;
#ifdef HFS_READER_TESTING
	hfs_test_scan_count++;
#endif

	if (read_node(context, 0, node) < 0 ||
	    node[8] != 1u || node[9] != 0u ||
	    be32(node + 4) != 0u ||
	    validate_catalog_node_offsets(node, &rec_count) < 0 ||
	    rec_count != 3u)
		return -1;
	if (catalog_node_record_offset(node, 1) -
	        catalog_node_record_offset(node, 0) <
	    30u)
		return -1;

	node_index = catalog_node_record_offset(node, 0);
	tree_depth = be16(node + node_index);
	root_node = be32(node + node_index + 2u);
	leaf_records = be32(node + node_index + 6u);
	first_leaf = be32(node + node_index + 10u);
	last_leaf = be32(node + node_index + 14u);
	if (be16(node + node_index + 18u) != HFS_NODE_SIZE)
		return -1;
	total_nodes = be32(node + node_index + 22u);
	free_nodes = be32(node + node_index + 26u);
	if (total_nodes != node_count || free_nodes > total_nodes)
		return -1;

	allocation_map_size = (total_nodes + 7u) / 8u;
	allocation_map = (unsigned char *) hfs_catalog_calloc(allocation_map_size, 1);
	visited_nodes = (unsigned char *) hfs_catalog_calloc(allocation_map_size, 1);
	if (!allocation_map || !visited_nodes)
		goto cleanup;
	if (copy_catalog_map_record(node, 2, allocation_map,
	                            allocation_map_size, &bytes_copied) < 0)
		goto cleanup;

	map_node = be32(node);
	previous_node = 0;
	while (bytes_copied < allocation_map_size) {
		unsigned int next_node;

		if (map_node == 0 || map_node >= total_nodes ||
		    catalog_map_bit(visited_nodes, map_node))
			goto cleanup;
		set_catalog_map_bit(visited_nodes, map_node);
		if (read_node(context, map_node, node) < 0 ||
		    node[8] != 2u || node[9] != 0u ||
		    be32(node + 4) != previous_node ||
		    validate_catalog_node_offsets(node, &rec_count) < 0 ||
		    rec_count != 1u ||
		    copy_catalog_map_record(node, 0, allocation_map,
		                            allocation_map_size, &bytes_copied) < 0)
			goto cleanup;
		next_node = be32(node);
		previous_node = map_node;
		map_node = next_node;
	}
	if (map_node != 0)
		goto cleanup;

	for (node_index = 0; node_index < total_nodes; node_index++) {
		if (catalog_map_bit(allocation_map, node_index))
			allocated_nodes++;
		if (catalog_map_bit(visited_nodes, node_index) &&
		    !catalog_map_bit(allocation_map, node_index))
			goto cleanup;
	}
	if (!catalog_map_bit(allocation_map, 0) ||
	    free_nodes != total_nodes - allocated_nodes)
		goto cleanup;
	if (leaf_records == 0) {
		if (tree_depth != 0 || root_node != 0 ||
		    first_leaf != 0 || last_leaf != 0)
			goto cleanup;
		result = 0;
		goto cleanup;
	}
	if (tree_depth == 0 || root_node == 0 || root_node >= total_nodes ||
	    first_leaf == 0 || first_leaf >= total_nodes ||
	    last_leaf == 0 || last_leaf >= total_nodes ||
	    !catalog_map_bit(allocation_map, root_node) ||
	    !catalog_map_bit(allocation_map, first_leaf) ||
	    !catalog_map_bit(allocation_map, last_leaf))
		goto cleanup;

	memset(visited_nodes, 0, allocation_map_size);
	node_index = first_leaf;
	previous_node = 0;
	while (node_index != 0) {
		unsigned int next_node;

		if (node_index >= total_nodes ||
		    !catalog_map_bit(allocation_map, node_index) ||
		    catalog_map_bit(visited_nodes, node_index))
			goto cleanup;
		set_catalog_map_bit(visited_nodes, node_index);
		if (read_node(context, node_index, node) < 0 ||
		    node[8] != 0xff || node[9] != 1u ||
		    be32(node + 4) != previous_node ||
		    validate_catalog_node_offsets(node, &rec_count) < 0 ||
		    rec_count > leaf_records - records_seen ||
		    scan_catalog_node(catalog, node) < 0)
			goto cleanup;
		records_seen += rec_count;
		next_node = be32(node);
		if ((node_index == last_leaf) != (next_node == 0))
			goto cleanup;
		previous_node = node_index;
		node_index = next_node;
	}
	if (previous_node != last_leaf || records_seen != leaf_records)
		goto cleanup;
	result = 0;

cleanup:
	free(visited_nodes);
	free(allocation_map);
	return result;
}

static int read_catalog_fork_node(void *context, unsigned int node_index,
                                  unsigned char node[HFS_NODE_SIZE])
{
	hfs_catalog_t *catalog = (hfs_catalog_t *) context;

	return read_fork_bytes(catalog->bin_fd, &catalog->volume,
	                       catalog->volume.catalog_extents,
	                       (long long) node_index * HFS_NODE_SIZE,
	                       node, HFS_NODE_SIZE);
}

static int scan_catalog(hfs_catalog_t *catalog)
{
	unsigned int node_count;

	if (!catalog || catalog->volume.catalog_file_size % HFS_NODE_SIZE != 0)
		return -1;
	node_count = catalog->volume.catalog_file_size / HFS_NODE_SIZE;
	return scan_catalog_tree(catalog, node_count, read_catalog_fork_node,
	                         catalog);
}

static int append_public_entry(hfs_catalog_t *catalog,
                               const hfs_catalog_entry_t *entry,
                               const char *path)
{
	hfs_file_entry_t *grown;
	hfs_file_entry_t *dst;
	int new_capacity;

	if (!catalog || !entry || !path)
		return -1;
	if ((unsigned int) catalog->files.num_files >= DXX_EXTRACT_MAX_ENTRIES)
		return -1;
	if (catalog->files.num_files == catalog->file_capacity) {
		new_capacity = catalog->file_capacity ? catalog->file_capacity * 2 : 128;
		if ((unsigned int) new_capacity > DXX_EXTRACT_MAX_ENTRIES)
			new_capacity = (int) DXX_EXTRACT_MAX_ENTRIES;
		grown = (hfs_file_entry_t *) hfs_catalog_realloc(
		    catalog->files.files, (size_t) new_capacity * sizeof(*grown));
		if (!grown)
			return -1;
		catalog->files.files = grown;
		catalog->file_capacity = new_capacity;
	}

	dst = &catalog->files.files[catalog->files.num_files++];
	memset(dst, 0, sizeof(*dst));
	snprintf(dst->path, sizeof(dst->path), "%s", path);
	dst->id = entry->id;
	dst->parent_id = entry->parent_id;
	dst->data_size = entry->data_size;
	dst->resource_size = entry->resource_size;
	memcpy(dst->data_extents, entry->data_extents, sizeof(dst->data_extents));
	memcpy(dst->resource_extents, entry->resource_extents, sizeof(dst->resource_extents));
	dst->is_dir = entry->is_dir;
	return 0;
}

static int build_public_catalog(hfs_catalog_t *catalog)
{
	int i;

	for (i = 0; i < catalog->entry_count; i++) {
		char path[HFS_PATH_LEN];

		if (catalog->entries[i].id == HFS_ROOT_DIR_ID)
			continue;
		if (build_catalog_path(catalog->entries, catalog->entry_count, i,
		                       path, sizeof(path), 0) < 0 &&
		    !path[0])
			continue;
		if (!path[0] || output_has_path(&catalog->files, path,
		                                catalog->entries[i].is_dir))
			continue;
		if (append_public_entry(catalog, &catalog->entries[i], path) < 0)
			return -1;
	}
	return 0;
}

int hfs_track_has_partition_map(int bin_fd, int track_start_sector, int track_num_sectors)
{
	unsigned char ddr[512];

	if (read_track_bytes(bin_fd, track_start_sector, track_num_sectors, 0, ddr, sizeof(ddr)) < 0)
		return 0;

	return be16(ddr) == DDR_SIG;
}

int hfs_find_partition(int bin_fd, int track_start_sector, int track_num_sectors,
                       hfs_partition_info_t *out)
{
	hfs_volume_t vol;

	return load_volume(bin_fd, track_start_sector, track_num_sectors, &vol, out, 0);
}

int hfs_list_files(int bin_fd, int track_start_sector, int track_num_sectors,
                   hfs_file_list_t *out)
{
	hfs_catalog_t *catalog;

	if (!out)
		return -1;

	memset(out, 0, sizeof(*out));
	if (hfs_catalog_open(bin_fd, track_start_sector, track_num_sectors, &catalog) < 0)
		return -1;
	*out = catalog->files;
	catalog->files.files = NULL;
	catalog->files.num_files = 0;
	hfs_catalog_close(catalog);
	return out->num_files;
}

void hfs_file_list_free(hfs_file_list_t *list)
{
	if (!list)
		return;
	free(list->files);
	memset(list, 0, sizeof(*list));
}

int hfs_catalog_open(int bin_fd, int track_start_sector, int track_num_sectors,
                     hfs_catalog_t **out)
{
	hfs_catalog_t *catalog;

	if (!out)
		return -1;
	*out = NULL;
	catalog = (hfs_catalog_t *) hfs_catalog_calloc(1, sizeof(*catalog));
	if (!catalog)
		return -1;
	catalog->bin_fd = bin_fd;
	if (load_volume(bin_fd, track_start_sector, track_num_sectors,
	                &catalog->volume, NULL, 1) < 0 ||
	    scan_catalog(catalog) < 0 || build_public_catalog(catalog) < 0) {
		hfs_catalog_close(catalog);
		return -1;
	}
	*out = catalog;
	return 0;
}

void hfs_catalog_close(hfs_catalog_t *catalog)
{
	if (!catalog)
		return;
	free(catalog->files.files);
	free(catalog->entries);
	free(catalog);
}

int hfs_catalog_file_count(const hfs_catalog_t *catalog)
{
	return catalog ? catalog->files.num_files : -1;
}

const hfs_file_entry_t *hfs_catalog_file_at(const hfs_catalog_t *catalog, int index)
{
	if (!catalog || index < 0 || index >= catalog->files.num_files)
		return NULL;
	return &catalog->files.files[index];
}

int hfs_catalog_extract_entry(hfs_catalog_t *catalog,
                              const hfs_file_entry_t *entry,
                              const char *output_path)
{
	unsigned char *buffer;
	int out_fd = -1;
	int entry_index;
	int entry_owned = 0;
	long long remaining;
	long long offset = 0;

	if (!catalog || !entry || entry->is_dir || !output_path)
		return -1;
	for (entry_index = 0; entry_index < catalog->files.num_files; entry_index++) {
		if (entry == &catalog->files.files[entry_index]) {
			entry_owned = 1;
			break;
		}
	}
	if (!entry_owned)
		return -1;
	if (entry->data_size > DXX_EXTRACT_MAX_ENTRY_BYTES ||
	    (long long) entry->data_size >
	        extent_record_capacity(&catalog->volume, entry->data_extents))
		return -1;
	buffer = (unsigned char *) hfs_catalog_calloc(1, 65536);
	if (!buffer)
		return -1;
	if (mkdirs_for_file(output_path) < 0)
		goto fail;

	out_fd = open_fd(output_path, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0644);
	if (out_fd < 0)
		goto fail;

	remaining = entry->data_size;
	while (remaining > 0) {
		int chunk = (int) (remaining > 65536 ? 65536 : remaining);

		if (read_fork_bytes(catalog->bin_fd, &catalog->volume,
		                    entry->data_extents, offset, buffer, chunk) < 0 ||
		    write_fd(out_fd, buffer, chunk) != chunk)
			goto fail;
		offset += chunk;
		remaining -= chunk;
	}

	if (close_fd(out_fd) < 0) {
		out_fd = -1;
		goto fail;
	}
	free(buffer);
	return entry->data_size;

fail:
	if (out_fd >= 0)
		close_fd(out_fd);
	remove(output_path);
	free(buffer);
	return -1;
}

int hfs_catalog_extract_path(hfs_catalog_t *catalog, const char *hfs_path,
                             const char *output_path)
{
	int i;

	if (!catalog || !hfs_path || !output_path)
		return -1;
	for (i = 0; i < catalog->files.num_files; i++) {
		const hfs_file_entry_t *entry = &catalog->files.files[i];
		if (!entry->is_dir && path_equals_ignore_case(entry->path, hfs_path))
			return hfs_catalog_extract_entry(catalog, entry, output_path);
	}
	return -1;
}

int hfs_extract_file(int bin_fd, int track_start_sector, int track_num_sectors,
                     const char *hfs_path, const char *output_path)
{
	hfs_catalog_t *catalog;
	int result;

	if (!hfs_path || !output_path)
		return -1;
	if (hfs_catalog_open(bin_fd, track_start_sector, track_num_sectors, &catalog) < 0)
		return -1;
	result = hfs_catalog_extract_path(catalog, hfs_path, output_path);
	hfs_catalog_close(catalog);
	return result;
}

#ifdef HFS_READER_TESTING
int hfs_test_dynamic_catalog_growth(int entry_count)
{
	hfs_catalog_t catalog;
	hfs_catalog_entry_t entry;
	int i;
	int result = 0;

	memset(&catalog, 0, sizeof(catalog));
	memset(&entry, 0, sizeof(entry));
	for (i = 0; i < entry_count; i++) {
		entry.id = (unsigned int) i + 1u;
		if (add_catalog_entry(&catalog, &entry) < 0) {
			result = -1;
			break;
		}
	}
	if (result == 0)
		result = catalog.entry_count;
	free(catalog.entries);
	return result;
}

void hfs_test_set_allocation_fail_after(int allocations)
{
	hfs_test_allocations_before_failure = allocations;
}

void hfs_test_reset_scan_count(void)
{
	hfs_test_scan_count = 0;
}

int hfs_test_get_scan_count(void)
{
	return hfs_test_scan_count;
}

static void init_test_volume(hfs_volume_t *vol)
{
	memset(vol, 0, sizeof(*vol));
	vol->physical_block_size = 512;
	vol->partition_block_count = 65535;
	vol->allocation_block_size = 512;
	vol->allocation_block_count = 65535;
	vol->partition_end_offset =
	    (long long) vol->partition_block_count * vol->physical_block_size;
}

int hfs_test_validate_extent_bounds(unsigned int partition_block_count,
                                    unsigned int physical_block_size,
                                    unsigned int first_allocation_block,
                                    unsigned int allocation_block_size,
                                    unsigned int allocation_block_count,
                                    unsigned int extent_start_block,
                                    unsigned int extent_block_count)
{
	hfs_volume_t vol;
	hfs_extent_t extents[HFS_MAX_EXTENTS];

	memset(&vol, 0, sizeof(vol));
	memset(extents, 0, sizeof(extents));
	vol.physical_block_size = physical_block_size;
	vol.partition_block_count = partition_block_count;
	vol.allocation_block_size = allocation_block_size;
	vol.allocation_block_count = allocation_block_count;
	vol.first_allocation_block = first_allocation_block;
	vol.partition_end_offset =
	    (long long) partition_block_count * physical_block_size;
	vol.allocation_area_offset =
	    (long long) first_allocation_block * HFS_LOGICAL_BLOCK_SIZE;
	extents[0].start_block = extent_start_block;
	extents[0].block_count = extent_block_count;
	return validate_extent_record(&vol, extents);
}

int hfs_test_validate_partition_read(unsigned int partition_block_count,
                                     unsigned int physical_block_size,
                                     long long partition_relative_offset,
                                     long long len)
{
	hfs_volume_t vol;

	memset(&vol, 0, sizeof(vol));
	vol.physical_block_size = physical_block_size;
	vol.partition_block_count = partition_block_count;
	vol.partition_offset = 4096;
	vol.partition_end_offset =
	    vol.partition_offset +
	    (long long) partition_block_count * physical_block_size;
	return partition_range_valid(
	    &vol, vol.partition_offset + partition_relative_offset, len);
}

int hfs_test_scan_catalog_node(const unsigned char *node, int node_size)
{
	hfs_catalog_t catalog;
	int result;

	if (!node || node_size != HFS_NODE_SIZE)
		return -1;
	memset(&catalog, 0, sizeof(catalog));
	init_test_volume(&catalog.volume);
	result = scan_catalog_node(&catalog, node);
	if (result == 0)
		result = catalog.entry_count;
	free(catalog.entries);
	return result;
}

typedef struct {
	const unsigned char *nodes;
	unsigned int node_count;
} hfs_test_catalog_tree_t;

static int read_test_catalog_node(void *context, unsigned int node_index,
                                  unsigned char node[HFS_NODE_SIZE])
{
	const hfs_test_catalog_tree_t *tree =
	    (const hfs_test_catalog_tree_t *) context;

	if (!tree || node_index >= tree->node_count)
		return -1;
	memcpy(node, tree->nodes + (size_t) node_index * HFS_NODE_SIZE,
	       HFS_NODE_SIZE);
	return 0;
}

int hfs_test_scan_catalog_tree(const unsigned char *nodes, int node_count,
                               char *first_name, int first_name_size)
{
	hfs_test_catalog_tree_t tree;
	hfs_catalog_t catalog;
	int result;

	if (!nodes || node_count <= 0 || !first_name || first_name_size <= 0)
		return -1;
	memset(&catalog, 0, sizeof(catalog));
	init_test_volume(&catalog.volume);
	tree.nodes = nodes;
	tree.node_count = (unsigned int) node_count;
	first_name[0] = '\0';
	result = scan_catalog_tree(&catalog, tree.node_count,
	                           read_test_catalog_node, &tree);
	if (result == 0) {
		result = catalog.entry_count;
		if (catalog.entry_count > 0)
			snprintf(first_name, (size_t) first_name_size, "%s",
			         catalog.entries[0].name);
	}
	free(catalog.entries);
	return result;
}
#endif
