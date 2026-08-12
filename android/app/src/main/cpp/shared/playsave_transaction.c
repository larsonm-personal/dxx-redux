#include "playsave_transaction.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <windows.h>
#define playsave_fsync  _commit
#define playsave_fileno _fileno
#define playsave_getpid _getpid
#else
#include <unistd.h>
#define playsave_fsync  fsync
#define playsave_fileno fileno
#define playsave_getpid getpid
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define PLAYSAVE_TRANSACTION_MAX_FILE (16u * 1024u * 1024u)

#ifdef PLAYSAVE_TRANSACTION_TESTING
static int playsave_test_failure;

void playsave_transaction_set_test_failure(int failure)
{
	playsave_test_failure = failure;
}

static int playsave_should_fail(int failure)
{
	return playsave_test_failure == failure;
}
#else
static int playsave_should_fail(int failure)
{
	(void) failure;
	return 0;
}
#endif

static int playsave_replace_path(const char *temporary_path,
                                 const char *path)
{
	if (playsave_should_fail(PLAYSAVE_TRANSACTION_FAIL_REPLACE))
		return 0;
#ifdef _WIN32
	{
		unsigned int attempt;
		for (attempt = 0; attempt < 5; ++attempt) {
			DWORD error;
			if (MoveFileExA(temporary_path, path,
			                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
				return 1;
			error = GetLastError();
			if (error != ERROR_ACCESS_DENIED && error != ERROR_SHARING_VIOLATION &&
			    error != ERROR_LOCK_VIOLATION)
				break;
			Sleep(10u << attempt);
		}
		return 0;
	}
#else
	return rename(temporary_path, path) == 0;
#endif
}

int playsave_atomic_replace_file(const char *path, const void *data,
                                 size_t size)
{
	char temporary_path[PATH_MAX];
	FILE *file;
	int path_length;
	int ok = 0;

	if (!path || (!data && size) || size > PLAYSAVE_TRANSACTION_MAX_FILE)
		return 0;
	path_length = snprintf(temporary_path, sizeof(temporary_path),
	                       "%s.tmp.%ld", path, (long) playsave_getpid());
	if (path_length < 0 || (size_t) path_length >= sizeof(temporary_path))
		return 0;
	file = fopen(temporary_path, "wb");
	if (!file)
		return 0;
	if (playsave_should_fail(PLAYSAVE_TRANSACTION_FAIL_WRITE) ||
	    (size && fwrite(data, 1, size, file) != size))
		goto done;
	if (fflush(file) != 0 ||
	    playsave_should_fail(PLAYSAVE_TRANSACTION_FAIL_SYNC) ||
	    playsave_fsync(playsave_fileno(file)) != 0)
		goto done;
	if (fclose(file) != 0) {
		file = NULL;
		goto done;
	}
	file = NULL;
	if (!playsave_replace_path(temporary_path, path))
		goto done;
	ok = 1;

done:
	if (file)
		fclose(file);
	if (!ok)
		remove(temporary_path);
	return ok;
}

int playsave_atomic_patch_file(const char *path,
                               const struct playsave_file_patch *patches, size_t patch_count)
{
	unsigned char *data = NULL;
	FILE *file;
	long file_size;
	size_t i;
	int result = 0;

	if (!path || (!patches && patch_count))
		return 0;
	file = fopen(path, "rb");
	if (!file)
		return 0;
	if (fseek(file, 0, SEEK_END) != 0 ||
	    (file_size = ftell(file)) < 0 ||
	    (unsigned long) file_size > PLAYSAVE_TRANSACTION_MAX_FILE ||
	    fseek(file, 0, SEEK_SET) != 0)
		goto done;
	data = (unsigned char *) malloc(file_size ? (size_t) file_size : 1);
	if (!data || (file_size && fread(data, 1, (size_t) file_size, file) !=
	                               (size_t) file_size))
		goto done;
	if (fclose(file) != 0) {
		file = NULL;
		goto done;
	}
	file = NULL;
	for (i = 0; i < patch_count; i++) {
		if ((!patches[i].data && patches[i].size) ||
		    patches[i].offset > (size_t) file_size ||
		    patches[i].size > (size_t) file_size - patches[i].offset)
			goto done;
		memcpy(data + patches[i].offset, patches[i].data, patches[i].size);
	}
	result = playsave_atomic_replace_file(path, data, (size_t) file_size);

done:
	if (file)
		fclose(file);
	free(data);
	return result;
}
