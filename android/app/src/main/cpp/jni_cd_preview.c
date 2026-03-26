/*
 * jni_cd_preview.c -- JNI bridge for CD audio preview in the launcher.
 *
 * Exposes cd_preview.h functions to CdPreviewBridge.kt.
 * Called from SetupActivity/MusicPickerPage (not from the game).
 */

#include <jni.h>
#include <stdio.h>
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
