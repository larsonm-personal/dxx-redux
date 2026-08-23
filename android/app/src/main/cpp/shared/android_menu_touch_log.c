#include "android_menu_touch_log.h"

#include "android_log.h"
#include "android_menu_scale.h"
#include "event.h"

enum { ANDROID_MENU_TOUCH_LOG_LIMIT = 160 };

const char *android_menu_touch_event_phase(int event_type)
{
	if (event_type == EVENT_MOUSE_BUTTON_DOWN)
		return "down";
	if (event_type == EVENT_MOUSE_BUTTON_UP)
		return "up";
	return "other";
}

void android_log_newmenu_touch_state(const android_newmenu_touch_log_state *state)
{
	static int diag_count;
	android_menu_scale_result scale;

	if (!state || diag_count >= ANDROID_MENU_TOUCH_LOG_LIMIT)
		return;
	diag_count++;
	android_menu_scale_get_state(&scale);
	debug_log(DLOG_GAME,
	          "[newmenu-touch] phase=%s mx=%d my=%d hit=%d citem=%d n=%d scroll=%d max=%d bounds=(%d,%d %dx%d) type=%d title='%s' subtitle='%s' item='%s' scale=%d src=(%d,%d %dx%d) dst=(%d,%d %dx%d)\n",
	          state->phase, state->mx, state->my, state->hit, state->current_item,
	          state->item_count, state->scroll_offset, state->max_on_menu,
	          state->x1, state->y1, state->x2 - state->x1, state->y2 - state->y1,
	          state->item_type, state->title, state->subtitle, state->item_text,
	          scale.active, scale.src.x, scale.src.y, scale.src.w, scale.src.h,
	          scale.dst.x, scale.dst.y, scale.dst.w, scale.dst.h);
}

void android_log_listbox_touch_state(const android_listbox_touch_log_state *state)
{
	static int diag_count;
	android_menu_scale_result scale;

	if (!state || diag_count >= ANDROID_MENU_TOUCH_LOG_LIMIT)
		return;
	diag_count++;
	android_menu_scale_get_state(&scale);
	debug_log(DLOG_GAME,
	          "[listbox-touch] %s mx=%d my=%d item=%d citem=%d first=%d n=%d line=%d box=(%d,%d %dx%d) bounds=(%d,%d %dx%d) title='%s' text='%s' scale=%d src=(%d,%d %dx%d) dst=(%d,%d %dx%d)\n",
	          state->phase, state->mx, state->my, state->item, state->current_item,
	          state->first_item, state->item_count, state->line_spacing,
	          state->box_x, state->box_y, state->box_w, state->box_h,
	          state->x1, state->y1, state->x2 - state->x1, state->y2 - state->y1,
	          state->title, state->item_text, scale.active,
	          scale.src.x, scale.src.y, scale.src.w, scale.src.h,
	          scale.dst.x, scale.dst.y, scale.dst.w, scale.dst.h);
}

void android_log_menu_drag_cancel(const char *menu_kind, int dx, int dy, int threshold)
{
	static int diag_count;

	if (diag_count >= ANDROID_MENU_TOUCH_LOG_LIMIT)
		return;
	diag_count++;
	debug_log(DLOG_GAME,
	          "[menu-touch] drag-cancel kind=%s dx=%d dy=%d threshold=%d\n",
	          menu_kind ? menu_kind : "unknown", dx, dy, threshold);
}
