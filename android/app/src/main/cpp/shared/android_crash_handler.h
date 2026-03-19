/*
 * android_crash_handler.h -- native signal handler for crash reporting
 */

#ifndef ANDROID_CRASH_HANDLER_H
#define ANDROID_CRASH_HANDLER_H

#ifdef ANDROID
/* Install signal handlers for SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL.
 * crash_dir is the directory where crash_native_<pid>.txt files are written. */
void android_crash_handler_init(const char *crash_dir);
#endif

#endif /* ANDROID_CRASH_HANDLER_H */
