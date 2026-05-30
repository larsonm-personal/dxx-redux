#ifndef ANDROID_REWIND_H
#define ANDROID_REWIND_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum android_rewind_status {
	ANDROID_REWIND_STATUS_RESTORED = 0,
	ANDROID_REWIND_STATUS_DISABLED = 1,
	ANDROID_REWIND_STATUS_NO_POINT = 2,
	ANDROID_REWIND_STATUS_FAILED = 3,
	ANDROID_REWIND_STATUS_NOT_HOST = 4,
	ANDROID_REWIND_STATUS_BLOCKED_MULTIPLAYER = 5,
} android_rewind_status;

void android_rewind_set_enabled(int enabled);
void android_rewind_set_target_seconds(int target_seconds);
void android_rewind_set_clients_can_request(int enabled);
int android_rewind_is_enabled(void);
int android_rewind_clients_can_request(void);
void android_rewind_reset_level(void);
void android_rewind_maybe_capture_frame(void);
int android_rewind_request(int *rewound_seconds);
int android_rewind_restore_game_time64_active(void);
int64_t android_rewind_restore_game_time64(void);
int android_rewind_get_restore_collision_delay_last_play_time(int64_t *last_play_time);
void android_rewind_finish_restore(void);

#ifdef __cplusplus
}
#endif

#endif /* ANDROID_REWIND_H */