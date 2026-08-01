#ifndef DXX_ANDROID_SURFACE_LIFECYCLE_H
#define DXX_ANDROID_SURFACE_LIFECYCLE_H

#ifdef ANDROID

#include <stdint.h>

#include <android/native_window.h>

struct android_surface_snapshot {
	ANativeWindow *window;
	uint64_t generation;
	int paused;
};

void android_surface_acquire_snapshot(struct android_surface_snapshot *snapshot);
void android_surface_release_snapshot(struct android_surface_snapshot *snapshot);

#endif

#endif
