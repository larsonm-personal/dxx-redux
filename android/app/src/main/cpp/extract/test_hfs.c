/*
 * test_hfs.c - Tests for Apple partition map and HFS volume detection.
 *
 * Runs a synthetic parser test and, when the sample Mac CD images are present,
 * probes the real D1 Mac discs under game_data/CD images.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define fd_from_file(fp) _fileno(fp)
#define open_bin(path)   _open(path, _O_RDONLY | _O_BINARY)
#define close_fd(fd)     _close(fd)
#define stat_file        _stat64
#define stat_t           struct __stat64
#else
#include <fcntl.h>
#include <unistd.h>
#define fd_from_file(fp) fileno(fp)
#define open_bin(path)   open(path, O_RDONLY)
#define close_fd(fd)     close(fd)
#define stat_file        stat
#define stat_t           struct stat
#endif

#include "cue_parser.h"
#include "hfs_reader.h"

#define RAW_SECTOR_SIZE    2352
#define USER_DATA_OFFSET   16
#define USER_DATA_SIZE     2048
#define HFS_TEST_NODE_SIZE 512

#define PRIMARY_CUE_PATH   "../../../../../../game_data/CD images/Descent - Mac macplay/Descent - Mac macplay.cue"
#define SECONDARY_CUE_PATH "../../../../../../game_data/CD images/d1 mac 2nd bin+cue/Descent [Mac].CUE"

static int tests_run;
static int tests_passed;
static int tests_skipped;

#define TEST(name)                \
	do {                          \
		tests_run++;              \
		printf("  %-50s ", name); \
		fflush(stdout);           \
	} while (0)

#define PASS()            \
	do {                  \
		tests_passed++;   \
		printf("PASS\n"); \
	} while (0)

#define SKIP(msg)                  \
	do {                           \
		tests_skipped++;           \
		printf("SKIP: %s\n", msg); \
	} while (0)

#define FAIL(msg)                  \
	do {                           \
		printf("FAIL: %s\n", msg); \
	} while (0)

static unsigned int be16(unsigned char hi, unsigned char lo)
{
	return ((unsigned int) hi << 8) | (unsigned int) lo;
}

static void put_be16(unsigned char *p, unsigned int value)
{
	p[0] = (unsigned char) ((value >> 8) & 0xff);
	p[1] = (unsigned char) (value & 0xff);
}

static void put_be32(unsigned char *p, unsigned int value)
{
	p[0] = (unsigned char) ((value >> 24) & 0xff);
	p[1] = (unsigned char) ((value >> 16) & 0xff);
	p[2] = (unsigned char) ((value >> 8) & 0xff);
	p[3] = (unsigned char) (value & 0xff);
}

static void write_track_bytes(unsigned char *raw, int raw_len,
                              int track_offset, const unsigned char *src, int len)
{
	while (len > 0) {
		int logical_sector = track_offset / USER_DATA_SIZE;
		int sector_offset = track_offset % USER_DATA_SIZE;
		int chunk = USER_DATA_SIZE - sector_offset;
		int absolute_offset;

		if (chunk > len)
			chunk = len;
		absolute_offset = logical_sector * RAW_SECTOR_SIZE + USER_DATA_OFFSET + sector_offset;
		if (absolute_offset + chunk > raw_len)
			return;
		memcpy(raw + absolute_offset, src, (size_t) chunk);
		src += chunk;
		track_offset += chunk;
		len -= chunk;
	}
}

static int file_exists(const char *path)
{
	stat_t st;
	return stat_file(path, &st) == 0;
}

static char *read_text_file(const char *path)
{
	FILE *f;
	long len;
	char *buf;

	f = fopen(path, "rb");
	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	buf = (char *) malloc((size_t) len + 1);
	if (!buf) {
		fclose(f);
		return NULL;
	}
	fread(buf, 1, (size_t) len, f);
	buf[len] = '\0';
	fclose(f);
	return buf;
}

static long long file_size(const char *path)
{
	stat_t st;
	if (stat_file(path, &st) != 0)
		return -1;
	return (long long) st.st_size;
}

static void path_dir(const char *path, char *dir, int dir_len)
{
	const char *last_sep = NULL;
	const char *p;

	for (p = path; *p; p++) {
		if (*p == '/' || *p == '\\')
			last_sep = p;
	}
	if (!last_sep) {
		dir[0] = '.';
		dir[1] = '\0';
		return;
	}
	if (last_sep - path >= dir_len)
		last_sep = path + dir_len - 1;
	memcpy(dir, path, (size_t) (last_sep - path));
	dir[last_sep - path] = '\0';
}

static void path_join(char *out, int out_len, const char *a, const char *b)
{
	snprintf(out, out_len, "%s/%s", a, b);
}

static const char *basename_only(const char *path)
{
	const char *last = path;
	const char *p;

	for (p = path; *p; p++) {
		if (*p == '/' || *p == '\\')
			last = p + 1;
	}

	return last;
}

static int str_equal_ignore_case(const char *a, const char *b)
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

static int read_file_prefix(const char *path, unsigned char *buf, int len)
{
	FILE *f;
	int n;

	f = fopen(path, "rb");
	if (!f)
		return -1;
	n = (int) fread(buf, 1, (size_t) len, f);
	fclose(f);
	return n;
}

typedef struct {
	int fd;
	int track_start_sector;
	int track_num_sectors;
} cue_data_track_t;

static int open_cue_data_track(const char *cue_path, cue_data_track_t *out)
{
	char cue_dir[1024];
	char bin_path[1024];
	char *cue_text;
	long long bin_sizes[CUE_MAX_FILES];
	cue_disc_t disc;
	int data_track_index = -1;
	int i;

	if (!out)
		return -1;
	memset(out, 0, sizeof(*out));
	out->fd = -1;

	if (!file_exists(cue_path))
		return -2;

	cue_text = read_text_file(cue_path);
	if (!cue_text)
		return -1;

	memset(&disc, 0, sizeof(disc));
	if (cue_parse(cue_text, NULL, 0, &disc) <= 0) {
		free(cue_text);
		return -1;
	}

	path_dir(cue_path, cue_dir, sizeof(cue_dir));
	for (i = 0; i < disc.num_files; i++) {
		path_join(bin_path, sizeof(bin_path), cue_dir, disc.files[i].filename);
		bin_sizes[i] = file_size(bin_path);
		if (bin_sizes[i] < 0) {
			free(cue_text);
			return -1;
		}
	}

	memset(&disc, 0, sizeof(disc));
	if (cue_parse(cue_text, bin_sizes, CUE_MAX_FILES, &disc) <= 0) {
		free(cue_text);
		return -1;
	}
	free(cue_text);

	for (i = 0; i < disc.num_tracks; i++) {
		if (disc.tracks[i].type == CUE_TRACK_DATA) {
			data_track_index = i;
			break;
		}
	}
	if (data_track_index < 0)
		return -1;

	path_join(bin_path, sizeof(bin_path), cue_dir, disc.files[disc.tracks[data_track_index].file_index].filename);
	out->fd = open_bin(bin_path);
	if (out->fd < 0)
		return -1;

	out->track_start_sector = disc.tracks[data_track_index].start_sector;
	out->track_num_sectors = disc.tracks[data_track_index].num_sectors;
	return 0;
}

static void close_cue_data_track(cue_data_track_t *track)
{
	if (track && track->fd >= 0) {
		close_fd(track->fd);
		track->fd = -1;
	}
}

static int load_hfs_list_from_cue(const char *cue_path,
                                  hfs_partition_info_t *info,
                                  hfs_file_list_t *list)
{
	cue_data_track_t track;
	int rc;

	rc = open_cue_data_track(cue_path, &track);
	if (rc < 0)
		return rc;

	if (info && hfs_find_partition(track.fd, track.track_start_sector, track.track_num_sectors, info) < 0) {
		close_cue_data_track(&track);
		return -1;
	}

	rc = hfs_list_files(track.fd, track.track_start_sector, track.track_num_sectors, list);
	close_cue_data_track(&track);
	return rc;
}

static const hfs_file_entry_t *find_entry_by_path(const hfs_file_list_t *list,
                                                  const char *path,
                                                  int is_dir)
{
	int i;

	for (i = 0; i < list->num_files; i++) {
		if (list->files[i].is_dir == is_dir && str_equal_ignore_case(list->files[i].path, path))
			return &list->files[i];
	}

	return NULL;
}

static const hfs_file_entry_t *find_entry_by_name_size(const hfs_file_list_t *list,
                                                       const char *name,
                                                       unsigned int size,
                                                       int is_dir)
{
	int i;

	for (i = 0; i < list->num_files; i++) {
		if (list->files[i].is_dir != is_dir)
			continue;
		if (size && list->files[i].data_size != size)
			continue;
		if (str_equal_ignore_case(basename_only(list->files[i].path), name))
			return &list->files[i];
	}

	return NULL;
}

static int build_synthetic_hfs_probe(void)
{
	unsigned char raw[RAW_SECTOR_SIZE * 4];
	unsigned char ddr[512];
	unsigned char apm[512];
	unsigned char mdb[512];
	hfs_partition_info_t info;
	FILE *tmp;
	int fd;

	memset(raw, 0, sizeof(raw));
	memset(ddr, 0, sizeof(ddr));
	memset(apm, 0, sizeof(apm));
	memset(mdb, 0, sizeof(mdb));

	put_be16(ddr + 0, be16(0x45, 0x52));
	put_be16(ddr + 2, 512);
	put_be32(ddr + 4, 16);
	write_track_bytes(raw, sizeof(raw), 0, ddr, sizeof(ddr));

	put_be16(apm + 0, be16(0x50, 0x4d));
	put_be32(apm + 4, 2);
	put_be32(apm + 8, 3);
	put_be32(apm + 12, 12);
	memcpy(apm + 16, "Descent", 7);
	memcpy(apm + 48, "Apple_HFS", 9);
	write_track_bytes(raw, sizeof(raw), 512, apm, sizeof(apm));

	put_be16(mdb + 0, be16(0x42, 0x44));
	put_be32(mdb + 20, 4096);
	put_be16(mdb + 28, 3);
	mdb[36] = 7;
	memcpy(mdb + 37, "Descent", 7);
	write_track_bytes(raw, sizeof(raw), 3 * 512 + 1024, mdb, sizeof(mdb));

	tmp = tmpfile();
	if (!tmp)
		return 0;
	if (fwrite(raw, 1, sizeof(raw), tmp) != sizeof(raw)) {
		fclose(tmp);
		return 0;
	}
	fflush(tmp);
	fd = fd_from_file(tmp);
#ifdef _WIN32
	_setmode(fd, _O_BINARY);
#endif

	if (!hfs_track_has_partition_map(fd, 0, 4)) {
		fclose(tmp);
		return 0;
	}
	if (hfs_find_partition(fd, 0, 4, &info) < 0) {
		fclose(tmp);
		return 0;
	}
	fclose(tmp);

	return info.physical_block_size == 512 &&
	       info.map_entry_count == 2 &&
	       info.partition_start_block == 3 &&
	       info.partition_block_count == 12 &&
	       strcmp(info.partition_type, "Apple_HFS") == 0 &&
	       strcmp(info.volume_name, "Descent") == 0;
}

static int catalog_name_safety_tests(void)
{
	static const unsigned char ordinary[] = "MacPlay Demos";
	static const unsigned char slash[] = "Mac/Play";
	static const unsigned char dot[] = ".";
	static const unsigned char dotdot[] = "..";
	char name[HFS_NAME_LEN];

	if (hfs_test_copy_catalog_name(ordinary, (int) sizeof(ordinary) - 1,
	                               name, sizeof(name)) < 0 ||
	    strcmp(name, "MacPlay Demos") != 0)
		return 0;
	if (hfs_test_copy_catalog_name(slash, (int) sizeof(slash) - 1,
	                               name, sizeof(name)) < 0 ||
	    strcmp(name, "Mac_Play") != 0)
		return 0;
	if (hfs_test_copy_catalog_name(dot, (int) sizeof(dot) - 1,
	                               name, sizeof(name)) == 0)
		return 0;
	if (hfs_test_copy_catalog_name(dotdot, (int) sizeof(dotdot) - 1,
	                               name, sizeof(name)) == 0)
		return 0;
	if (hfs_test_copy_catalog_name(ordinary, 0, name, sizeof(name)) == 0)
		return 0;

	return 1;
}

static void put_catalog_node_offset(unsigned char *node, unsigned int index,
                                    unsigned int offset)
{
	put_be16(node + HFS_TEST_NODE_SIZE - 2u * (index + 1u), offset);
}

static unsigned int build_catalog_test_record(unsigned char *node,
                                              unsigned int start,
                                              unsigned int record_type,
                                              unsigned int payload_size)
{
	unsigned int data_offset;

	node[start] = record_type == 1u || record_type == 2u ? 7 : 6;
	put_be32(node + start + 2u, 1);
	if (node[start] == 7) {
		node[start + 6u] = 1;
		node[start + 7u] = 'A';
	} else {
		node[start + 6u] = 0;
	}
	data_offset = start + 1u + node[start];
	if (data_offset & 1u)
		data_offset++;
	if (payload_size > 0)
		node[data_offset] = (unsigned char) record_type;
	if (record_type == 1u && payload_size >= 10u)
		put_be32(node + data_offset + 6u, 3);
	if (record_type == 2u && payload_size >= 98u)
		put_be32(node + data_offset + 20u, 3);
	return data_offset + payload_size;
}

static int scan_single_catalog_test_record(unsigned int record_type,
                                           unsigned int payload_size)
{
	unsigned char node[HFS_TEST_NODE_SIZE] = { 0 };
	unsigned int end;

	node[8] = 0xff;
	put_be16(node + 10, 1);
	end = build_catalog_test_record(node, 14, record_type, payload_size);
	put_catalog_node_offset(node, 0, 14);
	put_catalog_node_offset(node, 1, end);
	return hfs_test_scan_catalog_node(node, sizeof(node));
}

static int catalog_record_bounds_tests(void)
{
	unsigned char node[HFS_TEST_NODE_SIZE];
	unsigned int end;
	unsigned int i;

	for (i = 1; i < 10; i++) {
		if (scan_single_catalog_test_record(1, i) >= 0)
			return 0;
	}
	if (scan_single_catalog_test_record(1, 10) != 1)
		return 0;
	for (i = 1; i < 98; i++) {
		if (scan_single_catalog_test_record(2, i) >= 0)
			return 0;
	}
	if (scan_single_catalog_test_record(2, 98) != 1)
		return 0;

	memset(node, 0, sizeof(node));
	node[8] = 0xff;
	put_be16(node + 10, 47);
	end = 14;
	for (i = 0; i < 47; i++) {
		put_catalog_node_offset(node, i, end);
		end = build_catalog_test_record(node, end, 0, 1);
	}
	put_catalog_node_offset(node, 47, end);
	if (hfs_test_scan_catalog_node(node, sizeof(node)) != 0)
		return 0;

	memset(node, 0, sizeof(node));
	node[8] = 0xff;
	put_be16(node + 10, 48);
	end = 14;
	for (i = 0; i < 48; i++) {
		put_catalog_node_offset(node, i, end);
		end += end & 1u ? 8u : 9u;
	}
	put_catalog_node_offset(node, 48, end);
	if (hfs_test_scan_catalog_node(node, sizeof(node)) >= 0)
		return 0;

	memset(node, 0, sizeof(node));
	node[8] = 0xff;
	put_be16(node + 10, 249);
	if (hfs_test_scan_catalog_node(node, sizeof(node)) >= 0)
		return 0;

	memset(node, 0, sizeof(node));
	node[8] = 0xff;
	put_be16(node + 10, 2);
	put_catalog_node_offset(node, 0, 14);
	put_catalog_node_offset(node, 1, 30);
	put_catalog_node_offset(node, 2, 29);
	if (hfs_test_scan_catalog_node(node, sizeof(node)) >= 0)
		return 0;
	put_catalog_node_offset(node, 1, 14);
	put_catalog_node_offset(node, 2, 30);
	if (hfs_test_scan_catalog_node(node, sizeof(node)) >= 0)
		return 0;

	memset(node, 0, sizeof(node));
	node[8] = 0xff;
	put_be16(node + 10, 1);
	put_catalog_node_offset(node, 0, 14);
	put_catalog_node_offset(node, 1, 509);
	if (hfs_test_scan_catalog_node(node, sizeof(node)) >= 0)
		return 0;
	put_catalog_node_offset(node, 1, 20);
	if (hfs_test_scan_catalog_node(node, sizeof(node)) >= 0)
		return 0;

	memset(node, 0, sizeof(node));
	node[8] = 0xff;
	put_be16(node + 10, 1);
	node[14] = 7;
	node[20] = 2;
	put_catalog_node_offset(node, 0, 14);
	put_catalog_node_offset(node, 1, 22);
	return hfs_test_scan_catalog_node(node, sizeof(node)) < 0;
}

static void set_catalog_tree_allocated(unsigned char *header,
                                       unsigned int node_index)
{
	header[248u + node_index / 8u] |=
	    (unsigned char) (0x80u >> (node_index % 8u));
}

static void build_catalog_tree_header(unsigned char *nodes,
                                      unsigned int node_count,
                                      unsigned int allocated_mask,
                                      unsigned int first_leaf,
                                      unsigned int last_leaf,
                                      unsigned int leaf_records)
{
	unsigned char *header = nodes;
	unsigned int allocated_count = 0;
	unsigned int i;

	header[8] = 1;
	put_be16(header + 10, 3);
	put_be16(header + 14, first_leaf ? 1 : 0);
	put_be32(header + 16, first_leaf);
	put_be32(header + 20, leaf_records);
	put_be32(header + 24, first_leaf);
	put_be32(header + 28, last_leaf);
	put_be16(header + 32, HFS_TEST_NODE_SIZE);
	put_be16(header + 34, 37);
	put_be32(header + 36, node_count);
	for (i = 0; i < node_count; i++) {
		if (i < 32u && (allocated_mask & (1u << i))) {
			set_catalog_tree_allocated(header, i);
			allocated_count++;
		}
	}
	put_be32(header + 40, node_count - allocated_count);
	put_catalog_node_offset(header, 0, 14);
	put_catalog_node_offset(header, 1, 120);
	put_catalog_node_offset(header, 2, 248);
	put_catalog_node_offset(header, 3, 504);
}

static void build_catalog_tree_leaf(unsigned char *node,
                                    unsigned int forward_link,
                                    unsigned int backward_link,
                                    unsigned int id, const char *name)
{
	unsigned int data_offset;
	unsigned int end;
	unsigned int name_len = (unsigned int) strlen(name);

	put_be32(node, forward_link);
	put_be32(node + 4, backward_link);
	node[8] = 0xff;
	node[9] = 1;
	put_be16(node + 10, 1);
	node[14] = (unsigned char) (6u + name_len);
	put_be32(node + 16, 1);
	node[20] = (unsigned char) name_len;
	memcpy(node + 21, name, name_len);
	data_offset = 15u + node[14];
	if (data_offset & 1u)
		data_offset++;
	node[data_offset] = 1;
	put_be32(node + data_offset + 6u, id);
	end = data_offset + 10u;
	put_catalog_node_offset(node, 0, 14);
	put_catalog_node_offset(node, 1, end);
}

static void build_catalog_tree_map_node(unsigned char *node,
                                        unsigned int forward_link,
                                        unsigned int backward_link,
                                        unsigned char map_byte)
{
	put_be32(node, forward_link);
	put_be32(node + 4, backward_link);
	node[8] = 2;
	put_be16(node + 10, 1);
	node[14] = map_byte;
	put_catalog_node_offset(node, 0, 14);
	put_catalog_node_offset(node, 1, 15);
}

static int catalog_leaf_chain_tests(void)
{
	enum { TEST_NODE_COUNT = 6 };
	unsigned char nodes[TEST_NODE_COUNT * HFS_TEST_NODE_SIZE];
	unsigned char *large_nodes;
	char first_name[HFS_NAME_LEN];
	int result;

	memset(nodes, 0, sizeof(nodes));
	build_catalog_tree_header(nodes, TEST_NODE_COUNT, 0x07u, 1, 2, 2);
	build_catalog_tree_leaf(nodes + HFS_TEST_NODE_SIZE, 2, 0, 3, "A");
	build_catalog_tree_leaf(nodes + 2 * HFS_TEST_NODE_SIZE, 0, 1, 4, "B");
	if (hfs_test_scan_catalog_tree(nodes, TEST_NODE_COUNT,
	                               first_name, sizeof(first_name)) != 2 ||
	    strcmp(first_name, "A") != 0)
		return 0;

	memset(nodes, 0, sizeof(nodes));
	build_catalog_tree_header(nodes, TEST_NODE_COUNT, 0x05u, 2, 2, 1);
	build_catalog_tree_leaf(nodes + HFS_TEST_NODE_SIZE, 0, 0, 3, "Stale");
	build_catalog_tree_leaf(nodes + 2 * HFS_TEST_NODE_SIZE, 0, 0, 3, "Live");
	if (hfs_test_scan_catalog_tree(nodes, TEST_NODE_COUNT,
	                               first_name, sizeof(first_name)) != 1 ||
	    strcmp(first_name, "Live") != 0)
		return 0;

	memset(nodes, 0, sizeof(nodes));
	build_catalog_tree_header(nodes, TEST_NODE_COUNT, 0x01u, 1, 1, 1);
	build_catalog_tree_leaf(nodes + HFS_TEST_NODE_SIZE, 0, 0, 3, "Free");
	if (hfs_test_scan_catalog_tree(nodes, TEST_NODE_COUNT,
	                               first_name, sizeof(first_name)) >= 0)
		return 0;

	memset(nodes, 0, sizeof(nodes));
	build_catalog_tree_header(nodes, TEST_NODE_COUNT, 0x07u, 1, 2, 2);
	build_catalog_tree_leaf(nodes + HFS_TEST_NODE_SIZE, 2, 0, 3, "A");
	build_catalog_tree_leaf(nodes + 2 * HFS_TEST_NODE_SIZE, 1, 1, 4, "B");
	if (hfs_test_scan_catalog_tree(nodes, TEST_NODE_COUNT,
	                               first_name, sizeof(first_name)) >= 0)
		return 0;

	memset(nodes, 0, sizeof(nodes));
	build_catalog_tree_header(nodes, TEST_NODE_COUNT, 0x07u, 1, 2, 2);
	build_catalog_tree_leaf(nodes + HFS_TEST_NODE_SIZE, TEST_NODE_COUNT,
	                        0, 3, "A");
	build_catalog_tree_leaf(nodes + 2 * HFS_TEST_NODE_SIZE, 0, 1, 4, "B");
	if (hfs_test_scan_catalog_tree(nodes, TEST_NODE_COUNT,
	                               first_name, sizeof(first_name)) >= 0)
		return 0;

	memset(nodes, 0, sizeof(nodes));
	build_catalog_tree_header(nodes, TEST_NODE_COUNT, 0x07u, 1, 2, 2);
	build_catalog_tree_leaf(nodes + HFS_TEST_NODE_SIZE, 2, 0, 3, "A");
	build_catalog_tree_leaf(nodes + 2 * HFS_TEST_NODE_SIZE, 0, 1, 3, "B");
	if (hfs_test_scan_catalog_tree(nodes, TEST_NODE_COUNT,
	                               first_name, sizeof(first_name)) >= 0)
		return 0;

	large_nodes = (unsigned char *) calloc(2050u, HFS_TEST_NODE_SIZE);
	if (!large_nodes)
		return 0;
	build_catalog_tree_header(large_nodes, 2050, 0x0bu, 1, 1, 1);
	put_be32(large_nodes, 3);
	build_catalog_tree_leaf(large_nodes + HFS_TEST_NODE_SIZE, 0, 0, 3, "A");
	build_catalog_tree_map_node(large_nodes + 3 * HFS_TEST_NODE_SIZE, 0, 0, 0);
	result = hfs_test_scan_catalog_tree(large_nodes, 2050,
	                                    first_name, sizeof(first_name));
	free(large_nodes);
	return result == 1 && strcmp(first_name, "A") == 0;
}

static int probe_cue(const char *cue_path, hfs_partition_info_t *out)
{
	char cue_dir[1024];
	char bin_path[1024];
	char *cue_text;
	long long bin_sizes[CUE_MAX_FILES];
	cue_disc_t disc;
	int data_track_index = -1;
	int i;
	int fd;
	int rc;

	if (!file_exists(cue_path))
		return -2;

	cue_text = read_text_file(cue_path);
	if (!cue_text)
		return -1;

	memset(&disc, 0, sizeof(disc));
	if (cue_parse(cue_text, NULL, 0, &disc) <= 0) {
		free(cue_text);
		return -1;
	}

	path_dir(cue_path, cue_dir, sizeof(cue_dir));
	for (i = 0; i < disc.num_files; i++) {
		path_join(bin_path, sizeof(bin_path), cue_dir, disc.files[i].filename);
		bin_sizes[i] = file_size(bin_path);
		if (bin_sizes[i] < 0) {
			free(cue_text);
			return -1;
		}
	}

	memset(&disc, 0, sizeof(disc));
	if (cue_parse(cue_text, bin_sizes, CUE_MAX_FILES, &disc) <= 0) {
		free(cue_text);
		return -1;
	}
	free(cue_text);

	for (i = 0; i < disc.num_tracks; i++) {
		if (disc.tracks[i].type == CUE_TRACK_DATA) {
			data_track_index = i;
			break;
		}
	}
	if (data_track_index < 0)
		return -1;

	path_join(bin_path, sizeof(bin_path), cue_dir, disc.files[disc.tracks[data_track_index].file_index].filename);
	fd = open_bin(bin_path);
	if (fd < 0)
		return -1;

	rc = hfs_find_partition(fd,
	                        disc.tracks[data_track_index].start_sector,
	                        disc.tracks[data_track_index].num_sectors,
	                        out);
	close_fd(fd);
	return rc;
}

static int run_real_disc_test(const char *label, const char *cue_path,
                              const char *expected_volume,
                              unsigned int expected_blocks)
{
	hfs_partition_info_t info;
	TEST(label);
	if (!file_exists(cue_path)) {
		SKIP("sample media not present");
		return 1;
	}
	if (probe_cue(cue_path, &info) < 0) {
		FAIL("probe failed");
		return 0;
	}
	if (info.physical_block_size != 512) {
		FAIL("unexpected block size");
		return 0;
	}
	if (strcmp(info.partition_type, "Apple_HFS") != 0) {
		FAIL("expected Apple_HFS partition");
		return 0;
	}
	if (info.map_entry_count != 2) {
		FAIL("unexpected partition map count");
		return 0;
	}
	if (expected_volume && strcmp(info.volume_name, expected_volume) != 0) {
		FAIL("unexpected volume name");
		return 0;
	}
	if (expected_blocks && info.partition_block_count != expected_blocks) {
		FAIL("unexpected HFS partition size");
		return 0;
	}
	PASS();
	return 1;
}

static int run_primary_catalog_test(void)
{
	hfs_partition_info_t info;
	hfs_file_list_t list;
	const hfs_file_entry_t *install_entry;
	const hfs_file_entry_t *demos_dir;
	int valid;

	TEST("real_primary_macplay_catalog_listing");
	if (!file_exists(PRIMARY_CUE_PATH)) {
		SKIP("sample media not present");
		return 1;
	}
	if (load_hfs_list_from_cue(PRIMARY_CUE_PATH, &info, &list) < 0) {
		FAIL("listing failed");
		return 0;
	}
	if (info.physical_block_size != 512 || strcmp(info.volume_name, "Descent") != 0) {
		hfs_file_list_free(&list);
		FAIL("unexpected HFS volume metadata");
		return 0;
	}
	install_entry = find_entry_by_path(&list, "Install Descent", 0);
	demos_dir = find_entry_by_path(&list, "MacPlay Demos", 1);
	valid = install_entry && demos_dir && list.num_files >= 100;
	hfs_file_list_free(&list);
	if (!valid) {
		FAIL("expected catalog entries missing");
		return 0;
	}
	PASS();
	return 1;
}

static int run_primary_extract_tests(void)
{
	unsigned char prefix[8];
	cue_data_track_t track;
	hfs_catalog_t *catalog = NULL;
	const hfs_file_entry_t *install_entry = NULL;
	int bytes;
	int i;
	const char *install_out = "test_hfs_install_descent.bin";
	const char *install_out_2 = "test_hfs_install_descent_2.bin";

	TEST("real_primary_install_descent_extract");
	if (!file_exists(PRIMARY_CUE_PATH)) {
		SKIP("sample media not present");
		return 1;
	}
	if (open_cue_data_track(PRIMARY_CUE_PATH, &track) < 0) {
		FAIL("could not open MacPlay data track");
		return 0;
	}
	hfs_test_reset_scan_count();
	if (hfs_catalog_open(track.fd, track.track_start_sector,
	                     track.track_num_sectors, &catalog) < 0) {
		close_cue_data_track(&track);
		FAIL("catalog open failed");
		return 0;
	}
	for (i = 0; i < hfs_catalog_file_count(catalog); i++) {
		const hfs_file_entry_t *candidate = hfs_catalog_file_at(catalog, i);
		if (candidate && !candidate->is_dir &&
		    strcmp(candidate->path, "Install Descent") == 0) {
			install_entry = candidate;
			break;
		}
	}
	if (!install_entry ||
	    hfs_catalog_extract_entry(catalog, install_entry, install_out) < 0 ||
	    hfs_catalog_extract_entry(catalog, install_entry, install_out_2) < 0 ||
	    hfs_test_get_scan_count() != 1) {
		hfs_catalog_close(catalog);
		close_cue_data_track(&track);
		remove(install_out);
		remove(install_out_2);
		FAIL("extract failed");
		return 0;
	}
	hfs_catalog_close(catalog);
	close_cue_data_track(&track);
	bytes = read_file_prefix(install_out, prefix, 4);
	remove(install_out);
	remove(install_out_2);
	if (bytes != 4 || memcmp(prefix, "STi2", 4) != 0) {
		FAIL("missing STi2 magic");
		return 0;
	}
	PASS();
	return 1;
}

int main(void)
{
	int ok = 1;

	printf("Running HFS tests\n");

	TEST("synthetic_hfs_partition_probe");
	if (build_synthetic_hfs_probe())
		PASS();
	else {
		FAIL("synthetic probe mismatch");
		ok = 0;
	}

	TEST("catalog_component_path_safety");
	if (catalog_name_safety_tests())
		PASS();
	else {
		FAIL("unsafe catalog component accepted");
		ok = 0;
	}

	TEST("catalog_record_bounds");
	if (catalog_record_bounds_tests())
		PASS();
	else {
		FAIL("malformed catalog record accepted");
		ok = 0;
	}

	TEST("catalog_allocated_leaf_chain");
	if (catalog_leaf_chain_tests())
		PASS();
	else {
		FAIL("invalid or free catalog leaf accepted");
		ok = 0;
	}

	TEST("heap_catalog_growth_and_failures");
	hfs_test_set_allocation_fail_after(-1);
	if (hfs_test_dynamic_catalog_growth(1025) == 1025 &&
	    hfs_test_dynamic_catalog_growth(4096) == 4096 &&
	    hfs_test_dynamic_catalog_growth(4097) < 0) {
		hfs_test_set_allocation_fail_after(0);
		if (hfs_test_dynamic_catalog_growth(1) < 0)
			PASS();
		else {
			FAIL("allocation failure was not reported");
			ok = 0;
		}
	} else {
		FAIL("dynamic catalog growth bounds mismatch");
		ok = 0;
	}
	hfs_test_set_allocation_fail_after(-1);

	TEST("synthetic_non_hfs_track");
	{
		unsigned char raw[RAW_SECTOR_SIZE] = { 0 };
		FILE *tmp = tmpfile();
		int fd;
		if (!tmp || fwrite(raw, 1, sizeof(raw), tmp) != sizeof(raw)) {
			if (tmp)
				fclose(tmp);
			FAIL("could not create temp track");
			ok = 0;
		} else {
			fflush(tmp);
			fd = fd_from_file(tmp);
#ifdef _WIN32
			_setmode(fd, _O_BINARY);
#endif
			if (hfs_track_has_partition_map(fd, 0, 1) || hfs_find_partition(fd, 0, 1, NULL) == 0) {
				FAIL("unexpected HFS detection");
				ok = 0;
			} else {
				PASS();
			}
			fclose(tmp);
		}
	}

	if (!run_real_disc_test(
	        "real_primary_macplay_disc",
	        PRIMARY_CUE_PATH,
	        "Descent",
	        319989))
		ok = 0;

	if (!run_primary_catalog_test())
		ok = 0;

	if (!run_primary_extract_tests())
		ok = 0;

	if (!run_real_disc_test(
	        "real_secondary_mac_disc",
	        SECONDARY_CUE_PATH,
	        NULL,
	        319989))
		ok = 0;

	printf("\n%d/%d tests passed", tests_passed, tests_run);
	if (tests_skipped > 0)
		printf(" (%d skipped)", tests_skipped);
	printf("\n");

	return ok ? 0 : 1;
}
