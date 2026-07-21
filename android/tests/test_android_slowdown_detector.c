#include <stdio.h>
#include <string.h>

#include "android_slowdown_detector.h"

static int failures;

static void expect_true(const char *label, int value)
{
	if (value)
		return;
	fprintf(stderr, "%s: expected true\n", label);
	failures++;
}

static void expect_false(const char *label, int value)
{
	if (!value)
		return;
	fprintf(stderr, "%s: expected false\n", label);
	failures++;
}

static int feed_frames(struct android_slowdown_detector *detector,
	                   struct android_slowdown_frame *frame,
	                   int count, int frame_period_us, int work_us)
{
	int events = 0;
	int i;

	for (i = 0; i < count; i++) {
		frame->frame_id++;
		frame->end_us += frame_period_us;
		frame->total_us = frame_period_us;
		frame->wait_us = frame_period_us > work_us ? frame_period_us - work_us : 0;
		frame->render_us = work_us;
		events |= android_slowdown_detector_feed(detector, frame);
	}
	return events;
}

static void init_frame(struct android_slowdown_frame *frame, int max_fps)
{
	memset(frame, 0, sizeof(*frame));
	frame->level = 1;
	frame->viewer_segment = 0;
	frame->max_fps = max_fps;
}

static void test_25_fps_wait_does_not_trigger(void)
{
	struct android_slowdown_detector detector;
	struct android_slowdown_frame frame;
	int events;

	android_slowdown_detector_init(&detector);
	android_slowdown_detector_set_enabled(&detector, 1);
	init_frame(&frame, 25);
	events = feed_frames(&detector, &frame, 250, 40000, 1000);
	expect_false("25 FPS intentional wait", events & ANDROID_SLOWDOWN_EVENT_TRIGGER);
}

static void test_sustained_render_slowdown_triggers(void)
{
	struct android_slowdown_detector detector;
	struct android_slowdown_frame frame;
	int events;

	android_slowdown_detector_init(&detector);
	android_slowdown_detector_set_enabled(&detector, 1);
	init_frame(&frame, 120);
	feed_frames(&detector, &frame, 600, 8333, 7000);
	events = feed_frames(&detector, &frame, 50, 50000, 50000);
	expect_true("sustained render slowdown", events & ANDROID_SLOWDOWN_EVENT_TRIGGER);
	expect_true("capturing state", detector.state == ANDROID_SLOWDOWN_CAPTURING);
}

static void test_sustained_scheduler_slowdown_triggers(void)
{
	struct android_slowdown_detector detector;
	struct android_slowdown_frame frame;
	int events;

	android_slowdown_detector_init(&detector);
	android_slowdown_detector_set_enabled(&detector, 1);
	init_frame(&frame, 120);
	feed_frames(&detector, &frame, 600, 8333, 1000);
	events = feed_frames(&detector, &frame, 50, 50000, 1000);
	expect_true("sustained scheduler slowdown", events & ANDROID_SLOWDOWN_EVENT_TRIGGER);
}

static void test_cap_change_does_not_trigger(void)
{
	struct android_slowdown_detector detector;
	struct android_slowdown_frame frame;
	int events;

	android_slowdown_detector_init(&detector);
	android_slowdown_detector_set_enabled(&detector, 1);
	init_frame(&frame, 120);
	feed_frames(&detector, &frame, 600, 8333, 7000);
	frame.max_fps = 25;
	events = feed_frames(&detector, &frame, 250, 40000, 1000);
	expect_false("120 to 25 FPS cap change", events & ANDROID_SLOWDOWN_EVENT_TRIGGER);
}

static void test_three_severe_frames_trigger(void)
{
	struct android_slowdown_detector detector;
	struct android_slowdown_frame frame;
	int events = 0;

	android_slowdown_detector_init(&detector);
	android_slowdown_detector_set_enabled(&detector, 1);
	init_frame(&frame, 120);
	feed_frames(&detector, &frame, 480, 8333, 7000);
	events |= feed_frames(&detector, &frame, 1, 100000, 100000);
	events |= feed_frames(&detector, &frame, 5, 8333, 7000);
	events |= feed_frames(&detector, &frame, 1, 100000, 100000);
	events |= feed_frames(&detector, &frame, 5, 8333, 7000);
	events |= feed_frames(&detector, &frame, 1, 100000, 100000);
	expect_true("three severe frames", events & ANDROID_SLOWDOWN_EVENT_TRIGGER);
}

static void test_capture_ends_and_cools_down(void)
{
	struct android_slowdown_detector detector;
	struct android_slowdown_frame frame;
	int events;

	android_slowdown_detector_init(&detector);
	android_slowdown_detector_set_enabled(&detector, 1);
	init_frame(&frame, 120);
	feed_frames(&detector, &frame, 600, 8333, 7000);
	feed_frames(&detector, &frame, 50, 50000, 50000);
	events = feed_frames(&detector, &frame, 1201, 50000, 50000);
	expect_true("capture end event", events & ANDROID_SLOWDOWN_EVENT_CAPTURE_END);
	expect_true("cooldown state", detector.state == ANDROID_SLOWDOWN_COOLDOWN);
}

int main(void)
{
	expect_true("detector memory stays under 128 KiB",
	            sizeof(struct android_slowdown_detector) <= 128 * 1024);
	test_25_fps_wait_does_not_trigger();
	test_sustained_render_slowdown_triggers();
	test_sustained_scheduler_slowdown_triggers();
	test_cap_change_does_not_trigger();
	test_three_severe_frames_trigger();
	test_capture_ends_and_cools_down();

	if (failures) {
		fprintf(stderr, "%d slowdown detector test(s) failed\n", failures);
		return 1;
	}
	printf("android slowdown detector tests passed\n");
	return 0;
}
