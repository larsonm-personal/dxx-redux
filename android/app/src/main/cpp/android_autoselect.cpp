/*
 * android_autoselect.cpp -- JNI wrapper for weapon autoselect ordering
 * editor in the launcher.
 *
 * File format knowledge lives in playsave.c (D2 binary .plr) and
 * playsave.c (D1 text .plx).  Weapon names come from text.h.
 * Default orderings come from weapon.c.
 *
 * This file handles: JNI marshaling, pilot file scanning, and
 * dispatching to the appropriate playsave.c functions.
 */

#ifdef ANDROID

#include <jni.h>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <android/log.h>

/* Use built-in English strings so text.h macros return literals
 * without needing the game's text resource files loaded. */
#define USE_BUILTIN_ENGLISH_TEXT_STRINGS

extern "C" {
#include "dxxerror.h"
#include "playsave.h"
#include "weapon.h"
#include "text.h"
}

#define LOG_TAG   "DXX-Autoselect"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/*
 * Game-specific JNI function names.  Both libdxx-redux-d1.so and
 * libdxx-redux-d2.so are loaded into the same process, so the JNI
 * symbols must be unique.  Append D1/D2 to each method name.
 */
#define PASTE_(a, b, c) a##b##c
#define PASTE3(a, b, c) PASTE_(a, b, c)

#ifdef DXX_BUILD_DESCENT_II
#define GAME_TAG D2
#else
#define GAME_TAG D1
#endif

#define JNI_FUNC(method) \
	PASTE3(Java_com_dxxredux_app_NativeAutoselectPatcher_, method, GAME_TAG)

/* Order array lengths.  Shared constants: keep in sync with playsave.h */
#ifdef DXX_BUILD_DESCENT_II
#define PRIM_ORDER_LEN (MAX_PRIMARY_WEAPONS + 1)   /* 11 */
#define SEC_ORDER_LEN  (MAX_SECONDARY_WEAPONS + 1) /* 11 */
#else
#define PRIM_ORDER_LEN (MAX_PRIMARY_WEAPONS + 2)   /* 7 */
#define SEC_ORDER_LEN  (MAX_SECONDARY_WEAPONS + 1) /* 6 */
#endif

/* Max of D1/D2 lengths, for stack buffers */
#define MAX_ORDER_LEN 11

/* -- pilot file scanning ----------------------------------------- */

typedef int (*pilot_visitor_fn)(const char *path, void *ctx);

static int for_each_pilot(const char *files_dir, const char *subdir,
                          const char *ext, pilot_visitor_fn visitor, void *ctx)
{
	char dir_path[512];
	const char *subdirs[] = { "", "/Players" };
	size_t ext_len = strlen(ext);
	int total = 0;

	for (int d = 0; d < 2; d++) {
		snprintf(dir_path, sizeof(dir_path), "%s/%s%s", files_dir, subdir, subdirs[d]);
		DIR *dp = opendir(dir_path);
		if (!dp) continue;
		struct dirent *ent;
		while ((ent = readdir(dp)) != NULL) {
			size_t nlen = strlen(ent->d_name);
			if (nlen > ext_len && strcasecmp(ent->d_name + nlen - ext_len, ext) == 0) {
				char path[512];
				snprintf(path, sizeof(path), "%s/%s", dir_path, ent->d_name);
				total += visitor(path, ctx);
			}
		}
		closedir(dp);
	}
	return total;
}

struct read_summary_ctx {
	int valid_count;
	int mismatch;
	int have_reference;
	time_t newest_mtime;
	char newest_path[512];
	ubyte reference_primary[MAX_ORDER_LEN];
	ubyte reference_secondary[MAX_ORDER_LEN];
	ubyte newest_primary[MAX_ORDER_LEN];
	ubyte newest_secondary[MAX_ORDER_LEN];
};

static int order_matches(const ubyte *a, const ubyte *b, int len)
{
	return memcmp(a, b, (size_t) len) == 0;
}

static int order_pair_matches(const ubyte *primary_a, const ubyte *secondary_a,
                              const ubyte *primary_b, const ubyte *secondary_b)
{
	return order_matches(primary_a, primary_b, PRIM_ORDER_LEN) &&
	       order_matches(secondary_a, secondary_b, SEC_ORDER_LEN);
}

#ifdef DXX_BUILD_DESCENT_II
static int read_weapon_order_file(const char *path, ubyte *primary, ubyte *secondary)
{
	return plr_read_weapon_order(path, primary, PRIM_ORDER_LEN,
	                             secondary, SEC_ORDER_LEN);
}
#else
static int read_weapon_order_file(const char *path, ubyte *primary, ubyte *secondary)
{
	return plx_read_weapon_order(path, primary, PRIM_ORDER_LEN,
	                             secondary, SEC_ORDER_LEN);
}
#endif

static int read_summary_visitor(const char *path, void *ctx)
{
	struct read_summary_ctx *rc = (struct read_summary_ctx *) ctx;
	ubyte primary[MAX_ORDER_LEN], secondary[MAX_ORDER_LEN];
	if (!read_weapon_order_file(path, primary, secondary))
		return 0;

	if (!rc->have_reference) {
		memcpy(rc->reference_primary, primary, PRIM_ORDER_LEN);
		memcpy(rc->reference_secondary, secondary, SEC_ORDER_LEN);
		rc->have_reference = 1;
	} else if (!order_pair_matches(rc->reference_primary, rc->reference_secondary,
	                               primary, secondary)) {
		rc->mismatch = 1;
	}

	struct stat st;
	time_t mtime = 0;
	if (stat(path, &st) == 0)
		mtime = st.st_mtime;
	if (rc->valid_count == 0 || mtime > rc->newest_mtime) {
		rc->newest_mtime = mtime;
		snprintf(rc->newest_path, sizeof(rc->newest_path), "%s", path);
		memcpy(rc->newest_primary, primary, PRIM_ORDER_LEN);
		memcpy(rc->newest_secondary, secondary, SEC_ORDER_LEN);
	}

	rc->valid_count++;
	return 1;
}

static int read_autoselect_summary(const char *files_dir, const char *subdir,
                                   const char *ext, struct read_summary_ctx *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	return for_each_pilot(files_dir, subdir, ext, read_summary_visitor, ctx);
}

/* -- write visitor context --------------------------------------- */

struct write_ctx {
	const ubyte *primary;
	int prim_len;
	const ubyte *secondary;
	int sec_len;
};

#ifdef DXX_BUILD_DESCENT_II
static int write_visitor(const char *path, void *ctx)
{
	struct write_ctx *wc = (struct write_ctx *) ctx;
	return plr_patch_weapon_order(path, wc->primary, wc->prim_len,
	                              wc->secondary, wc->sec_len);
}
#else
static int write_visitor(const char *path, void *ctx)
{
	struct write_ctx *wc = (struct write_ctx *) ctx;
	return plx_write_weapon_order(path, wc->primary, wc->prim_len,
	                              wc->secondary, wc->sec_len);
}
#endif

/* -- JNI entry points -------------------------------------------- */

extern "C" JNIEXPORT jintArray JNICALL
JNI_FUNC(nativeReadAutoselect)(
    JNIEnv *env, jclass, jstring jfilesDir)
{
	const char *files_dir = env->GetStringUTFChars(jfilesDir, NULL);

#ifdef DXX_BUILD_DESCENT_II
	const char *subdir = "d2x-redux";
	const char *ext = ".plr";
#else
	const char *subdir = "d1x-redux";
	const char *ext = ".plx";
#endif

	struct read_summary_ctx summary;
	if (!read_autoselect_summary(files_dir, subdir, ext, &summary)) {
		LOGI("nativeReadAutoselect: no pilot file found in %s/%s", files_dir, subdir);
		env->ReleaseStringUTFChars(jfilesDir, files_dir);
		return env->NewIntArray(0);
	}

	env->ReleaseStringUTFChars(jfilesDir, files_dir);
	LOGI("nativeReadAutoselect: read newest from %s", summary.newest_path);

	int total = PRIM_ORDER_LEN + SEC_ORDER_LEN;
	jintArray result = env->NewIntArray(total);
	jint flat[MAX_ORDER_LEN * 2];
	for (int i = 0; i < PRIM_ORDER_LEN; i++) flat[i] = summary.newest_primary[i];
	for (int i = 0; i < SEC_ORDER_LEN; i++) flat[PRIM_ORDER_LEN + i] = summary.newest_secondary[i];
	env->SetIntArrayRegion(result, 0, total, flat);
	return result;
}

extern "C" JNIEXPORT jintArray JNICALL
JNI_FUNC(nativeReadAutoselectSummary)(
    JNIEnv *env, jclass, jstring jfilesDir)
{
	const char *files_dir = env->GetStringUTFChars(jfilesDir, NULL);

#ifdef DXX_BUILD_DESCENT_II
	const char *subdir = "d2x-redux";
	const char *ext = ".plr";
#else
	const char *subdir = "d1x-redux";
	const char *ext = ".plx";
#endif

	struct read_summary_ctx summary;
	if (!read_autoselect_summary(files_dir, subdir, ext, &summary)) {
		LOGI("nativeReadAutoselectSummary: no pilot file found in %s/%s", files_dir, subdir);
		env->ReleaseStringUTFChars(jfilesDir, files_dir);
		return env->NewIntArray(0);
	}

	env->ReleaseStringUTFChars(jfilesDir, files_dir);
	LOGI("nativeReadAutoselectSummary: read newest from %s (%d valid, mismatch=%d)",
	     summary.newest_path, summary.valid_count, summary.mismatch);

	int order_total = PRIM_ORDER_LEN + SEC_ORDER_LEN;
	int total = 4 + order_total;
	jintArray result = env->NewIntArray(total);
	jint flat[4 + MAX_ORDER_LEN * 2];
	flat[0] = summary.valid_count;
	flat[1] = summary.mismatch;
	flat[2] = PRIM_ORDER_LEN;
	flat[3] = SEC_ORDER_LEN;
	for (int i = 0; i < PRIM_ORDER_LEN; i++) flat[4 + i] = summary.newest_primary[i];
	for (int i = 0; i < SEC_ORDER_LEN; i++) flat[4 + PRIM_ORDER_LEN + i] = summary.newest_secondary[i];
	env->SetIntArrayRegion(result, 0, total, flat);
	return result;
}

extern "C" JNIEXPORT jint JNICALL
JNI_FUNC(nativeWriteAutoselect)(
    JNIEnv *env, jclass, jstring jfilesDir,
    jintArray jprimary, jintArray jsecondary)
{
	const char *files_dir = env->GetStringUTFChars(jfilesDir, NULL);

#ifdef DXX_BUILD_DESCENT_II
	const char *subdir = "d2x-redux";
	const char *ext = ".plr";
#else
	const char *subdir = "d1x-redux";
	const char *ext = ".plx";
#endif

	jint *jprim = env->GetIntArrayElements(jprimary, NULL);
	jint *jsec = env->GetIntArrayElements(jsecondary, NULL);
	int jprim_len = env->GetArrayLength(jprimary);
	int jsec_len = env->GetArrayLength(jsecondary);

	ubyte primary[MAX_ORDER_LEN], secondary[MAX_ORDER_LEN];
	for (int i = 0; i < PRIM_ORDER_LEN && i < jprim_len; i++)
		primary[i] = (ubyte) (jprim[i] & 0xFF);
	for (int i = 0; i < SEC_ORDER_LEN && i < jsec_len; i++)
		secondary[i] = (ubyte) (jsec[i] & 0xFF);

	env->ReleaseIntArrayElements(jprimary, jprim, JNI_ABORT);
	env->ReleaseIntArrayElements(jsecondary, jsec, JNI_ABORT);

	struct write_ctx wc = { primary, PRIM_ORDER_LEN, secondary, SEC_ORDER_LEN };
	int total = for_each_pilot(files_dir, subdir, ext, write_visitor, &wc);

	LOGI("nativeWriteAutoselect: patched %d file(s) in %s/%s", total, files_dir, subdir);
	env->ReleaseStringUTFChars(jfilesDir, files_dir);
	return (jint) total;
}

/*
 * Weapon entries: paired [indexStr, name, indexStr, name, ...]
 * Enumerates every weapon index that appears in an ordering
 * (including the separator) with its display name.
 * Names come from text.h; separator and D1 Quad Lasers are special-cased.
 */
static const char *primary_weapon_name(int idx)
{
	if (idx == 255)
		return "--- Never Autoselect below ---";
#ifndef DXX_BUILD_DESCENT_II
	if (idx == 16)
		return "Quad Lasers";
#endif
	unsigned u = (unsigned) idx;
	return PRIMARY_WEAPON_NAMES(u);
}

static const char *secondary_weapon_name(int idx)
{
	if (idx == 255)
		return "--- Never Autoselect below ---";
	unsigned u = (unsigned) idx;
	return SECONDARY_WEAPON_NAMES(u);
}

static jobjectArray build_weapon_entries(JNIEnv *env, const ubyte *order,
                                         int order_len,
                                         const char *(*name_fn)(int) )
{
	jclass strClass = env->FindClass("java/lang/String");
	jobjectArray result = env->NewObjectArray(order_len * 2, strClass, NULL);
	for (int i = 0; i < order_len; i++) {
		int idx = order[i];
		char idx_str[16];
		snprintf(idx_str, sizeof(idx_str), "%d", idx);
		env->SetObjectArrayElement(result, i * 2,
		                           env->NewStringUTF(idx_str));
		env->SetObjectArrayElement(result, i * 2 + 1,
		                           env->NewStringUTF(name_fn(idx)));
	}
	return result;
}

extern "C" JNIEXPORT jobjectArray JNICALL
JNI_FUNC(nativeGetPrimaryWeaponEntries)(
    JNIEnv *env, jclass)
{
	return build_weapon_entries(env, DefaultPrimaryOrder, PRIM_ORDER_LEN,
	                            primary_weapon_name);
}

extern "C" JNIEXPORT jobjectArray JNICALL
JNI_FUNC(nativeGetSecondaryWeaponEntries)(
    JNIEnv *env, jclass)
{
	return build_weapon_entries(env, DefaultSecondaryOrder, SEC_ORDER_LEN,
	                            secondary_weapon_name);
}

#endif /* ANDROID */
