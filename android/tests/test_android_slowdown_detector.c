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
		frame->begin_gap_us = frame_period_us;
		frame->flip_gap_us = frame_period_us;
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

static void test_under_eight_fps_triggers(void)
{
	struct android_slowdown_detector detector;
	struct android_slowdown_frame frame;
	int events;

	android_slowdown_detector_init(&detector);
	android_slowdown_detector_set_enabled(&detector, 1);
	init_frame(&frame, 120);
	feed_frames(&detector, &frame, 600, 8333, 1000);
	events = feed_frames(&detector, &frame, 10, 500000, 1000);
	expect_true("under 8 FPS", events & ANDROID_SLOWDOWN_EVENT_TRIGGER);
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

static void test_single_hard_cadence_stall_triggers(void)
{
	struct android_slowdown_detector detector;
	struct android_slowdown_frame frame;
	int events;

	android_slowdown_detector_init(&detector);
	android_slowdown_detector_set_enabled(&detector, 1);
	init_frame(&frame, 120);
	feed_frames(&detector, &frame, 480, 8333, 1000);
	events = feed_frames(&detector, &frame, 1, 300000, 1000);
	expect_true("single hard cadence stall",
	            events & ANDROID_SLOWDOWN_EVENT_TRIGGER);
}

static void test_resume_frame_is_suppressed(void)
{
	struct android_slowdown_detector detector;
	struct android_slowdown_frame frame;
	int events;

	android_slowdown_detector_init(&detector);
	android_slowdown_detector_set_enabled(&detector, 1);
	init_frame(&frame, 25);
	feed_frames(&detector, &frame, 100, 40000, 1000);
	android_slowdown_detector_suppress_next_frame(&detector);
	events = feed_frames(&detector, &frame, 1, 600000000, 1000);
	expect_false("resume frame", events & ANDROID_SLOWDOWN_EVENT_TRIGGER);
	expect_true("resume frame suppression consumed",
	            !detector.suppress_next_frame);
	events = feed_frames(&detector, &frame, 100, 40000, 1000);
	expect_false("frames after resume", events & ANDROID_SLOWDOWN_EVENT_TRIGGER);
}

static void test_network_and_robot_metrics_aggregate(void)
{
	struct android_slowdown_detector detector;
	struct android_slowdown_frame frame;

	android_slowdown_detector_init(&detector);
	android_slowdown_detector_set_enabled(&detector, 1);
	init_frame(&frame, 25);
	frame.network_us = 2000;
	frame.network_packets = 3;
	frame.network_bytes = 900;
	frame.remote_robot_updates = 2;
	frame.max_remote_robot_age_ms = 450;
	feed_frames(&detector, &frame, 26, 40000, 1000);
	expect_true("network time aggregate",
	            detector.completed_window.network_us == 52000);
	expect_true("network packet aggregate",
	            detector.completed_window.network_packets == 78);
	expect_true("network byte aggregate",
	            detector.completed_window.network_bytes == 23400);
	expect_true("remote update aggregate",
	            detector.completed_window.remote_robot_updates == 52);
	expect_true("remote age maximum",
	            detector.completed_window.max_remote_robot_age_ms == 450);
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

static void test_hard_stall_interrupts_cooldown(void)
{
	struct android_slowdown_detector detector;
	struct android_slowdown_frame frame;
	int events;

	android_slowdown_detector_init(&detector);
	android_slowdown_detector_set_enabled(&detector, 1);
	init_frame(&frame, 120);
	feed_frames(&detector, &frame, 600, 8333, 7000);
	feed_frames(&detector, &frame, 50, 50000, 50000);
	feed_frames(&detector, &frame, 1201, 50000, 50000);
	expect_true("hard-stall test entered cooldown",
	            detector.state == ANDROID_SLOWDOWN_COOLDOWN);

	events = feed_frames(&detector, &frame, 1, 40000, 1000);
	expect_false("ordinary cooldown frame remains suppressed",
	             events & ANDROID_SLOWDOWN_EVENT_TRIGGER);
	events = feed_frames(&detector, &frame, 1, 300000, 1000);
	expect_true("hard stall interrupts cooldown",
	            events & ANDROID_SLOWDOWN_EVENT_TRIGGER);
	expect_true("hard stall starts a fresh capture",
	            detector.state == ANDROID_SLOWDOWN_CAPTURING);
}

static int feed_stutter(struct android_stutter_detector *detector,
                        struct android_stutter_frame *sample,
                        int count, int interval_us, int work_us)
{
	int reports = 0;
	int i;
	for (i = 0; i < count; i++) {
		sample->frame.frame_id++;
		sample->frame.end_us += interval_us;
		sample->frame.total_us = work_us;
		reports += android_stutter_detector_feed(detector, sample);
	}
	return reports;
}

static void test_stutter_gameplay_trace(void)
{
	struct android_stutter_detector detector;
	struct android_stutter_frame sample;
	unsigned int before;
	int reports;
	memset(&sample, 0, sizeof(sample));
	init_frame(&sample.frame, 25);
	android_stutter_detector_reset(&detector);
	feed_stutter(&detector, &sample, 350, 40000, 5000);
	expect_true("stutter heartbeat proves 25 FPS monitoring", detector.completed.frames > 0);
	expect_false("steady 25 FPS is not a hitch", detector.completed.hitches);
	android_stutter_detector_flush(&detector);
	before = sample.frame.frame_id;
	sample.move_us = 55000;
	sample.outer_us[ANDROID_OUTER_PRESENT] = 9000;
	sample.outer_cpu_us[ANDROID_OUTER_PRESENT] = 1000;
	sample.network.stage[ANDROID_NETWORK_RECEIVE].wall_us = 45000;
	sample.network.stage[ANDROID_NETWORK_RECEIVE].cpu_us = 25;
	feed_stutter(&detector, &sample, 1, 80000, 70000);
	sample.move_us = 0;
	memset(sample.outer_us, 0, sizeof(sample.outer_us));
	memset(sample.outer_cpu_us, 0, sizeof(sample.outer_cpu_us));
	memset(&sample.network, 0, sizeof(sample.network));
	reports = feed_stutter(&detector, &sample, 25, 40000, 5000);
	expect_true("isolated 80 ms frame reported", reports == 1);
	expect_true("single hitch count", detector.completed.hitches == 1);
	expect_true("exact worst frame retained", detector.completed.worst.frame.frame_id == before + 1);
	expect_true("predecessor retained", detector.completed.before_worst.frame.frame_id == before);
	expect_true("movement contribution retained", detector.completed.worst.move_us == 55000);
	expect_true("network wall/CPU evidence retained with the hitch",
	            detector.completed.worst.network.stage[ANDROID_NETWORK_RECEIVE].wall_us == 45000 &&
	                detector.completed.worst.network.stage[ANDROID_NETWORK_RECEIVE].cpu_us == 25);
	expect_true("outside callback gap retained",
	            detector.completed.max_interval_us - detector.completed.worst.frame.total_us == 10000);
	expect_true("preceding presentation wall/CPU evidence stays with its frame",
	            detector.completed.worst.outer_us[ANDROID_OUTER_PRESENT] == 9000 &&
	                detector.completed.worst.outer_cpu_us[ANDROID_OUTER_PRESENT] == 1000 &&
	                detector.completed.before_worst.outer_us[ANDROID_OUTER_PRESENT] == 0);
	expect_true("cap aware threshold", detector.completed.threshold_us == 60000);
	/* Another short stall must still report, regardless of sustained recorder cooldown */
	feed_stutter(&detector, &sample, 1, 90000, 5000);
	feed_stutter(&detector, &sample, 25, 40000, 5000);
	expect_true("scheduler/event stall reported", detector.completed.max_interval_us == 90000);
	expect_true("spikes do not inflate baseline", detector.baseline_us == 40000);
}

static void test_stutter_burst_and_transitions(void)
{
	struct android_stutter_detector detector;
	struct android_stutter_frame sample;
	int reports;
	memset(&sample, 0, sizeof(sample));
	init_frame(&sample.frame, 120);
	android_stutter_detector_reset(&detector);
	feed_stutter(&detector, &sample, 500, 8333, 1000);
	android_stutter_detector_flush(&detector);
	reports = feed_stutter(&detector, &sample, 20, 50000, 45000);
	expect_true("one bounded report for burst", reports == 1);
	expect_true("burst count is not rate limited", detector.completed.hitches == 20);
	expect_true("50 ms threshold at high FPS", detector.completed.threshold_us == 50000);
	feed_stutter(&detector, &sample, 1, 300000, 280000);
	expect_true("exit flush retains pending stall", android_stutter_detector_flush(&detector));
	expect_true("100 ms bin", detector.completed.over_100ms == 1);
	expect_true("250 ms bin", detector.completed.over_250ms == 1);
	sample.frame.level++;
	feed_stutter(&detector, &sample, 1, 2000000, 1900000);
	feed_stutter(&detector, &sample, 500, 8333, 1000);
	android_stutter_detector_flush(&detector);
	expect_false("level load excluded", detector.completed.hitches);
	sample.frame.max_fps = 25;
	feed_stutter(&detector, &sample, 200, 40000, 1000);
	android_stutter_detector_flush(&detector);
	expect_false("cap change excluded", detector.completed.hitches);
	android_stutter_detector_reset(&detector);
	feed_stutter(&detector, &sample, 1, 10000000, 10000000);
	feed_stutter(&detector, &sample, 200, 40000, 1000);
	android_stutter_detector_flush(&detector);
	expect_false("resume excluded", detector.completed.hitches);
	feed_stutter(&detector, &sample, 1, 60000000, 1000);
	feed_stutter(&detector, &sample, 200, 40000, 1000);
	android_stutter_detector_flush(&detector);
	expect_false("long discontinuity excluded", detector.completed.hitches);
}

static void test_network_attribution_trace(void)
{
	struct android_network_frame frame;
	struct android_network_sample sample;
	char report[16384] = "";
	char short_report[32] = "";
	int i;
	memset(&frame, 0, sizeof(frame));
	memset(&sample, 0, sizeof(sample));
	sample.wall_us = 68000;
	sample.cpu_us = 15;
	sample.socket_id = 7;
	sample.packet_type = 4;
	sample.bytes = 90;
	sample.result = 90;
	strcpy(sample.packet_name, "TEST_PACKET");
	android_network_profile_record(&frame, ANDROID_NETWORK_RECEIVE, &sample);
	sample.wall_us = 12;
	sample.cpu_us = 10;
	android_network_profile_record(&frame, ANDROID_NETWORK_RECEIVE, &sample);
	expect_true("receive waiting distinguished from CPU work",
	            frame.stage[ANDROID_NETWORK_RECEIVE].wall_us == 68012 &&
	                frame.stage[ANDROID_NETWORK_RECEIVE].cpu_us == 25);
	expect_true("slow syscall metadata retained",
	            frame.stage[ANDROID_NETWORK_RECEIVE].worst.socket_id == 7 &&
	                frame.stage[ANDROID_NETWORK_RECEIVE].worst.bytes == 90 &&
	                frame.stage[ANDROID_NETWORK_RECEIVE].worst.result == 90);
	for (i = 0; i < 6; i++) {
		sample.wall_us = i * 100;
		sample.cpu_us = i * 80;
		sample.packet_type = i;
		android_network_profile_record(&frame, ANDROID_NETWORK_DISPATCH, &sample);
	}
	expect_true("all packet dispatches counted", frame.stage[ANDROID_NETWORK_DISPATCH].calls == 6);
	expect_true("packet detail bounded and sorted", frame.packet_count == 3 &&
	                                                    frame.packets[0].packet_type == 5 && frame.packets[1].packet_type == 4 &&
	                                                    frame.packets[2].packet_type == 3);
	expect_true("packet CPU detail retained", frame.packets[0].cpu_us == 400);
	sample.wall_us = 70000;
	sample.cpu_us = -1;
	sample.result = -1;
	sample.error = 11;
	android_network_profile_record(&frame, ANDROID_NETWORK_SEND, &sample);
	sample.cpu_us = 50;
	sample.wall_us = 60;
	sample.result = 90;
	sample.error = 0;
	android_network_profile_record(&frame, ANDROID_NETWORK_SEND, &sample);
	expect_true("unavailable CPU clock is not reported as zero work",
	            frame.stage[ANDROID_NETWORK_SEND].cpu_us == -1);
	expect_true("send error preserved", frame.stage[ANDROID_NETWORK_SEND].errors == 1 &&
	                                        frame.stage[ANDROID_NETWORK_SEND].worst.error == 11);
	sample.wall_us = 150000;
	sample.cpu_us = 1500;
	android_network_profile_record(&frame, ANDROID_NETWORK_LISTEN, &sample);
	expect_true("inclusive listen scope remains separate",
	            frame.stage[ANDROID_NETWORK_LISTEN].wall_us == 150000 &&
	                frame.stage[ANDROID_NETWORK_DISPATCH].wall_us == 1500);
	android_network_profile_format(report, sizeof(report), "worst", 1247, &frame);
	expect_true("report includes receive wall and CPU totals",
	            strstr(report, "stage=receive calls=2 errors=0 wall_us=68012 cpu_us=25") != NULL);
	expect_true("report identifies the slow packet",
	            strstr(report, "frame=1247 rank=1 packet=5 name=TEST_PACKET socket=7 bytes=90 wall_us=500 cpu_us=400") != NULL);
	expect_true("report preserves failed socket call details",
	            strstr(report, "max_result=-1 max_errno=11") != NULL);
	android_network_profile_format(short_report, sizeof(short_report), "worst", 1247, &frame);
	expect_true("short report remains terminated", short_report[sizeof(short_report) - 1] == '\0');
	/* Exercise the production report budget with maximum numeric widths */
	sample.wall_us = INT64_MAX / 2;
	sample.cpu_us = INT64_MAX / 2;
	memset(&frame, 0, sizeof(frame));
	for (i = 0; i < ANDROID_NETWORK_STAGE_COUNT; i++)
		android_network_profile_record(&frame, i, &sample);
	strcpy(report, "existing frame report\n");
	android_network_profile_format(report, sizeof(report), "before", UINT32_MAX, &frame);
	android_network_profile_format(report, sizeof(report), "worst", UINT32_MAX, &frame);
	expect_true("paired report fits with room for frame summaries", strlen(report) < sizeof(report) - 4096);
	expect_true("last report line intact", report[strlen(report) - 1] == '\n');
}

int main(void)
{
	expect_true("detector memory stays under 128 KiB",
	            sizeof(struct android_slowdown_detector) <= 128 * 1024);
	test_25_fps_wait_does_not_trigger();
	test_sustained_render_slowdown_triggers();
	test_sustained_scheduler_slowdown_triggers();
	test_under_eight_fps_triggers();
	test_cap_change_does_not_trigger();
	test_three_severe_frames_trigger();
	test_single_hard_cadence_stall_triggers();
	test_resume_frame_is_suppressed();
	test_network_and_robot_metrics_aggregate();
	test_capture_ends_and_cools_down();
	test_hard_stall_interrupts_cooldown();
	test_stutter_gameplay_trace();
	test_stutter_burst_and_transitions();
	test_network_attribution_trace();

	if (failures) {
		fprintf(stderr, "%d slowdown detector test(s) failed\n", failures);
		return 1;
	}
	printf("android slowdown detector tests passed\n");
	return 0;
}
