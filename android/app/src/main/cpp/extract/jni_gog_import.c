/*
 * jni_gog_import.c — JNI bridge for GOG installer extraction.
 *
 * Exposes the InnoSetup (.exe) and Mac .pkg readers to Kotlin via
 * GogImportBridge.kt.  Used by SetupActivity during the GOG installer
 * import flow.
 */

#include <jni.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#include "inno_reader.h"
#include "extract_limits.h"
#include "pkg_reader.h"
#include "game_file_extensions.h"
#include "jni_string.h"

#define TAG       "DXX-GogImport"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

/* ── Cross-platform case-insensitive compare ─────────────────────── */
#include <strings.h>
#define ci_cmp strcasecmp

static int has_game_extension(const char *path)
{
	return dxx_has_android_game_file_extension(path);
}

/* Audio extensions (.gog/.inst) — subset of game_extensions, optionally skipped */
static int is_audio_extension(const char *path)
{
	return dxx_is_android_gog_audio_extension(path);
}

static const char *basename_only(const char *path)
{
	const char *last = path;
	for (const char *p = path; *p; p++) {
		if (*p == '/' || *p == '\\') last = p + 1;
	}
	return last;
}

typedef struct {
	int include_audio;
} inno_selection_t;

static int select_inno_output(const char *path, void *user_data)
{
	inno_selection_t *selection = (inno_selection_t *) user_data;
	return has_game_extension(path) &&
	       (selection->include_audio || !is_audio_extension(path));
}

static void launcher_log(JNIEnv *env, const char *message)
{
	if (!env || !message || !message[0]) return;
	jclass cls = (*env)->FindClass(env, "com/dxxredux/app/LauncherDebugLog");
	if (!cls) {
		(*env)->ExceptionClear(env);
		return;
	}
	jmethodID mid = (*env)->GetStaticMethodID(env, cls, "log", "(Ljava/lang/String;)V");
	if (!mid) {
		(*env)->DeleteLocalRef(env, cls);
		(*env)->ExceptionClear(env);
		return;
	}
	jstring jmessage = dxx_jni_string_from_utf8(env, message);
	if (!jmessage) {
		(*env)->DeleteLocalRef(env, cls);
		(*env)->ExceptionClear(env);
		return;
	}
	(*env)->CallStaticVoidMethod(env, cls, mid, jmessage);
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionClear(env);
	(*env)->DeleteLocalRef(env, jmessage);
	(*env)->DeleteLocalRef(env, cls);
}

static void launcher_logf(JNIEnv *env, const char *fmt, ...)
{
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	launcher_log(env, buf);
}

static jobjectArray build_inno_file_list(JNIEnv *env, inno_archive_t *arc,
                                         jclass strClass)
{
	inno_selection_t selection = { 1 };
	if (!inno_output_names_unique(arc, select_inno_output, &selection))
		return NULL;
	jsize game_count = 0;
	for (uint32_t i = 0; i < arc->file_count; i++) {
		if (has_game_extension(arc->files[i].destination)) game_count++;
	}

	jobjectArray result = (*env)->NewObjectArray(env, game_count, strClass, NULL);
	jsize idx = 0;
	for (uint32_t i = 0; i < arc->file_count; i++) {
		if (!has_game_extension(arc->files[i].destination)) continue;
		const char *fname = basename_only(arc->files[i].destination);
		uint64_t size = 0;
		const inno_data_entry_t *data = inno_file_data_entry(arc, i);
		if (data)
			size = data->file_size;
		char buf[INNO_PATH_LEN + 32];
		snprintf(buf, sizeof(buf), "%s|%llu", fname, (unsigned long long) size);
		jstring s = dxx_jni_string_from_utf8(env, buf);
		if (!s) {
			(*env)->DeleteLocalRef(env, result);
			return NULL;
		}
		(*env)->SetObjectArrayElement(env, result, idx++, s);
		(*env)->DeleteLocalRef(env, s);
	}
	return result;
}

static int pkg_manifest_matches_java(JNIEnv *env, const pkg_archive_t *arc,
                                     jobjectArray expected_entries)
{
	if (!expected_entries ||
	    (*env)->GetArrayLength(env, expected_entries) != arc->file_count)
		return 0;
	for (int i = 0; i < arc->file_count; i++) {
		jstring value = (jstring) (*env)->GetObjectArrayElement(env,
		                                                        expected_entries, i);
		char *actual = NULL;
		char expected[PKG_PATH_LEN + 48];
		int matches;
		if (!value || !dxx_jni_string_to_utf8(env, value, &actual)) {
			if (value) (*env)->DeleteLocalRef(env, value);
			return 0;
		}
		snprintf(expected, sizeof(expected), "%s|%llu|%u",
		         arc->files[i].name, (unsigned long long) arc->files[i].size,
		         arc->files[i].crc32);
		matches = strcmp(actual, expected) == 0;
		free(actual);
		(*env)->DeleteLocalRef(env, value);
		if (!matches) return 0;
	}
	return 1;
}

/* ── Format detection ────────────────────────────────────────────── */

/*
 * Detect the installer format from a file path.
 *
 * Returns "innosetup", "pkg", or "unknown".
 */
JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_GogImportBridge_nativeDetectFormat(
    JNIEnv *env, jclass clazz, jstring path)
{
	char *p;
	if (!dxx_jni_string_to_utf8(env, path, &p))
		return NULL;

	const char *dot = strrchr(p, '.');
	const char *result;
	if (dot && ci_cmp(dot, ".pkg") == 0)
		result = "pkg";
	else if (dot && ci_cmp(dot, ".exe") == 0)
		result = "innosetup";
	else
		result = "unknown";

	free(p);
	return (*env)->NewStringUTF(env, result);
}

/* ── File listing ────────────────────────────────────────────────── */

/*
 * List game files in a GOG installer.
 *
 * Returns a String array of "name|size" entries (game files only).
 */
JNIEXPORT jobjectArray JNICALL
Java_com_dxxredux_app_GogImportBridge_nativeListFiles(
    JNIEnv *env, jclass clazz, jstring path)
{
	char *p;
	if (!dxx_jni_string_to_utf8(env, path, &p)) return NULL;

	const char *dot = strrchr(p, '.');
	int is_pkg = dot && ci_cmp(dot, ".pkg") == 0;

	jclass strClass = (*env)->FindClass(env, "java/lang/String");

	if (is_pkg) {
		pkg_archive_t arc;
		int n = pkg_open(p, &arc);
		free(p);
		if (n < 0) return NULL;

		/* All pkg files are already filtered to game files */
		jobjectArray result = (*env)->NewObjectArray(env, arc.file_count, strClass, NULL);
		for (int i = 0; i < arc.file_count; i++) {
			char buf[PKG_PATH_LEN + 48];
			snprintf(buf, sizeof(buf), "%s|%llu|%u",
			         arc.files[i].name, (unsigned long long) arc.files[i].size,
			         arc.files[i].crc32);
			jstring s = dxx_jni_string_from_utf8(env, buf);
			if (!s) {
				(*env)->DeleteLocalRef(env, result);
				pkg_close(&arc);
				return NULL;
			}
			(*env)->SetObjectArrayElement(env, result, i, s);
			(*env)->DeleteLocalRef(env, s);
		}
		pkg_close(&arc);
		LOGI("Listed %d files from .pkg", arc.file_count);
		return result;
	} else {
		/* InnoSetup */
		inno_archive_t arc;
		int n = inno_open(p, &arc);
		free(p);
		if (n < 0) return NULL;

		jobjectArray result = build_inno_file_list(env, &arc, strClass);
		inno_close(&arc);
		LOGI("Listed files from .exe (%d total)", n);
		return result;
	}
}

JNIEXPORT jobjectArray JNICALL
Java_com_dxxredux_app_GogImportBridge_nativeListFilesFromFd(
    JNIEnv *env, jclass clazz, jint fd)
{
	(void) clazz;
	jclass strClass = (*env)->FindClass(env, "java/lang/String");
	inno_archive_t arc;
	int n = inno_open_fd((int) fd, &arc);
	if (n < 0) return NULL;
	jobjectArray result = build_inno_file_list(env, &arc, strClass);
	inno_close(&arc);
	LOGI("Listed files from .exe fd (%d total)", n);
	return result;
}

/* ── Extraction ──────────────────────────────────────────────────── */

/* Progress callback state -- calls through JNI to a Kotlin lambda */
typedef struct {
	JNIEnv *env;
	jobject callback;
	jmethodID on_progress;
	long long total_bytes;     /* sum of all files to extract */
	long long completed_bytes; /* bytes for fully extracted files */
	int cancelled;
} gog_extract_ctx_t;

static int gog_progress_cb(const char *current_file,
                           long long bytes_done, long long bytes_total,
                           void *user_data)
{
	gog_extract_ctx_t *ctx = (gog_extract_ctx_t *) user_data;
	if (!ctx->callback) return 0;
	if (ctx->cancelled) return 1;

	/* Report overall progress: completed files + current file progress */
	long long overall_done = ctx->completed_bytes + bytes_done;
	long long overall_total = ctx->total_bytes > 0 ? ctx->total_bytes : bytes_total;

	jstring jfile = dxx_jni_string_from_utf8(ctx->env, current_file);
	if (!jfile) return 1;
	jint cancel = (*ctx->env)->CallIntMethod(ctx->env, ctx->callback,
	                                         ctx->on_progress,
	                                         jfile,
	                                         (jlong) overall_done,
	                                         (jlong) overall_total);
	(*ctx->env)->DeleteLocalRef(ctx->env, jfile);
	if ((*ctx->env)->ExceptionCheck(ctx->env)) {
		ctx->cancelled = 1;
		return 1;
	}
	if (cancel) ctx->cancelled = 1;
	return (int) cancel;
}

static int extract_inno_archive(JNIEnv *env, inno_archive_t *arc,
                                const char *out_dir, jobject progress,
                                jboolean includeAudio)
{
	inno_selection_t selection = { includeAudio != 0 };
	if (!inno_output_names_unique(arc, select_inno_output, &selection)) {
		LOGE("Colliding Inno output basenames");
		return -1;
	}
	gog_extract_ctx_t ctx = { env, NULL, NULL, 0, 0, 0 };
	if (progress) {
		ctx.callback = progress;
		jclass cls = (*env)->GetObjectClass(env, progress);
		ctx.on_progress = (*env)->GetMethodID(env, cls,
		                                      "onProgress", "(Ljava/lang/String;JJ)I");
	}

	long long total = 0;
	for (uint32_t i = 0; i < arc->file_count; i++) {
		if (!has_game_extension(arc->files[i].destination)) continue;
		if (!includeAudio && is_audio_extension(arc->files[i].destination)) continue;
		const inno_data_entry_t *data = inno_file_data_entry(arc, i);
		if (data)
			total += (long long) data->chunk_compressed_size;
	}
	ctx.total_bytes = total;

	int extracted = 0;
	for (uint32_t i = 0; i < arc->file_count; i++) {
		if (!has_game_extension(arc->files[i].destination)) continue;
		if (!includeAudio && is_audio_extension(arc->files[i].destination)) continue;
		const char *fname = basename_only(arc->files[i].destination);
		int is_audio = is_audio_extension(arc->files[i].destination);
		char out_path[1024];
		snprintf(out_path, sizeof(out_path), "%s/%s", out_dir, fname);
		long long file_comp_size = 0;
		const inno_data_entry_t *data = inno_file_data_entry(arc, i);
		if (data)
			file_comp_size = (long long) data->chunk_compressed_size;
		if (is_audio) {
			launcher_logf(env,
			              "launcher-gog-native-start file=%s comp_bytes=%lld gog_galaxy=%d",
			              fname,
			              file_comp_size,
			              arc->files[i].gog_galaxy ? 1 : 0);
		}
		const int file_result = inno_extract_file(arc, (int) i, out_path,
		                                          progress ? gog_progress_cb : NULL, &ctx);
		if (ctx.cancelled) {
			remove(out_path);
			return DXX_EXTRACT_CANCELLED;
		}
		if (file_result == 0) {
			extracted++;
			if (is_audio) {
				launcher_logf(env,
				              "launcher-gog-native-done file=%s comp_bytes=%lld gog_galaxy=%d",
				              fname,
				              file_comp_size,
				              arc->files[i].gog_galaxy ? 1 : 0);
			}
		} else {
			LOGE("Failed to extract: %s", arc->files[i].destination);
			launcher_logf(env,
			              "launcher-gog-native-fail file=%s audio=%d comp_bytes=%lld gog_galaxy=%d",
			              fname,
			              is_audio,
			              file_comp_size,
			              arc->files[i].gog_galaxy ? 1 : 0);
			return -1;
		}
		ctx.completed_bytes += file_comp_size;
	}
	return extracted;
}

/*
 * Extract game files from a GOG installer to outputDir.
 *
 * Returns number of files extracted, or -1 on error.
 */
JNIEXPORT jint JNICALL
Java_com_dxxredux_app_GogImportBridge_nativeExtractFiles(
    JNIEnv *env, jclass clazz,
    jstring path, jstring outputDir, jobject progress,
    jboolean includeAudio, jobjectArray expectedEntries)
{
	char *p;
	char *out_dir;
	if (!dxx_jni_string_to_utf8(env, path, &p)) return -1;

	if (!dxx_jni_string_to_utf8(env, outputDir, &out_dir)) {
		free(p);
		return -1;
	}

	/* Set up progress callback */
	gog_extract_ctx_t ctx = { env, NULL, NULL, 0, 0, 0 };
	if (progress) {
		ctx.callback = progress;
		jclass cls = (*env)->GetObjectClass(env, progress);
		ctx.on_progress = (*env)->GetMethodID(env, cls,
		                                      "onProgress", "(Ljava/lang/String;JJ)I");
	}

	const char *dot = strrchr(p, '.');
	int is_pkg = dot && ci_cmp(dot, ".pkg") == 0;
	int extracted;

	if (is_pkg) {
		pkg_archive_t arc;
		int n = pkg_open(p, &arc);
		if (n < 0) {
			LOGE("Failed to open .pkg: %s", p);
			extracted = -1;
		} else if (!pkg_manifest_matches_java(env, &arc, expectedEntries)) {
			LOGE("Analyzed .pkg manifest changed before extraction");
			extracted = -1;
			pkg_close(&arc);
		} else {
			int skip_audio = !includeAudio;
			extracted = pkg_extract_all(&arc, out_dir,
			                            progress ? gog_progress_cb : NULL, &ctx,
			                            skip_audio);
			pkg_close(&arc);
		}
	} else {
		/* InnoSetup */
		inno_archive_t arc;
		int n = inno_open(p, &arc);
		if (n < 0) {
			LOGE("Failed to open .exe: %s", p);
			extracted = -1;
		} else {
			extracted = extract_inno_archive(env, &arc, out_dir, progress, includeAudio);
			inno_close(&arc);
		}
	}

	free(p);
	free(out_dir);
	LOGI("Extracted %d files", extracted);
	return extracted;
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_GogImportBridge_nativeExtractFilesFromFd(
    JNIEnv *env, jclass clazz,
    jint fd, jstring outputDir, jobject progress,
    jboolean includeAudio)
{
	(void) clazz;
	char *out_dir;
	if (!dxx_jni_string_to_utf8(env, outputDir, &out_dir)) return -1;

	inno_archive_t arc;
	int n = inno_open_fd((int) fd, &arc);
	int extracted;
	if (n < 0) {
		LOGE("Failed to open .exe fd");
		extracted = -1;
	} else {
		extracted = extract_inno_archive(env, &arc, out_dir, progress, includeAudio);
		inno_close(&arc);
	}

	free(out_dir);
	LOGI("Extracted %d files from fd", extracted);
	return extracted;
}
