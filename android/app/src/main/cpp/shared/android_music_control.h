/*
 * android_music_control.h -- game-thread bridge for Android overlay music actions.
 */

#ifndef ANDROID_MUSIC_CONTROL_H
#define ANDROID_MUSIC_CONTROL_H

int android_music_control_apply_pending(void);
int android_music_set_source(const char *source);
void android_music_replay_current(void);

#endif /* ANDROID_MUSIC_CONTROL_H */
