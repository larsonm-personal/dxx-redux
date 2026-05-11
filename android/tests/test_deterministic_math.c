#include <stdio.h>

#include "deterministic_math.h"

static int expect_fix(const char *label, fix expected, fix actual)
{
	if (expected == actual)
		return 0;
	fprintf(stderr, "%s expected %d got %d\n", label, expected, actual);
	return 1;
}

int main(void)
{
	int failures = 0;

	failures += expect_fix("30hz keeps old half-step", F1_0 / 2,
		dxx_ai_path_smoothing_delta(F1_0, F1_0 / 30));
	failures += expect_fix("60hz halves the half-step", F1_0 / 4,
		dxx_ai_path_smoothing_delta(F1_0, F1_0 / 60));
	failures += expect_fix("negative values stay symmetric", -F1_0 / 4,
		dxx_ai_path_smoothing_delta(-F1_0, F1_0 / 60));
	failures += expect_fix("zero frame time gives no smoothing", 0,
		dxx_ai_path_smoothing_delta(F1_0, 0));
	failures += expect_fix("fractional frames use integer scaling", F1_0 / 45,
		dxx_ai_path_smoothing_delta(F1_0 / 15, F1_0 / 45));

	if (failures)
		return 1;

	puts("PASS: deterministic math helper");
	return 0;
}