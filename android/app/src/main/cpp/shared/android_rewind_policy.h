#ifndef ANDROID_REWIND_POLICY_H
#define ANDROID_REWIND_POLICY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct android_rewind_selection_snapshot {
	int64_t game_time64;
	int has_demo_timeline;
} android_rewind_selection_snapshot;

typedef enum android_rewind_request_access {
	ANDROID_REWIND_REQUEST_ALLOW = 0,
	ANDROID_REWIND_REQUEST_NOT_HOST = 1,
	ANDROID_REWIND_REQUEST_BLOCKED = 2,
} android_rewind_request_access;

/* Keep in sync with the Kotlin rewind target radio options in EnginePreferencesPage.kt. */
enum {
	ANDROID_REWIND_TARGET_SECONDS_SHORT = 5,
	ANDROID_REWIND_TARGET_SECONDS_DEFAULT = 10,
	ANDROID_REWIND_TARGET_SECONDS_LONG = 20,
};

int android_rewind_sanitize_target_seconds(int target_seconds);

int android_rewind_round_seconds_for_snapshot(int64_t current_game_time64,
                                              int64_t snapshot_game_time64);

int android_rewind_is_capture_context_allowed(int is_multiplayer,
                                              int is_coop,
                                              int is_host,
                                              int all_players_alive,
                                              int host_is_observer);

android_rewind_request_access android_rewind_classify_request_context(
    int is_multiplayer,
    int is_coop,
    int is_host,
    int all_players_alive,
    int host_is_observer,
    int has_duplicate_callsigns);

int android_rewind_is_client_request_allowed(int is_multiplayer,
                                             int is_coop,
                                             int is_host,
                                             int clients_can_request,
                                             int requester_valid);

/* Rewind snapshots are captured at frame boundaries before time and input-demo
 * recording advance, so the kept demo timeline is the current recorder count. */
uint32_t android_rewind_demo_timeline_frame_count(uint32_t recorder_frame_count);

/* Pure target-selection helper shared by the runtime rewind manager and host tests. */
int android_rewind_select_snapshot_index_for_game_time(
    const android_rewind_selection_snapshot *snapshots,
    int snapshot_count,
    int64_t current_game_time64,
    int64_t min_age_game_time64,
    int require_demo_timeline);

#ifdef __cplusplus
}
#endif

#endif /* ANDROID_REWIND_POLICY_H */