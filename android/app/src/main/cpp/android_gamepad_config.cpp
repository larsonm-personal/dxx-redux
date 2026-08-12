/*
 * android_gamepad_config.cpp -- Read controller_config.json (written by the
 * Setup-screen UI) and apply gamepad bindings to new pilot configs.
 *
 * This file is part of the two-layer virtual button/axis mapping model:
 *
 *   Physical input (Kotlin) -> virtual button/axis ID -> game action (kconfig)
 *
 * PlayerCfg.KeySettings[1][action_index] stores the virtual ID that the
 * game engine looks up when processing joystick events.  The virtual IDs
 * are defined by joy_init() in joy.c:
 *
 *   Buttons: 0-9 face/shoulder, 10-21 axis-buttons (2 per axis, neg/pos),
 *            22-25 D-pad, 100+ mixer (MIXER_BTN_BASE + kc_index, bypass button_map)
 *   Axes:    0=LX, 1=LY, 2=RX, 3=RY, 4=LT, 5=RT, 6=BK(virtual), 7=SU(virtual)
 *
 * The JNI entry point patches .plr files by calling plr_patch_keysettings()
 * in playsave.c -- the single source of truth for the .plr binary format.
 *
 * Uses nlohmann/json for parsing.
 */

#ifdef ANDROID

#include <jni.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <cstring>
#include <dirent.h>
#include <android/log.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "android_log.h"
#ifdef __cplusplus
}
#endif

using json = nlohmann::json;

#define LOG_TAG   "DXX-GamepadCfg"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#define PILOT_PASTE_(a, b, c) a##b##c
#define PILOT_PASTE3(a, b, c) PILOT_PASTE_(a, b, c)
#ifdef DXX_BUILD_DESCENT_II
#define PILOT_GAME_TAG D2
#else
#define PILOT_GAME_TAG D1
#endif
#define PILOT_JNI_FUNC(method) \
	PILOT_PASTE3(Java_com_dxxredux_app_NativePilotPatcher_, method, PILOT_GAME_TAG)

/* Engine headers (C linkage) */
extern "C" {
#include "shared/android_graphics_options.h"
#include "shared/android_axis_mailbox.h"
#include "shared/kconfig_android_shared.h"
#include "playsave.h"
#include "kconfig.h"
#include "joy.h"

extern struct player_config PlayerCfg;
}

static const char CONFIG_PATH[] =
    "/data/data/com.dxxredux.app/files/controller_config.json";

static int clamp_threshold_pct(int pct)
{
	if (pct < 5) pct = 5;
	if (pct > 95) pct = 95;
	return pct;
}

static int threshold_pct_to_axis_button_deadzone(int pct)
{
	return clamp_threshold_pct(pct) * 128 / 100;
}

static int threshold_pct_to_playercfg_deadzone(int pct, int scale)
{
	int raw_deadzone = threshold_pct_to_axis_button_deadzone(pct);
	int value = (raw_deadzone + scale / 2) / scale;
	if (value < 0) value = 0;
	if (value > 16) value = 16;
	return value;
}

static bool json_int_in_range(const json &value, int minimum, int maximum,
                              int *result)
{
	if (!result || !value.is_number_integer())
		return false;
	const int64_t parsed = value.get<int64_t>();
	if (parsed < minimum || parsed > maximum)
		return false;
	*result = static_cast<int>(parsed);
	return true;
}

static bool json_byte_array(const json &value, ubyte *output, size_t count,
                            size_t maximum_count)
{
	if (!output || !value.is_array() || value.size() < count ||
	    value.size() > maximum_count)
		return false;
	for (size_t index = 0; index < value.size(); ++index) {
		int parsed;
		if (!json_int_in_range(value[index], 0, 255, &parsed))
			return false;
		if (index < count)
			output[index] = static_cast<ubyte>(parsed);
	}
	return true;
}

/*
 * Read controller_config.json and apply key_settings arrays + control type
 * into the current PlayerCfg.  Returns true if a config was loaded.
 */
static bool load_config_into_playercfg(void)
{
	std::ifstream ifs(CONFIG_PATH);
	struct player_config staged;
	int staged_axis_deadzone[6];
	int axis_pct[6];
	int control_type;
	int automap_free_flight;
	json cfg;
	const kconfig_android_layout *const layout = kconfig_android_get_layout(
#ifdef DXX_BUILD_DESCENT_II
	    KCONFIG_ANDROID_D2
#else
	    KCONFIG_ANDROID_D1
#endif
	);
	if (!ifs.is_open())
		return false;
	staged = PlayerCfg;

	/* Prefer game-specific byte array (D1/D2 have different kconfig indices) */
#ifdef DXX_BUILD_DESCENT_II
	const char *joy_key = "key_settings_joystick_d2";
#else
	const char *joy_key = "key_settings_joystick_d1";
#endif
	static const struct {
		const char *name;
		int axis;
	} axis_map[] = {
		{ "LS_X", 0 },
		{ "LS_Y", 1 },
		{ "RS_X", 2 },
		{ "RS_Y", 3 },
		{ "LT", 4 },
		{ "RT", 5 },
	};
	static const struct {
		int keysettings_index;
		int deadzone_index;
		int scale;
	} analog_deadzone_map[] = {
		{ 15, 0, 8 },
		{ 13, 1, 8 },
		{ 17, 2, 8 },
		{ 19, 3, 8 },
		{ 21, 4, 8 },
		{ 23, 5, 3 },
	};

	try {
		int version;
		cfg = json::parse(ifs);
		if (!cfg.is_object() || !cfg.contains("version") ||
		    !json_int_in_range(cfg["version"], 4, 4, &version) ||
		    !cfg.contains(joy_key) ||
		    !json_byte_array(cfg[joy_key], staged.KeySettings[1],
		                     layout->joystick_size, layout->joystick_size) ||
		    !cfg.contains("key_settings_keyboard") ||
		    !json_byte_array(cfg["key_settings_keyboard"], staged.KeySettings[0],
		                     layout->settings_size, KCONFIG_ANDROID_MAX_SETTINGS) ||
		    !cfg.contains("control_type") ||
		    !json_int_in_range(cfg["control_type"], 0, 3, &control_type) ||
		    !cfg.contains("automap_free_flight") ||
		    !json_int_in_range(cfg["automap_free_flight"], 0, 1, &automap_free_flight) ||
		    !cfg.contains("thresholds") || !cfg["thresholds"].is_object())
			return false;
		staged.ControlType = static_cast<ubyte>(control_type);
		staged.AutomapFreeFlight = static_cast<ubyte>(automap_free_flight);
		for (size_t i = 0; i < sizeof(axis_map) / sizeof(axis_map[0]); ++i) {
			int pct;
			if (!cfg["thresholds"].contains(axis_map[i].name) ||
			    !json_int_in_range(cfg["thresholds"][axis_map[i].name], 5, 95, &pct))
				return false;
			axis_pct[axis_map[i].axis] = pct;
			staged_axis_deadzone[axis_map[i].axis] =
			    threshold_pct_to_axis_button_deadzone(pct);
		}
		for (size_t i = 0; i < sizeof(analog_deadzone_map) / sizeof(analog_deadzone_map[0]); ++i) {
			const int bound_axis = staged.KeySettings[1][analog_deadzone_map[i].keysettings_index];
			if (bound_axis >= 0 && bound_axis < 6)
				staged.JoystickDead[analog_deadzone_map[i].deadzone_index] =
				    threshold_pct_to_playercfg_deadzone(
				        axis_pct[bound_axis], analog_deadzone_map[i].scale);
		}
	} catch (...) {
		return false;
	}

	PlayerCfg = staged;
	for (int axis = 0; axis < 6; ++axis) {
		joy_axis_button_deadzone[axis] = staged_axis_deadzone[axis];
		android_axis_mailbox_set_button_deadzone(axis, staged_axis_deadzone[axis]);
	}
	return true;
}

static void apply_android_virtual_axis_defaults(void)
{
	/* Virtual gyro axes: only apply defaults if the user's config didn't
	 * specify values for these slots.  A half-axis combiner or explicit
	 * axis binding writes a real value (< 0xFF); clobbering it with the
	 * gyro axis would orphan the user's mapping. */
	if (PlayerCfg.KeySettings[1][19] == 0xFF)
		PlayerCfg.KeySettings[1][19] = 7; /* Slide U/D = axis 7 (SU) */
	if (PlayerCfg.KeySettings[1][21] == 0xFF)
		PlayerCfg.KeySettings[1][21] = 6; /* Bank L/R  = axis 6 (BK) */
}

extern "C" int android_reload_live_gamepad_config(void)
{
	if (!load_config_into_playercfg())
		return 0;

	apply_android_virtual_axis_defaults();
	LOGI("Reloaded live controller config: joy[1]=%d joy[19]=%d joy[21]=%d ctl=%d",
	     PlayerCfg.KeySettings[1][1], PlayerCfg.KeySettings[1][19],
	     PlayerCfg.KeySettings[1][21], PlayerCfg.ControlType);
	debug_log(DLOG_GAME,
	          "[joy-map] reload ctl=%d pitch=%d turn=%d slide=%d throttle=%d\n",
	          PlayerCfg.ControlType,
	          PlayerCfg.KeySettings[1][13],
	          PlayerCfg.KeySettings[1][15],
	          PlayerCfg.KeySettings[1][17],
	          PlayerCfg.KeySettings[1][23]);
	return 1;
}

/*
 * Called from menu.c when creating a brand-new pilot on Android.
 * Reads the JSON config if available; otherwise falls back to hardcoded
 * defaults for a typical Android gamepad.
 */
extern "C" void android_apply_gamepad_defaults(void)
{
	PlayerCfg.ControlType = 1; /* CONTROL_USING_JOYSTICK */
	PlayerCfg.AutomapFreeFlight = 1;

	if (!android_reload_live_gamepad_config()) {
		/* SetupActivity.writeDefaultControllerConfig() writes a config file
		 * from the bundled default.json asset before the game starts, so
		 * this path should not be reached in normal operation. */
		LOGI("No controller_config.json found -- controller bindings may be incomplete");
	} else {
		LOGI("Loaded controller config: joy[13]=%d joy[15]=%d joy[17]=%d joy[23]=%d",
		     PlayerCfg.KeySettings[1][13], PlayerCfg.KeySettings[1][15],
		     PlayerCfg.KeySettings[1][17], PlayerCfg.KeySettings[1][23]);
	}
	apply_android_virtual_axis_defaults();
	android_graphics_apply_pilot_defaults();
}

/* -- JNI entry point: patch all .plr files ----------------------- */

/* Scan files_dir for .plr files in the specified game's pref dir and patch each.
 * game must be "d1" or "d2". */
static int patch_all_plr_files(const char *files_dir,
                               const char *game,
                               const ubyte *kb, int kb_len,
                               const ubyte *joy, int joy_len,
                               const ubyte *mouse, int mouse_len,
                               int controlType)
{
	int total = 0;
	/* Scan only the specified game's pref dir. */
	const char *subdir = (strcmp(game, "d1") == 0) ? "d1x-redux" : "d2x-redux";
	char base_dir[512], players_dir[512];
	snprintf(base_dir, sizeof(base_dir), "%s/%s", files_dir, subdir);
	snprintf(players_dir, sizeof(players_dir), "%s/%s/Players", files_dir, subdir);
	const char *dirs[] = { base_dir, players_dir };

	for (int d = 0; d < 2; d++) {
		DIR *dp = opendir(dirs[d]);
		if (!dp) continue;
		struct dirent *ent;
		while ((ent = readdir(dp)) != NULL) {
			size_t nlen = strlen(ent->d_name);
			if (nlen < 5) continue;
			if (strcasecmp(ent->d_name + nlen - 4, ".plr") != 0) continue;

			char path[512];
			snprintf(path, sizeof(path), "%s/%s", dirs[d], ent->d_name);
			total += plr_patch_keysettings(path,
			                               (const ubyte *) kb, (int) kb_len,
			                               (const ubyte *) joy, (int) joy_len,
			                               (const ubyte *) mouse, (int) mouse_len,
			                               (int) controlType);
		}
		closedir(dp);
	}
	return total;
}

extern "C" JNIEXPORT jint JNICALL
PILOT_JNI_FUNC(nativePatchPilotFiles)(
    JNIEnv *env, jclass clazz,
    jstring jfilesDir, jbyteArray jjoy, jbyteArray jkb, jint controlType,
    jstring jgame)
{
	const char *files_dir = env->GetStringUTFChars(jfilesDir, NULL);
	const char *game = env->GetStringUTFChars(jgame, NULL);

	jbyte *joy = env->GetByteArrayElements(jjoy, NULL);
	jint joy_len = env->GetArrayLength(jjoy);

	jbyte *kb = env->GetByteArrayElements(jkb, NULL);
	jint kb_len = env->GetArrayLength(jkb);

	int total = patch_all_plr_files(files_dir, game,
	                                (const ubyte *) kb, (int) kb_len,
	                                (const ubyte *) joy, (int) joy_len,
	                                NULL, 0, (int) controlType);

	LOGI("nativePatchPilotFiles[%s]: patched %d file(s) in %s", game, total, files_dir);

	env->ReleaseByteArrayElements(jkb, kb, JNI_ABORT);
	env->ReleaseByteArrayElements(jjoy, joy, JNI_ABORT);
	env->ReleaseStringUTFChars(jgame, game);
	env->ReleaseStringUTFChars(jfilesDir, files_dir);

	return (jint) total;
}

/* -- JNI: reset all pilot files to engine defaults ------------ */

extern "C" JNIEXPORT jint JNICALL
PILOT_JNI_FUNC(nativeResetToDefaults)(
    JNIEnv *env, jclass, jstring jfilesDir, jstring jgame)
{
	ubyte kb[KCONFIG_ANDROID_MAX_SETTINGS];
	ubyte joy[KCONFIG_ANDROID_MAX_SETTINGS];
	ubyte mouse[KCONFIG_ANDROID_MAX_SETTINGS];
	const char *files_dir = env->GetStringUTFChars(jfilesDir, NULL);
	const char *game = env->GetStringUTFChars(jgame, NULL);
	const kconfig_android_layout *layout = kconfig_android_get_layout(
	    !strcmp(game, "d1") ? KCONFIG_ANDROID_D1 : KCONFIG_ANDROID_D2);
	memset(kb, 0xff, sizeof(kb));
	memset(joy, 0xff, sizeof(joy));
	memset(mouse, 0xff, sizeof(mouse));
	kconfig_get_default_settings(kb, joy, mouse);
	int total = patch_all_plr_files(files_dir, game,
	                                kb, (int) layout->settings_size,
	                                joy, (int) layout->settings_size,
	                                mouse, (int) layout->settings_size, 1);
	LOGI("nativeResetToDefaults[%s]: patched %d file(s) in %s", game, total, files_dir);
	env->ReleaseStringUTFChars(jgame, game);
	env->ReleaseStringUTFChars(jfilesDir, files_dir);
	return (jint) total;
}

/* -- JNI: build joystick KeySettings from (index, value) pairs -- */

/*
 * Takes parallel int arrays of kc_joystick[] indices and corresponding
 * values.  Returns a MAX_CONTROLS-byte array with correct defaults
 * (BT_INVERT slots = 0, others = 0xFF) and the specified values filled in.
 *
 * The indices are shared constants defined in ControllerConfigPage.kt
 * (BUTTON_KC_INDEX, AXIS_KC_INDEX) -- update both when the layout changes.
 */
extern "C" JNIEXPORT jbyteArray JNICALL
PILOT_JNI_FUNC(nativeBuildJoySettings)(
    JNIEnv *env, jclass, jintArray jindices, jintArray jvalues, jstring jgame)
{
	jint index_count = env->GetArrayLength(jindices);
	jint value_count = env->GetArrayLength(jvalues);
	jint count = index_count < value_count ? index_count : value_count;
	jint *indices = env->GetIntArrayElements(jindices, NULL);
	jint *values = env->GetIntArrayElements(jvalues, NULL);
	const char *game = env->GetStringUTFChars(jgame, NULL);
	const kconfig_android_layout *layout = kconfig_android_get_layout(
	    !strcmp(game, "d1") ? KCONFIG_ANDROID_D1 : KCONFIG_ANDROID_D2);

	ubyte out[KCONFIG_ANDROID_MAX_SETTINGS];
	kconfig_android_fill_joy_settings(layout, (const int *) indices,
	                                  (const int *) values, (int) count, out,
	                                  layout->joystick_size);

	env->ReleaseStringUTFChars(jgame, game);
	env->ReleaseIntArrayElements(jvalues, values, JNI_ABORT);
	env->ReleaseIntArrayElements(jindices, indices, JNI_ABORT);

	jbyteArray result = env->NewByteArray((jsize) layout->joystick_size);
	env->SetByteArrayRegion(result, 0, (jsize) layout->joystick_size,
	                        (const jbyte *) out);
	return result;
}

/* -- JNI: build keyboard KeySettings from (index, value) pairs -- */

extern "C" JNIEXPORT jbyteArray JNICALL
PILOT_JNI_FUNC(nativeBuildKbSettings)(
    JNIEnv *env, jclass, jintArray jindices, jintArray jvalues, jstring jgame)
{
	jint index_count = env->GetArrayLength(jindices);
	jint value_count = env->GetArrayLength(jvalues);
	jint count = index_count < value_count ? index_count : value_count;
	jint *indices = env->GetIntArrayElements(jindices, NULL);
	jint *values = env->GetIntArrayElements(jvalues, NULL);
	const char *game = env->GetStringUTFChars(jgame, NULL);
	const kconfig_android_layout *layout = kconfig_android_get_layout(
	    !strcmp(game, "d1") ? KCONFIG_ANDROID_D1 : KCONFIG_ANDROID_D2);

	ubyte out[KCONFIG_ANDROID_MAX_SETTINGS];
	kconfig_android_fill_kb_settings(
	    DefaultKeySettings[0], (const int *) indices,
	    (const int *) values, (int) count, out, layout->settings_size);

	env->ReleaseStringUTFChars(jgame, game);
	env->ReleaseIntArrayElements(jvalues, values, JNI_ABORT);
	env->ReleaseIntArrayElements(jindices, indices, JNI_ABORT);

	jbyteArray result = env->NewByteArray((jsize) layout->settings_size);
	env->SetByteArrayRegion(result, 0, (jsize) layout->settings_size,
	                        (const jbyte *) out);
	return result;
}

#endif /* ANDROID */
