#ifndef ANDROID_IDLE_SAVER_H
#define ANDROID_IDLE_SAVER_H

/* State values are shared with IdleScreenSaver.kt */
enum android_idle_saver_state {
	ANDROID_IDLE_DISABLED = 0,
	ANDROID_IDLE_COUNTING = 1,
	ANDROID_IDLE_WARNING = 2,
	ANDROID_IDLE_SLEEP = 3,
	ANDROID_IDLE_MULTIPLAYER = 4,
	ANDROID_IDLE_SAVE_FAILED = 5
};

unsigned int android_idle_saver_checkpoints(void);
int android_idle_saver_state(void);
int android_idle_saver_hidden(void);
int android_idle_saver_music_suspended(void);
int android_idle_saver_audio_suspended(void);
void android_idle_saver_tick(int has_game_window, int multiplayer);
void android_idle_saver_foreground_audio(void);

#endif
