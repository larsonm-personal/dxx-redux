/*
 * jni_gog_import.c — JNI bridge for GOG installer extraction.
 *
 * Exposes the InnoSetup (.exe) and Mac .pkg readers to Kotlin via
 * GogImportBridge.kt.  Used by SetupActivity during the GOG installer
 * import flow.
 */

#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#include "inno_reader.h"
#include "pkg_reader.h"

#define TAG       "DXX-GogImport"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

/* ── Cross-platform case-insensitive compare ─────────────────────── */
#include <strings.h>
#define ci_cmp strcasecmp

/* ── Game file extensions we extract ─────────────────────────────── */
static const char *game_extensions[] = {
	".hog", ".pig", ".ham", ".s11", ".s22", ".dem",
	".mvl", ".msn", ".mn2", ".gog", ".inst",
	NULL
};

static int has_game_extension(const char *path)
{
	const char *dot = strrchr(path, '.');
	if (!dot) return 0;
	for (const char **ext = game_extensions; *ext; ext++) {
		if (ci_cmp(dot, *ext) == 0) return 1;
	}
	return 0;
}

/* Audio extensions (.gog/.inst) — subset of game_extensions, optionally skipped */
static int is_audio_extension(const char *path)
{
	const char *dot = strrchr(path, '.');
	if (!dot) return 0;
	return ci_cmp(dot, ".gog") == 0 || ci_cmp(dot, ".inst") == 0;
}

static const char *basename_only(const char *path)
{
	const char *last = path;
	for (const char *p = path; *p; p++) {
		if (*p == '/' || *p == '\\') last = p + 1;
	}
	return last;
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
	const char *p = (*env)->GetStringUTFChars(env, path, NULL);
	if (!p) return (*env)->NewStringUTF(env, "unknown");

	const char *dot = strrchr(p, '.');
	const char *result;
	if (dot && ci_cmp(dot, ".pkg") == 0)
		result = "pkg";
	else if (dot && ci_cmp(dot, ".exe") == 0)
		result = "innosetup";
	else
		result = "unknown";

	(*env)->ReleaseStringUTFChars(env, path, p);
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
	const char *p = (*env)->GetStringUTFChars(env, path, NULL);
	if (!p) return NULL;

	const char *dot = strrchr(p, '.');
	int is_pkg = dot && ci_cmp(dot, ".pkg") == 0;

	jclass strClass = (*env)->FindClass(env, "java/lang/String");

	if (is_pkg) {
		pkg_archive_t arc;
		int n = pkg_open(p, &arc);
		(*env)->ReleaseStringUTFChars(env, path, p);
		if (n < 0) return NULL;

		/* All pkg files are already filtered to game files */
		jobjectArray result = (*env)->NewObjectArray(env, arc.file_count, strClass, NULL);
		for (int i = 0; i < arc.file_count; i++) {
			char buf[PKG_PATH_LEN + 32];
			snprintf(buf, sizeof(buf), "%s|%llu",
			         arc.files[i].name, (unsigned long long) arc.files[i].size);
			jstring s = (*env)->NewStringUTF(env, buf);
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
		(*env)->ReleaseStringUTFChars(env, path, p);
		if (n < 0) return NULL;

		/* Count game files */
		int game_count = 0;
		for (int i = 0; i < arc.file_count; i++) {
			if (has_game_extension(arc.files[i].destination))
				game_count++;
		}

		jobjectArray result = (*env)->NewObjectArray(env, game_count, strClass, NULL);
		int idx = 0;
		for (int i = 0; i < arc.file_count; i++) {
			if (!has_game_extension(arc.files[i].destination)) continue;
			const char *fname = basename_only(arc.files[i].destination);
			uint64_t size = 0;
			if (arc.files[i].location < (uint32_t) arc.data_entry_count)
				size = arc.data_entries[arc.files[i].location].file_size;
			char buf[INNO_PATH_LEN + 32];
			snprintf(buf, sizeof(buf), "%s|%llu", fname, (unsigned long long) size);
			jstring s = (*env)->NewStringUTF(env, buf);
			(*env)->SetObjectArrayElement(env, result, idx++, s);
			(*env)->DeleteLocalRef(env, s);
		}
		inno_close(&arc);
		LOGI("Listed %d game files from .exe (%d total)", game_count, n);
		return result;
	}
}

/* ── Extraction ──────────────────────────────────────────────────── */

/* Progress callback state -- calls through JNI to a Kotlin lambda */
typedef struct {
	JNIEnv *env;
	jobject callback;
	jmethodID on_progress;
	long long total_bytes;     /* sum of all files to extract */
	long long completed_bytes; /* bytes for fully extracted files */
} gog_extract_ctx_t;

static int gog_progress_cb(const char *current_file,
                           long long bytes_done, long long bytes_total,
                           void *user_data)
{
	gog_extract_ctx_t *ctx = (gog_extract_ctx_t *) user_data;
	if (!ctx->callback) return 0;

	/* Report overall progress: completed files + current file progress */
	long long overall_done = ctx->completed_bytes + bytes_done;
	long long overall_total = ctx->total_bytes > 0 ? ctx->total_bytes : bytes_total;

	jstring jfile = (*ctx->env)->NewStringUTF(ctx->env, current_file);
	jint cancel = (*ctx->env)->CallIntMethod(ctx->env, ctx->callback,
	                                         ctx->on_progress,
	                                         jfile,
	                                         (jlong) overall_done,
	                                         (jlong) overall_total);
	(*ctx->env)->DeleteLocalRef(ctx->env, jfile);
	return (int) cancel;
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
    jboolean includeAudio)
{
	const char *p = (*env)->GetStringUTFChars(env, path, NULL);
	if (!p) return -1;

	const char *out_dir = (*env)->GetStringUTFChars(env, outputDir, NULL);
	if (!out_dir) {
		(*env)->ReleaseStringUTFChars(env, path, p);
		return -1;
	}

	/* Set up progress callback */
	gog_extract_ctx_t ctx = { env, NULL, NULL, 0, 0 };
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
			/* Compute total bytes for overall progress */
			long long total = 0;
			for (int i = 0; i < arc.file_count; i++) {
				if (!has_game_extension(arc.files[i].destination)) continue;
				if (!includeAudio && is_audio_extension(arc.files[i].destination)) continue;
				if (arc.files[i].location < (uint32_t) arc.data_entry_count)
					total += (long long) arc.data_entries[arc.files[i].location].chunk_compressed_size;
			}
			ctx.total_bytes = total;
			ctx.completed_bytes = 0;

			extracted = 0;
			int errors = 0;
			for (int i = 0; i < arc.file_count; i++) {
				if (!has_game_extension(arc.files[i].destination)) continue;
				if (!includeAudio && is_audio_extension(arc.files[i].destination)) continue;
				const char *fname = basename_only(arc.files[i].destination);
				char out_path[1024];
				snprintf(out_path, sizeof(out_path), "%s/%s", out_dir, fname);
				long long file_comp_size = 0;
				if (arc.files[i].location < (uint32_t) arc.data_entry_count)
					file_comp_size = (long long) arc.data_entries[arc.files[i].location].chunk_compressed_size;
				if (inno_extract_file(&arc, i, out_path,
				                      progress ? gog_progress_cb : NULL, &ctx) == 0) {
					extracted++;
				} else {
					LOGE("Failed to extract: %s", arc.files[i].destination);
					errors++;
				}
				ctx.completed_bytes += file_comp_size;
			}
			inno_close(&arc);
			if (errors > 0 && extracted == 0) extracted = -1;
		}
	}

	(*env)->ReleaseStringUTFChars(env, path, p);
	(*env)->ReleaseStringUTFChars(env, outputDir, out_dir);
	LOGI("Extracted %d files", extracted);
	return extracted;
}
