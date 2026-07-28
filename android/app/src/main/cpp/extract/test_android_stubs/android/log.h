#ifndef DXX_TEST_ANDROID_LOG_H
#define DXX_TEST_ANDROID_LOG_H

#define ANDROID_LOG_INFO  4
#define ANDROID_LOG_WARN  5
#define ANDROID_LOG_ERROR 6

static inline int __android_log_print(int priority, const char *tag,
                                      const char *format, ...)
{
	(void) priority;
	(void) tag;
	(void) format;
	return 0;
}

#endif
