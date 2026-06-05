#ifndef ANDROID_MENU_REORDER_H
#define ANDROID_MENU_REORDER_H

#include "maths.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { ANDROID_MENU_REORDER_HOLD_TIME = F1_0 * 2 / 5 };

typedef struct android_menu_reorder_state {
	int grabbed;
	int button_down;
	fix64 button_time;
	int touch_candidate;
	fix64 touch_time;
} android_menu_reorder_state;

static inline void android_menu_reorder_init(android_menu_reorder_state *state)
{
	state->grabbed = 0;
	state->button_down = 0;
	state->button_time = 0;
	state->touch_candidate = -1;
	state->touch_time = 0;
}

static inline void android_menu_reorder_drop(android_menu_reorder_state *state)
{
	state->grabbed = 0;
	state->button_down = 0;
	state->touch_candidate = -1;
}

static inline void android_menu_reorder_start_button(android_menu_reorder_state *state, fix64 now)
{
	state->button_down = 1;
	state->button_time = now;
}

static inline void android_menu_reorder_stop_button(android_menu_reorder_state *state)
{
	state->button_down = 0;
}

static inline void android_menu_reorder_start_touch(android_menu_reorder_state *state, int item, fix64 now)
{
	state->touch_candidate = item;
	state->touch_time = now;
}

static inline int android_menu_reorder_hold_ready(fix64 now, fix64 start_time)
{
	return start_time && now - start_time >= ANDROID_MENU_REORDER_HOLD_TIME;
}

static inline int android_menu_reorder_button_ready(const android_menu_reorder_state *state, fix64 now)
{
	return state->button_down && android_menu_reorder_hold_ready(now, state->button_time);
}

static inline int android_menu_reorder_touch_ready(const android_menu_reorder_state *state, int mouse_down,
                                                   fix64 now)
{
	return mouse_down && state->touch_candidate >= 0 &&
	       android_menu_reorder_hold_ready(now, state->touch_time);
}

static inline void android_menu_reorder_mark_grabbed(android_menu_reorder_state *state, int item)
{
	state->grabbed = 1;
	state->button_down = 0;
	state->touch_candidate = item;
}

#ifdef __cplusplus
}
#endif

#endif
