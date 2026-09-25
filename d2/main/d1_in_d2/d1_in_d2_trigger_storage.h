/* Original trigger fields outside D2's fixed core disk record */
#ifndef D1_IN_D2_TRIGGER_STORAGE_H
#define D1_IN_D2_TRIGGER_STORAGE_H
#include "pstypes.h"

typedef struct d1_trigger_storage {
	sbyte type;
	sbyte link_num;
} d1_trigger_storage;

#endif
