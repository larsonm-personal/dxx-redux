#ifndef ANDROID_SAVE_SET_H
#define ANDROID_SAVE_SET_H

#include <stddef.h>

#define ANDROID_SAVE_SET_ROOT_PLAYERS  "Players/save_sets"
#define ANDROID_SAVE_SET_ROOT_LOCAL    "save_sets"
#define ANDROID_SAVE_SET_COOP_CALLSIGN "coopsave"

void android_save_set_sanitize_component(char *dst, size_t dst_size,
                                         const char *src,
                                         const char *fallback);
int android_save_set_build_slot_path(char *dst, size_t dst_size,
                                     int use_players_dir,
                                     const char *scope,
                                     const char *pilot,
                                     const char *mission,
                                     int slotnum,
                                     int coop);
int android_save_set_build_secret_path(char *dst, size_t dst_size,
                                       int use_players_dir,
                                       const char *pilot,
                                       const char *mission,
                                       int slotnum);
int android_save_set_build_sidecar_path(char *dst, size_t dst_size,
                                        int use_players_dir,
                                        const char *scope,
                                        const char *pilot,
                                        const char *mission,
                                        const char *filename);

#endif
