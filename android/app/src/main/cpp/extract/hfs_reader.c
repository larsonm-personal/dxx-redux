/*
 * Read Apple partition maps and classic HFS volume metadata from raw Mode 1 CD tracks.
 * This is an AI-generated standalone implementation based on public HFS/APM
 * structure documentation and the general approach used by machfs (MIT).
 * No machfs code is copied here.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define read_fd(fd, buf, n) _read(fd, buf, (unsigned int) (n))
#else
#include <unistd.h>
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
	unsigned char ddr[512];
	unsigned char mdb[512];
	unsigned char *entry_buf = NULL;
	unsigned int block_size;
	unsigned int total_blocks;
	unsigned int map_entries;
	unsigned int max_blocks_in_track;
	unsigned int i;

	if (out)
		memset(out, 0, sizeof(*out));

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

		if (out) {
			out->physical_block_size = block_size;
			out->total_blocks = total_blocks;
			out->map_entry_count = map_entries;
			out->partition_index = i;
			out->partition_start_block = partition_start_block;
			out->partition_block_count = partition_block_count;
			out->allocation_block_size = be32(mdb + 20);
			out->first_allocation_block = be16(mdb + 28);
			memcpy(out->partition_name, partition_name, sizeof(out->partition_name));
			memcpy(out->partition_type, partition_type, sizeof(out->partition_type));
			copy_pstring(mdb + 36, 28, out->volume_name, sizeof(out->volume_name));
		}

		HFS_LOG("Found Apple_HFS partition at block %u (%u blocks), volume '%s'",
		        partition_start_block, partition_block_count,
		        out ? out->volume_name : "");
		free(entry_buf);
		return 0;
	}

fail:
	free(entry_buf);
	return -1;
}