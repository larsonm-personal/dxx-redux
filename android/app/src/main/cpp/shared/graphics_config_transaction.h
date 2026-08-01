#ifndef GRAPHICS_CONFIG_TRANSACTION_H
#define GRAPHICS_CONFIG_TRANSACTION_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GRAPHICS_CONFIG_MAX_TARGETS   3
#define GRAPHICS_CONFIG_MAX_FILE_SIZE (16u * 1024u * 1024u)

enum graphics_config_transaction_result {
	GRAPHICS_CONFIG_TRANSACTION_OK = 0,
	GRAPHICS_CONFIG_TRANSACTION_INVALID = 1,
	GRAPHICS_CONFIG_TRANSACTION_READ_FAILED = 2,
	GRAPHICS_CONFIG_TRANSACTION_TOO_LARGE = 3,
	GRAPHICS_CONFIG_TRANSACTION_ALLOC_FAILED = 4,
	GRAPHICS_CONFIG_TRANSACTION_WRITE_FAILED = 5,
	GRAPHICS_CONFIG_TRANSACTION_FLUSH_FAILED = 6,
	GRAPHICS_CONFIG_TRANSACTION_SYNC_FAILED = 7,
	GRAPHICS_CONFIG_TRANSACTION_CLOSE_FAILED = 8,
	GRAPHICS_CONFIG_TRANSACTION_REPLACE_FAILED = 9,
	GRAPHICS_CONFIG_TRANSACTION_ROLLBACK_FAILED = 10
};

enum graphics_config_transaction_result
graphics_config_patch_files(const char *const *paths, size_t path_count,
                            const char *key, int value);
const char *graphics_config_transaction_result_name(
    enum graphics_config_transaction_result result);

enum graphics_config_transaction_test_failure {
	GRAPHICS_CONFIG_TRANSACTION_FAIL_NONE = 0,
	GRAPHICS_CONFIG_TRANSACTION_FAIL_READ = 1,
	GRAPHICS_CONFIG_TRANSACTION_FAIL_ALLOC = 2,
	GRAPHICS_CONFIG_TRANSACTION_FAIL_WRITE = 3,
	GRAPHICS_CONFIG_TRANSACTION_FAIL_FLUSH = 4,
	GRAPHICS_CONFIG_TRANSACTION_FAIL_SYNC = 5,
	GRAPHICS_CONFIG_TRANSACTION_FAIL_CLOSE = 6,
	GRAPHICS_CONFIG_TRANSACTION_FAIL_REPLACE = 7
};
#ifdef GRAPHICS_CONFIG_TRANSACTION_TESTING
void graphics_config_transaction_set_test_failure(int failure, size_t target_index);
#endif

#ifdef __cplusplus
}
#endif

#endif
