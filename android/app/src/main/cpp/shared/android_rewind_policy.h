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