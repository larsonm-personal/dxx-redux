#ifndef DXX_PHYSFSX_CHECKED_READ_H
#define DXX_PHYSFSX_CHECKED_READ_H

#include "physfsx.h"

typedef struct PHYSFSX_read_status {
	int failed;
} PHYSFSX_read_status;

static inline void PHYSFSX_readStatusReset(PHYSFSX_read_status *status)
{
	status->failed = 0;
}

static inline int PHYSFSX_readStatusFailed(const PHYSFSX_read_status *status)
{
	return status->failed;
}

static inline int PHYSFSX_readIntChecked(PHYSFSX_read_status *status, PHYSFS_file *file)
{
	PHYSFS_sint32 value = 0;
	if (!status->failed && !PHYSFS_readSLE32(file, &value))
		status->failed = 1;
	return value;
}

static inline short PHYSFSX_readShortChecked(PHYSFSX_read_status *status, PHYSFS_file *file)
{
	PHYSFS_sint16 value = 0;
	if (!status->failed && !PHYSFS_readSLE16(file, &value))
		status->failed = 1;
	return value;
}

static inline sbyte PHYSFSX_readByteChecked(PHYSFSX_read_status *status, PHYSFS_file *file)
{
	sbyte value = 0;
	if (!status->failed && PHYSFS_read(file, &value, sizeof(value), 1) != 1)
		status->failed = 1;
	return value;
}

static inline fix PHYSFSX_readFixChecked(PHYSFSX_read_status *status, PHYSFS_file *file)
{
	return PHYSFSX_readIntChecked(status, file);
}

static inline fixang PHYSFSX_readFixAngChecked(PHYSFSX_read_status *status, PHYSFS_file *file)
{
	return PHYSFSX_readShortChecked(status, file);
}

static inline void PHYSFSX_readVectorChecked(PHYSFSX_read_status *status, vms_vector *vector,
                                             PHYSFS_file *file)
{
	vector->x = PHYSFSX_readFixChecked(status, file);
	vector->y = PHYSFSX_readFixChecked(status, file);
	vector->z = PHYSFSX_readFixChecked(status, file);
}

static inline void PHYSFSX_readAngleVecChecked(PHYSFSX_read_status *status, vms_angvec *angles,
                                               PHYSFS_file *file)
{
	angles->p = PHYSFSX_readFixAngChecked(status, file);
	angles->b = PHYSFSX_readFixAngChecked(status, file);
	angles->h = PHYSFSX_readFixAngChecked(status, file);
}

static inline void PHYSFSX_readMatrixChecked(PHYSFSX_read_status *status, vms_matrix *matrix,
                                             PHYSFS_file *file)
{
	PHYSFSX_readVectorChecked(status, &matrix->rvec, file);
	PHYSFSX_readVectorChecked(status, &matrix->uvec, file);
	PHYSFSX_readVectorChecked(status, &matrix->fvec, file);
}

#endif
