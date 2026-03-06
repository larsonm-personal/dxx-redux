/*
 * android_gamepad_config.cpp — Read controller_config.json (written by the
 * Setup-screen UI) and apply gamepad bindings to new pilot configs.
 *
 * Uses nlohmann/json for parsing.
 */

#ifdef ANDROID

#include <nlohmann/json.hpp>
#include <fstream>
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

#endif /* ANDROID */
