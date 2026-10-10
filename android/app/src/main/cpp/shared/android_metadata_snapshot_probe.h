#ifndef ANDROID_METADATA_SNAPSHOT_PROBE_H
#define ANDROID_METADATA_SNAPSHOT_PROBE_H

#include <stddef.h>
#include <stdint.h>

/* Explicit Android D2 automation experiment, never used by normal saving */
typedef struct android_metadata_snapshot_result {
	size_t native_bytes, encoded_bytes, pool_bytes;
	int64_t capture_wall_us, capture_cpu_us;
	int64_t direct_wall_us, direct_cpu_us;
	int64_t worker_wall_us, worker_cpu_us;
	int equal, roundtrip_equal, live_changed, atomic_lock_free;
} android_metadata_snapshot_result;

int android_metadata_snapshot_reset(void);
int android_metadata_snapshot_start(int64_t epoch);
int android_metadata_snapshot_finish(android_metadata_snapshot_result *result);
void android_metadata_snapshot_discard(void);

#endif
