#include <stdio.h>

#include "android_route_metadata_progress_policy.h"

static int expect_update(
	const char *label,
	int expected_accepted,
	int expected_permille,
	int expected_state,
	android_route_metadata_progress_update actual)
{
	if (actual.accepted == expected_accepted &&
	    actual.permille == expected_permille &&
	    actual.state == expected_state)
		return 0;
	fprintf(
		stderr,
		"%s: expected {%d, %d, %d}, got {%d, %d, %d}\n",
		label,
		expected_accepted,
		expected_permille,
		expected_state,
		actual.accepted,
		actual.permille,
		actual.state);
	return 1;
}

int main(void)
{
	int failures = 0;

	failures += expect_update(
		"stale generation is ignored",
		0,
		300,
		ANDROID_ROUTE_METADATA_CALCULATING,
		android_route_metadata_progress_policy(
			4, 5, 300, ANDROID_ROUTE_METADATA_CALCULATING,
			900, ANDROID_ROUTE_METADATA_USEFUL));
	failures += expect_update(
		"calculating progress is monotonic",
		1,
		300,
		ANDROID_ROUTE_METADATA_CALCULATING,
		android_route_metadata_progress_policy(
			5, 5, 300, ANDROID_ROUTE_METADATA_CALCULATING,
			200, ANDROID_ROUTE_METADATA_CALCULATING));
	failures += expect_update(
		"unfinished progress never reaches complete",
		1,
		999,
		ANDROID_ROUTE_METADATA_CALCULATING,
		android_route_metadata_progress_policy(
			5, 5, 300, ANDROID_ROUTE_METADATA_CALCULATING,
			1000, ANDROID_ROUTE_METADATA_CALCULATING));
	failures += expect_update(
		"useful state is preserved during later calculation",
		1,
		850,
		ANDROID_ROUTE_METADATA_USEFUL,
		android_route_metadata_progress_policy(
			5, 5, 800, ANDROID_ROUTE_METADATA_USEFUL,
			850, ANDROID_ROUTE_METADATA_CALCULATING));
	failures += expect_update(
		"failure after useful data is reported",
		1,
		850,
		ANDROID_ROUTE_METADATA_FAILED,
		android_route_metadata_progress_policy(
			5, 5, 850, ANDROID_ROUTE_METADATA_USEFUL,
			700, ANDROID_ROUTE_METADATA_FAILED));
	failures += expect_update(
		"completion reaches exactly one thousand",
		1,
		1000,
		ANDROID_ROUTE_METADATA_COMPLETE,
		android_route_metadata_progress_policy(
			5, 5, 850, ANDROID_ROUTE_METADATA_USEFUL,
			1000, ANDROID_ROUTE_METADATA_COMPLETE));
	failures += expect_update(
		"completed state cannot be replaced by failure",
		1,
		1000,
		ANDROID_ROUTE_METADATA_COMPLETE,
		android_route_metadata_progress_policy(
			5, 5, 1000, ANDROID_ROUTE_METADATA_COMPLETE,
			900, ANDROID_ROUTE_METADATA_FAILED));
	failures += expect_update(
		"invalid state is ignored",
		0,
		300,
		ANDROID_ROUTE_METADATA_CALCULATING,
		android_route_metadata_progress_policy(
			5, 5, 300, ANDROID_ROUTE_METADATA_CALCULATING,
			400, 9));

	return failures != 0;
}
