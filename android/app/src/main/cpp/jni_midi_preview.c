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
#include "midi_metadata.h"
#include "audio_tag_metadata.h"
#include "music_synth.h"

#define TAG "DXX-MidiPreviewJNI"

/* ── MidiPreviewBridge ───────────────────────────────────────────────── */

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MidiPreviewBridge_nativeInit(
    JNIEnv *env, jclass clazz, jobject assetManager, jstring jpath, jboolean preferFm, jboolean reverb, jboolean chorus, jint equalizer)
{
	AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
	char *path = NULL;
	int result;
	if (!dxx_jni_string_to_utf8(env, jpath, &path)) return JNI_FALSE;
	result = midi_preview_init(mgr, path, preferFm == JNI_TRUE, reverb == JNI_TRUE, chorus == JNI_TRUE, equalizer);
	free(path);
	return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MidiPreviewBridge_nativeGetEq(JNIEnv *env, jclass clazz)
{
	(void) env;
	(void) clazz;
	return midi_preview_get_eq();
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MidiPreviewBridge_nativeValidateSoundfont(JNIEnv *env, jclass clazz, jstring jpath)
{
	char *path = NULL;
	music_synth *synth;
	if (!dxx_jni_string_to_utf8(env, jpath, &path)) return JNI_FALSE;
	synth = *path ? music_synth_load(NULL, path, 0) : NULL;
	free(path);
	if (!synth) return JNI_FALSE;
	music_synth_close(synth);
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MidiPreviewBridge_nativeStart(
    JNIEnv *env, jclass clazz,
    jbyteArray jdata, jboolean isHmp, jint sampleRate, jstring jhog, jstring jsong)
{
	jsize len;
	jbyte *data;
	int result;
	char *hog = NULL, *song = NULL;
	if (!jdata) return JNI_FALSE;
	len = (*env)->GetArrayLength(env, jdata);
	if ((*env)->ExceptionCheck(env)) return JNI_FALSE;
	data = (*env)->GetByteArrayElements(env, jdata, NULL);
	if (!data || (*env)->ExceptionCheck(env)) return JNI_FALSE;
	if (!dxx_jni_string_to_utf8(env, jhog, &hog) || !dxx_jni_string_to_utf8(env, jsong, &song)) {
		free(hog);
		free(song);
		(*env)->ReleaseByteArrayElements(env, jdata, data, JNI_ABORT);
		return JNI_FALSE;
	}
	result = midi_preview_start((const unsigned char *) data, (int) len,
	                            isHmp ? 1 : 0, (int) sampleRate, hog, song);
	free(hog);
	free(song);
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
	snprintf(buf, sizeof(buf), "%d|%d|%d|%s", state, pos, dur, midi_preview_is_fm() ? "ymfm" : "sf2");
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

JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MidiMetadataBridge_nativeParse(
    JNIEnv *env, jclass clazz, jbyteArray jdata, jboolean isHmp,
    jstring jsourceFilename, jboolean inheritedFromMidi)
{
	jsize length;
	jbyte *data;
	char *source_filename = NULL;
	char *json;
	jstring result;
	midi_metadata metadata;
	if (!jdata || !dxx_jni_string_to_utf8(env, jsourceFilename, &source_filename))
		return NULL;
	length = (*env)->GetArrayLength(env, jdata);
	if ((*env)->ExceptionCheck(env)) {
		free(source_filename);
		return NULL;
	}
	data = (*env)->GetByteArrayElements(env, jdata, NULL);
	if (!data || (*env)->ExceptionCheck(env)) {
		free(source_filename);
		return NULL;
	}
	midi_metadata_init(&metadata);
	midi_metadata_parse((const unsigned char *) data, (size_t) length,
	                    isHmp ? 1 : 0, &metadata);
	(*env)->ReleaseByteArrayElements(env, jdata, data, JNI_ABORT);
	json = midi_metadata_to_json(&metadata, source_filename,
	                             inheritedFromMidi ? 1 : 0);
	result = dxx_jni_string_from_utf8(env, json ? json : "{\"parse_status\":\"allocation_error\"}");
	free(json);
	free(source_filename);
	midi_metadata_free(&metadata);
	return result;
}

JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_AudioTagMetadataBridge_nativeParsePath(
    JNIEnv *env, jclass clazz, jstring jpath, jstring jextension)
{
	char *path = NULL;
	char *extension = NULL;
	char *json;
	jstring result;
	audio_tag_metadata metadata;
	if (!dxx_jni_string_to_utf8(env, jpath, &path)) return NULL;
	if (!dxx_jni_string_to_utf8(env, jextension, &extension)) {
		free(path);
		return NULL;
	}
	audio_tag_metadata_init(&metadata);
	audio_tag_metadata_parse_path(path, extension, &metadata);
	json = audio_tag_metadata_to_json(&metadata);
	result = dxx_jni_string_from_utf8(env, json ? json : "{\"parse_status\":\"io_error\"}");
	free(json);
	audio_tag_metadata_free(&metadata);
	free(extension);
	free(path);
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
