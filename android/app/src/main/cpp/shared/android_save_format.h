#ifndef ANDROID_SAVE_FORMAT_H
#define ANDROID_SAVE_FORMAT_H

/* Android save formats are pre-release: accept only the current format */
#define ANDROID_D1_SAVE_VERSION       20
#define ANDROID_D2_SAVE_VERSION       43
#define ANDROID_D1_IN_D2_SAVE_VERSION 44

static inline int android_save_version_supported(int version, int d1_engine, int d1_mission)
{
	return version == (d1_engine ? ANDROID_D1_SAVE_VERSION : d1_mission ? ANDROID_D1_IN_D2_SAVE_VERSION
	                                                                    : ANDROID_D2_SAVE_VERSION);
}

#endif
