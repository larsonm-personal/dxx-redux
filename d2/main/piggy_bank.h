#ifndef DXX_PIGGY_BANK_H
#define DXX_PIGGY_BANK_H

#include "physfs.h"

/* Native bank records without registry pointers, cache state or GPU handles
 * Fixed four-byte fields and padded names keep C/C++ packing identical */
typedef struct piggy_bank_bitmap {
	int width, height, flags, average_index, offset, size;
	char name[16];
} piggy_bank_bitmap;

typedef struct piggy_bank_sound {
	int length, offset;
	char name[12];
} piggy_bank_sound;

#ifdef DXX_XL_RUNTIME
/* Read existing native headers into caller-owned records; return count or -1
 * Callers retain the source bytes and own payload validation/allocation */
int piggy_read_bitmap_bank(PHYSFS_file *file, void *context,
                          int (*record)(void *, int, const piggy_bank_bitmap *));
int piggy_read_sound_bank(PHYSFS_file *file, void *context,
                         int (*record)(void *, int, const piggy_bank_sound *));
#endif

#endif
