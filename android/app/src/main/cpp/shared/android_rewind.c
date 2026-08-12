#include "android_rewind.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "android_jni_overlay.h"
#include "android_log.h"
#include "android_rewind_policy.h"
#include "android_save_meta.h"
#include "collide.h"
#include "fix.h"
#include "game.h"
#include "input_demo_recorder.h"
#include "input_demo_rng_trace.h"
#include "mission.h"
#include "multi.h"
#include "newdemo.h"
#include "player.h"
#include "pstypes.h"
#include "state.h"
#include "state_android_shared.h"

enum {
	ANDROID_REWIND_SNAPSHOT_LIMIT = 12,
	ANDROID_REWIND_INTERVAL = 5 * F1_0,
	ANDROID_REWIND_OVERLAY_TEXT_LEN = 64,
};

typedef struct android_rewind_snapshot {
	int level_num;
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
	int clients_can_request;
	int target_seconds;
	int has_level_identity;
	int level_num;
	char *mission;
	int64_t next_capture_game_time64;
	int has_restore_game_time64;
	int64_t restore_game_time64;
	int has_restore_collision_delay_last_play_time;
	int64_t restore_collision_delay_last_play_time;
	android_rewind_snapshot snapshots[ANDROID_REWIND_SNAPSHOT_LIMIT];
	int snapshot_count;
} android_rewind_session;

static android_rewind_session g_android_rewind_session = {
	1,
	0,
	ANDROID_REWIND_TARGET_SECONDS_DEFAULT,
	0,
	0,
	NULL,
	0,
	0,
	0,
	0,
	0,
	{ { 0 } },
	0,
};

static void android_rewind_record_overlay_text(const char *text)
{
	if (!text || !text[0])
		return;
	android_send_overlay_line(text);
}

static int android_rewind_has_duplicate_callsigns(void)
{
	int i;
	int j;

	for (i = 0; i < N_players; ++i) {
		if (!(Players[i].connected == CONNECT_PLAYING || Players[i].connected == CONNECT_WAITING))
			continue;
		for (j = i + 1; j < N_players; ++j) {
			if (!(Players[j].connected == CONNECT_PLAYING || Players[j].connected == CONNECT_WAITING))
				continue;
			if (!strcmp(Players[i].callsign, Players[j].callsign))
				return 1;
		}
	}
	return 0;
}

static int android_rewind_capture_context_allowed(void)
{
	const int is_multiplayer = (Game_mode & GM_MULTI) ? 1 : 0;
	const int is_coop = (Game_mode & GM_MULTI_COOP) ? 1 : 0;
	const int is_host = is_multiplayer ? multi_i_am_master() : 1;
	const int all_players_alive = is_multiplayer ? multi_all_players_alive() : 1;
	const int host_is_observer = (is_multiplayer && is_coop && Netgame.host_is_obs) ? 1 : 0;

	return android_rewind_is_capture_context_allowed(
	    is_multiplayer, is_coop, is_host, all_players_alive, host_is_observer);
}

static android_rewind_request_access android_rewind_request_context(void)
{
	const int is_multiplayer = (Game_mode & GM_MULTI) ? 1 : 0;
	const int is_coop = (Game_mode & GM_MULTI_COOP) ? 1 : 0;
	const int is_host = is_multiplayer ? multi_i_am_master() : 1;
	const int all_players_alive = is_multiplayer ? multi_all_players_alive() : 1;
	const int host_is_observer = (is_multiplayer && is_coop && Netgame.host_is_obs) ? 1 : 0;
	const int has_duplicate_callsigns = (is_multiplayer && is_coop) ? android_rewind_has_duplicate_callsigns() : 0;

	return android_rewind_classify_request_context(is_multiplayer,
	                                               is_coop,
	                                               is_host,
	                                               all_players_alive,
	                                               host_is_observer,
	                                               has_duplicate_callsigns);
}

static int android_rewind_current_level_matches_session(void)
{
	if (!g_android_rewind_session.has_level_identity)
		return 0;
	if (g_android_rewind_session.level_num != Current_level_num)
		return 0;
	if (!g_android_rewind_session.mission)
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
	free(g_android_rewind_session.mission);
	g_android_rewind_session.mission = NULL;
	g_android_rewind_session.next_capture_game_time64 = 0;
	g_android_rewind_session.snapshot_count = 0;
	android_rewind_clear_restore_overrides();
}

static int android_rewind_begin_level_history(void)
{
	size_t mission_bytes = strlen(Current_mission_filename) + 1;
	char *mission = malloc(mission_bytes);

	if (!mission) {
		debug_log(DLOG_GAME, "rewind disabled for level identity: mission name allocation failed");
		android_rewind_reset_history();
		return 0;
	}
	memcpy(mission, Current_mission_filename, mission_bytes);
	free(g_android_rewind_session.mission);
	g_android_rewind_session.mission = mission;
	g_android_rewind_session.has_level_identity = 1;
	g_android_rewind_session.level_num = Current_level_num;
	g_android_rewind_session.next_capture_game_time64 = 0;
	g_android_rewind_session.snapshot_count = 0;
	return 1;
}

static void android_rewind_snapshot_get_buffer(android_rewind_snapshot *snapshot,
                                               rewind_memory_buffer *buffer)
{
	if (!snapshot || !buffer)
		return;
	buffer->data = snapshot->data;
	buffer->size = snapshot->size;
	buffer->capacity = snapshot->capacity;
	buffer->error = 0;
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
		snapshot->recorder_frame_count =
		    android_rewind_demo_timeline_frame_count(input_demo_recorder_frame_count());
		snapshot->rng_event_count = input_demo_rng_trace_event_count();
		return;
	}
	snapshot->has_demo_timeline = 0;
	snapshot->recorder_frame_count = 0;
	snapshot->rng_event_count = 0;
}

static int android_rewind_capture_snapshot(android_rewind_snapshot *snapshot)
{
	rewind_memory_buffer buffer = { NULL, 0, 0, 0 };

	if (!snapshot)
		return 0;
	if (!state_save_to_memory(&buffer, "REWIND", ANDROID_SAVE_META_KIND_MANUAL, 1)) {
		rewind_memory_buffer_discard(&buffer);
		debug_log(DLOG_GAME, "rewind capture save failed: gt=%lld level=%d mission='%s'",
		          (long long) GameTime64, Current_level_num, Current_mission_filename);
		return 0;
	}
	{
		rewind_memory_buffer prior;

		android_rewind_snapshot_get_buffer(snapshot, &prior);
		rewind_memory_buffer_replace(&prior, &buffer);
		android_rewind_snapshot_set_buffer(snapshot, &prior);
	}
	snapshot->level_num = Current_level_num;
	snapshot->game_time64 = GameTime64;
	snapshot->has_collision_delay_last_play_time = 1;
	snapshot->collision_delay_last_play_time = collide_get_collision_delay_last_play_time();
	android_rewind_record_demo_timeline(snapshot);
	return 1;
}

static int android_rewind_select_snapshot_index(void)
{
	android_rewind_selection_snapshot snapshots[ANDROID_REWIND_SNAPSHOT_LIMIT];
	const int64_t min_age_game_time64 =
	    (int64_t) g_android_rewind_session.target_seconds * F1_0;
	const int require_demo_timeline =
	    (Newdemo_state == ND_STATE_RECORDING && input_demo_recorder_is_active()) ? 1 : 0;
	int i;

	for (i = 0; i < g_android_rewind_session.snapshot_count; ++i) {
		snapshots[i].game_time64 = g_android_rewind_session.snapshots[i].game_time64;
		snapshots[i].has_demo_timeline = g_android_rewind_session.snapshots[i].has_demo_timeline;
	}
	return android_rewind_select_snapshot_index_for_game_time(
	    snapshots,
	    g_android_rewind_session.snapshot_count,
	    GameTime64,
	    min_age_game_time64,
	    require_demo_timeline);
}

static void android_rewind_record_success_overlay(int rewound_seconds)
{
	char text[ANDROID_REWIND_OVERLAY_TEXT_LEN];

	snprintf(text, sizeof(text), "Rewinding %d seconds", rewound_seconds);
	android_rewind_record_overlay_text(text);
}

static void android_rewind_fill_authoritative_restore(
    android_rewind_authoritative_restore *restore,
    android_rewind_snapshot *snapshot,
    int snapshot_index,
    int rewound_seconds)
{
	if (!restore || !snapshot)
		return;
	memset(restore, 0, sizeof(*restore));
	android_rewind_snapshot_get_buffer(snapshot, &restore->buffer);
	restore->snapshot_index = snapshot_index;
	restore->rewound_seconds = rewound_seconds;
	restore->game_time64 = snapshot->game_time64;
	restore->has_collision_delay_last_play_time =
	    snapshot->has_collision_delay_last_play_time;
	restore->collision_delay_last_play_time =
	    snapshot->collision_delay_last_play_time;
}

static void android_rewind_apply_restore_overrides(
    const android_rewind_authoritative_restore *restore)
{
	if (!restore)
		return;
	g_android_rewind_session.has_restore_game_time64 = 1;
	g_android_rewind_session.restore_game_time64 = restore->game_time64;
	g_android_rewind_session.has_restore_collision_delay_last_play_time =
	    restore->has_collision_delay_last_play_time;
	g_android_rewind_session.restore_collision_delay_last_play_time =
	    restore->collision_delay_last_play_time;
}

void android_rewind_set_enabled(int enabled)
{
	g_android_rewind_session.enabled = enabled ? 1 : 0;
	if (!g_android_rewind_session.enabled)
		android_rewind_reset_history();
}

void android_rewind_set_target_seconds(int target_seconds)
{
	g_android_rewind_session.target_seconds = android_rewind_sanitize_target_seconds(target_seconds);
}

void android_rewind_set_clients_can_request(int enabled)
{
	g_android_rewind_session.clients_can_request = enabled ? 1 : 0;
}

int android_rewind_is_enabled(void)
{
	return g_android_rewind_session.enabled ? 1 : 0;
}

int android_rewind_clients_can_request(void)
{
	return g_android_rewind_session.clients_can_request ? 1 : 0;
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
	if (!android_rewind_capture_context_allowed() || Newdemo_state == ND_STATE_PLAYBACK || Current_level_num == 0) {
		android_rewind_reset_history();
		return;
	}
	if (!android_rewind_current_level_matches_session() && !android_rewind_begin_level_history())
		return;
	if (g_android_rewind_session.snapshot_count > 0 &&
	    GameTime64 < g_android_rewind_session.snapshots[g_android_rewind_session.snapshot_count - 1].game_time64 &&
	    !android_rewind_begin_level_history())
		return;
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
		captured = android_rewind_capture_snapshot(&rotated_snapshot);
		if (captured) {
			memmove(&g_android_rewind_session.snapshots[0], &g_android_rewind_session.snapshots[1],
			        sizeof(g_android_rewind_session.snapshots[0]) *
			            (ANDROID_REWIND_SNAPSHOT_LIMIT - 1));
			g_android_rewind_session.snapshots[ANDROID_REWIND_SNAPSHOT_LIMIT - 1] = rotated_snapshot;
		}
	}
	if (!captured)
		return;
	g_android_rewind_session.next_capture_game_time64 = GameTime64 + ANDROID_REWIND_INTERVAL;
}

int android_rewind_select_restore(android_rewind_authoritative_restore *restore)
{
	android_rewind_snapshot *snapshot;
	int snapshot_index;
	int seconds;
	android_rewind_request_access request_access;

	if (restore)
		memset(restore, 0, sizeof(*restore));
	if (!android_rewind_is_enabled())
		return ANDROID_REWIND_STATUS_DISABLED;
	request_access = android_rewind_request_context();
	if (request_access == ANDROID_REWIND_REQUEST_NOT_HOST) {
		android_rewind_record_overlay_text("Not host");
		return ANDROID_REWIND_STATUS_NOT_HOST;
	}
	if (request_access == ANDROID_REWIND_REQUEST_BLOCKED)
		return ANDROID_REWIND_STATUS_BLOCKED_MULTIPLAYER;
	if (!android_rewind_current_level_matches_session() || g_android_rewind_session.snapshot_count == 0)
		return ANDROID_REWIND_STATUS_NO_POINT;
	snapshot_index = android_rewind_select_snapshot_index();
	if (snapshot_index < 0)
		return ANDROID_REWIND_STATUS_NO_POINT;
	snapshot = &g_android_rewind_session.snapshots[snapshot_index];
	seconds = android_rewind_round_seconds_for_snapshot(GameTime64, snapshot->game_time64);
	android_rewind_fill_authoritative_restore(restore, snapshot,
	                                          snapshot_index, seconds);
	return ANDROID_REWIND_STATUS_RESTORED;
}

int android_rewind_restore_authoritative(const android_rewind_authoritative_restore *restore)
{
	int restore_ok;
	int64_t current_game_time64 = GameTime64;

	if (!restore || !restore->buffer.data || restore->buffer.size == 0)
		return ANDROID_REWIND_STATUS_FAILED;
	android_rewind_record_success_overlay(restore->rewound_seconds);
	android_rewind_apply_restore_overrides(restore);
	restore_ok = (Game_mode & GM_MULTI_COOP)
	                 ? state_restore_coop_from_memory(&restore->buffer)
	                 : state_restore_from_memory(&restore->buffer);
	android_rewind_finish_restore();
	if (!restore_ok) {
		debug_log(DLOG_GAME,
		          "rewind authoritative restore failed: gt=%lld target_gt=%lld bytes=%u",
		          (long long) current_game_time64,
		          (long long) restore->game_time64,
		          (unsigned int) restore->buffer.size);
		return ANDROID_REWIND_STATUS_FAILED;
	}
	if (restore->snapshot_index >= 0 &&
	    restore->snapshot_index < g_android_rewind_session.snapshot_count) {
		android_rewind_snapshot *snapshot =
		    &g_android_rewind_session.snapshots[restore->snapshot_index];
		if (Newdemo_state == ND_STATE_RECORDING &&
		    input_demo_recorder_is_active() && snapshot->has_demo_timeline) {
			if (!input_demo_recorder_truncate(snapshot->recorder_frame_count) ||
			    !input_demo_rng_trace_truncate(snapshot->rng_event_count)) {
				debug_log(DLOG_GAME, "rewind demo truncate failed: frames=%u rng=%u",
				          snapshot->recorder_frame_count,
				          (unsigned int) snapshot->rng_event_count);
				return ANDROID_REWIND_STATUS_FAILED;
			}
		}
		g_android_rewind_session.snapshot_count = restore->snapshot_index + 1;
		g_android_rewind_session.next_capture_game_time64 =
		    restore->game_time64 + ANDROID_REWIND_INTERVAL;
	} else {
		android_rewind_reset_history();
	}
	if (Game_mode & GM_MULTI_COOP)
		multi_send_score();
	debug_log(DLOG_GAME,
	          "rewind authoritative restored: seconds=%d gt=%lld target_gt=%lld index=%d count=%d",
	          restore->rewound_seconds, (long long) current_game_time64,
	          (long long) restore->game_time64, restore->snapshot_index,
	          g_android_rewind_session.snapshot_count);
	return ANDROID_REWIND_STATUS_RESTORED;
}

int android_rewind_request(int *rewound_seconds)
{
	android_rewind_authoritative_restore restore;
	int status = android_rewind_select_restore(&restore);

	if (rewound_seconds)
		*rewound_seconds = restore.rewound_seconds;
	if (status != ANDROID_REWIND_STATUS_RESTORED)
		return status;
	status = android_rewind_restore_authoritative(&restore);
	if (rewound_seconds)
		*rewound_seconds = restore.rewound_seconds;
	return status;
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
