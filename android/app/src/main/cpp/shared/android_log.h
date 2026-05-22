/*
 * android_log.h -- centralized debug logging from C to Kotlin
 *
 * All launcher-visible debug logging goes through debug_log().
 * Categories are defined in debug_log_categories.h (shared with Kotlin).
 * Convenience macros (COOPLOG, etc.) are defined here so they aren't
 * duplicated across multiple source files.
 */
#ifndef ANDROID_LOG_H
#define ANDROID_LOG_H

#ifdef ANDROID

#include "debug_log_categories.h"

/* Arm a short input-ignore window when an Android cutscene starts so
 * touch/controller taps from gameplay do not skip it immediately. */
void android_arm_cutscene_tap_suppress(void);

/* Per-category enabled flags (set from Kotlin via JNI). */
extern volatile int debug_log_enabled[DLOG_COUNT];

/* Format and send a log line if the category is enabled.
 * Inexpensive no-op when disabled (flag check only). */
void debug_log(int category, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/* Send a newline-delimited payload to Kotlin as one batched write. */
void debug_log_batch(int category, const char *payload);

/* Called from JNI when the user toggles a category checkbox. */
void debug_log_set_enabled(int category, int on);

/* Log to both con_printf (logcat / DLOG_GAME) and DLOG_NETWORK.
 * Callers must also include console.h for con_printf. */
#define COOPLOG(fmt, ...)                                       \
	do {                                                        \
		char _cl_buf[256];                                      \
		snprintf(_cl_buf, sizeof(_cl_buf), fmt, ##__VA_ARGS__); \
		con_printf(CON_NORMAL, "%s", _cl_buf);                  \
		debug_log(DLOG_NETWORK, "[COOP] %s", _cl_buf);          \
	} while (0)

#else
/* No-op on non-Android builds */
#define debug_log(...)       ((void) 0)
#define debug_log_batch(...) ((void) 0)
#define COOPLOG(fmt, ...)    ((void) 0)
#endif

#endif /* ANDROID_LOG_H */
