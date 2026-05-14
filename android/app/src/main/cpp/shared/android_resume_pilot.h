#ifndef DXX_REDUX_ANDROID_RESUME_PILOT_H
#define DXX_REDUX_ANDROID_RESUME_PILOT_H

#ifdef __cplusplus
extern "C" {
#endif

int android_load_pilot_from_resume_save(const char *save_path, const char *game_name);
void android_repair_player_callsign_for_autosave(const char *game_name);

#ifdef __cplusplus
}
#endif

#endif