#ifndef COOP_GEAR_RESTORE_H
#define COOP_GEAR_RESTORE_H

#include <stddef.h>

/* Optional gear is accepted independently; discarded records never grant credit */
typedef struct coop_gear_restore_result {
	size_t accepted;
	size_t discarded;
} coop_gear_restore_result;

#endif
