#include <stdio.h>

#include "coop_indicator_lines_math.h"

static int expect_fix(const char *label, fix expected, fix actual)
{
	if (expected == actual)
		return 0;
	fprintf(stderr, "%s expected %d got %d\n", label, expected, actual);
	return 1;
}

static int expect_int(const char *label, int expected, int actual)
{
	if (expected == actual)
		return 0;
	fprintf(stderr, "%s expected %d got %d\n", label, expected, actual);
	return 1;
}

int main(void)
{
	int failures = 0;

	failures += expect_fix("fade in quarter second", F1_0 / 4,
		coop_indicator_line_advance_alpha(0, 1, F1_0 / 4));
	failures += expect_fix("fade in clamps full", F1_0,
		coop_indicator_line_advance_alpha(F1_0 - F1_0 / 8, 1, F1_0 / 4));
	failures += expect_fix("fade out half second", F1_0 / 2,
		coop_indicator_line_advance_alpha(F1_0, 0, F1_0 / 2));
	failures += expect_fix("fade out clamps empty", 0,
		coop_indicator_line_advance_alpha(F1_0 / 8, 0, F1_0 / 4));
	failures += expect_fix("negative frame time holds", F1_0 / 2,
		coop_indicator_line_advance_alpha(F1_0 / 2, 1, -1));

	failures += expect_int("empty alpha disables blend", GR_FADE_OFF,
		coop_indicator_line_fade_level(0));
	failures += expect_int("full alpha keeps legacy fade",
		COOP_INDICATOR_LINE_FADE_LEVEL,
		coop_indicator_line_fade_level(F1_0));
	failures += expect_int("half alpha fades halfway", 23,
		coop_indicator_line_fade_level(F1_0 / 2));

	failures += expect_int("inner screen center", 1,
		coop_indicator_target_in_inner_screen(i2f(50), i2f(40), 100, 80));
	failures += expect_int("inner screen left margin inclusive", 1,
		coop_indicator_target_in_inner_screen(i2f(25), i2f(40), 100, 80));
	failures += expect_int("inner screen left edge outside", 0,
		coop_indicator_target_in_inner_screen(i2f(24), i2f(40), 100, 80));
	failures += expect_int("inner screen top edge outside", 0,
		coop_indicator_target_in_inner_screen(i2f(50), i2f(19), 100, 80));
	failures += expect_int("inner screen bad size outside", 0,
		coop_indicator_target_in_inner_screen(i2f(50), i2f(40), 0, 80));

	if (failures)
		return 1;

	puts("PASS: coop indicator line math");
	return 0;
}
