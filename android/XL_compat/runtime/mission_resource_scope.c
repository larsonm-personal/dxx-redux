#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "physfsx.h"
#include "mission.h"
#include "mission_resource_scope.h"

int xl_mission_resource_release(xl_mission_resource_scope *scope, const char *archive)
{
	if (scope->owns_mount) {
		const char *mount = PHYSFS_getMountPoint(archive);
		if (mount && !strcmp(mount, scope->private_mount) &&
		    !PHYSFS_unmount(archive)) return 0;
		scope->owns_mount = 0;
	}
	return 1;
}

static int prepared_resource_name(const char *name)
{
	const char *part = name;
	if (!name || !*name || *name == '/' || strchr(name, '\\') || strchr(name, ':')) return 0;
	for (;;) {
		const char *end = strchr(part, '/');
		const size_t length = end ? (size_t) (end - part) : strlen(part);
		if (!length || (length == 1 && part[0] == '.') ||
		    (length == 2 && part[0] == '.' && part[1] == '.')) return 0;
		if (!end) return 1;
		part = end + 1;
	}
}

PHYSFS_file *xl_mission_resource_open(xl_mission_resource_scope *scope,
                                      const char *archive, const char *directory, const char *resource)
{
	static unsigned int next_mount;
	char path[PATH_MAX];
	const char *mount, *origin;
	if (!prepared_resource_name(resource)) return NULL;
	if (!archive) {
		if (strlen(resource) >= sizeof(path)) return NULL;
		strcpy(path, resource);
		if (PHYSFSEXT_locateCorrectCase(path) != 0) return NULL;
		origin = PHYSFS_getRealDir(path);
		if (!origin || strcmp(origin, directory)) return NULL;
		return PHYSFSX_openReadBuffered(path);
	}
	mount = PHYSFS_getMountPoint(archive);
	if (!mount) {
		do {
			if (next_mount == UINT_MAX) return NULL;
			snprintf(scope->private_mount, sizeof(scope->private_mount), "__mission_prepare_%u/", ++next_mount);
		} while (PHYSFS_exists(scope->private_mount));
		if (!PHYSFS_mount(archive, scope->private_mount, 1)) return NULL;
		scope->owns_mount = 1;
		mount = PHYSFS_getMountPoint(archive);
		/* PhysFS owns mount-point normalization, including the trailing slash */
		if (!mount || strlen(mount) >= sizeof(scope->private_mount)) return NULL;
		strcpy(scope->private_mount, mount);
	}
	if (scope->owns_mount && mount && !strcmp(mount, scope->private_mount)) {
		const int size = snprintf(path, sizeof(path), "%s%s", scope->private_mount, resource);
		if (size < 0 || (size_t) size >= sizeof(path)) return NULL;
	} else if (mission_owns_resource_archive(archive)) {
		if (strlen(resource) >= sizeof(path)) return NULL;
		strcpy(path, resource);
	} else return NULL;
	if (PHYSFSEXT_locateCorrectCase(path) != 0) return NULL;
	origin = PHYSFS_getRealDir(path);
	if (!origin || strcmp(origin, archive)) return NULL;
	return PHYSFSX_openReadBuffered(path);
}
