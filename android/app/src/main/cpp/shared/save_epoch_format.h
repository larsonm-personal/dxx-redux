#ifndef SAVE_EPOCH_FORMAT_H
#define SAVE_EPOCH_FORMAT_H

#include <stdint.h>

#define SAVE_EPOCH_D1_VERSION 23
#define SAVE_EPOCH_D2_VERSION 47
#define SAVE_EPOCH_DISK_BYTES 8
/* Leave headroom for GameTime64 * 100 in the HUD and timestamp arithmetic. */
#define SAVE_EPOCH_MAX (INT64_MAX / 128)

static inline int save_epoch_valid(int64_t epoch)
{
	return epoch >= 0 && epoch <= SAVE_EPOCH_MAX;
}

static inline int save_timestamp_valid(int64_t timestamp)
{
	return timestamp >= -SAVE_EPOCH_MAX && timestamp <= SAVE_EPOCH_MAX;
}

#endif
