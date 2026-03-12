/*
 * android_gamepad_config.cpp -- Read controller_config.json (written by the
 * Setup-screen UI) and apply gamepad bindings to new pilot configs.
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

#define LOG_TAG "DXX-GamepadCfg"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

/* Engine headers (C linkage) */
extern "C" {
#include "playsave.h"
#include "kconfig.h"

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
    try { cfg = json::parse(ifs); }
    catch (...) { return false; }

    if (cfg.contains("key_settings_joystick") && cfg["key_settings_joystick"].is_array()) {
        auto &arr = cfg["key_settings_joystick"];
        for (size_t i = 0; i < arr.size() && i < MAX_CONTROLS; i++)
            PlayerCfg.KeySettings[1][i] = (ubyte)arr[i].get<int>();
    }
    if (cfg.contains("key_settings_keyboard") && cfg["key_settings_keyboard"].is_array()) {
        auto &arr = cfg["key_settings_keyboard"];
        for (size_t i = 0; i < arr.size() && i < MAX_CONTROLS; i++)
            PlayerCfg.KeySettings[0][i] = (ubyte)arr[i].get<int>();
    }
    if (cfg.contains("control_type"))
        PlayerCfg.ControlType = (ubyte)cfg["control_type"].get<int>();
    if (cfg.contains("automap_free_flight"))
        PlayerCfg.AutomapFreeFlight = (ubyte)cfg["automap_free_flight"].get<int>();

    return true;
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

    if (!load_config_into_playercfg()) {
        /* Hardcoded defaults when no config file exists */
        PlayerCfg.KeySettings[1][0]  = 0;   /* Fire Primary  = A button   */
        PlayerCfg.KeySettings[1][1]  = 1;   /* Fire Secondary= B button   */
        PlayerCfg.KeySettings[1][2]  = 21;  /* Accelerate    = +RT axis   */
        PlayerCfg.KeySettings[1][3]  = 19;  /* Reverse       = +LT axis   */
        PlayerCfg.KeySettings[1][13] = 3;   /* Pitch U/D     = axis 3 (RY)*/
        PlayerCfg.KeySettings[1][15] = 2;   /* Turn L/R      = axis 2 (RX)*/
        PlayerCfg.KeySettings[1][17] = 0;   /* Slide L/R     = axis 0 (LX)*/
        PlayerCfg.KeySettings[1][23] = 1;   /* Throttle      = axis 1 (LY)*/
    }
}

/* ── JNI entry point: patch all .plr files ─────────────────────── */

/* Scan files_dir (and Players/ subdir) for .plr files and patch each. */
static int patch_all_plr_files(const char *files_dir,
    const ubyte *kb, int kb_len,
    const ubyte *joy, int joy_len,
    const ubyte *mouse, int mouse_len,
    int controlType)
{
    int total = 0;
    /* Scan game-specific pref dirs where the engine actually writes .plr files.
     * PhysFS creates d1x-redux/ and d2x-redux/ under files_dir. */
    char d2_dir[512], d1_dir[512], d2_players[512], d1_players[512];
    snprintf(d2_dir, sizeof(d2_dir), "%s/d2x-redux", files_dir);
    snprintf(d1_dir, sizeof(d1_dir), "%s/d1x-redux", files_dir);
    snprintf(d2_players, sizeof(d2_players), "%s/d2x-redux/Players", files_dir);
    snprintf(d1_players, sizeof(d1_players), "%s/d1x-redux/Players", files_dir);
    const char *dirs[] = { d2_dir, d1_dir, d2_players, d1_players };

    for (int d = 0; d < 4; d++) {
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
                (const ubyte *)kb, (int)kb_len,
                (const ubyte *)joy, (int)joy_len,
                (const ubyte *)mouse, (int)mouse_len,
                (int)controlType);
        }
        closedir(dp);
    }
    return total;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_dxxredux_app_NativePilotPatcher_nativePatchPilotFiles(
    JNIEnv *env, jclass clazz,
    jstring jfilesDir, jbyteArray jjoy, jbyteArray jkb, jint controlType)
{
    const char *files_dir = env->GetStringUTFChars(jfilesDir, NULL);

    jbyte *joy = env->GetByteArrayElements(jjoy, NULL);
    jint joy_len = env->GetArrayLength(jjoy);

    jbyte *kb = env->GetByteArrayElements(jkb, NULL);
    jint kb_len = env->GetArrayLength(jkb);

    int total = patch_all_plr_files(files_dir,
        (const ubyte *)kb, (int)kb_len,
        (const ubyte *)joy, (int)joy_len,
        NULL, 0, (int)controlType);

    LOGI("nativePatchPilotFiles: patched %d file(s) in %s", total, files_dir);

    env->ReleaseByteArrayElements(jkb, kb, JNI_ABORT);
    env->ReleaseByteArrayElements(jjoy, joy, JNI_ABORT);
    env->ReleaseStringUTFChars(jfilesDir, files_dir);

    return (jint)total;
}

/* ── JNI: reset all pilot files to engine defaults ──────────── */

extern "C" JNIEXPORT jint JNICALL
Java_com_dxxredux_app_NativePilotPatcher_nativeResetToDefaults(
    JNIEnv *env, jclass, jstring jfilesDir)
{
    ubyte kb[MAX_CONTROLS], joy[MAX_CONTROLS], mouse[MAX_CONTROLS];
    kconfig_get_default_settings(kb, joy, mouse);

    const char *files_dir = env->GetStringUTFChars(jfilesDir, NULL);
    int total = patch_all_plr_files(files_dir,
        kb, MAX_CONTROLS, joy, MAX_CONTROLS, mouse, MAX_CONTROLS, 1);
    LOGI("nativeResetToDefaults: patched %d file(s) in %s", total, files_dir);
    env->ReleaseStringUTFChars(jfilesDir, files_dir);
    return (jint)total;
}

/* ── JNI: build joystick KeySettings from (index, value) pairs ── */

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
    kconfig_fill_joy_settings((const int *)indices, (const int *)values, (int)count, out);

    env->ReleaseIntArrayElements(jvalues, values, JNI_ABORT);
    env->ReleaseIntArrayElements(jindices, indices, JNI_ABORT);

    jbyteArray result = env->NewByteArray(MAX_CONTROLS);
    env->SetByteArrayRegion(result, 0, MAX_CONTROLS, (const jbyte *)out);
    return result;
}

/* ── JNI: build keyboard KeySettings from (index, value) pairs ── */

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_dxxredux_app_NativePilotPatcher_nativeBuildKbSettings(
    JNIEnv *env, jclass, jintArray jindices, jintArray jvalues)
{
    jint count = env->GetArrayLength(jindices);
    jint *indices = env->GetIntArrayElements(jindices, NULL);
    jint *values = env->GetIntArrayElements(jvalues, NULL);

    ubyte out[MAX_CONTROLS];
    kconfig_fill_kb_settings((const int *)indices, (const int *)values, (int)count, out);

    env->ReleaseIntArrayElements(jvalues, values, JNI_ABORT);
    env->ReleaseIntArrayElements(jindices, indices, JNI_ABORT);

    jbyteArray result = env->NewByteArray(MAX_CONTROLS);
    env->SetByteArrayRegion(result, 0, MAX_CONTROLS, (const jbyte *)out);
    return result;
}

#endif /* ANDROID */
