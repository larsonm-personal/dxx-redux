/*
 * android_gamepad_config.cpp — Read controller_config.json (written by the
 * Setup-screen UI) and apply gamepad bindings to new pilot configs.
 *
 * Also provides android_patch_plr_files() which patches existing .plr
 * binary pilot files in-place with joystick/keyboard KeySettings, so the
 * launcher's "Save (to all pilots)" writes directly to the engine's format.
 *
 * Uses nlohmann/json for parsing.
 */

#ifdef ANDROID

#include <jni.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <android/log.h>

using json = nlohmann::json;

#define LOG_TAG "DXX-GamepadCfg"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

/* Engine headers (C linkage) */
extern "C" {
#include "playsave.h"

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
 * Called from new_player_config() to set gamepad defaults for new pilots.
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
        PlayerCfg.KeySettings[1][19] = 1;   /* Slide U/D     = axis 1 (LY)*/
    }
}

/* ── .plr binary patching ────────────────────────────────────────────
 *
 * Binary layout of a version-24 .plr file up to KeySettings:
 *
 *   Offset  Size  Field
 *   ──────  ────  ─────
 *   0       4     SAVE_FILE_ID  (LE uint32 = 0x44504C52, "DPLR")
 *   4       2     version       (LE uint16)
 *   6       4     window w/h    (skip)
 *   10      9     single-byte fields (difficulty..automap_hires)
 *   19      2     NHighestLevels (LE uint16)
 *   21      N*10  HighestLevels array
 *   21+N*10 140   NetworkMessageMacro (4 * 35)
 *   ── ksBase = 21 + N*10 + 140 ──
 *   ksBase+0    60  KeySettings[0] keyboard
 *   ksBase+60   60  KeySettings[1] joystick
 *   ksBase+120 180  obsolete fields (zeros)
 *   ksBase+300  60  KeySettings[2] mouse
 *   ksBase+360 120  obsolete fields (zeros)
 *   ksBase+480   1  control_type_dos
 */

#define PLR_SAVE_FILE_ID   0x44504C52u   /* MAKE_SIG('D','P','L','R') as LE */
#define PLR_MIN_VERSION    17
#define PLR_FIXED_HEADER   19            /* bytes before NHighestLevels   */
#define PLR_HLI_SIZE       10            /* sizeof(hli) = char[9]+ubyte  */
#define PLR_MSG_BLOCK      140           /* 4 macros * 35 chars          */
#define PLR_KS_KB_OFF      0             /* KeySettings[0] rel to ksBase */
#define PLR_KS_JOY_OFF     60            /* KeySettings[1] rel to ksBase */
#define PLR_CT_OFF         480           /* control_type_dos rel to ksBase */

static uint16_t read_le16(FILE *f)
{
    unsigned char b[2];
    if (fread(b, 1, 2, f) != 2) return 0;
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

static uint32_t read_le32(FILE *f)
{
    unsigned char b[4];
    if (fread(b, 1, 4, f) != 4) return 0;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

/*
 * Patch a single .plr file in-place with the given KeySettings arrays
 * and control_type.  Returns 1 on success, 0 on failure.
 */
static int patch_one_plr(const char *path,
                         const unsigned char *joy, int joy_len,
                         const unsigned char *kb,  int kb_len,
                         int control_type)
{
    FILE *f = fopen(path, "r+b");
    if (!f) return 0;

    uint32_t id = read_le32(f);
    if (id != PLR_SAVE_FILE_ID) {
        LOGI("patch_one_plr: bad ID in %s (0x%08x)", path, id);
        fclose(f);
        return 0;
    }

    uint16_t ver = read_le16(f);
    if (ver < PLR_MIN_VERSION) {
        LOGI("patch_one_plr: old version %d in %s", ver, path);
        fclose(f);
        return 0;
    }

    /* Read NHighestLevels at offset 19 */
    fseek(f, PLR_FIXED_HEADER, SEEK_SET);
    uint16_t n_highest = read_le16(f);

    long ks_base = PLR_FIXED_HEADER + 2 + (long)n_highest * PLR_HLI_SIZE + PLR_MSG_BLOCK;

    /* Sanity: check file is large enough */
    fseek(f, 0, SEEK_END);
    long file_len = ftell(f);
    if (file_len < ks_base + PLR_CT_OFF + 1) {
        LOGI("patch_one_plr: %s too short (%ld < %ld)", path, file_len, ks_base + PLR_CT_OFF + 1);
        fclose(f);
        return 0;
    }

    /* Patch KeySettings[0] keyboard */
    fseek(f, ks_base + PLR_KS_KB_OFF, SEEK_SET);
    fwrite(kb, 1, kb_len < MAX_CONTROLS ? kb_len : MAX_CONTROLS, f);

    /* Patch KeySettings[1] joystick */
    fseek(f, ks_base + PLR_KS_JOY_OFF, SEEK_SET);
    fwrite(joy, 1, joy_len < MAX_CONTROLS ? joy_len : MAX_CONTROLS, f);

    /* Patch control_type_dos */
    fseek(f, ks_base + PLR_CT_OFF, SEEK_SET);
    unsigned char ct = (unsigned char)control_type;
    fwrite(&ct, 1, 1, f);

    fclose(f);
    LOGI("patch_one_plr: patched %s (nHighest=%d, ksBase=%ld)", path, n_highest, ks_base);
    return 1;
}

/*
 * Scan a directory for .plr files and patch each one.
 * Returns the number of files patched.
 */
static int patch_dir(const char *dir_path,
                     const unsigned char *joy, int joy_len,
                     const unsigned char *kb,  int kb_len,
                     int control_type)
{
    DIR *d = opendir(dir_path);
    if (!d) return 0;

    int patched = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        size_t nlen = strlen(ent->d_name);
        if (nlen < 5) continue;
        if (strcasecmp(ent->d_name + nlen - 4, ".plr") != 0) continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dir_path, ent->d_name);
        patched += patch_one_plr(path, joy, joy_len, kb, kb_len, control_type);
    }
    closedir(d);
    return patched;
}

/*
 * Patch all .plr files in files_dir and files_dir/Players/.
 * Called via JNI from the launcher's "Save (to all pilots)" button.
 */
extern "C" int android_patch_plr_files(const char *files_dir,
                                       const unsigned char *joy, int joy_len,
                                       const unsigned char *kb,  int kb_len,
                                       int control_type)
{
    int total = 0;
    total += patch_dir(files_dir, joy, joy_len, kb, kb_len, control_type);

    char players_dir[PATH_MAX];
    snprintf(players_dir, sizeof(players_dir), "%s/Players", files_dir);
    total += patch_dir(players_dir, joy, joy_len, kb, kb_len, control_type);

    LOGI("android_patch_plr_files: patched %d file(s) in %s", total, files_dir);
    return total;
}

/* ── JNI entry point ─────────────────────────────────────────────── */

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_NativePilotPatcher_nativePatchPilotFiles(
    JNIEnv *env, jclass clazz,
    jstring jfilesDir, jbyteArray jjoy, jbyteArray jkb, jint controlType)
{
    const char *files_dir = env->GetStringUTFChars(jfilesDir, NULL);

    jbyte *joy = env->GetByteArrayElements(jjoy, NULL);
    jint joy_len = env->GetArrayLength(jjoy);

    jbyte *kb = env->GetByteArrayElements(jkb, NULL);
    jint kb_len = env->GetArrayLength(jkb);

    int result = android_patch_plr_files(files_dir,
                                         (const unsigned char *)joy, (int)joy_len,
                                         (const unsigned char *)kb,  (int)kb_len,
                                         (int)controlType);

    env->ReleaseByteArrayElements(jkb, kb, JNI_ABORT);
    env->ReleaseByteArrayElements(jjoy, joy, JNI_ABORT);
    env->ReleaseStringUTFChars(jfilesDir, files_dir);

    return (jint)result;
}

#endif /* ANDROID */
