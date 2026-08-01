#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define make_dir(path)   _mkdir(path)
#define remove_dir(path) _rmdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define make_dir(path)   mkdir(path, 0700)
#define remove_dir(path) rmdir(path)
#endif

#include "graphics_config_transaction.h"

static const char *test_paths[3] = {
	"graphics_config_transaction_test/root.cfg",
	"graphics_config_transaction_test/d1.cfg",
	"graphics_config_transaction_test/d2.cfg"
};

static void write_bytes(const char *path, const void *data, size_t size)
{
	FILE *file = fopen(path, "wb");
	assert(file);
	assert(!size || fwrite(data, 1, size, file) == size);
	assert(fclose(file) == 0);
}

static unsigned char *read_bytes(const char *path, size_t *size)
{
	FILE *file = fopen(path, "rb");
	unsigned char *data;
	long length;
	assert(file);
	assert(fseek(file, 0, SEEK_END) == 0);
	length = ftell(file);
	assert(length >= 0);
	assert(fseek(file, 0, SEEK_SET) == 0);
	data = (unsigned char *) malloc(length ? (size_t) length : 1);
	assert(data);
	assert(!length || fread(data, 1, (size_t) length, file) == (size_t) length);
	assert(fclose(file) == 0);
	*size = (size_t) length;
	return data;
}

static void assert_file_equals(const char *path, const void *expected, size_t expected_size)
{
	size_t actual_size;
	unsigned char *actual = read_bytes(path, &actual_size);
	assert(actual_size == expected_size);
	assert(!actual_size || !memcmp(actual, expected, actual_size));
	free(actual);
}

static void reset_three_files(void)
{
	static const char root[] = "RootOnly=1\nTexFilt=0\n";
	static const char d1[] = "D1Only=1\nTexFilt=0\n";
	static const char d2[] = "D2Only=1\nTexFilt=0\n";
	write_bytes(test_paths[0], root, sizeof(root) - 1);
	write_bytes(test_paths[1], d1, sizeof(d1) - 1);
	write_bytes(test_paths[2], d2, sizeof(d2) - 1);
}

static void test_boundaries_and_long_lines(void)
{
	const size_t sizes[] = { 32766, 32767, 32768, 65537 };
	size_t i;
	for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
		unsigned char *input = (unsigned char *) malloc(sizes[i]);
		unsigned char *expected = (unsigned char *) malloc(sizes[i]);
		assert(input && expected);
		memset(input, 'x', sizes[i]);
		memcpy(input, "TexFilt=0\n", 10);
		memcpy(expected, input, sizes[i]);
		memcpy(expected, "TexFilt=2\n", 10);
		write_bytes(test_paths[0], input, sizes[i]);
		assert(graphics_config_patch_files(test_paths, 1, "TexFilt", 2) ==
		       GRAPHICS_CONFIG_TRANSACTION_OK);
		assert_file_equals(test_paths[0], expected, sizes[i]);
		free(input);
		free(expected);
	}
}

static void test_duplicate_absent_and_unterminated_keys(void)
{
	static const char duplicates[] = "TexFilt=0\nUnknown=a\nTexFilt=1";
	static const char replaced[] = "TexFilt=2\nUnknown=a\nTexFilt=2\n";
	static const char absent[] = "Unknown=kept";
	static const char appended[] = "Unknown=kept\nTexFilt=2\n";

	write_bytes(test_paths[0], duplicates, sizeof(duplicates) - 1);
	assert(graphics_config_patch_files(test_paths, 1, "TexFilt", 2) ==
	       GRAPHICS_CONFIG_TRANSACTION_OK);
	assert_file_equals(test_paths[0], replaced, sizeof(replaced) - 1);
	write_bytes(test_paths[0], absent, sizeof(absent) - 1);
	assert(graphics_config_patch_files(test_paths, 1, "TexFilt", 2) ==
	       GRAPHICS_CONFIG_TRANSACTION_OK);
	assert_file_equals(test_paths[0], appended, sizeof(appended) - 1);
}

static void test_failure_preserves_every_target(int failure, size_t target)
{
	static const char root[] = "RootOnly=1\nTexFilt=0\n";
	static const char d1[] = "D1Only=1\nTexFilt=0\n";
	static const char d2[] = "D2Only=1\nTexFilt=0\n";

	reset_three_files();
	graphics_config_transaction_set_test_failure(failure, target);
	assert(graphics_config_patch_files(test_paths, 3, "TexFilt", 2) !=
	       GRAPHICS_CONFIG_TRANSACTION_OK);
	graphics_config_transaction_set_test_failure(GRAPHICS_CONFIG_TRANSACTION_FAIL_NONE, 0);
	assert_file_equals(test_paths[0], root, sizeof(root) - 1);
	assert_file_equals(test_paths[1], d1, sizeof(d1) - 1);
	assert_file_equals(test_paths[2], d2, sizeof(d2) - 1);
}

static void test_failures_and_rollback(void)
{
	int failure;
	for (failure = GRAPHICS_CONFIG_TRANSACTION_FAIL_READ;
	     failure <= GRAPHICS_CONFIG_TRANSACTION_FAIL_CLOSE; failure++)
		test_failure_preserves_every_target(failure, 1);
	test_failure_preserves_every_target(GRAPHICS_CONFIG_TRANSACTION_FAIL_REPLACE, 0);
	test_failure_preserves_every_target(GRAPHICS_CONFIG_TRANSACTION_FAIL_REPLACE, 1);
	test_failure_preserves_every_target(GRAPHICS_CONFIG_TRANSACTION_FAIL_REPLACE, 2);
}

static void test_success_updates_every_target(void)
{
	static const char root[] = "RootOnly=1\nTexFilt=2\n";
	static const char d1[] = "D1Only=1\nTexFilt=2\n";
	static const char d2[] = "D2Only=1\nTexFilt=2\n";

	reset_three_files();
	assert(graphics_config_patch_files(test_paths, 3, "TexFilt", 2) ==
	       GRAPHICS_CONFIG_TRANSACTION_OK);
	assert_file_equals(test_paths[0], root, sizeof(root) - 1);
	assert_file_equals(test_paths[1], d1, sizeof(d1) - 1);
	assert_file_equals(test_paths[2], d2, sizeof(d2) - 1);
}

static void test_size_limit_preserves_original(void)
{
	size_t size = GRAPHICS_CONFIG_MAX_FILE_SIZE + 1;
	unsigned char *input = (unsigned char *) malloc(size);
	assert(input);
	memset(input, 'z', size);
	write_bytes(test_paths[0], input, size);
	assert(graphics_config_patch_files(test_paths, 1, "TexFilt", 2) ==
	       GRAPHICS_CONFIG_TRANSACTION_TOO_LARGE);
	assert_file_equals(test_paths[0], input, size);
	free(input);
}

int main(void)
{
	remove(test_paths[0]);
	remove(test_paths[1]);
	remove(test_paths[2]);
	remove_dir("graphics_config_transaction_test");
	assert(make_dir("graphics_config_transaction_test") == 0);
	test_boundaries_and_long_lines();
	test_duplicate_absent_and_unterminated_keys();
	test_failures_and_rollback();
	test_success_updates_every_target();
	test_size_limit_preserves_original();
	remove(test_paths[0]);
	remove(test_paths[1]);
	remove(test_paths[2]);
	assert(remove_dir("graphics_config_transaction_test") == 0);
	printf("graphics config transaction tests passed\n");
	return 0;
}
