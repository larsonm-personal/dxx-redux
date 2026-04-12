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
#include "hfs_reader.h"
#include "iso9660_reader.h"
#include "sti2_extract.h"
#include "sow_extract.h"

#define TAG       "DXX-DiscImport"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static const char *game_extensions[] = {
	"hog", "ham", "pig", "s11", "s22", "mn2", "mvl",
	"dxa", "cfg", "txt", "256", NULL
};

static const char *mac_game_extensions[] = {
	"hog", "ham", "pig", "s11", "s22", "mn2", "mvl",
	"dxa", "cfg", "txt", "256", "msn", "dem", NULL
};

/* ── CUE parsing ─────────────────────────────────────────────────────── */

/*
 * Parse a CUE sheet and return track info as an int array.
 *
 * Each track is encoded as 5 consecutive ints:
 *   [track_num, type, file_index, start_sector, num_sectors]
 *
 * binSizes: array of BIN file sizes (one per FILE directive).
 *
 * Returns null on parse failure.
 */
JNIEXPORT jintArray JNICALL
Java_com_dxxredux_app_DiscImportBridge_nativeParseCue(
    JNIEnv *env, jclass clazz,
    jstring cuePath, jlongArray binSizes)
{
	const char *cue_text_path = (*env)->GetStringUTFChars(env, cuePath, NULL);
	if (!cue_text_path) return NULL;

	/* Read CUE file contents */
	FILE *f = fopen(cue_text_path, "r");
	(*env)->ReleaseStringUTFChars(env, cuePath, cue_text_path);
	if (!f) {
		LOGE("Cannot open CUE file");
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (len <= 0 || len > 1024 * 1024) {
		fclose(f);
		LOGE("CUE file too large or empty: %ld", len);
		return NULL;
	}

	char *cue_text = (char *) malloc(len + 1);
	if (!cue_text) {
		fclose(f);
		return NULL;
	}
	size_t nread = fread(cue_text, 1, len, f);
	cue_text[nread] = '\0';
	fclose(f);
	if ((long) nread != len) {
		LOGE("CUE file read incomplete: got %zu of %ld", nread, len);
	}

	/* Get BIN sizes */
	long long *sizes = NULL;
	int num_sizes = 0;
	if (binSizes) {
		num_sizes = (*env)->GetArrayLength(env, binSizes);
		jlong *jsizes = (*env)->GetLongArrayElements(env, binSizes, NULL);
		sizes = (long long *) malloc(num_sizes * sizeof(long long));
		if (!sizes) {
			(*env)->ReleaseLongArrayElements(env, binSizes, jsizes, JNI_ABORT);
			free(cue_text);
			return NULL;
		}
		for (int i = 0; i < num_sizes; i++)
			sizes[i] = (long long) jsizes[i];
		(*env)->ReleaseLongArrayElements(env, binSizes, jsizes, JNI_ABORT);
	}

	/* Parse */
	cue_disc_t disc;
	memset(&disc, 0, sizeof(disc));
	int n = cue_parse(cue_text, sizes, num_sizes, &disc);
	free(cue_text);
	free(sizes);

	if (n <= 0) return NULL;

	/* Pack into int array: 5 ints per track */
	jintArray result = (*env)->NewIntArray(env, n * 5);
	jint *out = (*env)->GetIntArrayElements(env, result, NULL);
	for (int i = 0; i < n; i++) {
		out[i * 5] = disc.tracks[i].track_num;
		out[i * 5 + 1] = disc.tracks[i].type;
		out[i * 5 + 2] = disc.tracks[i].file_index;
		out[i * 5 + 3] = disc.tracks[i].start_sector;
		out[i * 5 + 4] = disc.tracks[i].num_sectors;
	}
	(*env)->ReleaseIntArrayElements(env, result, out, 0);

	LOGI("Parsed CUE: %d tracks, %d files", n, disc.num_files);
	return result;
}

/*
 * Get track titles from a parsed CUE sheet.
 * Returns a String array with one entry per track (empty string if no title).
 */
JNIEXPORT jobjectArray JNICALL
Java_com_dxxredux_app_DiscImportBridge_nativeGetCueTitles(
    JNIEnv *env, jclass clazz,
    jstring cuePath, jlongArray binSizes)
{
	const char *path = (*env)->GetStringUTFChars(env, cuePath, NULL);
	if (!path) return NULL;

	FILE *f = fopen(path, "r");
	(*env)->ReleaseStringUTFChars(env, cuePath, path);
	if (!f) return NULL;

	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (len <= 0 || len > 1024 * 1024) {
		fclose(f);
		return NULL;
	}

	char *cue_text = (char *) malloc(len + 1);
	if (!cue_text) {
		fclose(f);
		return NULL;
	}
	size_t nread2 = fread(cue_text, 1, len, f);
	cue_text[nread2] = '\0';
	fclose(f);

	long long *sizes = NULL;
	int num_sizes = 0;
	if (binSizes) {
		num_sizes = (*env)->GetArrayLength(env, binSizes);
		jlong *jsizes = (*env)->GetLongArrayElements(env, binSizes, NULL);
		sizes = (long long *) malloc(num_sizes * sizeof(long long));
		if (!sizes) {
			(*env)->ReleaseLongArrayElements(env, binSizes, jsizes, JNI_ABORT);
			free(cue_text);
			return NULL;
		}
		for (int i = 0; i < num_sizes; i++)
			sizes[i] = (long long) jsizes[i];
		(*env)->ReleaseLongArrayElements(env, binSizes, jsizes, JNI_ABORT);
	}

	cue_disc_t disc;
	memset(&disc, 0, sizeof(disc));
	int n = cue_parse(cue_text, sizes, num_sizes, &disc);
	free(cue_text);
	free(sizes);
	if (n <= 0) return NULL;

	jclass strClass = (*env)->FindClass(env, "java/lang/String");
	jobjectArray titles = (*env)->NewObjectArray(env, n, strClass, NULL);
	for (int i = 0; i < n; i++) {
		jstring t = (*env)->NewStringUTF(env, disc.tracks[i].title);
		(*env)->SetObjectArrayElement(env, titles, i, t);
		(*env)->DeleteLocalRef(env, t);
	}
	return titles;
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
    jint binFd, jint trackStart, jint trackSectors)
{
	if (binFd < 0) {
		LOGE("nativeListIsoFiles: invalid binFd %d", binFd);
		return NULL;
	}

	iso_file_list_t list;
	memset(&list, 0, sizeof(list));

	int n = iso_list_files(binFd, trackStart, trackSectors, &list);
	if (n < 0) {
		LOGE("iso_list_files failed");
		return NULL;
	}

	/* Filter to non-directory entries only */
	jclass strClass = (*env)->FindClass(env, "java/lang/String");
	int file_count = 0;
	for (int i = 0; i < list.num_files; i++)
		if (!list.files[i].is_dir) file_count++;

	jobjectArray result = (*env)->NewObjectArray(env, file_count, strClass, NULL);
	int idx = 0;
	for (int i = 0; i < list.num_files; i++) {
		if (list.files[i].is_dir) continue;
		char buf[ISO_PATH_LEN + 32];
		snprintf(buf, sizeof(buf), "%s|%u", list.files[i].path, list.files[i].size);
		jstring s = (*env)->NewStringUTF(env, buf);
		(*env)->SetObjectArrayElement(env, result, idx++, s);
		(*env)->DeleteLocalRef(env, s);
	}

	LOGI("Listed %d files from ISO", file_count);
	return result;
}

/* ── ISO 9660 extraction ─────────────────────────────────────────────── */

/* Progress callback state — calls through JNI to a Kotlin lambda */
typedef struct {
	JNIEnv *env;
	jobject callback; /* DiscImportBridge.ExtractProgress instance */
	jmethodID on_progress;
} extract_ctx_t;

static void init_extract_ctx(JNIEnv *env, jobject progress, extract_ctx_t *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->env = env;
	if (progress) {
		jclass cls;

		ctx->callback = progress;
		cls = (*env)->GetObjectClass(env, progress);
		ctx->on_progress = (*env)->GetMethodID(env, cls,
		                                       "onProgress", "(Ljava/lang/String;JJ)I");
	}
}

static int extract_progress_cb(const char *current_file,
                               long long bytes_done, long long bytes_total,
                               void *user_data)
{
	extract_ctx_t *ctx = (extract_ctx_t *) user_data;
	if (!ctx->callback) return 0;

	jstring jfile = (*ctx->env)->NewStringUTF(ctx->env, current_file);
	jint cancel = (*ctx->env)->CallIntMethod(ctx->env, ctx->callback,
	                                         ctx->on_progress,
	                                         jfile,
	                                         (jlong) bytes_done,
	                                         (jlong) bytes_total);
	(*ctx->env)->DeleteLocalRef(ctx->env, jfile);
	return (int) cancel;
}

static const char *path_basename(const char *path)
{
	const char *last = path;

	while (*path) {
		if (*path == '/' || *path == '\\')
			last = path + 1;
		path++;
	}

	return last;
}

static int str_equals_ignore_case(const char *a, const char *b)
{
	while (*a && *b) {
		char ca = *a;
		char cb = *b;

		if (ca >= 'A' && ca <= 'Z') ca = (char) (ca - 'A' + 'a');
		if (cb >= 'A' && cb <= 'Z') cb = (char) (cb - 'A' + 'a');
		if (ca != cb)
			return 0;
		a++;
		b++;
	}

	return *a == '\0' && *b == '\0';
}

static int ext_matches(const char *filename, const char **extensions)
{
	const char *dot;

	if (!extensions)
		return 1;

	dot = strrchr(filename, '.');
	if (!dot || !dot[1])
		return 0;
	dot++;

	while (*extensions) {
		if (str_equals_ignore_case(dot, *extensions))
			return 1;
		extensions++;
	}

	return 0;
}

static int read_file_to_buffer(const char *path, unsigned char **out_data, size_t *out_size)
{
	FILE *f;
	long len;
	unsigned char *data;

	if (!out_data || !out_size)
		return -1;
	*out_data = NULL;
	*out_size = 0;

	f = fopen(path, "rb");
	if (!f)
		return -1;
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return -1;
	}
	len = ftell(f);
	if (len < 0 || fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return -1;
	}
	data = (unsigned char *) malloc((size_t) len);
	if (!data && len != 0) {
		fclose(f);
		return -1;
	}
	if ((size_t) len != 0 && fread(data, 1, (size_t) len, f) != (size_t) len) {
		free(data);
		fclose(f);
		return -1;
	}
	if (fclose(f) != 0) {
		free(data);
		return -1;
	}

	*out_data = data;
	*out_size = (size_t) len;
	return 0;
}

static int extract_sti2_from_hfs(int bin_fd, int track_start, int track_sectors,
                                 const char *output_dir, extract_ctx_t *ctx)
{
	char archive_path[1024];
	unsigned char *archive_data = NULL;
	size_t archive_size = 0;
	int extracted;

	snprintf(archive_path, sizeof(archive_path), "%s/.install_descent.sti2", output_dir);
	if (hfs_extract_file(bin_fd, track_start, track_sectors,
	                     "Install Descent", archive_path) < 0)
		return -1;
	if (read_file_to_buffer(archive_path, &archive_data, &archive_size) < 0) {
		unlink(archive_path);
		return -1;
	}
	unlink(archive_path);
	if (!sti2_is_archive(archive_data, archive_size)) {
		free(archive_data);
		return -1;
	}

	extracted = sti2_extract_matching(archive_data, archive_size,
	                                  mac_game_extensions, output_dir,
	                                  ctx && ctx->callback ? extract_progress_cb : NULL,
	                                  ctx);
	free(archive_data);
	return extracted;
}

static int extract_hfs_matching_files(int bin_fd, int track_start, int track_sectors,
                                      const char *output_dir, extract_ctx_t *ctx)
{
	hfs_file_list_t list;
	long long total_bytes = 0;
	long long done_bytes = 0;
	int extracted = 0;
	int i;

	if (hfs_list_files(bin_fd, track_start, track_sectors, &list) < 0)
		return -1;

	for (i = 0; i < list.num_files; i++) {
		if (!list.files[i].is_dir &&
		    ext_matches(path_basename(list.files[i].path), mac_game_extensions))
			total_bytes += list.files[i].data_size;
	}

	for (i = 0; i < list.num_files; i++) {
		char output_path[1024];
		int written;

		if (list.files[i].is_dir ||
		    !ext_matches(path_basename(list.files[i].path), mac_game_extensions))
			continue;

		snprintf(output_path, sizeof(output_path), "%s/%s", output_dir, list.files[i].path);
		written = hfs_extract_file(bin_fd, track_start, track_sectors,
		                           list.files[i].path, output_path);
		if (written < 0)
			return -1;
		done_bytes += written;
		extracted++;
		if (ctx && ctx->callback &&
		    extract_progress_cb(list.files[i].path, done_bytes, total_bytes, ctx) != 0)
			return -1;
	}

	return extracted;
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
    jstring outputDir, jobject progress)
{
	if (binFd < 0) {
		LOGE("nativeExtractIsoFiles: invalid binFd %d", binFd);
		return -1;
	}

	const char *out_dir = (*env)->GetStringUTFChars(env, outputDir, NULL);
	if (!out_dir) return -1;

	/* List files first */
	iso_file_list_t list;
	memset(&list, 0, sizeof(list));
	int n = iso_list_files(binFd, trackStart, trackSectors, &list);
	if (n < 0) {
		(*env)->ReleaseStringUTFChars(env, outputDir, out_dir);
		return -1;
	}

	/* Set up progress callback */
	extract_ctx_t ctx;
	init_extract_ctx(env, progress, &ctx);

	int extracted = iso_extract_files(binFd, trackStart, trackSectors,
	                                  &list, out_dir, game_extensions,
	                                  progress ? extract_progress_cb : NULL,
	                                  &ctx);

	(*env)->ReleaseStringUTFChars(env, outputDir, out_dir);
	LOGI("Extracted %d files", extracted);
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
	const char *dir = (*env)->GetStringUTFChars(env, dirPath, NULL);
	if (!dir) return NULL;

	sow_file_list_t list;
	int n = sow_scan_dir(dir, &list);
	(*env)->ReleaseStringUTFChars(env, dirPath, dir);

	if (n < 0) {
		LOGE("sow_scan_dir failed");
		return NULL;
	}
	if (n == 0) {
		LOGI("No .sow files found");
		/* Return empty array */
		jclass strClass = (*env)->FindClass(env, "java/lang/String");
		return (*env)->NewObjectArray(env, 0, strClass, NULL);
	}

	jclass strClass = (*env)->FindClass(env, "java/lang/String");
	jobjectArray result = (*env)->NewObjectArray(env, list.count, strClass, NULL);
	for (int i = 0; i < list.count; i++) {
		jstring s = (*env)->NewStringUTF(env, list.paths[i]);
		(*env)->SetObjectArrayElement(env, result, i, s);
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
    jstring sowPath, jstring outputDir, jobject progress)
{
	const char *sow = (*env)->GetStringUTFChars(env, sowPath, NULL);
	if (!sow) return -1;

	const char *out_dir = (*env)->GetStringUTFChars(env, outputDir, NULL);
	if (!out_dir) {
		(*env)->ReleaseStringUTFChars(env, sowPath, sow);
		return -1;
	}

	/* Set up progress callback */
	extract_ctx_t ctx;
	init_extract_ctx(env, progress, &ctx);

	int extracted = sow_extract(sow, out_dir, game_extensions,
	                            progress ? extract_progress_cb : NULL, &ctx);
	LOGI("SOW extracted %d files from %s", extracted, sow);

	(*env)->ReleaseStringUTFChars(env, sowPath, sow);
	(*env)->ReleaseStringUTFChars(env, outputDir, out_dir);
	return extracted;
}

/* ── Mac HFS/STi2 extraction ─────────────────────────────────────────── */

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_DiscImportBridge_nativeExtractMacFiles(
    JNIEnv *env, jclass clazz,
    jint binFd, jint trackStart, jint trackSectors,
    jstring outputDir, jobject progress)
{
	extract_ctx_t ctx;
	hfs_partition_info_t hfs_info;
	const char *out_dir;
	int extracted;

	if (binFd < 0) {
		LOGE("nativeExtractMacFiles: invalid binFd %d", binFd);
		return -1;
	}

	out_dir = (*env)->GetStringUTFChars(env, outputDir, NULL);
	if (!out_dir)
		return -1;
	init_extract_ctx(env, progress, &ctx);

	if (hfs_find_partition(binFd, trackStart, trackSectors, &hfs_info) < 0) {
		(*env)->ReleaseStringUTFChars(env, outputDir, out_dir);
		return -1;
	}

	extracted = extract_sti2_from_hfs(binFd, trackStart, trackSectors, out_dir, &ctx);
	if (extracted <= 0)
		extracted = extract_hfs_matching_files(binFd, trackStart, trackSectors, out_dir, &ctx);

	LOGI("Mac import extracted %d files from HFS volume '%s'", extracted,
	     hfs_info.volume_name[0] ? hfs_info.volume_name : hfs_info.partition_name);

	(*env)->ReleaseStringUTFChars(env, outputDir, out_dir);
	return extracted;
}
