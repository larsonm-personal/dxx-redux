#ifndef D1_IN_D2_SAVE_FORMAT_H
#define D1_IN_D2_SAVE_FORMAT_H

/* Version 40 first binds original/custom/extension asset definitions */
#ifdef __ANDROID__
#define D1_IN_D2_SAVE_VERSION 44
#else
#define D1_IN_D2_SAVE_VERSION 42
#endif

static inline int d1_in_d2_save_version_supported(int version)
{
	return version == 40 || version == 42 || version == 44;
}

#endif
