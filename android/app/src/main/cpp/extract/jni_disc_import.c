/*
 * jni_disc_import.c — JNI bridge for BIN/CUE disc import operations.
 *
 * Exposes the CUE parser and ISO 9660 reader to Kotlin via
 * DiscImportBridge.kt.  Used by SetupActivity during the CD image
 * import flow.
 */

#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <android/log.h>

#include "cue_parser.h"
#include "iso9660_reader.h"
#include "sow_extract.h"

#define TAG "DXX-DiscImport"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

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

    char *cue_text = (char *)malloc(len + 1);
    if (!cue_text) { fclose(f); return NULL; }
    size_t nread = fread(cue_text, 1, len, f);
    cue_text[nread] = '\0';
    fclose(f);
    if ((long)nread != len) {
        LOGE("CUE file read incomplete: got %zu of %ld", nread, len);
    }

    /* Get BIN sizes */
    long long *sizes = NULL;
    int num_sizes = 0;
    if (binSizes) {
        num_sizes = (*env)->GetArrayLength(env, binSizes);
        jlong *jsizes = (*env)->GetLongArrayElements(env, binSizes, NULL);
        sizes = (long long *)malloc(num_sizes * sizeof(long long));
        if (!sizes) {
            (*env)->ReleaseLongArrayElements(env, binSizes, jsizes, JNI_ABORT);
            free(cue_text);
            return NULL;
        }
        for (int i = 0; i < num_sizes; i++)
            sizes[i] = (long long)jsizes[i];
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
        out[i * 5]     = disc.tracks[i].track_num;
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
    if (len <= 0 || len > 1024 * 1024) { fclose(f); return NULL; }

    char *cue_text = (char *)malloc(len + 1);
    if (!cue_text) { fclose(f); return NULL; }
    size_t nread2 = fread(cue_text, 1, len, f);
    cue_text[nread2] = '\0';
    fclose(f);

    long long *sizes = NULL;
    int num_sizes = 0;
    if (binSizes) {
        num_sizes = (*env)->GetArrayLength(env, binSizes);
        jlong *jsizes = (*env)->GetLongArrayElements(env, binSizes, NULL);
        sizes = (long long *)malloc(num_sizes * sizeof(long long));
        if (!sizes) {
            (*env)->ReleaseLongArrayElements(env, binSizes, jsizes, JNI_ABORT);
            free(cue_text);
            return NULL;
        }
        for (int i = 0; i < num_sizes; i++)
            sizes[i] = (long long)jsizes[i];
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
    JNIEnv  *env;
    jobject  callback;  /* DiscImportBridge.ExtractProgress instance */
    jmethodID on_progress;
} extract_ctx_t;

static int extract_progress_cb(const char *current_file,
                                long long bytes_done, long long bytes_total,
                                void *user_data)
{
    extract_ctx_t *ctx = (extract_ctx_t *)user_data;
    if (!ctx->callback) return 0;

    jstring jfile = (*ctx->env)->NewStringUTF(ctx->env, current_file);
    jint cancel = (*ctx->env)->CallIntMethod(ctx->env, ctx->callback,
                                              ctx->on_progress,
                                              jfile,
                                              (jlong)bytes_done,
                                              (jlong)bytes_total);
    (*ctx->env)->DeleteLocalRef(ctx->env, jfile);
    return (int)cancel;
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

    /* Game file extensions to extract */
    static const char *extensions[] = {
        "hog", "ham", "pig", "s11", "s22", "mn2", "mvl",
        "dxa", "cfg", "txt", "256", NULL
    };

    /* List files first */
    iso_file_list_t list;
    memset(&list, 0, sizeof(list));
    int n = iso_list_files(binFd, trackStart, trackSectors, &list);
    if (n < 0) {
        (*env)->ReleaseStringUTFChars(env, outputDir, out_dir);
        return -1;
    }

    /* Set up progress callback */
    extract_ctx_t ctx = { env, NULL, NULL };
    if (progress) {
        ctx.callback = progress;
        jclass cls = (*env)->GetObjectClass(env, progress);
        ctx.on_progress = (*env)->GetMethodID(env, cls,
                "onProgress", "(Ljava/lang/String;JJ)I");
    }

    int extracted = iso_extract_files(binFd, trackStart, trackSectors,
                                       &list, out_dir, extensions,
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

    /* Game file extensions to extract */
    static const char *extensions[] = {
        "hog", "ham", "pig", "s11", "s22", "mn2", "mvl",
        "dxa", "cfg", "txt", "256", NULL
    };

    /* Set up progress callback */
    extract_ctx_t ctx = { env, NULL, NULL };
    if (progress) {
        ctx.callback = progress;
        jclass cls = (*env)->GetObjectClass(env, progress);
        ctx.on_progress = (*env)->GetMethodID(env, cls,
                "onProgress", "(Ljava/lang/String;JJ)I");
    }

    int extracted = sow_extract(sow, out_dir, extensions,
                                progress ? extract_progress_cb : NULL, &ctx);

    (*env)->ReleaseStringUTFChars(env, sowPath, sow);
    (*env)->ReleaseStringUTFChars(env, outputDir, out_dir);

    LOGI("SOW extracted %d files from %s", extracted, sow);
    return extracted;
}
