/*
 * android_pilot_prefs.cpp -- JNI wrapper for launcher-side cockpit mode and
 * auto-level preferences stored in pilot-related files.
 *
 * File format knowledge stays in playsave.c so Kotlin does not duplicate the
 * D1/D2 player-file layouts.
 */

#ifdef ANDROID

#include <jni.h>
#include <cstring>
#include <cstdio>
#include <dirent.h>
#include <android/log.h>

extern "C" {
#include "playsave.h"
}

#define LOG_TAG   "DXX-PilotPrefs"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

/*
 * Both libdxx-redux-d1.so and libdxx-redux-d2.so can be loaded into the same
 * process.  Give each JNI symbol a D1/D2 suffix so ART resolves them cleanly.
 */
#define PASTE_(a, b, c) a##b##c
#define PASTE3(a, b, c) PASTE_(a, b, c)

#ifdef DXX_BUILD_DESCENT_II
#define GAME_TAG D2
#else
#define GAME_TAG D1
#endif

#define JNI_FUNC(method) \
	PASTE3(Java_com_dxxredux_app_NativePilotPreferences_, method, GAME_TAG)

static int find_first_pilot(const char *files_dir,
                            const char *subdir,
                            const char *ext,
                            char *path_out,
                            size_t path_size)
{
	char dir_path[512];
	const char *subdirs[] = { "", "/Players" };
	size_t ext_len = strlen(ext);

	for (int d = 0; d < 2; d++) {
		snprintf(dir_path, sizeof(dir_path), "%s/%s%s", files_dir, subdir, subdirs[d]);
		DIR *dp = opendir(dir_path);
		if (!dp) continue;
		struct dirent *ent;
		while ((ent = readdir(dp)) != NULL) {
			size_t nlen = strlen(ent->d_name);
			if (nlen > ext_len && strcasecmp(ent->d_name + nlen - ext_len, ext) == 0) {
				snprintf(path_out, path_size, "%s/%s", dir_path, ent->d_name);
				closedir(dp);
				return 1;
			}
		}
		closedir(dp);
	}

	return 0;
}

#ifndef DXX_BUILD_DESCENT_II
static int find_matching_ext(const char *path,
                             const char *new_ext,
                             char *path_out,
                             size_t path_size)
{
	const char *dot = strrchr(path, '.');
	FILE *f;
	size_t base_len;

	if (!dot) return 0;
	base_len = (size_t) (dot - path);
	if (base_len + strlen(new_ext) + 1 >= path_size) return 0;

	memcpy(path_out, path, base_len);
	path_out[base_len] = 0;
	strcat(path_out, new_ext);

	f = fopen(path_out, "rb");
	if (!f) return 0;
	fclose(f);
	return 1;
}
#endif

typedef int (*pilot_visitor_fn)(const char *path, void *ctx);

static int for_each_pilot(const char *files_dir,
                          const char *subdir,
                          const char *ext,
                          pilot_visitor_fn visitor,
                          void *ctx)
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

static void read_engine_prefs(const char *files_dir,
                              int *has_pilot,
                              int *cockpit_mode,
                              int *auto_leveling)
{
	android_get_default_pilot_prefs(cockpit_mode, auto_leveling);
	*has_pilot = 0;

#ifdef DXX_BUILD_DESCENT_II
	char pilot_path[512];
	if (find_first_pilot(files_dir, "d2x-redux", ".plr", pilot_path, sizeof(pilot_path))) {
		*has_pilot = 1;
		(void) plr_read_cockpit_autolevel(pilot_path, cockpit_mode, auto_leveling);
	}
#else
	char cockpit_path[512] = { 0 };
	char auto_path[512] = { 0 };
	int found_cockpit = find_first_pilot(files_dir, "d1x-redux", ".plx", cockpit_path, sizeof(cockpit_path));
	int found_auto = 0;

	if (found_cockpit && find_matching_ext(cockpit_path, ".plr", auto_path, sizeof(auto_path))) {
		found_auto = 1;
	} else {
		found_auto = find_first_pilot(files_dir, "d1x-redux", ".plr", auto_path, sizeof(auto_path));
	}

	if (!found_cockpit && found_auto) {
		found_cockpit = find_matching_ext(auto_path, ".plx", cockpit_path, sizeof(cockpit_path));
	}

	*has_pilot = found_cockpit || found_auto;
	if (found_cockpit)
		(void) plx_read_cockpit_mode(cockpit_path, cockpit_mode);
	if (found_auto)
		(void) plr_read_autoleveling(auto_path, auto_leveling);
#endif
}

struct write_ctx {
	int cockpit_mode;
	int auto_leveling;
};

#ifdef DXX_BUILD_DESCENT_II
static int write_visitor(const char *path, void *ctx)
{
	struct write_ctx *wc = (struct write_ctx *) ctx;
	return plr_patch_cockpit_autolevel(path, wc->cockpit_mode, wc->auto_leveling);
}
#else
static int write_cockpit_visitor(const char *path, void *ctx)
{
	struct write_ctx *wc = (struct write_ctx *) ctx;
	return plx_write_cockpit_mode(path, wc->cockpit_mode);
}

static int write_autolevel_visitor(const char *path, void *ctx)
{
	struct write_ctx *wc = (struct write_ctx *) ctx;
	return plr_patch_autoleveling(path, wc->auto_leveling);
}
#endif

extern "C" JNIEXPORT jintArray JNICALL
JNI_FUNC(nativeReadEnginePrefs)(JNIEnv *env, jclass, jstring jfilesDir)
{
	const char *files_dir = env->GetStringUTFChars(jfilesDir, NULL);
	int has_pilot = 0;
	int cockpit_mode = 0;
	int auto_leveling = 1;
	jint raw[3];
	jintArray result;

	read_engine_prefs(files_dir, &has_pilot, &cockpit_mode, &auto_leveling);
	LOGI("nativeReadEnginePrefs: has_pilot=%d cockpit=%d autolevel=%d", has_pilot, cockpit_mode, auto_leveling);

	env->ReleaseStringUTFChars(jfilesDir, files_dir);

	raw[0] = (jint) has_pilot;
	raw[1] = (jint) cockpit_mode;
	raw[2] = (jint) (auto_leveling ? 1 : 0);
	result = env->NewIntArray(3);
	env->SetIntArrayRegion(result, 0, 3, raw);
	return result;
}

extern "C" JNIEXPORT jint JNICALL
JNI_FUNC(nativeWriteEnginePrefs)(JNIEnv *env,
                                 jclass,
                                 jstring jfilesDir,
                                 jint cockpitMode,
                                 jboolean autoLeveling)
{
	const char *files_dir = env->GetStringUTFChars(jfilesDir, NULL);
	struct write_ctx wc;
	int total;

	wc.cockpit_mode = (int) cockpitMode;
	wc.auto_leveling = autoLeveling ? 1 : 0;

#ifdef DXX_BUILD_DESCENT_II
	total = for_each_pilot(files_dir, "d2x-redux", ".plr", write_visitor, &wc);
#else
	{
		int plx_total = for_each_pilot(files_dir, "d1x-redux", ".plx", write_cockpit_visitor, &wc);
		int plr_total = for_each_pilot(files_dir, "d1x-redux", ".plr", write_autolevel_visitor, &wc);
		total = (plx_total > plr_total) ? plx_total : plr_total;
	}
#endif

	LOGI("nativeWriteEnginePrefs: cockpit=%d autolevel=%d patched=%d", wc.cockpit_mode, wc.auto_leveling, total);
	env->ReleaseStringUTFChars(jfilesDir, files_dir);
	return (jint) total;
}

#endif