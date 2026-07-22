#ifndef DXX_ANDROID_EXTRACT_STI2_EXTRACT_H
#define DXX_ANDROID_EXTRACT_STI2_EXTRACT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STI2_NAME_LEN    32
#define STI2_PATH_LEN    256
#define STI2_MAX_ENTRIES 128

#define STI2_EXTRACT_UNSUPPORTED_ENCRYPTION (-2)

typedef int (*sti2_progress_fn)(const char *current_file,
                                long long bytes_done,
                                long long bytes_total,
                                void *user_data);

typedef struct {
	char path[STI2_PATH_LEN];
	unsigned int header_offset;
	unsigned int data_offset;
	unsigned int compressed_size;
	unsigned int uncompressed_size;
	unsigned int resource_offset;
	unsigned int resource_compressed_size;
	unsigned int resource_uncompressed_size;
	unsigned int data_method;
	unsigned int resource_method;
	int data_encrypted;
	int resource_encrypted;
	unsigned int file_type;
	unsigned int creator;
	unsigned int finder_flags;
	int is_directory;
} sti2_entry_t;

typedef struct {
	sti2_entry_t entries[STI2_MAX_ENTRIES];
	int num_entries;
	unsigned int declared_file_count;
	unsigned int declared_total_size;
} sti2_entry_list_t;

int sti2_is_archive(const unsigned char *archive_data, size_t archive_size);

int sti2_list_entries(const unsigned char *archive_data, size_t archive_size,
                      sti2_entry_list_t *out);

int sti2_find_entry_index(const sti2_entry_list_t *list, const char *path);

int sti2_extract_entry(const unsigned char *archive_data, size_t archive_size,
                       const sti2_entry_t *entry, const char *output_path);

int sti2_extract_matching(const unsigned char *archive_data, size_t archive_size,
                          const char **extensions, const char *output_dir,
                          sti2_progress_fn progress, void *user_data);

#ifdef STI2_EXTRACT_TESTING
int sti2_test_method14_code_lengths(const unsigned char *lengths, unsigned int count);
#endif

#ifdef __cplusplus
}
#endif

#endif
