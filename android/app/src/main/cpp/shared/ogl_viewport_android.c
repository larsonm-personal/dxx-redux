#ifdef ANDROID

#include <GLES3/gl3.h>

#include "ogl_viewport_android.h"

static int android_ogl_scale_axis(int value, int logical_extent,
                                  int physical_extent)
{
	long long scaled;

	if (logical_extent <= 0 || physical_extent <= 0)
		return value;
	scaled = (long long) value * physical_extent;
	if (scaled >= 0)
		scaled += logical_extent / 2;
	else
		scaled -= logical_extent / 2;
	return (int) (scaled / logical_extent);
}

void android_ogl_viewport_get_drawable_size(int logical_width,
                                            int logical_height, int *drawable_width, int *drawable_height)
{
	/* Android setBuffersGeometry keeps the EGL drawable at game resolution.
	 * The Java SurfaceView can be larger, but using that for GL viewport
	 * pushes centered menus off the actual drawable on some devices. */
	if (drawable_width)
		*drawable_width = logical_width;
	if (drawable_height)
		*drawable_height = logical_height;
}

void android_ogl_viewport_apply(struct android_ogl_viewport_state *state,
                                int x, int y, int width, int height, int logical_screen_width,
                                int logical_screen_height, int canvas_height, int keyboard_offset)
{
	int drawable_screen_width, drawable_screen_height;
	int viewport_y = canvas_height - y - height + keyboard_offset;
	int drawable_x, drawable_y, drawable_width, drawable_height;

	if (!state || !state->last_width || !state->last_height ||
	    !state->last_keyboard_offset)
		return;
	android_ogl_viewport_get_drawable_size(logical_screen_width,
	                                       logical_screen_height, &drawable_screen_width,
	                                       &drawable_screen_height);
	drawable_x = android_ogl_scale_axis(x, logical_screen_width,
	                                    drawable_screen_width);
	drawable_y = android_ogl_scale_axis(viewport_y, logical_screen_height,
	                                    drawable_screen_height);
	drawable_width = android_ogl_scale_axis(width, logical_screen_width,
	                                        drawable_screen_width);
	drawable_height = android_ogl_scale_axis(height, logical_screen_height,
	                                         drawable_screen_height);
	if (drawable_width <= 0 && width > 0)
		drawable_width = 1;
	if (drawable_height <= 0 && height > 0)
		drawable_height = 1;

	if (x != state->last_x || y != state->last_y ||
	    width != *state->last_width || height != *state->last_height ||
	    keyboard_offset != *state->last_keyboard_offset ||
	    drawable_x != state->last_physical_x ||
	    drawable_y != state->last_physical_y ||
	    drawable_width != state->last_physical_width ||
	    drawable_height != state->last_physical_height) {
		glViewport(drawable_x, drawable_y, drawable_width, drawable_height);
		state->last_x = x;
		state->last_y = y;
		*state->last_width = width;
		*state->last_height = height;
		*state->last_keyboard_offset = keyboard_offset;
		state->last_physical_x = drawable_x;
		state->last_physical_y = drawable_y;
		state->last_physical_width = drawable_width;
		state->last_physical_height = drawable_height;
	}
}

void android_ogl_viewport_fill_keyboard_gap(int screen_width,
                                            int screen_height, int gap_height)
{
	int drawable_width, drawable_height, drawable_gap_height;

	if (gap_height <= 0)
		return;
	if (gap_height > screen_height)
		gap_height = screen_height;
	android_ogl_viewport_get_drawable_size(screen_width, screen_height,
	                                       &drawable_width, &drawable_height);
	drawable_gap_height = android_ogl_scale_axis(gap_height, screen_height,
	                                             drawable_height);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glEnable(GL_SCISSOR_TEST);
	glScissor(0, 0, drawable_width, drawable_gap_height);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_SCISSOR_TEST);
}

#endif
