/* Android pack loading and evidence, used only on the engine thread */
#ifndef ANDROID_MOD_ASSETS_H
#define ANDROID_MOD_ASSETS_H
#ifdef __cplusplus
extern "C" {
#endif
void android_mod_sounds_reset(void);
#if defined(DXX_XL_PILOT) && defined(DXX_BUILD_DESCENT_II)
void android_mod_sound_level_prepare(int ordinal);
#endif
void android_mod_sound_apply(int sample, const char *name, int content_game);
#ifdef DXX_BUILD_DESCENT_II
void android_mod_sound_timing_apply(int sample);
#endif
void android_mod_sound_loaded(int sample, const char *filename);
void android_mod_sound_bank_loaded(int sample, const char *filename);
void android_mod_sound_played(int sample);
void android_mod_asset_loaded(const char *kind, const char *filename, int width, int height);
void android_mod_usage_flush(void);
#ifdef __cplusplus
}
#include <nlohmann/json_fwd.hpp>
nlohmann::json android_mod_usage_snapshot();
#endif
#endif
