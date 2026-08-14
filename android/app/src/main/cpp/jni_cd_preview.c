/*
 * jni_cd_preview.c -- JNI bridge for CD audio preview in the launcher.
 *
 * Exposes cd_preview.h functions to CdPreviewBridge.kt.
 * Called from SetupActivity/MusicPickerPage (not from the game).
 */

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <android/log.h>
#include "cd_preview.h"
#include "jni_string.h"

#define TAG "DXX-CdPreviewJNI"

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_CdPreviewBridge_nativeStart(
    JNIEnv *env, jclass clazz,
    jstring jbin_path, jstring jcue_path,
    jint audio_track, jint sample_rate)
{
	char *bin = NULL;
	char *cue = NULL;
	if (!dxx_jni_string_to_utf8(env, jbin_path, &bin)) return JNI_FALSE;
	if (!dxx_jni_string_to_utf8(env, jcue_path, &cue)) {
		free(bin);
		return JNI_FALSE;
	}
	int result = cd_preview_start(bin, cue, audio_track, sample_rate);
	free(cue);
	free(bin);
	return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_CdPreviewBridge_nativeStartMulti(
    JNIEnv *env, jclass clazz,
    jobjectArray jbin_paths, jstring jcue_path,
    jint audio_track, jint sample_rate)
{
	char *cue = NULL;
	char **bins;
	jstring *jbins;
	jsize count;
	int i;
	int result = 0;

	if (!jbin_paths || !jcue_path) return JNI_FALSE;
	count = (*env)->GetArrayLength(env, jbin_paths);
	if (count <= 0) return JNI_FALSE;
	bins = (char **) calloc((size_t) count, sizeof(*bins));
	jbins = (jstring *) calloc((size_t) count, sizeof(*jbins));
	if (!bins || !jbins) {
		free(bins);
		free(jbins);
		return JNI_FALSE;
	}
	if (!dxx_jni_string_to_utf8(env, jcue_path, &cue)) {
		free(bins);
		free(jbins);
		return JNI_FALSE;
	}

	for (i = 0; i < count; i++) {
		jbins[i] = (jstring) (*env)->GetObjectArrayElement(env, jbin_paths, i);
		if (!jbins[i] || (*env)->ExceptionCheck(env)) break;
		if (!dxx_jni_string_to_utf8(env, jbins[i], &bins[i])) break;
	}

	if (i == count)
		result = cd_preview_start_multi((const char *const *) bins, (int) count,
		                                cue, audio_track, sample_rate);

	for (i = 0; i < count; i++) {
		free(bins[i]);
		if (jbins[i] && !(*env)->ExceptionCheck(env))
			(*env)->DeleteLocalRef(env, jbins[i]);
	}
	free(cue);
	free(bins);
	free(jbins);
	return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_CdPreviewBridge_nativeStartFd(
    JNIEnv *env, jclass clazz,
    jint fd, jstring jcue_path,
    jint audio_track, jint sample_rate)
{
	char *cue = NULL;
	if (!dxx_jni_string_to_utf8(env, jcue_path, &cue)) return JNI_FALSE;
	int result = cd_preview_start_fd(fd, cue, audio_track, sample_rate);
	free(cue);
	return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_CdPreviewBridge_nativeStartMultiFd(
    JNIEnv *env, jclass clazz,
    jintArray jfds, jstring jcue_path,
    jint audio_track, jint sample_rate)
{
	char *cue = NULL;
	jint *fds;
	jsize count;
	int result;

	if (!jfds || !jcue_path) return JNI_FALSE;
	count = (*env)->GetArrayLength(env, jfds);
	if ((*env)->ExceptionCheck(env)) return JNI_FALSE;
	if (count <= 0) return JNI_FALSE;
	fds = (jint *) malloc((size_t) count * sizeof(*fds));
	if (!fds) return JNI_FALSE;
	(*env)->GetIntArrayRegion(env, jfds, 0, count, fds);
	if ((*env)->ExceptionCheck(env)) {
		free(fds);
		return JNI_FALSE;
	}
	if (!dxx_jni_string_to_utf8(env, jcue_path, &cue)) {
		free(fds);
		return JNI_FALSE;
	}
	result = cd_preview_start_multi_fd((const int *) fds, (int) count, cue, audio_track, sample_rate);
	free(cue);
	free(fds);
	return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_CdPreviewBridge_nativeStop(
    JNIEnv *env, jclass clazz)
{
	cd_preview_stop();
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_CdPreviewBridge_nativePause(
    JNIEnv *env, jclass clazz)
{
	cd_preview_pause();
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_CdPreviewBridge_nativeResume(
    JNIEnv *env, jclass clazz)
{
	cd_preview_resume();
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_CdPreviewBridge_nativeSeek(
    JNIEnv *env, jclass clazz, jfloat fraction)
{
	return cd_preview_seek(fraction) ? JNI_TRUE : JNI_FALSE;
}

/*
 * Returns "state|position_ms|duration_ms"
 *   state: 1=playing, -1=paused, 0=stopped
 */
JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_CdPreviewBridge_nativeGetState(
    JNIEnv *env, jclass clazz)
{
	int pos = 0, dur = 0;
	int state = cd_preview_get_state(&pos, &dur);
	char buf[64];
	snprintf(buf, sizeof(buf), "%d|%d|%d", state, pos, dur);
	return dxx_jni_string_from_utf8(env, buf);
}
