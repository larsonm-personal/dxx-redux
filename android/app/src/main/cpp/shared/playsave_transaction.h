#ifndef PLAYSAVE_TRANSACTION_H
#define PLAYSAVE_TRANSACTION_H

#include <stddef.h>

struct playsave_file_patch {
	size_t offset;
	const void *data;
	size_t size;
};

int playsave_atomic_replace_file(const char *path, const void *data,
                                 size_t size);
int playsave_atomic_patch_file(const char *path,
                               const struct playsave_file_patch *patches, size_t patch_count);

enum playsave_transaction_test_failure {
	PLAYSAVE_TRANSACTION_FAIL_NONE = 0,
	PLAYSAVE_TRANSACTION_FAIL_WRITE = 1,
	PLAYSAVE_TRANSACTION_FAIL_SYNC = 2,
	PLAYSAVE_TRANSACTION_FAIL_REPLACE = 3
};
#ifdef PLAYSAVE_TRANSACTION_TESTING
void playsave_transaction_set_test_failure(int failure);
#endif

#endif
