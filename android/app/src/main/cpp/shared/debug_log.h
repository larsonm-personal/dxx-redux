/*
 * debug_log.h -- per-category debug logging from C to Kotlin
 *
 * Provides a lightweight macro that checks a per-category enabled flag
 * before formatting a message and sending it via JNI to the Kotlin
 * DebugLog system.  Categories are defined in debug_log_categories.h
 * (shared with Kotlin).
 */
#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#ifdef ANDROID

#include "debug_log_categories.h"

/* Per-category enabled flags (set from Kotlin via JNI). */
extern volatile int debug_log_enabled[DLOG_COUNT];

/* Format and send a log line if the category is enabled.
 * Inexpensive no-op when disabled (flag check only). */
void debug_log(int category, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/* Called from JNI when the user toggles a category checkbox. */
void debug_log_set_enabled(int category, int on);

#else
/* No-op on non-Android builds */
#define debug_log(...) ((void) 0)
#endif

#endif /* DEBUG_LOG_H */
