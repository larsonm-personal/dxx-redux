#ifndef D1_IN_D2_SAVE_FORMAT_H
#define D1_IN_D2_SAVE_FORMAT_H

/* Version 40 first binds original/custom/extension asset definitions */
#ifdef __ANDROID__
#include "android_save_format.h"
#define D1_IN_D2_SAVE_VERSION ANDROID_D1_IN_D2_SAVE_VERSION
#else
#define D1_IN_D2_SAVE_VERSION 48
#endif

static inline int d1_in_d2_save_version_supported(int version)
{
#ifndef __ANDROID__
	/* Native version 42 already binds the same asset identity */
	if (version == 42 || version == 46) return 1;
#endif
	return version == D1_IN_D2_SAVE_VERSION;
}

#endif
