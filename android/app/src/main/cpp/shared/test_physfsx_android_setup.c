#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define getpid          _getpid
#define mkdir_one(path) _mkdir(path)
#else
#include <unistd.h>
#define mkdir_one(path) mkdir(path, 0700)
#endif

#include "physfsx_android_setup.h"

#define CHECK(condition)                                                         \
	do {                                                                         \
		if (!(condition)) {                                                      \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
			return 0;                                                            \
		}                                                                        \
	} while (0)

static char test_root[PHYSFSX_ANDROID_PATH_MAX];
static char game_path[PHYSFSX_ANDROID_PATH_MAX];
static char active_path[PHYSFSX_ANDROID_PATH_MAX];
static char current_write_dir[PHYSFSX_ANDROID_PATH_MAX];
static char fail_operation[PHYSFSX_ANDROID_PATH_MAX];
static char mount_paths[80][PHYSFSX_ANDROID_PATH_MAX];
static char unmount_paths[80][PHYSFSX_ANDROID_PATH_MAX];
static int mount_count;
static int unmount_count;

static const char *mock_pref_dir(const char *organization, const char *application)
{
	(void) organization;
	(void) application;
	return strcmp(fail_operation, "pref") == 0 ? NULL : test_root;
}

static const char *mock_get_write_dir(void)
{
	return current_write_dir[0] ? current_write_dir : NULL;
}

static int mock_set_write_dir(const char *path)
{
	if ((strcmp(fail_operation, "set-pref") == 0 && path && strcmp(path, test_root) == 0) ||
	    (strcmp(fail_operation, "set-game") == 0 && path && strcmp(path, game_path) == 0))
		return 0;
	snprintf(current_write_dir, sizeof(current_write_dir), "%s", path ? path : "");
	return 1;
}

static int mock_make_dir(const char *path)
{
	(void) path;
	return strcmp(fail_operation, "mkdir") != 0;
}

static int mock_mount(const char *path, const char *mount_point, int append)
{
	(void) mount_point;
	(void) append;
	if (strncmp(fail_operation, "mount:", 6) == 0 && strcmp(fail_operation + 6, path) == 0)
		return 0;
	snprintf(mount_paths[mount_count++], sizeof(mount_paths[0]), "%s", path);
	return 1;
}

static int mock_unmount(const char *path)
{
	snprintf(unmount_paths[unmount_count++], sizeof(unmount_paths[0]), "%s", path);
	return 1;
}

static int mock_register_saf(void)
{
	return strcmp(fail_operation, "register") != 0;
}

static const char *mock_base_dir(void)
{
	return "optional-base";
}

static int mock_add_relative(const char *path, int append)
{
	(void) path;
	(void) append;
	return 1;
}

static const char *mock_last_error(void)
{
	return "injected failure";
}

static const physfsx_android_setup_ops mock_ops = {
	mock_pref_dir,
	mock_get_write_dir,
	mock_set_write_dir,
	mock_make_dir,
	mock_mount,
	mock_unmount,
	mock_register_saf,
	mock_base_dir,
	mock_add_relative,
	mock_last_error,
};

static int write_text_file(const char *path, const char *text)
{
	FILE *file = fopen(path, "wb");
	if (!file)
		return 0;
	if (fputs(text, file) < 0 || fclose(file) != 0)
		return 0;
	return 1;
}

static int reset_fixture(int with_active, int with_saf, int with_mods)
{
	char path[PHYSFSX_ANDROID_PATH_MAX];
	char temp_root[PHYSFSX_ANDROID_PATH_MAX];
	const char *temp = getenv("TEMP");
	if (!temp)
		temp = getenv("TMPDIR");
	if (!temp)
		temp = ".";
	snprintf(temp_root, sizeof(temp_root), "%s/br0253-%ld-%d", temp, (long) getpid(), rand());
	CHECK(mkdir_one(temp_root) == 0 || errno == EEXIST);
	snprintf(test_root, sizeof(test_root), "%s/", temp_root);
	snprintf(game_path, sizeof(game_path), "%sd2x-redux/", test_root);
	CHECK(mkdir_one(game_path) == 0 || errno == EEXIST);
	snprintf(active_path, sizeof(active_path), "%sselected", temp_root);
	CHECK(mkdir_one(active_path) == 0 || errno == EEXIST);
	if (with_active) {
		snprintf(path, sizeof(path), "%s.active_set_path", game_path);
		CHECK(write_text_file(path, active_path));
	}
	if (with_saf) {
		snprintf(path, sizeof(path), "%s/.saf_manifest.json", active_path);
		CHECK(write_text_file(path, "{}"));
	}
	if (with_mods) {
		snprintf(path, sizeof(path), "%s.active_mod_paths", game_path);
		CHECK(write_text_file(path, "mod-one\nmod-two\tmissions\n"));
	}
	snprintf(current_write_dir, sizeof(current_write_dir), "previous-write");
	fail_operation[0] = '\0';
	mount_count = 0;
	unmount_count = 0;
	return 1;
}

static int test_nominal_order(void)
{
	physfsx_android_setup_result result;
	CHECK(reset_fixture(1, 1, 1));
	CHECK(physfsx_android_setup_search_paths("d2x-redux", &mock_ops, &result));
	CHECK(strcmp(current_write_dir, game_path) == 0);
	CHECK(mount_count == 7);
	CHECK(strcmp(mount_paths[0], game_path) == 0);
	CHECK(strcmp(mount_paths[1], test_root) == 0);
	CHECK(strcmp(mount_paths[2], active_path) == 0);
	CHECK(strstr(mount_paths[3], ".saf_manifest.json") != NULL);
	CHECK(strcmp(mount_paths[4], "mod-two") == 0);
	CHECK(strcmp(mount_paths[5], "mod-one") == 0);
	CHECK(strcmp(mount_paths[6], "optional-base") == 0);
	return 1;
}

static int test_active_mount_failure_rolls_back(void)
{
	physfsx_android_setup_result result;
	CHECK(reset_fixture(1, 0, 0));
	snprintf(fail_operation, sizeof(fail_operation), "mount:%s", active_path);
	CHECK(!physfsx_android_setup_search_paths("d2x-redux", &mock_ops, &result));
	CHECK(strcmp(result.operation, "mount active set") == 0);
	CHECK(strcmp(result.path, active_path) == 0);
	CHECK(strcmp(result.detail, "injected failure") == 0);
	CHECK(strcmp(current_write_dir, "previous-write") == 0);
	CHECK(unmount_count == 2);
	CHECK(strcmp(unmount_paths[0], test_root) == 0);
	CHECK(strcmp(unmount_paths[1], game_path) == 0);
	return 1;
}

static int test_late_mod_failure_removes_partial_stack(void)
{
	physfsx_android_setup_result result;
	CHECK(reset_fixture(1, 0, 1));
	snprintf(fail_operation, sizeof(fail_operation), "mount:mod-one");
	CHECK(!physfsx_android_setup_search_paths("d2x-redux", &mock_ops, &result));
	CHECK(strcmp(result.operation, "mount enabled mod") == 0);
	CHECK(strcmp(current_write_dir, "previous-write") == 0);
	CHECK(unmount_count == 4);
	CHECK(strcmp(unmount_paths[0], "mod-two") == 0);
	CHECK(strcmp(unmount_paths[1], active_path) == 0);
	return 1;
}

static int test_saf_manifest_failure_rolls_back_selected_content(void)
{
	physfsx_android_setup_result result;
	char saf_path[PHYSFSX_ANDROID_PATH_MAX];
	CHECK(reset_fixture(1, 1, 0));
	snprintf(saf_path, sizeof(saf_path), "%s/.saf_manifest.json", active_path);
	snprintf(fail_operation, sizeof(fail_operation), "mount:%s", saf_path);
	CHECK(!physfsx_android_setup_search_paths("d2x-redux", &mock_ops, &result));
	CHECK(strcmp(result.operation, "mount SAF manifest") == 0);
	CHECK(strcmp(result.path, saf_path) == 0);
	CHECK(strcmp(current_write_dir, "previous-write") == 0);
	CHECK(unmount_count == 3);
	CHECK(strcmp(unmount_paths[0], active_path) == 0);
	CHECK(strcmp(unmount_paths[1], test_root) == 0);
	CHECK(strcmp(unmount_paths[2], game_path) == 0);
	return 1;
}

static int test_required_setup_failures_abort(void)
{
	const char *failures[] = { "pref", "set-pref", "mkdir", "set-game", "register" };
	size_t i;
	for (i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
		physfsx_android_setup_result result;
		CHECK(reset_fixture(1, 0, 0));
		snprintf(fail_operation, sizeof(fail_operation), "%s", failures[i]);
		CHECK(!physfsx_android_setup_search_paths("d2x-redux", &mock_ops, &result));
		CHECK(result.operation[0] != '\0');
	}
	return 1;
}

static int test_absent_selection_uses_optional_fallbacks(void)
{
	physfsx_android_setup_result result;
	CHECK(reset_fixture(0, 0, 0));
	CHECK(physfsx_android_setup_search_paths("d2x-redux", &mock_ops, &result));
	CHECK(mount_count == 3);
	CHECK(strcmp(mount_paths[0], game_path) == 0);
	CHECK(strcmp(mount_paths[1], test_root) == 0);
	CHECK(strcmp(mount_paths[2], "optional-base") == 0);
	return 1;
}

int main(void)
{
	CHECK(test_nominal_order());
	CHECK(test_active_mount_failure_rolls_back());
	CHECK(test_late_mod_failure_removes_partial_stack());
	CHECK(test_saf_manifest_failure_rolls_back_selected_content());
	CHECK(test_required_setup_failures_abort());
	CHECK(test_absent_selection_uses_optional_fallbacks());
	printf("physfsx_android_setup tests passed\n");
	return 0;
}
