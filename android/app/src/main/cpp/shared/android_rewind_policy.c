#include "android_rewind_policy.h"

int android_rewind_select_snapshot_index_for_game_time(
    const android_rewind_selection_snapshot *snapshots,
    int snapshot_count,
    int64_t current_game_time64,
    int64_t min_age_game_time64,
    int require_demo_timeline)
{
	int viable_count = 0;
	int last_viable = -1;
	int i;
	int64_t target_time = current_game_time64 - min_age_game_time64;

	if (!snapshots || snapshot_count <= 0)
		return -1;
	for (i = 0; i < snapshot_count; ++i) {
		if (require_demo_timeline && !snapshots[i].has_demo_timeline)
			continue;
		viable_count++;
		last_viable = i;
	}
	if (viable_count == 0)
		return -1;
	for (i = snapshot_count - 1; i >= 0; --i) {
		if (require_demo_timeline && !snapshots[i].has_demo_timeline)
			continue;
		if (snapshots[i].game_time64 <= target_time)
			return i;
	}
	if (viable_count == 1 && last_viable >= 0 && snapshots[last_viable].game_time64 < current_game_time64)
		return last_viable;
	return -1;
}