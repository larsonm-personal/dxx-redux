/*
 * android_net_log.c -- JNI bridge from C to Kotlin NetLog
 *
 * Calls MainActivity.netLogFromNative(category, message) which delegates
 * to NetLog.log().  Uses the same g_jvm / g_activity globals as the
 * other JNI helpers (android_jni_overlay.c, jni_saf.c, etc.).
 */

#ifdef ANDROID

#include "android_net_log.h"
#include <jni.h>
#include <android/log.h>

extern JavaVM *g_jvm;
extern jobject g_activity;

void android_net_log(const char *category, const char *message)
{
	JNIEnv *env;
	int attached = 0;

	if (!g_jvm || !g_activity) return;
	if ((*g_jvm)->GetEnv(g_jvm, (void **) &env, JNI_VERSION_1_6) != JNI_OK) {
		if ((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != JNI_OK)
			return;
		attached = 1;
	}

	jclass cls = (*env)->GetObjectClass(env, g_activity);
	jmethodID mid = (*env)->GetMethodID(env, cls, "netLogFromNative",
	                                    "(Ljava/lang/String;Ljava/lang/String;)V");
	if (mid) {
		jstring jcat = (*env)->NewStringUTF(env, category);
		jstring jmsg = (*env)->NewStringUTF(env, message);
		(*env)->CallVoidMethod(env, g_activity, mid, jcat, jmsg);
		(*env)->DeleteLocalRef(env, jmsg);
		(*env)->DeleteLocalRef(env, jcat);
	}

	(*env)->DeleteLocalRef(env, cls);
	if (attached) (*g_jvm)->DetachCurrentThread(g_jvm);
}

#endif /* ANDROID */
