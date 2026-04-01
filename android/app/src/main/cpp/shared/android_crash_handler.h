/*
 * android_crash_handler.h -- native signal handler for crash reporting
 */

#ifndef ANDROID_CRASH_HANDLER_H
#define ANDROID_CRASH_HANDLER_H

#ifdef ANDROID
/* Install signal handlers for SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL.
 * crash_dir is the directory where crash_native_<pid>.txt files are written. */
void android_crash_handler_init(const char *crash_dir);

/* Return the crash directory path set by android_crash_handler_init(),
 * or NULL if not yet initialized.  Used by Error() to write crash files
 * from normal (non-signal) context. */
const char *android_crash_handler_get_dir(void);

/* Breadcrumb ring buffer -- records last N diagnostic markers.
 * Dumped into the crash file on signal.  Thread-safe for a single
 * writer (game thread) since the signal handler only reads when that
 * thread is stopped. */
void crash_breadcrumb(const char *msg);
void crash_breadcrumb_v(const char *fmt, ...);
#endif

#endif /* ANDROID_CRASH_HANDLER_H */
