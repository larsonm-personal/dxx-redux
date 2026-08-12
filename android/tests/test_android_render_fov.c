#include <stdint.h>
#include <stdio.h>

#include "android_render_fov.h"

static int expect_int(const char *label, int expected, int actual)
{
	if (expected == actual)
		return 0;
	fprintf(stderr, "%s: expected %d got %d\n", label, expected, actual);
	return 1;
}

int main(void)
{
	const int32_t base_zoom = 0x9000;
	int failures = 0;

	android_render_set_main_view_fov(0);
	failures += expect_int("default preference", 0, android_render_get_main_view_fov());
	failures += expect_int("default effective FOV", 0, android_render_main_view_fov_effective());
	failures += expect_int("default base zoom", base_zoom, android_render_main_view_zoom(base_zoom));

	android_render_set_main_view_fov(100);
	failures += expect_int("100 preference", 100, android_render_get_main_view_fov());
	failures += expect_int("100 zoom", 43940, android_render_main_view_zoom(base_zoom));
	android_render_set_main_view_fov(110);
	failures += expect_int("110 preference", 110, android_render_get_main_view_fov());
	failures += expect_int("110 zoom", 52658, android_render_main_view_zoom(base_zoom));
	android_render_set_main_view_fov(120);
	failures += expect_int("120 preference", 120, android_render_get_main_view_fov());
	failures += expect_int("120 zoom", 63858, android_render_main_view_zoom(base_zoom));

	android_render_set_main_view_fov(110);
	android_render_set_main_view_fov_locked(7);
	failures += expect_int("locked effective FOV", 0, android_render_main_view_fov_effective());
	failures += expect_int("locked base zoom", base_zoom, android_render_main_view_zoom(base_zoom));
	failures += expect_int("locked preference persists", 110, android_render_get_main_view_fov());
	android_render_set_main_view_fov_locked(0);
	failures += expect_int("unlocked preference restored", 110, android_render_main_view_fov_effective());

	android_render_set_main_view_fov(-1);
	failures += expect_int("negative FOV clamps", 0, android_render_get_main_view_fov());
	android_render_set_main_view_fov(101);
	failures += expect_int("unsupported FOV clamps", 0, android_render_get_main_view_fov());
	android_render_set_main_view_fov(121);
	failures += expect_int("high FOV clamps", 0, android_render_get_main_view_fov());

	if (failures)
		return 1;
	puts("PASS: Android render FOV policy preserves clamping, locking, preference, and zoom mapping");
	return 0;
}
