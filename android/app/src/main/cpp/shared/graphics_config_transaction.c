#include "graphics_config_transaction.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#include <windows.h>
#define config_close  _close
#define config_fdopen _fdopen
#define config_fileno _fileno
#define config_fsync  _commit
#define config_getpid _getpid
#define config_open(path) \
	_open(path, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE)
#else
#include <fcntl.h>
#include <unistd.h>
#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif
#define config_close      close
#define config_fdopen     fdopen
#define config_fileno     fileno
#define config_fsync      fsync
#define config_getpid     getpid
#define config_open(path) open(path, O_WRONLY | O_CREAT | O_EXCL, 0600)
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

struct staged_config {
	const char *path;
	unsigned char *original;
	size_t original_size;
	unsigned char *updated;
	size_t updated_size;
	char temporary_path[PATH_MAX];
	char backup_path[PATH_MAX];
	int existed;
	int published;
};

#ifdef GRAPHICS_CONFIG_TRANSACTION_TESTING
static int test_failure;
static size_t test_failure_target;

void graphics_config_transaction_set_test_failure(int failure, size_t target_index)
{
	test_failure = failure;
	test_failure_target = target_index;
}

static int should_fail(int failure, size_t target_index)
{
	return test_failure == failure && test_failure_target == target_index;
}
#else
static int should_fail(int failure, size_t target_index)
{
	(void) failure;
	(void) target_index;
	return 0;
}
#endif

static unsigned long next_unique_id(void)
{
#ifdef _WIN32
	static volatile LONG counter;
	return (unsigned long) InterlockedIncrement(&counter);
#else
	static unsigned long counter;
	return __sync_add_and_fetch(&counter, 1);
#endif
}

static void cleanup_staged(struct staged_config *staged, size_t count)
{
	size_t i;
	for (i = 0; i < count; i++) {
		if (staged[i].temporary_path[0])
			remove(staged[i].temporary_path);
		if (staged[i].backup_path[0])
			remove(staged[i].backup_path);
		free(staged[i].original);
		free(staged[i].updated);
	}
}

static enum graphics_config_transaction_result
read_complete_file(struct staged_config *staged, size_t target_index)
{
	FILE *file;
	long file_size;

	if (should_fail(GRAPHICS_CONFIG_TRANSACTION_FAIL_READ, target_index))
		return GRAPHICS_CONFIG_TRANSACTION_READ_FAILED;
	errno = 0;
	file = fopen(staged->path, "rb");
	if (!file) {
		if (errno == ENOENT)
			return GRAPHICS_CONFIG_TRANSACTION_OK;
		return GRAPHICS_CONFIG_TRANSACTION_READ_FAILED;
	}
	staged->existed = 1;
	if (fseek(file, 0, SEEK_END) != 0 || (file_size = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return GRAPHICS_CONFIG_TRANSACTION_READ_FAILED;
	}
	if ((unsigned long) file_size > GRAPHICS_CONFIG_MAX_FILE_SIZE) {
		fclose(file);
		return GRAPHICS_CONFIG_TRANSACTION_TOO_LARGE;
	}
	if (should_fail(GRAPHICS_CONFIG_TRANSACTION_FAIL_ALLOC, target_index)) {
		fclose(file);
		return GRAPHICS_CONFIG_TRANSACTION_ALLOC_FAILED;
	}
	staged->original = (unsigned char *) malloc(file_size ? (size_t) file_size : 1);
	if (!staged->original) {
		fclose(file);
		return GRAPHICS_CONFIG_TRANSACTION_ALLOC_FAILED;
	}
	staged->original_size = (size_t) file_size;
	if (staged->original_size &&
	    fread(staged->original, 1, staged->original_size, file) != staged->original_size) {
		fclose(file);
		return GRAPHICS_CONFIG_TRANSACTION_READ_FAILED;
	}
	if (fclose(file) != 0 || should_fail(GRAPHICS_CONFIG_TRANSACTION_FAIL_CLOSE, target_index))
		return GRAPHICS_CONFIG_TRANSACTION_CLOSE_FAILED;
	return GRAPHICS_CONFIG_TRANSACTION_OK;
}

static int line_has_key(const unsigned char *line, size_t line_size,
                        const char *key, size_t key_size)
{
	return line_size > key_size && !memcmp(line, key, key_size) && line[key_size] == '=';
}

static enum graphics_config_transaction_result
build_updated_file(struct staged_config *staged, const char *key, int value,
                   size_t target_index)
{
	char replacement[128];
	size_t replacement_size;
	size_t position = 0;
	size_t output_size = 0;
	size_t output_position = 0;
	size_t key_size = strlen(key);
	int replacement_length;
	int found = 0;

	replacement_length = snprintf(replacement, sizeof(replacement), "%s=%d\n", key, value);
	if (replacement_length < 0 || (size_t) replacement_length >= sizeof(replacement))
		return GRAPHICS_CONFIG_TRANSACTION_INVALID;
	replacement_size = (size_t) replacement_length;
	while (position < staged->original_size) {
		size_t line_end = position;
		while (line_end < staged->original_size && staged->original[line_end] != '\n')
			line_end++;
		if (line_end < staged->original_size)
			line_end++;
		if (line_has_key(staged->original + position, line_end - position, key, key_size)) {
			if (output_size > GRAPHICS_CONFIG_MAX_FILE_SIZE - replacement_size)
				return GRAPHICS_CONFIG_TRANSACTION_TOO_LARGE;
			output_size += replacement_size;
			found = 1;
		} else {
			if (output_size > GRAPHICS_CONFIG_MAX_FILE_SIZE - (line_end - position))
				return GRAPHICS_CONFIG_TRANSACTION_TOO_LARGE;
			output_size += line_end - position;
		}
		position = line_end;
	}
	if (!found) {
		size_t added_size = replacement_size;
		if (staged->original_size && staged->original[staged->original_size - 1] != '\n')
			added_size++;
		if (output_size > GRAPHICS_CONFIG_MAX_FILE_SIZE - added_size)
			return GRAPHICS_CONFIG_TRANSACTION_TOO_LARGE;
		output_size += added_size;
	}
	if (should_fail(GRAPHICS_CONFIG_TRANSACTION_FAIL_ALLOC, target_index))
		return GRAPHICS_CONFIG_TRANSACTION_ALLOC_FAILED;
	staged->updated = (unsigned char *) malloc(output_size ? output_size : 1);
	if (!staged->updated)
		return GRAPHICS_CONFIG_TRANSACTION_ALLOC_FAILED;
	staged->updated_size = output_size;
	position = 0;
	while (position < staged->original_size) {
		size_t line_end = position;
		while (line_end < staged->original_size && staged->original[line_end] != '\n')
			line_end++;
		if (line_end < staged->original_size)
			line_end++;
		if (line_has_key(staged->original + position, line_end - position, key, key_size)) {
			memcpy(staged->updated + output_position, replacement, replacement_size);
			output_position += replacement_size;
		} else {
			memcpy(staged->updated + output_position, staged->original + position,
			       line_end - position);
			output_position += line_end - position;
		}
		position = line_end;
	}
	if (!found) {
		if (staged->original_size && staged->original[staged->original_size - 1] != '\n')
			staged->updated[output_position++] = '\n';
		memcpy(staged->updated + output_position, replacement, replacement_size);
		output_position += replacement_size;
	}
	return output_position == output_size ? GRAPHICS_CONFIG_TRANSACTION_OK
	                                      : GRAPHICS_CONFIG_TRANSACTION_INVALID;
}

static enum graphics_config_transaction_result
write_private_file(const char *target_path, const char *tag, const unsigned char *data,
                   size_t size, size_t target_index, char *private_path,
                   size_t private_path_size)
{
	FILE *file;
	int fd;
	int attempt;
	int path_length;

	for (attempt = 0; attempt < 100; attempt++) {
		path_length = snprintf(private_path, private_path_size, "%s.%s.%ld.%lu",
		                       target_path, tag, (long) config_getpid(), next_unique_id());
		if (path_length < 0 || (size_t) path_length >= private_path_size)
			return GRAPHICS_CONFIG_TRANSACTION_INVALID;
		fd = config_open(private_path);
		if (fd >= 0)
			break;
		if (errno != EEXIST)
			return GRAPHICS_CONFIG_TRANSACTION_WRITE_FAILED;
	}
	if (attempt == 100)
		return GRAPHICS_CONFIG_TRANSACTION_WRITE_FAILED;
	file = config_fdopen(fd, "wb");
	if (!file) {
		config_close(fd);
		remove(private_path);
		private_path[0] = 0;
		return GRAPHICS_CONFIG_TRANSACTION_WRITE_FAILED;
	}
	if (should_fail(GRAPHICS_CONFIG_TRANSACTION_FAIL_WRITE, target_index) ||
	    (size && fwrite(data, 1, size, file) != size)) {
		fclose(file);
		remove(private_path);
		private_path[0] = 0;
		return GRAPHICS_CONFIG_TRANSACTION_WRITE_FAILED;
	}
	if (should_fail(GRAPHICS_CONFIG_TRANSACTION_FAIL_FLUSH, target_index) ||
	    fflush(file) != 0) {
		fclose(file);
		remove(private_path);
		private_path[0] = 0;
		return GRAPHICS_CONFIG_TRANSACTION_FLUSH_FAILED;
	}
	if (should_fail(GRAPHICS_CONFIG_TRANSACTION_FAIL_SYNC, target_index) ||
	    config_fsync(config_fileno(file)) != 0) {
		fclose(file);
		remove(private_path);
		private_path[0] = 0;
		return GRAPHICS_CONFIG_TRANSACTION_SYNC_FAILED;
	}
	if (fclose(file) != 0 || should_fail(GRAPHICS_CONFIG_TRANSACTION_FAIL_CLOSE, target_index)) {
		remove(private_path);
		private_path[0] = 0;
		return GRAPHICS_CONFIG_TRANSACTION_CLOSE_FAILED;
	}
	return GRAPHICS_CONFIG_TRANSACTION_OK;
}

static int replace_path(const char *source, const char *target, size_t target_index,
                        int inject_failure)
{
	if (inject_failure &&
	    should_fail(GRAPHICS_CONFIG_TRANSACTION_FAIL_REPLACE, target_index))
		return 0;
#ifdef _WIN32
	return MoveFileExA(source, target,
	                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
	return rename(source, target) == 0;
#endif
}

static int sync_parent_directory(const char *path)
{
#ifdef _WIN32
	(void) path;
	return 1;
#else
	char directory[PATH_MAX];
	char *slash;
	int fd;
	int ok;
	if (snprintf(directory, sizeof(directory), "%s", path) >= (int) sizeof(directory))
		return 0;
	slash = strrchr(directory, '/');
	if (!slash)
		return 1;
	if (slash == directory)
		slash[1] = 0;
	else
		*slash = 0;
	fd = open(directory, O_RDONLY | O_DIRECTORY);
	if (fd < 0)
		return 0;
	ok = fsync(fd) == 0 && close(fd) == 0;
	return ok;
#endif
}

static int rollback_published(struct staged_config *staged, size_t published_count)
{
	int ok = 1;
	while (published_count) {
		struct staged_config *item = &staged[--published_count];
		if (!item->published)
			continue;
		if (item->existed) {
			if (!replace_path(item->backup_path, item->path, published_count, 0))
				ok = 0;
			else
				item->backup_path[0] = 0;
		} else if (remove(item->path) != 0 && errno != ENOENT) {
			ok = 0;
		}
		if (!sync_parent_directory(item->path))
			ok = 0;
	}
	return ok;
}

enum graphics_config_transaction_result
graphics_config_patch_files(const char *const *paths, size_t path_count,
                            const char *key, int value)
{
	struct staged_config staged[GRAPHICS_CONFIG_MAX_TARGETS];
	enum graphics_config_transaction_result result = GRAPHICS_CONFIG_TRANSACTION_OK;
	size_t i;

	memset(staged, 0, sizeof(staged));
	if (!paths || !path_count || path_count > GRAPHICS_CONFIG_MAX_TARGETS ||
	    !key || !*key || strchr(key, '=') || strchr(key, '\n') || strchr(key, '\r'))
		return GRAPHICS_CONFIG_TRANSACTION_INVALID;
	for (i = 0; i < path_count; i++) {
		if (!paths[i] || !*paths[i]) {
			result = GRAPHICS_CONFIG_TRANSACTION_INVALID;
			goto done;
		}
		staged[i].path = paths[i];
		result = read_complete_file(&staged[i], i);
		if (result != GRAPHICS_CONFIG_TRANSACTION_OK)
			goto done;
		result = build_updated_file(&staged[i], key, value, i);
		if (result != GRAPHICS_CONFIG_TRANSACTION_OK)
			goto done;
		result = write_private_file(staged[i].path, "tmp", staged[i].updated,
		                            staged[i].updated_size, i, staged[i].temporary_path,
		                            sizeof(staged[i].temporary_path));
		if (result != GRAPHICS_CONFIG_TRANSACTION_OK)
			goto done;
		if (staged[i].existed) {
			result = write_private_file(staged[i].path, "bak", staged[i].original,
			                            staged[i].original_size, i, staged[i].backup_path,
			                            sizeof(staged[i].backup_path));
			if (result != GRAPHICS_CONFIG_TRANSACTION_OK)
				goto done;
		}
	}
	for (i = 0; i < path_count; i++) {
		if (!replace_path(staged[i].temporary_path, staged[i].path, i, 1)) {
			result = rollback_published(staged, i)
			             ? GRAPHICS_CONFIG_TRANSACTION_REPLACE_FAILED
			             : GRAPHICS_CONFIG_TRANSACTION_ROLLBACK_FAILED;
			goto done;
		}
		staged[i].temporary_path[0] = 0;
		staged[i].published = 1;
	}
	for (i = 0; i < path_count; i++) {
		if (!sync_parent_directory(staged[i].path)) {
			result = rollback_published(staged, path_count)
			             ? GRAPHICS_CONFIG_TRANSACTION_SYNC_FAILED
			             : GRAPHICS_CONFIG_TRANSACTION_ROLLBACK_FAILED;
			goto done;
		}
	}

done:
	cleanup_staged(staged, path_count);
	return result;
}

const char *graphics_config_transaction_result_name(
    enum graphics_config_transaction_result result)
{
	switch (result) {
		case GRAPHICS_CONFIG_TRANSACTION_OK: return "ok";
		case GRAPHICS_CONFIG_TRANSACTION_INVALID: return "invalid";
		case GRAPHICS_CONFIG_TRANSACTION_READ_FAILED: return "read_failed";
		case GRAPHICS_CONFIG_TRANSACTION_TOO_LARGE: return "too_large";
		case GRAPHICS_CONFIG_TRANSACTION_ALLOC_FAILED: return "alloc_failed";
		case GRAPHICS_CONFIG_TRANSACTION_WRITE_FAILED: return "write_failed";
		case GRAPHICS_CONFIG_TRANSACTION_FLUSH_FAILED: return "flush_failed";
		case GRAPHICS_CONFIG_TRANSACTION_SYNC_FAILED: return "sync_failed";
		case GRAPHICS_CONFIG_TRANSACTION_CLOSE_FAILED: return "close_failed";
		case GRAPHICS_CONFIG_TRANSACTION_REPLACE_FAILED: return "replace_failed";
		case GRAPHICS_CONFIG_TRANSACTION_ROLLBACK_FAILED: return "rollback_failed";
	}
	return "unknown";
}
