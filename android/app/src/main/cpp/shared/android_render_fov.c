#include "android_render_fov.h"

static int Android_main_view_fov_degrees;
static int Android_main_view_fov_locked_to_base;

static int android_render_clamp_main_view_fov(int fov_degrees)
{
	if (fov_degrees == 100 || fov_degrees == 110 || fov_degrees == 120)
		return fov_degrees;
	return 0;
}

void android_render_set_main_view_fov(int fov_degrees)
{
	Android_main_view_fov_degrees = android_render_clamp_main_view_fov(fov_degrees);
}

int android_render_get_main_view_fov(void)
{
	return Android_main_view_fov_degrees;
}

void android_render_set_main_view_fov_locked(int locked_to_base)
{
	Android_main_view_fov_locked_to_base = locked_to_base ? 1 : 0;
}

int android_render_main_view_fov_effective(void)
{
	return Android_main_view_fov_locked_to_base ? 0 : Android_main_view_fov_degrees;
}

int32_t android_render_main_view_zoom(int32_t base_zoom)
{
	switch (android_render_main_view_fov_effective()) {
		case 100:
			return 43940;
		case 110:
			return 52658;
		case 120:
			return 63858;
		default:
			return base_zoom;
	}
}
