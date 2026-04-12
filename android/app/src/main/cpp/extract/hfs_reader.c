/*
 * Read Apple partition maps and classic HFS volume metadata from raw Mode 1 CD tracks.
 * This is an AI-generated standalone implementation based on public HFS/APM
 * structure documentation and the general approach used by machfs (MIT).
 * No machfs code is copied here.
 */

#include <errno.h>
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

#define HFS_MAX_BLOCK_SIZE  8192
#define HFS_MAX_MAP_ENTRIES 128
#define HFS_NODE_SIZE       512
#define HFS_ROOT_PARENT_ID  1u
#define HFS_ROOT_DIR_ID     2u
#define HFS_MAX_DEPTH       32

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
	unsigned int first_allocation_block;
	unsigned int catalog_file_size;
	hfs_extent_t catalog_extents[HFS_MAX_EXTENTS];
	long long partition_offset;
	long long allocation_area_offset;
	char partition_name[HFS_PARTITION_TEXT_LEN];
	char partition_type[HFS_PARTITION_TEXT_LEN];
	char volume_name[HFS_VOLUME_NAME_LEN];
} hfs_volume_t;

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
	if (track_offset < 0 || track_offset + len > track_bytes)
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

static void copy_catalog_name(const unsigned char *src, int src_len,
                              char *dst, int dst_len)
{
	int i;
	int out_len = 0;

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
}

static void parse_extent_record(const unsigned char *p, hfs_extent_t extents[HFS_MAX_EXTENTS])
{
	int i;

	for (i = 0; i < HFS_MAX_EXTENTS; i++) {
		extents[i].start_block = be16(p + i * 4);
		extents[i].block_count = be16(p + i * 4 + 2);
	}
}

static long long extent_record_capacity(const hfs_volume_t *vol,
                                        const hfs_extent_t extents[HFS_MAX_EXTENTS])
{
	long long total = 0;
	int i;

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

static int add_catalog_entry(hfs_catalog_entry_t *entries, int *count,
                             const hfs_catalog_entry_t *entry)
{
	int existing;

	if (!entries || !count || !entry)
		return -1;

	existing = find_catalog_entry_by_id(entries, *count, entry->id);
	if (existing >= 0)
		return existing;
	if (*count >= HFS_MAX_FILES)
		return -1;

	entries[*count] = *entry;
	(*count)++;
	return *count - 1;
}

static int build_catalog_path(const hfs_catalog_entry_t *entries, int count, int index,
                              char *out, int out_len, int depth)
{
	char parent_path[HFS_PATH_LEN];
	int parent_index;

	if (!entries || !out || out_len <= 0 || index < 0 || index >= count || depth > HFS_MAX_DEPTH)
		return -1;

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
	unsigned int max_blocks_in_track;
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

	max_blocks_in_track = (unsigned int) (((long long) track_num_sectors * USER_DATA_SIZE) / block_size);
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
		    partition_start_block + partition_block_count > max_blocks_in_track)
			goto fail;

		partition_offset = (long long) partition_start_block * block_size;
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
		vol->first_allocation_block = be16(mdb + 28);
		vol->catalog_file_size = be32(mdb + 146);
		vol->partition_offset = partition_offset;
		vol->allocation_area_offset = partition_offset +
		                              (long long) vol->first_allocation_block * block_size;
		copy_pstring(mdb + 36, 28, vol->volume_name, sizeof(vol->volume_name));
		memcpy(vol->partition_name, partition_name, sizeof(vol->partition_name));
		memcpy(vol->partition_type, partition_type, sizeof(vol->partition_type));
		parse_extent_record(mdb + 150, vol->catalog_extents);

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
		    (vol->allocation_block_size == 0 ||
		     vol->catalog_file_size == 0 ||
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

static int scan_catalog(int bin_fd, const hfs_volume_t *vol,
                        hfs_catalog_entry_t *entries, int *entry_count)
{
	unsigned char node[HFS_NODE_SIZE];
	unsigned int node_count;
	unsigned int node_index;

	if (!vol || !entries || !entry_count)
		return -1;

	*entry_count = 0;
	node_count = vol->catalog_file_size / HFS_NODE_SIZE;
	if (node_count == 0)
		return -1;

	for (node_index = 0; node_index < node_count; node_index++) {
		unsigned int rec_count;
		unsigned int rec_index;

		if (read_fork_bytes(bin_fd, vol, vol->catalog_extents,
		                    (long long) node_index * HFS_NODE_SIZE,
		                    node, sizeof(node)) < 0)
			return -1;
		if (node[8] != 0xff)
			continue;

		rec_count = be16(node + 10);
		for (rec_index = 0; rec_index < rec_count; rec_index++) {
			unsigned int rec_off = be16(node + HFS_NODE_SIZE - 2 * (rec_index + 1));
			unsigned int key_len;
			unsigned int name_len;
			unsigned int parent_id;
			unsigned int data_off;
			hfs_catalog_entry_t entry;

			if (rec_off < 14 || rec_off >= HFS_NODE_SIZE - 2)
				continue;

			key_len = node[rec_off];
			if (key_len < 6 || key_len > 37 || rec_off + 1 + key_len > HFS_NODE_SIZE)
				continue;

			name_len = node[rec_off + 6];
			if (name_len > 31 || rec_off + 7 + name_len > HFS_NODE_SIZE || name_len + 6 > key_len)
				continue;

			parent_id = be32(node + rec_off + 2);
			data_off = rec_off + 1 + key_len;
			if (data_off & 1)
				data_off++;
			if (data_off + 2 > HFS_NODE_SIZE)
				continue;

			memset(&entry, 0, sizeof(entry));
			entry.parent_id = parent_id;
			copy_catalog_name(node + rec_off + 7, (int) name_len,
			                  entry.name, sizeof(entry.name));

			switch (node[data_off]) {
				case 1:
					entry.id = be32(node + data_off + 6);
					entry.is_dir = 1;
					if (entry.id != 0)
						add_catalog_entry(entries, entry_count, &entry);
					break;

				case 2:
					entry.id = be32(node + data_off + 20);
					entry.is_dir = 0;
					entry.data_size = be32(node + data_off + 26);
					entry.resource_size = be32(node + data_off + 36);
					parse_extent_record(node + data_off + 74, entry.data_extents);
					parse_extent_record(node + data_off + 86, entry.resource_extents);
					if (entry.id != 0)
						add_catalog_entry(entries, entry_count, &entry);
					break;

				default:
					break;
			}
		}
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
	hfs_catalog_entry_t entries[HFS_MAX_FILES];
	hfs_volume_t vol;
	int entry_count;
	int i;

	if (!out)
		return -1;

	memset(out, 0, sizeof(*out));
	if (load_volume(bin_fd, track_start_sector, track_num_sectors, &vol, NULL, 1) < 0)
		return -1;
	if (scan_catalog(bin_fd, &vol, entries, &entry_count) < 0)
		return -1;

	for (i = 0; i < entry_count; i++) {
		char path[HFS_PATH_LEN];
		hfs_file_entry_t *dst;

		if (entries[i].id == HFS_ROOT_DIR_ID)
			continue;
		if (build_catalog_path(entries, entry_count, i, path, sizeof(path), 0) < 0 && !path[0])
			continue;
		if (!path[0] || output_has_path(out, path, entries[i].is_dir))
			continue;
		if (out->num_files >= HFS_MAX_FILES)
			return -1;

		dst = &out->files[out->num_files++];
		memset(dst, 0, sizeof(*dst));
		snprintf(dst->path, sizeof(dst->path), "%s", path);
		dst->id = entries[i].id;
		dst->parent_id = entries[i].parent_id;
		dst->data_size = entries[i].data_size;
		dst->resource_size = entries[i].resource_size;
		memcpy(dst->data_extents, entries[i].data_extents, sizeof(dst->data_extents));
		memcpy(dst->resource_extents, entries[i].resource_extents, sizeof(dst->resource_extents));
		dst->is_dir = entries[i].is_dir;
	}

	return out->num_files;
}

int hfs_extract_file(int bin_fd, int track_start_sector, int track_num_sectors,
                     const char *hfs_path, const char *output_path)
{
	hfs_file_list_t list;
	hfs_volume_t vol;
	unsigned char buffer[65536];
	const hfs_file_entry_t *entry = NULL;
	int out_fd = -1;
	long long remaining;
	long long offset = 0;
	int i;

	if (!hfs_path || !output_path)
		return -1;

	if (load_volume(bin_fd, track_start_sector, track_num_sectors, &vol, NULL, 1) < 0)
		return -1;
	if (hfs_list_files(bin_fd, track_start_sector, track_num_sectors, &list) < 0)
		return -1;

	for (i = 0; i < list.num_files; i++) {
		if (!list.files[i].is_dir && path_equals_ignore_case(list.files[i].path, hfs_path)) {
			entry = &list.files[i];
			break;
		}
	}
	if (!entry)
		return -1;
	if ((long long) entry->data_size > extent_record_capacity(&vol, entry->data_extents))
		return -1;
	if (mkdirs_for_file(output_path) < 0)
		return -1;

	out_fd = open_fd(output_path, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0644);
	if (out_fd < 0)
		return -1;

	remaining = entry->data_size;
	while (remaining > 0) {
		int chunk = (int) (remaining > (long long) sizeof(buffer) ? sizeof(buffer) : remaining);

		if (read_fork_bytes(bin_fd, &vol, entry->data_extents, offset, buffer, chunk) < 0)
			goto fail;
		if (write_fd(out_fd, buffer, chunk) != chunk)
			goto fail;

		offset += chunk;
		remaining -= chunk;
	}

	close_fd(out_fd);
	return entry->data_size;

fail:
	if (out_fd >= 0)
		close_fd(out_fd);
	return -1;
}