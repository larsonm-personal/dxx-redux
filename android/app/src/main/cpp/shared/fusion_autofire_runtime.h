#ifndef FUSION_AUTOFIRE_RUNTIME_H
#define FUSION_AUTOFIRE_RUNTIME_H

/* Part of the unified D1 21 / D2 45 runtime tail.  The existing deadline
 * remains relative; this bit distinguishes an inactive timer from due-now. */
static inline int fusion_autofire_runtime_write(PHYSFS_file *fp, fix64 deadline)
{
	const PHYSFS_uint32 active = deadline != 0;
	return PHYSFS_write(fp, &active, sizeof(active), 1) == 1;
}

static inline int fusion_autofire_runtime_read(PHYSFS_file *fp, int swap, fix64 *deadline)
{
	PHYSFS_uint32 active;
	if (PHYSFS_read(fp, &active, sizeof(active), 1) != 1)
		return 0;
	if (swap)
		active = SWAPINT(active);
	if (active > 1)
		return 0;
	if (deadline) {
		if (!active)
			*deadline = 0;
		else if (!*deadline)
			*deadline = -1;
	}
	return 1;
}

#endif
