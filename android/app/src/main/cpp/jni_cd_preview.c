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

#define TAG "DXX-CdPreviewJNI"

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_CdPreviewBridge_nativeStart(
    JNIEnv *env, jclass clazz,
    jstring jbin_path, jstring jcue_path,
    jint audio_track, jint sample_rate)
{
	const char *bin = (*env)->GetStringUTFChars(env, jbin_path, NULL);
	const char *cue = (*env)->GetStringUTFChars(env, jcue_path, NULL);
	int result = cd_preview_start(bin, cue, audio_track, sample_rate);
	(*env)->ReleaseStringUTFChars(env, jcue_path, cue);
	(*env)->ReleaseStringUTFChars(env, jbin_path, bin);
	return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_CdPreviewBridge_nativeStartMulti(
    JNIEnv *env, jclass clazz,
    jobjectArray jbin_paths, jstring jcue_path,
    jint audio_track, jint sample_rate)
{
	const char *cue;
	const char **bins;
	jstring *jbins;
	jsize count;
	int i;
	int result = 0;

	if (!jbin_paths || !jcue_path) return JNI_FALSE;
	count = (*env)->GetArrayLength(env, jbin_paths);
	if (count <= 0) return JNI_FALSE;
	bins = (const char **) calloc((size_t) count, sizeof(*bins));
	jbins = (jstring *) calloc((size_t) count, sizeof(*jbins));
	if (!bins || !jbins) {
		free(bins);
		free(jbins);
		return JNI_FALSE;
	}
	cue = (*env)->GetStringUTFChars(env, jcue_path, NULL);
	if (!cue) {
		free(bins);
		free(jbins);
		return JNI_FALSE;
	}

	for (i = 0; i < count; i++) {
		jbins[i] = (jstring) (*env)->GetObjectArrayElement(env, jbin_paths, i);
		if (!jbins[i]) break;
		bins[i] = (*env)->GetStringUTFChars(env, jbins[i], NULL);
		if (!bins[i]) break;
	}

	if (i == count)
		result = cd_preview_start_multi(bins, (int) count, cue, audio_track, sample_rate);

	for (i = 0; i < count; i++) {
		if (jbins[i] && bins[i])
			(*env)->ReleaseStringUTFChars(env, jbins[i], bins[i]);
		if (jbins[i])
			(*env)->DeleteLocalRef(env, jbins[i]);
	}
	(*env)->ReleaseStringUTFChars(env, jcue_path, cue);
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
	const char *cue = (*env)->GetStringUTFChars(env, jcue_path, NULL);
	int result = cd_preview_start_fd(fd, cue, audio_track, sample_rate);
	(*env)->ReleaseStringUTFChars(env, jcue_path, cue);
	return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_CdPreviewBridge_nativeStartMultiFd(
    JNIEnv *env, jclass clazz,
    jintArray jfds, jstring jcue_path,
    jint audio_track, jint sample_rate)
{
	const char *cue;
	jint *fds;
	jsize count;
	int result;

	if (!jfds || !jcue_path) return JNI_FALSE;
	count = (*env)->GetArrayLength(env, jfds);
	if (count <= 0) return JNI_FALSE;
	fds = (*env)->GetIntArrayElements(env, jfds, NULL);
	if (!fds) return JNI_FALSE;
	cue = (*env)->GetStringUTFChars(env, jcue_path, NULL);
	if (!cue) {
		(*env)->ReleaseIntArrayElements(env, jfds, fds, JNI_ABORT);
		return JNI_FALSE;
	}
	result = cd_preview_start_multi_fd((const int *) fds, (int) count, cue, audio_track, sample_rate);
	(*env)->ReleaseStringUTFChars(env, jcue_path, cue);
	(*env)->ReleaseIntArrayElements(env, jfds, fds, JNI_ABORT);
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
	return (*env)->NewStringUTF(env, buf);
}
