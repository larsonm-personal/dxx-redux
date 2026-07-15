/*
 * jni_fingerprint.c -- JNI bridge for Chromaprint fingerprinting.
 *
 * Exposes fingerprint DB loading, audio fingerprinting, and matching
 * to Kotlin via FingerprintBridge.kt.  Used during disc import and
 * audio source scanning to automatically identify tracks.
 */

#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <android/log.h>

#include "chromaprint_db.h"
#include "fingerprint_gen.h"
#include "pcm_decoders.h"
#include "chromaprint.h"

#define TAG       "DXX-Fingerprint"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

/* ── Database loading ────────────────────────────────────────────────── */

/*
 * Set match confidence threshold (0.0-1.0).
 * Called before loading DB, from fingerprint_config.json5 values.
 */
JNIEXPORT void JNICALL
Java_com_dxxredux_app_FingerprintBridge_nativeSetMatchThreshold(
    JNIEnv *env, jclass clazz, jfloat threshold)
{
	chromaprint_db_set_threshold(threshold);
	LOGI("Match threshold set to %.3f", (double) threshold);
}

/*
 * Set duration pre-filter tolerance (0.0-1.0 fraction).
 */
JNIEXPORT void JNICALL
Java_com_dxxredux_app_FingerprintBridge_nativeSetDurationTolerance(
    JNIEnv *env, jclass clazz, jfloat tolerance)
{
	chromaprint_db_set_duration_tolerance(tolerance);
	LOGI("Duration tolerance set to %.3f", (double) tolerance);
}

/*
 * Load the fingerprint database from a JSON array string.
 * The JSON is the flattened known_discs chromaprint data produced by Kotlin.
 * Returns number of entries loaded, or -1 on error.
 */
JNIEXPORT jint JNICALL
Java_com_dxxredux_app_FingerprintBridge_nativeLoadFingerprintDb(
    JNIEnv *env, jclass clazz, jstring jsonData)
{
	const char *json = (*env)->GetStringUTFChars(env, jsonData, NULL);
	if (!json) return -1;

	int len = (int) strlen(json);
	int count = chromaprint_db_load(json, len);

	(*env)->ReleaseStringUTFChars(env, jsonData, json);
	LOGI("Loaded fingerprint DB: %d entries", count);
	return count;
}

/* ── Audio file fingerprinting ───────────────────────────────────────── */

/*
 * Fingerprint an audio file (mp3/ogg/flac).
 * Returns "encoded_fingerprint|duration_ms" or null on failure.
 */
JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_FingerprintBridge_nativeFingerprintAudioFile(
    JNIEnv *env, jclass clazz, jstring filePath)
{
	const char *path = (*env)->GetStringUTFChars(env, filePath, NULL);
	if (!path) return NULL;

	fingerprint_result_t fp = { 0 };
	int rc = fingerprint_from_audio_file(path, &fp);
	(*env)->ReleaseStringUTFChars(env, filePath, path);

	if (rc != 0 || !fp.encoded) {
		fingerprint_free(&fp);
		return NULL;
	}

	/* Pack as "encoded|duration_ms" */
	int buf_len = (int) strlen(fp.encoded) + 32;
	char *buf = (char *) malloc(buf_len);
	if (!buf) {
		fingerprint_free(&fp);
		return NULL;
	}
	snprintf(buf, buf_len, "%s|%d", fp.encoded, fp.duration_ms);

	jstring result = (*env)->NewStringUTF(env, buf);
	free(buf);
	fingerprint_free(&fp);
	return result;
}

/* ── CD-DA track fingerprinting ──────────────────────────────────────── */

/*
 * Fingerprint a CD-DA audio track from a BIN file descriptor.
 *
 * binFd        : open file descriptor for the BIN file
 * startSector  : first audio sector of the track
 * numSectors   : number of audio sectors in the track
 *
 * Returns "encoded_fingerprint|duration_ms" or null on failure.
 */
JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_FingerprintBridge_nativeFingerprintDiscTrack(
    JNIEnv *env, jclass clazz,
    jint binFd, jint startSector, jint numSectors)
{
	if (binFd < 0 || numSectors <= 0) return NULL;

	/* Read audio sectors from the BIN file */
	static const int SECTOR_SIZE = 2352;
	size_t data_size = (size_t) numSectors * SECTOR_SIZE;

	/* Sanity-check: don't allocate more than ~700MB (a full CD) */
	if (data_size > 750 * 1024 * 1024) {
		LOGE("Too many sectors: %d", numSectors);
		return NULL;
	}

	uint8_t *sector_data = (uint8_t *) malloc(data_size);
	if (!sector_data) {
		LOGE("Failed to allocate %zu bytes for sectors", data_size);
		return NULL;
	}

	off_t offset = (off_t) startSector * SECTOR_SIZE;
	if (lseek(binFd, offset, SEEK_SET) != offset) {
		LOGE("Seek failed to sector %d", startSector);
		free(sector_data);
		return NULL;
	}

	size_t total_read = 0;
	while (total_read < data_size) {
		ssize_t n = read(binFd, sector_data + total_read, data_size - total_read);
		if (n <= 0) break;
		total_read += n;
	}
	if (total_read < data_size) {
		LOGW("Short read: got %zu of %zu bytes", total_read, data_size);
		/* Proceed with what we have */
		numSectors = (int) (total_read / SECTOR_SIZE);
		if (numSectors <= 0) {
			free(sector_data);
			return NULL;
		}
	}

	fingerprint_result_t fp = { 0 };
	int rc = fingerprint_from_cd_sectors(sector_data, numSectors, &fp);
	free(sector_data);

	if (rc != 0 || !fp.encoded) {
		fingerprint_free(&fp);
		return NULL;
	}

	int buf_len = (int) strlen(fp.encoded) + 32;
	char *buf = (char *) malloc(buf_len);
	if (!buf) {
		fingerprint_free(&fp);
		return NULL;
	}
	snprintf(buf, buf_len, "%s|%d", fp.encoded, fp.duration_ms);

	jstring result = (*env)->NewStringUTF(env, buf);
	free(buf);
	fingerprint_free(&fp);
	return result;
}

/* ── Matching ────────────────────────────────────────────────────────── */

/*
 * Match a base64-encoded fingerprint against the loaded database.
 *
 * Returns "confidence|name|disc_id|track_num" or null if no match.
 */
JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_FingerprintBridge_nativeMatchFingerprint(
    JNIEnv *env, jclass clazz,
    jstring encodedFp, jint durationMs)
{
	const char *b64 = (*env)->GetStringUTFChars(env, encodedFp, NULL);
	if (!b64) return NULL;

	/* Decode the base64 fingerprint to raw */
	uint32_t *raw_fp = NULL;
	int raw_len = 0;
	int algorithm = 0;
	int ok = chromaprint_decode_fingerprint(b64, (int) strlen(b64),
	                                        &raw_fp, &raw_len,
	                                        &algorithm, 1);
	(*env)->ReleaseStringUTFChars(env, encodedFp, b64);

	if (!ok || !raw_fp) return NULL;

	chromaprint_db_match_t match = { 0 };
	int found = chromaprint_db_match(raw_fp, raw_len, durationMs, &match);
	chromaprint_dealloc(raw_fp);

	if (!found) return NULL;

	char buf[256];
	snprintf(buf, sizeof(buf), "%.4f|%s|%s|%d",
	         match.confidence, match.name, match.disc_id, match.track_num);

	return (*env)->NewStringUTF(env, buf);
}

/*
 * Convenience: fingerprint an audio file AND match it in one call.
 * Returns "confidence|name|disc_id|track_num" or null if no match.
 */
JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_FingerprintBridge_nativeFingerprintAndMatch(
    JNIEnv *env, jclass clazz, jstring filePath)
{
	const char *path = (*env)->GetStringUTFChars(env, filePath, NULL);
	if (!path) return NULL;

	fingerprint_result_t fp = { 0 };
	int rc = fingerprint_from_audio_file(path, &fp);
	(*env)->ReleaseStringUTFChars(env, filePath, path);

	if (rc != 0 || !fp.raw_fp) {
		fingerprint_free(&fp);
		return NULL;
	}

	chromaprint_db_match_t match = { 0 };
	int found = chromaprint_db_match(fp.raw_fp, fp.fp_len, fp.duration_ms, &match);
	fingerprint_free(&fp);

	if (!found) return NULL;

	char buf[256];
	snprintf(buf, sizeof(buf), "%.4f|%s|%s|%d",
	         match.confidence, match.name, match.disc_id, match.track_num);

	return (*env)->NewStringUTF(env, buf);
}

/* ── DB info ─────────────────────────────────────────────────────────── */

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_FingerprintBridge_nativeGetDbCount(
    JNIEnv *env, jclass clazz)
{
	return chromaprint_db_count();
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_FingerprintBridge_nativeFreeDb(
    JNIEnv *env, jclass clazz)
{
	chromaprint_db_free();
}
