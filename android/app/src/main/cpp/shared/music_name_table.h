#ifndef MUSIC_NAME_TABLE_H
#define MUSIC_NAME_TABLE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void music_name_table_clear_jukebox(void);
void music_name_table_clear_mission(void);
int music_name_table_load_jukebox(const char *json, size_t length);
int music_name_table_load_mission(const char *json, size_t length);
const char *music_name_table_lookup_jukebox(const char *path);
const char *music_name_table_lookup_mission(const char *path);

#ifdef __cplusplus
}
#endif

#endif
