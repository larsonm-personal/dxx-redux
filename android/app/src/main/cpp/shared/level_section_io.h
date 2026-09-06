#ifndef DXX_LEVEL_SECTION_IO_H
#define DXX_LEVEL_SECTION_IO_H

#include <physfs.h>
#include "dxxerror.h"

/* Serialized sections need not be adjacent, or in decoding order */
static inline int level_section_seek(PHYSFS_file *file, int offset, int count, const char *name)
{
	if (!count)
		return 1;
	if (count < 0 || offset < 0 || offset >= PHYSFS_fileLength(file) || !PHYSFS_seek(file, offset)) {
		Warning("Invalid level %s section offset=%d count=%d", name, offset, count);
		return 0;
	}
	return 1;
}

#endif
