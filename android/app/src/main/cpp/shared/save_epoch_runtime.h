#ifndef SAVE_EPOCH_RUNTIME_H
#define SAVE_EPOCH_RUNTIME_H

#include "save_epoch_format.h"

/* The authoritative epoch follows the legacy zero clock in new saves.
 * Existing relative timestamps and the effects animation reference keep their
 * wire meanings; the epoch must be selected before decoding those timestamps. */
static inline int save_epoch_write(PHYSFS_file *fp, fix64 epoch)
{
	PHYSFS_uint32 words[2];
	if (!save_epoch_valid(epoch))
		return 0;
	words[0] = (PHYSFS_uint32)epoch;
	words[1] = (PHYSFS_uint32)((PHYSFS_uint64)epoch >> 32);
	return PHYSFS_write(fp, words, sizeof(words), 1) == 1;
}

static inline int save_epoch_read(PHYSFS_file *fp, int swap, fix64 *epoch)
{
	PHYSFS_uint32 words[2];
	PHYSFS_uint64 value;
	if (PHYSFS_read(fp, words, sizeof(words), 1) != 1)
		return 0;
	if (swap) {
		words[0] = SWAPINT(words[0]);
		words[1] = SWAPINT(words[1]);
	}
	value = ((PHYSFS_uint64)words[1] << 32) | words[0];
	if (value > SAVE_EPOCH_MAX)
		return 0;
	*epoch = (fix64)value;
	return 1;
}

/* Called just after reading the mission name, before loading that mission.
 * The remaining fixed header is current level, next level and legacy clock. */
static inline int save_epoch_preflight(PHYSFS_file *fp, int swap)
{
	const PHYSFS_sint64 start = PHYSFS_tell(fp);
	fix64 epoch;
	int valid;
	if (start < 0 || !PHYSFS_seek(fp, (PHYSFS_uint64)start + 3 * sizeof(PHYSFS_uint32)))
		return 0;
	valid = save_epoch_read(fp, swap, &epoch);
	return PHYSFS_seek(fp, (PHYSFS_uint64)start) && valid;
}

/* Presence is separate from a relative deadline, whose zero means due now. */
static inline int save_timer_active_write(PHYSFS_file *fp, fix64 deadline)
{
	const PHYSFS_uint32 active = deadline != 0;
	return PHYSFS_write(fp, &active, sizeof(active), 1) == 1;
}

static inline int save_timer_active_read(PHYSFS_file *fp, int swap, fix64 *deadline)
{
	PHYSFS_uint32 active;
	if (PHYSFS_read(fp, &active, sizeof(active), 1) != 1)
		return 0;
	if (swap) active = SWAPINT(active);
	if (active > 1) return 0;
	if (deadline) {
		if (!active) *deadline = 0;
		else if (!*deadline) *deadline = -1;
	}
	return 1;
}

#endif
