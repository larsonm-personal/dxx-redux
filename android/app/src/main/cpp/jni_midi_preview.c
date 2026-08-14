/*
 * jni_midi_preview.c -- JNI bridge for MIDI preview and enumeration.
 *
 * Exposes midi_preview.h and midi_enumeration.h to Kotlin.
 * Follows the same pattern as jni_cd_preview.c.
 */

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include "jni_string.h"
#include "midi_preview.h"
#include "midi_enumeration.h"

#define TAG "DXX-MidiPreviewJNI"

/* ── MidiPreviewBridge ───────────────────────────────────────────────── */

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MidiPreviewBridge_nativeInit(
    JNIEnv *env, jclass clazz, jobject assetManager)
{
	AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
	return midi_preview_init(mgr) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MidiPreviewBridge_nativeStart(
    JNIEnv *env, jclass clazz,
    jbyteArray jdata, jboolean isHmp, jint sampleRate)
{
	jsize len;
	jbyte *data;
	int result;
	if (!jdata) return JNI_FALSE;
	len = (*env)->GetArrayLength(env, jdata);
	if ((*env)->ExceptionCheck(env)) return JNI_FALSE;
	data = (*env)->GetByteArrayElements(env, jdata, NULL);
	if (!data || (*env)->ExceptionCheck(env)) return JNI_FALSE;
	result = midi_preview_start((const unsigned char *) data, (int) len,
	                            isHmp ? 1 : 0, (int) sampleRate);
	(*env)->ReleaseByteArrayElements(env, jdata, data, JNI_ABORT);
	return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MidiPreviewBridge_nativeStop(
    JNIEnv *env, jclass clazz)
{
	midi_preview_stop();
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MidiPreviewBridge_nativePause(
    JNIEnv *env, jclass clazz)
{
	midi_preview_pause();
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MidiPreviewBridge_nativeResume(
    JNIEnv *env, jclass clazz)
{
	midi_preview_resume();
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MidiPreviewBridge_nativeSeek(
    JNIEnv *env, jclass clazz, jfloat fraction)
{
	return midi_preview_seek(fraction) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MidiPreviewBridge_nativeGetState(
    JNIEnv *env, jclass clazz)
{
	int pos = 0, dur = 0;
	int state = midi_preview_get_state(&pos, &dur);
	char buf[64];
	snprintf(buf, sizeof(buf), "%d|%d|%d", state, pos, dur);
	return dxx_jni_string_from_utf8(env, buf);
}

/* ── MidiEnumerationBridge ───────────────────────────────────────────── */

JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MidiEnumerationBridge_nativeEnumerateTracks(
    JNIEnv *env, jclass clazz, jstring jfilesDir)
{
	char *files_dir = NULL;
	if (!dxx_jni_string_to_utf8(env, jfilesDir, &files_dir)) return NULL;
	char *json = midi_enumerate_tracks(files_dir);
	free(files_dir);

	jstring result = dxx_jni_string_from_utf8(env, json ? json : "{\"sources\":[]}");
	free(json);
	return result;
}

/* ── HOG entry reader for Kotlin ─────────────────────────────────────── */

JNIEXPORT jbyteArray JNICALL
Java_com_dxxredux_app_MidiPreviewBridge_nativeReadHogEntry(
    JNIEnv *env, jclass clazz,
    jstring jhogPath, jstring jentryName)
{
	char *hog_path = NULL;
	char *entry_name = NULL;
	if (!dxx_jni_string_to_utf8(env, jhogPath, &hog_path)) return NULL;
	if (!dxx_jni_string_to_utf8(env, jentryName, &entry_name)) {
		free(hog_path);
		return NULL;
	}

	unsigned char *data = NULL;
	int data_len = 0;

	/* hog_read_entry declared in midi_preview.c */
	extern int hog_read_entry(const char *hog_path, const char *entry_name,
	                          unsigned char **out_data, int *out_len);

	int ok = hog_read_entry(hog_path, entry_name, &data, &data_len);

	free(entry_name);
	free(hog_path);

	if (!ok || !data) return NULL;

	jbyteArray result = (*env)->NewByteArray(env, data_len);
	if (!result || (*env)->ExceptionCheck(env)) {
		free(data);
		return NULL;
	}
	(*env)->SetByteArrayRegion(env, result, 0, data_len, (jbyte *) data);
	free(data);
	if ((*env)->ExceptionCheck(env)) return NULL;
	return result;
}
