/*
 * android_crash_handler.h -- native signal handler for crash reporting
 */

#ifndef ANDROID_CRASH_HANDLER_H
#define ANDROID_CRASH_HANDLER_H

#ifdef ANDROID
/* Initialize crash breadcrumb storage.
 * crash_dir is retained so Error() can write non-signal fatal errors into the
 * same directory as xCrash tombstones. install_info is currently unused. */
void android_crash_handler_init(const char *crash_dir, const char *install_info);

/* Return the crash directory path set by android_crash_handler_init(),
 * or NULL if not yet initialized.  Used by Error() to write crash files
 * from normal (non-signal) context. */
const char *android_crash_handler_get_dir(void);

/* Breadcrumb ring buffer -- records last N diagnostic markers.
 * Dumped into xCrash tombstones via JNI. Writers may come from more
 * than one thread; ordering is best-effort. */
void crash_breadcrumb(const char *msg);
void crash_breadcrumb_v(const char *fmt, ...);

/* Clean fatal exit: finish the Activity via JNI, then _exit(1).
 * Called by Error() instead of raw exit(1) so the Activity doesn't
 * freeze on the last rendered frame. */
void android_finish_and_exit(void);

/* android port: notify Kotlin layer that this client has become the new host
 * after the original host disconnected.  Starts LAN broadcasting so the
 * old host can find and rejoin the migrated game. */
void android_notify_host_migration(void);
#endif

#endif /* ANDROID_CRASH_HANDLER_H */
