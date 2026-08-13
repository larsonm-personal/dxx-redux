/* Shared Android PhysFS initialization for D1 and D2. */

#include "physfsx.h"
#include "args.h"

#include "physfsx_android_shared.h"
#include "physfsx_android_setup.h"

extern const PHYSFS_Archiver SAF_Archiver;

static int register_saf_archiver(void)
{
	return PHYSFS_registerArchiver(&SAF_Archiver);
}

void physfsx_android_init(int argc, char *argv[], const char *game_dir)
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
	if (!PHYSFS_init(argv[0]))
		Error("PhysicsFS initialization failed: %s", PHYSFS_getLastError());
	PHYSFS_permitSymbolicLinks(1);
	if (!physfsx_android_setup_search_paths(game_dir, &ops, &result))
		Error("Android content setup failed during %s for %s: %s",
		      result.operation, result.path, result.detail);
	InitArgsAndroid(argc, argv);
}
