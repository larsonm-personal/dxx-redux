/*
 * android_crash_handler.h -- native signal handler for crash reporting
 */

#ifndef ANDROID_CRASH_HANDLER_H
#define ANDROID_CRASH_HANDLER_H

#ifdef ANDROID
/* Initialize crash breadcrumb storage.
 * crash_dir is retained so Error() can write non-signal fatal errors into the
 * same directory as xCrash tombstones. install_header is cached so degraded
 * crash paths can still report immutable app/build/device metadata. */
void android_crash_handler_init(const char *crash_dir, const char *install_header);

/* Return the crash directory path set by android_crash_handler_init(),
 * or NULL if not yet initialized.  Used by Error() to write crash files
 * from normal (non-signal) context. */
const char *android_crash_handler_get_dir(void);

/* Return the install-time immutable header, or NULL if not yet initialized. */
const char *android_crash_handler_get_header(void);

/* Breadcrumb ring buffer -- records last N diagnostic markers.
 * Dumped into xCrash tombstones via JNI. Writers may come from more
 * than one thread; ordering is best-effort. */
void crash_breadcrumb(const char *msg);
void crash_breadcrumb_v(const char *fmt, ...);

/* Clean fatal exit: finish the Activity via JNI, then _exit(1).
 * Called by Error() instead of raw exit(1) so the Activity doesn't
 * freeze on the last rendered frame. */
void android_finish_and_exit(const char *message);
void android_fatal_error_exit(const char *message) __attribute__((noreturn));

/* Normal-context restore diagnostics, also surfaced in Advanced crash reports */
void android_restore_discard(const char *section, const char *reason,
                             unsigned int id, int signature, int object_index, int powerup);
void android_engine_session_begin(void);
void android_engine_expected_exit(const char *reason);
void android_engine_session_returned(void);
void android_restore_begin(const char *game, const char *filename, int level, int host, int visible);
void android_restore_phase(const char *phase);
void android_restore_metadata(const char *mission, int level, unsigned int checksum);
void android_restore_gear_summary(unsigned int pickups_kept, unsigned int pickups_discarded,
                                  unsigned int recovery_kept, unsigned int recovery_discarded);
void android_restore_finished(int success, int visible);

/* android port: notify Kotlin layer that this client has become the new host
 * after the original host disconnected.  Starts LAN broadcasting so the
 * old host can find and rejoin the migrated game. */
void android_notify_host_migration(void);
#else
#define android_restore_discard(...) ((void) 0)
#endif

#endif /* ANDROID_CRASH_HANDLER_H */
