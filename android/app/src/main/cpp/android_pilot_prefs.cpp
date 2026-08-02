/*
 * android_pilot_prefs.cpp -- JNI wrapper for launcher-side engine preferences
 * stored in pilot-related files.
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
                              int *auto_leveling,
                              int *show_counts,
                              int *show_boss_health_bar,
                              int *headlight_active_default)
{
	android_get_default_pilot_prefs(cockpit_mode, auto_leveling);
	android_get_default_hud_count_prefs(show_counts);
	android_get_default_boss_health_bar_prefs(show_boss_health_bar);
	*headlight_active_default = 0;
	*has_pilot = 0;

#ifdef DXX_BUILD_DESCENT_II
	char pilot_path[512];
	char plx_path[512];
	if (find_first_pilot(files_dir, "d2x-redux", ".plr", pilot_path, sizeof(pilot_path))) {
		*has_pilot = 1;
		(void) plr_read_cockpit_autolevel(pilot_path, cockpit_mode, auto_leveling, headlight_active_default);
	}
	if (find_first_pilot(files_dir, "d2x-redux", ".plx", plx_path, sizeof(plx_path))) {
		*has_pilot = 1;
		(void) plx_read_hud_prefs(plx_path, show_counts, show_boss_health_bar);
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
	if (found_cockpit) {
		(void) plx_read_cockpit_mode(cockpit_path, cockpit_mode);
		(void) plx_read_hud_prefs(cockpit_path, show_counts, show_boss_health_bar);
	}
	if (found_auto)
		(void) plr_read_autoleveling(auto_path, auto_leveling);
#endif
}

static void read_visual_prefs(const char *files_dir,
                              int *has_pilot,
                              int *alpha_effects,
                              int *dynlight_color)
{
	android_get_default_visual_prefs(alpha_effects, dynlight_color);
	*has_pilot = 0;

	char pilot_path[512];
#ifdef DXX_BUILD_DESCENT_II
	if (find_first_pilot(files_dir, "d2x-redux", ".plx", pilot_path, sizeof(pilot_path))) {
#else
	if (find_first_pilot(files_dir, "d1x-redux", ".plx", pilot_path, sizeof(pilot_path))) {
#endif
		*has_pilot = 1;
		(void) plx_read_visual_prefs(pilot_path, alpha_effects, dynlight_color);
	}
}

static void read_original_homing_prefs(const char *files_dir,
                                       int *has_pilot,
                                       int *original_homing)
{
	*has_pilot = 0;
	*original_homing = 0;

	char pilot_path[512];
#ifdef DXX_BUILD_DESCENT_II
	if (find_first_pilot(files_dir, "d2x-redux", ".plx", pilot_path, sizeof(pilot_path))) {
#else
	if (find_first_pilot(files_dir, "d1x-redux", ".plx", pilot_path, sizeof(pilot_path))) {
#endif
		*has_pilot = 1;
		(void) plx_read_original_homing(pilot_path, original_homing);
	}
}

static void read_music_prefs(const char *files_dir,
                             int *has_pilot,
                             int *source,
                             int *prefer_mission,
                             int *play_order,
                             int *volume)
{
	android_get_default_music_prefs(source, prefer_mission, play_order, volume);
	*has_pilot = 0;

	char pilot_path[512];
#ifdef DXX_BUILD_DESCENT_II
	if (find_first_pilot(files_dir, "d2x-redux", ".plx", pilot_path, sizeof(pilot_path))) {
#else
	if (find_first_pilot(files_dir, "d1x-redux", ".plx", pilot_path, sizeof(pilot_path))) {
#endif
		*has_pilot = 1;
		(void) plx_read_music_prefs(pilot_path, source, prefer_mission, play_order, volume);
	}
}

struct write_ctx {
	int cockpit_mode;
	int auto_leveling;
	int show_counts;
	int show_boss_health_bar;
	int headlight_active_default;
};

struct visual_write_ctx {
	int alpha_effects;
	int dynlight_color;
};

struct original_homing_write_ctx {
	int original_homing;
};

struct music_write_ctx {
	int source;
	int prefer_mission;
	int play_order;
	int volume;
};

static int write_visual_visitor(const char *path, void *ctx)
{
	struct visual_write_ctx *wc = (struct visual_write_ctx *) ctx;
	return plx_write_visual_prefs(path, wc->alpha_effects, wc->dynlight_color);
}

static int write_original_homing_visitor(const char *path, void *ctx)
{
	struct original_homing_write_ctx *wc = (struct original_homing_write_ctx *) ctx;
	return plx_write_original_homing(path, wc->original_homing);
}

static int write_music_visitor(const char *path, void *ctx)
{
	struct music_write_ctx *wc = (struct music_write_ctx *) ctx;
	return plx_write_music_prefs(path, wc->source, wc->prefer_mission, wc->play_order, wc->volume);
}

#ifdef DXX_BUILD_DESCENT_II
static int write_visitor(const char *path, void *ctx)
{
	struct write_ctx *wc = (struct write_ctx *) ctx;
	return plr_patch_cockpit_autolevel(path, wc->cockpit_mode, wc->auto_leveling, wc->headlight_active_default);
}

static int write_hud_counts_visitor(const char *path, void *ctx)
{
	struct write_ctx *wc = (struct write_ctx *) ctx;
	return plx_write_hud_prefs(path, wc->show_counts, wc->show_boss_health_bar);
}
#else
static int write_cockpit_visitor(const char *path, void *ctx)
{
	struct write_ctx *wc = (struct write_ctx *) ctx;
	int cockpit_result = plx_write_cockpit_mode(path, wc->cockpit_mode);
	int counts_result = plx_write_hud_prefs(path, wc->show_counts, wc->show_boss_health_bar);
	return cockpit_result || counts_result;
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
	int show_counts = 0;
	int show_boss_health_bar = 1;
	int headlight_active_default = 0;
	jint raw[6];
	jintArray result;

	read_engine_prefs(files_dir, &has_pilot, &cockpit_mode, &auto_leveling, &show_counts, &show_boss_health_bar,
	                  &headlight_active_default);
	LOGI("nativeReadEnginePrefs: has_pilot=%d cockpit=%d autolevel=%d counts=%d boss_health=%d headlight_default=%d",
	     has_pilot, cockpit_mode, auto_leveling, show_counts, show_boss_health_bar, headlight_active_default);

	env->ReleaseStringUTFChars(jfilesDir, files_dir);

	raw[0] = (jint) has_pilot;
	raw[1] = (jint) cockpit_mode;
	raw[2] = (jint) (auto_leveling ? 1 : 0);
	raw[3] = (jint) (show_counts ? 1 : 0);
	raw[4] = (jint) (show_boss_health_bar ? 1 : 0);
	raw[5] = (jint) (headlight_active_default ? 1 : 0);
	result = env->NewIntArray(6);
	env->SetIntArrayRegion(result, 0, 6, raw);
	return result;
}

extern "C" JNIEXPORT jint JNICALL
JNI_FUNC(nativeWriteEnginePrefs)(JNIEnv *env,
                                 jclass,
                                 jstring jfilesDir,
                                 jint cockpitMode,
                                 jboolean autoLeveling,
                                 jboolean showRobotHostageCounts,
                                 jboolean showBossHealthBar,
                                 jboolean headlightActiveDefault)
{
	const char *files_dir = env->GetStringUTFChars(jfilesDir, NULL);
	struct write_ctx wc;
	int total;

	wc.cockpit_mode = (int) cockpitMode;
	wc.auto_leveling = autoLeveling ? 1 : 0;
	wc.show_counts = showRobotHostageCounts ? 1 : 0;
	wc.show_boss_health_bar = showBossHealthBar ? 1 : 0;
	wc.headlight_active_default = headlightActiveDefault ? 1 : 0;

#ifdef DXX_BUILD_DESCENT_II
	{
		int plr_total = for_each_pilot(files_dir, "d2x-redux", ".plr", write_visitor, &wc);
		int plx_total = for_each_pilot(files_dir, "d2x-redux", ".plx", write_hud_counts_visitor, &wc);
		total = (plr_total > plx_total) ? plr_total : plx_total;
	}
#else
	{
		int plx_total = for_each_pilot(files_dir, "d1x-redux", ".plx", write_cockpit_visitor, &wc);
		int plr_total = for_each_pilot(files_dir, "d1x-redux", ".plr", write_autolevel_visitor, &wc);
		total = (plx_total > plr_total) ? plx_total : plr_total;
	}
#endif

	LOGI("nativeWriteEnginePrefs: cockpit=%d autolevel=%d counts=%d boss_health=%d headlight_default=%d patched=%d",
	     wc.cockpit_mode, wc.auto_leveling, wc.show_counts, wc.show_boss_health_bar, wc.headlight_active_default, total);
	env->ReleaseStringUTFChars(jfilesDir, files_dir);
	return (jint) total;
}

extern "C" JNIEXPORT jintArray JNICALL
JNI_FUNC(nativeReadVisualPrefs)(JNIEnv *env, jclass, jstring jfilesDir)
{
	const char *files_dir = env->GetStringUTFChars(jfilesDir, NULL);
	int has_pilot = 0;
	int alpha_effects = 0;
	int dynlight_color = 0;
	jint raw[3];
	jintArray result;

	read_visual_prefs(files_dir, &has_pilot, &alpha_effects, &dynlight_color);
	LOGI("nativeReadVisualPrefs: has_pilot=%d alpha=%d dynlight=%d", has_pilot, alpha_effects, dynlight_color);

	env->ReleaseStringUTFChars(jfilesDir, files_dir);

	raw[0] = (jint) has_pilot;
	raw[1] = (jint) (alpha_effects ? 1 : 0);
	raw[2] = (jint) (dynlight_color ? 1 : 0);
	result = env->NewIntArray(3);
	env->SetIntArrayRegion(result, 0, 3, raw);
	return result;
}

extern "C" JNIEXPORT jintArray JNICALL
JNI_FUNC(nativeReadOriginalHomingPrefs)(JNIEnv *env, jclass, jstring jfilesDir)
{
	const char *files_dir = env->GetStringUTFChars(jfilesDir, NULL);
	int has_pilot = 0;
	int original_homing = 0;
	jint raw[2];
	jintArray result;

	read_original_homing_prefs(files_dir, &has_pilot, &original_homing);
	LOGI("nativeReadOriginalHomingPrefs: has_pilot=%d original_homing=%d",
	     has_pilot, original_homing);
	env->ReleaseStringUTFChars(jfilesDir, files_dir);

	raw[0] = (jint) has_pilot;
	raw[1] = (jint) (original_homing ? 1 : 0);
	result = env->NewIntArray(2);
	env->SetIntArrayRegion(result, 0, 2, raw);
	return result;
}

extern "C" JNIEXPORT jint JNICALL
JNI_FUNC(nativeWriteOriginalHomingPrefs)(JNIEnv *env,
                                         jclass,
                                         jstring jfilesDir,
                                         jboolean originalHoming)
{
	const char *files_dir = env->GetStringUTFChars(jfilesDir, NULL);
	struct original_homing_write_ctx wc;
	int total;

	wc.original_homing = originalHoming ? 1 : 0;
#ifdef DXX_BUILD_DESCENT_II
	total = for_each_pilot(files_dir, "d2x-redux", ".plx", write_original_homing_visitor, &wc);
#else
	total = for_each_pilot(files_dir, "d1x-redux", ".plx", write_original_homing_visitor, &wc);
#endif

	LOGI("nativeWriteOriginalHomingPrefs: original_homing=%d patched=%d",
	     wc.original_homing, total);
	env->ReleaseStringUTFChars(jfilesDir, files_dir);
	return (jint) total;
}

extern "C" JNIEXPORT jint JNICALL
JNI_FUNC(nativeWriteVisualPrefs)(JNIEnv *env,
                                 jclass,
                                 jstring jfilesDir,
                                 jboolean alphaEffects,
                                 jboolean dynlightColor)
{
	const char *files_dir = env->GetStringUTFChars(jfilesDir, NULL);
	struct visual_write_ctx wc;
	int total;

	wc.alpha_effects = alphaEffects ? 1 : 0;
	wc.dynlight_color = dynlightColor ? 1 : 0;

#ifdef DXX_BUILD_DESCENT_II
	total = for_each_pilot(files_dir, "d2x-redux", ".plx", write_visual_visitor, &wc);
#else
	total = for_each_pilot(files_dir, "d1x-redux", ".plx", write_visual_visitor, &wc);
#endif

	LOGI("nativeWriteVisualPrefs: alpha=%d dynlight=%d patched=%d", wc.alpha_effects, wc.dynlight_color, total);
	env->ReleaseStringUTFChars(jfilesDir, files_dir);
	return (jint) total;
}

extern "C" JNIEXPORT jintArray JNICALL
JNI_FUNC(nativeReadMusicPrefs)(JNIEnv *env, jclass, jstring jfilesDir)
{
	const char *files_dir = env->GetStringUTFChars(jfilesDir, NULL);
	int has_pilot = 0;
	int source = 2;
	int prefer_mission = 1;
	int play_order = 0;
	int volume = 8;
	jint raw[5];
	jintArray result;

	read_music_prefs(files_dir, &has_pilot, &source, &prefer_mission, &play_order, &volume);
	LOGI("nativeReadMusicPrefs: has_pilot=%d source=%d prefer=%d play_order=%d volume=%d",
	     has_pilot, source, prefer_mission, play_order, volume);

	env->ReleaseStringUTFChars(jfilesDir, files_dir);

	raw[0] = (jint) has_pilot;
	raw[1] = (jint) source;
	raw[2] = (jint) (prefer_mission ? 1 : 0);
	raw[3] = (jint) play_order;
	raw[4] = (jint) volume;
	result = env->NewIntArray(5);
	env->SetIntArrayRegion(result, 0, 5, raw);
	return result;
}

extern "C" JNIEXPORT jint JNICALL
JNI_FUNC(nativeWriteMusicPrefs)(JNIEnv *env,
                                jclass,
                                jstring jfilesDir,
                                jint source,
                                jboolean preferMission,
                                jint playOrder,
                                jint volume)
{
	const char *files_dir = env->GetStringUTFChars(jfilesDir, NULL);
	struct music_write_ctx wc;
	int total;

	wc.source = (int) source;
	wc.prefer_mission = preferMission ? 1 : 0;
	wc.play_order = (int) playOrder;
	wc.volume = (int) volume;

#ifdef DXX_BUILD_DESCENT_II
	total = for_each_pilot(files_dir, "d2x-redux", ".plx", write_music_visitor, &wc);
#else
	total = for_each_pilot(files_dir, "d1x-redux", ".plx", write_music_visitor, &wc);
#endif

	LOGI("nativeWriteMusicPrefs: source=%d prefer=%d play_order=%d volume=%d patched=%d",
	     wc.source, wc.prefer_mission, wc.play_order, wc.volume, total);
	env->ReleaseStringUTFChars(jfilesDir, files_dir);
	return (jint) total;
}

#endif
