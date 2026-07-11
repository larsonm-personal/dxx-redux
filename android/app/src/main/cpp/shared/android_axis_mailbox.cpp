#include "android_axis_mailbox.h"

#include <algorithm>
#include <cstring>
#include <deque>
#include <mutex>

namespace
{

struct axis_mailbox_state {
	std::mutex lock;
	android_axis_generation next_generation = 0;
	int production_raw[ANDROID_AXIS_MAILBOX_AXIS_COUNT] = {};
	unsigned char production_touch[ANDROID_AXIS_MAILBOX_AXIS_COUNT] = {};
	android_axis_generation production_generation[ANDROID_AXIS_MAILBOX_AXIS_COUNT] = {};
	android_axis_generation production_consumed[ANDROID_AXIS_MAILBOX_AXIS_COUNT] = {};
	int automation_raw[ANDROID_AXIS_MAILBOX_AXIS_COUNT] = {};
	unsigned char automation_touch[ANDROID_AXIS_MAILBOX_AXIS_COUNT] = {};
	android_axis_generation automation_generation = 0;
	android_axis_generation automation_consumed = 0;
	int automation_active = 0;
	int automation_target_axis = -1;
	int automation_target_raw = 0;
	int automation_target_touch = 0;
	int applied_raw[ANDROID_AXIS_MAILBOX_AXIS_COUNT] = {};
	unsigned char applied_touch[ANDROID_AXIS_MAILBOX_AXIS_COUNT] = {};
	android_axis_generation last_published_generation = 0;
	android_axis_generation last_applied_generation = 0;
	unsigned long long publish_count = 0;
	unsigned long long coalesced_count = 0;
	unsigned long long drain_count = 0;
	unsigned long long dispatch_count = 0;
	int button_deadzone[ANDROID_AXIS_MAILBOX_AXIS_BUTTON_COUNT] = {};
	int effective_button_region[ANDROID_AXIS_MAILBOX_AXIS_BUTTON_COUNT] = {};
	std::deque<android_axis_mailbox_transition> button_transitions;

	axis_mailbox_state()
	{
		std::fill(button_deadzone,
		          button_deadzone + ANDROID_AXIS_MAILBOX_AXIS_BUTTON_COUNT, 38);
	}
};

axis_mailbox_state g_mailbox;

int clamp_raw_value(int raw_value)
{
	return std::max(-32768, std::min(32767, raw_value));
}

android_axis_generation next_generation_locked()
{
	if (++g_mailbox.next_generation == 0)
		++g_mailbox.next_generation;
	g_mailbox.last_published_generation = g_mailbox.next_generation;
	return g_mailbox.next_generation;
}

int button_region_locked(int axis, int raw_value)
{
	const int scaled_value = raw_value / 256;
	if (scaled_value > g_mailbox.button_deadzone[axis])
		return 1;
	if (scaled_value < -g_mailbox.button_deadzone[axis])
		return -1;
	return 0;
}

void record_effective_transition_locked(int axis, int raw_value,
                                        int touch_source,
                                        android_axis_generation generation)
{
	if (axis < 0 || axis >= ANDROID_AXIS_MAILBOX_AXIS_BUTTON_COUNT)
		return;
	const int new_region = button_region_locked(axis, raw_value);
	if (new_region == g_mailbox.effective_button_region[axis])
		return;

	android_axis_mailbox_transition transition = {};
	transition.generation = generation;
	transition.axis = axis;
	transition.raw_value = raw_value;
	transition.touch_source = touch_source != 0;
	g_mailbox.button_transitions.push_back(transition);
	g_mailbox.effective_button_region[axis] = new_region;
}

} // namespace

extern "C" android_axis_generation android_axis_mailbox_publish(int axis, int raw_value,
                                                                int touch_source)
{
	const unsigned char touch = touch_source != 0;
	return android_axis_mailbox_publish_batch(&axis, &raw_value, &touch, 1);
}

extern "C" android_axis_generation android_axis_mailbox_publish_batch(
    const int *axes, const int *raw_values,
    const unsigned char *touch_sources, int count)
{
	if (!axes || !raw_values || !touch_sources || count <= 0)
		return 0;
	int valid_count = 0;
	for (int index = 0; index < count; ++index)
		valid_count += axes[index] >= 0 && axes[index] < ANDROID_AXIS_MAILBOX_AXIS_COUNT;
	if (!valid_count)
		return 0;

	std::lock_guard<std::mutex> guard(g_mailbox.lock);
	const android_axis_generation generation = next_generation_locked();
	for (int index = 0; index < count; ++index) {
		const int axis = axes[index];
		if (axis < 0 || axis >= ANDROID_AXIS_MAILBOX_AXIS_COUNT)
			continue;
		if (g_mailbox.production_generation[axis] > g_mailbox.production_consumed[axis])
			++g_mailbox.coalesced_count;
		g_mailbox.production_raw[axis] = clamp_raw_value(raw_values[index]);
		g_mailbox.production_touch[axis] = touch_sources[index] != 0;
		g_mailbox.production_generation[axis] = generation;
		if (!g_mailbox.automation_active)
			record_effective_transition_locked(
			    axis, g_mailbox.production_raw[axis],
			    g_mailbox.production_touch[axis], generation);
	}
	++g_mailbox.publish_count;
	return generation;
}

extern "C" android_axis_generation android_axis_mailbox_publish_automation(
    const int raw_values[ANDROID_AXIS_MAILBOX_AXIS_COUNT],
    const unsigned char touch_sources[ANDROID_AXIS_MAILBOX_AXIS_COUNT],
    int target_axis, int target_raw_value, int target_touch_source)
{
	if (!raw_values || !touch_sources || target_axis < 0 ||
	    target_axis >= ANDROID_AXIS_MAILBOX_AXIS_COUNT)
		return 0;

	std::lock_guard<std::mutex> guard(g_mailbox.lock);
	if (g_mailbox.automation_active &&
	    g_mailbox.automation_generation > g_mailbox.automation_consumed)
		++g_mailbox.coalesced_count;
	const android_axis_generation generation = next_generation_locked();
	for (int axis = 0; axis < ANDROID_AXIS_MAILBOX_AXIS_COUNT; ++axis) {
		g_mailbox.automation_raw[axis] = clamp_raw_value(raw_values[axis]);
		g_mailbox.automation_touch[axis] = touch_sources[axis] != 0;
		record_effective_transition_locked(
		    axis, g_mailbox.automation_raw[axis],
		    g_mailbox.automation_touch[axis], generation);
	}
	g_mailbox.automation_generation = generation;
	g_mailbox.automation_active = 1;
	g_mailbox.automation_target_axis = target_axis;
	g_mailbox.automation_target_raw = clamp_raw_value(target_raw_value);
	g_mailbox.automation_target_touch = target_touch_source != 0;
	++g_mailbox.publish_count;
	return generation;
}

extern "C" android_axis_generation android_axis_mailbox_release_automation(void)
{
	std::lock_guard<std::mutex> guard(g_mailbox.lock);
	if (!g_mailbox.automation_active)
		return 0;

	const android_axis_generation generation = next_generation_locked();
	g_mailbox.automation_active = 0;
	g_mailbox.automation_target_axis = -1;
	g_mailbox.automation_target_raw = 0;
	g_mailbox.automation_target_touch = 0;
	for (int axis = 0; axis < ANDROID_AXIS_MAILBOX_AXIS_COUNT; ++axis) {
		g_mailbox.production_generation[axis] = generation;
		record_effective_transition_locked(
		    axis, g_mailbox.production_raw[axis],
		    g_mailbox.production_touch[axis], generation);
	}
	++g_mailbox.publish_count;
	return generation;
}

extern "C" void android_axis_mailbox_set_button_deadzone(int axis, int deadzone)
{
	if (axis < 0 || axis >= ANDROID_AXIS_MAILBOX_AXIS_BUTTON_COUNT)
		return;

	std::lock_guard<std::mutex> guard(g_mailbox.lock);
	g_mailbox.button_deadzone[axis] = std::max(0, std::min(127, deadzone));
	const int effective_raw = g_mailbox.automation_active
	                              ? g_mailbox.automation_raw[axis]
	                              : g_mailbox.production_raw[axis];
	g_mailbox.effective_button_region[axis] =
	    button_region_locked(axis, effective_raw);
}

extern "C" int android_axis_mailbox_take_snapshot(android_axis_mailbox_snapshot *out_snapshot)
{
	if (!out_snapshot)
		return 0;

	std::lock_guard<std::mutex> guard(g_mailbox.lock);
	memset(out_snapshot, 0, sizeof(*out_snapshot));
	out_snapshot->automation_active = g_mailbox.automation_active;
	out_snapshot->target_axis = g_mailbox.automation_target_axis;
	out_snapshot->target_raw_value = g_mailbox.automation_target_raw;
	out_snapshot->target_touch_source = g_mailbox.automation_target_touch;
	for (int axis = 0; axis < ANDROID_AXIS_MAILBOX_AXIS_COUNT; ++axis) {
		if (g_mailbox.automation_active) {
			out_snapshot->raw_value[axis] = g_mailbox.automation_raw[axis];
			out_snapshot->touch_source[axis] = g_mailbox.automation_touch[axis];
			out_snapshot->axis_generation[axis] = g_mailbox.automation_generation;
		} else {
			out_snapshot->raw_value[axis] = g_mailbox.production_raw[axis];
			out_snapshot->touch_source[axis] = g_mailbox.production_touch[axis];
			out_snapshot->axis_generation[axis] = g_mailbox.production_generation[axis];
		}
		out_snapshot->should_dispatch[axis] =
		    out_snapshot->raw_value[axis] != 0 ||
		    out_snapshot->raw_value[axis] != g_mailbox.applied_raw[axis] ||
		    out_snapshot->touch_source[axis] != g_mailbox.applied_touch[axis];
		out_snapshot->batch_generation =
		    std::max(out_snapshot->batch_generation, out_snapshot->axis_generation[axis]);
	}
	return 1;
}

extern "C" int android_axis_mailbox_take_transition(
    android_axis_generation through_generation,
    android_axis_mailbox_transition *out_transition)
{
	if (!out_transition)
		return 0;

	std::lock_guard<std::mutex> guard(g_mailbox.lock);
	if (g_mailbox.button_transitions.empty() ||
	    g_mailbox.button_transitions.front().generation > through_generation)
		return 0;
	*out_transition = g_mailbox.button_transitions.front();
	g_mailbox.button_transitions.pop_front();
	return 1;
}

extern "C" void android_axis_mailbox_mark_applied(
    const android_axis_mailbox_snapshot *snapshot, int dispatch_count)
{
	if (!snapshot)
		return;

	std::lock_guard<std::mutex> guard(g_mailbox.lock);
	for (int axis = 0; axis < ANDROID_AXIS_MAILBOX_AXIS_COUNT; ++axis) {
		g_mailbox.applied_raw[axis] = snapshot->raw_value[axis];
		g_mailbox.applied_touch[axis] = snapshot->touch_source[axis];
		if (!snapshot->automation_active)
			g_mailbox.production_consumed[axis] =
			    std::max(g_mailbox.production_consumed[axis], snapshot->axis_generation[axis]);
	}
	if (snapshot->automation_active)
		g_mailbox.automation_consumed =
		    std::max(g_mailbox.automation_consumed, snapshot->batch_generation);
	g_mailbox.last_applied_generation = snapshot->batch_generation;
	++g_mailbox.drain_count;
	if (dispatch_count > 0)
		g_mailbox.dispatch_count += (unsigned int) dispatch_count;
}

extern "C" void android_axis_mailbox_get_diagnostics(
    android_axis_mailbox_diagnostics *out_diagnostics)
{
	if (!out_diagnostics)
		return;

	std::lock_guard<std::mutex> guard(g_mailbox.lock);
	memset(out_diagnostics, 0, sizeof(*out_diagnostics));
	out_diagnostics->last_published_generation = g_mailbox.last_published_generation;
	out_diagnostics->last_applied_generation = g_mailbox.last_applied_generation;
	out_diagnostics->publish_count = g_mailbox.publish_count;
	out_diagnostics->coalesced_count = g_mailbox.coalesced_count;
	out_diagnostics->drain_count = g_mailbox.drain_count;
	out_diagnostics->dispatch_count = g_mailbox.dispatch_count;
	out_diagnostics->automation_active = g_mailbox.automation_active;
	if (g_mailbox.automation_active) {
		out_diagnostics->pending_axis_count =
		    g_mailbox.automation_generation > g_mailbox.automation_consumed ? 1 : 0;
	} else {
		for (int axis = 0; axis < ANDROID_AXIS_MAILBOX_AXIS_COUNT; ++axis)
			if (g_mailbox.production_generation[axis] >
			    g_mailbox.production_consumed[axis])
				++out_diagnostics->pending_axis_count;
	}
}

extern "C" void android_axis_mailbox_flush(void)
{
	/*
	 * SDL event flushing applies only to discrete input. Axis samples describe
	 * the producer's current state and InputMixer intentionally publishes each
	 * transition once. Consuming one here could permanently lose a release (or
	 * press) that raced with event_flush(). Leave it pending for the game-thread
	 * drain instead.
	 */
}

extern "C" void android_axis_mailbox_reset_for_tests(void)
{
	std::lock_guard<std::mutex> guard(g_mailbox.lock);
	g_mailbox.next_generation = 0;
	memset(g_mailbox.production_raw, 0, sizeof(g_mailbox.production_raw));
	memset(g_mailbox.production_touch, 0, sizeof(g_mailbox.production_touch));
	memset(g_mailbox.production_generation, 0, sizeof(g_mailbox.production_generation));
	memset(g_mailbox.production_consumed, 0, sizeof(g_mailbox.production_consumed));
	memset(g_mailbox.automation_raw, 0, sizeof(g_mailbox.automation_raw));
	memset(g_mailbox.automation_touch, 0, sizeof(g_mailbox.automation_touch));
	g_mailbox.automation_generation = 0;
	g_mailbox.automation_consumed = 0;
	g_mailbox.automation_active = 0;
	g_mailbox.automation_target_axis = -1;
	g_mailbox.automation_target_raw = 0;
	g_mailbox.automation_target_touch = 0;
	memset(g_mailbox.applied_raw, 0, sizeof(g_mailbox.applied_raw));
	memset(g_mailbox.applied_touch, 0, sizeof(g_mailbox.applied_touch));
	std::fill(g_mailbox.button_deadzone,
	          g_mailbox.button_deadzone + ANDROID_AXIS_MAILBOX_AXIS_BUTTON_COUNT,
	          38);
	memset(g_mailbox.effective_button_region, 0,
	       sizeof(g_mailbox.effective_button_region));
	g_mailbox.button_transitions.clear();
	g_mailbox.last_published_generation = 0;
	g_mailbox.last_applied_generation = 0;
	g_mailbox.publish_count = 0;
	g_mailbox.coalesced_count = 0;
	g_mailbox.drain_count = 0;
	g_mailbox.dispatch_count = 0;
}
