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

using json = nlohmann::json;

#define LOG_TAG   "DXX-GamepadCfg"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

/* Engine headers (C linkage) */
extern "C" {
#include "playsave.h"
#include "kconfig.h"
#include "joy.h"

extern struct player_config PlayerCfg;
}

static const char CONFIG_PATH[] =
    "/data/data/com.dxxredux.app/files/controller_config.json";

/*
 * Read controller_config.json and apply key_settings arrays + control type
 * into the current PlayerCfg.  Returns true if a config was loaded.
 */
static bool load_config_into_playercfg(void)
{
	std::ifstream ifs(CONFIG_PATH);
	if (!ifs.is_open()) return false;

	json cfg;
	try {
		cfg = json::parse(ifs);
	} catch (...) {
		return false;
	}

	/* Prefer game-specific byte array (D1/D2 have different kconfig indices) */
#ifdef DXX_BUILD_DESCENT_II
	const char *joy_key = "key_settings_joystick_d2";
#else
	const char *joy_key = "key_settings_joystick_d1";
#endif
	/* Fall back to the old unqualified key for pre-existing configs */
	if (!cfg.contains(joy_key) || !cfg[joy_key].is_array()) {
		joy_key = "key_settings_joystick";
	}
	if (cfg.contains(joy_key) && cfg[joy_key].is_array()) {
		auto &arr = cfg[joy_key];
		for (size_t i = 0; i < arr.size() && i < MAX_CONTROLS; i++)
			PlayerCfg.KeySettings[1][i] = (ubyte) arr[i].get<int>();
	}
	if (cfg.contains("key_settings_keyboard") && cfg["key_settings_keyboard"].is_array()) {
		auto &arr = cfg["key_settings_keyboard"];
		for (size_t i = 0; i < arr.size() && i < MAX_CONTROLS; i++)
			PlayerCfg.KeySettings[0][i] = (ubyte) arr[i].get<int>();
	}
	if (cfg.contains("control_type"))
		PlayerCfg.ControlType = (ubyte) cfg["control_type"].get<int>();
	if (cfg.contains("automap_free_flight"))
		PlayerCfg.AutomapFreeFlight = (ubyte) cfg["automap_free_flight"].get<int>();

	/* Per-axis thresholds for axis-to-button conversion.
	 * JSON stores percentage (5-95), C uses 0-128 scale. */
	if (cfg.contains("thresholds") && cfg["thresholds"].is_object()) {
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
		auto &thr = cfg["thresholds"];
		for (size_t i = 0; i < sizeof(axis_map) / sizeof(axis_map[0]); i++) {
			if (thr.contains(axis_map[i].name) && thr[axis_map[i].name].is_number()) {
				int pct = thr[axis_map[i].name].get<int>();
				if (pct < 5) pct = 5;
				if (pct > 95) pct = 95;
				joy_axis_button_deadzone[axis_map[i].axis] = pct * 128 / 100;
			}
		}
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
Java_com_dxxredux_app_NativePilotPatcher_nativePatchPilotFiles(
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
Java_com_dxxredux_app_NativePilotPatcher_nativeResetToDefaults(
    JNIEnv *env, jclass, jstring jfilesDir, jstring jgame)
{
	ubyte kb[MAX_CONTROLS], joy[MAX_CONTROLS], mouse[MAX_CONTROLS];
	kconfig_get_default_settings(kb, joy, mouse);

	const char *files_dir = env->GetStringUTFChars(jfilesDir, NULL);
	const char *game = env->GetStringUTFChars(jgame, NULL);
	int total = patch_all_plr_files(files_dir, game,
	                                kb, MAX_CONTROLS, joy, MAX_CONTROLS, mouse, MAX_CONTROLS, 1);
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
Java_com_dxxredux_app_NativePilotPatcher_nativeBuildJoySettings(
    JNIEnv *env, jclass, jintArray jindices, jintArray jvalues)
{
	jint count = env->GetArrayLength(jindices);
	jint *indices = env->GetIntArrayElements(jindices, NULL);
	jint *values = env->GetIntArrayElements(jvalues, NULL);

	ubyte out[MAX_CONTROLS];
	kconfig_fill_joy_settings((const int *) indices, (const int *) values, (int) count, out);

	env->ReleaseIntArrayElements(jvalues, values, JNI_ABORT);
	env->ReleaseIntArrayElements(jindices, indices, JNI_ABORT);

	jbyteArray result = env->NewByteArray(MAX_CONTROLS);
	env->SetByteArrayRegion(result, 0, MAX_CONTROLS, (const jbyte *) out);
	return result;
}

/* -- JNI: build keyboard KeySettings from (index, value) pairs -- */

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_dxxredux_app_NativePilotPatcher_nativeBuildKbSettings(
    JNIEnv *env, jclass, jintArray jindices, jintArray jvalues)
{
	jint count = env->GetArrayLength(jindices);
	jint *indices = env->GetIntArrayElements(jindices, NULL);
	jint *values = env->GetIntArrayElements(jvalues, NULL);

	ubyte out[MAX_CONTROLS];
	kconfig_fill_kb_settings((const int *) indices, (const int *) values, (int) count, out);

	env->ReleaseIntArrayElements(jvalues, values, JNI_ABORT);
	env->ReleaseIntArrayElements(jindices, indices, JNI_ABORT);

	jbyteArray result = env->NewByteArray(MAX_CONTROLS);
	env->SetByteArrayRegion(result, 0, MAX_CONTROLS, (const jbyte *) out);
	return result;
}

#endif /* ANDROID */
