/* Shared Android music controls for D1 and D2. */

#ifndef SONGS_ANDROID_SHARED_H
#define SONGS_ANDROID_SHARED_H

#include <stddef.h>

#define SONGS_TRACK_LIST_MAX_BYTES (4u * 1024u * 1024u)

int songs_next_track(void);
int songs_prev_track(void);
int songs_play_specific_track(int track);
int songs_get_track_info(int *out_type, int *out_track, int *out_total,
                         char *out_name, int name_size);
int songs_get_track_list(char *buf, size_t buf_size);
int android_music_redbook_track_for_ordinal(int first, int last, int ordinal);
int android_music_redbook_track_offset(int first, int last, int current, int offset);
void android_music_set_prefer_mission_soundtrack(int enabled);
int android_music_get_prefer_mission_soundtrack(void);

#endif /* SONGS_ANDROID_SHARED_H */
