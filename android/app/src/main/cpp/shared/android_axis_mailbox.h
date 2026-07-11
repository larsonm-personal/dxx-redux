#ifndef ANDROID_AXIS_MAILBOX_H
#define ANDROID_AXIS_MAILBOX_H

#define ANDROID_AXIS_MAILBOX_AXIS_COUNT        11
#define ANDROID_AXIS_MAILBOX_AXIS_BUTTON_COUNT 8

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long long android_axis_generation;

typedef struct android_axis_mailbox_snapshot {
	int raw_value[ANDROID_AXIS_MAILBOX_AXIS_COUNT];
	unsigned char touch_source[ANDROID_AXIS_MAILBOX_AXIS_COUNT];
	unsigned char should_dispatch[ANDROID_AXIS_MAILBOX_AXIS_COUNT];
	android_axis_generation axis_generation[ANDROID_AXIS_MAILBOX_AXIS_COUNT];
	android_axis_generation batch_generation;
	int automation_active;
	int target_axis;
	int target_raw_value;
	int target_touch_source;
} android_axis_mailbox_snapshot;

typedef struct android_axis_mailbox_diagnostics {
	android_axis_generation last_published_generation;
	android_axis_generation last_applied_generation;
	unsigned long long publish_count;
	unsigned long long coalesced_count;
	unsigned long long drain_count;
	unsigned long long dispatch_count;
	int automation_active;
	int pending_axis_count;
} android_axis_mailbox_diagnostics;

typedef struct android_axis_mailbox_transition {
	android_axis_generation generation;
	int axis;
	int raw_value;
	int touch_source;
} android_axis_mailbox_transition;

/* Publish the latest production value for one mixed Android axis */
android_axis_generation android_axis_mailbox_publish(int axis, int raw_value,
                                                     int touch_source);

/* Replace device input with one complete automation-owned axis vector */
android_axis_generation android_axis_mailbox_publish_automation(
    const int raw_values[ANDROID_AXIS_MAILBOX_AXIS_COUNT],
    const unsigned char touch_sources[ANDROID_AXIS_MAILBOX_AXIS_COUNT],
    int target_axis, int target_raw_value, int target_touch_source);

/* Return control to the latest production vector */
android_axis_generation android_axis_mailbox_release_automation(void);

/* Keep threshold-region transitions needed by axis-to-button bindings */
void android_axis_mailbox_set_button_deadzone(int axis, int deadzone);

/* Copy one coherent desired vector for game-thread delivery */
int android_axis_mailbox_take_snapshot(android_axis_mailbox_snapshot *out_snapshot);

/* Remove the next threshold transition at or before the snapshot generation */
int android_axis_mailbox_take_transition(
    android_axis_generation through_generation,
    android_axis_mailbox_transition *out_transition);

/* Acknowledge the exact snapshot delivered by the game thread */
void android_axis_mailbox_mark_applied(const android_axis_mailbox_snapshot *snapshot,
                                       int dispatch_count);

void android_axis_mailbox_get_diagnostics(android_axis_mailbox_diagnostics *out_diagnostics);

/* Android game-thread consumer implemented beside the native joy handlers */
int android_axis_mailbox_drain(int joystick_enabled);

/* Observe a discrete event-flush boundary without discarding continuous state */
void android_axis_mailbox_flush(void);

/* Test-only state reset used by the standalone native contract test */
void android_axis_mailbox_reset_for_tests(void);

#ifdef __cplusplus
}
#endif

#endif
