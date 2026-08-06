#ifndef ANDROID_LIFECYCLE_ACTIONS_H
#define ANDROID_LIFECYCLE_ACTIONS_H

#ifdef ANDROID

void android_lifecycle_actions_game_tick(int screen_is_game, int has_game_window,
                                         int game_window_is_front, int multiplayer_active);
void android_lifecycle_actions_request_wake(void);
void androidaud_background_pause(void);
void androidaud_background_resume(void);

#endif

#endif
