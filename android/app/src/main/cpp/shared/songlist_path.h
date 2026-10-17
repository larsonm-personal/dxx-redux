#ifndef DXX_SONGLIST_PATH_H
#define DXX_SONGLIST_PATH_H

#include <stdio.h>
#include <string.h>
#include "physfsx.h"
#include "strutil.h"

/* Song lists are root resources. Honor mount priority before filename case,
 * retaining exact-case preference within one mount and leaving the general
 * legacy case resolver unchanged
 */
static inline void songlist_path(const char *name, char *path, size_t size)
{
	char **files, **mounts, **file;
	size_t best = (size_t) -1;
	snprintf(path, size, "%s", name);
	if (strchr(name, '/') || strchr(name, '\\')) return;
	files = PHYSFS_enumerateFiles("");
	mounts = PHYSFS_getSearchPath();
	if (files && mounts) {
		for (file = files; *file; ++file) {
			const char *source;
			size_t rank;
			if (d_stricmp(*file, name)) continue;
			source = PHYSFS_getRealDir(*file);
			if (!source) continue;
			for (rank = 0; mounts[rank]; ++rank)
				if (!strcmp(mounts[rank], source)) break;
			if (mounts[rank] && (rank < best || (rank == best && !strcmp(*file, name)))) {
				best = rank;
				snprintf(path, size, "%s", *file);
			}
		}
	}
	PHYSFS_freeList(files);
	PHYSFS_freeList(mounts);
}

#endif
