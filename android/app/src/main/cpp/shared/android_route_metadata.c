#include "android_route_metadata.h"

#include <jni.h>

extern JavaVM *g_jvm;
extern jobject g_activity;

void android_route_metadata_request(
    const char *game,
    const char *mission,
    int level_num,
    const char *level_file)
{
	JNIEnv *env = NULL;
	jclass cls;
	jmethodID method;
	jstring jgame;
	jstring jmission;
	jstring jlevel_file;
	int attached = 0;

	if (!g_jvm || !g_activity || !game || !level_file)
		return;
	if ((*g_jvm)->GetEnv(g_jvm, (void **) &env, JNI_VERSION_1_6) != JNI_OK) {
		if ((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != JNI_OK)
			return;
		attached = 1;
	}
	cls = (*env)->GetObjectClass(env, g_activity);
	method = cls ? (*env)->GetMethodID(
	                   env, cls, "onRouteMetadataNeeded",
	                   "(Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;)V")
	             : NULL;
	jgame = (*env)->NewStringUTF(env, game);
	jmission = (*env)->NewStringUTF(env, mission ? mission : "");
	jlevel_file = (*env)->NewStringUTF(env, level_file);
	if (method && jgame && jmission && jlevel_file)
		(*env)->CallVoidMethod(
		    env, g_activity, method, jgame, jmission, (jint) level_num,
		    jlevel_file);
	if (jlevel_file)
		(*env)->DeleteLocalRef(env, jlevel_file);
	if (jmission)
		(*env)->DeleteLocalRef(env, jmission);
	if (jgame)
		(*env)->DeleteLocalRef(env, jgame);
	if (cls)
		(*env)->DeleteLocalRef(env, cls);
	if (attached)
		(*g_jvm)->DetachCurrentThread(g_jvm);
}
