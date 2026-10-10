#ifndef ANDROID_SAVE_PROBE_H
#define ANDROID_SAVE_PROBE_H

#include <stdint.h>

/* Android sequential laps for profiling and explicit automation probes */
#ifdef __ANDROID__
typedef struct android_save_probe_lap {
	const char *name;
	int64_t wall_us;
	int64_t cpu_us;
} android_save_probe_lap;

void android_save_probe_begin(void);
int android_save_probe_begin_if_idle(void);
void android_save_probe_mark(const char *name);
int android_save_probe_end(const android_save_probe_lap **laps);
#else
#define android_save_probe_mark(name) ((void) 0)
#endif

#endif
