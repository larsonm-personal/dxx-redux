/*
 * android_crash_handler.c -- breadcrumb storage for xCrash integration
 */

#ifdef ANDROID

#include "android_crash_handler.h"
#include <jni.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <android/log.h>

#define TAG "CrashHandler"

static char s_crash_dir[512];
static int s_initialized = 0;

/* Breadcrumb ring buffer for crash diagnostics */
#define CRUMB_COUNT 64
#define CRUMB_LEN   128

static char s_crumbs[CRUMB_COUNT][CRUMB_LEN];
static volatile int s_crumb_next = 0;

void crash_breadcrumb(const char *msg)
{
	int seq = __atomic_fetch_add(&s_crumb_next, 1, __ATOMIC_RELAXED);
	int idx = seq % CRUMB_COUNT;
	strncpy(s_crumbs[idx], msg, CRUMB_LEN - 1);
	s_crumbs[idx][CRUMB_LEN - 1] = '\0';
	__android_log_print(ANDROID_LOG_DEBUG, "DXX-CRUMB", "%s", msg);
}

void crash_breadcrumb_v(const char *fmt, ...)
{
	char buf[CRUMB_LEN];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	crash_breadcrumb(buf);
}

static char *append_text(char *cursor, size_t *remaining, const char *text)
{
	int written;

	if (*remaining == 0)
		return cursor;

	written = snprintf(cursor, *remaining, "%s", text);
	if (written < 0)
		return cursor;
	if ((size_t) written >= *remaining) {
		cursor += *remaining - 1;
		*remaining = 1;
		return cursor;
	}

	cursor += written;
	*remaining -= (size_t) written;
	return cursor;
}

static char *append_line(char *cursor, size_t *remaining, int seq, const char *msg)
{
	int written;

	if (*remaining == 0)
		return cursor;

	written = snprintf(cursor, *remaining, "[%d] %s\n", seq, msg);
	if (written < 0)
		return cursor;
	if ((size_t) written >= *remaining) {
		cursor += *remaining - 1;
		*remaining = 1;
		return cursor;
	}

	cursor += written;
	*remaining -= (size_t) written;
	return cursor;
}

static void format_breadcrumbs(char *buf, size_t buflen)
{
	int total;
	int start;
	int i;
	char *cursor = buf;
	size_t remaining = buflen;

	if (!buf || buflen == 0)
		return;

	buf[0] = '\0';
	total = __atomic_load_n(&s_crumb_next, __ATOMIC_ACQUIRE);
	start = (total > CRUMB_COUNT) ? total - CRUMB_COUNT : 0;
	if (total == 0)
		return;

	cursor = append_text(cursor, &remaining, "Breadcrumbs (oldest first):\n");
	for (i = start; i < total; i++) {
		int idx = i % CRUMB_COUNT;
		cursor = append_line(cursor, &remaining, i, s_crumbs[idx]);
	}
}

void android_crash_handler_init(const char *crash_dir, const char *install_info)
{
	(void) install_info;

	if (!crash_dir || strlen(crash_dir) >= sizeof(s_crash_dir) - 1) {
		__android_log_print(ANDROID_LOG_ERROR, TAG,
		                    "crash_dir path too long or null");
		return;
	}
	strncpy(s_crash_dir, crash_dir, sizeof(s_crash_dir) - 1);
	s_crash_dir[sizeof(s_crash_dir) - 1] = '\0';
	s_initialized = 1;

	__android_log_print(ANDROID_LOG_INFO, TAG,
	                    "Native breadcrumb storage initialized (dir=%s)", crash_dir);
}

const char *android_crash_handler_get_dir(void)
{
	return s_initialized ? s_crash_dir : NULL;
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_CrashLog_nativeInstallCrashHandler(JNIEnv *env, jobject thiz,
                                                         jstring crash_dir,
                                                         jstring install_info)
{
	(void) thiz;
	const char *dir = (*env)->GetStringUTFChars(env, crash_dir, NULL);
	const char *info = install_info ? (*env)->GetStringUTFChars(env, install_info, NULL) : NULL;
	if (dir) {
		android_crash_handler_init(dir, info);
		(*env)->ReleaseStringUTFChars(env, crash_dir, dir);
	}
	if (info)
		(*env)->ReleaseStringUTFChars(env, install_info, info);
}

JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_CrashLog_nativeGetBreadcrumbReport(JNIEnv *env, jobject thiz)
{
	char buf[CRUMB_COUNT * (CRUMB_LEN + 16) + 64];

	(void) thiz;
	format_breadcrumbs(buf, sizeof(buf));
	return (*env)->NewStringUTF(env, buf);
}

#endif /* ANDROID */
