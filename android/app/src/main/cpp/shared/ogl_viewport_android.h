#ifndef OGL_VIEWPORT_ANDROID_H
#define OGL_VIEWPORT_ANDROID_H

struct android_ogl_viewport_state {
	int *last_width;
	int *last_height;
	int *last_keyboard_offset;
	int last_x;
	int last_y;
	int last_physical_x;
	int last_physical_y;
	int last_physical_width;
	int last_physical_height;
};

void android_ogl_viewport_apply(struct android_ogl_viewport_state *state,
                                int x, int y, int width, int height, int logical_screen_width,
                                int logical_screen_height, int canvas_height, int keyboard_offset);
void android_ogl_viewport_get_drawable_size(int logical_width,
                                            int logical_height, int *drawable_width, int *drawable_height);
void android_ogl_viewport_fill_keyboard_gap(int screen_width,
                                            int screen_height, int gap_height);

#endif
