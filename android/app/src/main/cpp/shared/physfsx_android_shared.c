/* Shared Android PhysFS search-path initialization for D1 and D2. */

#include "physfsx.h"

#include "physfsx_android_shared.h"

extern const PHYSFS_Archiver SAF_Archiver;

static int register_saf_archiver(void)
{
	return PHYSFS_registerArchiver(&SAF_Archiver);
}

int physfsx_android_init_search_paths(const char *game_dir,
                                      physfsx_android_setup_result *result)
{
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
	return physfsx_android_setup_search_paths(game_dir, &ops, result);
}
