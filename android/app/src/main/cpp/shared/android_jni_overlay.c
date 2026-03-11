/*
 * Shared JNI overlay helpers — send overlay text to the Kotlin UI layer.
 * Used by both D1 and D2 track_names.c.
 */

#include "android_jni_overlay.h"

#ifdef ANDROID
#include <jni.h>
extern JavaVM  *g_jvm;
extern jobject  g_activity;

static void call_java_string_method(const char *method_name, const char *text)
{
	JNIEnv *env;
	int attached = 0;
	if (!g_jvm || !g_activity) return;
	if ((*g_jvm)->GetEnv(g_jvm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) {
		(*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);
		attached = 1;
	}
	jclass cls = (*env)->GetObjectClass(env, g_activity);
	jmethodID mid = (*env)->GetMethodID(env, cls, method_name, "(Ljava/lang/String;)V");
	if (mid) {
		jstring jstr = (*env)->NewStringUTF(env, text);
		(*env)->CallVoidMethod(env, g_activity, mid, jstr);
		(*env)->DeleteLocalRef(env, jstr);
	}
	if (attached) (*g_jvm)->DetachCurrentThread(g_jvm);
}

void android_send_track_name(const char *name) { call_java_string_method("showTrackName", name); }
void android_send_level_name(const char *name) { call_java_string_method("showLevelName", name); }

#else
void android_send_track_name(const char *name) { (void)name; }
void android_send_level_name(const char *name) { (void)name; }
#endif
