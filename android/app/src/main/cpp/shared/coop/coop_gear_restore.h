#ifndef COOP_GEAR_RESTORE_H
#define COOP_GEAR_RESTORE_H

#include <stddef.h>

/* Optional gear is accepted independently; discarded records never grant credit */
typedef struct coop_gear_restore_result {
	size_t accepted;
	size_t discarded;
} coop_gear_restore_result;

/* Transaction rollback must preserve every record, including unallocated sections */
static inline int coop_gear_restore_complete(coop_gear_restore_result result, size_t expected)
{
	return !result.discarded && result.accepted == expected;
}

#endif
