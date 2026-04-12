/*
 * android_log.c -- centralized per-category debug logging via JNI
 *
 * Sends formatted log lines to DebugLog.kt via MainActivity.debugLogFromNative().
 * Each category has an independent enable flag toggled from the UI.
 *
 * This is the single entry point for all launcher-visible debug logging.
 * Previous separate paths (android_net_log / netLogFromNative) have been
 * merged into debug_log() with integer category IDs.
 */

#ifdef ANDROID

#include "android_log.h"
#include <jni.h>
#include <stdio.h>
#include <stdarg.h>
#include <android/log.h>

extern JavaVM *g_jvm;
extern jobject g_activity;

volatile int debug_log_enabled[DLOG_COUNT] = { 0 };

static const char *category_tags[DLOG_COUNT] = {
	"NETWORK",
	"GRAPHICS",
	"TEXTURE",
	"GAME",
	"LAUNCHER",
};

void debug_log_set_enabled(int category, int on)
{
	if (category >= 0 && category < DLOG_COUNT)
		debug_log_enabled[category] = on ? 1 : 0;
}

void debug_log(int category, const char *fmt, ...)
{
	if (category < 0 || category >= DLOG_COUNT)
		return;
	if (!debug_log_enabled[category])
		return;

	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	/* Also print to logcat for immediate visibility */
	__android_log_print(ANDROID_LOG_DEBUG, "DXX-DLOG",
	                    "[%s] %s", category_tags[category], buf);

	if (!g_jvm || !g_activity)
		return;

	JNIEnv *env;
	int attached = 0;
	if ((*g_jvm)->GetEnv(g_jvm, (void **) &env, JNI_VERSION_1_6) != JNI_OK) {
		if ((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != JNI_OK)
			return;
		attached = 1;
	}

	jclass cls = (*env)->GetObjectClass(env, g_activity);
	jmethodID mid = (*env)->GetMethodID(env, cls, "debugLogFromNative",
	                                    "(ILjava/lang/String;)V");
	if (mid) {
		jstring jmsg = (*env)->NewStringUTF(env, buf);
		(*env)->CallVoidMethod(env, g_activity, mid, (jint) category, jmsg);
		(*env)->DeleteLocalRef(env, jmsg);
	}

	(*env)->DeleteLocalRef(env, cls);
	if (attached) (*g_jvm)->DetachCurrentThread(g_jvm);
}

#endif /* ANDROID */
