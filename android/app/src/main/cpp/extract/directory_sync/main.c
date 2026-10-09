#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int directory_syncs, directory_closes, fail_sync, fail_close, persistent;
static int descriptors[16];

static void require(int condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		exit(1);
	}
}

static int fixture_fsync(int fd)
{
	struct stat info;
	if (fstat(fd, &info) == 0 && S_ISDIR(info.st_mode)) {
		require(directory_syncs < 16, "bounded ordinary directory sync inventory");
		descriptors[directory_syncs++] = fd;
		if (fail_sync && (directory_syncs == fail_sync || (persistent && directory_syncs >= fail_sync))) {
			errno = EIO;
			return -1;
		}
	}
	return fsync(fd);
}

static int fixture_close(int fd)
{
	struct stat info;
	const int directory = fstat(fd, &info) == 0 && S_ISDIR(info.st_mode);
	if (directory) directory_closes++;
	const int result = close(fd);
	if (directory && fail_close && directory_closes == fail_close) {
		errno = EIO;
		return -1;
	}
	return result;
}

#define fsync fixture_fsync
#define close fixture_close
#include "../../shared/graphics_config_transaction.c"
#undef close
#undef fsync

static const char *paths[] = { "configs/root.cfg", "configs/d1.cfg", "configs/d2.cfg" };
static const char original[] = "Unknown=kept\nTexFilt=0\n";
static const char updated[] = "Unknown=kept\nTexFilt=2\n";

static int descriptor_count(void)
{
	int count = 0;
	DIR *directory = opendir("/proc/self/fd");
	struct dirent *entry;
	require(directory != NULL, "open actual process descriptor inventory");
	while ((entry = readdir(directory)))
		if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) count++;
	require(closedir(directory) == 0, "close descriptor inventory");
	return count;
}

static void write_original(const char *path)
{
	FILE *file = fopen(path, "wb");
	require(file != NULL, "open ordinary config");
	const size_t written = fwrite(original, 1, sizeof(original) - 1, file);
	const int closed = fclose(file);
	require(written == sizeof(original) - 1 && closed == 0, "write ordinary config");
}

static void check_bytes(const char *path, const char *expected)
{
	char bytes[64];
	FILE *file = fopen(path, "rb");
	require(file != NULL, "read actual transaction output");
	const size_t length = fread(bytes, 1, sizeof(bytes), file);
	const int closed = fclose(file);
	require(closed == 0 && length == strlen(expected) && !memcmp(bytes, expected, length), "preserve exact config output");
}

static void inspect_backups(unsigned retained)
{
	DIR *directory = opendir("configs");
	struct dirent *entry;
	unsigned found = 0;
	require(directory != NULL, "inspect retained originals");
	while ((entry = readdir(directory))) {
		const char *name = entry->d_name;
		if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
		int matched = 0;
		for (int i = 0; i < 3; ++i) {
			const char *leaf = strrchr(paths[i], '/') + 1;
			if (!strcmp(name, leaf)) {
				matched = 1;
				break;
			}
			const size_t length = strlen(leaf);
			if (!strncmp(name, leaf, length) && !strncmp(name + length, ".bak.", 5)) {
				const unsigned bit = 1u << i;
				char path[PATH_MAX];
				require((retained & bit) && !(found & bit), "one original backup per undurable rollback");
				require(snprintf(path, sizeof(path), "configs/%s", name) < (int) sizeof(path), "bounded retained path");
				check_bytes(path, original);
				require(remove(path) == 0, "remove verified owned backup");
				found |= bit;
				matched = 1;
				break;
			}
		}
		require(matched, "no unexpected private files");
	}
	require(closedir(directory) == 0 && found == retained, "all expected originals retained");
}

static void audit(int before, int expected_syncs)
{
	int live = 0;
	for (int i = 0; i < directory_syncs; ++i) {
		if (fcntl(descriptors[i], F_GETFD) != -1 || errno != EBADF) live++;
	}
	const int after = descriptor_count();
	if (directory_syncs != expected_syncs || directory_closes != directory_syncs || live || after != before) {
		fprintf(stderr, "directory lifetime: syncs=%d closes=%d live=%d descriptors=%d->%d\n", directory_syncs, directory_closes, live, before, after);
		// Immediately release the original implementation's one leaked descriptor before failing
		for (int i = 0; i < directory_syncs; ++i)
			if (fcntl(descriptors[i], F_GETFD) != -1) close(descriptors[i]);
		require(0, "every acquired directory descriptor closed on return");
	}
}

int main(void)
{
	require(mkdir("configs", 0700) == 0, "create isolated config directory");
	int cases = 0;
	for (int atomic = 0; atomic < 2; ++atomic) {
		for (int absent = 0; absent < 2; ++absent) {
			const int count = atomic ? 1 : 3;
			// Success, failure at each batch publication sync, and persistent rollback sync failure
			for (int failure = 0; failure <= count + 1; ++failure) {
				for (int i = 0; i < count; ++i) {
					remove(paths[i]);
					if (!absent) write_original(paths[i]);
				}
				directory_syncs = directory_closes = 0;
				persistent = failure == count + 1;
				fail_sync = persistent ? 1 : failure;
				const int before = descriptor_count();
				const enum graphics_config_transaction_result result = atomic
				                                                           ? graphics_config_atomic_replace(paths[0], updated, sizeof(updated) - 1)
				                                                           : graphics_config_patch_files(paths, count, "TexFilt", 2);
				audit(before, failure ? fail_sync + count : count);
				require(result == (failure ? (persistent ? GRAPHICS_CONFIG_TRANSACTION_ROLLBACK_FAILED : GRAPHICS_CONFIG_TRANSACTION_SYNC_FAILED) : GRAPHICS_CONFIG_TRANSACTION_OK), "preserve sync and rollback result propagation");
				for (int i = 0; i < count; ++i) {
					if (absent && failure) require(access(paths[i], F_OK) == -1 && errno == ENOENT, "rollback removes newly created target");
					else check_bytes(paths[i], failure ? original : (absent && !atomic ? "TexFilt=2\n" : updated));
				}
				inspect_backups(persistent && !absent ? (1u << count) - 1 : 0);
				cases++;
			}
		}
	}
	for (int atomic = 0; atomic < 2; ++atomic) {
		for (int i = 0; i < 3; ++i) write_original(paths[i]);
		directory_syncs = directory_closes = 0;
		fail_sync = persistent = 0;
		fail_close = 1;
		const int before = descriptor_count();
		const enum graphics_config_transaction_result result = atomic
		                                                           ? graphics_config_atomic_replace(paths[0], updated, sizeof(updated) - 1)
		                                                           : graphics_config_patch_files(paths, 3, "TexFilt", 2);
		audit(before, atomic ? 2 : 4);
		require(result == GRAPHICS_CONFIG_TRANSACTION_SYNC_FAILED, "directory close error still reports synchronization failure");
		for (int i = 0; i < 3; ++i) check_bytes(paths[i], original);
		inspect_backups(0);
		cases++;
	}
	fail_close = 0;
	// Publication failure rolls back its already-published sibling through the same sync owner
	for (int i = 0; i < 3; ++i) write_original(paths[i]);
	directory_syncs = directory_closes = 0;
	fail_sync = 1;
	persistent = 0;
	graphics_config_transaction_set_test_failure(GRAPHICS_CONFIG_TRANSACTION_FAIL_REPLACE, 1);
	int before = descriptor_count();
	const enum graphics_config_transaction_result result = graphics_config_patch_files(paths, 3, "TexFilt", 2);
	audit(before, 1);
	require(result == GRAPHICS_CONFIG_TRANSACTION_ROLLBACK_FAILED, "replace failure reports rollback sync failure");
	for (int i = 0; i < 3; ++i) check_bytes(paths[i], original);
	inspect_backups(1);
	cases++;
	graphics_config_transaction_set_test_failure(GRAPHICS_CONFIG_TRANSACTION_FAIL_READ, 0);
	directory_syncs = directory_closes = 0;
	before = descriptor_count();
	require(graphics_config_patch_files(paths, 3, "TexFilt", 2) == GRAPHICS_CONFIG_TRANSACTION_READ_FAILED, "ordinary prepublication read failure");
	audit(before, 0);
	cases++;
	graphics_config_transaction_set_test_failure(GRAPHICS_CONFIG_TRANSACTION_FAIL_NONE, 0);
	require(!sync_parent_directory("missing/config.cfg"), "ordinary parent open failure");
	audit(before, 0);
	cases++;
	require(sync_parent_directory("config.cfg"), "relative path without directory retains policy");
	audit(before, 0);
	cases++;
	for (int i = 0; i < 3; ++i) require(remove(paths[i]) == 0, "remove owned ordinary configs");
	require(rmdir("configs") == 0, "no staged or backup files remain");
	printf("PASS: %d actual directory sync lifetime cases\n", cases);
	return 0;
}
