#ifndef ANDROID_LIFECYCLE_DIAGNOSTICS_H
#define ANDROID_LIFECYCLE_DIAGNOSTICS_H

#ifdef ANDROID

#include <stdint.h>

enum android_lifecycle_visibility {
	ANDROID_LIFECYCLE_VISIBILITY_UNKNOWN = 0,
	ANDROID_LIFECYCLE_VISIBILITY_FOREGROUND,
	ANDROID_LIFECYCLE_VISIBILITY_BACKGROUND
};

enum android_lifecycle_workload {
	ANDROID_LIFECYCLE_WORKLOAD_UNKNOWN = 0,
	ANDROID_LIFECYCLE_WORKLOAD_NO_LEVEL,
	ANDROID_LIFECYCLE_WORKLOAD_STATIC_UI,
	ANDROID_LIFECYCLE_WORKLOAD_SINGLE_PLAYER_ACTIVE,
	ANDROID_LIFECYCLE_WORKLOAD_SINGLE_PLAYER_PAUSED,
	ANDROID_LIFECYCLE_WORKLOAD_TIME_DRIVEN_SCREEN,
	ANDROID_LIFECYCLE_WORKLOAD_MULTIPLAYER_ACTIVE
};

enum android_lifecycle_suspend_state {
	ANDROID_LIFECYCLE_SUSPEND_RUNNING = 0,
	ANDROID_LIFECYCLE_SUSPEND_QUIESCING,
	ANDROID_LIFECYCLE_SUSPEND_PARKED,
	ANDROID_LIFECYCLE_SUSPEND_WAKING
};

enum android_lifecycle_wake_reason {
	ANDROID_LIFECYCLE_WAKE_NONE = 0,
	ANDROID_LIFECYCLE_WAKE_FOREGROUND,
	ANDROID_LIFECYCLE_WAKE_EXTERNAL
};

enum android_lifecycle_checkpoint_state {
	ANDROID_LIFECYCLE_CHECKPOINT_NOT_NEEDED = 0,
	ANDROID_LIFECYCLE_CHECKPOINT_REQUESTED,
	ANDROID_LIFECYCLE_CHECKPOINT_WRITING,
	ANDROID_LIFECYCLE_CHECKPOINT_COMMITTED,
	ANDROID_LIFECYCLE_CHECKPOINT_SKIPPED,
	ANDROID_LIFECYCLE_CHECKPOINT_FAILED
};

enum android_lifecycle_transition_reason {
	ANDROID_LIFECYCLE_REASON_INITIAL = 0,
	ANDROID_LIFECYCLE_REASON_ACTIVITY_RESUME,
	ANDROID_LIFECYCLE_REASON_ACTIVITY_STOP,
	ANDROID_LIFECYCLE_REASON_MULTIPLAYER_TIMEOUT
};

enum android_lifecycle_work_counter {
	ANDROID_LIFECYCLE_COUNTER_EVENT_LOOP = 0,
	ANDROID_LIFECYCLE_COUNTER_DRAW_DISPATCH,
	ANDROID_LIFECYCLE_COUNTER_SWAP_ATTEMPT,
	ANDROID_LIFECYCLE_COUNTER_SWAP_PRESENTED,
	ANDROID_LIFECYCLE_COUNTER_MUSIC_PRODUCER_WAKE,
	ANDROID_LIFECYCLE_COUNTER_REDBOOK_PRODUCER_WAKE,
	ANDROID_LIFECYCLE_COUNTER_COUNT
};

struct android_lifecycle_diagnostic_snapshot {
	int requested_visibility;
	int observed_visibility;
	int workload;
	int suspend_state;
	int last_transition_reason;
	uint64_t request_generation;
	uint64_t acknowledgement_generation;
	uint64_t window_generation;
	uint64_t context_generation;
	int checkpoint_state;
	int checkpoint_slot;
	uint64_t checkpoint_generation;
	uint64_t park_entry_count;
	uint64_t wake_count;
	uint64_t parked_duration_ms;
	int last_wake_reason;
	uint64_t counters[ANDROID_LIFECYCLE_COUNTER_COUNT];
	uint64_t central_overlay_polls;
	uint64_t independent_overlay_polls;
};

void android_lifecycle_diagnostics_request_visibility(int visibility, int reason);
int android_lifecycle_diagnostics_requested_visibility(void);
int android_lifecycle_diagnostics_take_pending(int *visibility, int *reason,
                                               uint64_t *generation);
void android_lifecycle_diagnostics_acknowledge(uint64_t generation, int visibility);
void android_lifecycle_diagnostics_checkpoint_request(uint64_t generation, int slot);
int android_lifecycle_diagnostics_checkpoint_writing(void);
void android_lifecycle_diagnostics_checkpoint_finish(int state);
void android_lifecycle_diagnostics_set_suspend_state(int state);
int android_lifecycle_diagnostics_park_until_wake(uint64_t generation);
void android_lifecycle_diagnostics_request_external_wake(void);
void android_lifecycle_diagnostics_game_tick(int screen_is_menu, int screen_is_game,
                                             int has_game_window, int game_window_is_front, int multiplayer_active);
void android_lifecycle_diagnostics_observe_game_state(int screen_is_menu, int screen_is_game,
                                                      int has_game_window, int game_window_is_front,
                                                      int multiplayer_active);
void android_lifecycle_diagnostics_count(int counter);
void android_lifecycle_diagnostics_set_window_generation(uint64_t generation);
void android_lifecycle_diagnostics_note_context_created(void);
void android_lifecycle_diagnostics_set_ui_poll_counters(uint64_t central, uint64_t independent);
void android_lifecycle_diagnostics_get_snapshot(struct android_lifecycle_diagnostic_snapshot *out);
const char *android_lifecycle_diagnostics_visibility_name(int visibility);
const char *android_lifecycle_diagnostics_workload_name(int workload);
const char *android_lifecycle_diagnostics_suspend_name(int suspend_state);
const char *android_lifecycle_diagnostics_wake_reason_name(int wake_reason);
const char *android_lifecycle_diagnostics_checkpoint_name(int checkpoint_state);
const char *android_lifecycle_diagnostics_reason_name(int reason);

#endif

#endif
