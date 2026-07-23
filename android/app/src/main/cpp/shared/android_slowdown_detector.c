#include "android_slowdown_detector.h"

#include <limits.h>
#include <string.h>

#define WINDOW_US               1000000LL
#define DISCONTINUITY_US        30000000LL
#define LEVEL_SUPPRESS_US       3000000LL
#define CAPTURE_US              60000000LL
#define COOLDOWN_US             300000000LL
#define SEVERE_FRAME_US         100000
#define SEVERE_SPAN_US          2000000LL
#define SLOW_PERCENT            70
#define ABSOLUTE_SLOW_FPS_MILLI 8000

static int32_t clamp_i64_to_i32(int64_t value)
{
	if (value > INT_MAX)
		return INT_MAX;
	if (value < INT_MIN)
		return INT_MIN;
	return (int32_t) value;
}

static int32_t frame_nonwait_us(const struct android_slowdown_frame *frame)
{
	int64_t value = (int64_t) frame->total_us - frame->wait_us;

	if (value < 0)
		value = 0;
	return clamp_i64_to_i32(value);
}

static void reset_window(struct android_slowdown_detector *detector, int64_t start_us)
{
	memset(&detector->current_window, 0, sizeof(detector->current_window));
	detector->current_window.start_us = start_us;
}

static void reset_baseline(struct android_slowdown_detector *detector,
                           const struct android_slowdown_frame *frame)
{
	detector->baseline_fps_milli = 0;
	detector->slow_windows = 0;
	detector->configured_max_fps = frame->max_fps;
	detector->configured_vsync = frame->vsync;
	detector->suppress_until_us = frame->end_us + LEVEL_SUPPRESS_US;
	memset(detector->severe_frame_us, 0, sizeof(detector->severe_frame_us));
	reset_window(detector, frame->end_us);
}

static void ring_push(struct android_slowdown_detector *detector,
                      const struct android_slowdown_frame *frame)
{
	detector->ring[detector->ring_write] = *frame;
	detector->ring_write = (detector->ring_write + 1) % ANDROID_SLOWDOWN_RING_CAPACITY;
	if (detector->ring_count < ANDROID_SLOWDOWN_RING_CAPACITY)
		detector->ring_count++;
}

static void insert_worst(struct android_slowdown_window *window,
                         const struct android_slowdown_frame *frame)
{
	const int32_t nonwait_us = frame_nonwait_us(frame);
	int i;

	for (i = 0; i < ANDROID_SLOWDOWN_WORST_COUNT; i++) {
		if (nonwait_us <= frame_nonwait_us(&window->worst[i]))
			continue;
		if (i + 1 < ANDROID_SLOWDOWN_WORST_COUNT)
			memmove(&window->worst[i + 1], &window->worst[i],
			        (ANDROID_SLOWDOWN_WORST_COUNT - i - 1) * sizeof(window->worst[0]));
		window->worst[i] = *frame;
		break;
	}
}

static void note_severe_frame(struct android_slowdown_detector *detector,
                              const struct android_slowdown_frame *frame)
{
	if (frame_nonwait_us(frame) < SEVERE_FRAME_US)
		return;
	detector->severe_frame_us[0] = detector->severe_frame_us[1];
	detector->severe_frame_us[1] = detector->severe_frame_us[2];
	detector->severe_frame_us[2] = frame->end_us;
}

static int severe_triggered(const struct android_slowdown_detector *detector,
                            int64_t now_us)
{
	return detector->severe_frame_us[0] > 0 &&
	       now_us - detector->severe_frame_us[0] <= SEVERE_SPAN_US;
}

static int finish_window(struct android_slowdown_detector *detector, int64_t now_us)
{
	struct android_slowdown_window *window = &detector->current_window;
	const int64_t span_us = now_us - window->start_us;
	int32_t expected_fps_milli;
	int slow;

	if (span_us < WINDOW_US || window->frames <= 0)
		return 0;

	window->end_us = now_us;
	window->fps_milli = clamp_i64_to_i32((int64_t) window->frames * 1000000000LL / span_us);
	expected_fps_milli = detector->baseline_fps_milli;
	if (!detector->configured_vsync && detector->configured_max_fps > 0) {
		const int32_t cap_milli = detector->configured_max_fps * 1000;
		if (!expected_fps_milli || expected_fps_milli > cap_milli)
			expected_fps_milli = cap_milli;
	}
	window->expected_fps_milli = expected_fps_milli;
	detector->completed_window = *window;

	slow = expected_fps_milli > 0 &&
	       (int64_t) window->fps_milli * 100 < (int64_t) expected_fps_milli * SLOW_PERCENT;
	if (window->fps_milli < ABSOLUTE_SLOW_FPS_MILLI)
		detector->slow_windows = 2;
	else if (slow)
		detector->slow_windows++;
	else
		detector->slow_windows = 0;

	if (window->fps_milli > detector->baseline_fps_milli)
		detector->baseline_fps_milli = window->fps_milli;

	reset_window(detector, now_us);
	return 1;
}

void android_slowdown_detector_init(struct android_slowdown_detector *detector)
{
	memset(detector, 0, sizeof(*detector));
	detector->state = ANDROID_SLOWDOWN_DISABLED;
	detector->current_level = INT_MIN;
}

void android_slowdown_detector_set_enabled(struct android_slowdown_detector *detector,
                                           int enabled)
{
	if (!enabled) {
		if (detector->state != ANDROID_SLOWDOWN_DISABLED)
			android_slowdown_detector_init(detector);
		return;
	}
	if (detector->state == ANDROID_SLOWDOWN_DISABLED)
		detector->state = ANDROID_SLOWDOWN_ARMED;
}

int android_slowdown_detector_feed(struct android_slowdown_detector *detector,
                                   const struct android_slowdown_frame *frame)
{
	struct android_slowdown_window *window;
	const int32_t nonwait_us = frame_nonwait_us(frame);
	int events = 0;
	int window_finished;

	if (detector->state == ANDROID_SLOWDOWN_DISABLED)
		return 0;

	if (detector->last_frame_us && frame->end_us - detector->last_frame_us >= DISCONTINUITY_US) {
		detector->slow_windows = 0;
		memset(detector->severe_frame_us, 0, sizeof(detector->severe_frame_us));
		reset_window(detector, frame->end_us);
	}
	detector->last_frame_us = frame->end_us;

	if (frame->max_fps != detector->configured_max_fps ||
	    frame->vsync != detector->configured_vsync)
		reset_baseline(detector, frame);
	if (frame->level != detector->current_level) {
		detector->current_level = frame->level;
		detector->suppress_until_us = frame->end_us + LEVEL_SUPPRESS_US;
		detector->slow_windows = 0;
	}

	ring_push(detector, frame);
	note_severe_frame(detector, frame);

	window = &detector->current_window;
	if (!window->start_us)
		window->start_us = frame->end_us;
	window->frames++;
	window->total_us += frame->total_us;
	window->nonwait_us += nonwait_us;
	if (nonwait_us > window->max_nonwait_us)
		window->max_nonwait_us = nonwait_us;
	insert_worst(window, frame);

	window_finished = finish_window(detector, frame->end_us);
	if (window_finished && detector->state == ANDROID_SLOWDOWN_CAPTURING)
		events |= ANDROID_SLOWDOWN_EVENT_WINDOW;

	if (detector->state == ANDROID_SLOWDOWN_CAPTURING &&
	    frame->end_us >= detector->capture_end_us) {
		detector->state = ANDROID_SLOWDOWN_COOLDOWN;
		detector->cooldown_end_us = frame->end_us + COOLDOWN_US;
		events |= ANDROID_SLOWDOWN_EVENT_CAPTURE_END;
	} else if (detector->state == ANDROID_SLOWDOWN_COOLDOWN &&
	           frame->end_us >= detector->cooldown_end_us) {
		detector->state = ANDROID_SLOWDOWN_ARMED;
		detector->slow_windows = 0;
	}

	if (detector->state == ANDROID_SLOWDOWN_ARMED &&
	    frame->end_us >= detector->suppress_until_us &&
	    (detector->slow_windows >= 2 || severe_triggered(detector, frame->end_us))) {
		detector->state = ANDROID_SLOWDOWN_CAPTURING;
		detector->capture_id++;
		detector->capture_end_us = frame->end_us + CAPTURE_US;
		detector->trigger_severe = detector->slow_windows < 2;
		detector->slow_windows = 0;
		events |= ANDROID_SLOWDOWN_EVENT_TRIGGER;
	}

	return events;
}

int android_slowdown_detector_ring_count(const struct android_slowdown_detector *detector)
{
	return (int) detector->ring_count;
}

const struct android_slowdown_frame *android_slowdown_detector_ring_get(
    const struct android_slowdown_detector *detector, int oldest_index)
{
	uint32_t first;
	uint32_t index;

	if (oldest_index < 0 || (uint32_t) oldest_index >= detector->ring_count)
		return NULL;
	first = (detector->ring_write + ANDROID_SLOWDOWN_RING_CAPACITY - detector->ring_count) %
	        ANDROID_SLOWDOWN_RING_CAPACITY;
	index = (first + (uint32_t) oldest_index) % ANDROID_SLOWDOWN_RING_CAPACITY;
	return &detector->ring[index];
}

int android_slowdown_detector_detail_active(const struct android_slowdown_detector *detector,
                                            int64_t now_us)
{
	int64_t capture_elapsed_us;

	if (detector->state != ANDROID_SLOWDOWN_CAPTURING)
		return 0;
	capture_elapsed_us = now_us - (detector->capture_end_us - CAPTURE_US);
	if (capture_elapsed_us < 2000000LL)
		return 1;
	return capture_elapsed_us % 10000000LL < 1000000LL;
}
