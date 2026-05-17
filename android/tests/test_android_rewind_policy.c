#include <stdio.h>

#include "android_rewind_policy.h"
#include "fix.h"

static int expect_index(const char *label, int expected, int actual)
{
	if (expected == actual)
		return 0;
	fprintf(stderr, "%s: expected %d got %d\n", label, expected, actual);
	return 1;
}

static int expect_u32(const char *label, uint32_t expected, uint32_t actual)
{
	if (expected == actual)
		return 0;
	fprintf(stderr, "%s: expected %u got %u\n", label, expected, actual);
	return 1;
}

static int expect_i64(const char *label, int64_t expected, int64_t actual)
{
	if (expected == actual)
		return 0;
	fprintf(stderr, "%s: expected %lld got %lld\n", label, (long long) expected, (long long) actual);
	return 1;
}

int main(void)
{
	const android_rewind_selection_snapshot configurable_targets[] = {
		{500, 1},
		{1000, 1},
		{1500, 1},
		{2000, 1},
	};
	const android_rewind_selection_snapshot threshold_skip[] = {
		{500, 1},
		{1000, 1},
	};
	const android_rewind_selection_snapshot threshold_exact[] = {
		{500, 1},
		{1000, 1},
	};
	const android_rewind_selection_snapshot single_recent[] = {
		{1100, 1},
	};
	const android_rewind_selection_snapshot current_only[] = {
		{1200, 1},
	};
	const android_rewind_selection_snapshot single_recent_for_long[] = {
		{1800, 1},
	};
	const android_rewind_selection_snapshot demo_filtered[] = {
		{500, 0},
		{1000, 1},
		{1300, 0},
	};
	const uint32_t boundary_frame_count = 2;
	const uint32_t continued_frame_count = 3;
	const int64_t frame_time64 = 2622;
	const uint32_t kept_frame_count = android_rewind_demo_timeline_frame_count(boundary_frame_count);
	int failures = 0;

	failures += expect_index(
		"sanitize invalid rewind target to default",
		ANDROID_REWIND_TARGET_SECONDS_DEFAULT,
		android_rewind_sanitize_target_seconds(7));
	failures += expect_index(
		"single-player capture stays allowed",
		1,
		android_rewind_is_capture_context_allowed(0, 0, 1, 1, 0));
	failures += expect_index(
		"coop host capture stays allowed",
		1,
		android_rewind_is_capture_context_allowed(1, 1, 1, 1, 0));
	failures += expect_index(
		"coop client capture is blocked",
		0,
		android_rewind_is_capture_context_allowed(1, 1, 0, 1, 0));
	failures += expect_index(
		"observer host capture is blocked",
		0,
		android_rewind_is_capture_context_allowed(1, 1, 1, 1, 1));
	failures += expect_index(
		"single-player request stays allowed",
		ANDROID_REWIND_REQUEST_ALLOW,
		android_rewind_classify_request_context(0, 0, 1, 1, 0, 0));
	failures += expect_index(
		"coop host request stays allowed",
		ANDROID_REWIND_REQUEST_ALLOW,
		android_rewind_classify_request_context(1, 1, 1, 1, 0, 0));
	failures += expect_index(
		"coop client request reports not host",
		ANDROID_REWIND_REQUEST_NOT_HOST,
		android_rewind_classify_request_context(1, 1, 0, 1, 0, 0));
	failures += expect_index(
		"non-coop multiplayer request stays blocked",
		ANDROID_REWIND_REQUEST_BLOCKED,
		android_rewind_classify_request_context(1, 0, 1, 1, 0, 0));
	failures += expect_index(
		"dead coop player blocks host request",
		ANDROID_REWIND_REQUEST_BLOCKED,
		android_rewind_classify_request_context(1, 1, 1, 0, 0, 0));
	failures += expect_index(
		"duplicate coop callsigns block host request",
		ANDROID_REWIND_REQUEST_BLOCKED,
		android_rewind_classify_request_context(1, 1, 1, 1, 0, 1));
	failures += expect_index(
		"keep short rewind target",
		ANDROID_REWIND_TARGET_SECONDS_SHORT,
		android_rewind_sanitize_target_seconds(ANDROID_REWIND_TARGET_SECONDS_SHORT));
	failures += expect_index(
		"keep long rewind target",
		ANDROID_REWIND_TARGET_SECONDS_LONG,
		android_rewind_sanitize_target_seconds(ANDROID_REWIND_TARGET_SECONDS_LONG));
	failures += expect_index(
		"round snapshot delta to nearest whole second",
		8,
		android_rewind_round_seconds_for_snapshot(
			(13 * F1_0) - (F1_0 / 100),
			5 * F1_0));
	failures += expect_index(
		"minimum rounded rewind amount is one second",
		1,
		android_rewind_round_seconds_for_snapshot(10 * F1_0, 10 * F1_0));
	failures += expect_index(
		"5 second target picks the newest snapshot old enough",
		2,
		android_rewind_select_snapshot_index_for_game_time(
			configurable_targets,
			4,
			2400,
			500,
			0));
	failures += expect_index(
		"10 second target picks the next older snapshot",
		1,
		android_rewind_select_snapshot_index_for_game_time(
			configurable_targets,
			4,
			2400,
			1000,
			0));
	failures += expect_index(
		"20 second target falls back only when one older point remains",
		0,
		android_rewind_select_snapshot_index_for_game_time(
			single_recent_for_long,
			1,
			2400,
			2000,
			0));

	failures += expect_index(
		"skip point newer than min age",
		0,
		android_rewind_select_snapshot_index_for_game_time(threshold_skip, 2, 1299, 300, 0));
	failures += expect_index(
		"pick newest point at exact min age",
		1,
		android_rewind_select_snapshot_index_for_game_time(threshold_exact, 2, 1300, 300, 0));
	failures += expect_index(
		"allow single remaining older point",
		0,
		android_rewind_select_snapshot_index_for_game_time(single_recent, 1, 1200, 300, 0));
	failures += expect_index(
		"reject current snapshot as rewind target",
		-1,
		android_rewind_select_snapshot_index_for_game_time(current_only, 1, 1200, 300, 0));
	failures += expect_index(
		"require demo timeline when recording",
		1,
		android_rewind_select_snapshot_index_for_game_time(demo_filtered, 3, 1400, 300, 1));
	failures += expect_u32(
		"empty recorder count stays empty",
		0,
		android_rewind_demo_timeline_frame_count(0));
	failures += expect_u32(
		"first recorded frame remains a completed boundary frame",
		1,
		android_rewind_demo_timeline_frame_count(1));
	failures += expect_u32(
		"boundary snapshot keeps the current recorded timeline",
		8,
		android_rewind_demo_timeline_frame_count(8));
	failures += expect_i64(
		"rewind then continue produces replayable final game time",
		(int64_t) (boundary_frame_count + continued_frame_count) * frame_time64,
		(int64_t) (kept_frame_count + continued_frame_count) * frame_time64);

	if (failures)
		return 1;
	puts("PASS: rewind policy respects target age, demo timeline, and boundary frame counts");
	return 0;
}
