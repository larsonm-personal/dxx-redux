/*
 * android_net_log.h -- C-callable bridge to Kotlin NetLog
 *
 * Lets native code write to the user-exportable network log
 * (filesDir/netlogs/netlog_*.txt) via JNI.
 */
#ifndef ANDROID_NET_LOG_H
#define ANDROID_NET_LOG_H

#ifdef ANDROID

/* Log a line to the Kotlin NetLog system.
 * category: short tag like "MPDIAG", "NET", etc.
 * message:  free-form text (no newline needed). */
void android_net_log(const char *category, const char *message);

#endif /* ANDROID */
#endif /* ANDROID_NET_LOG_H */
