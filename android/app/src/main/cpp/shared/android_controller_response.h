#ifndef ANDROID_CONTROLLER_RESPONSE_H
#define ANDROID_CONTROLLER_RESPONSE_H

#include <stdint.h>
#include "android_axis_mailbox.h"

/* Kotlin supplies symmetric +/-32767 endpoints, already deadzoned and shaped
 * Preserve that precision until the final engine command conversion */
static inline int android_controller_command_time(int raw, int limit)
{
	if (raw > 32767) raw = 32767;
	if (raw < -32767) raw = -32767;
	return (int) ((int64_t) raw * limit / 32767);
}

#endif
