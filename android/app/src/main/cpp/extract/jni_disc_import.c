/*
 * jni_disc_import.c — JNI bridge for BIN/CUE disc import operations.
 *
 * Exposes the CUE parser and ISO 9660 reader to Kotlin via
 * DiscImportBridge.kt.  Used by SetupActivity during the CD image
 * import flow.
 */

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <android/log.h>

#include "cue_parser.h"
#include "cd_read_contract.h"
#include "hfs_reader.h"
#include "iso9660_reader.h"
#include "mac_hfs_extract.h"
#include "sow_extract.h"
#include "stuffit_extract.h"
#include "game_file_extensions.h"
#include "jni_string.h"

#define TAG       "DXX-DiscImport"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static jobjectArray build_iso_listing_array(JNIEnv *env, const iso_file_list_t *list)
{
	jclass strClass;
	jobjectArray result;
	int file_count = 0;
	int idx = 0;

	for (int i = 0; i < list->num_files; i++)
		if (!list->files[i].is_dir) file_count++;

	strClass = (*env)->FindClass(env, "java/lang/String");
	if (!strClass || (*env)->ExceptionCheck(env)) return NULL;
	result = (*env)->NewObjectArray(env, file_count, strClass, NULL);
	if (!result || (*env)->ExceptionCheck(env)) return NULL;
	(*env)->DeleteLocalRef(env, strClass);
	for (int i = 0; i < list->num_files; i++) {
		char buf[ISO_PATH_LEN + 32];
		jstring s;

		if (list->files[i].is_dir) continue;
		snprintf(buf, sizeof(buf), "%s|%u", list->files[i].path, list->files[i].size);
		s = dxx_jni_string_from_utf8(env, buf);
		if (!s) return NULL;
		(*env)->SetObjectArrayElement(env, result, idx++, s);
		if ((*env)->ExceptionCheck(env)) {
			(*env)->DeleteLocalRef(env, s);
			return NULL;
		}
		(*env)->DeleteLocalRef(env, s);
	}

	return result;
}

/* ── CUE parsing ─────────────────────────────────────────────────────── */

/* Parse one exact CUE snapshot and return one pipe-delimited row per track. */
JNIEXPORT jobjectArray JNICALL
Java_com_dxxredux_app_DiscImportBridge_nativeParseCueRows(
    JNIEnv *env, jclass clazz,
    jstring cuePath, jlongArray binSizes)
{
	char *path = NULL;
	char *cue_text = NULL;
	long long *sizes = NULL;
	int num_sizes = 0;
	jlong *jsizes = NULL;
	cue_disc_t disc;
	memset(&disc, 0, sizeof(disc));

	if (!dxx_jni_string_to_utf8(env, cuePath, &path)) return NULL;
	if (!cd_read_file_exact(path, CD_CUE_MAX_BYTES, &cue_text, NULL)) {
		LOGE("Cannot read complete CUE file");
		free(path);
		return NULL;
	}
	free(path);

	if (!binSizes || (num_sizes = (*env)->GetArrayLength(env, binSizes)) <= 0) {
		free(cue_text);
		return NULL;
	}
	jsizes = (*env)->GetLongArrayElements(env, binSizes, NULL);
	if (!jsizes) {
		free(cue_text);
		return NULL;
	}
	sizes = (long long *) malloc((size_t) num_sizes * sizeof(*sizes));
	if (!sizes) {
		(*env)->ReleaseLongArrayElements(env, binSizes, jsizes, JNI_ABORT);
		free(cue_text);
		return NULL;
	}
	for (int i = 0; i < num_sizes; i++) sizes[i] = (long long) jsizes[i];
	(*env)->ReleaseLongArrayElements(env, binSizes, jsizes, JNI_ABORT);

	int n = cue_parse(cue_text, sizes, num_sizes, &disc);
	free(cue_text);
	if (n <= 0) {
		free(sizes);
		return NULL;
	}
	for (int i = 0; i < n; i++) {
		const cue_track_info_t *track = &disc.tracks[i];
		if (track->file_index < 0 || track->file_index >= num_sizes ||
		    !cd_track_span_with_stride(track->start_sector, track->num_sectors,
		                               track->sector_size,
		                               sizes[track->file_index], NULL, NULL)) {
			LOGE("Invalid or incomplete span for track %d", track->track_num);
			free(sizes);
			return NULL;
		}
	}
	free(sizes);

	jclass strClass = (*env)->FindClass(env, "java/lang/String");
	if (!strClass) return NULL;
	jobjectArray rows = (*env)->NewObjectArray(env, n, strClass, NULL);
	(*env)->DeleteLocalRef(env, strClass);
	if (!rows) return NULL;
	for (int i = 0; i < n; i++) {
		const cue_track_info_t *track = &disc.tracks[i];
		size_t row_size = strlen(track->title) + 128;
		char *row = (char *) malloc(row_size);
		if (!row) return NULL;
		snprintf(row, row_size, "%d|%d|%d|%d|%d|%d|%d|%d|%s", track->track_num,
		         track->type, track->sector_mode, track->sector_size,
		         track->user_data_offset, track->file_index, track->start_sector,
		         track->num_sectors, track->title);
		jstring value = dxx_jni_string_from_utf8(env, row);
		free(row);
		if (!value) return NULL;
		(*env)->SetObjectArrayElement(env, rows, i, value);
		if ((*env)->ExceptionCheck(env)) {
			(*env)->DeleteLocalRef(env, value);
			return NULL;
		}
		(*env)->DeleteLocalRef(env, value);
	}
	LOGI("Parsed complete CUE: %d tracks, %d files", n, disc.num_files);
	return rows;
}

/* ── ISO 9660 listing ────────────────────────────────────────────────── */

/*
 * List files on an ISO 9660 data track.
 *
 * binFd          : open file descriptor for the BIN file
 * trackStart     : start sector of the data track
 * trackSectors   : number of sectors in the data track
 *
 * Returns a String array of "path|size" entries.
 */
JNIEXPORT jobjectArray JNICALL
Java_com_dxxredux_app_DiscImportBridge_nativeListIsoFiles(
    JNIEnv *env, jclass clazz,
    jint binFd, jint trackStart, jint trackSectors,
    jint sectorStride, jint userDataOffset)
{
	if (binFd < 0) {
		LOGE("nativeListIsoFiles: invalid binFd %d", binFd);
		return NULL;
	}

	iso_file_list_t *list = iso_file_list_create();
	if (!list) return NULL;

	int n = iso_list_track_files(binFd, trackStart, trackSectors,
	                             sectorStride, userDataOffset, list);
	if (n < 0) {
		LOGE("iso_list_files failed");
		iso_file_list_destroy(list);
		return NULL;
	}

	jobjectArray result = build_iso_listing_array(env, list);
	iso_file_list_destroy(list);
	LOGI("Listed %d ISO entries from BIN track", n);
	return result;
}

JNIEXPORT jobjectArray JNICALL
Java_com_dxxredux_app_DiscImportBridge_nativeListIsoImageFiles(
    JNIEnv *env, jclass clazz,
    jint isoFd)
{
	iso_file_list_t *list;
	int n;
	jobjectArray result;

	if (isoFd < 0) {
		LOGE("nativeListIsoImageFiles: invalid isoFd %d", isoFd);
		return NULL;
	}

	list = iso_file_list_create();
	if (!list) return NULL;
	n = iso_list_image_files(isoFd, list);
	if (n < 0) {
		LOGE("iso_list_image_files failed");
		iso_file_list_destroy(list);
		return NULL;
	}

	LOGI("Listed %d files from ISO image", n);
	result = build_iso_listing_array(env, list);
	iso_file_list_destroy(list);
	return result;
}

/* ── ISO 9660 extraction ─────────────────────────────────────────────── */

/* Progress callback state — calls through JNI to a Kotlin lambda */
typedef struct {
	JNIEnv *env;
	jobject callback; /* DiscImportBridge.ExtractProgress instance */
	jmethodID on_progress;
	dxx_extract_attempt_budget_t *budget;
} extract_ctx_t;

static int init_extract_ctx(JNIEnv *env, jobject progress,
                            dxx_extract_attempt_budget_t *budget,
                            extract_ctx_t *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->env = env;
	ctx->budget = budget;
	if (progress) {
		jclass cls;

		cls = (*env)->GetObjectClass(env, progress);
		if (!cls || (*env)->ExceptionCheck(env)) return 0;
		ctx->on_progress = (*env)->GetMethodID(env, cls,
		                                       "onProgress", "(Ljava/lang/String;JJ)I");
		if (!ctx->on_progress || (*env)->ExceptionCheck(env)) return 0;
		(*env)->DeleteLocalRef(env, cls);
		ctx->callback = progress;
	}
	return 1;
}

static int extract_progress_cb(const char *current_file,
                               long long bytes_done, long long bytes_total,
                               void *user_data)
{
	extract_ctx_t *ctx = (extract_ctx_t *) user_data;
	if (!ctx->callback) return 0;

	jstring jfile = dxx_jni_string_from_utf8(ctx->env, current_file);
	if (!jfile) return 1;
	jint cancel = (*ctx->env)->CallIntMethod(ctx->env, ctx->callback,
	                                         ctx->on_progress,
	                                         jfile,
	                                         (jlong) bytes_done,
	                                         (jlong) bytes_total);
	(*ctx->env)->DeleteLocalRef(ctx->env, jfile);
	if ((*ctx->env)->ExceptionCheck(ctx->env))
		return 1;
	if (cancel != 0)
		dxx_extract_attempt_cancel(ctx->budget);
	return (int) cancel;
}

static int init_attempt_budget(JNIEnv *env, jlongArray state,
                               dxx_extract_attempt_budget_t *budget)
{
	jlong values[3];

	if (!state || (*env)->GetArrayLength(env, state) != 3)
		return 0;
	(*env)->GetLongArrayRegion(env, state, 0, 3, values);
	if ((*env)->ExceptionCheck(env) || values[0] < 0 || values[1] < 0 ||
	    values[0] > (jlong) DXX_EXTRACT_MAX_TOTAL_BYTES ||
	    values[1] > (jlong) DXX_EXTRACT_MAX_ENTRIES)
		return 0;
	dxx_extract_attempt_budget_init(budget, NULL, NULL);
	budget->output_bytes = (uint64_t) values[0];
	budget->entries = (uint64_t) values[1];
	budget->cancelled = values[2] != 0;
	return 1;
}

static void store_attempt_budget(JNIEnv *env, jlongArray state,
                                 const dxx_extract_attempt_budget_t *budget)
{
	jlong values[3];

	values[0] = (jlong) budget->output_bytes;
	values[1] = (jlong) budget->entries;
	values[2] = budget->cancelled ? 1 : 0;
	(*env)->SetLongArrayRegion(env, state, 0, 3, values);
}

/*
 * Extract game files from an ISO 9660 data track.
 *
 * binFd       : open file descriptor for the BIN file
 * trackStart  : start sector of the data track
 * trackSectors: number of sectors in the data track
 * outputDir   : directory to extract files into
 * progress    : optional ExtractProgress callback object (may be null)
 *
 * Returns number of files extracted, or -1 on error.
 */
JNIEXPORT jint JNICALL
Java_com_dxxredux_app_DiscImportBridge_nativeExtractIsoFiles(
    JNIEnv *env, jclass clazz,
    jint binFd, jint trackStart, jint trackSectors,
    jint sectorStride, jint userDataOffset,
    jstring outputDir, jobject progress, jlongArray attemptState)
{
	dxx_extract_attempt_budget_t budget;
	if (binFd < 0) {
		LOGE("nativeExtractIsoFiles: invalid binFd %d", binFd);
		return -1;
	}
	if (!init_attempt_budget(env, attemptState, &budget))
		return -1;

	char *out_dir;
	if (!dxx_jni_string_to_utf8(env, outputDir, &out_dir)) return -1;

	/* List files first */
	iso_file_list_t *list = iso_file_list_create();
	if (!list) {
		free(out_dir);
		return -1;
	}
	int n = iso_list_track_files(binFd, trackStart, trackSectors,
	                             sectorStride, userDataOffset, list);
	if (n < 0) {
		iso_file_list_destroy(list);
		free(out_dir);
		return -1;
	}

	/* Set up progress callback */
	extract_ctx_t ctx;
	if (!init_extract_ctx(env, progress, &budget, &ctx)) {
		iso_file_list_destroy(list);
		free(out_dir);
		return -1;
	}

	int extracted = iso_extract_track_files_with_budget(binFd, trackStart, trackSectors,
	                                                    sectorStride, userDataOffset,
	                                                    list, out_dir, dxx_android_disc_extract_extensions,
	                                                    progress ? extract_progress_cb : NULL,
	                                                    &ctx, &budget);

	store_attempt_budget(env, attemptState, &budget);
	iso_file_list_destroy(list);
	free(out_dir);
	LOGI("Extracted %d files", extracted);
	return extracted;
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_DiscImportBridge_nativeExtractIsoImageFiles(
    JNIEnv *env, jclass clazz,
    jint isoFd,
    jstring outputDir, jobject progress, jlongArray attemptState)
{
	dxx_extract_attempt_budget_t budget;
	char *out_dir;
	iso_file_list_t *list;
	int n;
	extract_ctx_t ctx;
	int extracted;

	if (isoFd < 0) {
		LOGE("nativeExtractIsoImageFiles: invalid isoFd %d", isoFd);
		return -1;
	}
	if (!init_attempt_budget(env, attemptState, &budget))
		return -1;

	if (!dxx_jni_string_to_utf8(env, outputDir, &out_dir)) return -1;

	list = iso_file_list_create();
	if (!list) {
		free(out_dir);
		return -1;
	}
	n = iso_list_image_files(isoFd, list);
	if (n < 0) {
		iso_file_list_destroy(list);
		free(out_dir);
		return -1;
	}

	if (!init_extract_ctx(env, progress, &budget, &ctx)) {
		iso_file_list_destroy(list);
		free(out_dir);
		return -1;
	}
	extracted = iso_extract_image_files_with_budget(
	    isoFd, list, out_dir, dxx_android_disc_extract_extensions,
	    progress ? extract_progress_cb : NULL, &ctx, &budget);

	store_attempt_budget(env, attemptState, &budget);
	iso_file_list_destroy(list);
	free(out_dir);
	LOGI("Extracted %d files from ISO image", extracted);
	return extracted;
}

/* ── SOW (ARJ) archive scanning ──────────────────────────────────────── */

/*
 * Scan a directory tree for .sow files.
 *
 * dirPath: filesystem path to scan recursively
 *
 * Returns a String array of absolute .sow file paths, or null on error.
 */
JNIEXPORT jobjectArray JNICALL
Java_com_dxxredux_app_DiscImportBridge_nativeScanSowFiles(
    JNIEnv *env, jclass clazz,
    jstring dirPath)
{
	char *dir;
	if (!dxx_jni_string_to_utf8(env, dirPath, &dir)) return NULL;

	sow_file_list_t list;
	int n = sow_scan_dir(dir, &list);
	free(dir);

	if (n < 0) {
		LOGE("sow_scan_dir failed");
		return NULL;
	}
	if (n == 0) {
		LOGI("No .sow files found");
		/* Return empty array */
		jclass strClass = (*env)->FindClass(env, "java/lang/String");
		if (!strClass || (*env)->ExceptionCheck(env)) return NULL;
		jobjectArray empty = (*env)->NewObjectArray(env, 0, strClass, NULL);
		if (!empty || (*env)->ExceptionCheck(env)) return NULL;
		return empty;
	}

	jclass strClass = (*env)->FindClass(env, "java/lang/String");
	if (!strClass || (*env)->ExceptionCheck(env)) return NULL;
	jobjectArray result = (*env)->NewObjectArray(env, list.count, strClass, NULL);
	if (!result || (*env)->ExceptionCheck(env)) return NULL;
	for (int i = 0; i < list.count; i++) {
		jstring s = dxx_jni_string_from_utf8(env, list.paths[i]);
		if (!s) return NULL;
		(*env)->SetObjectArrayElement(env, result, i, s);
		if ((*env)->ExceptionCheck(env)) {
			(*env)->DeleteLocalRef(env, s);
			return NULL;
		}
		(*env)->DeleteLocalRef(env, s);
	}

	LOGI("Found %d .sow file(s)", list.count);
	return result;
}

/* ── SOW (ARJ) archive extraction ────────────────────────────────────── */

/*
 * Extract game files from a .sow (ARJ) archive.
 *
 * sowPath    : filesystem path to the .sow file
 * outputDir  : directory to extract files into
 * progress   : optional ExtractProgress callback (may be null)
 *
 * Returns number of files extracted, or -1 on error.
 */
JNIEXPORT jint JNICALL
Java_com_dxxredux_app_DiscImportBridge_nativeExtractSowFiles(
    JNIEnv *env, jclass clazz,
    jstring sowPath, jstring outputDir, jobject progress,
    jboolean appendExisting, jlongArray attemptState)
{
	dxx_extract_attempt_budget_t budget;
	char *sow;
	char *out_dir;
	if (!dxx_jni_string_to_utf8(env, sowPath, &sow)) return -1;
	if (!init_attempt_budget(env, attemptState, &budget)) {
		free(sow);
		return -1;
	}

	if (!dxx_jni_string_to_utf8(env, outputDir, &out_dir)) {
		free(sow);
		return -1;
	}

	/* Set up progress callback */
	extract_ctx_t ctx;
	if (!init_extract_ctx(env, progress, &budget, &ctx)) {
		free(sow);
		free(out_dir);
		return -1;
	}

	int extracted = sow_extract_with_budget(sow, out_dir, NULL,
	                                        progress ? extract_progress_cb : NULL,
	                                        &ctx, appendExisting == JNI_TRUE,
	                                        &budget);
	store_attempt_budget(env, attemptState, &budget);
	LOGI("SOW extracted %d files from %s (append=%s)", extracted, sow,
	     appendExisting == JNI_TRUE ? "true" : "false");

	free(sow);
	free(out_dir);
	return extracted;
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_DiscImportBridge_nativeExtractStuffitFiles(
    JNIEnv *env, jclass clazz,
    jstring sitPath, jstring outputDir, jobject progress)
{
	char *sit;
	char *out_dir;
	if (!dxx_jni_string_to_utf8(env, sitPath, &sit)) return -1;

	if (!dxx_jni_string_to_utf8(env, outputDir, &out_dir)) {
		free(sit);
		return -1;
	}

	extract_ctx_t ctx;
	if (!init_extract_ctx(env, progress, NULL, &ctx)) {
		free(sit);
		free(out_dir);
		return -1;
	}

	int extracted = stuffit_extract(sit, out_dir,
	                                dxx_android_mac_disc_extract_extensions,
	                                progress ? extract_progress_cb : NULL, &ctx);
	LOGI("StuffIt extracted %d files from %s", extracted, sit);

	free(sit);
	free(out_dir);
	return extracted;
}

/* ── Mac HFS/STi2 extraction ─────────────────────────────────────────── */

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_DiscImportBridge_nativeExtractMacFiles(
    JNIEnv *env, jclass clazz,
    jint binFd, jint trackStart, jint trackSectors,
    jstring outputDir, jobject progress, jlongArray attemptState)
{
	dxx_extract_attempt_budget_t budget;
	extract_ctx_t ctx;
	hfs_partition_info_t hfs_info;
	char *out_dir;
	int extracted;

	if (binFd < 0) {
		LOGE("nativeExtractMacFiles: invalid binFd %d", binFd);
		return -1;
	}
	if (!init_attempt_budget(env, attemptState, &budget))
		return -1;

	if (!dxx_jni_string_to_utf8(env, outputDir, &out_dir))
		return -1;
	if (!init_extract_ctx(env, progress, &budget, &ctx)) {
		free(out_dir);
		return -1;
	}

	if (hfs_find_partition(binFd, trackStart, trackSectors, &hfs_info) < 0) {
		free(out_dir);
		return -1;
	}

	extracted = mac_extract_files_from_hfs_track_with_budget(binFd, trackStart, trackSectors,
	                                                         out_dir,
	                                                         dxx_android_mac_disc_extract_extensions,
	                                                         dxx_android_mac_disc_extract_extensions,
	                                                         progress ? extract_progress_cb : NULL,
	                                                         &ctx, &budget);

	store_attempt_budget(env, attemptState, &budget);
	LOGI("Mac import extracted %d files from HFS volume '%s'", extracted,
	     hfs_info.volume_name[0] ? hfs_info.volume_name : hfs_info.partition_name);

	free(out_dir);
	return extracted;
}
