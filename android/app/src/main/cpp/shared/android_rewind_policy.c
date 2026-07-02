#include "android_rewind_policy.h"

#include "fix.h"

int android_rewind_is_capture_context_allowed(int is_multiplayer,
                                              int is_coop,
                                              int is_host,
                                              int all_players_alive,
                                              int host_is_observer)
{
	if (!is_multiplayer)
		return 1;
	if (!is_coop)
		return 0;
	if (!is_host || !all_players_alive || host_is_observer)
		return 0;
	return 1;
}

android_rewind_request_access android_rewind_classify_request_context(
    int is_multiplayer,
    int is_coop,
    int is_host,
    int all_players_alive,
    int host_is_observer,
    int has_duplicate_callsigns)
{
	if (!is_multiplayer)
		return ANDROID_REWIND_REQUEST_ALLOW;
	if (!is_coop)
		return ANDROID_REWIND_REQUEST_BLOCKED;
	if (!is_host)
		return ANDROID_REWIND_REQUEST_NOT_HOST;
	if (!all_players_alive || host_is_observer || has_duplicate_callsigns)
		return ANDROID_REWIND_REQUEST_BLOCKED;
	return ANDROID_REWIND_REQUEST_ALLOW;
}

int android_rewind_is_client_request_allowed(int is_multiplayer,
                                             int is_coop,
                                             int is_host,
                                             int clients_can_request,
                                             int requester_valid)
{
	return is_multiplayer && is_coop && is_host && clients_can_request && requester_valid;
}

int android_rewind_sanitize_target_seconds(int target_seconds)
{
	switch (target_seconds) {
		case ANDROID_REWIND_TARGET_SECONDS_SHORT:
		case ANDROID_REWIND_TARGET_SECONDS_DEFAULT:
		case ANDROID_REWIND_TARGET_SECONDS_LONG:
			return target_seconds;
		default:
			return ANDROID_REWIND_TARGET_SECONDS_DEFAULT;
	}
}

int android_rewind_round_seconds_for_snapshot(int64_t current_game_time64,
                                              int64_t snapshot_game_time64)
{
	int seconds = (int) ((current_game_time64 - snapshot_game_time64 + (F1_0 / 2)) / F1_0);

	if (seconds < 1)
		seconds = 1;
	return seconds;
}

uint32_t android_rewind_demo_timeline_frame_count(uint32_t recorder_frame_count)
{
	return recorder_frame_count;
}

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
