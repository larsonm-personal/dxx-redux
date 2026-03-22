/*
 * jni_music_control.c — JNI bridge for in-game track control.
 *
 * Exposes RBANextTrack/PrevTrack/PlaySpecificTrack and track info
 * queries to the Kotlin overlay (TouchOverlayView / MusicControlPanel).
 * All functions target MainActivity where the native library is loaded.
 */

#include <jni.h>
#include <stdio.h>
#include <string.h>
#include <android/log.h>

#include "rbaudio.h"

#define TAG       "DXX-MusicCtrl"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeNextTrack(JNIEnv *env, jobject thiz)
{
	return RBANextTrack();
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativePrevTrack(JNIEnv *env, jobject thiz)
{
	return RBAPrevTrack();
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativePlaySpecificTrack(
    JNIEnv *env, jobject thiz, jint track)
{
	return RBAPlaySpecificTrack(track);
}

JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetTrackName(
    JNIEnv *env, jobject thiz, jint track)
{
	const char *name = RBAGetTrackName(track);
	return (*env)->NewStringUTF(env, name ? name : "");
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetCurrentTrackNum(
    JNIEnv *env, jobject thiz)
{
	return RBAGetTrackNum();
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetNumAudioTracks(
    JNIEnv *env, jobject thiz)
{
	return RBAGetNumAudioTracks();
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetTotalTracks(
    JNIEnv *env, jobject thiz)
{
	return RBAGetNumberOfTracks();
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeIsAudioTrack(
    JNIEnv *env, jobject thiz, jint track)
{
	return RBAIsAudioTrack(track) ? JNI_TRUE : JNI_FALSE;
}

/*
 * Get full current track info as a string: "trackNum|sourceIndex|trackName"
 * Returns empty string if not playing.
 */
JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetCurrentTrackInfo(
    JNIEnv *env, jobject thiz)
{
	int track = 0, source = 0;
	char name[64] = "";
	char buf[128];

	if (RBAGetCurrentTrackInfo(&track, name, sizeof(name), &source) == 0) {
		snprintf(buf, sizeof(buf), "%d|%d|%s", track, source, name);
	} else {
		buf[0] = '\0';
	}
	return (*env)->NewStringUTF(env, buf);
}
