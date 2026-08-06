#ifdef ANDROID

#include <inttypes.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

#include "android_lifecycle_diagnostics.h"
#include "android_log.h"

static int g_requested_visibility = ANDROID_LIFECYCLE_VISIBILITY_UNKNOWN;
static int g_observed_visibility = ANDROID_LIFECYCLE_VISIBILITY_UNKNOWN;
static int g_workload = ANDROID_LIFECYCLE_WORKLOAD_UNKNOWN;
static int g_suspend_state = ANDROID_LIFECYCLE_SUSPEND_RUNNING;
static int g_transition_reason = ANDROID_LIFECYCLE_REASON_INITIAL;
static uint64_t g_request_generation;
static uint64_t g_acknowledgement_generation;
static uint64_t g_window_generation;
static uint64_t g_context_generation;
static int g_checkpoint_state = ANDROID_LIFECYCLE_CHECKPOINT_NOT_NEEDED;
static int g_checkpoint_slot = -1;
static uint64_t g_checkpoint_generation;
static uint64_t g_park_entry_count;
static uint64_t g_wake_count;
static uint64_t g_parked_duration_ms;
static int g_last_wake_reason = ANDROID_LIFECYCLE_WAKE_NONE;
static int g_external_wake_requested;
static uint64_t g_counters[ANDROID_LIFECYCLE_COUNTER_COUNT];
static uint64_t g_central_overlay_polls;
static uint64_t g_independent_overlay_polls;
static uint64_t g_last_periodic_log_ms;
static pthread_mutex_t g_park_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_park_cond = PTHREAD_COND_INITIALIZER;

extern int androidaud_get_play_count(void);

#define ANDROID_DORMANCY_LOG_INTERVAL_MS 60000

static uint64_t atomic_load_u64(const uint64_t *value)
{
	return __atomic_load_n(value, __ATOMIC_RELAXED);
}

static int atomic_load_int(const int *value)
{
	return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

static uint64_t monotonic_ms(void)
{
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	return ((uint64_t) now.tv_sec * 1000u) + ((uint64_t) now.tv_nsec / 1000000u);
}

static void log_summary(const char *event, uint64_t generation,
                        int visibility, int reason)
{
	debug_log(DLOG_DORMANCY,
	          "dormancy_v=3 event=%s elapsed_ms=%" PRIu64 " request_gen=%" PRIu64 " ack_gen=%" PRIu64 " requested=%s observed=%s workload=%s suspend=%s reason=%s window_gen=%" PRIu64 " context_gen=%" PRIu64 " checkpoint=%s checkpoint_gen=%" PRIu64 " checkpoint_slot=%d park_entries=%" PRIu64 " wakes=%" PRIu64 " parked_ms=%" PRIu64 " wake_reason=%s event_loops=%" PRIu64 " draws=%" PRIu64 " swap_attempts=%" PRIu64 " swap_presented=%" PRIu64 " audio_callbacks=%d music_wakes=%" PRIu64 " redbook_wakes=%" PRIu64 " central_polls=%" PRIu64 " independent_polls=%" PRIu64,
	          event, monotonic_ms(), generation,
	          atomic_load_u64(&g_acknowledgement_generation),
	          android_lifecycle_diagnostics_visibility_name(visibility),
	          android_lifecycle_diagnostics_visibility_name(
	              atomic_load_int(&g_observed_visibility)),
	          android_lifecycle_diagnostics_workload_name(
	              atomic_load_int(&g_workload)),
	          android_lifecycle_diagnostics_suspend_name(
	              atomic_load_int(&g_suspend_state)),
	          android_lifecycle_diagnostics_reason_name(reason),
	          atomic_load_u64(&g_window_generation),
	          atomic_load_u64(&g_context_generation),
	          android_lifecycle_diagnostics_checkpoint_name(
	              atomic_load_int(&g_checkpoint_state)),
	          atomic_load_u64(&g_checkpoint_generation),
	          atomic_load_int(&g_checkpoint_slot),
	          atomic_load_u64(&g_park_entry_count),
	          atomic_load_u64(&g_wake_count),
	          atomic_load_u64(&g_parked_duration_ms),
	          android_lifecycle_diagnostics_wake_reason_name(
	              atomic_load_int(&g_last_wake_reason)),
	          atomic_load_u64(&g_counters[ANDROID_LIFECYCLE_COUNTER_EVENT_LOOP]),
	          atomic_load_u64(&g_counters[ANDROID_LIFECYCLE_COUNTER_DRAW_DISPATCH]),
	          atomic_load_u64(&g_counters[ANDROID_LIFECYCLE_COUNTER_SWAP_ATTEMPT]),
	          atomic_load_u64(&g_counters[ANDROID_LIFECYCLE_COUNTER_SWAP_PRESENTED]),
	          androidaud_get_play_count(),
	          atomic_load_u64(&g_counters[ANDROID_LIFECYCLE_COUNTER_MUSIC_PRODUCER_WAKE]),
	          atomic_load_u64(&g_counters[ANDROID_LIFECYCLE_COUNTER_REDBOOK_PRODUCER_WAKE]),
	          atomic_load_u64(&g_central_overlay_polls),
	          atomic_load_u64(&g_independent_overlay_polls));
}

static void maybe_log_periodic_summary(void)
{
	uint64_t last;
	uint64_t now;

	if (!debug_log_enabled[DLOG_DORMANCY])
		return;
	now = monotonic_ms();
	last = atomic_load_u64(&g_last_periodic_log_ms);
	if (last && now - last < ANDROID_DORMANCY_LOG_INTERVAL_MS)
		return;
	if (!__atomic_compare_exchange_n(&g_last_periodic_log_ms, &last, now,
	                                 0, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
		return;
	log_summary("periodic", atomic_load_u64(&g_request_generation),
	            atomic_load_int(&g_requested_visibility),
	            atomic_load_int(&g_transition_reason));
}

void android_lifecycle_diagnostics_request_visibility(int visibility, int reason)
{
	uint64_t generation;

	pthread_mutex_lock(&g_park_mutex);
	__atomic_store_n(&g_transition_reason, reason, __ATOMIC_RELEASE);
	__atomic_store_n(&g_requested_visibility, visibility, __ATOMIC_RELEASE);
	generation = __atomic_add_fetch(&g_request_generation, 1, __ATOMIC_ACQ_REL);
	pthread_cond_broadcast(&g_park_cond);
	pthread_mutex_unlock(&g_park_mutex);
	log_summary("request", generation, visibility, reason);
}

int android_lifecycle_diagnostics_requested_visibility(void)
{
	return atomic_load_int(&g_requested_visibility);
}

int android_lifecycle_diagnostics_take_pending(int *visibility, int *reason,
                                               uint64_t *generation)
{
	uint64_t requested = atomic_load_u64(&g_request_generation);

	if (requested == atomic_load_u64(&g_acknowledgement_generation))
		return 0;
	if (visibility)
		*visibility = atomic_load_int(&g_requested_visibility);
	if (reason)
		*reason = atomic_load_int(&g_transition_reason);
	if (generation)
		*generation = requested;
	return 1;
}

void android_lifecycle_diagnostics_acknowledge(uint64_t generation, int visibility)
{
	int reason = atomic_load_int(&g_transition_reason);

	__atomic_store_n(&g_observed_visibility, visibility, __ATOMIC_RELEASE);
	__atomic_store_n(&g_acknowledgement_generation, generation, __ATOMIC_RELEASE);
	log_summary("observed", generation, visibility, reason);
}

void android_lifecycle_diagnostics_checkpoint_request(uint64_t generation, int slot)
{
	__atomic_store_n(&g_checkpoint_generation, generation, __ATOMIC_RELEASE);
	__atomic_store_n(&g_checkpoint_slot, slot, __ATOMIC_RELEASE);
	__atomic_store_n(&g_checkpoint_state, ANDROID_LIFECYCLE_CHECKPOINT_REQUESTED,
	                 __ATOMIC_RELEASE);
	log_summary("checkpoint_requested", generation,
	            atomic_load_int(&g_requested_visibility),
	            atomic_load_int(&g_transition_reason));
}

int android_lifecycle_diagnostics_checkpoint_writing(void)
{
	if (atomic_load_int(&g_checkpoint_state) !=
	    ANDROID_LIFECYCLE_CHECKPOINT_REQUESTED)
		return 0;
	__atomic_store_n(&g_checkpoint_state, ANDROID_LIFECYCLE_CHECKPOINT_WRITING,
	                 __ATOMIC_RELEASE);
	log_summary("checkpoint_writing", atomic_load_u64(&g_checkpoint_generation),
	            atomic_load_int(&g_requested_visibility),
	            atomic_load_int(&g_transition_reason));
	return 1;
}

void android_lifecycle_diagnostics_checkpoint_finish(int state)
{
	if (state != ANDROID_LIFECYCLE_CHECKPOINT_COMMITTED &&
	    state != ANDROID_LIFECYCLE_CHECKPOINT_SKIPPED &&
	    state != ANDROID_LIFECYCLE_CHECKPOINT_FAILED)
		return;
	__atomic_store_n(&g_checkpoint_state, state, __ATOMIC_RELEASE);
	log_summary("checkpoint_finished", atomic_load_u64(&g_checkpoint_generation),
	            atomic_load_int(&g_requested_visibility),
	            atomic_load_int(&g_transition_reason));
}

void android_lifecycle_diagnostics_set_suspend_state(int state)
{
	if (state < ANDROID_LIFECYCLE_SUSPEND_RUNNING ||
	    state > ANDROID_LIFECYCLE_SUSPEND_WAKING)
		return;
	__atomic_store_n(&g_suspend_state, state, __ATOMIC_RELEASE);
}

int android_lifecycle_diagnostics_park_until_wake(uint64_t generation)
{
	uint64_t parked_at;
	uint64_t parked_ms;
	int wake_reason;

	pthread_mutex_lock(&g_park_mutex);
	android_lifecycle_diagnostics_set_suspend_state(
	    ANDROID_LIFECYCLE_SUSPEND_PARKED);
	__atomic_add_fetch(&g_park_entry_count, 1, __ATOMIC_RELAXED);
	parked_at = monotonic_ms();
	log_summary("parked", generation,
	            ANDROID_LIFECYCLE_VISIBILITY_BACKGROUND,
	            atomic_load_int(&g_transition_reason));
	android_lifecycle_diagnostics_acknowledge(
	    generation, ANDROID_LIFECYCLE_VISIBILITY_BACKGROUND);
	while (atomic_load_int(&g_requested_visibility) ==
	           ANDROID_LIFECYCLE_VISIBILITY_BACKGROUND &&
	       !g_external_wake_requested)
		pthread_cond_wait(&g_park_cond, &g_park_mutex);
	wake_reason = g_external_wake_requested
	                  ? ANDROID_LIFECYCLE_WAKE_EXTERNAL
	                  : ANDROID_LIFECYCLE_WAKE_FOREGROUND;
	g_external_wake_requested = 0;
	parked_ms = monotonic_ms() - parked_at;
	__atomic_add_fetch(&g_parked_duration_ms, parked_ms, __ATOMIC_RELAXED);
	__atomic_add_fetch(&g_wake_count, 1, __ATOMIC_RELAXED);
	__atomic_store_n(&g_last_wake_reason, wake_reason, __ATOMIC_RELEASE);
	android_lifecycle_diagnostics_set_suspend_state(
	    ANDROID_LIFECYCLE_SUSPEND_WAKING);
	pthread_mutex_unlock(&g_park_mutex);
	log_summary("waking", atomic_load_u64(&g_request_generation),
	            atomic_load_int(&g_requested_visibility),
	            atomic_load_int(&g_transition_reason));
	return wake_reason;
}

void android_lifecycle_diagnostics_request_external_wake(void)
{
	pthread_mutex_lock(&g_park_mutex);
	g_external_wake_requested = 1;
	pthread_cond_broadcast(&g_park_cond);
	pthread_mutex_unlock(&g_park_mutex);
}

void android_lifecycle_diagnostics_game_tick(int screen_is_menu, int screen_is_game,
                                             int has_game_window, int game_window_is_front, int multiplayer_active)
{
	android_lifecycle_diagnostics_count(ANDROID_LIFECYCLE_COUNTER_EVENT_LOOP);
	android_lifecycle_diagnostics_observe_game_state(
	    screen_is_menu, screen_is_game, has_game_window,
	    game_window_is_front, multiplayer_active);
	maybe_log_periodic_summary();
}

void android_lifecycle_diagnostics_observe_game_state(int screen_is_menu, int screen_is_game,
                                                      int has_game_window, int game_window_is_front,
                                                      int multiplayer_active)
{
	int workload;

	if (multiplayer_active && has_game_window)
		workload = ANDROID_LIFECYCLE_WORKLOAD_MULTIPLAYER_ACTIVE;
	else if (has_game_window)
		workload = game_window_is_front
		               ? ANDROID_LIFECYCLE_WORKLOAD_SINGLE_PLAYER_ACTIVE
		               : ANDROID_LIFECYCLE_WORKLOAD_SINGLE_PLAYER_PAUSED;
	else if (screen_is_menu)
		workload = ANDROID_LIFECYCLE_WORKLOAD_STATIC_UI;
	else if (screen_is_game)
		workload = ANDROID_LIFECYCLE_WORKLOAD_NO_LEVEL;
	else
		workload = ANDROID_LIFECYCLE_WORKLOAD_TIME_DRIVEN_SCREEN;
	__atomic_store_n(&g_workload, workload, __ATOMIC_RELEASE);
}

void android_lifecycle_diagnostics_count(int counter)
{
	if (counter >= 0 && counter < ANDROID_LIFECYCLE_COUNTER_COUNT)
		__atomic_add_fetch(&g_counters[counter], 1, __ATOMIC_RELAXED);
}

void android_lifecycle_diagnostics_set_window_generation(uint64_t generation)
{
	__atomic_store_n(&g_window_generation, generation, __ATOMIC_RELEASE);
}

void android_lifecycle_diagnostics_note_context_created(void)
{
	__atomic_add_fetch(&g_context_generation, 1, __ATOMIC_ACQ_REL);
}

void android_lifecycle_diagnostics_set_ui_poll_counters(uint64_t central, uint64_t independent)
{
	__atomic_store_n(&g_central_overlay_polls, central, __ATOMIC_RELEASE);
	__atomic_store_n(&g_independent_overlay_polls, independent, __ATOMIC_RELEASE);
}

void android_lifecycle_diagnostics_get_snapshot(struct android_lifecycle_diagnostic_snapshot *out)
{
	int i;

	if (!out)
		return;
	memset(out, 0, sizeof(*out));
	out->requested_visibility = atomic_load_int(&g_requested_visibility);
	out->observed_visibility = atomic_load_int(&g_observed_visibility);
	out->workload = atomic_load_int(&g_workload);
	out->suspend_state = atomic_load_int(&g_suspend_state);
	out->last_transition_reason = atomic_load_int(&g_transition_reason);
	out->request_generation = atomic_load_u64(&g_request_generation);
	out->acknowledgement_generation = atomic_load_u64(&g_acknowledgement_generation);
	out->window_generation = atomic_load_u64(&g_window_generation);
	out->context_generation = atomic_load_u64(&g_context_generation);
	out->checkpoint_state = atomic_load_int(&g_checkpoint_state);
	out->checkpoint_slot = atomic_load_int(&g_checkpoint_slot);
	out->checkpoint_generation = atomic_load_u64(&g_checkpoint_generation);
	out->park_entry_count = atomic_load_u64(&g_park_entry_count);
	out->wake_count = atomic_load_u64(&g_wake_count);
	out->parked_duration_ms = atomic_load_u64(&g_parked_duration_ms);
	out->last_wake_reason = atomic_load_int(&g_last_wake_reason);
	for (i = 0; i < ANDROID_LIFECYCLE_COUNTER_COUNT; ++i)
		out->counters[i] = atomic_load_u64(&g_counters[i]);
	out->central_overlay_polls = atomic_load_u64(&g_central_overlay_polls);
	out->independent_overlay_polls = atomic_load_u64(&g_independent_overlay_polls);
}

const char *android_lifecycle_diagnostics_visibility_name(int visibility)
{
	switch (visibility) {
		case ANDROID_LIFECYCLE_VISIBILITY_FOREGROUND: return "foreground";
		case ANDROID_LIFECYCLE_VISIBILITY_BACKGROUND: return "background";
		default: return "unknown";
	}
}

const char *android_lifecycle_diagnostics_workload_name(int workload)
{
	switch (workload) {
		case ANDROID_LIFECYCLE_WORKLOAD_NO_LEVEL: return "no_level";
		case ANDROID_LIFECYCLE_WORKLOAD_STATIC_UI: return "static_ui";
		case ANDROID_LIFECYCLE_WORKLOAD_SINGLE_PLAYER_ACTIVE: return "single_player_active";
		case ANDROID_LIFECYCLE_WORKLOAD_SINGLE_PLAYER_PAUSED: return "single_player_paused";
		case ANDROID_LIFECYCLE_WORKLOAD_TIME_DRIVEN_SCREEN: return "time_driven_screen";
		case ANDROID_LIFECYCLE_WORKLOAD_MULTIPLAYER_ACTIVE: return "multiplayer_active";
		default: return "unknown";
	}
}

const char *android_lifecycle_diagnostics_suspend_name(int suspend_state)
{
	switch (suspend_state) {
		case ANDROID_LIFECYCLE_SUSPEND_QUIESCING: return "quiescing";
		case ANDROID_LIFECYCLE_SUSPEND_PARKED: return "parked";
		case ANDROID_LIFECYCLE_SUSPEND_WAKING: return "waking";
		default: return "running";
	}
}

const char *android_lifecycle_diagnostics_wake_reason_name(int wake_reason)
{
	switch (wake_reason) {
		case ANDROID_LIFECYCLE_WAKE_FOREGROUND: return "foreground";
		case ANDROID_LIFECYCLE_WAKE_EXTERNAL: return "external";
		default: return "none";
	}
}

const char *android_lifecycle_diagnostics_checkpoint_name(int checkpoint_state)
{
	switch (checkpoint_state) {
		case ANDROID_LIFECYCLE_CHECKPOINT_REQUESTED: return "requested";
		case ANDROID_LIFECYCLE_CHECKPOINT_WRITING: return "writing";
		case ANDROID_LIFECYCLE_CHECKPOINT_COMMITTED: return "committed";
		case ANDROID_LIFECYCLE_CHECKPOINT_SKIPPED: return "skipped";
		case ANDROID_LIFECYCLE_CHECKPOINT_FAILED: return "failed";
		default: return "not_needed";
	}
}

const char *android_lifecycle_diagnostics_reason_name(int reason)
{
	switch (reason) {
		case ANDROID_LIFECYCLE_REASON_ACTIVITY_RESUME: return "activity_resume";
		case ANDROID_LIFECYCLE_REASON_ACTIVITY_STOP: return "activity_stop";
		case ANDROID_LIFECYCLE_REASON_MULTIPLAYER_TIMEOUT: return "multiplayer_timeout";
		default: return "initial";
	}
}

#endif
