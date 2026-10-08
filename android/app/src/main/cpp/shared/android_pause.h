#ifndef DXX_ANDROID_PAUSE_H
#define DXX_ANDROID_PAUSE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Keep values and snapshot array layout synchronized with PauseState.kt */
enum android_pause_reason {
	ANDROID_PAUSE_UI = 1,
	ANDROID_PAUSE_MENU = 2,
	ANDROID_PAUSE_USER = 4,
	ANDROID_PAUSE_GRAPHICS = 8,
	ANDROID_PAUSE_BACKGROUND = 16,
	ANDROID_PAUSE_COOP = 32,
	ANDROID_PAUSE_OPERATION = 64
};
enum android_pause_ui { ANDROID_PAUSE_TRAY = 1,
	                    ANDROID_PAUSE_MUSIC = 2,
	                    ANDROID_PAUSE_QUICK_LOAD_PROMPT = 4 };
enum android_pause_action { ANDROID_PAUSE_RESUME = 1,
	                        ANDROID_PAUSE_OPEN_MENU,
	                        ANDROID_PAUSE_OPEN_SAVE,
	                        ANDROID_PAUSE_OPEN_LOAD,
	                        ANDROID_PAUSE_QUICK_LOAD };
enum android_pause_result { ANDROID_PAUSE_IDLE = 0,
	                        ANDROID_PAUSE_APPLIED,
	                        ANDROID_PAUSE_REJECTED,
	                        ANDROID_PAUSE_STALE,
	                        ANDROID_PAUSE_EXECUTING };

struct android_pause_snapshot {
	uint64_t revision, session, ui_owner, ui_revision, request, repairs;
	int reasons, simulation_paused, input_allowed, can_resume, has_game, game_front, result, legacy_depth;
};

/* Thread-safe UI mailbox; no engine/window access on the calling thread */
uint64_t android_pause_attach_ui(void);
void android_pause_detach_ui(uint64_t owner);
int android_pause_publish_ui(uint64_t owner, uint64_t session, uint64_t revision, unsigned modals);
uint64_t android_pause_request(uint64_t owner, uint64_t session, uint64_t revision, unsigned modals, int action);
void android_pause_get_snapshot(struct android_pause_snapshot *out);

/* Engine-thread APIs, including nested menu loops */
void android_pause_tick(void);
void android_pause_publish(void);
void android_pause_new_session(void);
void android_pause_outer_tick(void);
int android_pause_reasons(void);
int android_pause_input_allowed(void);
int android_pause_simulation_paused(void);
int android_pause_queue_action(int action);
int android_pause_legacy_depth(void);
void android_pause_clear_legacy(void);
int android_pause_debug_overlay(void);

#ifdef __cplusplus
}
#endif
#endif
