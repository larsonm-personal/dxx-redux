/*
 * jni_music_control.c -- JNI bridge for in-game track control.
 *
 * Uses the unified songs_*() API so next/prev/play/info work across
 * all music types (BUILTIN/MIDI, REDBOOK, CUSTOM/jukebox).
 * Falls back to RBA* for Redbook-specific queries (track names, types).
 */

#include <jni.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <android/log.h>

#include "songs.h"
#include "rbaudio.h"
#include "config.h"
#include "digi.h"

#define TAG       "DXX-MusicCtrl"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeNextTrack(JNIEnv *env, jobject thiz)
{
	return songs_next_track();
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativePrevTrack(JNIEnv *env, jobject thiz)
{
	return songs_prev_track();
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativePlaySpecificTrack(
    JNIEnv *env, jobject thiz, jint track)
{
	return songs_play_specific_track(track);
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
 * Unified current track info: "musicType|trackIndex|totalTracks|trackName"
 * Works for BUILTIN, REDBOOK, and CUSTOM music types.
 * Returns empty string if nothing is playing.
 */
JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetCurrentTrackInfo(
    JNIEnv *env, jobject thiz)
{
	int type = 0, track = 0, total = 0;
	char name[128] = "";
	char buf[256];

	if (songs_get_track_info(&type, &track, &total, name, sizeof(name)) == 0) {
		snprintf(buf, sizeof(buf), "%d|%d|%d|%s", type, track, total, name);
	} else {
		buf[0] = '\0';
	}
	return (*env)->NewStringUTF(env, buf);
}

/* Returns GameCfg.MusicType (0=none, 1=builtin, 2=redbook, 3=custom). */
JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetMusicType(
    JNIEnv *env, jobject thiz)
{
	return GameCfg.MusicType;
}

/* Returns JSON array of playable tracks for the track picker popup. */
JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetTrackList(
    JNIEnv *env, jobject thiz)
{
	char *buf = (char *) malloc(32768);
	jstring result;
	(void) thiz;
	if (!buf)
		return (*env)->NewStringUTF(env, "[]");
	songs_get_track_list(buf, 32768);
	result = (*env)->NewStringUTF(env, buf);
	free(buf);
	return result;
}
