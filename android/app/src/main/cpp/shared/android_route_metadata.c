#include "android_route_metadata.h"
#include "android_log.h"
#include "secretarea.h"

#include <jni.h>
#include <stdatomic.h>

extern JavaVM *g_jvm;
extern jobject g_activity;
static atomic_int Android_route_metadata_request_generation;
static atomic_int Android_route_metadata_progress_permille;
static atomic_int Android_route_metadata_progress_state;
static atomic_flag Android_route_metadata_progress_lock = ATOMIC_FLAG_INIT;

static void android_route_metadata_lock_progress(void)
{
	while (atomic_flag_test_and_set_explicit(
	    &Android_route_metadata_progress_lock, memory_order_acquire));
}

static void android_route_metadata_unlock_progress(void)
{
	atomic_flag_clear_explicit(
	    &Android_route_metadata_progress_lock, memory_order_release);
}

void android_route_metadata_invalidate_pending(void)
{
	android_route_metadata_lock_progress();
	atomic_fetch_add(&Android_route_metadata_request_generation, 1);
	atomic_store(&Android_route_metadata_progress_permille, 0);
	atomic_store(&Android_route_metadata_progress_state,
	             ANDROID_ROUTE_METADATA_CALCULATING);
	android_route_metadata_unlock_progress();
}

int android_route_metadata_get_progress_permille(void)
{
	return atomic_load(&Android_route_metadata_progress_permille);
}

int android_route_metadata_get_progress_state(void)
{
	return atomic_load(&Android_route_metadata_progress_state);
}

int android_route_metadata_get_request_generation(void)
{
	return atomic_load(&Android_route_metadata_request_generation);
}

void android_route_metadata_request(
    const char *game,
    const char *mission,
    int level_num,
    const char *level_file,
    const char *const *normal_level_files,
    int normal_level_count,
    const char *const *secret_level_files,
    const int *secret_entry_levels,
    int secret_level_count)
{
	JNIEnv *env = NULL;
	jclass cls;
	jmethodID method;
	jstring jgame;
	jstring jmission;
	jstring jlevel_file;
	jstring jroute_readiness;
	jclass string_class;
	jobjectArray jnormal_level_files = NULL;
	jobjectArray jsecret_level_files = NULL;
	jintArray jsecret_entry_levels = NULL;
	int request_generation;
	int attached = 0;

	if (!g_jvm || !g_activity || !game || !level_file)
		return;
	if ((*g_jvm)->GetEnv(g_jvm, (void **) &env, JNI_VERSION_1_6) != JNI_OK) {
		if ((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != JNI_OK)
			return;
		attached = 1;
	}
	cls = (*env)->GetObjectClass(env, g_activity);
	android_route_metadata_lock_progress();
	request_generation = atomic_fetch_add(
	                         &Android_route_metadata_request_generation, 1) +
	                     1;
	atomic_store(&Android_route_metadata_progress_permille, 0);
	atomic_store(&Android_route_metadata_progress_state,
	             ANDROID_ROUTE_METADATA_CALCULATING);
	android_route_metadata_unlock_progress();
	debug_log(DLOG_PROFILING,
	          "route_metadata request generation=%d game=%s mission=%s level=%d file=%s",
	          request_generation, game, mission ? mission : "", level_num,
	          level_file);
	method = cls ? (*env)->GetMethodID(
	                   env, cls, "onRouteMetadataNeeded",
	                   "(Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;"
	                   "[Ljava/lang/String;[Ljava/lang/String;[II)V")
	             : NULL;
	jgame = (*env)->NewStringUTF(env, game);
	jmission = (*env)->NewStringUTF(env, mission ? mission : "");
	jlevel_file = (*env)->NewStringUTF(env, level_file);
	jroute_readiness = (*env)->NewStringUTF(
	    env, level_metadata_route_readiness_name(
	             level_metadata_get_route_readiness()));
	string_class = (*env)->FindClass(env, "java/lang/String");
	if (string_class && normal_level_count >= 0)
		jnormal_level_files = (*env)->NewObjectArray(
		    env, normal_level_count, string_class, NULL);
	if (string_class && secret_level_count >= 0)
		jsecret_level_files = (*env)->NewObjectArray(
		    env, secret_level_count, string_class, NULL);
	if (secret_level_count >= 0)
		jsecret_entry_levels = (*env)->NewIntArray(env, secret_level_count);
	if (jnormal_level_files)
		for (int i = 0; i < normal_level_count; ++i) {
			jstring value = (*env)->NewStringUTF(
			    env, normal_level_files && normal_level_files[i] ? normal_level_files[i] : "");
			if (value) {
				(*env)->SetObjectArrayElement(env, jnormal_level_files, i, value);
				(*env)->DeleteLocalRef(env, value);
			}
		}
	if (jsecret_level_files)
		for (int i = 0; i < secret_level_count; ++i) {
			jstring value = (*env)->NewStringUTF(
			    env, secret_level_files && secret_level_files[i] ? secret_level_files[i] : "");
			if (value) {
				(*env)->SetObjectArrayElement(env, jsecret_level_files, i, value);
				(*env)->DeleteLocalRef(env, value);
			}
		}
	if (jsecret_entry_levels && secret_entry_levels)
		(*env)->SetIntArrayRegion(
		    env, jsecret_entry_levels, 0, secret_level_count,
		    (const jint *) secret_entry_levels);
	if (method && jgame && jmission && jlevel_file && jroute_readiness &&
	    jnormal_level_files && jsecret_level_files && jsecret_entry_levels)
		(*env)->CallVoidMethod(
		    env, g_activity, method, jgame, jmission, (jint) level_num,
		    jlevel_file, jroute_readiness, jnormal_level_files,
		    jsecret_level_files,
		    jsecret_entry_levels, (jint) request_generation);
	if (jsecret_entry_levels)
		(*env)->DeleteLocalRef(env, jsecret_entry_levels);
	if (jsecret_level_files)
		(*env)->DeleteLocalRef(env, jsecret_level_files);
	if (jnormal_level_files)
		(*env)->DeleteLocalRef(env, jnormal_level_files);
	if (string_class)
		(*env)->DeleteLocalRef(env, string_class);
	if (jroute_readiness)
		(*env)->DeleteLocalRef(env, jroute_readiness);
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

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeNotifyRouteMetadataFinished(
    JNIEnv *env,
    jobject activity,
    jint request_generation,
    jboolean success)
{
	int current_generation;
	int accepted;

	(void) env;
	(void) activity;
	android_route_metadata_lock_progress();
	current_generation =
	    atomic_load(&Android_route_metadata_request_generation);
	accepted = request_generation == current_generation;
	if (accepted) {
		if (success == JNI_TRUE) {
			if (atomic_load(&Android_route_metadata_progress_state) ==
			    ANDROID_ROUTE_METADATA_CALCULATING)
				atomic_store(&Android_route_metadata_progress_state,
				             ANDROID_ROUTE_METADATA_USEFUL);
		} else if (atomic_load(&Android_route_metadata_progress_state) ==
		           ANDROID_ROUTE_METADATA_CALCULATING) {
			atomic_store(&Android_route_metadata_progress_state,
			             ANDROID_ROUTE_METADATA_FAILED);
		}
	}
	android_route_metadata_unlock_progress();
	debug_log(DLOG_PROFILING,
	          "route_metadata result generation=%d current=%d success=%d accepted=%d",
	          (int) request_generation, current_generation, success == JNI_TRUE,
	          accepted);
	if (!accepted)
		return;
	level_metadata_note_background_result(success == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeNotifyRouteMetadataProgress(
    JNIEnv *env,
    jobject activity,
    jint request_generation,
    jint estimated_permille,
    jint state)
{
	android_route_metadata_progress_update update;
	int current_generation;
	int current_permille;
	int current_state;

	(void) env;
	(void) activity;
	android_route_metadata_lock_progress();
	current_generation =
	    atomic_load(&Android_route_metadata_request_generation);
	current_permille =
	    atomic_load(&Android_route_metadata_progress_permille);
	current_state =
	    atomic_load(&Android_route_metadata_progress_state);
	update = android_route_metadata_progress_policy(
	    request_generation, current_generation, current_permille,
	    current_state, estimated_permille, state);
	if (update.accepted) {
		atomic_store(&Android_route_metadata_progress_permille, update.permille);
		atomic_store(&Android_route_metadata_progress_state, update.state);
	}
	android_route_metadata_unlock_progress();
}
