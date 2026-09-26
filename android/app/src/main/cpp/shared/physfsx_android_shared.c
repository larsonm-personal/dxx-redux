/* Shared Android PhysFS initialization for D1 and D2. */

#include "physfsx.h"
#include "args.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "physfsx_android_shared.h"
#include "physfsx_android_setup.h"
#include "android_mission_assets.h"

extern const PHYSFS_Archiver SAF_Archiver;

static char mission_directory[PATH_MAX];

void physfsx_android_unmount_mission_directory(void)
{
	if (mission_directory[0] && !PHYSFS_unmount(mission_directory))
		Error("Cannot release mission directory %s: %s", mission_directory, PHYSFS_getLastError());
	mission_directory[0] = 0;
}

void physfsx_android_mount_mission_directory(const char *descriptor)
{
	const char *slash = strrchr(descriptor, '/');
	char directory[PATH_MAX];
	physfsx_android_unmount_mission_directory();
	const char *root = PHYSFS_getRealDir(descriptor);
	if (!root || !slash) return;
	int length = snprintf(directory, sizeof(directory), "%s/%.*s", root,
	                      (int) (slash - descriptor), descriptor);
	if (length < 0 || (size_t) length >= sizeof(directory)) return;
	/* Loose mission levels, briefings, and assets resolve beside their descriptor */
	if (!PHYSFS_getMountPoint(directory) && PHYSFS_mount(directory, NULL, 0))
		strcpy(mission_directory, directory);
}

static int register_saf_archiver(void)
{
	return PHYSFS_registerArchiver(&SAF_Archiver);
}

static int init_search_paths(char *argv0, const char *game_dir, const char *data_dir,
                             char *error, size_t error_size)
{
	physfsx_android_setup_result result;
	const physfsx_android_setup_ops ops = {
		PHYSFS_getPrefDir,
		PHYSFS_getWriteDir,
		PHYSFS_setWriteDir,
		PHYSFS_mkdir,
		PHYSFS_mount,
		PHYSFS_unmount,
		register_saf_archiver,
		PHYSFS_getBaseDir,
		PHYSFSX_addRelToSearchPath,
		PHYSFS_getLastError,
	};
	if (!PHYSFS_init(argv0)) {
		snprintf(error, error_size, "PhysicsFS initialization failed: %s", PHYSFS_getLastError());
		return 0;
	}
	PHYSFS_permitSymbolicLinks(1);
	if (!physfsx_android_setup_search_paths(game_dir, data_dir, &ops, &result)) {
		snprintf(error, error_size, "Android content setup failed during %s for %s: %s",
		         result.operation, result.path, result.detail);
		return 0;
	}
	return 1;
}

void physfsx_android_init(int argc, char *argv[], const char *game_dir)
{
	char error[1024];
	if (!init_search_paths(argv[0], game_dir, getenv("DXX_ANDROID_LEVEL_PREVIEW_DATA_DIR"), error, sizeof(error)))
		Error("%s", error);
	InitArgsAndroid(argc, argv);
	android_mission_assets_init();
}

int physfsx_android_init_metadata(int argc, char *argv[], const char *game_dir,
                                  const char *data_dir, char *error, size_t error_size)
{
	if (!data_dir || !data_dir[0]) {
		snprintf(error, error_size, "%s", "missing metadata data directory");
		return 0;
	}
	if (!init_search_paths(argv[0], game_dir, data_dir, error, error_size))
		return 0;
	InitArgsAndroid(argc, argv);
	/* Metadata requests own their mounts, independent of the selected game mission */
	android_mission_assets_shutdown();
	return 1;
}
