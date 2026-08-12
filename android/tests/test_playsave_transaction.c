#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "playsave_transaction.h"

static int failures;
static const char test_path[] = "test_playsave_transaction.tmp";

static void expect(int condition, const char *message)
{
	if (!condition) {
		printf("FAIL: %s\n", message);
		failures++;
	}
}

static int write_file(const unsigned char *data, size_t size)
{
	FILE *file = fopen(test_path, "wb");
	int ok = file && fwrite(data, 1, size, file) == size;

	if (file)
		fclose(file);
	return ok;
}

static int file_equals(const unsigned char *expected, size_t size)
{
	unsigned char *actual = (unsigned char *)malloc(size ? size : 1);
	FILE *file = fopen(test_path, "rb");
	long actual_size;
	int equal;

	if (!actual || !file) {
		free(actual);
		if (file)
			fclose(file);
		return 0;
	}
	fseek(file, 0, SEEK_END);
	actual_size = ftell(file);
	fseek(file, 0, SEEK_SET);
	equal = actual_size == (long)size &&
	        (!size || fread(actual, 1, size, file) == size) &&
	        !memcmp(actual, expected, size);
	fclose(file);
	free(actual);
	return equal;
}

int main(void)
{
	unsigned char original[64];
	unsigned char expected[64];
	unsigned char patch_data[] = { 9, 8, 7, 6 };
	struct playsave_file_patch patch = { 12, patch_data,
		                               sizeof(patch_data) };
	unsigned char *large;
	size_t i;

	for (i = 0; i < sizeof(original); i++)
		original[i] = (unsigned char)i;
	memcpy(expected, original, sizeof(expected));
	memcpy(expected + patch.offset, patch.data, patch.size);
	expect(write_file(original, sizeof(original)), "create fixture");
	expect(playsave_atomic_patch_file(test_path, &patch, 1),
	       "valid patch succeeds");
	expect(file_equals(expected, sizeof(expected)),
	       "valid patch changes only requested bytes");

	patch.offset = sizeof(original) - 1;
	patch.size = 4;
	expect(!playsave_atomic_patch_file(test_path, &patch, 1),
	       "short file patch is rejected");
	expect(file_equals(expected, sizeof(expected)),
	       "short file rejection preserves original");

	for (i = PLAYSAVE_TRANSACTION_FAIL_WRITE;
	     i <= PLAYSAVE_TRANSACTION_FAIL_REPLACE; i++) {
		expect(write_file(original, sizeof(original)), "reset failure fixture");
		patch.offset = 12;
		patch.size = sizeof(patch_data);
		playsave_transaction_set_test_failure((int)i);
		expect(!playsave_atomic_patch_file(test_path, &patch, 1),
		       "injected transaction failure is reported");
		expect(file_equals(original, sizeof(original)),
		       "injected failure preserves original");
		playsave_transaction_set_test_failure(
		    PLAYSAVE_TRANSACTION_FAIL_NONE);
	}

	large = (unsigned char *)malloc(65537);
	expect(large != NULL, "allocate large fixture");
	if (large) {
		unsigned int iteration;
		for (iteration = 0; iteration < 20; ++iteration) {
			for (i = 0; i < 65537; i++)
				large[i] = (unsigned char)(i * 31u + iteration);
			expect(playsave_atomic_replace_file(test_path, large, 65537),
			       "large atomic replacement succeeds");
			expect(file_equals(large, 65537),
			       "large replacement is not truncated");
		}
		free(large);
	}

	remove(test_path);
	return failures ? 1 : 0;
}
