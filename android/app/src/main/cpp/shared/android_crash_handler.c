/*
 * android_crash_handler.c -- breadcrumb storage for xCrash integration
 */

#ifdef ANDROID

#include "android_crash_handler.h"
#include <jni.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <android/log.h>

#define TAG               "CrashHandler"
#define SNAPSHOT_PATH_LEN 640

static char s_crash_dir[512];
static char s_install_header[2048];
static char s_snapshot_path[SNAPSHOT_PATH_LEN];
static int s_initialized = 0;
static pthread_mutex_t s_snapshot_lock = PTHREAD_MUTEX_INITIALIZER;

/* Breadcrumb ring buffer for crash diagnostics */
#define CRUMB_COUNT                64
#define CRUMB_LEN                  128
#define SNAPSHOT_BUF_LEN           (CRUMB_COUNT * (CRUMB_LEN + 16) + 64)
#define SNAPSHOT_REWRITE_THRESHOLD (SNAPSHOT_BUF_LEN - (CRUMB_LEN + 32))

static char s_crumbs[CRUMB_COUNT][CRUMB_LEN];
static volatile int s_crumb_next = 0;

static void persist_breadcrumb_snapshot(int seq, const char *msg);

static void write_text(int fd, const char *text)
{
	const char *cursor = text;
	size_t remaining;
	ssize_t written;

	if (fd < 0 || !text)
		return;
	remaining = strlen(text);
	while (remaining > 0) {
		written = write(fd, cursor, remaining);
		if (written <= 0)
			return;
		cursor += written;
		remaining -= (size_t) written;
	}
}

void crash_breadcrumb(const char *msg)
{
	int seq = __atomic_fetch_add(&s_crumb_next, 1, __ATOMIC_RELAXED);
	int idx = seq % CRUMB_COUNT;
	strncpy(s_crumbs[idx], msg, CRUMB_LEN - 1);
	s_crumbs[idx][CRUMB_LEN - 1] = '\0';
	persist_breadcrumb_snapshot(seq, s_crumbs[idx]);
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

static void rewrite_breadcrumb_snapshot_locked(void)
{
	char buf[SNAPSHOT_BUF_LEN];
	int fd;

	if (!s_initialized || !s_snapshot_path[0])
		return;
	format_breadcrumbs(buf, sizeof(buf));
	fd = open(s_snapshot_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
		return;
	if (buf[0])
		write_text(fd, buf);
	close(fd);
}

static void persist_breadcrumb_snapshot(int seq, const char *msg)
{
	char line[CRUMB_LEN + 32];
	int fd;
	off_t end;
	int written;

	if (!s_initialized || !s_snapshot_path[0] || !msg || !msg[0])
		return;
	pthread_mutex_lock(&s_snapshot_lock);
	fd = open(s_snapshot_path, O_WRONLY | O_CREAT, 0644);
	if (fd < 0)
		goto done;
	end = lseek(fd, 0, SEEK_END);
	if (end < 0) {
		close(fd);
		goto done;
	}
	if (end >= SNAPSHOT_REWRITE_THRESHOLD) {
		close(fd);
		rewrite_breadcrumb_snapshot_locked();
		goto done;
	}
	if (end == 0)
		write_text(fd, "Breadcrumbs (oldest first):\n");
	written = snprintf(line, sizeof(line), "[%d] %s\n", seq, msg);
	if (written > 0)
		write(fd, line, (size_t) ((written < (int) sizeof(line)) ? written : (int) sizeof(line) - 1));
	close(fd);
done:
	pthread_mutex_unlock(&s_snapshot_lock);
}

void android_crash_handler_init(const char *crash_dir, const char *install_header)
{
	if (!crash_dir || strlen(crash_dir) >= sizeof(s_crash_dir) - 1) {
		__android_log_print(ANDROID_LOG_ERROR, TAG,
		                    "crash_dir path too long or null");
		return;
	}
	strncpy(s_crash_dir, crash_dir, sizeof(s_crash_dir) - 1);
	s_crash_dir[sizeof(s_crash_dir) - 1] = '\0';
	if (install_header) {
		strncpy(s_install_header, install_header, sizeof(s_install_header) - 1);
		s_install_header[sizeof(s_install_header) - 1] = '\0';
	} else {
		s_install_header[0] = '\0';
	}
	snprintf(s_snapshot_path, sizeof(s_snapshot_path), "%s/crash_breadcrumbs_latest.txt", crash_dir);
	unlink(s_snapshot_path);
	s_initialized = 1;

	__android_log_print(ANDROID_LOG_INFO, TAG,
	                    "Native breadcrumb storage initialized (dir=%s)", crash_dir);
}

const char *android_crash_handler_get_dir(void)
{
	return s_initialized ? s_crash_dir : NULL;
}

const char *android_crash_handler_get_header(void)
{
	return (s_initialized && s_install_header[0]) ? s_install_header : NULL;
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_CrashLog_nativeInstallCrashHandler(JNIEnv *env, jobject thiz,
                                                         jstring crash_dir,
                                                         jstring install_header)
{
	(void) thiz;
	const char *dir = (*env)->GetStringUTFChars(env, crash_dir, NULL);
	const char *header = install_header ? (*env)->GetStringUTFChars(env, install_header, NULL) : NULL;
	if (dir) {
		android_crash_handler_init(dir, header);
		(*env)->ReleaseStringUTFChars(env, crash_dir, dir);
	}
	if (header)
		(*env)->ReleaseStringUTFChars(env, install_header, header);
}

JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_CrashLog_nativeGetBreadcrumbReport(JNIEnv *env, jobject thiz)
{
	char buf[CRUMB_COUNT * (CRUMB_LEN + 16) + 64];

	(void) thiz;
	format_breadcrumbs(buf, sizeof(buf));
	return (*env)->NewStringUTF(env, buf);
}

JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_CrashLog_nativeGetInstalledHeader(JNIEnv *env, jobject thiz)
{
	const char *header;

	(void) thiz;
	header = android_crash_handler_get_header();
	if (!header)
		return NULL;
	return (*env)->NewStringUTF(env, header);
}

#endif /* ANDROID */
