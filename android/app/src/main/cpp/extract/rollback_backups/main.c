#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#define make_dir(path)   _mkdir(path)
#define remove_dir(path) _rmdir(path)
#define missing(path)    (_access(path, 0) != 0)
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#define make_dir(path)   mkdir(path, 0700)
#define remove_dir(path) rmdir(path)
#define missing(path)    (access(path, F_OK) != 0)
#endif

static const char *config_paths[] = { "configs/root.cfg", "configs/d1.cfg", "configs/d2.cfg" };
static const char original[] = "Unknown=kept\nTexFilt=0\n";
static const char updated_bytes[] = "Unknown=kept\nTexFilt=2\n";
static int failed_target = -1, directory_syncs, fail_sync, persistent_sync;
static int rollback_attempts, fail_restore_write;
static int failed_removal = -1;
static char backups[3][4096];

static void require(int condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		exit(1);
	}
}

static int reject_restore(const char *source, const char *target)
{
	if (!strstr(source, ".bak.") && !strstr(source, ".rollback.")) return 0;
	rollback_attempts++;
	for (int i = 0; i < 3; ++i)
		if ((failed_target == i || failed_target == 3) && !strcmp(target, config_paths[i])) return 1;
	return 0;
}

static int fixture_remove(const char *path)
{
	if (failed_removal >= 0 && !strcmp(path, config_paths[failed_removal])) {
		errno = EACCES;
		return -1;
	}
	return remove(path);
}

static FILE *fixture_fdopen(int fd, const char *mode)
{
	if (fail_restore_write && rollback_attempts == 0 && directory_syncs) {
		errno = EIO;
		return NULL;
	}
#ifdef _WIN32
	return _fdopen(fd, mode);
#else
	return fdopen(fd, mode);
#endif
}

#ifdef _WIN32
static BOOL WINAPI fixture_move(const char *source, const char *target, DWORD flags)
{
	if (reject_restore(source, target)) {
		SetLastError(ERROR_ACCESS_DENIED);
		return FALSE;
	}
	return MoveFileExA(source, target, flags);
}
#define MoveFileExA fixture_move
#define _fdopen     fixture_fdopen
#else
static int fixture_rename(const char *source, const char *target)
{
	if (reject_restore(source, target)) {
		errno = EACCES;
		return -1;
	}
	return rename(source, target);
}
static int fixture_fsync(int fd)
{
	struct stat info;
	if (fstat(fd, &info) == 0 && S_ISDIR(info.st_mode)) {
		directory_syncs++;
		if (fail_sync && (directory_syncs == fail_sync || (persistent_sync && directory_syncs >= fail_sync))) {
			errno = EIO;
			return -1;
		}
	}
	return fsync(fd);
}
#define rename fixture_rename
#define fsync  fixture_fsync
#define fdopen fixture_fdopen
#endif
#define remove fixture_remove
#include "../../shared/graphics_config_transaction.c"
#undef remove
#ifdef _WIN32
#undef MoveFileExA
#undef _fdopen
#else
#undef rename
#undef fsync
#undef fdopen
#endif

static void check_bytes(const char *path, const char *expected)
{
	char bytes[64];
	FILE *file = fopen(path, "rb");
	require(file != NULL, "recoverable original file exists");
	const size_t size = fread(bytes, 1, sizeof(bytes), file);
	const int closed = fclose(file);
	require(closed == 0 && size == strlen(expected) && !memcmp(bytes, expected, size), "exact recoverable original bytes");
}

static void reset_files(int absent)
{
	failed_target = -1;
	failed_removal = -1;
	directory_syncs = fail_sync = persistent_sync = rollback_attempts = fail_restore_write = 0;
	graphics_config_transaction_set_test_failure(GRAPHICS_CONFIG_TRANSACTION_FAIL_NONE, 0);
	memset(backups, 0, sizeof(backups));
	for (int i = 0; i < 3; ++i) {
		remove(config_paths[i]);
		if (!absent) {
			FILE *file = fopen(config_paths[i], "wb");
			require(file != NULL, "create ordinary config");
			const size_t size = fwrite(original, 1, sizeof(original) - 1, file);
			const int closed = fclose(file);
			require(size == sizeof(original) - 1 && closed == 0, "write ordinary config");
		}
	}
}

static void inspect_private_files(unsigned retained)
{
#ifdef _WIN32
	WIN32_FIND_DATAA entry;
	HANDLE search = FindFirstFileA("configs/*", &entry);
	require(search != INVALID_HANDLE_VALUE, "inspect owned config directory");
	do {
		const char *name = entry.cFileName;
#else
	DIR *search = opendir("configs");
	struct dirent *entry;
	require(search != NULL, "inspect owned config directory");
	while ((entry = readdir(search))) {
		const char *name = entry->d_name;
#endif
		if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
		if (!strcmp(name, "unrelated.bak.keep")) {
			check_bytes("configs/unrelated.bak.keep", original);
			continue;
		}
		int matched = 0;
		for (int i = 0; i < 3; ++i) {
			const char *leaf = strrchr(config_paths[i], '/') + 1;
			if (!strcmp(name, leaf)) {
				matched = 1;
				break;
			}
			const size_t length = strlen(leaf);
			if (!strncmp(name, leaf, length) && !strncmp(name + length, ".bak.", 5)) {
				require((retained & (1u << i)) && !backups[i][0], "only failed target owns one retained backup");
				require(snprintf(backups[i], sizeof(backups[i]), "configs/%s", name) < (int) sizeof(backups[i]), "bounded owned backup path");
				check_bytes(backups[i], original);
				matched = 1;
				break;
			}
		}
		require(matched, "no temporary or unowned backup remains");
#ifdef _WIN32
	} while (FindNextFileA(search, &entry));
	const DWORD error = GetLastError();
	const BOOL closed = FindClose(search);
	require(error == ERROR_NO_MORE_FILES && closed, "complete owned directory inventory");
#else
	}
	require(closedir(search) == 0, "close owned directory inventory");
#endif
	for (int i = 0; i < 3; ++i) {
		require(!!backups[i][0] == !!(retained & (1u << i)), "every failed target retains its original backup");
		if (backups[i][0]) {
			// Recover using the actual retained disk copy after the failed call has returned
			require(replace_path(backups[i], config_paths[i], i, 0), "recover retained original after clearing fault");
			check_bytes(config_paths[i], original);
		}
	}
	if (retained) {
		fail_sync = persistent_sync = 0;
		graphics_config_transaction_set_test_failure(GRAPHICS_CONFIG_TRANSACTION_FAIL_NONE, 0);
		require(graphics_config_patch_files(config_paths, 3, "TexFilt", 2) == GRAPHICS_CONFIG_TRANSACTION_OK, "successful transaction after original recovery");
		for (int i = 0; i < 3; ++i) check_bytes(config_paths[i], updated_bytes);
		memset(backups, 0, sizeof(backups));
		inspect_private_files(0);
	}
}

int main(void)
{
	require(make_dir("configs") == 0, "create isolated rollback directory");
	FILE *unrelated = fopen("configs/unrelated.bak.keep", "wb");
	require(unrelated != NULL, "create unrelated file");
	const size_t written = fwrite(original, 1, sizeof(original) - 1, unrelated);
	const int closed = fclose(unrelated);
	require(written == sizeof(original) - 1 && closed == 0, "write unrelated file");
	int cases = 0;
	for (int publish = 0; publish < 3; ++publish) {
		for (int rollback = -1; rollback < publish; ++rollback) {
			reset_files(0);
			failed_target = rollback;
			graphics_config_transaction_set_test_failure(GRAPHICS_CONFIG_TRANSACTION_FAIL_REPLACE, publish);
			const enum graphics_config_transaction_result result = graphics_config_patch_files(config_paths, 3, "TexFilt", 2);
			require(result == (rollback < 0 ? GRAPHICS_CONFIG_TRANSACTION_REPLACE_FAILED : GRAPHICS_CONFIG_TRANSACTION_ROLLBACK_FAILED), "publication and rollback failure result");
			for (int i = 0; i < 3; ++i) check_bytes(config_paths[i], i == rollback ? updated_bytes : original);
			failed_target = -1;
			inspect_private_files(rollback < 0 ? 0 : 1u << rollback);
			cases++;
		}
	}
#ifndef _WIN32
	for (int atomic = 0; atomic < 2; ++atomic) {
		const int count = atomic ? 1 : 3;
		for (int sync = 1; sync <= count; ++sync) {
			for (int rollback = -1; rollback <= count; ++rollback) {
				reset_files(0);
				failed_target = rollback == count ? 3 : rollback;
				fail_sync = sync;
				const enum graphics_config_transaction_result result = atomic ? graphics_config_atomic_replace(config_paths[0], updated_bytes, sizeof(updated_bytes) - 1) : graphics_config_patch_files(config_paths, count, "TexFilt", 2);
				require(result == (rollback < 0 ? GRAPHICS_CONFIG_TRANSACTION_SYNC_FAILED : GRAPHICS_CONFIG_TRANSACTION_ROLLBACK_FAILED), "sync-triggered rollback failure result");
				for (int i = 0; i < count; ++i) check_bytes(config_paths[i], (rollback == i || rollback == count) ? updated_bytes : original);
				failed_target = -1;
				inspect_private_files(rollback < 0 ? 0 : (rollback == count ? (1u << count) - 1 : 1u << rollback));
				cases++;
			}
		}
		for (int fault = 0; fault < 2; ++fault) {
			reset_files(0);
			fail_sync = persistent_sync = 1;
			fail_restore_write = fault;
			require((atomic ? graphics_config_atomic_replace(config_paths[0], updated_bytes, sizeof(updated_bytes) - 1) : graphics_config_patch_files(config_paths, count, "TexFilt", 2)) == GRAPHICS_CONFIG_TRANSACTION_ROLLBACK_FAILED, "undurable or unwritten restoration retains backup");
			for (int i = 0; i < count; ++i) check_bytes(config_paths[i], fault ? updated_bytes : original);
			fail_restore_write = 0;
			inspect_private_files((1u << count) - 1);
			cases++;
		}
	}
#endif
	for (int failure = -1; failure < 2; ++failure) {
		reset_files(1);
		failed_removal = failure;
		graphics_config_transaction_set_test_failure(GRAPHICS_CONFIG_TRANSACTION_FAIL_REPLACE, 2);
		require(graphics_config_patch_files(config_paths, 3, "TexFilt", 2) == (failure < 0 ? GRAPHICS_CONFIG_TRANSACTION_REPLACE_FAILED : GRAPHICS_CONFIG_TRANSACTION_ROLLBACK_FAILED), "new target publication and rollback removal failure");
		for (int i = 0; i < 3; ++i) {
			if (i == failure) check_bytes(config_paths[i], "TexFilt=2\n");
			else require(missing(config_paths[i]) && errno == ENOENT, "rollback restores original absence");
		}
		failed_removal = -1;
		graphics_config_transaction_set_test_failure(GRAPHICS_CONFIG_TRANSACTION_FAIL_NONE, 0);
		inspect_private_files(0);
		require(graphics_config_patch_files(config_paths, 3, "TexFilt", 2) == GRAPHICS_CONFIG_TRANSACTION_OK, "successful retry after removal fault");
		for (int i = 0; i < 3; ++i) check_bytes(config_paths[i], "TexFilt=2\n");
		inspect_private_files(0);
		cases++;
	}
#ifndef _WIN32
	for (int atomic = 0; atomic < 2; ++atomic) {
		const int count = atomic ? 1 : 3;
		for (int failure = 0; failure < count; ++failure) {
			reset_files(1);
			failed_removal = failure;
			fail_sync = 1;
			require((atomic ? graphics_config_atomic_replace(config_paths[0], updated_bytes, sizeof(updated_bytes) - 1) : graphics_config_patch_files(config_paths, count, "TexFilt", 2)) == GRAPHICS_CONFIG_TRANSACTION_ROLLBACK_FAILED, "new target sync and removal failure");
			for (int i = 0; i < count; ++i) {
				if (i == failure) check_bytes(config_paths[i], atomic ? updated_bytes : "TexFilt=2\n");
				else require(missing(config_paths[i]) && errno == ENOENT, "healthy new target rollback");
			}
			failed_removal = -1;
			inspect_private_files(0);
			cases++;
		}
	}
#endif
	for (int atomic = 0; atomic < 2; ++atomic) {
		reset_files(0);
		require((atomic ? graphics_config_atomic_replace(config_paths[0], updated_bytes, sizeof(updated_bytes) - 1) : graphics_config_patch_files(config_paths, 3, "TexFilt", 2)) == GRAPHICS_CONFIG_TRANSACTION_OK, "ordinary successful publication");
		for (int i = 0; i < 3; ++i) check_bytes(config_paths[i], !atomic || i == 0 ? updated_bytes : original);
		inspect_private_files(0);
		cases++;
	}
	for (int i = 0; i < 3; ++i) require(remove(config_paths[i]) == 0, "remove owned targets");
	require(remove("configs/unrelated.bak.keep") == 0, "remove unchanged unrelated file");
	require(remove_dir("configs") == 0, "all private ownership accounted for");
	printf("PASS: %d actual rollback backup ownership cases\n", cases);
	return 0;
}
