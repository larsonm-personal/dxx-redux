#include "android_axis_mailbox.h"

#include <cassert>
#include <cstdio>

static int dispatch_count(const android_axis_mailbox_snapshot &snapshot)
{
	int count = 0;
	for (int axis = 0; axis < ANDROID_AXIS_MAILBOX_AXIS_COUNT; ++axis)
		count += snapshot.should_dispatch[axis] != 0;
	return count;
}

int main()
{
	android_axis_mailbox_snapshot snapshot = {};
	android_axis_mailbox_diagnostics diagnostics = {};

	android_axis_mailbox_reset_for_tests();
	assert(android_axis_mailbox_publish(-1, 50, 0) == 0);
	android_axis_mailbox_get_diagnostics(&diagnostics);
	assert(diagnostics.last_published_generation == 0);
	const android_axis_generation first = android_axis_mailbox_publish(2, 100, 0);
	const android_axis_generation second = android_axis_mailbox_publish(2, 200, 0);
	const android_axis_generation latest = android_axis_mailbox_publish(2, 300, 1);
	assert(first > 0 && second > first && latest > second);
	assert(android_axis_mailbox_take_snapshot(&snapshot));
	assert(!snapshot.automation_active);
	assert(snapshot.raw_value[2] == 300);
	assert(snapshot.touch_source[2]);
	assert(snapshot.axis_generation[2] == latest);
	assert(snapshot.should_dispatch[2]);
	android_axis_mailbox_mark_applied(&snapshot, dispatch_count(snapshot));

	android_axis_mailbox_get_diagnostics(&diagnostics);
	assert(diagnostics.publish_count == 3);
	assert(diagnostics.coalesced_count == 2);
	assert(diagnostics.last_applied_generation == latest);
	assert(diagnostics.pending_axis_count == 0);

	assert(android_axis_mailbox_take_snapshot(&snapshot));
	assert(snapshot.should_dispatch[2]);
	android_axis_mailbox_mark_applied(&snapshot, dispatch_count(snapshot));
	android_axis_mailbox_publish(2, 0, 0);
	assert(android_axis_mailbox_take_snapshot(&snapshot));
	assert(snapshot.should_dispatch[2]);
	android_axis_mailbox_mark_applied(&snapshot, dispatch_count(snapshot));
	assert(android_axis_mailbox_take_snapshot(&snapshot));
	assert(!snapshot.should_dispatch[2]);

	const android_axis_generation old_generation = android_axis_mailbox_publish(10, 10, 0);
	assert(android_axis_mailbox_take_snapshot(&snapshot));
	const android_axis_generation newer_generation = android_axis_mailbox_publish(10, 20, 0);
	assert(newer_generation > old_generation);
	android_axis_mailbox_mark_applied(&snapshot, dispatch_count(snapshot));
	android_axis_mailbox_get_diagnostics(&diagnostics);
	assert(diagnostics.pending_axis_count == 1);
	assert(android_axis_mailbox_take_snapshot(&snapshot));
	assert(snapshot.raw_value[10] == 20);
	assert(snapshot.axis_generation[10] == newer_generation);
	android_axis_mailbox_mark_applied(&snapshot, dispatch_count(snapshot));

	android_axis_mailbox_publish(0, 111, 0);
	android_axis_mailbox_publish(3, -222, 0);
	int automation_raw[ANDROID_AXIS_MAILBOX_AXIS_COUNT] = {};
	unsigned char automation_touch[ANDROID_AXIS_MAILBOX_AXIS_COUNT] = {};
	automation_raw[1] = 1000;
	automation_raw[6] = -2000;
	automation_touch[6] = 1;
	const android_axis_generation automation_generation =
		android_axis_mailbox_publish_automation(automation_raw, automation_touch,
		                                        6, automation_raw[6], 1);
	android_axis_mailbox_publish(1, 9999, 0);
	assert(android_axis_mailbox_take_snapshot(&snapshot));
	assert(snapshot.automation_active);
	assert(snapshot.batch_generation == automation_generation);
	assert(snapshot.target_axis == 6);
	assert(snapshot.target_raw_value == -2000);
	assert(snapshot.target_touch_source);
	assert(snapshot.raw_value[0] == 0);
	assert(snapshot.raw_value[1] == 1000);
	assert(snapshot.raw_value[6] == -2000);
	android_axis_mailbox_mark_applied(&snapshot, dispatch_count(snapshot));

	const android_axis_generation release_generation =
		android_axis_mailbox_release_automation();
	assert(release_generation > automation_generation);
	assert(android_axis_mailbox_take_snapshot(&snapshot));
	assert(!snapshot.automation_active);
	assert(snapshot.raw_value[0] == 111);
	assert(snapshot.raw_value[1] == 9999);
	assert(snapshot.raw_value[3] == -222);
	assert(snapshot.should_dispatch[0]);
	assert(snapshot.should_dispatch[1]);
	assert(snapshot.should_dispatch[3]);
	assert(snapshot.should_dispatch[6]);
	android_axis_mailbox_mark_applied(&snapshot, dispatch_count(snapshot));
	/* Discrete event flushes must preserve each continuous state transition. */
	android_axis_mailbox_publish(8, 44, 0);
	android_axis_mailbox_flush();
	assert(android_axis_mailbox_take_snapshot(&snapshot));
	assert(snapshot.raw_value[8] == 44);
	assert(snapshot.should_dispatch[8]);
	android_axis_mailbox_mark_applied(&snapshot, dispatch_count(snapshot));

	android_axis_mailbox_publish(8, 0, 0);
	android_axis_mailbox_flush();
	assert(android_axis_mailbox_take_snapshot(&snapshot));
	assert(snapshot.raw_value[8] == 0);
	assert(snapshot.should_dispatch[8]);
	android_axis_mailbox_mark_applied(&snapshot, dispatch_count(snapshot));
	assert(android_axis_mailbox_take_snapshot(&snapshot));
	assert(!snapshot.should_dispatch[8]);

	android_axis_mailbox_get_diagnostics(&diagnostics);
	assert(!diagnostics.automation_active);
	assert(diagnostics.pending_axis_count == 0);
	assert(diagnostics.dispatch_count >= 7);

	/* A press and release before one drain must retain both button regions. */
	android_axis_mailbox_reset_for_tests();
	android_axis_mailbox_set_button_deadzone(4, 38);
	android_axis_mailbox_publish(4, 1000, 0); /* below threshold */
	const android_axis_generation press_generation =
	    android_axis_mailbox_publish(4, 20000, 0);
	const android_axis_generation zero_generation =
	    android_axis_mailbox_publish(4, 0, 0);
	assert(android_axis_mailbox_take_snapshot(&snapshot));
	assert(snapshot.batch_generation == zero_generation);
	assert(snapshot.raw_value[4] == 0);
	assert(!snapshot.should_dispatch[4]);
	android_axis_mailbox_transition transition = {};
	assert(android_axis_mailbox_take_transition(snapshot.batch_generation,
	                                            &transition));
	assert(transition.generation == press_generation);
	assert(transition.axis == 4);
	assert(transition.raw_value == 20000);
	assert(android_axis_mailbox_take_transition(snapshot.batch_generation,
	                                            &transition));
	assert(transition.generation == zero_generation);
	assert(transition.axis == 4);
	assert(transition.raw_value == 0);
	assert(!android_axis_mailbox_take_transition(snapshot.batch_generation,
	                                             &transition));
	android_axis_mailbox_mark_applied(&snapshot, 2);
	puts("android axis mailbox tests passed");
	return 0;
}
