/*
 * Shared JNI overlay helpers — send overlay text to the Kotlin UI layer.
 * Used by both D1 and D2 track_names.c.
 */

#include "android_jni_overlay.h"
#include "overlay_ringbuf.h"

#ifdef ANDROID
#include <jni.h>
extern JavaVM *g_jvm;
extern jobject g_activity;

static void call_java_string_method(const char *method_name, const char *text)
{
	JNIEnv *env = NULL;
	jclass cls = NULL;
	jstring jstr = NULL;
	int attached = 0;
	if (!g_jvm || !g_activity || !method_name || !text) return;
	const jint get_env = (*g_jvm)->GetEnv(g_jvm, (void **) &env, JNI_VERSION_1_6);
	if (get_env == JNI_EDETACHED) {
		if ((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != JNI_OK)
			return;
		attached = 1;
	} else if (get_env != JNI_OK) {
		return;
	}
	cls = (*env)->GetObjectClass(env, g_activity);
	if (cls) {
		jmethodID mid = (*env)->GetMethodID(env, cls, method_name, "(Ljava/lang/String;)V");
		jstr = mid ? (*env)->NewStringUTF(env, text) : NULL;
		if (mid && jstr)
			(*env)->CallVoidMethod(env, g_activity, mid, jstr);
	}
	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
	}
	if (jstr) (*env)->DeleteLocalRef(env, jstr);
	if (cls) (*env)->DeleteLocalRef(env, cls);
	if (attached) (*g_jvm)->DetachCurrentThread(g_jvm);
}

void android_send_track_name(const char *name)
{
	call_java_string_method("showTrackName", name);
}
void android_send_level_name(const char *name)
{
	call_java_string_method("showLevelName", name);
}

void android_send_overlay_line(const char *text)
{
	call_java_string_method("showLevelName", text);
	overlay_ringbuf_add("rewind", text ? text : "");
}

#else
void android_send_track_name(const char *name)
{
	(void) name;
}
void android_send_level_name(const char *name)
{
	(void) name;
}
void android_send_overlay_line(const char *text)
{
	(void) text;
}
#endif
