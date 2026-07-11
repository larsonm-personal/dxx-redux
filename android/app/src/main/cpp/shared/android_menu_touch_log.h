#ifndef ANDROID_MENU_TOUCH_LOG_H
#define ANDROID_MENU_TOUCH_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct android_newmenu_touch_log_state {
	const char *phase;
	int mx;
	int my;
	int hit;
	int current_item;
	int item_count;
	int scroll_offset;
	int max_on_menu;
	int x1;
	int y1;
	int x2;
	int y2;
	int item_type;
	const char *title;
	const char *subtitle;
	const char *item_text;
} android_newmenu_touch_log_state;

typedef struct android_listbox_touch_log_state {
	const char *phase;
	int mx;
	int my;
	int item;
	int current_item;
	int first_item;
	int item_count;
	int line_spacing;
	int box_x;
	int box_y;
	int box_w;
	int box_h;
	int x1;
	int y1;
	int x2;
	int y2;
	const char *title;
	const char *item_text;
} android_listbox_touch_log_state;

const char *android_menu_touch_event_phase(int event_type);
void android_log_newmenu_touch_state(const android_newmenu_touch_log_state *state);
void android_log_listbox_touch_state(const android_listbox_touch_log_state *state);

#ifdef __cplusplus
}
#endif

#endif
