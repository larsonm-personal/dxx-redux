/*
 * Read Apple partition maps and classic HFS volume metadata from raw Mode 1 CD tracks.
 * This is an AI-generated standalone implementation based on public HFS/APM
 * structure documentation and the general approach used by machfs (MIT).
 * No machfs code is copied here.
 */

#ifndef HFS_READER_H
#define HFS_READER_H

#ifdef __cplusplus
extern "C" {
#endif

#define HFS_PARTITION_TEXT_LEN 33
#define HFS_VOLUME_NAME_LEN    28
#define HFS_NAME_LEN           32
#define HFS_PATH_LEN           256
#define HFS_MAX_EXTENTS        3

typedef struct {
	unsigned int start_block;
	unsigned int block_count;
} hfs_extent_t;

typedef struct {
	char path[HFS_PATH_LEN];
	unsigned int id;
	unsigned int parent_id;
	unsigned int data_size;
	unsigned int resource_size;
	hfs_extent_t data_extents[HFS_MAX_EXTENTS];
	hfs_extent_t resource_extents[HFS_MAX_EXTENTS];
	int is_dir;
} hfs_file_entry_t;

typedef struct {
	hfs_file_entry_t *files;
	int num_files;
} hfs_file_list_t;

typedef struct hfs_catalog hfs_catalog_t;

typedef struct {
	unsigned int physical_block_size;
	unsigned int total_blocks;
	unsigned int map_entry_count;
	unsigned int partition_index;
	unsigned int partition_start_block;
	unsigned int partition_block_count;
	unsigned int allocation_block_size;
	unsigned int first_allocation_block;
	char partition_name[HFS_PARTITION_TEXT_LEN];
	char partition_type[HFS_PARTITION_TEXT_LEN];
	char volume_name[HFS_VOLUME_NAME_LEN];
} hfs_partition_info_t;

/* Returns 1 if the track begins with an Apple driver descriptor record, else 0 */
int hfs_track_has_partition_map(int bin_fd, int track_start_sector, int track_num_sectors);

/* Returns 0 on success, -1 on parse failure or if no Apple_HFS partition is found */
int hfs_find_partition(int bin_fd, int track_start_sector, int track_num_sectors,
                       hfs_partition_info_t *out);

/* Returns the number found, or -1. Free a successful result with hfs_file_list_free. */
int hfs_list_files(int bin_fd, int track_start_sector, int track_num_sectors,
                   hfs_file_list_t *out);
void hfs_file_list_free(hfs_file_list_t *list);

/* Opens one owned volume/catalog context. The caller must close it. */
int hfs_catalog_open(int bin_fd, int track_start_sector, int track_num_sectors,
                     hfs_catalog_t **out);
void hfs_catalog_close(hfs_catalog_t *catalog);
int hfs_catalog_file_count(const hfs_catalog_t *catalog);
const hfs_file_entry_t *hfs_catalog_file_at(const hfs_catalog_t *catalog, int index);
int hfs_catalog_extract_entry(hfs_catalog_t *catalog,
                              const hfs_file_entry_t *entry,
                              const char *output_path);
int hfs_catalog_extract_path(hfs_catalog_t *catalog, const char *hfs_path,
                             const char *output_path);

/* Extracts a single HFS data-fork file by path. Returns bytes extracted, or -1 on failure */
int hfs_extract_file(int bin_fd, int track_start_sector, int track_num_sectors,
                     const char *hfs_path, const char *output_path);

#ifdef HFS_READER_TESTING
int hfs_test_copy_catalog_name(const unsigned char *src, int src_len,
                               char *dst, int dst_len);
int hfs_test_dynamic_catalog_growth(int entry_count);
void hfs_test_set_allocation_fail_after(int allocations);
void hfs_test_reset_scan_count(void);
int hfs_test_get_scan_count(void);
int hfs_test_scan_catalog_node(const unsigned char *node, int node_size);
int hfs_test_scan_catalog_tree(const unsigned char *nodes, int node_count,
                               char *first_name, int first_name_size);
#endif

#ifdef __cplusplus
}
#endif

#endif /* HFS_READER_H */
