#ifndef WIGGLE_PHASE_RUNTIME_H
#define WIGGLE_PHASE_RUNTIME_H

/* Include after controls declarations and the disk/rewind file adapter */
#define WIGGLE_PHASE_D1_SAVE_VERSION 21
#define WIGGLE_PHASE_D2_SAVE_VERSION 45
#define WIGGLE_PHASE_RUNTIME_DISK_BYTES 4

static inline int wiggle_phase_runtime_write(PHYSFS_file *fp)
{
	const PHYSFS_uint32 phase = controls_get_wiggle_phase();
	return PHYSFS_write(fp, &phase, sizeof(phase), 1) == 1;
}

/* Read and validate the complete record before changing live phase */
static inline int wiggle_phase_runtime_read(PHYSFS_file *fp, int swap, int apply)
{
	PHYSFS_uint32 phase;
	if (PHYSFS_read(fp, &phase, sizeof(phase), 1) != 1)
		return 0;
	if (swap)
		phase = SWAPINT(phase);
	if (phase > 0xffffu)
		return 0;
	if (apply)
		controls_restore_wiggle_phase(phase);
	return 1;
}

#endif
