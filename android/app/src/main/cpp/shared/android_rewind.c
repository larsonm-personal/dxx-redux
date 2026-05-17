#include "android_rewind.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "android_jni_overlay.h"
#include "android_log.h"
#include "android_save_meta.h"
#include "collide.h"
#include "fix.h"
#include "game.h"
#include "input_demo_recorder.h"
#include "input_demo_rng_trace.h"
#include "mission.h"
#include "newdemo.h"
#include "player.h"
#include "pstypes.h"
#include "state.h"

enum {
	ANDROID_REWIND_SNAPSHOT_LIMIT = 12,
	ANDROID_REWIND_INTERVAL = 5 * F1_0,
	ANDROID_REWIND_MIN_AGE = 3 * F1_0,
	ANDROID_REWIND_OVERLAY_TEXT_LEN = 64,
	ANDROID_REWIND_MISSION_LEN = 32,
};

typedef struct android_rewind_snapshot {
	int level_num;
	char mission[ANDROID_REWIND_MISSION_LEN];
	int64_t game_time64;
	int has_collision_delay_last_play_time;
	int64_t collision_delay_last_play_time;
	int has_demo_timeline;
	uint32_t recorder_frame_count;
	size_t rng_event_count;
	unsigned char *data;
	size_t size;
	size_t capacity;
} android_rewind_snapshot;

typedef struct android_rewind_session {
	int enabled;
	int has_level_identity;
	int level_num;
	char mission[ANDROID_REWIND_MISSION_LEN];
	int64_t next_capture_game_time64;
	int has_restore_game_time64;
	int64_t restore_game_time64;
	int has_restore_collision_delay_last_play_time;
	int64_t restore_collision_delay_last_play_time;
	android_rewind_snapshot snapshots[ANDROID_REWIND_SNAPSHOT_LIMIT];
	int snapshot_count;
} android_rewind_session;

static android_rewind_session g_android_rewind_session = {1, 0, 0, "", 0, 0, 0, 0, 0, {{0}}, 0};

static void android_rewind_copy_mission(char *dst, size_t dst_size)
{
	if (!dst || !dst_size)
		return;
	snprintf(dst, dst_size, "%s", Current_mission_filename);
}

static int android_rewind_current_level_matches_session(void)
{
	if (!g_android_rewind_session.has_level_identity)
		return 0;
	if (g_android_rewind_session.level_num != Current_level_num)
		return 0;
	return strcmp(g_android_rewind_session.mission, Current_mission_filename) == 0;
}

static void android_rewind_clear_restore_overrides(void)
{
	g_android_rewind_session.has_restore_game_time64 = 0;
	g_android_rewind_session.restore_game_time64 = 0;
	g_android_rewind_session.has_restore_collision_delay_last_play_time = 0;
	g_android_rewind_session.restore_collision_delay_last_play_time = 0;
}

static void android_rewind_reset_history(void)
{
	g_android_rewind_session.has_level_identity = 0;
	g_android_rewind_session.level_num = 0;
	g_android_rewind_session.mission[0] = '\0';
	g_android_rewind_session.next_capture_game_time64 = 0;
	g_android_rewind_session.snapshot_count = 0;
	android_rewind_clear_restore_overrides();
}

static void android_rewind_begin_level_history(void)
{
	g_android_rewind_session.has_level_identity = 1;
	g_android_rewind_session.level_num = Current_level_num;
	android_rewind_copy_mission(g_android_rewind_session.mission,
	                           sizeof(g_android_rewind_session.mission));
	g_android_rewind_session.next_capture_game_time64 = 0;
	g_android_rewind_session.snapshot_count = 0;
}

static void android_rewind_snapshot_get_buffer(android_rewind_snapshot *snapshot,
	                                           rewind_memory_buffer *buffer)
{
	if (!snapshot || !buffer)
		return;
	buffer->data = snapshot->data;
	buffer->size = snapshot->size;
	buffer->capacity = snapshot->capacity;
}

static void android_rewind_snapshot_set_buffer(android_rewind_snapshot *snapshot,
	                                           const rewind_memory_buffer *buffer)
{
	if (!snapshot || !buffer)
		return;
	snapshot->data = buffer->data;
	snapshot->size = buffer->size;
	snapshot->capacity = buffer->capacity;
}

static void android_rewind_record_demo_timeline(android_rewind_snapshot *snapshot)
{
	if (!snapshot)
		return;
	if (Newdemo_state == ND_STATE_RECORDING && input_demo_recorder_is_active()) {
		snapshot->has_demo_timeline = 1;
		snapshot->recorder_frame_count = input_demo_recorder_frame_count();
		snapshot->rng_event_count = input_demo_rng_trace_event_count();
		return;
	}
	snapshot->has_demo_timeline = 0;
	snapshot->recorder_frame_count = 0;
	snapshot->rng_event_count = 0;
}

static int android_rewind_capture_snapshot(android_rewind_snapshot *snapshot)
{
	rewind_memory_buffer buffer = {NULL, 0, 0};

	if (!snapshot)
		return 0;
	android_rewind_snapshot_get_buffer(snapshot, &buffer);
	if (!state_android_save_to_memory(&buffer, "REWIND", ANDROID_SAVE_META_KIND_MANUAL, 1)) {
		debug_log(DLOG_GAME, "rewind capture save failed: gt=%lld level=%d mission='%s'",
			(long long) GameTime64, Current_level_num, Current_mission_filename);
		return 0;
	}
	android_rewind_snapshot_set_buffer(snapshot, &buffer);
	snapshot->level_num = Current_level_num;
	android_rewind_copy_mission(snapshot->mission, sizeof(snapshot->mission));
	snapshot->game_time64 = GameTime64;
	snapshot->has_collision_delay_last_play_time = 1;
	snapshot->collision_delay_last_play_time = collide_get_collision_delay_last_play_time();
	android_rewind_record_demo_timeline(snapshot);
	return 1;
}

static int android_rewind_select_snapshot_index(void)
{
	const int require_demo_timeline =
		(Newdemo_state == ND_STATE_RECORDING && input_demo_recorder_is_active()) ? 1 : 0;
	int viable_count = 0;
	int last_viable = -1;
	int i;
	int64_t target_time = GameTime64 - ANDROID_REWIND_MIN_AGE;

	for (i = 0; i < g_android_rewind_session.snapshot_count; ++i) {
		android_rewind_snapshot *snapshot = &g_android_rewind_session.snapshots[i];

		if (require_demo_timeline && !snapshot->has_demo_timeline)
			continue;
		viable_count++;
		last_viable = i;
	}
	if (viable_count == 0)
		return -1;
	for (i = g_android_rewind_session.snapshot_count - 1; i >= 0; --i) {
		android_rewind_snapshot *snapshot = &g_android_rewind_session.snapshots[i];

		if (require_demo_timeline && !snapshot->has_demo_timeline)
			continue;
		if (snapshot->game_time64 <= target_time)
			return i;
	}
	if (viable_count == 1 && last_viable >= 0 &&
		g_android_rewind_session.snapshots[last_viable].game_time64 < GameTime64)
		return last_viable;
	return -1;
}

static void android_rewind_prepare_restore_overrides(const android_rewind_snapshot *snapshot)
{
	if (!snapshot)
		return;
	g_android_rewind_session.has_restore_game_time64 = 1;
	g_android_rewind_session.restore_game_time64 = snapshot->game_time64;
	g_android_rewind_session.has_restore_collision_delay_last_play_time =
		snapshot->has_collision_delay_last_play_time;
	g_android_rewind_session.restore_collision_delay_last_play_time =
		snapshot->collision_delay_last_play_time;
}

static void android_rewind_record_success_overlay(int rewound_seconds)
{
	char text[ANDROID_REWIND_OVERLAY_TEXT_LEN];

	snprintf(text, sizeof(text), "Rewound %d seconds", rewound_seconds);
	android_send_overlay_line(text);
}

void android_rewind_set_enabled(int enabled)
{
	g_android_rewind_session.enabled = enabled ? 1 : 0;
	if (!g_android_rewind_session.enabled)
		android_rewind_reset_history();
}

int android_rewind_is_enabled(void)
{
	return g_android_rewind_session.enabled ? 1 : 0;
}

void android_rewind_reset_level(void)
{
	android_rewind_reset_history();
}

void android_rewind_maybe_capture_frame(void)
{
	android_rewind_snapshot rotated_snapshot;
	int captured = 0;

	if (!android_rewind_is_enabled())
		return;
	if ((Game_mode & GM_MULTI) || Newdemo_state == ND_STATE_PLAYBACK || Current_level_num == 0) {
		android_rewind_reset_history();
		return;
	}
	if (!android_rewind_current_level_matches_session())
		android_rewind_begin_level_history();
	if (g_android_rewind_session.snapshot_count > 0 &&
		GameTime64 < g_android_rewind_session.snapshots[g_android_rewind_session.snapshot_count - 1].game_time64)
		android_rewind_begin_level_history();
	if (g_android_rewind_session.snapshot_count > 0 &&
		GameTime64 < g_android_rewind_session.next_capture_game_time64)
		return;
	if (g_android_rewind_session.snapshot_count > 0 &&
		g_android_rewind_session.snapshots[g_android_rewind_session.snapshot_count - 1].game_time64 == GameTime64)
		return;
	if (g_android_rewind_session.snapshot_count < ANDROID_REWIND_SNAPSHOT_LIMIT) {
		captured = android_rewind_capture_snapshot(
			&g_android_rewind_session.snapshots[g_android_rewind_session.snapshot_count]);
		if (captured)
			g_android_rewind_session.snapshot_count++;
	} else {
		rotated_snapshot = g_android_rewind_session.snapshots[0];
		memmove(&g_android_rewind_session.snapshots[0], &g_android_rewind_session.snapshots[1],
			sizeof(g_android_rewind_session.snapshots[0]) * (ANDROID_REWIND_SNAPSHOT_LIMIT - 1));
		g_android_rewind_session.snapshots[ANDROID_REWIND_SNAPSHOT_LIMIT - 1] = rotated_snapshot;
		captured = android_rewind_capture_snapshot(
			&g_android_rewind_session.snapshots[ANDROID_REWIND_SNAPSHOT_LIMIT - 1]);
		if (!captured)
			g_android_rewind_session.snapshot_count = ANDROID_REWIND_SNAPSHOT_LIMIT - 1;
	}
	if (!captured)
		return;
	g_android_rewind_session.next_capture_game_time64 = GameTime64 + ANDROID_REWIND_INTERVAL;
}

int android_rewind_request(int *rewound_seconds)
{
	android_rewind_snapshot *snapshot;
	rewind_memory_buffer buffer = {NULL, 0, 0};
	int snapshot_index;
	int restore_ok;
	int seconds;
	int64_t current_game_time64;

	if (rewound_seconds)
		*rewound_seconds = 0;
	if (!android_rewind_is_enabled())
		return ANDROID_REWIND_STATUS_DISABLED;
	if (!android_rewind_current_level_matches_session() || g_android_rewind_session.snapshot_count == 0)
		return ANDROID_REWIND_STATUS_NO_POINT;
	snapshot_index = android_rewind_select_snapshot_index();
	if (snapshot_index < 0)
		return ANDROID_REWIND_STATUS_NO_POINT;
	snapshot = &g_android_rewind_session.snapshots[snapshot_index];
	android_rewind_snapshot_get_buffer(snapshot, &buffer);
	current_game_time64 = GameTime64;
	android_rewind_prepare_restore_overrides(snapshot);
	restore_ok = state_android_restore_from_memory(&buffer);
	android_rewind_finish_restore();
	if (!restore_ok) {
		debug_log(DLOG_GAME, "rewind restore failed: index=%d gt=%lld target_gt=%lld bytes=%u",
			snapshot_index, (long long) current_game_time64, (long long) snapshot->game_time64,
			(unsigned int) snapshot->size);
		return ANDROID_REWIND_STATUS_FAILED;
	}
	if (Newdemo_state == ND_STATE_RECORDING && input_demo_recorder_is_active() && snapshot->has_demo_timeline) {
		if (!input_demo_recorder_truncate(snapshot->recorder_frame_count) ||
			!input_demo_rng_trace_truncate(snapshot->rng_event_count)) {
			debug_log(DLOG_GAME, "rewind demo truncate failed: frames=%u rng=%u",
				snapshot->recorder_frame_count, (unsigned int) snapshot->rng_event_count);
			return ANDROID_REWIND_STATUS_FAILED;
		}
	}
	g_android_rewind_session.snapshot_count = snapshot_index + 1;
	g_android_rewind_session.next_capture_game_time64 = snapshot->game_time64 + ANDROID_REWIND_INTERVAL;
	seconds = (int) ((current_game_time64 - snapshot->game_time64 + (F1_0 / 2)) / F1_0);
	if (seconds < 1)
		seconds = 1;
	if (rewound_seconds)
		*rewound_seconds = seconds;
	android_rewind_record_success_overlay(seconds);
	debug_log(DLOG_GAME, "rewind restored: seconds=%d gt=%lld target_gt=%lld slot=%d count=%d",
		seconds, (long long) current_game_time64, (long long) snapshot->game_time64,
		snapshot_index, g_android_rewind_session.snapshot_count);
	return ANDROID_REWIND_STATUS_RESTORED;
}

int android_rewind_restore_game_time64_active(void)
{
	return g_android_rewind_session.has_restore_game_time64 ? 1 : 0;
}

int64_t android_rewind_restore_game_time64(void)
{
	return g_android_rewind_session.restore_game_time64;
}

int android_rewind_get_restore_collision_delay_last_play_time(int64_t *last_play_time)
{
	if (!last_play_time || !g_android_rewind_session.has_restore_collision_delay_last_play_time)
		return 0;
	*last_play_time = g_android_rewind_session.restore_collision_delay_last_play_time;
	return 1;
}

void android_rewind_finish_restore(void)
{
	android_rewind_clear_restore_overrides();
}